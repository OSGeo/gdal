/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGLayer class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.9  2001/11/15 21:19:47  warmerda
 * added soft transaction semantics, handle null fields properly
 *
 * Revision 1.8  2001/11/15 16:10:12  warmerda
 * fixed some escaping issues with string field values
 *
 * Revision 1.7  2001/09/28 04:03:52  warmerda
 * partially upraded to PostGIS 0.6
 *
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2001/06/26 20:59:13  warmerda
 * implement efficient spatial and attribute query support
 *
 * Revision 1.4  2001/06/19 22:29:12  warmerda
 * upgraded to include PostGIS support
 *
 * Revision 1.3  2001/06/19 15:50:23  warmerda
 * added feature attribute query support
 *
 * Revision 1.2  2000/11/23 06:03:35  warmerda
 * added Oid support
 *
 * Revision 1.1  2000/10/17 17:46:51  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_pg.h"
#include <libpq/libpq-fs.h>

CPL_CVSID("$Id$");

#define CURSOR_PAGE	1

/************************************************************************/
/*                           OGRPGLayer()                               */
/************************************************************************/

OGRPGLayer::OGRPGLayer( OGRPGDataSource *poDSIn, const char * pszTableName,
                        int bUpdate )

{
    poDS = poDSIn;

    poFilterGeom = NULL;
    pszQuery = NULL;
    pszWHERE = CPLStrdup( "" );
    
    bUpdateAccess = bUpdate;
    bHasWkb = FALSE;
    bWkbAsOid = FALSE;
    bHasPostGISGeometry = FALSE;
    pszGeomColumn = NULL;

    iNextShapeId = 0;

    /* Eventually we may need to make these a unique name */
    pszCursorName = "OGRPGLayerReader";
    hCursorResult = NULL;

    poFeatureDefn = ReadTableDefinition( pszTableName );
}

/************************************************************************/
/*                            ~OGRPGLayer()                             */
/************************************************************************/

OGRPGLayer::~OGRPGLayer()

{
    ResetReading();

    delete poFeatureDefn;

    if( poFilterGeom != NULL )
        delete poFilterGeom;

    if( pszGeomColumn != NULL )
        CPLFree( pszGeomColumn );

    CPLFree( pszQuery );
    CPLFree( pszWHERE );
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/************************************************************************/

OGRFeatureDefn *OGRPGLayer::ReadTableDefinition( const char * pszTable )

{
    PGresult            *hResult;
    char		szCommand[1024];
    PGconn		*hPGConn = poDS->GetPGConn();
    
/* -------------------------------------------------------------------- */
/*      Fire off commands to get back the schema of the table.          */
/* -------------------------------------------------------------------- */
    poDS->FlushSoftTransaction();
    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );
        sprintf( szCommand, 
                 "DECLARE mycursor CURSOR for "
                 "SELECT a.attname, t.typname, a.attlen "
                 "FROM pg_class c, pg_attribute a, pg_type t "
                 "WHERE c.relname = '%s' "
                 "AND a.attnum > 0 AND a.attrelid = c.oid "
                 "AND a.atttypid = t.oid", 
                 pszTable );

        hResult = PQexec(hPGConn, szCommand );
    }

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );
        hResult = PQexec(hPGConn, "FETCH ALL in mycursor" );
    }

    if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", PQerrorMessage(hPGConn) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszTable );
    int		   iRecord;

    for( iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
    {
        const char	*pszType;
        OGRFieldDefn    oField( PQgetvalue( hResult, iRecord, 0 ), OFTString);

        pszType = PQgetvalue(hResult, iRecord, 1 );
        
        if( EQUAL(oField.GetNameRef(),"ogc_fid") )
        {
            bHasFid = TRUE;
            continue;
        }
        else if( EQUAL(pszType,"geometry") )
        {
            bHasPostGISGeometry = TRUE;
            pszGeomColumn = CPLStrdup( oField.GetNameRef());
            continue;
        }
        else if( EQUAL(oField.GetNameRef(),"WKB_GEOMETRY") )
        {
            bHasWkb = TRUE;
            if( EQUAL(pszType,"OID") )
                bWkbAsOid = TRUE;
            continue;
        }

        if( EQUAL(pszType,"varchar") )
        {
            oField.SetType( OFTString );
        }
        else if( EQUALN(pszType,"int",3) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType, "float", 5) ) 
        {
            oField.SetType( OFTReal );
        }
        else
        {
            oField.SetWidth( atoi(PQgetvalue(hResult,iRecord,2)) );
        }
        
        poDefn->AddFieldDefn( &oField );
    }

    PQclear( hResult );

    hResult = PQexec(hPGConn, "CLOSE mycursor");
    PQclear( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );

    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPGLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();

    BuildWhere();

    ResetReading();
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRPGLayer::BuildWhere()

{
    char	szWHERE[4096];

    CPLFree( pszWHERE );
    pszWHERE = NULL;

    szWHERE[0] = '\0';

    if( poFilterGeom != NULL && bHasPostGISGeometry )
    {
        OGREnvelope  sEnvelope;

        poFilterGeom->getEnvelope( &sEnvelope );
        sprintf( szWHERE, 
                 "WHERE %s && 'BOX3D(%.12f %.12f, %.12f %.12f)'::box3d ",
                 pszGeomColumn, 
                 sEnvelope.MinX, sEnvelope.MinY, 
                 sEnvelope.MaxX, sEnvelope.MaxY );
    }

    if( pszQuery != NULL )
    {
        if( strlen(szWHERE) == 0 )
            sprintf( szWHERE, "WHERE %s ", pszQuery  );
        else
            sprintf( szWHERE+strlen(szWHERE), "AND %s ", pszQuery );
    }

    pszWHERE = CPLStrdup(szWHERE);
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

char *OGRPGLayer::BuildFields()

{
    int		i, nSize;
    char	*pszFieldList;

    nSize = 25;
    if( pszGeomColumn )
        nSize += strlen(pszGeomColumn);

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 2;

    pszFieldList = (char *) CPLMalloc(nSize);

    if( pszGeomColumn )
    {
        if( bHasPostGISGeometry )
        {
            sprintf( pszFieldList, "AsText(%s)", pszGeomColumn );
        }
        else
        {
            sprintf( pszFieldList, "%s", pszGeomColumn );
        }
    }
    else
        pszFieldList[0] = '\0';

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        strcat( pszFieldList, pszName );
    }

    CPLAssert( (int) strlen(pszFieldList) < nSize );

    return pszFieldList;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPGLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree( this->pszQuery );
    this->pszQuery = CPLStrdup( pszQuery );

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGLayer::ResetReading()

{
    PGconn	*hPGConn = poDS->GetPGConn();
    char	szCommand[1024];

    iNextShapeId = 0;

    if( hCursorResult != NULL )
    {
        PQclear( hCursorResult );

        sprintf( szCommand, "CLOSE %s", pszCursorName );

        hCursorResult = PQexec(hPGConn, szCommand);
        PQclear( hCursorResult );

        hCursorResult = PQexec(hPGConn, "COMMIT" );
        PQclear( hCursorResult );

        hCursorResult = NULL;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGLayer::GetNextFeature()

{

    for( ; TRUE; )
    {
        OGRFeature	*poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (poFilterGeom == NULL
            || bHasPostGISGeometry
            || poFilterGeom->Intersect( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRPGLayer::GetNextRawFeature()

{
    PGconn	*hPGConn = poDS->GetPGConn();
    char	szCommand[4096];

/* -------------------------------------------------------------------- */
/*      Do we need to establish an initial query?                       */
/* -------------------------------------------------------------------- */
    if( iNextShapeId == 0 )
    {
        char	*pszFields = BuildFields();

        poDS->FlushSoftTransaction();
        hCursorResult = PQexec(hPGConn, "BEGIN");
        PQclear( hCursorResult );

        sprintf( szCommand, 
                 "DECLARE %s CURSOR for "
                 "SELECT %s FROM %s "
                 "%s", 
                 pszCursorName, pszFields, 
                 poFeatureDefn->GetName(), pszWHERE );

        CPLFree( pszFields );

        CPLDebug( "OGR_PG", "PQexec(%s)\n", 
                  szCommand );

        hCursorResult = PQexec(hPGConn, szCommand );
        PQclear( hCursorResult );

        sprintf( szCommand, "FETCH %d in %s", CURSOR_PAGE, pszCursorName );
        hCursorResult = PQexec(hPGConn, szCommand );

        nResultOffset = 0;
    }

/* -------------------------------------------------------------------- */
/*      Are we in some sort of error condition?                         */
/* -------------------------------------------------------------------- */
    if( hCursorResult == NULL 
        || PQresultStatus(hCursorResult) != PGRES_TUPLES_OK )
    {
        iNextShapeId = MAX(1,iNextShapeId);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to fetch more records?                               */
/* -------------------------------------------------------------------- */
    if( nResultOffset >= PQntuples(hCursorResult) )
    {
        PQclear( hCursorResult );

        sprintf( szCommand, "FETCH %d in %s", CURSOR_PAGE, pszCursorName );
        hCursorResult = PQexec(hPGConn, szCommand );

        nResultOffset = 0;
    }

/* -------------------------------------------------------------------- */
/*      Are we out of results?  If so complete the transaction, and     */
/*      cleanup, but don't reset the next shapeid.                      */
/* -------------------------------------------------------------------- */
    if( nResultOffset >= PQntuples(hCursorResult) )
    {
        PQclear( hCursorResult );

        sprintf( szCommand, "CLOSE %s", pszCursorName );

        hCursorResult = PQexec(hPGConn, szCommand);
        PQclear( hCursorResult );

        hCursorResult = PQexec(hPGConn, "COMMIT" );
        PQclear( hCursorResult );

        hCursorResult = NULL;

        iNextShapeId = MAX(1,iNextShapeId);

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int		iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    if( EQUAL(PQfname(hCursorResult,0),"OGC_FID") )
    {
        poFeature->SetFID( atoi(PQgetvalue(hCursorResult,nResultOffset,0)) );
    }
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;

    for( iField = 0; 
         iField < PQnfields(hCursorResult);
         iField++ )
    {
        int	iOGRField;

        if( bHasPostGISGeometry
            && (EQUAL(PQfname(hCursorResult,iField),pszGeomColumn)
                || EQUAL(PQfname(hCursorResult,iField),"astext") ) )
        {
            char	*pszWKT;
            OGRGeometry *poGeometry = NULL;
            
            pszWKT = PQgetvalue( hCursorResult, nResultOffset, iField );
            OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeometry );
            if( poGeometry != NULL )
                poFeature->SetGeometryDirectly( poGeometry );

            continue;
        }
        else if( EQUAL(PQfname(hCursorResult,iField),"WKB_GEOMETRY") )
        {
            if( bWkbAsOid )
            {
                poFeature->SetGeometryDirectly( 
                    OIDToGeometry( (Oid) atoi(
                        PQgetvalue( hCursorResult, 
                                    nResultOffset, iField ) ) ) );
            }
            else
            {
                poFeature->SetGeometryDirectly( 
                    BYTEAToGeometry( 
                        PQgetvalue( hCursorResult, 
                                    nResultOffset, iField ) ) );
            }
            continue;
        }

        iOGRField = 
            poFeatureDefn->GetFieldIndex(PQfname(hCursorResult,iField));

        if( iOGRField < 0 )
            continue;

        poFeature->SetField( iOGRField, 
                             PQgetvalue( hCursorResult, 
                                         nResultOffset, iField ) );
    }

    nResultOffset++;

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGLayer::GetFeature( long nFeatureId )

{
    return NULL;
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRPGLayer::SetFeature( OGRFeature *poFeature )

{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          BYTEAToGeometry()                           */
/************************************************************************/

OGRGeometry *OGRPGLayer::BYTEAToGeometry( const char *pszBytea )

{
    GByte       *pabyWKB;
    int iSrc=0, iDst=0;
    OGRGeometry *poGeometry;

    if( pszBytea == NULL )
        return NULL;

    pabyWKB = (GByte *) CPLMalloc(strlen(pszBytea));
    while( pszBytea[iSrc] != '\0' )
    {
        if( pszBytea[iSrc] == '\\' )
        {
            if( pszBytea[iSrc+1] >= '0' && pszBytea[iSrc+1] <= '9' )
            {
                pabyWKB[iDst++] = 
                    (pszBytea[iSrc+1] - 48) * 64
                    + (pszBytea[iSrc+2] - 48) * 8
                    + (pszBytea[iSrc+3] - 48) * 1;
                iSrc += 4;
            }
            else
            {
                pabyWKB[iDst++] = pszBytea[iSrc+1];
                iSrc += 2;
            }
        }
        else
        {
            pabyWKB[iDst++] = pszBytea[iSrc++];
        }
    }

    poGeometry = NULL;
    OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeometry, iDst );

    CPLFree( pabyWKB );
    return poGeometry;
}

/************************************************************************/
/*                          GeometryToBYTEA()                           */
/************************************************************************/

char *OGRPGLayer::GeometryToBYTEA( OGRGeometry * poGeometry )

{
    int		nWkbSize = poGeometry->WkbSize();
    GByte	*pabyWKB;
    char	*pszTextBuf, *pszRetBuf;

    pabyWKB = (GByte *) CPLMalloc(nWkbSize);
    if( poGeometry->exportToWkb( wkbNDR, pabyWKB ) != OGRERR_NONE )
        return CPLStrdup("");

    pszTextBuf = (char *) CPLMalloc(nWkbSize*5+1);

    int  iSrc, iDst=0;

    for( iSrc = 0; iSrc < nWkbSize; iSrc++ )
    {
        if( pabyWKB[iSrc] < 40 || pabyWKB[iSrc] > 126
            || pabyWKB[iSrc] == '\\' )
        {
            sprintf( pszTextBuf+iDst, "\\\\%03o", pabyWKB[iSrc] );
            iDst += 5;
        }
        else
            pszTextBuf[iDst++] = pabyWKB[iSrc];
    }
    pszTextBuf[iDst] = '\0';

    pszRetBuf = CPLStrdup( pszTextBuf );
    CPLFree( pszTextBuf );

    return pszRetBuf;
}

/************************************************************************/
/*                          OIDToGeometry()                             */
/************************************************************************/

OGRGeometry *OGRPGLayer::OIDToGeometry( Oid oid )

{
    PGconn	*hPGConn = poDS->GetPGConn();
    GByte       *pabyWKB;
    int		fd, nBytes;
    OGRGeometry *poGeometry;

#define MAX_WKB	500000

    if( oid == 0 )
        return NULL;

    fd = lo_open( hPGConn, oid, INV_READ );
    if( fd < 0 )
        return NULL;

    pabyWKB = (GByte *) CPLMalloc(MAX_WKB);
    nBytes = lo_read( hPGConn, fd, (char *) pabyWKB, MAX_WKB );
    lo_close( hPGConn, fd );

    poGeometry = NULL;
    OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeometry, nBytes );

    CPLFree( pabyWKB );

    return poGeometry;
}

/************************************************************************/
/*                           GeometryToOID()                            */
/************************************************************************/

Oid OGRPGLayer::GeometryToOID( OGRGeometry * poGeometry )

{
    PGconn	*hPGConn = poDS->GetPGConn();
    int		nWkbSize = poGeometry->WkbSize();
    GByte	*pabyWKB;
    Oid		oid;
    int		fd, nBytesWritten;

    pabyWKB = (GByte *) CPLMalloc(nWkbSize);
    if( poGeometry->exportToWkb( wkbNDR, pabyWKB ) != OGRERR_NONE )
        return 0;

    oid = lo_creat( hPGConn, INV_READ|INV_WRITE );
    
    fd = lo_open( hPGConn, oid, INV_WRITE );
    nBytesWritten = lo_write( hPGConn, fd, (char *) pabyWKB, nWkbSize );
    lo_close( hPGConn, fd );

    if( nBytesWritten != nWkbSize )
    {
        CPLDebug( "OGR_PG", 
                  "Only wrote %d bytes of %d intended for (fd=%d,oid=%d).\n",
                  nBytesWritten, nWkbSize, fd, oid );
    }

    CPLFree( pabyWKB );
    
    return oid;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRPGLayer::CreateFeature( OGRFeature *poFeature )

{
    PGconn		*hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    char		*pszCommand;
    int                 i, bNeedComma;
    unsigned int        nCommandBufSize;;
    OGRErr              eErr;

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
        return eErr;

    nCommandBufSize = 40000;
    pszCommand = (char *) CPLMalloc(nCommandBufSize);

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.  					*/
/* -------------------------------------------------------------------- */
    sprintf( pszCommand, "INSERT INTO %s (", poFeatureDefn->GetName() );

    if( bHasWkb && poFeature->GetGeometryRef() != NULL )
        strcat( pszCommand, "WKB_GEOMETRY, " );
    
    if( bHasPostGISGeometry && poFeature->GetGeometryRef() != NULL )
    {
        strcat( pszCommand, pszGeomColumn );
        strcat( pszCommand, ", " );
    }
    
    bNeedComma = FALSE;
    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            strcat( pszCommand, ", " );

        strcat( pszCommand, poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
    }

    strcat( pszCommand, ") VALUES (" );

    /* Set the geometry */
    if( bHasPostGISGeometry )
    {
        char	*pszWKT = NULL;

        poFeature->GetGeometryRef()->exportToWkt( &pszWKT );
        
        if( pszWKT != NULL 
            && strlen(pszCommand) + strlen(pszWKT) > nCommandBufSize - 10000 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszWKT) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        if( pszWKT != NULL )
        {
            sprintf( pszCommand + strlen(pszCommand), 
                     "GeometryFromText('%s'::TEXT,-1), ", pszWKT );
            OGRFree( pszWKT );
        }
        else
            strcat( pszCommand, "''," );
    }
    else if( bHasWkb && !bWkbAsOid && poFeature->GetGeometryRef() != NULL )
    {
        char	*pszBytea = GeometryToBYTEA( poFeature->GetGeometryRef() );

        if( strlen(pszCommand) + strlen(pszBytea) > nCommandBufSize - 10000 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszBytea) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        if( pszBytea != NULL )
        {
            sprintf( pszCommand + strlen(pszCommand), 
                     "'%s', ", pszBytea );
            CPLFree( pszBytea );
        }
        else
            strcat( pszCommand, "''," );
    }
    else if( bHasWkb && bWkbAsOid && poFeature->GetGeometryRef() != NULL )
    {
        Oid	oid = GeometryToOID( poFeature->GetGeometryRef() );

        if( oid != 0 )
        {
            sprintf( pszCommand + strlen(pszCommand), 
                     "'%d', ", oid );
        }
        else
            strcat( pszCommand, "''," );
    }

    /* Set the other fields */
    int nOffset = strlen(pszCommand);

    bNeedComma = FALSE;
    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            strcat( pszCommand+nOffset, ", " );
        else
            bNeedComma = TRUE;

        if( strlen(pszStrValue) + strlen(pszCommand+nOffset) + nOffset 
            > nCommandBufSize-50 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszStrValue) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }
        
        if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTString )
        {
            int		iChar;

            /* We need to quote and escape string fields. */
            strcat( pszCommand+nOffset, "'" );

            nOffset += strlen(pszCommand+nOffset);
            
            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( pszStrValue[iChar] == '\\' 
                    || pszStrValue[iChar] == '\'' )
                {
                    pszCommand[nOffset++] = '\\';
                    pszCommand[nOffset++] = pszStrValue[iChar];
                }
                else
                    pszCommand[nOffset++] = pszStrValue[iChar];
            }
            pszCommand[nOffset] = '\0';
            
            strcat( pszCommand+nOffset, "'" );
        }
        else
        {
            strcat( pszCommand+nOffset, pszStrValue );
        }
    }

    strcat( pszCommand+nOffset, ")" );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, pszCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLDebug( "OGR_PG", "PQexec(%s)\n", pszCommand );
        CPLFree( pszCommand );

        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INSERT command for new feature failed.\n%s", 
                  PQerrorMessage(hPGConn) );

        PQclear( hResult );
        
        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }
    CPLFree( pszCommand );

#ifdef notdef
    /* Should we use this oid to get back the FID and assign back to the
       feature?  I think we are supposed to. */
    Oid	nNewOID = PQoidValue( hResult );
    printf( "nNewOID = %d\n", (int) nNewOID );
#endif
    PQclear( hResult );

    return poDS->SoftCommit();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRPGLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      Use a more brute force mechanism if we have a spatial query     */
/*      in play.                                                        */
/* -------------------------------------------------------------------- */
    if( poFilterGeom != NULL && !bHasPostGISGeometry )
        return OGRLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      In theory it might be wise to cache this result, but it         */
/*      won't be trivial to work out the lifetime of the value.         */
/*      After all someone else could be adding records from another     */
/*      application when working against a database.                    */
/* -------------------------------------------------------------------- */
    PGconn		*hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    char		szCommand[4096];
    int			nCount = 0;

    poDS->FlushSoftTransaction();
    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    sprintf( szCommand, 
             "DECLARE countCursor CURSOR for "
             "SELECT count(*) FROM %s "
             "%s",
             poFeatureDefn->GetName(), pszWHERE );

    CPLDebug( "OGR_PG", "PQexec(%s)\n", 
              szCommand );

    hResult = PQexec(hPGConn, szCommand);
    PQclear( hResult );

    hResult = PQexec(hPGConn, "FETCH ALL in countCursor");
    if( hResult != NULL && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        nCount = atoi(PQgetvalue(hResult,0,0));
    else
        CPLDebug( "OGR_PG", "%s; failed.", szCommand );
    PQclear( hResult );

    hResult = PQexec(hPGConn, "CLOSE countCursor");
    PQclear( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );
    
    return nCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return poFilterGeom == NULL || bHasPostGISGeometry;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;

    else if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    else 
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRPGLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
    PGconn		*hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    char		szCommand[1024];
    char		szFieldType[256];

/* -------------------------------------------------------------------- */
/*      Work out the PostgreSQL type.                                   */
/* -------------------------------------------------------------------- */
    if( poField->GetType() == OFTInteger )
    {
        strcpy( szFieldType, "INTEGER" );
    }
    else if( poField->GetType() == OFTReal )
    {
        strcpy( szFieldType, "FLOAT8" );
    }
    else if( poField->GetType() == OFTString )
    {
        if( poField->GetWidth() == 0 )
            strcpy( szFieldType, "VARCHAR" );
        else
            sprintf( szFieldType, "CHAR(%d)", poField->GetWidth() );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create fields of type %s on PostgreSQL layers.\n",
                  OGRFieldDefn::GetFieldTypeName(poField->GetType()) );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    poDS->FlushSoftTransaction();
    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    sprintf( szCommand, "ALTER TABLE %s ADD COLUMN %s %s", 
             poFeatureDefn->GetName(), poField->GetNameRef(), szFieldType );
    hResult = PQexec(hPGConn, szCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s\n%s", szCommand, PQerrorMessage(hPGConn) );

        PQclear( hResult );
        hResult = PQexec( hPGConn, "ROLLBACK" );
        PQclear( hResult );

        return OGRERR_FAILURE;
    }

    PQclear( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );

    poFeatureDefn->AddFieldDefn( poField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRPGLayer::StartTransaction()

{
    return poDS->SoftStartTransaction();
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRPGLayer::CommitTransaction()

{
    return poDS->SoftCommit();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRPGLayer::RollbackTransaction()

{
    return poDS->SoftRollback();
}

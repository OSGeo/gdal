/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.3  2002/05/09 17:09:08  warmerda
 * Ensure nSRSId is set before creating any features.
 *
 * Revision 1.2  2002/05/09 16:48:08  warmerda
 * upgrade to quote table and field names
 *
 * Revision 1.1  2002/05/09 16:03:46  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_pg.h"
#include <libpq/libpq-fs.h>

CPL_CVSID("$Id$");

#define CURSOR_PAGE	1

/************************************************************************/
/*                          OGRPGTableLayer()                           */
/************************************************************************/

OGRPGTableLayer::OGRPGTableLayer( OGRPGDataSource *poDSIn, 
                                  const char * pszTableName,
                                  int bUpdate, int nSRSIdIn )

{
    poDS = poDSIn;

    pszQuery = NULL;
    pszWHERE = CPLStrdup( "" );
    pszQueryStatement = NULL;
    
    bUpdateAccess = bUpdate;

    iNextShapeId = 0;

    nSRSId = nSRSIdIn;

    poFeatureDefn = ReadTableDefinition( pszTableName );
}

/************************************************************************/
/*                          ~OGRPGTableLayer()                          */
/************************************************************************/

OGRPGTableLayer::~OGRPGTableLayer()

{
    CPLFree( pszQuery );
    CPLFree( pszWHERE );
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

OGRFeatureDefn *OGRPGTableLayer::ReadTableDefinition( const char * pszTable )

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

void OGRPGTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

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

void OGRPGTableLayer::BuildWhere()

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
                 "WHERE %s && GeometryFromText('BOX3D(%.12f %.12f, %.12f %.12f)'::box3d,%d) ",
                 pszGeomColumn, 
                 sEnvelope.MinX, sEnvelope.MinY, 
                 sEnvelope.MaxX, sEnvelope.MaxY,
                 nSRSId );
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
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRPGTableLayer::BuildFullQueryStatement()

{
    if( pszQueryStatement != NULL )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = NULL;
    }

    char szCommand[6000];
    char *pszFields = BuildFields();

    sprintf( szCommand, 
             "SELECT %s FROM \"%s\" %s", 
             pszFields, poFeatureDefn->GetName(), pszWHERE );
    
    CPLFree( pszFields );

    pszQueryStatement = CPLStrdup( szCommand );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGTableLayer::ResetReading()

{
    BuildFullQueryStatement();

    OGRPGLayer::ResetReading();
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

char *OGRPGTableLayer::BuildFields()

{
    int		i, nSize;
    char	*pszFieldList;

    nSize = 25;
    if( pszGeomColumn )
        nSize += strlen(pszGeomColumn);

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 4;

    pszFieldList = (char *) CPLMalloc(nSize);

    if( pszGeomColumn )
    {
        if( bHasPostGISGeometry )
        {
            sprintf( pszFieldList, "AsText(\"%s\")", pszGeomColumn );
        }
        else
        {
            sprintf( pszFieldList, "\"%s\"", pszGeomColumn );
        }
    }
    else
        pszFieldList[0] = '\0';

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        strcat( pszFieldList, "\"" );
        strcat( pszFieldList, pszName );
        strcat( pszFieldList, "\"" );
    }

    CPLAssert( (int) strlen(pszFieldList) < nSize );

    return pszFieldList;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPGTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree( this->pszQuery );
    this->pszQuery = CPLStrdup( pszQuery );

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}


/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRPGTableLayer::SetFeature( OGRFeature *poFeature )

{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateFeature( OGRFeature *poFeature )

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

        sprintf( pszCommand + strlen(pszCommand), "\"%s\"",
                 poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
    }

    strcat( pszCommand, ") VALUES (" );

    /* Set the geometry */
    if( bHasPostGISGeometry )
    {
        char	*pszWKT = NULL;

        // Do we need to force nSRSId to be set?
        if( nSRSId == -2 )
            GetSpatialRef();

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
                     "GeometryFromText('%s'::TEXT,%d), ", pszWKT, nSRSId );
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
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else 
        return OGRPGLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

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

    sprintf( szCommand, "ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s", 
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
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRPGTableLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      Use a more brute force mechanism if we have a spatial query     */
/*      in play.                                                        */
/* -------------------------------------------------------------------- */
    if( poFilterGeom != NULL && !bHasPostGISGeometry )
        return OGRPGLayer::GetFeatureCount( bForce );

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
/*                           GetSpatialRef()                            */
/*                                                                      */
/*      We override this to try and fetch the table SRID from the       */
/*      geometry_columns table if the srsid is -2 (meaning we           */
/*      haven't yet even looked for it).                                */
/************************************************************************/

OGRSpatialReference *OGRPGTableLayer::GetSpatialRef()

{
    if( nSRSId == -2 )
    {
        PGconn		*hPGConn = poDS->GetPGConn();
        PGresult        *hResult;
        char		szCommand[1024];

        nSRSId = -1;

        poDS->SoftStartTransaction();

        sprintf( szCommand, 
                 "SELECT srid FROM geometry_columns "
                 "WHERE f_table_name = '%s'",
                 poFeatureDefn->GetName() );
        hResult = PQexec(hPGConn, szCommand );

        if( hResult 
            && PQresultStatus(hResult) == PGRES_TUPLES_OK 
            && PQntuples(hResult) == 1 )
        {
            nSRSId = atoi(PQgetvalue(hResult,0,0));
        }

        poDS->SoftCommit();
    }

    return OGRPGLayer::GetSpatialRef();
}

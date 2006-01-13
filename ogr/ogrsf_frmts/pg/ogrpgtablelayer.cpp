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
 * Revision 1.49  2006/01/13 13:21:57  fwarmerdam
 * Ensure that column widths for VARCHAR are used if available.  Bug 1037
 *
 * Revision 1.48  2005/12/05 11:34:22  dron
 * Handle 'double precision[]' arrays in ReadTableDefinition().
 *
 * Revision 1.47  2005/11/28 16:18:32  osemykin
 * TestCapability() modified for right result of SetFeature() and DeleteFeature() capabilities;
 * Added system variable PGSQL_OGR_FID support for detecting FID column
 *
 * Revision 1.46  2005/11/18 12:42:55  dron
 * Handle varchar arrays when reading feature definition fields.
 *
 * Revision 1.45  2005/10/24 23:50:50  fwarmerdam
 * moved bUseCopy test into layer on creation of first feature
 *
 * Revision 1.44  2005/10/24 21:01:09  fwarmerdam
 * added PG_PRE74 define for old copy functions
 *
 * Revision 1.43  2005/10/24 06:38:27  cfis
 * Added code to support COPY on postgresql 7.3 - however it currently commented out until the appropriate #ifdefs are defined.
 *
 * Revision 1.42  2005/10/19 02:49:56  cfis
 * When using COPY geometries were always sent to NULL when sending data to postgresql, even when a geometry existed.  This is now fixed.
 *
 * Revision 1.41  2005/10/16 08:33:50  cfis
 * Temporary fix for bug 962.  If the same table name is in two different schemas then the number of fields for each table will be doubled.  It its in three, then the number of fields will be tripled, etc.  The appropriate solution is to make ogr postgresql schema aware.
 *
 * Revision 1.40  2005/10/16 04:25:42  cfis
 * Use the bCopyActive flag instead of checking the data source.
 *
 * Revision 1.39  2005/10/16 03:39:25  fwarmerdam
 * cleanup COPY support somewhat
 *
 * Revision 1.38  2005/10/16 01:38:34  cfis
 * Updates that add support for using COPY for inserting data to Postgresql.  COPY is less robust than INSERT, but signficantly faster.
 *
 * Revision 1.37  2005/09/26 04:37:17  cfis
 * If inserting a feature into postgresql failed, the program would crash since the wrong number of parameters were sent to CPLError.  Fixed by passing pszCommand.
 *
 * Revision 1.36  2005/09/21 00:55:42  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.35  2005/08/17 16:25:39  fwarmerdam
 * Patched error output to include command if CreateFeature() fails.
 *
 * Revision 1.34  2005/08/06 14:49:27  osemykin
 * Added BINARY CURSOR support
 * Use it with 'PGB:dbname=...' instead 'PG:dbname=...'
 *
 * Revision 1.33  2005/08/05 13:37:40  fwarmerdam
 * Bug 902: set pszGeomColumn if WKB_GEOMETRY found.
 *
 * Revision 1.32  2005/08/04 07:28:28  osemykin
 * Fixes for support postgis version < 0.9.0
 * geom as EWKT for PostGIS >= 1.0
 * geom as Text for PostGIS < 1.0
 *
 * Revision 1.31  2005/08/03 21:27:32  osemykin
 * GetExtent fixes for old PostGIS version support
 *
 * Revision 1.30  2005/08/03 13:22:50  fwarmerdam
 * Patched GetExtent method to support BOX3D return types as reported
 * by Markus on gdal-dev.
 *
 * Revision 1.29  2005/07/20 01:45:30  fwarmerdam
 * improved geometry dimension support, PostGIS 8 EWKT upgrades
 *
 * Revision 1.28  2005/05/05 20:47:52  dron
 * Override GetExtent() method for PostGIS layers with PostGIS standard function
 * extent() (Oleg Semykin <oleg.semykin@gmail.com>
 *
 * Revision 1.27  2005/05/04 19:14:27  dron
 * Determine layer geometry type from Geometry_Columns standard OGC table.
 * Works only for PostGIS layers. (Oleg Semykin <oleg.semykin@gmail.com>)
 *
 * Revision 1.26  2005/04/29 17:08:58  dron
 * Move nSRSId checking into constructor. Change spatial filter query syntax
 * to match PostGIS 1.0.0 syntax (Oleg Semykin <oleg.semykin@gmail.com>)
 *
 * Revision 1.25  2005/02/22 12:54:05  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.24  2005/02/11 22:17:10  fwarmerdam
 * Applied fix for bug 681.  The truncation logic was kicking
 * inappropriately for integerlist and stringlist values.
 *
 * Revision 1.23  2004/11/17 17:49:27  fwarmerdam
 * implemented SetFeature and DeleteFeature methods
 *
 * Revision 1.22  2004/09/16 18:24:47  fwarmerdam
 * fixed trimming code, just truncate long text
 *
 * Revision 1.21  2004/07/10 04:47:40  warmerda
 * Ensure that setting a NULL (or empty) query reset pszQuery to NULL.
 * Use soft transactions more carefully.
 * Ensure that rings are closed on polygons.
 *
 * Revision 1.20  2004/07/09 18:36:14  warmerda
 * Fixed last fix ... put the varchar stuff in side quotes and didn't
 * address the case with no length set.
 *
 * Revision 1.19  2004/07/09 16:34:23  warmerda
 * Added patch from Markus to trim strings to allowed length.
 *
 * Revision 1.18  2004/05/08 02:14:49  warmerda
 * added GetFeature() on table, generalize FID support a bit
 *
 * Revision 1.17  2004/04/30 17:52:42  warmerda
 * added layer name laundering
 *
 * Revision 1.16  2004/04/30 00:47:31  warmerda
 * launder field name oid to oid_
 *
 * Revision 1.15  2004/03/17 06:53:28  warmerda
 * Make command string arbitrary length in BuildFullQueryStatement() as
 * per report from Stephen Frost.
 *
 * Revision 1.14  2003/09/11 20:03:36  warmerda
 * avoid warning
 *
 * Revision 1.13  2003/05/21 03:59:42  warmerda
 * expand tabs
 *
 * Revision 1.12  2003/02/01 07:55:48  warmerda
 * avoid dependence on libpq-fs.h
 *
 * Revision 1.11  2003/01/08 22:07:14  warmerda
 * Added support for integer and real list field types
 *
 * Revision 1.10  2002/12/12 14:29:28  warmerda
 * fixed bug with creating features with no geometry in PostGIS
 *
 * Revision 1.9  2002/10/20 03:45:53  warmerda
 * quote table name in feature insert, and feature count commands
 *
 * Revision 1.8  2002/10/09 18:30:10  warmerda
 * substantial upgrade to type handling, and preservations of width/precision
 *
 * Revision 1.7  2002/10/09 14:16:32  warmerda
 * changed the way that character field widths are extracted from catalog
 *
 * Revision 1.6  2002/10/04 14:03:09  warmerda
 * added column name laundering support
 *
 * Revision 1.5  2002/09/19 17:40:42  warmerda
 * Make initial ResetReading() call to set full query expression in constructor.
 *
 * Revision 1.4  2002/05/09 17:21:54  warmerda
 * Don't add trailing command if no fields to be inserted.
 *
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
#include "cpl_string.h"
#include "ogr_pg.h"

CPL_CVSID("$Id$");

#define CURSOR_PAGE     1
#define USE_COPY_UNSET  -10

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

    ResetReading();

    bLaunderColumnNames = TRUE;
    bCopyActive = FALSE;

    // check SRID if it's necessary
    if( nSRSId == -2 )
        GetSpatialRef();

    bUseCopy = USE_COPY_UNSET;  // unknown
}

//************************************************************************/
/*                          ~OGRPGTableLayer()                          */
/************************************************************************/

OGRPGTableLayer::~OGRPGTableLayer()

{
    EndCopy();

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
    char                szCommand[1024];
    PGconn              *hPGConn = poDS->GetPGConn();
    char                szPrimaryKey[256];

    poDS->FlushSoftTransaction();

    /* -------------------------------------------- */
    /*          Detect table primary key            */
    /* -------------------------------------------- */

    /* -------------------------------------------- */
    /*          Check config options                */
    /* -------------------------------------------- */
    sprintf( szPrimaryKey, "%s", CPLGetConfigOption( "PGSQL_OGR_FID", "ogc_fid" ) );

    /* TODO make changes corresponded to Frank issues
    sprintf ( szCommand,
              "SELECT a.attname "
              "FROM pg_attribute a, pg_constraint c, pg_class cl "
              "WHERE c.contype='p' AND c.conrelid=cl.oid "
              "AND a.attnum = c.conkey[1] AND a.attrelid=cl.oid "
              "AND cl.relname = '%s'",
              pszTable );

    hResult = PQexec(hPGConn, szCommand );

    if ( hResult && PQntuples( hResult ) == 1 && PQgetisnull( hResult,0,0 ) == false )
    {
        sprintf( szPrimaryKey, "%s", PQgetvalue(hResult,0,0) );
        CPLDebug( "OGR_PG", "Primary key name (FID): %s", szPrimaryKey );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage(hPGConn) );
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to detect table primary key. Use default 'ogc_fid'");
    }*/

/* -------------------------------------------------------------------- */
/*      Fire off commands to get back the schema of the table.          */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );
        sprintf( szCommand,
                 "DECLARE mycursor CURSOR for "
                 "SELECT DISTINCT a.attname, t.typname, a.attlen,"
                 "       format_type(a.atttypid,a.atttypmod) "
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
    int            iRecord;

    poDefn->Reference();

    for( iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
    {
        const char      *pszType, *pszFormatType;
        OGRFieldDefn    oField( PQgetvalue( hResult, iRecord, 0 ), OFTString);

        pszType = PQgetvalue(hResult, iRecord, 1 );
        pszFormatType = PQgetvalue(hResult,iRecord,3);

        /* TODO: Add detection of other primary key to use as FID */
        if( EQUAL(oField.GetNameRef(),szPrimaryKey) )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            CPLDebug("OGR_PG","Using column '%s' as FID for table '%s'", pszFIDColumn, pszTable );
            continue;
        }
        else if( EQUAL(pszType,"geometry") )
        {
            bHasPostGISGeometry = TRUE;
            pszGeomColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
        else if( EQUAL(oField.GetNameRef(),"WKB_GEOMETRY") )
        {
            bHasWkb = TRUE;
            pszGeomColumn = CPLStrdup(oField.GetNameRef());
            if( EQUAL(pszType,"OID") )
                bWkbAsOid = TRUE;
            continue;
        }

        if( EQUAL(pszType,"text") )
        {
            oField.SetType( OFTString );
        }
        else if( EQUAL(pszFormatType,"character varying[]") )
        {
            oField.SetType( OFTStringList );
        }
        else if( EQUAL(pszType,"bpchar") || EQUAL(pszType,"varchar") )
        {
            int nWidth;

            nWidth = atoi(PQgetvalue(hResult,iRecord,2));
            if( nWidth == -1 )
            {
                if( EQUALN(pszFormatType,"character(",10) )
                    nWidth = atoi(pszFormatType+10);
                else if( EQUALN(pszFormatType,"character varying(",18) )
                    nWidth = atoi(pszFormatType+18);
                else
                    nWidth = 0;
            }
            oField.SetType( OFTString );
            oField.SetWidth( nWidth );
        }
        else if( EQUAL(pszType,"numeric") )
        {
            const char *pszFormatName = PQgetvalue(hResult,iRecord,3);
            const char *pszPrecision = strstr(pszFormatName,",");
            int    nWidth, nPrecision = 0;

            nWidth = atoi(pszFormatName + 8);
            if( pszPrecision != NULL )
                nPrecision = atoi(pszPrecision+1);

            if( nPrecision == 0 )
                oField.SetType( OFTInteger );
            else
                oField.SetType( OFTReal );

            oField.SetWidth( nWidth );
            oField.SetPrecision( nPrecision );
        }
        else if( EQUAL(pszFormatType,"integer[]") )
        {
            oField.SetType( OFTIntegerList );
        }
        else if( EQUAL(pszFormatType, "float[]")
                 || EQUAL(pszFormatType, "double precision[]") )
        {
            oField.SetType( OFTRealList );
        }
        else if( EQUAL(pszType,"int2") )
        {
            oField.SetType( OFTInteger );
            oField.SetWidth( 5 );
        }
        else if( EQUALN(pszType,"int",3) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"float",5) || EQUALN(pszType,"double",6) )
        {
            oField.SetType( OFTReal );
        }
        else if( EQUAL(pszType, "date") )
        {
            oField.SetType( OFTString );
            oField.SetWidth( 10 );
        }
        else if( EQUAL(pszType, "time") )
        {
            oField.SetType( OFTString );
            oField.SetWidth( 8 );
        }

        poDefn->AddFieldDefn( &oField );
    }

    PQclear( hResult );

    hResult = PQexec(hPGConn, "CLOSE mycursor");
    PQclear( hResult );

    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );

    // get layer geometry type (for PostGIS dataset)
    if ( bHasPostGISGeometry )
    {
        sprintf(szCommand,
                "SELECT type, coord_dimension FROM geometry_columns WHERE f_table_name='%s'",
                pszTable);

        hResult = PQexec(hPGConn,szCommand);
        if ( hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult,0,0) )
        {
            char * pszType = PQgetvalue(hResult,0,0);
            OGRwkbGeometryType nGeomType = wkbUnknown;

            nCoordDimension = MAX(2,MIN(3,atoi(PQgetvalue(hResult,0,1))));

            // check only standard OGC geometry types
            if ( EQUAL(pszType, "POINT") )
                nGeomType = wkbPoint;
            else if ( EQUAL(pszType,"LINESTRING"))
                nGeomType = wkbLineString;
            else if ( EQUAL(pszType,"POLYGON"))
                nGeomType = wkbPolygon;
            else if ( EQUAL(pszType,"MULTIPOINT"))
                nGeomType = wkbMultiPoint;
            else if ( EQUAL(pszType,"MULTILINESTRING"))
                nGeomType = wkbMultiLineString;
            else if ( EQUAL(pszType,"MULTIPOLYGON"))
                nGeomType = wkbMultiPolygon;
            else if ( EQUAL(pszType,"GEOMETRYCOLLECTION"))
                nGeomType = wkbGeometryCollection;

            if( nCoordDimension == 3 && nGeomType != wkbUnknown )
                nGeomType = (OGRwkbGeometryType) (nGeomType | wkb25DBit);

            CPLDebug("OGR_PG","Layer '%s' geometry type: %s:%s, Dim=%d",
                     pszTable, pszType, OGRGeometryTypeToName(nGeomType),
                     nCoordDimension );

            poDefn->SetGeomType( nGeomType );

            PQclear( hResult );
        }
    }

    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPGTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( !InstallFilter( poGeomIn ) )
        return;

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
    char        szWHERE[4096];

    CPLFree( pszWHERE );
    pszWHERE = NULL;

    szWHERE[0] = '\0';

    if( m_poFilterGeom != NULL && bHasPostGISGeometry )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        sprintf( szWHERE,
                 "WHERE %s && SetSRID('BOX3D(%.12f %.12f, %.12f %.12f)'::box3d,%d) ",
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

    char *pszFields = BuildFields();

    pszQueryStatement = (char *)
        CPLMalloc(strlen(pszFields)+strlen(pszWHERE)
                  +strlen(poFeatureDefn->GetName()) + 40);
    sprintf( pszQueryStatement,
             "SELECT %s FROM \"%s\" %s",
             pszFields, poFeatureDefn->GetName(), pszWHERE );

    CPLFree( pszFields );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGTableLayer::ResetReading()

{
    bUseCopy = USE_COPY_UNSET;

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
    int         i, nSize;
    char        *pszFieldList;

    nSize = 25;
    if( pszGeomColumn )
        nSize += strlen(pszGeomColumn);

    if( bHasFid )
        nSize += strlen(pszFIDColumn);

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 4;

    pszFieldList = (char *) CPLMalloc(nSize);
    pszFieldList[0] = '\0';

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
        sprintf( pszFieldList, "\"%s\"", pszFIDColumn );

    if( pszGeomColumn )
    {
        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        if( bHasPostGISGeometry )
        {
            if ( poDS->bUseBinaryCursor )
            {
                nSize += 10;
                sprintf( pszFieldList+strlen(pszFieldList),
                         "AsBinary(\"%s\")", pszGeomColumn );
            }
            else
            if ( poDS->sPostGISVersion.nMajor >= 1 )
                sprintf( pszFieldList+strlen(pszFieldList),
                        "AsEWKT(\"%s\")", pszGeomColumn );
            else
                sprintf( pszFieldList+strlen(pszFieldList),
                        "AsText(\"%s\")", pszGeomColumn );
        }
        else
        {
            sprintf( pszFieldList+strlen(pszFieldList),
                     "\"%s\"", pszGeomColumn );
        }
    }

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

    if( pszQuery == NULL || strlen(pszQuery) == 0 )
        this->pszQuery = NULL;
    else
        this->pszQuery = CPLStrdup( pszQuery );

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRPGTableLayer::DeleteFeature( long nFID )

{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    char                *pszCommand;

/* -------------------------------------------------------------------- */
/*      We can only delete features if we have a well defined FID       */
/*      column to target.                                               */
/* -------------------------------------------------------------------- */
    if( !bHasFid )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature(%d) failed.  Unable to delete features in tables without\n"
                  "a recognised FID column.",
                  nFID );
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Form the statement to drop the record.                          */
/* -------------------------------------------------------------------- */
    pszCommand = (char *) CPLMalloc( strlen(pszFIDColumn)
                                     + strlen(poFeatureDefn->GetName())
                                     + 100 );

    sprintf( pszCommand, "DELETE FROM \"%s\" WHERE \"%s\" = %ld",
             poFeatureDefn->GetName(), pszFIDColumn, nFID );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
        return eErr;

    CPLDebug( "OGR_PG", "PQexec(%s)\n", pszCommand );

    hResult = PQexec(hPGConn, pszCommand);
    CPLFree( pszCommand );

    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature() DELETE statement failed.\n%s",
                  PQerrorMessage(hPGConn) );

        PQclear( hResult );

        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }

    return poDS->SoftCommit();
}

/************************************************************************/
/*                             SetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by dropping the old copy of the     */
/*      feature in question (if there is one) and then creating a       */
/*      new one with the provided feature id.                           */
/************************************************************************/

OGRErr OGRPGTableLayer::SetFeature( OGRFeature *poFeature )

{
    OGRErr eErr;

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }

    eErr = DeleteFeature( poFeature->GetFID() );
    if( eErr != OGRERR_NONE )
        return eErr;

    return CreateFeature( poFeature );
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/
OGRErr OGRPGTableLayer::CreateFeature( OGRFeature *poFeature )
{
    // We avoid testing the config option too often. 
    if( bUseCopy == USE_COPY_UNSET )
        bUseCopy = CSLTestBoolean( CPLGetConfigOption( "PG_USE_COPY", "NO") );

    if( !bUseCopy )
    {
        return CreateFeatureViaInsert( poFeature );
    }
    else
    {
        if ( !bCopyActive )
            StartCopy();

        return CreateFeatureViaCopy( poFeature );
    }
}

/************************************************************************/
/*                       CreateFeatureViaInsert()                       */
/************************************************************************/
OGRErr OGRPGTableLayer::CreateFeatureViaInsert( OGRFeature *poFeature )

{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    char                *pszCommand;
    int                 i, bNeedComma = FALSE;
    unsigned int        nCommandBufSize;;
    OGRErr              eErr;

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
        return eErr;

    nCommandBufSize = 40000;
    pszCommand = (char *) CPLMalloc(nCommandBufSize);

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    sprintf( pszCommand, "INSERT INTO \"%s\" (", poFeatureDefn->GetName() );

    if( bHasWkb && poFeature->GetGeometryRef() != NULL )
    {
        strcat( pszCommand, "WKB_GEOMETRY " );
        bNeedComma = TRUE;
    }

    if( bHasPostGISGeometry && poFeature->GetGeometryRef() != NULL )
    {
        strcat( pszCommand, pszGeomColumn );
        strcat( pszCommand, " " );
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            strcat( pszCommand, ", " );
        strcat( pszCommand, pszFIDColumn );
        strcat( pszCommand, " " );
        bNeedComma = TRUE;
    }

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
    bNeedComma = poFeature->GetGeometryRef() != NULL;
    if( bHasPostGISGeometry && poFeature->GetGeometryRef() != NULL)
    {
        char    *pszWKT = NULL;

        if( poFeature->GetGeometryRef() != NULL )
        {
            OGRGeometry *poGeom = (OGRGeometry *) poFeature->GetGeometryRef();

            poGeom->closeRings();
            poGeom->setCoordinateDimension( nCoordDimension );

            poGeom->exportToWkt( &pszWKT );
        }

        if( pszWKT != NULL
            && strlen(pszCommand) + strlen(pszWKT) > nCommandBufSize - 10000 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszWKT) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        if( pszWKT != NULL )
        {
            if( poDS->sPostGISVersion.nMajor >= 1 )
                sprintf( pszCommand + strlen(pszCommand),
                         "GeomFromEWKT('SRID=%d;%s'::TEXT) ", nSRSId, pszWKT );
            else
                sprintf( pszCommand + strlen(pszCommand),
                         "GeometryFromText('%s'::TEXT,%d) ", pszWKT, nSRSId );
            OGRFree( pszWKT );
        }
        else
            strcat( pszCommand, "''" );
    }
    else if( bHasWkb && !bWkbAsOid && poFeature->GetGeometryRef() != NULL )
    {
        char    *pszBytea = GeometryToBYTEA( poFeature->GetGeometryRef() );

        if( strlen(pszCommand) + strlen(pszBytea) > nCommandBufSize - 10000 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszBytea) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        if( pszBytea != NULL )
        {
            sprintf( pszCommand + strlen(pszCommand),
                     "'%s'", pszBytea );
            CPLFree( pszBytea );
        }
        else
            strcat( pszCommand, "''" );
    }
    else if( bHasWkb && bWkbAsOid && poFeature->GetGeometryRef() != NULL )
    {
        Oid     oid = GeometryToOID( poFeature->GetGeometryRef() );

        if( oid != 0 )
        {
            sprintf( pszCommand + strlen(pszCommand),
                     "'%d' ", oid );
        }
        else
            strcat( pszCommand, "''" );
    }

    /* Set the FID */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            strcat( pszCommand, ", " );
        sprintf( pszCommand + strlen(pszCommand), "%ld ", poFeature->GetFID());
        bNeedComma = TRUE;
    }

    /* Set the other fields */
    int nOffset = strlen(pszCommand);

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            strcat( pszCommand+nOffset, ", " );
        else
            bNeedComma = TRUE;

        // We need special formatting for integer list values.
        if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTIntegerList )
        {
            int nCount, nOff = 0, j;
            const int *panItems = poFeature->GetFieldAsIntegerList(i,&nCount);

            pszNeedToFree = (char *) CPLMalloc(nCount * 13 + 10);
            strcpy( pszNeedToFree, "{" );
            for( j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += strlen(pszNeedToFree+nOff);
                sprintf( pszNeedToFree+nOff, "%d", panItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        // We need special formatting for real list values.
        if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTRealList )
        {
            int nCount, nOff = 0, j;
            const double *padfItems =poFeature->GetFieldAsDoubleList(i,&nCount);

            pszNeedToFree = (char *) CPLMalloc(nCount * 40 + 10);
            strcpy( pszNeedToFree, "{" );
            for( j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += strlen(pszNeedToFree+nOff);
                sprintf( pszNeedToFree+nOff, "%.16g", padfItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        // Grow the command buffer?
        if( strlen(pszStrValue) + strlen(pszCommand+nOffset) + nOffset
            > nCommandBufSize-50 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszStrValue) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal )
        {
            int         iChar;

            /* We need to quote and escape string fields. */
            strcat( pszCommand+nOffset, "'" );

            nOffset += strlen(pszCommand+nOffset);

            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTIntegerList
                    && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTRealList
                    && poFeatureDefn->GetFieldDefn(i)->GetWidth() > 0
                    && iChar == poFeatureDefn->GetFieldDefn(i)->GetWidth() )
                {
                    CPLDebug( "PG",
                              "Truncated %s field value, it was too long.",
                              poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
                    break;
                }

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

        if( pszNeedToFree )
            CPLFree( pszNeedToFree );
    }

    strcat( pszCommand+nOffset, ")" );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, pszCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLDebug( "OGR_PG", "PQexec(%s)\n", pszCommand );

        CPLError( CE_Failure, CPLE_AppDefined,
                  "INSERT command for new feature failed.\n%s\nCommand: %s",
                  PQerrorMessage(hPGConn), pszCommand);

        CPLFree( pszCommand );

        PQclear( hResult );

        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }
    CPLFree( pszCommand );

#ifdef notdef
    /* Should we use this oid to get back the FID and assign back to the
       feature?  I think we are supposed to. */
    Oid nNewOID = PQoidValue( hResult );
    printf( "nNewOID = %d\n", (int) nNewOID );
#endif
    PQclear( hResult );

    return poDS->SoftCommit();
}

/************************************************************************/
/*                        CreateFeatureViaCopy()                        */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateFeatureViaCopy( OGRFeature *poFeature )
{
    int nCommandBufSize = 4000;

    /* First process geometry */
    OGRGeometry *poGeometry = (OGRGeometry *) poFeature->GetGeometryRef();
    
    char *pszGeom = NULL;
    if ( poGeometry )
    {
        poGeometry->closeRings();
        poGeometry->setCoordinateDimension( nCoordDimension );

        pszGeom = GeometryToHex( poGeometry, nSRSId );
        nCommandBufSize = nCommandBufSize + strlen(pszGeom);
    }

    char *pszCommand = (char *) CPLMalloc(nCommandBufSize);

    if ( poGeometry )
    {
        sprintf( pszCommand, "%s", pszGeom);
        CPLFree( pszGeom );
    }
    else
    {
        sprintf( pszCommand, "\\N");
    }
    strcat( pszCommand, "\t" );


    /* Next process the field id column */
    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) != -1 )
    {
        /* Set the FID */
        if( poFeature->GetFID() != OGRNullFID )
        {
            sprintf( pszCommand + strlen(pszCommand), "%ld ", poFeature->GetFID());
        }
        else
	    {
	        strcat( pszCommand, "\\N" );
        }

        strcat( pszCommand, "\t" );
    }


    /* Now process the remaining fields */
    int nOffset = strlen(pszCommand);

    int nFieldCount = poFeatureDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount;  i++ )
    {
        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if( !poFeature->IsFieldSet( i ) )
        {
            strcat( pszCommand, "\\N" );

            if( i < nFieldCount - 1 )
                strcat( pszCommand, "\t" );

            continue;
        }

        // We need special formatting for integer list values.
        if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTIntegerList )
        {
            int nCount, nOff = 0, j;
            const int *panItems = poFeature->GetFieldAsIntegerList(i,&nCount);

            pszNeedToFree = (char *) CPLMalloc(nCount * 13 + 10);
            strcpy( pszNeedToFree, "{" );
            for( j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += strlen(pszNeedToFree+nOff);
                sprintf( pszNeedToFree+nOff, "%d", panItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        // We need special formatting for real list values.
        if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTRealList )
        {
            int nCount, nOff = 0, j;
            const double *padfItems =poFeature->GetFieldAsDoubleList(i,&nCount);

            pszNeedToFree = (char *) CPLMalloc(nCount * 40 + 10);
            strcpy( pszNeedToFree, "{" );
            for( j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += strlen(pszNeedToFree+nOff);
                sprintf( pszNeedToFree+nOff, "%.16g", padfItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        // Grow the command buffer?
        if( (int) (strlen(pszStrValue) + strlen(pszCommand+nOffset) + nOffset)
            > nCommandBufSize-50 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszStrValue) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal )
        {
            int         iChar;

            nOffset += strlen(pszCommand+nOffset);

            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTIntegerList
                    && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTRealList
                    && poFeatureDefn->GetFieldDefn(i)->GetWidth() > 0
                    && iChar == poFeatureDefn->GetFieldDefn(i)->GetWidth() )
                {
                    CPLDebug( "PG",
                              "Truncated %s field value, it was too long.",
                              poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
                    break;
                }

                /* Escape embedded \, \t, \n, \r since they will cause COPY
                   to misinterpret a line of text and thus abort */
                if( pszStrValue[iChar] == '\\' || 
                    pszStrValue[iChar] == '\t' || 
                    pszStrValue[iChar] == '\r' || 
                    pszStrValue[iChar] == '\n'   )
                {
                    pszCommand[nOffset++] = '\\';
                }

                pszCommand[nOffset++] = pszStrValue[iChar];
            }

            pszCommand[nOffset] = '\0';
//            strcat( pszCommand+nOffset, "'" );
        }
        else
        {
            strcat( pszCommand+nOffset, pszStrValue );
        }

        if( pszNeedToFree )
            CPLFree( pszNeedToFree );

        if( i < nFieldCount - 1 )
            strcat( pszCommand, "\t" );
    }

    /* Add end of line marker */
    strcat( pszCommand, "\n" );


    /* ------------------------------------------------------------ */
    /*      Execute the copy.                                       */
    /* ------------------------------------------------------------ */
    PGconn *hPGConn = poDS->GetPGConn();

    OGRErr result = OGRERR_NONE;

    /* This is for postgresql  7.4 and higher */
#if !defined(PG_PRE74)
    int copyResult = PQputCopyData(hPGConn, pszCommand, strlen(pszCommand));

    switch (copyResult)
    {
    case 0:
        CPLDebug( "OGR_PG", "PQexec(%s)\n", pszCommand );
        CPLError( CE_Failure, CPLE_AppDefined, "Writing COPY data blocked.");
        result = OGRERR_FAILURE;
        break;
    case -1:
        CPLDebug( "OGR_PG", "PQexec(%s)\n", pszCommand );
        CPLError( CE_Failure, CPLE_AppDefined, "%s", PQerrorMessage(hPGConn) );
        result = OGRERR_FAILURE;
        break;
    }
#else /* else defined(PG_PRE74) */
    int copyResult = PQputline(hPGConn, pszCommand);

    if (copyResult == EOF)
    {
      CPLDebug( "OGR_PG", "PQexec(%s)\n", pszCommand );
      CPLError( CE_Failure, CPLE_AppDefined, "Writing COPY data blocked.");
      result = OGRERR_FAILURE;
    }  
#endif /* end of defined(PG_PRE74) */

    /* Free the buffer we allocated before returning */
    CPLFree( pszCommand );

    return result;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGTableLayer::TestCapability( const char * pszCap )

{
    if ( bUpdateAccess )
    {
        if( EQUAL(pszCap,OLCSequentialWrite) || EQUAL(pszCap,OLCCreateField) )
            return TRUE;

        else if( EQUAL(pszCap,OLCRandomRead) || EQUAL(pszCap,OLCRandomWrite) )
            return bHasFid;
    }

    return OGRPGLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateField( OGRFieldDefn *poFieldIn, int bApproxOK )

{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    char                szCommand[1024];
    char                szFieldType[256];
    OGRFieldDefn        oField( poFieldIn );

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );

        if( EQUAL(oField.GetNameRef(),"oid") )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Renaming field 'oid' to 'oid_' to avoid conflict with internal oid field." );
            oField.SetName( "oid_" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Work out the PostgreSQL type.                                   */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "NUMERIC(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetWidth() > 0 && oField.GetPrecision() > 0
            && bPreservePrecision )
            sprintf( szFieldType, "NUMERIC(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "FLOAT8" );
    }
    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
            strcpy( szFieldType, "VARCHAR" );
        else
            sprintf( szFieldType, "CHAR(%d)", oField.GetWidth() );
    }
    else if( oField.GetType() == OFTIntegerList )
    {
        strcpy( szFieldType, "INTEGER[]" );
    }
    else if( oField.GetType() == OFTRealList )
    {
        strcpy( szFieldType, "FLOAT8[]" );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on PostgreSQL layers.  Creating as VARCHAR.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "VARCHAR" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on PostgreSQL layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    poDS->FlushSoftTransaction();
    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    sprintf( szCommand, "ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s",
             poFeatureDefn->GetName(), oField.GetNameRef(), szFieldType );
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

    poFeatureDefn->AddFieldDefn( &oField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGTableLayer::GetFeature( long nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Discard any existing resultset.                                 */
/* -------------------------------------------------------------------- */
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Issue query for a single record.                                */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = NULL;
    PGresult    *hResult;
    PGconn      *hPGConn = poDS->GetPGConn();
    char        *pszFieldList = BuildFields();
    char        *pszCommand = (char *) CPLMalloc(strlen(pszFieldList)+2000);

    poDS->FlushSoftTransaction();
    poDS->SoftStartTransaction();

    sprintf( pszCommand,
             "DECLARE getfeaturecursor CURSOR for "
             "SELECT %s FROM \"%s\" WHERE %s = %ld",
             pszFieldList, poFeatureDefn->GetName(), pszFIDColumn,
             nFeatureId );
    CPLFree( pszFieldList );

    hResult = PQexec(hPGConn, pszCommand );
    CPLFree( pszCommand );

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );
        hResult = PQexec(hPGConn, "FETCH ALL in getfeaturecursor" );

        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        {
            hCursorResult = hResult;
            poFeature = RecordToFeature( 0 );
            hCursorResult = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    PQclear( hResult );

    hResult = PQexec(hPGConn, "CLOSE getfeaturecursor");
    PQclear( hResult );

    poDS->FlushSoftTransaction();


    return poFeature;
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
    if( m_poFilterGeom != NULL && !bHasPostGISGeometry )
        return OGRPGLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      In theory it might be wise to cache this result, but it         */
/*      won't be trivial to work out the lifetime of the value.         */
/*      After all someone else could be adding records from another     */
/*      application when working against a database.                    */
/* -------------------------------------------------------------------- */
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    char                szCommand[4096];
    int                 nCount = 0;

    poDS->FlushSoftTransaction();
    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    sprintf( szCommand,
             "DECLARE countCursor CURSOR for "
             "SELECT count(*) FROM \"%s\" "
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
        PGconn          *hPGConn = poDS->GetPGConn();
        PGresult        *hResult;
        char            szCommand[1024];

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
        PQclear( hResult );

        poDS->SoftCommit();
    }

    return OGRPGLayer::GetSpatialRef();
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      For PostGIS use internal Extend(geometry) function              */
/*      in other cases we use standard OGRLayer::GetExtent()            */
/************************************************************************/

OGRErr OGRPGTableLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if ( psExtent == NULL )
        return OGRERR_FAILURE;

    if ( bHasPostGISGeometry )
    {
        PGconn          *hPGConn = poDS->GetPGConn();
        PGresult        *hResult;
        char            szCommand[1024];

        sprintf( szCommand, "SELECT Extent(\"%s\") FROM \"%s\"", pszGeomColumn, poFeatureDefn->GetName() );

        hResult = PQexec( hPGConn, szCommand );
        if( ! hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK || PQgetisnull(hResult,0,0) )
        {
            PQclear( hResult );
            CPLDebug("OGR_PG","Unable to get extent by PostGIS. Using standard OGRLayer method.");
            return OGRPGLayer::GetExtent( psExtent, bForce );
        }

        char * pszBox = PQgetvalue(hResult,0,0);
        char * ptr = pszBox;
        char szVals[64*6+6];

        while ( *ptr != '(' && ptr ) ptr++; ptr++;

        strncpy(szVals,ptr,strstr(ptr,")") - ptr);
        szVals[strstr(ptr,")") - ptr] = '\0';

        char ** papszTokens = CSLTokenizeString2(szVals," ,",CSLT_HONOURSTRINGS);
        int nTokenCnt = poDS->sPostGISVersion.nMajor >= 1 ? 4 : 6;

        if ( CSLCount(papszTokens) != nTokenCnt )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "Bad extent representation: '%s'", pszBox);
            CSLDestroy(papszTokens);
            PQclear(hResult);
            return OGRERR_FAILURE;
        }

        // Take X,Y coords
        // For PostGis ver >= 1.0.0 -> Tokens: X1 Y1 X2 Y2 (nTokenCnt = 4)
        // For PostGIS ver < 1.0.0 -> Tokens: X1 Y1 Z1 X2 Y2 Z2 (nTokenCnt = 6)
        // =>   X2 index calculated as nTokenCnt/2
        //      Y2 index caluclated as nTokenCnt/2+1
        
        psExtent->MinX = CPLScanDouble(papszTokens[0],strlen(papszTokens[0]),"C");
        psExtent->MinY = CPLScanDouble(papszTokens[1],strlen(papszTokens[1]),"C");
        psExtent->MaxX = CPLScanDouble(papszTokens[nTokenCnt/2],strlen(papszTokens[nTokenCnt/2]),"C");
        psExtent->MaxY = CPLScanDouble(papszTokens[nTokenCnt/2+1],strlen(papszTokens[nTokenCnt/2+1]),"C");

        CSLDestroy(papszTokens);

        PQclear( hResult );
        return OGRERR_NONE;
    }

    return OGRLayer::GetExtent( psExtent, bForce );
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

OGRErr OGRPGTableLayer::StartCopy()

{
    OGRErr result = OGRERR_NONE;

    /* Tell the datasource we are now planning to copy data */
    poDS->StartCopy( this ); 

    char *pszFields = BuildCopyFields();

    int size = strlen(pszFields) +  strlen(poFeatureDefn->GetName()) + 100;
    char *pszCommand = (char *) CPLMalloc(size);

    sprintf( pszCommand,
             "COPY \"%s\" (%s) FROM STDIN;",
             poFeatureDefn->GetName(), pszFields );

    CPLFree( pszFields );

    PGconn *hPGConn = poDS->GetPGConn();
    CPLDebug( "OGR_PG", "%s", pszCommand );
    PGresult *hResult = PQexec(hPGConn, pszCommand);

    if ( !hResult || (PQresultStatus(hResult) != PGRES_COPY_IN))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage(hPGConn) );
        result = OGRERR_FAILURE;
    }
    else
        bCopyActive = TRUE;

    PQclear( hResult );
    CPLFree( pszCommand );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/

OGRErr OGRPGTableLayer::EndCopy()

{
    if( !bCopyActive )
        return OGRERR_NONE;

    /* This method is called from the datasource when
       a COPY operation is ended */
    OGRErr result = OGRERR_NONE;

    PGconn *hPGConn = poDS->GetPGConn();
    CPLDebug( "OGR_PG", "PQputCopyEnd()" );

    bCopyActive = FALSE;


    /* This is for postgresql 7.4 and higher */
#if !defined(PG_PRE74)
    int copyResult = PQputCopyEnd(hPGConn, NULL);

    switch (copyResult)
    {
      case 0:
        CPLError( CE_Failure, CPLE_AppDefined, "Writing COPY data blocked.");
        result = OGRERR_FAILURE;
        break;
      case -1:
        CPLError( CE_Failure, CPLE_AppDefined, "%s", PQerrorMessage(hPGConn) );
        result = OGRERR_FAILURE;
        break;
    }

#else /* defined(PG_PRE74) */
    PQputline(hPGConn, "\\.\n");
    int copyResult = PQendcopy(hPGConn);

    if (copyResult != 0)
    {
      CPLError( CE_Failure, CPLE_AppDefined, "%s", PQerrorMessage(hPGConn) );
      result = OGRERR_FAILURE;
    }
#endif /* defined(PG_PRE74) */

    /* Now check the results of the copy */
    PGresult * hResult = PQgetResult( hPGConn );

    if( hResult && PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "COPY statement failed.\n%s",
                  PQerrorMessage(hPGConn) );

        result = OGRERR_FAILURE;
    }

    PQclear(hResult);

    bUseCopy = USE_COPY_UNSET;

    return result;
}

/************************************************************************/
/*                          BuildCopyFields()                           */
/************************************************************************/

char *OGRPGTableLayer::BuildCopyFields()
{
    int         i, nSize;
    char        *pszFieldList;
        
    nSize = 25;
    if( pszGeomColumn )
        nSize += strlen(pszGeomColumn);

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) != -1 )
        nSize += strlen(pszFIDColumn);

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 4;

    pszFieldList = (char *) CPLMalloc(nSize);
    pszFieldList[0] = '\0';

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) != -1 )
        sprintf( pszFieldList, "\"%s\"", pszFIDColumn );

    if( pszGeomColumn )
    {
        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        sprintf( pszFieldList+strlen(pszFieldList),
                 "\"%s\"", pszGeomColumn );
    }

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

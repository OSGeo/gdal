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
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"
#include "ogr_pg.h"

CPL_CVSID("$Id$");

#define CURSOR_PAGE     1
#define USE_COPY_UNSET  -10

/************************************************************************/
/*                          OGRPGTableLayer()                           */
/************************************************************************/

OGRPGTableLayer::OGRPGTableLayer( OGRPGDataSource *poDSIn,
                                  const char * pszTableNameIn,
                                  const char * pszSchemaNameIn,
                                  int bUpdate, int nSRSIdIn )

{
    poDS = poDSIn;

    pszQueryStatement = NULL;

    bUpdateAccess = bUpdate;

    iNextShapeId = 0;

    nSRSId = nSRSIdIn;

    pszTableName = CPLStrdup( pszTableNameIn );
    if (pszSchemaNameIn)
      pszSchemaName = CPLStrdup( pszSchemaNameIn );
    else
      pszSchemaName = NULL;

    pszSqlTableName = (char*)CPLMalloc(255); //set in ReadTableDefinition

    poFeatureDefn = ReadTableDefinition( pszTableName, pszSchemaName );

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
    CPLFree( pszSqlTableName );
    CPLFree( pszTableName );
    CPLFree( pszSchemaName );
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

OGRFeatureDefn *OGRPGTableLayer::ReadTableDefinition( const char * pszTableIn,
                                                      const char * pszSchemaNameIn )

{
    PGresult            *hResult;
    CPLString           osCommand;
    CPLString           osPrimaryKey;
    CPLString           osCurrentSchema;
    PGconn              *hPGConn = poDS->GetPGConn();

    poDS->FlushSoftTransaction();

    /* -------------------------------------------- */
    /*          Detect table primary key            */
    /* -------------------------------------------- */

    /* -------------------------------------------- */
    /*          Check config options                */
    /* -------------------------------------------- */
    osPrimaryKey = CPLGetConfigOption( "PGSQL_OGR_FID", "ogc_fid" );

    // 
    /* -------------------------------------------- */
    /*          Get the current schema              */
    /* -------------------------------------------- */
    hResult = PQexec(hPGConn,"SELECT current_schema()");
    if ( hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult,0,0) )
    {
        osCurrentSchema = PQgetvalue(hResult,0,0);

        PQclear( hResult );
    }

    /* TODO make changes corresponded to Frank issues
       
    sprintf ( szCommand,
              "SELECT a.attname "
              "FROM pg_attribute a, pg_constraint c, pg_class cl "
              "WHERE c.contype='p' AND c.conrelid=cl.oid "
              "AND a.attnum = c.conkey[1] AND a.attrelid=cl.oid "
              "AND cl.relname = '%s'",
              pszTableIn );

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
/*      Fire off commands to get back the columns of the table.          */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );

        CPLString osSchemaClause;
        if (pszSchemaNameIn)
          osSchemaClause.Printf("AND n.nspname='%s'", pszSchemaNameIn);

        osCommand.Printf(
                 "DECLARE mycursor CURSOR for "
                 "SELECT DISTINCT a.attname, t.typname, a.attlen,"
                 "       format_type(a.atttypid,a.atttypmod) "
                 "FROM pg_class c, pg_attribute a, pg_type t, pg_namespace n "
                 "WHERE c.relname = '%s' "
                 "AND a.attnum > 0 AND a.attrelid = c.oid "
                 "AND a.atttypid = t.oid "
                 "AND c.relnamespace=n.oid "
                 "%s",
                 pszTableIn, osSchemaClause.c_str());

        hResult = PQexec(hPGConn, osCommand.c_str() );
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
    char szLayerName[256];
    if ( pszSchemaNameIn && osCurrentSchema != pszSchemaNameIn )
    {
        sprintf( szLayerName, "%s.%s", pszSchemaNameIn, pszTableIn );
        sprintf( pszSqlTableName, "%s.\"%s\"", pszSchemaNameIn, pszTableIn );
    }
    else
    {
        //no prefix for current_schema in layer name, for backwards compatibility
        strcpy( szLayerName, pszTableIn );
        sprintf( pszSqlTableName, "\"%s\"", pszTableIn );
    }

    OGRFeatureDefn *poDefn = new OGRFeatureDefn( szLayerName );
    int            iRecord;

    poDefn->Reference();

    for( iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
    {
        const char      *pszType, *pszFormatType;
        OGRFieldDefn    oField( PQgetvalue( hResult, iRecord, 0 ), OFTString);

        pszType = PQgetvalue(hResult, iRecord, 1 );
        pszFormatType = PQgetvalue(hResult,iRecord,3);

        /* TODO: Add detection of other primary key to use as FID */
        if( EQUAL(oField.GetNameRef(),osPrimaryKey) )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            CPLDebug("OGR_PG","Using column '%s' as FID for table '%s'", pszFIDColumn, pszTableIn );
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
        else if( EQUALN(pszType, "timestamp",9) )
        {
            oField.SetType( OFTDateTime );
        }
        else if( EQUALN(pszType, "date",4) )
        {
            oField.SetType( OFTDate );
        }
        else if( EQUALN(pszType, "time",4) )
        {
            oField.SetType( OFTTime );
        }
        else
        {
            CPLDebug( "PG", "Field %s is of unknown type %s.", 
                      oField.GetNameRef(), pszType );
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
        osCommand.Printf(
            "SELECT type, coord_dimension FROM geometry_columns WHERE f_table_name='%s'",
            pszTableIn);

        hResult = PQexec(hPGConn,osCommand);
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
                     pszTableIn, pszType, OGRGeometryTypeToName(nGeomType),
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
    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRPGTableLayer::BuildWhere()

{
    osWHERE = "";

    if( m_poFilterGeom != NULL && bHasPostGISGeometry )
    {
        CPLDebug( "PG", "bHasPostGISGeometry == TRUE" );

        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        osWHERE.Printf("WHERE %s && SetSRID('BOX3D(%.12f %.12f, %.12f %.12f)'::box3d,%d) ",
                       pszGeomColumn,
                       sEnvelope.MinX, sEnvelope.MinY,
                       sEnvelope.MaxX, sEnvelope.MaxY,
                       nSRSId );
    }

    if( strlen(osQuery) > 0 )
    {
        if( strlen(osWHERE) == 0 )
        {
            osWHERE.Printf( "WHERE %s ", osQuery.c_str()  );
        }
        else	
        {
            osWHERE += "AND ";
            osWHERE += osQuery;
        }
    }

    // XXX - mloskot - some debugging logic, can be removed
    if( bHasPostGISGeometry )
        CPLDebug( "PG", "OGRPGTableLayer::BuildWhere returns: %s",
                  osWHERE.c_str() );
    else
        CPLDebug( "PG", "PostGIS is NOT available!" );
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
        CPLMalloc(strlen(pszFields)+strlen(osWHERE)
                  +strlen(pszSqlTableName) + 40);
    sprintf( pszQueryStatement,
             "SELECT %s FROM %s %s",
             pszFields, pszSqlTableName, osWHERE.c_str() );

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
    if( pszQuery == NULL )
        osQuery = "";
    else
        osQuery = pszQuery;

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
    CPLString            osCommand;

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
    osCommand.Printf( "DELETE FROM %s WHERE \"%s\" = %ld",
                      pszSqlTableName, pszFIDColumn, nFID );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
        return eErr;

    CPLDebug( "OGR_PG", "PQexec(%s)\n", osCommand.c_str() );

    hResult = PQexec(hPGConn, osCommand);

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
    OGRErr eErr(OGRERR_FAILURE);

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to SetFeature()." );
        return eErr;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return eErr;
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
    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

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
    PGresult            *hResult = NULL;
    CPLString           osCommand;
    int                 i = 0;
    int                 bNeedComma = FALSE;
    OGRErr              eErr = OGRERR_FAILURE;
    
    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeatureViaInsert()." );
        return eErr;
    }

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
    {
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO %s (", pszSqlTableName );

    if( bHasWkb && poFeature->GetGeometryRef() != NULL )
    {
        osCommand += "WKB_GEOMETRY ";
        bNeedComma = TRUE;
    }

    if( bHasPostGISGeometry && poFeature->GetGeometryRef() != NULL )
    {
        osCommand = osCommand + pszGeomColumn + " ";
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        
        osCommand = osCommand + pszFIDColumn + " ";
        bNeedComma = TRUE;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            osCommand += ", ";

        osCommand = osCommand 
            + "\"" + poFeatureDefn->GetFieldDefn(i)->GetNameRef() + "\"";
    }

    osCommand += ") VALUES (";

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

        if( pszWKT != NULL )
        {
            if( poDS->sPostGISVersion.nMajor >= 1 )
                osCommand +=
                    CPLString().Printf(
                        "GeomFromEWKT('SRID=%d;%s'::TEXT) ", nSRSId, pszWKT );
            else
                osCommand += 
                    CPLString().Printf(
                        "GeometryFromText('%s'::TEXT,%d) ", pszWKT, nSRSId );
            OGRFree( pszWKT );
        }
        else
            osCommand += "''";
    }
    else if( bHasWkb && !bWkbAsOid && poFeature->GetGeometryRef() != NULL )
    {
        char    *pszBytea = GeometryToBYTEA( poFeature->GetGeometryRef() );

        if( pszBytea != NULL )
        {
            osCommand = osCommand + "'" + pszBytea + "'";
            CPLFree( pszBytea );
        }
        else
            osCommand += "''";
    }
    else if( bHasWkb && bWkbAsOid && poFeature->GetGeometryRef() != NULL )
    {
        Oid     oid = GeometryToOID( poFeature->GetGeometryRef() );

        if( oid != 0 )
        {
            osCommand += CPLString().Printf( "'%d' ", oid );
        }
        else
            osCommand += "''";
    }

    /* Set the FID */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( "%ld ", poFeature->GetFID() );
        bNeedComma = TRUE;
    }


    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        // Flag indicating NULL or not-a-date date value
        // e.g. 0000-00-00 - there is no year 0
        OGRBoolean bIsDateNull = FALSE;

        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
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

        // Check if date is NULL: 0000-00-00
        if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDate )
        {
            if( EQUALN( pszStrValue, "0000", 4 ) )
            {
                pszStrValue = "NULL";
                bIsDateNull = TRUE;
            }
        }

        if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
            && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal
            && !bIsDateNull )
        {
            int         iChar;

            /* We need to quote and escape string fields. */
            osCommand += "'";

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
                    osCommand += '\\';
                    osCommand += pszStrValue[iChar];
                }
                else
                {
                    osCommand += pszStrValue[iChar];
                }
            }

            osCommand += "'";
        }
        else
        {
            osCommand += pszStrValue;
        }

        if( pszNeedToFree )
            CPLFree( pszNeedToFree );
    }

    osCommand += ")";

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    hResult = PQexec(hPGConn, osCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLDebug( "OGR_PG", "PQexec(%s)\n", osCommand.c_str() );

        CPLError( CE_Failure, CPLE_AppDefined,
                  "INSERT command for new feature failed.\n%s\nCommand: %s",
                  PQerrorMessage(hPGConn), osCommand.c_str() );

        PQclear( hResult );

        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }

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
    if ( NULL != poGeometry )
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
    CPLString           osCommand;
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
    else if( oField.GetType() == OFTDate )
    {
        strcpy( szFieldType, "date" );
    }
    else if( oField.GetType() == OFTTime )
    {
        strcpy( szFieldType, "time" );
    }
    else if( oField.GetType() == OFTDateTime )
    {
        strcpy( szFieldType, "timestamp with time zone" );
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

    osCommand.Printf( "ALTER TABLE %s ADD COLUMN \"%s\" %s",
                      pszSqlTableName, oField.GetNameRef(), szFieldType );
    hResult = PQexec(hPGConn, osCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s\n%s", 
                  osCommand.c_str(), 
                  PQerrorMessage(hPGConn) );

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
             "SELECT %s FROM %s WHERE %s = %ld",
             pszFieldList, pszSqlTableName, pszFIDColumn,
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
    CPLString           osCommand;
    int                 nCount = 0;

    poDS->FlushSoftTransaction();
    hResult = PQexec(hPGConn, "BEGIN");
    PQclear( hResult );

    osCommand.Printf(
        "DECLARE countCursor CURSOR for "
        "SELECT count(*) FROM %s "
        "%s",
        pszSqlTableName, osWHERE.c_str() );

    CPLDebug( "OGR_PG", "PQexec(%s)\n",
              osCommand.c_str() );

    hResult = PQexec(hPGConn, osCommand);
    PQclear( hResult );

    hResult = PQexec(hPGConn, "FETCH ALL in countCursor");
    if( hResult != NULL && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        nCount = atoi(PQgetvalue(hResult,0,0));
    else
        CPLDebug( "OGR_PG", "%s; failed.", osCommand.c_str() );
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
                 "WHERE f_table_name = '%s' AND f_table_schema = '%s'",
                 pszTableName, pszSchemaName );
        hResult = PQexec(hPGConn, szCommand );

        if( hResult
            && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) == 1 )
        {
            nSRSId = atoi(PQgetvalue(hResult,0,0));
        }
        else // I think perhaps an older version used f_schema_name.
        {
            PQclear( hResult );
            poDS->SoftCommit();
            poDS->SoftStartTransaction();
            sprintf( szCommand,
                     "SELECT srid FROM geometry_columns "
                     "WHERE f_table_name = '%s' AND f_schema_name = '%s'",
                     pszTableName, pszSchemaName );
            hResult = PQexec(hPGConn, szCommand );
            if( hResult
                && PQresultStatus(hResult) == PGRES_TUPLES_OK
                && PQntuples(hResult) == 1 )
            {
                nSRSId = atoi(PQgetvalue(hResult,0,0));
            }
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
        CPLString       osCommand;

        osCommand.Printf( "SELECT Extent(\"%s\") FROM %s", 
                          pszGeomColumn, pszSqlTableName );

        hResult = PQexec( hPGConn, osCommand );
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

    int size = strlen(pszFields) +  strlen(pszSqlTableName) + 100;
    char *pszCommand = (char *) CPLMalloc(size);

    sprintf( pszCommand,
             "COPY %s (%s) FROM STDIN;",
             pszSqlTableName, pszFields );

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

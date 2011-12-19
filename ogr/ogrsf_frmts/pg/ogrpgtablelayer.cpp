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

#include "ogr_pg.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"

#define PQexec this_is_an_error

CPL_CVSID("$Id$");

#define USE_COPY_UNSET  -10
static CPLString OGRPGEscapeStringList(PGconn *hPGConn,
                                       char** papszItems, int bForInsertOrUpdate);

/************************************************************************/
/*                          OGRPGTableLayer()                           */
/************************************************************************/

OGRPGTableLayer::OGRPGTableLayer( OGRPGDataSource *poDSIn,
                                  CPLString& osCurrentSchema,
                                  const char * pszTableNameIn,
                                  const char * pszSchemaNameIn,
                                  const char * pszGeomColumnIn,
                                  int bUpdate,
                                  int bAdvertizeGeomColumn,
                                  int nSRSIdIn )

{
    poDS = poDSIn;

    pszQueryStatement = NULL;

    bUpdateAccess = bUpdate;

    iNextShapeId = 0;

    nSRSId = nSRSIdIn;
    nGeomType = wkbUnknown;
    bGeometryInformationSet = FALSE;

    bLaunderColumnNames = TRUE;
    bPreservePrecision = TRUE;
    bCopyActive = FALSE;
    bUseCopy = USE_COPY_UNSET;  // unknown

    pszTableName = CPLStrdup( pszTableNameIn );
    if (pszGeomColumnIn)
        pszGeomColumn = CPLStrdup(pszGeomColumnIn);
    if (pszSchemaNameIn)
        pszSchemaName = CPLStrdup( pszSchemaNameIn );
    else if (strlen(osCurrentSchema))
        pszSchemaName = CPLStrdup( osCurrentSchema );
    else
        pszSchemaName = NULL;

    pszSqlGeomParentTableName = NULL;

    bHasWarnedIncompatibleGeom = FALSE;
    bHasWarnedAlreadySetFID = FALSE;

    /* Just in provision for people yelling about broken backward compatibility ... */
    bRetrieveFID = CSLTestBoolean(CPLGetConfigOption("OGR_PG_RETRIEVE_FID", "TRUE"));

/* -------------------------------------------------------------------- */
/*      Build the layer defn name.                                      */
/* -------------------------------------------------------------------- */
    if ( pszSchemaNameIn && osCurrentSchema != pszSchemaNameIn )
    {
        /* For backwards compatibility, don't report the geometry column name */
        /* if it's wkb_geometry */
        if (bAdvertizeGeomColumn && pszGeomColumnIn)
            osDefnName.Printf( "%s.%s(%s)", pszSchemaNameIn, pszTableName, pszGeomColumnIn );
        else
            osDefnName.Printf("%s.%s", pszSchemaNameIn, pszTableName );
        pszSqlTableName = CPLStrdup(CPLString().Printf("%s.%s",
                               OGRPGEscapeColumnName(pszSchemaNameIn).c_str(),
                               OGRPGEscapeColumnName(pszTableName).c_str() ));
    }
    else
    {
        //no prefix for current_schema in layer name, for backwards compatibility
        /* For backwards compatibility, don't report the geometry column name */
        /* if it's wkb_geometry */
        if (bAdvertizeGeomColumn && pszGeomColumnIn)
            osDefnName.Printf( "%s(%s)", pszTableName, pszGeomColumnIn );
        else
            osDefnName = pszTableName;
        pszSqlTableName = CPLStrdup(OGRPGEscapeColumnName(pszTableName));
    }

    osPrimaryKey = CPLGetConfigOption( "PGSQL_OGR_FID", "ogc_fid" );
}

//************************************************************************/
/*                          ~OGRPGTableLayer()                          */
/************************************************************************/

OGRPGTableLayer::~OGRPGTableLayer()

{
    EndCopy();
    CPLFree( pszSqlTableName );
    CPLFree( pszTableName );
    CPLFree( pszSqlGeomParentTableName );
    CPLFree( pszSchemaName );
}

/************************************************************************/
/*                      SetGeometryInformation()                        */
/************************************************************************/

void  OGRPGTableLayer::SetGeometryInformation(const char* pszType,
                                               int nCoordDimension,
                                               int nSRID,
                                               PostgisType ePostgisType)
{
    if (pszType == NULL || nCoordDimension == 0 || nSRID == UNDETERMINED_SRID ||
        ePostgisType == GEOM_TYPE_UNKNOWN)
        return;

    bGeometryInformationSet = TRUE;

    nGeomType = OGRFromOGCGeomType(pszType);

    this->nCoordDimension = nCoordDimension;
    this->nSRSId = nSRID;

    if( nCoordDimension == 3 && nGeomType != wkbUnknown )
        nGeomType = (OGRwkbGeometryType) (nGeomType | wkb25DBit);

    if( ePostgisType == GEOM_TYPE_GEOMETRY)
        bHasPostGISGeometry = TRUE;
    else if( ePostgisType == GEOM_TYPE_GEOGRAPHY)
        bHasPostGISGeography = TRUE;

    CPLDebug("PG","Layer '%s' geometry type: %s:%s, Dim=%d",
                pszTableName, pszType, OGRGeometryTypeToName(nGeomType),
                nCoordDimension );
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType  OGRPGTableLayer::GetGeomType()
{
    if (bGeometryInformationSet)
        return nGeomType;

    return GetLayerDefn()->GetGeomType();
}
    
/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

OGRFeatureDefn *OGRPGTableLayer::ReadTableDefinition()

{
    PGresult            *hResult;
    CPLString           osCommand;
    PGconn              *hPGConn = poDS->GetPGConn();

    poDS->FlushSoftTransaction();

    CPLString osSchemaClause;
    if( pszSchemaName )
        osSchemaClause.Printf("AND n.nspname='%s'", pszSchemaName);

    const char* pszTypnameEqualsAnyClause;
    if (poDS->sPostgreSQLVersion.nMajor == 7 && poDS->sPostgreSQLVersion.nMinor <= 3)
        pszTypnameEqualsAnyClause = "ANY(SELECT '{int2, int4, serial}')";
    else
        pszTypnameEqualsAnyClause = "ANY(ARRAY['int2','int4','serial'])";

    CPLString osEscapedTableNameSingleQuote = OGRPGEscapeString(hPGConn, pszTableName, -1, "");
    const char* pszEscapedTableNameSingleQuote = osEscapedTableNameSingleQuote.c_str();

    /* See #1889 for why we don't use 'AND a.attnum = ANY(i.indkey)' */
    osCommand.Printf("SELECT a.attname, a.attnum, t.typname, "
              "t.typname = %s AS isfid "
              "FROM pg_class c, pg_attribute a, pg_type t, pg_namespace n, pg_index i "
              "WHERE a.attnum > 0 AND a.attrelid = c.oid "
              "AND a.atttypid = t.oid AND c.relnamespace = n.oid "
              "AND c.oid = i.indrelid AND i.indisprimary = 't' "
              "AND t.typname !~ '^geom' AND c.relname = %s "
              "AND (i.indkey[0]=a.attnum OR i.indkey[1]=a.attnum OR i.indkey[2]=a.attnum "
              "OR i.indkey[3]=a.attnum OR i.indkey[4]=a.attnum OR i.indkey[5]=a.attnum "
              "OR i.indkey[6]=a.attnum OR i.indkey[7]=a.attnum OR i.indkey[8]=a.attnum "
              "OR i.indkey[9]=a.attnum) %s ORDER BY a.attnum",
              pszTypnameEqualsAnyClause, pszEscapedTableNameSingleQuote, osSchemaClause.c_str() );
     
    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

    if ( hResult && PGRES_TUPLES_OK == PQresultStatus(hResult) )
    {
        if ( PQntuples( hResult ) == 1 && PQgetisnull( hResult,0,0 ) == false )
        {
            /* Check if single-field PK can be represented as 32-bit integer. */
            CPLString osValue(PQgetvalue(hResult, 0, 3));
            if( osValue == "t" )
            {
                osPrimaryKey.Printf( "%s", PQgetvalue(hResult,0,0) );
                CPLDebug( "PG", "Primary key name (FID): %s", osPrimaryKey.c_str() );
            }
        }
        else if ( PQntuples( hResult ) > 1 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Multi-column primary key in \'%s\' detected but not supported.",
                      pszTableName );
        }

        OGRPGClearResult( hResult );
        /* Zero tuples means no PK is defined, perfectly valid case. */
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage(hPGConn) );
    }

/* -------------------------------------------------------------------- */
/*      Fire off commands to get back the columns of the table.          */
/* -------------------------------------------------------------------- */
    hResult = OGRPG_PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        OGRPGClearResult( hResult );

        osCommand.Printf(
                 "DECLARE mycursor CURSOR for "
                 "SELECT DISTINCT a.attname, t.typname, a.attlen,"
                 "       format_type(a.atttypid,a.atttypmod), a.attnum "
                 "FROM pg_class c, pg_attribute a, pg_type t, pg_namespace n "
                 "WHERE c.relname = %s "
                 "AND a.attnum > 0 AND a.attrelid = c.oid "
                 "AND a.atttypid = t.oid "
                 "AND c.relnamespace=n.oid "
                 "%s"
                 "ORDER BY a.attnum",
                 pszEscapedTableNameSingleQuote, osSchemaClause.c_str());

        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );
    }

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        OGRPGClearResult( hResult );
        hResult = OGRPG_PQexec(hPGConn, "FETCH ALL in mycursor" );
    }

    if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
    {
        OGRPGClearResult( hResult );

        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage(hPGConn) );
        return NULL;
    }

    if( PQntuples(hResult) == 0 )
    {
        OGRPGClearResult( hResult );

        hResult = OGRPG_PQexec(hPGConn, "CLOSE mycursor");
        OGRPGClearResult( hResult );

        hResult = OGRPG_PQexec(hPGConn, "COMMIT");
        OGRPGClearResult( hResult );

        CPLError( CE_Failure, CPLE_AppDefined,
                  "No field definitions found for '%s', is it a table?",
                  pszTableName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( osDefnName );
    int            iRecord;

    poDefn->Reference();

    for( iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
    {
        const char      *pszType = NULL;
        const char      *pszFormatType = NULL;
        OGRFieldDefn    oField( PQgetvalue( hResult, iRecord, 0 ), OFTString);

        pszType = PQgetvalue(hResult, iRecord, 1 );
        pszFormatType = PQgetvalue(hResult,iRecord,3);

        if( EQUAL(oField.GetNameRef(),osPrimaryKey) )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            CPLDebug("PG","Using column '%s' as FID for table '%s'", pszFIDColumn, pszTableName );
            continue;
        }
        else if( EQUAL(pszType,"geometry") )
        {
            bHasPostGISGeometry = TRUE;
            if (!pszGeomColumn)
                pszGeomColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
        else if( EQUAL(pszType,"geography") )
        {
            bHasPostGISGeography = TRUE;
            if (!pszGeomColumn)
                pszGeomColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
        else if( EQUAL(oField.GetNameRef(),"WKB_GEOMETRY") )
        {
            if (!pszGeomColumn)
            {
                bHasWkb = TRUE;
                pszGeomColumn = CPLStrdup(oField.GetNameRef());
                if( EQUAL(pszType,"OID") )
                    bWkbAsOid = TRUE;
            }
            continue;
        }

        if( EQUAL(pszType,"text") )
        {
            oField.SetType( OFTString );
        }
        else if( EQUAL(pszType,"_bpchar") ||
                 EQUAL(pszType,"_varchar") ||
                 EQUAL(pszType,"_text"))
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
        else if( EQUAL(pszType,"bool") )
        {
            oField.SetType( OFTInteger );
            oField.SetWidth( 1 );
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
            {
                // FIXME : If nWidth > 10, OFTInteger may not be large enough */
                oField.SetType( OFTInteger );
            }
            else
                oField.SetType( OFTReal );

            oField.SetWidth( nWidth );
            oField.SetPrecision( nPrecision );
        }
        else if( EQUAL(pszFormatType,"integer[]") )
        {
            oField.SetType( OFTIntegerList );
        }
        else if( EQUAL(pszFormatType, "float[]") ||
                 EQUAL(pszFormatType, "real[]") ||
                 EQUAL(pszFormatType, "double precision[]") )
        {
            oField.SetType( OFTRealList );
        }
        else if( EQUAL(pszType,"int2") )
        {
            oField.SetType( OFTInteger );
            oField.SetWidth( 5 );
        }
        else if( EQUAL(pszType,"int8") )
        {
            /* FIXME: OFTInteger can not handle 64bit integers */
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"int",3) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"float",5) ||
                 EQUALN(pszType,"double",6) ||
                 EQUAL(pszType,"real") )
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
        else if( EQUAL(pszType,"bytea") )
        {
            oField.SetType( OFTBinary );
        }

        else
        {
            CPLDebug( "PG", "Field %s is of unknown format type %s (type=%s).", 
                      oField.GetNameRef(), pszFormatType, pszType );
        }

        poDefn->AddFieldDefn( &oField );
    }

    OGRPGClearResult( hResult );

    hResult = OGRPG_PQexec(hPGConn, "CLOSE mycursor");
    OGRPGClearResult( hResult );

    hResult = OGRPG_PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    /* If geometry type, SRID, etc... have always been set by SetGeometryInformation() */
    /* no need to issue a new SQL query. Just record the geom type in the layer definition */
    if (bGeometryInformationSet)
    {
        ;
    }
    // get layer geometry type (for PostGIS dataset)
    else if ( bHasPostGISGeometry || bHasPostGISGeography )
    {
      /* Get the geometry type and dimensions from the table, or */
      /* from its parents if it is a derived table, or from the parent of the parent, etc.. */
      int bGoOn = TRUE;

      while(bGoOn)
      {
        osCommand.Printf(
            "SELECT type, coord_dimension%s FROM %s WHERE f_table_name='%s'",
            (nSRSId == UNDETERMINED_SRID) ? ", srid" : "",
            (bHasPostGISGeometry) ? "geometry_columns" : "geography_columns",
            (pszSqlGeomParentTableName) ? pszSqlGeomParentTableName : pszTableName);
        if (pszGeomColumn)
        {
            osCommand += CPLString().Printf(" AND %s='%s'",
                (bHasPostGISGeometry) ? "f_geometry_column" : "f_geography_column",
                pszGeomColumn);
        }
        if (pszSchemaName)
        {
            osCommand += CPLString().Printf(" AND f_table_schema='%s'", pszSchemaName);
        }

        hResult = OGRPG_PQexec(hPGConn,osCommand);

        if ( hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult,0,0) )
        {
            char * pszType = PQgetvalue(hResult,0,0);

            nCoordDimension = MAX(2,MIN(3,atoi(PQgetvalue(hResult,0,1))));

            if (nSRSId == UNDETERMINED_SRID)
                nSRSId = atoi(PQgetvalue(hResult,0,2));

            SetGeometryInformation(pszType, nCoordDimension, nSRSId,
                                   (bHasPostGISGeometry) ? GEOM_TYPE_GEOMETRY : GEOM_TYPE_GEOGRAPHY);

            bGoOn = FALSE;
        }
        else
        {
            CPLString osEscapedTableNameSingleQuote = OGRPGEscapeString(hPGConn,
                    (pszSqlGeomParentTableName) ? pszSqlGeomParentTableName : pszTableName, -1, "");
            const char* pszEscapedTableNameSingleQuote = osEscapedTableNameSingleQuote.c_str();

            /* Fetch the name of the parent table */
            if (pszSchemaName)
            {
                osCommand.Printf("SELECT pg_class.relname FROM pg_class WHERE oid = "
                                "(SELECT pg_inherits.inhparent FROM pg_inherits WHERE inhrelid = "
                                "(SELECT c.oid FROM pg_class c, pg_namespace n WHERE c.relname = %s AND c.relnamespace=n.oid AND n.nspname = '%s'))",
                                pszEscapedTableNameSingleQuote, pszSchemaName );
            }
            else
            {
                osCommand.Printf("SELECT pg_class.relname FROM pg_class WHERE oid = "
                                "(SELECT pg_inherits.inhparent FROM pg_inherits WHERE inhrelid = "
                                "(SELECT pg_class.oid FROM pg_class WHERE relname = %s))",
                                pszEscapedTableNameSingleQuote );
            }

            OGRPGClearResult( hResult );
            hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

            if ( hResult && PQntuples( hResult ) == 1 && !PQgetisnull( hResult,0,0 ) )
            {
                CPLFree(pszSqlGeomParentTableName);
                pszSqlGeomParentTableName = CPLStrdup( PQgetvalue(hResult,0,0) );
            }
            else
            {
                /* No more parent : stop recursion */
                bGoOn = FALSE;
            }
        }

        OGRPGClearResult( hResult );
      }

      if (nSRSId == UNDETERMINED_SRID)
          nSRSId = poDS->GetUndefinedSRID();
    }
    else if (pszGeomColumn == NULL)
    {
        nGeomType = wkbNone;
    }

    poDefn->SetGeomType( nGeomType );

    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPGTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    GetLayerDefn();

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

    if( m_poFilterGeom != NULL && (bHasPostGISGeometry || bHasPostGISGeography) )
    {
        char szBox3D_1[128];
        char szBox3D_2[128];
        char* pszComma;
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        snprintf(szBox3D_1, sizeof(szBox3D_1), "%.12f %.12f", sEnvelope.MinX, sEnvelope.MinY);
        while((pszComma = strchr(szBox3D_1, ',')) != NULL)
            *pszComma = '.';
        snprintf(szBox3D_2, sizeof(szBox3D_2), "%.12f %.12f", sEnvelope.MaxX, sEnvelope.MaxY);
        while((pszComma = strchr(szBox3D_2, ',')) != NULL)
            *pszComma = '.';
        osWHERE.Printf("WHERE %s && SetSRID('BOX3D(%s, %s)'::box3d,%d) ",
                       OGRPGEscapeColumnName(pszGeomColumn).c_str(),
                       szBox3D_1, szBox3D_2, nSRSId );
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

    CPLString osFields = BuildFields();

    pszQueryStatement = (char *)
        CPLMalloc(strlen(osFields)+strlen(osWHERE)
                  +strlen(pszSqlTableName) + 40);
    sprintf( pszQueryStatement,
             "SELECT %s FROM %s %s",
             osFields.c_str(), pszSqlTableName, osWHERE.c_str() );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGTableLayer::ResetReading()

{
    GetLayerDefn();

    bUseCopy = USE_COPY_UNSET;

    BuildFullQueryStatement();

    OGRPGLayer::ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGTableLayer::GetNextFeature()

{
    GetLayerDefn();

    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        /* We just have to look if there is a geometry filter */
        /* If there's a PostGIS geometry column, the spatial filter */
        /* is already taken into account in the select request */
        /* The attribute filter is always taken into account by the select request */
        if( m_poFilterGeom == NULL
            || bHasPostGISGeometry
            || bHasPostGISGeography
            || FilterGeometry( poFeature->GetGeometryRef() )  )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

CPLString OGRPGTableLayer::BuildFields()

{
    int     i = 0;
    CPLString osFieldList;

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
    {
        osFieldList += OGRPGEscapeColumnName(pszFIDColumn);
    }

    if( pszGeomColumn )
    {
        if( strlen(osFieldList) > 0 )
            osFieldList += ", ";

        if( bHasPostGISGeometry )
        {
            if ( poDS->bUseBinaryCursor )
            {
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
            }
            else if (CSLTestBoolean(CPLGetConfigOption("PG_USE_BASE64", "NO")) &&
                     nCoordDimension != 4 /* we don't know how to decode 4-dim EWKB for now */)
            {
                if (poDS->sPostGISVersion.nMajor >= 2)
                    osFieldList += "encode(ST_AsEWKB(";
                else
                    osFieldList += "encode(AsEWKB(";
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
                osFieldList += "), 'base64') AS EWKBBase64";
            }
            else if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) &&
                     nCoordDimension != 4 && /* we don't know how to decode 4-dim EWKB for now */
                      /* perhaps works also for older version, but I didn't check */
                      (poDS->sPostGISVersion.nMajor > 1 ||
                      (poDS->sPostGISVersion.nMajor == 1 && poDS->sPostGISVersion.nMinor >= 1)) )
            {
                /* This will return EWKB in an hex encoded form */
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
            }
            else if ( poDS->sPostGISVersion.nMajor >= 1 )
            {
                if (poDS->sPostGISVersion.nMajor >= 2)
                    osFieldList += "ST_AsEWKT(";
                else
                    osFieldList += "AsEWKT(";
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
                osFieldList += ")";
            }
            else
            {
                osFieldList += "AsText(";
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
                osFieldList += ")";
            }
        }
        else if ( bHasPostGISGeography )
        {
            if ( poDS->bUseBinaryCursor )
            {
                osFieldList += "ST_AsBinary(";
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
                osFieldList += ")";
            }
            else if (CSLTestBoolean(CPLGetConfigOption("PG_USE_BASE64", "NO")))
            {
                osFieldList += "encode(ST_AsBinary(";
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
                osFieldList += "), 'base64') AS BinaryBase64";
            }
            else if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
            {
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
            }
            else
            {
                osFieldList += "ST_AsText(";
                osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
                osFieldList += ")";
            }
        }
        else
        {
            osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
        }
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(osFieldList) > 0 )
            osFieldList += ", ";

        /* With a binary cursor, it is not possible to get the time zone */
        /* of a timestamptz column. So we fallback to asking it in text mode */
        if ( poDS->bUseBinaryCursor &&
             poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDateTime)
        {
            osFieldList += "CAST (";
            osFieldList += OGRPGEscapeColumnName(pszName);
            osFieldList += " AS text)";
        }
        else
        {
            osFieldList += OGRPGEscapeColumnName(pszName);
        }
    }

    return osFieldList;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPGTableLayer::SetAttributeFilter( const char *pszQuery )

{
    GetLayerDefn();

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
    PGconn      *hPGConn = poDS->GetPGConn();
    PGresult    *hResult = NULL;
    CPLString   osCommand;

    GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      We can only delete features if we have a well defined FID       */
/*      column to target.                                               */
/* -------------------------------------------------------------------- */
    if( !bHasFid )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature(%ld) failed.  Unable to delete features in tables without\n"
                  "a recognised FID column.",
                  nFID );
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Form the statement to drop the record.                          */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "DELETE FROM %s WHERE %s = %ld",
                      pszSqlTableName, OGRPGEscapeColumnName(pszFIDColumn).c_str(), nFID );

/* -------------------------------------------------------------------- */
/*      Execute the delete.                                             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
        return eErr;

    hResult = OGRPG_PQexec(hPGConn, osCommand);

    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature() DELETE statement failed.\n%s",
                  PQerrorMessage(hPGConn) );

        OGRPGClearResult( hResult );

        poDS->SoftRollback();
        eErr = OGRERR_FAILURE;
    }
    else
    {
        OGRPGClearResult( hResult );

        eErr = poDS->SoftCommit();
    }

    return eErr;
}

/************************************************************************/
/*                          AppendFieldValue()                          */
/*                                                                      */
/* Used by CreateFeatureViaInsert() and SetFeature() to format a        */
/* non-empty field value                                                */
/************************************************************************/

void OGRPGTableLayer::AppendFieldValue(PGconn *hPGConn, CPLString& osCommand,
                                       OGRFeature* poFeature, int i)
{
    int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

    // We need special formatting for integer list values.
    if(  nOGRFieldType == OFTIntegerList )
    {
        int nCount, nOff = 0, j;
        const int *panItems = poFeature->GetFieldAsIntegerList(i,&nCount);
        char *pszNeedToFree = NULL;

        pszNeedToFree = (char *) CPLMalloc(nCount * 13 + 10);
        strcpy( pszNeedToFree, "'{" );
        for( j = 0; j < nCount; j++ )
        {
            if( j != 0 )
                strcat( pszNeedToFree+nOff, "," );

            nOff += strlen(pszNeedToFree+nOff);
            sprintf( pszNeedToFree+nOff, "%d", panItems[j] );
        }
        strcat( pszNeedToFree+nOff, "}'" );

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for real list values.
    else if( nOGRFieldType == OFTRealList )
    {
        int nCount, nOff = 0, j;
        const double *padfItems =poFeature->GetFieldAsDoubleList(i,&nCount);
        char *pszNeedToFree = NULL;

        pszNeedToFree = (char *) CPLMalloc(nCount * 40 + 10);
        strcpy( pszNeedToFree, "'{" );
        for( j = 0; j < nCount; j++ )
        {
            if( j != 0 )
                strcat( pszNeedToFree+nOff, "," );

            nOff += strlen(pszNeedToFree+nOff);
            //Check for special values. They need to be quoted.
            if( CPLIsNan(padfItems[j]) )
                sprintf( pszNeedToFree+nOff, "NaN" );
            else if( CPLIsInf(padfItems[j]) )
                sprintf( pszNeedToFree+nOff, (padfItems[j] > 0) ? "Infinity" : "-Infinity" );
            else
                sprintf( pszNeedToFree+nOff, "%.16g", padfItems[j] );

            char* pszComma = strchr(pszNeedToFree+nOff, ',');
            if (pszComma)
                *pszComma = '.';
        }
        strcat( pszNeedToFree+nOff, "}'" );

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for string list values.
    else if( nOGRFieldType == OFTStringList )
    {
        char **papszItems = poFeature->GetFieldAsStringList(i);

        osCommand += OGRPGEscapeStringList(hPGConn, papszItems, TRUE);

        return;
    }

    // Binary formatting
    else if( nOGRFieldType == OFTBinary )
    {
        osCommand += "'";

        int nLen = 0;
        GByte* pabyData = poFeature->GetFieldAsBinary( i, &nLen );
        char* pszBytea = GByteArrayToBYTEA( pabyData, nLen);

        osCommand += pszBytea;

        CPLFree(pszBytea);
        osCommand += "'";

        return;
    }

    // Flag indicating NULL or not-a-date date value
    // e.g. 0000-00-00 - there is no year 0
    OGRBoolean bIsDateNull = FALSE;

    const char *pszStrValue = poFeature->GetFieldAsString(i);

    // Check if date is NULL: 0000-00-00
    if( nOGRFieldType == OFTDate )
    {
        if( EQUALN( pszStrValue, "0000", 4 ) )
        {
            pszStrValue = "NULL";
            bIsDateNull = TRUE;
        }
    }
    else if ( nOGRFieldType == OFTReal )
    {
        char* pszComma = strchr((char*)pszStrValue, ',');
        if (pszComma)
            *pszComma = '.';
        //Check for special values. They need to be quoted.
        double dfVal = poFeature->GetFieldAsDouble(i);
        if( CPLIsNan(dfVal) )
            pszStrValue = "'NaN'";
        else if( CPLIsInf(dfVal) )
            pszStrValue = (dfVal > 0) ? "'Infinity'" : "'-Infinity'";
    }

    if( nOGRFieldType != OFTInteger && nOGRFieldType != OFTReal
        && !bIsDateNull )
    {
        osCommand += OGRPGEscapeString(hPGConn, pszStrValue,
                                        poFeatureDefn->GetFieldDefn(i)->GetWidth(),
                                        poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
    }
    else
    {
        osCommand += pszStrValue;
    }
}

/************************************************************************/
/*                             SetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by an UPDATE SQL command            */
/************************************************************************/

OGRErr OGRPGTableLayer::SetFeature( OGRFeature *poFeature )

{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult = NULL;
    CPLString           osCommand;
    int                 i = 0;
    int                 bNeedComma = FALSE;
    OGRErr              eErr = OGRERR_FAILURE;

    GetLayerDefn();

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

    if( !bHasFid )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to update features in tables without\n"
                  "a recognised FID column.");
        return eErr;

    }

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
    {
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Form the UPDATE command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "UPDATE %s SET ", pszSqlTableName );

    /* Set the geometry */
    if( bHasWkb )
    {
        osCommand += "WKB_GEOMETRY = ";
        if ( poFeature->GetGeometryRef() != NULL )
        {
            if( !bWkbAsOid  )
            {
                char    *pszBytea = GeometryToBYTEA( poFeature->GetGeometryRef() );

                if( pszBytea != NULL )
                {
                    osCommand = osCommand + "'" + pszBytea + "'";
                    CPLFree( pszBytea );
                }
                else
                    osCommand += "NULL";
            }
            else
            {
                Oid     oid = GeometryToOID( poFeature->GetGeometryRef() );

                if( oid != 0 )
                {
                    osCommand += CPLString().Printf( "'%d' ", oid );
                }
                else
                    osCommand += "NULL";
            }
        }
        else
            osCommand += "NULL";
        bNeedComma = TRUE;
    }
    else if( bHasPostGISGeometry || bHasPostGISGeography )
    {
        osCommand = osCommand + OGRPGEscapeColumnName(pszGeomColumn) + " = ";
        OGRGeometry *poGeom = NULL;
        
        if( poFeature->GetGeometryRef() != NULL )
        {
            poGeom = (OGRGeometry *) poFeature->GetGeometryRef();

            poGeom->closeRings();
            poGeom->setCoordinateDimension( nCoordDimension );

        }

        if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
        {
            if ( poGeom != NULL )
            {
                char* pszHexEWKB = GeometryToHex( poGeom, nSRSId );
                if ( bHasPostGISGeography )
                    osCommand += CPLString().Printf("'%s'::GEOGRAPHY", pszHexEWKB);
                else
                    osCommand += CPLString().Printf("'%s'::GEOMETRY", pszHexEWKB);
                OGRFree( pszHexEWKB );
            }
            else
                osCommand += "NULL";    
        }
        else
        {
            char    *pszWKT = NULL;
    
            if (poGeom != NULL)
                poGeom->exportToWkt( &pszWKT );

            if( pszWKT != NULL )
            {
                if( bHasPostGISGeography )
                    osCommand +=
                        CPLString().Printf(
                            "ST_GeographyFromText('SRID=%d;%s'::TEXT) ", nSRSId, pszWKT );
                else if( poDS->sPostGISVersion.nMajor >= 1 )
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
                osCommand += "NULL";

        }
        bNeedComma = TRUE;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        osCommand = osCommand 
            + OGRPGEscapeColumnName(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + " = ";

        if( !poFeature->IsFieldSet( i ) )
        {
            osCommand += "NULL";
        }
        else
        {
            AppendFieldValue(hPGConn, osCommand, poFeature, i);
        }
    }

    /* Add the WHERE clause */
    osCommand += " WHERE ";
    osCommand = osCommand + OGRPGEscapeColumnName(pszFIDColumn) + " = ";
    osCommand += CPLString().Printf( "%ld ", poFeature->GetFID() );

/* -------------------------------------------------------------------- */
/*      Execute the update.                                             */
/* -------------------------------------------------------------------- */
    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "UPDATE command for feature %ld failed.\n%s\nCommand: %s",
                  poFeature->GetFID(), PQerrorMessage(hPGConn), osCommand.c_str() );

        OGRPGClearResult( hResult );

        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    return poDS->SoftCommit();
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateFeature( OGRFeature *poFeature )
{
    GetLayerDefn();

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
/*                       OGRPGEscapeColumnName( )                       */
/************************************************************************/

CPLString OGRPGEscapeColumnName(const char* pszColumnName)
{
    CPLString osStr;

    osStr += "\"";

    char ch;
    for(int i=0; (ch = pszColumnName[i]) != '\0'; i++)
    {
        if (ch == '"')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    osStr += "\"";

    return osStr;
}

/************************************************************************/
/*                         OGRPGEscapeString( )                         */
/************************************************************************/

CPLString OGRPGEscapeString(PGconn *hPGConn,
                            const char* pszStrValue, int nMaxLength,
                            const char* pszFieldName)
{
    CPLString osCommand;

    /* We need to quote and escape string fields. */
    osCommand += "'";

    int nSrcLen = strlen(pszStrValue);
    if (nMaxLength > 0 && nSrcLen > nMaxLength)
    {
        CPLDebug( "PG",
                  "Truncated %s field value, it was too long.",
                  pszFieldName );
        nSrcLen = nMaxLength;
        
        while( nSrcLen > 0 && ((unsigned char *) pszStrValue)[nSrcLen-1] > 127 )
        {
            CPLDebug( "PG", "Backup to start of multi-byte character." );
            nSrcLen--;
        }
    }

    char* pszDestStr = (char*)CPLMalloc(2 * nSrcLen + 1);

    /* -------------------------------------------------------------------- */
    /*  PQescapeStringConn was introduced in PostgreSQL security releases   */
    /*  8.1.4, 8.0.8, 7.4.13, 7.3.15                                        */
    /*  PG_HAS_PQESCAPESTRINGCONN is added by a test in 'configure'         */
    /*  so it is not set by default when building OGR for Win32             */
    /* -------------------------------------------------------------------- */
#if defined(PG_HAS_PQESCAPESTRINGCONN)
    int nError;
    PQescapeStringConn (hPGConn, pszDestStr, pszStrValue, nSrcLen, &nError);
    if (nError == 0)
        osCommand += pszDestStr;
    else
        CPLError(CE_Warning, CPLE_AppDefined, 
                 "PQescapeString(): %s\n"
                 "  input: '%s'\n"
                 "    got: '%s'\n",
                 PQerrorMessage( hPGConn ),
                 pszStrValue, pszDestStr );
#else
    PQescapeString(pszDestStr, pszStrValue, nSrcLen);
    osCommand += pszDestStr;
#endif
    CPLFree(pszDestStr);

    osCommand += "'";

    return osCommand;
}


/************************************************************************/
/*                       OGRPGEscapeStringList( )                         */
/************************************************************************/

static CPLString OGRPGEscapeStringList(PGconn *hPGConn,
                                       char** papszItems, int bForInsertOrUpdate)
{
    int bFirstItem = TRUE;
    CPLString osStr;
    if (bForInsertOrUpdate)
        osStr += "ARRAY[";
    else
        osStr += "{";
    while(*papszItems)
    {
        if (!bFirstItem)
        {
            osStr += ',';
        }

        char* pszStr = *papszItems;
        if (*pszStr != '\0')
        {
            if (bForInsertOrUpdate)
                osStr += OGRPGEscapeString(hPGConn, pszStr, -1, "");
            else
            {
                osStr += '"';

                while(*pszStr)
                {
                    if (*pszStr == '"' )
                        osStr += "\\";
                    osStr += *pszStr;
                    pszStr++;
                }

                osStr += '"';
            }
        }
        else
            osStr += "NULL";

        bFirstItem = FALSE;

        papszItems++;
    }
    if (bForInsertOrUpdate)
        osStr += "]";
    else
        osStr += "}";
    return osStr;
}

/************************************************************************/
/*                       CreateFeatureViaInsert()                       */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateFeatureViaInsert( OGRFeature *poFeature )

{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    CPLString           osCommand;
    int                 i;
    int                 bNeedComma = FALSE;
    OGRErr              eErr;

    eErr = poDS->SoftStartTransaction();
    if( eErr != OGRERR_NONE )
    {
        return eErr;
    }

    int bEmptyInsert = FALSE;

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO %s (", pszSqlTableName );

    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if( bHasWkb && poGeom != NULL )
    {
        osCommand += "WKB_GEOMETRY ";
        bNeedComma = TRUE;
    }
    else if( (bHasPostGISGeometry || bHasPostGISGeography) && poGeom != NULL )
    {
        osCommand = osCommand + OGRPGEscapeColumnName(pszGeomColumn) + " ";
        bNeedComma = TRUE;
    }

    /* Use case of ogr_pg_60 test */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        
        osCommand = osCommand + OGRPGEscapeColumnName(pszFIDColumn) + " ";
        bNeedComma = TRUE;
    }

    int nFieldCount = poFeatureDefn->GetFieldCount();
    for( i = 0; i < nFieldCount; i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            osCommand += ", ";

        osCommand = osCommand 
            + OGRPGEscapeColumnName(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }

    if (!bNeedComma)
        bEmptyInsert = TRUE;

    osCommand += ") VALUES (";

    /* Set the geometry */
    bNeedComma = FALSE;
    if( (bHasPostGISGeometry || bHasPostGISGeography) && poGeom != NULL)
    {
        
        CheckGeomTypeCompatibility(poGeom);

        poGeom->closeRings();
        poGeom->setCoordinateDimension( nCoordDimension );


        if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
        {
            char    *pszHexEWKB = GeometryToHex( poGeom, nSRSId );
            if ( bHasPostGISGeography )
                osCommand += CPLString().Printf("'%s'::GEOGRAPHY", pszHexEWKB);
            else
                osCommand += CPLString().Printf("'%s'::GEOMETRY", pszHexEWKB);
            OGRFree( pszHexEWKB );
        }
        else
        { 
            char    *pszWKT = NULL;
            poGeom->exportToWkt( &pszWKT );

            if( pszWKT != NULL )
            {
                if( bHasPostGISGeography )
                    osCommand +=
                        CPLString().Printf(
                            "ST_GeographyFromText('SRID=%d;%s'::TEXT) ", nSRSId, pszWKT );
                else if( poDS->sPostGISVersion.nMajor >= 1 )
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
        bNeedComma = TRUE;
    }
    else if( bHasWkb && !bWkbAsOid && poGeom != NULL )
    {
        char    *pszBytea = GeometryToBYTEA( poGeom );

        if( pszBytea != NULL )
        {
            osCommand = osCommand + "'" + pszBytea + "'";
            CPLFree( pszBytea );
        }
        else
            osCommand += "''";
            
        bNeedComma = TRUE;
    }
    else if( bHasWkb && bWkbAsOid && poGeom != NULL )
    {
        Oid     oid = GeometryToOID( poGeom );

        if( oid != 0 )
        {
            osCommand += CPLString().Printf( "'%d' ", oid );
        }
        else
            osCommand += "''";
            
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( "%ld ", poFeature->GetFID() );
        bNeedComma = TRUE;
    }


    for( i = 0; i < nFieldCount; i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        AppendFieldValue(hPGConn, osCommand, poFeature, i);
    }

    osCommand += ")";

    if (bEmptyInsert)
        osCommand.Printf( "INSERT INTO %s DEFAULT VALUES", pszSqlTableName );

    int bReturnRequested = FALSE;
    /* RETURNING is only available since Postgres 8.2 */
    /* We only get the FID, but we also could add the unset fields to get */
    /* the default values */
    if (bRetrieveFID && pszFIDColumn != NULL && poFeature->GetFID() == OGRNullFID &&
        (poDS->sPostgreSQLVersion.nMajor >= 9 ||
         (poDS->sPostgreSQLVersion.nMajor == 8 && poDS->sPostgreSQLVersion.nMinor >= 2)))
    {
        bReturnRequested = TRUE;
        osCommand += " RETURNING ";
        osCommand += OGRPGEscapeColumnName(pszFIDColumn);
    }

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if (bReturnRequested && PQresultStatus(hResult) == PGRES_TUPLES_OK &&
        PQntuples(hResult) == 1 && PQnfields(hResult) == 1 )
    {
        const char* pszFID = PQgetvalue(hResult, 0, 0 );
        long nFID = atol(pszFID);
        poFeature->SetFID(nFID);
    }
    else if( bReturnRequested || PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "INSERT command for new feature failed.\n%s\nCommand: %s",
                  PQerrorMessage(hPGConn), osCommand.c_str() );

        if( !bHasWarnedAlreadySetFID && poFeature->GetFID() != OGRNullFID &&
            pszFIDColumn != NULL )
        {
            bHasWarnedAlreadySetFID = TRUE;
            CPLError(CE_Warning, CPLE_AppDefined,
                    "You've inserted feature with an already set FID and that's perhaps the reason for the failure. "
                    "If so, this can happen if you reuse the same feature object for sequential insertions. "
                    "Indeed, since GDAL 1.8.0, the FID of an inserted feature is got from the server, so it is not a good idea"
                    "to reuse it afterwards... All in all, try unsetting the FID with SetFID(-1) before calling CreateFeature()");
        }

        OGRPGClearResult( hResult );

        poDS->SoftRollback();

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    return poDS->SoftCommit();
}

/************************************************************************/
/*                        CreateFeatureViaCopy()                        */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateFeatureViaCopy( OGRFeature *poFeature )
{
    PGconn              *hPGConn = poDS->GetPGConn();
    CPLString            osCommand;

    /* First process geometry */
    OGRGeometry *poGeometry = (OGRGeometry *) poFeature->GetGeometryRef();
    
    char *pszGeom = NULL;
    if ( NULL != poGeometry && (bHasWkb || bHasPostGISGeometry || bHasPostGISGeography))
    {
        poGeometry->closeRings();
        poGeometry->setCoordinateDimension( nCoordDimension );
        
        CheckGeomTypeCompatibility(poGeometry);

        if (bHasWkb)
            pszGeom = GeometryToBYTEA( poGeometry );
        else
            pszGeom = GeometryToHex( poGeometry, nSRSId );
    }

    if ( pszGeom )
    {
        osCommand += pszGeom,
        CPLFree( pszGeom );
    }
    else if (nGeomType != wkbNone)
    {
        osCommand = "\\N";
    }

    /* Next process the field id column */
    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) != -1 )
    {
        if (osCommand.size() > 0)
            osCommand += "\t";
            
        /* Set the FID */
        if( poFeature->GetFID() != OGRNullFID )
        {
            osCommand += CPLString().Printf("%ld ", poFeature->GetFID());
        }
        else
        {
            osCommand += "\\N" ;
        }
    }


    /* Now process the remaining fields */

    int nFieldCount = poFeatureDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount;  i++ )
    {
        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if (i > 0 || osCommand.size() > 0)
            osCommand += "\t";
            
        if( !poFeature->IsFieldSet( i ) )
        {
            osCommand += "\\N" ;

            continue;
        }

        int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

        // We need special formatting for integer list values.
        if( nOGRFieldType == OFTIntegerList )
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
        else if( nOGRFieldType == OFTRealList )
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
                //Check for special values. They need to be quoted.
                if( CPLIsNan(padfItems[j]) )
                    sprintf( pszNeedToFree+nOff, "NaN" );
                else if( CPLIsInf(padfItems[j]) )
                    sprintf( pszNeedToFree+nOff, (padfItems[j] > 0) ? "Infinity" : "-Infinity" );
                else
                    sprintf( pszNeedToFree+nOff, "%.16g", padfItems[j] );

                char* pszComma = strchr(pszNeedToFree+nOff, ',');
                if (pszComma)
                    *pszComma = '.';
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }


        // We need special formatting for string list values.
        else if( nOGRFieldType == OFTStringList )
        {
            CPLString osStr;
            char **papszItems = poFeature->GetFieldAsStringList(i);

            pszStrValue = pszNeedToFree = CPLStrdup(OGRPGEscapeStringList(hPGConn, papszItems, FALSE));
        }

        // Binary formatting
        else if( nOGRFieldType == OFTBinary )
        {
            int nLen = 0;
            GByte* pabyData = poFeature->GetFieldAsBinary( i, &nLen );
            char* pszBytea = GByteArrayToBYTEA( pabyData, nLen);

            pszStrValue = pszNeedToFree = pszBytea;
        }

        else if( nOGRFieldType == OFTReal )
        {
            char* pszComma = strchr((char*)pszStrValue, ',');
            if (pszComma)
                *pszComma = '.';
            //Check for special values. They need to be quoted.
            double dfVal = poFeature->GetFieldAsDouble(i);
            if( CPLIsNan(dfVal) )
                pszStrValue = "NaN";
            else if( CPLIsInf(dfVal) )
                pszStrValue = (dfVal > 0) ? "Infinity" : "-Infinity";
        }

        if( nOGRFieldType != OFTIntegerList &&
            nOGRFieldType != OFTRealList &&
            nOGRFieldType != OFTInteger &&
            nOGRFieldType != OFTReal &&
            nOGRFieldType != OFTBinary )
        {
            int         iChar;

            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetWidth() > 0
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
                    osCommand += '\\';
                }

                osCommand += pszStrValue[iChar];
            }
        }
        else
        {
            osCommand += pszStrValue;
        }

        if( pszNeedToFree )
            CPLFree( pszNeedToFree );
    }

    /* Add end of line marker */
    osCommand += "\n";


    /* ------------------------------------------------------------ */
    /*      Execute the copy.                                       */
    /* ------------------------------------------------------------ */

    OGRErr result = OGRERR_NONE;

    /* This is for postgresql  7.4 and higher */
#if !defined(PG_PRE74)
    int copyResult = PQputCopyData(hPGConn, osCommand.c_str(), strlen(osCommand.c_str()));
    //CPLDebug("PG", "PQputCopyData(%s)", osCommand.c_str());

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
#else /* else defined(PG_PRE74) */
    int copyResult = PQputline(hPGConn, osCommand.c_str());

    if (copyResult == EOF)
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Writing COPY data blocked.");
      result = OGRERR_FAILURE;
    }  
#endif /* end of defined(PG_PRE74) */

    return result;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGTableLayer::TestCapability( const char * pszCap )

{
    GetLayerDefn();

    if ( bUpdateAccess )
    {
        if( EQUAL(pszCap,OLCSequentialWrite) ||
            EQUAL(pszCap,OLCCreateField) ||
            EQUAL(pszCap,OLCDeleteField) ||
            EQUAL(pszCap,OLCAlterFieldDefn) )
            return TRUE;

        else if( EQUAL(pszCap,OLCRandomWrite) ||
                 EQUAL(pszCap,OLCDeleteFeature) )
            return bHasFid;
    }

    if( EQUAL(pszCap,OLCRandomRead) )
        return bHasFid;

    else if( EQUAL(pszCap,OLCFastFeatureCount) ||
             EQUAL(pszCap,OLCFastSetNextByIndex) )
        return m_poFilterGeom == NULL || bHasPostGISGeometry || bHasPostGISGeography;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return bHasPostGISGeometry || bHasPostGISGeography;

    else if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return bHasPostGISGeometry;

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                        OGRPGTableLayerGetType()                      */
/************************************************************************/

static CPLString OGRPGTableLayerGetType(OGRFieldDefn& oField,
                                        int bPreservePrecision,
                                        int bApproxOK)
{
    char                szFieldType[256];

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
        if (oField.GetWidth() > 0 &&  bPreservePrecision )
            sprintf( szFieldType, "VARCHAR(%d)",  oField.GetWidth() );
        else
            strcpy( szFieldType, "VARCHAR");
    }
    else if( oField.GetType() == OFTIntegerList )
    {
        strcpy( szFieldType, "INTEGER[]" );
    }
    else if( oField.GetType() == OFTRealList )
    {
        strcpy( szFieldType, "FLOAT8[]" );
    }
    else if( oField.GetType() == OFTStringList )
    {
        strcpy( szFieldType, "varchar[]" );
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
    else if( oField.GetType() == OFTBinary )
    {
        strcpy( szFieldType, "bytea" );
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
        strcpy( szFieldType, "");
    }

    return szFieldType;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateField( OGRFieldDefn *poFieldIn, int bApproxOK )

{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult = NULL;
    CPLString           osCommand;
    CPLString           osFieldType;
    OGRFieldDefn        oField( poFieldIn );

    GetLayerDefn();

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

    osFieldType = OGRPGTableLayerGetType(oField, bPreservePrecision, bApproxOK);
    if (osFieldType.size() == 0)
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    poDS->FlushSoftTransaction();
    hResult = OGRPG_PQexec(hPGConn, "BEGIN");
    OGRPGClearResult( hResult );

    osCommand.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                      pszSqlTableName, OGRPGEscapeColumnName(oField.GetNameRef()).c_str(),
                      osFieldType.c_str() );
    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s\n%s", 
                  osCommand.c_str(), 
                  PQerrorMessage(hPGConn) );

        OGRPGClearResult( hResult );

        hResult = OGRPG_PQexec( hPGConn, "ROLLBACK" );
        OGRPGClearResult( hResult );

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    hResult = OGRPG_PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    poFeatureDefn->AddFieldDefn( &oField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRPGTableLayer::DeleteField( int iField )
{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult = NULL;
    CPLString           osCommand;

    GetLayerDefn();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't delete fields on a read-only datasource.");
        return OGRERR_FAILURE;
    }

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    poDS->FlushSoftTransaction();

    hResult = OGRPG_PQexec(hPGConn, "BEGIN");
    OGRPGClearResult( hResult );

    osCommand.Printf( "ALTER TABLE %s DROP COLUMN %s",
                      pszSqlTableName,
                      OGRPGEscapeColumnName(poFeatureDefn->GetFieldDefn(iField)->GetNameRef()).c_str() );
    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s\n%s",
                  osCommand.c_str(),
                  PQerrorMessage(hPGConn) );

        OGRPGClearResult( hResult );

        hResult = OGRPG_PQexec( hPGConn, "ROLLBACK" );
        OGRPGClearResult( hResult );

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    hResult = OGRPG_PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    return poFeatureDefn->DeleteFieldDefn( iField );
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRPGTableLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult = NULL;
    CPLString           osCommand;

    GetLayerDefn();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't alter field definition on a read-only datasource.");
        return OGRERR_FAILURE;
    }

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn       *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
    OGRFieldDefn        oField( poNewFieldDefn );

    poDS->FlushSoftTransaction();

    hResult = OGRPG_PQexec(hPGConn, "BEGIN");
    OGRPGClearResult( hResult );

    if (!(nFlags & ALTER_TYPE_FLAG))
        oField.SetType(poFieldDefn->GetType());

    if (!(nFlags & ALTER_WIDTH_PRECISION_FLAG))
    {
        oField.SetWidth(poFieldDefn->GetWidth());
        oField.SetPrecision(poFieldDefn->GetPrecision());
    }

    if ((nFlags & ALTER_TYPE_FLAG) ||
        (nFlags & ALTER_WIDTH_PRECISION_FLAG))
    {
        CPLString osFieldType = OGRPGTableLayerGetType(oField,
                                                       bPreservePrecision,
                                                       TRUE);
        if (osFieldType.size() == 0)
        {
            hResult = OGRPG_PQexec( hPGConn, "ROLLBACK" );
            OGRPGClearResult( hResult );

            return OGRERR_FAILURE;
        }

        osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s TYPE %s",
                        pszSqlTableName,
                        OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str(),
                        osFieldType.c_str() );
        hResult = OGRPG_PQexec(hPGConn, osCommand);
        if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s\n%s",
                    osCommand.c_str(),
                    PQerrorMessage(hPGConn) );

            OGRPGClearResult( hResult );

            hResult = OGRPG_PQexec( hPGConn, "ROLLBACK" );
            OGRPGClearResult( hResult );

            return OGRERR_FAILURE;
        }
        OGRPGClearResult( hResult );
    }


    if( (nFlags & ALTER_NAME_FLAG) )
    {
        if (bLaunderColumnNames)
        {
            char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );
            oField.SetName( pszSafeName );
            CPLFree( pszSafeName );
        }

        if( EQUAL(oField.GetNameRef(),"oid") )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Renaming field 'oid' to 'oid_' to avoid conflict with internal oid field." );
            oField.SetName( "oid_" );
        }

        if ( strcmp(poFieldDefn->GetNameRef(), oField.GetNameRef()) != 0 )
        {
            osCommand.Printf( "ALTER TABLE %s RENAME COLUMN %s TO %s",
                            pszSqlTableName,
                            OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str(),
                            OGRPGEscapeColumnName(oField.GetNameRef()).c_str() );
            hResult = OGRPG_PQexec(hPGConn, osCommand);
            if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "%s\n%s",
                        osCommand.c_str(),
                        PQerrorMessage(hPGConn) );

                OGRPGClearResult( hResult );

                hResult = OGRPG_PQexec( hPGConn, "ROLLBACK" );
                OGRPGClearResult( hResult );

                return OGRERR_FAILURE;
            }
            OGRPGClearResult( hResult );
        }
    }

    hResult = OGRPG_PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    if (nFlags & ALTER_NAME_FLAG)
        poFieldDefn->SetName(oField.GetNameRef());
    if (nFlags & ALTER_TYPE_FLAG)
        poFieldDefn->SetType(oField.GetType());
    if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
    {
        poFieldDefn->SetWidth(oField.GetWidth());
        poFieldDefn->SetPrecision(oField.GetPrecision());
    }

    return OGRERR_NONE;

}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGTableLayer::GetFeature( long nFeatureId )

{
    GetLayerDefn();

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
    PGresult    *hResult = NULL;
    PGconn      *hPGConn = poDS->GetPGConn();
    CPLString    osFieldList = BuildFields();
    CPLString    osCommand;

    poDS->FlushSoftTransaction();
    poDS->SoftStartTransaction();

    osCommand.Printf(
             "DECLARE getfeaturecursor %s for "
             "SELECT %s FROM %s WHERE %s = %ld",
              ( poDS->bUseBinaryCursor ) ? "BINARY CURSOR" : "CURSOR",
             osFieldList.c_str(), pszSqlTableName, OGRPGEscapeColumnName(pszFIDColumn).c_str(),
             nFeatureId );

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        OGRPGClearResult( hResult );

        hResult = OGRPG_PQexec(hPGConn, "FETCH ALL in getfeaturecursor" );

        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        {
            int nRows = PQntuples(hResult);
            if (nRows > 0)
            {
                hCursorResult = hResult;
                CreateMapFromFieldNameToIndex();
                poFeature = RecordToFeature( 0 );
                hCursorResult = NULL;

                if (nRows > 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%d rows in response to the WHERE %s = %ld clause !",
                             nRows, pszFIDColumn, nFeatureId );
                }
            }
            else
            {
                 CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read feature with unknown feature id (%ld).", nFeatureId );
            }
        }
    }
    else if ( hResult && PQresultStatus(hResult) == PGRES_FATAL_ERROR )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQresultErrorMessage( hResult ) );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    OGRPGClearResult( hResult );

    hResult = OGRPG_PQexec(hPGConn, "CLOSE getfeaturecursor");
    OGRPGClearResult( hResult );

    poDS->FlushSoftTransaction();

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRPGTableLayer::GetFeatureCount( int bForce )

{
    GetLayerDefn();

    if( TestCapability(OLCFastFeatureCount) == FALSE )
        return OGRPGLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      In theory it might be wise to cache this result, but it         */
/*      won't be trivial to work out the lifetime of the value.         */
/*      After all someone else could be adding records from another     */
/*      application when working against a database.                    */
/* -------------------------------------------------------------------- */
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult = NULL;
    CPLString           osCommand;
    int                 nCount = 0;

    osCommand.Printf(
        "SELECT count(*) FROM %s %s",
        pszSqlTableName, osWHERE.c_str() );

    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( hResult != NULL && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        nCount = atoi(PQgetvalue(hResult,0,0));
    else
        CPLDebug( "PG", "%s; failed.", osCommand.c_str() );
    OGRPGClearResult( hResult );

    return nCount;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/*                                                                      */
/*      We override this to try and fetch the table SRID from the       */
/*      geometry_columns table if the srsid is UNDETERMINED_SRID        */
/*      (meaning we haven't yet even looked for it).                    */
/************************************************************************/

OGRSpatialReference *OGRPGTableLayer::GetSpatialRef()

{
    if( nSRSId == UNDETERMINED_SRID )
    {
        PGconn      *hPGConn = poDS->GetPGConn();
        PGresult    *hResult = NULL;
        CPLString    osCommand;

        nSRSId = poDS->GetUndefinedSRID();

        poDS->SoftStartTransaction();

        osCommand.Printf(
                    "SELECT srid FROM geometry_columns "
                    "WHERE f_table_name = '%s'",
                    pszTableName);

        if (pszGeomColumn)
        {
            osCommand += CPLString().Printf(" AND f_geometry_column = '%s'", pszGeomColumn);
        }

        if (pszSchemaName)
        {
            osCommand += CPLString().Printf(" AND f_table_schema = '%s'", pszSchemaName);
        }

        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

        if( hResult
            && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) == 1 )
        {
            nSRSId = atoi(PQgetvalue(hResult,0,0));
        }

        OGRPGClearResult( hResult );

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
    CPLString   osCommand;

    GetLayerDefn();

    const char* pszExtentFct;
    if (poDS->sPostGISVersion.nMajor >= 2)
        pszExtentFct = "ST_Extent";
    else
        pszExtentFct = "Extent";

    if ( TestCapability(OLCFastGetExtent) )
    {
        osCommand.Printf( "SELECT %s(%s) FROM %s", pszExtentFct,
                          OGRPGEscapeColumnName(pszGeomColumn).c_str(), pszSqlTableName );
    }
    else if ( bHasPostGISGeography )
    {
        /* Probably not very efficient, but more efficient than client-side implementation */
        osCommand.Printf( "SELECT %s(ST_GeomFromWKB(ST_AsBinary(%s))) FROM %s",
                          pszExtentFct,
                          OGRPGEscapeColumnName(pszGeomColumn).c_str(), pszSqlTableName );
    }

    return RunGetExtentRequest(psExtent, bForce, osCommand);
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

OGRErr OGRPGTableLayer::StartCopy()

{
    OGRErr result = OGRERR_NONE;

    /* Tell the datasource we are now planning to copy data */
    poDS->StartCopy( this ); 

    CPLString osFields = BuildCopyFields();

    int size = strlen(osFields) +  strlen(pszSqlTableName) + 100;
    char *pszCommand = (char *) CPLMalloc(size);

    sprintf( pszCommand,
             "COPY %s (%s) FROM STDIN;",
             pszSqlTableName, osFields.c_str() );

    PGconn *hPGConn = poDS->GetPGConn();
    PGresult *hResult = OGRPG_PQexec(hPGConn, pszCommand);

    if ( !hResult || (PQresultStatus(hResult) != PGRES_COPY_IN))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage(hPGConn) );
        result = OGRERR_FAILURE;
    }
    else
        bCopyActive = TRUE;

    OGRPGClearResult( hResult );
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
    CPLDebug( "PG", "PQputCopyEnd()" );

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

    OGRPGClearResult( hResult );

    bUseCopy = USE_COPY_UNSET;

    return result;
}

/************************************************************************/
/*                          BuildCopyFields()                           */
/************************************************************************/

CPLString OGRPGTableLayer::BuildCopyFields()
{
    int     i = 0;
    CPLString osFieldList;

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) != -1 )
    {
        osFieldList += OGRPGEscapeColumnName(pszFIDColumn);
    }

    if( pszGeomColumn )
    {
        if( strlen(osFieldList) > 0 )
            osFieldList += ", ";

        osFieldList += OGRPGEscapeColumnName(pszGeomColumn);
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(osFieldList) > 0 )
            osFieldList += ", ";

        osFieldList += OGRPGEscapeColumnName(pszName);
    }

    return osFieldList;
}

/************************************************************************/
/*                    CheckGeomTypeCompatibility()                      */
/************************************************************************/

void OGRPGTableLayer::CheckGeomTypeCompatibility(OGRGeometry* poGeom)
{
    if (bHasWarnedIncompatibleGeom)
        return;
        
    OGRwkbGeometryType eFlatLayerGeomType = wkbFlatten(poFeatureDefn->GetGeomType());
    OGRwkbGeometryType eFlatGeomType = wkbFlatten(poGeom->getGeometryType());
    if (eFlatLayerGeomType == wkbUnknown)
        return;

    if (eFlatLayerGeomType == wkbGeometryCollection)
        bHasWarnedIncompatibleGeom = eFlatGeomType != wkbMultiPoint &&
                                     eFlatGeomType != wkbMultiLineString &&
                                     eFlatGeomType != wkbMultiPolygon &&
                                     eFlatGeomType != wkbGeometryCollection;
    else
        bHasWarnedIncompatibleGeom = (eFlatGeomType != eFlatLayerGeomType);
    
    if (bHasWarnedIncompatibleGeom)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Geometry to be inserted is of type %s, whereas the layer geometry type is %s.\n"
                 "Insertion is likely to fail",
                 OGRGeometryTypeToName(poGeom->getGeometryType()), 
                 OGRGeometryTypeToName(poFeatureDefn->GetGeomType()));
    }
}

/************************************************************************/
/*                  GetLayerDefnCanReturnNULL()                         */
/************************************************************************/

OGRFeatureDefn * OGRPGTableLayer::GetLayerDefnCanReturnNULL()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    poFeatureDefn = ReadTableDefinition();

    if( poFeatureDefn )
    {
        ResetReading();
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                         GetLayerDefn()                              */
/************************************************************************/

OGRFeatureDefn * OGRPGTableLayer::GetLayerDefn()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    GetLayerDefnCanReturnNULL();
    if (poFeatureDefn == NULL)
    {
        poFeatureDefn = new OGRFeatureDefn(pszTableName);
        poFeatureDefn->Reference();
    }
    return poFeatureDefn;
}

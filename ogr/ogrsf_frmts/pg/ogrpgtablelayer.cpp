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

#define UNSUPPORTED_OP_READ_ONLY "%s : unsupported operation on a read-only datasource."


/************************************************************************/
/*                        OGRPGTableFeatureDefn                         */
/************************************************************************/

class OGRPGTableFeatureDefn : public OGRPGFeatureDefn
{
    private:
        OGRPGTableLayer *poLayer;

        void SolveFields();

    public:
        OGRPGTableFeatureDefn( OGRPGTableLayer* poLayerIn,
                               const char * pszName = NULL ) :
            OGRPGFeatureDefn(pszName), poLayer(poLayerIn)
        {
        }

        virtual void UnsetLayer()
        {
            poLayer = NULL;
            OGRPGFeatureDefn::UnsetLayer();
        }

        virtual int         GetFieldCount()
            { SolveFields(); return OGRPGFeatureDefn::GetFieldCount(); }
        virtual OGRFieldDefn *GetFieldDefn( int i )
            { SolveFields(); return OGRPGFeatureDefn::GetFieldDefn(i); }
        virtual int         GetFieldIndex( const char * pszName )
            { SolveFields(); return OGRPGFeatureDefn::GetFieldIndex(pszName); }

        virtual int         GetGeomFieldCount()
            { if (poLayer != NULL && !poLayer->HasGeometryInformation())
                  SolveFields();
              return OGRPGFeatureDefn::GetGeomFieldCount(); }
        virtual OGRGeomFieldDefn *GetGeomFieldDefn( int i )
            { if (poLayer != NULL && !poLayer->HasGeometryInformation())
                  SolveFields();
              return OGRPGFeatureDefn::GetGeomFieldDefn(i); }
        virtual int         GetGeomFieldIndex( const char * pszName)
            { if (poLayer != NULL && !poLayer->HasGeometryInformation())
                  SolveFields();
              return OGRPGFeatureDefn::GetGeomFieldIndex(pszName); }

};

/************************************************************************/
/*                           SolveFields()                              */
/************************************************************************/

void OGRPGTableFeatureDefn::SolveFields()
{
    if( poLayer == NULL )
        return;

    poLayer->ReadTableDefinition();
}

/************************************************************************/
/*                          OGRPGTableLayer()                           */
/************************************************************************/

OGRPGTableLayer::OGRPGTableLayer( OGRPGDataSource *poDSIn,
                                  CPLString& osCurrentSchema,
                                  const char * pszTableNameIn,
                                  const char * pszSchemaNameIn,
                                  const char * pszGeomColForced,
                                  int bUpdate )

{
    poDS = poDSIn;

    pszQueryStatement = NULL;

    bUpdateAccess = bUpdate;

    iNextShapeId = 0;

    bGeometryInformationSet = FALSE;

    bLaunderColumnNames = TRUE;
    bPreservePrecision = TRUE;
    bCopyActive = FALSE;
    bUseCopy = USE_COPY_UNSET;  // unknown
    bFIDColumnInCopyFields = FALSE;
    bFirstInsertion = TRUE;

    pszTableName = CPLStrdup( pszTableNameIn );
    if (pszSchemaNameIn)
        pszSchemaName = CPLStrdup( pszSchemaNameIn );
    else if (strlen(osCurrentSchema))
        pszSchemaName = CPLStrdup( osCurrentSchema );
    else
        pszSchemaName = NULL;
    this->pszGeomColForced =
        pszGeomColForced ? CPLStrdup(pszGeomColForced) : NULL;

    pszSqlGeomParentTableName = NULL;
    bTableDefinitionValid = -1;

    bHasWarnedIncompatibleGeom = FALSE;
    bHasWarnedAlreadySetFID = FALSE;

    /* Just in provision for people yelling about broken backward compatibility ... */
    bRetrieveFID = CSLTestBoolean(CPLGetConfigOption("OGR_PG_RETRIEVE_FID", "TRUE"));

/* -------------------------------------------------------------------- */
/*      Build the layer defn name.                                      */
/* -------------------------------------------------------------------- */
    CPLString osDefnName;
    if ( pszSchemaNameIn && osCurrentSchema != pszSchemaNameIn )
    {
        osDefnName.Printf("%s.%s", pszSchemaNameIn, pszTableName );
        pszSqlTableName = CPLStrdup(CPLString().Printf("%s.%s",
                               OGRPGEscapeColumnName(pszSchemaNameIn).c_str(),
                               OGRPGEscapeColumnName(pszTableName).c_str() ));
    }
    else
    {
        //no prefix for current_schema in layer name, for backwards compatibility
        osDefnName = pszTableName;
        pszSqlTableName = CPLStrdup(OGRPGEscapeColumnName(pszTableName));
    }
    if( pszGeomColForced != NULL )
    {
        osDefnName += "(";
        osDefnName += pszGeomColForced;
        osDefnName += ")";
    }

    osPrimaryKey = CPLGetConfigOption( "PGSQL_OGR_FID", "ogc_fid" );

    papszOverrideColumnTypes = NULL;
    nForcedSRSId = UNDETERMINED_SRID;
    nForcedDimension = -1;
    bCreateSpatialIndexFlag = TRUE;
    bInResetReading = FALSE;

    poFeatureDefn = new OGRPGTableFeatureDefn( this, osDefnName );
    poFeatureDefn->Reference();
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
    CPLFree( pszGeomColForced );
    CSLDestroy( papszOverrideColumnTypes );
}

/************************************************************************/
/*                      SetGeometryInformation()                        */
/************************************************************************/

void  OGRPGTableLayer::SetGeometryInformation(PGGeomColumnDesc* pasDesc,
                                              int nGeomFieldCount)
{
    /* flag must be set before instanciating geometry fields */
    bGeometryInformationSet = TRUE;

    for(int i=0; i<nGeomFieldCount; i++)
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            new OGRPGGeomFieldDefn(this, pasDesc[i].pszName);
        poGeomFieldDefn->nSRSId = pasDesc[i].nSRID;
        poGeomFieldDefn->nCoordDimension = pasDesc[i].nCoordDimension;
        poGeomFieldDefn->ePostgisType = pasDesc[i].ePostgisType;
        if( pasDesc[i].pszGeomType != NULL )
        {
            OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(pasDesc[i].pszGeomType);
            if( poGeomFieldDefn->nCoordDimension == 3 && eGeomType != wkbUnknown )
                eGeomType = (OGRwkbGeometryType) (eGeomType | wkb25DBit);
            poGeomFieldDefn->SetType(eGeomType);
        }
        poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
    }
}


/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

int OGRPGTableLayer::ReadTableDefinition()

{
    PGresult            *hResult;
    CPLString           osCommand;
    PGconn              *hPGConn = poDS->GetPGConn();

    if( bTableDefinitionValid >= 0 )
        return bTableDefinitionValid;
    bTableDefinitionValid = FALSE;

    poDS->FlushSoftTransaction();

    CPLString osSchemaClause;
    if( pszSchemaName )
        osSchemaClause.Printf("AND n.nspname=%s",
                              OGRPGEscapeString(hPGConn, pszSchemaName).c_str());

    const char* pszTypnameEqualsAnyClause;
    if (poDS->sPostgreSQLVersion.nMajor == 7 && poDS->sPostgreSQLVersion.nMinor <= 3)
        pszTypnameEqualsAnyClause = "ANY(SELECT '{int2, int4, serial}')";
    else
        pszTypnameEqualsAnyClause = "ANY(ARRAY['int2','int4','serial'])";

    CPLString osEscapedTableNameSingleQuote = OGRPGEscapeString(hPGConn, pszTableName);
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
                 "%s "
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
        return bTableDefinitionValid;
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
        return bTableDefinitionValid;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    int            iRecord;

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
        else if( EQUAL(pszType,"geometry") ||
                 EQUAL(pszType,"geography") ||
                 EQUAL(oField.GetNameRef(),"WKB_GEOMETRY") )
        {
            OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
            if( !bGeometryInformationSet )
            {
                if( pszGeomColForced == NULL ||
                    EQUAL(pszGeomColForced, oField.GetNameRef()) )
                    poGeomFieldDefn = new OGRPGGeomFieldDefn(this, oField.GetNameRef());
            }
            else
            {
                int idx = poFeatureDefn->GetGeomFieldIndex(oField.GetNameRef());
                if( idx >= 0 )
                    poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(idx);
            }
            if( poGeomFieldDefn != NULL )
            {
                if( EQUAL(pszType,"geometry") )
                    poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOMETRY;
                else if( EQUAL(pszType,"geography") )
                {
                    poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOGRAPHY;
                    poGeomFieldDefn->nSRSId = 4326;
                }
                else
                {
                    poGeomFieldDefn->ePostgisType = GEOM_TYPE_WKB;
                    if( EQUAL(pszType,"OID") )
                        bWkbAsOid = TRUE;
                }
                if( !bGeometryInformationSet )
                    poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
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
            if( EQUAL(pszFormatName, "numeric") )
                oField.SetType( OFTReal );
            else
            {
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

        poFeatureDefn->AddFieldDefn( &oField );
    }

    OGRPGClearResult( hResult );

    hResult = OGRPG_PQexec(hPGConn, "CLOSE mycursor");
    OGRPGClearResult( hResult );

    hResult = OGRPG_PQexec(hPGConn, "COMMIT");
    OGRPGClearResult( hResult );

    bTableDefinitionValid = TRUE;

    ResetReading();

    /* If geometry type, SRID, etc... have always been set by SetGeometryInformation() */
    /* no need to issue a new SQL query. Just record the geom type in the layer definition */
    if (bGeometryInformationSet)
    {
        return TRUE;
    }
    bGeometryInformationSet = TRUE;

    // get layer geometry type (for PostGIS dataset)
    for(int iField = 0; iField < poFeatureDefn->GetGeomFieldCount(); iField++)
    {
      OGRPGGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->myGetGeomFieldDefn(iField);

      /* Get the geometry type and dimensions from the table, or */
      /* from its parents if it is a derived table, or from the parent of the parent, etc.. */
      int bGoOn = TRUE;
      int bHasPostGISGeometry =
        (poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY);

      while(bGoOn)
      {
        CPLString osEscapedTableNameSingleQuote = OGRPGEscapeString(hPGConn,
                (pszSqlGeomParentTableName) ? pszSqlGeomParentTableName : pszTableName);
        const char* pszEscapedTableNameSingleQuote = osEscapedTableNameSingleQuote.c_str();

        osCommand.Printf(
            "SELECT type, coord_dimension, srid FROM %s WHERE f_table_name = %s",
            (bHasPostGISGeometry) ? "geometry_columns" : "geography_columns",
            pszEscapedTableNameSingleQuote);

        osCommand += CPLString().Printf(" AND %s=%s",
            (bHasPostGISGeometry) ? "f_geometry_column" : "f_geography_column",
            OGRPGEscapeString(hPGConn,poGeomFieldDefn->GetNameRef()).c_str());

        if (pszSchemaName)
        {
            osCommand += CPLString().Printf(" AND f_table_schema = %s",
                                            OGRPGEscapeString(hPGConn,pszSchemaName).c_str());
        }

        hResult = OGRPG_PQexec(hPGConn,osCommand);

        if ( hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult,0,0) )
        {
            const char* pszType = PQgetvalue(hResult,0,0);

            int nCoordDimension = MAX(2,MIN(3,atoi(PQgetvalue(hResult,0,1))));

            int nSRSId = atoi(PQgetvalue(hResult,0,2));

            poGeomFieldDefn->nCoordDimension = nCoordDimension;
            poGeomFieldDefn->nSRSId = nSRSId;
            OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(pszType);
            if( poGeomFieldDefn->nCoordDimension == 3 && eGeomType != wkbUnknown )
                eGeomType = (OGRwkbGeometryType) (eGeomType | wkb25DBit);
            poGeomFieldDefn->SetType(eGeomType);

            bGoOn = FALSE;
        }
        else
        {
            /* Fetch the name of the parent table */
            if (pszSchemaName)
            {
                osCommand.Printf("SELECT pg_class.relname FROM pg_class WHERE oid = "
                                "(SELECT pg_inherits.inhparent FROM pg_inherits WHERE inhrelid = "
                                "(SELECT c.oid FROM pg_class c, pg_namespace n WHERE c.relname = %s AND c.relnamespace=n.oid AND n.nspname = %s))",
                                pszEscapedTableNameSingleQuote,
                                OGRPGEscapeString(hPGConn, pszSchemaName).c_str() );
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
    }

    return bTableDefinitionValid;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPGTableLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }
    m_iGeomFieldFilter = iGeomField;

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
    OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
        poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);

    if( m_poFilterGeom != NULL && poGeomFieldDefn != NULL && (
            poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
            poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY) )
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
        osWHERE.Printf("WHERE %s && %s('BOX3D(%s, %s)'::box3d,%d) ",
                       OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef()).c_str(),
                       (poDS->sPostGISVersion.nMajor >= 2) ? "ST_SetSRID" : "SetSRID",
                       szBox3D_1, szBox3D_2, poGeomFieldDefn->nSRSId );
    }

    if( strlen(osQuery) > 0 )
    {
        if( strlen(osWHERE) == 0 )
        {
            osWHERE.Printf( "WHERE %s ", osQuery.c_str()  );
        }
        else
        {
            osWHERE += "AND (";
            osWHERE += osQuery;
            osWHERE += ")";
        }
    }
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRPGTableLayer::BuildFullQueryStatement()

{
    CPLString osFields = BuildFields();
    if( pszQueryStatement != NULL )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = NULL;
    }
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
    if( bInResetReading )
        return;
    bInResetReading = TRUE;

    bUseCopy = USE_COPY_UNSET;

    BuildFullQueryStatement();

    OGRPGLayer::ResetReading();

    bInResetReading = FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGTableLayer::GetNextFeature()

{
    OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
        poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);
    poFeatureDefn->GetFieldCount();

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
            || poGeomFieldDefn == NULL
            || poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY
            || poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) )  )
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

    poFeatureDefn->GetFieldCount();

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
    {
        osFieldList += OGRPGEscapeColumnName(pszFIDColumn);
    }

    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(i);
        CPLString osEscapedGeom =
            OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef());

        if( osFieldList.size() > 0 )
            osFieldList += ", ";

        if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY )
        {
            if ( poDS->bUseBinaryCursor )
            {
                osFieldList += osEscapedGeom;
            }
            else if (CSLTestBoolean(CPLGetConfigOption("PG_USE_BASE64", "NO")) &&
                     poGeomFieldDefn->nCoordDimension != 4 /* we don't know how to decode 4-dim EWKB for now */)
            {
                if (poDS->sPostGISVersion.nMajor >= 2)
                    osFieldList += "encode(ST_AsEWKB(";
                else
                    osFieldList += "encode(AsEWKB(";
                osFieldList += osEscapedGeom;
                osFieldList += "), 'base64') AS ";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("EWKBBase64_%s", poGeomFieldDefn->GetNameRef()));
            }
            else if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) &&
                     poGeomFieldDefn->nCoordDimension != 4 && /* we don't know how to decode 4-dim EWKB for now */
                      /* perhaps works also for older version, but I didn't check */
                      (poDS->sPostGISVersion.nMajor > 1 ||
                      (poDS->sPostGISVersion.nMajor == 1 && poDS->sPostGISVersion.nMinor >= 1)) )
            {
                /* This will return EWKB in an hex encoded form */
                osFieldList += osEscapedGeom;
            }
            else if ( poDS->sPostGISVersion.nMajor >= 1 )
            {
                if (poDS->sPostGISVersion.nMajor >= 2)
                    osFieldList += "ST_AsEWKT(";
                else
                    osFieldList += "AsEWKT(";
                osFieldList += osEscapedGeom;
                osFieldList += ") AS ";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("AsEWKT_%s", poGeomFieldDefn->GetNameRef()));

            }
            else
            {
                osFieldList += "AsText(";
                osFieldList += osEscapedGeom;
                osFieldList += ") AS ";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("AsText_%s", poGeomFieldDefn->GetNameRef()));
            }
        }
        else if ( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
        {
            if ( poDS->bUseBinaryCursor )
            {
                osFieldList += "ST_AsBinary(";
                osFieldList += osEscapedGeom;
                osFieldList += ") AS";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("AsBinary_%s", poGeomFieldDefn->GetNameRef()));
            }
            else if (CSLTestBoolean(CPLGetConfigOption("PG_USE_BASE64", "NO")))
            {
                osFieldList += "encode(ST_AsBinary(";
                osFieldList += osEscapedGeom;
                osFieldList += "), 'base64') AS ";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("BinaryBase64_%s", poGeomFieldDefn->GetNameRef()));
            }
            else if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
            {
                osFieldList += osEscapedGeom;
            }
            else
            {
                osFieldList += "ST_AsText(";
                osFieldList += osEscapedGeom;
                osFieldList += ") AS ";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("AsText_%s", poGeomFieldDefn->GetNameRef()));
            }
        }
        else
        {
            osFieldList += osEscapedGeom;
        }
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( osFieldList.size() > 0 )
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

    GetLayerDefn()->GetFieldCount();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }

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
        if (poDS->bUseEscapeStringSyntax)
            osCommand += "E";

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
                                        poFeatureDefn->GetName(),
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

    GetLayerDefn()->GetFieldCount();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "SetFeature");
        return OGRERR_FAILURE;
    }

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
    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_WKB )
        {
            if( bNeedComma )
                osCommand += ", ";
            else
                bNeedComma = TRUE;

            osCommand += OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef());
            osCommand += " = ";
            if ( poGeom != NULL )
            {
                if( !bWkbAsOid  )
                {
                    char    *pszBytea = GeometryToBYTEA( poGeom );

                    if( pszBytea != NULL )
                    {
                        if (poDS->bUseEscapeStringSyntax)
                            osCommand += "E";
                        osCommand = osCommand + "'" + pszBytea + "'";
                        CPLFree( pszBytea );
                    }
                    else
                        osCommand += "NULL";
                }
                else
                {
                    Oid     oid = GeometryToOID( poGeom );

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
        }
        else if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY ||
                 poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY )
        {
            if( bNeedComma )
                osCommand += ", ";
            else
                bNeedComma = TRUE;

            osCommand += OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef());
            osCommand += " = ";
            if( poGeom != NULL )
            {
                poGeom->closeRings();
                poGeom->setCoordinateDimension( poGeomFieldDefn->nCoordDimension );
            }

            if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
            {
                if ( poGeom != NULL )
                {
                    char* pszHexEWKB = GeometryToHex( poGeom, poGeomFieldDefn->nSRSId );
                    if ( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
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

                int nSRSId = poGeomFieldDefn->nSRSId;
                if( pszWKT != NULL )
                {
                    if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
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
        }
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
    GetLayerDefn()->GetFieldCount();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateFeature");
        return OGRERR_FAILURE;
    }

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

    if( bFirstInsertion )
    {
        bFirstInsertion = FALSE;
        if( CSLTestBoolean(CPLGetConfigOption("OGR_TRUNCATE", "NO")) )
        {
            PGconn              *hPGConn = poDS->GetPGConn();
            PGresult            *hResult;
            CPLString            osCommand;

            osCommand.Printf("TRUNCATE TABLE %s", pszSqlTableName );
            hResult = OGRPG_PQexec( hPGConn, osCommand.c_str() );
            OGRPGClearResult( hResult );
        }
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
        {
            /* This is a heuristics. If the first feature to be copied has a */
            /* FID set (and that a FID column has been identified), then we will */
            /* try to copy FID values from features. Otherwise, we will not */
            /* do and assume that the FID column is an autoincremented column. */
            StartCopy(poFeature->GetFID() != OGRNullFID);
        }

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
                            const char* pszTableName,
                            const char* pszFieldName )
{
    CPLString osCommand;

    /* We need to quote and escape string fields. */
    osCommand += "'";

    int nSrcLen = strlen(pszStrValue);
    if (nMaxLength > 0 && nSrcLen > nMaxLength)
    {
        CPLDebug( "PG",
                  "Truncated %s.%s field value '%s' to %d characters.",
                  pszTableName, pszFieldName, pszStrValue, nMaxLength );
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
                osStr += OGRPGEscapeString(hPGConn, pszStr);
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

    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == NULL )
            continue;
        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            osCommand += ", ";
        osCommand += OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef()) + " ";
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
    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == NULL )
            continue;
        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY ||
            poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY )
        {
            CheckGeomTypeCompatibility(i, poGeom);

            poGeom->closeRings();
            poGeom->setCoordinateDimension( poGeomFieldDefn->nCoordDimension );

            int nSRSId = poGeomFieldDefn->nSRSId;

            if ( !CSLTestBoolean(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
            {
                char    *pszHexEWKB = GeometryToHex( poGeom, nSRSId );
                if ( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
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
                    if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
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
        }
        else if( !bWkbAsOid )
        {
            char    *pszBytea = GeometryToBYTEA( poGeom );

            if( pszBytea != NULL )
            {
                if (poDS->bUseEscapeStringSyntax)
                    osCommand += "E";
                osCommand = osCommand + "'" + pszBytea + "'";
                CPLFree( pszBytea );
            }
            else
                osCommand += "''";
        }
        else if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_WKB &&
                 bWkbAsOid && poGeom != NULL )
        {
            Oid     oid = GeometryToOID( poGeom );

            if( oid != 0 )
            {
                osCommand += CPLString().Printf( "'%d' ", oid );
            }
            else
                osCommand += "''";
        }
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
    int                  i;

    /* First process geometry */
    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);

        char *pszGeom = NULL;
        if ( NULL != poGeom )
        {
            CheckGeomTypeCompatibility(i, poGeom);

            poGeom->closeRings();
            poGeom->setCoordinateDimension( poGeomFieldDefn->nCoordDimension );

            if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_WKB )
                pszGeom = GeometryToBYTEA( poGeom );
            else
                pszGeom = GeometryToHex( poGeom, poGeomFieldDefn->nSRSId );
        }

        if (osCommand.size() > 0)
            osCommand += "\t";

        if ( pszGeom )
        {
            osCommand += pszGeom;
            CPLFree( pszGeom );
        }
        else
        {
            osCommand += "\\N";
        }
    }
    
    /* Next process the field id column */
    int nFIDIndex = -1;
    if( bFIDColumnInCopyFields )
    {
        if (osCommand.size() > 0)
            osCommand += "\t";

        nFIDIndex = poFeatureDefn->GetFieldIndex( pszFIDColumn );

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
    int bAddTab = osCommand.size() > 0;

    for( i = 0; i < nFieldCount;  i++ )
    {
        if (i == nFIDIndex)
            continue;

        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if (bAddTab)
            osCommand += "\t";
        bAddTab = TRUE;

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
                              "Truncated %s.%s field value '%s' to %d characters.",
                              poFeatureDefn->GetName(),
                              poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                              pszStrValue,
                              poFeatureDefn->GetFieldDefn(i)->GetWidth() );
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
    if ( bUpdateAccess )
    {
        if( EQUAL(pszCap,OLCSequentialWrite) ||
            EQUAL(pszCap,OLCCreateField) ||
            EQUAL(pszCap,OLCCreateGeomField) ||
            EQUAL(pszCap,OLCDeleteField) ||
            EQUAL(pszCap,OLCAlterFieldDefn) )
            return TRUE;

        else if( EQUAL(pszCap,OLCRandomWrite) ||
                 EQUAL(pszCap,OLCDeleteFeature) )
        {
            GetLayerDefn()->GetFieldCount();
            return bHasFid;
        }
    }

    if( EQUAL(pszCap,OLCRandomRead) )
    {
        GetLayerDefn()->GetFieldCount();
        return bHasFid;
    }

    else if( EQUAL(pszCap,OLCFastFeatureCount) ||
             EQUAL(pszCap,OLCFastSetNextByIndex) )
    {
        if( m_poFilterGeom == NULL )
            return TRUE;
        OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);
        return poGeomFieldDefn == NULL ||
               poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
               poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY;
    }

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);
        return poGeomFieldDefn == NULL ||
               poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
               poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY;
    }

    else if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(0);
        return poGeomFieldDefn != NULL &&
               poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY;
    }

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

    GetLayerDefn()->GetFieldCount();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateField");
        return OGRERR_FAILURE;
    }

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


    const char* pszOverrideType = CSLFetchNameValue(papszOverrideColumnTypes, oField.GetNameRef());
    if( pszOverrideType != NULL )
        osFieldType = pszOverrideType;
    else
    {
        osFieldType = OGRPGTableLayerGetType(oField, bPreservePrecision, bApproxOK);
        if (osFieldType.size() == 0)
            return OGRERR_FAILURE;
    }

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
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                         int bApproxOK )
{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if( eType == wkbNone )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }
    
    CPLString               osCommand;
    OGRPGGeomFieldDefn *poGeomField =
        new OGRPGGeomFieldDefn( this, poGeomFieldIn->GetNameRef() );
    poGeomField->SetSpatialRef(poGeomFieldIn->GetSpatialRef());

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( poGeomField->GetNameRef() );

        poGeomField->SetName( pszSafeName );
        CPLFree( pszSafeName );
    }
    
    OGRSpatialReference* poSRS = poGeomField->GetSpatialRef();
    int nSRSId = poDS->GetUndefinedSRID();
    if( nForcedSRSId != UNDETERMINED_SRID )
        nSRSId = nForcedSRSId;
    else if( poSRS != NULL )
        nSRSId = poDS->FetchSRSId( poSRS );

    int nDimension = 3;
    if( wkbFlatten(eType) == eType )
        nDimension = 2;
    if( nForcedDimension > 0 )
    {
        nDimension = nForcedDimension;
        if( nDimension == 2 )
            eType = (OGRwkbGeometryType)( eType & ~wkb25DBit );
        else
            eType = (OGRwkbGeometryType)( eType | wkb25DBit );
    }
    poGeomField->SetType(eType);
    poGeomField->nSRSId = nSRSId;
    poGeomField->nCoordDimension = nDimension;
    poGeomField->ePostgisType = GEOM_TYPE_GEOMETRY;

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    poDS->FlushSoftTransaction();
    
    const char *pszGeometryType = OGRToOGCGeomType(poGeomField->GetType());
    osCommand.Printf(
            "SELECT AddGeometryColumn(%s,%s,%s,%d,'%s',%d)",
            OGRPGEscapeString(hPGConn, pszSchemaName).c_str(),
            OGRPGEscapeString(hPGConn, poFeatureDefn->GetName()).c_str(),
            OGRPGEscapeString(hPGConn, poGeomField->GetNameRef()).c_str(),
            nSRSId, pszGeometryType, nDimension );

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

    if( !hResult
        || PQresultStatus(hResult) != PGRES_TUPLES_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "AddGeometryColumn failed." );

        OGRPGClearResult( hResult );
        delete poGeomField;

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    if( bCreateSpatialIndexFlag )
    {
        osCommand.Printf("CREATE INDEX %s ON %s USING GIST (%s)",
                        OGRPGEscapeColumnName(
                            CPLSPrintf("%s_%s_geom_idx", GetName(), poGeomField->GetNameRef())).c_str(),
                        pszSqlTableName,
                        OGRPGEscapeColumnName(poGeomField->GetNameRef()).c_str());

        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

        if( !hResult
            || PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "CREATE INDEX failed." );

            OGRPGClearResult( hResult );
            delete poGeomField;

            return OGRERR_FAILURE;
        }
        
        OGRPGClearResult( hResult );
    }

    poFeatureDefn->AddGeomFieldDefn( poGeomField, FALSE );

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

    GetLayerDefn()->GetFieldCount();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteField");
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

    GetLayerDefn()->GetFieldCount();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "AlterFieldDefn");
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
    GetLayerDefn()->GetFieldCount();

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
/*                             ResolveSRID()                            */
/************************************************************************/

void OGRPGTableLayer::ResolveSRID(OGRPGGeomFieldDefn* poGFldDefn)

{
    PGconn      *hPGConn = poDS->GetPGConn();
    PGresult    *hResult = NULL;
    CPLString    osCommand;

    int nSRSId = poDS->GetUndefinedSRID();

    poDS->SoftStartTransaction();

    osCommand.Printf(
                "SELECT srid FROM geometry_columns "
                "WHERE f_table_name = %s AND "
                "f_geometry_column = %s",
                OGRPGEscapeString(hPGConn, pszTableName).c_str(),
                OGRPGEscapeString(hPGConn, poGFldDefn->GetNameRef()).c_str());

    if (pszSchemaName)
    {
        osCommand += CPLString().Printf(" AND f_table_schema = %s",
                                        OGRPGEscapeString(hPGConn, pszSchemaName).c_str());
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

    poGFldDefn->nSRSId = nSRSId;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

OGRErr OGRPGTableLayer::StartCopy(int bSetFID)

{
    /* Tell the datasource we are now planning to copy data */
    poDS->StartCopy( this ); 

    CPLString osFields = BuildCopyFields(bSetFID);

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

CPLString OGRPGTableLayer::BuildCopyFields(int bSetFID)
{
    int     i = 0;
    int     nFIDIndex = -1;
    CPLString osFieldList;

    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->myGetGeomFieldDefn(i);
        if( osFieldList.size() > 0 )
            osFieldList += ", ";
        osFieldList += OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef());
    }

    bFIDColumnInCopyFields = (pszFIDColumn != NULL && bSetFID);
    if( bFIDColumnInCopyFields )
    {
        if( osFieldList.size() > 0 )
            osFieldList += ", ";

        nFIDIndex = poFeatureDefn->GetFieldIndex( pszFIDColumn );

        osFieldList += OGRPGEscapeColumnName(pszFIDColumn);
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if (i == nFIDIndex)
            continue;

        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( osFieldList.size() > 0 )
            osFieldList += ", ";

        osFieldList += OGRPGEscapeColumnName(pszName);
    }

    return osFieldList;
}

/************************************************************************/
/*                    CheckGeomTypeCompatibility()                      */
/************************************************************************/

void OGRPGTableLayer::CheckGeomTypeCompatibility(int iGeomField,
                                                 OGRGeometry* poGeom)
{
    if (bHasWarnedIncompatibleGeom)
        return;

    OGRwkbGeometryType eExpectedGeomType =
        poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetType();
    OGRwkbGeometryType eFlatLayerGeomType = wkbFlatten(eExpectedGeomType);
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
                 OGRGeometryTypeToName(eExpectedGeomType));
    }
}

/************************************************************************/
/*                        SetOverrideColumnTypes()                      */
/************************************************************************/

void OGRPGTableLayer::SetOverrideColumnTypes( const char* pszOverrideColumnTypes )
{
    if( pszOverrideColumnTypes == NULL )
        return;

    const char* pszIter = pszOverrideColumnTypes;
    CPLString osCur;
    while(*pszIter != '\0')
    {
        if( *pszIter == '(' )
        {
            /* Ignore commas inside ( ) pair */
            while(*pszIter != '\0')
            {
                if( *pszIter == ')' )
                {
                    osCur += *pszIter;
                    pszIter ++;
                    break;
                }
                osCur += *pszIter;
                pszIter ++;
            }
            if( *pszIter == '\0')
                break;
        }

        if( *pszIter == ',' )
        {
            papszOverrideColumnTypes = CSLAddString(papszOverrideColumnTypes, osCur);
            osCur = "";
        }
        else
            osCur += *pszIter;
        pszIter ++;
    }
    if( osCur.size() )
        papszOverrideColumnTypes = CSLAddString(papszOverrideColumnTypes, osCur);
}

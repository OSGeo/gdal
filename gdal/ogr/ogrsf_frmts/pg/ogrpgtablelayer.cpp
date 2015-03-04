/******************************************************************************
 * $Id$

 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_p.h"

#define PQexec this_is_an_error

CPL_CVSID("$Id$");


#define USE_COPY_UNSET  -10

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
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRPGTableLayer::GetFIDColumn() 

{
    ReadTableDefinition();

    if( pszFIDColumn != NULL )
        return pszFIDColumn;
    else
        return "";
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

    bGeometryInformationSet = FALSE;

    bLaunderColumnNames = TRUE;
    bPreservePrecision = TRUE;
    bCopyActive = FALSE;
    bUseCopy = USE_COPY_UNSET;  // unknown
    bUseCopyByDefault = FALSE;
    bFIDColumnInCopyFields = FALSE;
    bFirstInsertion = TRUE;

    pszTableName = CPLStrdup( pszTableNameIn );
    if (pszSchemaNameIn)
        pszSchemaName = CPLStrdup( pszSchemaNameIn );
    else
        pszSchemaName = CPLStrdup( osCurrentSchema );
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
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    
    bAutoFIDOnCreateViaCopy = FALSE;
    
    bDifferedCreation = FALSE;
    iFIDAsRegularColumnIndex = -1;
}

//************************************************************************/
/*                          ~OGRPGTableLayer()                          */
/************************************************************************/

OGRPGTableLayer::~OGRPGTableLayer()

{
    if( bDifferedCreation ) RunDifferedCreationIfNecessary();
    if ( bCopyActive ) EndCopy();
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
        poGeomFieldDefn->SetNullable(pasDesc[i].bNullable);
        poGeomFieldDefn->nSRSId = pasDesc[i].nSRID;
        poGeomFieldDefn->nCoordDimension = pasDesc[i].nCoordDimension;
        poGeomFieldDefn->ePostgisType = pasDesc[i].ePostgisType;
        if( pasDesc[i].pszGeomType != NULL )
        {
            OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(pasDesc[i].pszGeomType);
            if( poGeomFieldDefn->nCoordDimension == 3 && eGeomType != wkbUnknown )
                eGeomType = wkbSetZ(eGeomType);
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

    CPLString osSchemaClause;
    osSchemaClause.Printf("AND n.nspname=%s",
                              OGRPGEscapeString(hPGConn, pszSchemaName).c_str());

    const char* pszTypnameEqualsAnyClause;
    if (poDS->sPostgreSQLVersion.nMajor == 7 && poDS->sPostgreSQLVersion.nMinor <= 3)
        pszTypnameEqualsAnyClause = "ANY(SELECT '{int2, int4, int8, serial, bigserial}')";
    else
        pszTypnameEqualsAnyClause = "ANY(ARRAY['int2','int4','int8','serial','bigserial'])";

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
            /* Check if single-field PK can be represented as integer. */
            CPLString osValue(PQgetvalue(hResult, 0, 3));
            if( osValue == "t" )
            {
                osPrimaryKey.Printf( "%s", PQgetvalue(hResult,0,0) );
                const char* pszFIDType = PQgetvalue(hResult, 0, 2);
                CPLDebug( "PG", "Primary key name (FID): %s, type : %s",
                          osPrimaryKey.c_str(), pszFIDType );
                if( EQUAL(pszFIDType, "int8") )
                    SetMetadataItem(OLMD_FID64, "YES");
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
    osCommand.Printf(
                "SELECT DISTINCT a.attname, t.typname, a.attlen,"
                "       format_type(a.atttypid,a.atttypmod), a.attnum, a.attnotnull, a.atthasdef "
                "FROM pg_class c, pg_attribute a, pg_type t, pg_namespace n "
                "WHERE c.relname = %s "
                "AND a.attnum > 0 AND a.attrelid = c.oid "
                "AND a.atttypid = t.oid "
                "AND c.relnamespace=n.oid "
                "%s "
                "ORDER BY a.attnum",
                pszEscapedTableNameSingleQuote, osSchemaClause.c_str());

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

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

        CPLDebug( "PG",
                  "No field definitions found for '%s', is it a table?",
                  pszTableName );
        return bTableDefinitionValid;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    int            iRecord;
    int            bHasDefault = FALSE;
    for( iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
    {
        const char      *pszType = NULL;
        const char      *pszFormatType = NULL;
        const char      *pszNotNull = NULL;
        const char      *pszHasDef = NULL;
        OGRFieldDefn    oField( PQgetvalue( hResult, iRecord, 0 ), OFTString);

        pszType = PQgetvalue(hResult, iRecord, 1 );
        int nWidth = atoi(PQgetvalue(hResult,iRecord,2));
        pszFormatType = PQgetvalue(hResult,iRecord,3);
        pszNotNull = PQgetvalue(hResult,iRecord,5);
        pszHasDef = PQgetvalue(hResult,iRecord,6);

        if( pszNotNull && EQUAL(pszNotNull, "t") )
            oField.SetNullable(FALSE);
        if( pszHasDef && EQUAL(pszHasDef, "t") )
            bHasDefault = TRUE;

        if( EQUAL(oField.GetNameRef(),osPrimaryKey) )
        {
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
                poGeomFieldDefn->SetNullable(oField.IsNullable());
                if( !bGeometryInformationSet )
                    poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
            }
            continue;
        }

        OGRPGCommonLayerSetType(oField, pszType, pszFormatType, nWidth);

        //CPLDebug("PG", "name=%s, type=%s", oField.GetNameRef(), pszType);
        poFeatureDefn->AddFieldDefn( &oField );
    }

    OGRPGClearResult( hResult );

    if( bHasDefault )
    {
        osCommand.Printf(
                 "SELECT a.attname, pg_get_expr(def.adbin, c.oid) "
                 "FROM pg_attrdef def, pg_class c, pg_attribute a, pg_type t, pg_namespace n "
                 "WHERE c.relname = %s AND a.attnum > 0 AND a.attrelid = c.oid "
                 "AND a.atttypid = t.oid AND c.relnamespace=n.oid AND "
                 "def.adrelid = c.oid AND def.adnum = a.attnum "
                 "%s "
                 "ORDER BY a.attnum",
                 pszEscapedTableNameSingleQuote, osSchemaClause.c_str());

        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );
        if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
        {
            OGRPGClearResult( hResult );

            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s", PQerrorMessage(hPGConn) );
            return bTableDefinitionValid;
        }

        for( iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
        {
            const char      *pszName = PQgetvalue( hResult, iRecord, 0 );
            const char      *pszDefault = PQgetvalue( hResult, iRecord, 1 );
            int nIdx = poFeatureDefn->GetFieldIndex(pszName);
            if( nIdx >= 0 )
            {
                OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(nIdx);
                OGRPGCommonLayerNormalizeDefault(poFieldDefn, pszDefault);
            }
        }

        OGRPGClearResult( hResult );
    }

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

        osCommand += CPLString().Printf(" AND f_table_schema = %s",
                                            OGRPGEscapeString(hPGConn,pszSchemaName).c_str());

        hResult = OGRPG_PQexec(hPGConn,osCommand);

        if ( hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult,0,0) )
        {
            const char* pszType = PQgetvalue(hResult,0,0);

            int nCoordDimension = atoi(PQgetvalue(hResult,0,1));

            int nSRSId = atoi(PQgetvalue(hResult,0,2));

            poGeomFieldDefn->nCoordDimension = nCoordDimension;
            if( nSRSId > 0 )
                poGeomFieldDefn->nSRSId = nSRSId;
            OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(pszType);
            if( poGeomFieldDefn->nCoordDimension == 3 && eGeomType != wkbUnknown )
                eGeomType = wkbSetZ(eGeomType);
            poGeomFieldDefn->SetType(eGeomType);

            bGoOn = FALSE;
        }
        else
        {
            /* Fetch the name of the parent table */
            osCommand.Printf("SELECT pg_class.relname FROM pg_class WHERE oid = "
                            "(SELECT pg_inherits.inhparent FROM pg_inherits WHERE inhrelid = "
                            "(SELECT c.oid FROM pg_class c, pg_namespace n WHERE c.relname = %s AND c.relnamespace=n.oid AND n.nspname = %s))",
                            pszEscapedTableNameSingleQuote,
                            OGRPGEscapeString(hPGConn, pszSchemaName).c_str() );

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
/*                         SetTableDefinition()                         */
/************************************************************************/

void OGRPGTableLayer::SetTableDefinition(const char* pszFIDColumnName,
                                           const char* pszGFldName,
                                           OGRwkbGeometryType eType,
                                           const char* pszGeomType,
                                           int nSRSId,
                                           int nCoordDimension)
{
    bTableDefinitionValid = TRUE;
    bGeometryInformationSet = TRUE;
    if( pszFIDColumnName[0] == '"' &&
             pszFIDColumnName[strlen(pszFIDColumnName)-1] == '"')
    {
        pszFIDColumn = CPLStrdup(pszFIDColumnName + 1);
        pszFIDColumn[strlen(pszFIDColumn)-1] = '\0';
    }
    else
        pszFIDColumn = CPLStrdup(pszFIDColumnName);
    poFeatureDefn->SetGeomType(wkbNone);
    if( eType != wkbNone )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = new OGRPGGeomFieldDefn(this, pszGFldName);
        poGeomFieldDefn->SetType(eType);
        poGeomFieldDefn->nCoordDimension = nCoordDimension;

        if( EQUAL(pszGeomType,"geometry") )
        {
            poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOMETRY;
            poGeomFieldDefn->nSRSId = nSRSId;
        }
        else if( EQUAL(pszGeomType,"geography") )
        {
            poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOGRAPHY;
            poGeomFieldDefn->nSRSId = 4326;
        }
        else
        {
            poGeomFieldDefn->ePostgisType = GEOM_TYPE_WKB;
            if( EQUAL(pszGeomType,"OID") )
                bWkbAsOid = TRUE;
        }
        poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
    }
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
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
        {
            if( sEnvelope.MinX < -180.0 )
                sEnvelope.MinX = -180.0;
            if( sEnvelope.MinY < -90.0 )
                sEnvelope.MinY = -90.0;
            if( sEnvelope.MaxX > 180.0 )
                sEnvelope.MaxX = 180.0;
            if( sEnvelope.MaxY > 90.0 )
                sEnvelope.MaxY = 90.0;
        }
        CPLsnprintf(szBox3D_1, sizeof(szBox3D_1), "%.18g %.18g", sEnvelope.MinX, sEnvelope.MinY);
        CPLsnprintf(szBox3D_2, sizeof(szBox3D_2), "%.18g %.18g", sEnvelope.MaxX, sEnvelope.MaxY);
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

    if( bDifferedCreation ) RunDifferedCreationIfNecessary();
    poDS->EndCopy();
    bUseCopyByDefault = FALSE;

    BuildFullQueryStatement();

    OGRPGLayer::ResetReading();

    bInResetReading = FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGTableLayer::GetNextFeature()

{
    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return NULL;
    poDS->EndCopy();

    if( pszQueryStatement == NULL )
        ResetReading();

    if( pszQueryStatement == NULL )
        ResetReading();

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
        {
            if( poFeature && iFIDAsRegularColumnIndex >= 0 )
            {
                poFeature->SetField(iFIDAsRegularColumnIndex, poFeature->GetFID());
            }
            return poFeature;
        }

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

    if( pszFIDColumn != NULL && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
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
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

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

OGRErr OGRPGTableLayer::DeleteFeature( GIntBig nFID )

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

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();
    bAutoFIDOnCreateViaCopy = FALSE;

/* -------------------------------------------------------------------- */
/*      We can only delete features if we have a well defined FID       */
/*      column to target.                                               */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature(" CPL_FRMT_GIB ") failed.  Unable to delete features in tables without\n"
                  "a recognised FID column.",
                  nFID );
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Form the statement to drop the record.                          */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "DELETE FROM %s WHERE %s = " CPL_FRMT_GIB,
                      pszSqlTableName, OGRPGEscapeColumnName(pszFIDColumn).c_str(), nFID );

/* -------------------------------------------------------------------- */
/*      Execute the delete.                                             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    hResult = OGRPG_PQexec(hPGConn, osCommand);

    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature() DELETE statement failed.\n%s",
                  PQerrorMessage(hPGConn) );

        OGRPGClearResult( hResult );

        eErr = OGRERR_FAILURE;
    }
    else
    {
        OGRPGClearResult( hResult );

        eErr = OGRERR_NONE;
    }

    return eErr;
}

/************************************************************************/
/*                             ISetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by an UPDATE SQL command            */
/************************************************************************/

OGRErr OGRPGTableLayer::ISetFeature( OGRFeature *poFeature )

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

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

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

    if( pszFIDColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to update features in tables without\n"
                  "a recognised FID column.");
        return eErr;

    }

    /* In case the FID column has also been created as a regular field */
    if( iFIDAsRegularColumnIndex >= 0 )
    {
        if( !poFeature->IsFieldSet( iFIDAsRegularColumnIndex ) ||
            poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex) != poFeature->GetFID() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Inconsistant values of FID and field of same name");
            return CE_Failure;
        }
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
                    char    *pszBytea = GeometryToBYTEA( poGeom, poDS->sPostGISVersion.nMajor < 2 );

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
                    char* pszHexEWKB = OGRGeometryToHexEWKB( poGeom, poGeomFieldDefn->nSRSId,
                                                             poDS->sPostGISVersion.nMajor < 2 );
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
        if( iFIDAsRegularColumnIndex == i )
            continue;
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
            OGRPGCommonAppendFieldValue(osCommand, poFeature, i,
                                        (OGRPGCommonEscapeStringCbk)OGRPGEscapeString, hPGConn);
        }
    }

    /* Add the WHERE clause */
    osCommand += " WHERE ";
    osCommand = osCommand + OGRPGEscapeColumnName(pszFIDColumn) + " = ";
    osCommand += CPLString().Printf( CPL_FRMT_GIB, poFeature->GetFID() );

/* -------------------------------------------------------------------- */
/*      Execute the update.                                             */
/* -------------------------------------------------------------------- */
    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "UPDATE command for feature " CPL_FRMT_GIB " failed.\n%s\nCommand: %s",
                  poFeature->GetFID(), PQerrorMessage(hPGConn), osCommand.c_str() );

        OGRPGClearResult( hResult );

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRPGTableLayer::ICreateFeature( OGRFeature *poFeature )
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

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    /* In case the FID column has also been created as a regular field */
    if( iFIDAsRegularColumnIndex >= 0 )
    {
        if( poFeature->GetFID() == OGRNullFID )
        {
            if( poFeature->IsFieldSet( iFIDAsRegularColumnIndex ) )
            {
                poFeature->SetFID(
                    poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex));
            }
        }
        else
        {
            if( !poFeature->IsFieldSet( iFIDAsRegularColumnIndex ) ||
                poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex) != poFeature->GetFID() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Inconsistant values of FID and field of same name");
                return CE_Failure;
            }
        }
    }

    /* Auto-promote FID column to 64bit if necessary */
    if( pszFIDColumn != NULL &&
        (GIntBig)(int)poFeature->GetFID() != poFeature->GetFID() &&
        GetMetadataItem(OLMD_FID64) == NULL )
    {
        poDS->EndCopy();

        CPLString osCommand;
        osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s TYPE INT8",
                        pszSqlTableName,
                        OGRPGEscapeColumnName(pszFIDColumn).c_str() );
        PGconn              *hPGConn = poDS->GetPGConn();
        PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
        if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s\n%s",
                    osCommand.c_str(),
                    PQerrorMessage(hPGConn) );

            OGRPGClearResult( hResult );

            return OGRERR_FAILURE;
        }
        OGRPGClearResult( hResult );
        
        SetMetadataItem(OLMD_FID64, "YES");
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

    OGRErr eErr;
    if( !bUseCopy )
    {
        eErr = CreateFeatureViaInsert( poFeature );
    }
    else
    {
        /* If there's a unset field with a default value, then we must use */
        /* a specific INSERT statement to avoid unset fields to be bound to NULL */
        int bHasDefaultValue = FALSE;
        int iField;
        int nFieldCount = poFeatureDefn->GetFieldCount();
        for( iField = 0; iField < nFieldCount; iField++ )
        {
            if( !poFeature->IsFieldSet( iField ) &&
                poFeature->GetFieldDefnRef(iField)->GetDefault() != NULL )
            {
                bHasDefaultValue = TRUE;
                break;
            }
        }
        if( bHasDefaultValue )
        {
            eErr = CreateFeatureViaInsert( poFeature );
        }
        else
        {
            int bFIDSet = (pszFIDColumn != NULL && poFeature->GetFID() != OGRNullFID);
            if( bCopyActive && bFIDSet != bFIDColumnInCopyFields )
            {
                eErr = CreateFeatureViaInsert( poFeature );
            }
            else if( !bCopyActive && poFeatureDefn->GetFieldCount() == 0 &&
                     poFeatureDefn->GetGeomFieldCount() == 0 && !bFIDSet )
            {
                eErr = CreateFeatureViaInsert( poFeature );
            }
            else
            {
                if ( !bCopyActive )
                {
                    /* This is a heuristics. If the first feature to be copied has a */
                    /* FID set (and that a FID column has been identified), then we will */
                    /* try to copy FID values from features. Otherwise, we will not */
                    /* do and assume that the FID column is an autoincremented column. */
                    bFIDColumnInCopyFields = bFIDSet;
                }

                eErr = CreateFeatureViaCopy( poFeature );
                if( bFIDSet )
                    bAutoFIDOnCreateViaCopy = FALSE;
                if( eErr == CE_None && bAutoFIDOnCreateViaCopy )
                {
                    poFeature->SetFID( ++iNextShapeId );
                }
            }
        }
    }

    if( eErr == CE_None && iFIDAsRegularColumnIndex >= 0 )
    {
        poFeature->SetField(iFIDAsRegularColumnIndex, poFeature->GetFID());
    }

    return eErr;
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
    int nSrcLenUTF = CPLStrlenUTF8(pszStrValue);

    if (nMaxLength > 0 && nSrcLenUTF > nMaxLength)
    {
        CPLDebug( "PG",
                  "Truncated %s.%s field value '%s' to %d characters.",
                  pszTableName, pszFieldName, pszStrValue, nMaxLength );

        int iUTF8Char = 0;
        for(int iChar = 0; iChar < nSrcLen; iChar++ )
        {
            if( (((unsigned char *) pszStrValue)[iChar] & 0xc0) != 0x80 )
            {
                if( iUTF8Char == nMaxLength )
                {
                    nSrcLen = iChar;
                    break;
                }
                iUTF8Char ++;
            }
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
/*                       CreateFeatureViaInsert()                       */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateFeatureViaInsert( OGRFeature *poFeature )

{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    CPLString           osCommand;
    int                 i;
    int                 bNeedComma = FALSE;

    int bEmptyInsert = FALSE;

    poDS->EndCopy();

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
        if( iFIDAsRegularColumnIndex == i )
            continue;
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
                char    *pszHexEWKB = OGRGeometryToHexEWKB( poGeom, nSRSId,
                                                            poDS->sPostGISVersion.nMajor < 2 );
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
            char    *pszBytea = GeometryToBYTEA( poGeom, poDS->sPostGISVersion.nMajor < 2 );

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
        osCommand += CPLString().Printf( CPL_FRMT_GIB " ", poFeature->GetFID() );
        bNeedComma = TRUE;
    }


    for( i = 0; i < nFieldCount; i++ )
    {
        if( iFIDAsRegularColumnIndex == i )
            continue;
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        OGRPGCommonAppendFieldValue(osCommand, poFeature, i,
                                    (OGRPGCommonEscapeStringCbk)OGRPGEscapeString, hPGConn);
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
        poFeature->SetFID(CPLAtoGIntBig(pszFID));
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

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    return OGRERR_NONE;
}

/************************************************************************/
/*                        CreateFeatureViaCopy()                        */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateFeatureViaCopy( OGRFeature *poFeature )
{
    PGconn              *hPGConn = poDS->GetPGConn();
    CPLString            osCommand;
    int                  i;

    /* Tell the datasource we are now planning to copy data */
    poDS->StartCopy( this ); 

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
                pszGeom = GeometryToBYTEA( poGeom, poDS->sPostGISVersion.nMajor < 2 );
            else
                pszGeom = OGRGeometryToHexEWKB( poGeom, poGeomFieldDefn->nSRSId,
                                                poDS->sPostGISVersion.nMajor < 2 );
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
    
    OGRPGCommonAppendCopyFieldsExceptGeom(osCommand,
                                          poFeature,
                                          pszFIDColumn,
                                          bFIDColumnInCopyFields,
                                          (OGRPGCommonEscapeStringCbk)OGRPGEscapeString,
                                          hPGConn);

    /* Add end of line marker */
    osCommand += "\n";


    /* ------------------------------------------------------------ */
    /*      Execute the copy.                                       */
    /* ------------------------------------------------------------ */

    OGRErr result = OGRERR_NONE;

    /* This is for postgresql  7.4 and higher */
#if !defined(PG_PRE74)
    int copyResult = PQputCopyData(hPGConn, osCommand.c_str(), strlen(osCommand.c_str()));
#ifdef DEBUG_VERBOSE
    CPLDebug("PG", "PQputCopyData(%s)", osCommand.c_str());
#endif

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
            return pszFIDColumn != NULL;
        }
    }

    if( EQUAL(pszCap,OLCRandomRead) )
    {
        GetLayerDefn()->GetFieldCount();
        return pszFIDColumn != NULL;
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

    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;

    else
        return FALSE;
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
    
    if( pszFIDColumn != NULL &&
        EQUAL( oField.GetNameRef(), pszFIDColumn ) &&
        oField.GetType() != OFTInteger &&
        oField.GetType() != OFTInteger64 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oField.GetNameRef());
        return CE_Failure;
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
        osFieldType = OGRPGCommonLayerGetType(oField, bPreservePrecision, bApproxOK);
        if (osFieldType.size() == 0)
            return OGRERR_FAILURE;
    }

    CPLString osNotNullDefault;
    if( !oField.IsNullable() )
        osNotNullDefault += " NOT NULL";
    if( oField.GetDefault() != NULL && !oField.IsDefaultDriverSpecific() )
    {
        osNotNullDefault += " DEFAULT ";
        osNotNullDefault += OGRPGCommonLayerGetPGDefault(&oField);
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if( bDifferedCreation )
    {
        if( !(pszFIDColumn != NULL && EQUAL(pszFIDColumn,oField.GetNameRef())) )
        {
            osCreateTable += ", ";
            osCreateTable += OGRPGEscapeColumnName(oField.GetNameRef());
            osCreateTable += " ";
            osCreateTable += osFieldType;
            osCreateTable += osNotNullDefault;
        }
    }
    else
    {
        poDS->EndCopy();

        osCommand.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                        pszSqlTableName, OGRPGEscapeColumnName(oField.GetNameRef()).c_str(),
                        osFieldType.c_str() );
        osCommand += osNotNullDefault;
        
        hResult = OGRPG_PQexec(hPGConn, osCommand);
        if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s\n%s", 
                    osCommand.c_str(), 
                    PQerrorMessage(hPGConn) );

            OGRPGClearResult( hResult );

            return OGRERR_FAILURE;
        }

        OGRPGClearResult( hResult );
    }

    poFeatureDefn->AddFieldDefn( &oField );

    if( pszFIDColumn != NULL &&
        EQUAL( oField.GetNameRef(), pszFIDColumn ) )
    {
        iFIDAsRegularColumnIndex = poFeatureDefn->GetFieldCount() - 1;
    }

    return OGRERR_NONE;
}


/************************************************************************/
/*                        RunAddGeometryColumn()                        */
/************************************************************************/

OGRErr OGRPGTableLayer::RunAddGeometryColumn( OGRPGGeomFieldDefn *poGeomField )
{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    CPLString            osCommand;

    const char *pszGeometryType = OGRToOGCGeomType(poGeomField->GetType());
    osCommand.Printf(
            "SELECT AddGeometryColumn(%s,%s,%s,%d,'%s',%d)",
            OGRPGEscapeString(hPGConn, pszSchemaName).c_str(),
            OGRPGEscapeString(hPGConn, pszTableName).c_str(),
            OGRPGEscapeString(hPGConn, poGeomField->GetNameRef()).c_str(),
            poGeomField->nSRSId, pszGeometryType, poGeomField->nCoordDimension );

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

    if( !hResult
        || PQresultStatus(hResult) != PGRES_TUPLES_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "AddGeometryColumn failed for layer %s.",
                  GetName());

        OGRPGClearResult( hResult );

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );
    
    if( !poGeomField->IsNullable() )
    {
        osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s SET NOT NULL",
                          pszSqlTableName,
                          OGRPGEscapeColumnName(poGeomField->GetNameRef()).c_str() );

        hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
        OGRPGClearResult( hResult );
    }

    return OGRERR_NONE;
}


/************************************************************************/
/*                        RunCreateSpatialIndex()                       */
/************************************************************************/

OGRErr OGRPGTableLayer::RunCreateSpatialIndex( OGRPGGeomFieldDefn *poGeomField )
{
    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult;
    CPLString            osCommand;

    osCommand.Printf("CREATE INDEX %s ON %s USING GIST (%s)",
                    OGRPGEscapeColumnName(
                        CPLSPrintf("%s_%s_geom_idx", pszTableName, poGeomField->GetNameRef())).c_str(),
                    pszSqlTableName,
                    OGRPGEscapeColumnName(poGeomField->GetNameRef()).c_str());

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

    if( !hResult
        || PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "CREATE INDEX failed for layer %s.", GetName());

        OGRPGClearResult( hResult );

        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRPGTableLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                         CPL_UNUSED int bApproxOK )
{
    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if( eType == wkbNone )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }
    
    OGRPGGeomFieldDefn *poGeomField =
        new OGRPGGeomFieldDefn( this, poGeomFieldIn->GetNameRef() );
    if( EQUAL(poGeomField->GetNameRef(), "") )
    {
        if( poFeatureDefn->GetGeomFieldCount() == 0 )
            poGeomField->SetName( "wkb_geometry" );
        else
            poGeomField->SetName(
                CPLSPrintf("wkb_geometry%d", poFeatureDefn->GetGeomFieldCount()+1) );
    }
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
        eType = OGR_GT_SetModifier(eType, nDimension == 3, FALSE);
    }
    poGeomField->SetType(eType);
    poGeomField->SetNullable( poGeomFieldIn->IsNullable() );
    poGeomField->nSRSId = nSRSId;
    poGeomField->nCoordDimension = nDimension;
    poGeomField->ePostgisType = GEOM_TYPE_GEOMETRY;

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if( !bDifferedCreation )
    {
        poDS->EndCopy();

        if( RunAddGeometryColumn(poGeomField) != OGRERR_NONE )
        {
            delete poGeomField;

            return OGRERR_FAILURE;
        }

        if( bCreateSpatialIndexFlag )
        {
            if( RunCreateSpatialIndex(poGeomField) != OGRERR_NONE )
            {
                delete poGeomField;

                return OGRERR_FAILURE;
            }
        }
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

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

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

        return OGRERR_FAILURE;
    }

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

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

    OGRFieldDefn       *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
    OGRFieldDefn        oField( poNewFieldDefn );

    poDS->SoftStartTransaction();

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
        CPLString osFieldType = OGRPGCommonLayerGetType(oField,
                                                       bPreservePrecision,
                                                       TRUE);
        if (osFieldType.size() == 0)
        {
            poDS->SoftRollbackTransaction();

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

            poDS->SoftRollbackTransaction();

            return OGRERR_FAILURE;
        }
        OGRPGClearResult( hResult );
    }

    if( (nFlags & ALTER_NULLABLE_FLAG) &&
        poFieldDefn->IsNullable() != poNewFieldDefn->IsNullable() )
    {
        oField.SetNullable(poNewFieldDefn->IsNullable());

        if( poNewFieldDefn->IsNullable() )
            osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s DROP NOT NULL",
                    pszSqlTableName,
                    OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str() );
        else
            osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s SET NOT NULL",
                    pszSqlTableName,
                    OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str() );

        hResult = OGRPG_PQexec(hPGConn, osCommand);
        if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s\n%s",
                    osCommand.c_str(),
                    PQerrorMessage(hPGConn) );

            OGRPGClearResult( hResult );

            poDS->SoftRollbackTransaction();

            return OGRERR_FAILURE;
        }
        OGRPGClearResult( hResult );
    }
    
    if( (nFlags & ALTER_DEFAULT_FLAG) &&
        ((poFieldDefn->GetDefault() == NULL && poNewFieldDefn->GetDefault() != NULL) ||
         (poFieldDefn->GetDefault() != NULL && poNewFieldDefn->GetDefault() == NULL) ||
         (poFieldDefn->GetDefault() != NULL && poNewFieldDefn->GetDefault() != NULL &&
          strcmp(poFieldDefn->GetDefault(), poNewFieldDefn->GetDefault()) != 0)) )
    {
        oField.SetDefault(poNewFieldDefn->GetDefault());

        if( poNewFieldDefn->GetDefault() == NULL )
            osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s DROP DEFAULT",
                    pszSqlTableName,
                    OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str() );
        else
            osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s SET DEFAULT %s",
                    pszSqlTableName,
                    OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str(),
                    OGRPGCommonLayerGetPGDefault(poNewFieldDefn).c_str());

        hResult = OGRPG_PQexec(hPGConn, osCommand);
        if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "%s\n%s",
                    osCommand.c_str(),
                    PQerrorMessage(hPGConn) );

            OGRPGClearResult( hResult );

            poDS->SoftRollbackTransaction();

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

                poDS->SoftRollbackTransaction();

                return OGRERR_FAILURE;
            }
            OGRPGClearResult( hResult );
        }
    }

    poDS->SoftCommitTransaction();

    if (nFlags & ALTER_NAME_FLAG)
        poFieldDefn->SetName(oField.GetNameRef());
    if (nFlags & ALTER_TYPE_FLAG)
        poFieldDefn->SetType(oField.GetType());
    if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
    {
        poFieldDefn->SetWidth(oField.GetWidth());
        poFieldDefn->SetPrecision(oField.GetPrecision());
    }
    if (nFlags & ALTER_NULLABLE_FLAG)
        poFieldDefn->SetNullable(oField.IsNullable());
    if (nFlags & ALTER_DEFAULT_FLAG)
        poFieldDefn->SetDefault(oField.GetDefault());

    return OGRERR_NONE;

}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGTableLayer::GetFeature( GIntBig nFeatureId )

{
    GetLayerDefn()->GetFieldCount();

    if( pszFIDColumn == NULL )
        return OGRLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Issue query for a single record.                                */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = NULL;
    PGresult    *hResult = NULL;
    PGconn      *hPGConn = poDS->GetPGConn();
    CPLString    osFieldList = BuildFields();
    CPLString    osCommand;

    poDS->EndCopy();
    poDS->SoftStartTransaction();

    osCommand.Printf(
             "DECLARE getfeaturecursor %s for "
             "SELECT %s FROM %s WHERE %s = " CPL_FRMT_GIB,
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
                int* panTempMapFieldNameToIndex = NULL;
                int* panTempMapFieldNameToGeomIndex = NULL;
                CreateMapFromFieldNameToIndex(hResult,
                                              poFeatureDefn,
                                              panTempMapFieldNameToIndex,
                                              panTempMapFieldNameToGeomIndex);
                poFeature = RecordToFeature(hResult,
                                            panTempMapFieldNameToIndex,
                                            panTempMapFieldNameToGeomIndex,
                                            0 );
                CPLFree(panTempMapFieldNameToIndex);
                CPLFree(panTempMapFieldNameToGeomIndex);
                if( poFeature && iFIDAsRegularColumnIndex >= 0 )
                {
                    poFeature->SetField(iFIDAsRegularColumnIndex, poFeature->GetFID());
                }

                if (nRows > 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%d rows in response to the WHERE %s = " CPL_FRMT_GIB " clause !",
                             nRows, pszFIDColumn, nFeatureId );
                }
            }
            else
            {
                 CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read feature with unknown feature id (" CPL_FRMT_GIB ").", nFeatureId );
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

    poDS->SoftCommitTransaction();

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRPGTableLayer::GetFeatureCount( int bForce )

{
    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return 0;
    poDS->EndCopy();

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
    GIntBig              nCount = 0;

    osCommand.Printf(
        "SELECT count(*) FROM %s %s",
        pszSqlTableName, osWHERE.c_str() );

    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( hResult != NULL && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        nCount = CPLAtoGIntBig(PQgetvalue(hResult,0,0));
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

    osCommand.Printf(
                "SELECT srid FROM geometry_columns "
                "WHERE f_table_name = %s AND "
                "f_geometry_column = %s",
                OGRPGEscapeString(hPGConn, pszTableName).c_str(),
                OGRPGEscapeString(hPGConn, poGFldDefn->GetNameRef()).c_str());

    osCommand += CPLString().Printf(" AND f_table_schema = %s",
                                    OGRPGEscapeString(hPGConn, pszSchemaName).c_str());

    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

    if( hResult
        && PQresultStatus(hResult) == PGRES_TUPLES_OK
        && PQntuples(hResult) == 1 )
    {
        nSRSId = atoi(PQgetvalue(hResult,0,0));
    }

    OGRPGClearResult( hResult );

    /* With PostGIS 2.0, SRID = 0 can also mean that there's no constraint */
    /* so we need to fetch from values */
    /* We assume that all geometry of this column have identical SRID */
    if( nSRSId <= 0 && poGFldDefn->ePostgisType == GEOM_TYPE_GEOMETRY )
    {
        CPLString osGetSRID;

        const char* psGetSRIDFct;
        if (poDS->sPostGISVersion.nMajor >= 2)
            psGetSRIDFct = "ST_SRID";
        else
            psGetSRIDFct = "getsrid";

        osGetSRID += "SELECT ";
        osGetSRID += psGetSRIDFct;
        osGetSRID += "(";
        osGetSRID += OGRPGEscapeColumnName(poGFldDefn->GetNameRef());
        osGetSRID += ") FROM ";
        osGetSRID += pszSqlTableName;
        osGetSRID += " LIMIT 1";

        hResult = OGRPG_PQexec(poDS->GetPGConn(), osGetSRID );
        if( hResult
            && PQresultStatus(hResult) == PGRES_TUPLES_OK
            && PQntuples(hResult) == 1 )
        {
            nSRSId = atoi(PQgetvalue(hResult,0,0));
        }

        OGRPGClearResult( hResult );
    }

    poGFldDefn->nSRSId = nSRSId;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

OGRErr OGRPGTableLayer::StartCopy()

{
    /*CPLDebug("PG", "OGRPGDataSource(%p)::StartCopy(%p)", poDS, this);*/

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
    /*CPLDebug("PG", "OGRPGDataSource(%p)::EndCopy(%p)", poDS, this);*/

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

    if( !bUseCopyByDefault )
        bUseCopy = USE_COPY_UNSET;

    return result;
}

/************************************************************************/
/*                          BuildCopyFields()                           */
/************************************************************************/

CPLString OGRPGTableLayer::BuildCopyFields()
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

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      For PostGIS use internal ST_EstimatedExtent(geometry) function  */
/*      if bForce == 0                                                  */
/************************************************************************/

OGRErr OGRPGTableLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    CPLString   osCommand;

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

    OGRPGGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->myGetGeomFieldDefn(iGeomField);

    const char* pszExtentFct;
    // if bForce is 0 and ePostgisType is not GEOM_TYPE_GEOGRAPHY we can use 
    // the ST_EstimatedExtent function which is quicker
    // ST_EstimatedExtent was called ST_Estimated_Extent up to PostGIS 2.0.x
    // ST_EstimatedExtent returns NULL in absence of statistics (an exception before 
    //   PostGIS 1.5.4)
    if ( bForce == 0 && TestCapability(OLCFastGetExtent) )
    {
        PGconn              *hPGConn = poDS->GetPGConn();

        if ( poDS->sPostGISVersion.nMajor > 2 ||
             ( poDS->sPostGISVersion.nMajor == 2 && poDS->sPostGISVersion.nMinor >= 1 ) )
            pszExtentFct = "ST_EstimatedExtent";
        else
            pszExtentFct = "ST_Estimated_Extent";

        osCommand.Printf( "SELECT %s(%s, %s, %s)",
                        pszExtentFct,
                        OGRPGEscapeString(hPGConn, pszSchemaName).c_str(),
                        OGRPGEscapeString(hPGConn, pszTableName).c_str(),
                        OGRPGEscapeString(hPGConn, poGeomFieldDefn->GetNameRef()).c_str() );

        /* Quiet error: ST_Estimated_Extent may return an error if statistics */
        /* have not been computed */
        if( RunGetExtentRequest(psExtent, bForce, osCommand, TRUE) == OGRERR_NONE )
            return OGRERR_NONE;

        CPLDebug("PG","Unable to get extimated extent by PostGIS. Trying real extent.");
    }

    return OGRPGLayer::GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                        SetDifferedCreation()                         */
/************************************************************************/

void OGRPGTableLayer::SetDifferedCreation(int bDifferedCreationIn, CPLString osCreateTableIn)
{
    bDifferedCreation = bDifferedCreationIn;
    osCreateTable = osCreateTableIn;
}

/************************************************************************/
/*                      RunDifferedCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRPGTableLayer::RunDifferedCreationIfNecessary()
{
    if( !bDifferedCreation )
        return OGRERR_NONE;
    bDifferedCreation = FALSE;

    poDS->EndCopy();

    int i;

    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn *poGeomField = (OGRPGGeomFieldDefn*) poFeatureDefn->GetGeomFieldDefn(i);

        if (poDS->sPostGISVersion.nMajor >= 2 ||
            poGeomField->ePostgisType == GEOM_TYPE_GEOGRAPHY)
        {
            const char *pszGeometryType = OGRToOGCGeomType(poGeomField->GetType());

            osCreateTable += ", ";
            osCreateTable += OGRPGEscapeColumnName(poGeomField->GetNameRef());
            osCreateTable += " ";
            if( poGeomField->ePostgisType == GEOM_TYPE_GEOMETRY )
                osCreateTable += "geometry(";
            else
                osCreateTable += "geography(";
            osCreateTable += pszGeometryType;
            if( poGeomField->nCoordDimension == 3 )
                osCreateTable += "Z";
            if( poGeomField->nSRSId > 0 )
                osCreateTable += CPLSPrintf(",%d", poGeomField->nSRSId);
            osCreateTable += ")";
            if( !poGeomField->IsNullable() )
                osCreateTable += " NOT NULL";
        }
    }

    osCreateTable += " )";
    CPLString osCommand(osCreateTable);
    
    PGresult            *hResult;
    PGconn              *hPGConn = poDS->GetPGConn();
    
    hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "%s\n%s", osCommand.c_str(), PQerrorMessage(hPGConn) );

        OGRPGClearResult( hResult );
        return OGRERR_FAILURE;
    }

    OGRPGClearResult( hResult );

    // For PostGIS 1.X, use AddGeometryColumn() to create geometry columns
    if (poDS->sPostGISVersion.nMajor < 2)
    {
        for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRPGGeomFieldDefn *poGeomField = (OGRPGGeomFieldDefn*) poFeatureDefn->GetGeomFieldDefn(i);
            if( poGeomField->ePostgisType == GEOM_TYPE_GEOMETRY &&
                RunAddGeometryColumn(poGeomField) != OGRERR_NONE )
            {
                return OGRERR_FAILURE;
            }
        }
    }
    
    if( bCreateSpatialIndexFlag )
    {
        for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRPGGeomFieldDefn *poGeomField = (OGRPGGeomFieldDefn*) poFeatureDefn->GetGeomFieldDefn(i);
            if( RunCreateSpatialIndex(poGeomField) != OGRERR_NONE )
            {
                return OGRERR_FAILURE;
            }
        }
    }

    return OGRERR_NONE;
}

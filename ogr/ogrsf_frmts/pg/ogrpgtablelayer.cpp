/******************************************************************************

 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

#define UNSUPPORTED_OP_READ_ONLY "%s : unsupported operation on a read-only datasource."

/************************************************************************/
/*                        OGRPGTableFeatureDefn                         */
/************************************************************************/

class OGRPGTableFeatureDefn final: public OGRPGFeatureDefn
{
    private:
        OGRPGTableFeatureDefn (const OGRPGTableFeatureDefn& ) = delete;
        OGRPGTableFeatureDefn& operator= (const OGRPGTableFeatureDefn& ) = delete;

        OGRPGTableLayer *poLayer = nullptr;

        void SolveFields() const;

    public:
        explicit OGRPGTableFeatureDefn( OGRPGTableLayer* poLayerIn,
                               const char * pszName = nullptr ) :
            OGRPGFeatureDefn(pszName), poLayer(poLayerIn)
        {
        }

        virtual void UnsetLayer() override
        {
            poLayer = nullptr;
            OGRPGFeatureDefn::UnsetLayer();
        }

        virtual int         GetFieldCount() const override
            { SolveFields(); return OGRPGFeatureDefn::GetFieldCount(); }
        virtual OGRFieldDefn *GetFieldDefn( int i ) override
            { SolveFields(); return OGRPGFeatureDefn::GetFieldDefn(i); }
        virtual const OGRFieldDefn *GetFieldDefn( int i ) const override
            { SolveFields(); return OGRPGFeatureDefn::GetFieldDefn(i); }
        virtual int         GetFieldIndex( const char * pszName ) const override
            { SolveFields(); return OGRPGFeatureDefn::GetFieldIndex(pszName); }

        virtual int         GetGeomFieldCount() const override
            { if (poLayer != nullptr && !poLayer->HasGeometryInformation())
                  SolveFields();
              return OGRPGFeatureDefn::GetGeomFieldCount(); }
        virtual OGRPGGeomFieldDefn *GetGeomFieldDefn( int i ) override
            { if (poLayer != nullptr && !poLayer->HasGeometryInformation())
                  SolveFields();
              return OGRPGFeatureDefn::GetGeomFieldDefn(i); }
        virtual const OGRPGGeomFieldDefn *GetGeomFieldDefn( int i ) const override
            { if (poLayer != nullptr && !poLayer->HasGeometryInformation())
                  SolveFields();
              return OGRPGFeatureDefn::GetGeomFieldDefn(i); }
        virtual int         GetGeomFieldIndex( const char * pszName) const override
            { if (poLayer != nullptr && !poLayer->HasGeometryInformation())
                  SolveFields();
              return OGRPGFeatureDefn::GetGeomFieldIndex(pszName); }
};

/************************************************************************/
/*                           SolveFields()                              */
/************************************************************************/

void OGRPGTableFeatureDefn::SolveFields() const
{
    if( poLayer == nullptr )
        return;

    poLayer->ReadTableDefinition();
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRPGTableLayer::GetFIDColumn()

{
    ReadTableDefinition();

    if( pszFIDColumn != nullptr )
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
                                  const char * pszDescriptionIn,
                                  const char * pszGeomColForcedIn,
                                  int bUpdate ) :
    bUpdateAccess(bUpdate),
    pszTableName(CPLStrdup(pszTableNameIn)),
    pszSchemaName(CPLStrdup(pszSchemaNameIn ?
                            pszSchemaNameIn : osCurrentSchema.c_str())),
    pszDescription(pszDescriptionIn ? CPLStrdup(pszDescriptionIn) : nullptr),
    osPrimaryKey(CPLGetConfigOption( "PGSQL_OGR_FID", "ogc_fid" )),
    pszGeomColForced(pszGeomColForcedIn ? CPLStrdup(pszGeomColForcedIn) : nullptr),
    // Just in provision for people yelling about broken backward compatibility.
    bRetrieveFID(CPLTestBool(
        CPLGetConfigOption("OGR_PG_RETRIEVE_FID", "TRUE")))
{
    poDS = poDSIn;
    pszQueryStatement = nullptr;

/* -------------------------------------------------------------------- */
/*      Build the layer defn name.                                      */
/* -------------------------------------------------------------------- */
    CPLString osDefnName;
    if( pszSchemaNameIn && osCurrentSchema != pszSchemaNameIn )
    {
        osDefnName.Printf("%s.%s", pszSchemaNameIn, pszTableName );
        pszSqlTableName = CPLStrdup(
            CPLString().Printf("%s.%s",
                               OGRPGEscapeColumnName(pszSchemaNameIn).c_str(),
                               OGRPGEscapeColumnName(pszTableName).c_str() ));
    }
    else
    {
        // no prefix for current_schema in layer name, for backwards
        // compatibility.
        osDefnName = pszTableName;
        pszSqlTableName = CPLStrdup(OGRPGEscapeColumnName(pszTableName));
    }
    if( pszGeomColForced != nullptr )
    {
        osDefnName += "(";
        osDefnName += pszGeomColForced;
        osDefnName += ")";
    }

    poFeatureDefn = new OGRPGTableFeatureDefn( this, osDefnName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    if( pszDescriptionIn != nullptr && !EQUAL(pszDescriptionIn, "") )
    {
        OGRLayer::SetMetadataItem("DESCRIPTION", pszDescriptionIn);
    }
}

//************************************************************************/
/*                          ~OGRPGTableLayer()                          */
/************************************************************************/

OGRPGTableLayer::~OGRPGTableLayer()

{
    if( bDeferredCreation ) RunDeferredCreationIfNecessary();
    if( bCopyActive ) EndCopy();
    UpdateSequenceIfNeeded();

    CPLFree( pszSqlTableName );
    CPLFree( pszTableName );
    CPLFree( pszSqlGeomParentTableName );
    CPLFree( pszSchemaName );
    CPLFree( pszDescription );
    CPLFree( pszGeomColForced );
    CSLDestroy( papszOverrideColumnTypes );
}

/************************************************************************/
/*                          GetMetadataDomainList()                     */
/************************************************************************/

char ** OGRPGTableLayer::GetMetadataDomainList()
{
    if( pszDescription == nullptr )
        GetMetadata();
    if( pszDescription != nullptr && pszDescription[0] != '\0' )
        return CSLAddString(nullptr, "");
    return nullptr;
}

/************************************************************************/
/*                              GetMetadata()                           */
/************************************************************************/

char ** OGRPGTableLayer::GetMetadata(const char* pszDomain)
{
    if( (pszDomain == nullptr || EQUAL(pszDomain, "")) &&
        pszDescription == nullptr )
    {
        PGconn              *hPGConn = poDS->GetPGConn();
        CPLString osCommand;
        osCommand.Printf(
            "SELECT d.description FROM pg_class c "
            "JOIN pg_namespace n ON c.relnamespace=n.oid "
            "JOIN pg_description d "
            "ON d.objoid = c.oid AND d.classoid = 'pg_class'::regclass::oid AND d.objsubid = 0 "
            "WHERE c.relname = %s AND n.nspname = %s AND c.relkind in ('r', 'v') ",
            OGRPGEscapeString(hPGConn, pszTableName).c_str(),
            OGRPGEscapeString(hPGConn, pszSchemaName).c_str());
        PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

        const char* pszDesc = nullptr;
        if ( hResult && PGRES_TUPLES_OK == PQresultStatus(hResult) &&
             PQntuples( hResult ) == 1  )
        {
            pszDesc = PQgetvalue(hResult,0,0);
            if( pszDesc )
                OGRLayer::SetMetadataItem("DESCRIPTION", pszDesc);
        }
        pszDescription = CPLStrdup(pszDesc ? pszDesc : "");

        OGRPGClearResult( hResult );
    }

    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                            GetMetadataItem()                         */
/************************************************************************/

const char *OGRPGTableLayer::GetMetadataItem(const char* pszName, const char* pszDomain)
{
    GetMetadata(pszDomain);
    return OGRLayer::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                              SetMetadata()                           */
/************************************************************************/

CPLErr OGRPGTableLayer::SetMetadata(char** papszMD, const char* pszDomain)
{
    OGRLayer::SetMetadata(papszMD, pszDomain);
    if( !osForcedDescription.empty() && (pszDomain == nullptr || EQUAL(pszDomain, "")) )
    {
        OGRLayer::SetMetadataItem("DESCRIPTION", osForcedDescription);
    }

    if( !bDeferredCreation && (pszDomain == nullptr || EQUAL(pszDomain, "")) )
    {
        const char* l_pszDescription = OGRLayer::GetMetadataItem("DESCRIPTION");
        if( l_pszDescription == nullptr )
            l_pszDescription = "";
        PGconn              *hPGConn = poDS->GetPGConn();
        CPLString osCommand;

        osCommand.Printf( "COMMENT ON TABLE %s IS %s",
                           pszSqlTableName,
                           l_pszDescription[0] != '\0' ?
                              OGRPGEscapeString(hPGConn, l_pszDescription).c_str() : "NULL" );
        PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );
        OGRPGClearResult( hResult );

        CPLFree(pszDescription);
        pszDescription = CPLStrdup(l_pszDescription);
    }

    return CE_None;
}

/************************************************************************/
/*                            SetMetadataItem()                         */
/************************************************************************/

CPLErr OGRPGTableLayer::SetMetadataItem(const char* pszName, const char* pszValue,
                                        const char* pszDomain)
{
    if( (pszDomain == nullptr || EQUAL(pszDomain, "")) && pszName != nullptr &&
        EQUAL(pszName, "DESCRIPTION") && !osForcedDescription.empty() )
    {
        pszValue = osForcedDescription;
    }
    OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
    if( !bDeferredCreation &&
        (pszDomain == nullptr || EQUAL(pszDomain, "")) && pszName != nullptr &&
        EQUAL(pszName, "DESCRIPTION") )
    {
        SetMetadata( GetMetadata() );
    }
    return CE_None;
}

/************************************************************************/
/*                      SetForcedDescription()                          */
/************************************************************************/

void OGRPGTableLayer::SetForcedDescription( const char* pszDescriptionIn )
{
    osForcedDescription = pszDescriptionIn;
    CPLFree(pszDescription);
    pszDescription = CPLStrdup( pszDescriptionIn );
    SetMetadataItem( "DESCRIPTION", osForcedDescription );
}

/************************************************************************/
/*                      SetGeometryInformation()                        */
/************************************************************************/

void  OGRPGTableLayer::SetGeometryInformation(PGGeomColumnDesc* pasDesc,
                                              int nGeomFieldCount)
{
    // Flag must be set before instantiating geometry fields.
    bGeometryInformationSet = TRUE;

    for(int i=0; i<nGeomFieldCount; i++)
    {
        auto poGeomFieldDefn =
            cpl::make_unique<OGRPGGeomFieldDefn>(this, pasDesc[i].pszName);
        poGeomFieldDefn->SetNullable(pasDesc[i].bNullable);
        poGeomFieldDefn->nSRSId = pasDesc[i].nSRID;
        poGeomFieldDefn->GeometryTypeFlags = pasDesc[i].GeometryTypeFlags;
        poGeomFieldDefn->ePostgisType = pasDesc[i].ePostgisType;
        if( pasDesc[i].pszGeomType != nullptr )
        {
            OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(pasDesc[i].pszGeomType);
            if( (poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_3D) && (eGeomType != wkbUnknown) )
                eGeomType = wkbSetZ(eGeomType);
            if( (poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) && (eGeomType != wkbUnknown) )
                eGeomType = wkbSetM(eGeomType);
            poGeomFieldDefn->SetType(eGeomType);
        }
        poFeatureDefn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
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
    PGconn *hPGConn = poDS->GetPGConn();

    if( bTableDefinitionValid >= 0 )
        return bTableDefinitionValid;
    bTableDefinitionValid = FALSE;

    poDS->EndCopy();

/* -------------------------------------------------------------------- */
/*      Get the OID of the table.                                       */
/* -------------------------------------------------------------------- */

    CPLString osCommand;
    osCommand.Printf("SELECT c.oid FROM pg_class c "
                     "JOIN pg_namespace n ON c.relnamespace=n.oid "
                     "WHERE c.relname = %s AND n.nspname = %s",
                     OGRPGEscapeString(hPGConn, pszTableName).c_str(),
                     OGRPGEscapeString(hPGConn, pszSchemaName).c_str());
    unsigned int nTableOID = 0;
    {
        PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );
        if ( hResult && PGRES_TUPLES_OK == PQresultStatus(hResult) )
        {
            if ( PQntuples( hResult ) == 1 && PQgetisnull( hResult,0,0 ) == false )
            {
                nTableOID = static_cast<unsigned>(CPLAtoGIntBig(PQgetvalue(hResult, 0, 0)));
                OGRPGClearResult( hResult );
            }
            else
            {
                CPLDebug("PG", "Could not retrieve table oid for %s", pszTableName);
                OGRPGClearResult( hResult );
                return FALSE;
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", PQerrorMessage(hPGConn) );
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Identify the integer primary key.                               */
/* -------------------------------------------------------------------- */

    const char* pszTypnameEqualsAnyClause =
        poDS->sPostgreSQLVersion.nMajor == 7 &&
        poDS->sPostgreSQLVersion.nMinor <= 3
        ? "ANY(SELECT '{int2, int4, int8, serial, bigserial}')"
        : "ANY(ARRAY['int2','int4','int8','serial','bigserial'])";

    const char* pszAttnumEqualAnyIndkey =
        poDS->sPostgreSQLVersion.nMajor > 8 ||
        (poDS->sPostgreSQLVersion.nMajor == 8 &&
         poDS->sPostgreSQLVersion.nMinor >= 2)
        ? "a.attnum = ANY(i.indkey)"
        : "(i.indkey[0]=a.attnum OR i.indkey[1]=a.attnum OR i.indkey[2]=a.attnum "
        "OR i.indkey[3]=a.attnum OR i.indkey[4]=a.attnum OR i.indkey[5]=a.attnum "
        "OR i.indkey[6]=a.attnum OR i.indkey[7]=a.attnum OR i.indkey[8]=a.attnum "
        "OR i.indkey[9]=a.attnum)";

    /* See #1889 for why we don't use 'AND a.attnum = ANY(i.indkey)' */
    osCommand.Printf(
              "SELECT a.attname, a.attnum, t.typname, t.typname = %s AS isfid "
              "FROM pg_attribute a "
              "JOIN pg_type t ON t.oid = a.atttypid "
              "JOIN pg_index i ON i.indrelid = a.attrelid "
              "WHERE a.attnum > 0 AND a.attrelid = %u "
              "AND i.indisprimary = 't' "
              "AND t.typname !~ '^geom' "
              "AND %s ORDER BY a.attnum",
              pszTypnameEqualsAnyClause,
              nTableOID,
              pszAttnumEqualAnyIndkey );

    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

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
/*      Fire off commands to get back the columns of the table.         */
/* -------------------------------------------------------------------- */
    osCommand.Printf(
        "SELECT a.attname, t.typname, a.attlen,"
        "       format_type(a.atttypid,a.atttypmod), a.attnotnull, def.def, i.indisunique%s "
        "FROM pg_attribute a "
        "JOIN pg_type t ON t.oid = a.atttypid "
        "LEFT JOIN "
        "(SELECT adrelid, adnum, pg_get_expr(adbin, adrelid) AS def FROM pg_attrdef) def "
        "ON def.adrelid = a.attrelid AND def.adnum = a.attnum "
        // Find unique constraints that are on a single column only
        "LEFT JOIN "
        "(SELECT DISTINCT indrelid, indkey, indisunique FROM pg_index WHERE indisunique) i "
        "ON i.indrelid = a.attrelid AND i.indkey[0] = a.attnum AND i.indkey[1] IS NULL "
        "WHERE a.attnum > 0 AND a.attrelid = %u "
        "ORDER BY a.attnum",
        (poDS->sPostgreSQLVersion.nMajor >= 12 ? ", a.attgenerated" : ""),
        nTableOID);

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
    for( int iRecord = 0; iRecord < PQntuples(hResult); iRecord++ )
    {
        OGRFieldDefn    oField( PQgetvalue( hResult, iRecord, 0 ), OFTString);

        const char      *pszType = PQgetvalue(hResult, iRecord, 1 );
        int nWidth = atoi(PQgetvalue(hResult,iRecord,2));
        const char      *pszFormatType = PQgetvalue(hResult,iRecord,3);
        const char      *pszNotNull = PQgetvalue(hResult,iRecord,4);
        const char      *pszDefault = PQgetisnull(hResult,iRecord,5) ? nullptr : PQgetvalue(hResult,iRecord,5);
        const char      *pszIsUnique = PQgetvalue(hResult,iRecord,6);
        const char      *pszGenerated =
            poDS->sPostgreSQLVersion.nMajor >= 12 ? PQgetvalue( hResult, iRecord, 7 ) : "";

        if( pszNotNull && EQUAL(pszNotNull, "t") )
            oField.SetNullable(FALSE);
        if( pszIsUnique && EQUAL(pszIsUnique, "t") )
            oField.SetUnique(TRUE);

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
            const auto InitGeomField = [this, &pszType, &oField](
                                        OGRPGGeomFieldDefn* poGeomFieldDefn)
            {
                if( EQUAL(pszType,"geometry") )
                    poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOMETRY;
                else if( EQUAL(pszType,"geography") )
                {
                    poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOGRAPHY;
                    if( !(poDS->sPostGISVersion.nMajor >= 3 ||
                         (poDS->sPostGISVersion.nMajor == 2 && poDS->sPostGISVersion.nMinor >= 2)) )
                    {
                        // EPSG:4326 was a requirement for geography before PostGIS 2.2
                        poGeomFieldDefn->nSRSId = 4326;
                    }
                }
                else
                {
                    poGeomFieldDefn->ePostgisType = GEOM_TYPE_WKB;
                    if( EQUAL(pszType,"OID") )
                        bWkbAsOid = TRUE;
                }
                poGeomFieldDefn->SetNullable(oField.IsNullable());
            };

            if( !bGeometryInformationSet )
            {
                if( pszGeomColForced == nullptr ||
                    EQUAL(pszGeomColForced, oField.GetNameRef()) )
                {
                    auto poGeomFieldDefn = cpl::make_unique<OGRPGGeomFieldDefn>(
                        this, oField.GetNameRef());
                    InitGeomField(poGeomFieldDefn.get());
                    poFeatureDefn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
                }
            }
            else
            {
                int idx = poFeatureDefn->GetGeomFieldIndex(oField.GetNameRef());
                if( idx >= 0 )
                {
                    auto poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(idx);
                    InitGeomField(poGeomFieldDefn);
                }
            }

            continue;
        }

        OGRPGCommonLayerSetType(oField, pszType, pszFormatType, nWidth);

        if( pszDefault )
        {
            OGRPGCommonLayerNormalizeDefault(&oField, pszDefault);
        }

        //CPLDebug("PG", "name=%s, type=%s", oField.GetNameRef(), pszType);
        poFeatureDefn->AddFieldDefn( &oField );
        m_abGeneratedColumns.push_back(
            pszGenerated != nullptr && pszGenerated[0] != '\0' );
    }

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
        poFeatureDefn->GetGeomFieldDefn(iField);

      /* Get the geometry type and dimensions from the table, or */
      /* from its parents if it is a derived table, or from the parent of the parent, etc.. */
      int bGoOn = poDS->m_bHasGeometryColumns;
      const bool bHasPostGISGeometry =
        (poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY);

      while(bGoOn)
      {
        const CPLString osEscapedThisOrParentTableName(
            OGRPGEscapeString(hPGConn,
                (pszSqlGeomParentTableName) ? pszSqlGeomParentTableName : pszTableName));
        osCommand.Printf(
            "SELECT type, coord_dimension, srid FROM %s WHERE f_table_name = %s",
            (bHasPostGISGeometry) ? "geometry_columns" : "geography_columns",
            osEscapedThisOrParentTableName.c_str());

        osCommand += CPLString().Printf(" AND %s=%s",
            (bHasPostGISGeometry) ? "f_geometry_column" : "f_geography_column",
            OGRPGEscapeString(hPGConn,poGeomFieldDefn->GetNameRef()).c_str());

        osCommand += CPLString().Printf(" AND f_table_schema = %s",
                                            OGRPGEscapeString(hPGConn,pszSchemaName).c_str());

        hResult = OGRPG_PQexec(hPGConn,osCommand);

        if ( hResult && PQntuples(hResult) == 1 && !PQgetisnull(hResult,0,0) )
        {
            const char* pszType = PQgetvalue(hResult,0,0);

            int dim = atoi(PQgetvalue(hResult,0,1));
            bool bHasM = pszType[strlen(pszType)-1] == 'M';
            int GeometryTypeFlags = 0;
            if( dim == 3 )
            {
                if (bHasM)
                    GeometryTypeFlags |= OGRGeometry::OGR_G_MEASURED;
                else
                    GeometryTypeFlags |= OGRGeometry::OGR_G_3D;
            }
            else if( dim == 4 )
                GeometryTypeFlags |= OGRGeometry::OGR_G_3D | OGRGeometry::OGR_G_MEASURED;

            int nSRSId = atoi(PQgetvalue(hResult,0,2));

            poGeomFieldDefn->GeometryTypeFlags = GeometryTypeFlags;
            if( nSRSId > 0 )
                poGeomFieldDefn->nSRSId = nSRSId;
            OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(pszType);
            if( poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_3D && eGeomType != wkbUnknown )
                eGeomType = wkbSetZ(eGeomType);
            if( poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED && eGeomType != wkbUnknown )
                eGeomType = wkbSetM(eGeomType);
            poGeomFieldDefn->SetType(eGeomType);

            bGoOn = FALSE;
        }
        else
        {
            /* Fetch the name of the parent table */
            osCommand.Printf(
                "SELECT pg_class.relname FROM pg_class WHERE oid = "
                "(SELECT pg_inherits.inhparent FROM pg_inherits WHERE inhrelid = "
                "(SELECT c.oid FROM pg_class c, pg_namespace n "
                "WHERE c.relname = %s AND c.relnamespace=n.oid AND "
                "n.nspname = %s))",
                            osEscapedThisOrParentTableName.c_str(),
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
                                           int GeometryTypeFlags)
{
    bTableDefinitionValid = TRUE;
    bGeometryInformationSet = TRUE;
    pszFIDColumn = CPLStrdup(pszFIDColumnName);
    poFeatureDefn->SetGeomType(wkbNone);
    if( eType != wkbNone )
    {
        auto poGeomFieldDefn = cpl::make_unique<OGRPGGeomFieldDefn>(this, pszGFldName);
        poGeomFieldDefn->SetType(eType);
        poGeomFieldDefn->GeometryTypeFlags = GeometryTypeFlags;

        if( EQUAL(pszGeomType,"geometry") )
        {
            poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOMETRY;
            poGeomFieldDefn->nSRSId = nSRSId;
        }
        else if( EQUAL(pszGeomType,"geography") )
        {
            poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOGRAPHY;
            poGeomFieldDefn->nSRSId = nSRSId;
        }
        else
        {
            poGeomFieldDefn->ePostgisType = GEOM_TYPE_WKB;
            if( EQUAL(pszGeomType,"OID") )
                bWkbAsOid = TRUE;
        }
        poFeatureDefn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
    }
    else if( pszGFldName != nullptr )
    {
        m_osFirstGeometryFieldName = pszGFldName;
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
    OGRPGGeomFieldDefn* poGeomFieldDefn = nullptr;
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
        poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);

    if( m_poFilterGeom != nullptr && poGeomFieldDefn != nullptr &&
        poDS->sPostGISVersion.nMajor >= 0 && (
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

    if( !osQuery.empty() )
    {
        if( osWHERE.empty() )
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
    if( pszQueryStatement != nullptr )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = nullptr;
    }
    pszQueryStatement = static_cast<char *>(
        CPLMalloc(osFields.size()+osWHERE.size()
                  +strlen(pszSqlTableName) + 40));
    snprintf( pszQueryStatement,
              osFields.size()+osWHERE.size()
                  +strlen(pszSqlTableName) + 40,
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

    if( bDeferredCreation ) RunDeferredCreationIfNecessary();
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
    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return nullptr;
    poDS->EndCopy();

    if( pszQueryStatement == nullptr )
        ResetReading();

    OGRPGGeomFieldDefn* poGeomFieldDefn = nullptr;
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
        poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);
    poFeatureDefn->GetFieldCount();

    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == nullptr )
            return nullptr;

        /* We just have to look if there is a geometry filter */
        /* If there's a PostGIS geometry column, the spatial filter */
        /* is already taken into account in the select request */
        /* The attribute filter is always taken into account by the select request */
        if( m_poFilterGeom == nullptr
            || poGeomFieldDefn == nullptr
            || poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY
            || poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) )  )
        {
            if( iFIDAsRegularColumnIndex >= 0 )
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

    if( pszFIDColumn != nullptr && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
    {
        osFieldList += OGRPGEscapeColumnName(pszFIDColumn);
    }

    for( i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->GetGeomFieldDefn(i);
        CPLString osEscapedGeom =
            OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef());

        if( !osFieldList.empty() )
            osFieldList += ", ";

        if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY )
        {
            if ( poDS->sPostGISVersion.nMajor < 0 || poDS->bUseBinaryCursor )
            {
                osFieldList += osEscapedGeom;
            }
            else if (CPLTestBool(CPLGetConfigOption("PG_USE_BASE64", "NO")))
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
            else if ( !CPLTestBool(CPLGetConfigOption("PG_USE_TEXT", "NO")) &&
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
#if defined(BINARY_CURSOR_ENABLED)
            if ( poDS->bUseBinaryCursor )
            {
                osFieldList += "ST_AsBinary(";
                osFieldList += osEscapedGeom;
                osFieldList += ") AS";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("AsBinary_%s", poGeomFieldDefn->GetNameRef()));
            }
            else
#endif
            if (CPLTestBool(CPLGetConfigOption("PG_USE_BASE64", "NO")))
            {
                osFieldList += "encode(ST_AsEWKB(";
                osFieldList += osEscapedGeom;
                osFieldList += "::geometry), 'base64') AS ";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("EWKBBase64_%s", poGeomFieldDefn->GetNameRef()));
            }
            else if ( !CPLTestBool(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
            {
                osFieldList += osEscapedGeom;
            }
            else
            {
                osFieldList += "ST_AsEWKT(";
                osFieldList += osEscapedGeom;
                osFieldList += "::geometry) AS ";
                osFieldList += OGRPGEscapeColumnName(
                    CPLSPrintf("AsEWKT_%s", poGeomFieldDefn->GetNameRef()));
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

        if( !osFieldList.empty() )
            osFieldList += ", ";

#if defined(BINARY_CURSOR_ENABLED)
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
#endif
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
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    if( pszQuery == nullptr )
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
    CPLString   osCommand;

    GetLayerDefn()->GetFieldCount();

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();
    bAutoFIDOnCreateViaCopy = FALSE;

/* -------------------------------------------------------------------- */
/*      We can only delete features if we have a well defined FID       */
/*      column to target.                                               */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn == nullptr )
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

    PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);

    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature() DELETE statement failed.\n%s",
                  PQerrorMessage(hPGConn) );

        eErr = OGRERR_FAILURE;
    }
    else
    {
        if( EQUAL(PQcmdStatus(hResult), "DELETE 0") )
            eErr = OGRERR_NON_EXISTING_FEATURE;
        else
            eErr = OGRERR_NONE;
    }

    OGRPGClearResult( hResult );

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

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

    if( nullptr == poFeature )
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

    if( pszFIDColumn == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to update features in tables without\n"
                  "a recognised FID column.");
        return eErr;
    }

    /* In case the FID column has also been created as a regular field */
    if( iFIDAsRegularColumnIndex >= 0 )
    {
        if( !poFeature->IsFieldSetAndNotNull( iFIDAsRegularColumnIndex ) ||
            poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex) != poFeature->GetFID() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Inconsistent values of FID and field of same name");
            return OGRERR_FAILURE;
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
            poFeatureDefn->GetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_WKB )
        {
            if( bNeedComma )
                osCommand += ", ";
            else
                bNeedComma = TRUE;

            osCommand += OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef());
            osCommand += " = ";
            if ( poGeom != nullptr )
            {
                if( !bWkbAsOid  )
                {
                    char    *pszBytea = GeometryToBYTEA( poGeom,
                                                         poDS->sPostGISVersion.nMajor,
                                                         poDS->sPostGISVersion.nMinor );

                    if( pszBytea != nullptr )
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
            if( poGeom != nullptr )
            {
                poGeom->closeRings();
                poGeom->set3D(poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_3D);
                poGeom->setMeasured(poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);
            }

            if ( !CPLTestBool(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
            {
                if ( poGeom != nullptr )
                {
                    char* pszHexEWKB = OGRGeometryToHexEWKB( poGeom, poGeomFieldDefn->nSRSId,
                                                             poDS->sPostGISVersion.nMajor,
                                                             poDS->sPostGISVersion.nMinor);
                    if ( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
                        osCommand += CPLString().Printf("'%s'::GEOGRAPHY", pszHexEWKB);
                    else
                        osCommand += CPLString().Printf("'%s'::GEOMETRY", pszHexEWKB);
                    CPLFree( pszHexEWKB );
                }
                else
                    osCommand += "NULL";
            }
            else
            {
                char    *pszWKT = nullptr;

                if (poGeom != nullptr)
                    poGeom->exportToWkt( &pszWKT, wkbVariantIso );

                int nSRSId = poGeomFieldDefn->nSRSId;
                if( pszWKT != nullptr )
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
                    CPLFree( pszWKT );
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
        if( !poFeature->IsFieldSet(i) )
            continue;
        if( m_abGeneratedColumns[i] )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        osCommand = osCommand
            + OGRPGEscapeColumnName(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + " = ";

        if( poFeature->IsFieldNull( i ) )
        {
            osCommand += "NULL";
        }
        else
        {
            OGRPGCommonAppendFieldValue(osCommand, poFeature, i,
                                        OGRPGEscapeString, hPGConn);
        }
    }
    if( !bNeedComma ) // nothing to do
        return OGRERR_NONE;

    /* Add the WHERE clause */
    osCommand += " WHERE ";
    osCommand = osCommand + OGRPGEscapeColumnName(pszFIDColumn) + " = ";
    osCommand += CPLString().Printf( CPL_FRMT_GIB, poFeature->GetFID() );

/* -------------------------------------------------------------------- */
/*      Execute the update.                                             */
/* -------------------------------------------------------------------- */
    PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "UPDATE command for feature " CPL_FRMT_GIB " failed.\n%s\nCommand: %s",
                  poFeature->GetFID(), PQerrorMessage(hPGConn), osCommand.c_str() );

        OGRPGClearResult( hResult );

        return OGRERR_FAILURE;
    }

    if( EQUAL(PQcmdStatus(hResult), "UPDATE 0") )
        eErr = OGRERR_NON_EXISTING_FEATURE;
    else
        eErr = OGRERR_NONE;

    OGRPGClearResult( hResult );

    return eErr;
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

    if( nullptr == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    /* In case the FID column has also been created as a regular field */
    GIntBig nFID = poFeature->GetFID();
    if( iFIDAsRegularColumnIndex >= 0 )
    {
        if( nFID == OGRNullFID )
        {
            if( poFeature->IsFieldSetAndNotNull( iFIDAsRegularColumnIndex ) )
            {
                poFeature->SetFID(
                    poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex));
            }
        }
        else
        {
            if( !poFeature->IsFieldSetAndNotNull( iFIDAsRegularColumnIndex ) ||
                poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex) != nFID )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Inconsistent values of FID and field of same name");
                return OGRERR_FAILURE;
            }
        }
    }

    /* Auto-promote FID column to 64bit if necessary */
    if( pszFIDColumn != nullptr &&
        !CPL_INT64_FITS_ON_INT32(nFID) &&
        GetMetadataItem(OLMD_FID64) == nullptr )
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
        if( CPLTestBool(CPLGetConfigOption("OGR_TRUNCATE", "NO")) )
        {
            PGconn *hPGConn = poDS->GetPGConn();
            CPLString osCommand;

            osCommand.Printf("TRUNCATE TABLE %s", pszSqlTableName );
            PGresult *hResult = OGRPG_PQexec( hPGConn, osCommand.c_str() );
            OGRPGClearResult( hResult );
        }
    }

    // We avoid testing the config option too often.
    if( bUseCopy == USE_COPY_UNSET )
        bUseCopy = CPLTestBool( CPLGetConfigOption( "PG_USE_COPY", "NO") );

    OGRErr eErr;
    if( !bUseCopy )
    {
        eErr = CreateFeatureViaInsert( poFeature );
    }
    else
    {
        /* If there's a unset field with a default value, then we must use */
        /* a specific INSERT statement to avoid unset fields to be bound to NULL */
        bool bHasDefaultValue = false;
        const int nFieldCount = poFeatureDefn->GetFieldCount();
        for( int iField = 0; iField < nFieldCount; iField++ )
        {
            if( !poFeature->IsFieldSet( iField ) &&
                poFeature->GetFieldDefnRef(iField)->GetDefault() != nullptr )
            {
                bHasDefaultValue = true;
                break;
            }
        }
        if( bHasDefaultValue )
        {
            eErr = CreateFeatureViaInsert( poFeature );
        }
        else
        {
            bool bFIDSet = (pszFIDColumn != nullptr && poFeature->GetFID() != OGRNullFID);
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
                    bNeedToUpdateSequence = bFIDSet;
                }

                eErr = CreateFeatureViaCopy( poFeature );
                if( bFIDSet )
                    bAutoFIDOnCreateViaCopy = FALSE;
                if( eErr == OGRERR_NONE && bAutoFIDOnCreateViaCopy )
                {
                    poFeature->SetFID( ++iNextShapeId );
                }
            }
        }
    }

    if( eErr == OGRERR_NONE && iFIDAsRegularColumnIndex >= 0 )
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
    CPLString osStr = "\"";

    char ch = '\0';
    for( int i = 0; (ch = pszColumnName[i]) != '\0'; i++ )
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

CPLString OGRPGEscapeString(void *hPGConnIn,
                            const char* pszStrValue, int nMaxLength,
                            const char* pszTableName,
                            const char* pszFieldName )
{
    PGconn *hPGConn = reinterpret_cast<PGconn*>(hPGConnIn);
    CPLString osCommand;

    /* We need to quote and escape string fields. */
    osCommand += "'";

    int nSrcLen = static_cast<int>(strlen(pszStrValue));
    int nSrcLenUTF = CPLStrlenUTF8(pszStrValue);

    if (nMaxLength > 0 && nSrcLenUTF > nMaxLength)
    {
        CPLDebug( "PG",
                  "Truncated %s.%s field value '%s' to %d characters.",
                  pszTableName, pszFieldName, pszStrValue, nMaxLength );

        int iUTF8Char = 0;
        for(int iChar = 0; iChar < nSrcLen; iChar++ )
        {
            if( ((reinterpret_cast<const unsigned char *>(pszStrValue))[iChar] & 0xc0) != 0x80 )
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

    char* pszDestStr = static_cast<char*>(CPLMalloc(2 * nSrcLen + 1));

    int nError = 0;
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
    CPLString           osCommand;
    int                 bNeedComma = FALSE;

    int bEmptyInsert = FALSE;

    poDS->EndCopy();

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO %s (", pszSqlTableName );

    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->GetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == nullptr )
            continue;
        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            osCommand += ", ";
        osCommand += OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef()) + " ";
    }

    /* Use case of ogr_pg_60 test */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != nullptr )
    {
        bNeedToUpdateSequence = true;

        if( bNeedComma )
            osCommand += ", ";

        osCommand = osCommand + OGRPGEscapeColumnName(pszFIDColumn) + " ";
        bNeedComma = TRUE;
    }
    else
    {
        UpdateSequenceIfNeeded();
    }

    const int nFieldCount = poFeatureDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( iFIDAsRegularColumnIndex == i )
            continue;
        if( !poFeature->IsFieldSet( i ) )
            continue;
        if( m_abGeneratedColumns[i] )
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
    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->GetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == nullptr )
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
            poGeom->set3D(poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_3D);
            poGeom->setMeasured(poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);

            int nSRSId = poGeomFieldDefn->nSRSId;

            if ( !CPLTestBool(CPLGetConfigOption("PG_USE_TEXT", "NO")) )
            {
                char    *pszHexEWKB = OGRGeometryToHexEWKB( poGeom, nSRSId,
                                                            poDS->sPostGISVersion.nMajor,
                                                            poDS->sPostGISVersion.nMinor );
                if ( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
                    osCommand += CPLString().Printf("'%s'::GEOGRAPHY", pszHexEWKB);
                else
                    osCommand += CPLString().Printf("'%s'::GEOMETRY", pszHexEWKB);
                CPLFree( pszHexEWKB );
            }
            else
            {
                char    *pszWKT = nullptr;
                poGeom->exportToWkt( &pszWKT, wkbVariantIso );

                if( pszWKT != nullptr )
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
                    CPLFree( pszWKT );
                }
                else
                    osCommand += "''";
            }
        }
        else if( !bWkbAsOid )
        {
            char    *pszBytea = GeometryToBYTEA( poGeom,
                                                 poDS->sPostGISVersion.nMajor,
                                                 poDS->sPostGISVersion.nMinor );

            if( pszBytea != nullptr )
            {
                if (poDS->bUseEscapeStringSyntax)
                    osCommand += "E";
                osCommand = osCommand + "'" + pszBytea + "'";
                CPLFree( pszBytea );
            }
            else
                osCommand += "''";
        }
        else if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_WKB /* && bWkbAsOid */ )
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

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != nullptr )
    {
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( CPL_FRMT_GIB " ", poFeature->GetFID() );
        bNeedComma = TRUE;
    }

    for( int i = 0; i < nFieldCount; i++ )
    {
        if( iFIDAsRegularColumnIndex == i )
            continue;
        if( !poFeature->IsFieldSet( i ) )
            continue;
        if( m_abGeneratedColumns[i] )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        OGRPGCommonAppendFieldValue(osCommand, poFeature, i,
                                    OGRPGEscapeString, hPGConn);
    }

    osCommand += ")";

    if (bEmptyInsert)
        osCommand.Printf( "INSERT INTO %s DEFAULT VALUES", pszSqlTableName );

    int bReturnRequested = FALSE;
    /* RETURNING is only available since Postgres 8.2 */
    /* We only get the FID, but we also could add the unset fields to get */
    /* the default values */
    if (bRetrieveFID && pszFIDColumn != nullptr && poFeature->GetFID() == OGRNullFID &&
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
    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand);
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
            pszFIDColumn != nullptr )
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

    /* Tell the datasource we are now planning to copy data */
    poDS->StartCopy( this );

    /* First process geometry */
    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn =
            poFeatureDefn->GetGeomFieldDefn(i);
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);

        char *pszGeom = nullptr;
        if ( nullptr != poGeom )
        {
            CheckGeomTypeCompatibility(i, poGeom);

            poGeom->closeRings();
            poGeom->set3D(poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_3D);
            poGeom->setMeasured(poGeomFieldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);

            if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_WKB )
                pszGeom = GeometryToBYTEA( poGeom,
                                           poDS->sPostGISVersion.nMajor,
                                           poDS->sPostGISVersion.nMinor );
            else
                pszGeom = OGRGeometryToHexEWKB( poGeom, poGeomFieldDefn->nSRSId,
                                                poDS->sPostGISVersion.nMajor,
                                                poDS->sPostGISVersion.nMinor );
        }

        if (!osCommand.empty())
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

    std::vector<bool> abFieldsToInclude(m_abGeneratedColumns.size(), true);
    for(size_t i = 0; i < abFieldsToInclude.size(); i++)
        abFieldsToInclude[i] = !m_abGeneratedColumns[i];

    OGRPGCommonAppendCopyFieldsExceptGeom(osCommand,
                                          poFeature,
                                          pszFIDColumn,
                                          CPL_TO_BOOL(bFIDColumnInCopyFields),
                                          abFieldsToInclude,
                                          OGRPGEscapeString,
                                          hPGConn);

    /* Add end of line marker */
    osCommand += "\n";

    // PostgreSQL doesn't provide very helpful reporting of invalid UTF-8
    // content in COPY mode.
    if( poDS->IsUTF8ClientEncoding() &&
        !CPLIsUTF8(osCommand.c_str(), static_cast<int>(osCommand.size())) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non UTF-8 content found when writing feature " CPL_FRMT_GIB
                 " of layer %s: %s",
                 poFeature->GetFID(),
                 poFeatureDefn->GetName(),
                 osCommand.c_str());
        return OGRERR_FAILURE;
    }

    /* ------------------------------------------------------------ */
    /*      Execute the copy.                                       */
    /* ------------------------------------------------------------ */

    OGRErr result = OGRERR_NONE;

    int copyResult = PQputCopyData(hPGConn, osCommand.c_str(),
                                   static_cast<int>(osCommand.size()));
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
            EQUAL(pszCap,OLCAlterFieldDefn)||
            EQUAL(pszCap,OLCRename) )
            return TRUE;

        else if( EQUAL(pszCap,OLCRandomWrite) ||
                 EQUAL(pszCap,OLCDeleteFeature) )
        {
            GetLayerDefn()->GetFieldCount();
            return pszFIDColumn != nullptr;
        }
    }

    if( EQUAL(pszCap,OLCRandomRead) )
    {
        GetLayerDefn()->GetFieldCount();
        return pszFIDColumn != nullptr;
    }

    else if( EQUAL(pszCap,OLCFastFeatureCount) ||
             EQUAL(pszCap,OLCFastSetNextByIndex) )
    {
        if( m_poFilterGeom == nullptr )
            return TRUE;
        OGRPGGeomFieldDefn* poGeomFieldDefn = nullptr;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);
        return poGeomFieldDefn == nullptr ||
               (poDS->sPostGISVersion.nMajor >= 0 &&
                (poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
                 poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY));
    }

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = nullptr;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter);
        return poGeomFieldDefn == nullptr ||
               (poDS->sPostGISVersion.nMajor >= 0 &&
               (poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY));
    }

    else if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = nullptr;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->GetGeomFieldDefn(0);
        return poGeomFieldDefn != nullptr &&
               poDS->sPostGISVersion.nMajor >= 0 &&
               poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY;
    }

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;

    else if( EQUAL(pszCap,OLCMeasuredGeometries) )
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

    if( pszFIDColumn != nullptr &&
        EQUAL( oField.GetNameRef(), pszFIDColumn ) &&
        oField.GetType() != OFTInteger &&
        oField.GetType() != OFTInteger64 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oField.GetNameRef());
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = OGRPGCommonLaunderName( oField.GetNameRef(), "PG" );

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
    if( pszOverrideType != nullptr )
        osFieldType = pszOverrideType;
    else
    {
        osFieldType = OGRPGCommonLayerGetType(oField,
                                              CPL_TO_BOOL(bPreservePrecision),
                                              CPL_TO_BOOL(bApproxOK));
        if (osFieldType.empty())
            return OGRERR_FAILURE;
    }

    CPLString osConstraints;
    if( !oField.IsNullable() )
        osConstraints += " NOT NULL";
    if( oField.IsUnique() )
        osConstraints += " UNIQUE";
    if( oField.GetDefault() != nullptr && !oField.IsDefaultDriverSpecific() )
    {
        osConstraints += " DEFAULT ";
        osConstraints += OGRPGCommonLayerGetPGDefault(&oField);
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if( bDeferredCreation )
    {
        if( !(pszFIDColumn != nullptr && EQUAL(pszFIDColumn,oField.GetNameRef())) )
        {
            osCreateTable += ", ";
            osCreateTable += OGRPGEscapeColumnName(oField.GetNameRef());
            osCreateTable += " ";
            osCreateTable += osFieldType;
            osCreateTable += osConstraints;
        }
    }
    else
    {
        poDS->EndCopy();

        osCommand.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                        pszSqlTableName, OGRPGEscapeColumnName(oField.GetNameRef()).c_str(),
                        osFieldType.c_str() );
        osCommand += osConstraints;

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
    }

    poFeatureDefn->AddFieldDefn( &oField );
    m_abGeneratedColumns.resize( poFeatureDefn->GetFieldCount() );

    if( pszFIDColumn != nullptr &&
        EQUAL( oField.GetNameRef(), pszFIDColumn ) )
    {
        iFIDAsRegularColumnIndex = poFeatureDefn->GetFieldCount() - 1;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RunAddGeometryColumn()                        */
/************************************************************************/

OGRErr OGRPGTableLayer::RunAddGeometryColumn( const OGRPGGeomFieldDefn *poGeomField )
{
    PGconn *hPGConn = poDS->GetPGConn();

    const char *pszGeometryType = OGRToOGCGeomType(poGeomField->GetType());
    const char *suffix = "";
    int dim = 2;
    if( (poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_3D) &&
        (poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) )
        dim = 4;
    else if( (poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) )
    {
        if( !(wkbFlatten(poGeomField->GetType()) == wkbUnknown) )
            suffix = "M";
        dim = 3;
    }
    else if( poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_3D )
        dim = 3;

    CPLString osCommand;
    osCommand.Printf(
            "SELECT AddGeometryColumn(%s,%s,%s,%d,'%s%s',%d)",
            OGRPGEscapeString(hPGConn, pszSchemaName).c_str(),
            OGRPGEscapeString(hPGConn, pszTableName).c_str(),
            OGRPGEscapeString(hPGConn, poGeomField->GetNameRef()).c_str(),
            poGeomField->nSRSId, pszGeometryType, suffix, dim );

    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

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

OGRErr OGRPGTableLayer::RunCreateSpatialIndex( const OGRPGGeomFieldDefn *poGeomField )
{
    PGconn *hPGConn = poDS->GetPGConn();
    CPLString osCommand;

    osCommand.Printf("CREATE INDEX %s ON %s USING %s (%s)",
                    OGRPGEscapeColumnName(
                        CPLSPrintf("%s_%s_geom_idx", pszTableName, poGeomField->GetNameRef())).c_str(),
                    pszSqlTableName,
                    osSpatialIndexType.c_str(),
                    OGRPGEscapeColumnName(poGeomField->GetNameRef()).c_str());

    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());

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

    // Check if GEOMETRY_NAME layer creation option was set, but no initial
    // column was created in ICreateLayer()
    CPLString osGeomFieldName =
        ( m_osFirstGeometryFieldName.size() ) ? m_osFirstGeometryFieldName :
                                                CPLString(poGeomFieldIn->GetNameRef());
    m_osFirstGeometryFieldName = ""; // reset for potential next geom columns

    auto poGeomField =
        cpl::make_unique<OGRPGGeomFieldDefn>( this, osGeomFieldName );
    if( EQUAL(poGeomField->GetNameRef(), "") )
    {
        if( poFeatureDefn->GetGeomFieldCount() == 0 )
            poGeomField->SetName( "wkb_geometry" );
        else
            poGeomField->SetName(
                CPLSPrintf("wkb_geometry%d", poFeatureDefn->GetGeomFieldCount()+1) );
    }
    auto l_poSRS = poGeomFieldIn->GetSpatialRef();
    if( l_poSRS )
    {
        l_poSRS = l_poSRS->Clone();
        l_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poGeomField->SetSpatialRef(l_poSRS);
        l_poSRS->Release();
    }
/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = OGRPGCommonLaunderName( poGeomField->GetNameRef(), "PG" );

        poGeomField->SetName( pszSafeName );
        CPLFree( pszSafeName );
    }

    OGRSpatialReference* poSRS = poGeomField->GetSpatialRef();
    int nSRSId = poDS->GetUndefinedSRID();
    if( nForcedSRSId != UNDETERMINED_SRID )
        nSRSId = nForcedSRSId;
    else if( poSRS != nullptr )
        nSRSId = poDS->FetchSRSId( poSRS );

    int GeometryTypeFlags = 0;
    if( OGR_GT_HasZ(eType) )
        GeometryTypeFlags |= OGRGeometry::OGR_G_3D;
    if( OGR_GT_HasM(eType) )
        GeometryTypeFlags |= OGRGeometry::OGR_G_MEASURED;
    if( nForcedGeometryTypeFlags >= 0 )
    {
        GeometryTypeFlags = nForcedGeometryTypeFlags;
        eType = OGR_GT_SetModifier(eType,
                                   GeometryTypeFlags & OGRGeometry::OGR_G_3D,
                                   GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);
    }
    poGeomField->SetType(eType);
    poGeomField->SetNullable( poGeomFieldIn->IsNullable() );
    poGeomField->nSRSId = nSRSId;
    poGeomField->GeometryTypeFlags = GeometryTypeFlags;
    poGeomField->ePostgisType = GEOM_TYPE_GEOMETRY;

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if( !bDeferredCreation )
    {
        poDS->EndCopy();

        if( RunAddGeometryColumn(poGeomField.get()) != OGRERR_NONE )
        {
            return OGRERR_FAILURE;
        }

        if( bCreateSpatialIndexFlag )
        {
            if( RunCreateSpatialIndex(poGeomField.get()) != OGRERR_NONE )
            {
                return OGRERR_FAILURE;
            }
        }
    }

    poFeatureDefn->AddGeomFieldDefn( std::move(poGeomField) );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRPGTableLayer::DeleteField( int iField )
{
    PGconn              *hPGConn = poDS->GetPGConn();
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

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

    osCommand.Printf( "ALTER TABLE %s DROP COLUMN %s",
                      pszSqlTableName,
                      OGRPGEscapeColumnName(poFeatureDefn->GetFieldDefn(iField)->GetNameRef()).c_str() );
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

    m_abGeneratedColumns.erase(m_abGeneratedColumns.begin() + iField);

    return poFeatureDefn->DeleteFieldDefn( iField );
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRPGTableLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn )
{
    PGconn              *hPGConn = poDS->GetPGConn();
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

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

    OGRFieldDefn       *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
    OGRFieldDefn        oField( poNewFieldDefn );

    poDS->SoftStartTransaction();

    if (!(nFlagsIn & ALTER_TYPE_FLAG))
    {
        oField.SetSubType(OFSTNone);
        oField.SetType(poFieldDefn->GetType());
        oField.SetSubType(poFieldDefn->GetSubType());
    }

    if (!(nFlagsIn & ALTER_WIDTH_PRECISION_FLAG))
    {
        oField.SetWidth(poFieldDefn->GetWidth());
        oField.SetPrecision(poFieldDefn->GetPrecision());
    }

    if ((nFlagsIn & ALTER_TYPE_FLAG) ||
        (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG))
    {
        CPLString osFieldType = OGRPGCommonLayerGetType(oField,
                                                       CPL_TO_BOOL(bPreservePrecision),
                                                       true);
        if (osFieldType.empty())
        {
            poDS->SoftRollbackTransaction();

            return OGRERR_FAILURE;
        }

        osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s TYPE %s",
                        pszSqlTableName,
                        OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str(),
                        osFieldType.c_str() );

        PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
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

    if( (nFlagsIn & ALTER_NULLABLE_FLAG) &&
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

        PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
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

    // Only supports adding a unique constraint
    if( (nFlagsIn & ALTER_UNIQUE_FLAG) &&
        !poFieldDefn->IsUnique() &&
        poNewFieldDefn->IsUnique() )
    {
        oField.SetUnique(poNewFieldDefn->IsUnique());

        osCommand.Printf( "ALTER TABLE %s ADD UNIQUE (%s)",
                pszSqlTableName,
                OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str() );

        PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
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
    else if( (nFlagsIn & ALTER_UNIQUE_FLAG) &&
              poFieldDefn->IsUnique() &&
              !poNewFieldDefn->IsUnique() )
    {
        oField.SetUnique(TRUE);
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Dropping a UNIQUE constraint is not supported currently");
    }

    if( (nFlagsIn & ALTER_DEFAULT_FLAG) &&
        ((poFieldDefn->GetDefault() == nullptr && poNewFieldDefn->GetDefault() != nullptr) ||
         (poFieldDefn->GetDefault() != nullptr && poNewFieldDefn->GetDefault() == nullptr) ||
         (poFieldDefn->GetDefault() != nullptr && poNewFieldDefn->GetDefault() != nullptr &&
          strcmp(poFieldDefn->GetDefault(), poNewFieldDefn->GetDefault()) != 0)) )
    {
        oField.SetDefault(poNewFieldDefn->GetDefault());

        if( poNewFieldDefn->GetDefault() == nullptr )
            osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s DROP DEFAULT",
                    pszSqlTableName,
                    OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str() );
        else
            osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s SET DEFAULT %s",
                    pszSqlTableName,
                    OGRPGEscapeColumnName(poFieldDefn->GetNameRef()).c_str(),
                    OGRPGCommonLayerGetPGDefault(poNewFieldDefn).c_str());

        PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
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

    if( (nFlagsIn & ALTER_NAME_FLAG) )
    {
        if (bLaunderColumnNames)
        {
            char    *pszSafeName = OGRPGCommonLaunderName( oField.GetNameRef(), "PG" );
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
            PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
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

    if (nFlagsIn & ALTER_NAME_FLAG)
        poFieldDefn->SetName(oField.GetNameRef());
    if (nFlagsIn & ALTER_TYPE_FLAG)
    {
        poFieldDefn->SetSubType(OFSTNone);
        poFieldDefn->SetType(oField.GetType());
        poFieldDefn->SetSubType(oField.GetSubType());
    }
    if (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG)
    {
        poFieldDefn->SetWidth(oField.GetWidth());
        poFieldDefn->SetPrecision(oField.GetPrecision());
    }
    if (nFlagsIn & ALTER_NULLABLE_FLAG)
        poFieldDefn->SetNullable(oField.IsNullable());
    if (nFlagsIn & ALTER_DEFAULT_FLAG)
        poFieldDefn->SetDefault(oField.GetDefault());
    if (nFlagsIn & ALTER_UNIQUE_FLAG)
        poFieldDefn->SetUnique(oField.IsUnique());

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGTableLayer::GetFeature( GIntBig nFeatureId )

{
    GetLayerDefn()->GetFieldCount();

    if( pszFIDColumn == nullptr )
        return OGRLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Issue query for a single record.                                */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = nullptr;
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

    PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        OGRPGClearResult( hResult );

        hResult = OGRPG_PQexec(hPGConn, "FETCH ALL in getfeaturecursor" );

        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        {
            int nRows = PQntuples(hResult);
            if (nRows > 0)
            {
                int* panTempMapFieldNameToIndex = nullptr;
                int* panTempMapFieldNameToGeomIndex = nullptr;
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
    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
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
    CPLString           osCommand;
    GIntBig              nCount = 0;

    osCommand.Printf(
        "SELECT count(*) FROM %s %s",
        pszSqlTableName, osWHERE.c_str() );

    PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( hResult != nullptr && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        nCount = CPLAtoGIntBig(PQgetvalue(hResult,0,0));
    else
        CPLDebug( "PG", "%s; failed.", osCommand.c_str() );
    OGRPGClearResult( hResult );

    return nCount;
}

/************************************************************************/
/*                             ResolveSRID()                            */
/************************************************************************/

void OGRPGTableLayer::ResolveSRID(const OGRPGGeomFieldDefn* poGFldDefn)

{
    PGconn      *hPGConn = poDS->GetPGConn();
    CPLString    osCommand;

    int nSRSId = poDS->GetUndefinedSRID();
    if( !poDS->m_bHasGeometryColumns )
    {
        poGFldDefn->nSRSId = nSRSId;
        return;
    }

    osCommand.Printf(
                "SELECT srid FROM geometry_columns "
                "WHERE f_table_name = %s AND "
                "f_geometry_column = %s",
                OGRPGEscapeString(hPGConn, pszTableName).c_str(),
                OGRPGEscapeString(hPGConn, poGFldDefn->GetNameRef()).c_str());

    osCommand += CPLString().Printf(" AND f_table_schema = %s",
                                    OGRPGEscapeString(hPGConn, pszSchemaName).c_str());

    PGresult* hResult = OGRPG_PQexec(hPGConn, osCommand.c_str() );

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
    if( nSRSId <= 0 && poGFldDefn->ePostgisType == GEOM_TYPE_GEOMETRY &&
        poDS->sPostGISVersion.nMajor >= 0 )
    {
        const char* psGetSRIDFct
            = poDS->sPostGISVersion.nMajor >= 2 ? "ST_SRID" : "getsrid";

        CPLString osGetSRID;
        osGetSRID += "SELECT ";
        osGetSRID += psGetSRIDFct;
        osGetSRID += "(";
        osGetSRID += OGRPGEscapeColumnName(poGFldDefn->GetNameRef());
        osGetSRID += ") FROM ";
        osGetSRID += pszSqlTableName;
        osGetSRID += " WHERE (";
        osGetSRID += OGRPGEscapeColumnName(poGFldDefn->GetNameRef());
        osGetSRID += " IS NOT NULL) LIMIT 1";

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

    size_t size = osFields.size() +  strlen(pszSqlTableName) + 100;
    char *pszCommand = static_cast<char *>(CPLMalloc(size));

    snprintf( pszCommand, size,
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

    int copyResult = PQputCopyEnd(hPGConn, nullptr);

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

    UpdateSequenceIfNeeded();

    return result;
}

/************************************************************************/
/*                       UpdateSequenceIfNeeded()                       */
/************************************************************************/

void OGRPGTableLayer::UpdateSequenceIfNeeded()
{
    if( bNeedToUpdateSequence && pszFIDColumn != nullptr )
    {
        PGconn *hPGConn = poDS->GetPGConn();
        CPLString osCommand;
        osCommand.Printf(
            "SELECT setval(pg_get_serial_sequence(%s, %s), MAX(%s)) FROM %s",
            OGRPGEscapeString(hPGConn, pszSqlTableName).c_str(),
            OGRPGEscapeString(hPGConn, pszFIDColumn).c_str(),
            OGRPGEscapeColumnName(pszFIDColumn).c_str(),
            pszSqlTableName);
        PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand);
        OGRPGClearResult( hResult );
        bNeedToUpdateSequence = false;
    }
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
            poFeatureDefn->GetGeomFieldDefn(i);
        if( !osFieldList.empty() )
            osFieldList += ", ";
        osFieldList += OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef());
    }

    if( bFIDColumnInCopyFields )
    {
        if( !osFieldList.empty() )
            osFieldList += ", ";

        nFIDIndex = poFeatureDefn->GetFieldIndex( pszFIDColumn );

        osFieldList += OGRPGEscapeColumnName(pszFIDColumn);
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if (i == nFIDIndex)
            continue;
        if( m_abGeneratedColumns[i] )
            continue;

        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( !osFieldList.empty() )
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
    if( pszOverrideColumnTypes == nullptr )
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
    if( !osCur.empty() )
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

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();

    OGRPGGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->GetGeomFieldDefn(iGeomField);

    // if bForce is 0 and ePostgisType is not GEOM_TYPE_GEOGRAPHY we can use
    // the ST_EstimatedExtent function which is quicker
    // ST_EstimatedExtent was called ST_Estimated_Extent up to PostGIS 2.0.x
    // ST_EstimatedExtent returns NULL in absence of statistics (an exception before
    //   PostGIS 1.5.4)
    if ( bForce == 0 && TestCapability(OLCFastGetExtent) )
    {
        PGconn *hPGConn = poDS->GetPGConn();

        const char* pszExtentFct =
            poDS->sPostGISVersion.nMajor > 2 ||
            ( poDS->sPostGISVersion.nMajor == 2 && poDS->sPostGISVersion.nMinor >= 1 )
            ? "ST_EstimatedExtent"
            : "ST_Estimated_Extent";

        osCommand.Printf( "SELECT %s(%s, %s, %s)",
                        pszExtentFct,
                        OGRPGEscapeString(hPGConn, pszSchemaName).c_str(),
                        OGRPGEscapeString(hPGConn, pszTableName).c_str(),
                        OGRPGEscapeString(hPGConn, poGeomFieldDefn->GetNameRef()).c_str() );

        /* Quiet error: ST_Estimated_Extent may return an error if statistics */
        /* have not been computed */
        if( RunGetExtentRequest(psExtent, bForce, osCommand, TRUE) == OGRERR_NONE )
            return OGRERR_NONE;

        CPLDebug(
            "PG",
            "Unable to get estimated extent by PostGIS. Trying real extent." );
    }

    return OGRPGLayer::GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                           Rename()                                   */
/************************************************************************/

OGRErr OGRPGTableLayer::Rename(const char* pszNewName)
{
    if( !TestCapability(OLCRename) )
        return OGRERR_FAILURE;

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    poDS->EndCopy();
    ResetReading();

    char* pszNewSqlTableName = CPLStrdup(OGRPGEscapeColumnName(pszNewName));
    PGconn *hPGConn = poDS->GetPGConn();
    CPLString osCommand;
    osCommand.Printf("ALTER TABLE %s RENAME TO %s",
                     pszSqlTableName, pszNewSqlTableName);
    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand);

    OGRErr eRet = OGRERR_NONE;
    if ( !hResult || PQresultStatus(hResult) != PGRES_COMMAND_OK )
    {
        eRet = OGRERR_FAILURE;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage(hPGConn) );

        CPLFree(pszNewSqlTableName);
    }
    else
    {
        CPLFree(pszTableName);
        pszTableName = CPLStrdup(pszNewName);

        CPLFree(pszSqlTableName);
        pszSqlTableName = pszNewSqlTableName;

        SetDescription(pszNewName);
        poFeatureDefn->SetName(pszNewName);
    }

    OGRPGClearResult( hResult );

    return eRet;
}

/************************************************************************/
/*                        SetDeferredCreation()                         */
/************************************************************************/

void OGRPGTableLayer::SetDeferredCreation(int bDeferredCreationIn, CPLString osCreateTableIn)
{
    bDeferredCreation = bDeferredCreationIn;
    osCreateTable = osCreateTableIn;
}

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRPGTableLayer::RunDeferredCreationIfNecessary()
{
    if( !bDeferredCreation )
        return OGRERR_NONE;
    bDeferredCreation = FALSE;

    poDS->EndCopy();

    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRPGGeomFieldDefn *poGeomField = poFeatureDefn->GetGeomFieldDefn(i);

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
            if( (poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_3D) && (poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) )
                osCreateTable += "ZM";
            else if( poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_3D )
                osCreateTable += "Z";
            else if( poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED )
                osCreateTable += "M";
            if( poGeomField->nSRSId > 0 )
                osCreateTable += CPLSPrintf(",%d", poGeomField->nSRSId);
            osCreateTable += ")";
            if( !poGeomField->IsNullable() )
                osCreateTable += " NOT NULL";
        }
    }

    osCreateTable += " )";
    CPLString osCommand(osCreateTable);

    PGconn *hPGConn = poDS->GetPGConn();

    PGresult *hResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
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
        for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRPGGeomFieldDefn *poGeomField = poFeatureDefn->GetGeomFieldDefn(i);
            if( poGeomField->ePostgisType == GEOM_TYPE_GEOMETRY &&
                RunAddGeometryColumn(poGeomField) != OGRERR_NONE )
            {
                return OGRERR_FAILURE;
            }
        }
    }

    if( bCreateSpatialIndexFlag )
    {
        for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRPGGeomFieldDefn *poGeomField = poFeatureDefn->GetGeomFieldDefn(i);
            if( RunCreateSpatialIndex(poGeomField) != OGRERR_NONE )
            {
                return OGRERR_FAILURE;
            }
        }
    }

    char** papszMD = OGRLayer::GetMetadata();
    if( papszMD != nullptr )
        SetMetadata( papszMD );

    return OGRERR_NONE;
}

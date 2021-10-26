/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDumpDataSource class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <cstring>
#include "ogr_pgdump.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGRPGDumpDataSource()                           */
/************************************************************************/

OGRPGDumpDataSource::OGRPGDumpDataSource( const char* pszNameIn,
                                          char** papszOptions ) :
    pszName(CPLStrdup(pszNameIn))
{
    const char *pszCRLFFormat = CSLFetchNameValue( papszOptions, "LINEFORMAT");

    bool bUseCRLF = false;
    if( pszCRLFFormat == nullptr )
    {
#ifdef WIN32
        bUseCRLF = true;
#endif
    }
    else if( EQUAL(pszCRLFFormat, "CRLF") )
    {
        bUseCRLF = true;
    }
    else if( EQUAL(pszCRLFFormat, "LF") )
    {
        bUseCRLF = false;
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = true;
#endif
    }

    if( bUseCRLF )
        pszEOL =  "\r\n";
}

/************************************************************************/
/*                          ~OGRPGDumpDataSource()                          */
/************************************************************************/

OGRPGDumpDataSource::~OGRPGDumpDataSource()

{
    EndCopy();
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    if( fp )
    {
        LogCommit();
        VSIFCloseL(fp);
        fp = nullptr;
    }
    CPLFree(papoLayers);
    CPLFree(pszName);
}

/************************************************************************/
/*                         LogStartTransaction()                        */
/************************************************************************/

void OGRPGDumpDataSource::LogStartTransaction()
{
    if( bInTransaction )
        return;
    bInTransaction = true;
    Log("BEGIN");
}

/************************************************************************/
/*                             LogCommit()                              */
/************************************************************************/

void OGRPGDumpDataSource::LogCommit()
{
    EndCopy();

    if( !bInTransaction )
        return;
    bInTransaction = false;
    Log("COMMIT");
}

/************************************************************************/
/*                         OGRPGCommonLaunderName()                     */
/************************************************************************/

char *OGRPGCommonLaunderName( const char *pszSrcName,
                              const char* pszDebugPrefix )

{
    char *pszSafeName = CPLStrdup( pszSrcName );

    for( int i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) tolower( pszSafeName[i] );
        if( pszSafeName[i] == '\'' ||
            pszSafeName[i] == '-' ||
            pszSafeName[i] == '#' )
        {
            pszSafeName[i] = '_';
        }
    }

    if( strcmp(pszSrcName,pszSafeName) != 0 )
        CPLDebug(pszDebugPrefix, "LaunderName('%s') -> '%s'",
                 pszSrcName, pszSafeName);

    return pszSafeName;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPGDumpDataSource::ICreateLayer( const char * pszLayerName,
                                   OGRSpatialReference *poSRS,
                                   OGRwkbGeometryType eType,
                                   char ** papszOptions )

{
    const char* pszFIDColumnNameIn = CSLFetchNameValue(papszOptions, "FID");
    CPLString osFIDColumnName;
    if (pszFIDColumnNameIn == nullptr)
        osFIDColumnName = "ogc_fid";
    else
    {
        if( CPLFetchBool(papszOptions,"LAUNDER", true) )
        {
            char *pszLaunderedFid =
                OGRPGCommonLaunderName(pszFIDColumnNameIn, "PGDump");
            osFIDColumnName = pszLaunderedFid;
            CPLFree(pszLaunderedFid);
        }
        else
        {
            osFIDColumnName = pszFIDColumnNameIn;
        }
    }
    const CPLString osFIDColumnNameEscaped =
        OGRPGDumpEscapeColumnName(osFIDColumnName);

    if (STARTS_WITH(pszLayerName, "pg"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The layer name should not begin by 'pg' as it is a reserved "
                 "prefix");
    }

    bool bHavePostGIS = true;
    // bHavePostGIS = CPLFetchBool(papszOptions, "POSTGIS", true);

    const bool bCreateTable = CPLFetchBool(papszOptions, "CREATE_TABLE", true);
    const bool bCreateSchema =
        CPLFetchBool(papszOptions, "CREATE_SCHEMA", true);
    const char* pszDropTable =
        CSLFetchNameValueDef(papszOptions, "DROP_TABLE", "IF_EXISTS");
    int GeometryTypeFlags = 0;

    if( OGR_GT_HasZ((OGRwkbGeometryType)eType) )
        GeometryTypeFlags |= OGRGeometry::OGR_G_3D;
    if( OGR_GT_HasM((OGRwkbGeometryType)eType) )
        GeometryTypeFlags |= OGRGeometry::OGR_G_MEASURED;

    int ForcedGeometryTypeFlags = -1;
    const char* pszDim = CSLFetchNameValue( papszOptions, "DIM");
    if( pszDim != nullptr )
    {
        if( EQUAL(pszDim, "XY") || EQUAL(pszDim, "2") )
        {
            GeometryTypeFlags = 0;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else if( EQUAL(pszDim, "XYZ") || EQUAL(pszDim, "3") )
        {
            GeometryTypeFlags = OGRGeometry::OGR_G_3D;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else if( EQUAL(pszDim, "XYM") )
        {
            GeometryTypeFlags = OGRGeometry::OGR_G_MEASURED;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else if( EQUAL(pszDim, "XYZM") || EQUAL(pszDim, "4") )
        {
            GeometryTypeFlags = OGRGeometry::OGR_G_3D | OGRGeometry::OGR_G_MEASURED;
            ForcedGeometryTypeFlags = GeometryTypeFlags;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for DIM");
        }
    }

    const int nDimension =
        2 + ((GeometryTypeFlags & OGRGeometry::OGR_G_3D) ? 1 : 0)
        + ((GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) ? 1 : 0);

    /* Should we turn layers with None geometry type as Unknown/GEOMETRY */
    /* so they are still recorded in geometry_columns table ? (#4012) */
    const bool bNoneAsUnknown =
        CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "NONE_AS_UNKNOWN", "NO"));

    if( bNoneAsUnknown && eType == wkbNone )
        eType = wkbUnknown;
    else if( eType == wkbNone )
        bHavePostGIS = false;

    const bool bExtractSchemaFromLayerName =
        CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "EXTRACT_SCHEMA_FROM_LAYER_NAME", "YES"));

    // Postgres Schema handling:

    // Extract schema name from input layer name or passed with -lco SCHEMA.
    // Set layer name to "schema.table" or to "table" if schema ==
    // current_schema() Usage without schema name is backwards compatible

    const char* pszDotPos = strstr(pszLayerName,".");
    char *pszTableName = nullptr;
    char *pszSchemaName = nullptr;

    if ( pszDotPos != nullptr && bExtractSchemaFromLayerName )
    {
      const int length = static_cast<int>(pszDotPos - pszLayerName);
      pszSchemaName = (char*)CPLMalloc(length+1);
      strncpy(pszSchemaName, pszLayerName, length);
      pszSchemaName[length] = '\0';

      if( CPLFetchBool(papszOptions, "LAUNDER", true) )
          pszTableName = OGRPGCommonLaunderName( pszDotPos + 1, "PGDump" ); //skip "."
      else
          pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
      pszSchemaName = nullptr;
      if( CPLFetchBool(papszOptions, "LAUNDER", true) )
          pszTableName = OGRPGCommonLaunderName( pszLayerName, "PGDump" ); //skip "."
      else
          pszTableName = CPLStrdup( pszLayerName ); //skip "."
    }

    LogCommit();

/* -------------------------------------------------------------------- */
/*      Set the default schema for the layers.                          */
/* -------------------------------------------------------------------- */
    CPLString osCommand;

    if( CSLFetchNameValue( papszOptions, "SCHEMA" ) != nullptr )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue( papszOptions, "SCHEMA" ));
        if( bCreateSchema )
        {
            osCommand.Printf("CREATE SCHEMA \"%s\"", pszSchemaName);
            Log(osCommand);
        }
    }

    if ( pszSchemaName == nullptr)
    {
        pszSchemaName = CPLStrdup("public");
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?                                  */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Layer %s already exists, CreateLayer failed.\n",
                      pszLayerName );
            CPLFree( pszTableName );
            CPLFree( pszSchemaName );
            return nullptr;
        }
    }

    if( bCreateTable && (EQUAL(pszDropTable, "YES") ||
                         EQUAL(pszDropTable, "ON") ||
                         EQUAL(pszDropTable, "TRUE") ||
                         EQUAL(pszDropTable, "IF_EXISTS")) )
    {
        if (EQUAL(pszDropTable, "IF_EXISTS"))
            osCommand.Printf("DROP TABLE IF EXISTS \"%s\".\"%s\" CASCADE",
                             pszSchemaName, pszTableName );
        else
            osCommand.Printf("DROP TABLE \"%s\".\"%s\" CASCADE",
                             pszSchemaName, pszTableName );
        Log(osCommand);
    }

/* -------------------------------------------------------------------- */
/*      Handle the GEOM_TYPE option.                                    */
/* -------------------------------------------------------------------- */
    const char *pszGeomType = CSLFetchNameValue( papszOptions, "GEOM_TYPE" );
    if( pszGeomType == nullptr )
    {
        pszGeomType = "geometry";
    }

    if( !EQUAL(pszGeomType,"geometry") && !EQUAL(pszGeomType, "geography"))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "GEOM_TYPE in PostGIS enabled databases must be 'geometry' or "
            "'geography'.  Creation of layer %s with GEOM_TYPE %s has failed.",
            pszLayerName, pszGeomType );
        CPLFree( pszTableName );
        CPLFree( pszSchemaName );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding tot the srs table if needed.                             */
/* -------------------------------------------------------------------- */
    const char* pszPostgisVersion =
        CSLFetchNameValueDef( papszOptions, "POSTGIS_VERSION", "2.2" );
    const int nPostGISMajor = atoi(pszPostgisVersion);
    const char* pszPostgisVersionDot = strchr(pszPostgisVersion, '.');
    const int nPostGISMinor =
          pszPostgisVersionDot ? atoi(pszPostgisVersionDot+1) : 0;
    const int nUnknownSRSId = nPostGISMajor >= 2 ? 0 : -1;

    int nSRSId = nUnknownSRSId;
    int nForcedSRSId = -2;
    if( CSLFetchNameValue( papszOptions, "SRID") != nullptr )
    {
        nSRSId = atoi(CSLFetchNameValue( papszOptions, "SRID"));
        nForcedSRSId = nSRSId;
    }
    else
    {
        if( poSRS )
        {
            const char* pszAuthorityName = poSRS->GetAuthorityName(nullptr);
            if( pszAuthorityName != nullptr && EQUAL( pszAuthorityName, "EPSG" ) )
            {
                /* Assume the EPSG Id is the SRS ID. Might be a wrong guess ! */
                nSRSId = atoi( poSRS->GetAuthorityCode(nullptr) );
            }
            else
            {
                const char* pszGeogCSName = poSRS->GetAttrValue("GEOGCS");
                if( pszGeogCSName != nullptr &&
                    EQUAL(pszGeogCSName, "GCS_WGS_1984") )
                {
                    nSRSId = 4326;
                }
            }
        }
    }

    CPLString osEscapedTableNameSingleQuote =
        OGRPGDumpEscapeString(pszTableName);
    const char* pszEscapedTableNameSingleQuote =
        osEscapedTableNameSingleQuote.c_str();

    const char *pszGeometryType = OGRToOGCGeomType(eType);

    const char *pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
    if( bHavePostGIS && !EQUAL(pszGeomType, "geography") )
    {
        if( pszGFldName == nullptr )
            pszGFldName = "wkb_geometry";

        if( nPostGISMajor < 2 )
        {
            // Sometimes there is an old cruft entry in the geometry_columns
            // table if things were not properly cleaned up before.  We make
            // an effort to clean out such cruft.
            //
            // Note: PostGIS 2.0 defines geometry_columns as a view (no clean up
            // is needed).

            osCommand.Printf(
                "DELETE FROM geometry_columns "
                "WHERE f_table_name = %s AND f_table_schema = '%s'",
                pszEscapedTableNameSingleQuote, pszSchemaName );
            if( bCreateTable )
                Log(osCommand);
        }
    }

    LogStartTransaction();

/* -------------------------------------------------------------------- */
/*      Create a basic table with the FID.  Also include the            */
/*      geometry if this is not a PostGIS enabled table.                */
/* -------------------------------------------------------------------- */
    const bool bFID64 = CPLFetchBool(papszOptions, "FID64", false);
    const char* pszSerialType = bFID64 ? "BIGSERIAL": "SERIAL";

    CPLString osCreateTable;
    const bool bTemporary = CPLFetchBool( papszOptions, "TEMPORARY", false );
    if( bTemporary )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup("pg_temp_1");
        osCreateTable.Printf("CREATE TEMPORARY TABLE \"%s\"", pszTableName);
    }
    else
    {
        osCreateTable.Printf("CREATE%s TABLE \"%s\".\"%s\"",
                             CPLFetchBool( papszOptions, "UNLOGGED", false ) ?
                             " UNLOGGED": "",
                             pszSchemaName, pszTableName);
    }

    if( !bHavePostGIS )
    {
        if (eType == wkbNone)
            osCommand.Printf(
                "%s ( "
                "   %s %s, "
                "   CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                osCreateTable.c_str(), osFIDColumnNameEscaped.c_str(),
                pszSerialType, pszTableName, osFIDColumnNameEscaped.c_str() );
        else
            osCommand.Printf(
                "%s ( "
                "   %s %s, "
                "   WKB_GEOMETRY %s, "
                "   CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                osCreateTable.c_str(), osFIDColumnNameEscaped.c_str(),
                pszSerialType, pszGeomType, pszTableName,
                osFIDColumnNameEscaped.c_str() );
    }
    else if ( EQUAL(pszGeomType, "geography") )
    {
        if( CSLFetchNameValue( papszOptions, "GEOMETRY_NAME") != nullptr )
            pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "the_geog";

        const char *suffix = "";
        if( (GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) &&
            (GeometryTypeFlags & OGRGeometry::OGR_G_3D) )
        {
            suffix = "ZM";
        }
        else if( (GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) )
        {
            suffix = "M";
        }
        else if( (GeometryTypeFlags & OGRGeometry::OGR_G_3D) )
        {
            suffix = "Z";
        }

        if( nSRSId )
            osCommand.Printf(
                "%s ( %s %s, \"%s\" geography(%s%s,%d), "
                "CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                osCreateTable.c_str(), osFIDColumnNameEscaped.c_str(),
                pszSerialType, pszGFldName, pszGeometryType, suffix, nSRSId,
                pszTableName, osFIDColumnNameEscaped.c_str() );
        else
            osCommand.Printf(
                "%s ( %s %s, \"%s\" geography(%s%s), "
                "CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                osCreateTable.c_str(), osFIDColumnNameEscaped.c_str(),
                pszSerialType, pszGFldName, pszGeometryType, suffix,
                pszTableName, osFIDColumnNameEscaped.c_str() );
    }
    else
    {
        osCommand.Printf(
            "%s ( %s %s, CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
            osCreateTable.c_str(), osFIDColumnNameEscaped.c_str(),
            pszSerialType, pszTableName, osFIDColumnNameEscaped.c_str() );
    }

    if( bCreateTable )
        Log(osCommand);

/* -------------------------------------------------------------------- */
/*      Eventually we should be adding this table to a table of         */
/*      "geometric layers", capturing the WKT projection, and           */
/*      perhaps some other housekeeping.                                */
/* -------------------------------------------------------------------- */
    if( bCreateTable && bHavePostGIS && !EQUAL(pszGeomType, "geography") )
    {
        const char *suffix = "";
        if( GeometryTypeFlags == static_cast<int>(OGRGeometry::OGR_G_MEASURED) &&
            wkbFlatten(eType) != wkbUnknown )
        {
            suffix = "M";
        }

        osCommand.Printf(
                "SELECT AddGeometryColumn('%s',%s,'%s',%d,'%s%s',%d)",
                pszSchemaName, pszEscapedTableNameSingleQuote, pszGFldName,
                nSRSId, pszGeometryType, suffix, nDimension );
        Log(osCommand);
    }

    const char *pszSI = CSLFetchNameValueDef( papszOptions, "SPATIAL_INDEX", "GIST" );
    const bool bCreateSpatialIndex = ( EQUAL(pszSI, "GIST") ||
        EQUAL(pszSI, "SPGIST") || EQUAL(pszSI, "BRIN") ||
        EQUAL(pszSI, "YES") || EQUAL(pszSI, "ON") || EQUAL(pszSI, "TRUE") );
    if( !bCreateSpatialIndex && !EQUAL(pszSI, "NO") && !EQUAL(pszSI, "OFF") &&
        !EQUAL(pszSI, "FALSE") && !EQUAL(pszSI, "NONE") )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "SPATIAL_INDEX=%s not supported", pszSI);
    }
    const char* pszSpatialIndexType = EQUAL(pszSI, "SPGIST") ? "SPGIST" :
                                      EQUAL(pszSI, "BRIN") ? "BRIN" : "GIST";

    if( bCreateTable && bHavePostGIS && bCreateSpatialIndex )
    {
/* -------------------------------------------------------------------- */
/*      Create the spatial index.                                       */
/*                                                                      */
/*      We're doing this before we add geometry and record to the table */
/*      so this may not be exactly the best way to do it.               */
/* -------------------------------------------------------------------- */
        osCommand.Printf(
            "CREATE INDEX \"%s_%s_geom_idx\" "
            "ON \"%s\".\"%s\" "
            "USING %s (\"%s\")",
            pszTableName, pszGFldName, pszSchemaName, pszTableName,
            pszSpatialIndexType,
            pszGFldName);

        Log(osCommand);
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    const bool bWriteAsHex =
        !CPLFetchBool(papszOptions, "WRITE_EWKT_GEOM", false);

    OGRPGDumpLayer *poLayer =
        new OGRPGDumpLayer( this, pszSchemaName, pszTableName,
                            osFIDColumnName, bWriteAsHex, bCreateTable );
    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", true) );
    poLayer->SetPrecisionFlag( CPLFetchBool(papszOptions, "PRECISION", true));

    const char* pszOverrideColumnTypes =
        CSLFetchNameValue( papszOptions, "COLUMN_TYPES" );
    poLayer->SetOverrideColumnTypes(pszOverrideColumnTypes);
    poLayer->SetUnknownSRSId(nUnknownSRSId);
    poLayer->SetForcedSRSId(nForcedSRSId);
    poLayer->SetCreateSpatialIndex(bCreateSpatialIndex, pszSpatialIndexType);
    poLayer->SetPostGISVersion(nPostGISMajor, nPostGISMinor);
    poLayer->SetForcedGeometryTypeFlags(ForcedGeometryTypeFlags);

    const char* pszDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");
    if( pszDescription != nullptr )
        poLayer->SetForcedDescription( pszDescription );

    if( bHavePostGIS )
    {
        OGRGeomFieldDefn oTmp( pszGFldName, eType );
        auto poGeomField = cpl::make_unique<OGRPGDumpGeomFieldDefn>(&oTmp);
        poGeomField->nSRSId = nSRSId;
        poGeomField->GeometryTypeFlags = GeometryTypeFlags;
        poLayer->GetLayerDefn()->AddGeomFieldDefn(std::move(poGeomField));
    }
    else if( pszGFldName )
        poLayer->SetGeometryFieldName(pszGFldName);

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRPGDumpLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRPGDumpLayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;

    CPLFree( pszTableName );
    CPLFree( pszSchemaName );

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGDumpDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPGDumpDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                  Log()                               */
/************************************************************************/

bool OGRPGDumpDataSource::Log( const char* pszStr, bool bAddSemiColumn )
{
    if( fp == nullptr )
    {
        if( bTriedOpen )
            return false;
        bTriedOpen = true;
        fp = VSIFOpenL(pszName, "wb");
        if (fp == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszName);
            return false;
        }
    }

    if( bAddSemiColumn )
        VSIFPrintfL(fp, "%s;%s", pszStr, pszEOL);
    else
        VSIFPrintfL(fp, "%s%s", pszStr, pszEOL);
    return true;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/
void OGRPGDumpDataSource::StartCopy( OGRPGDumpLayer *poPGLayer )
{
    EndCopy();
    poLayerInCopyMode = poPGLayer;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/
OGRErr OGRPGDumpDataSource::EndCopy()
{
    if( poLayerInCopyMode != nullptr )
    {
        OGRErr result = poLayerInCopyMode->EndCopy();
        poLayerInCopyMode = nullptr;

        return result;
    }

    return OGRERR_NONE;
}

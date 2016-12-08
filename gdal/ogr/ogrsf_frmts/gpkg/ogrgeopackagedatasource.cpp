/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements GDALGeoPackageDataset class
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_geopackage.h"
#include "ogr_p.h"
#include "swq.h"
#include "gdalwarper.h"

#include <cstdlib>

#include <algorithm>

CPL_CVSID("$Id$");

// 1.1.1: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) in the
// application id.
// http://opengis.github.io/geopackage/#_file_format
// 0x47503130 = 1196437808
static const int GPKG_APPLICATION_ID = 1196437808;

/* "GP10" in ASCII bytes */
static const char aGpkgId[4] = {0x47, 0x50, 0x31, 0x30};
static const size_t szGpkgIdPos = 68;

/************************************************************************/
/*                             Tiling schemes                           */
/************************************************************************/

typedef struct
{
    const char* pszName;
    int         nEPSGCode;
    double      dfMinX;
    double      dfMaxY;
    int         nTileXCountZoomLevel0;
    int         nTileYCountZoomLevel0;
    int         nTileWidth;
    int         nTileHeight;
    double      dfPixelXSizeZoomLevel0;
    double      dfPixelYSizeZoomLevel0;
} TilingSchemeDefinition;

static const TilingSchemeDefinition asTilingShemes[] =
{
    /* See http://portal.opengeospatial.org/files/?artifact_id=35326 (WMTS 1.0), Annex E.3 */
    { "GoogleCRS84Quad",
      4326,
      -180.0, 180.0,
      1, 1,
      256, 256,
      360.0 / 256, 360.0 / 256 },

    /* See http://portal.opengeospatial.org/files/?artifact_id=35326 (WMTS 1.0), Annex E.4 */
    { "GoogleMapsCompatible",
      3857,
      -(156543.0339280410*256) /2, (156543.0339280410*256) /2,
      1, 1,
      256, 256,
      156543.0339280410, 156543.0339280410 },

    /* See InspireCRS84Quad at http://inspire.ec.europa.eu/documents/Network_Services/TechnicalGuidance_ViewServices_v3.0.pdf */
    /* This is exactly the same as PseudoTMS_GlobalGeodetic */
    { "InspireCRS84Quad",
      4326,
      -180.0, 90.0,
      2, 1,
      256, 256,
      180.0 / 256, 180.0 / 256 },

    /* See global-geodetic at http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
    { "PseudoTMS_GlobalGeodetic",
      4326,
      -180.0, 90.0,
      2, 1,
      256, 256,
      180.0 / 256, 180.0 / 256 },

    /* See global-mercator at http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
    { "PseudoTMS_GlobalMercator",
      3857,
      -20037508.34, 20037508.34,
      2, 2,
      256, 256,
      78271.516, 78271.516 },
};

/* Only recent versions of SQLite will let us muck with application_id */
/* via a PRAGMA statement, so we have to write directly into the */
/* file header here. */
/* We do this at the *end* of initialization so that there is */
/* data to write down to a file, and we will have a writable file */
/* once we close the SQLite connection */
OGRErr GDALGeoPackageDataset::SetApplicationId()
{
    CPLAssert( hDB != NULL );
    CPLAssert( m_pszFilename != NULL );

#ifdef SPATIALITE_412_OR_LATER
    FinishNewSpatialite();
#endif
    /* Have to flush the file before f***ing with the header */
    CloseDB();

    size_t szWritten = 0;

    /* Open for modification, write to application id area */
    VSILFILE *pfFile = VSIFOpenL( m_pszFilename, "rb+" );
    if( pfFile == NULL )
        return OGRERR_FAILURE;
    VSIFSeekL(pfFile, szGpkgIdPos, SEEK_SET);
    szWritten = VSIFWriteL(aGpkgId, 1, 4, pfFile);
    VSIFCloseL(pfFile);

    /* If we didn't write out exactly four bytes, something */
    /* terrible has happened */
    if ( szWritten != 4 )
    {
        return OGRERR_FAILURE;
    }

    /* And re-open the file */
    if( !OpenOrCreateDB(SQLITE_OPEN_READWRITE) )
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}

bool GDALGeoPackageDataset::ReOpenDB()
{
    CPLAssert( hDB != NULL );
    CPLAssert( m_pszFilename != NULL );

#ifdef SPATIALITE_412_OR_LATER
    FinishNewSpatialite();
#endif
    CloseDB();

    /* And re-open the file */
    return OpenOrCreateDB(SQLITE_OPEN_READWRITE);
}

/* Returns the first row of first column of SQL as integer */
OGRErr GDALGeoPackageDataset::PragmaCheck(
    const char * pszPragma, const char * pszExpected, int nRowsExpected )
{
    CPLAssert( pszPragma != NULL );
    CPLAssert( pszExpected != NULL );
    CPLAssert( nRowsExpected >= 0 );

    char **papszResult = NULL;
    int nRowCount = 0;
    int nColCount = 0;
    char *pszErrMsg = NULL;

    int rc = sqlite3_get_table(
        hDB,
        CPLSPrintf("PRAGMA %s", pszPragma),
        &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to execute PRAGMA %s", pszPragma);
        return OGRERR_FAILURE;
    }

    if ( nRowCount != nRowsExpected )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "bad result for PRAGMA %s, got %d rows, expected %d",
                  pszPragma, nRowCount, nRowsExpected );
        return OGRERR_FAILURE;
    }

    if ( nRowCount > 0 && ! EQUAL(papszResult[1], pszExpected) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "invalid %s (expected '%s', got '%s')",
                  pszPragma, pszExpected, papszResult[1]);
        return OGRERR_FAILURE;
    }

    sqlite3_free_table(papszResult);

    return OGRERR_NONE;
}

static OGRErr GDALGPKGImportFromEPSG(OGRSpatialReference *poSpatialRef,
                                     int nEPSGCode)
{
    CPLPushErrorHandler(CPLQuietErrorHandler);
    const OGRErr eErr = poSpatialRef->importFromEPSG(nEPSGCode);
    CPLPopErrorHandler();
    CPLErrorReset();
    return eErr;
}

OGRSpatialReference* GDALGeoPackageDataset::GetSpatialRef(int iSrsId)
{
    /* Should we do something special with undefined SRS ? */
    if( iSrsId == 0 || iSrsId == -1 )
    {
        return NULL;
    }

    CPLString oSQL;
    oSQL.Printf( "SELECT definition, organization, organization_coordsys_id "
                 "FROM gpkg_spatial_ref_sys WHERE srs_id = %d", iSrsId );

    SQLResult oResult;
    OGRErr err = SQLQuery(hDB, oSQL.c_str(), &oResult);

    if ( err != OGRERR_NONE || oResult.nRowCount != 1 )
    {
        SQLResultFree(&oResult);
        CPLError( CE_Warning, CPLE_AppDefined,
                  "unable to read srs_id '%d' from gpkg_spatial_ref_sys",
                  iSrsId);
        return NULL;
    }

    const char *pszWkt = SQLResultGetValue(&oResult, 0, 0);
    if ( ! pszWkt )
    {
        SQLResultFree(&oResult);
        CPLError( CE_Warning, CPLE_AppDefined,
                  "null definition for srs_id '%d' in gpkg_spatial_ref_sys",
                  iSrsId);
        return NULL;
    }

    const char* pszOrganization = SQLResultGetValue(&oResult, 1, 0);
    const char* pszOrganizationCoordsysID = SQLResultGetValue(&oResult, 2, 0);

    OGRSpatialReference *poSpatialRef = new OGRSpatialReference();
    // Try to import first from EPSG code, and then from WKT
    if( !(pszOrganization && pszOrganizationCoordsysID
          && EQUAL(pszOrganization, "EPSG") &&
          GDALGPKGImportFromEPSG(poSpatialRef, atoi(pszOrganizationCoordsysID))
          == OGRERR_NONE) &&
        poSpatialRef->SetFromUserInput(pszWkt) != OGRERR_NONE )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to parse srs_id '%d' well-known text '%s'",
                iSrsId, pszWkt);
        SQLResultFree(&oResult);
        delete poSpatialRef;
        return NULL;
    }

    SQLResultFree(&oResult);
    return poSpatialRef;
}

const char * GDALGeoPackageDataset::GetSrsName(
    const OGRSpatialReference *poSRS )
{
    const OGR_SRSNode *node = poSRS->GetAttrNode("PROJCS");

    /* Projected coordinate system? */
    if ( node != NULL )
    {
        return node->GetChild(0)->GetValue();
    }

    /* Geographic coordinate system? */
    if ( (node = poSRS->GetAttrNode("GEOGCS")) != NULL )
    {
        return node->GetChild(0)->GetValue();
    }

    // Something odd.  Return empty.
    return "Unnamed SRS";
}

int GDALGeoPackageDataset::GetSrsId(const OGRSpatialReference * cpoSRS)
{
    if( cpoSRS == NULL )
        return DEFAULT_SRID;

    OGRSpatialReference *poSRS = cpoSRS->Clone();

    const char* pszAuthorityName = poSRS->GetAuthorityName( NULL );

    if ( pszAuthorityName == NULL || strlen(pszAuthorityName) == 0 )
    {
        // Try to force identify an EPSG code.
        poSRS->AutoIdentifyEPSG();

        pszAuthorityName = poSRS->GetAuthorityName(NULL);
        if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
            if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                poSRS->importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = poSRS->GetAuthorityName(NULL);
            }
        }
    }

    // Check whether the EPSG authority code is already mapped to a
    // SRS ID.
    char *pszSQL = NULL;
    int nSRSId = DEFAULT_SRID;
    int nAuthorityCode = 0;
    OGRErr err = OGRERR_NONE;
    bool bCanUseAuthorityCode = false;
    if ( pszAuthorityName != NULL && strlen(pszAuthorityName) > 0 )
    {
        // For the root authority name 'EPSG', the authority code
        // should always be integral
        nAuthorityCode = atoi( poSRS->GetAuthorityCode(NULL) );

        pszSQL = sqlite3_mprintf(
                         "SELECT srs_id FROM gpkg_spatial_ref_sys WHERE "
                         "upper(organization) = upper('%q') AND "
                         "organization_coordsys_id = %d",
                         pszAuthorityName, nAuthorityCode );

        nSRSId = SQLGetInteger(hDB, pszSQL, &err);
        sqlite3_free(pszSQL);

        // Got a match? Return it!
        if ( OGRERR_NONE == err )
        {
            delete poSRS;
            return nSRSId;
        }

        // No match, but maybe we can use the nAuthorityCode as the nSRSId?
        pszSQL = sqlite3_mprintf(
                         "SELECT Count(*) FROM gpkg_spatial_ref_sys WHERE "
                         "srs_id = %d", nAuthorityCode );

        // Yep, we can!
        if ( ! SQLGetInteger(hDB, pszSQL, &err) && err == OGRERR_NONE )
            bCanUseAuthorityCode = true;
        sqlite3_free(pszSQL);
    }

    // Translate SRS to WKT.
    char *pszWKT = NULL;
    if( poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        delete poSRS;
        CPLFree(pszWKT);
        return DEFAULT_SRID;
    }

    // Reuse the authority code number as SRS_ID if we can
    if ( bCanUseAuthorityCode )
    {
        nSRSId = nAuthorityCode;
    }
    // Otherwise, generate a new SRS_ID number (max + 1)
    else
    {
        // Get the current maximum srid in the srs table.
        const int nMaxSRSId
            = SQLGetInteger(
                hDB, "SELECT MAX(srs_id) FROM gpkg_spatial_ref_sys", &err );
        if ( OGRERR_NONE != err )
        {
            CPLFree(pszWKT);
            delete poSRS;
            return DEFAULT_SRID;
        }

        nSRSId = nMaxSRSId + 1;
    }

    // Add new SRS row to gpkg_spatial_ref_sys.
    if( pszAuthorityName != NULL && nAuthorityCode > 0 )
    {
        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_spatial_ref_sys "
            "(srs_name,srs_id,organization,organization_coordsys_id,"
            "definition) VALUES ('%q', %d, upper('%q'), %d, '%q')",
            GetSrsName(poSRS), nSRSId, pszAuthorityName, nAuthorityCode,
            pszWKT );
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_spatial_ref_sys "
            "(srs_name,srs_id,organization,organization_coordsys_id,"
            "definition) VALUES ('%q', %d, upper('%q'), %d, '%q')",
            GetSrsName(poSRS), nSRSId, "NONE", nSRSId, pszWKT );
    }

    // Add new row to gpkg_spatial_ref_sys.
    CPL_IGNORE_RET_VAL( SQLCommand(hDB, pszSQL) );

    // Free everything that was allocated.
    CPLFree(pszWKT);
    sqlite3_free(pszSQL);
    delete poSRS;

    return nSRSId;
}

/************************************************************************/
/*                        GDALGeoPackageDataset()                       */
/************************************************************************/

GDALGeoPackageDataset::GDALGeoPackageDataset() :
    m_papoLayers(NULL),
    m_nLayers(0),
    m_bUtf8(false),
    m_bIdentifierAsCO(false),
    m_bDescriptionAsCO(false),
    m_bHasReadMetadataFromStorage(false),
    m_bMetadataDirty(false),
    m_papszSubDatasets(NULL),
    m_pszProjection(NULL),
    m_bRecordInsertedInGPKGContent(false),
    m_bGeoTransformValid(false),
    m_nSRID(-1),  // Unknown cartesian.
    m_dfTMSMinX(0.0),
    m_dfTMSMaxY(0.0),
    m_nOverviewCount(0),
    m_papoOverviewDS(NULL),
    m_bZoomOther(false),
    m_bInFlushCache(false),
    m_osTilingScheme("CUSTOM")
{
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                       ~GDALGeoPackageDataset()                       */
/************************************************************************/

GDALGeoPackageDataset::~GDALGeoPackageDataset()
{
    SetPamFlags(0);

    if( m_poParentDS == NULL && !m_osRasterTable.empty() &&
        !m_bGeoTransformValid )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Raster table %s not correctly initialized due to missing "
                  "call to SetGeoTransform()",
                  m_osRasterTable.c_str());
    }

    FlushCache();
    FlushMetadata();

    // Destroy overviews before cleaning m_hTempDB as they could still
    // need it
    for( int i = 0; i < m_nOverviewCount; i++ )
        delete m_papoOverviewDS[i];

    if( m_poParentDS != NULL )
    {
        hDB = NULL;
    }

    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];

    CPLFree( m_papoLayers );
    CPLFree( m_papoOverviewDS );
    CSLDestroy( m_papszSubDatasets );
    CPLFree(m_pszProjection);
}

/************************************************************************/
/*                         ICanIWriteBlock()                            */
/************************************************************************/

bool GDALGeoPackageDataset::ICanIWriteBlock()
{
    if( !bUpdate )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported on dataset opened in read-only mode");
        return false;
    }

    if( !m_bGeoTransformValid || m_nSRID == UNKNOWN_SRID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported if georeferencing not set");
        return false;
    }
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int GDALGeoPackageDataset::Open( GDALOpenInfo* poOpenInfo )
{
    CPLAssert( m_nLayers == 0 );
    CPLAssert( hDB == NULL );

    SetDescription( poOpenInfo->pszFilename );
    CPLString osFilename( poOpenInfo->pszFilename );
    CPLString osSubdatasetTableName;
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GPKG:") )
    {
        char** papszTokens = CSLTokenizeString2(poOpenInfo->pszFilename, ":", 0);
        if( CSLCount(papszTokens) != 3 )
        {
            CSLDestroy(papszTokens);
            return FALSE;
        }

        osFilename = papszTokens[1];
        osSubdatasetTableName = papszTokens[2];

        CSLDestroy(papszTokens);
    }

    bUpdate = poOpenInfo->eAccess == GA_Update;
    eAccess = poOpenInfo->eAccess; /* hum annoying duplication */
    m_pszFilename = CPLStrdup( osFilename );

    /* See if we can open the SQLite database */
    if( !OpenOrCreateDB(bUpdate
                        ? SQLITE_OPEN_READWRITE
                        : SQLITE_OPEN_READONLY) )
        return FALSE;

    /* Requirement 6: The SQLite PRAGMA integrity_check SQL command SHALL return “ok” */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    /* Disable integrity check by default, since it is expensive on big files */
    if( CPLTestBool(CPLGetConfigOption("OGR_GPKG_INTEGRITY_CHECK", "NO")) &&
        OGRERR_NONE != PragmaCheck("integrity_check", "ok", 1) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pragma integrity_check on '%s' failed",
                  m_pszFilename);
        return FALSE;
    }

    /* Requirement 7: The SQLite PRAGMA foreign_key_check() SQL with no */
    /* parameter value SHALL return an empty result set */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    if ( CPLTestBool(CPLGetConfigOption("OGR_GPKG_FOREIGN_KEY_CHECK", "YES")) &&
         OGRERR_NONE != PragmaCheck("foreign_key_check", "", 0) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "pragma foreign_key_check on '%s' failed. You can disable "
                  "this check by setting the OGR_GPKG_FOREIGN_KEY_CHECK "
                  "configuration option to NO",
                  m_pszFilename);
        return FALSE;
    }

    /* OGR UTF-8 capability, we'll advertise UTF-8 support if we have it */
    if ( OGRERR_NONE == PragmaCheck("encoding", "UTF-8", 1) )
    {
        m_bUtf8 = true;
    }
    else
    {
        m_bUtf8 = false;
    }

    /* Check for requirement metadata tables */
    /* Requirement 10: gpkg_spatial_ref_sys must exist */
    /* Requirement 13: gpkg_contents must exist */
    static std::string aosGpkgTables[] = {
        "gpkg_spatial_ref_sys",
        "gpkg_contents"
    };

    for ( int i = 0;
          i < (int)(sizeof(aosGpkgTables) / sizeof(aosGpkgTables[0]));
          i++ )
    {
        SQLResult oResult;
        char *pszSQL = sqlite3_mprintf("pragma table_info('%q')", aosGpkgTables[i].c_str());
        const OGRErr err = SQLQuery(hDB, pszSQL, &oResult);
        sqlite3_free(pszSQL);

        if  ( err != OGRERR_NONE )
            return FALSE;

        if ( oResult.nRowCount <= 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "required GeoPackage table '%s' is missing", aosGpkgTables[i].c_str());
            SQLResultFree(&oResult);
            return FALSE;
        }

        SQLResultFree(&oResult);
    }

    CheckUnknownExtensions();

    int bRet = FALSE;
    bool bHasGPKGGeometryColumns = false;
    if( poOpenInfo->nOpenFlags & GDAL_OF_VECTOR )
    {
        SQLResult oResult;
        const OGRErr err =
            SQLQuery(hDB, "pragma table_info('gpkg_geometry_columns')", &oResult);
        bHasGPKGGeometryColumns = err == OGRERR_NONE && oResult.nRowCount > 0;
        SQLResultFree(&oResult);
    }
    if( bHasGPKGGeometryColumns )
    {
        /* Load layer definitions for all tables in gpkg_contents & gpkg_geometry_columns */
        /* and non-spatial tables as well */
        std::string osSQL =
            "SELECT c.table_name, c.identifier, 1 as is_spatial, c.min_x, c.min_y, c.max_x, c.max_y, 1 AS is_gpkg_table "
            "  FROM gpkg_geometry_columns g JOIN gpkg_contents c ON (g.table_name = c.table_name)"
            "  WHERE c.data_type = 'features' ";

        if (HasGDALAspatialExtension()) {
            osSQL +=
                "UNION ALL "
                "SELECT table_name, identifier, 0 as is_spatial, 0 AS xmin, 0 AS ymin, 0 AS xmax, 0 AS ymax, 1 AS is_gpkg_table "
                "  FROM gpkg_contents"
                "  WHERE data_type = 'aspatial' ";
        }

        if( CPLTestBool(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "LIST_ALL_TABLES", "YES")) )
        {
            // vgpkg_ is Spatialite virtual table
            osSQL += "UNION ALL "
                    "SELECT name, name, 0 as is_spatial, 0 AS xmin, 0 AS ymin, 0 AS xmax, 0 AS ymax, 0 AS is_gpkg_table "
                    "FROM sqlite_master WHERE type IN ('table', 'view') AND name NOT LIKE 'gpkg_%' "
                    "AND name NOT LIKE 'vgpkg_%' "
                    "AND name NOT LIKE 'rtree_%' AND name NOT LIKE 'sqlite_%' "
                    "AND name NOT IN (SELECT table_name FROM gpkg_contents)";
        }

        SQLResult oResult;
        OGRErr err = SQLQuery(hDB, osSQL.c_str(), &oResult);
        if  ( err != OGRERR_NONE )
        {
            SQLResultFree(&oResult);
            return FALSE;
        }

        if ( oResult.nRowCount > 0 )
        {
            m_papoLayers = (OGRGeoPackageTableLayer**)CPLMalloc(sizeof(OGRGeoPackageTableLayer*) * oResult.nRowCount);

            for ( int i = 0; i < oResult.nRowCount; i++ )
            {
                const char *pszTableName = SQLResultGetValue(&oResult, 0, i);
                if ( ! pszTableName )
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "unable to read table name for layer(%d)", i);
                    continue;
                }
                bool bIsSpatial = CPL_TO_BOOL(SQLResultGetValueAsInteger(&oResult, 2, i));
                bool bIsGpkgTable = CPL_TO_BOOL(SQLResultGetValueAsInteger(&oResult, 7, i));
                OGRGeoPackageTableLayer *poLayer = new OGRGeoPackageTableLayer(this, pszTableName);
                if( OGRERR_NONE != poLayer->ReadTableDefinition(bIsSpatial, bIsGpkgTable) )
                {
                    delete poLayer;
                    CPLError(CE_Warning, CPLE_AppDefined, "unable to read table definition for '%s'", pszTableName);
                    continue;
                }
                m_papoLayers[m_nLayers++] = poLayer;
            }
        }

        SQLResultFree(&oResult);
        bRet = TRUE;
    }

    bool bHasTileMatrixSet = false;
    if( poOpenInfo->nOpenFlags & GDAL_OF_RASTER )
    {
        SQLResult oResult;
        const OGRErr err =
            SQLQuery(hDB,
                     "pragma table_info('gpkg_tile_matrix_set')", &oResult);
        bHasTileMatrixSet = err == OGRERR_NONE && oResult.nRowCount > 0;
        SQLResultFree(&oResult);
    }
    if( bHasTileMatrixSet )
    {
        SQLResult oResult;
        std::string osSQL =
            "SELECT c.table_name, c.identifier, c.description, c.srs_id, c.min_x, c.min_y, c.max_x, c.max_y, "
            "tms.min_x, tms.min_y, tms.max_x, tms.max_y FROM gpkg_contents c JOIN gpkg_tile_matrix_set tms ON "
            "c.table_name = tms.table_name WHERE data_type = 'tiles'";
        if( CSLFetchNameValue( poOpenInfo->papszOpenOptions, "TABLE") )
            osSubdatasetTableName = CSLFetchNameValue( poOpenInfo->papszOpenOptions, "TABLE");
        if( !osSubdatasetTableName.empty() )
        {
            char* pszTmp = sqlite3_mprintf(" AND c.table_name='%q'", osSubdatasetTableName.c_str());
            osSQL += pszTmp;
            sqlite3_free(pszTmp);
            SetPhysicalFilename( osFilename.c_str() );
        }

        const OGRErr err = SQLQuery(hDB, osSQL.c_str(), &oResult);
        if  ( err != OGRERR_NONE )
        {
            SQLResultFree(&oResult);
            return FALSE;
        }

        if( oResult.nRowCount == 0 && !osSubdatasetTableName.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find table '%s' in GeoPackage dataset",
                     osSubdatasetTableName.c_str());
        }
        else if( oResult.nRowCount == 1 )
        {
            const char *pszTableName = SQLResultGetValue(&oResult, 0, 0);
            const char* pszIdentifier = SQLResultGetValue(&oResult, 1, 0);
            const char* pszDescription = SQLResultGetValue(&oResult, 2, 0);
            const char* pszSRSId = SQLResultGetValue(&oResult, 3, 0);
            const char* pszMinX = SQLResultGetValue(&oResult, 4, 0);
            const char* pszMinY = SQLResultGetValue(&oResult, 5, 0);
            const char* pszMaxX = SQLResultGetValue(&oResult, 6, 0);
            const char* pszMaxY = SQLResultGetValue(&oResult, 7, 0);
            const char* pszTMSMinX = SQLResultGetValue(&oResult, 8, 0);
            const char* pszTMSMinY = SQLResultGetValue(&oResult, 9, 0);
            const char* pszTMSMaxX = SQLResultGetValue(&oResult, 10, 0);
            const char* pszTMSMaxY = SQLResultGetValue(&oResult, 11, 0);
            if( pszTableName != NULL && pszTMSMinX != NULL && pszTMSMinY != NULL &&
                pszTMSMaxX != NULL && pszTMSMaxY != NULL )
            {
                bRet = OpenRaster( pszTableName, pszIdentifier, pszDescription,
                                   pszSRSId ? atoi(pszSRSId) : 0,
                                   CPLAtof(pszTMSMinX), CPLAtof(pszTMSMinY),
                                   CPLAtof(pszTMSMaxX), CPLAtof(pszTMSMaxY),
                                   pszMinX, pszMinY, pszMaxX, pszMaxY,
                                   poOpenInfo->papszOpenOptions );
            }
        }
        else if( oResult.nRowCount >= 1 )
        {
            bRet = TRUE;

            int nSDSCount = 0;
            for ( int i = 0; i < oResult.nRowCount; i++ )
            {
                const char *pszTableName = SQLResultGetValue(&oResult, 0, i);
                const char *pszIdentifier = SQLResultGetValue(&oResult, 1, i);
                if( pszTableName != NULL )
                {
                    m_papszSubDatasets = CSLSetNameValue( m_papszSubDatasets,
                                                        CPLSPrintf("SUBDATASET_%d_NAME", nSDSCount+1),
                                                        CPLSPrintf("GPKG:%s:%s", m_pszFilename, pszTableName) );
                    if( pszIdentifier )
                        m_papszSubDatasets = CSLSetNameValue( m_papszSubDatasets,
                                                              CPLSPrintf("SUBDATASET_%d_DESC", nSDSCount+1),
                                                              CPLSPrintf("%s - %s", pszTableName, pszIdentifier) );
                    else
                        m_papszSubDatasets = CSLSetNameValue( m_papszSubDatasets,
                                                              CPLSPrintf("SUBDATASET_%d_DESC", nSDSCount+1),
                                                              pszTableName );
                }
                nSDSCount ++;
            }
        }

        SQLResultFree(&oResult);
    }

    return bRet;
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

bool GDALGeoPackageDataset::InitRaster( GDALGeoPackageDataset* poParentDS,
                                        const char* pszTableName,
                                        double dfMinX,
                                        double dfMinY,
                                        double dfMaxX,
                                        double dfMaxY,
                                        const char* pszContentsMinX,
                                        const char* pszContentsMinY,
                                        const char* pszContentsMaxX,
                                        const char* pszContentsMaxY,
                                        char** papszOpenOptionsIn,
                                        const SQLResult& oResult,
                                        int nIdxInResult )
{
    m_osRasterTable = pszTableName;
    m_dfTMSMinX = dfMinX;
    m_dfTMSMaxY = dfMaxY;

    int nZoomLevel = atoi(SQLResultGetValue(&oResult, 0, nIdxInResult));
    double dfPixelXSize = CPLAtof(SQLResultGetValue(&oResult, 1, nIdxInResult));
    double dfPixelYSize = CPLAtof(SQLResultGetValue(&oResult, 2, nIdxInResult));
    int nTileWidth = atoi(SQLResultGetValue(&oResult, 3, nIdxInResult));
    int nTileHeight = atoi(SQLResultGetValue(&oResult, 4, nIdxInResult));
    int nTileMatrixWidth = atoi(SQLResultGetValue(&oResult, 5, nIdxInResult));
    int nTileMatrixHeight = atoi(SQLResultGetValue(&oResult, 6, nIdxInResult));

    /* Use content bounds in priority over tile_matrix_set bounds */
    double dfGDALMinX = dfMinX;
    double dfGDALMinY = dfMinY;
    double dfGDALMaxX = dfMaxX;
    double dfGDALMaxY = dfMaxY;
    pszContentsMinX = CSLFetchNameValueDef(papszOpenOptionsIn, "MINX", pszContentsMinX);
    pszContentsMinY = CSLFetchNameValueDef(papszOpenOptionsIn, "MINY", pszContentsMinY);
    pszContentsMaxX = CSLFetchNameValueDef(papszOpenOptionsIn, "MAXX", pszContentsMaxX);
    pszContentsMaxY = CSLFetchNameValueDef(papszOpenOptionsIn, "MAXY", pszContentsMaxY);
    if( pszContentsMinX != NULL && pszContentsMinY != NULL &&
        pszContentsMaxX != NULL && pszContentsMaxY != NULL )
    {
        dfGDALMinX = CPLAtof(pszContentsMinX);
        dfGDALMinY = CPLAtof(pszContentsMinY);
        dfGDALMaxX = CPLAtof(pszContentsMaxX);
        dfGDALMaxY = CPLAtof(pszContentsMaxY);
    }
    if( dfGDALMinX >= dfGDALMaxX || dfGDALMinY >= dfGDALMaxY )
    {
        return false;
    }

    int nBandCount = atoi(CSLFetchNameValueDef(papszOpenOptionsIn, "BAND_COUNT", "4"));
    if( nBandCount != 1 && nBandCount != 2 && nBandCount != 3 && nBandCount != 4 )
        nBandCount = 4;

    return InitRaster(poParentDS, pszTableName, nZoomLevel, nBandCount, dfMinX, dfMaxY,
                      dfPixelXSize, dfPixelYSize, nTileWidth, nTileHeight,
                      nTileMatrixWidth, nTileMatrixHeight,
                      dfGDALMinX, dfGDALMinY, dfGDALMaxX, dfGDALMaxY );
}

/************************************************************************/
/*                      ComputeTileAndPixelShifts()                     */
/************************************************************************/

void GDALGeoPackageDataset::ComputeTileAndPixelShifts()
{
    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);

    // Compute shift between GDAL origin and TileMatrixSet origin
    int nShiftXPixels = (int)floor(0.5 + (m_adfGeoTransform[0] - m_dfTMSMinX) /  m_adfGeoTransform[1]);
    m_nShiftXTiles = (int)floor(1.0 * nShiftXPixels / nTileWidth);
    m_nShiftXPixelsMod = ((nShiftXPixels % nTileWidth) + nTileWidth) % nTileWidth;
    int nShiftYPixels = (int)floor(0.5 + (m_adfGeoTransform[3] - m_dfTMSMaxY) /  m_adfGeoTransform[5]);
    m_nShiftYTiles = (int)floor(1.0 * nShiftYPixels / nTileHeight);
    m_nShiftYPixelsMod = ((nShiftYPixels % nTileHeight) + nTileHeight) % nTileHeight;
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

bool GDALGeoPackageDataset::InitRaster( GDALGeoPackageDataset* poParentDS,
                                        const char* pszTableName,
                                        int nZoomLevel,
                                        int nBandCount,
                                        double dfTMSMinX,
                                        double dfTMSMaxY,
                                        double dfPixelXSize,
                                        double dfPixelYSize,
                                        int nTileWidth,
                                        int nTileHeight,
                                        int nTileMatrixWidth,
                                        int nTileMatrixHeight,
                                        double dfGDALMinX,
                                        double dfGDALMinY,
                                        double dfGDALMaxX,
                                        double dfGDALMaxY )
{
    m_osRasterTable = pszTableName;
    m_dfTMSMinX = dfTMSMinX;
    m_dfTMSMaxY = dfTMSMaxY;
    m_nZoomLevel = nZoomLevel;
    m_nTileMatrixWidth = nTileMatrixWidth;
    m_nTileMatrixHeight = nTileMatrixHeight;

    m_bGeoTransformValid = true;
    m_adfGeoTransform[0] = dfGDALMinX;
    m_adfGeoTransform[1] = dfPixelXSize;
    m_adfGeoTransform[3] = dfGDALMaxY;
    m_adfGeoTransform[5] = -dfPixelYSize;
    double dfRasterXSize = 0.5 + (dfGDALMaxX - dfGDALMinX) / dfPixelXSize;
    double dfRasterYSize = 0.5 + (dfGDALMaxY - dfGDALMinY) / dfPixelYSize;
    if( dfRasterXSize > INT_MAX || dfRasterYSize > INT_MAX )
        return false;
    nRasterXSize = (int)dfRasterXSize;
    nRasterYSize = (int)dfRasterYSize;

    m_pabyCachedTiles = (GByte*) VSI_MALLOC3_VERBOSE(4 * 4, nTileWidth, nTileHeight);
    if( m_pabyCachedTiles == NULL )
    {
        return false;
    }

    for(int i = 1; i <= nBandCount; i ++)
        SetBand( i, new GDALGeoPackageRasterBand(this, nTileWidth, nTileHeight) );

    ComputeTileAndPixelShifts();

    GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    GDALPamDataset::SetMetadataItem("ZOOM_LEVEL", CPLSPrintf("%d", m_nZoomLevel));

    if( poParentDS )
    {
        m_poParentDS = poParentDS;
        bUpdate = poParentDS->bUpdate;
        eAccess = poParentDS->eAccess;
        hDB = poParentDS->hDB;
        m_eTF = poParentDS->m_eTF;
        m_nQuality = poParentDS->m_nQuality;
        m_nZLevel = poParentDS->m_nZLevel;
        m_bDither = poParentDS->m_bDither;
        /*m_nSRID = poParentDS->m_nSRID;*/
        m_osWHERE = poParentDS->m_osWHERE;
        SetDescription(CPLSPrintf("%s - zoom_level=%d",
                                  poParentDS->GetDescription(), m_nZoomLevel));
    }

    return true;
}

/************************************************************************/
/*                 GDALGPKGMBTilesGetTileFormat()                       */
/************************************************************************/

GPKGTileFormat GDALGPKGMBTilesGetTileFormat(const char* pszTF )
{
    GPKGTileFormat eTF = GPKG_TF_PNG_JPEG;
    if( pszTF )
    {
        if( EQUAL(pszTF, "PNG_JPEG") )
            eTF = GPKG_TF_PNG_JPEG;
        else if( EQUAL(pszTF, "PNG") )
            eTF = GPKG_TF_PNG;
        else if( EQUAL(pszTF, "PNG8") )
            eTF = GPKG_TF_PNG8;
        else if( EQUAL(pszTF, "JPEG") )
            eTF = GPKG_TF_JPEG;
        else if( EQUAL(pszTF, "WEBP") )
            eTF = GPKG_TF_WEBP;
    }
    return eTF;
}

/************************************************************************/
/*                         OpenRaster()                                 */
/************************************************************************/

bool GDALGeoPackageDataset::OpenRaster( const char* pszTableName,
                                        const char* pszIdentifier,
                                        const char* pszDescription,
                                        int nSRSId,
                                        double dfMinX,
                                        double dfMinY,
                                        double dfMaxX,
                                        double dfMaxY,
                                        const char* pszContentsMinX,
                                        const char* pszContentsMinY,
                                        const char* pszContentsMaxX,
                                        const char* pszContentsMaxY,
                                        char** papszOpenOptionsIn )
{
    OGRErr err;
    SQLResult oResult;

    if( dfMinX >= dfMaxX || dfMinY >= dfMaxY )
        return false;

    m_bRecordInsertedInGPKGContent = true;
    m_nSRID = nSRSId;
    if( nSRSId > 0 )
    {
        OGRSpatialReference* poSRS = GetSpatialRef( nSRSId );
        if( poSRS )
        {
            poSRS->exportToWkt(&m_pszProjection);
            delete poSRS;
        }
    }

    /* The NOT NULL are just in case the tables would have been built without */
    /* the mandatory constraints */
    char* pszQuotedTableName = sqlite3_mprintf("'%q'", pszTableName);
    CPLString osQuotedTableName(pszQuotedTableName);
    sqlite3_free(pszQuotedTableName);
    char* pszSQL = sqlite3_mprintf(
            "SELECT zoom_level, pixel_x_size, pixel_y_size, tile_width, tile_height, matrix_width, matrix_height FROM gpkg_tile_matrix tm "
            "WHERE table_name = %s AND pixel_x_size > 0 "
            "AND pixel_y_size > 0 AND tile_width > 0 AND tile_height > 0 AND matrix_width > 0 AND matrix_height > 0",
            osQuotedTableName.c_str());
    CPLString osSQL(pszSQL);
    const char* pszZoomLevel =  CSLFetchNameValue(papszOpenOptionsIn, "ZOOM_LEVEL");
    if( pszZoomLevel )
    {
        if( bUpdate )
            osSQL += CPLSPrintf(" AND zoom_level <= %d", atoi(pszZoomLevel));
        else
        {
            osSQL += CPLSPrintf(" AND (zoom_level = %d OR (zoom_level < %d AND EXISTS(SELECT 1 FROM %s WHERE zoom_level = tm.zoom_level LIMIT 1)))",
                                atoi(pszZoomLevel), atoi(pszZoomLevel), osQuotedTableName.c_str());
        }
    }
    // In read-only mode, only lists non empty zoom levels
    else if( !bUpdate )
    {
        osSQL += CPLSPrintf(" AND EXISTS(SELECT 1 FROM %s WHERE zoom_level = tm.zoom_level LIMIT 1)",
                            osQuotedTableName.c_str());
    }
    else if( pszZoomLevel == NULL )
    {
        osSQL += CPLSPrintf(" AND zoom_level <= (SELECT MAX(zoom_level) FROM %s)",
                            osQuotedTableName.c_str());
    }
    osSQL += " ORDER BY zoom_level DESC";

    err = SQLQuery(hDB, osSQL.c_str(), &oResult);
    if( err != OGRERR_NONE || oResult.nRowCount == 0 )
    {
        if( err == OGRERR_NONE && oResult.nRowCount == 0 &&
            pszContentsMinX != NULL && pszContentsMinY != NULL &&
            pszContentsMaxX != NULL && pszContentsMaxY != NULL )
        {
            SQLResultFree(&oResult);
            osSQL = pszSQL;
            osSQL += " ORDER BY zoom_level DESC";
            if( !bUpdate )
                osSQL += " LIMIT 1";
            err = SQLQuery(hDB, osSQL.c_str(), &oResult);
        }
        if( err != OGRERR_NONE || oResult.nRowCount == 0 )
        {
            SQLResultFree(&oResult);
            sqlite3_free(pszSQL);
            return false;
        }
    }
    sqlite3_free(pszSQL);

    // If USE_TILE_EXTENT=YES, then query the tile table to find which tiles
    // actually exist.
    CPLString osContentsMinX, osContentsMinY, osContentsMaxX, osContentsMaxY;
    if( CPLTestBool(CSLFetchNameValueDef(papszOpenOptionsIn, "USE_TILE_EXTENT", "NO")) )
    {
        pszSQL = sqlite3_mprintf(
            "SELECT MIN(tile_column), MIN(tile_row), MAX(tile_column), MAX(tile_row) FROM \"%w\" WHERE zoom_level = %d",
            pszTableName, atoi(SQLResultGetValue(&oResult, 0, 0)));
        SQLResult oResult2;
        err = SQLQuery(hDB, pszSQL, &oResult2);
        sqlite3_free(pszSQL);
        if  ( err != OGRERR_NONE || oResult2.nRowCount == 0 )
        {
            SQLResultFree(&oResult);
            SQLResultFree(&oResult2);
            return false;
        }
        const double dfPixelXSize = CPLAtof(SQLResultGetValue(&oResult, 1, 0));
        const double dfPixelYSize = CPLAtof(SQLResultGetValue(&oResult, 2, 0));
        const int nTileWidth = atoi(SQLResultGetValue(&oResult, 3, 0));
        const int nTileHeight = atoi(SQLResultGetValue(&oResult, 4, 0));
        osContentsMinX = CPLSPrintf("%.18g", dfMinX + dfPixelXSize * nTileWidth * atoi(SQLResultGetValue(&oResult2, 0, 0)));
        osContentsMaxY = CPLSPrintf("%.18g", dfMaxY - dfPixelYSize * nTileHeight * atoi(SQLResultGetValue(&oResult2, 1, 0)));
        osContentsMaxX = CPLSPrintf("%.18g", dfMinX + dfPixelXSize * nTileWidth * (1 + atoi(SQLResultGetValue(&oResult2, 2, 0))));
        osContentsMinY = CPLSPrintf("%.18g", dfMaxY - dfPixelYSize * nTileHeight * (1 + atoi(SQLResultGetValue(&oResult2, 3, 0))));
        pszContentsMinX = osContentsMinX.c_str();
        pszContentsMinY = osContentsMinY.c_str();
        pszContentsMaxX = osContentsMaxX.c_str();
        pszContentsMaxY = osContentsMaxY.c_str();
        SQLResultFree(&oResult2);
    }

    if( !InitRaster(
            NULL, pszTableName, dfMinX, dfMinY, dfMaxX, dfMaxY,
            pszContentsMinX, pszContentsMinY, pszContentsMaxX, pszContentsMaxY,
            papszOpenOptionsIn, oResult, 0) )
    {
        SQLResultFree(&oResult);
        return false;
    }

    CheckUnknownExtensions(true);

    // Do this after CheckUnknownExtensions() so that m_eTF is set to GPKG_TF_WEBP
    // if the table already registers the gpkg_webp extension
    const char* pszTF = CSLFetchNameValue(papszOpenOptionsIn, "TILE_FORMAT");
    if( pszTF )
    {
        if( !bUpdate )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "DRIVER open option ignored in read-only mode");
        }
        else
        {
            GPKGTileFormat eTF = GDALGPKGMBTilesGetTileFormat(pszTF);
            if( eTF == GPKG_TF_WEBP && m_eTF != eTF )
            {
                if( !RegisterWebPExtension() )
                    return false;
            }
            m_eTF = eTF;
        }
    }

    ParseCompressionOptions(papszOpenOptionsIn);

    m_osWHERE = CSLFetchNameValueDef(papszOpenOptionsIn, "WHERE", "");

    // Set metadata
    if( pszIdentifier && pszIdentifier[0] )
        GDALPamDataset::SetMetadataItem("IDENTIFIER", pszIdentifier);
    if( pszDescription && pszDescription[0] )
        GDALPamDataset::SetMetadataItem("DESCRIPTION", pszDescription);

    // Add overviews
    for( int i = 1; i < oResult.nRowCount; i++ )
    {
        GDALGeoPackageDataset* poOvrDS = new GDALGeoPackageDataset();
        poOvrDS->InitRaster(
            this, pszTableName, dfMinX, dfMinY, dfMaxX, dfMaxY,
            pszContentsMinX, pszContentsMinY, pszContentsMaxX, pszContentsMaxY,
            papszOpenOptionsIn, oResult, i);

        m_papoOverviewDS = (GDALGeoPackageDataset**) CPLRealloc(m_papoOverviewDS,
                        sizeof(GDALGeoPackageDataset*) * (m_nOverviewCount+1));
        m_papoOverviewDS[m_nOverviewCount ++] = poOvrDS;

        int nTileWidth, nTileHeight;
        poOvrDS->GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
        if( poOvrDS->GetRasterXSize() < nTileWidth &&
            poOvrDS->GetRasterYSize() < nTileHeight )
        {
            break;
        }
    }

    SQLResultFree(&oResult);

    return true;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* GDALGeoPackageDataset::GetProjectionRef()
{
    return (m_pszProjection) ? m_pszProjection : "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetProjection( const char* pszProjection )
{
    if( nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on read-only dataset");
        return CE_Failure;
    }

    int nSRID = -1;
    if( pszProjection == NULL || pszProjection[0] == '\0' )
    {
      // nSRID = -1;
    }
    else
    {
        OGRSpatialReference oSRS;
        if( oSRS.SetFromUserInput(pszProjection) != OGRERR_NONE )
            return CE_Failure;
        nSRID = GetSrsId( &oSRS );
    }

    for(size_t iScheme = 0;
               iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
               iScheme++ )
    {
        if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            if( nSRID != asTilingShemes[iScheme].nEPSGCode )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Projection should be EPSG:%d for %s tiling scheme",
                         asTilingShemes[iScheme].nEPSGCode,
                         m_osTilingScheme.c_str());
                return CE_Failure;
            }
        }
    }

    m_nSRID = nSRID;
    CPLFree(m_pszProjection);
    m_pszProjection = pszProjection ? CPLStrdup(pszProjection) : CPLStrdup("");

    if( m_bRecordInsertedInGPKGContent )
    {
        char* pszSQL = sqlite3_mprintf("UPDATE gpkg_contents SET srs_id = %d WHERE table_name = '%q'",
                                        m_nSRID, m_osRasterTable.c_str());
        OGRErr eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if ( eErr != OGRERR_NONE )
            return CE_Failure;

        pszSQL = sqlite3_mprintf("UPDATE gpkg_tile_matrix_set SET srs_id = %d WHERE table_name = '%q'",
                                 m_nSRID, m_osRasterTable.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if ( eErr != OGRERR_NONE )
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::GetGeoTransform( double* padfGeoTransform )
{
    memcpy(padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double));
    if( !m_bGeoTransformValid )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetGeoTransform( double* padfGeoTransform )
{
    if( nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGeoTransform() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGeoTransform() not supported on read-only dataset");
        return CE_Failure;
    }
    if( m_bGeoTransformValid )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot modify geotransform once set");
        return CE_Failure;
    }
    if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0 ||
        padfGeoTransform[5] > 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up non rotated geotransform supported");
        return CE_Failure;
    }

    for(size_t iScheme = 0;
               iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
               iScheme++ )
    {
        if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            double dfPixelXSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0;
            double dfPixelYSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelYSizeZoomLevel0;
            for( m_nZoomLevel = 0; m_nZoomLevel < 25; m_nZoomLevel++ )
            {
                double dfExpectedPixelXSize = dfPixelXSizeZoomLevel0 / (1 << m_nZoomLevel);
                double dfExpectedPixelYSize = dfPixelYSizeZoomLevel0 / (1 << m_nZoomLevel);
                if( fabs( padfGeoTransform[1] - dfExpectedPixelXSize ) < 1e-8 * dfExpectedPixelXSize &&
                    fabs( fabs(padfGeoTransform[5]) - dfExpectedPixelYSize ) < 1e-8 * dfExpectedPixelYSize )
                {
                    break;
                }
            }
            if( m_nZoomLevel == 25 )
            {
                m_nZoomLevel = -1;
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Could not find an appropriate zoom level of %s tiling scheme that matches raster pixel size",
                         m_osTilingScheme.c_str());
                return CE_Failure;
            }
            break;
        }
    }

    memcpy(m_adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
    m_bGeoTransformValid = true;

    return FinalizeRasterRegistration();
}

/************************************************************************/
/*                      FinalizeRasterRegistration()                    */
/************************************************************************/

CPLErr GDALGeoPackageDataset::FinalizeRasterRegistration()
{
    OGRErr eErr;

    m_dfTMSMinX = m_adfGeoTransform[0];
    m_dfTMSMaxY = m_adfGeoTransform[3];

    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
    m_nTileMatrixWidth = (nRasterXSize + nTileWidth - 1) / nTileWidth;
    m_nTileMatrixHeight = (nRasterYSize + nTileHeight - 1) / nTileHeight;

    if( m_nZoomLevel < 0 )
    {
        m_nZoomLevel = 0;
        while( (nRasterXSize >> m_nZoomLevel) > nTileWidth ||
            (nRasterYSize >> m_nZoomLevel) > nTileHeight )
            m_nZoomLevel ++;
    }

    double dfPixelXSizeZoomLevel0 = m_adfGeoTransform[1] * (1 << m_nZoomLevel);
    double dfPixelYSizeZoomLevel0 = fabs(m_adfGeoTransform[5]) * (1 << m_nZoomLevel);
    int nTileXCountZoomLevel0 = ((nRasterXSize >> m_nZoomLevel) + nTileWidth - 1) / nTileWidth;
    int nTileYCountZoomLevel0 = ((nRasterYSize >> m_nZoomLevel) + nTileHeight - 1) / nTileHeight;

    for(size_t iScheme = 0;
               iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
               iScheme++ )
    {
        if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            CPLAssert( m_nZoomLevel >= 0 );
            m_dfTMSMinX = asTilingShemes[iScheme].dfMinX;
            m_dfTMSMaxY = asTilingShemes[iScheme].dfMaxY;
            dfPixelXSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0;
            dfPixelYSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelYSizeZoomLevel0;
            nTileXCountZoomLevel0 = asTilingShemes[iScheme].nTileXCountZoomLevel0;
            nTileYCountZoomLevel0 = asTilingShemes[iScheme].nTileYCountZoomLevel0;
            m_nTileMatrixWidth = nTileXCountZoomLevel0 * (1 << m_nZoomLevel);
            m_nTileMatrixHeight = nTileYCountZoomLevel0 * (1 << m_nZoomLevel);
            break;
        }
    }

    ComputeTileAndPixelShifts();

    double dfGDALMinX = m_adfGeoTransform[0];
    double dfGDALMinY = m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
    double dfGDALMaxX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
    double dfGDALMaxY = m_adfGeoTransform[3];

    SoftStartTransaction();

    const char* pszCurrentDate = CPLGetConfigOption("OGR_CURRENT_DATE", NULL);
    CPLString osInsertGpkgContentsFormatting("INSERT INTO gpkg_contents "
            "(table_name,data_type,identifier,description,min_x,min_y,max_x,max_y,last_change,srs_id) VALUES "
            "('%q','tiles','%q','%q',%.18g,%.18g,%.18g,%.18g,");
    osInsertGpkgContentsFormatting += ( pszCurrentDate ) ? "'%q'" : "%s";
    osInsertGpkgContentsFormatting += ",%d)";
    char* pszSQL =
        sqlite3_mprintf(osInsertGpkgContentsFormatting.c_str(),
        m_osRasterTable.c_str(),
        m_osIdentifier.c_str(),
        m_osDescription.c_str(),
        dfGDALMinX, dfGDALMinY, dfGDALMaxX, dfGDALMaxY,
        pszCurrentDate ? pszCurrentDate : "strftime('%Y-%m-%dT%H:%M:%fZ','now')",
        m_nSRID);

    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( eErr != OGRERR_NONE )
        return CE_Failure;

    double dfTMSMaxX = m_dfTMSMinX + nTileXCountZoomLevel0 * nTileWidth * dfPixelXSizeZoomLevel0;
    double dfTMSMinY = m_dfTMSMaxY - nTileYCountZoomLevel0 * nTileHeight * dfPixelYSizeZoomLevel0;

    pszSQL = sqlite3_mprintf("INSERT INTO gpkg_tile_matrix_set "
            "(table_name,srs_id,min_x,min_y,max_x,max_y) VALUES "
            "('%q',%d,%.18g,%.18g,%.18g,%.18g)",
            m_osRasterTable.c_str(), m_nSRID,
            m_dfTMSMinX,dfTMSMinY,dfTMSMaxX,m_dfTMSMaxY);
    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( eErr != OGRERR_NONE )
        return CE_Failure;

    m_papoOverviewDS = (GDALGeoPackageDataset**) CPLCalloc(sizeof(GDALGeoPackageDataset*),
                                                           m_nZoomLevel);

    for( int i = 0; i <= m_nZoomLevel; i++ )
    {
        double dfPixelXSizeZoomLevel = 0.0;
        double dfPixelYSizeZoomLevel = 0.0;
        int nTileMatrixWidth = 0;
        int nTileMatrixHeight = 0;
        if( EQUAL(m_osTilingScheme, "CUSTOM") )
        {
            dfPixelXSizeZoomLevel = m_adfGeoTransform[1] * (1 << (m_nZoomLevel-i));
            dfPixelYSizeZoomLevel = fabs(m_adfGeoTransform[5]) * (1 << (m_nZoomLevel-i));
            nTileMatrixWidth = ((nRasterXSize >> (m_nZoomLevel-i)) + nTileWidth - 1) / nTileWidth;
            nTileMatrixHeight = ((nRasterYSize >> (m_nZoomLevel-i)) + nTileHeight - 1) / nTileHeight;
        }
        else
        {
            dfPixelXSizeZoomLevel = dfPixelXSizeZoomLevel0 / (1 << i);
            dfPixelYSizeZoomLevel = dfPixelYSizeZoomLevel0 / (1 << i);
            nTileMatrixWidth = nTileXCountZoomLevel0 * (1 << i);
            nTileMatrixHeight = nTileYCountZoomLevel0 * (1 << i);
        }
        pszSQL = sqlite3_mprintf("INSERT INTO gpkg_tile_matrix "
                "(table_name,zoom_level,matrix_width,matrix_height,tile_width,tile_height,pixel_x_size,pixel_y_size) VALUES "
                "('%q',%d,%d,%d,%d,%d,%.18g,%.18g)",
                m_osRasterTable.c_str(),i,nTileMatrixWidth,nTileMatrixHeight,
                nTileWidth,nTileHeight,dfPixelXSizeZoomLevel,dfPixelYSizeZoomLevel);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if ( eErr != OGRERR_NONE )
            return CE_Failure;

        if( i < m_nZoomLevel )
        {
            GDALGeoPackageDataset* poOvrDS = new GDALGeoPackageDataset();
            poOvrDS->InitRaster( this, m_osRasterTable, i, nBands,
                                 m_dfTMSMinX, m_dfTMSMaxY,
                                 dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel,
                                 nTileWidth, nTileHeight,
                                 nTileMatrixWidth,nTileMatrixHeight,
                                 dfGDALMinX, dfGDALMinY,
                                 dfGDALMaxX, dfGDALMaxY );

            m_papoOverviewDS[m_nZoomLevel-1-i] = poOvrDS;
        }
    }

    SoftCommitTransaction();

    m_nOverviewCount = m_nZoomLevel;
    m_bRecordInsertedInGPKGContent = true;

    return CE_None;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void GDALGeoPackageDataset::FlushCache()
{
    IFlushCacheWithErrCode();
}

CPLErr GDALGeoPackageDataset::IFlushCacheWithErrCode()

{
    if( m_bInFlushCache )
        return CE_None;
    m_bInFlushCache = true;
    // Short circuit GDALPamDataset to avoid serialization to .aux.xml
    GDALDataset::FlushCache();

    for( int i = 0; i < m_nLayers; i++ )
    {
        m_papoLayers[i]->RunDeferredCreationIfNecessary();
        m_papoLayers[i]->CreateSpatialIndexIfNecessary();
    }

    // Update raster table last_change column in gpkg_contents if needed
    if( m_bHasModifiedTiles )
    {
        const char* pszCurrentDate = CPLGetConfigOption("OGR_CURRENT_DATE", NULL);
        char *pszSQL = NULL ;

        if( pszCurrentDate )
        {
            pszSQL = sqlite3_mprintf(
                        "UPDATE gpkg_contents SET "
                        "last_change = '%q'"
                        "WHERE table_name = '%q' AND "
                        "Lower(data_type) = 'tiles'",
                        m_osRasterTable.c_str(), pszCurrentDate);
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                        "UPDATE gpkg_contents SET "
                        "last_change = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now')"
                        "WHERE table_name = '%q' AND "
                        "Lower(data_type) = 'tiles'",
                        m_osRasterTable.c_str());
        }

        CPL_IGNORE_RET_VAL(SQLCommand(hDB, pszSQL));
        sqlite3_free(pszSQL);

        m_bHasModifiedTiles = false;
    }

    CPLErr eErr = FlushTiles();

    m_bInFlushCache = false;
    return eErr;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

static int GetFloorPowerOfTwo(int n)
{
    int p2 = 1;
    while( (n = n >> 1) > 0 )
    {
        p2 <<= 1;
    }
    return p2;
}

CPLErr GDALGeoPackageDataset::IBuildOverviews(
                        const char * pszResampling,
                        int nOverviews, int * panOverviewList,
                        int nBandsIn, CPL_UNUSED int * panBandList,
                        GDALProgressFunc pfnProgress, void * pProgressData )
{
    if( GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on a database opened in read-only mode");
        return CE_Failure;
    }
    if( m_poParentDS != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on overview dataset");
        return CE_Failure;
    }

    if( nOverviews == 0 )
    {
        for(int i=0;i<m_nOverviewCount;i++)
            m_papoOverviewDS[i]->FlushCache();
        char* pszSQL = sqlite3_mprintf("DELETE FROM \"%w\" WHERE zoom_level < %d",
                                       m_osRasterTable.c_str(),
                                       m_nZoomLevel);
        char* pszErrMsg = NULL;
        int ret = sqlite3_exec(hDB, pszSQL, NULL, NULL, &pszErrMsg);
        sqlite3_free(pszSQL);
        if( ret != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failure: %s",
                     pszErrMsg ? pszErrMsg : "");
            sqlite3_free(pszErrMsg);
            return CE_Failure;
        }
        return CE_None;
    }

    if( nBandsIn != nBands )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in GPKG only"
                  "supported when operating on all bands." );
        return CE_Failure;
    }

    if( m_nOverviewCount == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Image too small to support overviews");
        return CE_Failure;
    }

    FlushCache();
    for(int i=0;i<nOverviews;i++)
    {
        if( panOverviewList[i] < 2 )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Overview factor must be >= 2");
            return CE_Failure;
        }

        bool bFound = false;
        int jCandidate = -1;
        int nMaxOvFactor = 0;
        for( int j = 0; j < m_nOverviewCount; j++ )
        {
            GDALDataset* poODS = m_papoOverviewDS[j];

            int nOvFactor = GDALComputeOvFactor(poODS->GetRasterXSize(),
                                                GetRasterXSize(),
                                                poODS->GetRasterYSize(),
                                                GetRasterYSize());
            if( nOvFactor > 64 &&
                std::abs(nOvFactor - GetFloorPowerOfTwo(nOvFactor)) <= 2 )
            {
                nOvFactor = GetFloorPowerOfTwo(nOvFactor);
            }
            nMaxOvFactor = nOvFactor;

            if( nOvFactor == panOverviewList[i]
                || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                    GetRasterXSize(),
                                                    GetRasterYSize() ) )
            {
                bFound = true;
                break;
            }

            if( jCandidate < 0 && nOvFactor > panOverviewList[i] )
                jCandidate = j;
        }

        if( !bFound )
        {
            /* Mostly for debug */
            if( !CPLTestBool(CPLGetConfigOption("ALLOW_GPKG_ZOOM_OTHER_EXTENSION", "YES")) )
            {
                CPLString osOvrList;
                for(int j=0;j<m_nOverviewCount;j++)
                {
                    GDALDataset* poODS = m_papoOverviewDS[j];

                    /* Compute overview factor */
                    int nOvFactor = (int)
                        (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());
                    int nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactor);
                    if( nODSXSize != poODS->GetRasterXSize() )
                    {
                        int nOvFactorPowerOfTwo = GetFloorPowerOfTwo(nOvFactor);
                        nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactorPowerOfTwo);
                        if( nODSXSize == poODS->GetRasterXSize() )
                            nOvFactor = nOvFactorPowerOfTwo;
                        else
                        {
                            nOvFactorPowerOfTwo <<= 1;
                            nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactorPowerOfTwo);
                            if( nODSXSize == poODS->GetRasterXSize() )
                                nOvFactor = nOvFactorPowerOfTwo;
                        }
                    }
                    if( j != 0 )
                        osOvrList += " ";
                    osOvrList += CPLSPrintf("%d", nOvFactor);
                }
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Only overviews %s can be computed", osOvrList.c_str());
                return CE_Failure;
            }
            else
            {
                int nOvFactor = panOverviewList[i];
                if( jCandidate < 0 )
                    jCandidate = m_nOverviewCount;

                int nOvXSize = GetRasterXSize() / nOvFactor;
                int nOvYSize = GetRasterYSize() / nOvFactor;
                if( nOvXSize < 8 || nOvYSize < 8)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Too big overview factor : %d. Would result in a %dx%d overview",
                             nOvFactor, nOvXSize, nOvYSize);
                    return CE_Failure;
                }
                if( !(jCandidate == m_nOverviewCount && nOvFactor == 2 * nMaxOvFactor) &&
                    !m_bZoomOther )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Use of overview factor %d causes gpkg_zoom_other extension to be needed",
                            nOvFactor);
                    RegisterZoomOtherExtension();
                    m_bZoomOther = true;
                }

                SoftStartTransaction();

                CPLAssert(jCandidate > 0);
                int nNewZoomLevel = m_papoOverviewDS[jCandidate-1]->m_nZoomLevel;

                char* pszSQL;
                OGRErr eErr;
                for(int k=0;k<=jCandidate;k++)
                {
                    pszSQL = sqlite3_mprintf("UPDATE gpkg_tile_matrix SET zoom_level = %d "
                        "WHERE table_name = '%q' AND zoom_level = %d",
                        m_nZoomLevel - k + 1,
                        m_osRasterTable.c_str(),
                        m_nZoomLevel - k);
                    eErr = SQLCommand(hDB, pszSQL);
                    sqlite3_free(pszSQL);
                    if ( eErr != OGRERR_NONE )
                    {
                        SoftRollbackTransaction();
                        return CE_Failure;
                    }

                    pszSQL = sqlite3_mprintf("UPDATE \"%w\" SET zoom_level = %d "
                        "WHERE zoom_level = %d",
                        m_osRasterTable.c_str(),
                        m_nZoomLevel - k + 1,
                        m_nZoomLevel - k);
                    eErr = SQLCommand(hDB, pszSQL);
                    sqlite3_free(pszSQL);
                    if ( eErr != OGRERR_NONE )
                    {
                        SoftRollbackTransaction();
                        return CE_Failure;
                    }
                }

                double dfGDALMinX = m_adfGeoTransform[0];
                double dfGDALMinY = m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
                double dfGDALMaxX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
                double dfGDALMaxY = m_adfGeoTransform[3];
                double dfPixelXSizeZoomLevel = m_adfGeoTransform[1] * nOvFactor;
                double dfPixelYSizeZoomLevel = fabs(m_adfGeoTransform[5]) * nOvFactor;
                int nTileWidth, nTileHeight;
                GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
                int nTileMatrixWidth = (nOvXSize + nTileWidth - 1) / nTileWidth;
                int nTileMatrixHeight = (nOvYSize + nTileHeight - 1) / nTileHeight;
                pszSQL = sqlite3_mprintf("INSERT INTO gpkg_tile_matrix "
                        "(table_name,zoom_level,matrix_width,matrix_height,tile_width,tile_height,pixel_x_size,pixel_y_size) VALUES "
                        "('%q',%d,%d,%d,%d,%d,%.18g,%.18g)",
                        m_osRasterTable.c_str(),nNewZoomLevel,nTileMatrixWidth,nTileMatrixHeight,
                        nTileWidth,nTileHeight,dfPixelXSizeZoomLevel,dfPixelYSizeZoomLevel);
                eErr = SQLCommand(hDB, pszSQL);
                sqlite3_free(pszSQL);
                if ( eErr != OGRERR_NONE )
                {
                    SoftRollbackTransaction();
                    return CE_Failure;
                }

                SoftCommitTransaction();

                m_nZoomLevel ++; /* this change our zoom level as well as previous overviews */
                for(int k=0;k<jCandidate;k++)
                    m_papoOverviewDS[k]->m_nZoomLevel ++;

                GDALGeoPackageDataset* poOvrDS = new GDALGeoPackageDataset();
                poOvrDS->InitRaster(
                    this, m_osRasterTable,
                    nNewZoomLevel, nBands,
                    m_dfTMSMinX, m_dfTMSMaxY,
                    dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel,
                    nTileWidth, nTileHeight,
                    nTileMatrixWidth,nTileMatrixHeight,
                    dfGDALMinX, dfGDALMinY,
                    dfGDALMaxX, dfGDALMaxY );
                m_papoOverviewDS = (GDALGeoPackageDataset**) CPLRealloc(
                    m_papoOverviewDS, sizeof(GDALGeoPackageDataset*) * (m_nOverviewCount+1));

                if( jCandidate < m_nOverviewCount )
                {
                    memmove(m_papoOverviewDS + jCandidate + 1,
                            m_papoOverviewDS + jCandidate,
                            sizeof(GDALGeoPackageDataset*) * (m_nOverviewCount-jCandidate));
                }
                m_papoOverviewDS[jCandidate] = poOvrDS;
                m_nOverviewCount ++;
            }
        }
    }

    GDALRasterBand*** papapoOverviewBands = (GDALRasterBand ***) CPLCalloc(sizeof(void*),nBands);
    CPLErr eErr = CE_None;
    for( int iBand = 0; eErr == CE_None && iBand < nBands; iBand++ )
    {
        papapoOverviewBands[iBand] = (GDALRasterBand **) CPLCalloc(sizeof(void*),nOverviews);
        int iCurOverview = 0;
        for(int i=0;i<nOverviews;i++)
        {
            int j = 0;  // Used after for.
            for( ; j < m_nOverviewCount; j++ )
            {
                GDALDataset* poODS = m_papoOverviewDS[j];

                int nOvFactor =
                    GDALComputeOvFactor(poODS->GetRasterXSize(),
                                        GetRasterXSize(),
                                        poODS->GetRasterYSize(),
                                        GetRasterYSize());
                if( nOvFactor > 64 &&
                    std::abs(nOvFactor - GetFloorPowerOfTwo(nOvFactor)) <= 2 )
                {
                    nOvFactor = GetFloorPowerOfTwo(nOvFactor);
                }

                if( nOvFactor == panOverviewList[i]
                    || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                        GetRasterXSize(),
                                                        GetRasterYSize() ) )
                {
                    papapoOverviewBands[iBand][iCurOverview] = poODS->GetRasterBand(iBand+1);
                    iCurOverview++ ;
                    break;
                }
            }
            if( j == m_nOverviewCount )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not find dataset corresponding to ov factor %d",
                         panOverviewList[i]);
                eErr = CE_Failure;
            }
        }
        if( eErr == CE_None )
        {
            CPLAssert(iCurOverview == nOverviews);
        }
    }

    if( eErr == CE_None )
        eErr = GDALRegenerateOverviewsMultiBand(nBands, papoBands,
                                     nOverviews, papapoOverviewBands,
                                     pszResampling, pfnProgress, pProgressData );

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        CPLFree(papapoOverviewBands[iBand]);
    }
    CPLFree(papapoOverviewBands);

    return eErr;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GDALGeoPackageDataset::GetMetadataDomainList()
{
    GetMetadata();
    if( !m_osRasterTable.empty() )
        GetMetadata("GEOPACKAGE");
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                        CheckMetadataDomain()                         */
/************************************************************************/

const char* GDALGeoPackageDataset::CheckMetadataDomain( const char* pszDomain )
{
    if( pszDomain != NULL && EQUAL(pszDomain, "GEOPACKAGE") &&
        m_osRasterTable.empty() )
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "Using GEOPACKAGE for a non-raster geopackage is not supported. "
                 "Using default domain instead");
        return NULL;
    }
    return pszDomain;
}

/************************************************************************/
/*                           HasMetadataTables()                        */
/************************************************************************/

bool GDALGeoPackageDataset::HasMetadataTables()
{
    OGRErr err;
    const int nCount = SQLGetInteger(hDB,
                  "SELECT COUNT(*) FROM sqlite_master WHERE name IN "
                  "('gpkg_metadata', 'gpkg_metadata_reference') "
                  "AND type IN ('table', 'view')", &err);
    return err == OGRERR_NONE && nCount == 2;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALGeoPackageDataset::GetMetadata( const char *pszDomain )

{
    pszDomain = CheckMetadataDomain(pszDomain);
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return m_papszSubDatasets;

    if( m_bHasReadMetadataFromStorage )
        return GDALPamDataset::GetMetadata( pszDomain );

    m_bHasReadMetadataFromStorage = true;

    if( !HasMetadataTables() )
        return GDALPamDataset::GetMetadata( pszDomain );

    char* pszSQL = NULL;
    if( !m_osRasterTable.empty() )
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, mdr.reference_scope FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE mdr.reference_scope = 'geopackage' OR "
            "(mdr.reference_scope = 'table' AND mdr.table_name = '%q') ORDER BY md.id",
            m_osRasterTable.c_str());
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, mdr.reference_scope FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE mdr.reference_scope = 'geopackage' ORDER BY md.id");
    }

    SQLResult oResult;
    OGRErr err = SQLQuery(hDB, pszSQL, &oResult);
    sqlite3_free(pszSQL);
    if  ( err != OGRERR_NONE )
    {
        SQLResultFree(&oResult);
        return GDALPamDataset::GetMetadata( pszDomain );
    }

    char** papszMetadata = CSLDuplicate(GDALPamDataset::GetMetadata());

    /* GDAL metadata */
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
        {
            CPLXMLNode* psXMLNode = CPLParseXMLString(pszMetadata);
            if( psXMLNode )
            {
                GDALMultiDomainMetadata oLocalMDMD;
                oLocalMDMD.XMLInit(psXMLNode, FALSE);
                if( !m_osRasterTable.empty() && bIsGPKGScope )
                {
                    oMDMD.SetMetadata( oLocalMDMD.GetMetadata(), "GEOPACKAGE" );
                }
                else
                {
                    papszMetadata = CSLMerge(papszMetadata, oLocalMDMD.GetMetadata());
                    char** papszDomainList = oLocalMDMD.GetDomainList();
                    char** papszIter = papszDomainList;
                    while( papszIter && *papszIter )
                    {
                        if( !EQUAL(*papszIter, "") && !EQUAL(*papszIter, "IMAGE_STRUCTURE") )
                            oMDMD.SetMetadata(oLocalMDMD.GetMetadata(*papszIter), *papszIter);
                        papszIter ++;
                    }
                }
                CPLDestroyXMLNode(psXMLNode);
            }
        }
    }

    GDALPamDataset::SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    papszMetadata = NULL;

    /* Add non-GDAL metadata now */
    int nNonGDALMDILocal = 1;
    int nNonGDALMDIGeopackage = 1;
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
            continue;

        if( !m_osRasterTable.empty() && bIsGPKGScope )
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDIGeopackage),
                                   pszMetadata,
                                   "GEOPACKAGE" );
            nNonGDALMDIGeopackage ++;
        }
        /*else if( strcmp( pszMDStandardURI, "http://www.isotc211.org/2005/gmd" ) == 0 &&
            strcmp( pszMimeType, "text/xml" ) == 0 )
        {
            char* apszMD[2];
            apszMD[0] = (char*)pszMetadata;
            apszMD[1] = NULL;
            oMDMD.SetMetadata(apszMD, "xml:MD_Metadata");
        }*/
        else
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDILocal),
                                   pszMetadata );
            nNonGDALMDILocal ++;
        }
    }

    SQLResultFree(&oResult);

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                            WriteMetadata()                           */
/************************************************************************/

void GDALGeoPackageDataset::WriteMetadata(CPLXMLNode* psXMLNode, /* will be destroyed by the method */
                                          const char* pszTableName)
{
    int bIsEmpty = (psXMLNode == NULL);
    char *pszXML = NULL;
    if( !bIsEmpty )
    {
        CPLXMLNode* psMasterXMLNode = CPLCreateXMLNode( NULL, CXT_Element,
                                                        "GDALMultiDomainMetadata" );
        psMasterXMLNode->psChild = psXMLNode;
        pszXML = CPLSerializeXMLTree(psMasterXMLNode);
        CPLDestroyXMLNode(psMasterXMLNode);
    }
    // cppcheck-suppress uselessAssignmentPtrArg
    psXMLNode = NULL;

    char* pszSQL = NULL;
    if( pszTableName && pszTableName[0] != '\0' )
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.id FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE md.md_scope = 'dataset' AND md.md_standard_uri='http://gdal.org' "
            "AND md.mime_type='text/xml' AND mdr.reference_scope = 'table' AND mdr.table_name = '%q'",
            pszTableName);
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.id FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE md.md_scope = 'dataset' AND md.md_standard_uri='http://gdal.org' "
            "AND md.mime_type='text/xml' AND mdr.reference_scope = 'geopackage'");
    }
    OGRErr err;
    int mdId = SQLGetInteger(hDB, pszSQL, &err);
    if( err != OGRERR_NONE )
        mdId = -1;
    sqlite3_free(pszSQL);

    if( bIsEmpty )
    {
        if( mdId >= 0 )
        {
            SQLCommand(hDB,
                       CPLSPrintf("DELETE FROM gpkg_metadata_reference WHERE md_file_id = %d", mdId));
            SQLCommand(hDB,
                       CPLSPrintf("DELETE FROM gpkg_metadata WHERE id = %d", mdId));
        }
    }
    else
    {
        if( mdId >= 0 )
        {
            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_metadata SET metadata = '%q' WHERE id = %d",
                pszXML, mdId);
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_metadata (md_scope, md_standard_uri, mime_type, metadata) VALUES "
                "('dataset','http://gdal.org','text/xml','%q')",
                pszXML);
        }
        SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        CPLFree(pszXML);

        if( mdId < 0 )
        {
            const sqlite_int64 nFID = sqlite3_last_insert_rowid( hDB );
            if( pszTableName != NULL && pszTableName[0] != '\0' )
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_metadata_reference (reference_scope, table_name, timestamp, md_file_id) VALUES "
                    "('table', '%q', strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now'), %d)",
                    pszTableName, (int)nFID);
            }
            else
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_metadata_reference (reference_scope, timestamp, md_file_id) VALUES "
                    "('geopackage', strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now'), %d)",
                    (int)nFID);
            }
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_metadata_reference SET timestamp = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now') WHERE md_file_id = %d",
                mdId);
        }
        SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }
}

/************************************************************************/
/*                        CreateMetadataTables()                        */
/************************************************************************/

bool GDALGeoPackageDataset::CreateMetadataTables()
{
    const bool bCreateTriggers =
        CPLTestBool(CPLGetConfigOption("CREATE_TRIGGERS", "YES"));

    /* From C.10. gpkg_metadata Table 35. gpkg_metadata Table Definition SQL  */
    const char* pszMetadata =
        "CREATE TABLE gpkg_metadata ("
        "id INTEGER CONSTRAINT m_pk PRIMARY KEY ASC NOT NULL UNIQUE,"
        "md_scope TEXT NOT NULL DEFAULT 'dataset',"
        "md_standard_uri TEXT NOT NULL,"
        "mime_type TEXT NOT NULL DEFAULT 'text/xml',"
        "metadata TEXT NOT NULL"
        ")";

    if ( OGRERR_NONE != SQLCommand(hDB, pszMetadata) )
        return false;

    /* From D.2. metadata Table 40. metadata Trigger Definition SQL  */
    const char* pszMetadataTriggers =
    "CREATE TRIGGER 'gpkg_metadata_md_scope_insert' "
    "BEFORE INSERT ON 'gpkg_metadata' "
    "FOR EACH ROW BEGIN "
    "SELECT RAISE(ABORT, 'insert on table gpkg_metadata violates "
    "constraint: md_scope must be one of undefined | fieldSession | "
    "collectionSession | series | dataset | featureType | feature | "
    "attributeType | attribute | tile | model | catalogue | schema | "
    "taxonomy software | service | collectionHardware | "
    "nonGeographicDataset | dimensionGroup') "
    "WHERE NOT(NEW.md_scope IN "
    "('undefined','fieldSession','collectionSession','series','dataset', "
    "'featureType','feature','attributeType','attribute','tile','model', "
    "'catalogue','schema','taxonomy','software','service', "
    "'collectionHardware','nonGeographicDataset','dimensionGroup')); "
    "END; "
    "CREATE TRIGGER 'gpkg_metadata_md_scope_update' "
    "BEFORE UPDATE OF 'md_scope' ON 'gpkg_metadata' "
    "FOR EACH ROW BEGIN "
    "SELECT RAISE(ABORT, 'update on table gpkg_metadata violates "
    "constraint: md_scope must be one of undefined | fieldSession | "
    "collectionSession | series | dataset | featureType | feature | "
    "attributeType | attribute | tile | model | catalogue | schema | "
    "taxonomy software | service | collectionHardware | "
    "nonGeographicDataset | dimensionGroup') "
    "WHERE NOT(NEW.md_scope IN "
    "('undefined','fieldSession','collectionSession','series','dataset', "
    "'featureType','feature','attributeType','attribute','tile','model', "
    "'catalogue','schema','taxonomy','software','service', "
    "'collectionHardware','nonGeographicDataset','dimensionGroup')); "
    "END";
    if ( bCreateTriggers && OGRERR_NONE != SQLCommand(hDB, pszMetadataTriggers) )
        return false;

    /* From C.11. gpkg_metadata_reference Table 36. gpkg_metadata_reference Table Definition SQL */
    const char* pszMetadataReference =
        "CREATE TABLE gpkg_metadata_reference ("
        "reference_scope TEXT NOT NULL,"
        "table_name TEXT,"
        "column_name TEXT,"
        "row_id_value INTEGER,"
        "timestamp DATETIME NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
        "md_file_id INTEGER NOT NULL,"
        "md_parent_id INTEGER,"
        "CONSTRAINT crmr_mfi_fk FOREIGN KEY (md_file_id) REFERENCES gpkg_metadata(id),"
        "CONSTRAINT crmr_mpi_fk FOREIGN KEY (md_parent_id) REFERENCES gpkg_metadata(id)"
        ")";

    if ( OGRERR_NONE != SQLCommand(hDB, pszMetadataReference) )
        return false;

    /* From D.3. metadata_reference Table 41. gpkg_metadata_reference Trigger Definition SQL   */
    const char* pszMetadataReferenceTriggers =
        "CREATE TRIGGER 'gpkg_metadata_reference_reference_scope_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: reference_scope must be one of \"geopackage\", "
        "table\", \"column\", \"row\", \"row/col\"') "
        "WHERE NOT NEW.reference_scope IN "
        "('geopackage','table','column','row','row/col'); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_reference_scope_update' "
        "BEFORE UPDATE OF 'reference_scope' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: reference_scope must be one of \"geopackage\", "
        "\"table\", \"column\", \"row\", \"row/col\"') "
        "WHERE NOT NEW.reference_scope IN "
        "('geopackage','table','column','row','row/col'); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_column_name_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: column name must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"row\"') "
        "WHERE (NEW.reference_scope IN ('geopackage','table','row') "
        "AND NEW.column_name IS NOT NULL); "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: column name must be defined for the specified "
        "table when reference_scope is \"column\" or \"row/col\"') "
        "WHERE (NEW.reference_scope IN ('column','row/col') "
        "AND NOT NEW.table_name IN ( "
        "SELECT name FROM SQLITE_MASTER WHERE type = 'table' "
        "AND name = NEW.table_name "
        "AND sql LIKE ('%' || NEW.column_name || '%'))); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_column_name_update' "
        "BEFORE UPDATE OF column_name ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: column name must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"row\"') "
        "WHERE (NEW.reference_scope IN ('geopackage','table','row') "
        "AND NEW.column_nameIS NOT NULL); "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: column name must be defined for the specified "
        "table when reference_scope is \"column\" or \"row/col\"') "
        "WHERE (NEW.reference_scope IN ('column','row/col') "
        "AND NOT NEW.table_name IN ( "
        "SELECT name FROM SQLITE_MASTER WHERE type = 'table' "
        "AND name = NEW.table_name "
        "AND sql LIKE ('%' || NEW.column_name || '%'))); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_row_id_value_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: row_id_value must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"column\"') "
        "WHERE NEW.reference_scope IN ('geopackage','table','column') "
        "AND NEW.row_id_value IS NOT NULL; "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: row_id_value must exist in specified table when "
        "reference_scope is \"row\" or \"row/col\"') "
        "WHERE NEW.reference_scope IN ('row','row/col') "
        "AND NOT EXISTS (SELECT rowid "
        "FROM (SELECT NEW.table_name AS table_name) WHERE rowid = "
        "NEW.row_id_value); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_row_id_value_update' "
        "BEFORE UPDATE OF 'row_id_value' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: row_id_value must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"column\"') "
        "WHERE NEW.reference_scope IN ('geopackage','table','column') "
        "AND NEW.row_id_value IS NOT NULL; "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: row_id_value must exist in specified table when "
        "reference_scope is \"row\" or \"row/col\"') "
        "WHERE NEW.reference_scope IN ('row','row/col') "
        "AND NOT EXISTS (SELECT rowid "
        "FROM (SELECT NEW.table_name AS table_name) WHERE rowid = "
        "NEW.row_id_value); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: timestamp must be a valid time in ISO 8601 "
        "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
        "WHERE NOT (NEW.timestamp GLOB "
        "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z' "
        "AND strftime('%s',NEW.timestamp) NOT NULL); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_update' "
        "BEFORE UPDATE OF 'timestamp' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: timestamp must be a valid time in ISO 8601 "
        "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
        "WHERE NOT (NEW.timestamp GLOB "
        "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z' "
        "AND strftime('%s',NEW.timestamp) NOT NULL); "
        "END";
    if ( bCreateTriggers && OGRERR_NONE != SQLCommand(hDB, pszMetadataReferenceTriggers) )
        return false;

    return true;
}

/************************************************************************/
/*                            FlushMetadata()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::FlushMetadata()
{
    if( !m_bMetadataDirty || m_poParentDS != NULL ||
        !CPLTestBool(CPLGetConfigOption("CREATE_METADATA_TABLES", "YES")) )
        return CE_None;
    if( !HasMetadataTables() && !CreateMetadataTables() )
        return CE_Failure;
    m_bMetadataDirty = false;

    if( !m_osRasterTable.empty() )
    {
        const char* pszIdentifier = GetMetadataItem("IDENTIFIER");
        const char* pszDescription = GetMetadataItem("DESCRIPTION");
        if( !m_bIdentifierAsCO && pszIdentifier != NULL &&
            pszIdentifier != m_osIdentifier )
        {
            m_osIdentifier = pszIdentifier;
            char* pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET identifier = '%q' WHERE table_name = '%q'",
                pszIdentifier, m_osRasterTable.c_str());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
        if( !m_bDescriptionAsCO && pszDescription != NULL &&
            pszDescription != m_osDescription )
        {
            m_osDescription = pszDescription;
            char* pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET description = '%q' WHERE table_name = '%q'",
                pszDescription, m_osRasterTable.c_str());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
    }

    char** papszMDDup = NULL;
    for( char** papszIter = GetMetadata(); papszIter && *papszIter; ++papszIter )
    {
        if( STARTS_WITH_CI(*papszIter, "IDENTIFIER=") )
            continue;
        if( STARTS_WITH_CI(*papszIter, "DESCRIPTION=") )
            continue;
        if( STARTS_WITH_CI(*papszIter, "ZOOM_LEVEL=") )
            continue;
        if( STARTS_WITH_CI(*papszIter, "GPKG_METADATA_ITEM_") )
            continue;
        papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
    }

    CPLXMLNode* psXMLNode = NULL;
    {
        GDALMultiDomainMetadata oLocalMDMD;
        char** papszDomainList = oMDMD.GetDomainList();
        char** papszIter = papszDomainList;
        oLocalMDMD.SetMetadata(papszMDDup);
        while( papszIter && *papszIter )
        {
            if( !EQUAL(*papszIter, "") &&
                !EQUAL(*papszIter, "IMAGE_STRUCTURE") &&
                !EQUAL(*papszIter, "GEOPACKAGE") )
                oLocalMDMD.SetMetadata(oMDMD.GetMetadata(*papszIter), *papszIter);
            papszIter ++;
        }
        psXMLNode = oLocalMDMD.Serialize();
    }

    CSLDestroy(papszMDDup);
    papszMDDup = NULL;

    WriteMetadata(psXMLNode, m_osRasterTable.c_str() );

    if( !m_osRasterTable.empty() )
    {
        char** papszGeopackageMD = GetMetadata("GEOPACKAGE");

        papszMDDup = NULL;
        for( char** papszIter = papszGeopackageMD; papszIter && *papszIter; ++papszIter )
        {
            papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
        }

        GDALMultiDomainMetadata oLocalMDMD;
        oLocalMDMD.SetMetadata(papszMDDup);
        CSLDestroy(papszMDDup);
        papszMDDup = NULL;
        psXMLNode = oLocalMDMD.Serialize();

        WriteMetadata(psXMLNode, NULL);
    }

    for(int i=0;i<m_nLayers;i++)
    {
        const char* pszIdentifier = m_papoLayers[i]->GetMetadataItem("IDENTIFIER");
        const char* pszDescription = m_papoLayers[i]->GetMetadataItem("DESCRIPTION");
        if( pszIdentifier != NULL )
        {
            char* pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET identifier = '%q' WHERE table_name = '%q'",
                pszIdentifier, m_papoLayers[i]->GetName());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
        if( pszDescription != NULL )
        {
            char* pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET description = '%q' WHERE table_name = '%q'",
                pszDescription, m_papoLayers[i]->GetName());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }

        papszMDDup = NULL;
        for( char** papszIter = m_papoLayers[i]->GetMetadata(); papszIter && *papszIter; ++papszIter )
        {
            if( STARTS_WITH_CI(*papszIter, "IDENTIFIER=") )
                continue;
            if( STARTS_WITH_CI(*papszIter, "DESCRIPTION=") )
                continue;
            if( STARTS_WITH_CI(*papszIter, "OLMD_FID64=") )
                continue;
            papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
        }

        {
            GDALMultiDomainMetadata oLocalMDMD;
            char** papszDomainList = m_papoLayers[i]->GetMetadataDomainList();
            char** papszIter = papszDomainList;
            oLocalMDMD.SetMetadata(papszMDDup);
            while( papszIter && *papszIter )
            {
                if( !EQUAL(*papszIter, "") )
                    oLocalMDMD.SetMetadata(m_papoLayers[i]->GetMetadata(*papszIter), *papszIter);
                papszIter ++;
            }
            CSLDestroy(papszDomainList);
            psXMLNode = oLocalMDMD.Serialize();
        }

        CSLDestroy(papszMDDup);
        papszMDDup = NULL;

        WriteMetadata(psXMLNode, m_papoLayers[i]->GetName() );
    }

    return CE_None;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALGeoPackageDataset::GetMetadataItem( const char * pszName,
                                                    const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetMetadata( char ** papszMetadata, const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = true;
    GetMetadata(); /* force loading from storage if needed */
    return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetMetadataItem( const char * pszName,
                                               const char * pszValue,
                                               const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = true;
    GetMetadata(); /* force loading from storage if needed */
    return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

int GDALGeoPackageDataset::Create( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char **papszOptions )
{
    CPLString osCommand;

    /* First, ensure there isn't any such file yet. */
    VSIStatBufL sStatBuf;

    if( nBandsIn != 0 )
    {
        if( eDT != GDT_Byte )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Only Byte supported");
            return FALSE;
        }
        if( nBandsIn != 1 && nBandsIn != 2 && nBandsIn != 3 && nBandsIn != 4 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), 3 (RGB) or 4 (RGBA) band dataset supported");
            return FALSE;
        }
    }

    bool bFileExists = false;
    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
    {
        bFileExists = true;
        if( nBandsIn == 0 ||
            !CPLTestBool(CSLFetchNameValueDef(papszOptions, "APPEND_SUBDATASET", "NO")) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "A file system object called '%s' already exists.",
                    pszFilename );

            return FALSE;
        }
    }
    m_pszFilename = CPLStrdup(pszFilename);
    m_bNew = true;
    bUpdate = TRUE;
    eAccess = GA_Update; /* hum annoying duplication */

    // for test/debug purposes only. true is the nominal value
    m_bPNGSupports2Bands = CPLTestBool(CPLGetConfigOption("GPKG_PNG_SUPPORTS_2BANDS", "TRUE"));
    m_bPNGSupportsCT = CPLTestBool(CPLGetConfigOption("GPKG_PNG_SUPPORTS_CT", "TRUE"));

    if( !OpenOrCreateDB(bFileExists
                        ? SQLITE_OPEN_READWRITE
                        : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE) )
        return FALSE;

    /* Default to synchronous=off for performance for new file */
    if( !bFileExists &&
        CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", NULL) == NULL )
    {
        sqlite3_exec( hDB, "PRAGMA synchronous = OFF", NULL, NULL, NULL );
    }

    /* OGR UTF-8 support. If we set the UTF-8 Pragma early on, it */
    /* will be written into the main file and supported henceforth */
    SQLCommand(hDB, "PRAGMA encoding = \"UTF-8\"");

    SoftStartTransaction();

    int bCreateTriggers = CPLTestBool(CPLGetConfigOption("CREATE_TRIGGERS", "YES"));
    int bCreateGeometryColumns = CPLTestBool(CPLGetConfigOption("CREATE_GEOMETRY_COLUMNS", "YES"));
    if( !bFileExists )
    {
        /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) in the application id */
        /* http://opengis.github.io/geopackage/#_file_format */
        const char *pszPragma = CPLSPrintf("PRAGMA application_id = %d", GPKG_APPLICATION_ID);

        if ( OGRERR_NONE != SQLCommand(hDB, pszPragma) )
            return FALSE;

        /* Requirement 10: A GeoPackage SHALL include a gpkg_spatial_ref_sys table */
        /* http://opengis.github.io/geopackage/#spatial_ref_sys */
        const char *pszSpatialRefSys =
            "CREATE TABLE gpkg_spatial_ref_sys ("
            "srs_name TEXT NOT NULL,"
            "srs_id INTEGER NOT NULL PRIMARY KEY,"
            "organization TEXT NOT NULL,"
            "organization_coordsys_id INTEGER NOT NULL,"
            "definition  TEXT NOT NULL,"
            "description TEXT"
            ")";

        if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSys) )
            return FALSE;

        /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage SHALL */
        /* contain a record for EPSG:4326, the geodetic WGS84 SRS */
        /* http://opengis.github.io/geopackage/#spatial_ref_sys */
        const char *pszSpatialRefSysRecord =
            "INSERT INTO gpkg_spatial_ref_sys ("
            "srs_name, srs_id, organization, organization_coordsys_id, definition, description"
            ") VALUES ("
            "'WGS 84 geodetic', 4326, 'EPSG', 4326, '"
            "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]]"
            "', 'longitude/latitude coordinates in decimal degrees on the WGS 84 spheroid'"
            ")";

        if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSysRecord) )
            return FALSE;

        /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage SHALL */
        /* contain a record with an srs_id of -1, an organization of “NONE”, */
        /* an organization_coordsys_id of -1, and definition “undefined” */
        /* for undefined Cartesian coordinate reference systems */
        /* http://opengis.github.io/geopackage/#spatial_ref_sys */
        pszSpatialRefSysRecord =
            "INSERT INTO gpkg_spatial_ref_sys ("
            "srs_name, srs_id, organization, organization_coordsys_id, definition, description"
            ") VALUES ("
            "'Undefined cartesian SRS', -1, 'NONE', -1, 'undefined', 'undefined cartesian coordinate reference system'"
            ")";

        if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSysRecord) )
            return FALSE;

        /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage SHALL */
        /* contain a record with an srs_id of 0, an organization of “NONE”, */
        /* an organization_coordsys_id of 0, and definition “undefined” */
        /* for undefined geographic coordinate reference systems */
        /* http://opengis.github.io/geopackage/#spatial_ref_sys */
        pszSpatialRefSysRecord =
            "INSERT INTO gpkg_spatial_ref_sys ("
            "srs_name, srs_id, organization, organization_coordsys_id, definition, description"
            ") VALUES ("
            "'Undefined geographic SRS', 0, 'NONE', 0, 'undefined', 'undefined geographic coordinate reference system'"
            ")";

        if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSysRecord) )
            return FALSE;

        /* Requirement 13: A GeoPackage file SHALL include a gpkg_contents table */
        /* http://opengis.github.io/geopackage/#_contents */
        const char *pszContents =
            "CREATE TABLE gpkg_contents ("
            "table_name TEXT NOT NULL PRIMARY KEY,"
            "data_type TEXT NOT NULL,"
            "identifier TEXT UNIQUE,"
            "description TEXT DEFAULT '',"
            "last_change DATETIME NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
            "min_x DOUBLE, min_y DOUBLE,"
            "max_x DOUBLE, max_y DOUBLE,"
            "srs_id INTEGER,"
            "CONSTRAINT fk_gc_r_srs_id FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys(srs_id)"
            ")";

        if ( OGRERR_NONE != SQLCommand(hDB, pszContents) )
            return FALSE;

        /* Requirement 21: A GeoPackage with a gpkg_contents table row with a “features” */
        /* data_type SHALL contain a gpkg_geometry_columns table or updateable view */
        /* http://opengis.github.io/geopackage/#_geometry_columns */
        const char *pszGeometryColumns =
            "CREATE TABLE gpkg_geometry_columns ("
            "table_name TEXT NOT NULL,"
            "column_name TEXT NOT NULL,"
            "geometry_type_name TEXT NOT NULL,"
            "srs_id INTEGER NOT NULL,"
            "z TINYINT NOT NULL,"
            "m TINYINT NOT NULL,"
            "CONSTRAINT pk_geom_cols PRIMARY KEY (table_name, column_name),"
            "CONSTRAINT uk_gc_table_name UNIQUE (table_name),"
            "CONSTRAINT fk_gc_tn FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name),"
            "CONSTRAINT fk_gc_srs FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys (srs_id)"
            ")";

        if ( bCreateGeometryColumns && OGRERR_NONE != SQLCommand(hDB, pszGeometryColumns) )
            return FALSE;

        /* From C.5. gpkg_tile_matrix_set Table 28. gpkg_tile_matrix_set Table Creation SQL  */
        const char *pszTileMatrixSet =
            "CREATE TABLE gpkg_tile_matrix_set ("
            "table_name TEXT NOT NULL PRIMARY KEY,"
            "srs_id INTEGER NOT NULL,"
            "min_x DOUBLE NOT NULL,"
            "min_y DOUBLE NOT NULL,"
            "max_x DOUBLE NOT NULL,"
            "max_y DOUBLE NOT NULL,"
            "CONSTRAINT fk_gtms_table_name FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name),"
            "CONSTRAINT fk_gtms_srs FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys (srs_id)"
            ")";

        if ( OGRERR_NONE != SQLCommand(hDB, pszTileMatrixSet) )
            return FALSE;

        /* From C.6. gpkg_tile_matrix Table 29. gpkg_tile_matrix Table Creation SQL */
        const char *pszTileMatrix =
            "CREATE TABLE gpkg_tile_matrix ("
            "table_name TEXT NOT NULL,"
            "zoom_level INTEGER NOT NULL,"
            "matrix_width INTEGER NOT NULL,"
            "matrix_height INTEGER NOT NULL,"
            "tile_width INTEGER NOT NULL,"
            "tile_height INTEGER NOT NULL,"
            "pixel_x_size DOUBLE NOT NULL,"
            "pixel_y_size DOUBLE NOT NULL,"
            "CONSTRAINT pk_ttm PRIMARY KEY (table_name, zoom_level),"
            "CONSTRAINT fk_tmm_table_name FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name)"
            ")";

        if ( OGRERR_NONE != SQLCommand(hDB, pszTileMatrix) )
            return FALSE;

        /* From D.1. gpkg_tile_matrix Table 39. gpkg_tile_matrix Trigger Definition SQL */
        const char* pszTileMatrixTrigger =
        "CREATE TRIGGER 'gpkg_tile_matrix_zoom_level_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: zoom_level cannot be less than 0') "
        "WHERE (NEW.zoom_level < 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_zoom_level_update' "
        "BEFORE UPDATE of zoom_level ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: zoom_level cannot be less than 0') "
        "WHERE (NEW.zoom_level < 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_width_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: matrix_width cannot be less than 1') "
        "WHERE (NEW.matrix_width < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_width_update' "
        "BEFORE UPDATE OF matrix_width ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: matrix_width cannot be less than 1') "
        "WHERE (NEW.matrix_width < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_height_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: matrix_height cannot be less than 1') "
        "WHERE (NEW.matrix_height < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_height_update' "
        "BEFORE UPDATE OF matrix_height ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: matrix_height cannot be less than 1') "
        "WHERE (NEW.matrix_height < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_x_size_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: pixel_x_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_x_size > 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_x_size_update' "
        "BEFORE UPDATE OF pixel_x_size ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: pixel_x_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_x_size > 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_y_size_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: pixel_y_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_y_size > 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_y_size_update' "
        "BEFORE UPDATE OF pixel_y_size ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: pixel_y_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_y_size > 0); "
        "END;";
        if ( bCreateTriggers && OGRERR_NONE != SQLCommand(hDB, pszTileMatrixTrigger) )
            return FALSE;

        if( CPLTestBool(CPLGetConfigOption("CREATE_METADATA_TABLES", "YES")) &&
            !CreateMetadataTables() )
            return FALSE;
    }

    if( nBandsIn != 0 )
    {
        const char* pszTableName = CPLGetBasename(m_pszFilename);
        m_osRasterTable = CSLFetchNameValueDef(papszOptions, "RASTER_TABLE", pszTableName);
        m_bIdentifierAsCO = CSLFetchNameValue(papszOptions, "RASTER_IDENTIFIER" ) != NULL;
        m_osIdentifier = CSLFetchNameValueDef(papszOptions, "RASTER_IDENTIFIER", m_osRasterTable);
        m_bDescriptionAsCO = CSLFetchNameValue(papszOptions, "RASTER_DESCRIPTION" ) != NULL;
        m_osDescription = CSLFetchNameValueDef(papszOptions, "RASTER_DESCRIPTION", "");

        /* From C.7. sample_tile_pyramid (Informative) Table 31. EXAMPLE: tiles table Create Table SQL (Informative) */
        char* pszSQL = sqlite3_mprintf("CREATE TABLE \"%w\" ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "zoom_level INTEGER NOT NULL,"
          "tile_column INTEGER NOT NULL,"
          "tile_row INTEGER NOT NULL,"
          "tile_data BLOB NOT NULL,"
          "UNIQUE (zoom_level, tile_column, tile_row)"
        ")", m_osRasterTable.c_str());
        OGRErr eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if ( OGRERR_NONE != eErr )
            return FALSE;

        /* From D.5. sample_tile_pyramid Table 43. tiles table Trigger Definition SQL  */
        char* pszSQLTriggers = sqlite3_mprintf("CREATE TRIGGER \"%w_zoom_insert\" "
        "BEFORE INSERT ON \"%w\" "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: zoom_level not specified for table in gpkg_tile_matrix') "
        "WHERE NOT (NEW.zoom_level IN (SELECT zoom_level FROM gpkg_tile_matrix WHERE table_name = '%q')) ; "
        "END; "
        "CREATE TRIGGER \"%w_zoom_update\" "
        "BEFORE UPDATE OF zoom_level ON \"%w\" "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: zoom_level not specified for table in gpkg_tile_matrix') "
        "WHERE NOT (NEW.zoom_level IN (SELECT zoom_level FROM gpkg_tile_matrix WHERE table_name = '%q')) ; "
        "END; "
        "CREATE TRIGGER \"%w_tile_column_insert\" "
        "BEFORE INSERT ON \"%w\" "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_column cannot be < 0') "
        "WHERE (NEW.tile_column < 0) ; "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_column must by < matrix_width specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_column < (SELECT matrix_width FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; "
        "CREATE TRIGGER \"%w_tile_column_update\" "
        "BEFORE UPDATE OF tile_column ON \"%w\" "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_column cannot be < 0') "
        "WHERE (NEW.tile_column < 0) ; "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_column must by < matrix_width specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_column < (SELECT matrix_width FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; "
        "CREATE TRIGGER \"%w_tile_row_insert\" "
        "BEFORE INSERT ON \"%w\" "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_row cannot be < 0') "
        "WHERE (NEW.tile_row < 0) ; "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_row must by < matrix_height specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_row < (SELECT matrix_height FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; "
        "CREATE TRIGGER \"%w_tile_row_update\" "
        "BEFORE UPDATE OF tile_row ON \"%w\" "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_row cannot be < 0') "
        "WHERE (NEW.tile_row < 0) ; "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_row must by < matrix_height specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_row < (SELECT matrix_height FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; ",
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str()
        );
        if( bCreateTriggers )
        {
            eErr = SQLCommand(hDB, pszSQLTriggers);
            sqlite3_free(pszSQLTriggers);
            if ( OGRERR_NONE != eErr )
                return FALSE;
        }

        nRasterXSize = nXSize;
        nRasterYSize = nYSize;

        const char* pszTileSize = CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "256");
        const char* pszTileWidth = CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", pszTileSize);
        const char* pszTileHeight = CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", pszTileSize);
        int nTileWidth = atoi(pszTileWidth);
        int nTileHeight = atoi(pszTileHeight);
        if( (nTileWidth < 8 || nTileWidth > 4096 || nTileHeight < 8 || nTileHeight > 4096) &&
            !CPLTestBool(CPLGetConfigOption("GPKG_ALLOW_CRAZY_SETTINGS", "NO")) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid block dimensions: %dx%d",
                     nTileWidth, nTileHeight);
            return FALSE;
        }

        m_pabyCachedTiles = (GByte*) VSI_MALLOC3_VERBOSE(4 * 4, nTileWidth, nTileHeight);
        if( m_pabyCachedTiles == NULL )
        {
            return FALSE;
        }

        for(int i = 1; i <= nBandsIn; i ++)
            SetBand( i, new GDALGeoPackageRasterBand(this, nTileWidth, nTileHeight) );

        GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        GDALPamDataset::SetMetadataItem("IDENTIFIER", m_osIdentifier);
        if( !m_osDescription.empty() )
            GDALPamDataset::SetMetadataItem("DESCRIPTION", m_osDescription);

        const char* pszTF = CSLFetchNameValue(papszOptions, "TILE_FORMAT");
        if( pszTF )
            m_eTF = GDALGPKGMBTilesGetTileFormat(pszTF);

        ParseCompressionOptions(papszOptions);

        if( m_eTF == GPKG_TF_WEBP )
        {
            if( !RegisterWebPExtension() )
                return FALSE;
        }

        const char* pszTilingScheme = CSLFetchNameValue(papszOptions, "TILING_SCHEME");
        if( pszTilingScheme )
        {
            m_osTilingScheme = pszTilingScheme;
            bool bFound = false;
            for(size_t iScheme = 0;
                iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
                 iScheme++ )
            {
                if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
                {
                    if( nTileWidth != asTilingShemes[iScheme].nTileWidth ||
                        nTileHeight != asTilingShemes[iScheme].nTileHeight )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                "Tile dimension should be %dx%d for %s tiling scheme",
                                asTilingShemes[iScheme].nTileWidth,
                                asTilingShemes[iScheme].nTileHeight,
                                m_osTilingScheme.c_str());
                        return FALSE;
                    }

                    // Implicitly sets SRS.
                    OGRSpatialReference oSRS;
                    if( oSRS.importFromEPSG(asTilingShemes[iScheme].nEPSGCode)
                        != OGRERR_NONE )
                        return FALSE;
                    char* pszWKT = NULL;
                    oSRS.exportToWkt(&pszWKT);
                    SetProjection(pszWKT);
                    CPLFree(pszWKT);

                    bFound = true;
                    break;
                }
            }
            if( !bFound )
                m_osTilingScheme = "CUSTOM";
        }
    }

    SoftCommitTransaction();

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) */
    /* in the application id field of the SQLite database header */
    /* We have to do this after there's some content so the database file */
    /* is not zero length */
    SetApplicationId();

    /* Default to synchronous=off for performance for new file */
    if( !bFileExists &&
        CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", NULL) == NULL )
    {
        sqlite3_exec( hDB, "PRAGMA synchronous = OFF", NULL, NULL, NULL );
    }

    return TRUE;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

typedef struct
{
    const char*         pszName;
    GDALResampleAlg     eResampleAlg;
} WarpResamplingAlg;

static const WarpResamplingAlg asResamplingAlg[] =
{
    { "NEAREST", GRA_NearestNeighbour },
    { "BILINEAR", GRA_Bilinear },
    { "CUBIC", GRA_Cubic },
    { "CUBICSPLINE", GRA_CubicSpline },
    { "LANCZOS", GRA_Lanczos },
    { "MODE", GRA_Mode },
    { "AVERAGE", GRA_Average },
};

GDALDataset* GDALGeoPackageDataset::CreateCopy( const char *pszFilename,
                                                   GDALDataset *poSrcDS,
                                                   int bStrict,
                                                   char ** papszOptions,
                                                   GDALProgressFunc pfnProgress,
                                                   void * pProgressData )
{
    const char* pszTilingScheme =
            CSLFetchNameValueDef(papszOptions, "TILING_SCHEME", "CUSTOM");

    char** papszUpdatedOptions = CSLDuplicate(papszOptions);
    if( CPLTestBool(CSLFetchNameValueDef(papszOptions, "APPEND_SUBDATASET", "NO")) &&
        CSLFetchNameValue(papszOptions, "RASTER_TABLE") == NULL )
    {
        papszUpdatedOptions = CSLSetNameValue(papszUpdatedOptions,
                                              "RASTER_TABLE",
                                              CPLGetBasename(poSrcDS->GetDescription()));
    }

    if( EQUAL(pszTilingScheme, "CUSTOM") )
    {
        GDALDriver* poThisDriver = (GDALDriver*)GDALGetDriverByName("GPKG");
        if( !poThisDriver )
        {
            CSLDestroy(papszUpdatedOptions);
            return NULL;
        }
        GDALDataset* poDS = poThisDriver->DefaultCreateCopy(
                                    pszFilename, poSrcDS, bStrict,
                                    papszUpdatedOptions, pfnProgress, pProgressData );
        CSLDestroy(papszUpdatedOptions);
        return poDS;
    }

    int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 && nBands != 2 && nBands != 3 && nBands != 4 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), 3 (RGB) or "
                 "4 (RGBA) band dataset supported");
        CSLDestroy(papszUpdatedOptions);
        return NULL;
    }

    bool bFound = false;
    int nEPSGCode = 0;
    size_t iScheme = 0;  // Used after for.
    for( ;
         iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
         iScheme++ )
    {
        if( EQUAL(pszTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            nEPSGCode = asTilingShemes[iScheme].nEPSGCode;
            bFound = true;
            break;
        }
    }
    if( !bFound )
    {
        CSLDestroy(papszUpdatedOptions);
        return NULL;
    }

    OGRSpatialReference oSRS;
    if( oSRS.importFromEPSG(nEPSGCode) != OGRERR_NONE )
    {
        CSLDestroy(papszUpdatedOptions);
        return NULL;
    }
    char* pszWKT = NULL;
    oSRS.exportToWkt(&pszWKT);
    char** papszTO = CSLSetNameValue( NULL, "DST_SRS", pszWKT );
    void* hTransformArg =
            GDALCreateGenImgProjTransformer2( poSrcDS, NULL, papszTO );
    if( hTransformArg == NULL )
    {
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return NULL;
    }

    GDALTransformerInfo* psInfo = (GDALTransformerInfo*)hTransformArg;
    double adfGeoTransform[6];
    double adfExtent[4];
    int    nXSize, nYSize;

    if ( GDALSuggestedWarpOutput2( poSrcDS,
                                  psInfo->pfnTransform, hTransformArg,
                                  adfGeoTransform,
                                  &nXSize, &nYSize,
                                  adfExtent, 0 ) != CE_None )
    {
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        GDALDestroyGenImgProjTransformer( hTransformArg );
        return NULL;
    }

    GDALDestroyGenImgProjTransformer( hTransformArg );
    hTransformArg = NULL;

    // Hack to compensate for GDALSuggestedWarpOutput2() failure when
    // reprojection latitude = +/- 90 to EPSG:3857.
    double adfSrcGeoTransform[6];
    if( nEPSGCode == 3857 && poSrcDS->GetGeoTransform(adfSrcGeoTransform) == CE_None )
    {
        const char* pszSrcWKT = poSrcDS->GetProjectionRef();
        if( pszSrcWKT != NULL && pszSrcWKT[0] != '\0' )
        {
            OGRSpatialReference oSrcSRS;
            if( oSrcSRS.SetFromUserInput( pszSrcWKT ) == OGRERR_NONE &&
                oSrcSRS.IsGeographic() )
            {
                const double minLat =
                    std::min(adfSrcGeoTransform[3],
                             adfSrcGeoTransform[3] +
                             poSrcDS->GetRasterYSize() *
                             adfSrcGeoTransform[5]);
                const double maxLat =
                    std::max(adfSrcGeoTransform[3],
                             adfSrcGeoTransform[3] +
                             poSrcDS->GetRasterYSize() *
                             adfSrcGeoTransform[5]);
                double maxNorthing = adfGeoTransform[3];
                double minNorthing =
                    adfGeoTransform[3] + adfGeoTransform[5] * nYSize;
                bool bChanged = false;
                const double SPHERICAL_RADIUS = 6378137.0;
                const double MAX_GM =
                    SPHERICAL_RADIUS * M_PI;  // 20037508.342789244
                if( maxLat > 89.9999999 )
                {
                    bChanged = true;
                    maxNorthing = MAX_GM;
                }
                if( minLat <= -89.9999999 )
                {
                    bChanged = true;
                    minNorthing = -MAX_GM;
                }
                if( bChanged )
                {
                    adfGeoTransform[3] = maxNorthing;
                    nYSize = int((maxNorthing - minNorthing) / (-adfGeoTransform[5]) + 0.5);
                    adfExtent[1] = maxNorthing + nYSize * adfGeoTransform[5];
                    adfExtent[3] = maxNorthing;
                }
            }
        }
    }

    double dfComputedRes = adfGeoTransform[1];
    double dfPrevRes = 0.0;
    double dfRes = 0.0;
    int nZoomLevel = 0;  // Used after for.
    for( ; nZoomLevel < 25; nZoomLevel++ )
    {
        dfRes = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);
        if( dfComputedRes > dfRes )
            break;
        dfPrevRes = dfRes;
    }
    if( nZoomLevel == 25 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not find an appropriate zoom level");
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return NULL;
    }

    const char* pszZoomLevelStrategy = CSLFetchNameValueDef(papszOptions,
                                                            "ZOOM_LEVEL_STRATEGY",
                                                            "AUTO");
    if( fabs( dfComputedRes - dfRes ) / dfRes > 1e-8 )
    {
        if( EQUAL(pszZoomLevelStrategy, "LOWER") )
        {
            if( nZoomLevel > 0 )
                nZoomLevel --;
        }
        else if( EQUAL(pszZoomLevelStrategy, "UPPER") )
        {
            /* do nothing */
        }
        else if( nZoomLevel > 0 )
        {
            if( dfPrevRes / dfComputedRes < dfComputedRes / dfRes )
                nZoomLevel --;
        }
    }

    dfRes = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);

    double dfMinX = adfExtent[0];
    double dfMinY = adfExtent[1];
    double dfMaxX = adfExtent[2];
    double dfMaxY = adfExtent[3];

    nXSize = (int) ( 0.5 + ( dfMaxX - dfMinX ) / dfRes );
    nYSize = (int) ( 0.5 + ( dfMaxY - dfMinY ) / dfRes );
    adfGeoTransform[1] = dfRes;
    adfGeoTransform[5] = -dfRes;

    int nTargetBands = nBands;
    /* For grey level or RGB, if there's reprojection involved, add an alpha */
    /* channel */
    if( (nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() == NULL) ||
        nBands == 3 )
    {
        OGRSpatialReference oSrcSRS;
        oSrcSRS.SetFromUserInput(poSrcDS->GetProjectionRef());
        oSrcSRS.AutoIdentifyEPSG();
        if( oSrcSRS.GetAuthorityCode(NULL) == NULL ||
            atoi(oSrcSRS.GetAuthorityCode(NULL)) != nEPSGCode )
        {
            nTargetBands ++;
        }
    }

    GDALResampleAlg eResampleAlg = GRA_Bilinear;
    const char* pszResampling = CSLFetchNameValue(papszOptions, "RESAMPLING");
    if( pszResampling )
    {
        for(size_t iAlg = 0; iAlg < sizeof(asResamplingAlg)/sizeof(asResamplingAlg[0]); iAlg ++)
        {
            if( EQUAL(pszResampling, asResamplingAlg[iAlg].pszName) )
            {
                eResampleAlg = asResamplingAlg[iAlg].eResampleAlg;
                break;
            }
        }
    }

    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL &&
        eResampleAlg != GRA_NearestNeighbour && eResampleAlg != GRA_Mode )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Input dataset has a color table, which will likely lead to "
                 "bad results when using a resampling method other than "
                 "nearest neighbour or mode. Converting the dataset to 24/32 bit "
                 "(e.g. with gdal_translate -expand rgb/rgba) is advised.");
    }

    GDALGeoPackageDataset* poDS = new GDALGeoPackageDataset();
    if( !(poDS->Create( pszFilename, nXSize, nYSize, nTargetBands, GDT_Byte,
                        papszUpdatedOptions )) )
    {
        delete poDS;
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return NULL;
    }
    CSLDestroy(papszUpdatedOptions);
    papszUpdatedOptions = NULL;
    poDS->SetGeoTransform(adfGeoTransform);
    poDS->SetProjection(pszWKT);
    CPLFree(pszWKT);
    pszWKT = NULL;
    if( nTargetBands == 1 && nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL )
    {
        poDS->GetRasterBand(1)->SetColorTable( poSrcDS->GetRasterBand(1)->GetColorTable() );
    }

    hTransformArg =
        GDALCreateGenImgProjTransformer2( poSrcDS, poDS, papszTO );
    CSLDestroy(papszTO);
    if( hTransformArg == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Warp the transformer with a linear approximator                 */
/* -------------------------------------------------------------------- */
    hTransformArg =
        GDALCreateApproxTransformer( GDALGenImgProjTransform,
                                     hTransformArg, 0.125 );
    GDALApproxTransformerOwnsSubtransformer(hTransformArg, TRUE);

/* -------------------------------------------------------------------- */
/*      Setup warp options.                                             */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALCreateWarpOptions();

    psWO->papszWarpOptions = CSLSetNameValue(NULL, "OPTIMIZE_SIZE", "YES");
    psWO->eWorkingDataType = GDT_Byte;
    psWO->eResampleAlg = eResampleAlg;

    psWO->hSrcDS = poSrcDS;
    psWO->hDstDS = poDS;

    psWO->pfnTransformer = GDALApproxTransform;
    psWO->pTransformerArg = hTransformArg;

    psWO->pfnProgress = pfnProgress;
    psWO->pProgressArg = pProgressData;

/* -------------------------------------------------------------------- */
/*      Setup band mapping.                                             */
/* -------------------------------------------------------------------- */

    if( nBands == 2 || nBands == 4 )
        psWO->nBandCount = nBands - 1;
    else
        psWO->nBandCount = nBands;

    psWO->panSrcBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));
    psWO->panDstBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));

    for( int i = 0; i < psWO->nBandCount; i++ )
    {
        psWO->panSrcBands[i] = i+1;
        psWO->panDstBands[i] = i+1;
    }

    if( nBands == 2 || nBands == 4 )
    {
        psWO->nSrcAlphaBand = nBands;
    }
    if( nTargetBands == 2 || nTargetBands == 4 )
    {
        psWO->nDstAlphaBand = nTargetBands;
    }

/* -------------------------------------------------------------------- */
/*      Initialize and execute the warp.                                */
/* -------------------------------------------------------------------- */
    GDALWarpOperation oWO;

    CPLErr eErr = oWO.Initialize( psWO );
    if( eErr == CE_None )
    {
        /*if( bMulti )
            eErr = oWO.ChunkAndWarpMulti( 0, 0, nXSize, nYSize );
        else*/
        eErr = oWO.ChunkAndWarpImage( 0, 0, nXSize, nYSize );
    }
    if (eErr != CE_None)
    {
        delete poDS;
        poDS = NULL;
    }

    GDALDestroyTransformer( hTransformArg );
    GDALDestroyWarpOptions( psWO );

    return poDS;
}

/************************************************************************/
/*                        ParseCompressionOptions()                     */
/************************************************************************/

void GDALGeoPackageDataset::ParseCompressionOptions(char** papszOptions)
{
    const char* pszZLevel = CSLFetchNameValue(papszOptions, "ZLEVEL");
    if( pszZLevel )
        m_nZLevel = atoi(pszZLevel);

    const char* pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    if( pszQuality )
        m_nQuality = atoi(pszQuality);

    const char* pszDither = CSLFetchNameValue(papszOptions, "DITHER");
    if( pszDither )
        m_bDither = CPLTestBool(pszDither);
}

/************************************************************************/
/*                          RegisterWebPExtension()                     */
/************************************************************************/

bool GDALGeoPackageDataset::RegisterWebPExtension()
{
    CreateExtensionsTableIfNecessary();

    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "('%q', 'tile_data', 'gpkg_webp', 'GeoPackage 1.0 Specification Annex P', 'read-write')",
        m_osRasterTable.c_str());
    const OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);

    return OGRERR_NONE == eErr;
}

/************************************************************************/
/*                       RegisterZoomOtherExtension()                   */
/************************************************************************/

bool GDALGeoPackageDataset::RegisterZoomOtherExtension()
{
    CreateExtensionsTableIfNecessary();

    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name, extension_name, definition, scope) "
        "VALUES "
        "('%q', 'gpkg_zoom_other', 'GeoPackage 1.0 Specification Annex O', 'read-write')",
        m_osRasterTable.c_str());
    const OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    return OGRERR_NONE == eErr;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer* GDALGeoPackageDataset::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return NULL;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                          ICreateLayer()                              */
/* Options:                                                             */
/*   FID = primary key name                                             */
/*   OVERWRITE = YES|NO, overwrite existing layer?                      */
/*   SPATIAL_INDEX = YES|NO, TBD                                        */
/************************************************************************/

OGRLayer* GDALGeoPackageDataset::ICreateLayer( const char * pszLayerName,
                                      OGRSpatialReference * poSpatialRef,
                                      OGRwkbGeometryType eGType,
                                      char **papszOptions )
{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  m_pszFilename, pszLayerName );

        return NULL;
    }

    // Check identifier unicity
    const char* pszIdentifier = CSLFetchNameValue(papszOptions, "IDENTIFIER");
    if( pszIdentifier != NULL && pszIdentifier[0] == '\0' )
        pszIdentifier = NULL;
    if( pszIdentifier != NULL )
    {
        for(int i = 0; i < m_nLayers; ++i )
        {
            const char* pszOtherIdentifier =
                m_papoLayers[i]->GetMetadataItem("IDENTIFIER");
            if( pszOtherIdentifier == NULL )
                pszOtherIdentifier = m_papoLayers[i]->GetName();
            if( pszOtherIdentifier != NULL &&
                EQUAL(pszOtherIdentifier, pszIdentifier) &&
                !EQUAL(m_papoLayers[i]->GetName(), pszLayerName) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                     "Identifier %s is already used by table %s",
                     pszIdentifier, m_papoLayers[i]->GetName());
                return NULL;
            }
        }

        // In case there would be table in gpkg_contents not listed as a
        // vector layer
        char* pszSQL = sqlite3_mprintf(
            "SELECT table_name FROM gpkg_contents WHERE identifier = '%q'",
            pszIdentifier);
        SQLResult oResult;
        OGRErr eErr = SQLQuery (hDB, pszSQL, &oResult);
        sqlite3_free(pszSQL);
        if( eErr == OGRERR_NONE && oResult.nRowCount > 0 &&
            SQLResultGetValue(&oResult, 0, 0) != NULL &&
            !EQUAL(SQLResultGetValue(&oResult, 0, 0), pszLayerName) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Identifier %s is already used by table %s",
                     pszIdentifier, SQLResultGetValue(&oResult, 0, 0));
            SQLResultFree(&oResult);
            return NULL;
        }
        SQLResultFree(&oResult);
    }

    /* Read GEOMETRY_NAME option */
    const char* pszGeomColumnName = CSLFetchNameValue(papszOptions, "GEOMETRY_NAME");
    if (pszGeomColumnName == NULL) /* deprecated name */
        pszGeomColumnName = CSLFetchNameValue(papszOptions, "GEOMETRY_COLUMN");
    if (pszGeomColumnName == NULL)
        pszGeomColumnName = "geom";
    const bool bGeomNullable =
        CPLFetchBool(papszOptions, "GEOMETRY_NULLABLE", true);

    /* Read FID option */
    const char* pszFIDColumnName = CSLFetchNameValue(papszOptions, "FID");
    if (pszFIDColumnName == NULL)
        pszFIDColumnName = "fid";

    if ( strspn(pszFIDColumnName, "`~!@#$%^&*()+-={}|[]\\:\";'<>?,./") > 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The primary key (%s) name may not contain special characters or spaces",
                 pszFIDColumnName);
        return NULL;
    }

    /* Avoiding gpkg prefixes is not an official requirement, but seems wise */
    if (STARTS_WITH(pszLayerName, "gpkg"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The layer name may not begin with 'gpkg' as it is a reserved geopackage prefix");
        return NULL;
    }

    /* Preemptively try and avoid sqlite3 syntax errors due to  */
    /* illegal characters. */
    if ( strspn(pszLayerName, "`~!@#$%^&*()+-={}|[]\\:\";'<>?,./") > 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The layer name may not contain special characters or spaces");
        return NULL;
    }

    /* Check for any existing layers that already use this name */
    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName, m_papoLayers[iLayer]->GetName()) )
        {
            const char *pszOverwrite = CSLFetchNameValue(papszOptions,"OVERWRITE");
            if( pszOverwrite != NULL && CPLTestBool(pszOverwrite) )
            {
                DeleteLayer( iLayer );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );
                return NULL;
            }
        }
    }

    /* Create a blank layer. */
    OGRGeoPackageTableLayer *poLayer = new OGRGeoPackageTableLayer(this, pszLayerName);

    poLayer->SetCreationParameters( eGType, pszGeomColumnName,
                                    bGeomNullable,
                                    poSpatialRef,
                                    pszFIDColumnName,
                                    pszIdentifier,
                                    CSLFetchNameValue(papszOptions, "DESCRIPTION") );

    /* Should we create a spatial index ? */
    const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    int bCreateSpatialIndex = ( pszSI == NULL || CPLTestBool(pszSI) );
    if( eGType != wkbNone && bCreateSpatialIndex )
    {
        poLayer->SetDeferredSpatialIndexCreation(true);
    }

    poLayer->SetPrecisionFlag( CPLFetchBool(papszOptions, "PRECISION", true) );
    poLayer->SetTruncateFieldsFlag(
        CPLFetchBool(papszOptions, "TRUNCATE_FIELDS", false));
    if( eGType == wkbNone )
    {
        poLayer->SetRegisterAsAspatial( CPLFetchBool(
            (const char**)papszOptions,"REGISTER_AS_ASPATIAL",true) );
    }

    m_papoLayers = (OGRGeoPackageTableLayer**)CPLRealloc(m_papoLayers,  sizeof(OGRGeoPackageTableLayer*) * (m_nLayers+1));
    m_papoLayers[m_nLayers++] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                          FindLayerIndex()                            */
/************************************************************************/

int GDALGeoPackageDataset::FindLayerIndex( const char *pszLayerName )

{
    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,m_papoLayers[iLayer]->GetName()) )
            return iLayer;
    }
    return -1;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr GDALGeoPackageDataset::DeleteLayer( int iLayer )
{
    if( !bUpdate || iLayer < 0 || iLayer >= m_nLayers )
        return OGRERR_FAILURE;

    m_papoLayers[iLayer]->ResetReading();
    m_papoLayers[iLayer]->RunDeferredCreationIfNecessary();
    m_papoLayers[iLayer]->CreateSpatialIndexIfNecessary();

    CPLString osLayerName = m_papoLayers[iLayer]->GetName();

    CPLDebug( "GPKG", "DeleteLayer(%s)", osLayerName.c_str() );

    if( SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( m_papoLayers[iLayer]->HasSpatialIndex() )
        m_papoLayers[iLayer]->DropSpatialIndex();

    char* pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_geometry_columns WHERE table_name = '%q'",
             osLayerName.c_str());
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);

    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf(
                "DELETE FROM gpkg_contents WHERE table_name = '%q'",
                osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE && HasExtensionsTable() )
    {
        pszSQL = sqlite3_mprintf(
                "DELETE FROM gpkg_extensions WHERE table_name = '%q'",
                osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE && HasMetadataTables() )
    {
        pszSQL = sqlite3_mprintf(
                "DELETE FROM gpkg_metadata_reference WHERE table_name = '%q'",
                osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE && HasDataColumnsTable() )
    {
        pszSQL = sqlite3_mprintf(
                "DELETE FROM gpkg_data_columns WHERE table_name = '%q'",
                osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf("DROP TABLE \"%w\"", osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    // Check foreign key integrity
    if ( eErr == OGRERR_NONE )
    {
        eErr = PragmaCheck("foreign_key_check", "", 0);
    }

    if( eErr == OGRERR_NONE )
    {
        eErr = SoftCommitTransaction();
        if( eErr == OGRERR_NONE )
        {
            /* Delete the layer object and remove the gap in the layers list */
            delete m_papoLayers[iLayer];
            memmove( m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
                    sizeof(void *) * (m_nLayers - iLayer - 1) );
            m_nLayers--;
        }
    }
    else
    {
        SoftRollbackTransaction();
    }

    return eErr;
}

/************************************************************************/
/*                       TestCapability()                               */
/************************************************************************/

int GDALGeoPackageDataset::TestCapability( const char * pszCap )
{
    if ( EQUAL(pszCap,ODsCCreateLayer) ||
         EQUAL(pszCap,ODsCDeleteLayer) ||
         EQUAL(pszCap,"RenameLayer") )
    {
         return bUpdate;
    }
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bUpdate;

    return OGRSQLiteBaseDataSource::TestCapability(pszCap);
}

/************************************************************************/
/*                       ResetReadingAllLayers()                        */
/************************************************************************/

void GDALGeoPackageDataset::ResetReadingAllLayers()
{
    for( int i = 0; i < m_nLayers; i++ )
    {
        m_papoLayers[i]->ResetReading();
    }
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

static const char* const apszFuncsWithSideEffects[] =
{
    "CreateSpatialIndex",
    "DisableSpatialIndex",
    "HasSpatialIndex",
};

OGRLayer * GDALGeoPackageDataset::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
    m_bHasReadMetadataFromStorage = false;

    FlushMetadata();
    for( int i = 0; i < m_nLayers; i++ )
    {
        m_papoLayers[i]->RunDeferredCreationIfNecessary();
        m_papoLayers[i]->CreateSpatialIndexIfNecessary();
    }

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + strlen("DELLAYER:");

        while( *pszLayerName == ' ' )
            pszLayerName++;

        int idx = FindLayerIndex( pszLayerName );
        if( idx >= 0 )
            DeleteLayer( idx );
        else
            CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer: %s",
                     pszLayerName);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case RECOMPUTE EXTENT ON command.                       */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "RECOMPUTE EXTENT ON ") )
    {
        const char *pszLayerName = pszSQLCommand + strlen("RECOMPUTE EXTENT ON ");

        while( *pszLayerName == ' ' )
            pszLayerName++;

        int idx = FindLayerIndex( pszLayerName );
        if( idx >= 0 )
        {
            m_papoLayers[idx]->RecomputeExtent();
        }
        else
            CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer: %s",
                     pszLayerName);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Intercept DROP TABLE                                            */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DROP TABLE ") )
    {
        const char *pszLayerName = pszSQLCommand + strlen("DROP TABLE ");

        while( *pszLayerName == ' ' )
            pszLayerName++;

        int idx = FindLayerIndex( SQLUnescapeDoubleQuote(pszLayerName) );
        if( idx >= 0 )
        {
            DeleteLayer( idx );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Intercept ALTER TABLE ... RENAME TO                             */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "ALTER TABLE ") )
    {
        char **papszTokens = SQLTokenize( pszSQLCommand );
        /* ALTER TABLE src_table RENAME TO dst_table */
        if( CSLCount(papszTokens) == 6 && EQUAL(papszTokens[3], "RENAME") &&
            EQUAL(papszTokens[4], "TO") )
        {
            const char* pszSrcTableName = papszTokens[2];
            const char* pszDstTableName = papszTokens[5];
            OGRGeoPackageTableLayer* poSrcLayer =
                (OGRGeoPackageTableLayer*)GetLayerByName(
                        SQLUnescapeDoubleQuote(pszSrcTableName));
            if( poSrcLayer )
            {
                poSrcLayer->RenameTo( SQLUnescapeDoubleQuote(pszDstTableName) );
                CSLDestroy(papszTokens);
                return NULL;
            }
        }
        CSLDestroy(papszTokens);
    }

    if( EQUAL(pszSQLCommand, "VACUUM") )
    {
        ResetReadingAllLayers();
    }

    if( EQUAL(pszSQLCommand, "BEGIN") )
    {
        SoftStartTransaction();
        return NULL;
    }
    else if( EQUAL(pszSQLCommand, "COMMIT") )
    {
        SoftCommitTransaction();
        return NULL;
    }
    else if( EQUAL(pszSQLCommand, "ROLLBACK") )
    {
        SoftRollbackTransaction();
        return NULL;
    }

    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return GDALDataset::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );
    else if( pszDialect != NULL && EQUAL(pszDialect,"INDIRECT_SQLITE") )
        return GDALDataset::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          "SQLITE" );

/* -------------------------------------------------------------------- */
/*      Prepare statement.                                              */
/* -------------------------------------------------------------------- */
    sqlite3_stmt *hSQLStmt = NULL;

    CPLString osSQLCommand = pszSQLCommand;

    /* This will speed-up layer creation */
    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer definition. */
    bool bUseStatementForGetNextFeature = true;
    bool bEmptyLayer = false;

    if( osSQLCommand.ifind("SELECT ") == 0 &&
        osSQLCommand.ifind(" UNION ") == std::string::npos &&
        osSQLCommand.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCommand.ifind(" EXCEPT ") == std::string::npos )
    {
        size_t nOrderByPos = osSQLCommand.ifind(" ORDER BY ");
        if( nOrderByPos != std::string::npos )
        {
            osSQLCommand.resize(nOrderByPos);
            bUseStatementForGetNextFeature = false;
        }
    }

    int rc = sqlite3_prepare( hDB, osSQLCommand.c_str(),
                              static_cast<int>(osSQLCommand.size()),
                              &hSQLStmt, NULL );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "In ExecuteSQL(): sqlite3_prepare(%s):\n  %s",
                pszSQLCommand, sqlite3_errmsg(hDB) );

        if( hSQLStmt != NULL )
        {
            sqlite3_finalize( hSQLStmt );
        }

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we get a resultset?                                          */
/* -------------------------------------------------------------------- */
    rc = sqlite3_step( hSQLStmt );
    if( rc != SQLITE_ROW )
    {
        if ( rc != SQLITE_DONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                  "In ExecuteSQL(): sqlite3_step(%s):\n  %s",
                  pszSQLCommand, sqlite3_errmsg(hDB) );

            sqlite3_finalize( hSQLStmt );
            return NULL;
        }

        if( EQUAL(pszSQLCommand, "VACUUM") )
        {
            sqlite3_finalize( hSQLStmt );
            /* VACUUM rewrites the DB, so we need to reset the application id */
            SetApplicationId();
            return NULL;
        }

        if( !STARTS_WITH_CI(pszSQLCommand, "SELECT ") )
        {
            sqlite3_finalize( hSQLStmt );
            return NULL;
        }

        bUseStatementForGetNextFeature = false;
        bEmptyLayer = true;
    }

/* -------------------------------------------------------------------- */
/*      Special case for some functions which must be run               */
/*      only once                                                       */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") )
    {
        for( unsigned int i=0; i < sizeof(apszFuncsWithSideEffects) /
               sizeof(apszFuncsWithSideEffects[0]); i++ )
        {
            if( EQUALN(apszFuncsWithSideEffects[i], pszSQLCommand + 7,
                       strlen(apszFuncsWithSideEffects[i])) )
            {
                if (sqlite3_column_count( hSQLStmt ) == 1 &&
                    sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_INTEGER )
                {
                    int ret = sqlite3_column_int( hSQLStmt, 0 );

                    sqlite3_finalize( hSQLStmt );

                    return new OGRSQLiteSingleFeatureLayer
                                        ( apszFuncsWithSideEffects[i], ret );
                }
            }
        }
    }
    else if( STARTS_WITH_CI(pszSQLCommand, "PRAGMA ") )
    {
        if (sqlite3_column_count( hSQLStmt ) == 1 &&
            sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_INTEGER )
        {
            int ret = sqlite3_column_int( hSQLStmt, 0 );

            sqlite3_finalize( hSQLStmt );

            return new OGRSQLiteSingleFeatureLayer
                                ( pszSQLCommand + 7, ret );
        }
        else if (sqlite3_column_count( hSQLStmt ) == 1 &&
                 sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_TEXT )
        {
            const char* pszRet = (const char*) sqlite3_column_text( hSQLStmt, 0 );

            OGRLayer* poRet = new OGRSQLiteSingleFeatureLayer
                                ( pszSQLCommand + 7, pszRet );

            sqlite3_finalize( hSQLStmt );

            return poRet;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = NULL;

    CPLString osSQL = pszSQLCommand;
    poLayer = new OGRGeoPackageSelectLayer(
        this, osSQL, hSQLStmt,
        bUseStatementForGetNextFeature, bEmptyLayer );

    if( poSpatialFilter != NULL && poLayer->GetLayerDefn()->GetGeomFieldCount() > 0 )
        poLayer->SetSpatialFilter( 0, poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void GDALGeoPackageDataset::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                         HasExtensionsTable()                         */
/************************************************************************/

bool GDALGeoPackageDataset::HasExtensionsTable()
{
    SQLResult oResultTable;
    OGRErr err = SQLQuery(hDB,
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions' "
        "AND type IN ('table', 'view')", &oResultTable);
    bool bHasExtensionsTable = ( err == OGRERR_NONE && oResultTable.nRowCount == 1 );
    SQLResultFree(&oResultTable);
    return bHasExtensionsTable;
}

/************************************************************************/
/*                    CheckUnknownExtensions()                          */
/************************************************************************/

void GDALGeoPackageDataset::CheckUnknownExtensions(bool bCheckRasterTable)
{
    if( !HasExtensionsTable() )
        return;

    char* pszSQL = NULL;
    if( !bCheckRasterTable)
        pszSQL = sqlite3_mprintf(
            "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE (table_name IS NULL AND extension_name != 'gdal_aspatial')"
#ifdef WORKAROUND_SQLITE3_BUGS
            " OR 0"
#endif
        );
    else
        pszSQL = sqlite3_mprintf(
            "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE table_name = '%q'"
#ifdef WORKAROUND_SQLITE3_BUGS
            " OR 0"
#endif
            ,
            m_osRasterTable.c_str());

    SQLResult oResultTable;
    OGRErr err = SQLQuery(GetDB(), pszSQL, &oResultTable);
    sqlite3_free(pszSQL);
    if ( err == OGRERR_NONE && oResultTable.nRowCount > 0 )
    {
        for(int i=0; i<oResultTable.nRowCount;i++)
        {
            const char* pszExtName = SQLResultGetValue(&oResultTable, 0, i);
            const char* pszDefinition = SQLResultGetValue(&oResultTable, 1, i);
            const char* pszScope = SQLResultGetValue(&oResultTable, 2, i);
            if( pszExtName == NULL ) pszExtName = "(null)";
            if( pszDefinition == NULL ) pszDefinition = "(null)";
            if( pszScope == NULL ) pszScope = "(null)";

            if( EQUAL(pszExtName, "gpkg_webp") )
            {
                if( GDALGetDriverByName("WEBP") == NULL )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Table %s contains WEBP tiles, but GDAL configured "
                             "without WEBP support. Data will be missing",
                             m_osRasterTable.c_str());
                }
                m_eTF = GPKG_TF_WEBP;
                continue;
            }
            if( EQUAL(pszExtName, "gpkg_zoom_other") )
            {
                m_bZoomOther = true;
                continue;
            }

            if( GetUpdate() && EQUAL(pszScope, "write-only") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented for safe write-support, but is not currently. "
                         "Update of that database are strongly discouraged to avoid corruption.",
                         pszExtName, pszDefinition);
            }
            else if( GetUpdate() && EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented in order to read/write it safely, but is not currently. "
                         "Some data may be missing while reading that database, and updates are strongly discouraged.",
                         pszExtName, pszDefinition);
            }
            else if( EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented in order to read it safely, but is not currently. "
                         "Some data may be missing while reading that database.",
                         pszExtName, pszDefinition);
            }
        }
    }
    SQLResultFree(&oResultTable);
}

/************************************************************************/
/*                         HasGDALAspatialExtension()                       */
/************************************************************************/

bool GDALGeoPackageDataset::HasGDALAspatialExtension()
{
    if (!HasExtensionsTable())
        return false;

    SQLResult oResultTable;
    OGRErr err = SQLQuery(hDB,
        "SELECT * FROM gpkg_extensions "
        "WHERE (extension_name = 'gdal_aspatial' "
        "AND table_name IS NULL "
        "AND column_name IS NULL)"
#ifdef WORKAROUND_SQLITE3_BUGS
        " OR 0"
#endif
        , &oResultTable);
    bool bHasExtension = ( err == OGRERR_NONE && oResultTable.nRowCount == 1 );
    SQLResultFree(&oResultTable);
    return bHasExtension;
}

/************************************************************************/
/*                  CreateGDALAspatialExtension()                       */
/************************************************************************/

OGRErr GDALGeoPackageDataset::CreateGDALAspatialExtension()
{
    CreateExtensionsTableIfNecessary();

    if( HasGDALAspatialExtension() )
        return OGRERR_NONE;

    const char* pszCreateAspatialExtension =
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "(NULL, NULL, 'gdal_aspatial', 'http://gdal.org/geopackage_aspatial.html', 'read-write')";

    return SQLCommand(hDB, pszCreateAspatialExtension);
}

/************************************************************************/
/*                  CreateExtensionsTableIfNecessary()                  */
/************************************************************************/

OGRErr GDALGeoPackageDataset::CreateExtensionsTableIfNecessary()
{
    /* Check if the table gpkg_extensions exists */
    if( HasExtensionsTable() )
        return OGRERR_NONE;

    /* Requirement 79 : Every extension of a GeoPackage SHALL be registered */
    /* in a corresponding row in the gpkg_extensions table. The absence of a */
    /* gpkg_extensions table or the absence of rows in gpkg_extensions table */
    /* SHALL both indicate the absence of extensions to a GeoPackage. */
    const char* pszCreateGpkgExtensions =
        "CREATE TABLE gpkg_extensions ("
        "table_name TEXT,"
        "column_name TEXT,"
        "extension_name TEXT NOT NULL,"
        "definition TEXT NOT NULL,"
        "scope TEXT NOT NULL,"
        "CONSTRAINT ge_tce UNIQUE (table_name, column_name, extension_name)"
        ")";

    return SQLCommand(hDB, pszCreateGpkgExtensions);
}

/************************************************************************/
/*                         HasDataColumnsTable()                        */
/************************************************************************/

bool GDALGeoPackageDataset::HasDataColumnsTable()
{
    SQLResult oResultTable;
    OGRErr err = SQLQuery(hDB,
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_data_columns' "
        "AND type IN ('table', 'view')", &oResultTable);
    bool bHasExtensionsTable = ( err == OGRERR_NONE && oResultTable.nRowCount == 1 );
    SQLResultFree(&oResultTable);
    return bHasExtensionsTable;
}

/************************************************************************/
/*                     OGRGeoPackageGetHeader()                         */
/************************************************************************/

static bool OGRGeoPackageGetHeader( sqlite3_context* pContext,
                                    CPL_UNUSED int argc,
                                    sqlite3_value** argv,
                                    GPkgHeader* psHeader,
                                    bool bNeedExtent )
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null(pContext);
        return false;
    }
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    const GByte* pabyBLOB = (const GByte *) sqlite3_value_blob (argv[0]);
    if( nBLOBLen < 8 ||
        GPkgHeaderFromWKB(pabyBLOB, nBLOBLen, psHeader) != OGRERR_NONE )
    {
        sqlite3_result_null(pContext);
        return false;
    }
    if( !(psHeader->bExtentHasXY) && bNeedExtent )
    {
        OGRGeometry *poGeom = GPkgGeometryToOGR(pabyBLOB, nBLOBLen, NULL);
        if( poGeom == NULL || poGeom->IsEmpty() )
        {
            sqlite3_result_null(pContext);
            delete poGeom;
            return false;
        }
        OGREnvelope sEnvelope;
        poGeom->getEnvelope(&sEnvelope);
        psHeader->MinX = sEnvelope.MinX;
        psHeader->MaxX = sEnvelope.MaxX;
        psHeader->MinY = sEnvelope.MinY;
        psHeader->MaxY = sEnvelope.MaxY;
        delete poGeom;
    }
    return true;
}

/************************************************************************/
/*                      OGRGeoPackageSTMinX()                           */
/************************************************************************/

static
void OGRGeoPackageSTMinX(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true) )
        return;
    sqlite3_result_double( pContext, sHeader.MinX );
}

/************************************************************************/
/*                      OGRGeoPackageSTMinY()                           */
/************************************************************************/

static
void OGRGeoPackageSTMinY(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true) )
        return;
    sqlite3_result_double( pContext, sHeader.MinY );
}

/************************************************************************/
/*                      OGRGeoPackageSTMaxX()                           */
/************************************************************************/

static
void OGRGeoPackageSTMaxX(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true) )
        return;
    sqlite3_result_double( pContext, sHeader.MaxX );
}

/************************************************************************/
/*                      OGRGeoPackageSTMaxY()                           */
/************************************************************************/

static
void OGRGeoPackageSTMaxY(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true) )
        return;
    sqlite3_result_double( pContext, sHeader.MaxY );
}

/************************************************************************/
/*                     OGRGeoPackageSTIsEmpty()                         */
/************************************************************************/

static
void OGRGeoPackageSTIsEmpty(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false) )
        return;
    sqlite3_result_int( pContext, sHeader.bEmpty );
}

/************************************************************************/
/*                    OGRGeoPackageSTGeometryType()                     */
/************************************************************************/

static
void OGRGeoPackageSTGeometryType(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false) )
        return;

    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    const GByte* pabyBLOB = (const GByte *) sqlite3_value_blob (argv[0]);
    OGRwkbGeometryType eGeometryType;
    if( nBLOBLen <= (int)sHeader.szHeader )
    {
        sqlite3_result_null( pContext );
        return;
    }
    OGRErr err = OGRReadWKBGeometryType( (GByte*)pabyBLOB + sHeader.szHeader,
                                         wkbVariantIso, &eGeometryType );
    if( err != OGRERR_NONE )
        sqlite3_result_null( pContext );
    else
        sqlite3_result_text( pContext, OGRToOGCGeomType(eGeometryType), -1, SQLITE_TRANSIENT );
}

/************************************************************************/
/*                    OGRGeoPackageGPKGIsAssignable()                   */
/************************************************************************/

static
void OGRGeoPackageGPKGIsAssignable(sqlite3_context* pContext,
                                   CPL_UNUSED int argc,
                                   sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_int( pContext, 0 );
        return;
    }

    const char* pszExpected = (const char*)sqlite3_value_text(argv[0]);
    const char* pszActual = (const char*)sqlite3_value_text(argv[1]);
    int bIsAssignable = OGR_GT_IsSubClassOf( OGRFromOGCGeomType(pszActual),
                                             OGRFromOGCGeomType(pszExpected) );
    sqlite3_result_int( pContext, bIsAssignable );
}

/************************************************************************/
/*                     OGRGeoPackageSTSRID()                            */
/************************************************************************/

static
void OGRGeoPackageSTSRID(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false) )
        return;
    sqlite3_result_int( pContext, sHeader.iSrsId );
}

/************************************************************************/
/*                  OGRGeoPackageCreateSpatialIndex()                   */
/************************************************************************/

static
void OGRGeoPackageCreateSpatialIndex(sqlite3_context* pContext,
                                     CPL_UNUSED int argc,
                                     sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_int( pContext, 0 );
        return;
    }

    const char* pszTableName = (const char*)sqlite3_value_text(argv[0]);
    const char* pszGeomName = (const char*)sqlite3_value_text(argv[1]);
    GDALGeoPackageDataset* poDS = (GDALGeoPackageDataset* )sqlite3_user_data(pContext);

    OGRGeoPackageTableLayer* poLyr = (OGRGeoPackageTableLayer*)poDS->GetLayerByName(pszTableName);
    if( poLyr == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int( pContext, 0 );
        return;
    }
    if( !EQUAL(poLyr->GetGeometryColumn(), pszGeomName) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int( pContext, 0 );
        return;
    }

    sqlite3_result_int( pContext, poLyr->CreateSpatialIndex() );
}

/************************************************************************/
/*                  OGRGeoPackageDisableSpatialIndex()                  */
/************************************************************************/

static
void OGRGeoPackageDisableSpatialIndex(sqlite3_context* pContext,
                                      CPL_UNUSED int argc,
                                      sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_int( pContext, 0 );
        return;
    }

    const char* pszTableName = (const char*)sqlite3_value_text(argv[0]);
    const char* pszGeomName = (const char*)sqlite3_value_text(argv[1]);
    GDALGeoPackageDataset* poDS = (GDALGeoPackageDataset* )sqlite3_user_data(pContext);

    OGRGeoPackageTableLayer* poLyr = (OGRGeoPackageTableLayer*)poDS->GetLayerByName(pszTableName);
    if( poLyr == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int( pContext, 0 );
        return;
    }
    if( !EQUAL(poLyr->GetGeometryColumn(), pszGeomName) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int( pContext, 0 );
        return;
    }

    sqlite3_result_int( pContext, poLyr->DropSpatialIndex(true) );
}

/************************************************************************/
/*                  OGRGeoPackageHasSpatialIndex()                      */
/************************************************************************/

static
void OGRGeoPackageHasSpatialIndex(sqlite3_context* pContext,
                                  CPL_UNUSED int argc,
                                  sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_int( pContext, 0 );
        return;
    }

    const char* pszTableName = (const char*)sqlite3_value_text(argv[0]);
    const char* pszGeomName = (const char*)sqlite3_value_text(argv[1]);
    GDALGeoPackageDataset* poDS = (GDALGeoPackageDataset* )sqlite3_user_data(pContext);

    OGRGeoPackageTableLayer* poLyr = (OGRGeoPackageTableLayer*)poDS->GetLayerByName(pszTableName);
    if( poLyr == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int( pContext, 0 );
        return;
    }
    if( !EQUAL(poLyr->GetGeometryColumn(), pszGeomName) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int( pContext, 0 );
        return;
    }

    poLyr->RunDeferredCreationIfNecessary();
    poLyr->CreateSpatialIndexIfNecessary();

    sqlite3_result_int( pContext, poLyr->HasSpatialIndex() );
}

/************************************************************************/
/*                       GPKG_hstore_get_value()                        */
/************************************************************************/

static
void GPKG_hstore_get_value(sqlite3_context* pContext,
                           CPL_UNUSED int argc,
                           sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_null (pContext);
        return;
    }

    const char* pszHStore = (const char*)sqlite3_value_text(argv[0]);
    const char* pszSearchedKey = (const char*)sqlite3_value_text(argv[1]);
    char* pszValue = OGRHStoreGetValue(pszHStore, pszSearchedKey);
    if( pszValue != NULL )
        sqlite3_result_text( pContext, pszValue, -1, CPLFree );
    else
        sqlite3_result_null( pContext );
}

/************************************************************************/
/*                      GPKG_GDAL_GetMemFileFromBlob()                  */
/************************************************************************/

static CPLString GPKG_GDAL_GetMemFileFromBlob(sqlite3_value** argv)
{
    int nBytes = sqlite3_value_bytes (argv[0]);
    const GByte* pabyBLOB = (const GByte *) sqlite3_value_blob (argv[0]);
    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/GPKG_GDAL_GetMemFileFromBlob_%p", argv);
    VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(),
                                          (GByte*)pabyBLOB,
                                          nBytes, FALSE);
    VSIFCloseL(fp);
    return osMemFileName;
}

/************************************************************************/
/*                       GPKG_GDAL_GetMimeType()                        */
/************************************************************************/

static
void GPKG_GDAL_GetMimeType(sqlite3_context* pContext,
                           CPL_UNUSED int argc,
                           sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    GDALDriver* poDriver = (GDALDriver*)GDALIdentifyDriver(osMemFileName, NULL);
    if( poDriver != NULL )
    {
        const char* pszRes = NULL;
        if( EQUAL(poDriver->GetDescription(), "PNG") )
            pszRes = "image/png";
        else if( EQUAL(poDriver->GetDescription(), "JPEG") )
            pszRes = "image/jpeg";
        else if( EQUAL(poDriver->GetDescription(), "WEBP") )
            pszRes = "image/x-webp";
        else
            pszRes = CPLSPrintf("gdal/%s", poDriver->GetDescription());
        sqlite3_result_text( pContext, pszRes, -1, SQLITE_TRANSIENT );
    }
    else
        sqlite3_result_null (pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                       GPKG_GDAL_GetBandCount()                       */
/************************************************************************/

static
void GPKG_GDAL_GetBandCount(sqlite3_context* pContext,
                           CPL_UNUSED int argc,
                           sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    GDALDataset* poDS = (GDALDataset*) GDALOpenEx(osMemFileName,
                                                  GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                                  NULL, NULL, NULL);
    if( poDS != NULL )
    {
        sqlite3_result_int( pContext, poDS->GetRasterCount() );
        GDALClose( poDS );
    }
    else
        sqlite3_result_null (pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                       GPKG_GDAL_HasColorTable()                      */
/************************************************************************/

static
void GPKG_GDAL_HasColorTable(sqlite3_context* pContext,
                             CPL_UNUSED int argc,
                             sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    GDALDataset* poDS = (GDALDataset*) GDALOpenEx(osMemFileName,
                                                  GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                                  NULL, NULL, NULL);
    if( poDS != NULL )
    {
        sqlite3_result_int( pContext,
                            poDS->GetRasterCount() == 1 &&
                            poDS->GetRasterBand(1)->GetColorTable() != NULL );
        GDALClose( poDS );
    }
    else
        sqlite3_result_null (pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                         OpenOrCreateDB()                             */
/************************************************************************/

bool GDALGeoPackageDataset::OpenOrCreateDB(int flags)
{
    const bool bSuccess =
        CPL_TO_BOOL(OGRSQLiteBaseDataSource::OpenOrCreateDB(flags, FALSE));
    if( !bSuccess )
        return false;

#ifdef SPATIALITE_412_OR_LATER
    InitNewSpatialite();

    // Enable SpatiaLite 4.3 "amphibious" mode, i.e. that SpatiaLite functions
    // that take geometries will accept GPKG encoded geometries without
    // explicit conversion.
    sqlite3_exec(hDB, "SELECT EnableGpkgAmphibiousMode()", NULL, NULL, NULL);
#endif

    /* Used by RTree Spatial Index Extension */
    sqlite3_create_function(hDB, "ST_MinX", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMinX, NULL, NULL);
    sqlite3_create_function(hDB, "ST_MinY", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMinY, NULL, NULL);
    sqlite3_create_function(hDB, "ST_MaxX", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMaxX, NULL, NULL);
    sqlite3_create_function(hDB, "ST_MaxY", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMaxY, NULL, NULL);
    sqlite3_create_function(hDB, "ST_IsEmpty", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTIsEmpty, NULL, NULL);

    /* Used by Geometry Type Triggers Extension */
    sqlite3_create_function(hDB, "ST_GeometryType", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTGeometryType, NULL, NULL);
    sqlite3_create_function(hDB, "GPKG_IsAssignable", 2, SQLITE_ANY, NULL,
                            OGRGeoPackageGPKGIsAssignable, NULL, NULL);

    /* Used by Geometry SRS ID Triggers Extension */
    sqlite3_create_function(hDB, "ST_SRID", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTSRID, NULL, NULL);

    /* Spatialite-like functions */
    sqlite3_create_function(hDB, "CreateSpatialIndex", 2, SQLITE_ANY, this,
                            OGRGeoPackageCreateSpatialIndex, NULL, NULL);
    sqlite3_create_function(hDB, "DisableSpatialIndex", 2, SQLITE_ANY, this,
                            OGRGeoPackageDisableSpatialIndex, NULL, NULL);
    sqlite3_create_function(hDB, "HasSpatialIndex", 2, SQLITE_ANY, this,
                            OGRGeoPackageHasSpatialIndex, NULL, NULL);

    // HSTORE functions
    sqlite3_create_function(hDB, "hstore_get_value", 2, SQLITE_ANY, NULL,
                            GPKG_hstore_get_value, NULL, NULL);

    // Debug functions
    if( CPLTestBool(CPLGetConfigOption("GPKG_DEBUG", "FALSE")) )
    {
        sqlite3_create_function(hDB, "GDAL_GetMimeType", 1, SQLITE_ANY, NULL,
                                GPKG_GDAL_GetMimeType, NULL, NULL);
        sqlite3_create_function(hDB, "GDAL_GetBandCount", 1, SQLITE_ANY, NULL,
                                GPKG_GDAL_GetBandCount, NULL, NULL);
        sqlite3_create_function(hDB, "GDAL_HasColorTable", 1, SQLITE_ANY, NULL,
                                GPKG_GDAL_HasColorTable, NULL, NULL);
    }

    return true;
}

/************************************************************************/
/*                   GetLayerWithGetSpatialWhereByName()                */
/************************************************************************/

std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>
    GDALGeoPackageDataset::GetLayerWithGetSpatialWhereByName( const char* pszName )
{
    OGRGeoPackageLayer* poRet = (OGRGeoPackageLayer*) GetLayerByName(pszName);
    return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>(poRet, poRet);
}

/************************************************************************/
/*                       CommitTransaction()                        */
/************************************************************************/

OGRErr GDALGeoPackageDataset::CommitTransaction()

{
    if( nSoftTransactionLevel == 1 )
    {
        FlushMetadata();
        for( int i = 0; i < m_nLayers; i++ )
        {
            m_papoLayers[i]->RunDeferredCreationIfNecessary();
        }
    }

    return OGRSQLiteBaseDataSource::CommitTransaction();
}

/************************************************************************/
/*                     RollbackTransaction()                            */
/************************************************************************/

OGRErr GDALGeoPackageDataset::RollbackTransaction()

{
    if( nSoftTransactionLevel == 1 )
    {
        FlushMetadata();
        for( int i = 0; i < m_nLayers; i++ )
        {
            m_papoLayers[i]->RunDeferredCreationIfNecessary();
            m_papoLayers[i]->CreateSpatialIndexIfNecessary();
            m_papoLayers[i]->ResetReading();
        }
    }

    return OGRSQLiteBaseDataSource::RollbackTransaction();
}

/************************************************************************/
/*                       GetGeometryTypeString()                        */
/************************************************************************/

const char* GDALGeoPackageDataset::GetGeometryTypeString(OGRwkbGeometryType eType)
{
    const char* pszGPKGGeomType = OGRToOGCGeomType(eType);
    if( EQUAL(pszGPKGGeomType, "GEOMETRYCOLLECTION") &&
        CPLTestBool(CPLGetConfigOption("OGR_GPKG_GEOMCOLLECTION", "YES")) )
    {
        pszGPKGGeomType = "GEOMCOLLECTION";
    }
    return pszGPKGGeomType;
}

/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements GDALGeoPackageDataset class
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_swq.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "ogrgeopackageutility.h"
#include "ogrsqliteutility.h"
#include "ogr_wkb.h"
#include "vrt/vrtdataset.h"

#include "tilematrixset.hpp"

#include <cstdlib>

#include <algorithm>
#include <limits>
#include <sstream>

#define COMPILATION_ALLOWED
#define DEFINE_OGRSQLiteSQLFunctionsSetCaseSensitiveLike
#include "ogrsqlitesqlfunctionscommon.cpp"

// Keep in sync prototype of those 2 functions between gdalopeninfo.cpp,
// ogrsqlitedatasource.cpp and ogrgeopackagedatasource.cpp
void GDALOpenInfoDeclareFileNotToOpen(const char *pszFilename,
                                      const GByte *pabyHeader,
                                      int nHeaderBytes);
void GDALOpenInfoUnDeclareFileNotToOpen(const char *pszFilename);

/************************************************************************/
/*                             Tiling schemes                           */
/************************************************************************/

typedef struct
{
    const char *pszName;
    int nEPSGCode;
    double dfMinX;
    double dfMaxY;
    int nTileXCountZoomLevel0;
    int nTileYCountZoomLevel0;
    int nTileWidth;
    int nTileHeight;
    double dfPixelXSizeZoomLevel0;
    double dfPixelYSizeZoomLevel0;
} TilingSchemeDefinition;

static const TilingSchemeDefinition asTilingSchemes[] = {
    /* See http://portal.opengeospatial.org/files/?artifact_id=35326 (WMTS 1.0),
       Annex E.3 */
    {"GoogleCRS84Quad", 4326, -180.0, 180.0, 1, 1, 256, 256, 360.0 / 256,
     360.0 / 256},

    /* See global-mercator at
       http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
    {"PseudoTMS_GlobalMercator", 3857, -20037508.34, 20037508.34, 2, 2, 256,
     256, 78271.516, 78271.516},
};

// Setting it above 30 would lead to integer overflow ((1 << 31) > INT_MAX)
constexpr int MAX_ZOOM_LEVEL = 30;

/************************************************************************/
/*                     GetTilingScheme()                                */
/************************************************************************/

static std::unique_ptr<TilingSchemeDefinition>
GetTilingScheme(const char *pszName)
{
    if (EQUAL(pszName, "CUSTOM"))
        return nullptr;

    for (const auto &tilingScheme : asTilingSchemes)
    {
        if (EQUAL(pszName, tilingScheme.pszName))
        {
            return std::unique_ptr<TilingSchemeDefinition>(
                new TilingSchemeDefinition(tilingScheme));
        }
    }

    if (EQUAL(pszName, "PseudoTMS_GlobalGeodetic"))
        pszName = "InspireCRS84Quad";

    auto poTM = gdal::TileMatrixSet::parse(pszName);
    if (poTM == nullptr)
        return nullptr;
    if (!poTM->haveAllLevelsSameTopLeft())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported tiling scheme: not all zoom levels have same top "
                 "left corner");
        return nullptr;
    }
    if (!poTM->haveAllLevelsSameTileSize())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported tiling scheme: not all zoom levels have same "
                 "tile size");
        return nullptr;
    }
    if (!poTM->hasOnlyPowerOfTwoVaryingScales())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported tiling scheme: resolution of consecutive zoom "
                 "levels is not always 2");
        return nullptr;
    }
    if (poTM->hasVariableMatrixWidth())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported tiling scheme: some levels have variable matrix "
                 "width");
        return nullptr;
    }
    auto poTilingScheme = std::make_unique<TilingSchemeDefinition>();
    poTilingScheme->pszName = pszName;

    OGRSpatialReference oSRS;
    if (oSRS.SetFromUserInput(poTM->crs().c_str()) != OGRERR_NONE)
    {
        return nullptr;
    }
    if (poTM->crs() == "http://www.opengis.net/def/crs/OGC/1.3/CRS84")
    {
        poTilingScheme->nEPSGCode = 4326;
    }
    else
    {
        const char *pszAuthName = oSRS.GetAuthorityName(nullptr);
        const char *pszAuthCode = oSRS.GetAuthorityCode(nullptr);
        if (pszAuthName == nullptr || !EQUAL(pszAuthName, "EPSG") ||
            pszAuthCode == nullptr)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported tiling scheme: only EPSG CRS supported");
            return nullptr;
        }
        poTilingScheme->nEPSGCode = atoi(pszAuthCode);
    }
    const auto &zoomLevel0 = poTM->tileMatrixList()[0];
    poTilingScheme->dfMinX = zoomLevel0.mTopLeftX;
    poTilingScheme->dfMaxY = zoomLevel0.mTopLeftY;
    poTilingScheme->nTileXCountZoomLevel0 = zoomLevel0.mMatrixWidth;
    poTilingScheme->nTileYCountZoomLevel0 = zoomLevel0.mMatrixHeight;
    poTilingScheme->nTileWidth = zoomLevel0.mTileWidth;
    poTilingScheme->nTileHeight = zoomLevel0.mTileHeight;
    poTilingScheme->dfPixelXSizeZoomLevel0 = zoomLevel0.mResX;
    poTilingScheme->dfPixelYSizeZoomLevel0 = zoomLevel0.mResY;

    const bool bInvertAxis = oSRS.EPSGTreatsAsLatLong() != FALSE ||
                             oSRS.EPSGTreatsAsNorthingEasting() != FALSE;
    if (bInvertAxis)
    {
        std::swap(poTilingScheme->dfMinX, poTilingScheme->dfMaxY);
        std::swap(poTilingScheme->dfPixelXSizeZoomLevel0,
                  poTilingScheme->dfPixelYSizeZoomLevel0);
    }
    return poTilingScheme;
}

static const char *pszCREATE_GPKG_GEOMETRY_COLUMNS =
    "CREATE TABLE gpkg_geometry_columns ("
    "table_name TEXT NOT NULL,"
    "column_name TEXT NOT NULL,"
    "geometry_type_name TEXT NOT NULL,"
    "srs_id INTEGER NOT NULL,"
    "z TINYINT NOT NULL,"
    "m TINYINT NOT NULL,"
    "CONSTRAINT pk_geom_cols PRIMARY KEY (table_name, column_name),"
    "CONSTRAINT uk_gc_table_name UNIQUE (table_name),"
    "CONSTRAINT fk_gc_tn FOREIGN KEY (table_name) REFERENCES "
    "gpkg_contents(table_name),"
    "CONSTRAINT fk_gc_srs FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys "
    "(srs_id)"
    ")";

/* Only recent versions of SQLite will let us muck with application_id */
/* via a PRAGMA statement, so we have to write directly into the */
/* file header here. */
/* We do this at the *end* of initialization so that there is */
/* data to write down to a file, and we will have a writable file */
/* once we close the SQLite connection */
OGRErr GDALGeoPackageDataset::SetApplicationAndUserVersionId()
{
    CPLAssert(hDB != nullptr);

    const CPLString osPragma(CPLString().Printf("PRAGMA application_id = %u;"
                                                "PRAGMA user_version = %u",
                                                m_nApplicationId,
                                                m_nUserVersion));
    return SQLCommand(hDB, osPragma.c_str());
}

bool GDALGeoPackageDataset::CloseDB()
{
    OGRSQLiteUnregisterSQLFunctions(m_pSQLFunctionData);
    m_pSQLFunctionData = nullptr;
    return OGRSQLiteBaseDataSource::CloseDB();
}

bool GDALGeoPackageDataset::ReOpenDB()
{
    CPLAssert(hDB != nullptr);
    CPLAssert(m_pszFilename != nullptr);

    FinishSpatialite();

    CloseDB();

    /* And re-open the file */
    return OpenOrCreateDB(SQLITE_OPEN_READWRITE);
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

OGRSpatialReference *
GDALGeoPackageDataset::GetSpatialRef(int iSrsId, bool bFallbackToEPSG,
                                     bool bEmitErrorIfNotFound)
{
    std::map<int, OGRSpatialReference *>::const_iterator oIter =
        m_oMapSrsIdToSrs.find(iSrsId);
    if (oIter != m_oMapSrsIdToSrs.end())
    {
        if (oIter->second == nullptr)
            return nullptr;
        oIter->second->Reference();
        return oIter->second;
    }

    if (iSrsId == 0 || iSrsId == -1)
    {
        OGRSpatialReference *poSpatialRef = new OGRSpatialReference();
        poSpatialRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        // See corresponding tests in GDALGeoPackageDataset::GetSrsId
        if (iSrsId == 0)
        {
            poSpatialRef->SetGeogCS("Undefined geographic SRS", "unknown",
                                    "unknown", SRS_WGS84_SEMIMAJOR,
                                    SRS_WGS84_INVFLATTENING);
        }
        else if (iSrsId == -1)
        {
            poSpatialRef->SetLocalCS("Undefined Cartesian SRS");
            poSpatialRef->SetLinearUnits(SRS_UL_METER, 1.0);
        }

        m_oMapSrsIdToSrs[iSrsId] = poSpatialRef;
        poSpatialRef->Reference();
        return poSpatialRef;
    }

    CPLString oSQL;
    oSQL.Printf("SELECT srs_name, definition, organization, "
                "organization_coordsys_id%s%s "
                "FROM gpkg_spatial_ref_sys WHERE "
                "srs_id = %d LIMIT 2",
                m_bHasDefinition12_063 ? ", definition_12_063" : "",
                m_bHasEpochColumn ? ", epoch" : "", iSrsId);

    auto oResult = SQLQuery(hDB, oSQL.c_str());

    if (!oResult || oResult->RowCount() != 1)
    {
        if (bFallbackToEPSG)
        {
            CPLDebug("GPKG",
                     "unable to read srs_id '%d' from gpkg_spatial_ref_sys",
                     iSrsId);
            OGRSpatialReference *poSRS = new OGRSpatialReference();
            if (poSRS->importFromEPSG(iSrsId) == OGRERR_NONE)
            {
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                return poSRS;
            }
            poSRS->Release();
        }
        else if (bEmitErrorIfNotFound)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "unable to read srs_id '%d' from gpkg_spatial_ref_sys",
                     iSrsId);
            m_oMapSrsIdToSrs[iSrsId] = nullptr;
        }
        return nullptr;
    }

    const char *pszName = oResult->GetValue(0, 0);
    if (pszName && EQUAL(pszName, "Undefined SRS"))
    {
        m_oMapSrsIdToSrs[iSrsId] = nullptr;
        return nullptr;
    }
    const char *pszWkt = oResult->GetValue(1, 0);
    if (pszWkt == nullptr)
        return nullptr;
    const char *pszOrganization = oResult->GetValue(2, 0);
    const char *pszOrganizationCoordsysID = oResult->GetValue(3, 0);
    const char *pszWkt2 =
        m_bHasDefinition12_063 ? oResult->GetValue(4, 0) : nullptr;
    if (pszWkt2 && !EQUAL(pszWkt2, "undefined"))
        pszWkt = pszWkt2;
    const char *pszCoordinateEpoch =
        m_bHasEpochColumn ? oResult->GetValue(5, 0) : nullptr;
    const double dfCoordinateEpoch =
        pszCoordinateEpoch ? CPLAtof(pszCoordinateEpoch) : 0.0;

    OGRSpatialReference *poSpatialRef = new OGRSpatialReference();
    poSpatialRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    // Try to import first from EPSG code, and then from WKT
    if (!(pszOrganization && pszOrganizationCoordsysID &&
          EQUAL(pszOrganization, "EPSG") &&
          (atoi(pszOrganizationCoordsysID) == iSrsId ||
           (dfCoordinateEpoch > 0 && strstr(pszWkt, "DYNAMIC[") == nullptr)) &&
          GDALGPKGImportFromEPSG(
              poSpatialRef, atoi(pszOrganizationCoordsysID)) == OGRERR_NONE) &&
        poSpatialRef->importFromWkt(pszWkt) != OGRERR_NONE)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unable to parse srs_id '%d' well-known text '%s'", iSrsId,
                 pszWkt);
        delete poSpatialRef;
        m_oMapSrsIdToSrs[iSrsId] = nullptr;
        return nullptr;
    }

    poSpatialRef->StripTOWGS84IfKnownDatumAndAllowed();
    poSpatialRef->SetCoordinateEpoch(dfCoordinateEpoch);
    m_oMapSrsIdToSrs[iSrsId] = poSpatialRef;
    poSpatialRef->Reference();
    return poSpatialRef;
}

const char *GDALGeoPackageDataset::GetSrsName(const OGRSpatialReference &oSRS)
{
    const char *pszName = oSRS.GetName();
    if (pszName)
        return pszName;

    // Something odd.  Return empty.
    return "Unnamed SRS";
}

/* Add the definition_12_063 column to an existing gpkg_spatial_ref_sys table */
bool GDALGeoPackageDataset::ConvertGpkgSpatialRefSysToExtensionWkt2(
    bool bForceEpoch)
{
    const bool bAddEpoch = (m_nUserVersion >= GPKG_1_4_VERSION || bForceEpoch);
    auto oResultTable = SQLQuery(
        hDB, "SELECT srs_name, srs_id, organization, organization_coordsys_id, "
             "definition, description FROM gpkg_spatial_ref_sys LIMIT 100000");
    if (!oResultTable)
        return false;

    // Temporary remove foreign key checks
    const GPKGTemporaryForeignKeyCheckDisabler
        oGPKGTemporaryForeignKeyCheckDisabler(this);

    bool bRet = SoftStartTransaction() == OGRERR_NONE;

    if (bRet)
    {
        std::string osSQL("CREATE TABLE gpkg_spatial_ref_sys_temp ("
                          "srs_name TEXT NOT NULL,"
                          "srs_id INTEGER NOT NULL PRIMARY KEY,"
                          "organization TEXT NOT NULL,"
                          "organization_coordsys_id INTEGER NOT NULL,"
                          "definition TEXT NOT NULL,"
                          "description TEXT, "
                          "definition_12_063 TEXT NOT NULL");
        if (bAddEpoch)
            osSQL += ", epoch DOUBLE";
        osSQL += ")";
        bRet = SQLCommand(hDB, osSQL.c_str()) == OGRERR_NONE;
    }

    if (bRet)
    {
        for (int i = 0; bRet && i < oResultTable->RowCount(); i++)
        {
            const char *pszSrsName = oResultTable->GetValue(0, i);
            const char *pszSrsId = oResultTable->GetValue(1, i);
            const char *pszOrganization = oResultTable->GetValue(2, i);
            const char *pszOrganizationCoordsysID =
                oResultTable->GetValue(3, i);
            const char *pszDefinition = oResultTable->GetValue(4, i);
            if (pszSrsName == nullptr || pszSrsId == nullptr ||
                pszOrganization == nullptr ||
                pszOrganizationCoordsysID == nullptr)
            {
                // should not happen as there are NOT NULL constraints
                // But a database could lack such NOT NULL constraints or have
                // large values that would cause a memory allocation failure.
            }
            const char *pszDescription = oResultTable->GetValue(5, i);
            char *pszSQL;

            OGRSpatialReference oSRS;
            if (pszOrganization && pszOrganizationCoordsysID &&
                EQUAL(pszOrganization, "EPSG"))
            {
                oSRS.importFromEPSG(atoi(pszOrganizationCoordsysID));
            }
            if (!oSRS.IsEmpty() && pszDefinition &&
                !EQUAL(pszDefinition, "undefined"))
            {
                oSRS.SetFromUserInput(pszDefinition);
            }
            char *pszWKT2 = nullptr;
            if (!oSRS.IsEmpty())
            {
                const char *const apszOptionsWkt2[] = {"FORMAT=WKT2_2015",
                                                       nullptr};
                oSRS.exportToWkt(&pszWKT2, apszOptionsWkt2);
                if (pszWKT2 && pszWKT2[0] == '\0')
                {
                    CPLFree(pszWKT2);
                    pszWKT2 = nullptr;
                }
            }
            if (pszWKT2 == nullptr)
            {
                pszWKT2 = CPLStrdup("undefined");
            }

            if (pszDescription)
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_spatial_ref_sys_temp(srs_name, srs_id, "
                    "organization, organization_coordsys_id, definition, "
                    "description, definition_12_063) VALUES ('%q', '%q', '%q', "
                    "'%q', '%q', '%q', '%q')",
                    pszSrsName, pszSrsId, pszOrganization,
                    pszOrganizationCoordsysID, pszDefinition, pszDescription,
                    pszWKT2);
            }
            else
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_spatial_ref_sys_temp(srs_name, srs_id, "
                    "organization, organization_coordsys_id, definition, "
                    "description, definition_12_063) VALUES ('%q', '%q', '%q', "
                    "'%q', '%q', NULL, '%q')",
                    pszSrsName, pszSrsId, pszOrganization,
                    pszOrganizationCoordsysID, pszDefinition, pszWKT2);
            }

            CPLFree(pszWKT2);
            bRet &= SQLCommand(hDB, pszSQL) == OGRERR_NONE;
            sqlite3_free(pszSQL);
        }
    }

    if (bRet)
    {
        bRet =
            SQLCommand(hDB, "DROP TABLE gpkg_spatial_ref_sys") == OGRERR_NONE;
    }
    if (bRet)
    {
        bRet = SQLCommand(hDB, "ALTER TABLE gpkg_spatial_ref_sys_temp RENAME "
                               "TO gpkg_spatial_ref_sys") == OGRERR_NONE;
    }
    if (bRet)
    {
        bRet = OGRERR_NONE == CreateExtensionsTableIfNecessary() &&
               OGRERR_NONE == SQLCommand(hDB,
                                         "INSERT INTO gpkg_extensions "
                                         "(table_name, column_name, "
                                         "extension_name, definition, scope) "
                                         "VALUES "
                                         "('gpkg_spatial_ref_sys', "
                                         "'definition_12_063', 'gpkg_crs_wkt', "
                                         "'http://www.geopackage.org/spec120/"
                                         "#extension_crs_wkt', 'read-write')");
    }
    if (bRet && bAddEpoch)
    {
        bRet =
            OGRERR_NONE ==
                SQLCommand(hDB, "UPDATE gpkg_extensions SET extension_name = "
                                "'gpkg_crs_wkt_1_1' "
                                "WHERE extension_name = 'gpkg_crs_wkt'") &&
            OGRERR_NONE ==
                SQLCommand(
                    hDB,
                    "INSERT INTO gpkg_extensions "
                    "(table_name, column_name, extension_name, definition, "
                    "scope) "
                    "VALUES "
                    "('gpkg_spatial_ref_sys', 'epoch', 'gpkg_crs_wkt_1_1', "
                    "'http://www.geopackage.org/spec/#extension_crs_wkt', "
                    "'read-write')");
    }
    if (bRet)
    {
        SoftCommitTransaction();
        m_bHasDefinition12_063 = true;
        if (bAddEpoch)
            m_bHasEpochColumn = true;
    }
    else
    {
        SoftRollbackTransaction();
    }

    return bRet;
}

int GDALGeoPackageDataset::GetSrsId(const OGRSpatialReference *poSRSIn)
{
    const char *pszName = poSRSIn ? poSRSIn->GetName() : nullptr;
    if (!poSRSIn || poSRSIn->IsEmpty() ||
        (pszName && EQUAL(pszName, "Undefined SRS")))
    {
        OGRErr err = OGRERR_NONE;
        const int nSRSId = SQLGetInteger(
            hDB,
            "SELECT srs_id FROM gpkg_spatial_ref_sys WHERE srs_name = "
            "'Undefined SRS' AND organization = 'GDAL'",
            &err);
        if (err == OGRERR_NONE)
            return nSRSId;

        // The below WKT definitions are somehow questionable (using a unknown
        // unit). For GDAL >= 3.9, they won't be used. They will only be used
        // for earlier versions.
        const char *pszSQL;
#define UNDEFINED_CRS_SRS_ID 99999
        static_assert(UNDEFINED_CRS_SRS_ID == FIRST_CUSTOM_SRSID - 1);
#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)
        if (m_bHasDefinition12_063)
        {
            /* clang-format off */
            pszSQL =
                "INSERT INTO gpkg_spatial_ref_sys "
                "(srs_name,srs_id,organization,organization_coordsys_id,"
                "definition, definition_12_063, description) VALUES "
                "('Undefined SRS'," XSTRINGIFY(UNDEFINED_CRS_SRS_ID) ",'GDAL',"
                XSTRINGIFY(UNDEFINED_CRS_SRS_ID) ","
                "'LOCAL_CS[\"Undefined SRS\",LOCAL_DATUM[\"unknown\",32767],"
                "UNIT[\"unknown\",0],AXIS[\"Easting\",EAST],"
                "AXIS[\"Northing\",NORTH]]',"
                "'ENGCRS[\"Undefined SRS\",EDATUM[\"unknown\"],CS[Cartesian,2],"
                "AXIS[\"easting\",east,ORDER[1],LENGTHUNIT[\"unknown\",0]],"
                "AXIS[\"northing\",north,ORDER[2],LENGTHUNIT[\"unknown\",0]]]',"
                "'Custom undefined coordinate reference system')";
            /* clang-format on */
        }
        else
        {
            /* clang-format off */
            pszSQL =
                "INSERT INTO gpkg_spatial_ref_sys "
                "(srs_name,srs_id,organization,organization_coordsys_id,"
                "definition, description) VALUES "
                "('Undefined SRS'," XSTRINGIFY(UNDEFINED_CRS_SRS_ID) ",'GDAL',"
                XSTRINGIFY(UNDEFINED_CRS_SRS_ID) ","
                "'LOCAL_CS[\"Undefined SRS\",LOCAL_DATUM[\"unknown\",32767],"
                "UNIT[\"unknown\",0],AXIS[\"Easting\",EAST],"
                "AXIS[\"Northing\",NORTH]]',"
                "'Custom undefined coordinate reference system')";
            /* clang-format on */
        }
        if (SQLCommand(hDB, pszSQL) == OGRERR_NONE)
            return UNDEFINED_CRS_SRS_ID;
#undef UNDEFINED_CRS_SRS_ID
#undef XSTRINGIFY
#undef STRINGIFY
        return -1;
    }

    std::unique_ptr<OGRSpatialReference> poSRS(poSRSIn->Clone());

    if (poSRS->IsGeographic() || poSRS->IsLocal())
    {
        // See corresponding tests in GDALGeoPackageDataset::GetSpatialRef
        if (pszName != nullptr && strlen(pszName) > 0)
        {
            if (EQUAL(pszName, "Undefined geographic SRS"))
                return 0;

            if (EQUAL(pszName, "Undefined Cartesian SRS"))
                return -1;
        }
    }

    const char *pszAuthorityName = poSRS->GetAuthorityName(nullptr);

    if (pszAuthorityName == nullptr || strlen(pszAuthorityName) == 0)
    {
        // Try to force identify an EPSG code.
        poSRS->AutoIdentifyEPSG();

        pszAuthorityName = poSRS->GetAuthorityName(nullptr);
        if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char *pszAuthorityCode = poSRS->GetAuthorityCode(nullptr);
            if (pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0)
            {
                /* Import 'clean' SRS */
                poSRS->importFromEPSG(atoi(pszAuthorityCode));

                pszAuthorityName = poSRS->GetAuthorityName(nullptr);
            }
        }

        poSRS->SetCoordinateEpoch(poSRSIn->GetCoordinateEpoch());
    }

    // Check whether the EPSG authority code is already mapped to a
    // SRS ID.
    char *pszSQL = nullptr;
    int nSRSId = DEFAULT_SRID;
    int nAuthorityCode = 0;
    OGRErr err = OGRERR_NONE;
    bool bCanUseAuthorityCode = false;
    const char *const apszIsSameOptions[] = {
        "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES",
        "IGNORE_COORDINATE_EPOCH=YES", nullptr};
    if (pszAuthorityName != nullptr && strlen(pszAuthorityName) > 0)
    {
        const char *pszAuthorityCode = poSRS->GetAuthorityCode(nullptr);
        if (pszAuthorityCode)
        {
            if (CPLGetValueType(pszAuthorityCode) == CPL_VALUE_INTEGER)
            {
                nAuthorityCode = atoi(pszAuthorityCode);
            }
            else
            {
                CPLDebug("GPKG",
                         "SRS has %s:%s identification, but the code not "
                         "being an integer value cannot be stored as such "
                         "in the database.",
                         pszAuthorityName, pszAuthorityCode);
                pszAuthorityName = nullptr;
            }
        }
    }

    if (pszAuthorityName != nullptr && strlen(pszAuthorityName) > 0 &&
        poSRSIn->GetCoordinateEpoch() == 0)
    {
        pszSQL =
            sqlite3_mprintf("SELECT srs_id FROM gpkg_spatial_ref_sys WHERE "
                            "upper(organization) = upper('%q') AND "
                            "organization_coordsys_id = %d",
                            pszAuthorityName, nAuthorityCode);

        nSRSId = SQLGetInteger(hDB, pszSQL, &err);
        sqlite3_free(pszSQL);

        // Got a match? Return it!
        if (OGRERR_NONE == err)
        {
            auto poRefSRS = GetSpatialRef(nSRSId);
            bool bOK =
                (poRefSRS == nullptr ||
                 poSRS->IsSame(poRefSRS, apszIsSameOptions) ||
                 !CPLTestBool(CPLGetConfigOption("OGR_GPKG_CHECK_SRS", "YES")));
            if (poRefSRS)
                poRefSRS->Release();
            if (bOK)
            {
                return nSRSId;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Passed SRS uses %s:%d identification, but its "
                         "definition is not compatible with the "
                         "definition of that object already in the database. "
                         "Registering it as a new entry into the database.",
                         pszAuthorityName, nAuthorityCode);
                pszAuthorityName = nullptr;
                nAuthorityCode = 0;
            }
        }
    }

    // Translate SRS to WKT.
    CPLCharUniquePtr pszWKT1;
    CPLCharUniquePtr pszWKT2_2015;
    CPLCharUniquePtr pszWKT2_2019;
    const char *const apszOptionsWkt1[] = {"FORMAT=WKT1_GDAL", nullptr};
    const char *const apszOptionsWkt2_2015[] = {"FORMAT=WKT2_2015", nullptr};
    const char *const apszOptionsWkt2_2019[] = {"FORMAT=WKT2_2019", nullptr};

    std::string osEpochTest;
    if (poSRSIn->GetCoordinateEpoch() > 0 && m_bHasEpochColumn)
    {
        osEpochTest =
            CPLSPrintf(" AND epoch = %.18g", poSRSIn->GetCoordinateEpoch());
    }

    if (!(poSRS->IsGeographic() && poSRS->GetAxesCount() == 3))
    {
        char *pszTmp = nullptr;
        poSRS->exportToWkt(&pszTmp, apszOptionsWkt1);
        pszWKT1.reset(pszTmp);
        if (pszWKT1 && pszWKT1.get()[0] == '\0')
        {
            pszWKT1.reset();
        }
    }
    {
        char *pszTmp = nullptr;
        poSRS->exportToWkt(&pszTmp, apszOptionsWkt2_2015);
        pszWKT2_2015.reset(pszTmp);
        if (pszWKT2_2015 && pszWKT2_2015.get()[0] == '\0')
        {
            pszWKT2_2015.reset();
        }
    }
    {
        char *pszTmp = nullptr;
        poSRS->exportToWkt(&pszTmp, apszOptionsWkt2_2019);
        pszWKT2_2019.reset(pszTmp);
        if (pszWKT2_2019 && pszWKT2_2019.get()[0] == '\0')
        {
            pszWKT2_2019.reset();
        }
    }

    if (!pszWKT1 && !pszWKT2_2015 && !pszWKT2_2019)
    {
        return DEFAULT_SRID;
    }

    if (poSRSIn->GetCoordinateEpoch() == 0 || m_bHasEpochColumn)
    {
        // Search if there is already an existing entry with this WKT
        if (m_bHasDefinition12_063 && (pszWKT2_2015 || pszWKT2_2019))
        {
            if (pszWKT1)
            {
                pszSQL = sqlite3_mprintf(
                    "SELECT srs_id FROM gpkg_spatial_ref_sys WHERE "
                    "(definition = '%q' OR definition_12_063 IN ('%q','%q'))%s",
                    pszWKT1.get(),
                    pszWKT2_2015 ? pszWKT2_2015.get() : "invalid",
                    pszWKT2_2019 ? pszWKT2_2019.get() : "invalid",
                    osEpochTest.c_str());
            }
            else
            {
                pszSQL = sqlite3_mprintf(
                    "SELECT srs_id FROM gpkg_spatial_ref_sys WHERE "
                    "definition_12_063 IN ('%q', '%q')%s",
                    pszWKT2_2015 ? pszWKT2_2015.get() : "invalid",
                    pszWKT2_2019 ? pszWKT2_2019.get() : "invalid",
                    osEpochTest.c_str());
            }
        }
        else if (pszWKT1)
        {
            pszSQL =
                sqlite3_mprintf("SELECT srs_id FROM gpkg_spatial_ref_sys WHERE "
                                "definition = '%q'%s",
                                pszWKT1.get(), osEpochTest.c_str());
        }
        else
        {
            pszSQL = nullptr;
        }
        if (pszSQL)
        {
            nSRSId = SQLGetInteger(hDB, pszSQL, &err);
            sqlite3_free(pszSQL);
            if (OGRERR_NONE == err)
            {
                return nSRSId;
            }
        }
    }

    if (pszAuthorityName != nullptr && strlen(pszAuthorityName) > 0 &&
        poSRSIn->GetCoordinateEpoch() == 0)
    {
        bool bTryToReuseSRSId = true;
        if (EQUAL(pszAuthorityName, "EPSG"))
        {
            OGRSpatialReference oSRS_EPSG;
            if (GDALGPKGImportFromEPSG(&oSRS_EPSG, nAuthorityCode) ==
                OGRERR_NONE)
            {
                if (!poSRS->IsSame(&oSRS_EPSG, apszIsSameOptions) &&
                    CPLTestBool(
                        CPLGetConfigOption("OGR_GPKG_CHECK_SRS", "YES")))
                {
                    bTryToReuseSRSId = false;
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Passed SRS uses %s:%d identification, but its "
                        "definition is not compatible with the "
                        "official definition of the object. "
                        "Registering it as a non-%s entry into the database.",
                        pszAuthorityName, nAuthorityCode, pszAuthorityName);
                    pszAuthorityName = nullptr;
                    nAuthorityCode = 0;
                }
            }
        }
        if (bTryToReuseSRSId)
        {
            // No match, but maybe we can use the nAuthorityCode as the nSRSId?
            pszSQL = sqlite3_mprintf(
                "SELECT Count(*) FROM gpkg_spatial_ref_sys WHERE "
                "srs_id = %d",
                nAuthorityCode);

            // Yep, we can!
            if (SQLGetInteger(hDB, pszSQL, nullptr) == 0)
                bCanUseAuthorityCode = true;
            sqlite3_free(pszSQL);
        }
    }

    bool bConvertGpkgSpatialRefSysToExtensionWkt2 = false;
    bool bForceEpoch = false;
    if (!m_bHasDefinition12_063 && pszWKT1 == nullptr &&
        (pszWKT2_2015 != nullptr || pszWKT2_2019 != nullptr))
    {
        bConvertGpkgSpatialRefSysToExtensionWkt2 = true;
    }

    // Add epoch column if needed
    if (poSRSIn->GetCoordinateEpoch() > 0 && !m_bHasEpochColumn)
    {
        if (m_bHasDefinition12_063)
        {
            if (SoftStartTransaction() != OGRERR_NONE)
                return DEFAULT_SRID;
            if (SQLCommand(hDB, "ALTER TABLE gpkg_spatial_ref_sys "
                                "ADD COLUMN epoch DOUBLE") != OGRERR_NONE ||
                SQLCommand(hDB, "UPDATE gpkg_extensions SET extension_name = "
                                "'gpkg_crs_wkt_1_1' "
                                "WHERE extension_name = 'gpkg_crs_wkt'") !=
                    OGRERR_NONE ||
                SQLCommand(
                    hDB,
                    "INSERT INTO gpkg_extensions "
                    "(table_name, column_name, extension_name, definition, "
                    "scope) "
                    "VALUES "
                    "('gpkg_spatial_ref_sys', 'epoch', 'gpkg_crs_wkt_1_1', "
                    "'http://www.geopackage.org/spec/#extension_crs_wkt', "
                    "'read-write')") != OGRERR_NONE)
            {
                SoftRollbackTransaction();
                return DEFAULT_SRID;
            }

            if (SoftCommitTransaction() != OGRERR_NONE)
                return DEFAULT_SRID;

            m_bHasEpochColumn = true;
        }
        else
        {
            bConvertGpkgSpatialRefSysToExtensionWkt2 = true;
            bForceEpoch = true;
        }
    }

    if (bConvertGpkgSpatialRefSysToExtensionWkt2 &&
        !ConvertGpkgSpatialRefSysToExtensionWkt2(bForceEpoch))
    {
        return DEFAULT_SRID;
    }

    // Reuse the authority code number as SRS_ID if we can
    if (bCanUseAuthorityCode)
    {
        nSRSId = nAuthorityCode;
    }
    // Otherwise, generate a new SRS_ID number (max + 1)
    else
    {
        // Get the current maximum srid in the srs table.
        const int nMaxSRSId = SQLGetInteger(
            hDB, "SELECT MAX(srs_id) FROM gpkg_spatial_ref_sys", nullptr);
        nSRSId = std::max(FIRST_CUSTOM_SRSID, nMaxSRSId + 1);
    }

    std::string osEpochColumn;
    std::string osEpochVal;
    if (poSRSIn->GetCoordinateEpoch() > 0)
    {
        osEpochColumn = ", epoch";
        osEpochVal = CPLSPrintf(", %.18g", poSRSIn->GetCoordinateEpoch());
    }

    // Add new SRS row to gpkg_spatial_ref_sys.
    if (m_bHasDefinition12_063)
    {
        // Force WKT2_2019 when we have a dynamic CRS and coordinate epoch
        const char *pszWKT2 = poSRSIn->IsDynamic() &&
                                      poSRSIn->GetCoordinateEpoch() > 0 &&
                                      pszWKT2_2019
                                  ? pszWKT2_2019.get()
                              : pszWKT2_2015 ? pszWKT2_2015.get()
                                             : pszWKT2_2019.get();

        if (pszAuthorityName != nullptr && nAuthorityCode > 0)
        {
            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_spatial_ref_sys "
                "(srs_name,srs_id,organization,organization_coordsys_id,"
                "definition, definition_12_063%s) VALUES "
                "('%q', %d, upper('%q'), %d, '%q', '%q'%s)",
                osEpochColumn.c_str(), GetSrsName(*poSRS), nSRSId,
                pszAuthorityName, nAuthorityCode,
                pszWKT1 ? pszWKT1.get() : "undefined",
                pszWKT2 ? pszWKT2 : "undefined", osEpochVal.c_str());
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_spatial_ref_sys "
                "(srs_name,srs_id,organization,organization_coordsys_id,"
                "definition, definition_12_063%s) VALUES "
                "('%q', %d, upper('%q'), %d, '%q', '%q'%s)",
                osEpochColumn.c_str(), GetSrsName(*poSRS), nSRSId, "NONE",
                nSRSId, pszWKT1 ? pszWKT1.get() : "undefined",
                pszWKT2 ? pszWKT2 : "undefined", osEpochVal.c_str());
        }
    }
    else
    {
        if (pszAuthorityName != nullptr && nAuthorityCode > 0)
        {
            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_spatial_ref_sys "
                "(srs_name,srs_id,organization,organization_coordsys_id,"
                "definition) VALUES ('%q', %d, upper('%q'), %d, '%q')",
                GetSrsName(*poSRS), nSRSId, pszAuthorityName, nAuthorityCode,
                pszWKT1 ? pszWKT1.get() : "undefined");
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_spatial_ref_sys "
                "(srs_name,srs_id,organization,organization_coordsys_id,"
                "definition) VALUES ('%q', %d, upper('%q'), %d, '%q')",
                GetSrsName(*poSRS), nSRSId, "NONE", nSRSId,
                pszWKT1 ? pszWKT1.get() : "undefined");
        }
    }

    // Add new row to gpkg_spatial_ref_sys.
    CPL_IGNORE_RET_VAL(SQLCommand(hDB, pszSQL));

    // Free everything that was allocated.
    sqlite3_free(pszSQL);

    return nSRSId;
}

/************************************************************************/
/*                        GDALGeoPackageDataset()                       */
/************************************************************************/

GDALGeoPackageDataset::GDALGeoPackageDataset()
    : m_nApplicationId(GPKG_APPLICATION_ID), m_nUserVersion(GPKG_1_2_VERSION),
      m_papoLayers(nullptr), m_nLayers(0),
#ifdef ENABLE_GPKG_OGR_CONTENTS
      m_bHasGPKGOGRContents(false),
#endif
      m_bHasGPKGGeometryColumns(false), m_bHasDefinition12_063(false),
      m_bIdentifierAsCO(false), m_bDescriptionAsCO(false),
      m_bHasReadMetadataFromStorage(false), m_bMetadataDirty(false),
      m_bRecordInsertedInGPKGContent(false), m_bGeoTransformValid(false),
      m_nSRID(-1),  // Unknown Cartesian.
      m_dfTMSMinX(0.0), m_dfTMSMaxY(0.0), m_nOverviewCount(0),
      m_papoOverviewDS(nullptr), m_bZoomOther(false), m_bInFlushCache(false),
      m_osTilingScheme("CUSTOM"), m_bMapTableToExtensionsBuilt(false),
      m_bMapTableToContentsBuilt(false)
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
    GDALGeoPackageDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr GDALGeoPackageDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (eAccess == GA_Update && m_poParentDS == nullptr &&
            !m_osRasterTable.empty() && !m_bGeoTransformValid)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Raster table %s not correctly initialized due to missing "
                     "call to SetGeoTransform()",
                     m_osRasterTable.c_str());
        }

        if (GDALGeoPackageDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        // Destroy bands now since we don't want
        // GDALGPKGMBTilesLikeRasterBand::FlushCache() to run after dataset
        // destruction
        for (int i = 0; i < nBands; i++)
            delete papoBands[i];
        nBands = 0;
        CPLFree(papoBands);
        papoBands = nullptr;

        // Destroy overviews before cleaning m_hTempDB as they could still
        // need it
        for (int i = 0; i < m_nOverviewCount; i++)
            delete m_papoOverviewDS[i];

        if (m_poParentDS)
        {
            hDB = nullptr;
        }

        for (int i = 0; i < m_nLayers; i++)
            delete m_papoLayers[i];

        CPLFree(m_papoLayers);
        CPLFree(m_papoOverviewDS);

        std::map<int, OGRSpatialReference *>::iterator oIter =
            m_oMapSrsIdToSrs.begin();
        for (; oIter != m_oMapSrsIdToSrs.end(); ++oIter)
        {
            OGRSpatialReference *poSRS = oIter->second;
            if (poSRS)
                poSRS->Release();
        }

        if (!CloseDB())
            eErr = CE_Failure;

        if (OGRSQLiteBaseDataSource::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                         ICanIWriteBlock()                            */
/************************************************************************/

bool GDALGeoPackageDataset::ICanIWriteBlock()
{
    if (!GetUpdate())
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "IWriteBlock() not supported on dataset opened in read-only mode");
        return false;
    }

    if (m_pabyCachedTiles == nullptr)
    {
        return false;
    }

    if (!m_bGeoTransformValid || m_nSRID == UNKNOWN_SRID)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported if georeferencing not set");
        return false;
    }
    return true;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GDALGeoPackageDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, int *panBandMap, GSpacing nPixelSpace, GSpacing nLineSpace,
    GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)

{
    CPLErr eErr = OGRSQLiteBaseDataSource::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace,
        psExtraArg);

    // If writing all bands, in non-shifted mode, flush all entirely written
    // tiles This can avoid "stressing" the block cache with too many dirty
    // blocks. Note: this logic would be useless with a per-dataset block cache.
    if (eErr == CE_None && eRWFlag == GF_Write && nXSize == nBufXSize &&
        nYSize == nBufYSize && nBandCount == nBands &&
        m_nShiftXPixelsMod == 0 && m_nShiftYPixelsMod == 0)
    {
        auto poBand =
            cpl::down_cast<GDALGPKGMBTilesLikeRasterBand *>(GetRasterBand(1));
        int nBlockXSize, nBlockYSize;
        poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        const int nBlockXStart = DIV_ROUND_UP(nXOff, nBlockXSize);
        const int nBlockYStart = DIV_ROUND_UP(nYOff, nBlockYSize);
        const int nBlockXEnd = (nXOff + nXSize) / nBlockXSize;
        const int nBlockYEnd = (nYOff + nYSize) / nBlockYSize;
        for (int nBlockY = nBlockXStart; nBlockY < nBlockYEnd; nBlockY++)
        {
            for (int nBlockX = nBlockYStart; nBlockX < nBlockXEnd; nBlockX++)
            {
                GDALRasterBlock *poBlock =
                    poBand->AccessibleTryGetLockedBlockRef(nBlockX, nBlockY);
                if (poBlock)
                {
                    // GetDirty() should be true in most situation (otherwise
                    // it means the block cache is under extreme pressure!)
                    if (poBlock->GetDirty())
                    {
                        // IWriteBlock() on one band will check the dirty state
                        // of the corresponding blocks in other bands, to decide
                        // if it can call WriteTile(), so we have only to do
                        // that on one of the bands
                        if (poBlock->Write() != CE_None)
                            eErr = CE_Failure;
                    }
                    poBlock->DropLock();
                }
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                          GetOGRTableLimit()                          */
/************************************************************************/

static int GetOGRTableLimit()
{
    return atoi(CPLGetConfigOption("OGR_TABLE_LIMIT", "10000"));
}

/************************************************************************/
/*                      GetNameTypeMapFromSQliteMaster()                */
/************************************************************************/

const std::map<CPLString, CPLString> &
GDALGeoPackageDataset::GetNameTypeMapFromSQliteMaster()
{
    if (!m_oMapNameToType.empty())
        return m_oMapNameToType;

    CPLString osSQL(
        "SELECT name, type FROM sqlite_master WHERE "
        "type IN ('view', 'table') OR "
        "(name LIKE 'trigger_%_feature_count_%' AND type = 'trigger')");
    const int nTableLimit = GetOGRTableLimit();
    if (nTableLimit > 0)
    {
        osSQL += " LIMIT ";
        osSQL += CPLSPrintf("%d", 1 + 3 * nTableLimit);
    }

    auto oResult = SQLQuery(hDB, osSQL);
    if (oResult)
    {
        for (int i = 0; i < oResult->RowCount(); i++)
        {
            const char *pszName = oResult->GetValue(0, i);
            const char *pszType = oResult->GetValue(1, i);
            m_oMapNameToType[CPLString(pszName).toupper()] = pszType;
        }
    }

    return m_oMapNameToType;
}

/************************************************************************/
/*                    RemoveTableFromSQLiteMasterCache()                */
/************************************************************************/

void GDALGeoPackageDataset::RemoveTableFromSQLiteMasterCache(
    const char *pszTableName)
{
    m_oMapNameToType.erase(CPLString(pszTableName).toupper());
}

/************************************************************************/
/*                  GetUnknownExtensionsTableSpecific()                 */
/************************************************************************/

const std::map<CPLString, std::vector<GPKGExtensionDesc>> &
GDALGeoPackageDataset::GetUnknownExtensionsTableSpecific()
{
    if (m_bMapTableToExtensionsBuilt)
        return m_oMapTableToExtensions;
    m_bMapTableToExtensionsBuilt = true;

    if (!HasExtensionsTable())
        return m_oMapTableToExtensions;

    CPLString osSQL(
        "SELECT table_name, extension_name, definition, scope "
        "FROM gpkg_extensions WHERE "
        "table_name IS NOT NULL "
        "AND extension_name IS NOT NULL "
        "AND definition IS NOT NULL "
        "AND scope IS NOT NULL "
        "AND extension_name NOT IN ('gpkg_geom_CIRCULARSTRING', "
        "'gpkg_geom_COMPOUNDCURVE', 'gpkg_geom_CURVEPOLYGON', "
        "'gpkg_geom_MULTICURVE', "
        "'gpkg_geom_MULTISURFACE', 'gpkg_geom_CURVE', 'gpkg_geom_SURFACE', "
        "'gpkg_geom_POLYHEDRALSURFACE', 'gpkg_geom_TIN', 'gpkg_geom_TRIANGLE', "
        "'gpkg_rtree_index', 'gpkg_geometry_type_trigger', "
        "'gpkg_srs_id_trigger', "
        "'gpkg_crs_wkt', 'gpkg_crs_wkt_1_1', 'gpkg_schema', "
        "'gpkg_related_tables', 'related_tables'"
#ifdef HAVE_SPATIALITE
        ", 'gdal_spatialite_computed_geom_column'"
#endif
        ")");
    const int nTableLimit = GetOGRTableLimit();
    if (nTableLimit > 0)
    {
        osSQL += " LIMIT ";
        osSQL += CPLSPrintf("%d", 1 + 10 * nTableLimit);
    }

    auto oResult = SQLQuery(hDB, osSQL);
    if (oResult)
    {
        for (int i = 0; i < oResult->RowCount(); i++)
        {
            const char *pszTableName = oResult->GetValue(0, i);
            const char *pszExtensionName = oResult->GetValue(1, i);
            const char *pszDefinition = oResult->GetValue(2, i);
            const char *pszScope = oResult->GetValue(3, i);
            if (pszTableName && pszExtensionName && pszDefinition && pszScope)
            {
                GPKGExtensionDesc oDesc;
                oDesc.osExtensionName = pszExtensionName;
                oDesc.osDefinition = pszDefinition;
                oDesc.osScope = pszScope;
                m_oMapTableToExtensions[CPLString(pszTableName).toupper()]
                    .push_back(oDesc);
            }
        }
    }

    return m_oMapTableToExtensions;
}

/************************************************************************/
/*                           GetContents()                              */
/************************************************************************/

const std::map<CPLString, GPKGContentsDesc> &
GDALGeoPackageDataset::GetContents()
{
    if (m_bMapTableToContentsBuilt)
        return m_oMapTableToContents;
    m_bMapTableToContentsBuilt = true;

    CPLString osSQL("SELECT table_name, data_type, identifier, "
                    "description, min_x, min_y, max_x, max_y "
                    "FROM gpkg_contents");
    const int nTableLimit = GetOGRTableLimit();
    if (nTableLimit > 0)
    {
        osSQL += " LIMIT ";
        osSQL += CPLSPrintf("%d", 1 + nTableLimit);
    }

    auto oResult = SQLQuery(hDB, osSQL);
    if (oResult)
    {
        for (int i = 0; i < oResult->RowCount(); i++)
        {
            const char *pszTableName = oResult->GetValue(0, i);
            if (pszTableName == nullptr)
                continue;
            const char *pszDataType = oResult->GetValue(1, i);
            const char *pszIdentifier = oResult->GetValue(2, i);
            const char *pszDescription = oResult->GetValue(3, i);
            const char *pszMinX = oResult->GetValue(4, i);
            const char *pszMinY = oResult->GetValue(5, i);
            const char *pszMaxX = oResult->GetValue(6, i);
            const char *pszMaxY = oResult->GetValue(7, i);
            GPKGContentsDesc oDesc;
            if (pszDataType)
                oDesc.osDataType = pszDataType;
            if (pszIdentifier)
                oDesc.osIdentifier = pszIdentifier;
            if (pszDescription)
                oDesc.osDescription = pszDescription;
            if (pszMinX)
                oDesc.osMinX = pszMinX;
            if (pszMinY)
                oDesc.osMinY = pszMinY;
            if (pszMaxX)
                oDesc.osMaxX = pszMaxX;
            if (pszMaxY)
                oDesc.osMaxY = pszMaxY;
            m_oMapTableToContents[CPLString(pszTableName).toupper()] =
                std::move(oDesc);
        }
    }

    return m_oMapTableToContents;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int GDALGeoPackageDataset::Open(GDALOpenInfo *poOpenInfo,
                                const std::string &osFilenameInZip)
{
    m_osFilenameInZip = osFilenameInZip;
    CPLAssert(m_nLayers == 0);
    CPLAssert(hDB == nullptr);

    SetDescription(poOpenInfo->pszFilename);
    CPLString osFilename(poOpenInfo->pszFilename);
    CPLString osSubdatasetTableName;
    GByte abyHeaderLetMeHerePlease[100];
    const GByte *pabyHeader = poOpenInfo->pabyHeader;
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GPKG:"))
    {
        char **papszTokens = CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                                                CSLT_HONOURSTRINGS);
        int nCount = CSLCount(papszTokens);
        if (nCount < 2)
        {
            CSLDestroy(papszTokens);
            return FALSE;
        }

        if (nCount <= 3)
        {
            osFilename = papszTokens[1];
        }
        /* GPKG:C:\BLA.GPKG:foo */
        else if (nCount == 4 && strlen(papszTokens[1]) == 1 &&
                 (papszTokens[2][0] == '/' || papszTokens[2][0] == '\\'))
        {
            osFilename = CPLString(papszTokens[1]) + ":" + papszTokens[2];
        }
        // GPKG:/vsicurl/http[s]://[user:passwd@]example.com[:8080]/foo.gpkg:bar
        else if (/*nCount >= 4 && */
                 (EQUAL(papszTokens[1], "/vsicurl/http") ||
                  EQUAL(papszTokens[1], "/vsicurl/https")))
        {
            osFilename = CPLString(papszTokens[1]);
            for (int i = 2; i < nCount - 1; i++)
            {
                osFilename += ':';
                osFilename += papszTokens[i];
            }
        }
        if (nCount >= 3)
            osSubdatasetTableName = papszTokens[nCount - 1];

        CSLDestroy(papszTokens);
        VSILFILE *fp = VSIFOpenL(osFilename, "rb");
        if (fp != nullptr)
        {
            VSIFReadL(abyHeaderLetMeHerePlease, 1, 100, fp);
            VSIFCloseL(fp);
        }
        pabyHeader = abyHeaderLetMeHerePlease;
    }
    else if (poOpenInfo->pabyHeader &&
             STARTS_WITH(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                         "SQLite format 3"))
    {
        m_bCallUndeclareFileNotToOpen = true;
        GDALOpenInfoDeclareFileNotToOpen(osFilename, poOpenInfo->pabyHeader,
                                         poOpenInfo->nHeaderBytes);
    }

    eAccess = poOpenInfo->eAccess;
    if (!m_osFilenameInZip.empty())
    {
        m_pszFilename = CPLStrdup(CPLSPrintf(
            "/vsizip/{%s}/%s", osFilename.c_str(), m_osFilenameInZip.c_str()));
    }
    else
    {
        m_pszFilename = CPLStrdup(osFilename);
    }

    if (poOpenInfo->papszOpenOptions)
    {
        CSLDestroy(papszOpenOptions);
        papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);
    }

#ifdef ENABLE_SQL_GPKG_FORMAT
    if (poOpenInfo->pabyHeader &&
        STARTS_WITH(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                    "-- SQL GPKG") &&
        poOpenInfo->fpL != nullptr)
    {
        if (sqlite3_open_v2(":memory:", &hDB, SQLITE_OPEN_READWRITE, nullptr) !=
            SQLITE_OK)
        {
            return FALSE;
        }

        InstallSQLFunctions();

        // Ingest the lines of the dump
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        const char *pszLine;
        while ((pszLine = CPLReadLineL(poOpenInfo->fpL)) != nullptr)
        {
            if (STARTS_WITH(pszLine, "--"))
                continue;

            // Reject a few words tat might have security implications
            // Basically we just want to allow CREATE TABLE and INSERT INTO
            if (CPLString(pszLine).ifind("ATTACH") != std::string::npos ||
                CPLString(pszLine).ifind("DETACH") != std::string::npos ||
                CPLString(pszLine).ifind("PRAGMA") != std::string::npos ||
                CPLString(pszLine).ifind("SELECT") != std::string::npos ||
                CPLString(pszLine).ifind("UPDATE") != std::string::npos ||
                CPLString(pszLine).ifind("REPLACE") != std::string::npos ||
                CPLString(pszLine).ifind("DELETE") != std::string::npos ||
                CPLString(pszLine).ifind("DROP") != std::string::npos ||
                CPLString(pszLine).ifind("ALTER") != std::string::npos ||
                CPLString(pszLine).ifind("VIRTUAL") != std::string::npos)
            {
                bool bOK = false;
                // Accept creation of spatial index
                if (STARTS_WITH_CI(pszLine, "CREATE VIRTUAL TABLE "))
                {
                    const char *pszStr =
                        pszLine + strlen("CREATE VIRTUAL TABLE ");
                    if (*pszStr == '"')
                        pszStr++;
                    while ((*pszStr >= 'a' && *pszStr <= 'z') ||
                           (*pszStr >= 'A' && *pszStr <= 'Z') || *pszStr == '_')
                    {
                        pszStr++;
                    }
                    if (*pszStr == '"')
                        pszStr++;
                    if (EQUAL(pszStr,
                              " USING rtree(id, minx, maxx, miny, maxy);"))
                    {
                        bOK = true;
                    }
                }
                // Accept INSERT INTO rtree_poly_geom SELECT fid, ST_MinX(geom),
                // ST_MaxX(geom), ST_MinY(geom), ST_MaxY(geom) FROM poly;
                else if (STARTS_WITH_CI(pszLine, "INSERT INTO rtree_") &&
                         CPLString(pszLine).ifind("SELECT") !=
                             std::string::npos)
                {
                    char **papszTokens =
                        CSLTokenizeString2(pszLine, " (),,", 0);
                    if (CSLCount(papszTokens) == 15 &&
                        EQUAL(papszTokens[3], "SELECT") &&
                        EQUAL(papszTokens[5], "ST_MinX") &&
                        EQUAL(papszTokens[7], "ST_MaxX") &&
                        EQUAL(papszTokens[9], "ST_MinY") &&
                        EQUAL(papszTokens[11], "ST_MaxY") &&
                        EQUAL(papszTokens[13], "FROM"))
                    {
                        bOK = TRUE;
                    }
                    CSLDestroy(papszTokens);
                }

                if (!bOK)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Rejected statement: %s", pszLine);
                    return FALSE;
                }
            }
            char *pszErrMsg = nullptr;
            if (sqlite3_exec(hDB, pszLine, nullptr, nullptr, &pszErrMsg) !=
                SQLITE_OK)
            {
                if (pszErrMsg)
                    CPLDebug("SQLITE", "Error %s", pszErrMsg);
            }
            sqlite3_free(pszErrMsg);
        }
    }

    else if (pabyHeader != nullptr)
#endif
    {
        if (poOpenInfo->fpL)
        {
            // See above comment about -wal locking for the importance of
            // closing that file, prior to calling sqlite3_open()
            VSIFCloseL(poOpenInfo->fpL);
            poOpenInfo->fpL = nullptr;
        }

        /* See if we can open the SQLite database */
        if (!OpenOrCreateDB(GetUpdate() ? SQLITE_OPEN_READWRITE
                                        : SQLITE_OPEN_READONLY))
            return FALSE;

        memcpy(&m_nApplicationId, pabyHeader + knApplicationIdPos, 4);
        m_nApplicationId = CPL_MSBWORD32(m_nApplicationId);
        memcpy(&m_nUserVersion, pabyHeader + knUserVersionPos, 4);
        m_nUserVersion = CPL_MSBWORD32(m_nUserVersion);
        if (m_nApplicationId == GP10_APPLICATION_ID)
        {
            CPLDebug("GPKG", "GeoPackage v1.0");
        }
        else if (m_nApplicationId == GP11_APPLICATION_ID)
        {
            CPLDebug("GPKG", "GeoPackage v1.1");
        }
        else if (m_nApplicationId == GPKG_APPLICATION_ID &&
                 m_nUserVersion >= GPKG_1_2_VERSION)
        {
            CPLDebug("GPKG", "GeoPackage v%d.%d.%d", m_nUserVersion / 10000,
                     (m_nUserVersion % 10000) / 100, m_nUserVersion % 100);
        }
    }

    /* Requirement 6: The SQLite PRAGMA integrity_check SQL command SHALL return
     * “ok” */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    /* Disable integrity check by default, since it is expensive on big files */
    if (CPLTestBool(CPLGetConfigOption("OGR_GPKG_INTEGRITY_CHECK", "NO")) &&
        OGRERR_NONE != PragmaCheck("integrity_check", "ok", 1))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pragma integrity_check on '%s' failed", m_pszFilename);
        return FALSE;
    }

    /* Requirement 7: The SQLite PRAGMA foreign_key_check() SQL with no */
    /* parameter value SHALL return an empty result set */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    /* Disable the check by default, since it is to corrupt databases, and */
    /* that causes issues to downstream software that can't open them. */
    if (CPLTestBool(CPLGetConfigOption("OGR_GPKG_FOREIGN_KEY_CHECK", "NO")) &&
        OGRERR_NONE != PragmaCheck("foreign_key_check", "", 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pragma foreign_key_check on '%s' failed.", m_pszFilename);
        return FALSE;
    }

    /* Check for requirement metadata tables */
    /* Requirement 10: gpkg_spatial_ref_sys must exist */
    /* Requirement 13: gpkg_contents must exist */
    if (SQLGetInteger(hDB,
                      "SELECT COUNT(*) FROM sqlite_master WHERE "
                      "name IN ('gpkg_spatial_ref_sys', 'gpkg_contents') AND "
                      "type IN ('table', 'view')",
                      nullptr) != 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "At least one of the required GeoPackage tables, "
                 "gpkg_spatial_ref_sys or gpkg_contents, is missing");
        return FALSE;
    }

    DetectSpatialRefSysColumns();

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (SQLGetInteger(hDB,
                      "SELECT 1 FROM sqlite_master WHERE "
                      "name = 'gpkg_ogr_contents' AND type = 'table'",
                      nullptr) == 1)
    {
        m_bHasGPKGOGRContents = true;
    }
#endif

    CheckUnknownExtensions();

    int bRet = FALSE;
    bool bHasGPKGExtRelations = false;
    if (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR)
    {
        m_bHasGPKGGeometryColumns =
            SQLGetInteger(hDB,
                          "SELECT 1 FROM sqlite_master WHERE "
                          "name = 'gpkg_geometry_columns' AND "
                          "type IN ('table', 'view')",
                          nullptr) == 1;
        bHasGPKGExtRelations = HasGpkgextRelationsTable();
    }
    if (m_bHasGPKGGeometryColumns)
    {
        /* Load layer definitions for all tables in gpkg_contents &
         * gpkg_geometry_columns */
        /* and non-spatial tables as well */
        std::string osSQL =
            "SELECT c.table_name, c.identifier, 1 as is_spatial, "
            "g.column_name, g.geometry_type_name, g.z, g.m, c.min_x, c.min_y, "
            "c.max_x, c.max_y, 1 AS is_in_gpkg_contents, "
            "(SELECT type FROM sqlite_master WHERE lower(name) = "
            "lower(c.table_name) AND type IN ('table', 'view')) AS object_type "
            "  FROM gpkg_geometry_columns g "
            "  JOIN gpkg_contents c ON (g.table_name = c.table_name)"
            "  WHERE "
            "  c.table_name <> 'ogr_empty_table' AND"
            "  c.data_type = 'features' "
            // aspatial: Was the only method available in OGR 2.0 and 2.1
            // attributes: GPKG 1.2 or later
            "UNION ALL "
            "SELECT table_name, identifier, 0 as is_spatial, NULL, NULL, 0, 0, "
            "0 AS xmin, 0 AS ymin, 0 AS xmax, 0 AS ymax, 1 AS "
            "is_in_gpkg_contents, "
            "(SELECT type FROM sqlite_master WHERE lower(name) = "
            "lower(table_name) AND type IN ('table', 'view')) AS object_type "
            "  FROM gpkg_contents"
            "  WHERE data_type IN ('aspatial', 'attributes') ";

        const char *pszListAllTables = CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "LIST_ALL_TABLES", "AUTO");
        bool bHasASpatialOrAttributes = HasGDALAspatialExtension();
        if (!bHasASpatialOrAttributes)
        {
            auto oResultTable =
                SQLQuery(hDB, "SELECT * FROM gpkg_contents WHERE "
                              "data_type = 'attributes' LIMIT 1");
            bHasASpatialOrAttributes =
                (oResultTable && oResultTable->RowCount() == 1);
        }
        if (bHasGPKGExtRelations)
        {
            osSQL += "UNION ALL "
                     "SELECT mapping_table_name, mapping_table_name, 0 as "
                     "is_spatial, NULL, NULL, 0, 0, 0 AS "
                     "xmin, 0 AS ymin, 0 AS xmax, 0 AS ymax, 0 AS "
                     "is_in_gpkg_contents, 'table' AS object_type "
                     "FROM gpkgext_relations WHERE "
                     "lower(mapping_table_name) NOT IN (SELECT "
                     "lower(table_name) FROM "
                     "gpkg_contents)";
        }
        if (EQUAL(pszListAllTables, "YES") ||
            (!bHasASpatialOrAttributes && EQUAL(pszListAllTables, "AUTO")))
        {
            // vgpkg_ is Spatialite virtual table
            osSQL +=
                "UNION ALL "
                "SELECT name, name, 0 as is_spatial, NULL, NULL, 0, 0, 0 AS "
                "xmin, 0 AS ymin, 0 AS xmax, 0 AS ymax, 0 AS "
                "is_in_gpkg_contents, type AS object_type "
                "FROM sqlite_master WHERE type IN ('table', 'view') "
                "AND name NOT LIKE 'gpkg_%' "
                "AND name NOT LIKE 'vgpkg_%' "
                "AND name NOT LIKE 'rtree_%' AND name NOT LIKE 'sqlite_%' "
                // Avoid reading those views from simple_sewer_features.gpkg
                "AND name NOT IN ('st_spatial_ref_sys', 'spatial_ref_sys', "
                "'st_geometry_columns', 'geometry_columns') "
                "AND lower(name) NOT IN (SELECT lower(table_name) FROM "
                "gpkg_contents)";
            if (bHasGPKGExtRelations)
            {
                osSQL += " AND lower(name) NOT IN (SELECT "
                         "lower(mapping_table_name) FROM "
                         "gpkgext_relations)";
            }
        }
        const int nTableLimit = GetOGRTableLimit();
        if (nTableLimit > 0)
        {
            osSQL += " LIMIT ";
            osSQL += CPLSPrintf("%d", 1 + nTableLimit);
        }

        auto oResult = SQLQuery(hDB, osSQL.c_str());
        if (!oResult)
        {
            return FALSE;
        }

        if (nTableLimit > 0 && oResult->RowCount() > nTableLimit)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "File has more than %d vector tables. "
                     "Limiting to first %d (can be overridden with "
                     "OGR_TABLE_LIMIT config option)",
                     nTableLimit, nTableLimit);
            oResult->LimitRowCount(nTableLimit);
        }

        if (oResult->RowCount() > 0)
        {
            bRet = TRUE;

            m_papoLayers = static_cast<OGRGeoPackageTableLayer **>(CPLMalloc(
                sizeof(OGRGeoPackageTableLayer *) * oResult->RowCount()));

            std::map<std::string, int> oMapTableRefCount;
            for (int i = 0; i < oResult->RowCount(); i++)
            {
                const char *pszTableName = oResult->GetValue(0, i);
                if (pszTableName == nullptr)
                    continue;
                if (++oMapTableRefCount[pszTableName] == 2)
                {
                    // This should normally not happen if all constraints are
                    // properly set
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Table %s appearing several times in "
                             "gpkg_contents and/or gpkg_geometry_columns",
                             pszTableName);
                }
            }

            std::set<std::string> oExistingLayers;
            for (int i = 0; i < oResult->RowCount(); i++)
            {
                const char *pszTableName = oResult->GetValue(0, i);
                if (pszTableName == nullptr)
                    continue;
                const bool bTableHasSeveralGeomColumns =
                    oMapTableRefCount[pszTableName] > 1;
                bool bIsSpatial = CPL_TO_BOOL(oResult->GetValueAsInteger(2, i));
                const char *pszGeomColName = oResult->GetValue(3, i);
                const char *pszGeomType = oResult->GetValue(4, i);
                const char *pszZ = oResult->GetValue(5, i);
                const char *pszM = oResult->GetValue(6, i);
                bool bIsInGpkgContents =
                    CPL_TO_BOOL(oResult->GetValueAsInteger(11, i));
                if (!bIsInGpkgContents)
                    m_bNonSpatialTablesNonRegisteredInGpkgContentsFound = true;
                const char *pszObjectType = oResult->GetValue(12, i);
                if (pszObjectType == nullptr ||
                    !(EQUAL(pszObjectType, "table") ||
                      EQUAL(pszObjectType, "view")))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Table/view %s is referenced in gpkg_contents, "
                             "but does not exist",
                             pszTableName);
                    continue;
                }
                // Non-standard and undocumented behavior:
                // if the same table appears to have several geometry columns,
                // handle it for now as multiple layers named
                // "table_name (geom_col_name)"
                // The way we handle that might change in the future (e.g
                // could be a single layer with multiple geometry columns)
                const std::string osLayerNameWithGeomColName =
                    pszGeomColName ? std::string(pszTableName) + " (" +
                                         pszGeomColName + ')'
                                   : std::string(pszTableName);
                if (oExistingLayers.find(osLayerNameWithGeomColName) !=
                    oExistingLayers.end())
                    continue;
                oExistingLayers.insert(osLayerNameWithGeomColName);
                const std::string osLayerName = bTableHasSeveralGeomColumns
                                                    ? osLayerNameWithGeomColName
                                                    : std::string(pszTableName);
                OGRGeoPackageTableLayer *poLayer =
                    new OGRGeoPackageTableLayer(this, osLayerName.c_str());
                bool bHasZ = pszZ && atoi(pszZ) > 0;
                bool bHasM = pszM && atoi(pszM) > 0;
                if (pszGeomType && EQUAL(pszGeomType, "GEOMETRY"))
                {
                    if (pszZ && atoi(pszZ) == 2)
                        bHasZ = false;
                    if (pszM && atoi(pszM) == 2)
                        bHasM = false;
                }
                poLayer->SetOpeningParameters(
                    pszTableName, pszObjectType, bIsInGpkgContents, bIsSpatial,
                    pszGeomColName, pszGeomType, bHasZ, bHasM);
                m_papoLayers[m_nLayers++] = poLayer;
            }
        }
    }

    bool bHasTileMatrixSet = false;
    if (poOpenInfo->nOpenFlags & GDAL_OF_RASTER)
    {
        bHasTileMatrixSet = SQLGetInteger(hDB,
                                          "SELECT 1 FROM sqlite_master WHERE "
                                          "name = 'gpkg_tile_matrix_set' AND "
                                          "type IN ('table', 'view')",
                                          nullptr) == 1;
    }
    if (bHasTileMatrixSet)
    {
        std::string osSQL =
            "SELECT c.table_name, c.identifier, c.description, c.srs_id, "
            "c.min_x, c.min_y, c.max_x, c.max_y, "
            "tms.min_x, tms.min_y, tms.max_x, tms.max_y, c.data_type "
            "FROM gpkg_contents c JOIN gpkg_tile_matrix_set tms ON "
            "c.table_name = tms.table_name WHERE "
            "data_type IN ('tiles', '2d-gridded-coverage')";
        if (CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TABLE"))
            osSubdatasetTableName =
                CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TABLE");
        if (!osSubdatasetTableName.empty())
        {
            char *pszTmp = sqlite3_mprintf(" AND c.table_name='%q'",
                                           osSubdatasetTableName.c_str());
            osSQL += pszTmp;
            sqlite3_free(pszTmp);
            SetPhysicalFilename(osFilename.c_str());
        }
        const int nTableLimit = GetOGRTableLimit();
        if (nTableLimit > 0)
        {
            osSQL += " LIMIT ";
            osSQL += CPLSPrintf("%d", 1 + nTableLimit);
        }

        auto oResult = SQLQuery(hDB, osSQL.c_str());
        if (!oResult)
        {
            return FALSE;
        }

        if (oResult->RowCount() == 0 && !osSubdatasetTableName.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find table '%s' in GeoPackage dataset",
                     osSubdatasetTableName.c_str());
        }
        else if (oResult->RowCount() == 1)
        {
            const char *pszTableName = oResult->GetValue(0, 0);
            const char *pszIdentifier = oResult->GetValue(1, 0);
            const char *pszDescription = oResult->GetValue(2, 0);
            const char *pszSRSId = oResult->GetValue(3, 0);
            const char *pszMinX = oResult->GetValue(4, 0);
            const char *pszMinY = oResult->GetValue(5, 0);
            const char *pszMaxX = oResult->GetValue(6, 0);
            const char *pszMaxY = oResult->GetValue(7, 0);
            const char *pszTMSMinX = oResult->GetValue(8, 0);
            const char *pszTMSMinY = oResult->GetValue(9, 0);
            const char *pszTMSMaxX = oResult->GetValue(10, 0);
            const char *pszTMSMaxY = oResult->GetValue(11, 0);
            const char *pszDataType = oResult->GetValue(12, 0);
            if (pszTableName && pszTMSMinX && pszTMSMinY && pszTMSMaxX &&
                pszTMSMaxY)
            {
                bRet = OpenRaster(
                    pszTableName, pszIdentifier, pszDescription,
                    pszSRSId ? atoi(pszSRSId) : 0, CPLAtof(pszTMSMinX),
                    CPLAtof(pszTMSMinY), CPLAtof(pszTMSMaxX),
                    CPLAtof(pszTMSMaxY), pszMinX, pszMinY, pszMaxX, pszMaxY,
                    EQUAL(pszDataType, "tiles"), poOpenInfo->papszOpenOptions);
            }
        }
        else if (oResult->RowCount() >= 1)
        {
            bRet = TRUE;

            if (nTableLimit > 0 && oResult->RowCount() > nTableLimit)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "File has more than %d raster tables. "
                         "Limiting to first %d (can be overridden with "
                         "OGR_TABLE_LIMIT config option)",
                         nTableLimit, nTableLimit);
                oResult->LimitRowCount(nTableLimit);
            }

            int nSDSCount = 0;
            for (int i = 0; i < oResult->RowCount(); i++)
            {
                const char *pszTableName = oResult->GetValue(0, i);
                const char *pszIdentifier = oResult->GetValue(1, i);
                if (pszTableName == nullptr)
                    continue;
                m_aosSubDatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", nSDSCount + 1),
                    CPLSPrintf("GPKG:%s:%s", m_pszFilename, pszTableName));
                m_aosSubDatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_DESC", nSDSCount + 1),
                    pszIdentifier
                        ? CPLSPrintf("%s - %s", pszTableName, pszIdentifier)
                        : pszTableName);
                nSDSCount++;
            }
        }
    }

    if (!bRet && (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR))
    {
        if ((poOpenInfo->nOpenFlags & GDAL_OF_UPDATE))
        {
            bRet = TRUE;
        }
        else
        {
            CPLDebug("GPKG",
                     "This GeoPackage has no vector content and is opened "
                     "in read-only mode. If you open it in update mode, "
                     "opening will be successful.");
        }
    }

    if (eAccess == GA_Update)
    {
        FixupWrongRTreeTrigger();
        FixupWrongMedataReferenceColumnNameUpdate();
    }

    SetPamFlags(GetPamFlags() & ~GPF_DIRTY);

    return bRet;
}

/************************************************************************/
/*                    DetectSpatialRefSysColumns()                      */
/************************************************************************/

void GDALGeoPackageDataset::DetectSpatialRefSysColumns()
{
    // Detect definition_12_063 column
    {
        sqlite3_stmt *hSQLStmt = nullptr;
        int rc = sqlite3_prepare_v2(
            hDB, "SELECT definition_12_063 FROM gpkg_spatial_ref_sys ", -1,
            &hSQLStmt, nullptr);
        if (rc == SQLITE_OK)
        {
            m_bHasDefinition12_063 = true;
            sqlite3_finalize(hSQLStmt);
        }
    }

    // Detect epoch column
    if (m_bHasDefinition12_063)
    {
        sqlite3_stmt *hSQLStmt = nullptr;
        int rc =
            sqlite3_prepare_v2(hDB, "SELECT epoch FROM gpkg_spatial_ref_sys ",
                               -1, &hSQLStmt, nullptr);
        if (rc == SQLITE_OK)
        {
            m_bHasEpochColumn = true;
            sqlite3_finalize(hSQLStmt);
        }
    }
}

/************************************************************************/
/*                    FixupWrongRTreeTrigger()                          */
/************************************************************************/

void GDALGeoPackageDataset::FixupWrongRTreeTrigger()
{
    auto oResult = SQLQuery(
        hDB,
        "SELECT name, sql FROM sqlite_master WHERE type = 'trigger' AND "
        "NAME LIKE 'rtree_%_update3' AND sql LIKE '% AFTER UPDATE OF % ON %'");
    if (oResult == nullptr)
        return;
    if (oResult->RowCount() > 0)
    {
        CPLDebug("GPKG", "Fixing incorrect trigger(s) related to RTree");
    }
    for (int i = 0; i < oResult->RowCount(); i++)
    {
        const char *pszName = oResult->GetValue(0, i);
        const char *pszSQL = oResult->GetValue(1, i);
        const char *pszPtr1 = strstr(pszSQL, " AFTER UPDATE OF ");
        if (pszPtr1)
        {
            const char *pszPtr = pszPtr1 + strlen(" AFTER UPDATE OF ");
            // Skipping over geometry column name
            while (*pszPtr == ' ')
                pszPtr++;
            if (pszPtr[0] == '"' || pszPtr[0] == '\'')
            {
                char chStringDelim = pszPtr[0];
                pszPtr++;
                while (*pszPtr != '\0' && *pszPtr != chStringDelim)
                {
                    if (*pszPtr == '\\' && pszPtr[1] == chStringDelim)
                        pszPtr += 2;
                    else
                        pszPtr += 1;
                }
                if (*pszPtr == chStringDelim)
                    pszPtr++;
            }
            else
            {
                pszPtr++;
                while (*pszPtr != ' ')
                    pszPtr++;
            }
            if (*pszPtr == ' ')
            {
                SQLCommand(hDB,
                           ("DROP TRIGGER \"" + SQLEscapeName(pszName) + "\"")
                               .c_str());
                CPLString newSQL;
                newSQL.assign(pszSQL, pszPtr1 - pszSQL);
                newSQL += " AFTER UPDATE";
                newSQL += pszPtr;
                SQLCommand(hDB, newSQL);
            }
        }
    }
}

/************************************************************************/
/*             FixupWrongMedataReferenceColumnNameUpdate()              */
/************************************************************************/

void GDALGeoPackageDataset::FixupWrongMedataReferenceColumnNameUpdate()
{
    // Fix wrong trigger that was generated by GDAL < 2.4.0
    // See https://github.com/qgis/QGIS/issues/42768
    auto oResult = SQLQuery(
        hDB, "SELECT sql FROM sqlite_master WHERE type = 'trigger' AND "
             "NAME ='gpkg_metadata_reference_column_name_update' AND "
             "sql LIKE '%column_nameIS%'");
    if (oResult == nullptr)
        return;
    if (oResult->RowCount() == 1)
    {
        CPLDebug("GPKG", "Fixing incorrect trigger "
                         "gpkg_metadata_reference_column_name_update");
        const char *pszSQL = oResult->GetValue(0, 0);
        std::string osNewSQL(
            CPLString(pszSQL).replaceAll("column_nameIS", "column_name IS"));

        SQLCommand(hDB,
                   "DROP TRIGGER gpkg_metadata_reference_column_name_update");
        SQLCommand(hDB, osNewSQL.c_str());
    }
}

/************************************************************************/
/*                  ClearCachedRelationships()                          */
/************************************************************************/

void GDALGeoPackageDataset::ClearCachedRelationships()
{
    m_bHasPopulatedRelationships = false;
    m_osMapRelationships.clear();
}

/************************************************************************/
/*                           LoadRelationships()                        */
/************************************************************************/

void GDALGeoPackageDataset::LoadRelationships() const
{
    m_osMapRelationships.clear();

    std::vector<std::string> oExcludedTables;
    if (HasGpkgextRelationsTable())
    {
        LoadRelationshipsUsingRelatedTablesExtension();

        for (const auto &oRelationship : m_osMapRelationships)
        {
            oExcludedTables.emplace_back(
                oRelationship.second->GetMappingTableName());
        }
    }

    // Also load relationships defined using foreign keys (i.e. one-to-many
    // relationships). Here we must exclude any relationships defined from the
    // related tables extension, we don't want them included twice.
    LoadRelationshipsFromForeignKeys(oExcludedTables);
    m_bHasPopulatedRelationships = true;
}

/************************************************************************/
/*         LoadRelationshipsUsingRelatedTablesExtension()               */
/************************************************************************/

void GDALGeoPackageDataset::LoadRelationshipsUsingRelatedTablesExtension() const
{
    m_osMapRelationships.clear();

    auto oResultTable = SQLQuery(
        hDB, "SELECT base_table_name, base_primary_column, "
             "related_table_name, related_primary_column, relation_name, "
             "mapping_table_name FROM gpkgext_relations");
    if (oResultTable && oResultTable->RowCount() > 0)
    {
        for (int i = 0; i < oResultTable->RowCount(); i++)
        {
            const char *pszBaseTableName = oResultTable->GetValue(0, i);
            if (!pszBaseTableName)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Could not retrieve base_table_name from "
                         "gpkgext_relations");
                continue;
            }
            const char *pszBasePrimaryColumn = oResultTable->GetValue(1, i);
            if (!pszBasePrimaryColumn)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Could not retrieve base_primary_column from "
                         "gpkgext_relations");
                continue;
            }
            const char *pszRelatedTableName = oResultTable->GetValue(2, i);
            if (!pszRelatedTableName)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Could not retrieve related_table_name from "
                         "gpkgext_relations");
                continue;
            }
            const char *pszRelatedPrimaryColumn = oResultTable->GetValue(3, i);
            if (!pszRelatedPrimaryColumn)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Could not retrieve related_primary_column from "
                         "gpkgext_relations");
                continue;
            }
            const char *pszRelationName = oResultTable->GetValue(4, i);
            if (!pszRelationName)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Could not retrieve relation_name from gpkgext_relations");
                continue;
            }
            const char *pszMappingTableName = oResultTable->GetValue(5, i);
            if (!pszMappingTableName)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Could not retrieve mapping_table_name from "
                         "gpkgext_relations");
                continue;
            }

            // confirm that mapping table exists
            char *pszSQL =
                sqlite3_mprintf("SELECT 1 FROM sqlite_master WHERE "
                                "name='%q' AND type IN ('table', 'view')",
                                pszMappingTableName);
            const int nMappingTableCount = SQLGetInteger(hDB, pszSQL, nullptr);
            sqlite3_free(pszSQL);

            if (nMappingTableCount < 1)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Relationship mapping table %s does not exist",
                         pszMappingTableName);
                continue;
            }

            const std::string osRelationName = GenerateNameForRelationship(
                pszBaseTableName, pszRelatedTableName, pszRelationName);

            std::string osType{};
            // defined requirement classes -- for these types the relation name
            // will be specific string value from the related tables extension.
            // In this case we need to construct a unique relationship name
            // based on the related tables
            if (EQUAL(pszRelationName, "media") ||
                EQUAL(pszRelationName, "simple_attributes") ||
                EQUAL(pszRelationName, "features") ||
                EQUAL(pszRelationName, "attributes") ||
                EQUAL(pszRelationName, "tiles"))
            {
                osType = pszRelationName;
            }
            else
            {
                // user defined types default to features
                osType = "features";
            }

            std::unique_ptr<GDALRelationship> poRelationship(
                new GDALRelationship(osRelationName, pszBaseTableName,
                                     pszRelatedTableName, GRC_MANY_TO_MANY));

            poRelationship->SetLeftTableFields({pszBasePrimaryColumn});
            poRelationship->SetRightTableFields({pszRelatedPrimaryColumn});
            poRelationship->SetLeftMappingTableFields({"base_id"});
            poRelationship->SetRightMappingTableFields({"related_id"});
            poRelationship->SetMappingTableName(pszMappingTableName);
            poRelationship->SetRelatedTableType(osType);

            m_osMapRelationships[osRelationName] = std::move(poRelationship);
        }
    }
}

/************************************************************************/
/*                GenerateNameForRelationship()                         */
/************************************************************************/

std::string GDALGeoPackageDataset::GenerateNameForRelationship(
    const char *pszBaseTableName, const char *pszRelatedTableName,
    const char *pszType)
{
    // defined requirement classes -- for these types the relation name will be
    // specific string value from the related tables extension. In this case we
    // need to construct a unique relationship name based on the related tables
    if (EQUAL(pszType, "media") || EQUAL(pszType, "simple_attributes") ||
        EQUAL(pszType, "features") || EQUAL(pszType, "attributes") ||
        EQUAL(pszType, "tiles"))
    {
        std::ostringstream stream;
        stream << pszBaseTableName << '_' << pszRelatedTableName << '_'
               << pszType;
        return stream.str();
    }
    else
    {
        // user defined types default to features
        return pszType;
    }
}

/************************************************************************/
/*                       ValidateRelationship()                         */
/************************************************************************/

bool GDALGeoPackageDataset::ValidateRelationship(
    const GDALRelationship *poRelationship, std::string &failureReason)
{

    if (poRelationship->GetCardinality() !=
        GDALRelationshipCardinality::GRC_MANY_TO_MANY)
    {
        failureReason = "Only many to many relationships are supported";
        return false;
    }

    std::string osRelatedTableType = poRelationship->GetRelatedTableType();
    if (!osRelatedTableType.empty() && osRelatedTableType != "features" &&
        osRelatedTableType != "media" &&
        osRelatedTableType != "simple_attributes" &&
        osRelatedTableType != "attributes" && osRelatedTableType != "tiles")
    {
        failureReason =
            ("Related table type " + osRelatedTableType +
             " is not a valid value for the GeoPackage specification. "
             "Valid values are: features, media, simple_attributes, "
             "attributes, tiles.")
                .c_str();
        return false;
    }

    const std::string &osLeftTableName = poRelationship->GetLeftTableName();
    OGRGeoPackageLayer *poLeftTable = cpl::down_cast<OGRGeoPackageLayer *>(
        GetLayerByName(osLeftTableName.c_str()));
    if (!poLeftTable)
    {
        failureReason = ("Left table " + osLeftTableName +
                         " is not an existing layer in the dataset")
                            .c_str();
        return false;
    }
    const std::string &osRightTableName = poRelationship->GetRightTableName();
    OGRGeoPackageLayer *poRightTable = cpl::down_cast<OGRGeoPackageLayer *>(
        GetLayerByName(osRightTableName.c_str()));
    if (!poRightTable)
    {
        failureReason = ("Right table " + osRightTableName +
                         " is not an existing layer in the dataset")
                            .c_str();
        return false;
    }

    const auto &aosLeftTableFields = poRelationship->GetLeftTableFields();
    if (aosLeftTableFields.empty())
    {
        failureReason = "No left table fields were specified";
        return false;
    }
    else if (aosLeftTableFields.size() > 1)
    {
        failureReason = "Only a single left table field is permitted for the "
                        "GeoPackage specification";
        return false;
    }
    else
    {
        // validate left field exists
        if (poLeftTable->GetLayerDefn()->GetFieldIndex(
                aosLeftTableFields[0].c_str()) < 0 &&
            !EQUAL(poLeftTable->GetFIDColumn(), aosLeftTableFields[0].c_str()))
        {
            failureReason = ("Left table field " + aosLeftTableFields[0] +
                             " does not exist in " + osLeftTableName)
                                .c_str();
            return false;
        }
    }

    const auto &aosRightTableFields = poRelationship->GetRightTableFields();
    if (aosRightTableFields.empty())
    {
        failureReason = "No right table fields were specified";
        return false;
    }
    else if (aosRightTableFields.size() > 1)
    {
        failureReason = "Only a single right table field is permitted for the "
                        "GeoPackage specification";
        return false;
    }
    else
    {
        // validate right field exists
        if (poRightTable->GetLayerDefn()->GetFieldIndex(
                aosRightTableFields[0].c_str()) < 0 &&
            !EQUAL(poRightTable->GetFIDColumn(),
                   aosRightTableFields[0].c_str()))
        {
            failureReason = ("Right table field " + aosRightTableFields[0] +
                             " does not exist in " + osRightTableName)
                                .c_str();
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

bool GDALGeoPackageDataset::InitRaster(
    GDALGeoPackageDataset *poParentDS, const char *pszTableName, double dfMinX,
    double dfMinY, double dfMaxX, double dfMaxY, const char *pszContentsMinX,
    const char *pszContentsMinY, const char *pszContentsMaxX,
    const char *pszContentsMaxY, char **papszOpenOptionsIn,
    const SQLResult &oResult, int nIdxInResult)
{
    m_osRasterTable = pszTableName;
    m_dfTMSMinX = dfMinX;
    m_dfTMSMaxY = dfMaxY;

    // Despite prior checking, the type might be Binary and
    // SQLResultGetValue() not working properly on it
    int nZoomLevel = atoi(oResult.GetValue(0, nIdxInResult));
    if (nZoomLevel < 0 || nZoomLevel > 65536)
    {
        return false;
    }
    double dfPixelXSize = CPLAtof(oResult.GetValue(1, nIdxInResult));
    double dfPixelYSize = CPLAtof(oResult.GetValue(2, nIdxInResult));
    if (dfPixelXSize <= 0 || dfPixelYSize <= 0)
    {
        return false;
    }
    int nTileWidth = atoi(oResult.GetValue(3, nIdxInResult));
    int nTileHeight = atoi(oResult.GetValue(4, nIdxInResult));
    if (nTileWidth <= 0 || nTileWidth > 65536 || nTileHeight <= 0 ||
        nTileHeight > 65536)
    {
        return false;
    }
    int nTileMatrixWidth = static_cast<int>(
        std::min(static_cast<GIntBig>(INT_MAX),
                 CPLAtoGIntBig(oResult.GetValue(5, nIdxInResult))));
    int nTileMatrixHeight = static_cast<int>(
        std::min(static_cast<GIntBig>(INT_MAX),
                 CPLAtoGIntBig(oResult.GetValue(6, nIdxInResult))));
    if (nTileMatrixWidth <= 0 || nTileMatrixHeight <= 0)
    {
        return false;
    }

    /* Use content bounds in priority over tile_matrix_set bounds */
    double dfGDALMinX = dfMinX;
    double dfGDALMinY = dfMinY;
    double dfGDALMaxX = dfMaxX;
    double dfGDALMaxY = dfMaxY;
    pszContentsMinX =
        CSLFetchNameValueDef(papszOpenOptionsIn, "MINX", pszContentsMinX);
    pszContentsMinY =
        CSLFetchNameValueDef(papszOpenOptionsIn, "MINY", pszContentsMinY);
    pszContentsMaxX =
        CSLFetchNameValueDef(papszOpenOptionsIn, "MAXX", pszContentsMaxX);
    pszContentsMaxY =
        CSLFetchNameValueDef(papszOpenOptionsIn, "MAXY", pszContentsMaxY);
    if (pszContentsMinX != nullptr && pszContentsMinY != nullptr &&
        pszContentsMaxX != nullptr && pszContentsMaxY != nullptr)
    {
        if (CPLAtof(pszContentsMinX) < CPLAtof(pszContentsMaxX) &&
            CPLAtof(pszContentsMinY) < CPLAtof(pszContentsMaxY))
        {
            dfGDALMinX = CPLAtof(pszContentsMinX);
            dfGDALMinY = CPLAtof(pszContentsMinY);
            dfGDALMaxX = CPLAtof(pszContentsMaxX);
            dfGDALMaxY = CPLAtof(pszContentsMaxY);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Illegal min_x/min_y/max_x/max_y values for %s in open "
                     "options and/or gpkg_contents. Using bounds of "
                     "gpkg_tile_matrix_set instead",
                     pszTableName);
        }
    }
    if (dfGDALMinX >= dfGDALMaxX || dfGDALMinY >= dfGDALMaxY)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Illegal min_x/min_y/max_x/max_y values for %s", pszTableName);
        return false;
    }

    int nBandCount = 0;
    const char *pszBAND_COUNT =
        CSLFetchNameValue(papszOpenOptionsIn, "BAND_COUNT");
    if (poParentDS)
    {
        nBandCount = poParentDS->GetRasterCount();
    }
    else if (m_eDT != GDT_Byte)
    {
        if (pszBAND_COUNT != nullptr && !EQUAL(pszBAND_COUNT, "AUTO") &&
            !EQUAL(pszBAND_COUNT, "1"))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "BAND_COUNT ignored for non-Byte data");
        }
        nBandCount = 1;
    }
    else
    {
        if (pszBAND_COUNT != nullptr && !EQUAL(pszBAND_COUNT, "AUTO"))
        {
            nBandCount = atoi(pszBAND_COUNT);
            if (nBandCount == 1)
                GetMetadata("IMAGE_STRUCTURE");
        }
        else
        {
            GetMetadata("IMAGE_STRUCTURE");
            nBandCount = m_nBandCountFromMetadata;
            if (nBandCount == 1)
                m_eTF = GPKG_TF_PNG;
        }
        if (nBandCount == 1 && !m_osTFFromMetadata.empty())
        {
            m_eTF = GDALGPKGMBTilesGetTileFormat(m_osTFFromMetadata.c_str());
        }
        if (nBandCount <= 0 || nBandCount > 4)
            nBandCount = 4;
    }

    return InitRaster(poParentDS, pszTableName, nZoomLevel, nBandCount, dfMinX,
                      dfMaxY, dfPixelXSize, dfPixelYSize, nTileWidth,
                      nTileHeight, nTileMatrixWidth, nTileMatrixHeight,
                      dfGDALMinX, dfGDALMinY, dfGDALMaxX, dfGDALMaxY);
}

/************************************************************************/
/*                      ComputeTileAndPixelShifts()                     */
/************************************************************************/

bool GDALGeoPackageDataset::ComputeTileAndPixelShifts()
{
    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);

    // Compute shift between GDAL origin and TileMatrixSet origin
    const double dfShiftXPixels =
        (m_adfGeoTransform[0] - m_dfTMSMinX) / m_adfGeoTransform[1];
    if (dfShiftXPixels / nTileWidth <= INT_MIN ||
        dfShiftXPixels / nTileWidth > INT_MAX)
        return false;
    const int64_t nShiftXPixels =
        static_cast<int64_t>(floor(0.5 + dfShiftXPixels));
    m_nShiftXTiles = static_cast<int>(nShiftXPixels / nTileWidth);
    if (nShiftXPixels < 0 && (nShiftXPixels % nTileWidth) != 0)
        m_nShiftXTiles--;
    m_nShiftXPixelsMod =
        (static_cast<int>(nShiftXPixels % nTileWidth) + nTileWidth) %
        nTileWidth;

    const double dfShiftYPixels =
        (m_adfGeoTransform[3] - m_dfTMSMaxY) / m_adfGeoTransform[5];
    if (dfShiftYPixels / nTileHeight <= INT_MIN ||
        dfShiftYPixels / nTileHeight > INT_MAX)
        return false;
    const int64_t nShiftYPixels =
        static_cast<int64_t>(floor(0.5 + dfShiftYPixels));
    m_nShiftYTiles = static_cast<int>(nShiftYPixels / nTileHeight);
    if (nShiftYPixels < 0 && (nShiftYPixels % nTileHeight) != 0)
        m_nShiftYTiles--;
    m_nShiftYPixelsMod =
        (static_cast<int>(nShiftYPixels % nTileHeight) + nTileHeight) %
        nTileHeight;
    return true;
}

/************************************************************************/
/*                            AllocCachedTiles()                        */
/************************************************************************/

bool GDALGeoPackageDataset::AllocCachedTiles()
{
    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);

    // We currently need 4 caches because of
    // GDALGPKGMBTilesLikePseudoDataset::ReadTile(int nRow, int nCol)
    const int nCacheCount = 4;
    /*
            (m_nShiftXPixelsMod != 0 || m_nShiftYPixelsMod != 0) ? 4 :
            (GetUpdate() && m_eDT == GDT_Byte) ? 2 : 1;
    */
    m_pabyCachedTiles = static_cast<GByte *>(VSI_MALLOC3_VERBOSE(
        cpl::fits_on<int>(nCacheCount * (m_eDT == GDT_Byte ? 4 : 1) *
                          m_nDTSize),
        nTileWidth, nTileHeight));
    if (m_pabyCachedTiles == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too big tiles: %d x %d",
                 nTileWidth, nTileHeight);
        return false;
    }

    return true;
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

bool GDALGeoPackageDataset::InitRaster(
    GDALGeoPackageDataset *poParentDS, const char *pszTableName, int nZoomLevel,
    int nBandCount, double dfTMSMinX, double dfTMSMaxY, double dfPixelXSize,
    double dfPixelYSize, int nTileWidth, int nTileHeight, int nTileMatrixWidth,
    int nTileMatrixHeight, double dfGDALMinX, double dfGDALMinY,
    double dfGDALMaxX, double dfGDALMaxY)
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
    if (dfRasterXSize > INT_MAX || dfRasterYSize > INT_MAX)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too big raster: %f x %f",
                 dfRasterXSize, dfRasterYSize);
        return false;
    }
    nRasterXSize = std::max(1, static_cast<int>(dfRasterXSize));
    nRasterYSize = std::max(1, static_cast<int>(dfRasterYSize));

    if (poParentDS)
    {
        m_poParentDS = poParentDS;
        eAccess = poParentDS->eAccess;
        hDB = poParentDS->hDB;
        m_eTF = poParentDS->m_eTF;
        m_eDT = poParentDS->m_eDT;
        m_nDTSize = poParentDS->m_nDTSize;
        m_dfScale = poParentDS->m_dfScale;
        m_dfOffset = poParentDS->m_dfOffset;
        m_dfPrecision = poParentDS->m_dfPrecision;
        m_usGPKGNull = poParentDS->m_usGPKGNull;
        m_nQuality = poParentDS->m_nQuality;
        m_nZLevel = poParentDS->m_nZLevel;
        m_bDither = poParentDS->m_bDither;
        /*m_nSRID = poParentDS->m_nSRID;*/
        m_osWHERE = poParentDS->m_osWHERE;
        SetDescription(CPLSPrintf("%s - zoom_level=%d",
                                  poParentDS->GetDescription(), m_nZoomLevel));
    }

    for (int i = 1; i <= nBandCount; i++)
    {
        GDALGeoPackageRasterBand *poNewBand =
            new GDALGeoPackageRasterBand(this, nTileWidth, nTileHeight);
        if (poParentDS)
        {
            int bHasNoData = FALSE;
            double dfNoDataValue =
                poParentDS->GetRasterBand(1)->GetNoDataValue(&bHasNoData);
            if (bHasNoData)
                poNewBand->SetNoDataValueInternal(dfNoDataValue);
        }
        SetBand(i, poNewBand);

        if (nBandCount == 1 && m_poCTFromMetadata)
        {
            poNewBand->AssignColorTable(m_poCTFromMetadata.get());
        }
        if (!m_osNodataValueFromMetadata.empty())
        {
            poNewBand->SetNoDataValueInternal(
                CPLAtof(m_osNodataValueFromMetadata.c_str()));
        }
    }

    if (!ComputeTileAndPixelShifts())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Overflow occurred in ComputeTileAndPixelShifts()");
        return false;
    }

    GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    GDALPamDataset::SetMetadataItem("ZOOM_LEVEL",
                                    CPLSPrintf("%d", m_nZoomLevel));

    return AllocCachedTiles();
}

/************************************************************************/
/*                 GDALGPKGMBTilesGetTileFormat()                       */
/************************************************************************/

GPKGTileFormat GDALGPKGMBTilesGetTileFormat(const char *pszTF)
{
    GPKGTileFormat eTF = GPKG_TF_PNG_JPEG;
    if (pszTF)
    {
        if (EQUAL(pszTF, "PNG_JPEG") || EQUAL(pszTF, "AUTO"))
            eTF = GPKG_TF_PNG_JPEG;
        else if (EQUAL(pszTF, "PNG"))
            eTF = GPKG_TF_PNG;
        else if (EQUAL(pszTF, "PNG8"))
            eTF = GPKG_TF_PNG8;
        else if (EQUAL(pszTF, "JPEG"))
            eTF = GPKG_TF_JPEG;
        else if (EQUAL(pszTF, "WEBP"))
            eTF = GPKG_TF_WEBP;
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsuppoted value for TILE_FORMAT: %s", pszTF);
        }
    }
    return eTF;
}

const char *GDALMBTilesGetTileFormatName(GPKGTileFormat eTF)
{
    switch (eTF)
    {
        case GPKG_TF_PNG:
        case GPKG_TF_PNG8:
            return "png";
        case GPKG_TF_JPEG:
            return "jpg";
        case GPKG_TF_WEBP:
            return "webp";
        default:
            break;
    }
    CPLError(CE_Failure, CPLE_NotSupported,
             "Unsuppoted value for TILE_FORMAT: %d", static_cast<int>(eTF));
    return nullptr;
}

/************************************************************************/
/*                         OpenRaster()                                 */
/************************************************************************/

bool GDALGeoPackageDataset::OpenRaster(
    const char *pszTableName, const char *pszIdentifier,
    const char *pszDescription, int nSRSId, double dfMinX, double dfMinY,
    double dfMaxX, double dfMaxY, const char *pszContentsMinX,
    const char *pszContentsMinY, const char *pszContentsMaxX,
    const char *pszContentsMaxY, bool bIsTiles, char **papszOpenOptionsIn)
{
    if (dfMinX >= dfMaxX || dfMinY >= dfMaxY)
        return false;

    // Config option just for debug, and for example force set to NaN
    // which is not supported
    CPLString osDataNull = CPLGetConfigOption("GPKG_NODATA", "");
    CPLString osUom;
    CPLString osFieldName;
    CPLString osGridCellEncoding;
    if (!bIsTiles)
    {
        char *pszSQL = sqlite3_mprintf(
            "SELECT datatype, scale, offset, data_null, precision FROM "
            "gpkg_2d_gridded_coverage_ancillary "
            "WHERE tile_matrix_set_name = '%q' "
            "AND datatype IN ('integer', 'float')"
            "AND (scale > 0 OR scale IS NULL)",
            pszTableName);
        auto oResult = SQLQuery(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (!oResult || oResult->RowCount() == 0)
        {
            return false;
        }
        const char *pszDataType = oResult->GetValue(0, 0);
        const char *pszScale = oResult->GetValue(1, 0);
        const char *pszOffset = oResult->GetValue(2, 0);
        const char *pszDataNull = oResult->GetValue(3, 0);
        const char *pszPrecision = oResult->GetValue(4, 0);
        if (pszDataNull)
            osDataNull = pszDataNull;
        if (EQUAL(pszDataType, "float"))
        {
            SetDataType(GDT_Float32);
            m_eTF = GPKG_TF_TIFF_32BIT_FLOAT;
        }
        else
        {
            SetDataType(GDT_Float32);
            m_eTF = GPKG_TF_PNG_16BIT;
            const double dfScale = pszScale ? CPLAtof(pszScale) : 1.0;
            const double dfOffset = pszOffset ? CPLAtof(pszOffset) : 0.0;
            if (dfScale == 1.0)
            {
                if (dfOffset == 0.0)
                {
                    SetDataType(GDT_UInt16);
                }
                else if (dfOffset == -32768.0)
                {
                    SetDataType(GDT_Int16);
                }
                // coverity[tainted_data]
                else if (dfOffset == -32767.0 && !osDataNull.empty() &&
                         CPLAtof(osDataNull) == 65535.0)
                // Given that we will map the nodata value to -32768
                {
                    SetDataType(GDT_Int16);
                }
            }

            // Check that the tile offset and scales are compatible of a
            // final integer result.
            if (m_eDT != GDT_Float32)
            {
                // coverity[tainted_data]
                if (dfScale == 1.0 && dfOffset == -32768.0 &&
                    !osDataNull.empty() && CPLAtof(osDataNull) == 65535.0)
                {
                    // Given that we will map the nodata value to -32768
                    pszSQL = sqlite3_mprintf(
                        "SELECT 1 FROM "
                        "gpkg_2d_gridded_tile_ancillary WHERE "
                        "tpudt_name = '%q' "
                        "AND NOT ((offset = 0.0 or offset = 1.0) "
                        "AND scale = 1.0) "
                        "LIMIT 1",
                        pszTableName);
                }
                else
                {
                    pszSQL = sqlite3_mprintf(
                        "SELECT 1 FROM "
                        "gpkg_2d_gridded_tile_ancillary WHERE "
                        "tpudt_name = '%q' "
                        "AND NOT (offset = 0.0 AND scale = 1.0) LIMIT 1",
                        pszTableName);
                }
                sqlite3_stmt *hSQLStmt = nullptr;
                int rc =
                    sqlite3_prepare_v2(hDB, pszSQL, -1, &hSQLStmt, nullptr);

                if (rc == SQLITE_OK)
                {
                    if (sqlite3_step(hSQLStmt) == SQLITE_ROW)
                    {
                        SetDataType(GDT_Float32);
                    }
                    sqlite3_finalize(hSQLStmt);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error when running %s", pszSQL);
                }
                sqlite3_free(pszSQL);
            }

            SetGlobalOffsetScale(dfOffset, dfScale);
        }
        if (pszPrecision)
            m_dfPrecision = CPLAtof(pszPrecision);

        // Request those columns in a separate query, so as to keep
        // compatibility with pre OGC 17-066r1 databases
        pszSQL =
            sqlite3_mprintf("SELECT uom, field_name, grid_cell_encoding FROM "
                            "gpkg_2d_gridded_coverage_ancillary "
                            "WHERE tile_matrix_set_name = '%q'",
                            pszTableName);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        oResult = SQLQuery(hDB, pszSQL);
        CPLPopErrorHandler();
        sqlite3_free(pszSQL);
        if (oResult && oResult->RowCount() == 1)
        {
            const char *pszUom = oResult->GetValue(0, 0);
            if (pszUom)
                osUom = pszUom;
            const char *pszFieldName = oResult->GetValue(1, 0);
            if (pszFieldName)
                osFieldName = pszFieldName;
            const char *pszGridCellEncoding = oResult->GetValue(2, 0);
            if (pszGridCellEncoding)
                osGridCellEncoding = pszGridCellEncoding;
        }
    }

    m_bRecordInsertedInGPKGContent = true;
    m_nSRID = nSRSId;

    OGRSpatialReference *poSRS = GetSpatialRef(nSRSId);
    if (poSRS)
    {
        m_oSRS = *poSRS;
        poSRS->Release();
    }

    /* Various sanity checks added in the SELECT */
    char *pszQuotedTableName = sqlite3_mprintf("'%q'", pszTableName);
    CPLString osQuotedTableName(pszQuotedTableName);
    sqlite3_free(pszQuotedTableName);
    char *pszSQL = sqlite3_mprintf(
        "SELECT zoom_level, pixel_x_size, pixel_y_size, tile_width, "
        "tile_height, matrix_width, matrix_height "
        "FROM gpkg_tile_matrix tm "
        "WHERE table_name = %s "
        // INT_MAX would be the theoretical maximum value to avoid
        // overflows, but that's already a insane value.
        "AND zoom_level >= 0 AND zoom_level <= 65536 "
        "AND pixel_x_size > 0 AND pixel_y_size > 0 "
        "AND tile_width >= 1 AND tile_width <= 65536 "
        "AND tile_height >= 1 AND tile_height <= 65536 "
        "AND matrix_width >= 1 AND matrix_height >= 1",
        osQuotedTableName.c_str());
    CPLString osSQL(pszSQL);
    const char *pszZoomLevel =
        CSLFetchNameValue(papszOpenOptionsIn, "ZOOM_LEVEL");
    if (pszZoomLevel)
    {
        if (GetUpdate())
            osSQL += CPLSPrintf(" AND zoom_level <= %d", atoi(pszZoomLevel));
        else
        {
            osSQL += CPLSPrintf(
                " AND (zoom_level = %d OR (zoom_level < %d AND EXISTS(SELECT 1 "
                "FROM %s WHERE zoom_level = tm.zoom_level LIMIT 1)))",
                atoi(pszZoomLevel), atoi(pszZoomLevel),
                osQuotedTableName.c_str());
        }
    }
    // In read-only mode, only lists non empty zoom levels
    else if (!GetUpdate())
    {
        osSQL += CPLSPrintf(" AND EXISTS(SELECT 1 FROM %s WHERE zoom_level = "
                            "tm.zoom_level LIMIT 1)",
                            osQuotedTableName.c_str());
    }
    else  // if( pszZoomLevel == nullptr )
    {
        osSQL +=
            CPLSPrintf(" AND zoom_level <= (SELECT MAX(zoom_level) FROM %s)",
                       osQuotedTableName.c_str());
    }
    osSQL += " ORDER BY zoom_level DESC";
    // To avoid denial of service.
    osSQL += " LIMIT 100";

    auto oResult = SQLQuery(hDB, osSQL.c_str());
    if (!oResult || oResult->RowCount() == 0)
    {
        if (oResult && oResult->RowCount() == 0 && pszContentsMinX != nullptr &&
            pszContentsMinY != nullptr && pszContentsMaxX != nullptr &&
            pszContentsMaxY != nullptr)
        {
            osSQL = pszSQL;
            osSQL += " ORDER BY zoom_level DESC";
            if (!GetUpdate())
                osSQL += " LIMIT 1";
            oResult = SQLQuery(hDB, osSQL.c_str());
        }
        if (!oResult || oResult->RowCount() == 0)
        {
            if (oResult && pszZoomLevel != nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "ZOOM_LEVEL is probably not valid w.r.t tile "
                         "table content");
            }
            sqlite3_free(pszSQL);
            return false;
        }
    }
    sqlite3_free(pszSQL);

    // If USE_TILE_EXTENT=YES, then query the tile table to find which tiles
    // actually exist.

    // CAUTION: Do not move those variables inside inner scope !
    CPLString osContentsMinX, osContentsMinY, osContentsMaxX, osContentsMaxY;

    if (CPLTestBool(
            CSLFetchNameValueDef(papszOpenOptionsIn, "USE_TILE_EXTENT", "NO")))
    {
        pszSQL = sqlite3_mprintf(
            "SELECT MIN(tile_column), MIN(tile_row), MAX(tile_column), "
            "MAX(tile_row) FROM \"%w\" WHERE zoom_level = %d",
            pszTableName, atoi(oResult->GetValue(0, 0)));
        auto oResult2 = SQLQuery(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (!oResult2 || oResult2->RowCount() == 0 ||
            // Can happen if table is empty
            oResult2->GetValue(0, 0) == nullptr ||
            // Can happen if table has no NOT NULL constraint on tile_row
            // and that all tile_row are NULL
            oResult2->GetValue(1, 0) == nullptr)
        {
            return false;
        }
        const double dfPixelXSize = CPLAtof(oResult->GetValue(1, 0));
        const double dfPixelYSize = CPLAtof(oResult->GetValue(2, 0));
        const int nTileWidth = atoi(oResult->GetValue(3, 0));
        const int nTileHeight = atoi(oResult->GetValue(4, 0));
        osContentsMinX =
            CPLSPrintf("%.18g", dfMinX + dfPixelXSize * nTileWidth *
                                             atoi(oResult2->GetValue(0, 0)));
        osContentsMaxY =
            CPLSPrintf("%.18g", dfMaxY - dfPixelYSize * nTileHeight *
                                             atoi(oResult2->GetValue(1, 0)));
        osContentsMaxX = CPLSPrintf(
            "%.18g", dfMinX + dfPixelXSize * nTileWidth *
                                  (1 + atoi(oResult2->GetValue(2, 0))));
        osContentsMinY = CPLSPrintf(
            "%.18g", dfMaxY - dfPixelYSize * nTileHeight *
                                  (1 + atoi(oResult2->GetValue(3, 0))));
        pszContentsMinX = osContentsMinX.c_str();
        pszContentsMinY = osContentsMinY.c_str();
        pszContentsMaxX = osContentsMaxX.c_str();
        pszContentsMaxY = osContentsMaxY.c_str();
    }

    if (!InitRaster(nullptr, pszTableName, dfMinX, dfMinY, dfMaxX, dfMaxY,
                    pszContentsMinX, pszContentsMinY, pszContentsMaxX,
                    pszContentsMaxY, papszOpenOptionsIn, *oResult, 0))
    {
        return false;
    }

    auto poBand =
        reinterpret_cast<GDALGeoPackageRasterBand *>(GetRasterBand(1));
    if (!osDataNull.empty())
    {
        double dfGPKGNoDataValue = CPLAtof(osDataNull);
        if (m_eTF == GPKG_TF_PNG_16BIT)
        {
            if (dfGPKGNoDataValue < 0 || dfGPKGNoDataValue > 65535 ||
                static_cast<int>(dfGPKGNoDataValue) != dfGPKGNoDataValue)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "data_null = %.18g is invalid for integer data_type",
                         dfGPKGNoDataValue);
            }
            else
            {
                m_usGPKGNull = static_cast<GUInt16>(dfGPKGNoDataValue);
                if (m_eDT == GDT_Int16 && m_usGPKGNull > 32767)
                    dfGPKGNoDataValue = -32768.0;
                else if (m_eDT == GDT_Float32)
                {
                    // Pick a value that is unlikely to be hit with offset &
                    // scale
                    dfGPKGNoDataValue = -std::numeric_limits<float>::max();
                }
                poBand->SetNoDataValueInternal(dfGPKGNoDataValue);
            }
        }
        else
        {
            poBand->SetNoDataValueInternal(
                static_cast<float>(dfGPKGNoDataValue));
        }
    }
    if (!osUom.empty())
    {
        poBand->SetUnitTypeInternal(osUom);
    }
    if (!osFieldName.empty())
    {
        GetRasterBand(1)->GDALRasterBand::SetDescription(osFieldName);
    }
    if (!osGridCellEncoding.empty())
    {
        if (osGridCellEncoding == "grid-value-is-center")
        {
            GDALPamDataset::SetMetadataItem(GDALMD_AREA_OR_POINT,
                                            GDALMD_AOP_POINT);
        }
        else if (osGridCellEncoding == "grid-value-is-area")
        {
            GDALPamDataset::SetMetadataItem(GDALMD_AREA_OR_POINT,
                                            GDALMD_AOP_AREA);
        }
        else
        {
            GDALPamDataset::SetMetadataItem(GDALMD_AREA_OR_POINT,
                                            GDALMD_AOP_POINT);
            GetRasterBand(1)->GDALRasterBand::SetMetadataItem(
                "GRID_CELL_ENCODING", osGridCellEncoding);
        }
    }

    CheckUnknownExtensions(true);

    // Do this after CheckUnknownExtensions() so that m_eTF is set to
    // GPKG_TF_WEBP if the table already registers the gpkg_webp extension
    const char *pszTF = CSLFetchNameValue(papszOpenOptionsIn, "TILE_FORMAT");
    if (pszTF)
    {
        if (!GetUpdate())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "TILE_FORMAT open option ignored in read-only mode");
        }
        else if (m_eTF == GPKG_TF_PNG_16BIT ||
                 m_eTF == GPKG_TF_TIFF_32BIT_FLOAT)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "TILE_FORMAT open option ignored on gridded coverages");
        }
        else
        {
            GPKGTileFormat eTF = GDALGPKGMBTilesGetTileFormat(pszTF);
            if (eTF == GPKG_TF_WEBP && m_eTF != eTF)
            {
                if (!RegisterWebPExtension())
                    return false;
            }
            m_eTF = eTF;
        }
    }

    ParseCompressionOptions(papszOpenOptionsIn);

    m_osWHERE = CSLFetchNameValueDef(papszOpenOptionsIn, "WHERE", "");

    // Set metadata
    if (pszIdentifier && pszIdentifier[0])
        GDALPamDataset::SetMetadataItem("IDENTIFIER", pszIdentifier);
    if (pszDescription && pszDescription[0])
        GDALPamDataset::SetMetadataItem("DESCRIPTION", pszDescription);

    // Add overviews
    for (int i = 1; i < oResult->RowCount(); i++)
    {
        GDALGeoPackageDataset *poOvrDS = new GDALGeoPackageDataset();
        poOvrDS->ShareLockWithParentDataset(this);
        if (!poOvrDS->InitRaster(this, pszTableName, dfMinX, dfMinY, dfMaxX,
                                 dfMaxY, pszContentsMinX, pszContentsMinY,
                                 pszContentsMaxX, pszContentsMaxY,
                                 papszOpenOptionsIn, *oResult, i))
        {
            delete poOvrDS;
            break;
        }

        m_papoOverviewDS = static_cast<GDALGeoPackageDataset **>(
            CPLRealloc(m_papoOverviewDS, sizeof(GDALGeoPackageDataset *) *
                                             (m_nOverviewCount + 1)));
        m_papoOverviewDS[m_nOverviewCount++] = poOvrDS;

        int nTileWidth, nTileHeight;
        poOvrDS->GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
        if (eAccess == GA_ReadOnly && poOvrDS->GetRasterXSize() < nTileWidth &&
            poOvrDS->GetRasterYSize() < nTileHeight)
        {
            break;
        }
    }

    return true;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *GDALGeoPackageDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    if (nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on read-only dataset");
        return CE_Failure;
    }

    const int nSRID = GetSrsId(poSRS);
    const auto poTS = GetTilingScheme(m_osTilingScheme);
    if (poTS && nSRID != poTS->nEPSGCode)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Projection should be EPSG:%d for %s tiling scheme",
                 poTS->nEPSGCode, m_osTilingScheme.c_str());
        return CE_Failure;
    }

    m_nSRID = nSRID;
    m_oSRS.Clear();
    if (poSRS)
        m_oSRS = *poSRS;

    if (m_bRecordInsertedInGPKGContent)
    {
        char *pszSQL = sqlite3_mprintf("UPDATE gpkg_contents SET srs_id = %d "
                                       "WHERE lower(table_name) = lower('%q')",
                                       m_nSRID, m_osRasterTable.c_str());
        OGRErr eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (eErr != OGRERR_NONE)
            return CE_Failure;

        pszSQL = sqlite3_mprintf("UPDATE gpkg_tile_matrix_set SET srs_id = %d "
                                 "WHERE lower(table_name) = lower('%q')",
                                 m_nSRID, m_osRasterTable.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (eErr != OGRERR_NONE)
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::GetGeoTransform(double *padfGeoTransform)
{
    memcpy(padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double));
    if (!m_bGeoTransformValid)
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetGeoTransform(double *padfGeoTransform)
{
    if (nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGeoTransform() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGeoTransform() not supported on read-only dataset");
        return CE_Failure;
    }
    if (m_bGeoTransformValid)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot modify geotransform once set");
        return CE_Failure;
    }
    if (padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0 ||
        padfGeoTransform[5] > 0.0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up non rotated geotransform supported");
        return CE_Failure;
    }

    if (m_nZoomLevel < 0)
    {
        const auto poTS = GetTilingScheme(m_osTilingScheme);
        if (poTS)
        {
            double dfPixelXSizeZoomLevel0 = poTS->dfPixelXSizeZoomLevel0;
            double dfPixelYSizeZoomLevel0 = poTS->dfPixelYSizeZoomLevel0;
            for (m_nZoomLevel = 0; m_nZoomLevel < MAX_ZOOM_LEVEL;
                 m_nZoomLevel++)
            {
                double dfExpectedPixelXSize =
                    dfPixelXSizeZoomLevel0 / (1 << m_nZoomLevel);
                double dfExpectedPixelYSize =
                    dfPixelYSizeZoomLevel0 / (1 << m_nZoomLevel);
                if (fabs(padfGeoTransform[1] - dfExpectedPixelXSize) <
                        1e-8 * dfExpectedPixelXSize &&
                    fabs(fabs(padfGeoTransform[5]) - dfExpectedPixelYSize) <
                        1e-8 * dfExpectedPixelYSize)
                {
                    break;
                }
            }
            if (m_nZoomLevel == MAX_ZOOM_LEVEL)
            {
                m_nZoomLevel = -1;
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "Could not find an appropriate zoom level of %s tiling "
                    "scheme that matches raster pixel size",
                    m_osTilingScheme.c_str());
                return CE_Failure;
            }
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

    if (m_nZoomLevel < 0)
    {
        m_nZoomLevel = 0;
        while ((nRasterXSize >> m_nZoomLevel) > nTileWidth ||
               (nRasterYSize >> m_nZoomLevel) > nTileHeight)
            m_nZoomLevel++;
    }

    double dfPixelXSizeZoomLevel0 = m_adfGeoTransform[1] * (1 << m_nZoomLevel);
    double dfPixelYSizeZoomLevel0 =
        fabs(m_adfGeoTransform[5]) * (1 << m_nZoomLevel);
    int nTileXCountZoomLevel0 =
        std::max(1, DIV_ROUND_UP((nRasterXSize >> m_nZoomLevel), nTileWidth));
    int nTileYCountZoomLevel0 =
        std::max(1, DIV_ROUND_UP((nRasterYSize >> m_nZoomLevel), nTileHeight));

    const auto poTS = GetTilingScheme(m_osTilingScheme);
    if (poTS)
    {
        CPLAssert(m_nZoomLevel >= 0);
        m_dfTMSMinX = poTS->dfMinX;
        m_dfTMSMaxY = poTS->dfMaxY;
        dfPixelXSizeZoomLevel0 = poTS->dfPixelXSizeZoomLevel0;
        dfPixelYSizeZoomLevel0 = poTS->dfPixelYSizeZoomLevel0;
        nTileXCountZoomLevel0 = poTS->nTileXCountZoomLevel0;
        nTileYCountZoomLevel0 = poTS->nTileYCountZoomLevel0;
    }
    m_nTileMatrixWidth = nTileXCountZoomLevel0 * (1 << m_nZoomLevel);
    m_nTileMatrixHeight = nTileYCountZoomLevel0 * (1 << m_nZoomLevel);

    if (!ComputeTileAndPixelShifts())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Overflow occurred in ComputeTileAndPixelShifts()");
        return CE_Failure;
    }

    if (!AllocCachedTiles())
    {
        return CE_Failure;
    }

    double dfGDALMinX = m_adfGeoTransform[0];
    double dfGDALMinY =
        m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
    double dfGDALMaxX =
        m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
    double dfGDALMaxY = m_adfGeoTransform[3];

    if (SoftStartTransaction() != OGRERR_NONE)
        return CE_Failure;

    const char *pszCurrentDate =
        CPLGetConfigOption("OGR_CURRENT_DATE", nullptr);
    CPLString osInsertGpkgContentsFormatting(
        "INSERT INTO gpkg_contents "
        "(table_name,data_type,identifier,description,min_x,min_y,max_x,max_y,"
        "last_change,srs_id) VALUES "
        "('%q','%q','%q','%q',%.18g,%.18g,%.18g,%.18g,");
    osInsertGpkgContentsFormatting += (pszCurrentDate) ? "'%q'" : "%s";
    osInsertGpkgContentsFormatting += ",%d)";
    char *pszSQL = sqlite3_mprintf(
        osInsertGpkgContentsFormatting.c_str(), m_osRasterTable.c_str(),
        (m_eDT == GDT_Byte) ? "tiles" : "2d-gridded-coverage",
        m_osIdentifier.c_str(), m_osDescription.c_str(), dfGDALMinX, dfGDALMinY,
        dfGDALMaxX, dfGDALMaxY,
        pszCurrentDate ? pszCurrentDate
                       : "strftime('%Y-%m-%dT%H:%M:%fZ','now')",
        m_nSRID);

    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (eErr != OGRERR_NONE)
    {
        SoftRollbackTransaction();
        return CE_Failure;
    }

    double dfTMSMaxX = m_dfTMSMinX + nTileXCountZoomLevel0 * nTileWidth *
                                         dfPixelXSizeZoomLevel0;
    double dfTMSMinY = m_dfTMSMaxY - nTileYCountZoomLevel0 * nTileHeight *
                                         dfPixelYSizeZoomLevel0;

    pszSQL =
        sqlite3_mprintf("INSERT INTO gpkg_tile_matrix_set "
                        "(table_name,srs_id,min_x,min_y,max_x,max_y) VALUES "
                        "('%q',%d,%.18g,%.18g,%.18g,%.18g)",
                        m_osRasterTable.c_str(), m_nSRID, m_dfTMSMinX,
                        dfTMSMinY, dfTMSMaxX, m_dfTMSMaxY);
    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (eErr != OGRERR_NONE)
    {
        SoftRollbackTransaction();
        return CE_Failure;
    }

    m_papoOverviewDS = static_cast<GDALGeoPackageDataset **>(
        CPLCalloc(sizeof(GDALGeoPackageDataset *), m_nZoomLevel));

    for (int i = 0; i <= m_nZoomLevel; i++)
    {
        double dfPixelXSizeZoomLevel = 0.0;
        double dfPixelYSizeZoomLevel = 0.0;
        int nTileMatrixWidth = 0;
        int nTileMatrixHeight = 0;
        if (EQUAL(m_osTilingScheme, "CUSTOM"))
        {
            dfPixelXSizeZoomLevel =
                m_adfGeoTransform[1] * (1 << (m_nZoomLevel - i));
            dfPixelYSizeZoomLevel =
                fabs(m_adfGeoTransform[5]) * (1 << (m_nZoomLevel - i));
        }
        else
        {
            dfPixelXSizeZoomLevel = dfPixelXSizeZoomLevel0 / (1 << i);
            dfPixelYSizeZoomLevel = dfPixelYSizeZoomLevel0 / (1 << i);
        }
        nTileMatrixWidth = nTileXCountZoomLevel0 * (1 << i);
        nTileMatrixHeight = nTileYCountZoomLevel0 * (1 << i);

        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_tile_matrix "
            "(table_name,zoom_level,matrix_width,matrix_height,tile_width,tile_"
            "height,pixel_x_size,pixel_y_size) VALUES "
            "('%q',%d,%d,%d,%d,%d,%.18g,%.18g)",
            m_osRasterTable.c_str(), i, nTileMatrixWidth, nTileMatrixHeight,
            nTileWidth, nTileHeight, dfPixelXSizeZoomLevel,
            dfPixelYSizeZoomLevel);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (eErr != OGRERR_NONE)
        {
            SoftRollbackTransaction();
            return CE_Failure;
        }

        if (i < m_nZoomLevel)
        {
            GDALGeoPackageDataset *poOvrDS = new GDALGeoPackageDataset();
            poOvrDS->ShareLockWithParentDataset(this);
            poOvrDS->InitRaster(this, m_osRasterTable, i, nBands, m_dfTMSMinX,
                                m_dfTMSMaxY, dfPixelXSizeZoomLevel,
                                dfPixelYSizeZoomLevel, nTileWidth, nTileHeight,
                                nTileMatrixWidth, nTileMatrixHeight, dfGDALMinX,
                                dfGDALMinY, dfGDALMaxX, dfGDALMaxY);

            m_papoOverviewDS[m_nZoomLevel - 1 - i] = poOvrDS;
        }
    }

    if (!m_osSQLInsertIntoGpkg2dGriddedCoverageAncillary.empty())
    {
        eErr = SQLCommand(
            hDB, m_osSQLInsertIntoGpkg2dGriddedCoverageAncillary.c_str());
        m_osSQLInsertIntoGpkg2dGriddedCoverageAncillary.clear();
        if (eErr != OGRERR_NONE)
        {
            SoftRollbackTransaction();
            return CE_Failure;
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

CPLErr GDALGeoPackageDataset::FlushCache(bool bAtClosing)
{
    if (m_bInFlushCache)
        return CE_None;

    if (eAccess == GA_Update || !m_bMetadataDirty)
    {
        SetPamFlags(GetPamFlags() & ~GPF_DIRTY);
    }

    if (m_bRemoveOGREmptyTable)
    {
        m_bRemoveOGREmptyTable = false;
        RemoveOGREmptyTable();
    }

    CPLErr eErr = IFlushCacheWithErrCode(bAtClosing);

    FlushMetadata();

    if (eAccess == GA_Update || !m_bMetadataDirty)
    {
        // Needed again as above IFlushCacheWithErrCode()
        // may have call GDALGeoPackageRasterBand::InvalidateStatistics()
        // which modifies metadata
        SetPamFlags(GetPamFlags() & ~GPF_DIRTY);
    }

    return eErr;
}

CPLErr GDALGeoPackageDataset::IFlushCacheWithErrCode(bool bAtClosing)

{
    if (m_bInFlushCache)
        return CE_None;
    m_bInFlushCache = true;
    if (hDB && eAccess == GA_ReadOnly && bAtClosing)
    {
        // Clean-up metadata that will go to PAM by removing items that
        // are reconstructed.
        CPLStringList aosMD;
        for (CSLConstList papszIter = GetMetadata(); papszIter && *papszIter;
             ++papszIter)
        {
            char *pszKey = nullptr;
            CPLParseNameValue(*papszIter, &pszKey);
            if (pszKey &&
                (EQUAL(pszKey, "AREA_OR_POINT") ||
                 EQUAL(pszKey, "IDENTIFIER") || EQUAL(pszKey, "DESCRIPTION") ||
                 EQUAL(pszKey, "ZOOM_LEVEL") ||
                 STARTS_WITH(pszKey, "GPKG_METADATA_ITEM_")))
            {
                // remove it
            }
            else
            {
                aosMD.AddString(*papszIter);
            }
            CPLFree(pszKey);
        }
        oMDMD.SetMetadata(aosMD.List());
        oMDMD.SetMetadata(nullptr, "IMAGE_STRUCTURE");

        GDALPamDataset::FlushCache(bAtClosing);
    }
    else
    {
        // Short circuit GDALPamDataset to avoid serialization to .aux.xml
        GDALDataset::FlushCache(bAtClosing);
    }

    for (int i = 0; i < m_nLayers; i++)
    {
        m_papoLayers[i]->RunDeferredCreationIfNecessary();
        m_papoLayers[i]->CreateSpatialIndexIfNecessary();
    }

    // Update raster table last_change column in gpkg_contents if needed
    if (m_bHasModifiedTiles)
    {
        for (int i = 1; i <= nBands; ++i)
        {
            auto poBand =
                cpl::down_cast<GDALGeoPackageRasterBand *>(GetRasterBand(i));
            if (!poBand->HaveStatsMetadataBeenSetInThisSession())
            {
                poBand->InvalidateStatistics();
                if (psPam && psPam->pszPamFilename)
                    VSIUnlink(psPam->pszPamFilename);
            }
        }

        UpdateGpkgContentsLastChange(m_osRasterTable);

        m_bHasModifiedTiles = false;
    }

    CPLErr eErr = FlushTiles();

    m_bInFlushCache = false;
    return eErr;
}

/************************************************************************/
/*                       GetCurrentDateEscapedSQL()                      */
/************************************************************************/

std::string GDALGeoPackageDataset::GetCurrentDateEscapedSQL()
{
    const char *pszCurrentDate =
        CPLGetConfigOption("OGR_CURRENT_DATE", nullptr);
    if (pszCurrentDate)
        return '\'' + SQLEscapeLiteral(pszCurrentDate) + '\'';
    return "strftime('%Y-%m-%dT%H:%M:%fZ','now')";
}

/************************************************************************/
/*                    UpdateGpkgContentsLastChange()                    */
/************************************************************************/

OGRErr
GDALGeoPackageDataset::UpdateGpkgContentsLastChange(const char *pszTableName)
{
    char *pszSQL =
        sqlite3_mprintf("UPDATE gpkg_contents SET "
                        "last_change = %s "
                        "WHERE lower(table_name) = lower('%q')",
                        GetCurrentDateEscapedSQL().c_str(), pszTableName);
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    return eErr;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::IBuildOverviews(
    const char *pszResampling, int nOverviews, const int *panOverviewList,
    int nBandsIn, const int * /*panBandList*/, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)
{
    if (GetAccess() != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on a database opened in "
                 "read-only mode");
        return CE_Failure;
    }
    if (m_poParentDS != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on overview dataset");
        return CE_Failure;
    }

    if (nOverviews == 0)
    {
        for (int i = 0; i < m_nOverviewCount; i++)
            m_papoOverviewDS[i]->FlushCache(false);

        SoftStartTransaction();

        if (m_eTF == GPKG_TF_PNG_16BIT || m_eTF == GPKG_TF_TIFF_32BIT_FLOAT)
        {
            char *pszSQL = sqlite3_mprintf(
                "DELETE FROM gpkg_2d_gridded_tile_ancillary WHERE id IN "
                "(SELECT y.id FROM \"%w\" x "
                "JOIN gpkg_2d_gridded_tile_ancillary y "
                "ON x.id = y.tpudt_id AND y.tpudt_name = '%q' AND "
                "x.zoom_level < %d)",
                m_osRasterTable.c_str(), m_osRasterTable.c_str(), m_nZoomLevel);
            OGRErr eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
            if (eErr != OGRERR_NONE)
            {
                SoftRollbackTransaction();
                return CE_Failure;
            }
        }

        char *pszSQL =
            sqlite3_mprintf("DELETE FROM \"%w\" WHERE zoom_level < %d",
                            m_osRasterTable.c_str(), m_nZoomLevel);
        OGRErr eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (eErr != OGRERR_NONE)
        {
            SoftRollbackTransaction();
            return CE_Failure;
        }

        SoftCommitTransaction();

        return CE_None;
    }

    if (nBandsIn != nBands)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Generation of overviews in GPKG only"
                 "supported when operating on all bands.");
        return CE_Failure;
    }

    if (m_nOverviewCount == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Image too small to support overviews");
        return CE_Failure;
    }

    FlushCache(false);
    for (int i = 0; i < nOverviews; i++)
    {
        if (panOverviewList[i] < 2)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Overview factor must be >= 2");
            return CE_Failure;
        }

        bool bFound = false;
        int jCandidate = -1;
        int nMaxOvFactor = 0;
        for (int j = 0; j < m_nOverviewCount; j++)
        {
            auto poODS = m_papoOverviewDS[j];
            const int nOvFactor = static_cast<int>(
                0.5 + poODS->m_adfGeoTransform[1] / m_adfGeoTransform[1]);

            nMaxOvFactor = nOvFactor;

            if (nOvFactor == panOverviewList[i])
            {
                bFound = true;
                break;
            }

            if (jCandidate < 0 && nOvFactor > panOverviewList[i])
                jCandidate = j;
        }

        if (!bFound)
        {
            /* Mostly for debug */
            if (!CPLTestBool(CPLGetConfigOption(
                    "ALLOW_GPKG_ZOOM_OTHER_EXTENSION", "YES")))
            {
                CPLString osOvrList;
                for (int j = 0; j < m_nOverviewCount; j++)
                {
                    auto poODS = m_papoOverviewDS[j];
                    const int nOvFactor =
                        static_cast<int>(0.5 + poODS->m_adfGeoTransform[1] /
                                                   m_adfGeoTransform[1]);

                    if (j != 0)
                        osOvrList += " ";
                    osOvrList += CPLSPrintf("%d", nOvFactor);
                }
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Only overviews %s can be computed",
                         osOvrList.c_str());
                return CE_Failure;
            }
            else
            {
                int nOvFactor = panOverviewList[i];
                if (jCandidate < 0)
                    jCandidate = m_nOverviewCount;

                int nOvXSize = std::max(1, GetRasterXSize() / nOvFactor);
                int nOvYSize = std::max(1, GetRasterYSize() / nOvFactor);
                if (!(jCandidate == m_nOverviewCount &&
                      nOvFactor == 2 * nMaxOvFactor) &&
                    !m_bZoomOther)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Use of overview factor %d causes gpkg_zoom_other "
                             "extension to be needed",
                             nOvFactor);
                    RegisterZoomOtherExtension();
                    m_bZoomOther = true;
                }

                SoftStartTransaction();

                CPLAssert(jCandidate > 0);
                int nNewZoomLevel =
                    m_papoOverviewDS[jCandidate - 1]->m_nZoomLevel;

                char *pszSQL;
                OGRErr eErr;
                for (int k = 0; k <= jCandidate; k++)
                {
                    pszSQL = sqlite3_mprintf(
                        "UPDATE gpkg_tile_matrix SET zoom_level = %d "
                        "WHERE lower(table_name) = lower('%q') AND zoom_level "
                        "= %d",
                        m_nZoomLevel - k + 1, m_osRasterTable.c_str(),
                        m_nZoomLevel - k);
                    eErr = SQLCommand(hDB, pszSQL);
                    sqlite3_free(pszSQL);
                    if (eErr != OGRERR_NONE)
                    {
                        SoftRollbackTransaction();
                        return CE_Failure;
                    }

                    pszSQL =
                        sqlite3_mprintf("UPDATE \"%w\" SET zoom_level = %d "
                                        "WHERE zoom_level = %d",
                                        m_osRasterTable.c_str(),
                                        m_nZoomLevel - k + 1, m_nZoomLevel - k);
                    eErr = SQLCommand(hDB, pszSQL);
                    sqlite3_free(pszSQL);
                    if (eErr != OGRERR_NONE)
                    {
                        SoftRollbackTransaction();
                        return CE_Failure;
                    }
                }

                double dfGDALMinX = m_adfGeoTransform[0];
                double dfGDALMinY =
                    m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
                double dfGDALMaxX =
                    m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
                double dfGDALMaxY = m_adfGeoTransform[3];
                double dfPixelXSizeZoomLevel = m_adfGeoTransform[1] * nOvFactor;
                double dfPixelYSizeZoomLevel =
                    fabs(m_adfGeoTransform[5]) * nOvFactor;
                int nTileWidth, nTileHeight;
                GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
                int nTileMatrixWidth = (nOvXSize + nTileWidth - 1) / nTileWidth;
                int nTileMatrixHeight =
                    (nOvYSize + nTileHeight - 1) / nTileHeight;
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_tile_matrix "
                    "(table_name,zoom_level,matrix_width,matrix_height,tile_"
                    "width,tile_height,pixel_x_size,pixel_y_size) VALUES "
                    "('%q',%d,%d,%d,%d,%d,%.18g,%.18g)",
                    m_osRasterTable.c_str(), nNewZoomLevel, nTileMatrixWidth,
                    nTileMatrixHeight, nTileWidth, nTileHeight,
                    dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel);
                eErr = SQLCommand(hDB, pszSQL);
                sqlite3_free(pszSQL);
                if (eErr != OGRERR_NONE)
                {
                    SoftRollbackTransaction();
                    return CE_Failure;
                }

                SoftCommitTransaction();

                m_nZoomLevel++; /* this change our zoom level as well as
                                   previous overviews */
                for (int k = 0; k < jCandidate; k++)
                    m_papoOverviewDS[k]->m_nZoomLevel++;

                GDALGeoPackageDataset *poOvrDS = new GDALGeoPackageDataset();
                poOvrDS->ShareLockWithParentDataset(this);
                poOvrDS->InitRaster(
                    this, m_osRasterTable, nNewZoomLevel, nBands, m_dfTMSMinX,
                    m_dfTMSMaxY, dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel,
                    nTileWidth, nTileHeight, nTileMatrixWidth,
                    nTileMatrixHeight, dfGDALMinX, dfGDALMinY, dfGDALMaxX,
                    dfGDALMaxY);
                m_papoOverviewDS =
                    static_cast<GDALGeoPackageDataset **>(CPLRealloc(
                        m_papoOverviewDS, sizeof(GDALGeoPackageDataset *) *
                                              (m_nOverviewCount + 1)));

                if (jCandidate < m_nOverviewCount)
                {
                    memmove(m_papoOverviewDS + jCandidate + 1,
                            m_papoOverviewDS + jCandidate,
                            sizeof(GDALGeoPackageDataset *) *
                                (m_nOverviewCount - jCandidate));
                }
                m_papoOverviewDS[jCandidate] = poOvrDS;
                m_nOverviewCount++;
            }
        }
    }

    GDALRasterBand ***papapoOverviewBands = static_cast<GDALRasterBand ***>(
        CPLCalloc(sizeof(GDALRasterBand **), nBands));
    CPLErr eErr = CE_None;
    for (int iBand = 0; eErr == CE_None && iBand < nBands; iBand++)
    {
        papapoOverviewBands[iBand] = static_cast<GDALRasterBand **>(
            CPLCalloc(sizeof(GDALRasterBand *), nOverviews));
        int iCurOverview = 0;
        for (int i = 0; i < nOverviews; i++)
        {
            int j = 0;  // Used after for.
            for (; j < m_nOverviewCount; j++)
            {
                auto poODS = m_papoOverviewDS[j];
                const int nOvFactor = static_cast<int>(
                    0.5 + poODS->m_adfGeoTransform[1] / m_adfGeoTransform[1]);

                if (nOvFactor == panOverviewList[i])
                {
                    papapoOverviewBands[iBand][iCurOverview] =
                        poODS->GetRasterBand(iBand + 1);
                    iCurOverview++;
                    break;
                }
            }
            if (j == m_nOverviewCount)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not find dataset corresponding to ov factor %d",
                         panOverviewList[i]);
                eErr = CE_Failure;
            }
        }
        if (eErr == CE_None)
        {
            CPLAssert(iCurOverview == nOverviews);
        }
    }

    if (eErr == CE_None)
        eErr = GDALRegenerateOverviewsMultiBand(
            nBands, papoBands, nOverviews, papapoOverviewBands, pszResampling,
            pfnProgress, pProgressData, papszOptions);

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        CPLFree(papapoOverviewBands[iBand]);
    }
    CPLFree(papapoOverviewBands);

    return eErr;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **GDALGeoPackageDataset::GetFileList()
{
    TryLoadXML();
    return GDALPamDataset::GetFileList();
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GDALGeoPackageDataset::GetMetadataDomainList()
{
    GetMetadata();
    if (!m_osRasterTable.empty())
        GetMetadata("GEOPACKAGE");
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE, "SUBDATASETS", nullptr);
}

/************************************************************************/
/*                        CheckMetadataDomain()                         */
/************************************************************************/

const char *GDALGeoPackageDataset::CheckMetadataDomain(const char *pszDomain)
{
    if (pszDomain != nullptr && EQUAL(pszDomain, "GEOPACKAGE") &&
        m_osRasterTable.empty())
    {
        CPLError(
            CE_Warning, CPLE_IllegalArg,
            "Using GEOPACKAGE for a non-raster geopackage is not supported. "
            "Using default domain instead");
        return nullptr;
    }
    return pszDomain;
}

/************************************************************************/
/*                           HasMetadataTables()                        */
/************************************************************************/

bool GDALGeoPackageDataset::HasMetadataTables() const
{
    if (m_nHasMetadataTables < 0)
    {
        const int nCount =
            SQLGetInteger(hDB,
                          "SELECT COUNT(*) FROM sqlite_master WHERE name IN "
                          "('gpkg_metadata', 'gpkg_metadata_reference') "
                          "AND type IN ('table', 'view')",
                          nullptr);
        m_nHasMetadataTables = nCount == 2;
    }
    return CPL_TO_BOOL(m_nHasMetadataTables);
}

/************************************************************************/
/*                         HasDataColumnsTable()                        */
/************************************************************************/

bool GDALGeoPackageDataset::HasDataColumnsTable() const
{
    const int nCount = SQLGetInteger(
        hDB,
        "SELECT 1 FROM sqlite_master WHERE name = 'gpkg_data_columns'"
        "AND type IN ('table', 'view')",
        nullptr);
    return nCount == 1;
}

/************************************************************************/
/*                    HasDataColumnConstraintsTable()                   */
/************************************************************************/

bool GDALGeoPackageDataset::HasDataColumnConstraintsTable() const
{
    const int nCount = SQLGetInteger(hDB,
                                     "SELECT 1 FROM sqlite_master WHERE name = "
                                     "'gpkg_data_column_constraints'"
                                     "AND type IN ('table', 'view')",
                                     nullptr);
    return nCount == 1;
}

/************************************************************************/
/*                  HasDataColumnConstraintsTableGPKG_1_0()             */
/************************************************************************/

bool GDALGeoPackageDataset::HasDataColumnConstraintsTableGPKG_1_0() const
{
    if (m_nApplicationId != GP10_APPLICATION_ID)
        return false;
    // In GPKG 1.0, the columns were named minIsInclusive, maxIsInclusive
    // They were changed in 1.1 to min_is_inclusive, max_is_inclusive
    bool bRet = false;
    sqlite3_stmt *hSQLStmt = nullptr;
    int rc = sqlite3_prepare_v2(hDB,
                                "SELECT minIsInclusive, maxIsInclusive FROM "
                                "gpkg_data_column_constraints",
                                -1, &hSQLStmt, nullptr);
    if (rc == SQLITE_OK)
    {
        bRet = true;
        sqlite3_finalize(hSQLStmt);
    }
    return bRet;
}

/************************************************************************/
/*      CreateColumnsTableAndColumnConstraintsTablesIfNecessary()       */
/************************************************************************/

bool GDALGeoPackageDataset::
    CreateColumnsTableAndColumnConstraintsTablesIfNecessary()
{
    if (!HasDataColumnsTable())
    {
        // Geopackage < 1.3 had
        // CONSTRAINT fk_gdc_tn FOREIGN KEY (table_name) REFERENCES
        // gpkg_contents(table_name) instead of the unique constraint.
        if (OGRERR_NONE !=
            SQLCommand(
                GetDB(),
                "CREATE TABLE gpkg_data_columns ("
                "table_name TEXT NOT NULL,"
                "column_name TEXT NOT NULL,"
                "name TEXT,"
                "title TEXT,"
                "description TEXT,"
                "mime_type TEXT,"
                "constraint_name TEXT,"
                "CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),"
                "CONSTRAINT gdc_tn UNIQUE (table_name, name));"))
        {
            return false;
        }
    }
    if (!HasDataColumnConstraintsTable())
    {
        const char *min_is_inclusive = m_nApplicationId != GP10_APPLICATION_ID
                                           ? "min_is_inclusive"
                                           : "minIsInclusive";
        const char *max_is_inclusive = m_nApplicationId != GP10_APPLICATION_ID
                                           ? "max_is_inclusive"
                                           : "maxIsInclusive";

        const std::string osSQL(
            CPLSPrintf("CREATE TABLE gpkg_data_column_constraints ("
                       "constraint_name TEXT NOT NULL,"
                       "constraint_type TEXT NOT NULL,"
                       "value TEXT,"
                       "min NUMERIC,"
                       "%s BOOLEAN,"
                       "max NUMERIC,"
                       "%s BOOLEAN,"
                       "description TEXT,"
                       "CONSTRAINT gdcc_ntv UNIQUE (constraint_name, "
                       "constraint_type, value));",
                       min_is_inclusive, max_is_inclusive));
        if (OGRERR_NONE != SQLCommand(GetDB(), osSQL.c_str()))
        {
            return false;
        }
    }
    if (CreateExtensionsTableIfNecessary() != OGRERR_NONE)
    {
        return false;
    }
    if (SQLGetInteger(GetDB(),
                      "SELECT 1 FROM gpkg_extensions WHERE "
                      "table_name = 'gpkg_data_columns'",
                      nullptr) != 1)
    {
        if (OGRERR_NONE !=
            SQLCommand(
                GetDB(),
                "INSERT INTO gpkg_extensions "
                "(table_name,column_name,extension_name,definition,scope) "
                "VALUES ('gpkg_data_columns', NULL, 'gpkg_schema', "
                "'http://www.geopackage.org/spec121/#extension_schema', "
                "'read-write')"))
        {
            return false;
        }
    }
    if (SQLGetInteger(GetDB(),
                      "SELECT 1 FROM gpkg_extensions WHERE "
                      "table_name = 'gpkg_data_column_constraints'",
                      nullptr) != 1)
    {
        if (OGRERR_NONE !=
            SQLCommand(
                GetDB(),
                "INSERT INTO gpkg_extensions "
                "(table_name,column_name,extension_name,definition,scope) "
                "VALUES ('gpkg_data_column_constraints', NULL, 'gpkg_schema', "
                "'http://www.geopackage.org/spec121/#extension_schema', "
                "'read-write')"))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                        HasGpkgextRelationsTable()                    */
/************************************************************************/

bool GDALGeoPackageDataset::HasGpkgextRelationsTable() const
{
    const int nCount = SQLGetInteger(
        hDB,
        "SELECT 1 FROM sqlite_master WHERE name = 'gpkgext_relations'"
        "AND type IN ('table', 'view')",
        nullptr);
    return nCount == 1;
}

/************************************************************************/
/*                    CreateRelationsTableIfNecessary()                 */
/************************************************************************/

bool GDALGeoPackageDataset::CreateRelationsTableIfNecessary()
{
    if (HasGpkgextRelationsTable())
    {
        return true;
    }

    if (OGRERR_NONE !=
        SQLCommand(GetDB(), "CREATE TABLE gpkgext_relations ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                            "base_table_name TEXT NOT NULL,"
                            "base_primary_column TEXT NOT NULL DEFAULT 'id',"
                            "related_table_name TEXT NOT NULL,"
                            "related_primary_column TEXT NOT NULL DEFAULT 'id',"
                            "relation_name TEXT NOT NULL,"
                            "mapping_table_name TEXT NOT NULL UNIQUE);"))
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                        HasQGISLayerStyles()                          */
/************************************************************************/

bool GDALGeoPackageDataset::HasQGISLayerStyles() const
{
    // QGIS layer_styles extension:
    // https://github.com/pka/qgpkg/blob/master/qgis_geopackage_extension.md
    bool bRet = false;
    const int nCount =
        SQLGetInteger(hDB,
                      "SELECT 1 FROM sqlite_master WHERE name = 'layer_styles'"
                      "AND type = 'table'",
                      nullptr);
    if (nCount == 1)
    {
        sqlite3_stmt *hSQLStmt = nullptr;
        int rc = sqlite3_prepare_v2(
            hDB, "SELECT f_table_name, f_geometry_column FROM layer_styles", -1,
            &hSQLStmt, nullptr);
        if (rc == SQLITE_OK)
        {
            bRet = true;
            sqlite3_finalize(hSQLStmt);
        }
    }
    return bRet;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALGeoPackageDataset::GetMetadata(const char *pszDomain)

{
    pszDomain = CheckMetadataDomain(pszDomain);
    if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
        return m_aosSubDatasets.List();

    if (m_bHasReadMetadataFromStorage)
        return GDALPamDataset::GetMetadata(pszDomain);

    m_bHasReadMetadataFromStorage = true;

    TryLoadXML();

    if (!HasMetadataTables())
        return GDALPamDataset::GetMetadata(pszDomain);

    char *pszSQL = nullptr;
    if (!m_osRasterTable.empty())
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, "
            "mdr.reference_scope FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE "
            "(mdr.reference_scope = 'geopackage' OR "
            "(mdr.reference_scope = 'table' AND lower(mdr.table_name) = "
            "lower('%q'))) ORDER BY md.id "
            "LIMIT 1000",  // to avoid denial of service
            m_osRasterTable.c_str());
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, "
            "mdr.reference_scope FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE "
            "mdr.reference_scope = 'geopackage' ORDER BY md.id "
            "LIMIT 1000"  // to avoid denial of service
        );
    }

    auto oResult = SQLQuery(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (!oResult)
    {
        return GDALPamDataset::GetMetadata(pszDomain);
    }

    char **papszMetadata = CSLDuplicate(GDALPamDataset::GetMetadata());

    /* GDAL metadata */
    for (int i = 0; i < oResult->RowCount(); i++)
    {
        const char *pszMetadata = oResult->GetValue(0, i);
        const char *pszMDStandardURI = oResult->GetValue(1, i);
        const char *pszMimeType = oResult->GetValue(2, i);
        const char *pszReferenceScope = oResult->GetValue(3, i);
        if (pszMetadata && pszMDStandardURI && pszMimeType &&
            pszReferenceScope && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/xml"))
        {
            CPLXMLNode *psXMLNode = CPLParseXMLString(pszMetadata);
            if (psXMLNode)
            {
                GDALMultiDomainMetadata oLocalMDMD;
                oLocalMDMD.XMLInit(psXMLNode, FALSE);
                if (!m_osRasterTable.empty() &&
                    EQUAL(pszReferenceScope, "geopackage"))
                {
                    oMDMD.SetMetadata(oLocalMDMD.GetMetadata(), "GEOPACKAGE");
                }
                else
                {
                    papszMetadata =
                        CSLMerge(papszMetadata, oLocalMDMD.GetMetadata());
                    CSLConstList papszDomainList = oLocalMDMD.GetDomainList();
                    CSLConstList papszIter = papszDomainList;
                    while (papszIter && *papszIter)
                    {
                        if (EQUAL(*papszIter, "IMAGE_STRUCTURE"))
                        {
                            CSLConstList papszMD =
                                oLocalMDMD.GetMetadata(*papszIter);
                            const char *pszBAND_COUNT =
                                CSLFetchNameValue(papszMD, "BAND_COUNT");
                            if (pszBAND_COUNT)
                                m_nBandCountFromMetadata = atoi(pszBAND_COUNT);

                            const char *pszCOLOR_TABLE =
                                CSLFetchNameValue(papszMD, "COLOR_TABLE");
                            if (pszCOLOR_TABLE)
                            {
                                const CPLStringList aosTokens(
                                    CSLTokenizeString2(pszCOLOR_TABLE, "{,",
                                                       0));
                                if ((aosTokens.size() % 4) == 0)
                                {
                                    const int nColors = aosTokens.size() / 4;
                                    m_poCTFromMetadata =
                                        std::make_unique<GDALColorTable>();
                                    for (int iColor = 0; iColor < nColors;
                                         ++iColor)
                                    {
                                        GDALColorEntry sEntry;
                                        sEntry.c1 = static_cast<short>(
                                            atoi(aosTokens[4 * iColor + 0]));
                                        sEntry.c2 = static_cast<short>(
                                            atoi(aosTokens[4 * iColor + 1]));
                                        sEntry.c3 = static_cast<short>(
                                            atoi(aosTokens[4 * iColor + 2]));
                                        sEntry.c4 = static_cast<short>(
                                            atoi(aosTokens[4 * iColor + 3]));
                                        m_poCTFromMetadata->SetColorEntry(
                                            iColor, &sEntry);
                                    }
                                }
                            }

                            const char *pszTILE_FORMAT =
                                CSLFetchNameValue(papszMD, "TILE_FORMAT");
                            if (pszTILE_FORMAT)
                            {
                                m_osTFFromMetadata = pszTILE_FORMAT;
                                oMDMD.SetMetadataItem("TILE_FORMAT",
                                                      pszTILE_FORMAT,
                                                      "IMAGE_STRUCTURE");
                            }

                            const char *pszNodataValue =
                                CSLFetchNameValue(papszMD, "NODATA_VALUE");
                            if (pszNodataValue)
                            {
                                m_osNodataValueFromMetadata = pszNodataValue;
                            }
                        }

                        else if (!EQUAL(*papszIter, "") &&
                                 !STARTS_WITH(*papszIter, "BAND_"))
                        {
                            oMDMD.SetMetadata(
                                oLocalMDMD.GetMetadata(*papszIter), *papszIter);
                        }
                        papszIter++;
                    }
                }
                CPLDestroyXMLNode(psXMLNode);
            }
        }
    }

    GDALPamDataset::SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    papszMetadata = nullptr;

    /* Add non-GDAL metadata now */
    int nNonGDALMDILocal = 1;
    int nNonGDALMDIGeopackage = 1;
    for (int i = 0; i < oResult->RowCount(); i++)
    {
        const char *pszMetadata = oResult->GetValue(0, i);
        const char *pszMDStandardURI = oResult->GetValue(1, i);
        const char *pszMimeType = oResult->GetValue(2, i);
        const char *pszReferenceScope = oResult->GetValue(3, i);
        if (pszMetadata == nullptr || pszMDStandardURI == nullptr ||
            pszMimeType == nullptr || pszReferenceScope == nullptr)
        {
            // should not happen as there are NOT NULL constraints
            // But a database could lack such NOT NULL constraints or have
            // large values that would cause a memory allocation failure.
            continue;
        }
        int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if (EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/xml"))
            continue;

        if (!m_osRasterTable.empty() && bIsGPKGScope)
        {
            oMDMD.SetMetadataItem(
                CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDIGeopackage),
                pszMetadata, "GEOPACKAGE");
            nNonGDALMDIGeopackage++;
        }
        /*else if( strcmp( pszMDStandardURI, "http://www.isotc211.org/2005/gmd"
        ) == 0 && strcmp( pszMimeType, "text/xml" ) == 0 )
        {
            char* apszMD[2];
            apszMD[0] = (char*)pszMetadata;
            apszMD[1] = NULL;
            oMDMD.SetMetadata(apszMD, "xml:MD_Metadata");
        }*/
        else
        {
            oMDMD.SetMetadataItem(
                CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDILocal),
                pszMetadata);
            nNonGDALMDILocal++;
        }
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                            WriteMetadata()                           */
/************************************************************************/

void GDALGeoPackageDataset::WriteMetadata(
    CPLXMLNode *psXMLNode, /* will be destroyed by the method */
    const char *pszTableName)
{
    const bool bIsEmpty = (psXMLNode == nullptr);
    if (!HasMetadataTables())
    {
        if (bIsEmpty || !CreateMetadataTables())
        {
            CPLDestroyXMLNode(psXMLNode);
            return;
        }
    }

    char *pszXML = nullptr;
    if (!bIsEmpty)
    {
        CPLXMLNode *psMasterXMLNode =
            CPLCreateXMLNode(nullptr, CXT_Element, "GDALMultiDomainMetadata");
        psMasterXMLNode->psChild = psXMLNode;
        pszXML = CPLSerializeXMLTree(psMasterXMLNode);
        CPLDestroyXMLNode(psMasterXMLNode);
    }
    // cppcheck-suppress uselessAssignmentPtrArg
    psXMLNode = nullptr;

    char *pszSQL = nullptr;
    if (pszTableName && pszTableName[0] != '\0')
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.id FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE md.md_scope = 'dataset' AND "
            "md.md_standard_uri='http://gdal.org' "
            "AND md.mime_type='text/xml' AND mdr.reference_scope = 'table' AND "
            "lower(mdr.table_name) = lower('%q')",
            pszTableName);
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.id FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE md.md_scope = 'dataset' AND "
            "md.md_standard_uri='http://gdal.org' "
            "AND md.mime_type='text/xml' AND mdr.reference_scope = "
            "'geopackage'");
    }
    OGRErr err;
    int mdId = SQLGetInteger(hDB, pszSQL, &err);
    if (err != OGRERR_NONE)
        mdId = -1;
    sqlite3_free(pszSQL);

    if (bIsEmpty)
    {
        if (mdId >= 0)
        {
            SQLCommand(
                hDB,
                CPLSPrintf(
                    "DELETE FROM gpkg_metadata_reference WHERE md_file_id = %d",
                    mdId));
            SQLCommand(
                hDB,
                CPLSPrintf("DELETE FROM gpkg_metadata WHERE id = %d", mdId));
        }
    }
    else
    {
        if (mdId >= 0)
        {
            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_metadata SET metadata = '%q' WHERE id = %d",
                pszXML, mdId);
        }
        else
        {
            pszSQL =
                sqlite3_mprintf("INSERT INTO gpkg_metadata (md_scope, "
                                "md_standard_uri, mime_type, metadata) VALUES "
                                "('dataset','http://gdal.org','text/xml','%q')",
                                pszXML);
        }
        SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        CPLFree(pszXML);

        if (mdId < 0)
        {
            const sqlite_int64 nFID = sqlite3_last_insert_rowid(hDB);
            if (pszTableName != nullptr && pszTableName[0] != '\0')
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_metadata_reference (reference_scope, "
                    "table_name, timestamp, md_file_id) VALUES "
                    "('table', '%q', %s, %d)",
                    pszTableName, GetCurrentDateEscapedSQL().c_str(),
                    static_cast<int>(nFID));
            }
            else
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_metadata_reference (reference_scope, "
                    "timestamp, md_file_id) VALUES "
                    "('geopackage', %s, %d)",
                    GetCurrentDateEscapedSQL().c_str(), static_cast<int>(nFID));
            }
        }
        else
        {
            pszSQL = sqlite3_mprintf("UPDATE gpkg_metadata_reference SET "
                                     "timestamp = %s WHERE md_file_id = %d",
                                     GetCurrentDateEscapedSQL().c_str(), mdId);
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
        CPLTestBool(CPLGetConfigOption("CREATE_TRIGGERS", "NO"));

    /* From C.10. gpkg_metadata Table 35. gpkg_metadata Table Definition SQL  */
    CPLString osSQL = "CREATE TABLE gpkg_metadata ("
                      "id INTEGER CONSTRAINT m_pk PRIMARY KEY ASC NOT NULL,"
                      "md_scope TEXT NOT NULL DEFAULT 'dataset',"
                      "md_standard_uri TEXT NOT NULL,"
                      "mime_type TEXT NOT NULL DEFAULT 'text/xml',"
                      "metadata TEXT NOT NULL DEFAULT ''"
                      ")";

    /* From D.2. metadata Table 40. metadata Trigger Definition SQL  */
    const char *pszMetadataTriggers =
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
    if (bCreateTriggers)
    {
        osSQL += ";";
        osSQL += pszMetadataTriggers;
    }

    /* From C.11. gpkg_metadata_reference Table 36. gpkg_metadata_reference
     * Table Definition SQL */
    osSQL += ";"
             "CREATE TABLE gpkg_metadata_reference ("
             "reference_scope TEXT NOT NULL,"
             "table_name TEXT,"
             "column_name TEXT,"
             "row_id_value INTEGER,"
             "timestamp DATETIME NOT NULL DEFAULT "
             "(strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
             "md_file_id INTEGER NOT NULL,"
             "md_parent_id INTEGER,"
             "CONSTRAINT crmr_mfi_fk FOREIGN KEY (md_file_id) REFERENCES "
             "gpkg_metadata(id),"
             "CONSTRAINT crmr_mpi_fk FOREIGN KEY (md_parent_id) REFERENCES "
             "gpkg_metadata(id)"
             ")";

    /* From D.3. metadata_reference Table 41. gpkg_metadata_reference Trigger
     * Definition SQL   */
    const char *pszMetadataReferenceTriggers =
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
        "AND NEW.column_name IS NOT NULL); "
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
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_row_id_value_update' "
        "BEFORE UPDATE OF 'row_id_value' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: row_id_value must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"column\"') "
        "WHERE NEW.reference_scope IN ('geopackage','table','column') "
        "AND NEW.row_id_value IS NOT NULL; "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: timestamp must be a valid time in ISO 8601 "
        "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
        "WHERE NOT (NEW.timestamp GLOB "
        "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-"
        "5][0-9].[0-9][0-9][0-9]Z' "
        "AND strftime('%s',NEW.timestamp) NOT NULL); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_update' "
        "BEFORE UPDATE OF 'timestamp' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: timestamp must be a valid time in ISO 8601 "
        "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
        "WHERE NOT (NEW.timestamp GLOB "
        "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-"
        "5][0-9].[0-9][0-9][0-9]Z' "
        "AND strftime('%s',NEW.timestamp) NOT NULL); "
        "END";
    if (bCreateTriggers)
    {
        osSQL += ";";
        osSQL += pszMetadataReferenceTriggers;
    }

    if (CreateExtensionsTableIfNecessary() != OGRERR_NONE)
        return false;

    osSQL += ";";
    osSQL += "INSERT INTO gpkg_extensions "
             "(table_name, column_name, extension_name, definition, scope) "
             "VALUES "
             "('gpkg_metadata', NULL, 'gpkg_metadata', "
             "'http://www.geopackage.org/spec120/#extension_metadata', "
             "'read-write')";

    osSQL += ";";
    osSQL += "INSERT INTO gpkg_extensions "
             "(table_name, column_name, extension_name, definition, scope) "
             "VALUES "
             "('gpkg_metadata_reference', NULL, 'gpkg_metadata', "
             "'http://www.geopackage.org/spec120/#extension_metadata', "
             "'read-write')";

    const bool bOK = SQLCommand(hDB, osSQL) == OGRERR_NONE;
    m_nHasMetadataTables = bOK;
    return bOK;
}

/************************************************************************/
/*                            FlushMetadata()                           */
/************************************************************************/

void GDALGeoPackageDataset::FlushMetadata()
{
    if (!m_bMetadataDirty || m_poParentDS != nullptr ||
        m_nCreateMetadataTables == FALSE)
        return;
    m_bMetadataDirty = false;

    if (eAccess == GA_ReadOnly)
    {
        return;
    }

    bool bCanWriteAreaOrPoint =
        !m_bGridCellEncodingAsCO &&
        (m_eTF == GPKG_TF_PNG_16BIT || m_eTF == GPKG_TF_TIFF_32BIT_FLOAT);
    if (!m_osRasterTable.empty())
    {
        const char *pszIdentifier =
            GDALGeoPackageDataset::GetMetadataItem("IDENTIFIER");
        const char *pszDescription =
            GDALGeoPackageDataset::GetMetadataItem("DESCRIPTION");
        if (!m_bIdentifierAsCO && pszIdentifier != nullptr &&
            pszIdentifier != m_osIdentifier)
        {
            m_osIdentifier = pszIdentifier;
            char *pszSQL =
                sqlite3_mprintf("UPDATE gpkg_contents SET identifier = '%q' "
                                "WHERE lower(table_name) = lower('%q')",
                                pszIdentifier, m_osRasterTable.c_str());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
        if (!m_bDescriptionAsCO && pszDescription != nullptr &&
            pszDescription != m_osDescription)
        {
            m_osDescription = pszDescription;
            char *pszSQL =
                sqlite3_mprintf("UPDATE gpkg_contents SET description = '%q' "
                                "WHERE lower(table_name) = lower('%q')",
                                pszDescription, m_osRasterTable.c_str());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
        if (bCanWriteAreaOrPoint)
        {
            const char *pszAreaOrPoint =
                GDALGeoPackageDataset::GetMetadataItem(GDALMD_AREA_OR_POINT);
            if (pszAreaOrPoint && EQUAL(pszAreaOrPoint, GDALMD_AOP_AREA))
            {
                bCanWriteAreaOrPoint = false;
                char *pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_2d_gridded_coverage_ancillary SET "
                    "grid_cell_encoding = 'grid-value-is-area' WHERE "
                    "lower(tile_matrix_set_name) = lower('%q')",
                    m_osRasterTable.c_str());
                SQLCommand(hDB, pszSQL);
                sqlite3_free(pszSQL);
            }
            else if (pszAreaOrPoint && EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT))
            {
                bCanWriteAreaOrPoint = false;
                char *pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_2d_gridded_coverage_ancillary SET "
                    "grid_cell_encoding = 'grid-value-is-center' WHERE "
                    "lower(tile_matrix_set_name) = lower('%q')",
                    m_osRasterTable.c_str());
                SQLCommand(hDB, pszSQL);
                sqlite3_free(pszSQL);
            }
        }
    }

    char **papszMDDup = nullptr;
    for (char **papszIter = GDALGeoPackageDataset::GetMetadata();
         papszIter && *papszIter; ++papszIter)
    {
        if (STARTS_WITH_CI(*papszIter, "IDENTIFIER="))
            continue;
        if (STARTS_WITH_CI(*papszIter, "DESCRIPTION="))
            continue;
        if (STARTS_WITH_CI(*papszIter, "ZOOM_LEVEL="))
            continue;
        if (STARTS_WITH_CI(*papszIter, "GPKG_METADATA_ITEM_"))
            continue;
        if ((m_eTF == GPKG_TF_PNG_16BIT || m_eTF == GPKG_TF_TIFF_32BIT_FLOAT) &&
            !bCanWriteAreaOrPoint &&
            STARTS_WITH_CI(*papszIter, GDALMD_AREA_OR_POINT))
        {
            continue;
        }
        papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
    }

    CPLXMLNode *psXMLNode = nullptr;
    {
        GDALMultiDomainMetadata oLocalMDMD;
        CSLConstList papszDomainList = oMDMD.GetDomainList();
        CSLConstList papszIter = papszDomainList;
        oLocalMDMD.SetMetadata(papszMDDup);
        while (papszIter && *papszIter)
        {
            if (!EQUAL(*papszIter, "") &&
                !EQUAL(*papszIter, "IMAGE_STRUCTURE") &&
                !EQUAL(*papszIter, "GEOPACKAGE"))
            {
                oLocalMDMD.SetMetadata(oMDMD.GetMetadata(*papszIter),
                                       *papszIter);
            }
            papszIter++;
        }
        if (m_nBandCountFromMetadata > 0)
        {
            oLocalMDMD.SetMetadataItem(
                "BAND_COUNT", CPLSPrintf("%d", m_nBandCountFromMetadata),
                "IMAGE_STRUCTURE");
            if (nBands == 1)
            {
                const auto poCT = GetRasterBand(1)->GetColorTable();
                if (poCT)
                {
                    std::string osVal("{");
                    const int nColorCount = poCT->GetColorEntryCount();
                    for (int i = 0; i < nColorCount; ++i)
                    {
                        if (i > 0)
                            osVal += ',';
                        const GDALColorEntry *psEntry = poCT->GetColorEntry(i);
                        osVal +=
                            CPLSPrintf("{%d,%d,%d,%d}", psEntry->c1,
                                       psEntry->c2, psEntry->c3, psEntry->c4);
                    }
                    osVal += '}';
                    oLocalMDMD.SetMetadataItem("COLOR_TABLE", osVal.c_str(),
                                               "IMAGE_STRUCTURE");
                }
            }
            if (nBands == 1)
            {
                const char *pszTILE_FORMAT = nullptr;
                switch (m_eTF)
                {
                    case GPKG_TF_PNG_JPEG:
                        pszTILE_FORMAT = "JPEG_PNG";
                        break;
                    case GPKG_TF_PNG:
                        break;
                    case GPKG_TF_PNG8:
                        pszTILE_FORMAT = "PNG8";
                        break;
                    case GPKG_TF_JPEG:
                        pszTILE_FORMAT = "JPEG";
                        break;
                    case GPKG_TF_WEBP:
                        pszTILE_FORMAT = "WEBP";
                        break;
                    case GPKG_TF_PNG_16BIT:
                        break;
                    case GPKG_TF_TIFF_32BIT_FLOAT:
                        break;
                }
                if (pszTILE_FORMAT)
                    oLocalMDMD.SetMetadataItem("TILE_FORMAT", pszTILE_FORMAT,
                                               "IMAGE_STRUCTURE");
            }
        }
        if (GetRasterCount() > 0 &&
            GetRasterBand(1)->GetRasterDataType() == GDT_Byte)
        {
            int bHasNoData = FALSE;
            const double dfNoDataValue =
                GetRasterBand(1)->GetNoDataValue(&bHasNoData);
            if (bHasNoData)
            {
                oLocalMDMD.SetMetadataItem("NODATA_VALUE",
                                           CPLSPrintf("%.18g", dfNoDataValue),
                                           "IMAGE_STRUCTURE");
            }
        }
        for (int i = 1; i <= GetRasterCount(); ++i)
        {
            auto poBand =
                cpl::down_cast<GDALGeoPackageRasterBand *>(GetRasterBand(i));
            poBand->AddImplicitStatistics(false);
            char **papszMD = GetRasterBand(i)->GetMetadata();
            poBand->AddImplicitStatistics(true);
            if (papszMD)
            {
                oLocalMDMD.SetMetadata(papszMD, CPLSPrintf("BAND_%d", i));
            }
        }
        psXMLNode = oLocalMDMD.Serialize();
    }

    CSLDestroy(papszMDDup);
    papszMDDup = nullptr;

    WriteMetadata(psXMLNode, m_osRasterTable.c_str());

    if (!m_osRasterTable.empty())
    {
        char **papszGeopackageMD =
            GDALGeoPackageDataset::GetMetadata("GEOPACKAGE");

        papszMDDup = nullptr;
        for (char **papszIter = papszGeopackageMD; papszIter && *papszIter;
             ++papszIter)
        {
            papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
        }

        GDALMultiDomainMetadata oLocalMDMD;
        oLocalMDMD.SetMetadata(papszMDDup);
        CSLDestroy(papszMDDup);
        papszMDDup = nullptr;
        psXMLNode = oLocalMDMD.Serialize();

        WriteMetadata(psXMLNode, nullptr);
    }

    for (int i = 0; i < m_nLayers; i++)
    {
        const char *pszIdentifier =
            m_papoLayers[i]->GetMetadataItem("IDENTIFIER");
        const char *pszDescription =
            m_papoLayers[i]->GetMetadataItem("DESCRIPTION");
        if (pszIdentifier != nullptr)
        {
            char *pszSQL =
                sqlite3_mprintf("UPDATE gpkg_contents SET identifier = '%q' "
                                "WHERE lower(table_name) = lower('%q')",
                                pszIdentifier, m_papoLayers[i]->GetName());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
        if (pszDescription != nullptr)
        {
            char *pszSQL =
                sqlite3_mprintf("UPDATE gpkg_contents SET description = '%q' "
                                "WHERE lower(table_name) = lower('%q')",
                                pszDescription, m_papoLayers[i]->GetName());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }

        papszMDDup = nullptr;
        for (char **papszIter = m_papoLayers[i]->GetMetadata();
             papszIter && *papszIter; ++papszIter)
        {
            if (STARTS_WITH_CI(*papszIter, "IDENTIFIER="))
                continue;
            if (STARTS_WITH_CI(*papszIter, "DESCRIPTION="))
                continue;
            if (STARTS_WITH_CI(*papszIter, "OLMD_FID64="))
                continue;
            papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
        }

        {
            GDALMultiDomainMetadata oLocalMDMD;
            char **papszDomainList = m_papoLayers[i]->GetMetadataDomainList();
            char **papszIter = papszDomainList;
            oLocalMDMD.SetMetadata(papszMDDup);
            while (papszIter && *papszIter)
            {
                if (!EQUAL(*papszIter, ""))
                    oLocalMDMD.SetMetadata(
                        m_papoLayers[i]->GetMetadata(*papszIter), *papszIter);
                papszIter++;
            }
            CSLDestroy(papszDomainList);
            psXMLNode = oLocalMDMD.Serialize();
        }

        CSLDestroy(papszMDDup);
        papszMDDup = nullptr;

        WriteMetadata(psXMLNode, m_papoLayers[i]->GetName());
    }
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALGeoPackageDataset::GetMetadataItem(const char *pszName,
                                                   const char *pszDomain)
{
    pszDomain = CheckMetadataDomain(pszDomain);
    return CSLFetchNameValue(GetMetadata(pszDomain), pszName);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetMetadata(char **papszMetadata,
                                          const char *pszDomain)
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = true;
    GetMetadata(); /* force loading from storage if needed */
    return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetMetadataItem(const char *pszName,
                                              const char *pszValue,
                                              const char *pszDomain)
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = true;
    GetMetadata(); /* force loading from storage if needed */
    return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

int GDALGeoPackageDataset::Create(const char *pszFilename, int nXSize,
                                  int nYSize, int nBandsIn, GDALDataType eDT,
                                  char **papszOptions)
{
    CPLString osCommand;

    /* First, ensure there isn't any such file yet. */
    VSIStatBufL sStatBuf;

    if (nBandsIn != 0)
    {
        if (eDT == GDT_Byte)
        {
            if (nBandsIn != 1 && nBandsIn != 2 && nBandsIn != 3 &&
                nBandsIn != 4)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), "
                         "3 (RGB) or 4 (RGBA) band dataset supported for "
                         "Byte datatype");
                return FALSE;
            }
        }
        else if (eDT == GDT_Int16 || eDT == GDT_UInt16 || eDT == GDT_Float32)
        {
            if (nBandsIn != 1)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Only single band dataset supported for non Byte "
                         "datatype");
                return FALSE;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only Byte, Int16, UInt16 or Float32 supported");
            return FALSE;
        }
    }

    const size_t nFilenameLen = strlen(pszFilename);
    const bool bGpkgZip =
        (nFilenameLen > strlen(".gpkg.zip") &&
         !STARTS_WITH(pszFilename, "/vsizip/") &&
         EQUAL(pszFilename + nFilenameLen - strlen(".gpkg.zip"), ".gpkg.zip"));

    const bool bUseTempFile =
        bGpkgZip || (CPLTestBool(CPLGetConfigOption(
                         "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "NO")) &&
                     (VSIHasOptimizedReadMultiRange(pszFilename) != FALSE ||
                      EQUAL(CPLGetConfigOption(
                                "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", ""),
                            "FORCED")));

    bool bFileExists = false;
    if (VSIStatL(pszFilename, &sStatBuf) == 0)
    {
        bFileExists = true;
        if (nBandsIn == 0 || bUseTempFile ||
            !CPLTestBool(
                CSLFetchNameValueDef(papszOptions, "APPEND_SUBDATASET", "NO")))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "A file system object called '%s' already exists.",
                     pszFilename);

            return FALSE;
        }
    }

    if (bUseTempFile)
    {
        if (bGpkgZip)
        {
            std::string osFilenameInZip(CPLGetFilename(pszFilename));
            osFilenameInZip.resize(osFilenameInZip.size() - strlen(".zip"));
            m_osFinalFilename =
                std::string("/vsizip/{") + pszFilename + "}/" + osFilenameInZip;
        }
        else
        {
            m_osFinalFilename = pszFilename;
        }
        m_pszFilename =
            CPLStrdup(CPLGenerateTempFilename(CPLGetFilename(pszFilename)));
        CPLDebug("GPKG", "Creating temporary file %s", m_pszFilename);
    }
    else
    {
        m_pszFilename = CPLStrdup(pszFilename);
    }
    m_bNew = true;
    eAccess = GA_Update;
    m_bDateTimeWithTZ =
        EQUAL(CSLFetchNameValueDef(papszOptions, "DATETIME_FORMAT", "WITH_TZ"),
              "WITH_TZ");

    // for test/debug purposes only. true is the nominal value
    m_bPNGSupports2Bands =
        CPLTestBool(CPLGetConfigOption("GPKG_PNG_SUPPORTS_2BANDS", "TRUE"));
    m_bPNGSupportsCT =
        CPLTestBool(CPLGetConfigOption("GPKG_PNG_SUPPORTS_CT", "TRUE"));

    if (!OpenOrCreateDB(bFileExists
                            ? SQLITE_OPEN_READWRITE
                            : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return FALSE;

    /* Default to synchronous=off for performance for new file */
    if (!bFileExists &&
        CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", nullptr) == nullptr)
    {
        SQLCommand(hDB, "PRAGMA synchronous = OFF");
    }

    /* OGR UTF-8 support. If we set the UTF-8 Pragma early on, it */
    /* will be written into the main file and supported henceforth */
    SQLCommand(hDB, "PRAGMA encoding = \"UTF-8\"");

    if (bFileExists)
    {
        VSILFILE *fp = VSIFOpenL(pszFilename, "rb");
        if (fp)
        {
            GByte abyHeader[100];
            VSIFReadL(abyHeader, 1, sizeof(abyHeader), fp);
            VSIFCloseL(fp);

            memcpy(&m_nApplicationId, abyHeader + knApplicationIdPos, 4);
            m_nApplicationId = CPL_MSBWORD32(m_nApplicationId);
            memcpy(&m_nUserVersion, abyHeader + knUserVersionPos, 4);
            m_nUserVersion = CPL_MSBWORD32(m_nUserVersion);

            if (m_nApplicationId == GP10_APPLICATION_ID)
            {
                CPLDebug("GPKG", "GeoPackage v1.0");
            }
            else if (m_nApplicationId == GP11_APPLICATION_ID)
            {
                CPLDebug("GPKG", "GeoPackage v1.1");
            }
            else if (m_nApplicationId == GPKG_APPLICATION_ID &&
                     m_nUserVersion >= GPKG_1_2_VERSION)
            {
                CPLDebug("GPKG", "GeoPackage v%d.%d.%d", m_nUserVersion / 10000,
                         (m_nUserVersion % 10000) / 100, m_nUserVersion % 100);
            }
        }

        DetectSpatialRefSysColumns();
    }

    const char *pszVersion = CSLFetchNameValue(papszOptions, "VERSION");
    if (pszVersion && !EQUAL(pszVersion, "AUTO"))
    {
        if (EQUAL(pszVersion, "1.0"))
        {
            m_nApplicationId = GP10_APPLICATION_ID;
            m_nUserVersion = 0;
        }
        else if (EQUAL(pszVersion, "1.1"))
        {
            m_nApplicationId = GP11_APPLICATION_ID;
            m_nUserVersion = 0;
        }
        else if (EQUAL(pszVersion, "1.2"))
        {
            m_nApplicationId = GPKG_APPLICATION_ID;
            m_nUserVersion = GPKG_1_2_VERSION;
        }
        else if (EQUAL(pszVersion, "1.3"))
        {
            m_nApplicationId = GPKG_APPLICATION_ID;
            m_nUserVersion = GPKG_1_3_VERSION;
        }
        else if (EQUAL(pszVersion, "1.4"))
        {
            m_nApplicationId = GPKG_APPLICATION_ID;
            m_nUserVersion = GPKG_1_4_VERSION;
        }
    }

    SoftStartTransaction();

    CPLString osSQL;
    if (!bFileExists)
    {
        /* Requirement 10: A GeoPackage SHALL include a gpkg_spatial_ref_sys
         * table */
        /* http://opengis.github.io/geopackage/#spatial_ref_sys */
        osSQL = "CREATE TABLE gpkg_spatial_ref_sys ("
                "srs_name TEXT NOT NULL,"
                "srs_id INTEGER NOT NULL PRIMARY KEY,"
                "organization TEXT NOT NULL,"
                "organization_coordsys_id INTEGER NOT NULL,"
                "definition  TEXT NOT NULL,"
                "description TEXT";
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "CRS_WKT_EXTENSION",
                                             "NO")) ||
            (nBandsIn != 0 && eDT != GDT_Byte))
        {
            m_bHasDefinition12_063 = true;
            osSQL += ", definition_12_063 TEXT NOT NULL";
            if (m_nUserVersion >= GPKG_1_4_VERSION)
            {
                osSQL += ", epoch DOUBLE";
                m_bHasEpochColumn = true;
            }
        }
        osSQL += ")"
                 ";"
                 /* Requirement 11: The gpkg_spatial_ref_sys table in a
                    GeoPackage SHALL */
                 /* contain a record for EPSG:4326, the geodetic WGS84 SRS */
                 /* http://opengis.github.io/geopackage/#spatial_ref_sys */

                 "INSERT INTO gpkg_spatial_ref_sys ("
                 "srs_name, srs_id, organization, organization_coordsys_id, "
                 "definition, description";
        if (m_bHasDefinition12_063)
            osSQL += ", definition_12_063";
        osSQL +=
            ") VALUES ("
            "'WGS 84 geodetic', 4326, 'EPSG', 4326, '"
            "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS "
            "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],"
            "AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
            "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY["
            "\"EPSG\",\"9122\"]],AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\","
            "EAST],AUTHORITY[\"EPSG\",\"4326\"]]"
            "', 'longitude/latitude coordinates in decimal degrees on the WGS "
            "84 spheroid'";
        if (m_bHasDefinition12_063)
            osSQL +=
                ", 'GEODCRS[\"WGS 84\", DATUM[\"World Geodetic System 1984\", "
                "ELLIPSOID[\"WGS 84\",6378137, 298.257223563, "
                "LENGTHUNIT[\"metre\", 1.0]]], PRIMEM[\"Greenwich\", 0.0, "
                "ANGLEUNIT[\"degree\",0.0174532925199433]], CS[ellipsoidal, "
                "2], AXIS[\"latitude\", north, ORDER[1]], AXIS[\"longitude\", "
                "east, ORDER[2]], ANGLEUNIT[\"degree\", 0.0174532925199433], "
                "ID[\"EPSG\", 4326]]'";
        osSQL +=
            ")"
            ";"
            /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage
               SHALL */
            /* contain a record with an srs_id of -1, an organization of “NONE”,
             */
            /* an organization_coordsys_id of -1, and definition “undefined” */
            /* for undefined Cartesian coordinate reference systems */
            /* http://opengis.github.io/geopackage/#spatial_ref_sys */
            "INSERT INTO gpkg_spatial_ref_sys ("
            "srs_name, srs_id, organization, organization_coordsys_id, "
            "definition, description";
        if (m_bHasDefinition12_063)
            osSQL += ", definition_12_063";
        osSQL += ") VALUES ("
                 "'Undefined Cartesian SRS', -1, 'NONE', -1, 'undefined', "
                 "'undefined Cartesian coordinate reference system'";
        if (m_bHasDefinition12_063)
            osSQL += ", 'undefined'";
        osSQL +=
            ")"
            ";"
            /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage
               SHALL */
            /* contain a record with an srs_id of 0, an organization of “NONE”,
             */
            /* an organization_coordsys_id of 0, and definition “undefined” */
            /* for undefined geographic coordinate reference systems */
            /* http://opengis.github.io/geopackage/#spatial_ref_sys */
            "INSERT INTO gpkg_spatial_ref_sys ("
            "srs_name, srs_id, organization, organization_coordsys_id, "
            "definition, description";
        if (m_bHasDefinition12_063)
            osSQL += ", definition_12_063";
        osSQL += ") VALUES ("
                 "'Undefined geographic SRS', 0, 'NONE', 0, 'undefined', "
                 "'undefined geographic coordinate reference system'";
        if (m_bHasDefinition12_063)
            osSQL += ", 'undefined'";
        osSQL += ")"
                 ";"
                 /* Requirement 13: A GeoPackage file SHALL include a
                    gpkg_contents table */
                 /* http://opengis.github.io/geopackage/#_contents */
                 "CREATE TABLE gpkg_contents ("
                 "table_name TEXT NOT NULL PRIMARY KEY,"
                 "data_type TEXT NOT NULL,"
                 "identifier TEXT UNIQUE,"
                 "description TEXT DEFAULT '',"
                 "last_change DATETIME NOT NULL DEFAULT "
                 "(strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
                 "min_x DOUBLE, min_y DOUBLE,"
                 "max_x DOUBLE, max_y DOUBLE,"
                 "srs_id INTEGER,"
                 "CONSTRAINT fk_gc_r_srs_id FOREIGN KEY (srs_id) REFERENCES "
                 "gpkg_spatial_ref_sys(srs_id)"
                 ")";

#ifdef ENABLE_GPKG_OGR_CONTENTS
        if (CPLFetchBool(papszOptions, "ADD_GPKG_OGR_CONTENTS", true))
        {
            m_bHasGPKGOGRContents = true;
            osSQL += ";"
                     "CREATE TABLE gpkg_ogr_contents("
                     "table_name TEXT NOT NULL PRIMARY KEY,"
                     "feature_count INTEGER DEFAULT NULL"
                     ")";
        }
#endif

        /* Requirement 21: A GeoPackage with a gpkg_contents table row with a
         * “features” */
        /* data_type SHALL contain a gpkg_geometry_columns table or updateable
         * view */
        /* http://opengis.github.io/geopackage/#_geometry_columns */
        const bool bCreateGeometryColumns =
            CPLTestBool(CPLGetConfigOption("CREATE_GEOMETRY_COLUMNS", "YES"));
        if (bCreateGeometryColumns)
        {
            m_bHasGPKGGeometryColumns = true;
            osSQL += ";";
            osSQL += pszCREATE_GPKG_GEOMETRY_COLUMNS;
        }
    }

    const bool bCreateTriggers =
        CPLTestBool(CPLGetConfigOption("CREATE_TRIGGERS", "YES"));
    if ((bFileExists && nBandsIn != 0 &&
         SQLGetInteger(
             hDB,
             "SELECT 1 FROM sqlite_master WHERE name = 'gpkg_tile_matrix_set' "
             "AND type in ('table', 'view')",
             nullptr) == 0) ||
        (!bFileExists &&
         CPLTestBool(CPLGetConfigOption("CREATE_RASTER_TABLES", "YES"))))
    {
        if (!osSQL.empty())
            osSQL += ";";

        /* From C.5. gpkg_tile_matrix_set Table 28. gpkg_tile_matrix_set Table
         * Creation SQL  */
        osSQL += "CREATE TABLE gpkg_tile_matrix_set ("
                 "table_name TEXT NOT NULL PRIMARY KEY,"
                 "srs_id INTEGER NOT NULL,"
                 "min_x DOUBLE NOT NULL,"
                 "min_y DOUBLE NOT NULL,"
                 "max_x DOUBLE NOT NULL,"
                 "max_y DOUBLE NOT NULL,"
                 "CONSTRAINT fk_gtms_table_name FOREIGN KEY (table_name) "
                 "REFERENCES gpkg_contents(table_name),"
                 "CONSTRAINT fk_gtms_srs FOREIGN KEY (srs_id) REFERENCES "
                 "gpkg_spatial_ref_sys (srs_id)"
                 ")"
                 ";"

                 /* From C.6. gpkg_tile_matrix Table 29. gpkg_tile_matrix Table
                    Creation SQL */
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
                 "CONSTRAINT fk_tmm_table_name FOREIGN KEY (table_name) "
                 "REFERENCES gpkg_contents(table_name)"
                 ")";

        if (bCreateTriggers)
        {
            /* From D.1. gpkg_tile_matrix Table 39. gpkg_tile_matrix Trigger
             * Definition SQL */
            const char *pszTileMatrixTrigger =
                "CREATE TRIGGER 'gpkg_tile_matrix_zoom_level_insert' "
                "BEFORE INSERT ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' "
                "violates constraint: zoom_level cannot be less than 0') "
                "WHERE (NEW.zoom_level < 0); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_zoom_level_update' "
                "BEFORE UPDATE of zoom_level ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' "
                "violates constraint: zoom_level cannot be less than 0') "
                "WHERE (NEW.zoom_level < 0); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_matrix_width_insert' "
                "BEFORE INSERT ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' "
                "violates constraint: matrix_width cannot be less than 1') "
                "WHERE (NEW.matrix_width < 1); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_matrix_width_update' "
                "BEFORE UPDATE OF matrix_width ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' "
                "violates constraint: matrix_width cannot be less than 1') "
                "WHERE (NEW.matrix_width < 1); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_matrix_height_insert' "
                "BEFORE INSERT ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' "
                "violates constraint: matrix_height cannot be less than 1') "
                "WHERE (NEW.matrix_height < 1); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_matrix_height_update' "
                "BEFORE UPDATE OF matrix_height ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' "
                "violates constraint: matrix_height cannot be less than 1') "
                "WHERE (NEW.matrix_height < 1); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_pixel_x_size_insert' "
                "BEFORE INSERT ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' "
                "violates constraint: pixel_x_size must be greater than 0') "
                "WHERE NOT (NEW.pixel_x_size > 0); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_pixel_x_size_update' "
                "BEFORE UPDATE OF pixel_x_size ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' "
                "violates constraint: pixel_x_size must be greater than 0') "
                "WHERE NOT (NEW.pixel_x_size > 0); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_pixel_y_size_insert' "
                "BEFORE INSERT ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' "
                "violates constraint: pixel_y_size must be greater than 0') "
                "WHERE NOT (NEW.pixel_y_size > 0); "
                "END; "
                "CREATE TRIGGER 'gpkg_tile_matrix_pixel_y_size_update' "
                "BEFORE UPDATE OF pixel_y_size ON 'gpkg_tile_matrix' "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' "
                "violates constraint: pixel_y_size must be greater than 0') "
                "WHERE NOT (NEW.pixel_y_size > 0); "
                "END;";
            osSQL += ";";
            osSQL += pszTileMatrixTrigger;
        }
    }

    if (!osSQL.empty() && OGRERR_NONE != SQLCommand(hDB, osSQL))
        return FALSE;

    if (!bFileExists)
    {
        const char *pszMetadataTables =
            CSLFetchNameValue(papszOptions, "METADATA_TABLES");
        if (pszMetadataTables)
            m_nCreateMetadataTables = int(CPLTestBool(pszMetadataTables));

        if (m_nCreateMetadataTables == TRUE && !CreateMetadataTables())
            return FALSE;

        if (m_bHasDefinition12_063)
        {
            if (OGRERR_NONE != CreateExtensionsTableIfNecessary() ||
                OGRERR_NONE !=
                    SQLCommand(hDB, "INSERT INTO gpkg_extensions "
                                    "(table_name, column_name, extension_name, "
                                    "definition, scope) "
                                    "VALUES "
                                    "('gpkg_spatial_ref_sys', "
                                    "'definition_12_063', 'gpkg_crs_wkt', "
                                    "'http://www.geopackage.org/spec120/"
                                    "#extension_crs_wkt', 'read-write')"))
            {
                return FALSE;
            }
            if (m_bHasEpochColumn)
            {
                if (OGRERR_NONE !=
                        SQLCommand(
                            hDB, "UPDATE gpkg_extensions SET extension_name = "
                                 "'gpkg_crs_wkt_1_1' "
                                 "WHERE extension_name = 'gpkg_crs_wkt'") ||
                    OGRERR_NONE !=
                        SQLCommand(hDB, "INSERT INTO gpkg_extensions "
                                        "(table_name, column_name, "
                                        "extension_name, definition, scope) "
                                        "VALUES "
                                        "('gpkg_spatial_ref_sys', 'epoch', "
                                        "'gpkg_crs_wkt_1_1', "
                                        "'http://www.geopackage.org/spec/"
                                        "#extension_crs_wkt', "
                                        "'read-write')"))
                {
                    return FALSE;
                }
            }
        }
    }

    if (nBandsIn != 0)
    {
        const char *pszTableName = CPLGetBasename(m_pszFilename);
        m_osRasterTable =
            CSLFetchNameValueDef(papszOptions, "RASTER_TABLE", pszTableName);
        if (m_osRasterTable.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RASTER_TABLE must be set to a non empty value");
            return FALSE;
        }
        m_bIdentifierAsCO =
            CSLFetchNameValue(papszOptions, "RASTER_IDENTIFIER") != nullptr;
        m_osIdentifier = CSLFetchNameValueDef(papszOptions, "RASTER_IDENTIFIER",
                                              m_osRasterTable);
        m_bDescriptionAsCO =
            CSLFetchNameValue(papszOptions, "RASTER_DESCRIPTION") != nullptr;
        m_osDescription =
            CSLFetchNameValueDef(papszOptions, "RASTER_DESCRIPTION", "");
        SetDataType(eDT);
        if (eDT == GDT_Int16)
            SetGlobalOffsetScale(-32768.0, 1.0);

        /* From C.7. sample_tile_pyramid (Informative) Table 31. EXAMPLE: tiles
         * table Create Table SQL (Informative) */
        char *pszSQL =
            sqlite3_mprintf("CREATE TABLE \"%w\" ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                            "zoom_level INTEGER NOT NULL,"
                            "tile_column INTEGER NOT NULL,"
                            "tile_row INTEGER NOT NULL,"
                            "tile_data BLOB NOT NULL,"
                            "UNIQUE (zoom_level, tile_column, tile_row)"
                            ")",
                            m_osRasterTable.c_str());
        osSQL = pszSQL;
        sqlite3_free(pszSQL);

        if (bCreateTriggers)
        {
            /* From D.5. sample_tile_pyramid Table 43. tiles table Trigger
             * Definition SQL  */
            pszSQL = sqlite3_mprintf(
                "CREATE TRIGGER \"%w_zoom_insert\" "
                "BEFORE INSERT ON \"%w\" "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''%q'' violates "
                "constraint: zoom_level not specified for table in "
                "gpkg_tile_matrix') "
                "WHERE NOT (NEW.zoom_level IN (SELECT zoom_level FROM "
                "gpkg_tile_matrix WHERE lower(table_name) = lower('%q'))) ; "
                "END; "
                "CREATE TRIGGER \"%w_zoom_update\" "
                "BEFORE UPDATE OF zoom_level ON \"%w\" "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''%q'' violates "
                "constraint: zoom_level not specified for table in "
                "gpkg_tile_matrix') "
                "WHERE NOT (NEW.zoom_level IN (SELECT zoom_level FROM "
                "gpkg_tile_matrix WHERE lower(table_name) = lower('%q'))) ; "
                "END; "
                "CREATE TRIGGER \"%w_tile_column_insert\" "
                "BEFORE INSERT ON \"%w\" "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''%q'' violates "
                "constraint: tile_column cannot be < 0') "
                "WHERE (NEW.tile_column < 0) ; "
                "SELECT RAISE(ABORT, 'insert on table ''%q'' violates "
                "constraint: tile_column must by < matrix_width specified for "
                "table and zoom level in gpkg_tile_matrix') "
                "WHERE NOT (NEW.tile_column < (SELECT matrix_width FROM "
                "gpkg_tile_matrix WHERE lower(table_name) = lower('%q') AND "
                "zoom_level = NEW.zoom_level)); "
                "END; "
                "CREATE TRIGGER \"%w_tile_column_update\" "
                "BEFORE UPDATE OF tile_column ON \"%w\" "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''%q'' violates "
                "constraint: tile_column cannot be < 0') "
                "WHERE (NEW.tile_column < 0) ; "
                "SELECT RAISE(ABORT, 'update on table ''%q'' violates "
                "constraint: tile_column must by < matrix_width specified for "
                "table and zoom level in gpkg_tile_matrix') "
                "WHERE NOT (NEW.tile_column < (SELECT matrix_width FROM "
                "gpkg_tile_matrix WHERE lower(table_name) = lower('%q') AND "
                "zoom_level = NEW.zoom_level)); "
                "END; "
                "CREATE TRIGGER \"%w_tile_row_insert\" "
                "BEFORE INSERT ON \"%w\" "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'insert on table ''%q'' violates "
                "constraint: tile_row cannot be < 0') "
                "WHERE (NEW.tile_row < 0) ; "
                "SELECT RAISE(ABORT, 'insert on table ''%q'' violates "
                "constraint: tile_row must by < matrix_height specified for "
                "table and zoom level in gpkg_tile_matrix') "
                "WHERE NOT (NEW.tile_row < (SELECT matrix_height FROM "
                "gpkg_tile_matrix WHERE lower(table_name) = lower('%q') AND "
                "zoom_level = NEW.zoom_level)); "
                "END; "
                "CREATE TRIGGER \"%w_tile_row_update\" "
                "BEFORE UPDATE OF tile_row ON \"%w\" "
                "FOR EACH ROW BEGIN "
                "SELECT RAISE(ABORT, 'update on table ''%q'' violates "
                "constraint: tile_row cannot be < 0') "
                "WHERE (NEW.tile_row < 0) ; "
                "SELECT RAISE(ABORT, 'update on table ''%q'' violates "
                "constraint: tile_row must by < matrix_height specified for "
                "table and zoom level in gpkg_tile_matrix') "
                "WHERE NOT (NEW.tile_row < (SELECT matrix_height FROM "
                "gpkg_tile_matrix WHERE lower(table_name) = lower('%q') AND "
                "zoom_level = NEW.zoom_level)); "
                "END; ",
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str(),
                m_osRasterTable.c_str(), m_osRasterTable.c_str());

            osSQL += ";";
            osSQL += pszSQL;
            sqlite3_free(pszSQL);
        }

        OGRErr eErr = SQLCommand(hDB, osSQL);
        if (OGRERR_NONE != eErr)
            return FALSE;

        const char *pszTF = CSLFetchNameValue(papszOptions, "TILE_FORMAT");
        if (eDT == GDT_Int16 || eDT == GDT_UInt16)
        {
            m_eTF = GPKG_TF_PNG_16BIT;
            if (pszTF)
            {
                if (!EQUAL(pszTF, "AUTO") && !EQUAL(pszTF, "PNG"))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Only AUTO or PNG supported "
                             "as tile format for Int16 / UInt16");
                }
            }
        }
        else if (eDT == GDT_Float32)
        {
            m_eTF = GPKG_TF_TIFF_32BIT_FLOAT;
            if (pszTF)
            {
                if (EQUAL(pszTF, "PNG"))
                    m_eTF = GPKG_TF_PNG_16BIT;
                else if (!EQUAL(pszTF, "AUTO") && !EQUAL(pszTF, "TIFF"))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Only AUTO, PNG or TIFF supported "
                             "as tile format for Float32");
                }
            }
        }
        else
        {
            if (pszTF)
            {
                m_eTF = GDALGPKGMBTilesGetTileFormat(pszTF);
                if (nBandsIn == 1 && m_eTF != GPKG_TF_PNG)
                    m_bMetadataDirty = true;
            }
            else if (nBandsIn == 1)
                m_eTF = GPKG_TF_PNG;
        }

        if (eDT != GDT_Byte)
        {
            if (!CreateTileGriddedTable(papszOptions))
                return FALSE;
        }

        nRasterXSize = nXSize;
        nRasterYSize = nYSize;

        const char *pszTileSize =
            CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "256");
        const char *pszTileWidth =
            CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", pszTileSize);
        const char *pszTileHeight =
            CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", pszTileSize);
        int nTileWidth = atoi(pszTileWidth);
        int nTileHeight = atoi(pszTileHeight);
        if ((nTileWidth < 8 || nTileWidth > 4096 || nTileHeight < 8 ||
             nTileHeight > 4096) &&
            !CPLTestBool(CPLGetConfigOption("GPKG_ALLOW_CRAZY_SETTINGS", "NO")))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid block dimensions: %dx%d", nTileWidth,
                     nTileHeight);
            return FALSE;
        }

        for (int i = 1; i <= nBandsIn; i++)
            SetBand(
                i, new GDALGeoPackageRasterBand(this, nTileWidth, nTileHeight));

        GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL",
                                        "IMAGE_STRUCTURE");
        GDALPamDataset::SetMetadataItem("IDENTIFIER", m_osIdentifier);
        if (!m_osDescription.empty())
            GDALPamDataset::SetMetadataItem("DESCRIPTION", m_osDescription);

        ParseCompressionOptions(papszOptions);

        if (m_eTF == GPKG_TF_WEBP)
        {
            if (!RegisterWebPExtension())
                return FALSE;
        }

        m_osTilingScheme =
            CSLFetchNameValueDef(papszOptions, "TILING_SCHEME", "CUSTOM");
        if (!EQUAL(m_osTilingScheme, "CUSTOM"))
        {
            const auto poTS = GetTilingScheme(m_osTilingScheme);
            if (!poTS)
                return FALSE;

            if (nTileWidth != poTS->nTileWidth ||
                nTileHeight != poTS->nTileHeight)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Tile dimension should be %dx%d for %s tiling scheme",
                         poTS->nTileWidth, poTS->nTileHeight,
                         m_osTilingScheme.c_str());
                return FALSE;
            }

            const char *pszZoomLevel =
                CSLFetchNameValue(papszOptions, "ZOOM_LEVEL");
            if (pszZoomLevel)
            {
                m_nZoomLevel = atoi(pszZoomLevel);
                int nMaxZoomLevelForThisTM = MAX_ZOOM_LEVEL;
                while ((1 << nMaxZoomLevelForThisTM) >
                           INT_MAX / poTS->nTileXCountZoomLevel0 ||
                       (1 << nMaxZoomLevelForThisTM) >
                           INT_MAX / poTS->nTileYCountZoomLevel0)
                {
                    --nMaxZoomLevelForThisTM;
                }

                if (m_nZoomLevel < 0 || m_nZoomLevel > nMaxZoomLevelForThisTM)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ZOOM_LEVEL = %s is invalid. It should be in "
                             "[0,%d] range",
                             pszZoomLevel, nMaxZoomLevelForThisTM);
                    return FALSE;
                }
            }

            // Implicitly sets SRS.
            OGRSpatialReference oSRS;
            if (oSRS.importFromEPSG(poTS->nEPSGCode) != OGRERR_NONE)
                return FALSE;
            char *pszWKT = nullptr;
            oSRS.exportToWkt(&pszWKT);
            SetProjection(pszWKT);
            CPLFree(pszWKT);
        }
        else
        {
            if (CSLFetchNameValue(papszOptions, "ZOOM_LEVEL"))
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "ZOOM_LEVEL only supported for TILING_SCHEME != CUSTOM");
                return false;
            }
        }
    }

    if (bFileExists && nBandsIn > 0 && eDT == GDT_Byte)
    {
        // If there was an ogr_empty_table table, we can remove it
        RemoveOGREmptyTable();
    }

    SoftCommitTransaction();

    /* Requirement 2 */
    /* We have to do this after there's some content so the database file */
    /* is not zero length */
    SetApplicationAndUserVersionId();

    /* Default to synchronous=off for performance for new file */
    if (!bFileExists &&
        CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", nullptr) == nullptr)
    {
        SQLCommand(hDB, "PRAGMA synchronous = OFF");
    }

    return TRUE;
}

/************************************************************************/
/*                        RemoveOGREmptyTable()                         */
/************************************************************************/

void GDALGeoPackageDataset::RemoveOGREmptyTable()
{
    // Run with sqlite3_exec since we don't want errors to be emitted
    sqlite3_exec(hDB, "DROP TABLE IF EXISTS ogr_empty_table", nullptr, nullptr,
                 nullptr);
    sqlite3_exec(
        hDB, "DELETE FROM gpkg_contents WHERE table_name = 'ogr_empty_table'",
        nullptr, nullptr, nullptr);
#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_bHasGPKGOGRContents)
    {
        sqlite3_exec(hDB,
                     "DELETE FROM gpkg_ogr_contents WHERE "
                     "table_name = 'ogr_empty_table'",
                     nullptr, nullptr, nullptr);
    }
#endif
    sqlite3_exec(hDB,
                 "DELETE FROM gpkg_geometry_columns WHERE "
                 "table_name = 'ogr_empty_table'",
                 nullptr, nullptr, nullptr);
}

/************************************************************************/
/*                        CreateTileGriddedTable()                      */
/************************************************************************/

bool GDALGeoPackageDataset::CreateTileGriddedTable(char **papszOptions)
{
    CPLString osSQL;
    if (!HasGriddedCoverageAncillaryTable())
    {
        // It doesn't exist. So create gpkg_extensions table if necessary, and
        // gpkg_2d_gridded_coverage_ancillary & gpkg_2d_gridded_tile_ancillary,
        // and register them as extensions.
        if (CreateExtensionsTableIfNecessary() != OGRERR_NONE)
            return false;

        // Req 1 /table-defs/coverage-ancillary
        osSQL = "CREATE TABLE gpkg_2d_gridded_coverage_ancillary ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                "tile_matrix_set_name TEXT NOT NULL UNIQUE,"
                "datatype TEXT NOT NULL DEFAULT 'integer',"
                "scale REAL NOT NULL DEFAULT 1.0,"
                "offset REAL NOT NULL DEFAULT 0.0,"
                "precision REAL DEFAULT 1.0,"
                "data_null REAL,"
                "grid_cell_encoding TEXT DEFAULT 'grid-value-is-center',"
                "uom TEXT,"
                "field_name TEXT DEFAULT 'Height',"
                "quantity_definition TEXT DEFAULT 'Height',"
                "CONSTRAINT fk_g2dgtct_name FOREIGN KEY(tile_matrix_set_name) "
                "REFERENCES gpkg_tile_matrix_set ( table_name ) "
                "CHECK (datatype in ('integer','float')))"
                ";"
                // Requirement 2 /table-defs/tile-ancillary
                "CREATE TABLE gpkg_2d_gridded_tile_ancillary ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                "tpudt_name TEXT NOT NULL,"
                "tpudt_id INTEGER NOT NULL,"
                "scale REAL NOT NULL DEFAULT 1.0,"
                "offset REAL NOT NULL DEFAULT 0.0,"
                "min REAL DEFAULT NULL,"
                "max REAL DEFAULT NULL,"
                "mean REAL DEFAULT NULL,"
                "std_dev REAL DEFAULT NULL,"
                "CONSTRAINT fk_g2dgtat_name FOREIGN KEY (tpudt_name) "
                "REFERENCES gpkg_contents(table_name),"
                "UNIQUE (tpudt_name, tpudt_id))"
                ";"
                // Requirement 6 /gpkg-extensions
                "INSERT INTO gpkg_extensions "
                "(table_name, column_name, extension_name, definition, scope) "
                "VALUES ('gpkg_2d_gridded_coverage_ancillary', NULL, "
                "'gpkg_2d_gridded_coverage', "
                "'http://docs.opengeospatial.org/is/17-066r1/17-066r1.html', "
                "'read-write')"
                ";"
                // Requirement 6 /gpkg-extensions
                "INSERT INTO gpkg_extensions "
                "(table_name, column_name, extension_name, definition, scope) "
                "VALUES ('gpkg_2d_gridded_tile_ancillary', NULL, "
                "'gpkg_2d_gridded_coverage', "
                "'http://docs.opengeospatial.org/is/17-066r1/17-066r1.html', "
                "'read-write')"
                ";";
    }

    // Requirement 6 /gpkg-extensions
    char *pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES ('%q', 'tile_data', "
        "'gpkg_2d_gridded_coverage', "
        "'http://docs.opengeospatial.org/is/17-066r1/17-066r1.html', "
        "'read-write')",
        m_osRasterTable.c_str());
    osSQL += pszSQL;
    osSQL += ";";
    sqlite3_free(pszSQL);

    // Requirement 7 /gpkg-2d-gridded-coverage-ancillary
    // Requirement 8 /gpkg-2d-gridded-coverage-ancillary-set-name
    // Requirement 9 /gpkg-2d-gridded-coverage-ancillary-datatype
    m_dfPrecision =
        CPLAtof(CSLFetchNameValueDef(papszOptions, "PRECISION", "1"));
    CPLString osGridCellEncoding(CSLFetchNameValueDef(
        papszOptions, "GRID_CELL_ENCODING", "grid-value-is-center"));
    m_bGridCellEncodingAsCO =
        CSLFetchNameValue(papszOptions, "GRID_CELL_ENCODING") != nullptr;
    CPLString osUom(CSLFetchNameValueDef(papszOptions, "UOM", ""));
    CPLString osFieldName(
        CSLFetchNameValueDef(papszOptions, "FIELD_NAME", "Height"));
    CPLString osQuantityDefinition(
        CSLFetchNameValueDef(papszOptions, "QUANTITY_DEFINITION", "Height"));

    pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_2d_gridded_coverage_ancillary "
        "(tile_matrix_set_name, datatype, scale, offset, precision, "
        "grid_cell_encoding, uom, field_name, quantity_definition) "
        "VALUES (%Q, '%s', %.18g, %.18g, %.18g, %Q, %Q, %Q, %Q)",
        m_osRasterTable.c_str(),
        (m_eTF == GPKG_TF_PNG_16BIT) ? "integer" : "float", m_dfScale,
        m_dfOffset, m_dfPrecision, osGridCellEncoding.c_str(),
        osUom.empty() ? nullptr : osUom.c_str(), osFieldName.c_str(),
        osQuantityDefinition.c_str());
    m_osSQLInsertIntoGpkg2dGriddedCoverageAncillary = pszSQL;
    sqlite3_free(pszSQL);

    // Requirement 3 /gpkg-spatial-ref-sys-row
    auto oResultTable = SQLQuery(
        hDB, "SELECT * FROM gpkg_spatial_ref_sys WHERE srs_id = 4979 LIMIT 2");
    bool bHasEPSG4979 = (oResultTable && oResultTable->RowCount() == 1);
    if (!bHasEPSG4979)
    {
        if (!m_bHasDefinition12_063 &&
            !ConvertGpkgSpatialRefSysToExtensionWkt2(/*bForceEpoch=*/false))
        {
            return false;
        }

        // This is WKT 2...
        const char *pszWKT =
            "GEODCRS[\"WGS 84\","
            "DATUM[\"World Geodetic System 1984\","
            "  ELLIPSOID[\"WGS 84\",6378137,298.257223563,"
            "LENGTHUNIT[\"metre\",1.0]]],"
            "CS[ellipsoidal,3],"
            "  AXIS[\"latitude\",north,ORDER[1],ANGLEUNIT[\"degree\","
            "0.0174532925199433]],"
            "  AXIS[\"longitude\",east,ORDER[2],ANGLEUNIT[\"degree\","
            "0.0174532925199433]],"
            "  AXIS[\"ellipsoidal height\",up,ORDER[3],"
            "LENGTHUNIT[\"metre\",1.0]],"
            "ID[\"EPSG\",4979]]";

        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_spatial_ref_sys "
            "(srs_name,srs_id,organization,organization_coordsys_id,"
            "definition,definition_12_063) VALUES "
            "('WGS 84 3D', 4979, 'EPSG', 4979, 'undefined', '%q')",
            pszWKT);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

    return SQLCommand(hDB, osSQL) == OGRERR_NONE;
}

/************************************************************************/
/*                    HasGriddedCoverageAncillaryTable()                */
/************************************************************************/

bool GDALGeoPackageDataset::HasGriddedCoverageAncillaryTable()
{
    auto oResultTable = SQLQuery(
        hDB, "SELECT * FROM sqlite_master WHERE type IN ('table', 'view') AND "
             "name = 'gpkg_2d_gridded_coverage_ancillary'");
    bool bHasTable = (oResultTable && oResultTable->RowCount() == 1);
    return bHasTable;
}

/************************************************************************/
/*                      GetUnderlyingDataset()                          */
/************************************************************************/

static GDALDataset *GetUnderlyingDataset(GDALDataset *poSrcDS)
{
    if (auto poVRTDS = dynamic_cast<VRTDataset *>(poSrcDS))
    {
        auto poTmpDS = poVRTDS->GetSingleSimpleSource();
        if (poTmpDS)
            return poTmpDS;
    }

    return poSrcDS;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

typedef struct
{
    const char *pszName;
    GDALResampleAlg eResampleAlg;
} WarpResamplingAlg;

static const WarpResamplingAlg asResamplingAlg[] = {
    {"NEAREST", GRA_NearestNeighbour},
    {"BILINEAR", GRA_Bilinear},
    {"CUBIC", GRA_Cubic},
    {"CUBICSPLINE", GRA_CubicSpline},
    {"LANCZOS", GRA_Lanczos},
    {"MODE", GRA_Mode},
    {"AVERAGE", GRA_Average},
    {"RMS", GRA_RMS},
};

GDALDataset *GDALGeoPackageDataset::CreateCopy(const char *pszFilename,
                                               GDALDataset *poSrcDS,
                                               int bStrict, char **papszOptions,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData)
{
    const char *pszTilingScheme =
        CSLFetchNameValueDef(papszOptions, "TILING_SCHEME", "CUSTOM");

    CPLStringList apszUpdatedOptions(CSLDuplicate(papszOptions));
    if (CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "APPEND_SUBDATASET", "NO")) &&
        CSLFetchNameValue(papszOptions, "RASTER_TABLE") == nullptr)
    {
        CPLString osBasename(
            CPLGetBasename(GetUnderlyingDataset(poSrcDS)->GetDescription()));
        apszUpdatedOptions.SetNameValue("RASTER_TABLE", osBasename);
    }

    const int nBands = poSrcDS->GetRasterCount();
    if (nBands != 1 && nBands != 2 && nBands != 3 && nBands != 4)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), 3 (RGB) or "
                 "4 (RGBA) band dataset supported");
        return nullptr;
    }

    const char *pszUnitType = poSrcDS->GetRasterBand(1)->GetUnitType();
    if (CSLFetchNameValue(papszOptions, "UOM") == nullptr && pszUnitType &&
        !EQUAL(pszUnitType, ""))
    {
        apszUpdatedOptions.SetNameValue("UOM", pszUnitType);
    }

    if (EQUAL(pszTilingScheme, "CUSTOM"))
    {
        if (CSLFetchNameValue(papszOptions, "ZOOM_LEVEL"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "ZOOM_LEVEL only supported for TILING_SCHEME != CUSTOM");
            return nullptr;
        }

        GDALGeoPackageDataset *poDS = nullptr;
        GDALDriver *poThisDriver =
            reinterpret_cast<GDALDriver *>(GDALGetDriverByName("GPKG"));
        if (poThisDriver != nullptr)
        {
            poDS = cpl::down_cast<GDALGeoPackageDataset *>(
                poThisDriver->DefaultCreateCopy(pszFilename, poSrcDS, bStrict,
                                                apszUpdatedOptions, pfnProgress,
                                                pProgressData));

            if (poDS != nullptr &&
                poSrcDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte &&
                nBands <= 3)
            {
                poDS->m_nBandCountFromMetadata = nBands;
                poDS->m_bMetadataDirty = true;
            }
        }
        if (poDS)
            poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
        return poDS;
    }

    const auto poTS = GetTilingScheme(pszTilingScheme);
    if (!poTS)
    {
        return nullptr;
    }
    const int nEPSGCode = poTS->nEPSGCode;

    OGRSpatialReference oSRS;
    if (oSRS.importFromEPSG(nEPSGCode) != OGRERR_NONE)
    {
        return nullptr;
    }
    char *pszWKT = nullptr;
    oSRS.exportToWkt(&pszWKT);
    char **papszTO = CSLSetNameValue(nullptr, "DST_SRS", pszWKT);

    void *hTransformArg = nullptr;

    // Hack to compensate for GDALSuggestedWarpOutput2() failure (or not
    // ideal suggestion with PROJ 8) when reprojecting latitude = +/- 90 to
    // EPSG:3857.
    double adfSrcGeoTransform[6];
    std::unique_ptr<GDALDataset> poTmpDS;
    bool bEPSG3857Adjust = false;
    if (nEPSGCode == 3857 &&
        poSrcDS->GetGeoTransform(adfSrcGeoTransform) == CE_None &&
        adfSrcGeoTransform[2] == 0 && adfSrcGeoTransform[4] == 0 &&
        adfSrcGeoTransform[5] < 0)
    {
        const auto poSrcSRS = poSrcDS->GetSpatialRef();
        if (poSrcSRS && poSrcSRS->IsGeographic())
        {
            double maxLat = adfSrcGeoTransform[3];
            double minLat = adfSrcGeoTransform[3] +
                            poSrcDS->GetRasterYSize() * adfSrcGeoTransform[5];
            // Corresponds to the latitude of below MAX_GM
            constexpr double MAX_LAT = 85.0511287798066;
            bool bModified = false;
            if (maxLat > MAX_LAT)
            {
                maxLat = MAX_LAT;
                bModified = true;
            }
            if (minLat < -MAX_LAT)
            {
                minLat = -MAX_LAT;
                bModified = true;
            }
            if (bModified)
            {
                CPLStringList aosOptions;
                aosOptions.AddString("-of");
                aosOptions.AddString("VRT");
                aosOptions.AddString("-projwin");
                aosOptions.AddString(
                    CPLSPrintf("%.18g", adfSrcGeoTransform[0]));
                aosOptions.AddString(CPLSPrintf("%.18g", maxLat));
                aosOptions.AddString(
                    CPLSPrintf("%.18g", adfSrcGeoTransform[0] +
                                            poSrcDS->GetRasterXSize() *
                                                adfSrcGeoTransform[1]));
                aosOptions.AddString(CPLSPrintf("%.18g", minLat));
                auto psOptions =
                    GDALTranslateOptionsNew(aosOptions.List(), nullptr);
                poTmpDS.reset(GDALDataset::FromHandle(GDALTranslate(
                    "", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
                GDALTranslateOptionsFree(psOptions);
                if (poTmpDS)
                {
                    bEPSG3857Adjust = true;
                    hTransformArg = GDALCreateGenImgProjTransformer2(
                        GDALDataset::FromHandle(poTmpDS.get()), nullptr,
                        papszTO);
                }
            }
        }
    }
    if (hTransformArg == nullptr)
    {
        hTransformArg =
            GDALCreateGenImgProjTransformer2(poSrcDS, nullptr, papszTO);
    }

    if (hTransformArg == nullptr)
    {
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return nullptr;
    }

    GDALTransformerInfo *psInfo =
        static_cast<GDALTransformerInfo *>(hTransformArg);
    double adfGeoTransform[6];
    double adfExtent[4];
    int nXSize, nYSize;

    if (GDALSuggestedWarpOutput2(poSrcDS, psInfo->pfnTransform, hTransformArg,
                                 adfGeoTransform, &nXSize, &nYSize, adfExtent,
                                 0) != CE_None)
    {
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        GDALDestroyGenImgProjTransformer(hTransformArg);
        return nullptr;
    }

    GDALDestroyGenImgProjTransformer(hTransformArg);
    hTransformArg = nullptr;
    poTmpDS.reset();

    if (bEPSG3857Adjust)
    {
        constexpr double SPHERICAL_RADIUS = 6378137.0;
        constexpr double MAX_GM =
            SPHERICAL_RADIUS * M_PI;  // 20037508.342789244
        double maxNorthing = adfGeoTransform[3];
        double minNorthing = adfGeoTransform[3] + adfGeoTransform[5] * nYSize;
        bool bChanged = false;
        if (maxNorthing > MAX_GM)
        {
            bChanged = true;
            maxNorthing = MAX_GM;
        }
        if (minNorthing < -MAX_GM)
        {
            bChanged = true;
            minNorthing = -MAX_GM;
        }
        if (bChanged)
        {
            adfGeoTransform[3] = maxNorthing;
            nYSize =
                int((maxNorthing - minNorthing) / (-adfGeoTransform[5]) + 0.5);
            adfExtent[1] = maxNorthing + nYSize * adfGeoTransform[5];
            adfExtent[3] = maxNorthing;
        }
    }

    double dfComputedRes = adfGeoTransform[1];
    double dfPrevRes = 0.0;
    double dfRes = 0.0;
    int nZoomLevel = 0;  // Used after for.
    const char *pszZoomLevel = CSLFetchNameValue(papszOptions, "ZOOM_LEVEL");
    if (pszZoomLevel)
    {
        nZoomLevel = atoi(pszZoomLevel);

        int nMaxZoomLevelForThisTM = MAX_ZOOM_LEVEL;
        while ((1 << nMaxZoomLevelForThisTM) >
                   INT_MAX / poTS->nTileXCountZoomLevel0 ||
               (1 << nMaxZoomLevelForThisTM) >
                   INT_MAX / poTS->nTileYCountZoomLevel0)
        {
            --nMaxZoomLevelForThisTM;
        }

        if (nZoomLevel < 0 || nZoomLevel > nMaxZoomLevelForThisTM)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ZOOM_LEVEL = %s is invalid. It should be in [0,%d] range",
                     pszZoomLevel, nMaxZoomLevelForThisTM);
            CPLFree(pszWKT);
            CSLDestroy(papszTO);
            return nullptr;
        }
    }
    else
    {
        for (; nZoomLevel < MAX_ZOOM_LEVEL; nZoomLevel++)
        {
            dfRes = poTS->dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);
            if (dfComputedRes > dfRes ||
                fabs(dfComputedRes - dfRes) / dfRes <= 1e-8)
                break;
            dfPrevRes = dfRes;
        }
        if (nZoomLevel == MAX_ZOOM_LEVEL ||
            (1 << nZoomLevel) > INT_MAX / poTS->nTileXCountZoomLevel0 ||
            (1 << nZoomLevel) > INT_MAX / poTS->nTileYCountZoomLevel0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not find an appropriate zoom level");
            CPLFree(pszWKT);
            CSLDestroy(papszTO);
            return nullptr;
        }

        if (nZoomLevel > 0 && fabs(dfComputedRes - dfRes) / dfRes > 1e-8)
        {
            const char *pszZoomLevelStrategy = CSLFetchNameValueDef(
                papszOptions, "ZOOM_LEVEL_STRATEGY", "AUTO");
            if (EQUAL(pszZoomLevelStrategy, "LOWER"))
            {
                nZoomLevel--;
            }
            else if (EQUAL(pszZoomLevelStrategy, "UPPER"))
            {
                /* do nothing */
            }
            else
            {
                if (dfPrevRes / dfComputedRes < dfComputedRes / dfRes)
                    nZoomLevel--;
            }
        }
    }

    dfRes = poTS->dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);

    double dfMinX = adfExtent[0];
    double dfMinY = adfExtent[1];
    double dfMaxX = adfExtent[2];
    double dfMaxY = adfExtent[3];

    nXSize = static_cast<int>(0.5 + (dfMaxX - dfMinX) / dfRes);
    nYSize = static_cast<int>(0.5 + (dfMaxY - dfMinY) / dfRes);
    adfGeoTransform[1] = dfRes;
    adfGeoTransform[5] = -dfRes;

    const GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    int nTargetBands = nBands;
    /* For grey level or RGB, if there's reprojection involved, add an alpha */
    /* channel */
    if (eDT == GDT_Byte &&
        ((nBands == 1 &&
          poSrcDS->GetRasterBand(1)->GetColorTable() == nullptr) ||
         nBands == 3))
    {
        OGRSpatialReference oSrcSRS;
        oSrcSRS.SetFromUserInput(poSrcDS->GetProjectionRef());
        oSrcSRS.AutoIdentifyEPSG();
        if (oSrcSRS.GetAuthorityCode(nullptr) == nullptr ||
            atoi(oSrcSRS.GetAuthorityCode(nullptr)) != nEPSGCode)
        {
            nTargetBands++;
        }
    }

    GDALResampleAlg eResampleAlg = GRA_Bilinear;
    const char *pszResampling = CSLFetchNameValue(papszOptions, "RESAMPLING");
    if (pszResampling)
    {
        for (size_t iAlg = 0;
             iAlg < sizeof(asResamplingAlg) / sizeof(asResamplingAlg[0]);
             iAlg++)
        {
            if (EQUAL(pszResampling, asResamplingAlg[iAlg].pszName))
            {
                eResampleAlg = asResamplingAlg[iAlg].eResampleAlg;
                break;
            }
        }
    }

    if (nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
        eResampleAlg != GRA_NearestNeighbour && eResampleAlg != GRA_Mode)
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "Input dataset has a color table, which will likely lead to "
            "bad results when using a resampling method other than "
            "nearest neighbour or mode. Converting the dataset to 24/32 bit "
            "(e.g. with gdal_translate -expand rgb/rgba) is advised.");
    }

    GDALGeoPackageDataset *poDS = new GDALGeoPackageDataset();
    if (!(poDS->Create(pszFilename, nXSize, nYSize, nTargetBands, eDT,
                       apszUpdatedOptions)))
    {
        delete poDS;
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return nullptr;
    }

    // Assign nodata values before the SetGeoTransform call.
    // SetGeoTransform will trigger creation of the overview datasets for each
    // zoom level and at that point the nodata value needs to be known.
    int bHasNoData = FALSE;
    double dfNoDataValue =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoData);
    if (eDT != GDT_Byte && bHasNoData)
    {
        poDS->GetRasterBand(1)->SetNoDataValue(dfNoDataValue);
    }

    poDS->SetGeoTransform(adfGeoTransform);
    poDS->SetProjection(pszWKT);
    CPLFree(pszWKT);
    pszWKT = nullptr;
    if (nTargetBands == 1 && nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr)
    {
        poDS->GetRasterBand(1)->SetColorTable(
            poSrcDS->GetRasterBand(1)->GetColorTable());
    }

    hTransformArg = GDALCreateGenImgProjTransformer2(poSrcDS, poDS, papszTO);
    CSLDestroy(papszTO);
    if (hTransformArg == nullptr)
    {
        delete poDS;
        return nullptr;
    }

    poDS->SetMetadata(poSrcDS->GetMetadata());

    /* -------------------------------------------------------------------- */
    /*      Warp the transformer with a linear approximator                 */
    /* -------------------------------------------------------------------- */
    hTransformArg = GDALCreateApproxTransformer(GDALGenImgProjTransform,
                                                hTransformArg, 0.125);
    GDALApproxTransformerOwnsSubtransformer(hTransformArg, TRUE);

    /* -------------------------------------------------------------------- */
    /*      Setup warp options.                                             */
    /* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALCreateWarpOptions();

    psWO->papszWarpOptions = CSLSetNameValue(nullptr, "OPTIMIZE_SIZE", "YES");
    psWO->papszWarpOptions =
        CSLSetNameValue(psWO->papszWarpOptions, "SAMPLE_GRID", "YES");
    if (bHasNoData)
    {
        if (dfNoDataValue == 0.0)
        {
            // Do not initialize in the case where nodata != 0, since we
            // want the GeoPackage driver to return empty tiles at the nodata
            // value instead of 0 as GDAL core would
            psWO->papszWarpOptions =
                CSLSetNameValue(psWO->papszWarpOptions, "INIT_DEST", "0");
        }

        psWO->padfSrcNoDataReal =
            static_cast<double *>(CPLMalloc(sizeof(double)));
        psWO->padfSrcNoDataReal[0] = dfNoDataValue;

        psWO->padfDstNoDataReal =
            static_cast<double *>(CPLMalloc(sizeof(double)));
        psWO->padfDstNoDataReal[0] = dfNoDataValue;
    }
    psWO->eWorkingDataType = eDT;
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

    if (nBands == 2 || nBands == 4)
        psWO->nBandCount = nBands - 1;
    else
        psWO->nBandCount = nBands;

    psWO->panSrcBands =
        static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));
    psWO->panDstBands =
        static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));

    for (int i = 0; i < psWO->nBandCount; i++)
    {
        psWO->panSrcBands[i] = i + 1;
        psWO->panDstBands[i] = i + 1;
    }

    if (nBands == 2 || nBands == 4)
    {
        psWO->nSrcAlphaBand = nBands;
    }
    if (nTargetBands == 2 || nTargetBands == 4)
    {
        psWO->nDstAlphaBand = nTargetBands;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize and execute the warp.                                */
    /* -------------------------------------------------------------------- */
    GDALWarpOperation oWO;

    CPLErr eErr = oWO.Initialize(psWO);
    if (eErr == CE_None)
    {
        /*if( bMulti )
            eErr = oWO.ChunkAndWarpMulti( 0, 0, nXSize, nYSize );
        else*/
        eErr = oWO.ChunkAndWarpImage(0, 0, nXSize, nYSize);
    }
    if (eErr != CE_None)
    {
        delete poDS;
        poDS = nullptr;
    }

    GDALDestroyTransformer(hTransformArg);
    GDALDestroyWarpOptions(psWO);

    if (poDS)
        poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);

    return poDS;
}

/************************************************************************/
/*                        ParseCompressionOptions()                     */
/************************************************************************/

void GDALGeoPackageDataset::ParseCompressionOptions(char **papszOptions)
{
    const char *pszZLevel = CSLFetchNameValue(papszOptions, "ZLEVEL");
    if (pszZLevel)
        m_nZLevel = atoi(pszZLevel);

    const char *pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    if (pszQuality)
        m_nQuality = atoi(pszQuality);

    const char *pszDither = CSLFetchNameValue(papszOptions, "DITHER");
    if (pszDither)
        m_bDither = CPLTestBool(pszDither);
}

/************************************************************************/
/*                          RegisterWebPExtension()                     */
/************************************************************************/

bool GDALGeoPackageDataset::RegisterWebPExtension()
{
    if (CreateExtensionsTableIfNecessary() != OGRERR_NONE)
        return false;

    char *pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "('%q', 'tile_data', 'gpkg_webp', "
        "'http://www.geopackage.org/spec120/#extension_tiles_webp', "
        "'read-write')",
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
    if (CreateExtensionsTableIfNecessary() != OGRERR_NONE)
        return false;

    char *pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "('%q', 'tile_data', 'gpkg_zoom_other', "
        "'http://www.geopackage.org/spec120/#extension_zoom_other_intervals', "
        "'read-write')",
        m_osRasterTable.c_str());
    const OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    return OGRERR_NONE == eErr;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *GDALGeoPackageDataset::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= m_nLayers)
        return nullptr;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                           LaunderName()                              */
/************************************************************************/

/** Launder identifiers (table, column names) according to guidance at
 * https://www.geopackage.org/guidance/getting-started.html:
 * "For maximum interoperability, start your database identifiers (table names,
 * column names, etc.) with a lowercase character and only use lowercase
 * characters, numbers 0-9, and underscores (_)."
 */

/* static */
std::string GDALGeoPackageDataset::LaunderName(const std::string &osStr)
{
    char *pszASCII = CPLUTF8ForceToASCII(osStr.c_str(), '_');
    const std::string osStrASCII(pszASCII);
    CPLFree(pszASCII);

    std::string osRet;
    osRet.reserve(osStrASCII.size());

    for (size_t i = 0; i < osStrASCII.size(); ++i)
    {
        if (osRet.empty())
        {
            if (osStrASCII[i] >= 'A' && osStrASCII[i] <= 'Z')
            {
                osRet += (osStrASCII[i] - 'A' + 'a');
            }
            else if (osStrASCII[i] >= 'a' && osStrASCII[i] <= 'z')
            {
                osRet += osStrASCII[i];
            }
            else
            {
                continue;
            }
        }
        else if (osStrASCII[i] >= 'A' && osStrASCII[i] <= 'Z')
        {
            osRet += (osStrASCII[i] - 'A' + 'a');
        }
        else if ((osStrASCII[i] >= 'a' && osStrASCII[i] <= 'z') ||
                 (osStrASCII[i] >= '0' && osStrASCII[i] <= '9') ||
                 osStrASCII[i] == '_')
        {
            osRet += osStrASCII[i];
        }
        else
        {
            osRet += '_';
        }
    }

    if (osRet.empty() && !osStrASCII.empty())
        return LaunderName(std::string("x").append(osStrASCII));

    if (osRet != osStr)
    {
        CPLDebug("PG", "LaunderName('%s') -> '%s'", osStr.c_str(),
                 osRet.c_str());
    }

    return osRet;
}

/************************************************************************/
/*                          ICreateLayer()                              */
/************************************************************************/

OGRLayer *
GDALGeoPackageDataset::ICreateLayer(const char *pszLayerName,
                                    const OGRGeomFieldDefn *poSrcGeomFieldDefn,
                                    CSLConstList papszOptions)
{
    /* -------------------------------------------------------------------- */
    /*      Verify we are in update mode.                                   */
    /* -------------------------------------------------------------------- */
    if (!GetUpdate())
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "New layer %s cannot be created.\n",
                 m_pszFilename, pszLayerName);

        return nullptr;
    }

    const bool bLaunder =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "LAUNDER", "NO"));
    const std::string osTableName(bLaunder ? LaunderName(pszLayerName)
                                           : std::string(pszLayerName));

    const auto eGType =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetSpatialRef() : nullptr;

    if (!m_bHasGPKGGeometryColumns)
    {
        if (SQLCommand(hDB, pszCREATE_GPKG_GEOMETRY_COLUMNS) != OGRERR_NONE)
        {
            return nullptr;
        }
        m_bHasGPKGGeometryColumns = true;
    }

    // Check identifier unicity
    const char *pszIdentifier = CSLFetchNameValue(papszOptions, "IDENTIFIER");
    if (pszIdentifier != nullptr && pszIdentifier[0] == '\0')
        pszIdentifier = nullptr;
    if (pszIdentifier != nullptr)
    {
        for (int i = 0; i < m_nLayers; ++i)
        {
            const char *pszOtherIdentifier =
                m_papoLayers[i]->GetMetadataItem("IDENTIFIER");
            if (pszOtherIdentifier == nullptr)
                pszOtherIdentifier = m_papoLayers[i]->GetName();
            if (pszOtherIdentifier != nullptr &&
                EQUAL(pszOtherIdentifier, pszIdentifier) &&
                !EQUAL(m_papoLayers[i]->GetName(), osTableName.c_str()))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Identifier %s is already used by table %s",
                         pszIdentifier, m_papoLayers[i]->GetName());
                return nullptr;
            }
        }

        // In case there would be table in gpkg_contents not listed as a
        // vector layer
        char *pszSQL = sqlite3_mprintf(
            "SELECT table_name FROM gpkg_contents WHERE identifier = '%q' "
            "LIMIT 2",
            pszIdentifier);
        auto oResult = SQLQuery(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (oResult && oResult->RowCount() > 0 &&
            oResult->GetValue(0, 0) != nullptr &&
            !EQUAL(oResult->GetValue(0, 0), osTableName.c_str()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Identifier %s is already used by table %s", pszIdentifier,
                     oResult->GetValue(0, 0));
            return nullptr;
        }
    }

    /* Read GEOMETRY_NAME option */
    const char *pszGeomColumnName =
        CSLFetchNameValue(papszOptions, "GEOMETRY_NAME");
    if (pszGeomColumnName == nullptr) /* deprecated name */
        pszGeomColumnName = CSLFetchNameValue(papszOptions, "GEOMETRY_COLUMN");
    if (pszGeomColumnName == nullptr && poSrcGeomFieldDefn)
    {
        pszGeomColumnName = poSrcGeomFieldDefn->GetNameRef();
        if (pszGeomColumnName && pszGeomColumnName[0] == 0)
            pszGeomColumnName = nullptr;
    }
    if (pszGeomColumnName == nullptr)
        pszGeomColumnName = "geom";
    const bool bGeomNullable =
        CPLFetchBool(papszOptions, "GEOMETRY_NULLABLE", true);

    /* Read FID option */
    const char *pszFIDColumnName = CSLFetchNameValue(papszOptions, "FID");
    if (pszFIDColumnName == nullptr)
        pszFIDColumnName = "fid";

    if (CPLTestBool(CPLGetConfigOption("GPKG_NAME_CHECK", "YES")))
    {
        if (strspn(pszFIDColumnName, "`~!@#$%^&*()+-={}|[]\\:\";'<>?,./") > 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The primary key (%s) name may not contain special "
                     "characters or spaces",
                     pszFIDColumnName);
            return nullptr;
        }

        /* Avoiding gpkg prefixes is not an official requirement, but seems wise
         */
        if (STARTS_WITH(osTableName.c_str(), "gpkg"))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The layer name may not begin with 'gpkg' as it is a "
                     "reserved geopackage prefix");
            return nullptr;
        }

        /* Preemptively try and avoid sqlite3 syntax errors due to  */
        /* illegal characters. */
        if (strspn(osTableName.c_str(), "`~!@#$%^&*()+-={}|[]\\:\";'<>?,./") >
            0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "The layer name may not contain special characters or spaces");
            return nullptr;
        }
    }

    /* Check for any existing layers that already use this name */
    for (int iLayer = 0; iLayer < m_nLayers; iLayer++)
    {
        if (EQUAL(osTableName.c_str(), m_papoLayers[iLayer]->GetName()))
        {
            const char *pszOverwrite =
                CSLFetchNameValue(papszOptions, "OVERWRITE");
            if (pszOverwrite != nullptr && CPLTestBool(pszOverwrite))
            {
                DeleteLayer(iLayer);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer %s already exists, CreateLayer failed.\n"
                         "Use the layer creation option OVERWRITE=YES to "
                         "replace it.",
                         osTableName.c_str());
                return nullptr;
            }
        }
    }

    if (m_nLayers == 1)
    {
        // Async RTree building doesn't play well with multiple layer:
        // SQLite3 locks being hold for a long time, random failed commits,
        // etc.
        m_papoLayers[0]->FinishOrDisableThreadedRTree();
    }

    /* Create a blank layer. */
    auto poLayer = std::unique_ptr<OGRGeoPackageTableLayer>(
        new OGRGeoPackageTableLayer(this, osTableName.c_str()));

    OGRSpatialReference *poSRS = nullptr;
    if (poSpatialRef)
    {
        poSRS = poSpatialRef->Clone();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    poLayer->SetCreationParameters(
        eGType,
        bLaunder ? LaunderName(pszGeomColumnName).c_str() : pszGeomColumnName,
        bGeomNullable, poSRS, CSLFetchNameValue(papszOptions, "SRID"),
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetCoordinatePrecision()
                           : OGRGeomCoordinatePrecision(),
        CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "DISCARD_COORD_LSB", "NO")),
        CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "UNDO_DISCARD_COORD_LSB_ON_READING", "NO")),
        bLaunder ? LaunderName(pszFIDColumnName).c_str() : pszFIDColumnName,
        pszIdentifier, CSLFetchNameValue(papszOptions, "DESCRIPTION"));
    if (poSRS)
    {
        poSRS->Release();
    }

    poLayer->SetLaunder(bLaunder);

    /* Should we create a spatial index ? */
    const char *pszSI = CSLFetchNameValue(papszOptions, "SPATIAL_INDEX");
    int bCreateSpatialIndex = (pszSI == nullptr || CPLTestBool(pszSI));
    if (eGType != wkbNone && bCreateSpatialIndex)
    {
        poLayer->SetDeferredSpatialIndexCreation(true);
    }

    poLayer->SetPrecisionFlag(CPLFetchBool(papszOptions, "PRECISION", true));
    poLayer->SetTruncateFieldsFlag(
        CPLFetchBool(papszOptions, "TRUNCATE_FIELDS", false));
    if (eGType == wkbNone)
    {
        const char *pszASpatialVariant = CSLFetchNameValueDef(
            papszOptions, "ASPATIAL_VARIANT",
            m_bNonSpatialTablesNonRegisteredInGpkgContentsFound
                ? "NOT_REGISTERED"
                : "GPKG_ATTRIBUTES");
        GPKGASpatialVariant eASpatialVariant = GPKG_ATTRIBUTES;
        if (EQUAL(pszASpatialVariant, "GPKG_ATTRIBUTES"))
            eASpatialVariant = GPKG_ATTRIBUTES;
        else if (EQUAL(pszASpatialVariant, "OGR_ASPATIAL"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "ASPATIAL_VARIANT=OGR_ASPATIAL is no longer supported");
            return nullptr;
        }
        else if (EQUAL(pszASpatialVariant, "NOT_REGISTERED"))
            eASpatialVariant = NOT_REGISTERED;
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported value for ASPATIAL_VARIANT: %s",
                     pszASpatialVariant);
            return nullptr;
        }
        poLayer->SetASpatialVariant(eASpatialVariant);
    }

    const char *pszDateTimePrecision =
        CSLFetchNameValueDef(papszOptions, "DATETIME_PRECISION", "AUTO");
    if (EQUAL(pszDateTimePrecision, "MILLISECOND"))
    {
        poLayer->SetDateTimePrecision(OGRISO8601Precision::MILLISECOND);
    }
    else if (EQUAL(pszDateTimePrecision, "SECOND"))
    {
        if (m_nUserVersion < GPKG_1_4_VERSION)
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "DATETIME_PRECISION=SECOND is only valid since GeoPackage 1.4");
        poLayer->SetDateTimePrecision(OGRISO8601Precision::SECOND);
    }
    else if (EQUAL(pszDateTimePrecision, "MINUTE"))
    {
        if (m_nUserVersion < GPKG_1_4_VERSION)
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "DATETIME_PRECISION=MINUTE is only valid since GeoPackage 1.4");
        poLayer->SetDateTimePrecision(OGRISO8601Precision::MINUTE);
    }
    else if (EQUAL(pszDateTimePrecision, "AUTO"))
    {
        if (m_nUserVersion < GPKG_1_4_VERSION)
            poLayer->SetDateTimePrecision(OGRISO8601Precision::MILLISECOND);
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported value for DATETIME_PRECISION: %s",
                 pszDateTimePrecision);
        return nullptr;
    }

    // If there was an ogr_empty_table table, we can remove it
    // But do it at dataset closing, otherwise locking performance issues
    // can arise (probably when transactions are used).
    m_bRemoveOGREmptyTable = true;

    m_papoLayers = static_cast<OGRGeoPackageTableLayer **>(CPLRealloc(
        m_papoLayers, sizeof(OGRGeoPackageTableLayer *) * (m_nLayers + 1)));
    auto poRet = poLayer.release();
    m_papoLayers[m_nLayers] = poRet;
    m_nLayers++;
    return poRet;
}

/************************************************************************/
/*                          FindLayerIndex()                            */
/************************************************************************/

int GDALGeoPackageDataset::FindLayerIndex(const char *pszLayerName)

{
    for (int iLayer = 0; iLayer < m_nLayers; iLayer++)
    {
        if (EQUAL(pszLayerName, m_papoLayers[iLayer]->GetName()))
            return iLayer;
    }
    return -1;
}

/************************************************************************/
/*                       DeleteLayerCommon()                            */
/************************************************************************/

OGRErr GDALGeoPackageDataset::DeleteLayerCommon(const char *pszLayerName)
{
    // Temporary remove foreign key checks
    const GPKGTemporaryForeignKeyCheckDisabler
        oGPKGTemporaryForeignKeyCheckDisabler(this);

    char *pszSQL = sqlite3_mprintf(
        "DELETE FROM gpkg_contents WHERE lower(table_name) = lower('%q')",
        pszLayerName);
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);

    if (eErr == OGRERR_NONE && HasExtensionsTable())
    {
        pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_extensions WHERE lower(table_name) = lower('%q')",
            pszLayerName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if (eErr == OGRERR_NONE && HasMetadataTables())
    {
        // Delete from gpkg_metadata metadata records that are only referenced
        // by the table we are about to drop
        pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_metadata WHERE id IN ("
            "SELECT DISTINCT md_file_id FROM "
            "gpkg_metadata_reference WHERE "
            "lower(table_name) = lower('%q') AND md_parent_id is NULL) "
            "AND id NOT IN ("
            "SELECT DISTINCT md_file_id FROM gpkg_metadata_reference WHERE "
            "md_file_id IN (SELECT DISTINCT md_file_id FROM "
            "gpkg_metadata_reference WHERE "
            "lower(table_name) = lower('%q') AND md_parent_id is NULL) "
            "AND lower(table_name) <> lower('%q'))",
            pszLayerName, pszLayerName, pszLayerName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        if (eErr == OGRERR_NONE)
        {
            pszSQL =
                sqlite3_mprintf("DELETE FROM gpkg_metadata_reference WHERE "
                                "lower(table_name) = lower('%q')",
                                pszLayerName);
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
    }

    if (eErr == OGRERR_NONE && HasGpkgextRelationsTable())
    {
        // Remove reference to potential corresponding mapping table in
        // gpkg_extensions
        pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_extensions WHERE "
            "extension_name IN ('related_tables', "
            "'gpkg_related_tables') AND lower(table_name) = "
            "(SELECT lower(mapping_table_name) FROM gpkgext_relations WHERE "
            "lower(base_table_name) = lower('%q') OR "
            "lower(related_table_name) = lower('%q') OR "
            "lower(mapping_table_name) = lower('%q'))",
            pszLayerName, pszLayerName, pszLayerName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        if (eErr == OGRERR_NONE)
        {
            // Remove reference to potential corresponding mapping table in
            // gpkgext_relations
            pszSQL =
                sqlite3_mprintf("DELETE FROM gpkgext_relations WHERE "
                                "lower(base_table_name) = lower('%q') OR "
                                "lower(related_table_name) = lower('%q') OR "
                                "lower(mapping_table_name) = lower('%q')",
                                pszLayerName, pszLayerName, pszLayerName);
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }

        if (eErr == OGRERR_NONE && HasExtensionsTable())
        {
            // If there is no longer any mapping table, then completely
            // remove any reference to the extension in gpkg_extensions
            // as mandated per the related table specification.
            OGRErr err;
            if (SQLGetInteger(hDB,
                              "SELECT COUNT(*) FROM gpkg_extensions WHERE "
                              "extension_name IN ('related_tables', "
                              "'gpkg_related_tables') AND "
                              "lower(table_name) != 'gpkgext_relations'",
                              &err) == 0)
            {
                eErr = SQLCommand(hDB, "DELETE FROM gpkg_extensions WHERE "
                                       "extension_name IN ('related_tables', "
                                       "'gpkg_related_tables')");
            }

            ClearCachedRelationships();
        }
    }

    if (eErr == OGRERR_NONE)
    {
        pszSQL = sqlite3_mprintf("DROP TABLE \"%w\"", pszLayerName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    // Check foreign key integrity
    if (eErr == OGRERR_NONE)
    {
        eErr = PragmaCheck("foreign_key_check", "", 0);
    }

    return eErr;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr GDALGeoPackageDataset::DeleteLayer(int iLayer)
{
    if (!GetUpdate() || iLayer < 0 || iLayer >= m_nLayers)
        return OGRERR_FAILURE;

    m_papoLayers[iLayer]->ResetReading();
    m_papoLayers[iLayer]->SyncToDisk();

    CPLString osLayerName = m_papoLayers[iLayer]->GetName();

    CPLDebug("GPKG", "DeleteLayer(%s)", osLayerName.c_str());

    // Temporary remove foreign key checks
    const GPKGTemporaryForeignKeyCheckDisabler
        oGPKGTemporaryForeignKeyCheckDisabler(this);

    OGRErr eErr = SoftStartTransaction();

    if (eErr == OGRERR_NONE)
    {
        if (m_papoLayers[iLayer]->HasSpatialIndex())
            m_papoLayers[iLayer]->DropSpatialIndex();

        char *pszSQL =
            sqlite3_mprintf("DELETE FROM gpkg_geometry_columns WHERE "
                            "lower(table_name) = lower('%q')",
                            osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if (eErr == OGRERR_NONE && HasDataColumnsTable())
    {
        char *pszSQL = sqlite3_mprintf("DELETE FROM gpkg_data_columns WHERE "
                                       "lower(table_name) = lower('%q')",
                                       osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (eErr == OGRERR_NONE && m_bHasGPKGOGRContents)
    {
        char *pszSQL = sqlite3_mprintf("DELETE FROM gpkg_ogr_contents WHERE "
                                       "lower(table_name) = lower('%q')",
                                       osLayerName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }
#endif

    if (eErr == OGRERR_NONE)
    {
        eErr = DeleteLayerCommon(osLayerName.c_str());
    }

    if (eErr == OGRERR_NONE)
    {
        eErr = SoftCommitTransaction();
        if (eErr == OGRERR_NONE)
        {
            /* Delete the layer object and remove the gap in the layers list */
            delete m_papoLayers[iLayer];
            memmove(m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
                    sizeof(void *) * (m_nLayers - iLayer - 1));
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
/*                       DeleteRasterLayer()                            */
/************************************************************************/

OGRErr GDALGeoPackageDataset::DeleteRasterLayer(const char *pszLayerName)
{
    // Temporary remove foreign key checks
    const GPKGTemporaryForeignKeyCheckDisabler
        oGPKGTemporaryForeignKeyCheckDisabler(this);

    OGRErr eErr = SoftStartTransaction();

    if (eErr == OGRERR_NONE)
    {
        char *pszSQL = sqlite3_mprintf("DELETE FROM gpkg_tile_matrix WHERE "
                                       "lower(table_name) = lower('%q')",
                                       pszLayerName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if (eErr == OGRERR_NONE)
    {
        char *pszSQL = sqlite3_mprintf("DELETE FROM gpkg_tile_matrix_set WHERE "
                                       "lower(table_name) = lower('%q')",
                                       pszLayerName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    if (eErr == OGRERR_NONE && HasGriddedCoverageAncillaryTable())
    {
        char *pszSQL =
            sqlite3_mprintf("DELETE FROM gpkg_2d_gridded_coverage_ancillary "
                            "WHERE lower(tile_matrix_set_name) = lower('%q')",
                            pszLayerName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        if (eErr == OGRERR_NONE)
        {
            pszSQL =
                sqlite3_mprintf("DELETE FROM gpkg_2d_gridded_tile_ancillary "
                                "WHERE lower(tpudt_name) = lower('%q')",
                                pszLayerName);
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
    }

    if (eErr == OGRERR_NONE)
    {
        eErr = DeleteLayerCommon(pszLayerName);
    }

    if (eErr == OGRERR_NONE)
    {
        eErr = SoftCommitTransaction();
    }
    else
    {
        SoftRollbackTransaction();
    }

    return eErr;
}

/************************************************************************/
/*                    DeleteVectorOrRasterLayer()                       */
/************************************************************************/

bool GDALGeoPackageDataset::DeleteVectorOrRasterLayer(const char *pszLayerName)
{

    int idx = FindLayerIndex(pszLayerName);
    if (idx >= 0)
    {
        DeleteLayer(idx);
        return true;
    }

    char *pszSQL =
        sqlite3_mprintf("SELECT 1 FROM gpkg_contents WHERE "
                        "lower(table_name) = lower('%q') "
                        "AND data_type IN ('tiles', '2d-gridded-coverage')",
                        pszLayerName);
    bool bIsRasterTable = SQLGetInteger(hDB, pszSQL, nullptr) == 1;
    sqlite3_free(pszSQL);
    if (bIsRasterTable)
    {
        DeleteRasterLayer(pszLayerName);
        return true;
    }
    return false;
}

/************************************************************************/
/*                       TestCapability()                               */
/************************************************************************/

int GDALGeoPackageDataset::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer) || EQUAL(pszCap, ODsCDeleteLayer) ||
        EQUAL(pszCap, "RenameLayer"))
    {
        return GetUpdate();
    }
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite) ||
             EQUAL(pszCap, GDsCAddRelationship) ||
             EQUAL(pszCap, GDsCDeleteRelationship) ||
             EQUAL(pszCap, GDsCUpdateRelationship) ||
             EQUAL(pszCap, ODsCAddFieldDomain))
        return GetUpdate();

    return OGRSQLiteBaseDataSource::TestCapability(pszCap);
}

/************************************************************************/
/*                       ResetReadingAllLayers()                        */
/************************************************************************/

void GDALGeoPackageDataset::ResetReadingAllLayers()
{
    for (int i = 0; i < m_nLayers; i++)
    {
        m_papoLayers[i]->ResetReading();
    }
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

static const char *const apszFuncsWithSideEffects[] = {
    "CreateSpatialIndex",
    "DisableSpatialIndex",
    "HasSpatialIndex",
    "RegisterGeometryExtension",
};

OGRLayer *GDALGeoPackageDataset::ExecuteSQL(const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect)

{
    m_bHasReadMetadataFromStorage = false;

    FlushMetadata();

    while (*pszSQLCommand != '\0' &&
           isspace(static_cast<unsigned char>(*pszSQLCommand)))
        pszSQLCommand++;

    CPLString osSQLCommand(pszSQLCommand);
    if (!osSQLCommand.empty() && osSQLCommand.back() == ';')
        osSQLCommand.resize(osSQLCommand.size() - 1);

    if (pszDialect == nullptr || !EQUAL(pszDialect, "DEBUG"))
    {
        // Some SQL commands will influence the feature count behind our
        // back, so disable it in that case.
#ifdef ENABLE_GPKG_OGR_CONTENTS
        const bool bInsertOrDelete =
            osSQLCommand.ifind("insert into ") != std::string::npos ||
            osSQLCommand.ifind("insert or replace into ") !=
                std::string::npos ||
            osSQLCommand.ifind("delete from ") != std::string::npos;
        const bool bRollback =
            osSQLCommand.ifind("rollback ") != std::string::npos;
#endif

        for (int i = 0; i < m_nLayers; i++)
        {
            if (m_papoLayers[i]->SyncToDisk() != OGRERR_NONE)
                return nullptr;
#ifdef ENABLE_GPKG_OGR_CONTENTS
            if (bRollback || (bInsertOrDelete &&
                              osSQLCommand.ifind(m_papoLayers[i]->GetName()) !=
                                  std::string::npos))
            {
                m_papoLayers[i]->DisableFeatureCount();
            }
#endif
        }
    }

    if (EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like = 0") ||
        EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like=0") ||
        EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like =0") ||
        EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like= 0"))
    {
        OGRSQLiteSQLFunctionsSetCaseSensitiveLike(m_pSQLFunctionData, false);
    }
    else if (EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like = 1") ||
             EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like=1") ||
             EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like =1") ||
             EQUAL(pszSQLCommand, "PRAGMA case_sensitive_like= 1"))
    {
        OGRSQLiteSQLFunctionsSetCaseSensitiveLike(m_pSQLFunctionData, true);
    }

    /* -------------------------------------------------------------------- */
    /*      DEBUG "SELECT nolock" command.                                  */
    /* -------------------------------------------------------------------- */
    if (pszDialect != nullptr && EQUAL(pszDialect, "DEBUG") &&
        EQUAL(osSQLCommand, "SELECT nolock"))
    {
        return new OGRSQLiteSingleFeatureLayer(osSQLCommand, m_bNoLock ? 1 : 0);
    }

    /* -------------------------------------------------------------------- */
    /*      Special case DELLAYER: command.                                 */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(osSQLCommand, "DELLAYER:"))
    {
        const char *pszLayerName = osSQLCommand.c_str() + strlen("DELLAYER:");

        while (*pszLayerName == ' ')
            pszLayerName++;

        if (!DeleteVectorOrRasterLayer(pszLayerName))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer: %s",
                     pszLayerName);
        }
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case RECOMPUTE EXTENT ON command.                       */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(osSQLCommand, "RECOMPUTE EXTENT ON "))
    {
        const char *pszLayerName =
            osSQLCommand.c_str() + strlen("RECOMPUTE EXTENT ON ");

        while (*pszLayerName == ' ')
            pszLayerName++;

        int idx = FindLayerIndex(pszLayerName);
        if (idx >= 0)
        {
            m_papoLayers[idx]->RecomputeExtent();
        }
        else
            CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer: %s",
                     pszLayerName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Intercept DROP TABLE                                            */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(osSQLCommand, "DROP TABLE "))
    {
        const char *pszLayerName = osSQLCommand.c_str() + strlen("DROP TABLE ");

        while (*pszLayerName == ' ')
            pszLayerName++;

        if (DeleteVectorOrRasterLayer(SQLUnescape(pszLayerName)))
            return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Intercept ALTER TABLE src_table RENAME TO dst_table             */
    /*      and       ALTER TABLE table RENAME COLUMN src_name TO dst_name  */
    /*      and       ALTER TABLE table DROP COLUMN col_name                */
    /*                                                                      */
    /*      We do this because SQLite mechanisms can't deal with updating   */
    /*      literal values in gpkg_ tables that refer to table and column   */
    /*      names.                                                          */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(osSQLCommand, "ALTER TABLE "))
    {
        char **papszTokens = SQLTokenize(osSQLCommand);
        /* ALTER TABLE src_table RENAME TO dst_table */
        if (CSLCount(papszTokens) == 6 && EQUAL(papszTokens[3], "RENAME") &&
            EQUAL(papszTokens[4], "TO"))
        {
            const char *pszSrcTableName = papszTokens[2];
            const char *pszDstTableName = papszTokens[5];
            OGRGeoPackageTableLayer *poSrcLayer =
                dynamic_cast<OGRGeoPackageTableLayer *>(
                    GetLayerByName(SQLUnescape(pszSrcTableName)));
            if (poSrcLayer)
            {
                poSrcLayer->Rename(SQLUnescape(pszDstTableName));
                CSLDestroy(papszTokens);
                return nullptr;
            }
        }
        /* ALTER TABLE table RENAME COLUMN src_name TO dst_name */
        else if (CSLCount(papszTokens) == 8 &&
                 EQUAL(papszTokens[3], "RENAME") &&
                 EQUAL(papszTokens[4], "COLUMN") && EQUAL(papszTokens[6], "TO"))
        {
            const char *pszTableName = papszTokens[2];
            const char *pszSrcColumn = papszTokens[5];
            const char *pszDstColumn = papszTokens[7];
            OGRGeoPackageTableLayer *poLayer =
                dynamic_cast<OGRGeoPackageTableLayer *>(
                    GetLayerByName(SQLUnescape(pszTableName)));
            if (poLayer)
            {
                int nSrcFieldIdx = poLayer->GetLayerDefn()->GetFieldIndex(
                    SQLUnescape(pszSrcColumn));
                if (nSrcFieldIdx >= 0)
                {
                    // OFTString or any type will do as we just alter the name
                    // so it will be ignored.
                    OGRFieldDefn oFieldDefn(SQLUnescape(pszDstColumn),
                                            OFTString);
                    poLayer->AlterFieldDefn(nSrcFieldIdx, &oFieldDefn,
                                            ALTER_NAME_FLAG);
                    CSLDestroy(papszTokens);
                    return nullptr;
                }
            }
        }
        /* ALTER TABLE table DROP COLUMN col_name */
        else if (CSLCount(papszTokens) == 6 && EQUAL(papszTokens[3], "DROP") &&
                 EQUAL(papszTokens[4], "COLUMN"))
        {
            const char *pszTableName = papszTokens[2];
            const char *pszColumnName = papszTokens[5];
            OGRGeoPackageTableLayer *poLayer =
                dynamic_cast<OGRGeoPackageTableLayer *>(
                    GetLayerByName(SQLUnescape(pszTableName)));
            if (poLayer)
            {
                int nFieldIdx = poLayer->GetLayerDefn()->GetFieldIndex(
                    SQLUnescape(pszColumnName));
                if (nFieldIdx >= 0)
                {
                    poLayer->DeleteField(nFieldIdx);
                    CSLDestroy(papszTokens);
                    return nullptr;
                }
            }
        }
        CSLDestroy(papszTokens);
    }

    if (EQUAL(osSQLCommand, "VACUUM"))
    {
        ResetReadingAllLayers();
    }

    if (EQUAL(osSQLCommand, "BEGIN"))
    {
        SoftStartTransaction();
        return nullptr;
    }
    else if (EQUAL(osSQLCommand, "COMMIT"))
    {
        SoftCommitTransaction();
        return nullptr;
    }
    else if (EQUAL(osSQLCommand, "ROLLBACK"))
    {
        SoftRollbackTransaction();
        return nullptr;
    }

    else if (pszDialect != nullptr && EQUAL(pszDialect, "INDIRECT_SQLITE"))
        return GDALDataset::ExecuteSQL(osSQLCommand, poSpatialFilter, "SQLITE");
    else if (pszDialect != nullptr && !EQUAL(pszDialect, "") &&
             !EQUAL(pszDialect, "NATIVE") && !EQUAL(pszDialect, "SQLITE") &&
             !EQUAL(pszDialect, "DEBUG"))
        return GDALDataset::ExecuteSQL(osSQLCommand, poSpatialFilter,
                                       pszDialect);

    /* -------------------------------------------------------------------- */
    /*      Prepare statement.                                              */
    /* -------------------------------------------------------------------- */
    sqlite3_stmt *hSQLStmt = nullptr;

    /* This will speed-up layer creation */
    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer definition. */
    bool bUseStatementForGetNextFeature = true;
    bool bEmptyLayer = false;
    CPLString osSQLCommandTruncated(osSQLCommand);

    if (osSQLCommand.ifind("SELECT ") == 0 &&
        CPLString(osSQLCommand.substr(1)).ifind("SELECT ") ==
            std::string::npos &&
        osSQLCommand.ifind(" UNION ") == std::string::npos &&
        osSQLCommand.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCommand.ifind(" EXCEPT ") == std::string::npos)
    {
        size_t nOrderByPos = osSQLCommand.ifind(" ORDER BY ");
        if (nOrderByPos != std::string::npos)
        {
            osSQLCommandTruncated.resize(nOrderByPos);
            bUseStatementForGetNextFeature = false;
        }
    }

    int rc = prepareSql(hDB, osSQLCommandTruncated.c_str(),
                        static_cast<int>(osSQLCommandTruncated.size()),
                        &hSQLStmt, nullptr);

    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In ExecuteSQL(): sqlite3_prepare_v2(%s):\n  %s",
                 osSQLCommandTruncated.c_str(), sqlite3_errmsg(hDB));

        if (hSQLStmt != nullptr)
        {
            sqlite3_finalize(hSQLStmt);
        }

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we get a resultset?                                          */
    /* -------------------------------------------------------------------- */
    rc = sqlite3_step(hSQLStmt);

    for (int i = 0; i < m_nLayers; i++)
    {
        m_papoLayers[i]->RunDeferredDropRTreeTableIfNecessary();
    }

    if (rc != SQLITE_ROW)
    {
        if (rc != SQLITE_DONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "In ExecuteSQL(): sqlite3_step(%s):\n  %s",
                     osSQLCommandTruncated.c_str(), sqlite3_errmsg(hDB));

            sqlite3_finalize(hSQLStmt);
            return nullptr;
        }

        if (EQUAL(osSQLCommand, "VACUUM"))
        {
            sqlite3_finalize(hSQLStmt);
            /* VACUUM rewrites the DB, so we need to reset the application id */
            SetApplicationAndUserVersionId();
            return nullptr;
        }

        if (!STARTS_WITH_CI(osSQLCommand, "SELECT "))
        {
            sqlite3_finalize(hSQLStmt);
            return nullptr;
        }

        bUseStatementForGetNextFeature = false;
        bEmptyLayer = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for some functions which must be run               */
    /*      only once                                                       */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(osSQLCommand, "SELECT "))
    {
        for (unsigned int i = 0; i < sizeof(apszFuncsWithSideEffects) /
                                         sizeof(apszFuncsWithSideEffects[0]);
             i++)
        {
            if (EQUALN(apszFuncsWithSideEffects[i], osSQLCommand.c_str() + 7,
                       strlen(apszFuncsWithSideEffects[i])))
            {
                if (sqlite3_column_count(hSQLStmt) == 1 &&
                    sqlite3_column_type(hSQLStmt, 0) == SQLITE_INTEGER)
                {
                    int ret = sqlite3_column_int(hSQLStmt, 0);

                    sqlite3_finalize(hSQLStmt);

                    return new OGRSQLiteSingleFeatureLayer(
                        apszFuncsWithSideEffects[i], ret);
                }
            }
        }
    }
    else if (STARTS_WITH_CI(osSQLCommand, "PRAGMA "))
    {
        if (sqlite3_column_count(hSQLStmt) == 1 &&
            sqlite3_column_type(hSQLStmt, 0) == SQLITE_INTEGER)
        {
            int ret = sqlite3_column_int(hSQLStmt, 0);

            sqlite3_finalize(hSQLStmt);

            return new OGRSQLiteSingleFeatureLayer(osSQLCommand.c_str() + 7,
                                                   ret);
        }
        else if (sqlite3_column_count(hSQLStmt) == 1 &&
                 sqlite3_column_type(hSQLStmt, 0) == SQLITE_TEXT)
        {
            const char *pszRet = reinterpret_cast<const char *>(
                sqlite3_column_text(hSQLStmt, 0));

            OGRLayer *poRet = new OGRSQLiteSingleFeatureLayer(
                osSQLCommand.c_str() + 7, pszRet);

            sqlite3_finalize(hSQLStmt);

            return poRet;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create layer.                                                   */
    /* -------------------------------------------------------------------- */

    OGRLayer *poLayer = new OGRGeoPackageSelectLayer(
        this, osSQLCommand, hSQLStmt, bUseStatementForGetNextFeature,
        bEmptyLayer);

    if (poSpatialFilter != nullptr &&
        poLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
        poLayer->SetSpatialFilter(0, poSpatialFilter);

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void GDALGeoPackageDataset::ReleaseResultSet(OGRLayer *poLayer)

{
    delete poLayer;
}

/************************************************************************/
/*                         HasExtensionsTable()                         */
/************************************************************************/

bool GDALGeoPackageDataset::HasExtensionsTable()
{
    return SQLGetInteger(
               hDB,
               "SELECT 1 FROM sqlite_master WHERE name = 'gpkg_extensions' "
               "AND type IN ('table', 'view')",
               nullptr) == 1;
}

/************************************************************************/
/*                    CheckUnknownExtensions()                          */
/************************************************************************/

void GDALGeoPackageDataset::CheckUnknownExtensions(bool bCheckRasterTable)
{
    if (!HasExtensionsTable())
        return;

    char *pszSQL = nullptr;
    if (!bCheckRasterTable)
        pszSQL = sqlite3_mprintf(
            "SELECT extension_name, definition, scope FROM gpkg_extensions "
            "WHERE (table_name IS NULL "
            "AND extension_name IS NOT NULL "
            "AND definition IS NOT NULL "
            "AND scope IS NOT NULL "
            "AND extension_name NOT IN ("
            "'gdal_aspatial', "
            "'gpkg_elevation_tiles', "  // Old name before GPKG 1.2 approval
            "'2d_gridded_coverage', "  // Old name after GPKG 1.2 and before OGC
                                       // 17-066r1 finalization
            "'gpkg_2d_gridded_coverage', "  // Name in OGC 17-066r1 final
            "'gpkg_metadata', "
            "'gpkg_schema', "
            "'gpkg_crs_wkt', "
            "'gpkg_crs_wkt_1_1', "
            "'related_tables', 'gpkg_related_tables')) "
#ifdef WORKAROUND_SQLITE3_BUGS
            "OR 0 "
#endif
            "LIMIT 1000");
    else
        pszSQL = sqlite3_mprintf(
            "SELECT extension_name, definition, scope FROM gpkg_extensions "
            "WHERE (lower(table_name) = lower('%q') "
            "AND extension_name IS NOT NULL "
            "AND definition IS NOT NULL "
            "AND scope IS NOT NULL "
            "AND extension_name NOT IN ("
            "'gpkg_elevation_tiles', "  // Old name before GPKG 1.2 approval
            "'2d_gridded_coverage', "  // Old name after GPKG 1.2 and before OGC
                                       // 17-066r1 finalization
            "'gpkg_2d_gridded_coverage', "  // Name in OGC 17-066r1 final
            "'gpkg_metadata', "
            "'gpkg_schema', "
            "'gpkg_crs_wkt', "
            "'gpkg_crs_wkt_1_1', "
            "'related_tables', 'gpkg_related_tables')) "
#ifdef WORKAROUND_SQLITE3_BUGS
            "OR 0 "
#endif
            "LIMIT 1000",
            m_osRasterTable.c_str());

    auto oResultTable = SQLQuery(GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if (oResultTable && oResultTable->RowCount() > 0)
    {
        for (int i = 0; i < oResultTable->RowCount(); i++)
        {
            const char *pszExtName = oResultTable->GetValue(0, i);
            const char *pszDefinition = oResultTable->GetValue(1, i);
            const char *pszScope = oResultTable->GetValue(2, i);
            if (pszExtName == nullptr || pszDefinition == nullptr ||
                pszScope == nullptr)
            {
                continue;
            }

            if (EQUAL(pszExtName, "gpkg_webp"))
            {
                if (GDALGetDriverByName("WEBP") == nullptr)
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Table %s contains WEBP tiles, but GDAL configured "
                        "without WEBP support. Data will be missing",
                        m_osRasterTable.c_str());
                }
                m_eTF = GPKG_TF_WEBP;
                continue;
            }
            if (EQUAL(pszExtName, "gpkg_zoom_other"))
            {
                m_bZoomOther = true;
                continue;
            }

            if (GetUpdate() && EQUAL(pszScope, "write-only"))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Database relies on the '%s' (%s) extension that should "
                    "be implemented for safe write-support, but is not "
                    "currently. "
                    "Update of that database are strongly discouraged to avoid "
                    "corruption.",
                    pszExtName, pszDefinition);
            }
            else if (GetUpdate() && EQUAL(pszScope, "read-write"))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Database relies on the '%s' (%s) extension that should "
                    "be implemented in order to read/write it safely, but is "
                    "not currently. "
                    "Some data may be missing while reading that database, and "
                    "updates are strongly discouraged.",
                    pszExtName, pszDefinition);
            }
            else if (EQUAL(pszScope, "read-write") &&
                     // None of the NGA extensions at
                     // http://ngageoint.github.io/GeoPackage/docs/extensions/
                     // affect read-only scenarios
                     !STARTS_WITH(pszExtName, "nga_"))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Database relies on the '%s' (%s) extension that should "
                    "be implemented in order to read it safely, but is not "
                    "currently. "
                    "Some data may be missing while reading that database.",
                    pszExtName, pszDefinition);
            }
        }
    }
}

/************************************************************************/
/*                         HasGDALAspatialExtension()                       */
/************************************************************************/

bool GDALGeoPackageDataset::HasGDALAspatialExtension()
{
    if (!HasExtensionsTable())
        return false;

    auto oResultTable = SQLQuery(hDB, "SELECT * FROM gpkg_extensions "
                                      "WHERE (extension_name = 'gdal_aspatial' "
                                      "AND table_name IS NULL "
                                      "AND column_name IS NULL)"
#ifdef WORKAROUND_SQLITE3_BUGS
                                      " OR 0"
#endif
    );
    bool bHasExtension = (oResultTable && oResultTable->RowCount() == 1);
    return bHasExtension;
}

/************************************************************************/
/*                  CreateExtensionsTableIfNecessary()                  */
/************************************************************************/

OGRErr GDALGeoPackageDataset::CreateExtensionsTableIfNecessary()
{
    /* Check if the table gpkg_extensions exists */
    if (HasExtensionsTable())
        return OGRERR_NONE;

    /* Requirement 79 : Every extension of a GeoPackage SHALL be registered */
    /* in a corresponding row in the gpkg_extensions table. The absence of a */
    /* gpkg_extensions table or the absence of rows in gpkg_extensions table */
    /* SHALL both indicate the absence of extensions to a GeoPackage. */
    const char *pszCreateGpkgExtensions =
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
/*                    OGR_GPKG_Intersects_Spatial_Filter()              */
/************************************************************************/

void OGR_GPKG_Intersects_Spatial_Filter(sqlite3_context *pContext, int argc,
                                        sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_int(pContext, 0);
        return;
    }

    auto poLayer =
        static_cast<OGRGeoPackageTableLayer *>(sqlite3_user_data(pContext));

    const int nBLOBLen = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));

    GPkgHeader sHeader;
    if (poLayer->m_bFilterIsEnvelope &&
        OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false, false, 0))
    {
        if (sHeader.bExtentHasXY)
        {
            OGREnvelope sEnvelope;
            sEnvelope.MinX = sHeader.MinX;
            sEnvelope.MinY = sHeader.MinY;
            sEnvelope.MaxX = sHeader.MaxX;
            sEnvelope.MaxY = sHeader.MaxY;
            if (poLayer->m_sFilterEnvelope.Contains(sEnvelope))
            {
                sqlite3_result_int(pContext, 1);
                return;
            }
        }

        // Check if at least one point falls into the layer filter envelope
        // nHeaderLen is > 0 for GeoPackage geometries
        if (sHeader.nHeaderLen > 0 &&
            OGRWKBIntersectsPessimistic(pabyBLOB + sHeader.nHeaderLen,
                                        nBLOBLen - sHeader.nHeaderLen,
                                        poLayer->m_sFilterEnvelope))
        {
            sqlite3_result_int(pContext, 1);
            return;
        }
    }

    auto poGeom = std::unique_ptr<OGRGeometry>(
        GPkgGeometryToOGR(pabyBLOB, nBLOBLen, nullptr));
    if (poGeom == nullptr)
    {
        // Try also spatialite geometry blobs
        OGRGeometry *poGeomSpatialite = nullptr;
        if (OGRSQLiteImportSpatiaLiteGeometry(pabyBLOB, nBLOBLen,
                                              &poGeomSpatialite) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid geometry");
            sqlite3_result_int(pContext, 0);
            return;
        }
        poGeom.reset(poGeomSpatialite);
    }

    sqlite3_result_int(pContext, poLayer->FilterGeometry(poGeom.get()));
}

/************************************************************************/
/*                      OGRGeoPackageSTMinX()                           */
/************************************************************************/

static void OGRGeoPackageSTMinX(sqlite3_context *pContext, int argc,
                                sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true, false))
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_double(pContext, sHeader.MinX);
}

/************************************************************************/
/*                      OGRGeoPackageSTMinY()                           */
/************************************************************************/

static void OGRGeoPackageSTMinY(sqlite3_context *pContext, int argc,
                                sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true, false))
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_double(pContext, sHeader.MinY);
}

/************************************************************************/
/*                      OGRGeoPackageSTMaxX()                           */
/************************************************************************/

static void OGRGeoPackageSTMaxX(sqlite3_context *pContext, int argc,
                                sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true, false))
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_double(pContext, sHeader.MaxX);
}

/************************************************************************/
/*                      OGRGeoPackageSTMaxY()                           */
/************************************************************************/

static void OGRGeoPackageSTMaxY(sqlite3_context *pContext, int argc,
                                sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true, false))
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_double(pContext, sHeader.MaxY);
}

/************************************************************************/
/*                     OGRGeoPackageSTIsEmpty()                         */
/************************************************************************/

static void OGRGeoPackageSTIsEmpty(sqlite3_context *pContext, int argc,
                                   sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false, false))
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_int(pContext, sHeader.bEmpty);
}

/************************************************************************/
/*                    OGRGeoPackageSTGeometryType()                     */
/************************************************************************/

static void OGRGeoPackageSTGeometryType(sqlite3_context *pContext, int /*argc*/,
                                        sqlite3_value **argv)
{
    GPkgHeader sHeader;

    int nBLOBLen = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));
    OGRwkbGeometryType eGeometryType;

    if (nBLOBLen < 8 ||
        GPkgHeaderFromWKB(pabyBLOB, nBLOBLen, &sHeader) != OGRERR_NONE)
    {
        if (OGRSQLiteGetSpatialiteGeometryHeader(
                pabyBLOB, nBLOBLen, nullptr, &eGeometryType, nullptr, nullptr,
                nullptr, nullptr, nullptr) == OGRERR_NONE)
        {
            sqlite3_result_text(pContext, OGRToOGCGeomType(eGeometryType), -1,
                                SQLITE_TRANSIENT);
            return;
        }
        else
        {
            sqlite3_result_null(pContext);
            return;
        }
    }

    if (static_cast<size_t>(nBLOBLen) < sHeader.nHeaderLen + 5)
    {
        sqlite3_result_null(pContext);
        return;
    }

    OGRErr err = OGRReadWKBGeometryType(pabyBLOB + sHeader.nHeaderLen,
                                        wkbVariantIso, &eGeometryType);
    if (err != OGRERR_NONE)
        sqlite3_result_null(pContext);
    else
        sqlite3_result_text(pContext, OGRToOGCGeomType(eGeometryType), -1,
                            SQLITE_TRANSIENT);
}

/************************************************************************/
/*                 OGRGeoPackageSTEnvelopesIntersects()                 */
/************************************************************************/

static void OGRGeoPackageSTEnvelopesIntersects(sqlite3_context *pContext,
                                               int argc, sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true, false))
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    const double dfMinX = sqlite3_value_double(argv[1]);
    if (sHeader.MaxX < dfMinX)
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    const double dfMinY = sqlite3_value_double(argv[2]);
    if (sHeader.MaxY < dfMinY)
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    const double dfMaxX = sqlite3_value_double(argv[3]);
    if (sHeader.MinX > dfMaxX)
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    const double dfMaxY = sqlite3_value_double(argv[4]);
    sqlite3_result_int(pContext, sHeader.MinY <= dfMaxY);
}

/************************************************************************/
/*              OGRGeoPackageSTEnvelopesIntersectsTwoParams()           */
/************************************************************************/

static void
OGRGeoPackageSTEnvelopesIntersectsTwoParams(sqlite3_context *pContext, int argc,
                                            sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, true, false, 0))
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    GPkgHeader sHeader2;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader2, true, false,
                                1))
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    if (sHeader.MaxX < sHeader2.MinX)
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    if (sHeader.MaxY < sHeader2.MinY)
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    if (sHeader.MinX > sHeader2.MaxX)
    {
        sqlite3_result_int(pContext, FALSE);
        return;
    }
    sqlite3_result_int(pContext, sHeader.MinY <= sHeader2.MaxY);
}

/************************************************************************/
/*                    OGRGeoPackageGPKGIsAssignable()                   */
/************************************************************************/

static void OGRGeoPackageGPKGIsAssignable(sqlite3_context *pContext,
                                          int /*argc*/, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_TEXT)
    {
        sqlite3_result_int(pContext, 0);
        return;
    }

    const char *pszExpected =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const char *pszActual =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    int bIsAssignable = OGR_GT_IsSubClassOf(OGRFromOGCGeomType(pszActual),
                                            OGRFromOGCGeomType(pszExpected));
    sqlite3_result_int(pContext, bIsAssignable);
}

/************************************************************************/
/*                     OGRGeoPackageSTSRID()                            */
/************************************************************************/

static void OGRGeoPackageSTSRID(sqlite3_context *pContext, int argc,
                                sqlite3_value **argv)
{
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false, false))
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_int(pContext, sHeader.iSrsId);
}

/************************************************************************/
/*                     OGRGeoPackageSetSRID()                           */
/************************************************************************/

static void OGRGeoPackageSetSRID(sqlite3_context *pContext, int /* argc */,
                                 sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }
    const int nDestSRID = sqlite3_value_int(argv[1]);
    GPkgHeader sHeader;
    int nBLOBLen = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));

    if (nBLOBLen < 8 ||
        GPkgHeaderFromWKB(pabyBLOB, nBLOBLen, &sHeader) != OGRERR_NONE)
    {
        // Try also spatialite geometry blobs
        OGRGeometry *poGeom = nullptr;
        if (OGRSQLiteImportSpatiaLiteGeometry(pabyBLOB, nBLOBLen, &poGeom) !=
            OGRERR_NONE)
        {
            sqlite3_result_null(pContext);
            return;
        }
        size_t nBLOBDestLen = 0;
        GByte *pabyDestBLOB =
            GPkgGeometryFromOGR(poGeom, nDestSRID, nullptr, &nBLOBDestLen);
        if (!pabyDestBLOB)
        {
            sqlite3_result_null(pContext);
            return;
        }
        sqlite3_result_blob(pContext, pabyDestBLOB,
                            static_cast<int>(nBLOBDestLen), VSIFree);
        return;
    }

    GByte *pabyDestBLOB = static_cast<GByte *>(CPLMalloc(nBLOBLen));
    memcpy(pabyDestBLOB, pabyBLOB, nBLOBLen);
    int32_t nSRIDToSerialize = nDestSRID;
    if (OGR_SWAP(sHeader.eByteOrder))
        nSRIDToSerialize = CPL_SWAP32(nSRIDToSerialize);
    memcpy(pabyDestBLOB + 4, &nSRIDToSerialize, 4);
    sqlite3_result_blob(pContext, pabyDestBLOB, nBLOBLen, VSIFree);
}

/************************************************************************/
/*                   OGRGeoPackageSTMakeValid()                         */
/************************************************************************/

static void OGRGeoPackageSTMakeValid(sqlite3_context *pContext, int argc,
                                     sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }
    int nBLOBLen = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));

    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false, false))
    {
        sqlite3_result_null(pContext);
        return;
    }

    auto poGeom = std::unique_ptr<OGRGeometry>(
        GPkgGeometryToOGR(pabyBLOB, nBLOBLen, nullptr));
    if (poGeom == nullptr)
    {
        // Try also spatialite geometry blobs
        OGRGeometry *poGeomPtr = nullptr;
        if (OGRSQLiteImportSpatiaLiteGeometry(pabyBLOB, nBLOBLen, &poGeomPtr) !=
            OGRERR_NONE)
        {
            sqlite3_result_null(pContext);
            return;
        }
        poGeom.reset(poGeomPtr);
    }
    auto poValid = std::unique_ptr<OGRGeometry>(poGeom->MakeValid());
    if (poValid == nullptr)
    {
        sqlite3_result_null(pContext);
        return;
    }

    size_t nBLOBDestLen = 0;
    GByte *pabyDestBLOB = GPkgGeometryFromOGR(poValid.get(), sHeader.iSrsId,
                                              nullptr, &nBLOBDestLen);
    if (!pabyDestBLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_blob(pContext, pabyDestBLOB, static_cast<int>(nBLOBDestLen),
                        VSIFree);
}

/************************************************************************/
/*                   OGRGeoPackageSTArea()                              */
/************************************************************************/

static void OGRGeoPackageSTArea(sqlite3_context *pContext, int /*argc*/,
                                sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }
    const int nBLOBLen = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));

    GPkgHeader sHeader;
    std::unique_ptr<OGRGeometry> poGeom;
    if (GPkgHeaderFromWKB(pabyBLOB, nBLOBLen, &sHeader) == OGRERR_NONE)
    {
        if (sHeader.bEmpty)
        {
            sqlite3_result_double(pContext, 0);
            return;
        }
        const GByte *pabyWkb = pabyBLOB + sHeader.nHeaderLen;
        size_t nWKBSize = nBLOBLen - sHeader.nHeaderLen;
        bool bNeedSwap;
        uint32_t nType;
        if (OGRWKBGetGeomType(pabyWkb, nWKBSize, bNeedSwap, nType))
        {
            if (nType == wkbPolygon || nType == wkbPolygon25D ||
                nType == wkbPolygon + 1000 ||  // wkbPolygonZ
                nType == wkbPolygonM || nType == wkbPolygonZM)
            {
                double dfArea;
                if (OGRWKBPolygonGetArea(pabyWkb, nWKBSize, dfArea))
                {
                    sqlite3_result_double(pContext, dfArea);
                    return;
                }
            }
            else if (nType == wkbMultiPolygon || nType == wkbMultiPolygon25D ||
                     nType == wkbMultiPolygon + 1000 ||  // wkbMultiPolygonZ
                     nType == wkbMultiPolygonM || nType == wkbMultiPolygonZM)
            {
                double dfArea;
                if (OGRWKBMultiPolygonGetArea(pabyWkb, nWKBSize, dfArea))
                {
                    sqlite3_result_double(pContext, dfArea);
                    return;
                }
            }
        }

        // For curve geometries, fallback to OGRGeometry methods
        poGeom.reset(GPkgGeometryToOGR(pabyBLOB, nBLOBLen, nullptr));
    }
    else
    {
        // Try also spatialite geometry blobs
        OGRGeometry *poGeomPtr = nullptr;
        if (OGRSQLiteImportSpatiaLiteGeometry(pabyBLOB, nBLOBLen, &poGeomPtr) !=
            OGRERR_NONE)
        {
            sqlite3_result_null(pContext);
            return;
        }
        poGeom.reset(poGeomPtr);
    }
    auto poSurface = dynamic_cast<OGRSurface *>(poGeom.get());
    if (poSurface == nullptr)
    {
        auto poMultiSurface = dynamic_cast<OGRMultiSurface *>(poGeom.get());
        if (poMultiSurface == nullptr)
        {
            sqlite3_result_double(pContext, 0);
        }
        else
        {
            sqlite3_result_double(pContext, poMultiSurface->get_Area());
        }
    }
    else
    {
        sqlite3_result_double(pContext, poSurface->get_Area());
    }
}

/************************************************************************/
/*                     OGRGeoPackageGeodesicArea()                      */
/************************************************************************/

static void OGRGeoPackageGeodesicArea(sqlite3_context *pContext, int argc,
                                      sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }
    if (sqlite3_value_int(argv[1]) != 1)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "ST_Area(geom, use_ellipsoid) is only supported for "
                 "use_ellipsoid = 1");
    }

    const int nBLOBLen = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false, false))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid geometry");
        sqlite3_result_blob(pContext, nullptr, 0, nullptr);
        return;
    }

    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));

    OGRSpatialReference *poSrcSRS = poDS->GetSpatialRef(sHeader.iSrsId, true);
    if (poSrcSRS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "SRID set on geometry (%d) is invalid", sHeader.iSrsId);
        sqlite3_result_blob(pContext, nullptr, 0, nullptr);
        return;
    }

    auto poGeom = std::unique_ptr<OGRGeometry>(
        GPkgGeometryToOGR(pabyBLOB, nBLOBLen, nullptr));
    if (poGeom == nullptr)
    {
        // Try also spatialite geometry blobs
        OGRGeometry *poGeomSpatialite = nullptr;
        if (OGRSQLiteImportSpatiaLiteGeometry(pabyBLOB, nBLOBLen,
                                              &poGeomSpatialite) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid geometry");
            sqlite3_result_blob(pContext, nullptr, 0, nullptr);
            return;
        }
        poGeom.reset(poGeomSpatialite);
    }

    poGeom->assignSpatialReference(poSrcSRS);
    sqlite3_result_double(
        pContext, OGR_G_GeodesicArea(OGRGeometry::ToHandle(poGeom.get())));
}

/************************************************************************/
/*                      OGRGeoPackageTransform()                        */
/************************************************************************/

void OGRGeoPackageTransform(sqlite3_context *pContext, int argc,
                            sqlite3_value **argv);

void OGRGeoPackageTransform(sqlite3_context *pContext, int argc,
                            sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB ||
        sqlite3_value_type(argv[1]) != SQLITE_INTEGER)
    {
        sqlite3_result_blob(pContext, nullptr, 0, nullptr);
        return;
    }

    const int nBLOBLen = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));
    GPkgHeader sHeader;
    if (!OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, false, false))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid geometry");
        sqlite3_result_blob(pContext, nullptr, 0, nullptr);
        return;
    }

    const int nDestSRID = sqlite3_value_int(argv[1]);
    if (sHeader.iSrsId == nDestSRID)
    {
        // Return blob unmodified
        sqlite3_result_blob(pContext, pabyBLOB, nBLOBLen, SQLITE_TRANSIENT);
        return;
    }

    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));

    // Try to get the cached coordinate transformation
    OGRCoordinateTransformation *poCT;
    if (poDS->m_nLastCachedCTSrcSRId == sHeader.iSrsId &&
        poDS->m_nLastCachedCTDstSRId == nDestSRID)
    {
        poCT = poDS->m_poLastCachedCT.get();
    }
    else
    {
        OGRSpatialReference *poSrcSRS =
            poDS->GetSpatialRef(sHeader.iSrsId, true);
        if (poSrcSRS == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SRID set on geometry (%d) is invalid", sHeader.iSrsId);
            sqlite3_result_blob(pContext, nullptr, 0, nullptr);
            return;
        }

        OGRSpatialReference *poDstSRS = poDS->GetSpatialRef(nDestSRID, true);
        if (poDstSRS == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Target SRID (%d) is invalid",
                     nDestSRID);
            sqlite3_result_blob(pContext, nullptr, 0, nullptr);
            poSrcSRS->Release();
            return;
        }
        poCT = OGRCreateCoordinateTransformation(poSrcSRS, poDstSRS);
        poSrcSRS->Release();
        poDstSRS->Release();

        if (poCT == nullptr)
        {
            sqlite3_result_blob(pContext, nullptr, 0, nullptr);
            return;
        }

        // Cache coordinate transformation for potential later reuse
        poDS->m_nLastCachedCTSrcSRId = sHeader.iSrsId;
        poDS->m_nLastCachedCTDstSRId = nDestSRID;
        poDS->m_poLastCachedCT.reset(poCT);
        poCT = poDS->m_poLastCachedCT.get();
    }

    auto poGeom = std::unique_ptr<OGRGeometry>(
        GPkgGeometryToOGR(pabyBLOB, nBLOBLen, nullptr));
    if (poGeom == nullptr)
    {
        // Try also spatialite geometry blobs
        OGRGeometry *poGeomSpatialite = nullptr;
        if (OGRSQLiteImportSpatiaLiteGeometry(pabyBLOB, nBLOBLen,
                                              &poGeomSpatialite) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid geometry");
            sqlite3_result_blob(pContext, nullptr, 0, nullptr);
            return;
        }
        poGeom.reset(poGeomSpatialite);
    }

    if (poGeom->transform(poCT) != OGRERR_NONE)
    {
        sqlite3_result_blob(pContext, nullptr, 0, nullptr);
        return;
    }

    size_t nBLOBDestLen = 0;
    GByte *pabyDestBLOB =
        GPkgGeometryFromOGR(poGeom.get(), nDestSRID, nullptr, &nBLOBDestLen);
    if (!pabyDestBLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_blob(pContext, pabyDestBLOB, static_cast<int>(nBLOBDestLen),
                        VSIFree);
}

/************************************************************************/
/*                      OGRGeoPackageSridFromAuthCRS()                  */
/************************************************************************/

static void OGRGeoPackageSridFromAuthCRS(sqlite3_context *pContext,
                                         int /*argc*/, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_INTEGER)
    {
        sqlite3_result_int(pContext, -1);
        return;
    }

    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));

    char *pszSQL = sqlite3_mprintf(
        "SELECT srs_id FROM gpkg_spatial_ref_sys WHERE "
        "lower(organization) = lower('%q') AND organization_coordsys_id = %d",
        sqlite3_value_text(argv[0]), sqlite3_value_int(argv[1]));
    OGRErr err = OGRERR_NONE;
    int nSRSId = SQLGetInteger(poDS->GetDB(), pszSQL, &err);
    sqlite3_free(pszSQL);
    if (err != OGRERR_NONE)
        nSRSId = -1;
    sqlite3_result_int(pContext, nSRSId);
}

/************************************************************************/
/*                    OGRGeoPackageImportFromEPSG()                     */
/************************************************************************/

static void OGRGeoPackageImportFromEPSG(sqlite3_context *pContext, int /*argc*/,
                                        sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER)
    {
        sqlite3_result_int(pContext, -1);
        return;
    }

    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));
    OGRSpatialReference oSRS;
    if (oSRS.importFromEPSG(sqlite3_value_int(argv[0])) != OGRERR_NONE)
    {
        sqlite3_result_int(pContext, -1);
        return;
    }

    sqlite3_result_int(pContext, poDS->GetSrsId(&oSRS));
}

/************************************************************************/
/*               OGRGeoPackageRegisterGeometryExtension()               */
/************************************************************************/

static void OGRGeoPackageRegisterGeometryExtension(sqlite3_context *pContext,
                                                   int /*argc*/,
                                                   sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[2]) != SQLITE_TEXT)
    {
        sqlite3_result_int(pContext, 0);
        return;
    }

    const char *pszTableName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const char *pszGeomName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    const char *pszGeomType =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[2]));

    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));

    OGRGeoPackageTableLayer *poLyr = cpl::down_cast<OGRGeoPackageTableLayer *>(
        poDS->GetLayerByName(pszTableName));
    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int(pContext, 0);
        return;
    }
    if (!EQUAL(poLyr->GetGeometryColumn(), pszGeomName))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int(pContext, 0);
        return;
    }
    const OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(pszGeomType);
    if (eGeomType == wkbUnknown)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry type name");
        sqlite3_result_int(pContext, 0);
        return;
    }

    sqlite3_result_int(
        pContext,
        static_cast<int>(poLyr->CreateGeometryExtensionIfNecessary(eGeomType)));
}

/************************************************************************/
/*                  OGRGeoPackageCreateSpatialIndex()                   */
/************************************************************************/

static void OGRGeoPackageCreateSpatialIndex(sqlite3_context *pContext,
                                            int /*argc*/, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_TEXT)
    {
        sqlite3_result_int(pContext, 0);
        return;
    }

    const char *pszTableName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const char *pszGeomName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));

    OGRGeoPackageTableLayer *poLyr = cpl::down_cast<OGRGeoPackageTableLayer *>(
        poDS->GetLayerByName(pszTableName));
    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int(pContext, 0);
        return;
    }
    if (!EQUAL(poLyr->GetGeometryColumn(), pszGeomName))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int(pContext, 0);
        return;
    }

    sqlite3_result_int(pContext, poLyr->CreateSpatialIndex());
}

/************************************************************************/
/*                  OGRGeoPackageDisableSpatialIndex()                  */
/************************************************************************/

static void OGRGeoPackageDisableSpatialIndex(sqlite3_context *pContext,
                                             int /*argc*/, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_TEXT)
    {
        sqlite3_result_int(pContext, 0);
        return;
    }

    const char *pszTableName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const char *pszGeomName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));

    OGRGeoPackageTableLayer *poLyr = cpl::down_cast<OGRGeoPackageTableLayer *>(
        poDS->GetLayerByName(pszTableName));
    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int(pContext, 0);
        return;
    }
    if (!EQUAL(poLyr->GetGeometryColumn(), pszGeomName))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int(pContext, 0);
        return;
    }

    sqlite3_result_int(pContext, poLyr->DropSpatialIndex(true));
}

/************************************************************************/
/*                  OGRGeoPackageHasSpatialIndex()                      */
/************************************************************************/

static void OGRGeoPackageHasSpatialIndex(sqlite3_context *pContext,
                                         int /*argc*/, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_TEXT)
    {
        sqlite3_result_int(pContext, 0);
        return;
    }

    const char *pszTableName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const char *pszGeomName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));

    OGRGeoPackageTableLayer *poLyr = cpl::down_cast<OGRGeoPackageTableLayer *>(
        poDS->GetLayerByName(pszTableName));
    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int(pContext, 0);
        return;
    }
    if (!EQUAL(poLyr->GetGeometryColumn(), pszGeomName))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int(pContext, 0);
        return;
    }

    poLyr->RunDeferredCreationIfNecessary();
    poLyr->CreateSpatialIndexIfNecessary();

    sqlite3_result_int(pContext, poLyr->HasSpatialIndex());
}

/************************************************************************/
/*                       GPKG_hstore_get_value()                        */
/************************************************************************/

static void GPKG_hstore_get_value(sqlite3_context *pContext, int /*argc*/,
                                  sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_TEXT)
    {
        sqlite3_result_null(pContext);
        return;
    }

    const char *pszHStore =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const char *pszSearchedKey =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    char *pszValue = OGRHStoreGetValue(pszHStore, pszSearchedKey);
    if (pszValue != nullptr)
        sqlite3_result_text(pContext, pszValue, -1, CPLFree);
    else
        sqlite3_result_null(pContext);
}

/************************************************************************/
/*                      GPKG_GDAL_GetMemFileFromBlob()                  */
/************************************************************************/

static CPLString GPKG_GDAL_GetMemFileFromBlob(sqlite3_value **argv)
{
    int nBytes = sqlite3_value_bytes(argv[0]);
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));
    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/GPKG_GDAL_GetMemFileFromBlob_%p", argv);
    VSILFILE *fp = VSIFileFromMemBuffer(
        osMemFileName.c_str(), const_cast<GByte *>(pabyBLOB), nBytes, FALSE);
    VSIFCloseL(fp);
    return osMemFileName;
}

/************************************************************************/
/*                       GPKG_GDAL_GetMimeType()                        */
/************************************************************************/

static void GPKG_GDAL_GetMimeType(sqlite3_context *pContext, int /*argc*/,
                                  sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    GDALDriver *poDriver =
        GDALDriver::FromHandle(GDALIdentifyDriver(osMemFileName, nullptr));
    if (poDriver != nullptr)
    {
        const char *pszRes = nullptr;
        if (EQUAL(poDriver->GetDescription(), "PNG"))
            pszRes = "image/png";
        else if (EQUAL(poDriver->GetDescription(), "JPEG"))
            pszRes = "image/jpeg";
        else if (EQUAL(poDriver->GetDescription(), "WEBP"))
            pszRes = "image/x-webp";
        else if (EQUAL(poDriver->GetDescription(), "GTIFF"))
            pszRes = "image/tiff";
        else
            pszRes = CPLSPrintf("gdal/%s", poDriver->GetDescription());
        sqlite3_result_text(pContext, pszRes, -1, SQLITE_TRANSIENT);
    }
    else
        sqlite3_result_null(pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                       GPKG_GDAL_GetBandCount()                       */
/************************************************************************/

static void GPKG_GDAL_GetBandCount(sqlite3_context *pContext, int /*argc*/,
                                   sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(osMemFileName, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                          nullptr, nullptr, nullptr));
    if (poDS != nullptr)
    {
        sqlite3_result_int(pContext, poDS->GetRasterCount());
    }
    else
        sqlite3_result_null(pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                       GPKG_GDAL_HasColorTable()                      */
/************************************************************************/

static void GPKG_GDAL_HasColorTable(sqlite3_context *pContext, int /*argc*/,
                                    sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(osMemFileName, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                          nullptr, nullptr, nullptr));
    if (poDS != nullptr)
    {
        sqlite3_result_int(
            pContext, poDS->GetRasterCount() == 1 &&
                          poDS->GetRasterBand(1)->GetColorTable() != nullptr);
    }
    else
        sqlite3_result_null(pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                      GetRasterLayerDataset()                         */
/************************************************************************/

GDALDataset *
GDALGeoPackageDataset::GetRasterLayerDataset(const char *pszLayerName)
{
    auto oIter = m_oCachedRasterDS.find(pszLayerName);
    if (oIter != m_oCachedRasterDS.end())
        return oIter->second.get();

    auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        (std::string("GPKG:\"") + m_pszFilename + "\":" + pszLayerName).c_str(),
        GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
    if (!poDS)
    {
        return nullptr;
    }
    m_oCachedRasterDS[pszLayerName] = std::move(poDS);
    return m_oCachedRasterDS[pszLayerName].get();
}

/************************************************************************/
/*                   GPKG_gdal_get_layer_pixel_value()                  */
/************************************************************************/

static void GPKG_gdal_get_layer_pixel_value(sqlite3_context *pContext,
                                            CPL_UNUSED int argc,
                                            sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_INTEGER ||
        sqlite3_value_type(argv[2]) != SQLITE_TEXT ||
        (sqlite3_value_type(argv[3]) != SQLITE_INTEGER &&
         sqlite3_value_type(argv[3]) != SQLITE_FLOAT) ||
        (sqlite3_value_type(argv[4]) != SQLITE_INTEGER &&
         sqlite3_value_type(argv[4]) != SQLITE_FLOAT))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid arguments to gdal_get_layer_pixel_value()");
        sqlite3_result_null(pContext);
        return;
    }

    const char *pszLayerName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));

    GDALGeoPackageDataset *poGlobalDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));
    auto poDS = poGlobalDS->GetRasterLayerDataset(pszLayerName);
    if (!poDS)
    {
        sqlite3_result_null(pContext);
        return;
    }

    const int nBand = sqlite3_value_int(argv[1]);
    auto poBand = poDS->GetRasterBand(nBand);
    if (!poBand)
    {
        sqlite3_result_null(pContext);
        return;
    }

    const char *pszCoordType =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[2]));
    int x, y;
    if (EQUAL(pszCoordType, "georef"))
    {
        const double X = sqlite3_value_double(argv[3]);
        const double Y = sqlite3_value_double(argv[4]);
        double adfGeoTransform[6];
        if (poDS->GetGeoTransform(adfGeoTransform) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        double adfInvGT[6];
        if (!GDALInvGeoTransform(adfGeoTransform, adfInvGT))
        {
            sqlite3_result_null(pContext);
            return;
        }
        x = static_cast<int>(adfInvGT[0] + X * adfInvGT[1] + Y * adfInvGT[2]);
        y = static_cast<int>(adfInvGT[3] + X * adfInvGT[4] + Y * adfInvGT[5]);
    }
    else if (EQUAL(pszCoordType, "pixel"))
    {
        x = sqlite3_value_int(argv[3]);
        y = sqlite3_value_int(argv[4]);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for 3rd argument of gdal_get_pixel_value(): "
                 "only 'georef' or 'pixel' are supported");
        sqlite3_result_null(pContext);
        return;
    }
    if (x < 0 || x >= poDS->GetRasterXSize() || y < 0 ||
        y >= poDS->GetRasterYSize())
    {
        sqlite3_result_null(pContext);
        return;
    }
    const auto eDT = poBand->GetRasterDataType();
    if (eDT != GDT_UInt64 && GDALDataTypeIsInteger(eDT))
    {
        int64_t nValue = 0;
        if (poBand->RasterIO(GF_Read, x, y, 1, 1, &nValue, 1, 1, GDT_Int64, 0,
                             0, nullptr) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        return sqlite3_result_int64(pContext, nValue);
    }
    else
    {
        double dfValue = 0;
        if (poBand->RasterIO(GF_Read, x, y, 1, 1, &dfValue, 1, 1, GDT_Float64,
                             0, 0, nullptr) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        return sqlite3_result_double(pContext, dfValue);
    }
}

/************************************************************************/
/*                       GPKG_ogr_layer_Extent()                        */
/************************************************************************/

static void GPKG_ogr_layer_Extent(sqlite3_context *pContext, int /*argc*/,
                                  sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: Invalid argument type",
                 "ogr_layer_Extent");
        sqlite3_result_null(pContext);
        return;
    }

    const char *pszLayerName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    GDALGeoPackageDataset *poDS =
        static_cast<GDALGeoPackageDataset *>(sqlite3_user_data(pContext));
    OGRLayer *poLayer = poDS->GetLayerByName(pszLayerName);
    if (!poLayer)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: unknown layer",
                 "ogr_layer_Extent");
        sqlite3_result_null(pContext);
        return;
    }

    if (poLayer->GetGeomType() == wkbNone)
    {
        sqlite3_result_null(pContext);
        return;
    }

    OGREnvelope sExtent;
    if (poLayer->GetExtent(&sExtent) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: Cannot fetch layer extent",
                 "ogr_layer_Extent");
        sqlite3_result_null(pContext);
        return;
    }

    OGRPolygon oPoly;
    OGRLinearRing *poRing = new OGRLinearRing();
    oPoly.addRingDirectly(poRing);
    poRing->addPoint(sExtent.MinX, sExtent.MinY);
    poRing->addPoint(sExtent.MaxX, sExtent.MinY);
    poRing->addPoint(sExtent.MaxX, sExtent.MaxY);
    poRing->addPoint(sExtent.MinX, sExtent.MaxY);
    poRing->addPoint(sExtent.MinX, sExtent.MinY);

    const auto poSRS = poLayer->GetSpatialRef();
    const int nSRID = poDS->GetSrsId(poSRS);
    size_t nBLOBDestLen = 0;
    GByte *pabyDestBLOB =
        GPkgGeometryFromOGR(&oPoly, nSRID, nullptr, &nBLOBDestLen);
    if (!pabyDestBLOB)
    {
        sqlite3_result_null(pContext);
        return;
    }
    sqlite3_result_blob(pContext, pabyDestBLOB, static_cast<int>(nBLOBDestLen),
                        VSIFree);
}

/************************************************************************/
/*                      InstallSQLFunctions()                           */
/************************************************************************/

#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

#ifndef SQLITE_INNOCUOUS
#define SQLITE_INNOCUOUS 0
#endif

#ifndef UTF8_INNOCUOUS
#define UTF8_INNOCUOUS (SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS)
#endif

void GDALGeoPackageDataset::InstallSQLFunctions()
{
    InitSpatialite();

    // Enable SpatiaLite 4.3 "amphibious" mode, i.e. that SpatiaLite functions
    // that take geometries will accept GPKG encoded geometries without
    // explicit conversion.
    // Use sqlite3_exec() instead of SQLCommand() since we don't want verbose
    // error.
    sqlite3_exec(hDB, "SELECT EnableGpkgAmphibiousMode()", nullptr, nullptr,
                 nullptr);

    /* Used by RTree Spatial Index Extension */
    sqlite3_create_function(hDB, "ST_MinX", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTMinX, nullptr, nullptr);
    sqlite3_create_function(hDB, "ST_MinY", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTMinY, nullptr, nullptr);
    sqlite3_create_function(hDB, "ST_MaxX", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTMaxX, nullptr, nullptr);
    sqlite3_create_function(hDB, "ST_MaxY", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTMaxY, nullptr, nullptr);
    sqlite3_create_function(hDB, "ST_IsEmpty", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTIsEmpty, nullptr, nullptr);

    /* Used by Geometry Type Triggers Extension */
    sqlite3_create_function(hDB, "ST_GeometryType", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTGeometryType, nullptr, nullptr);
    sqlite3_create_function(hDB, "GPKG_IsAssignable", 2, UTF8_INNOCUOUS,
                            nullptr, OGRGeoPackageGPKGIsAssignable, nullptr,
                            nullptr);

    /* Used by Geometry SRS ID Triggers Extension */
    sqlite3_create_function(hDB, "ST_SRID", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTSRID, nullptr, nullptr);

    /* Spatialite-like functions */
    sqlite3_create_function(hDB, "CreateSpatialIndex", 2, SQLITE_UTF8, this,
                            OGRGeoPackageCreateSpatialIndex, nullptr, nullptr);
    sqlite3_create_function(hDB, "DisableSpatialIndex", 2, SQLITE_UTF8, this,
                            OGRGeoPackageDisableSpatialIndex, nullptr, nullptr);
    sqlite3_create_function(hDB, "HasSpatialIndex", 2, SQLITE_UTF8, this,
                            OGRGeoPackageHasSpatialIndex, nullptr, nullptr);

    // HSTORE functions
    sqlite3_create_function(hDB, "hstore_get_value", 2, UTF8_INNOCUOUS, nullptr,
                            GPKG_hstore_get_value, nullptr, nullptr);

    // Override a few Spatialite functions to work with gpkg_spatial_ref_sys
    sqlite3_create_function(hDB, "ST_Transform", 2, UTF8_INNOCUOUS, this,
                            OGRGeoPackageTransform, nullptr, nullptr);
    sqlite3_create_function(hDB, "Transform", 2, UTF8_INNOCUOUS, this,
                            OGRGeoPackageTransform, nullptr, nullptr);
    sqlite3_create_function(hDB, "SridFromAuthCRS", 2, SQLITE_UTF8, this,
                            OGRGeoPackageSridFromAuthCRS, nullptr, nullptr);

    sqlite3_create_function(hDB, "ST_EnvIntersects", 2, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTEnvelopesIntersectsTwoParams,
                            nullptr, nullptr);
    sqlite3_create_function(
        hDB, "ST_EnvelopesIntersects", 2, UTF8_INNOCUOUS, nullptr,
        OGRGeoPackageSTEnvelopesIntersectsTwoParams, nullptr, nullptr);

    sqlite3_create_function(hDB, "ST_EnvIntersects", 5, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTEnvelopesIntersects, nullptr,
                            nullptr);
    sqlite3_create_function(hDB, "ST_EnvelopesIntersects", 5, UTF8_INNOCUOUS,
                            nullptr, OGRGeoPackageSTEnvelopesIntersects,
                            nullptr, nullptr);

    // Implementation that directly hacks the GeoPackage geometry blob header
    sqlite3_create_function(hDB, "SetSRID", 2, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSetSRID, nullptr, nullptr);

    // GDAL specific function
    sqlite3_create_function(hDB, "ImportFromEPSG", 1, SQLITE_UTF8, this,
                            OGRGeoPackageImportFromEPSG, nullptr, nullptr);

    // May be used by ogrmerge.py
    sqlite3_create_function(hDB, "RegisterGeometryExtension", 3, SQLITE_UTF8,
                            this, OGRGeoPackageRegisterGeometryExtension,
                            nullptr, nullptr);

    if (OGRGeometryFactory::haveGEOS())
    {
        sqlite3_create_function(hDB, "ST_MakeValid", 1, UTF8_INNOCUOUS, nullptr,
                                OGRGeoPackageSTMakeValid, nullptr, nullptr);
    }

    sqlite3_create_function(hDB, "ST_Area", 1, UTF8_INNOCUOUS, nullptr,
                            OGRGeoPackageSTArea, nullptr, nullptr);
    sqlite3_create_function(hDB, "ST_Area", 2, UTF8_INNOCUOUS, this,
                            OGRGeoPackageGeodesicArea, nullptr, nullptr);

    // Debug functions
    if (CPLTestBool(CPLGetConfigOption("GPKG_DEBUG", "FALSE")))
    {
        sqlite3_create_function(hDB, "GDAL_GetMimeType", 1,
                                SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                                GPKG_GDAL_GetMimeType, nullptr, nullptr);
        sqlite3_create_function(hDB, "GDAL_GetBandCount", 1,
                                SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                                GPKG_GDAL_GetBandCount, nullptr, nullptr);
        sqlite3_create_function(hDB, "GDAL_HasColorTable", 1,
                                SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                                GPKG_GDAL_HasColorTable, nullptr, nullptr);
    }

    sqlite3_create_function(hDB, "gdal_get_layer_pixel_value", 5, SQLITE_UTF8,
                            this, GPKG_gdal_get_layer_pixel_value, nullptr,
                            nullptr);

    // Function from VirtualOGR
    sqlite3_create_function(hDB, "ogr_layer_Extent", 1, SQLITE_UTF8, this,
                            GPKG_ogr_layer_Extent, nullptr, nullptr);

    m_pSQLFunctionData = OGRSQLiteRegisterSQLFunctionsCommon(hDB);
}

/************************************************************************/
/*                         OpenOrCreateDB()                             */
/************************************************************************/

bool GDALGeoPackageDataset::OpenOrCreateDB(int flags)
{
    const bool bSuccess = OGRSQLiteBaseDataSource::OpenOrCreateDB(
        flags, /*bRegisterOGR2SQLiteExtensions=*/false,
        /*bLoadExtensions=*/true);
    if (!bSuccess)
        return false;

    // Turning on recursive_triggers is needed so that DELETE triggers fire
    // in a INSERT OR REPLACE statement. In particular this is needed to
    // make sure gpkg_ogr_contents.feature_count is properly updated.
    SQLCommand(hDB, "PRAGMA recursive_triggers = 1");

    InstallSQLFunctions();

    const char *pszSqlitePragma =
        CPLGetConfigOption("OGR_SQLITE_PRAGMA", nullptr);
    OGRErr eErr = OGRERR_NONE;
    if ((!pszSqlitePragma || !strstr(pszSqlitePragma, "trusted_schema")) &&
        // Older sqlite versions don't have this pragma
        SQLGetInteger(hDB, "PRAGMA trusted_schema", &eErr) == 0 &&
        eErr == OGRERR_NONE)
    {
        bool bNeedsTrustedSchema = false;

        // Current SQLite versions require PRAGMA trusted_schema = 1 to be
        // able to use the RTree from triggers, which is only needed when
        // modifying the RTree.
        if (((flags & SQLITE_OPEN_READWRITE) != 0 ||
             (flags & SQLITE_OPEN_CREATE) != 0) &&
            OGRSQLiteRTreeRequiresTrustedSchemaOn())
        {
            bNeedsTrustedSchema = true;
        }

#ifdef HAVE_SPATIALITE
        // Spatialite <= 5.1.0 doesn't declare its functions as SQLITE_INNOCUOUS
        if (!bNeedsTrustedSchema && HasExtensionsTable() &&
            SQLGetInteger(
                hDB,
                "SELECT 1 FROM gpkg_extensions WHERE "
                "extension_name ='gdal_spatialite_computed_geom_column'",
                nullptr) == 1 &&
            SpatialiteRequiresTrustedSchemaOn() && AreSpatialiteTriggersSafe())
        {
            bNeedsTrustedSchema = true;
        }
#endif

        if (bNeedsTrustedSchema)
        {
            CPLDebug("GPKG", "Setting PRAGMA trusted_schema = 1");
            SQLCommand(hDB, "PRAGMA trusted_schema = 1");
        }
    }

    return true;
}

/************************************************************************/
/*                   GetLayerWithGetSpatialWhereByName()                */
/************************************************************************/

std::pair<OGRLayer *, IOGRSQLiteGetSpatialWhere *>
GDALGeoPackageDataset::GetLayerWithGetSpatialWhereByName(const char *pszName)
{
    OGRGeoPackageLayer *poRet =
        cpl::down_cast<OGRGeoPackageLayer *>(GetLayerByName(pszName));
    return std::pair(poRet, poRet);
}

/************************************************************************/
/*                       CommitTransaction()                            */
/************************************************************************/

OGRErr GDALGeoPackageDataset::CommitTransaction()

{
    if (nSoftTransactionLevel == 1)
    {
        FlushMetadata();
        for (int i = 0; i < m_nLayers; i++)
        {
            m_papoLayers[i]->DoJobAtTransactionCommit();
        }
    }

    return OGRSQLiteBaseDataSource::CommitTransaction();
}

/************************************************************************/
/*                     RollbackTransaction()                            */
/************************************************************************/

OGRErr GDALGeoPackageDataset::RollbackTransaction()

{
#ifdef ENABLE_GPKG_OGR_CONTENTS
    std::vector<bool> abAddTriggers;
    std::vector<bool> abTriggersDeletedInTransaction;
#endif
    if (nSoftTransactionLevel == 1)
    {
        FlushMetadata();
        for (int i = 0; i < m_nLayers; i++)
        {
#ifdef ENABLE_GPKG_OGR_CONTENTS
            abAddTriggers.push_back(
                m_papoLayers[i]->GetAddOGRFeatureCountTriggers());
            abTriggersDeletedInTransaction.push_back(
                m_papoLayers[i]
                    ->GetOGRFeatureCountTriggersDeletedInTransaction());
            m_papoLayers[i]->SetAddOGRFeatureCountTriggers(false);
#endif
            m_papoLayers[i]->DoJobAtTransactionRollback();
#ifdef ENABLE_GPKG_OGR_CONTENTS
            m_papoLayers[i]->DisableFeatureCount();
#endif
        }
    }

    OGRErr eErr = OGRSQLiteBaseDataSource::RollbackTransaction();
#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (!abAddTriggers.empty())
    {
        for (int i = 0; i < m_nLayers; i++)
        {
            if (abTriggersDeletedInTransaction[i])
            {
                m_papoLayers[i]->SetOGRFeatureCountTriggersEnabled(true);
            }
            else
            {
                m_papoLayers[i]->SetAddOGRFeatureCountTriggers(
                    abAddTriggers[i]);
            }
        }
    }
#endif
    return eErr;
}

/************************************************************************/
/*                       GetGeometryTypeString()                        */
/************************************************************************/

const char *
GDALGeoPackageDataset::GetGeometryTypeString(OGRwkbGeometryType eType)
{
    const char *pszGPKGGeomType = OGRToOGCGeomType(eType);
    if (EQUAL(pszGPKGGeomType, "GEOMETRYCOLLECTION") &&
        CPLTestBool(CPLGetConfigOption("OGR_GPKG_GEOMCOLLECTION", "NO")))
    {
        pszGPKGGeomType = "GEOMCOLLECTION";
    }
    return pszGPKGGeomType;
}

/************************************************************************/
/*                           GetFieldDomainNames()                      */
/************************************************************************/

std::vector<std::string>
GDALGeoPackageDataset::GetFieldDomainNames(CSLConstList) const
{
    if (!HasDataColumnConstraintsTable())
        return std::vector<std::string>();

    std::vector<std::string> oDomainNamesList;

    std::unique_ptr<SQLResult> oResultTable;
    {
        std::string osSQL =
            "SELECT DISTINCT constraint_name "
            "FROM gpkg_data_column_constraints "
            "WHERE constraint_name NOT LIKE '_%_domain_description' "
            "ORDER BY constraint_name "
            "LIMIT 10000"  // to avoid denial of service
            ;
        oResultTable = SQLQuery(hDB, osSQL.c_str());
        if (!oResultTable)
            return oDomainNamesList;
    }

    if (oResultTable->RowCount() == 10000)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Number of rows returned for field domain names has been "
                 "truncated.");
    }
    else if (oResultTable->RowCount() > 0)
    {
        oDomainNamesList.reserve(oResultTable->RowCount());
        for (int i = 0; i < oResultTable->RowCount(); i++)
        {
            const char *pszConstraintName = oResultTable->GetValue(0, i);
            if (!pszConstraintName)
                continue;

            oDomainNamesList.emplace_back(pszConstraintName);
        }
    }

    return oDomainNamesList;
}

/************************************************************************/
/*                           GetFieldDomain()                           */
/************************************************************************/

const OGRFieldDomain *
GDALGeoPackageDataset::GetFieldDomain(const std::string &name) const
{
    const auto baseRet = GDALDataset::GetFieldDomain(name);
    if (baseRet)
        return baseRet;

    if (!HasDataColumnConstraintsTable())
        return nullptr;

    const bool bIsGPKG10 = HasDataColumnConstraintsTableGPKG_1_0();
    const char *min_is_inclusive =
        bIsGPKG10 ? "minIsInclusive" : "min_is_inclusive";
    const char *max_is_inclusive =
        bIsGPKG10 ? "maxIsInclusive" : "max_is_inclusive";

    std::unique_ptr<SQLResult> oResultTable;
    // Note: for coded domains, we use a little trick by using a dummy
    // _{domainname}_domain_description enum that has a single entry whose
    // description is the description of the main domain.
    {
        char *pszSQL = sqlite3_mprintf(
            "SELECT constraint_type, value, min, %s, "
            "max, %s, description, constraint_name "
            "FROM gpkg_data_column_constraints "
            "WHERE constraint_name IN ('%q', "
            "'_%q_domain_description') "
            "AND length(constraint_type) < 100 "  // to
                                                  // avoid
                                                  // denial
                                                  // of
                                                  // service
            "AND (value IS NULL OR length(value) < "
            "10000) "  // to avoid denial
                       // of service
            "AND (description IS NULL OR "
            "length(description) < 10000) "  // to
                                             // avoid
                                             // denial
                                             // of
                                             // service
            "ORDER BY value "
            "LIMIT 10000",  // to avoid denial of
                            // service
            min_is_inclusive, max_is_inclusive, name.c_str(), name.c_str());
        oResultTable = SQLQuery(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (!oResultTable)
            return nullptr;
    }
    if (oResultTable->RowCount() == 0)
    {
        return nullptr;
    }
    if (oResultTable->RowCount() == 10000)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Number of rows returned for field domain %s has been "
                 "truncated.",
                 name.c_str());
    }

    // Try to find the field domain data type from fields that implement it
    int nFieldType = -1;
    OGRFieldSubType eSubType = OFSTNone;
    if (HasDataColumnsTable())
    {
        char *pszSQL = sqlite3_mprintf(
            "SELECT table_name, column_name FROM gpkg_data_columns WHERE "
            "constraint_name = '%q' LIMIT 10",
            name.c_str());
        auto oResultTable2 = SQLQuery(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (oResultTable2 && oResultTable2->RowCount() >= 1)
        {
            for (int iRecord = 0; iRecord < oResultTable2->RowCount();
                 iRecord++)
            {
                const char *pszTableName = oResultTable2->GetValue(0, iRecord);
                const char *pszColumnName = oResultTable2->GetValue(1, iRecord);
                if (pszTableName == nullptr || pszColumnName == nullptr)
                    continue;
                OGRLayer *poLayer =
                    const_cast<GDALGeoPackageDataset *>(this)->GetLayerByName(
                        pszTableName);
                if (poLayer)
                {
                    const auto poFDefn = poLayer->GetLayerDefn();
                    int nIdx = poFDefn->GetFieldIndex(pszColumnName);
                    if (nIdx >= 0)
                    {
                        const auto poFieldDefn = poFDefn->GetFieldDefn(nIdx);
                        const auto eType = poFieldDefn->GetType();
                        if (nFieldType < 0)
                        {
                            nFieldType = eType;
                            eSubType = poFieldDefn->GetSubType();
                        }
                        else if ((eType == OFTInteger64 || eType == OFTReal) &&
                                 nFieldType == OFTInteger)
                        {
                            // ok
                        }
                        else if (eType == OFTInteger &&
                                 (nFieldType == OFTInteger64 ||
                                  nFieldType == OFTReal))
                        {
                            nFieldType = OFTInteger;
                            eSubType = OFSTNone;
                        }
                        else if (nFieldType != eType)
                        {
                            nFieldType = -1;
                            eSubType = OFSTNone;
                            break;
                        }
                    }
                }
            }
        }
    }

    std::unique_ptr<OGRFieldDomain> poDomain;
    std::vector<OGRCodedValue> asValues;
    bool error = false;
    CPLString osLastConstraintType;
    int nFieldTypeFromEnumCode = -1;
    std::string osConstraintDescription;
    std::string osDescrConstraintName("_");
    osDescrConstraintName += name;
    osDescrConstraintName += "_domain_description";
    for (int iRecord = 0; iRecord < oResultTable->RowCount(); iRecord++)
    {
        const char *pszConstraintType = oResultTable->GetValue(0, iRecord);
        if (pszConstraintType == nullptr)
            continue;
        const char *pszValue = oResultTable->GetValue(1, iRecord);
        const char *pszMin = oResultTable->GetValue(2, iRecord);
        const bool bIsMinIncluded =
            oResultTable->GetValueAsInteger(3, iRecord) == 1;
        const char *pszMax = oResultTable->GetValue(4, iRecord);
        const bool bIsMaxIncluded =
            oResultTable->GetValueAsInteger(5, iRecord) == 1;
        const char *pszDescription = oResultTable->GetValue(6, iRecord);
        const char *pszConstraintName = oResultTable->GetValue(7, iRecord);

        if (!osLastConstraintType.empty() && osLastConstraintType != "enum")
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Only constraint of type 'enum' can have multiple rows");
            error = true;
            break;
        }

        if (strcmp(pszConstraintType, "enum") == 0)
        {
            if (pszValue == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "NULL in 'value' column of enumeration");
                error = true;
                break;
            }
            if (osDescrConstraintName == pszConstraintName)
            {
                if (pszDescription)
                {
                    osConstraintDescription = pszDescription;
                }
                continue;
            }
            if (asValues.empty())
            {
                asValues.reserve(oResultTable->RowCount() + 1);
            }
            OGRCodedValue cv;
            // intended: the 'value' column in GPKG is actually the code
            cv.pszCode = VSI_STRDUP_VERBOSE(pszValue);
            if (cv.pszCode == nullptr)
            {
                error = true;
                break;
            }
            if (pszDescription)
            {
                cv.pszValue = VSI_STRDUP_VERBOSE(pszDescription);
                if (cv.pszValue == nullptr)
                {
                    VSIFree(cv.pszCode);
                    error = true;
                    break;
                }
            }
            else
            {
                cv.pszValue = nullptr;
            }

            // If we can't get the data type from field definition, guess it
            // from code.
            if (nFieldType < 0 && nFieldTypeFromEnumCode != OFTString)
            {
                switch (CPLGetValueType(cv.pszCode))
                {
                    case CPL_VALUE_INTEGER:
                    {
                        if (nFieldTypeFromEnumCode != OFTReal &&
                            nFieldTypeFromEnumCode != OFTInteger64)
                        {
                            const auto nVal = CPLAtoGIntBig(cv.pszCode);
                            if (nVal < std::numeric_limits<int>::min() ||
                                nVal > std::numeric_limits<int>::max())
                            {
                                nFieldTypeFromEnumCode = OFTInteger64;
                            }
                            else
                            {
                                nFieldTypeFromEnumCode = OFTInteger;
                            }
                        }
                        break;
                    }

                    case CPL_VALUE_REAL:
                        nFieldTypeFromEnumCode = OFTReal;
                        break;

                    case CPL_VALUE_STRING:
                        nFieldTypeFromEnumCode = OFTString;
                        break;
                }
            }

            asValues.emplace_back(cv);
        }
        else if (strcmp(pszConstraintType, "range") == 0)
        {
            OGRField sMin;
            OGRField sMax;
            OGR_RawField_SetUnset(&sMin);
            OGR_RawField_SetUnset(&sMax);
            if (nFieldType != OFTInteger && nFieldType != OFTInteger64)
                nFieldType = OFTReal;
            if (pszMin != nullptr &&
                CPLAtof(pszMin) != -std::numeric_limits<double>::infinity())
            {
                if (nFieldType == OFTInteger)
                    sMin.Integer = atoi(pszMin);
                else if (nFieldType == OFTInteger64)
                    sMin.Integer64 = CPLAtoGIntBig(pszMin);
                else /* if( nFieldType == OFTReal ) */
                    sMin.Real = CPLAtof(pszMin);
            }
            if (pszMax != nullptr &&
                CPLAtof(pszMax) != std::numeric_limits<double>::infinity())
            {
                if (nFieldType == OFTInteger)
                    sMax.Integer = atoi(pszMax);
                else if (nFieldType == OFTInteger64)
                    sMax.Integer64 = CPLAtoGIntBig(pszMax);
                else /* if( nFieldType == OFTReal ) */
                    sMax.Real = CPLAtof(pszMax);
            }
            poDomain.reset(new OGRRangeFieldDomain(
                name, pszDescription ? pszDescription : "",
                static_cast<OGRFieldType>(nFieldType), eSubType, sMin,
                bIsMinIncluded, sMax, bIsMaxIncluded));
        }
        else if (strcmp(pszConstraintType, "glob") == 0)
        {
            if (pszValue == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "NULL in 'value' column of glob");
                error = true;
                break;
            }
            if (nFieldType < 0)
                nFieldType = OFTString;
            poDomain.reset(new OGRGlobFieldDomain(
                name, pszDescription ? pszDescription : "",
                static_cast<OGRFieldType>(nFieldType), eSubType, pszValue));
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unhandled constraint_type: %s", pszConstraintType);
            error = true;
            break;
        }

        osLastConstraintType = pszConstraintType;
    }

    if (!asValues.empty())
    {
        if (nFieldType < 0)
            nFieldType = nFieldTypeFromEnumCode;
        poDomain.reset(
            new OGRCodedFieldDomain(name, osConstraintDescription,
                                    static_cast<OGRFieldType>(nFieldType),
                                    eSubType, std::move(asValues)));
    }

    if (error)
    {
        return nullptr;
    }

    m_oMapFieldDomains[name] = std::move(poDomain);
    return GDALDataset::GetFieldDomain(name);
}

/************************************************************************/
/*                           AddFieldDomain()                           */
/************************************************************************/

bool GDALGeoPackageDataset::AddFieldDomain(
    std::unique_ptr<OGRFieldDomain> &&domain, std::string &failureReason)
{
    const std::string domainName(domain->GetName());
    if (!GetUpdate())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AddFieldDomain() not supported on read-only dataset");
        return false;
    }
    if (GetFieldDomain(domainName) != nullptr)
    {
        failureReason = "A domain of identical name already exists";
        return false;
    }
    if (!CreateColumnsTableAndColumnConstraintsTablesIfNecessary())
        return false;

    const bool bIsGPKG10 = HasDataColumnConstraintsTableGPKG_1_0();
    const char *min_is_inclusive =
        bIsGPKG10 ? "minIsInclusive" : "min_is_inclusive";
    const char *max_is_inclusive =
        bIsGPKG10 ? "maxIsInclusive" : "max_is_inclusive";

    const auto &osDescription = domain->GetDescription();
    switch (domain->GetDomainType())
    {
        case OFDT_CODED:
        {
            const auto poCodedDomain =
                cpl::down_cast<const OGRCodedFieldDomain *>(domain.get());
            if (!osDescription.empty())
            {
                // We use a little trick by using a dummy
                // _{domainname}_domain_description enum that has a single
                // entry whose description is the description of the main
                // domain.
                char *pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_data_column_constraints ("
                    "constraint_name, constraint_type, value, "
                    "min, %s, max, %s, "
                    "description) VALUES ("
                    "'_%q_domain_description', 'enum', '', NULL, NULL, NULL, "
                    "NULL, %Q)",
                    min_is_inclusive, max_is_inclusive, domainName.c_str(),
                    osDescription.c_str());
                CPL_IGNORE_RET_VAL(SQLCommand(hDB, pszSQL));
                sqlite3_free(pszSQL);
            }
            const auto &enumeration = poCodedDomain->GetEnumeration();
            for (int i = 0; enumeration[i].pszCode != nullptr; ++i)
            {
                char *pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_data_column_constraints ("
                    "constraint_name, constraint_type, value, "
                    "min, %s, max, %s, "
                    "description) VALUES ("
                    "'%q', 'enum', '%q', NULL, NULL, NULL, NULL, %Q)",
                    min_is_inclusive, max_is_inclusive, domainName.c_str(),
                    enumeration[i].pszCode, enumeration[i].pszValue);
                bool ok = SQLCommand(hDB, pszSQL) == OGRERR_NONE;
                sqlite3_free(pszSQL);
                if (!ok)
                    return false;
            }
            break;
        }

        case OFDT_RANGE:
        {
            const auto poRangeDomain =
                cpl::down_cast<const OGRRangeFieldDomain *>(domain.get());
            const auto eFieldType = poRangeDomain->GetFieldType();
            if (eFieldType != OFTInteger && eFieldType != OFTInteger64 &&
                eFieldType != OFTReal)
            {
                failureReason = "Only range domains of numeric type are "
                                "supported in GeoPackage";
                return false;
            }

            double dfMin = -std::numeric_limits<double>::infinity();
            double dfMax = std::numeric_limits<double>::infinity();
            bool bMinIsInclusive = true;
            const auto &sMin = poRangeDomain->GetMin(bMinIsInclusive);
            bool bMaxIsInclusive = true;
            const auto &sMax = poRangeDomain->GetMax(bMaxIsInclusive);
            if (eFieldType == OFTInteger)
            {
                if (!OGR_RawField_IsUnset(&sMin))
                    dfMin = sMin.Integer;
                if (!OGR_RawField_IsUnset(&sMax))
                    dfMax = sMax.Integer;
            }
            else if (eFieldType == OFTInteger64)
            {
                if (!OGR_RawField_IsUnset(&sMin))
                    dfMin = static_cast<double>(sMin.Integer64);
                if (!OGR_RawField_IsUnset(&sMax))
                    dfMax = static_cast<double>(sMax.Integer64);
            }
            else /* if( eFieldType == OFTReal ) */
            {
                if (!OGR_RawField_IsUnset(&sMin))
                    dfMin = sMin.Real;
                if (!OGR_RawField_IsUnset(&sMax))
                    dfMax = sMax.Real;
            }

            sqlite3_stmt *hInsertStmt = nullptr;
            const char *pszSQL =
                CPLSPrintf("INSERT INTO gpkg_data_column_constraints ("
                           "constraint_name, constraint_type, value, "
                           "min, %s, max, %s, "
                           "description) VALUES ("
                           "?, 'range', NULL, ?, ?, ?, ?, ?)",
                           min_is_inclusive, max_is_inclusive);
            if (sqlite3_prepare_v2(hDB, pszSQL, -1, &hInsertStmt, nullptr) !=
                SQLITE_OK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to prepare SQL: %s", pszSQL);
                return false;
            }
            sqlite3_bind_text(hInsertStmt, 1, domainName.c_str(),
                              static_cast<int>(domainName.size()),
                              SQLITE_TRANSIENT);
            sqlite3_bind_double(hInsertStmt, 2, dfMin);
            sqlite3_bind_int(hInsertStmt, 3, bMinIsInclusive ? 1 : 0);
            sqlite3_bind_double(hInsertStmt, 4, dfMax);
            sqlite3_bind_int(hInsertStmt, 5, bMaxIsInclusive ? 1 : 0);
            if (osDescription.empty())
            {
                sqlite3_bind_null(hInsertStmt, 6);
            }
            else
            {
                sqlite3_bind_text(hInsertStmt, 6, osDescription.c_str(),
                                  static_cast<int>(osDescription.size()),
                                  SQLITE_TRANSIENT);
            }
            const int sqlite_err = sqlite3_step(hInsertStmt);
            sqlite3_finalize(hInsertStmt);
            if (sqlite_err != SQLITE_OK && sqlite_err != SQLITE_DONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to execute insertion: %s",
                         sqlite3_errmsg(hDB));
                return false;
            }

            break;
        }

        case OFDT_GLOB:
        {
            const auto poGlobDomain =
                cpl::down_cast<const OGRGlobFieldDomain *>(domain.get());
            char *pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_data_column_constraints ("
                "constraint_name, constraint_type, value, "
                "min, %s, max, %s, "
                "description) VALUES ("
                "'%q', 'glob', '%q', NULL, NULL, NULL, NULL, %Q)",
                min_is_inclusive, max_is_inclusive, domainName.c_str(),
                poGlobDomain->GetGlob().c_str(),
                osDescription.empty() ? nullptr : osDescription.c_str());
            bool ok = SQLCommand(hDB, pszSQL) == OGRERR_NONE;
            sqlite3_free(pszSQL);
            if (!ok)
                return false;

            break;
        }
    }

    m_oMapFieldDomains[domainName] = std::move(domain);
    return true;
}

/************************************************************************/
/*                          AddRelationship()                           */
/************************************************************************/

bool GDALGeoPackageDataset::AddRelationship(
    std::unique_ptr<GDALRelationship> &&relationship,
    std::string &failureReason)
{
    if (!GetUpdate())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AddRelationship() not supported on read-only dataset");
        return false;
    }

    const std::string osRelationshipName = GenerateNameForRelationship(
        relationship->GetLeftTableName().c_str(),
        relationship->GetRightTableName().c_str(),
        relationship->GetRelatedTableType().c_str());
    // sanity checks
    if (GetRelationship(osRelationshipName) != nullptr)
    {
        failureReason = "A relationship of identical name already exists";
        return false;
    }

    if (!ValidateRelationship(relationship.get(), failureReason))
    {
        return false;
    }

    if (CreateExtensionsTableIfNecessary() != OGRERR_NONE)
    {
        return false;
    }
    if (!CreateRelationsTableIfNecessary())
    {
        failureReason = "Could not create gpkgext_relations table";
        return false;
    }
    if (SQLGetInteger(GetDB(),
                      "SELECT 1 FROM gpkg_extensions WHERE "
                      "table_name = 'gpkgext_relations'",
                      nullptr) != 1)
    {
        if (OGRERR_NONE !=
            SQLCommand(
                GetDB(),
                "INSERT INTO gpkg_extensions "
                "(table_name,column_name,extension_name,definition,scope) "
                "VALUES ('gpkgext_relations', NULL, 'gpkg_related_tables', "
                "'http://www.geopackage.org/18-000.html', "
                "'read-write')"))
        {
            failureReason =
                "Could not create gpkg_extensions entry for gpkgext_relations";
            return false;
        }
    }

    const std::string &osLeftTableName = relationship->GetLeftTableName();
    const std::string &osRightTableName = relationship->GetRightTableName();
    const auto &aosLeftTableFields = relationship->GetLeftTableFields();
    const auto &aosRightTableFields = relationship->GetRightTableFields();

    std::string osRelatedTableType = relationship->GetRelatedTableType();
    if (osRelatedTableType.empty())
    {
        osRelatedTableType = "features";
    }

    // generate mapping table if not set
    CPLString osMappingTableName = relationship->GetMappingTableName();
    if (osMappingTableName.empty())
    {
        int nIndex = 1;
        osMappingTableName = osLeftTableName + "_" + osRightTableName;
        while (FindLayerIndex(osMappingTableName.c_str()) >= 0)
        {
            nIndex += 1;
            osMappingTableName.Printf("%s_%s_%d", osLeftTableName.c_str(),
                                      osRightTableName.c_str(), nIndex);
        }

        // determine whether base/related keys are unique
        bool bBaseKeyIsUnique = false;
        {
            const std::set<std::string> uniqueBaseFieldsUC =
                SQLGetUniqueFieldUCConstraints(GetDB(),
                                               osLeftTableName.c_str());
            if (uniqueBaseFieldsUC.find(
                    CPLString(aosLeftTableFields[0]).toupper()) !=
                uniqueBaseFieldsUC.end())
            {
                bBaseKeyIsUnique = true;
            }
        }
        bool bRelatedKeyIsUnique = false;
        {
            const std::set<std::string> uniqueRelatedFieldsUC =
                SQLGetUniqueFieldUCConstraints(GetDB(),
                                               osRightTableName.c_str());
            if (uniqueRelatedFieldsUC.find(
                    CPLString(aosRightTableFields[0]).toupper()) !=
                uniqueRelatedFieldsUC.end())
            {
                bRelatedKeyIsUnique = true;
            }
        }

        // create mapping table

        std::string osBaseIdDefinition = "base_id INTEGER";
        if (bBaseKeyIsUnique)
        {
            char *pszSQL = sqlite3_mprintf(
                " CONSTRAINT 'fk_base_id_%q' REFERENCES \"%w\"(\"%w\") ON "
                "DELETE CASCADE ON UPDATE CASCADE DEFERRABLE INITIALLY "
                "DEFERRED",
                osMappingTableName.c_str(), osLeftTableName.c_str(),
                aosLeftTableFields[0].c_str());
            osBaseIdDefinition += pszSQL;
            sqlite3_free(pszSQL);
        }

        std::string osRelatedIdDefinition = "related_id INTEGER";
        if (bRelatedKeyIsUnique)
        {
            char *pszSQL = sqlite3_mprintf(
                " CONSTRAINT 'fk_related_id_%q' REFERENCES \"%w\"(\"%w\") ON "
                "DELETE CASCADE ON UPDATE CASCADE DEFERRABLE INITIALLY "
                "DEFERRED",
                osMappingTableName.c_str(), osRightTableName.c_str(),
                aosRightTableFields[0].c_str());
            osRelatedIdDefinition += pszSQL;
            sqlite3_free(pszSQL);
        }

        char *pszSQL = sqlite3_mprintf("CREATE TABLE \"%w\" ("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                       "%s, %s);",
                                       osMappingTableName.c_str(),
                                       osBaseIdDefinition.c_str(),
                                       osRelatedIdDefinition.c_str());
        OGRErr eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (eErr != OGRERR_NONE)
        {
            failureReason =
                ("Could not create mapping table " + osMappingTableName)
                    .c_str();
            return false;
        }

        /*
         * Strictly speaking we should NOT be inserting the mapping table into gpkg_contents.
         * The related tables extension explicitly states that the mapping table should only be
         * in the gpkgext_relations table and not in gpkg_contents. (See also discussion at
         * https://github.com/opengeospatial/geopackage/issues/679).
         *
         * However, if we don't insert the mapping table into gpkg_contents then it is no longer
         * visible to some clients (eg ESRI software only allows opening tables that are present
         * in gpkg_contents). So we'll do this anyway, for maximum compatibility and flexibility.
         *
         * More related discussion is at https://github.com/OSGeo/gdal/pull/9258
         */
        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_contents "
            "(table_name,data_type,identifier,description,last_change,srs_id) "
            "VALUES "
            "('%q','attributes','%q','Mapping table for relationship between "
            "%q and %q',%s,0)",
            osMappingTableName.c_str(), /*table_name*/
            osMappingTableName.c_str(), /*identifier*/
            osLeftTableName.c_str(),    /*description left table name*/
            osRightTableName.c_str(),   /*description right table name*/
            GDALGeoPackageDataset::GetCurrentDateEscapedSQL().c_str());

        // Note -- we explicitly ignore failures here, because hey, we aren't really
        // supposed to be adding this table to gpkg_contents anyway!
        (void)SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        pszSQL = sqlite3_mprintf(
            "CREATE INDEX \"idx_%w_base_id\" ON \"%w\" (base_id);",
            osMappingTableName.c_str(), osMappingTableName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (eErr != OGRERR_NONE)
        {
            failureReason = ("Could not create index for " +
                             osMappingTableName + " (base_id)")
                                .c_str();
            return false;
        }

        pszSQL = sqlite3_mprintf(
            "CREATE INDEX \"idx_%qw_related_id\" ON \"%w\" (related_id);",
            osMappingTableName.c_str(), osMappingTableName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if (eErr != OGRERR_NONE)
        {
            failureReason = ("Could not create index for " +
                             osMappingTableName + " (related_id)")
                                .c_str();
            return false;
        }
    }
    else
    {
        // validate mapping table structure
        if (OGRGeoPackageTableLayer *poLayer =
                cpl::down_cast<OGRGeoPackageTableLayer *>(
                    GetLayerByName(osMappingTableName)))
        {
            if (poLayer->GetLayerDefn()->GetFieldIndex("base_id") < 0)
            {
                failureReason =
                    ("Field base_id must exist in " + osMappingTableName)
                        .c_str();
                return false;
            }
            if (poLayer->GetLayerDefn()->GetFieldIndex("related_id") < 0)
            {
                failureReason =
                    ("Field related_id must exist in " + osMappingTableName)
                        .c_str();
                return false;
            }
        }
        else
        {
            failureReason =
                ("Could not retrieve table " + osMappingTableName).c_str();
            return false;
        }
    }

    char *pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name,column_name,extension_name,definition,scope) "
        "VALUES ('%q', NULL, 'gpkg_related_tables', "
        "'http://www.geopackage.org/18-000.html', "
        "'read-write')",
        osMappingTableName.c_str());
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (eErr != OGRERR_NONE)
    {
        failureReason = ("Could not insert mapping table " +
                         osMappingTableName + " into gpkg_extensions")
                            .c_str();
        return false;
    }

    pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkgext_relations "
        "(base_table_name,base_primary_column,related_table_name,related_"
        "primary_column,relation_name,mapping_table_name) "
        "VALUES ('%q', '%q', '%q', '%q', '%q', '%q')",
        osLeftTableName.c_str(), aosLeftTableFields[0].c_str(),
        osRightTableName.c_str(), aosRightTableFields[0].c_str(),
        osRelatedTableType.c_str(), osMappingTableName.c_str());
    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (eErr != OGRERR_NONE)
    {
        failureReason = "Could not insert relationship into gpkgext_relations";
        return false;
    }

    ClearCachedRelationships();
    LoadRelationships();
    return true;
}

/************************************************************************/
/*                         DeleteRelationship()                         */
/************************************************************************/

bool GDALGeoPackageDataset::DeleteRelationship(const std::string &name,
                                               std::string &failureReason)
{
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DeleteRelationship() not supported on read-only dataset");
        return false;
    }

    // ensure relationships are up to date before we try to remove one
    ClearCachedRelationships();
    LoadRelationships();

    std::string osMappingTableName;
    {
        const GDALRelationship *poRelationship = GetRelationship(name);
        if (poRelationship == nullptr)
        {
            failureReason = "Could not find relationship with name " + name;
            return false;
        }

        osMappingTableName = poRelationship->GetMappingTableName();
    }

    // DeleteLayerCommon will delete existing relationship objects, so we can't
    // refer to poRelationship or any of its members previously obtained here
    if (DeleteLayerCommon(osMappingTableName.c_str()) != OGRERR_NONE)
    {
        failureReason =
            "Could not remove mapping layer name " + osMappingTableName;

        // relationships may have been left in an inconsistent state -- reload
        // them now
        ClearCachedRelationships();
        LoadRelationships();
        return false;
    }

    ClearCachedRelationships();
    LoadRelationships();
    return true;
}

/************************************************************************/
/*                        UpdateRelationship()                          */
/************************************************************************/

bool GDALGeoPackageDataset::UpdateRelationship(
    std::unique_ptr<GDALRelationship> &&relationship,
    std::string &failureReason)
{
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "UpdateRelationship() not supported on read-only dataset");
        return false;
    }

    // ensure relationships are up to date before we try to update one
    ClearCachedRelationships();
    LoadRelationships();

    const std::string &osRelationshipName = relationship->GetName();
    const std::string &osLeftTableName = relationship->GetLeftTableName();
    const std::string &osRightTableName = relationship->GetRightTableName();
    const std::string &osMappingTableName = relationship->GetMappingTableName();
    const auto &aosLeftTableFields = relationship->GetLeftTableFields();
    const auto &aosRightTableFields = relationship->GetRightTableFields();

    // sanity checks
    {
        const GDALRelationship *poExistingRelationship =
            GetRelationship(osRelationshipName);
        if (poExistingRelationship == nullptr)
        {
            failureReason =
                "The relationship should already exist to be updated";
            return false;
        }

        if (!ValidateRelationship(relationship.get(), failureReason))
        {
            return false;
        }

        // we don't permit changes to the participating tables
        if (osLeftTableName != poExistingRelationship->GetLeftTableName())
        {
            failureReason = ("Cannot change base table from " +
                             poExistingRelationship->GetLeftTableName() +
                             " to " + osLeftTableName)
                                .c_str();
            return false;
        }
        if (osRightTableName != poExistingRelationship->GetRightTableName())
        {
            failureReason = ("Cannot change related table from " +
                             poExistingRelationship->GetRightTableName() +
                             " to " + osRightTableName)
                                .c_str();
            return false;
        }
        if (osMappingTableName != poExistingRelationship->GetMappingTableName())
        {
            failureReason = ("Cannot change mapping table from " +
                             poExistingRelationship->GetMappingTableName() +
                             " to " + osMappingTableName)
                                .c_str();
            return false;
        }
    }

    std::string osRelatedTableType = relationship->GetRelatedTableType();
    if (osRelatedTableType.empty())
    {
        osRelatedTableType = "features";
    }

    char *pszSQL = sqlite3_mprintf(
        "DELETE FROM gpkgext_relations WHERE mapping_table_name='%q'",
        osMappingTableName.c_str());
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (eErr != OGRERR_NONE)
    {
        failureReason =
            "Could not delete old relationship from gpkgext_relations";
        return false;
    }

    pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkgext_relations "
        "(base_table_name,base_primary_column,related_table_name,related_"
        "primary_column,relation_name,mapping_table_name) "
        "VALUES ('%q', '%q', '%q', '%q', '%q', '%q')",
        osLeftTableName.c_str(), aosLeftTableFields[0].c_str(),
        osRightTableName.c_str(), aosRightTableFields[0].c_str(),
        osRelatedTableType.c_str(), osMappingTableName.c_str());
    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (eErr != OGRERR_NONE)
    {
        failureReason =
            "Could not insert updated relationship into gpkgext_relations";
        return false;
    }

    ClearCachedRelationships();
    LoadRelationships();
    return true;
}

/************************************************************************/
/*                    GetSqliteMasterContent()                          */
/************************************************************************/

const std::vector<SQLSqliteMasterContent> &
GDALGeoPackageDataset::GetSqliteMasterContent()
{
    if (m_aoSqliteMasterContent.empty())
    {
        auto oResultTable =
            SQLQuery(hDB, "SELECT sql, type, tbl_name FROM sqlite_master");
        if (oResultTable)
        {
            for (int rowCnt = 0; rowCnt < oResultTable->RowCount(); ++rowCnt)
            {
                SQLSqliteMasterContent row;
                const char *pszSQL = oResultTable->GetValue(0, rowCnt);
                row.osSQL = pszSQL ? pszSQL : "";
                const char *pszType = oResultTable->GetValue(1, rowCnt);
                row.osType = pszType ? pszType : "";
                const char *pszTableName = oResultTable->GetValue(2, rowCnt);
                row.osTableName = pszTableName ? pszTableName : "";
                m_aoSqliteMasterContent.emplace_back(std::move(row));
            }
        }
    }
    return m_aoSqliteMasterContent;
}

/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
 * Copyright (c) 2026, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_pmtiles.h"

#include "cpl_json.h"

#include "mvtutils.h"

#include <algorithm>
#include <math.h>

/************************************************************************/
/*                         ~OGRPMTilesDataset()                         */
/************************************************************************/

OGRPMTilesDataset::~OGRPMTilesDataset()
{
    if (!m_osMetadataFilename.empty())
        VSIUnlink(m_osMetadataFilename.c_str());
    if (!m_osTmpGPKGFilename.empty())
        VSIUnlink(m_osTmpGPKGFilename.c_str());
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

const OGRLayer *OGRPMTilesDataset::GetLayer(int iLayer) const

{
    if (iLayer < 0 || iLayer >= GetLayerCount())
        return nullptr;
    return m_apoLayers[iLayer].get();
}

/************************************************************************/
/*                     LongLatToSphericalMercator()                     */
/************************************************************************/

static void LongLatToSphericalMercator(double *x, double *y)
{
    double X = SPHERICAL_RADIUS * (*x) / 180 * M_PI;
    double Y = SPHERICAL_RADIUS * log(tan(M_PI / 4 + 0.5 * (*y) / 180 * M_PI));
    *x = X;
    *y = Y;
}

/************************************************************************/
/*                           GetCompression()                           */
/************************************************************************/

/*static*/ const char *OGRPMTilesDataset::GetCompression(uint8_t nVal)
{
    switch (nVal)
    {
        case pmtiles::COMPRESSION_UNKNOWN:
            return "unknown";
        case pmtiles::COMPRESSION_NONE:
            return "none";
        case pmtiles::COMPRESSION_GZIP:
            return "gzip";
        case pmtiles::COMPRESSION_BROTLI:
            return "brotli";
        case pmtiles::COMPRESSION_ZSTD:
            return "zstd";
        default:
            break;
    }
    return CPLSPrintf("invalid (%d)", nVal);
}

/************************************************************************/
/*                            GetTileType()                             */
/************************************************************************/

/* static */
const char *OGRPMTilesDataset::GetTileType(const pmtiles::headerv3 &sHeader)
{
    switch (sHeader.tile_type)
    {
        case pmtiles::TILETYPE_UNKNOWN:
            return "unknown";
        case pmtiles::TILETYPE_MVT:
            return "MVT";
        case pmtiles::TILETYPE_PNG:
            return "PNG";
        case pmtiles::TILETYPE_JPEG:
            return "JPEG";
        case pmtiles::TILETYPE_WEBP:
            return "WEBP";
        case pmtiles::TILETYPE_AVIF:
            return "AVIF";
        case pmtiles::TILETYPE_MAPLIBRE_VECTOR_TILE:
            return "MAPLIBRE_VECTOR_TILE";
        default:
            break;
    }
    return CPLSPrintf("invalid (%d)", sHeader.tile_type);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRPMTilesDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!poOpenInfo->fpL || poOpenInfo->nHeaderBytes < PMTILES_HEADER_LENGTH)
        return false;

    SetDescription(poOpenInfo->pszFilename);

    // Borrow file handle
    m_poFileUniquePtr.reset(poOpenInfo->fpL);
    poOpenInfo->fpL = nullptr;

    m_poFile = m_poFileUniquePtr.get();

    // Deserizalize header
    std::string osHeader;
    osHeader.assign(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                    PMTILES_HEADER_LENGTH);
    try
    {
        m_sHeader = pmtiles::deserialize_header(osHeader);
    }
    catch (const std::exception &)
    {
        return false;
    }

    // Check tile type
    const bool bAcceptAnyTileType = CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "ACCEPT_ANY_TILE_TYPE", "NO"));
    if (bAcceptAnyTileType)
    {
        // do nothing. Internal use only by /vsipmtiles/
    }
    else if (m_sHeader.tile_type == pmtiles::TILETYPE_MVT)
    {
        if ((poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Tile type %s not handled in raster mode",
                     GetTileType(m_sHeader));
            return false;
        }
    }
    else if (m_sHeader.tile_type == pmtiles::TILETYPE_PNG ||
             m_sHeader.tile_type == pmtiles::TILETYPE_JPEG ||
             m_sHeader.tile_type == pmtiles::TILETYPE_WEBP ||
             m_sHeader.tile_type == pmtiles::TILETYPE_AVIF)
    {
        if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Tile type %s not handled in vector mode",
                     GetTileType(m_sHeader));
            return false;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Tile type %s not handled by the driver",
                 GetTileType(m_sHeader));
        return false;
    }

    // Check compression method for metadata and directories
    CPLDebugOnly("PMTiles", "internal_compression = %s",
                 GetCompression(m_sHeader.internal_compression));

    if (m_sHeader.internal_compression == pmtiles::COMPRESSION_GZIP)
    {
        m_psInternalDecompressor = CPLGetDecompressor("gzip");
    }
    else if (m_sHeader.internal_compression == pmtiles::COMPRESSION_ZSTD)
    {
        m_psInternalDecompressor = CPLGetDecompressor("zstd");
        if (m_psInternalDecompressor == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "File %s requires ZSTD decompression, but not available "
                     "in this GDAL build",
                     poOpenInfo->pszFilename);
            return false;
        }
    }
    else if (m_sHeader.internal_compression != pmtiles::COMPRESSION_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled internal_compression = %s",
                 GetCompression(m_sHeader.internal_compression));
        return false;
    }

    // Check compression for tile data
    if (!CPLTestBool(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                          "DECOMPRESS_TILES", "YES")))
    {
        // do nothing. Internal use only by /vsipmtiles/
    }
    else
    {
        CPLDebugOnly("PMTiles", "tile_compression = %s",
                     GetCompression(m_sHeader.tile_compression));

        if (m_sHeader.tile_compression == pmtiles::COMPRESSION_UNKNOWN)
        {
            // Python pmtiles-convert generates this. The MVT driver can autodetect
            // uncompressed and GZip-compressed tiles automatically.
        }
        else if (m_sHeader.tile_compression == pmtiles::COMPRESSION_GZIP)
        {
            m_psTileDataDecompressor = CPLGetDecompressor("gzip");
        }
        else if (m_sHeader.tile_compression == pmtiles::COMPRESSION_ZSTD)
        {
            m_psTileDataDecompressor = CPLGetDecompressor("zstd");
            if (m_psTileDataDecompressor == nullptr)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "File %s requires ZSTD decompression, but not available "
                    "in this GDAL build",
                    poOpenInfo->pszFilename);
                return false;
            }
        }
        else if (m_sHeader.tile_compression != pmtiles::COMPRESSION_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unhandled tile_compression = %s",
                     GetCompression(m_sHeader.tile_compression));
            return false;
        }
    }

    // Read metadata
    const auto *posMetadata =
        ReadInternal(m_sHeader.json_metadata_offset,
                     m_sHeader.json_metadata_bytes, "metadata");
    if (!posMetadata)
        return false;
    CPLDebugOnly("PMTiles", "Metadata = %s", posMetadata->c_str());
    m_osMetadata = *posMetadata;

    m_osMetadataFilename =
        VSIMemGenerateHiddenFilename("pmtiles_metadata.json");
    VSIFCloseL(VSIFileFromMemBuffer(m_osMetadataFilename.c_str(),
                                    reinterpret_cast<GByte *>(&m_osMetadata[0]),
                                    m_osMetadata.size(), false));

    CPLJSONDocument oJsonDoc;
    if (!oJsonDoc.LoadMemory(m_osMetadata))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse metadata");
        return false;
    }

    auto oJsonRoot = oJsonDoc.GetRoot();
    for (const auto &oChild : oJsonRoot.GetChildren())
    {
        if (oChild.GetType() == CPLJSONObject::Type::String)
        {
            if (oChild.GetName() == "json")
            {
                // Tippecanoe metadata includes a "json" item, which is a
                // serialized JSON object with vector_layers[] and layers[]
                // arrays we are interested in later.
                // so use "json" content as the new root
                if (!oJsonDoc.LoadMemory(oChild.ToString()))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot parse 'json' metadata item");
                    return false;
                }
                oJsonRoot = oJsonDoc.GetRoot();
            }
            // Tippecanoe generates a "strategies" member with serialized JSON
            else if (oChild.GetName() != "strategies")
            {
                SetMetadataItem(oChild.GetName().c_str(),
                                oChild.ToString().c_str());
            }
        }
    }

    m_nMinZoomLevel = m_sHeader.min_zoom;
    m_nMaxZoomLevel = m_sHeader.max_zoom;
    if (m_nMinZoomLevel > m_nMaxZoomLevel)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "min_zoom(=%d) > max_zoom(=%d)",
                 m_nMinZoomLevel, m_nMaxZoomLevel);
        return false;
    }
    if (m_nMinZoomLevel > 30)
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Clamping min_zoom from %d to %d",
                 m_nMinZoomLevel, 30);
        m_nMinZoomLevel = 30;
    }
    if (m_nMaxZoomLevel > 30)
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Clamping max_zoom from %d to %d",
                 m_nMaxZoomLevel, 30);
        m_nMaxZoomLevel = 30;
    }

    if (bAcceptAnyTileType)
        return true;

    const int nZoomLevel =
        atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ZOOM_LEVEL",
                                  CPLSPrintf("%d", m_nMaxZoomLevel)));
    if (nZoomLevel < m_nMinZoomLevel || nZoomLevel > m_nMaxZoomLevel)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid zoom level. Should be in [%d,%d] range",
                 m_nMinZoomLevel, m_nMaxZoomLevel);
        return false;
    }
    SetMetadataItem("ZOOM_LEVEL", CPLSPrintf("%d", nZoomLevel));

    m_osClipOpenOption =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "CLIP", "");

    if (m_sHeader.tile_type == pmtiles::TILETYPE_MVT)
        return OpenVector(poOpenInfo, oJsonRoot, nZoomLevel);
    else
        return OpenRaster(nZoomLevel);
}

/************************************************************************/
/*                             OpenVector()                             */
/************************************************************************/

bool OGRPMTilesDataset::OpenVector(const GDALOpenInfo *poOpenInfo,
                                   const CPLJSONObject &oJsonRoot,
                                   int nZoomLevel)
{
    // If using the pmtiles go utility, vector_layers and tilestats are
    // moved from Tippecanoe's json metadata item to the root element.
    CPLJSONArray oVectorLayers = oJsonRoot.GetArray("vector_layers");
    if (oVectorLayers.Size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing vector_layers[] metadata");
        return false;
    }

    CPLJSONArray oTileStatLayers = oJsonRoot.GetArray("tilestats/layers");

    const bool bZoomLevelFromSpatialFilter = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "ZOOM_LEVEL_AUTO",
        CPLTestBool(CPLGetConfigOption("MVT_ZOOM_LEVEL_AUTO", "NO")));
    const bool bJsonField =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "JSON_FIELD", false);

    double dfMinX = m_sHeader.min_lon_e7 / 10e6;
    double dfMinY = m_sHeader.min_lat_e7 / 10e6;
    double dfMaxX = m_sHeader.max_lon_e7 / 10e6;
    double dfMaxY = m_sHeader.max_lat_e7 / 10e6;
    LongLatToSphericalMercator(&dfMinX, &dfMinY);
    LongLatToSphericalMercator(&dfMaxX, &dfMaxY);

    for (int i = 0; i < oVectorLayers.Size(); i++)
    {
        CPLJSONObject oId = oVectorLayers[i].GetObj("id");
        if (oId.IsValid() && oId.GetType() == CPLJSONObject::Type::String)
        {
            OGRwkbGeometryType eGeomType = wkbUnknown;
            if (oTileStatLayers.IsValid())
            {
                eGeomType = OGRMVTFindGeomTypeFromTileStat(
                    oTileStatLayers, oId.ToString().c_str());
            }
            if (eGeomType == wkbUnknown)
            {
                eGeomType = OGRPMTilesVectorLayer::GuessGeometryType(
                    this, oId.ToString().c_str(), nZoomLevel);
            }

            CPLJSONObject oFields = oVectorLayers[i].GetObj("fields");
            CPLJSONArray oAttributesFromTileStats =
                OGRMVTFindAttributesFromTileStat(oTileStatLayers,
                                                 oId.ToString().c_str());

            m_apoLayers.push_back(std::make_unique<OGRPMTilesVectorLayer>(
                this, oId.ToString().c_str(), oFields, oAttributesFromTileStats,
                bJsonField, dfMinX, dfMinY, dfMaxX, dfMaxY, eGeomType,
                nZoomLevel, bZoomLevelFromSpatialFilter));
        }
    }

    return true;
}

/************************************************************************/
/*                             OpenRaster()                             */
/************************************************************************/

bool OGRPMTilesDataset::OpenRaster(int nZoomLevel)
{
    m_nZoomLevel = nZoomLevel;

    m_oSRS.importFromEPSG(3857);

    // Open a tile to get its dimension in pixel (to compute the resolution)
    OGRPMTilesTileIterator oIter(this, nZoomLevel);
    auto sTile = oIter.GetNextTile();
    if (sTile.offset == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File does not contain any tile at zoom level %d", nZoomLevel);
        return false;
    }

    const auto *posStr = ReadTileData(sTile.offset, sTile.length);
    if (!posStr)
    {
        // Error message emitted by ReadTileData()
        return false;
    }
    const auto &osTileData = *posStr;

    auto poDM = GetGDALDriverManager();
    if (!poDM->GetDriverByName(GetTileType(m_sHeader)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s driver needed for inner working of PMTiles driver is not "
                 "available",
                 GetTileType(m_sHeader));
        return false;
    }
    m_poGPKGDriver = poDM->GetDriverByName("GPKG");
    if (!m_poGPKGDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GeoPackage driver needed for inner working of PMTiles driver "
                 "is not available");
        return false;
    }
    if (!poDM->GetDriverByName("GTI"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GTI driver needed for inner working of PMTiles driver is not "
                 "available");
        return false;
    }
    const std::string osTmpFilename = VSIMemGenerateHiddenFilename(
        CPLSPrintf("pmtiles_%u_%u_tile", sTile.x, sTile.y));
    VSIFCloseL(VSIFileFromMemBuffer(
        osTmpFilename.c_str(),
        reinterpret_cast<GByte *>(const_cast<char *>(osTileData.data())),
        osTileData.size(), false));

    const char *const apszAllowedDrivers[] = {GetTileType(m_sHeader), nullptr};
    auto poTileDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        osTmpFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
        apszAllowedDrivers));
    const int nTileSize = poTileDS ? poTileDS->GetRasterXSize() : 0;
    if (nTileSize == 0)
        return false;

    const double dfRes = 2 * MAX_GM / (1 << nZoomLevel) / nTileSize;
    constexpr double EPSILON = 1e-2;

    // Compute the raster georeferenced extent from the bounding box in the
    // PMTiles metadata
    double dfMinX = m_sHeader.min_lon_e7 / 10e6;
    double dfMinY = m_sHeader.min_lat_e7 / 10e6;
    double dfMaxX = m_sHeader.max_lon_e7 / 10e6;
    double dfMaxY = m_sHeader.max_lat_e7 / 10e6;
    LongLatToSphericalMercator(&dfMinX, &dfMinY);
    LongLatToSphericalMercator(&dfMaxX, &dfMaxY);

    // Align with the resolution at the zoom level of interest, to avoid
    // resampling due to sub-pixel shifts
    dfMinX = std::floor(dfMinX / dfRes + EPSILON) * dfRes;
    dfMinY = std::floor(dfMinY / dfRes + EPSILON) * dfRes;
    dfMaxX = std::ceil(dfMaxX / dfRes - EPSILON) * dfRes;
    dfMaxY = std::ceil(dfMaxY / dfRes - EPSILON) * dfRes;

    // Compute the geotransform
    m_gt.xorig = dfMinX;
    m_gt.xscale = dfRes;
    m_gt.yorig = dfMaxY;
    m_gt.yscale = -dfRes;

    // Compute the raster dimension
    const double dfXSize = (dfMaxX - dfMinX) / dfRes;
    const double dfYSize = (dfMaxY - dfMinY) / dfRes;
    if (dfXSize > INT_MAX || dfYSize > INT_MAX)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too large zoom level %d compared to dataset extent",
                 m_nZoomLevel);
        return false;
    }
    nRasterXSize = std::max(1, static_cast<int>(dfXSize + EPSILON));
    nRasterYSize = std::max(1, static_cast<int>(dfYSize + EPSILON));

    // Create bands
    const int nTargetBands = poTileDS->GetRasterBand(1)->GetColorTable()
                                 ? 3
                                 : poTileDS->GetRasterCount();
    for (int i = 0; i < nTargetBands; ++i)
        SetBand(i + 1, std::make_unique<GDALPMTilesRasterBand>(this, i + 1,
                                                               nTileSize));

    m_osTmpGPKGFilename = VSIMemGenerateHiddenFilename("pmtiles.gti.gpkg");

    if (nTargetBands > 1)
        SetMetadataItem("INTERLEAVING", "PIXEL", "IMAGE_STRUCTURE");

    // Create overview datasets
    for (int iOvr = 0; iOvr < m_nZoomLevel - m_nMinZoomLevel; ++iOvr)
    {
        auto poOvrDS = std::make_unique<OGRPMTilesDataset>();
        poOvrDS->SetDescription(GetDescription());
        poOvrDS->m_psInternalDecompressor = m_psInternalDecompressor;
        poOvrDS->m_psTileDataDecompressor = m_psTileDataDecompressor;
        poOvrDS->m_poFile = m_poFile;
        poOvrDS->m_nZoomLevel = m_nZoomLevel - 1 - iOvr;
        poOvrDS->m_osTmpGPKGFilename = m_osTmpGPKGFilename;
        poOvrDS->m_poGPKGDriver = m_poGPKGDriver;
        poOvrDS->m_sHeader = m_sHeader;
        poOvrDS->m_oSRS = m_oSRS;
        poOvrDS->m_gt = m_gt;
        poOvrDS->m_gt.xscale *= (1 << (iOvr + 1));
        poOvrDS->m_gt.yscale *= (1 << (iOvr + 1));
        poOvrDS->nRasterXSize = std::max(
            1,
            static_cast<int>((dfMaxX - dfMinX) / poOvrDS->m_gt.xscale + 0.5));
        poOvrDS->nRasterYSize = std::max(
            1,
            static_cast<int>((dfMaxY - dfMinY) / -poOvrDS->m_gt.yscale + 0.5));
        poOvrDS->m_gt.xscale = (dfMaxX - dfMinX) / poOvrDS->nRasterXSize;
        poOvrDS->m_gt.yscale = -(dfMaxY - dfMinY) / poOvrDS->nRasterYSize;

        for (int i = 0; i < nTargetBands; ++i)
            poOvrDS->SetBand(i + 1, std::make_unique<GDALPMTilesRasterBand>(
                                        poOvrDS.get(), i + 1, nTileSize));

        if (nTargetBands > 1)
            poOvrDS->SetMetadataItem("INTERLEAVING", "PIXEL",
                                     "IMAGE_STRUCTURE");

        m_apoOverviews.push_back(std::move(poOvrDS));
    }

    return true;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr OGRPMTilesDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
    GSpacing nLineSpace, GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)
{

    if (nBufXSize < nXSize && nBufYSize < nYSize && AreOverviewsEnabled())
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    // Compute the georeferenced coordinates of the window of interest
    // defined by (nXOff, nYOff, nXSize, nYSize) (or the floating point
    // coordinates)
    const double dfXOff =
        psExtraArg->bFloatingPointWindowValidity ? psExtraArg->dfXOff : nXOff;
    const double dfYOff =
        psExtraArg->bFloatingPointWindowValidity ? psExtraArg->dfYOff : nYOff;
    const double dfXSize =
        psExtraArg->bFloatingPointWindowValidity ? psExtraArg->dfXSize : nXSize;
    const double dfYSize =
        psExtraArg->bFloatingPointWindowValidity ? psExtraArg->dfYSize : nYSize;
    const double dfMinX = m_gt.xorig + dfXOff * m_gt.xscale;
    const double dfMaxY = m_gt.yorig + dfYOff * m_gt.yscale;
    const double dfMaxX = m_gt.xorig + (dfXOff + dfXSize) * m_gt.xscale;
    const double dfMinY = m_gt.yorig + (dfYOff + dfYSize) * m_gt.yscale;
    const double dfTileDim = 2 * MAX_GM / (1 << m_nZoomLevel);

    // Compute the minimum and maximum tile indices covering the window
    // of interest
    constexpr double EPSILON = 1e-5;
    const int nTileMinX = std::max(
        0, static_cast<int>(floor((dfMinX + MAX_GM) / dfTileDim + EPSILON)));
    // PMTiles uses a Y=MAX_GM as the y=0 tile
    const int nTileMinY = std::max(
        0, static_cast<int>(floor((MAX_GM - dfMaxY) / dfTileDim + EPSILON)));
    const int nTileMaxX = std::min(
        static_cast<int>(floor((dfMaxX + MAX_GM) / dfTileDim + EPSILON)),
        (1 << m_nZoomLevel) - 1);
    const int nTileMaxY = std::min(
        static_cast<int>(floor((MAX_GM - dfMinY) / dfTileDim + EPSILON)),
        (1 << m_nZoomLevel) - 1);

    // Create a GTI GeoPackage file with the tiles that intersect the window
    // of interest.
    VSIUnlink(m_osTmpGPKGFilename.c_str());
    auto poGPKGDS = std::unique_ptr<GDALDataset>(m_poGPKGDriver->Create(
        m_osTmpGPKGFilename.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    auto poLayer = poGPKGDS ? poGPKGDS->CreateLayer("index") : nullptr;
    if (!poLayer)
    {
        return CE_Failure;
    }
    OGRFieldDefn oFieldDefn("location", OFTString);
    CPL_IGNORE_RET_VAL(poLayer->CreateField(&oFieldDefn));

    // Add layer-level metadata items recognized by the GTI driver.
    const double dfMosaicMinX = -MAX_GM + nTileMinX * dfTileDim;
    const double dfMosaicMinY = MAX_GM - (nTileMaxY + 1) * dfTileDim;
    const double dfMosaicMaxX = -MAX_GM + (nTileMaxX + 1) * dfTileDim;
    const double dfMosaicMaxY = MAX_GM - nTileMinY * dfTileDim;
    poLayer->SetMetadataItem("RESX", CPLSPrintf("%.17g", m_gt.xscale));
    poLayer->SetMetadataItem("RESY", CPLSPrintf("%.17g", -m_gt.yscale));
    poLayer->SetMetadataItem("MINX", CPLSPrintf("%.17g", dfMosaicMinX));
    poLayer->SetMetadataItem("MINY", CPLSPrintf("%.17g", dfMosaicMinY));
    poLayer->SetMetadataItem("MAXX", CPLSPrintf("%.17g", dfMosaicMaxX));
    poLayer->SetMetadataItem("MAXY", CPLSPrintf("%.17g", dfMosaicMaxY));
    poLayer->SetMetadataItem("DATA_TYPE", "UInt8");
    poLayer->SetMetadataItem("BAND_COUNT", CPLSPrintf("%d", nBands));
    if (nBands <= 4)
    {
        poLayer->SetMetadataItem("COLOR_INTERPRETATION",
                                 nBands == 1   ? "gray"
                                 : nBands == 2 ? "gray,alpha"
                                 : nBands == 3 ? "red,green,blue"
                                               : "red,green,blue,alpha");
    }

    poGPKGDS->StartTransaction();
    OGRPMTilesTileIterator oTileIter(this, m_nZoomLevel, nTileMinX, nTileMinY,
                                     nTileMaxX, nTileMaxY);
    while (true)
    {
        const auto sTile = oTileIter.GetNextTile();
        if (sTile.offset == 0)
        {
            break;
        }

        OGRFeature oFeature(poLayer->GetLayerDefn());
        oFeature.SetField(0, CPLSPrintf("/vsipmtiles/%s/%d/%d/%d%s",
                                        GetDescription(), m_nZoomLevel, sTile.x,
                                        sTile.y,
                                        VSIPMTilesGetTileExtension(this)));
        auto poLR = std::make_unique<OGRLinearRing>();
        const double dfTileMinX = -MAX_GM + sTile.x * dfTileDim;
        const double dfTileMaxX = -MAX_GM + (sTile.x + 1) * dfTileDim;
        const double dfTileMaxY = MAX_GM - sTile.y * dfTileDim;
        const double dfTileMinY = MAX_GM - (sTile.y + 1) * dfTileDim;
        poLR->addPoint(dfTileMinX, dfTileMinY);
        poLR->addPoint(dfTileMinX, dfTileMaxY);
        poLR->addPoint(dfTileMaxX, dfTileMaxY);
        poLR->addPoint(dfTileMaxX, dfTileMinY);
        poLR->addPoint(dfTileMinX, dfTileMinY);
        auto poPoly = std::make_unique<OGRPolygon>();
        poPoly->addRing(std::move(poLR));
        oFeature.SetGeometry(std::move(poPoly));
        CPL_IGNORE_RET_VAL(poLayer->CreateFeature(&oFeature));
    }
    poGPKGDS->CommitTransaction();
    poGPKGDS.reset();

    // Open the GTI GeoPackage file has a raster
    const char *const apszAllowedDriversGTI[] = {"GTI", nullptr};
    CPLStringList aosOpenOptionsGTI;
    aosOpenOptionsGTI.SetNameValue("ALLOWED_RASTER_DRIVERS",
                                   GetTileType(m_sHeader));
    auto poGTIDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        m_osTmpGPKGFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
        apszAllowedDriversGTI, aosOpenOptionsGTI.List()));
    if (!poGTIDS)
    {
        return CE_Failure;
    }

    // Compute the window of interest relative to the origin of the GTI dataset
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.eResampleAlg = psExtraArg->eResampleAlg;
    sExtraArg.bFloatingPointWindowValidity =
        psExtraArg->bFloatingPointWindowValidity;
    sExtraArg.dfXOff = (dfMinX - dfMosaicMinX) / m_gt.xscale;
    sExtraArg.dfYOff = (dfMosaicMaxY - dfMaxY) / -m_gt.yscale;
    sExtraArg.dfXSize = psExtraArg->dfXSize;
    sExtraArg.dfYSize = psExtraArg->dfYSize;

    const int nGTIXOff = static_cast<int>(sExtraArg.dfXOff + EPSILON);
    const int nGTIYOff = static_cast<int>(sExtraArg.dfYOff + EPSILON);

    // Redirect the pixel request to the GTI dataset
    return poGTIDS->RasterIO(GF_Read, nGTIXOff, nGTIYOff, nXSize, nYSize, pData,
                             nBufXSize, nBufYSize, eBufType, nBandCount,
                             panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                             &sExtraArg);
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

const std::string *OGRPMTilesDataset::Read(const CPLCompressor *psDecompressor,
                                           uint64_t nOffset, uint64_t nSize,
                                           const char *pszDataType)
{
    if (nSize > 10 * 1024 * 1024)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too large amount of %s to read: " CPL_FRMT_GUIB
                 " bytes at offset " CPL_FRMT_GUIB,
                 pszDataType, static_cast<GUIntBig>(nSize),
                 static_cast<GUIntBig>(nOffset));
        return nullptr;
    }
    m_osBuffer.resize(static_cast<size_t>(nSize));
    if (m_poFile->Seek(nOffset, SEEK_SET) != 0 ||
        m_poFile->Read(&m_osBuffer[0], m_osBuffer.size(), 1) != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read %s of length %u at offset " CPL_FRMT_GUIB,
                 pszDataType, unsigned(nSize), static_cast<GUIntBig>(nOffset));
        return nullptr;
    }

    if (psDecompressor)
    {
        m_osDecompressedBuffer.resize(32 + 16 * m_osBuffer.size());
        for (int iTry = 0; iTry < 2; ++iTry)
        {
            void *pOutputData = &m_osDecompressedBuffer[0];
            size_t nOutputSize = m_osDecompressedBuffer.size();
            if (!psDecompressor->pfnFunc(m_osBuffer.data(), m_osBuffer.size(),
                                         &pOutputData, &nOutputSize, nullptr,
                                         psDecompressor->user_data))
            {
                if (iTry == 0)
                {
                    pOutputData = nullptr;
                    nOutputSize = 0;
                    if (psDecompressor->pfnFunc(
                            m_osBuffer.data(), m_osBuffer.size(), &pOutputData,
                            &nOutputSize, nullptr, psDecompressor->user_data))
                    {
                        CPLDebug("PMTiles",
                                 "Buffer of size %u uncompresses to %u bytes",
                                 unsigned(nSize), unsigned(nOutputSize));
                        m_osDecompressedBuffer.resize(nOutputSize);
                        continue;
                    }
                }

                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot decompress %s of length %u at "
                         "offset " CPL_FRMT_GUIB,
                         pszDataType, unsigned(nSize),
                         static_cast<GUIntBig>(nOffset));
                return nullptr;
            }
            m_osDecompressedBuffer.resize(nOutputSize);
            break;
        }
        return &m_osDecompressedBuffer;
    }
    else
    {
        return &m_osBuffer;
    }
}

/************************************************************************/
/*                            ReadInternal()                            */
/************************************************************************/

const std::string *OGRPMTilesDataset::ReadInternal(uint64_t nOffset,
                                                   uint64_t nSize,
                                                   const char *pszDataType)
{
    return Read(m_psInternalDecompressor, nOffset, nSize, pszDataType);
}

/************************************************************************/
/*                            ReadTileData()                            */
/************************************************************************/

const std::string *OGRPMTilesDataset::ReadTileData(uint64_t nOffset,
                                                   uint64_t nSize)
{
    return Read(m_psTileDataDecompressor, nOffset, nSize, "tile data");
}

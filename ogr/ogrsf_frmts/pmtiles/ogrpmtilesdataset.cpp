/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
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

#include "ogr_pmtiles.h"

#include "cpl_json.h"

#include "mvtutils.h"

#include <math.h>

/************************************************************************/
/*                       ~OGRPMTilesDataset()                           */
/************************************************************************/

OGRPMTilesDataset::~OGRPMTilesDataset()
{
    if (!m_osMetadataFilename.empty())
        VSIUnlink(m_osMetadataFilename.c_str());
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPMTilesDataset::GetLayer(int iLayer)

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
/*                            GetCompression()                          */
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
/*                           GetTileType()                              */
/************************************************************************/

/* static */
const char *OGRPMTilesDataset::GetTileType(const pmtiles::headerv3 &sHeader)
{
    switch (sHeader.tile_type)
    {
        case pmtiles::TILETYPE_UNKNOWN:
            return "unknown";
        case pmtiles::TILETYPE_PNG:
            return "PNG";
        case pmtiles::TILETYPE_JPEG:
            return "JPEG";
        case pmtiles::TILETYPE_WEBP:
            return "WEBP";
        case pmtiles::TILETYPE_MVT:
            return "MVT";
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
    if (!poOpenInfo->fpL || poOpenInfo->nHeaderBytes < 127)
        return false;

    SetDescription(poOpenInfo->pszFilename);

    // Borrow file handle
    m_poFile.reset(poOpenInfo->fpL);
    poOpenInfo->fpL = nullptr;

    // Deserizalize header
    std::string osHeader;
    osHeader.assign(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                    127);
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
    else if (m_sHeader.tile_type != pmtiles::TILETYPE_MVT)
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

    m_osMetadataFilename = CPLSPrintf("/vsimem/pmtiles/metadata_%p.json", this);
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

    double dfMinX = m_sHeader.min_lon_e7 / 10e6;
    double dfMinY = m_sHeader.min_lat_e7 / 10e6;
    double dfMaxX = m_sHeader.max_lon_e7 / 10e6;
    double dfMaxY = m_sHeader.max_lat_e7 / 10e6;
    LongLatToSphericalMercator(&dfMinX, &dfMinY);
    LongLatToSphericalMercator(&dfMaxX, &dfMaxY);

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

    const bool bZoomLevelFromSpatialFilter = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "ZOOM_LEVEL_AUTO",
        CPLTestBool(CPLGetConfigOption("MVT_ZOOM_LEVEL_AUTO", "NO")));
    const bool bJsonField =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "JSON_FIELD", false);

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
/*                              Read()                                  */
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
    m_poFile->Seek(nOffset, SEEK_SET);
    if (m_poFile->Read(&m_osBuffer[0], m_osBuffer.size(), 1) != 1)
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
/*                              ReadInternal()                          */
/************************************************************************/

const std::string *OGRPMTilesDataset::ReadInternal(uint64_t nOffset,
                                                   uint64_t nSize,
                                                   const char *pszDataType)
{
    return Read(m_psInternalDecompressor, nOffset, nSize, pszDataType);
}

/************************************************************************/
/*                              ReadTileData()                          */
/************************************************************************/

const std::string *OGRPMTilesDataset::ReadTileData(uint64_t nOffset,
                                                   uint64_t nSize)
{
    return Read(m_psTileDataDecompressor, nOffset, nSize, "tile data");
}

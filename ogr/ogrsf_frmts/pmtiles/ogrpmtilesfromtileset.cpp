/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrpmtilesfromtileset.h"
#include "ogr_pmtiles.h"

#include "cpl_json.h"
#include "cpl_md5.h"
#include "cpl_vsi_virtual.h"
#include "gdal_priv.h"

#include "include_pmtiles.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>

/************************************************************************/
/*            OGRPMTilesConvertFromTilesetInitializeHeader()            */
/************************************************************************/

static void OGRPMTilesConvertFromTilesetInitializeHeader(
    GDALDataset *poSrcDS, CSLConstList papszOptions, int nMinZoom, int nMaxZoom,
    pmtiles::headerv3 &sHeader, const char *&pszExt, std::string &osMetadata)
{
    OGREnvelope sExtent;
    poSrcDS->GetExtentWGS84LongLat(&sExtent);

    constexpr double MAX_LAT = 85.0511287798066;
    sExtent.MinY = std::max(-MAX_LAT, sExtent.MinY);
    sExtent.MaxY = std::min(MAX_LAT, sExtent.MaxY);

    CPLJSONObject oObj;
    const char *pszTileFormat =
        CSLFetchNameValueDef(papszOptions, "TILE_FORMAT", "PNG");
    const char *pszVersion = CSLFetchNameValueDef(
        papszOptions, "VERSION", EQUAL(pszTileFormat, "WEBP") ? "1.3" : "1.1");
    oObj.Set("version", pszVersion);
    oObj.Set("name", CSLFetchNameValueDef(papszOptions, "NAME", ""));
    oObj.Set("description",
             CSLFetchNameValueDef(papszOptions, "DESCRIPTION", ""));
    oObj.Set("type", CSLFetchNameValueDef(papszOptions, "TYPE", "overlay"));
    if (const char *pszElevationType =
            CSLFetchNameValue(papszOptions, "ELEVATION_TYPE"))
        oObj.Set("elevation_type", pszElevationType);
    uint8_t tile_type = pmtiles::TILETYPE_UNKNOWN;
    if (EQUAL(pszTileFormat, "PNG"))
    {
        pszExt = "png";
        tile_type = pmtiles::TILETYPE_PNG;
    }
    else if (EQUAL(pszTileFormat, "JPEG"))
    {
        pszExt = "jpg";
        tile_type = pmtiles::TILETYPE_JPEG;
    }
    else if (EQUAL(pszTileFormat, "WEBP"))
    {
        pszExt = "webp";
        tile_type = pmtiles::TILETYPE_WEBP;
    }
    else
        CPLAssert(false);

    oObj.Set("format", pszExt);
    oObj.Set("scheme", "xyz");
    oObj.Set("bounds", CPLSPrintf("%.17g,%.17g,%.17g,%.17g", sExtent.MinX,
                                  sExtent.MinY, sExtent.MaxX, sExtent.MaxY));
    const double dfCenterLong = (sExtent.MinX + sExtent.MaxX) / 2;
    const double dfCenterLat = (sExtent.MinY + sExtent.MaxY) / 2;
    const int nCenterZoom = nMaxZoom;
    oObj.Set("center", CPLSPrintf("%.17g,%.17g,%d", dfCenterLong, dfCenterLat,
                                  nCenterZoom));
    oObj.Set("minzoom", CPLSPrintf("%d", nMinZoom));
    oObj.Set("maxzoom", CPLSPrintf("%d", nMaxZoom));

    CPLJSONDocument oMetadataDoc;
    oMetadataDoc.SetRoot(oObj);
    osMetadata = oMetadataDoc.SaveAsString();
    // CPLDebugOnly("PMTiles", "Metadata = %s", osMetadata.c_str());

    sHeader.root_dir_offset = PMTILES_HEADER_LENGTH;
    sHeader.root_dir_bytes = 0;
    sHeader.json_metadata_offset = 0;
    sHeader.json_metadata_bytes = 0;
    sHeader.leaf_dirs_offset = 0;
    sHeader.leaf_dirs_bytes = 0;
    sHeader.tile_data_offset = 0;
    sHeader.tile_data_bytes = 0;
    sHeader.addressed_tiles_count = 0;
    sHeader.tile_entries_count = 0;
    sHeader.tile_contents_count = 0;
    sHeader.clustered = true;
    sHeader.internal_compression = pmtiles::COMPRESSION_GZIP;
    sHeader.tile_compression = pmtiles::COMPRESSION_NONE;
    sHeader.tile_type = tile_type;
    sHeader.min_zoom = static_cast<uint8_t>(nMinZoom);
    sHeader.max_zoom = static_cast<uint8_t>(nMaxZoom);
    sHeader.min_lon_e7 = static_cast<int32_t>(sExtent.MinX * 10e6);
    sHeader.min_lat_e7 = static_cast<int32_t>(sExtent.MinY * 10e6);
    sHeader.max_lon_e7 = static_cast<int32_t>(sExtent.MaxX * 10e6);
    sHeader.max_lat_e7 = static_cast<int32_t>(sExtent.MaxY * 10e6);
    sHeader.center_zoom = static_cast<uint8_t>(nCenterZoom);
    sHeader.center_lon_e7 = static_cast<int32_t>(dfCenterLong * 10e6);
    sHeader.center_lat_e7 = static_cast<int32_t>(dfCenterLat * 10e6);
}

/************************************************************************/
/*                    OGRPMTilesConvertFromTileset()                    */
/************************************************************************/

bool OGRPMTilesConvertFromTileset(const char *pszDestName,
                                  const char *pszSrcDirectory,
                                  GDALDataset *poSrcDS,
                                  CSLConstList papszOptions)
{
    const CPLStringList aosZoomLevelDirs(VSIReadDir(pszSrcDirectory));
    int nMinZoom = INT_MAX;
    int nMaxZoom = 0;
    for (const char *pszName : cpl::Iterate(aosZoomLevelDirs))
    {
        if (CPLGetValueType(pszName) == CPL_VALUE_INTEGER)
        {
            const int nZoom = atoi(pszName);
            nMinZoom = std::min(nMinZoom, nZoom);
            nMaxZoom = std::max(nMaxZoom, nZoom);
        }
    }
    if (nMinZoom > nMaxZoom)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No valid tile found");
        return false;
    }

    pmtiles::headerv3 sHeader;
    const char *pszExt = "";
    std::string osMetadata;
    OGRPMTilesConvertFromTilesetInitializeHeader(
        poSrcDS, papszOptions, nMinZoom, nMaxZoom, sHeader, pszExt, osMetadata);

    struct TileEntry
    {
        uint64_t nTileId;
        std::array<unsigned char, 16> abyMD5;
    };

    // In a first step browse through the tiles table to compute the PMTiles
    // tile_id of each tile, and compute a hash of the tile data for
    // deduplication
    std::vector<TileEntry> asTileEntries;
    std::vector<GByte> abyTileData;
    std::map<uint64_t, uint32_t> oMapTileIdToFileSize;
    for (int nZoom = nMinZoom; nZoom <= nMaxZoom; ++nZoom)
    {
        const std::string osZoomDir(CPLFormFilenameSafe(
            pszSrcDirectory, CPLSPrintf("%d", nZoom), nullptr));
        std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDirX(
            VSIOpenDir(osZoomDir.c_str(), 0, nullptr), VSICloseDir);
        if (!psDirX)
            return false;
        while (const VSIDIREntry *psXEntry = VSIGetNextDirEntry(psDirX.get()))
        {
            const std::string osXDir(CPLFormFilenameSafe(
                osZoomDir.c_str(), psXEntry->pszName, nullptr));
            const int nX = atoi(psXEntry->pszName);
            std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDirY(
                VSIOpenDir(osXDir.c_str(), 0, nullptr), VSICloseDir);
            if (!psDirY)
                return false;
            while (const VSIDIREntry *psYEntry =
                       VSIGetNextDirEntry(psDirY.get()))
            {
                const int nY = atoi(psYEntry->pszName);
                uint64_t nTileId;
                try
                {
                    nTileId = pmtiles::zxy_to_tileid(
                        static_cast<uint8_t>(nZoom), nX, nY);
                }
                catch (const std::exception &e)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot compute tile id: %s", e.what());
                    return false;
                }

                const std::string osTileFilename(CPLFormFilenameSafe(
                    osXDir.c_str(), psYEntry->pszName, nullptr));

                VSIStatBufL sStatBuf;
                if (VSIStatL(osTileFilename.c_str(), &sStatBuf) != 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot stat file: %s", osTileFilename.c_str());
                    return false;
                }

                // Arbitrary (but must not be larger than UINT32_MAX per PMTiles spec)
                constexpr uint32_t MAX_TILE_SIZE = 100 * 1024 * 1024;
                if (sStatBuf.st_size > MAX_TILE_SIZE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Too large file: %s",
                             osTileFilename.c_str());
                    return false;
                }

                const uint32_t nFileSize =
                    static_cast<uint32_t>(sStatBuf.st_size);

                if (abyTileData.size() < nFileSize)
                    abyTileData.resize(nFileSize);

                auto fp = VSIVirtualHandleUniquePtr(
                    VSIFOpenL(osTileFilename.c_str(), "rb"));
                if (!fp)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s",
                             osTileFilename.c_str());
                    return false;
                }
                if (fp->Read(abyTileData.data(), nFileSize) != nFileSize)
                {
                    return false;
                }

                oMapTileIdToFileSize[nTileId] = nFileSize;

                TileEntry sEntry;
                sEntry.nTileId = nTileId;

                CPLMD5Context md5context;
                CPLMD5Init(&md5context);
                CPLMD5Update(&md5context, abyTileData.data(), nFileSize);
                CPLMD5Final(&sEntry.abyMD5[0], &md5context);
                try
                {
                    asTileEntries.push_back(sEntry);
                }
                catch (const std::exception &e)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Out of memory browsing through tiles: %s",
                             e.what());
                    return false;
                }
            }
        }
    }

    // Sort the tiles by ascending tile_id. This is a requirement to build
    // the PMTiles directories.
    std::sort(asTileEntries.begin(), asTileEntries.end(),
              [](const TileEntry &a, const TileEntry &b)
              { return a.nTileId < b.nTileId; });

    // Let's gather tile data in
    // a way that corresponds to the "clustered" mode, that is
    // "offsets are either contiguous with the previous offset+length, or
    // refer to a lesser offset, when writing with deduplication."

    std::vector<pmtiles::entryv3> asPMTilesEntries;
    uint64_t nFileOffset = 0;
    std::unordered_map<std::array<unsigned char, 16>,
                       std::pair<uint64_t, uint32_t>,
                       HashArray<unsigned char, 16>>
        oMapMD5ToOffsetLen;
    {
        uint64_t nLastTileId = 0;
        std::array<unsigned char, 16> abyLastMD5{0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0};
        for (const auto &sEntry : asTileEntries)
        {
            if (sEntry.nTileId == nLastTileId + 1 &&
                sEntry.abyMD5 == abyLastMD5)
            {
                // If the tile id immediately follows the previous one and
                // has the same tile data, increase the run_length
                asPMTilesEntries.back().run_length++;
            }
            else
            {
                pmtiles::entryv3 sPMTilesEntry;
                sPMTilesEntry.tile_id = sEntry.nTileId;
                sPMTilesEntry.run_length = 1;

                auto oIter = oMapMD5ToOffsetLen.find(sEntry.abyMD5);
                if (oIter != oMapMD5ToOffsetLen.end())
                {
                    // Point to previously written tile data if this content
                    // has already been written
                    sPMTilesEntry.offset = oIter->second.first;
                    sPMTilesEntry.length = oIter->second.second;
                }
                else
                {
                    const auto oIterToFileSize =
                        oMapTileIdToFileSize.find(sEntry.nTileId);
                    CPLAssert(oIterToFileSize != oMapTileIdToFileSize.end());
                    const uint32_t nTileDataLength = oIterToFileSize->second;

                    sPMTilesEntry.offset = nFileOffset;
                    sPMTilesEntry.length = nTileDataLength;

                    oMapMD5ToOffsetLen[sEntry.abyMD5] =
                        std::pair<uint64_t, uint32_t>(nFileOffset,
                                                      nTileDataLength);

                    nFileOffset += nTileDataLength;
                }

                asPMTilesEntries.push_back(sPMTilesEntry);

                nLastTileId = sEntry.nTileId;
                abyLastMD5 = sEntry.abyMD5;
            }
        }
    }

    const CPLCompressor *psCompressor = CPLGetCompressor("gzip");
    assert(psCompressor);
    std::string osCompressed;

    struct compression_exception : std::exception
    {
        const char *what() const noexcept override
        {
            return "Compression failed";
        }
    };

    const auto oCompressFunc = [psCompressor,
                                &osCompressed](const std::string &osBytes,
                                               uint8_t) -> std::string
    {
        osCompressed.resize(32 + osBytes.size() * 2);
        size_t nOutputSize = osCompressed.size();
        void *pOutputData = &osCompressed[0];
        if (!psCompressor->pfnFunc(osBytes.data(), osBytes.size(), &pOutputData,
                                   &nOutputSize, nullptr,
                                   psCompressor->user_data))
        {
            throw compression_exception();
        }
        osCompressed.resize(nOutputSize);
        return osCompressed;
    };

    std::string osCompressedMetadata;

    std::string osRootBytes;
    std::string osLeaveBytes;
    int nNumLeaves;
    try
    {
        osCompressedMetadata =
            oCompressFunc(osMetadata, pmtiles::COMPRESSION_GZIP);

        // Build the root and leave directories (one depth max)
        std::tie(osRootBytes, osLeaveBytes, nNumLeaves) =
            pmtiles::make_root_leaves(oCompressFunc, pmtiles::COMPRESSION_GZIP,
                                      asPMTilesEntries);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot build directories: %s",
                 e.what());
        return false;
    }

    // Finalize the header fields related to offsets and size of the
    // different parts of the file
    sHeader.root_dir_bytes = osRootBytes.size();
    sHeader.json_metadata_offset =
        sHeader.root_dir_offset + sHeader.root_dir_bytes;
    sHeader.json_metadata_bytes = osCompressedMetadata.size();
    sHeader.leaf_dirs_offset =
        sHeader.json_metadata_offset + sHeader.json_metadata_bytes;
    sHeader.leaf_dirs_bytes = osLeaveBytes.size();
    sHeader.tile_data_offset =
        sHeader.leaf_dirs_offset + sHeader.leaf_dirs_bytes;
    sHeader.tile_data_bytes = nFileOffset;

    // Number of tiles that are addressable in the PMTiles archive, that is
    // the number of tiles we would have if not deduplicating them
    sHeader.addressed_tiles_count = asTileEntries.size();

    // Number of tile entries in root and leave directories
    // ie entries whose run_length >= 1
    sHeader.tile_entries_count = asPMTilesEntries.size();

    // Number of distinct tile blobs
    sHeader.tile_contents_count = oMapMD5ToOffsetLen.size();

    // Now build the file!
    auto poFile = VSIVirtualHandleUniquePtr(VSIFOpenL(pszDestName, "wb"));
    if (!poFile)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s for write",
                 pszDestName);
        return false;
    }
    const auto osHeader = sHeader.serialize();

    if (poFile->Write(osHeader.data(), osHeader.size(), 1) != 1 ||
        poFile->Write(osRootBytes.data(), osRootBytes.size(), 1) != 1 ||
        poFile->Write(osCompressedMetadata.data(), osCompressedMetadata.size(),
                      1) != 1 ||
        (!osLeaveBytes.empty() &&
         poFile->Write(osLeaveBytes.data(), osLeaveBytes.size(), 1) != 1))
    {
        CPLError(CE_Failure, CPLE_FileIO, "Failed writing");
        return false;
    }

    // Copy tile content at end of the output file.
    {
        uint64_t nLastTileId = 0;
        uint64_t nFileOffset2 = 0;
        std::array<unsigned char, 16> abyLastMD5{0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0, 0, 0};
        std::unordered_set<std::array<unsigned char, 16>,
                           HashArray<unsigned char, 16>>
            oSetMD5;
        for (const auto &sEntry : asTileEntries)
        {
            if (sEntry.nTileId == nLastTileId + 1 &&
                sEntry.abyMD5 == abyLastMD5)
            {
                // If the tile id immediately follows the previous one and
                // has the same tile data, do nothing
            }
            else
            {
                auto oIter = oSetMD5.find(sEntry.abyMD5);
                if (oIter == oSetMD5.end())
                {
                    int nZ, nX, nY;
                    try
                    {
                        const auto sXYZ =
                            pmtiles::tileid_to_zxy(sEntry.nTileId);
                        nZ = sXYZ.z;
                        nY = sXYZ.y;
                        nX = sXYZ.x;
                    }
                    catch (const std::exception &e)
                    {
                        // shouldn't happen given previous checks
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot compute xyz: %s", e.what());
                        return false;
                    }

                    const std::string osZoomDir(CPLFormFilenameSafe(
                        pszSrcDirectory, CPLSPrintf("%d", nZ), nullptr));
                    const std::string osXDir(CPLFormFilenameSafe(
                        osZoomDir.c_str(), CPLSPrintf("%d", nX), nullptr));
                    const std::string osTileFilename(CPLFormFilenameSafe(
                        osXDir.c_str(), CPLSPrintf("%d", nY), pszExt));

                    const auto oIterToFileSize =
                        oMapTileIdToFileSize.find(sEntry.nTileId);
                    CPLAssert(oIterToFileSize != oMapTileIdToFileSize.end());
                    const uint32_t nTileDataLength = oIterToFileSize->second;

                    auto fp = VSIVirtualHandleUniquePtr(
                        VSIFOpenL(osTileFilename.c_str(), "rb"));
                    if (!fp)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s",
                                 osTileFilename.c_str());
                        return false;
                    }
                    if (fp->Read(abyTileData.data(), nTileDataLength) !=
                        nTileDataLength)
                    {
                        return false;
                    }

                    oSetMD5.insert(sEntry.abyMD5);

                    if (poFile->Write(abyTileData.data(), nTileDataLength, 1) !=
                        1)
                    {
                        CPLError(CE_Failure, CPLE_FileIO, "Failed writing");
                        return false;
                    }

                    nFileOffset2 += nTileDataLength;
                }

                nLastTileId = sEntry.nTileId;
                abyLastMD5 = sEntry.abyMD5;
            }
        }

        CPL_IGNORE_RET_VAL(nFileOffset2);
        CPLAssert(nFileOffset2 == nFileOffset);
    }

    if (poFile->Close() != 0)
        return false;

    return true;
}

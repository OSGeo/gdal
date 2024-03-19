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

#include "cpl_json.h"

#include "ogrsf_frmts.h"
#include "ogrpmtilesfrommbtiles.h"

#include "include_pmtiles.h"

#include "cpl_compressor.h"
#include "cpl_md5.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_map>
#include <utility>

/************************************************************************/
/*                         ProcessMetadata()                            */
/************************************************************************/

static bool ProcessMetadata(GDALDataset *poSQLiteDS, pmtiles::headerv3 &sHeader,
                            std::string &osMetadata)
{

    auto poMetadata = poSQLiteDS->GetLayerByName("metadata");
    if (!poMetadata)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "metadata table not found");
        return false;
    }

    const int iName = poMetadata->GetLayerDefn()->GetFieldIndex("name");
    const int iValue = poMetadata->GetLayerDefn()->GetFieldIndex("value");
    if (iName < 0 || iValue < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Bad structure for metadata table");
        return false;
    }

    CPLJSONObject oObj;
    CPLJSONDocument oJsonDoc;
    for (auto &&poFeature : poMetadata)
    {
        const char *pszName = poFeature->GetFieldAsString(iName);
        const char *pszValue = poFeature->GetFieldAsString(iValue);
        if (EQUAL(pszName, "json"))
        {
            if (!oJsonDoc.LoadMemory(pszValue))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot parse 'json' metadata item");
                return false;
            }
            for (const auto &oChild : oJsonDoc.GetRoot().GetChildren())
            {
                oObj.Add(oChild.GetName(), oChild);
            }
        }
        else
        {
            oObj.Add(pszName, pszValue);
        }
    }

    // MBTiles advertises scheme=tms. Override this
    oObj.Set("scheme", "xyz");

    const auto osFormat = oObj.GetString("format", "{missing}");
    if (osFormat != "pbf")
    {
        CPLError(CE_Failure, CPLE_AppDefined, "format=%s unhandled",
                 osFormat.c_str());
        return false;
    }

    int nMinZoom = atoi(oObj.GetString("minzoom", "-1").c_str());
    if (nMinZoom < 0 || nMinZoom > 255)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing or invalid minzoom");
        return false;
    }

    int nMaxZoom = atoi(oObj.GetString("maxzoom", "-1").c_str());
    if (nMaxZoom < 0 || nMaxZoom > 255)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing or invalid maxzoom");
        return false;
    }

    const CPLStringList aosCenter(
        CSLTokenizeString2(oObj.GetString("center").c_str(), ",", 0));
    if (aosCenter.size() != 3)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Expected 3 values for center");
        return false;
    }
    const double dfCenterLong = CPLAtof(aosCenter[0]);
    const double dfCenterLat = CPLAtof(aosCenter[1]);
    if (std::fabs(dfCenterLong) > 180 || std::fabs(dfCenterLat) > 90)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid center");
        return false;
    }
    const int nCenterZoom = atoi(aosCenter[2]);
    if (nCenterZoom < 0 || nCenterZoom > 255)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing or invalid center zoom");
        return false;
    }

    const CPLStringList aosBounds(
        CSLTokenizeString2(oObj.GetString("bounds").c_str(), ",", 0));
    if (aosBounds.size() != 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Expected 4 values for bounds");
        return false;
    }
    const double dfMinX = CPLAtof(aosBounds[0]);
    const double dfMinY = CPLAtof(aosBounds[1]);
    const double dfMaxX = CPLAtof(aosBounds[2]);
    const double dfMaxY = CPLAtof(aosBounds[3]);
    if (std::fabs(dfMinX) > 180 || std::fabs(dfMinY) > 90 ||
        std::fabs(dfMaxX) > 180 || std::fabs(dfMaxY) > 90)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid bounds");
        return false;
    }

    CPLJSONDocument oMetadataDoc;
    oMetadataDoc.SetRoot(oObj);
    osMetadata = oMetadataDoc.SaveAsString();
    // CPLDebugOnly("PMTiles", "Metadata = %s", osMetadata.c_str());

    sHeader.root_dir_offset = 127;
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
    sHeader.tile_compression = pmtiles::COMPRESSION_GZIP;
    sHeader.tile_type = pmtiles::TILETYPE_MVT;
    sHeader.min_zoom = static_cast<uint8_t>(nMinZoom);
    sHeader.max_zoom = static_cast<uint8_t>(nMaxZoom);
    sHeader.min_lon_e7 = static_cast<int32_t>(dfMinX * 10e6);
    sHeader.min_lat_e7 = static_cast<int32_t>(dfMinY * 10e6);
    sHeader.max_lon_e7 = static_cast<int32_t>(dfMaxX * 10e6);
    sHeader.max_lat_e7 = static_cast<int32_t>(dfMaxY * 10e6);
    sHeader.center_zoom = static_cast<uint8_t>(nCenterZoom);
    sHeader.center_lon_e7 = static_cast<int32_t>(dfCenterLong * 10e6);
    sHeader.center_lat_e7 = static_cast<int32_t>(dfCenterLat * 10e6);

    return true;
}

/************************************************************************/
/*                               HashArray()                            */
/************************************************************************/

// From https://codereview.stackexchange.com/questions/171999/specializing-stdhash-for-stdarray
// We do not use std::hash<std::array<T, N>> as the name of the struct
// because with gcc 5.4 we get the following error:
// https://stackoverflow.com/questions/25594644/warning-specialization-of-template-in-different-namespace
template <class T, size_t N> struct HashArray
{
    CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
    size_t operator()(const std::array<T, N> &key) const
    {
        std::hash<T> hasher;
        size_t result = 0;
        for (size_t i = 0; i < N; ++i)
        {
            result = result * 31 + hasher(key[i]);
        }
        return result;
    }
};

/************************************************************************/
/*                    OGRPMTilesConvertFromMBTiles()                    */
/************************************************************************/

bool OGRPMTilesConvertFromMBTiles(const char *pszDestName,
                                  const char *pszSrcName)
{
    const char *const apszAllowedDrivers[] = {"SQLite", nullptr};
    auto poSQLiteDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(pszSrcName, GDAL_OF_VECTOR, apszAllowedDrivers));
    if (!poSQLiteDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot open %s with SQLite driver", pszSrcName);
        return false;
    }

    pmtiles::headerv3 sHeader;
    std::string osMetadata;
    if (!ProcessMetadata(poSQLiteDS.get(), sHeader, osMetadata))
        return false;

    auto poTilesLayer = poSQLiteDS->GetLayerByName("tiles");
    if (!poTilesLayer)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "tiles table not found");
        return false;
    }

    const int iZoomLevel =
        poTilesLayer->GetLayerDefn()->GetFieldIndex("zoom_level");
    const int iTileColumn =
        poTilesLayer->GetLayerDefn()->GetFieldIndex("tile_column");
    const int iTileRow =
        poTilesLayer->GetLayerDefn()->GetFieldIndex("tile_row");
    const int iTileData =
        poTilesLayer->GetLayerDefn()->GetFieldIndex("tile_data");
    if (iZoomLevel < 0 || iTileColumn < 0 || iTileRow < 0 || iTileData < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad structure for tiles table");
        return false;
    }

    struct TileEntry
    {
        uint64_t nTileId;
        std::array<unsigned char, 16> abyMD5;
    };

    // In a first step browse through the tiles table to compute the PMTiles
    // tile_id of each tile, and compute a hash of the tile data for
    // deduplication
    std::vector<TileEntry> asTileEntries;
    for (auto &&poFeature : poTilesLayer)
    {
        const int nZoomLevel = poFeature->GetFieldAsInteger(iZoomLevel);
        if (nZoomLevel < 0 || nZoomLevel > 30)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Skipping tile with missing or invalid zoom_level");
            continue;
        }
        const int nColumn = poFeature->GetFieldAsInteger(iTileColumn);
        if (nColumn < 0 || nColumn >= (1 << nZoomLevel))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Skipping tile with missing or invalid tile_column");
            continue;
        }
        const int nRow = poFeature->GetFieldAsInteger(iTileRow);
        if (nRow < 0 || nRow >= (1 << nZoomLevel))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Skipping tile with missing or invalid tile_row");
            continue;
        }
        // MBTiles uses a 0=bottom-most row, whereas PMTiles uses
        // 0=top-most row
        const int nY = (1 << nZoomLevel) - 1 - nRow;
        uint64_t nTileId;
        try
        {
            nTileId = pmtiles::zxy_to_tileid(static_cast<uint8_t>(nZoomLevel),
                                             nColumn, nY);
        }
        catch (const std::exception &e)
        {
            // shouldn't happen given previous checks
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot compute tile id: %s",
                     e.what());
            return false;
        }
        int nTileDataLength = 0;
        const GByte *pabyData =
            poFeature->GetFieldAsBinary(iTileData, &nTileDataLength);
        if (!pabyData)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing tile_data");
            return false;
        }

        TileEntry sEntry;
        sEntry.nTileId = nTileId;

        CPLMD5Context md5context;
        CPLMD5Init(&md5context);
        CPLMD5Update(&md5context, pabyData, nTileDataLength);
        CPLMD5Final(&sEntry.abyMD5[0], &md5context);
        try
        {
            asTileEntries.push_back(sEntry);
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Out of memory browsing through tiles: %s", e.what());
            return false;
        }
    }

    // Sort the tiles by ascending tile_id. This is a requirement to build
    // the PMTiles directories.
    std::sort(asTileEntries.begin(), asTileEntries.end(),
              [](const TileEntry &a, const TileEntry &b)
              { return a.nTileId < b.nTileId; });

    // Let's build a temporary file that contains the tile data in
    // a way that corresponds to the "clustered" mode, that is
    // "offsets are either contiguous with the previous offset+length, or
    // refer to a lesser offset, when writing with deduplication."
    std::string osTmpFile(std::string(pszDestName) + ".tmp");
    if (!VSIIsLocal(pszDestName))
    {
        osTmpFile = CPLGenerateTempFilename(CPLGetFilename(pszDestName));
    }

    auto poTmpFile =
        VSIVirtualHandleUniquePtr(VSIFOpenL(osTmpFile.c_str(), "wb+"));
    VSIUnlink(osTmpFile.c_str());
    if (!poTmpFile)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s for write",
                 osTmpFile.c_str());
        return false;
    }

    struct ResetAndUnlinkTmpFile
    {
        VSIVirtualHandleUniquePtr &m_poFile;
        std::string m_osFilename;

        ResetAndUnlinkTmpFile(VSIVirtualHandleUniquePtr &poFile,
                              const std::string &osFilename)
            : m_poFile(poFile), m_osFilename(osFilename)
        {
        }

        ~ResetAndUnlinkTmpFile()
        {
            m_poFile.reset();
            VSIUnlink(m_osFilename.c_str());
        }
    };

    ResetAndUnlinkTmpFile oReseer(poTmpFile, osTmpFile);

    std::vector<pmtiles::entryv3> asPMTilesEntries;
    uint64_t nLastTileId = 0;
    uint64_t nFileOffset = 0;
    std::array<unsigned char, 16> abyLastMD5;
    std::unordered_map<std::array<unsigned char, 16>,
                       std::pair<uint64_t, uint32_t>,
                       HashArray<unsigned char, 16>>
        oMapMD5ToOffsetLen;
    for (const auto &sEntry : asTileEntries)
    {
        if (sEntry.nTileId == nLastTileId + 1 && sEntry.abyMD5 == abyLastMD5)
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
                try
                {
                    const auto sXYZ = pmtiles::tileid_to_zxy(sEntry.nTileId);
                    poTilesLayer->SetAttributeFilter(CPLSPrintf(
                        "zoom_level = %d AND tile_column = %u AND tile_row = "
                        "%u",
                        sXYZ.z, sXYZ.x, (1U << sXYZ.z) - 1U - sXYZ.y));
                }
                catch (const std::exception &e)
                {
                    // shouldn't happen given previous checks
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot compute xyz: %s", e.what());
                    return false;
                }
                poTilesLayer->ResetReading();
                auto poFeature =
                    std::unique_ptr<OGRFeature>(poTilesLayer->GetNextFeature());
                if (!poFeature)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot find tile");
                    return false;
                }
                int nTileDataLength = 0;
                const GByte *pabyData =
                    poFeature->GetFieldAsBinary(iTileData, &nTileDataLength);
                if (!pabyData)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Missing tile_data");
                    return false;
                }

                sPMTilesEntry.offset = nFileOffset;
                sPMTilesEntry.length = nTileDataLength;

                oMapMD5ToOffsetLen[sEntry.abyMD5] =
                    std::pair<uint64_t, uint32_t>(nFileOffset, nTileDataLength);

                nFileOffset += nTileDataLength;

                if (poTmpFile->Write(pabyData, nTileDataLength, 1) != 1)
                {
                    CPLError(CE_Failure, CPLE_FileIO, "Failed writing");
                    return false;
                }
            }

            asPMTilesEntries.push_back(sPMTilesEntry);

            nLastTileId = sEntry.nTileId;
            abyLastMD5 = sEntry.abyMD5;
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

    // Nomber of tiles that are addressable in the PMTiles archive, that is
    // the number of tiles we would have if not deduplicating them
    sHeader.addressed_tiles_count = asTileEntries.size();

    // Number of tile entries in root and leave directories
    // ie entries whose run_length >= 1
    sHeader.tile_entries_count = asPMTilesEntries.size();

    // Number of distinct tile blobs
    sHeader.tile_contents_count = oMapMD5ToOffsetLen.size();

    // Now build the final file!
    auto poFile = VSIVirtualHandleUniquePtr(VSIFOpenL(pszDestName, "wb"));
    if (!poFile)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s for write",
                 pszDestName);
        return false;
    }
    const auto osHeader = sHeader.serialize();

    if (poTmpFile->Seek(0, SEEK_SET) != 0 ||
        poFile->Write(osHeader.data(), osHeader.size(), 1) != 1 ||
        poFile->Write(osRootBytes.data(), osRootBytes.size(), 1) != 1 ||
        poFile->Write(osCompressedMetadata.data(), osCompressedMetadata.size(),
                      1) != 1 ||
        (!osLeaveBytes.empty() &&
         poFile->Write(osLeaveBytes.data(), osLeaveBytes.size(), 1) != 1))
    {
        CPLError(CE_Failure, CPLE_FileIO, "Failed writing");
        return false;
    }

    // Copy content of the temporary file at end of the output file.
    std::string oCopyBuffer;
    oCopyBuffer.resize(1024 * 1024);
    const uint64_t nTotalSize = nFileOffset;
    nFileOffset = 0;
    while (nFileOffset < nTotalSize)
    {
        const size_t nToRead = static_cast<size_t>(
            std::min<uint64_t>(nTotalSize - nFileOffset, oCopyBuffer.size()));
        if (poTmpFile->Read(&oCopyBuffer[0], nToRead, 1) != 1 ||
            poFile->Write(&oCopyBuffer[0], nToRead, 1) != 1)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Failed writing");
            return false;
        }
        nFileOffset += nToRead;
    }

    if (poFile->Close() != 0)
        return false;

    return true;
}

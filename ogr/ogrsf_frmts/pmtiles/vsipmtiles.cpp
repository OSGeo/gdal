/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Virtual file system /vsipmtiles/ PMTiles
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

#include "cpl_vsi_virtual.h"

#include "vsipmtiles.h"
#include "ogr_pmtiles.h"

#include "cpl_json.h"

#include <set>

#define ENDS_WITH_CI(a, b)                                                     \
    (strlen(a) >= strlen(b) && EQUAL(a + strlen(a) - strlen(b), b))

constexpr const char *PMTILES_HEADER_JSON = "pmtiles_header.json";
constexpr const char *METADATA_JSON = "metadata.json";

/************************************************************************/
/*                   VSIPMTilesFilesystemHandler                        */
/************************************************************************/

class VSIPMTilesFilesystemHandler final : public VSIFilesystemHandler
{
  public:
    VSIPMTilesFilesystemHandler() = default;

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError, CSLConstList papszOptions) override;
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;
};

/************************************************************************/
/*                   VSIPMTilesGetTileExtension()                       */
/************************************************************************/

static const char *VSIPMTilesGetTileExtension(OGRPMTilesDataset *poDS)
{
    const auto &sHeader = poDS->GetHeader();
    switch (sHeader.tile_type)
    {
        case pmtiles::TILETYPE_PNG:
            return ".png";
        case pmtiles::TILETYPE_JPEG:
            return ".jpg";
        case pmtiles::TILETYPE_WEBP:
            return ".webp";
        case pmtiles::TILETYPE_MVT:
            return ".mvt";
    }
    if (sHeader.tile_compression == pmtiles::COMPRESSION_GZIP)
        return ".bin.gz";
    if (sHeader.tile_compression == pmtiles::COMPRESSION_ZSTD)
        return ".bin.zstd";
    return ".bin";
}

/************************************************************************/
/*                  VSIPMTilesGetPMTilesHeaderJson()                    */
/************************************************************************/

static std::string VSIPMTilesGetPMTilesHeaderJson(OGRPMTilesDataset *poDS)
{
    const auto &sHeader = poDS->GetHeader();
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot;
    oRoot.Set("root_dir_offset", sHeader.root_dir_offset);
    oRoot.Set("json_metadata_offset", sHeader.json_metadata_offset);
    oRoot.Set("json_metadata_bytes", sHeader.json_metadata_bytes);
    oRoot.Set("leaf_dirs_offset", sHeader.leaf_dirs_offset);
    oRoot.Set("leaf_dirs_bytes", sHeader.leaf_dirs_bytes);
    oRoot.Set("tile_data_offset", sHeader.tile_data_offset);
    oRoot.Set("tile_data_bytes", sHeader.tile_data_bytes);
    oRoot.Set("addressed_tiles_count", sHeader.addressed_tiles_count);
    oRoot.Set("tile_entries_count", sHeader.tile_entries_count);
    oRoot.Set("tile_contents_count", sHeader.tile_contents_count);
    oRoot.Set("clustered", sHeader.clustered);
    oRoot.Set("internal_compression", sHeader.internal_compression);
    oRoot.Set("internal_compression_str",
              OGRPMTilesDataset::GetCompression(sHeader.internal_compression));
    oRoot.Set("tile_compression", sHeader.tile_compression);
    oRoot.Set("tile_compression_str",
              OGRPMTilesDataset::GetCompression(sHeader.tile_compression));
    oRoot.Set("tile_type", sHeader.tile_type);
    oRoot.Set("tile_type_str", OGRPMTilesDataset::GetTileType(sHeader));
    oRoot.Set("min_zoom", sHeader.min_zoom);
    oRoot.Set("max_zoom", sHeader.max_zoom);
    oRoot.Set("min_lon_e7", sHeader.min_lon_e7);
    oRoot.Set("min_lon_e7_float", sHeader.min_lon_e7 / 10e6);
    oRoot.Set("min_lat_e7", sHeader.min_lat_e7);
    oRoot.Set("min_lat_e7_float", sHeader.min_lat_e7 / 10e6);
    oRoot.Set("max_lon_e7", sHeader.max_lon_e7);
    oRoot.Set("max_lon_e7_float", sHeader.max_lon_e7 / 10e6);
    oRoot.Set("max_lat_e7", sHeader.max_lat_e7);
    oRoot.Set("max_lat_e7_float", sHeader.max_lat_e7 / 10e6);
    oRoot.Set("center_zoom", sHeader.center_zoom);
    oRoot.Set("center_lon_e7", sHeader.center_lon_e7);
    oRoot.Set("center_lat_e7", sHeader.center_lat_e7);
    oDoc.SetRoot(oRoot);
    return oDoc.SaveAsString();
}

/************************************************************************/
/*                           VSIPMTilesOpen()                           */
/************************************************************************/

static std::unique_ptr<OGRPMTilesDataset>
VSIPMTilesOpen(const char *pszFilename, std::string &osSubfilename,
               int &nComponents, int &nZ, int &nX, int &nY)
{
    if (!STARTS_WITH(pszFilename, "/vsipmtiles/"))
        return nullptr;
    pszFilename += strlen("/vsipmtiles/");

    std::string osFilename(pszFilename);
    if (!osFilename.empty() && osFilename.back() == '/')
        osFilename.resize(osFilename.size() - 1);
    pszFilename = osFilename.c_str();

    nZ = nX = nY = -1;
    nComponents = 0;
    std::string osPmtilesFilename;

    const char *pszPmtilesExt = strstr(pszFilename, ".pmtiles");
    if (!pszPmtilesExt)
        return nullptr;

    CPLStringList aosTokens;
    do
    {
        if (pszPmtilesExt[strlen(".pmtiles")] == '/')
        {
            const char *pszSubFile = pszPmtilesExt + strlen(".pmtiles/");
            osPmtilesFilename.assign(pszFilename, pszSubFile - pszFilename - 1);
            osSubfilename = pszPmtilesExt + strlen(".pmtiles/");
            if (osSubfilename == METADATA_JSON ||
                osSubfilename == PMTILES_HEADER_JSON)
            {
                break;
            }
        }
        else
        {
            osPmtilesFilename = pszFilename;
            osSubfilename.clear();
            break;
        }

        aosTokens = CSLTokenizeString2(osSubfilename.c_str(), "/", 0);
        nComponents = aosTokens.size();
        if (nComponents >= 4)
            return nullptr;

        if (CPLGetValueType(aosTokens[0]) != CPL_VALUE_INTEGER)
            return nullptr;
        nZ = atoi(aosTokens[0]);
        if (nComponents == 1)
            break;

        if (CPLGetValueType(aosTokens[1]) != CPL_VALUE_INTEGER)
            return nullptr;
        nX = atoi(aosTokens[1]);
        if (nComponents == 2)
            break;

        break;
    } while (false);

    GDALOpenInfo oOpenInfo(osPmtilesFilename.c_str(), GA_ReadOnly);
    CPLStringList aosOptions;
    aosOptions.SetNameValue("DECOMPRESS_TILES", "NO");
    aosOptions.SetNameValue("ACCEPT_ANY_TILE_TYPE", "YES");
    oOpenInfo.papszOpenOptions = aosOptions.List();
    auto poDS = std::make_unique<OGRPMTilesDataset>();
    {
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        if (!poDS->Open(&oOpenInfo))
            return nullptr;
    }

    if (nComponents == 3)
    {
        const char *pszTileExt = VSIPMTilesGetTileExtension(poDS.get());
        if (!ENDS_WITH_CI(aosTokens[2], pszTileExt))
            return nullptr;
        aosTokens[2][strlen(aosTokens[2]) - strlen(pszTileExt)] = 0;
        if (CPLGetValueType(aosTokens[2]) != CPL_VALUE_INTEGER)
            return nullptr;
        nY = atoi(aosTokens[2]);
    }
    return poDS;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

VSIVirtualHandle *
VSIPMTilesFilesystemHandler::Open(const char *pszFilename,
                                  const char *pszAccess, bool /*bSetError*/,
                                  CSLConstList /*papszOptions*/)
{
    if (strchr(pszAccess, '+') || strchr(pszAccess, 'w') ||
        strchr(pszAccess, 'a'))
        return nullptr;
    std::string osSubfilename;
    int nComponents;
    int nZ;
    int nX;
    int nY;
    auto poDS =
        VSIPMTilesOpen(pszFilename, osSubfilename, nComponents, nZ, nX, nY);
    if (!poDS)
        return nullptr;

    if (osSubfilename == METADATA_JSON)
    {
        return VSIFileFromMemBuffer(nullptr,
                                    reinterpret_cast<GByte *>(CPLStrdup(
                                        poDS->GetMetadataContent().c_str())),
                                    poDS->GetMetadataContent().size(), true);
    }

    if (osSubfilename == PMTILES_HEADER_JSON)
    {
        const auto osStr = VSIPMTilesGetPMTilesHeaderJson(poDS.get());
        return VSIFileFromMemBuffer(
            nullptr, reinterpret_cast<GByte *>(CPLStrdup(osStr.c_str())),
            osStr.size(), true);
    }

    if (nComponents != 3)
        return nullptr;

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);

    OGRPMTilesTileIterator oIter(poDS.get(), nZ, nX, nY, nX, nY);
    auto sTile = oIter.GetNextTile();
    if (sTile.offset == 0)
        return nullptr;

    const auto *posStr = poDS->ReadTileData(sTile.offset, sTile.length);
    if (!posStr)
    {
        return nullptr;
    }

    GByte *pabyData = static_cast<GByte *>(CPLMalloc(posStr->size()));
    memcpy(pabyData, posStr->data(), posStr->size());
    return VSIFileFromMemBuffer(nullptr, pabyData, posStr->size(), true);
}

/************************************************************************/
/*                               Stat()                                 */
/************************************************************************/

int VSIPMTilesFilesystemHandler::Stat(const char *pszFilename,
                                      VSIStatBufL *pStatBuf, int /*nFlags*/)
{
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    std::string osSubfilename;
    int nComponents;
    int nZ;
    int nX;
    int nY;
    auto poDS =
        VSIPMTilesOpen(pszFilename, osSubfilename, nComponents, nZ, nX, nY);
    if (!poDS)
        return -1;

    if (osSubfilename.empty())
        return -1;

    VSIStatBufL sStatPmtiles;
    if (VSIStatL(poDS->GetDescription(), &sStatPmtiles) == 0)
    {
        pStatBuf->st_mtime = sStatPmtiles.st_mtime;
    }

    if (osSubfilename == METADATA_JSON)
    {
        pStatBuf->st_mode = S_IFREG;
        pStatBuf->st_size = poDS->GetMetadataContent().size();
        return 0;
    }

    if (osSubfilename == PMTILES_HEADER_JSON)
    {
        pStatBuf->st_mode = S_IFREG;
        pStatBuf->st_size = VSIPMTilesGetPMTilesHeaderJson(poDS.get()).size();
        return 0;
    }

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);

    OGRPMTilesTileIterator oIter(poDS.get(), nZ, nX, nY, nX, nY);
    auto sTile = oIter.GetNextTile();
    if (sTile.offset == 0)
        return -1;

    if (nComponents <= 2)
    {
        pStatBuf->st_mode = S_IFDIR;
        return 0;
    }

    pStatBuf->st_mode = S_IFREG;
    pStatBuf->st_size = sTile.length;
    return 0;
}

/************************************************************************/
/*                            ReadDirEx()                               */
/************************************************************************/

char **VSIPMTilesFilesystemHandler::ReadDirEx(const char *pszFilename,
                                              int nMaxFiles)
{
    std::string osSubfilename;
    int nComponents;
    int nZ;
    int nX;
    int nY;
    auto poDS =
        VSIPMTilesOpen(pszFilename, osSubfilename, nComponents, nZ, nX, nY);
    if (!poDS)
        return nullptr;

    if (osSubfilename.empty())
    {
        CPLStringList aosFiles;
        aosFiles.AddString(PMTILES_HEADER_JSON);
        aosFiles.AddString(METADATA_JSON);
        for (int i = poDS->GetMinZoomLevel(); i <= poDS->GetMaxZoomLevel(); ++i)
        {
            OGRPMTilesTileIterator oIter(poDS.get(), i);
            auto sTile = oIter.GetNextTile();
            if (sTile.offset != 0)
            {
                if (nMaxFiles > 0 && aosFiles.size() >= nMaxFiles)
                    break;
                aosFiles.AddString(CPLSPrintf("%d", i));
            }
        }
        return aosFiles.StealList();
    }

    if (nComponents == 1)
    {
        std::set<int> oSetX;
        OGRPMTilesTileIterator oIter(poDS.get(), nZ);
        while (true)
        {
            auto sTile = oIter.GetNextTile();
            if (sTile.offset == 0)
                break;
            oSetX.insert(sTile.x);
            if (nMaxFiles > 0 && static_cast<int>(oSetX.size()) >= nMaxFiles)
                break;
            if (oSetX.size() == 1024 * 1024)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too many tiles");
                return nullptr;
            }
        }
        CPLStringList aosFiles;
        for (int x : oSetX)
        {
            aosFiles.AddString(CPLSPrintf("%d", x));
        }
        return aosFiles.StealList();
    }

    if (nComponents == 2)
    {
        std::set<int> oSetY;
        OGRPMTilesTileIterator oIter(poDS.get(), nZ, nX, -1, nX, -1);
        while (true)
        {
            auto sTile = oIter.GetNextTile();
            if (sTile.offset == 0)
                break;
            oSetY.insert(sTile.y);
            if (nMaxFiles > 0 && static_cast<int>(oSetY.size()) >= nMaxFiles)
                break;
            if (oSetY.size() == 1024 * 1024)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too many tiles");
                return nullptr;
            }
        }
        CPLStringList aosFiles;
        const char *pszTileExt = VSIPMTilesGetTileExtension(poDS.get());
        for (int y : oSetY)
        {
            aosFiles.AddString(CPLSPrintf("%d%s", y, pszTileExt));
        }
        return aosFiles.StealList();
    }

    return nullptr;
}

/************************************************************************/
/*                         VSIPMTilesRegister()                         */
/************************************************************************/

void VSIPMTilesRegister()
{
    if (VSIFileManager::GetHandler("/vsipmtiles/") ==
        VSIFileManager::GetHandler("/"))
    {
        VSIFileManager::InstallHandler("/vsipmtiles/",
                                       new VSIPMTilesFilesystemHandler());
    }
}

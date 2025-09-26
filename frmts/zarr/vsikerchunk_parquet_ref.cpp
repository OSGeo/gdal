/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver. Virtual file system for
 *           https://fsspec.github.io/kerchunk/spec.html#parquet-references
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "vsikerchunk.h"

#include "cpl_json.h"
#include "cpl_mem_cache.h"
#include "cpl_vsi_error.h"
#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <cinttypes>
#include <functional>
#include <limits>
#include <mutex>
#include <set>
#include <utility>

extern "C" int CPL_DLL GDALIsInGlobalDestructor();

/************************************************************************/
/*                         VSIZarrArrayInfo                             */
/************************************************************************/

struct VSIZarrArrayInfo
{
    std::vector<uint64_t> anChunkCount{};
};

/************************************************************************/
/*                    VSIKerchunkParquetRefFile                         */
/************************************************************************/

struct VSIKerchunkParquetRefFile
{
    int m_nRecordSize = 0;
    std::map<std::string, std::vector<GByte>> m_oMapKeys{};
    std::map<std::string, VSIZarrArrayInfo> m_oMapArrayInfo{};
};

/************************************************************************/
/*                    VSIKerchunkParquetRefFileSystem                   */
/************************************************************************/

class VSIKerchunkParquetRefFileSystem final : public VSIFilesystemHandler
{
  public:
    VSIKerchunkParquetRefFileSystem()
    {
        IsFileSystemInstantiated() = true;
    }

    ~VSIKerchunkParquetRefFileSystem();

    static bool &IsFileSystemInstantiated()
    {
        static bool bIsFileSystemInstantiated = false;
        return bIsFileSystemInstantiated;
    }

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError, CSLConstList papszOptions) override;

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;

    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;

    void CleanCache();

  private:
    lru11::Cache<std::string, std::shared_ptr<VSIKerchunkParquetRefFile>,
                 std::mutex>
        m_oCache{};

    std::mutex m_oParquetCacheMutex{};
    lru11::Cache<std::string, std::shared_ptr<GDALDataset>> *m_poParquetCache{};

    static std::pair<std::string, std::string>
    SplitFilename(const char *pszFilename);

    std::shared_ptr<VSIKerchunkParquetRefFile>
    Load(const std::string &osRootFilename);

    struct ChunkInfo
    {
        std::string osParquetFileDirectory{};
        std::unique_ptr<OGRFeature> poFeature{};
        int iPathField = -1;
        int iOffsetField = -1;
        int iSizeField = -1;
        int iRawField = -1;
    };

    ChunkInfo
    GetChunkInfo(const std::string &osRootFilename,
                 const std::shared_ptr<VSIKerchunkParquetRefFile> &refFile,
                 const std::string &osKey);

    CPL_DISALLOW_COPY_ASSIGN(VSIKerchunkParquetRefFileSystem)
};

/************************************************************************/
/*               ~VSIKerchunkParquetRefFileSystem()                     */
/************************************************************************/

VSIKerchunkParquetRefFileSystem::~VSIKerchunkParquetRefFileSystem()
{
    CleanCache();
    IsFileSystemInstantiated() = false;
}

/************************************************************************/
/*            VSIKerchunkParquetRefFileSystem::CleanCache()             */
/************************************************************************/

void VSIKerchunkParquetRefFileSystem::CleanCache()
{
    // If we are in the unloading of the library do not try to close
    // datasets to avoid crashes and prefer leaking memory...
    if (!GDALIsInGlobalDestructor())
    {
        std::lock_guard<std::mutex> oLock(m_oParquetCacheMutex);
        if (m_poParquetCache)
        {
            m_poParquetCache->clear();
            delete m_poParquetCache;
            m_poParquetCache = nullptr;
        }
    }
}

/************************************************************************/
/*            VSIKerchunkParquetRefFileSystem::SplitFilename()          */
/************************************************************************/

/*static*/
std::pair<std::string, std::string>
VSIKerchunkParquetRefFileSystem::SplitFilename(const char *pszFilename)
{
    if (!STARTS_WITH(pszFilename, PARQUET_REF_FS_PREFIX))
        return {std::string(), std::string()};

    std::string osRootFilename;

    pszFilename += strlen(PARQUET_REF_FS_PREFIX);

    if (*pszFilename == '{')
    {
        // Parse /vsikerchunk_parquet_ref/{/path/to/some/parquet_root}[key]
        int nLevel = 1;
        ++pszFilename;
        for (; *pszFilename; ++pszFilename)
        {
            if (*pszFilename == '{')
            {
                ++nLevel;
            }
            else if (*pszFilename == '}')
            {
                --nLevel;
                if (nLevel == 0)
                {
                    ++pszFilename;
                    break;
                }
            }
            osRootFilename += *pszFilename;
        }
        if (nLevel != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid %s syntax: should be "
                     "%s{/path/to/some/file}[/optional_key]",
                     PARQUET_REF_FS_PREFIX, PARQUET_REF_FS_PREFIX);
            return {std::string(), std::string()};
        }

        return {osRootFilename,
                *pszFilename == '/' ? pszFilename + 1 : pszFilename};
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid %s syntax: should be "
                 "%s{/path/to/root/dir}[/optional_key]",
                 PARQUET_REF_FS_PREFIX, PARQUET_REF_FS_PREFIX);
        return {std::string(), std::string()};
    }
}

/************************************************************************/
/*              VSIKerchunkParquetRefFileSystem::Load()                 */
/************************************************************************/

std::shared_ptr<VSIKerchunkParquetRefFile>
VSIKerchunkParquetRefFileSystem::Load(const std::string &osRootFilename)
{
    std::shared_ptr<VSIKerchunkParquetRefFile> refFile;
    if (m_oCache.tryGet(osRootFilename, refFile))
        return refFile;

    CPLJSONDocument oDoc;

    const std::string osZMetataFilename =
        CPLFormFilenameSafe(osRootFilename.c_str(), ".zmetadata", nullptr);
    if (!oDoc.Load(osZMetataFilename))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VSIKerchunkParquetRefFileSystem: cannot open %s",
                 osZMetataFilename.c_str());
        return nullptr;
    }

    const auto oRoot = oDoc.GetRoot();
    const auto oRecordSize = oRoot.GetObj("record_size");
    if (!oRecordSize.IsValid() ||
        oRecordSize.GetType() != CPLJSONObject::Type::Integer)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VSIKerchunkParquetRefFileSystem: key 'record_size' missing "
                 "or not of type integer");
        return nullptr;
    }

    const auto oMetadata = oRoot.GetObj("metadata");
    if (!oMetadata.IsValid() ||
        oMetadata.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VSIKerchunkParquetRefFileSystem: key 'metadata' missing "
                 "or not of type dict");
        return nullptr;
    }

    refFile = std::make_shared<VSIKerchunkParquetRefFile>();
    refFile->m_nRecordSize = oRecordSize.ToInteger();
    if (refFile->m_nRecordSize < 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VSIKerchunkParquetRefFileSystem: Invalid 'record_size'");
        return nullptr;
    }

    for (const auto &oEntry : oMetadata.GetChildren())
    {
        const std::string osKeyName = oEntry.GetName();
        if (oEntry.GetType() == CPLJSONObject::Type::Object)
        {
            const std::string osSerialized =
                oEntry.Format(CPLJSONObject::PrettyFormat::Plain);
            std::vector<GByte> abyValue;
            abyValue.insert(
                abyValue.end(),
                reinterpret_cast<const GByte *>(osSerialized.data()),
                reinterpret_cast<const GByte *>(osSerialized.data()) +
                    osSerialized.size());

            refFile->m_oMapKeys[osKeyName] = std::move(abyValue);

            if (cpl::ends_with(osKeyName, "/.zarray"))
            {
                const auto oShape = oEntry.GetArray("shape");
                const auto oChunks = oEntry.GetArray("chunks");
                if (!oShape.IsValid())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "VSIKerchunkParquetRefFileSystem: "
                             "missing 'shape' entry for key '%s'",
                             osKeyName.c_str());
                    return nullptr;
                }
                else if (!oChunks.IsValid())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "VSIKerchunkParquetRefFileSystem: "
                             "missing 'chunks' entry for key '%s'",
                             osKeyName.c_str());
                    return nullptr;
                }
                else if (oShape.Size() != oChunks.Size())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "VSIKerchunkParquetRefFileSystem: "
                             "'shape' and 'chunks' entries have not the same "
                             "number of values for key '%s'",
                             osKeyName.c_str());
                    return nullptr;
                }
                else if (oShape.Size() > 32)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "VSIKerchunkParquetRefFileSystem: "
                             "'shape' has too many dimensions for key '%s'",
                             osKeyName.c_str());
                    return nullptr;
                }
                else
                {
                    VSIZarrArrayInfo arrayInfo;
                    uint64_t nTotalChunks = 1;
                    for (int i = 0; i < oShape.Size(); ++i)
                    {
                        const uint64_t nSize = oShape[i].ToLong();
                        const uint64_t nChunkSize = oChunks[i].ToLong();
                        if (nSize == 0)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "VSIKerchunkParquetRefFileSystem: "
                                     "shape[%d]=0 in "
                                     "array definition for key '%s'",
                                     i, osKeyName.c_str());
                            return nullptr;
                        }
                        else if (nChunkSize == 0)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "VSIKerchunkParquetRefFileSystem: "
                                     "chunks[%d]=0 in "
                                     "array definition for key '%s'",
                                     i, osKeyName.c_str());
                            return nullptr;
                        }
                        const auto nChunkCount =
                            DIV_ROUND_UP(nSize, nChunkSize);
                        if (nChunkCount >
                            std::numeric_limits<uint64_t>::max() / nTotalChunks)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "VSIKerchunkParquetRefFileSystem: "
                                "product(shape[]) > UINT64_MAX for key '%s'",
                                osKeyName.c_str());
                            return nullptr;
                        }
                        nTotalChunks *= nChunkCount;
                        arrayInfo.anChunkCount.push_back(nChunkCount);
                    }
                    const std::string osArrayDir = osKeyName.substr(
                        0, osKeyName.size() - strlen("/.zarray"));
                    refFile->m_oMapArrayInfo[osArrayDir] = std::move(arrayInfo);
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VSIKerchunkParquetRefFileSystem: invalid value type for "
                     "key '%s'",
                     osKeyName.c_str());
            return nullptr;
        }
    }

    m_oCache.insert(osRootFilename, refFile);
    return refFile;
}

/************************************************************************/
/*           VSIKerchunkParquetRefFileSystem::GetChunkInfo()            */
/************************************************************************/

VSIKerchunkParquetRefFileSystem::ChunkInfo
VSIKerchunkParquetRefFileSystem::GetChunkInfo(
    const std::string &osRootFilename,
    const std::shared_ptr<VSIKerchunkParquetRefFile> &refFile,
    const std::string &osKey)
{
    ChunkInfo info;

    const std::string osArrayPath = CPLGetPathSafe(osKey.c_str());
    const auto oIterArray = refFile->m_oMapArrayInfo.find(osArrayPath);
    const std::string osIndices = CPLGetFilename(osKey.c_str());
    if (oIterArray != refFile->m_oMapArrayInfo.end() && !osIndices.empty() &&
        osIndices[0] >= '0' && osIndices[0] <= '9')
    {
        const auto &oArrayInfo = oIterArray->second;
        const CPLStringList aosIndices(
            CSLTokenizeString2(osIndices.c_str(), ".", 0));
        if ((static_cast<size_t>(aosIndices.size()) ==
             oArrayInfo.anChunkCount.size()) ||
            (aosIndices.size() == 1 && strcmp(aosIndices[0], "0") == 0 &&
             oArrayInfo.anChunkCount.empty()))
        {
            std::vector<uint64_t> anIndices;
            for (size_t i = 0; i < oArrayInfo.anChunkCount.size(); ++i)
            {
                char *endptr = nullptr;
                anIndices.push_back(std::strtoull(aosIndices[i], &endptr, 10));
                if (aosIndices[i][0] == '-' ||
                    endptr != aosIndices[i] + strlen(aosIndices[i]) ||
                    anIndices[i] >= oArrayInfo.anChunkCount[i])
                {
                    return info;
                }
            }

            uint64_t nLinearIndex = 0;
            uint64_t nMulFactor = 1;
            for (size_t i = anIndices.size(); i > 0;)
            {
                --i;
                nLinearIndex += anIndices[i] * nMulFactor;
                nMulFactor *= oArrayInfo.anChunkCount[i];
            }

            CPLDebugOnly("VSIKerchunkParquetRefFileSystem",
                         "Linear chunk index %" PRIu64, nLinearIndex);

            const uint64_t nParquetIdx = nLinearIndex / refFile->m_nRecordSize;
            const int nIdxInParquet =
                static_cast<int>(nLinearIndex % refFile->m_nRecordSize);

            const std::string osParquetFilename = CPLFormFilenameSafe(
                CPLFormFilenameSafe(osRootFilename.c_str(), osArrayPath.c_str(),
                                    nullptr)
                    .c_str(),
                CPLSPrintf("refs.%" PRIu64 ".parq", nParquetIdx), nullptr);
            CPLDebugOnly("VSIKerchunkParquetRefFileSystem",
                         "Looking for entry %d in Parquet file %s",
                         nIdxInParquet, osParquetFilename.c_str());

            std::lock_guard<std::mutex> oLock(m_oParquetCacheMutex);
            std::shared_ptr<GDALDataset> poDS;
            if (!m_poParquetCache)
            {
                m_poParquetCache = std::make_unique<lru11::Cache<
                    std::string, std::shared_ptr<GDALDataset>>>()
                                       .release();
            }
            if (!m_poParquetCache->tryGet(osParquetFilename, poDS))
            {
                const char *const apszAllowedDrivers[] = {"PARQUET", "ADBC",
                                                          nullptr};
                CPLConfigOptionSetter oSetter(
                    "OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", "NO", false);
                poDS.reset(
                    GDALDataset::Open(osParquetFilename.c_str(),
                                      GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                                      apszAllowedDrivers, nullptr, nullptr));
                if (poDS)
                    m_poParquetCache->insert(osParquetFilename, poDS);
            }

            if (poDS && poDS->GetLayerCount() == 1)
            {
                const auto IsIntOrInt64 = [](OGRFieldType eType)
                { return eType == OFTInteger || eType == OFTInteger64; };
                auto poLayer = poDS->GetLayer(0);
                const auto poDefn = poLayer->GetLayerDefn();
                info.iPathField = poDefn->GetFieldIndex("path");
                info.iOffsetField = poDefn->GetFieldIndex("offset");
                info.iSizeField = poDefn->GetFieldIndex("size");
                info.iRawField = poDefn->GetFieldIndex("raw");
                if (info.iPathField >= 0 && info.iOffsetField >= 0 &&
                    info.iSizeField >= 0 && info.iRawField >= 0 &&
                    poDefn->GetFieldDefn(info.iPathField)->GetType() ==
                        OFTString &&
                    IsIntOrInt64(
                        poDefn->GetFieldDefn(info.iOffsetField)->GetType()) &&
                    IsIntOrInt64(
                        poDefn->GetFieldDefn(info.iSizeField)->GetType()) &&
                    poDefn->GetFieldDefn(info.iRawField)->GetType() ==
                        OFTBinary)
                {
                    info.osParquetFileDirectory =
                        CPLGetPathSafe(osParquetFilename.c_str());
                    info.poFeature.reset(poLayer->GetFeature(nIdxInParquet));
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s has an unexpected field structure",
                             osParquetFilename.c_str());
                }
            }
        }
    }
    return info;
}

/************************************************************************/
/*               VSIKerchunkParquetRefFileSystem::Open()                */
/************************************************************************/

VSIVirtualHandle *VSIKerchunkParquetRefFileSystem::Open(
    const char *pszFilename, const char *pszAccess, bool /* bSetError */,
    CSLConstList /* papszOptions */)
{
    CPLDebugOnly("VSIKerchunkParquetRefFileSystem", "Open(%s)", pszFilename);
    if (strcmp(pszAccess, "r") != 0 && strcmp(pszAccess, "rb") != 0)
        return nullptr;

    const auto [osRootFilename, osKey] = SplitFilename(pszFilename);
    if (osRootFilename.empty())
        return nullptr;

    const auto refFile = Load(osRootFilename);
    if (!refFile)
        return nullptr;

    const auto oIter = refFile->m_oMapKeys.find(osKey);
    if (oIter == refFile->m_oMapKeys.end())
    {
        const auto info = GetChunkInfo(osRootFilename, refFile, osKey);
        if (info.poFeature)
        {
            if (info.poFeature->IsFieldSetAndNotNull(info.iRawField))
            {
                auto psField = info.poFeature->GetRawFieldRef(info.iRawField);
                // Borrow binary data to feature
                GByte *abyData = psField->Binary.paData;
                int nSize = psField->Binary.nCount;
                psField->Binary.paData = nullptr;
                psField->Binary.nCount = 0;
                // and transmit its ownership to the VSIMem file
                return VSIFileFromMemBuffer(nullptr, abyData, nSize,
                                            /* bTakeOwnership = */ true);
            }
            else
            {
                const uint64_t nOffset =
                    info.poFeature->GetFieldAsInteger64(info.iOffsetField);
                const int nSize =
                    info.poFeature->GetFieldAsInteger(info.iSizeField);

                std::string osVSIPath = VSIKerchunkMorphURIToVSIPath(
                    info.poFeature->GetFieldAsString(info.iPathField),
                    info.osParquetFileDirectory);
                if (osVSIPath.empty())
                    return nullptr;

                const std::string osPath =
                    nSize ? CPLSPrintf("/vsisubfile/%" PRIu64 "_%u,%s", nOffset,
                                       nSize, osVSIPath.c_str())
                          : std::move(osVSIPath);
                CPLDebugOnly("VSIKerchunkParquetRefFileSystem", "Opening %s",
                             osPath.c_str());
                CPLConfigOptionSetter oSetter("GDAL_DISABLE_READDIR_ON_OPEN",
                                              "EMPTY_DIR", false);
                auto fp = VSIFOpenEx2L(osPath.c_str(), "rb", true, nullptr);
                if (!fp)
                {
                    if (!VSIToCPLError(CE_Failure, CPLE_FileIO))
                        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                                 osPath.c_str());
                }
                return fp;
            }
        }

        return nullptr;
    }

    const auto &abyValue = oIter->second;
    return VSIFileFromMemBuffer(nullptr, const_cast<GByte *>(abyValue.data()),
                                abyValue.size(), /* bTakeOwnership = */ false);
}

/************************************************************************/
/*               VSIKerchunkParquetRefFileSystem::Stat()                */
/************************************************************************/

int VSIKerchunkParquetRefFileSystem::Stat(const char *pszFilename,
                                          VSIStatBufL *pStatBuf, int nFlags)
{
    CPLDebugOnly("VSIKerchunkParquetRefFileSystem", "Stat(%s)", pszFilename);
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    const auto [osRootFilename, osKey] = SplitFilename(pszFilename);
    if (osRootFilename.empty())
        return -1;

    const auto refFile = Load(osRootFilename);
    if (!refFile)
        return -1;

    if (osKey.empty())
    {
        pStatBuf->st_mode = S_IFDIR;
        return 0;
    }

    const auto oIter = refFile->m_oMapKeys.find(osKey);
    if (oIter == refFile->m_oMapKeys.end())
    {
        const auto info = GetChunkInfo(osRootFilename, refFile, osKey);
        if (info.poFeature)
        {
            if (info.poFeature->IsFieldSetAndNotNull(info.iRawField))
            {
                int nSize = 0;
                info.poFeature->GetFieldAsBinary(info.iRawField, &nSize);
                pStatBuf->st_size = nSize;
            }
            else
            {
                pStatBuf->st_size =
                    info.poFeature->GetFieldAsInteger64(info.iSizeField);
                if (pStatBuf->st_size == 0)
                {
                    const std::string osVSIPath = VSIKerchunkMorphURIToVSIPath(
                        info.poFeature->GetFieldAsString(info.iPathField),
                        info.osParquetFileDirectory);
                    if (osVSIPath.empty())
                        return -1;
                    return VSIStatExL(osVSIPath.c_str(), pStatBuf, nFlags);
                }
            }
            pStatBuf->st_mode = S_IFREG;
            return 0;
        }

        if (cpl::contains(refFile->m_oMapKeys, osKey + "/.zgroup") ||
            cpl::contains(refFile->m_oMapKeys, osKey + "/.zarray"))
        {
            pStatBuf->st_mode = S_IFDIR;
            return 0;
        }

        return -1;
    }

    const auto &abyValue = oIter->second;
    pStatBuf->st_size = abyValue.size();
    pStatBuf->st_mode = S_IFREG;

    return 0;
}

/************************************************************************/
/*             VSIKerchunkParquetRefFileSystem::ReadDirEx()             */
/************************************************************************/

char **VSIKerchunkParquetRefFileSystem::ReadDirEx(const char *pszDirname,
                                                  int nMaxFiles)
{
    CPLDebugOnly("VSIKerchunkParquetRefFileSystem", "ReadDir(%s)", pszDirname);

    const auto [osRootFilename, osAskedKey] = SplitFilename(pszDirname);
    if (osRootFilename.empty())
        return nullptr;

    const auto refFile = Load(osRootFilename);
    if (!refFile)
        return nullptr;

    std::set<std::string> set;
    for (const auto &[key, value] : refFile->m_oMapKeys)
    {
        if (osAskedKey.empty())
        {
            const auto nPos = key.find('/');
            if (nPos == std::string::npos)
                set.insert(key);
            else
                set.insert(key.substr(0, nPos));
        }
        else if (key.size() > osAskedKey.size() &&
                 cpl::starts_with(key, osAskedKey) &&
                 key[osAskedKey.size()] == '/')
        {
            std::string subKey = key.substr(osAskedKey.size() + 1);
            const auto nPos = subKey.find('/');
            if (nPos == std::string::npos)
                set.insert(std::move(subKey));
            else
                set.insert(subKey.substr(0, nPos));
        }
    }

    CPLStringList aosRet;
    for (const std::string &v : set)
    {
        // CPLDebugOnly("VSIKerchunkParquetRefFileSystem", ".. %s", v.c_str());
        aosRet.AddString(v.c_str());
    }

    // Synthetize file names for x.y.z chunks
    const auto oIterArray = refFile->m_oMapArrayInfo.find(osAskedKey);
    if (oIterArray != refFile->m_oMapArrayInfo.end())
    {
        const auto &oArrayInfo = oIterArray->second;
        if (oArrayInfo.anChunkCount.empty())
        {
            aosRet.AddString("0");
        }
        else
        {
            std::string osCurElt;
            std::function<bool(size_t)> Enumerate;
            if (nMaxFiles <= 0)
                nMaxFiles = 100 * 1024 * 1024;

            Enumerate = [nMaxFiles, &aosRet, &oArrayInfo, &osCurElt,
                         &Enumerate](size_t iDim)
            {
                const size_t sizeBefore = osCurElt.size();
                for (uint64_t i = 0; i < oArrayInfo.anChunkCount[iDim]; ++i)
                {
                    osCurElt += CPLSPrintf("%" PRIu64, i);
                    if (iDim + 1 < oArrayInfo.anChunkCount.size())
                    {
                        osCurElt += '.';
                        if (!Enumerate(iDim + 1))
                            return false;
                    }
                    else
                    {
                        if (aosRet.size() >= nMaxFiles)
                            return false;
                        aosRet.AddString(osCurElt);
                    }
                    osCurElt.resize(sizeBefore);
                }
                return true;
            };

            Enumerate(0);
        }
    }

    return aosRet.StealList();
}

/************************************************************************/
/*               VSIInstallKerchunkParquetRefFileSystem()               */
/************************************************************************/

void VSIInstallKerchunkParquetRefFileSystem()
{
    static std::mutex oMutex;
    std::lock_guard<std::mutex> oLock(oMutex);
    // cppcheck-suppress knownConditionTrueFalse
    if (!VSIKerchunkParquetRefFileSystem::IsFileSystemInstantiated())
    {
        VSIFileManager::InstallHandler(
            PARQUET_REF_FS_PREFIX,
            std::make_unique<VSIKerchunkParquetRefFileSystem>().release());
    }
}

/************************************************************************/
/*                VSIKerchunkParquetRefFileSystemCleanCache()           */
/************************************************************************/

void VSIKerchunkParquetRefFileSystemCleanCache()
{
    auto poFS = dynamic_cast<VSIKerchunkParquetRefFileSystem *>(
        VSIFileManager::GetHandler(PARQUET_REF_FS_PREFIX));
    if (poFS)
        poFS->CleanCache();
}

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Icechunk driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "icechunkdrivercore.h"
#include "icechunkutils.h"
#include "icechunkmanifest.h"
#include "icechunkrepo.h"
#include "icechunksnapshot.h"

#include "cpl_mem_cache.h"
#include "cpl_vsi_virtual.h"

#include <cinttypes>
#include <limits>
#include <mutex>
#include <utility>

namespace gdal::icechunk
{

/************************************************************************/
/*                        VSIIcechunkFileSystem                         */
/************************************************************************/

class VSIIcechunkFileSystem final : public VSIFilesystemHandler
{
  public:
    // If the input of SplitFilename() is /vsiicechunk/{/path/to/some/icechunk/repo?branch=my_branch]}/optional_key
    // - osRootFilenameWithBranchOrTag = "/path/to/some/icechunk/repo?branch=my_branch"
    // - osRootFilename = "/path/to/some/icechunk/repo"
    // - osBranchName = "my_branch"
    // - osKey = "/optional_key"
    //
    // If /optional_key is not present, osKey is set to "/"
    struct Ref
    {
        std::string osRootFilenameWithBranchOrTag{};
        std::string osRootFilename{};
        std::string osBranchName{};
        std::string osTagName{};
        std::string osKey{};
        bool ignoreTimestampEtag = false;
    };

    VSIIcechunkFileSystem()
    {
        bool *pbInstantiated = &IsFileSystemInstantiated();
        *pbInstantiated = true;
    }

    ~VSIIcechunkFileSystem() override;

    static bool &IsFileSystemInstantiated()
    {
        static bool bIsFileSystemInstantiated = false;
        return bIsFileSystemInstantiated;
    }

    VSIVirtualHandleUniquePtr Open(const char *pszFilename,
                                   const char *pszAccess, bool bSetError,
                                   CSLConstList papszOptions) override;

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;

    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;

    void ClearCaches();

    char **GetFileMetadata(const char *pszFilename, const char *pszDomain,
                           CSLConstList papszOptions) override;

  private:
    using RepoAndSnapshot = std::pair<std::shared_ptr<IcechunkRepo>,
                                      std::shared_ptr<IcechunkSnapshot>>;

    lru11::Cache<std::string, RepoAndSnapshot, std::mutex> m_oCache{};

    static Ref SplitFilename(const char *pszFilename);

    RepoAndSnapshot Load(const Ref &ref);
    std::pair<VSIIcechunkFileSystem::Ref, RepoAndSnapshot>
    Load(const char *pszFilename);

    struct FileInfo
    {
        bool bIsDir = false;
        std::string osFilename{};
        uint64_t nOffset = 0;
        uint64_t nSize = 0;
        const void *pabyData = nullptr;

        // To keep pabyData alive
        std::shared_ptr<IcechunkFile> dataOwner{};
    };

    FileInfo GetFileInfo(const char *pszFilename);
};

/************************************************************************/
/*                       ~VSIIcechunkFileSystem()                       */
/************************************************************************/

VSIIcechunkFileSystem::~VSIIcechunkFileSystem()
{
    bool *pbInstantiated = &IsFileSystemInstantiated();
    *pbInstantiated = false;
}

/************************************************************************/
/*                            ClearCaches()                             */
/************************************************************************/

void VSIIcechunkFileSystem::ClearCaches()
{
    m_oCache.clear();
}

/************************************************************************/
/*                VSIIcechunkFileSystem::SplitFilename()                */
/************************************************************************/

/** Decompose a filename into a repository root file name, a branch/tag name
 * and a key.
 */

/*static*/
VSIIcechunkFileSystem::Ref
VSIIcechunkFileSystem::SplitFilename(const char *pszFilename)
{
    Ref ref{};
    if (!STARTS_WITH(pszFilename, FS_PREFIX))
        return ref;

    std::string osRootFilename;

    pszFilename += strlen(FS_PREFIX);

    if (*pszFilename == '{')
    {
        // Parse /vsiicechunk/{/path/to/some/icechunk/repo[?[branch|tag]=<name>]}[/optional_key]
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
        if (nLevel == 0)
        {
            ref.osRootFilenameWithBranchOrTag = osRootFilename;

            ref.osRootFilename = GetFilenameFromDatasetName(
                osRootFilename, ref.osBranchName, ref.osTagName,
                ref.ignoreTimestampEtag);
            if (ref.osBranchName.empty() && ref.osTagName.empty())
                ref.osBranchName = "main";

            ref.osKey = *pszFilename == 0 ? "/" : pszFilename;
        }
    }
    if (ref.osRootFilename.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid %s syntax for \"%s\": should be "
                 "%s{/path/to/some/icechunk/repo[?[branch|tag]=<name>]}[/"
                 "optional_key]",
                 FS_PREFIX, pszFilename, FS_PREFIX);
    }
    return ref;
}

/************************************************************************/
/*                    VSIIcechunkFileSystem::Load()                     */
/************************************************************************/

/** Load the repo and snapshot files associated to the passed ref.
 *
 * Uses an internal cache.
 */
VSIIcechunkFileSystem::RepoAndSnapshot
VSIIcechunkFileSystem::Load(const Ref &ref)
{
    VSIIcechunkFileSystem::RepoAndSnapshot repoAndSnapshot;
    if (!m_oCache.tryGet(ref.osRootFilenameWithBranchOrTag, repoAndSnapshot))
    {
        auto repo = IcechunkRepo::Open(ref.osRootFilename.c_str());
        if (repo)
        {
            auto snapshot = !ref.osBranchName.empty()
                                ? repo->OpenSnapshotOnBranch(ref.osBranchName)
                                : repo->OpenSnapshotOnTag(ref.osTagName);
            if (snapshot)
            {
                repoAndSnapshot.first = std::move(repo);
                repoAndSnapshot.second = std::move(snapshot);
                m_oCache.insert(ref.osRootFilenameWithBranchOrTag,
                                repoAndSnapshot);
            }
        }
    }
    return repoAndSnapshot;
}

/************************************************************************/
/*                    VSIIcechunkFileSystem::Load()                     */
/************************************************************************/

/** Load the repo and snapshot files associated to the passed filename.
 *
 * Uses an internal cache.
 */
std::pair<VSIIcechunkFileSystem::Ref, VSIIcechunkFileSystem::RepoAndSnapshot>
VSIIcechunkFileSystem::Load(const char *pszFilename)
{
    const auto ref = SplitFilename(pszFilename);
    if (ref.osRootFilename.empty())
        return {};

    return {ref, Load(ref)};
}

/************************************************************************/
/*                          GetChunkIndices()                           */
/************************************************************************/

static ChunkIdx GetChunkIndices(const IcechunkSnapshot::Node &node,
                                const char *pszChunkIndices)
{
    const CPLStringList aosChunkIdx(
        CSLTokenizeString2(pszChunkIndices, "/", 0));
    ChunkIdx anChunkIdx;
    if (static_cast<size_t>(aosChunkIdx.size()) <= node.numChunks.size())
    {
        for (int i = 0; i < aosChunkIdx.size(); ++i)
        {
            if (CPLGetValueType(aosChunkIdx[i]) != CPL_VALUE_INTEGER)
                return {};
            const int nIdx = atoi(aosChunkIdx[i]);
            if (nIdx < 0 || static_cast<unsigned>(nIdx) >= node.numChunks[i])
                return {};
            anChunkIdx.push_back(static_cast<unsigned>(nIdx));
        }
    }
    return anChunkIdx;
}

/************************************************************************/
/*                            GetChunkRef()                             */
/************************************************************************/

static std::pair<std::shared_ptr<IcechunkManifest>,
                 const IcechunkManifest::ChunkRef *>
GetChunkRef(const IcechunkRepo &repo, const IcechunkSnapshot &snapshot,
            const IcechunkSnapshot::Node &node, const ChunkIdx &anChunkIdx)
{
    const auto *manifestId = node.findManifestIdForChunk(anChunkIdx);
    if (!manifestId)
    {
        // This is not necessary an error. This can happen for sparse chunks.
        return {};
    }

    const auto *manifestInfo = snapshot.GetManifestInfoFromId(*manifestId);
    if (!manifestInfo)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Manifest %s not referenced in snapshot %s",
                 CrockfordBase32Encode(*manifestId).c_str(),
                 snapshot.GetFilename().c_str());
        return {};
    }

    auto manifest =
        repo.OpenManifest(manifestInfo->strId, manifestInfo->sizeBytes,
                          manifestInfo->numChunkRefs);
    if (!manifest)
    {
        // OpenManifest() will have emitted an error
        return {};
    }

    const auto *chunkRef = manifest->GetChunkRef(node.id, anChunkIdx);
    return {std::move(manifest), chunkRef};
}

/************************************************************************/
/*                          GetChunkFilename()                          */
/************************************************************************/

static std::string GetChunkFilename(const IcechunkManifest &manifest,
                                    const IcechunkManifest::ChunkRef &chunkRef)
{
    std::string osChunkFilename;
    if (!chunkRef.chunkId.empty())
    {
        osChunkFilename = manifest.GetChunkFilename(chunkRef.chunkId);
    }
    else
    {
        static const struct
        {
            const char *pszStandardPrefix;
            const char *pszVSIPrefix;
        } asPrefixes[] = {
            {"s3://", "/vsis3/"},
            {"gs://", "/vsigs/"},
            {"gcs://", "/vsigs/"},
            {"az://", "/vsiaz/"},
            {"azure://", "/vsiaz/"},
            {"http://", "/vsicurl/http://"},
            {"https://", "/vsicurl/https://"},
        };

        for (const auto &sPrefix : asPrefixes)
        {
            if (cpl::starts_with(chunkRef.location, sPrefix.pszStandardPrefix))
            {
                osChunkFilename = std::string(sPrefix.pszVSIPrefix)
                                      .append(chunkRef.location.substr(
                                          strlen(sPrefix.pszStandardPrefix)));
                break;
            }
        }
        if (osChunkFilename.empty())
        {
            if (CPLTestBool(CPLGetConfigOption(
                    "ICECHUNK_ALLOW_LOCAL_CHUNK_LOCATION", "NO")))
            {
                osChunkFilename = chunkRef.location;
            }
            else
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Access to non-network chunk location '%s' disabled by "
                    "default. Set the ICECHUNK_ALLOW_LOCAL_CHUNK_LOCATION "
                    "cICECHUNK_ALLOW_LOCAL_CHUNK_LOCATION configuration option "
                    "to YES to enable it.",
                    chunkRef.location.c_str());
            }
        }
    }
    return osChunkFilename;
}

/************************************************************************/
/*                 VSIIcechunkFileSystem::GetFileInfo()                 */
/************************************************************************/

VSIIcechunkFileSystem::FileInfo
VSIIcechunkFileSystem::GetFileInfo(const char *pszFilename)
{
    FileInfo info;

    auto [ref, repoAndSnapshot] = Load(pszFilename);
    const auto &[repo, snapshot] = repoAndSnapshot;
    if (!snapshot)
        return info;

    // Deal with chunk directory
    const auto nLastCPos = ref.osKey.rfind("/c");
    if (nLastCPos != std::string::npos)
    {
        const std::string osTmpKey = ref.osKey.substr(0, nLastCPos);
        const auto *nodePtr = snapshot->GetNodeFromPath(osTmpKey);
        if (nodePtr && nodePtr->isArray)
        {
            const auto &node = *nodePtr;
            ChunkIdx anChunkIdx;
            if (nLastCPos + 2 == ref.osKey.size())
            {
                info.bIsDir = !node.numChunks.empty();
            }
            else
            {
                if (node.numChunks.empty())
                    return info;
                anChunkIdx = GetChunkIndices(
                    node, ref.osKey.substr(nLastCPos + 2).c_str());
            }

            if (!info.bIsDir)
            {
                if (anChunkIdx.empty() && !node.numChunks.empty())
                {
                    // wrong path
                }
                else if (anChunkIdx.size() < node.numChunks.size())
                {
                    info.bIsDir = true;
                }
                else
                {
                    const auto [manifest, chunkRef] =
                        GetChunkRef(*repo, *snapshot, node, anChunkIdx);
                    if (chunkRef)
                    {
                        if (chunkRef->length)
                        {
                            info.osFilename =
                                GetChunkFilename(*manifest, *chunkRef);
                            if (!info.osFilename.empty())
                            {
                                if (!ref.ignoreTimestampEtag &&
                                    chunkRef->checksumLastModified > 0)
                                {
                                    VSIStatBufL sStat;
                                    if (VSIStatL(info.osFilename.c_str(),
                                                 &sStat) != 0)
                                    {
                                        CPLError(CE_Failure, CPLE_AppDefined,
                                                 "Stat() on %s failed",
                                                 info.osFilename.c_str());
                                        return {};
                                    }
                                    if (static_cast<int64_t>(sStat.st_mtime) !=
                                        static_cast<int64_t>(
                                            chunkRef->checksumLastModified))
                                    {
                                        CPLError(
                                            CE_Failure, CPLE_AppDefined,
                                            "Last modified timestamp "
                                            "verification on %s failed: got "
                                            "%" PRId64
                                            ", expected %u. If you want to "
                                            "ignore this check, append "
                                            "'?ignore-timestamp-etag=yes' to "
                                            "the connection string",
                                            info.osFilename.c_str(),
                                            static_cast<int64_t>(
                                                sStat.st_mtime),
                                            chunkRef->checksumLastModified);
                                        return {};
                                    }
                                }

                                info.nOffset = chunkRef->offset;
                                info.nSize = chunkRef->length;
                            }
                        }
                        else
                        {
                            info.nSize = chunkRef->inlineContent.size();
                            info.pabyData = chunkRef->inlineContent.data();
                            info.dataOwner = std::move(manifest);
                        }
                    }
                }
            }

            return info;
        }
    }

    bool bIsZarrDotJson = false;
    std::string key = ref.osKey;
    if (cpl::ends_with(key, "/zarr.json"))
    {
        bIsZarrDotJson = true;
        key.resize(key.size() - strlen("/zarr.json"));
        if (key.empty())
            key = "/";
    }

    if (key == "/" && snapshot->GetNodeCount() <= 1)
    {
        info.bIsDir = true;
    }
    else if (const auto *node = snapshot->GetNodeFromPath(key))
    {
        if (bIsZarrDotJson)
        {
            info.nSize = node->content.size();
            info.pabyData = node->content.data();
            info.dataOwner = std::move(snapshot);
        }
        else
        {
            info.bIsDir = true;
        }
    }

    return info;
}

/************************************************************************/
/*                    VSIIcechunkFileSystem::Open()                     */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSIIcechunkFileSystem::Open(const char *pszFilename, const char *pszAccess,
                            bool /* bSetError */,
                            CSLConstList /* papszOptions */)
{
    CPLDebugOnly("VSIIcechunkFileSystem", "Open(%s)", pszFilename);
    if (strcmp(pszAccess, "r") != 0 && strcmp(pszAccess, "rb") != 0)
        return nullptr;

    auto info = GetFileInfo(pszFilename);
    if (!info.osFilename.empty())
    {
        CPLConfigOptionSetter oSetter("GDAL_DISABLE_READDIR_ON_OPEN",
                                      "EMPTY_DIR", false);

        const std::string osSubfileName =
            CPLSPrintf("/vsisubfile/%" PRIu64 "_%" PRIu64 ",%s", info.nOffset,
                       info.nSize, info.osFilename.c_str());
        auto fp = VSIFilesystemHandler::OpenStatic(osSubfileName.c_str(), "rb");
        if (fp)
        {
            VSIStatBufL sStat;
            if (VSIStatL(info.osFilename.c_str(), &sStat) != 0 ||
                info.nOffset + info.nSize >
                    static_cast<uint64_t>(sStat.st_size))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "(offset,length)=(%" PRIu64 ",%" PRIu64
                         ") beyond %s size",
                         info.nOffset, info.nSize, info.osFilename.c_str());
            }
            else
            {
                return fp;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                     info.osFilename.c_str());
        }
    }
    else if (info.dataOwner)
    {
        const size_t nSize = static_cast<size_t>(info.nSize);
        if constexpr (sizeof(size_t) < sizeof(uint64_t))
        {
            // Guaranteed because info.nSize is the result of container.size()
            CPLAssert(nSize == info.nSize);
        }
        GByte *pabyData = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nSize));
        if (pabyData)
        {
            memcpy(pabyData, info.pabyData, nSize);
            return VSIVirtualHandleUniquePtr(
                VSIFileFromMemBuffer(nullptr, pabyData, nSize,
                                     /* bTakeOwnership = */ true));
        }
    }

    return nullptr;
}

/************************************************************************/
/*                    VSIIcechunkFileSystem::Stat()                     */
/************************************************************************/

int VSIIcechunkFileSystem::Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
                                int /* nFlags */)
{
    CPLDebugOnly("VSIIcechunkFileSystem", "Stat(%s)", pszFilename);
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    int nRet = -1;
    auto info = GetFileInfo(pszFilename);
    if (!info.osFilename.empty())
    {
        CPLConfigOptionSetter oSetter("GDAL_DISABLE_READDIR_ON_OPEN",
                                      "EMPTY_DIR", false);
        nRet = VSIStatL(info.osFilename.c_str(), pStatBuf);
        if (nRet == 0)
        {
            if (info.nOffset + info.nSize >
                static_cast<uint64_t>(pStatBuf->st_size))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "(offset,length)=(%" PRIu64 ",%" PRIu64
                         ") beyond %s size",
                         info.nOffset, info.nSize, info.osFilename.c_str());
                nRet = -1;
            }
            else
            {
                pStatBuf->st_size = info.nSize;
            }
        }
    }
    else if (info.dataOwner)
    {
        nRet = VSIStatL(info.dataOwner->GetFilename().c_str(), pStatBuf);
        if (nRet == 0)
        {
            pStatBuf->st_mode = S_IFREG;
            pStatBuf->st_size = info.nSize;
        }
    }
    else if (info.bIsDir)
    {
        nRet = 0;
        pStatBuf->st_mode = S_IFDIR;
    }

    return nRet;
}

/************************************************************************/
/*               VSIIcechunkFileSystem::GetFileMetadata()               */
/************************************************************************/

char **VSIIcechunkFileSystem::GetFileMetadata(const char *pszFilename,
                                              const char *pszDomain,
                                              CSLConstList /* papszOptions */)
{
    if (!pszDomain || !EQUAL(pszDomain, "CHUNK_INFO"))
        return nullptr;

    CPLStringList aosMetadata;

    auto info = GetFileInfo(pszFilename);
    if (!info.osFilename.empty())
    {
        aosMetadata.SetNameValue("SIZE", CPLSPrintf("%" PRIu64, info.nSize));
        aosMetadata.SetNameValue("OFFSET",
                                 CPLSPrintf("%" PRIu64, info.nOffset));
        aosMetadata.SetNameValue("FILENAME", info.osFilename.c_str());
    }
    else if (info.dataOwner)
    {
        aosMetadata.SetNameValue("SIZE", CPLSPrintf("%" PRIu64, info.nSize));
        if (info.nSize <
            static_cast<size_t>(std::numeric_limits<int>::max() - 1))
        {
            char *pszBase64 =
                CPLBase64Encode(static_cast<int>(info.nSize),
                                static_cast<const GByte *>(info.pabyData));
            aosMetadata.SetNameValue("BASE64", pszBase64);
            CPLFree(pszBase64);
        }
    }

    return aosMetadata.StealList();
}

/************************************************************************/
/*                  VSIIcechunkFileSystem::ReadDirEx()                  */
/************************************************************************/

char **VSIIcechunkFileSystem::ReadDirEx(const char *pszDirname, int nMaxFiles)
{
    CPLDebugOnly("VSIIcechunkFileSystem", "ReadDirEx(%s, %d)", pszDirname,
                 nMaxFiles);

    auto [ref, repoAndSnapshot] = Load(pszDirname);
    const auto &[repo, snapshot] = repoAndSnapshot;
    if (!snapshot)
        return nullptr;

    CPLStringList aosFiles;

    // Deal with chunk directory
    const auto nLastCPos = ref.osKey.rfind("/c");
    if (nLastCPos != std::string::npos)
    {
        const std::string osTmpKey = ref.osKey.substr(0, nLastCPos);
        const auto *nodePtr = snapshot->GetNodeFromPath(osTmpKey);
        if (nodePtr && nodePtr->isArray)
        {
            const auto &node = *nodePtr;

            if (!(nLastCPos + 2 == ref.osKey.size() && node.numChunks.empty()))
            {
                auto anChunkIdx = GetChunkIndices(
                    node, ref.osKey.substr(nLastCPos + 2).c_str());
                if (anChunkIdx.size() < node.numChunks.size() &&
                    !(anChunkIdx.empty() && nLastCPos + 2 != ref.osKey.size()))
                {
                    const size_t iDim = anChunkIdx.size();
                    anChunkIdx.push_back(0);
                    for (uint32_t i = 0; i < node.numChunks[iDim]; ++i)
                    {
                        if (iDim + 1 == node.numChunks.size())
                        {
                            anChunkIdx.back() = i;
                            if (!node.findManifestIdForChunk(anChunkIdx))
                                continue;
                        }
                        aosFiles.push_back(std::to_string(i));
                        if (nMaxFiles > 0 && aosFiles.size() == nMaxFiles)
                            break;
                    }
                }
            }

            return aosFiles.StealList();
        }
    }

    CPLAssert(ref.osKey == "/" ||
              (!ref.osKey.empty() && ref.osKey.back() != '/'));
    const std::string refKeyWithTrailingSlash =
        ref.osKey == "/" ? ref.osKey : std::string(ref.osKey).append("/");
    for (const auto &node : snapshot->GetNodes())
    {
        CPLAssert(node.path == "/" ||
                  (!node.path.empty() && node.path.back() != '/'));
        if (node.path == ref.osKey)
        {
            aosFiles.push_back("zarr.json");
            if (node.isArray)
            {
                aosFiles.push_back("c");
            }
        }
        else if (cpl::starts_with(node.path, refKeyWithTrailingSlash))
        {
            std::string dirName =
                node.path.substr(refKeyWithTrailingSlash.size());
            if (dirName.find('/') == std::string::npos)
            {
                aosFiles.push_back(dirName);
            }
        }
        if (nMaxFiles > 0 && aosFiles.size() == nMaxFiles)
            break;
    }

    return aosFiles.StealList();
}

/************************************************************************/
/*                    VSIInstallIcechunkFileSystem()                    */
/************************************************************************/

void VSIInstallIcechunkFileSystem()
{
    static std::mutex oMutex;
    std::lock_guard<std::mutex> oLock(oMutex);
    // cppcheck-suppress knownConditionTrueFalse
    if (!VSIIcechunkFileSystem::IsFileSystemInstantiated())
    {
        VSIFileManager::InstallHandler(
            FS_PREFIX, std::make_shared<VSIIcechunkFileSystem>());
    }
}

/************************************************************************/
/*                  VSIIcechunkFileSystemClearCaches()                  */
/************************************************************************/

void VSIIcechunkFileSystemClearCaches()
{
    auto poFS = dynamic_cast<VSIIcechunkFileSystem *>(
        VSIFileManager::GetHandler(FS_PREFIX));
    if (poFS)
        poFS->ClearCaches();
}

}  // namespace gdal::icechunk

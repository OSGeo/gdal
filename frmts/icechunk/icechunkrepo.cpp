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

#include "icechunkrepo.h"
#include "icechunkmanifest.h"
#include "icechunksnapshot.h"
#include "icechunkutils.h"
#include "icechunkdrivercore.h"

#include "cpl_json.h"
#include "cpl_mem_cache.h"

#include <cinttypes>
#include <limits>
#include <mutex>

/* ------------------------------------------------------------------------- */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

#include "generated/repo_generated.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* ------------------------------------------------------------------------- */

using namespace flatbuffers;
using namespace generated;

namespace gdal::icechunk
{
IcechunkFile::IcechunkFile() = default;

IcechunkFile::~IcechunkFile() = default;

IcechunkRepo::IcechunkRepo() = default;

IcechunkRepo::~IcechunkRepo() = default;

/************************************************************************/
/*                        IcechunkRepo::OpenV1()                        */
/************************************************************************/

/* static */
std::unique_ptr<IcechunkRepo> IcechunkRepo::OpenV1(const char *pszRootPath)
{
    auto repo = std::make_unique<IcechunkRepo>();
    repo->m_osRootPath = pszRootPath;

    const std::string osRefsDir =
        CPLFormFilenameSafe(pszRootPath, "refs", nullptr);
    const CPLStringList aosRefs(VSIReadDir(osRefsDir.c_str()));
    for (const char *pszRef : aosRefs)
    {
        if (strcmp(pszRef, ".") != 0 && strcmp(pszRef, "..") != 0)
        {
            const std::string osRefDir =
                CPLFormFilenameSafe(osRefsDir.c_str(), pszRef, nullptr);
            if (STARTS_WITH(pszRef, "branch."))
            {
                CPLJSONDocument oDoc;
                if (oDoc.Load(CPLFormFilenameSafe(osRefDir.c_str(), "ref.json",
                                                  nullptr)))
                {
                    const auto osSnapshotId =
                        oDoc.GetRoot().GetString("snapshot");
                    repo->m_oMapBranchNameToSnapshotId[pszRef +
                                                       strlen("branch.")] =
                        osSnapshotId;
                }
            }
            else if (STARTS_WITH(pszRef, "tag."))
            {
                CPLJSONDocument oDoc;
                if (oDoc.Load(CPLFormFilenameSafe(osRefDir.c_str(), "ref.json",
                                                  nullptr)))
                {
                    const auto osSnapshotId =
                        oDoc.GetRoot().GetString("snapshot");
                    repo->m_oMapTagNameToSnapshotId[pszRef + strlen("tag.")] =
                        osSnapshotId;
                }
            }
        }
    }

    return repo;
}

/************************************************************************/
/*                           ProcessConfig()                            */
/************************************************************************/

static void ProcessConfig(const CPLJSONObject &oConfig)
{
    const auto oVCC = oConfig["virtual_chunk_containers"];
    if (oVCC.GetType() == CPLJSONObject::Type::Object)
    {
        for (const auto &oVCCChild : oVCC.GetChildren())
        {
            const auto osURLPrefix = oVCCChild.GetString("url_prefix");
            const auto oStore = oVCCChild["store"];
            if (cpl::starts_with(osURLPrefix, "s3://") &&
                oStore.GetType() == CPLJSONObject::Type::Object)
            {
                const auto oS3 = oStore["s3"];
                if (oS3.GetType() == CPLJSONObject::Type::Object)
                {
                    std::string osPath("/vsis3/");
                    osPath += osURLPrefix.substr(strlen("s3://"));

                    const std::string osRegion = oS3.GetString("region");
                    if (!osRegion.empty())
                    {
                        CPLDebug("Icechunk", "Set AWS_DEFAULT_REGION=%s for %s",
                                 osRegion.c_str(), osPath.c_str());
                        VSISetPathSpecificOption(osPath.c_str(),
                                                 "AWS_DEFAULT_REGION",
                                                 osRegion.c_str());
                    }

                    const bool bAnonymous = oS3.GetBool("anonymous");
                    if (bAnonymous)
                    {
                        CPLDebug("Icechunk",
                                 "Set AWS_NO_SIGN_REQUEST=YES for %s",
                                 osPath.c_str());
                        VSISetPathSpecificOption(osPath.c_str(),
                                                 "AWS_NO_SIGN_REQUEST", "YES");
                    }

                    const bool bRequesterPays = oS3.GetBool("requester_pays");
                    if (bRequesterPays &&
                        !CPLTestBool(VSIGetPathSpecificOption(
                            osPath.c_str(), "AWS_REQUEST_PAYER", "NO")))
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "AWS_REQUEST_PAYER=YES must be set to "
                                 "access %s",
                                 osPath.c_str());
                    }
                }
            }
            else if ((cpl::starts_with(osURLPrefix, "gs://") ||
                      cpl::starts_with(osURLPrefix, "gcs://")) &&
                     oStore.GetType() == CPLJSONObject::Type::Object)
            {
                const auto oGCS = oStore["gcs"];
                if (oGCS.GetType() == CPLJSONObject::Type::Object)
                {
                    std::string osPath("/vsigs/");
                    osPath += osURLPrefix.substr(osURLPrefix.find("://") + 3);

                    const auto bAnonymous = oGCS.GetBool("anonymous");
                    if (bAnonymous)
                    {
                        CPLDebug("Icechunk",
                                 "Set GS_NO_SIGN_REQUEST=YES for %s",
                                 osPath.c_str());
                        VSISetPathSpecificOption(osPath.c_str(),
                                                 "GS_NO_SIGN_REQUEST", "YES");
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                         IcechunkRepo::Open()                         */
/************************************************************************/

/** Open a "repo" file */
/* static */
std::unique_ptr<IcechunkRepo> IcechunkRepo::Open(const char *pszFilename,
                                                 VSIVirtualHandle *fp)
{
    CPLDebugOnly("Icechunk", "Opening repo %s", pszFilename);
    std::string osFilename(pszFilename);
    VSIVirtualHandleUniquePtr tmpFp;
    if (!fp)
    {
        for (int iAttempt = 1; iAttempt <= 2; ++iAttempt)
        {
            if (iAttempt == 2 ||
                strcmp(CPLGetFilename(pszFilename), "repo") != 0)
            {
                osFilename =
                    CPLFormFilenameSafe(osFilename.c_str(), "repo", nullptr);
                tmpFp =
                    VSIFilesystemHandler::OpenStatic(osFilename.c_str(), "rb");
                // For network file systems, try to read one byte
                char chDummy = 1;
                if (tmpFp && tmpFp->Read(&chDummy, 1) != 1)
                {
                    tmpFp.reset();
                }

                if (tmpFp)
                {
                    CPL_IGNORE_RET_VAL(tmpFp->Seek(0, SEEK_SET));
                    pszFilename = osFilename.c_str();
                }
                else
                {
                    VSIStatBufL sStat;
                    if (VSIStatL(
                            CPLFormFilenameSafe(pszFilename, "refs", nullptr)
                                .c_str(),
                            &sStat) == 0)
                    {
                        // Icechunk v1
                        return OpenV1(pszFilename);
                    }
                }
                break;
            }
            else
            {
                tmpFp = VSIFilesystemHandler::OpenStatic(pszFilename, "rb");
                // For network file systems, try to read one byte
                char chDummy = 1;
                if (!tmpFp || tmpFp->Read(&chDummy, 1) != 1)
                {
                    tmpFp.reset();
                    // This might be a repository named "repo". Retry with
                    // that assumption
                }
                else
                {
                    if (tmpFp)
                        CPL_IGNORE_RET_VAL(tmpFp->Seek(0, SEEK_SET));
                    break;
                }
            }
        }

        fp = tmpFp.get();
        if (!fp)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszFilename);
            return nullptr;
        }
    }

    int nFileVersion = 0;
    auto [buffer, size] =
        DecompressFile(pszFilename, fp, FILE_TYPE_REPO_INFO, &nFileVersion);
    if (!buffer)
        return nullptr;

    {
        Verifier verifier(buffer.get(), size);
        if (!VerifyRepoBuffer(verifier))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: invalid Repo Flatbuffer",
                     pszFilename);
            return nullptr;
        }
    }

    auto repo = std::make_unique<IcechunkRepo>();
    repo->m_osRootPath = CPLGetPathSafe(pszFilename);

    const Repo *repoPtr = GetRepo(buffer.get());

    const int nSpecVersion = repoPtr->spec_version();
    CPLDebugOnly("Icechunk", "Repo spec_version = %d", nSpecVersion);
    if (nSpecVersion != 1 && nSpecVersion != 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: invalid spec_version %d",
                 pszFilename, nSpecVersion);
        return nullptr;
    }
    if (nFileVersion != nSpecVersion)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s: file version=%d != spec_version=%d", pszFilename,
                 nFileVersion, nSpecVersion);
        return nullptr;
    }

    const auto status = repoPtr->status();
    if (status)
    {
        const auto availability = status->availability();
        if (availability == RepoAvailability::Offline)
        {
            const auto reason = status->limited_availability_reason();
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: repository is offline: %s", pszFilename,
                     reason ? reason->c_str() : "unknown reason");
            return nullptr;
        }
    }

    {
        const auto metadata = repoPtr->metadata();
        if (metadata && nSpecVersion == 2)
        {
            for (const auto &md : *metadata)
            {
                if (const auto value = md->value())
                {
                    if (!flexbuffers::VerifyBuffer(value->data(),
                                                   value->size()))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: flexbuffers::VerifyBuffer() failed",
                                 pszFilename);
                        return nullptr;
                    }
                    if constexpr (IS_DEBUG_BUILD)
                    {
                        std::string val;
                        flexbuffers::GetRoot(value->data(), value->size())
                            .ToString(true, true, val);
                        CPLDebugOnly("Icechunk", "metadata %s=%s",
                                     md->name() ? md->name()->c_str()
                                                : "(null)",
                                     val.c_str());
                    }
                }
            }
        }
    }

    if (const auto config = repoPtr->config())
    {
        if (!flexbuffers::VerifyBuffer(config->data(), config->size()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: flexbuffers::VerifyBuffer() failed", pszFilename);
            return nullptr;
        }
        std::string val;
        flexbuffers::GetRoot(config->data(), config->size())
            .ToString(true, true, val);
        CPLDebugOnly("Icechunk", "config %s", val.c_str());
        CPLJSONDocument oDoc;
        if (!oDoc.LoadMemory(val))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: invalid configuration: %s", pszFilename, val.c_str());
            return nullptr;
        }
        ProcessConfig(oDoc.GetRoot());
    }

    const auto snapshots = repoPtr->snapshots();
    CPLAssertAlways(snapshots);  // guaranteed by VerifyRepoBuffer()
    for (uint32_t snapshotIdx = 0; snapshotIdx < snapshots->size();
         ++snapshotIdx)
    {
        const auto *snapshot = (*snapshots)[snapshotIdx];
        const auto *id = snapshot->id();
        CPLAssertNotNull(id);           // guaranteed by VerifyRepoBuffer()
        CPLAssertNotNull(id->bytes());  // guaranteed by VerifyRepoBuffer()

        const auto *message = snapshot->message();
        CPLAssertNotNull(message);  // guaranteed by VerifyRepoBuffer()

        CPLDebugOnly("Icechunk",
                     "snapshot '%s', parent_offset %d, message '%s'",
                     CrockfordBase32Encode(*(id->bytes())).c_str(),
                     snapshot->parent_offset(), message->c_str());

        const auto metadata = snapshot->metadata();
        if (metadata && nSpecVersion == 2)
        {
            for (const auto &md : *metadata)
            {
                if (const auto value = md->value())
                {
                    if (!flexbuffers::VerifyBuffer(value->data(),
                                                   value->size()))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: flexbuffers::VerifyBuffer() failed",
                                 pszFilename);
                        return nullptr;
                    }
                    if constexpr (IS_DEBUG_BUILD)
                    {
                        std::string val;
                        flexbuffers::GetRoot(value->data(), value->size())
                            .ToString(true, true, val);
                        CPLDebugOnly("Icechunk", "  metadata %s=%s",
                                     md->name() ? md->name()->c_str()
                                                : "(null)",
                                     val.c_str());
                    }
                }
            }
        }
    }

    // Parse tags
    const auto tags = repoPtr->tags();
    CPLAssertAlways(tags);  // guaranteed by VerifyRepoBuffer()
    for (const auto &ref : *tags)
    {
        const auto *refName = ref->name();
        CPLAssertNotNull(refName);  // guaranteed by VerifyRepoBuffer()
        CPLDebugOnly("Icechunk", "tag '%s', snapshot_index %u",
                     refName->c_str(), ref->snapshot_index());

        if (ref->snapshot_index() >= snapshots->size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: tag '%s', invalid snapshot_index %u", pszFilename,
                     refName->c_str(), ref->snapshot_index());
            return nullptr;
        }

        const auto *snapshot = (*snapshots)[ref->snapshot_index()];
        const auto *id = snapshot->id();
        CPLAssertNotNull(id);  // guaranteed by VerifyRepoBuffer()
        if (!repo->m_oMapTagNameToSnapshotId
                 .insert({GetString(refName),
                          CrockfordBase32Encode(*(id->bytes()))})
                 .second)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: more than one tag '%s'",
                     pszFilename, refName->c_str());
            return nullptr;
        }
    }

    // Parse branches
    const auto branches = repoPtr->branches();
    CPLAssertAlways(branches);  // guaranteed by VerifyRepoBuffer()
    for (const auto &ref : *branches)
    {
        const auto *refName = ref->name();
        CPLAssertNotNull(refName);  // guaranteed by VerifyRepoBuffer()
        CPLDebugOnly("Icechunk", "branch '%s', snapshot_index %u",
                     refName->c_str(), ref->snapshot_index());

        if (ref->snapshot_index() >= snapshots->size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: branch '%s', invalid snapshot_index %u", pszFilename,
                     refName->c_str(), ref->snapshot_index());
            return nullptr;
        }

        const auto *snapshot = (*snapshots)[ref->snapshot_index()];
        const auto *id = snapshot->id();
        CPLAssertNotNull(id);  // guaranteed by VerifyRepoBuffer()
        if (!repo->m_oMapBranchNameToSnapshotId
                 .insert({GetString(refName),
                          CrockfordBase32Encode(*(id->bytes()))})
                 .second)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: more than one branch '%s'", pszFilename,
                     refName->c_str());
            return nullptr;
        }
    }

    return repo;
}

/************************************************************************/
/*                 IcechunkRepo::OpenSnapshotOnBranch()                 */
/************************************************************************/

/** Open the snapshot corresponding to the passed branch name. */
std::unique_ptr<IcechunkSnapshot>
IcechunkRepo::OpenSnapshotOnBranch(const std::string &name,
                                   bool emitErrorIfUnknownBranch) const
{
    const auto oIter = m_oMapBranchNameToSnapshotId.find(name);
    if (oIter == m_oMapBranchNameToSnapshotId.end())
    {
        if (emitErrorIfUnknownBranch)
            CPLError(CE_Failure, CPLE_AppDefined, "No branch '%s'",
                     name.c_str());
        return nullptr;
    }
    const auto &snapshotId = oIter->second;

    std::string osSnapshotFilename = CPLFormFilenameSafe(
        CPLFormFilenameSafe(m_osRootPath.c_str(), "snapshots", nullptr).c_str(),
        snapshotId.c_str(), nullptr);
    return IcechunkSnapshot::Open(osSnapshotFilename.c_str());
}

/************************************************************************/
/*                  IcechunkRepo::OpenSnapshotOnTag()                   */
/************************************************************************/

/** Open the snapshot corresponding to the passed tag name. */
std::unique_ptr<IcechunkSnapshot>
IcechunkRepo::OpenSnapshotOnTag(const std::string &name,
                                bool emitErrorIfUnknownTag) const
{
    const auto oIter = m_oMapTagNameToSnapshotId.find(name);
    if (oIter == m_oMapTagNameToSnapshotId.end())
    {
        if (emitErrorIfUnknownTag)
            CPLError(CE_Failure, CPLE_AppDefined, "No tag '%s'", name.c_str());
        return nullptr;
    }
    const auto &snapshotId = oIter->second;

    std::string osSnapshotFilename = CPLFormFilenameSafe(
        CPLFormFilenameSafe(m_osRootPath.c_str(), "snapshots", nullptr).c_str(),
        snapshotId.c_str(), nullptr);
    return IcechunkSnapshot::Open(osSnapshotFilename.c_str());
}

/************************************************************************/
/*                            GetRepoCache()                            */
/************************************************************************/

using CacheType =
    lru11::Cache<std::string, std::shared_ptr<IcechunkManifest>, std::mutex>;

static CacheType &GetRepoCache()
{
    static lru11::Cache<std::string, std::shared_ptr<IcechunkManifest>,
                        std::mutex>
        goCache;
    return goCache;
}

/************************************************************************/
/*                     IcechunkRepo::OpenManifest()                     */
/************************************************************************/

/** Open the manifest corresponding to the passed manifest id. */
std::shared_ptr<IcechunkManifest>
IcechunkRepo::OpenManifest(const std::string &manifestId,
                           uint64_t nExpectedFileSize,
                           uint32_t nExpectedChunkRefs) const
{
    CacheType &goCache = GetRepoCache();
    std::shared_ptr<IcechunkManifest> manifest;
    const std::string cacheKey =
        std::string(m_osRootPath).append("|").append(manifestId);
    if (goCache.tryGet(cacheKey, manifest))
        return manifest;

    std::string osFilename = CPLFormFilenameSafe(
        CPLFormFilenameSafe(m_osRootPath.c_str(), "manifests", nullptr).c_str(),
        manifestId.c_str(), nullptr);
    manifest = IcechunkManifest::Open(osFilename.c_str());
    if (manifest)
    {
        VSIStatBufL sStat;
        if (VSIStatL(manifest->GetFilename().c_str(), &sStat) == 0 &&
            static_cast<uint64_t>(sStat.st_size) != nExpectedFileSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Actual file size of manifest %s = %" PRIu64
                     " does not match expected one = %" PRIu64,
                     osFilename.c_str(), static_cast<uint64_t>(sStat.st_size),
                     nExpectedFileSize);
            manifest.reset();
        }
        else if (manifest->GetChunkRefsCount() != nExpectedChunkRefs)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Actual count of chunk references in manifest %s = %u "
                     "does not match expected one = %u",
                     osFilename.c_str(), manifest->GetChunkRefsCount(),
                     nExpectedChunkRefs);
            manifest.reset();
        }
        else
        {
            goCache.insert(cacheKey, manifest);
        }
    }
    return manifest;
}

/************************************************************************/
/*                     IcechunkRepo::ClearCaches()                      */
/************************************************************************/

/* static */ void IcechunkRepo::ClearCaches()
{
    CacheType &goCache = GetRepoCache();
    goCache.clear();
}

}  // namespace gdal::icechunk

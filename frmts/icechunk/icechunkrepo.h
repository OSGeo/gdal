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

#ifndef ICECHUNKREPO_H
#define ICECHUNKREPO_H

#include <string>
#include <memory>

#include "icechunkfile.h"
#include "cpl_vsi_virtual.h"

namespace gdal::icechunk
{
class IcechunkSnapshot;
class IcechunkManifest;

/************************************************************************/
/*                             IcechunkRepo                             */
/************************************************************************/

/** Instance of https://icechunk.io/en/stable/reference/spec/#repo-info-file
 *
 * It is thread-safe.
 */
class IcechunkRepo final : public IcechunkFile
{
  public:
    IcechunkRepo();
    ~IcechunkRepo() override;

    static std::unique_ptr<IcechunkRepo> Open(const char *pszFilename,
                                              VSIVirtualHandle *fp = nullptr);

    std::unique_ptr<IcechunkSnapshot>
    OpenSnapshotOnBranch(const std::string &id,
                         bool emitErrorIfUnknownBranch = true) const;

    std::unique_ptr<IcechunkSnapshot>
    OpenSnapshotOnTag(const std::string &id,
                      bool emitErrorIfUnknownTag = true) const;

    std::shared_ptr<IcechunkManifest>
    OpenManifest(const std::string &manifestId, uint64_t nExpectedFileSize,
                 uint32_t nExpectedChunkRefs) const;

    const std::map<std::string, std::string> &GetTags() const
    {
        return m_oMapTagNameToSnapshotId;
    }

    const std::map<std::string, std::string> &GetBranches() const
    {
        return m_oMapBranchNameToSnapshotId;
    }

    static void ClearCaches();

  private:
    std::string m_osRootPath{};
    std::map<std::string, std::string> m_oMapTagNameToSnapshotId{};
    std::map<std::string, std::string> m_oMapBranchNameToSnapshotId{};

    static std::unique_ptr<IcechunkRepo> OpenV1(const char *pszRootPath);
};

}  // namespace gdal::icechunk

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vfs copy" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VFS_COPY_INCLUDED
#define GDALALG_VFS_COPY_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                          GDALVFSCopyAlgorithm                        */
/************************************************************************/

class GDALVFSCopyAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "copy";
    static constexpr const char *DESCRIPTION =
        "Copy files located on GDAL Virtual file systems (VSI).";
    static constexpr const char *HELP_URL = "/programs/gdal_vfs_copy.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"cp"};
    }

    GDALVFSCopyAlgorithm();

  private:
    std::string m_source{};
    std::string m_destination{};
    bool m_recursive = false;
    bool m_skip = false;

    bool RunImpl(GDALProgressFunc, void *) override;

    bool CopySingle(const std::string &src, const std::string &dst,
                    uint64_t size, GDALProgressFunc pfnProgress,
                    void *pProgressData) const;

    bool CopyRecursive(const std::string &src, const std::string &dst,
                       int depth, int maxdepth, uint64_t &curAmount,
                       uint64_t totalAmount, GDALProgressFunc pfnProgress,
                       void *pProgressData) const;
};

//! @endcond

#endif

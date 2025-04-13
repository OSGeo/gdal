/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vfs delete" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Deleteright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VFS_DELETE_INCLUDED
#define GDALALG_VFS_DELETE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALVFSDeleteAlgorithm                        */
/************************************************************************/

class GDALVFSDeleteAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "delete";
    static constexpr const char *DESCRIPTION =
        "Delete files located on GDAL Virtual file systems (VSI).";
    static constexpr const char *HELP_URL = "/programs/gdal_vfs_delete.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"rm", "rmdir", "del"};
    }

    GDALVFSDeleteAlgorithm();

  private:
    std::string m_filename{};
    bool m_recursive = false;

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif

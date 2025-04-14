/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vfs" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"

#include "gdalalg_vfs_copy.h"
#include "gdalalg_vfs_delete.h"
#include "gdalalg_vfs_list.h"

/************************************************************************/
/*                           GDALVFSAlgorithm                           */
/************************************************************************/

class GDALVFSAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "vfs";
    static constexpr const char *DESCRIPTION =
        "GDAL Virtual file system (VSI) commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_vfs.html";

    GDALVFSAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALVFSCopyAlgorithm>();
        RegisterSubAlgorithm<GDALVFSDeleteAlgorithm>();
        RegisterSubAlgorithm<GDALVFSListAlgorithm>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "vfs\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALVFSAlgorithm);

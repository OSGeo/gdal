/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "fs" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"

#include "gdalalg_fs_ls.h"

/************************************************************************/
/*                           GDALFSAlgorithm                            */
/************************************************************************/

class GDALFSAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "fs";
    static constexpr const char *DESCRIPTION =
        "GDAL Virtual file system (VSI) commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_fs.html";

    GDALFSAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALFSListAlgorithm>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "fs\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALFSAlgorithm);

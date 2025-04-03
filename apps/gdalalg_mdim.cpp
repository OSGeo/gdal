/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"

#include "gdalalg_mdim_info.h"
#include "gdalalg_mdim_convert.h"

/************************************************************************/
/*                         GDALMdimAlgorithm                            */
/************************************************************************/

class GDALMdimAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "mdim";
    static constexpr const char *DESCRIPTION = "Multidimensional commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim.html";

    GDALMdimAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALMdimInfoAlgorithm>();
        RegisterSubAlgorithm<GDALMdimConvertAlgorithm>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "mdim\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALMdimAlgorithm);

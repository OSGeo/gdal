/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"

#include "gdalalg_raster_info.h"
#include "gdalalg_raster_convert.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_raster_reproject.h"

/************************************************************************/
/*                         GDALRasterAlgorithm                          */
/************************************************************************/

class GDALRasterAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "raster";
    static constexpr const char *DESCRIPTION = "Raster commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    GDALRasterAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALRasterInfoAlgorithm>();
        RegisterSubAlgorithm<GDALRasterConvertAlgorithm>();
        RegisterSubAlgorithm<GDALRasterPipelineAlgorithm>();
        RegisterSubAlgorithm<GDALRasterReprojectAlgorithmStandalone>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "raster\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALRasterAlgorithm);

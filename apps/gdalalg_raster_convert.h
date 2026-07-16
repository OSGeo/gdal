/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CONVERT_INCLUDED
#define GDALALG_RASTER_CONVERT_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALRasterConvertAlgorithm                      */
/************************************************************************/

class GDALRasterConvertAlgorithm final : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION = "Convert a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_convert.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR, "translate"};
    }

    explicit GDALRasterConvertAlgorithm(bool standalone = true,
                                        bool openForMixedRasterVector = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

//! @endcond

#endif

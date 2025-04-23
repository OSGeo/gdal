/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error.h"
#include "gdalalgorithm.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_vector_pipeline.h"
#include "gdalalg_dispatcher.h"
#include "gdal_priv.h"

/************************************************************************/
/*                       GDALPipelineAlgorithm                          */
/************************************************************************/

class GDALPipelineAlgorithm final
    : public GDALDispatcherAlgorithm<GDALRasterPipelineAlgorithm,
                                     GDALVectorPipelineAlgorithm>
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION =
        "Execute a pipeline (shortcut for 'gdal raster pipeline' or 'gdal "
        "vector pipeline').";
    static constexpr const char *HELP_URL = "/programs/gdal_pipeline.html";

    GDALPipelineAlgorithm()
        : GDALDispatcherAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        // only for the help message
        AddOutputFormatArg(&m_format).SetDefault("json").SetChoices("json",
                                                                    "text");
        AddInputDatasetArg(&m_dataset);

        m_longDescription =
            "For all options, run 'gdal raster pipeline --help' or "
            "'gdal vector pipeline --help'";
    }

  private:
    std::unique_ptr<GDALRasterPipelineAlgorithm> m_rasterPipeline{};
    std::unique_ptr<GDALVectorPipelineAlgorithm> m_vectorPipeline{};

    std::string m_format{};
    GDALArgDatasetValue m_dataset{};

    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "pipeline\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALPipelineAlgorithm);

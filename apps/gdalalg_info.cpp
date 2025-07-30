/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error.h"
#include "gdalalgorithm.h"
#include "gdalalg_raster_info.h"
#include "gdalalg_vector_info.h"
#include "gdalalg_dispatcher.h"
#include "gdal_priv.h"

/************************************************************************/
/*                          GDALInfoAlgorithm                           */
/************************************************************************/

class GDALInfoAlgorithm final
    : public GDALDispatcherAlgorithm<GDALRasterInfoAlgorithm,
                                     GDALVectorInfoAlgorithm>
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a dataset (shortcut for 'gdal raster info' or "
        "'gdal vector info').";
    static constexpr const char *HELP_URL = "/programs/gdal_info.html";

    GDALInfoAlgorithm() : GDALDispatcherAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        // only for the help message
        AddOutputFormatArg(&m_format).SetChoices("json", "text");
        AddInputDatasetArg(&m_dataset);

        m_longDescription = "For all options, run 'gdal raster info --help' or "
                            "'gdal vector info --help'";
    }

  private:
    std::unique_ptr<GDALRasterInfoAlgorithm> m_rasterInfo{};
    std::unique_ptr<GDALVectorInfoAlgorithm> m_vectorInfo{};

    std::string m_format{};
    GDALArgDatasetValue m_dataset{};

    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "info\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALInfoAlgorithm);

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error.h"
#include "gdalalgorithm.h"
#include "gdalalg_raster_convert.h"
#include "gdalalg_vector_convert.h"
#include "gdalalg_dispatcher.h"
#include "gdal_priv.h"

/************************************************************************/
/*                        GDALConvertAlgorithm                          */
/************************************************************************/

class GDALConvertAlgorithm
    : public GDALDispatcherAlgorithm<GDALRasterConvertAlgorithm,
                                     GDALVectorConvertAlgorithm>
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION =
        "Convert a dataset (shortcut for 'gdal raster convert' or "
        "'gdal vector convert').";
    static constexpr const char *HELP_URL = "/programs/gdal_convert.html";

    GDALConvertAlgorithm()
        : GDALDispatcherAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        // only for the help message
        AddProgressArg();
        AddOutputFormatArg(&m_format);
        AddInputDatasetArg(&m_inputDataset);
        AddOutputDatasetArg(&m_outputDataset);

        m_longDescription = "For all options, run 'gdal raster convert --help' "
                            "or 'gdal vector convert --help'";
    }

  private:
    std::string m_format{};
    GDALArgDatasetValue m_inputDataset{};
    GDALArgDatasetValue m_outputDataset{};
};

GDAL_STATIC_REGISTER_ALG(GDALConvertAlgorithm);

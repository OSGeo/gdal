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

#ifndef GDALALG_CONVERT_INCLUDED
#define GDALALG_CONVERT_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"
#include "gdalalg_raster_convert.h"
#include "gdalalg_vector_convert.h"
#include "gdalalg_dispatcher.h"

/************************************************************************/
/*                         GDALConvertAlgorithm                         */
/************************************************************************/

class GDALConvertAlgorithm final
    : public GDALDispatcherAlgorithm<GDALRasterConvertAlgorithm,
                                     GDALVectorConvertAlgorithm>
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION =
        "Convert a dataset (shortcut for 'gdal raster convert' or "
        "'gdal vector convert').";
    static constexpr const char *HELP_URL = "/programs/gdal_convert.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR, "translate"};
    }

    GDALConvertAlgorithm();

    ~GDALConvertAlgorithm() override;

  private:
    std::string m_format{};
    GDALArgDatasetValue m_inputDataset{};
    GDALArgDatasetValue m_outputDataset{};
};

//! @endcond

#endif

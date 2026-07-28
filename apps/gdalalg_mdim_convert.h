/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_CONVERT_INCLUDED
#define GDALALG_MDIM_CONVERT_INCLUDED

#include "gdalalg_mdim_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALMdimConvertAlgorithm                       */
/************************************************************************/

class GDALMdimConvertAlgorithm final : public GDALMdimPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION =
        "Convert a multidimensional dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_convert.html";

    explicit GDALMdimConvertAlgorithm();

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    bool m_strict = false;
    std::vector<std::string> m_arrays{};
    std::vector<std::string> m_arrayOptions{};
    std::vector<std::string> m_groups{};
    std::vector<std::string> m_subsets{};
    std::vector<std::string> m_scaleAxes{};
};

//! @endcond

#endif

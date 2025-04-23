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

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALMdimConvertAlgorithm                         */
/************************************************************************/

class GDALMdimConvertAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION =
        "Convert a multidimensional dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_convert.html";

    explicit GDALMdimConvertAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_outputFormat{};
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    bool m_strict = false;
    std::vector<std::string> m_arrays{};
    std::vector<std::string> m_arrayOptions{};
    std::vector<std::string> m_groups{};
    std::vector<std::string> m_subsets{};
    std::vector<std::string> m_scaleAxes{};
};

//! @endcond

#endif

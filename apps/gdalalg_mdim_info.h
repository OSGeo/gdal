/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_INFO_INCLUDED
#define GDALALG_MDIM_INFO_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALMdimInfoAlgorithm                         */
/************************************************************************/

class GDALMdimInfoAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a multidimensional dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_info.html";

    GDALMdimInfoAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_format{};
    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::string m_output{};
    bool m_detailed = false;
    std::string m_array{};
    int m_limit = 0;
    std::vector<std::string> m_arrayOptions{};
    bool m_stats = false;
    bool m_stdout = false;
};

//! @endcond

#endif

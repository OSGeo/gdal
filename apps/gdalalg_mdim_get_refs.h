/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim get-refs" subcommand
 * Author:   Michael Sumner <mdsumner at gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Michael Sumner <mdsumner at gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_GET_REFS_INCLUDED
#define GDALALG_MDIM_GET_REFS_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALMdimGetRefsAlgorithm                      */
/************************************************************************/

class GDALMdimGetRefsAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "get-refs";
    static constexpr const char *DESCRIPTION =
        "Return byte references from a multidimensional raster source as "
        "vector/table layer.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_get_refs.html";

    explicit GDALMdimGetRefsAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_outputFormat{};
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::string m_array{};
    GDALArgDatasetValue m_outputDataset{};
    bool m_overwrite = false;
    std::vector<std::string> m_creationOptions{};
};

//! @endcond

#endif

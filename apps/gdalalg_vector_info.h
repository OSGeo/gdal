/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_INFO_INCLUDED
#define GDALALG_VECTOR_INFO_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorInfoAlgorithm                        */
/************************************************************************/

class GDALVectorInfoAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_info.html";

    GDALVectorInfoAlgorithm();

    void SetDataset(GDALDataset *poDS)
    {
        auto arg = GetArg(GDAL_ARG_NAME_INPUT);
        if (arg)
        {
            arg->Set(poDS);
            arg->SetSkipIfAlreadySet();
        }
    }

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_format{};
    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    bool m_update = false;
    std::vector<std::string> m_layerNames{};
    bool m_listFeatures = false;
    bool m_stdout = false;
    std::string m_sql{};
    std::string m_where{};
    std::string m_dialect{};
    std::string m_output{};
};

//! @endcond

#endif

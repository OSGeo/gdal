/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CONVERT_INCLUDED
#define GDALALG_VECTOR_CONVERT_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorConvertAlgorithm                       */
/************************************************************************/

class GDALVectorConvertAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION = "Convert a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_convert.html";

    GDALVectorConvertAlgorithm();

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

    std::string m_outputFormat{};
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};
    bool m_overwrite = false;
    bool m_update = false;
    bool m_overwriteLayer = false;
    bool m_appendLayer = false;
    std::vector<std::string> m_inputLayerNames{};
    std::string m_outputLayerName{};
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "select" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_SELECT_INCLUDED
#define GDALALG_VECTOR_SELECT_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorSelectAlgorithm                         */
/************************************************************************/

class GDALVectorSelectAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "select";
    static constexpr const char *DESCRIPTION =
        "Select a subset of fields from a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_select.html";

    explicit GDALVectorSelectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    std::vector<std::string> m_fields{};
    bool m_ignoreMissingFields = false;
    bool m_exclude = false;
};

/************************************************************************/
/*                 GDALVectorSelectAlgorithmStandalone                  */
/************************************************************************/

class GDALVectorSelectAlgorithmStandalone final
    : public GDALVectorSelectAlgorithm
{
  public:
    GDALVectorSelectAlgorithmStandalone()
        : GDALVectorSelectAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_SELECT_INCLUDED */

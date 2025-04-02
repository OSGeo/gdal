/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "filter" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_FILTER_INCLUDED
#define GDALALG_VECTOR_FILTER_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorFilterAlgorithm                         */
/************************************************************************/

class GDALVectorFilterAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "filter";
    static constexpr const char *DESCRIPTION = "Filter a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_filter.html";

    explicit GDALVectorFilterAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    std::vector<double> m_bbox{};
    std::string m_where{};
};

/************************************************************************/
/*                 GDALVectorFilterAlgorithmStandalone                  */
/************************************************************************/

class GDALVectorFilterAlgorithmStandalone final
    : public GDALVectorFilterAlgorithm
{
  public:
    GDALVectorFilterAlgorithmStandalone()
        : GDALVectorFilterAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_FILTER_INCLUDED */

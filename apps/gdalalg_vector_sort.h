/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector sort"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_SORT_INCLUDED
#define GDALALG_VECTOR_SORT_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                          GDALVectorSortAlgorithm                     */
/************************************************************************/

class GDALVectorSortAlgorithm : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "sort";
    static constexpr const char *DESCRIPTION =
        "Spatially order the features in a layer";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_sort.html";

    explicit GDALVectorSortAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_geomField{};
    std::string m_sortMethod{"hilbert"};
    bool m_useTempfile{false};
};

/************************************************************************/
/*                      GDALVectorSortAlgorithmStandalone               */
/************************************************************************/

class GDALVectorSortAlgorithmStandalone final : public GDALVectorSortAlgorithm
{
  public:
    GDALVectorSortAlgorithmStandalone()
        : GDALVectorSortAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorSortAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_SORT_INCLUDED */

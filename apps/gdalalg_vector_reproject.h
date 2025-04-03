/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reproject" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_REPROJECT_INCLUDED
#define GDALALG_VECTOR_REPROJECT_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorReprojectAlgorithm                      */
/************************************************************************/

class GDALVectorReprojectAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "reproject";
    static constexpr const char *DESCRIPTION = "Reproject a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_pipeline.html";

    explicit GDALVectorReprojectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    std::string m_srsCrs{};
    std::string m_dstCrs{};
};

/************************************************************************/
/*                 GDALVectorReprojectAlgorithmStandalone               */
/************************************************************************/

class GDALVectorReprojectAlgorithmStandalone final
    : public GDALVectorReprojectAlgorithm
{
  public:
    GDALVectorReprojectAlgorithmStandalone()
        : GDALVectorReprojectAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_REPROJECT_INCLUDED */

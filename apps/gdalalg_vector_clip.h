/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "clip" step of "vector pipeline", or "gdal vector clip" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CLIP_INCLUDED
#define GDALALG_VECTOR_CLIP_INCLUDED

#include "gdalalg_vector_pipeline.h"
#include "gdalalg_clip_common.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorClipAlgorithm                        */
/************************************************************************/

class GDALVectorClipAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm,
      public GDALClipCommon
{
  public:
    static constexpr const char *NAME = "clip";
    static constexpr const char *DESCRIPTION = "Clip a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_clip.html";

    explicit GDALVectorClipAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
};

/************************************************************************/
/*                   GDALVectorClipAlgorithmStandalone                  */
/************************************************************************/

class GDALVectorClipAlgorithmStandalone final : public GDALVectorClipAlgorithm
{
  public:
    GDALVectorClipAlgorithmStandalone()
        : GDALVectorClipAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_CLIP_INCLUDED */

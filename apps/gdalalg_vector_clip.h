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

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorClipAlgorithm                        */
/************************************************************************/

class GDALVectorClipAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "clip";
    static constexpr const char *DESCRIPTION = "Clip a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_clip.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    explicit GDALVectorClipAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    std::vector<double> m_bbox{};
    std::string m_bboxCrs{};
    std::string m_geometry{};
    std::string m_geometryCrs{};
    GDALArgDatasetValue m_likeDataset{};
    std::string m_likeLayer{};
    std::string m_likeSQL{};
    std::string m_likeWhere{};
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

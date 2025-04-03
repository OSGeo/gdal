/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "edit" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_EDIT_INCLUDED
#define GDALALG_RASTER_EDIT_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorEditAlgorithm                        */
/************************************************************************/

class GDALVectorEditAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "edit";
    static constexpr const char *DESCRIPTION =
        "Edit metadata of a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_edit.html";

    explicit GDALVectorEditAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    std::string m_overrideCrs{};
    std::string m_geometryType{};
    std::vector<std::string> m_metadata{};
    std::vector<std::string> m_unsetMetadata{};
    std::vector<std::string> m_layerMetadata{};
    std::vector<std::string> m_unsetLayerMetadata{};
};

/************************************************************************/
/*                   GDALVectorEditAlgorithmStandalone                  */
/************************************************************************/

class GDALVectorEditAlgorithmStandalone final : public GDALVectorEditAlgorithm
{
  public:
    GDALVectorEditAlgorithmStandalone()
        : GDALVectorEditAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_EDIT_INCLUDED */

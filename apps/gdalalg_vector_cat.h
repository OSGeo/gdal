/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector cat" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CAT_INCLUDED
#define GDALALG_VECTOR_CAT_INCLUDED

#include "gdalalg_vector_pipeline.h"

#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorCatAlgorithm                         */
/************************************************************************/

class GDALVectorCatAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "cat";
    static constexpr const char *DESCRIPTION = "Concatenate vector datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_cat.html";

    explicit GDALVectorCatAlgorithm(bool bStandalone = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_layerNameTemplate{};
    std::string m_sourceLayerFieldName{};
    std::string m_sourceLayerFieldContent{};
    std::string m_mode = "merge-per-layer-name";
    std::string m_fieldStrategy = "union";
    std::string m_srsCrs{};
    std::string m_dstCrs{};

    std::vector<std::unique_ptr<OGRLayer>> m_tempLayersKeeper{};
};

/************************************************************************/
/*                   GDALVectorCatAlgorithmStandalone                   */
/************************************************************************/

class GDALVectorCatAlgorithmStandalone final : public GDALVectorCatAlgorithm
{
  public:
    GDALVectorCatAlgorithmStandalone()
        : GDALVectorCatAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector concat" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CONCAT_INCLUDED
#define GDALALG_VECTOR_CONCAT_INCLUDED

#include "gdalalg_vector_pipeline.h"

#include "ogrsf_frmts.h"
#include "ogrlayerpool.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorConcatAlgorithm                      */
/************************************************************************/

class GDALVectorConcatAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "concat";
    static constexpr const char *DESCRIPTION = "Concatenate vector datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_concat.html";

    explicit GDALVectorConcatAlgorithm(bool bStandalone = false);
    ~GDALVectorConcatAlgorithm() override;

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

    std::unique_ptr<OGRLayerPool> m_poLayerPool{};
    std::vector<std::unique_ptr<OGRLayer>> m_tempLayersKeeper{};
};

/************************************************************************/
/*                   GDALVectorConcatAlgorithmStandalone                */
/************************************************************************/

class GDALVectorConcatAlgorithmStandalone final
    : public GDALVectorConcatAlgorithm
{
  public:
    GDALVectorConcatAlgorithmStandalone()
        : GDALVectorConcatAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector layer-algebra" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_LAYER_ALGEBRA_INCLUDED
#define GDALALG_VECTOR_LAYER_ALGEBRA_INCLUDED

#include "gdalvectorpipelinestepalgorithm.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALVectorLayerAlgebraAlgorithm                    */
/************************************************************************/

class GDALVectorLayerAlgebraAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "layer-algebra";
    static constexpr const char *DESCRIPTION =
        "Perform algebraic operation between 2 layers.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_layer_algebra.html";

    explicit GDALVectorLayerAlgebraAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool
    CanHandleNextStep(GDALPipelineStepAlgorithm *poNextStep) const override;

  private:
    std::string m_operation{};

    std::string m_inputLayerName{};
    GDALArgDatasetValue m_methodDataset{};
    std::string m_methodLayerName{};

    // Output arguments
    std::string m_geometryType{};

    std::string m_inputPrefix{};
    std::vector<std::string> m_inputFields{};
    bool m_noInputFields = false;
    bool m_allInputFields = false;

    std::string m_methodPrefix{};
    std::vector<std::string> m_methodFields{};
    bool m_noMethodFields = false;
    bool m_allMethodFields = false;

    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

/************************************************************************/
/*              GDALVectorLayerAlgebraAlgorithmStandalone               */
/************************************************************************/

class GDALVectorLayerAlgebraAlgorithmStandalone final
    : public GDALVectorLayerAlgebraAlgorithm
{
  public:
    GDALVectorLayerAlgebraAlgorithmStandalone()
        : GDALVectorLayerAlgebraAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorLayerAlgebraAlgorithmStandalone() override;
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "update" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_UPDATE_INCLUDED
#define GDALALG_VECTOR_UPDATE_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorUpdateAlgorithm                       */
/************************************************************************/

class GDALVectorUpdateAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "update";
    static constexpr const char *DESCRIPTION =
        "Update an existing vector dataset with an input vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_update.html";

    explicit GDALVectorUpdateAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool OutputDatasetAllowedBeforeRunningStep() const override
    {
        return true;
    }

  protected:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_activeLayer{};

    static constexpr const char *MODE_MERGE = "merge";
    static constexpr const char *MODE_UPDATE_ONLY = "update-only";
    static constexpr const char *MODE_APPEND_ONLY = "append-only";

    std::string m_mode{MODE_MERGE};
    std::vector<std::string> m_key{};
};

/************************************************************************/
/*                 GDALVectorUpdateAlgorithmStandalone                  */
/************************************************************************/

class GDALVectorUpdateAlgorithmStandalone final
    : public GDALVectorUpdateAlgorithm
{
  public:
    GDALVectorUpdateAlgorithmStandalone()
        : GDALVectorUpdateAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorUpdateAlgorithmStandalone() override;

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

//! @endcond

#endif /* GDALALG_VECTOR_UPDATE_INCLUDED */

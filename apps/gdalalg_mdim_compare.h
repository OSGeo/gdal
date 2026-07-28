/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim compare" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_COMPARE_INCLUDED
#define GDALALG_MDIM_COMPARE_INCLUDED

#include "gdalmdimpipelinestepalgorithm.h"
#include "gdalalg_compare_common.h"

//! @cond Doxygen_Suppress

class GDALMDArray;

/************************************************************************/
/*                       GDALMdimCompareAlgorithm                       */
/************************************************************************/

class GDALMdimCompareAlgorithm /* non final */
    : public GDALMdimPipelineStepAlgorithm,
      public GDALCompareCommon
{
  public:
    static constexpr const char *NAME = "compare";
    static constexpr const char *DESCRIPTION =
        "Compare two multidimensional datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_compare.html";

    explicit GDALMdimCompareAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool CanBeLastStep() const override
    {
        return true;
    }

    int GetOutputType() const override
    {
        return 0;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    void CompareArray(std::vector<std::string> &aosReport,
                      const std::shared_ptr<GDALMDArray> &poRefArray,
                      const std::shared_ptr<GDALMDArray> &poInputArray,
                      GDALProgressFunc pfnProgress, void *pProgressData);
};

/************************************************************************/
/*                  GDALMdimCompareAlgorithmStandalone                  */
/************************************************************************/

class GDALMdimCompareAlgorithmStandalone final : public GDALMdimCompareAlgorithm
{
  public:
    GDALMdimCompareAlgorithmStandalone()
        : GDALMdimCompareAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALMdimCompareAlgorithmStandalone() override;
};

//! @endcond

#endif

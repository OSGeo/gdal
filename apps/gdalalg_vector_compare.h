/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector compare" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_COMPARE_INCLUDED
#define GDALALG_VECTOR_COMPARE_INCLUDED

#include "gdalvectorpipelinestepalgorithm.h"
#include "gdalalg_compare_common.h"

//! @cond Doxygen_Suppress

class GDALDataset;
class OGRLayer;

/************************************************************************/
/*                      GDALVectorCompareAlgorithm                      */
/************************************************************************/

class GDALVectorCompareAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm,
      public GDALCompareCommon
{
  public:
    static constexpr const char *NAME = "compare";
    static constexpr const char *DESCRIPTION = "Compare two vector datasets.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_compare.html";

    explicit GDALVectorCompareAlgorithm(bool standaloneStep = false);

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

    bool CompareLayer(std::vector<std::string> &aosReport, OGRLayer *poRefLayer,
                      OGRLayer *poInputLayer, GDALProgressFunc,
                      void *pProgressData);

    bool m_laxGeometryComparison = false;

    bool m_skipAllOptional = false;
    bool m_skipCRS = false;
    bool m_skipMetadata = false;
    bool m_skipFID = false;
    // If adding a new skip flag, make sure that m_skipAll takes it into account
};

/************************************************************************/
/*                 GDALVectorCompareAlgorithmStandalone                 */
/************************************************************************/

class GDALVectorCompareAlgorithmStandalone final
    : public GDALVectorCompareAlgorithm
{
  public:
    GDALVectorCompareAlgorithmStandalone()
        : GDALVectorCompareAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCompareAlgorithmStandalone() override;
};

//! @endcond

#endif

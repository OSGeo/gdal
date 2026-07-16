/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_INFO_INCLUDED
#define GDALALG_VECTOR_INFO_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorInfoAlgorithm                        */
/************************************************************************/

class GDALVectorInfoAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_info.html";

    explicit GDALVectorInfoAlgorithm(bool standaloneStep = false);

    bool CanBeLastStep() const override
    {
        return true;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<std::string> m_layerNames{};
    bool m_listFeatures = false;
    bool m_summaryOnly = false;
    std::string m_sql{};
    std::string m_where{};
    std::string m_dialect{};
    int m_limit = 0;
};

/************************************************************************/
/*                  GDALVectorInfoAlgorithmStandalone                   */
/************************************************************************/

class GDALVectorInfoAlgorithmStandalone final : public GDALVectorInfoAlgorithm
{
  public:
    GDALVectorInfoAlgorithmStandalone()
        : GDALVectorInfoAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorInfoAlgorithmStandalone() override;
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_INFO_INCLUDED
#define GDALALG_MDIM_INFO_INCLUDED

#include "gdalalg_mdim_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALMdimInfoAlgorithm                         */
/************************************************************************/

class GDALMdimInfoAlgorithm /* non final */
    : public GDALMdimPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a multidimensional dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_info.html";

    explicit GDALMdimInfoAlgorithm(bool standaloneStep = false,
                                   bool openForMixedRasterVector = false);

    bool CanBeLastStep() const override
    {
        return true;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    bool m_summary = false;
    bool m_detailed = false;
    std::string m_array{};
    int m_limit = 0;
    std::vector<std::string> m_arrayOptions{};
    bool m_stats = false;
};

/************************************************************************/
/*                   GDALMdimInfoAlgorithmStandalone                    */
/************************************************************************/

class GDALMdimInfoAlgorithmStandalone final : public GDALMdimInfoAlgorithm
{
  public:
    GDALMdimInfoAlgorithmStandalone()
        : GDALMdimInfoAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALMdimInfoAlgorithmStandalone() override;
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reproject" step of "mdim pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_REPROJECT_INCLUDED
#define GDALALG_MDIM_REPROJECT_INCLUDED

#include "gdalmdimpipelinestepalgorithm.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALMdimReprojectAlgorithm                      */
/************************************************************************/

class GDALMdimReprojectAlgorithm /* non final */
    : public GDALMdimPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "reproject";
    static constexpr const char *DESCRIPTION =
        "Reproject a multidimensional dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_mdim_reproject.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR, "warp"};
    }

    explicit GDALMdimReprojectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_dstCrs{};
    std::string m_resampling{};
};

/************************************************************************/
/*                 GDALMdimReprojectAlgorithmStandalone                 */
/************************************************************************/

class GDALMdimReprojectAlgorithmStandalone final
    : public GDALMdimReprojectAlgorithm
{
  public:
    GDALMdimReprojectAlgorithmStandalone()
        : GDALMdimReprojectAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALMdimReprojectAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_REPROJECT_INCLUDED */

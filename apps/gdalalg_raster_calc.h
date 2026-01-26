/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "calc" step of "raster pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CALC_INCLUDED
#define GDALALG_RASTER_CALC_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterCalcAlgorithm                        */
/************************************************************************/

class GDALRasterCalcAlgorithm : public GDALRasterPipelineStepAlgorithm
{
  public:
    explicit GDALRasterCalcAlgorithm(bool standaloneStep = false) noexcept;

    static constexpr const char *NAME = "calc";
    static constexpr const char *DESCRIPTION = "Perform raster algebra";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_calc.html";

    bool CanBeFirstStep() const override
    {
        return true;
    }

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<std::string> m_expr{};
    std::string m_dialect{"muparser"};
    bool m_flatten{false};
    std::string m_type{};
    std::string m_nodata{};
    bool m_noCheckCRS{false};
    bool m_noCheckExtent{false};
    bool m_noCheckExpression{false};
    bool m_propagateNoData{false};
};

/************************************************************************/
/*                  GDALRasterCalcAlgorithmStandalone                   */
/************************************************************************/

class GDALRasterCalcAlgorithmStandalone final : public GDALRasterCalcAlgorithm
{
  public:
    GDALRasterCalcAlgorithmStandalone()
        : GDALRasterCalcAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterCalcAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_CALC_INCLUDED */

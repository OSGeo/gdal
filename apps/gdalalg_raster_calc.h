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

class GDALRasterCalcAlgorithm : public GDALAlgorithm
{
  public:
    explicit GDALRasterCalcAlgorithm() noexcept;

    static constexpr const char *NAME = "calc";
    static constexpr const char *DESCRIPTION = "Perform raster algebra";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_calc.html";

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<std::string> m_inputs{};
    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_expr{};
    GDALArgDatasetValue m_outputDataset{};
    std::string m_format{};
    std::string m_type{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite{false};
    bool m_NoCheckSRS{false};
    bool m_NoCheckExtent{false};
};

//! @endcond

#endif /* GDALALG_RASTER_CALC_INCLUDED */

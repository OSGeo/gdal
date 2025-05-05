/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "reclassify" step of "raster pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences, LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_RECLASSIFY_INCLUDED
#define GDALALG_RASTER_RECLASSIFY_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/*************gg***********************************************************/
/*                     GDALRasterReclassifyAlgorithm                      */
/**************************************************************************/

class GDALRasterReclassifyAlgorithm : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "reclassify";
    static constexpr const char *DESCRIPTION =
        "Reclassify values in a raster dataset";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_reclassify.html";

    explicit GDALRasterReclassifyAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_mapping{};
    std::string m_type{};
};

/************************************************************************/
/*                  GDALRasterResizeAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterReclassifyAlgorithmStandalone final
    : public GDALRasterReclassifyAlgorithm
{
  public:
    GDALRasterReclassifyAlgorithmStandalone()
        : GDALRasterReclassifyAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_RECLASSIFY_INCLUDED */

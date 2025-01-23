/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reproject" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_REPROJECT_INCLUDED
#define GDALALG_RASTER_REPROJECT_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterReprojectAlgorithm                      */
/************************************************************************/

class GDALRasterReprojectAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "reproject";
    static constexpr const char *DESCRIPTION = "Reproject a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_reproject.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    explicit GDALRasterReprojectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_srsCrs{};
    std::string m_dstCrs{};
    std::string m_resampling{};
    std::vector<double> m_resolution{};
    std::vector<double> m_bbox{};
    bool m_targetAlignedPixels = false;
};

/************************************************************************/
/*                 GDALRasterReprojectAlgorithmStandalone               */
/************************************************************************/

class GDALRasterReprojectAlgorithmStandalone final
    : public GDALRasterReprojectAlgorithm
{
  public:
    GDALRasterReprojectAlgorithmStandalone()
        : GDALRasterReprojectAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_REPROJECT_INCLUDED */

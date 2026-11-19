/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster clean-collar" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CLEAN_COLLAR_INCLUDED
#define GDALALG_RASTER_CLEAN_COLLAR_INCLUDED

#include "gdalrasterpipelinenonnativelystreamingalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterCleanCollarAlgorithm                    */
/************************************************************************/

class GDALRasterCleanCollarAlgorithm /* non final */
    : public GDALRasterPipelineNonNativelyStreamingAlgorithm
{
  public:
    static constexpr const char *NAME = "clean-collar";
    static constexpr const char *DESCRIPTION =
        "Clean the collar of a raster dataset, removing noise.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_clean_collar.html";

    explicit GDALRasterCleanCollarAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<std::string> m_color{};
    int m_colorThreshold = 15;
    int m_pixelDistance = 2;
    bool m_addAlpha = false;
    bool m_addMask = false;
    std::string m_algorithm = "floodfill";
};

/************************************************************************/
/*               GDALRasterCleanCollarAlgorithmStandalone               */
/************************************************************************/

class GDALRasterCleanCollarAlgorithmStandalone final
    : public GDALRasterCleanCollarAlgorithm
{
  public:
    GDALRasterCleanCollarAlgorithmStandalone()
        : GDALRasterCleanCollarAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterCleanCollarAlgorithmStandalone() override;
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster viewshed" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_VIEWSHED_INCLUDED
#define GDALALG_RASTER_VIEWSHED_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterViewshedAlgorithm                       */
/************************************************************************/

class GDALRasterViewshedAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "viewshed";
    static constexpr const char *DESCRIPTION =
        "Compute the viewshed of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_viewshed.html";

    explicit GDALRasterViewshedAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    std::string m_format{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;

    std::vector<double> m_observerPos{};
    double m_targetHeight = 0;

    std::string m_outputMode = "normal";

    int m_band = 1;
    double m_maxDistance = 0;
    double m_curveCoefficient = 0.85714;
    int m_observerSpacing = 10;
    int m_numThreads = 3;
    int m_dstNoData = -1;
    int m_visibleVal = 255;
    int m_invisibleVal = 0;
    int m_outOfRangeVal = 0;

    // Work variables
    std::string m_numThreadsStr{};
};

//! @endcond

#endif

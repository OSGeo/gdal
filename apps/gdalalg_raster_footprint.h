/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster footprint" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_FOOTPRINT_INCLUDED
#define GDALALG_RASTER_FOOTPRINT_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterFootprintAlgorithm                       */
/************************************************************************/

class GDALRasterFootprintAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "footprint";
    static constexpr const char *DESCRIPTION =
        "Compute the footprint of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_footprint.html";

    explicit GDALRasterFootprintAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    std::string m_format{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};
    std::string m_outputLayerName = "footprint";
    bool m_append = false;
    bool m_overwrite = false;

    std::vector<int> m_bands{};
    std::string m_combineBands = "union";
    int m_overview = -1;
    std::vector<double> m_srcNoData{};
    std::string m_coordinateSystem{};
    std::string m_dstCrs{};
    bool m_splitMultiPolygons = false;
    bool m_convexHull = false;
    double m_densifyVal = 0;
    double m_simplifyVal = 0;
    double m_minRingArea = 0;
    std::string m_maxPoints = "100";
    std::string m_locationField = "location";
    bool m_noLocation = false;
    bool m_writeAbsolutePaths = false;
};

//! @endcond

#endif

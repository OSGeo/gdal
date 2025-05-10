/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster tile" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_TILE_INCLUDED
#define GDALALG_RASTER_TILE_INCLUDED

#include "gdalalgorithm.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterTileAlgorithm                        */
/************************************************************************/

class GDALRasterTileAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "tile";
    static constexpr const char *DESCRIPTION =
        "Generate tiles in separate files from a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_tile.html";

    GDALRasterTileAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::string m_outputFormat = "PNG";
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_metadata{};
    bool m_copySrcMetadata = false;
    std::string m_outputDirectory{};
    std::string m_tilingScheme{};
    std::string m_convention = "xyz";
    std::string m_resampling{};
    std::string m_overviewResampling{};
    int m_minZoomLevel = -1;
    int m_maxZoomLevel = -1;
    bool m_noIntersectionIsOK = false;
    int m_minTileX = -1;
    int m_minTileY = -1;
    int m_maxTileX = -1;
    int m_maxTileY = -1;
    int m_tileSize = 0;
    bool m_addalpha = false;
    bool m_noalpha = false;
    double m_dstNoData = 0;
    bool m_skipBlank = false;
    bool m_auxXML = false;
    bool m_resume = false;
    int m_numThreads = 0;
    bool m_kml = false;

    std::string m_excludedValues{};
    double m_excludedValuesPctThreshold = 50;
    double m_nodataValuesPctThreshold = 100;

    std::vector<std::string> m_webviewers{};
    std::string m_url{};
    std::string m_title{};
    std::string m_copyright{};
    std::string m_mapmlTemplate{};

    // Work variables
    std::string m_numThreadsStr{"ALL_CPUS"};
    std::map<std::string, std::string> m_mapTileMatrixIdentifierToScheme{};
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster polygonize" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_POLYGONIZE_INCLUDED
#define GDALALG_RASTER_POLYGONIZE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterPolygonizeAlgorithm                    */
/************************************************************************/

class GDALRasterPolygonizeAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "polygonize";
    static constexpr const char *DESCRIPTION =
        "Create a polygon feature dataset from a raster band.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_polygonize.html";

    GDALRasterPolygonizeAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_outputFormat{};
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};
    bool m_overwrite = false;
    bool m_update = false;
    bool m_overwriteLayer = false;
    bool m_appendLayer = false;

    // polygonize specific arguments
    int m_band = 1;
    std::string m_outputLayerName = "polygonize";
    std::string m_attributeName = "DN";
    bool m_connectDiagonalPixels = false;
};

//! @endcond

#endif

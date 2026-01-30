/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pixelinfo" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PIXEL_INFO_INCLUDED
#define GDALALG_RASTER_PIXEL_INFO_INCLUDED

#include "gdalalgorithm.h"
#include "cpl_vsi_virtual.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterPixelInfoAlgorithm                     */
/************************************************************************/

class GDALRasterPixelInfoAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "pixel-info";
    static constexpr const char *DESCRIPTION =
        "Return information on a pixel of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pixel_info.html";

    GDALRasterPixelInfoAlgorithm();
    ~GDALRasterPixelInfoAlgorithm() override;

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_rasterDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    GDALArgDatasetValue m_vectorDataset{};
    std::string m_inputLayerName{};
    std::vector<std::string> m_includeFields{"ALL"};

    std::string m_format{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};
    bool m_overwrite = false;

    std::string m_outputString{};
    std::vector<int> m_band{};
    int m_overview = -1;
    std::vector<double> m_pos{};
    std::string m_posCrs{};
    std::string m_resampling = "nearest";
    bool m_promotePixelValueToZ = false;

    VSIVirtualHandleUniquePtr m_outputFile{};
    std::string m_osTmpFilename{};
};

//! @endcond

#endif

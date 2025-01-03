/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster buildvrt" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_BUILDVRT_INCLUDED
#define GDALALG_RASTER_BUILDVRT_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterBuildVRTAlgorithm                       */
/************************************************************************/

class GDALRasterBuildVRTAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "buildvrt";
    static constexpr const char *DESCRIPTION = "Build a virtual dataset (VRT).";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_buildvrt.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    explicit GDALRasterBuildVRTAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<GDALArgDatasetValue> m_inputDatasets{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    bool m_separate = false;
    std::string m_resolution{};
    std::vector<double> m_bbox{};
    bool m_targetAlignedPixels = false;
    std::vector<double> m_srcNoData{};
    std::vector<double> m_vrtNoData{};
    std::vector<int> m_bands{};
    bool m_hideNoData = false;
    bool m_addAlpha = false;
};

//! @endcond

#endif

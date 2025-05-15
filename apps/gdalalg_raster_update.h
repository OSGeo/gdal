/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster update" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_UPDATE_INCLUDED
#define GDALALG_RASTER_UPDATE_INCLUDED

#include "gdalalgorithm.h"

#include "gdalalg_clip_common.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterUpdateAlgorithm                        */
/************************************************************************/

class GDALRasterUpdateAlgorithm final : public GDALAlgorithm,
                                        public GDALClipCommon
{
  public:
    static constexpr const char *NAME = "update";
    static constexpr const char *DESCRIPTION =
        "Update the destination raster with the content of the input one.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_update.html";

    explicit GDALRasterUpdateAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    GDALArgDatasetValue m_outputDataset{};
    bool m_update = true;

    std::string m_resampling{};
    std::vector<std::string> m_warpOptions{};
    std::vector<std::string> m_transformOptions{};
    double m_errorThreshold = std::numeric_limits<double>::quiet_NaN();
    bool m_noUpdateOverviews = false;
};

//! @endcond

#endif

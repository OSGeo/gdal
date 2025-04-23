/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster create" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CREATE_INCLUDED
#define GDALALG_RASTER_CREATE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterCreateAlgorithm                        */
/************************************************************************/

class GDALRasterCreateAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "create";
    static constexpr const char *DESCRIPTION = "Create a new raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_create.html";

    GDALRasterCreateAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_outputFormat{};
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    bool m_append = false;
    std::vector<int> m_size{};
    int m_bandCount = 1;
    std::string m_type = "Byte";
    std::string m_crs{};
    std::vector<double> m_bbox{};
    std::vector<std::string> m_metadata{};
    std::string m_nodata{};
    std::vector<double> m_burnValues{};
    bool m_copyOverviews = false;
    bool m_copyMetadata = false;
};

//! @endcond

#endif

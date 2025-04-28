/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster sieve" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_SIEVE_INCLUDED
#define GDALALG_RASTER_SIEVE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterSieveAlgorithm                           */
/************************************************************************/

class GDALRasterSieveAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "sieve";
    static constexpr const char *DESCRIPTION =
        "Remove small polygons from a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_sieve.html";

    explicit GDALRasterSieveAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    std::string m_format{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;

    int m_band = 1;
    int m_sizeThreshold = 2;
    bool m_connectDiagonalPixels = false;
    GDALArgDatasetValue m_maskDataset{};
};

//! @endcond

#endif

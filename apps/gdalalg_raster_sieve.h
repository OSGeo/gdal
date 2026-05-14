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

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterSieveAlgorithm                       */
/************************************************************************/

class GDALRasterSieveAlgorithm /* non final */
    : public GDALRasterPipelineNonNativelyStreamingAlgorithm
{
  public:
    static constexpr const char *NAME = "sieve";
    static constexpr const char *DESCRIPTION =
        "Remove small polygons from a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_sieve.html";

    explicit GDALRasterSieveAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    int m_band = 1;
    int m_sizeThreshold = 2;
    bool m_connectDiagonalPixels = false;
    GDALArgDatasetValue m_maskDataset{};
};

/************************************************************************/
/*                  GDALRasterSieveAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterSieveAlgorithmStandalone final : public GDALRasterSieveAlgorithm
{
  public:
    GDALRasterSieveAlgorithmStandalone()
        : GDALRasterSieveAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterSieveAlgorithmStandalone() override;
};

//! @endcond

#endif

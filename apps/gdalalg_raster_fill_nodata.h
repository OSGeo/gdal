/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal raster fill-nodata" standalone command
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_FILL_NODATA_INCLUDED
#define GDALALG_RASTER_FILL_NODATA_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterFillNodataAlgorithm                     */
/************************************************************************/

class GDALRasterFillNodataAlgorithm /* non final */
    : public GDALRasterPipelineNonNativelyStreamingAlgorithm
{
  public:
    static constexpr const char *NAME = "fill-nodata";
    static constexpr const char *DESCRIPTION =
        "Fill nodata raster regions by interpolation from edges.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_fill_nodata.html";

    explicit GDALRasterFillNodataAlgorithm(
        bool standaloneStep = false) noexcept;

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    // The maximum distance (in pixels) that the algorithm will search out for values to interpolate. The default is 100 pixels.
    int m_maxDistance = 100;
    // The number of 3x3 average filter smoothing iterations to run after the interpolation to dampen artifacts. The default is zero smoothing iterations.
    int m_smoothingIterations = 0;
    // The band to operate on, by default the first band is operated on.
    int m_band = 1;
    // Use the first band of the specified file as a validity mask (zero is invalid, non-zero is valid).
    GDALArgDatasetValue m_maskDataset{};
    // By default, pixels are interpolated using an inverse distance weighting (inv_dist). It is also possible to choose a nearest neighbour (nearest) strategy.
    std::string m_strategy = "invdist";
};

/************************************************************************/
/*               GDALRasterFillNodataAlgorithmStandalone                */
/************************************************************************/

class GDALRasterFillNodataAlgorithmStandalone final
    : public GDALRasterFillNodataAlgorithm
{
  public:
    GDALRasterFillNodataAlgorithmStandalone()
        : GDALRasterFillNodataAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterFillNodataAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_FILLNODATA_INCLUDED */

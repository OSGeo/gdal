/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "shift-longitude" step of "raster pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_SHIFT_LONGITUDE_INCLUDED
#define GDALALG_RASTER_SHIFT_LONGITUDE_INCLUDED

#include "gdalrasterpipelinestepalgorithm.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALRasterShiftLongitudeAlgorithm                   */
/************************************************************************/

class GDALRasterShiftLongitudeAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "shift-longitude";
    static constexpr const char *DESCRIPTION =
        "Shift a raster dataset (e.g. 0-360 to -180-180)";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_shift_longitude.html";

    explicit GDALRasterShiftLongitudeAlgorithm(bool standaloneStep = false);

  protected:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

  private:
    double m_minX{0};
    double m_maxX{0};
    std::string m_nodata{};
};

/************************************************************************/
/*             GDALRasterShiftLongitudeAlgorithmStandalone              */
/************************************************************************/

class GDALRasterShiftLongitudeAlgorithmStandalone final
    : public GDALRasterShiftLongitudeAlgorithm
{
  public:
    GDALRasterShiftLongitudeAlgorithmStandalone()
        : GDALRasterShiftLongitudeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterShiftLongitudeAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_SHIFT_LONGITUDE_INCLUDED */

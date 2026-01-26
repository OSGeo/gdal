/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster proximity" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PROXIMITY_INCLUDED
#define GDALALG_RASTER_PROXIMITY_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterProximityAlgorithm                     */
/************************************************************************/

class GDALRasterProximityAlgorithm /* non final */
    : public GDALRasterPipelineNonNativelyStreamingAlgorithm
{
  public:
    static constexpr const char *NAME = "proximity";
    static constexpr const char *DESCRIPTION =
        "Produces a raster proximity map.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_proximity.html";

    explicit GDALRasterProximityAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    double m_noDataValue = 0.0;
    int m_inputBand = 1;
    std::string m_outputDataType =
        "Float32";  // Byte|Int16|UInt16|Int32|UInt32|Float32|Float64;
    std::vector<double> m_targetPixelValues{};
    std::string m_distanceUnits = "pixel";  // pixel|geo
    double m_maxDistance = 0.0;
    double m_fixedBufferValue = 0.0;
};

/************************************************************************/
/*                GDALRasterProximityAlgorithmStandalone                */
/************************************************************************/

class GDALRasterProximityAlgorithmStandalone final
    : public GDALRasterProximityAlgorithm
{
  public:
    GDALRasterProximityAlgorithmStandalone()
        : GDALRasterProximityAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterProximityAlgorithmStandalone() override;
};

//! @endcond

#endif

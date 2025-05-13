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

#include "gdalalgorithm.h"
#include "gdal.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterProximityAlgorithm                       */
/************************************************************************/

class GDALRasterProximityAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "proximity";
    static constexpr const char *DESCRIPTION =
        "Produces a raster proximity map.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_proximity.html";

    explicit GDALRasterProximityAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};

    std::string m_outputFormat{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;

    double m_noDataValue = 0.0;
    int m_inputBand = 1;
    std::string m_outputDataType =
        "Float32";  // Byte|Int16|UInt16|Int32|UInt32|Float32|Float64;
    std::vector<double> m_targetPixelValues{};
    std::string m_distanceUnits = "pixel";  // pixel|geo
    double m_maxDistance = 0.0;
    double m_fixedBufferValue = 0.0;
};

//! @endcond

#endif

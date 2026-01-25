/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster contour" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CONTOUR_INCLUDED
#define GDALALG_RASTER_CONTOUR_INCLUDED

#include <limits>

#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALRasterContourAlgorithm                      */
/************************************************************************/

class GDALRasterContourAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "contour";
    static constexpr const char *DESCRIPTION =
        "Creates a vector contour from a raster elevation model (DEM).";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_contour.html";

    explicit GDALRasterContourAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    int GetInputType() const override
    {
        return GDAL_OF_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_VECTOR;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    // gdal_contour specific arguments
    int m_band = 1;                     // -b
    std::string m_elevAttributeName{};  // -a <name>
    std::string m_amin{};               // -amin <value>
    std::string m_amax{};               // -amax <value>
    bool m_3d = false;                  // -3d
                                        // -inodata (skipped)
    double m_sNodata =
        std::numeric_limits<double>::quiet_NaN();  // -snodata <value>
    double m_interval =
        std::numeric_limits<double>::quiet_NaN();  // -i <interval>
    double m_offset =
        std::numeric_limits<double>::quiet_NaN();  // -off <offset>
    std::vector<std::string>
        m_levels{};     // -fl <level>[,<level>...] MIN/MAX are also supported
    int m_expBase = 0;  // -e <base>
    bool m_polygonize = false;    // -p
    int m_groupTransactions = 0;  // gt <n>
};

/************************************************************************/
/*                 GDALRasterContourAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterContourAlgorithmStandalone final
    : public GDALRasterContourAlgorithm
{
  public:
    GDALRasterContourAlgorithmStandalone()
        : GDALRasterContourAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterContourAlgorithmStandalone() override;
};

//! @endcond

#endif

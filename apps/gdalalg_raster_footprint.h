/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster footprint" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_FOOTPRINT_INCLUDED
#define GDALALG_RASTER_FOOTPRINT_INCLUDED

#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterFootprintAlgorithm                     */
/************************************************************************/

class GDALRasterFootprintAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "footprint";
    static constexpr const char *DESCRIPTION =
        "Compute the footprint of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_footprint.html";

    explicit GDALRasterFootprintAlgorithm(bool standaloneStep = false);

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

    std::vector<int> m_bands{};
    std::string m_combineBands = "union";
    int m_overview = -1;
    std::vector<double> m_srcNoData{};
    std::string m_coordinateSystem{};
    std::string m_dstCrs{};
    bool m_splitMultiPolygons = false;
    bool m_convexHull = false;
    double m_densifyVal = 0;
    double m_simplifyVal = 0;
    double m_minRingArea = 0;
    std::string m_maxPoints = "100";
    std::string m_locationField = "location";
    bool m_noLocation = false;
    bool m_writeAbsolutePaths = false;
};

/************************************************************************/
/*                GDALRasterFootprintAlgorithmStandalone                */
/************************************************************************/

class GDALRasterFootprintAlgorithmStandalone final
    : public GDALRasterFootprintAlgorithm
{
  public:
    GDALRasterFootprintAlgorithmStandalone()
        : GDALRasterFootprintAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterFootprintAlgorithmStandalone() override;
};

//! @endcond

#endif

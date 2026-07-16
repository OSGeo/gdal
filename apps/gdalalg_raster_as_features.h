/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "as-features" step of "gdal pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences, LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_AS_FEATURES_INCLUDED
#define GDALALG_RASTER_AS_FEATURES_INCLUDED

#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterAsFeaturesAlgorithm                     */
/************************************************************************/

class GDALRasterAsFeaturesAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "as-features";
    static constexpr const char *DESCRIPTION =
        "Create features from pixels of a raster dataset";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_as_features.html";

    explicit GDALRasterAsFeaturesAlgorithm(bool standaloneStep = false);

    ~GDALRasterAsFeaturesAlgorithm() override;

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

    std::vector<int> m_bands{};
    std::string m_geomTypeName = "none";
    bool m_skipNoData = false;
    bool m_includeXY = false;
    bool m_includeRowCol = false;
};

/************************************************************************/
/*               GDALRasterAsFeaturesAlgorithmStandalone                */
/************************************************************************/

class GDALRasterAsFeaturesAlgorithmStandalone final
    : public GDALRasterAsFeaturesAlgorithm
{
  public:
    GDALRasterAsFeaturesAlgorithmStandalone()
        : GDALRasterAsFeaturesAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterAsFeaturesAlgorithmStandalone() override;
};

//! @endcond

#endif

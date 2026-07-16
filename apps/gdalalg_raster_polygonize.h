/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster polygonize" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_POLYGONIZE_INCLUDED
#define GDALALG_RASTER_POLYGONIZE_INCLUDED

#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterPolygonizeAlgorithm                     */
/************************************************************************/

class GDALRasterPolygonizeAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "polygonize";
    static constexpr const char *DESCRIPTION =
        "Create a polygon feature dataset from a raster band.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_polygonize.html";

    explicit GDALRasterPolygonizeAlgorithm(bool standaloneStep = false);

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

    bool
    CanHandleNextStep(GDALPipelineStepAlgorithm *poNextStep) const override;

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    // polygonize specific arguments
    int m_band = 1;
    std::string m_attributeName = "DN";
    bool m_connectDiagonalPixels = false;

    // hidden
    int m_commitInterval = 0;
};

/************************************************************************/
/*               GDALRasterPolygonizeAlgorithmStandalone                */
/************************************************************************/

class GDALRasterPolygonizeAlgorithmStandalone final
    : public GDALRasterPolygonizeAlgorithm
{
  public:
    GDALRasterPolygonizeAlgorithmStandalone()
        : GDALRasterPolygonizeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterPolygonizeAlgorithmStandalone() override;
};

//! @endcond

#endif

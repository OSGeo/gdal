/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"

#include "gdalalg_raster_info.h"
#include "gdalalg_raster_aspect.h"
#include "gdalalg_raster_astype.h"
#include "gdalalg_raster_calc.h"
#include "gdalalg_raster_clip.h"
#include "gdalalg_raster_clean_collar.h"
#include "gdalalg_raster_color_map.h"
#include "gdalalg_raster_convert.h"
#include "gdalalg_raster_create.h"
#include "gdalalg_raster_edit.h"
#include "gdalalg_raster_contour.h"
#include "gdalalg_raster_footprint.h"
#include "gdalalg_raster_hillshade.h"
#include "gdalalg_raster_index.h"
#include "gdalalg_raster_mosaic.h"
#include "gdalalg_raster_overview.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_raster_polygonize.h"
#include "gdalalg_raster_reproject.h"
#include "gdalalg_raster_resize.h"
#include "gdalalg_raster_roughness.h"
#include "gdalalg_raster_scale.h"
#include "gdalalg_raster_select.h"
#include "gdalalg_raster_slope.h"
#include "gdalalg_raster_stack.h"
#include "gdalalg_raster_tpi.h"
#include "gdalalg_raster_tri.h"
#include "gdalalg_raster_unscale.h"
#include "gdalalg_raster_viewshed.h"

/************************************************************************/
/*                         GDALRasterAlgorithm                          */
/************************************************************************/

class GDALRasterAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "raster";
    static constexpr const char *DESCRIPTION = "Raster commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster.html";

    GDALRasterAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALRasterInfoAlgorithm>();
        RegisterSubAlgorithm<GDALRasterAspectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterAsTypeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterCalcAlgorithm>();
        RegisterSubAlgorithm<GDALRasterCleanCollarAlgorithm>();
        RegisterSubAlgorithm<GDALRasterColorMapAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterConvertAlgorithm>();
        RegisterSubAlgorithm<GDALRasterClipAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterCreateAlgorithm>();
        RegisterSubAlgorithm<GDALRasterEditAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterFootprintAlgorithm>();
        RegisterSubAlgorithm<GDALRasterHillshadeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterIndexAlgorithm>();
        RegisterSubAlgorithm<GDALRasterOverviewAlgorithm>();
        RegisterSubAlgorithm<GDALRasterPipelineAlgorithm>();
        RegisterSubAlgorithm<GDALRasterReprojectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterMosaicAlgorithm>();
        RegisterSubAlgorithm<GDALRasterPolygonizeAlgorithm>();
        RegisterSubAlgorithm<GDALRasterResizeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterRoughnessAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterContourAlgorithm>();
        RegisterSubAlgorithm<GDALRasterScaleAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterSelectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterSlopeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterStackAlgorithm>();
        RegisterSubAlgorithm<GDALRasterTPIAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterTRIAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterUnscaleAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterViewshedAlgorithm>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "raster\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALRasterAlgorithm);

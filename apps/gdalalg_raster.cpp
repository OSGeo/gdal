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
#include "gdalalg_raster_calc.h"
#include "gdalalg_raster_clip.h"
#include "gdalalg_raster_clean_collar.h"
#include "gdalalg_raster_color_map.h"
#include "gdalalg_raster_convert.h"
#include "gdalalg_raster_create.h"
#include "gdalalg_raster_edit.h"
#include "gdalalg_raster_contour.h"
#include "gdalalg_raster_footprint.h"
#include "gdalalg_raster_fill_nodata.h"
#include "gdalalg_raster_hillshade.h"
#include "gdalalg_raster_index.h"
#include "gdalalg_raster_mosaic.h"
#include "gdalalg_raster_overview.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_raster_pixel_info.h"
#include "gdalalg_raster_polygonize.h"
#include "gdalalg_raster_reclassify.h"
#include "gdalalg_raster_reproject.h"
#include "gdalalg_raster_resize.h"
#include "gdalalg_raster_roughness.h"
#include "gdalalg_raster_scale.h"
#include "gdalalg_raster_select.h"
#include "gdalalg_raster_set_type.h"
#include "gdalalg_raster_sieve.h"
#include "gdalalg_raster_slope.h"
#include "gdalalg_raster_stack.h"
#include "gdalalg_raster_tile.h"
#include "gdalalg_raster_tpi.h"
#include "gdalalg_raster_tri.h"
#include "gdalalg_raster_unscale.h"
#include "gdalalg_raster_viewshed.h"

#include "gdal_priv.h"

#ifndef _
#define _(x) (x)
#endif

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
        AddArg("drivers", 0, _("Display raster driver list as JSON document"),
               &m_drivers);

        AddOutputStringArg(&m_output);

        RegisterSubAlgorithm<GDALRasterInfoAlgorithm>();
        RegisterSubAlgorithm<GDALRasterAspectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterCalcAlgorithm>();
        RegisterSubAlgorithm<GDALRasterCleanCollarAlgorithm>();
        RegisterSubAlgorithm<GDALRasterColorMapAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterConvertAlgorithm>();
        RegisterSubAlgorithm<GDALRasterClipAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterCreateAlgorithm>();
        RegisterSubAlgorithm<GDALRasterEditAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterFootprintAlgorithm>();
        RegisterSubAlgorithm<GDALRasterHillshadeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterFillNodataAlgorithm>();
        RegisterSubAlgorithm<GDALRasterIndexAlgorithm>();
        RegisterSubAlgorithm<GDALRasterOverviewAlgorithm>();
        RegisterSubAlgorithm<GDALRasterPipelineAlgorithm>();
        RegisterSubAlgorithm<GDALRasterPixelInfoAlgorithm>();
        RegisterSubAlgorithm<GDALRasterReclassifyAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterReprojectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterMosaicAlgorithm>();
        RegisterSubAlgorithm<GDALRasterPolygonizeAlgorithm>();
        RegisterSubAlgorithm<GDALRasterResizeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterRoughnessAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterContourAlgorithm>();
        RegisterSubAlgorithm<GDALRasterScaleAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterSelectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterSetTypeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterSieveAlgorithm>();
        RegisterSubAlgorithm<GDALRasterSlopeAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterStackAlgorithm>();
        RegisterSubAlgorithm<GDALRasterTileAlgorithm>();
        RegisterSubAlgorithm<GDALRasterTPIAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterTRIAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterUnscaleAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALRasterViewshedAlgorithm>();
    }

  private:
    std::string m_output{};
    bool m_drivers = false;

    bool RunImpl(GDALProgressFunc, void *) override
    {
        if (m_drivers)
        {
            m_output = GDALPrintDriverList(GDAL_OF_RASTER, true);
            return true;
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "The Run() method should not be called directly on the \"gdal "
                "raster\" program.");
            return false;
        }
    }
};

GDAL_STATIC_REGISTER_ALG(GDALRasterAlgorithm);

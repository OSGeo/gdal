/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"

#include "gdalalg_vector_info.h"
#include "gdalalg_vector_clip.h"
#include "gdalalg_vector_concat.h"
#include "gdalalg_vector_convert.h"
#include "gdalalg_vector_edit.h"
#include "gdalalg_vector_geom.h"
#include "gdalalg_vector_grid.h"
#include "gdalalg_vector_pipeline.h"
#include "gdalalg_vector_rasterize.h"
#include "gdalalg_vector_filter.h"
#include "gdalalg_vector_reproject.h"
#include "gdalalg_vector_select.h"
#include "gdalalg_vector_sql.h"

/************************************************************************/
/*                         GDALVectorAlgorithm                          */
/************************************************************************/

class GDALVectorAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "vector";
    static constexpr const char *DESCRIPTION = "Vector commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector.html";

    GDALVectorAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALVectorInfoAlgorithm>();
        RegisterSubAlgorithm<GDALVectorClipAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALVectorConcatAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALVectorConvertAlgorithm>();
        RegisterSubAlgorithm<GDALVectorEditAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALVectorGridAlgorithm>();
        RegisterSubAlgorithm<GDALVectorRasterizeAlgorithm>();
        RegisterSubAlgorithm<GDALVectorPipelineAlgorithm>();
        RegisterSubAlgorithm<GDALVectorFilterAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALVectorGeomAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALVectorReprojectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALVectorSelectAlgorithmStandalone>();
        RegisterSubAlgorithm<GDALVectorSQLAlgorithmStandalone>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "vector\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALVectorAlgorithm);

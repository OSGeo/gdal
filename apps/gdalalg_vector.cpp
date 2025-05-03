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

#include "gdal_priv.h"

#ifndef _
#define _(x) (x)
#endif

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
        AddArg("drivers", 0,
               _("Display vector driver list as JSON document and exit"),
               &m_drivers);

        AddOutputStringArg(&m_output);

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
    std::string m_output{};
    bool m_drivers = false;

    bool RunImpl(GDALProgressFunc, void *) override
    {
        if (m_drivers)
        {
            m_output = GDALPrintDriverList(GDAL_OF_VECTOR, true);
            return true;
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "The Run() method should not be called directly on the \"gdal "
                "vector\" program.");
            return false;
        }
    }
};

GDAL_STATIC_REGISTER_ALG(GDALVectorAlgorithm);

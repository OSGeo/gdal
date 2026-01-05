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

//! @cond Doxygen_Suppress

#include "gdalalg_vector.h"

#include "gdalalg_vector_info.h"
#include "gdalalg_vector_buffer.h"
#include "gdalalg_vector_check_geometry.h"
#include "gdalalg_vector_check_coverage.h"
#include "gdalalg_vector_clean_coverage.h"
#include "gdalalg_vector_clip.h"
#include "gdalalg_vector_concat.h"
#include "gdalalg_vector_convert.h"
#include "gdalalg_vector_edit.h"
#include "gdalalg_vector_explode_collections.h"
#include "gdalalg_vector_grid.h"
#include "gdalalg_vector_index.h"
#include "gdalalg_vector_layer_algebra.h"
#include "gdalalg_vector_make_point.h"
#include "gdalalg_vector_make_valid.h"
#include "gdalalg_vector_pipeline.h"
#include "gdalalg_vector_rasterize.h"
#include "gdalalg_vector_filter.h"
#include "gdalalg_vector_partition.h"
#include "gdalalg_vector_reproject.h"
#include "gdalalg_vector_segmentize.h"
#include "gdalalg_vector_select.h"
#include "gdalalg_vector_set_field_type.h"
#include "gdalalg_vector_set_geom_type.h"
#include "gdalalg_vector_simplify.h"
#include "gdalalg_vector_simplify_coverage.h"
#include "gdalalg_vector_sort.h"
#include "gdalalg_vector_sql.h"
#include "gdalalg_vector_update.h"
#include "gdalalg_vector_swap_xy.h"

#include "gdal_priv.h"

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                         GDALVectorAlgorithm                          */
/************************************************************************/

GDALVectorAlgorithm::GDALVectorAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddArg("drivers", 0,
           _("Display vector driver list as JSON document and exit"),
           &m_drivers);

    AddOutputStringArg(&m_output);

    RegisterSubAlgorithm<GDALVectorInfoAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorBufferAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorCheckCoverageAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorCheckGeometryAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorCleanCoverageAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorClipAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorConcatAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorConvertAlgorithm>();
    RegisterSubAlgorithm<GDALVectorEditAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorExplodeCollectionsAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorGridAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorRasterizeAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorPipelineAlgorithm>();
    RegisterSubAlgorithm<GDALVectorFilterAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorIndexAlgorithm>();
    RegisterSubAlgorithm<GDALVectorLayerAlgebraAlgorithm>();
    RegisterSubAlgorithm<GDALVectorMakePointAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorMakeValidAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorPartitionAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorReprojectAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSegmentizeAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSelectAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSetFieldTypeAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSetGeomTypeAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSimplifyAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSimplifyCoverageAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSortAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSQLAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorUpdateAlgorithmStandalone>();
    RegisterSubAlgorithm<GDALVectorSwapXYAlgorithmStandalone>();
}

bool GDALVectorAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    if (m_drivers)
    {
        m_output = GDALPrintDriverList(GDAL_OF_VECTOR, true);
        return true;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "vector\" program.");
        return false;
    }
}

//! @endcond

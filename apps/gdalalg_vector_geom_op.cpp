/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "geom-op" step of "vector pipeline", or "gdal vector geom-op" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_op.h"
#include "gdalalg_vector_geom_op_set_type.h"
#include "gdalalg_vector_geom_op_explode_collections.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*         GDALVectorGeomOpAlgorithm::GDALVectorGeomOpAlgorithm()       */
/************************************************************************/

GDALVectorGeomOpAlgorithm::GDALVectorGeomOpAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep = */ false)
{
    if (standaloneStep)
    {
        RegisterSubAlgorithm<GDALVectorGeomOpSetTypeAlgorithmStandalone>();
        RegisterSubAlgorithm<
            GDALVectorGeomOpExplodeCollectionsAlgorithmStandalone>();
    }
    else
    {
        RegisterSubAlgorithm<GDALVectorGeomOpSetTypeAlgorithm>();
        RegisterSubAlgorithm<GDALVectorGeomOpExplodeCollectionsAlgorithm>();
    }
}

/************************************************************************/
/*               GDALVectorGeomOpAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorGeomOpAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "vector geom-op\" program.");
    return false;
}

//! @endcond

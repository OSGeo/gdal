/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "geom" step of "vector pipeline", or "gdal vector geom" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom.h"
#include "gdalalg_vector_geom_set_type.h"
#include "gdalalg_vector_geom_explode_collections.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*           GDALVectorGeomAlgorithm::GDALVectorGeomAlgorithm()         */
/************************************************************************/

GDALVectorGeomAlgorithm::GDALVectorGeomAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep = */ false)
{
    if (standaloneStep)
    {
        RegisterSubAlgorithm<GDALVectorGeomSetTypeAlgorithmStandalone>();
        RegisterSubAlgorithm<
            GDALVectorGeomExplodeCollectionsAlgorithmStandalone>();
    }
    else
    {
        RegisterSubAlgorithm<GDALVectorGeomSetTypeAlgorithm>();
        RegisterSubAlgorithm<GDALVectorGeomExplodeCollectionsAlgorithm>();
    }
}

/************************************************************************/
/*                GDALVectorGeomAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALVectorGeomAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "vector geom\" program.");
    return false;
}

//! @endcond

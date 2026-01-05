/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "gdalalg_vsi.h"

#include "gdalalg_vsi_copy.h"
#include "gdalalg_vsi_delete.h"
#include "gdalalg_vsi_list.h"
#include "gdalalg_vsi_move.h"
#include "gdalalg_vsi_sync.h"
#include "gdalalg_vsi_sozip.h"

/************************************************************************/
/*                           GDALVSIAlgorithm                           */
/************************************************************************/

GDALVSIAlgorithm::GDALVSIAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    RegisterSubAlgorithm<GDALVSICopyAlgorithm>();
    RegisterSubAlgorithm<GDALVSIDeleteAlgorithm>();
    RegisterSubAlgorithm<GDALVSIListAlgorithm>();
    RegisterSubAlgorithm<GDALVSIMoveAlgorithm>();
    RegisterSubAlgorithm<GDALVSISyncAlgorithm>();
    RegisterSubAlgorithm<GDALVSISOZIPAlgorithm>();
}

bool GDALVSIAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "vsi\" program.");
    return false;
}

//! @endcond

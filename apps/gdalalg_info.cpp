/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "cpl_error.h"
#include "gdalalg_info.h"
#include "gdalalg_raster_info.h"
#include "gdalalg_vector_info.h"
#include "gdalalg_dispatcher.h"
#include "gdal_priv.h"

/************************************************************************/
/*                          GDALInfoAlgorithm                           */
/************************************************************************/

GDALInfoAlgorithm::GDALInfoAlgorithm()
    : GDALDispatcherAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    // only for the help message
    AddOutputFormatArg(&m_format).SetChoices("json", "text");
    AddInputDatasetArg(&m_dataset);

    m_longDescription = "For all options, run 'gdal raster info --help' or "
                        "'gdal vector info --help'";
}

bool GDALInfoAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "info\" program.");
    return false;
}

//! @endcond

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "gdalalg_dataset.h"

#include "gdalalg_dataset_identify.h"
#include "gdalalg_dataset_check.h"
#include "gdalalg_dataset_copy.h"
#include "gdalalg_dataset_rename.h"
#include "gdalalg_dataset_delete.h"

/************************************************************************/
/*                         GDALDatasetAlgorithm                         */
/************************************************************************/

GDALDatasetAlgorithm::GDALDatasetAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    RegisterSubAlgorithm<GDALDatasetIdentifyAlgorithm>();
    RegisterSubAlgorithm<GDALDatasetCheckAlgorithm>();
    RegisterSubAlgorithm<GDALDatasetCopyAlgorithm>();
    RegisterSubAlgorithm<GDALDatasetRenameAlgorithm>();
    RegisterSubAlgorithm<GDALDatasetDeleteAlgorithm>();
}

GDALDatasetAlgorithm::~GDALDatasetAlgorithm() = default;

//! @endcond

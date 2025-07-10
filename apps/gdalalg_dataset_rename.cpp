/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset rename" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_dataset_rename.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*       GDALDatasetRenameAlgorithm::GDALDatasetRenameAlgorithm()       */
/************************************************************************/

GDALDatasetRenameAlgorithm::GDALDatasetRenameAlgorithm()
    : GDALDatasetCopyRenameCommonAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
}

GDALDatasetRenameAlgorithm::~GDALDatasetRenameAlgorithm() = default;

//! @endcond

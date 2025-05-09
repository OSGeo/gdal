/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "manage-dataset rename" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_manage_dataset_rename.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/* GDALManageDatasetRenameAlgorithm::GDALManageDatasetRenameAlgorithm() */
/************************************************************************/

GDALManageDatasetRenameAlgorithm::GDALManageDatasetRenameAlgorithm()
    : GDALManageDatasetCopyRenameCommonAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
}

//! @endcond

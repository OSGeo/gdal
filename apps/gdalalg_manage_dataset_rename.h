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

#ifndef GDALALG_MANAGE_DATASET_RENAME_INCLUDED
#define GDALALG_MANAGE_DATASET_RENAME_INCLUDED

#include "gdalalgorithm.h"
#include "gdalalg_manage_dataset_copy.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALManageDatasetRenameAlgorithm                     */
/************************************************************************/

class GDALManageDatasetRenameAlgorithm final
    : public GDALManageDatasetCopyRenameCommonAlgorithm
{
  public:
    static constexpr const char *NAME = "rename";
    static constexpr const char *DESCRIPTION = "Rename files of a dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_manage_dataset_rename.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"ren", "mv"};
    }

    GDALManageDatasetRenameAlgorithm();
};

//! @endcond

#endif

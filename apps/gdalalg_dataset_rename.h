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

#ifndef GDALALG_DATASET_RENAME_INCLUDED
#define GDALALG_DATASET_RENAME_INCLUDED

#include "gdalalgorithm.h"
#include "gdalalg_dataset_copy.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALDatasetRenameAlgorithm                      */
/************************************************************************/

class GDALDatasetRenameAlgorithm final
    : public GDALDatasetCopyRenameCommonAlgorithm
{
  public:
    static constexpr const char *NAME = "rename";
    static constexpr const char *DESCRIPTION = "Rename files of a dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_dataset_rename.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"ren", "mv"};
    }

    GDALDatasetRenameAlgorithm();
    ~GDALDatasetRenameAlgorithm() override;
};

//! @endcond

#endif

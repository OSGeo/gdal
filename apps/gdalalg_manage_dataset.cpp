/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "manage-dataset" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"

#include "gdalalg_manage_dataset_identify.h"
#include "gdalalg_manage_dataset_copy.h"
#include "gdalalg_manage_dataset_rename.h"
#include "gdalalg_manage_dataset_delete.h"

/************************************************************************/
/*                      GDALManageDatasetAlgorithm                      */
/************************************************************************/

class GDALManageDatasetAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "manage-dataset";
    static constexpr const char *DESCRIPTION = "Commands to manage datasets.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_manage_dataset.html";

    GDALManageDatasetAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALManageDatasetIdentifyAlgorithm>();
        RegisterSubAlgorithm<GDALManageDatasetCopyAlgorithm>();
        RegisterSubAlgorithm<GDALManageDatasetRenameAlgorithm>();
        RegisterSubAlgorithm<GDALManageDatasetDeleteAlgorithm>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "manage-dataset\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALManageDatasetAlgorithm);

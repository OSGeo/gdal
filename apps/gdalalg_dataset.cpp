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

#include "gdalalgorithm.h"

#include "gdalalg_dataset_identify.h"
#include "gdalalg_dataset_copy.h"
#include "gdalalg_dataset_rename.h"
#include "gdalalg_dataset_delete.h"

/************************************************************************/
/*                         GDALDatasetAlgorithm                         */
/************************************************************************/

class GDALDatasetAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "dataset";
    static constexpr const char *DESCRIPTION = "Commands to manage datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_dataset.html";

    GDALDatasetAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALDatasetIdentifyAlgorithm>();
        RegisterSubAlgorithm<GDALDatasetCopyAlgorithm>();
        RegisterSubAlgorithm<GDALDatasetRenameAlgorithm>();
        RegisterSubAlgorithm<GDALDatasetDeleteAlgorithm>();
    }

    ~GDALDatasetAlgorithm() override;

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "dataset\" program.");
        return false;
    }
};

GDALDatasetAlgorithm::~GDALDatasetAlgorithm() = default;

GDAL_STATIC_REGISTER_ALG(GDALDatasetAlgorithm);

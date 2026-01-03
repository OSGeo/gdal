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

#ifndef GDALALG_DATASET_INCLUDED
#define GDALALG_DATASET_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

/************************************************************************/
/*                         GDALDatasetAlgorithm                         */
/************************************************************************/

class GDALDatasetAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "dataset";
    static constexpr const char *DESCRIPTION = "Commands to manage datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_dataset.html";

    GDALDatasetAlgorithm();
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

//! @endcond

#endif

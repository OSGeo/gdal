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

#include "gdalalgorithm.h"

#include "gdalalg_vsi_copy.h"
#include "gdalalg_vsi_delete.h"
#include "gdalalg_vsi_list.h"
#include "gdalalg_vsi_move.h"
#include "gdalalg_vsi_sync.h"
#include "gdalalg_vsi_sozip.h"

/************************************************************************/
/*                           GDALVSIAlgorithm                           */
/************************************************************************/

class GDALVSIAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "vsi";
    static constexpr const char *DESCRIPTION =
        "GDAL Virtual System Interface (VSI) commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi.html";

    GDALVSIAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        RegisterSubAlgorithm<GDALVSICopyAlgorithm>();
        RegisterSubAlgorithm<GDALVSIDeleteAlgorithm>();
        RegisterSubAlgorithm<GDALVSIListAlgorithm>();
        RegisterSubAlgorithm<GDALVSIMoveAlgorithm>();
        RegisterSubAlgorithm<GDALVSISyncAlgorithm>();
        RegisterSubAlgorithm<GDALVSISOZIPAlgorithm>();
    }

  private:
    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "vsi\" program.");
        return false;
    }
};

GDAL_STATIC_REGISTER_ALG(GDALVSIAlgorithm);

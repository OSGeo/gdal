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

#ifndef GDALALG_VSI_INCLUDED
#define GDALALG_VSI_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

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

    GDALVSIAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi sozip" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VSI_SOZIP_INCLUDED
#define GDALALG_VSI_SOZIP_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALVSISOZIPAlgorithm                         */
/************************************************************************/

class GDALVSISOZIPAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "sozip";
    static constexpr const char *DESCRIPTION =
        "Seek-optimized ZIP (SOZIP) commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_sozip.html";

    GDALVSISOZIPAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif

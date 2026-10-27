/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "crs" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_CRS_INCLUDED
#define GDALALG_CRS_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"
#include "gdalalg_crs_info.h"
#include "gdalalgorithm.h"

/************************************************************************/
/*                           GDALCRSAlgorithm                           */
/************************************************************************/

class GDALCRSAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "crs";
    static constexpr const char *DESCRIPTION = "Commands to manage a CRS.";
    static constexpr const char *HELP_URL = "/programs/gdal_crs.html";

    GDALCRSAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif

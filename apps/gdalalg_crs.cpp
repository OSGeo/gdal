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

//! @cond Doxygen_Suppress

#include "cpl_error.h"
#include "gdalalg_crs.h"
#include "gdalalg_crs_info.h"
#include "gdalalgorithm.h"
#include "gdal_priv.h"

/************************************************************************/
/*                           GDALCRSAlgorithm                           */
/************************************************************************/

GDALCRSAlgorithm::GDALCRSAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    RegisterSubAlgorithm<GDALCRSInfoAlgorithm>();
}

//! @endcond

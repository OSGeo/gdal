/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_INFO_INCLUDED
#define GDALALG_INFO_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"
#include "gdalalg_raster_info.h"
#include "gdalalg_vector_info.h"
#include "gdalalg_dispatcher.h"

/************************************************************************/
/*                          GDALInfoAlgorithm                           */
/************************************************************************/

class GDALInfoAlgorithm final
    : public GDALDispatcherAlgorithm<GDALRasterInfoAlgorithm,
                                     GDALVectorInfoAlgorithm>
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a dataset (shortcut for 'gdal raster info' or "
        "'gdal vector info').";
    static constexpr const char *HELP_URL = "/programs/gdal_info.html";

    GDALInfoAlgorithm();

  private:
    std::string m_format{};
    GDALArgDatasetValue m_dataset{};

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif

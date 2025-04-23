/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Raster Interpolation
 * Purpose:  Interpolation algorithms with cache
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2024, Javier Jimenez Shaw
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_INTERPOLATEATPOINT_H_INCLUDED
#define GDAL_INTERPOLATEATPOINT_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include "gdal.h"

#include "cpl_mem_cache.h"
#include "gdal_priv.h"

#include <memory>

using DoublePointsCache =
    lru11::Cache<uint64_t, std::shared_ptr<std::vector<double>>>;

class CPL_DLL GDALDoublePointsCache
{
  public:
    std::unique_ptr<DoublePointsCache> cache{};
};

bool CPL_DLL GDALInterpolateAtPoint(GDALRasterBand *pBand,
                                    GDALRIOResampleAlg eResampleAlg,
                                    std::unique_ptr<DoublePointsCache> &cache,
                                    const double dfXIn, const double dfYIn,
                                    double *pdfOutputReal,
                                    double *pdfOutputImag);

/*! @endcond */

#endif /* ndef GDAL_INTERPOLATEATPOINT_H_INCLUDED */

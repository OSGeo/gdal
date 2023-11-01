/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  netCDF driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef NETCDFDRIVERCORE_H
#define NETCDFDRIVERCORE_H

#include "gdal_priv.h"

#include "netcdfformatenum.h"

constexpr const char *DRIVER_NAME = "netCDF";
constexpr const char *LONG_NAME = "Network Common Data Format";
constexpr const char *EXTENSIONS = "nc";
constexpr const char *OPENOPTIONLIST =
    "<OpenOptionList>"
    "   <Option name='HONOUR_VALID_RANGE' type='boolean' scope='raster' "
    "description='Whether to set to nodata pixel values outside of the "
    "validity range' default='YES'/>"
    "   <Option name='IGNORE_XY_AXIS_NAME_CHECKS' type='boolean' "
    "scope='raster' "
    "description='Whether X/Y dimensions should be always considered as "
    "geospatial axis, even if the lack conventional attributes confirming "
    "it.'"
    " default='NO'/>"
    "   <Option name='VARIABLES_AS_BANDS' type='boolean' scope='raster' "
    "description='Whether 2D variables that share the same indexing "
    "dimensions "
    "should be exposed as several bands of a same dataset instead of "
    "several "
    "subdatasets.' default='NO'/>"
    "   <Option name='ASSUME_LONGLAT' type='boolean' scope='raster' "
    "description='Whether when all else has failed for determining a CRS, "
    "a "
    "meaningful geotransform has been found, and is within the  "
    "bounds -180,360 -90,90, assume OGC:CRS84.' default='NO'/>"
    "   <Option name='PRESERVE_AXIS_UNIT_IN_CRS' type='boolean' "
    "scope='raster' description='Whether unusual linear axis unit (km) "
    "should be kept as such, instead of being normalized to metre' "
    "default='NO'/>"
    "</OpenOptionList>";

NetCDFFormatEnum CPL_DLL netCDFIdentifyFormat(GDALOpenInfo *poOpenInfo,
                                              bool bCheckExt);
int CPL_DLL netCDFDatasetIdentify(GDALOpenInfo *poOpenInfo);
GDALSubdatasetInfo CPL_DLL *
NCDFDriverGetSubdatasetInfo(const char *pszFileName);

#endif

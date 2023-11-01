/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF5 driver
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

#ifndef HDF5DRIVERCORE_H
#define HDF5DRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *HDF5_DRIVER_NAME = "HDF5";
constexpr const char *HDF5_LONG_NAME = "Hierarchical Data Format Release 5";
constexpr const char *HDF5_EXTENSIONS = "h5 hdf5";

constexpr const char *HDF5_IMAGE_DRIVER_NAME = "HDF5Image";
constexpr const char *HDF5_IMAGE_LONG_NAME = "HDF5 Dataset";

constexpr const char *BAG_DRIVER_NAME = "BAG";
constexpr const char *BAG_LONG_NAME = "Bathymetry Attributed Grid";
constexpr const char *BAG_EXTENSIONS = "bag";
constexpr const char *BAG_OPENOPTIONLIST =
    "<OpenOptionList>"
    "   <Option name='MODE' type='string-select' default='AUTO'>"
    "       <Value>AUTO</Value>"
    "       <Value>LOW_RES_GRID</Value>"
    "       <Value>LIST_SUPERGRIDS</Value>"
    "       <Value>RESAMPLED_GRID</Value>"
    "       <Value>INTERPOLATED</Value>"
    "   </Option>"
    "   <Option name='SUPERGRIDS_INDICES' type='string' description="
    "'Tuple(s) (y1,x1),(y2,x2),...  of supergrids, by indices, to expose "
    "as subdatasets'/>"
    "   <Option name='MINX' type='float' description='Minimum X value of "
    "area of interest'/>"
    "   <Option name='MINY' type='float' description='Minimum Y value of "
    "area of interest'/>"
    "   <Option name='MAXX' type='float' description='Maximum X value of "
    "area of interest'/>"
    "   <Option name='MAXY' type='float' description='Maximum Y value of "
    "area of interest'/>"
    "   <Option name='RESX' type='float' description="
    "'Horizontal resolution. Only used for "
    "MODE=RESAMPLED_GRID/INTERPOLATED'/>"
    "   <Option name='RESY' type='float' description="
    "'Vertical resolution (positive value). Only used for "
    "MODE=RESAMPLED_GRID/INTERPOLATED'/>"
    "   <Option name='RES_STRATEGY' type='string-select' description="
    "'Which strategy to apply to select the resampled grid resolution. "
    "Only used for MODE=RESAMPLED_GRID/INTERPOLATED' default='AUTO'>"
    "       <Value>AUTO</Value>"
    "       <Value>MIN</Value>"
    "       <Value>MAX</Value>"
    "       <Value>MEAN</Value>"
    "   </Option>"
    "   <Option name='RES_FILTER_MIN' type='float' description="
    "'Minimum resolution of supergrids to take into account (excluded "
    "bound). "
    "Only used for MODE=RESAMPLED_GRID, INTERPOLATED or LIST_SUPERGRIDS' "
    "default='0'/>"
    "   <Option name='RES_FILTER_MAX' type='float' description="
    "'Maximum resolution of supergrids to take into account (included "
    "bound). "
    "Only used for MODE=RESAMPLED_GRID, INTERPOLATED or LIST_SUPERGRIDS' "
    "default='inf'/>"
    "   <Option name='VALUE_POPULATION' type='string-select' description="
    "'Which value population strategy to apply to compute the resampled "
    "cell "
    "values. Only used for MODE=RESAMPLED_GRID' default='MAX'>"
    "       <Value>MIN</Value>"
    "       <Value>MAX</Value>"
    "       <Value>MEAN</Value>"
    "       <Value>COUNT</Value>"
    "   </Option>"
    "   <Option name='SUPERGRIDS_MASK' type='boolean' description="
    "'Whether the dataset should consist of a mask band indicating if a "
    "supergrid node matches each target pixel. Only used for "
    "MODE=RESAMPLED_GRID' default='NO'/>"
    "   <Option name='NODATA_VALUE' type='float' default='1000000'/>"
    "   <Option name='REPORT_VERTCRS' type='boolean' default='YES'/>"
    "</OpenOptionList>";

constexpr const char *S102_DRIVER_NAME = "S102";
constexpr const char *S102_LONG_NAME = "S-102 Bathymetric Surface Product";
constexpr const char *S102_EXTENSIONS = "h5";
constexpr const char *S102_OPENOPTIONLIST =
    "<OpenOptionList>"
    "   <Option name='DEPTH_OR_ELEVATION' type='string-select' "
    "default='DEPTH'>"
    "       <Value>DEPTH</Value>"
    "       <Value>ELEVATION</Value>"
    "   </Option>"
    "   <Option name='NORTH_UP' type='boolean' default='YES' "
    "description='Whether the top line of the dataset should be the "
    "northern-most one'/>"
    "</OpenOptionList>";

int CPL_DLL HDF5DatasetIdentify(GDALOpenInfo *poOpenInfo);
GDALSubdatasetInfo CPL_DLL *
HDF5DriverGetSubdatasetInfo(const char *pszFileName);

int CPL_DLL HDF5ImageDatasetIdentify(GDALOpenInfo *poOpenInfo);

int CPL_DLL BAGDatasetIdentify(GDALOpenInfo *poOpenInfo);

int CPL_DLL S102DatasetIdentify(GDALOpenInfo *poOpenInfo);

#endif

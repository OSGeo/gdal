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

constexpr const char *HDF5_IMAGE_DRIVER_NAME = "HDF5Image";

constexpr const char *BAG_DRIVER_NAME = "BAG";

constexpr const char *S102_DRIVER_NAME = "S102";

constexpr const char *S104_DRIVER_NAME = "S104";

constexpr const char *S111_DRIVER_NAME = "S111";

int CPL_DLL HDF5DatasetIdentify(GDALOpenInfo *poOpenInfo);

int CPL_DLL HDF5ImageDatasetIdentify(GDALOpenInfo *poOpenInfo);

int CPL_DLL BAGDatasetIdentify(GDALOpenInfo *poOpenInfo);

int CPL_DLL S102DatasetIdentify(GDALOpenInfo *poOpenInfo);

int CPL_DLL S104DatasetIdentify(GDALOpenInfo *poOpenInfo);

int CPL_DLL S111DatasetIdentify(GDALOpenInfo *poOpenInfo);

void CPL_DLL HDF5DriverSetCommonMetadata(GDALDriver *poDriver);

void CPL_DLL HDF5ImageDriverSetCommonMetadata(GDALDriver *poDriver);

void CPL_DLL BAGDriverSetCommonMetadata(GDALDriver *poDriver);

void CPL_DLL S102DriverSetCommonMetadata(GDALDriver *poDriver);

void CPL_DLL S104DriverSetCommonMetadata(GDALDriver *poDriver);

void CPL_DLL S111DriverSetCommonMetadata(GDALDriver *poDriver);

#endif

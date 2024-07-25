/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  ECW driver
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

#ifndef ECWDRIVERCORE_H
#define ECWDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *ECW_DRIVER_NAME = "ECW";

constexpr const char *JP2ECW_DRIVER_NAME = "JP2ECW";

#define ECWDatasetIdentifyECW PLUGIN_SYMBOL_NAME(ECWDatasetIdentifyECW)
#define ECWDatasetIdentifyJPEG2000                                             \
    PLUGIN_SYMBOL_NAME(ECWDatasetIdentifyJPEG2000)
#define ECWDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(ECWDriverSetCommonMetadata)
#define JP2ECWDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(JP2ECWDriverSetCommonMetadata)

int ECWDatasetIdentifyECW(GDALOpenInfo *poOpenInfo);

int ECWDatasetIdentifyJPEG2000(GDALOpenInfo *poOpenInfo);

void ECWDriverSetCommonMetadata(GDALDriver *poDriver);

void JP2ECWDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

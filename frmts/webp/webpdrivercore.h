/******************************************************************************
 *
 * Project:  GDAL WEBP Driver
 * Purpose:  Implement GDAL WEBP Support based on libwebp
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef WEBPDRIVERCORE_H
#define WEBPDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "WEBP";

#define WEBPDriverIdentify PLUGIN_SYMBOL_NAME(WEBPDriverIdentify)
#define WEBPDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(WEBPDriverSetCommonMetadata)

int WEBPDriverIdentify(GDALOpenInfo *poOpenInfo);

void WEBPDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

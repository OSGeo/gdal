/******************************************************************************
 * Project:  GDAL
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Purpose:
 * JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2014-2016, Even Rouault
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

#ifndef JP2LURADRIVERCORE_H
#define JP2LURADRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "JP2Lura";

constexpr unsigned char jpc_header[] = {0xff, 0x4f, 0xff,
                                        0x51};  // SOC + RSIZ markers
constexpr unsigned char jp2_box_jp[] = {0x6a, 0x50, 0x20, 0x20}; /* 'jP  ' */

#define JP2LuraDriverIdentify PLUGIN_SYMBOL_NAME(JP2LuraDriverIdentify)
#define JP2LuraDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(JP2LuraDriverSetCommonMetadata)

int JP2LuraDriverIdentify(GDALOpenInfo *poOpenInfo);

void JP2LuraDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

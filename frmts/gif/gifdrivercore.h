/******************************************************************************
 *
 * Project:  GIF Driver
 * Purpose:  Implement GDAL GIF Support using libungif code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GIFDRIVERCORE_H
#define GIFDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *GIF_DRIVER_NAME = "GIF";

constexpr const char *BIGGIF_DRIVER_NAME = "BIGGIF";

#define GIFDriverIdentify PLUGIN_SYMBOL_NAME(GIFDriverIdentify)
#define BIGGIFDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(BIGGIFDriverSetCommonMetadata)
#define GIFDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(GIFDriverSetCommonMetadata)

int GIFDriverIdentify(GDALOpenInfo *poOpenInfo);

void BIGGIFDriverSetCommonMetadata(GDALDriver *poDriver);

void GIFDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

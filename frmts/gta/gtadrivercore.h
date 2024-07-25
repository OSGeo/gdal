/******************************************************************************
 *
 * Project:  GTA read/write Driver
 * Purpose:  GDAL bindings over GTA library.
 * Author:   Martin Lambers, marlam@marlam.de
 *
 ******************************************************************************
 * Copyright (c) 2010, 2011, Martin Lambers <marlam@marlam.de>
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

#ifndef GTADRIVERCORE_H
#define GTADRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "GTA";

#define GTADriverIdentify PLUGIN_SYMBOL_NAME(GTADriverIdentify)

#define GTADriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(GTADriverSetCommonMetadata)

int GTADriverIdentify(GDALOpenInfo *poOpenInfo);

void GTADriverSetCommonMetadata(GDALDriver *poDriver);

#endif

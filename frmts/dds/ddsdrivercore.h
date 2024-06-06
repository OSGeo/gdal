/******************************************************************************
 *
 * Project:  DDS Driver
 * Purpose:  Implement GDAL DDS Support
 * Author:   Alan Boudreault, aboudreault@mapgears.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Alan Boudreault
 * Copyright (c) 2013,2019, Even Rouault <even dot rouault at spatialys.com>
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
 ******************************************************************************/

#ifndef DDSDRIVERCORE_H
#define DDSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "DDS";

#define DDS_SIGNATURE "DDS "

#define DDSDriverIdentify PLUGIN_SYMBOL_NAME(DDSDriverIdentify)
#define DDSDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(DDSDriverSetCommonMetadata)

int DDSDriverIdentify(GDALOpenInfo *poOpenInfo);

void DDSDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

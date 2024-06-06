/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  MrSID driver
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

#ifndef MrSIDDRIVERCORE_H
#define MrSIDDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *MRSID_DRIVER_NAME = "MrSID";

constexpr const char *JP2MRSID_DRIVER_NAME = "JP2MrSID";

#define MrSIDIdentify PLUGIN_SYMBOL_NAME(MrSIDIdentify)
#define MrSIDJP2Identify PLUGIN_SYMBOL_NAME(MrSIDJP2Identify)
#define MrSIDDriverSetCommonMetadata                                           \
    PLUGIN_SYMBOL_NAME(MrSIDDriverSetCommonMetadata)
#define JP2MrSIDDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(JP2MrSIDDriverSetCommonMetadata)

int MrSIDIdentify(GDALOpenInfo *poOpenInfo);

int MrSIDJP2Identify(GDALOpenInfo *poOpenInfo);

void MrSIDDriverSetCommonMetadata(GDALDriver *poDriver);

void JP2MrSIDDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

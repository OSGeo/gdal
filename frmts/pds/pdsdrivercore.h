/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Planetary drivers
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault
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

#ifndef PDSDRIVERCORE_H
#define PDSDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *PDS_DRIVER_NAME = "PDS";

#define GetVICARLabelOffsetFromPDS3                                            \
    PLUGIN_SYMBOL_NAME(GetVICARLabelOffsetFromPDS3)
#define PDSDriverIdentify PLUGIN_SYMBOL_NAME(PDSDriverIdentify)
#define PDSDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(PDSDriverSetCommonMetadata)

vsi_l_offset GetVICARLabelOffsetFromPDS3(const char *pszHdr, VSILFILE *fp,
                                         std::string &osVICARHeader);

int PDSDriverIdentify(GDALOpenInfo *poOpenInfo);

void PDSDriverSetCommonMetadata(GDALDriver *poDriver);

constexpr const char *PDS4_DRIVER_NAME = "PDS4";

#define PDS4DriverIdentify PLUGIN_SYMBOL_NAME(PDS4DriverIdentify)
#define PDS4DriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(PDS4DriverSetCommonMetadata)

int PDS4DriverIdentify(GDALOpenInfo *poOpenInfo);

void PDS4DriverSetCommonMetadata(GDALDriver *poDriver);

constexpr const char *ISIS2_DRIVER_NAME = "ISIS2";

#define ISIS2DriverIdentify PLUGIN_SYMBOL_NAME(ISIS2DriverIdentify)
#define ISIS2DriverSetCommonMetadata                                           \
    PLUGIN_SYMBOL_NAME(ISIS2DriverSetCommonMetadata)

int ISIS2DriverIdentify(GDALOpenInfo *poOpenInfo);

void ISIS2DriverSetCommonMetadata(GDALDriver *poDriver);

constexpr const char *ISIS3_DRIVER_NAME = "ISIS3";

#define ISIS3DriverIdentify PLUGIN_SYMBOL_NAME(ISIS3DriverIdentify)
#define ISIS3DriverSetCommonMetadata                                           \
    PLUGIN_SYMBOL_NAME(ISIS3DriverSetCommonMetadata)

int ISIS3DriverIdentify(GDALOpenInfo *poOpenInfo);

void ISIS3DriverSetCommonMetadata(GDALDriver *poDriver);

constexpr const char *VICAR_DRIVER_NAME = "VICAR";

#define VICARGetLabelOffset PLUGIN_SYMBOL_NAME(VICARGetLabelOffset)
#define VICARDriverSetCommonMetadata                                           \
    PLUGIN_SYMBOL_NAME(VICARDriverSetCommonMetadata)

int VICARGetLabelOffset(GDALOpenInfo *poOpenInfo);

void VICARDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

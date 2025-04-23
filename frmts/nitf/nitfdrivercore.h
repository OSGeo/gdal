/******************************************************************************
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  NITFDataset and driver related implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef NITFDRIVERCORE_H
#define NITFDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *NITF_DRIVER_NAME = "NITF";

#define NITFDriverIdentify PLUGIN_SYMBOL_NAME(NITFDriverIdentify)
#define NITFDriverSetCommonMetadata                                            \
    PLUGIN_SYMBOL_NAME(NITFDriverSetCommonMetadata)

int NITFDriverIdentify(GDALOpenInfo *poOpenInfo);

void NITFDriverSetCommonMetadata(GDALDriver *poDriver);

constexpr const char *RPFTOC_DRIVER_NAME = "RPFTOC";

#define RPFTOCDriverIdentify PLUGIN_SYMBOL_NAME(RPFTOCDriverIdentify)
#define RPFTOCDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(RPFTOCDriverSetCommonMetadata)
#define RPFTOCIsNonNITFFileTOC PLUGIN_SYMBOL_NAME(RPFTOCIsNonNITFFileTOC)

int RPFTOCDriverIdentify(GDALOpenInfo *poOpenInfo);

void RPFTOCDriverSetCommonMetadata(GDALDriver *poDriver);

int RPFTOCIsNonNITFFileTOC(GDALOpenInfo *poOpenInfo, const char *pszFilename);

constexpr const char *ECRGTOC_DRIVER_NAME = "ECRGTOC";

#define ECRGTOCDriverIdentify PLUGIN_SYMBOL_NAME(ECRGTOCDriverIdentify)
#define ECRGTOCDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(ECRGTOCDriverSetCommonMetadata)

int ECRGTOCDriverIdentify(GDALOpenInfo *poOpenInfo);

void ECRGTOCDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

/******************************************************************************
 *
 * Project:  MSG Driver
 * Purpose:  GDALDataset driver for MSG translator for read support.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef MSGDRIVERCORE_H
#define MSGDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "MSG";

#define MSGDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(MSGDriverSetCommonMetadata)

void MSGDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Definition of OGRXODRDriverCore.
 * Author:   Michael Scholz, German Aerospace Center (DLR)
 *           Gülsen Bardak, German Aerospace Center (DLR)        
 *
 ******************************************************************************
 * Copyright 2024 German Aerospace Center (DLR), Institute of Transportation Systems
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#pragma once

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "XODR";

#define OGRXODRDriverIdentify PLUGIN_SYMBOL_NAME(OGRXODRDriverIdentify)
#define OGRXODRDriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(OGRXODRDriverSetCommonMetadata)

int OGRXODRDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRXODRDriverSetCommonMetadata(GDALDriver *poDriver);
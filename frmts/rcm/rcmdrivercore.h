/******************************************************************************
 *
 * Project:  DRDC Ottawa GEOINT
 * Purpose:  Radarsat Constellation Mission - XML Products (product.xml) driver
 * Author:   Roberto Caron, MDA
 *           on behalf of DRDC Ottawa
 *
 ******************************************************************************
 * Copyright (c) 2020, DRDC Ottawa
 *
 * Based on the RS2 Dataset Class
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RCMDRIVERCORE_H
#define RCMDRIVERCORE_H

#include "gdal_priv.h"

// Should be size of larged possible filename.
constexpr int CPL_PATH_BUF_SIZE = 2048;
constexpr char szLayerCalibration[] = "RCM_CALIB";
constexpr char chLayerSeparator = ':';
constexpr char szSIGMA0[] = "SIGMA0";
constexpr char szGAMMA[] = "GAMMA";
constexpr char szBETA0[] = "BETA0";
constexpr char szUNCALIB[] = "UNCALIB";

constexpr const char *RCM_DRIVER_NAME = "RCM";

/*** Function to concat 'metadata' with a folder separator with the filename
 * 'product.xml'  ***/
/*
 *  Should return either 'metadata\product.xml' or 'metadata/product.xml'
 */
inline CPLString GetMetadataProduct()
{
    // Always begin by the layer calibration name
    CPLString ptr;
    ptr.append("metadata");
    ptr.append("/");
    ptr.append("product.xml");

    /* return metadata product filename */
    return ptr;
}

int CPL_DLL RCMDatasetIdentify(GDALOpenInfo *poOpenInfo);

void CPL_DLL RCMDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

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
constexpr char szLayerSeparator[] = ":";
constexpr char szSIGMA0[] = "SIGMA0";
constexpr char szGAMMA[] = "GAMMA";
constexpr char szBETA0[] = "BETA0";
constexpr char szUNCALIB[] = "UNCALIB";
constexpr char szPathSeparator[] =
#ifdef _WIN32 /* Defined if Win32 and Win64 */
    "\\";
#else
    "/";
#endif
constexpr char cPathSeparator =
#ifdef _WIN32 /* Defined if Win32 and Win64 */
    '\\';
#else
    '/';
#endif
constexpr char cOppositePathSeparator =
#ifdef _WIN32 /* Defined if Win32 and Win64 */
    '/';
#else
    '\\';
#endif

constexpr const char *RCM_DRIVER_NAME = "RCM";

/*** Function to format calibration for unique identification for Layer Name
 * ***/
/*
 *  RCM_CALIB : { SIGMA0 | GAMMA0 | BETA0 | UNCALIB } : product.xml full path
 */
inline CPLString FormatCalibration(const char *pszCalibName,
                                   const char *pszFilename)
{
    CPLString ptr;

    // Always begin by the layer calibrtion name
    ptr.append(szLayerCalibration);

    if (pszCalibName != nullptr || pszFilename != nullptr)
    {
        if (pszCalibName != nullptr)
        {
            // A separator is needed before concat calibration name
            ptr.append(szLayerSeparator);
            // Add calibration name
            ptr.append(pszCalibName);
        }

        if (pszFilename != nullptr)
        {
            // A separator is needed before concat full filename name
            ptr.append(szLayerSeparator);
            // Add full filename name
            ptr.append(pszFilename);
        }
    }
    else
    {
        // Always add a separator even though there are no name to concat
        ptr.append(szLayerSeparator);
    }

    /* return calibration format */
    return ptr;
}

/*** Function to concat 'metadata' with a folder separator with the filename
 * 'product.xml'  ***/
/*
 *  Should return either 'metadata\product.xml' or 'metadata/product.xml'
 */
inline CPLString GetMetadataProduct()
{
    // Always begin by the layer calibrtion name
    CPLString ptr;
    ptr.append("metadata");
    ptr.append(szPathSeparator);
    ptr.append("product.xml");

    /* return metadata product filename */
    return ptr;
}

int CPL_DLL RCMDatasetIdentify(GDALOpenInfo *poOpenInfo);

void CPL_DLL RCMDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

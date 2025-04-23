/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements translation between GeoTIFF normalized projection
 *           definitions and OpenGIS WKT SRS format.
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GT_WKT_SRS_H_INCLUDED
#define GT_WKT_SRS_H_INCLUDED

#include "cpl_port.h"
#include "ogr_srs_api.h"

#include "geo_normalize.h"
#include "geotiff.h"

CPL_C_START
char CPL_DLL *GTIFGetOGISDefn(GTIF *, GTIFDefn *);
int CPL_DLL GTIFSetFromOGISDefn(GTIF *, const char *);

typedef enum
{
    GEOTIFF_KEYS_STANDARD,
    GEOTIFF_KEYS_ESRI_PE
} GTIFFKeysFlavorEnum;

typedef enum
{
    GEOTIFF_VERSION_AUTO,
    GEOTIFF_VERSION_1_0,
    GEOTIFF_VERSION_1_1
} GeoTIFFVersionEnum;

OGRSpatialReferenceH GTIFGetOGISDefnAsOSR(GTIF *, GTIFDefn *);

int GTIFSetFromOGISDefnEx(GTIF *, OGRSpatialReferenceH, GTIFFKeysFlavorEnum,
                          GeoTIFFVersionEnum);

CPL_C_END

#endif  // GT_WKT_SRS_H_INCLUDED

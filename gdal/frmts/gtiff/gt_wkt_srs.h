/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements translation between GeoTIFF normalized projection
 *           definitions and OpenGIS WKT SRS format.
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GT_WKT_SRS_H_INCLUDED
#define GT_WKT_SRS_H_INCLUDED

#include "cpl_port.h"
#include "ogr_srs_api.h"

#include "geo_normalize.h"
#include "geotiff.h"

CPL_C_START
char CPL_DLL *GTIFGetOGISDefn( GTIF *, GTIFDefn * );
int  CPL_DLL GTIFSetFromOGISDefn( GTIF *, const char * );

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

OGRSpatialReferenceH GTIFGetOGISDefnAsOSR( GTIF *, GTIFDefn * );

int GTIFSetFromOGISDefnEx( GTIF *, OGRSpatialReferenceH, GTIFFKeysFlavorEnum,
                           GeoTIFFVersionEnum );

CPL_C_END

#endif // GT_WKT_SRS_H_INCLUDED

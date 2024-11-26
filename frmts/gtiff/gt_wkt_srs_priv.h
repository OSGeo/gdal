/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Internal methods of gt_wkt_srs.cpp shared with gt_citation.cpp
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GT_WKT_SRS_PRIV_H_INCLUDED
#define GT_WKT_SRS_PRIV_H_INCLUDED

#include "geotiff.h"

#if LIBGEOTIFF_VERSION >= 1600

#define GDALGTIFKeyGetASCII GTIFKeyGetASCII
#define GDALGTIFKeyGetSHORT GTIFKeyGetSHORT
#define GDALGTIFKeyGetDOUBLE GTIFKeyGetDOUBLE

#else

int CPL_DLL GDALGTIFKeyGetASCII(GTIF *hGTIF, geokey_t key, char *szStr,
                                int szStrMaxLen);

int CPL_DLL GDALGTIFKeyGetSHORT(GTIF *hGTIF, geokey_t key,
                                unsigned short *pnVal, int nIndex, int nCount);

int CPL_DLL GDALGTIFKeyGetDOUBLE(GTIF *hGTIF, geokey_t key, double *pdfVal,
                                 int nIndex, int nCount);

#endif

#endif  // GT_WKT_SRS_PRIV_H_INCLUDED

/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Read/Write in-memory GeoTIFF file
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GT_WKT_SRS_FOR_GDAL_H_INCLUDED
#define GT_WKT_SRS_FOR_GDAL_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"
#include "ogr_srs_api.h"

CPL_C_START

CPLErr CPL_DLL GTIFMemBufFromWkt(const char *pszWKT,
                                 const double *padfGeoTransform, int nGCPCount,
                                 const GDAL_GCP *pasGCPList, int *pnSize,
                                 unsigned char **ppabyBuffer);

CPLErr GTIFMemBufFromSRS(OGRSpatialReferenceH hSRS,
                         const double *padfGeoTransform, int nGCPCount,
                         const GDAL_GCP *pasGCPList, int *pnSize,
                         unsigned char **ppabyBuffer, int bPixelIsPoint,
                         char **papszRPCMD);

CPLErr CPL_DLL GTIFWktFromMemBuf(int nSize, unsigned char *pabyBuffer,
                                 char **ppszWKT, double *padfGeoTransform,
                                 int *pnGCPCount, GDAL_GCP **ppasGCPList);

CPLErr GTIFWktFromMemBufEx(int nSize, unsigned char *pabyBuffer,
                           OGRSpatialReferenceH *phSRS,
                           double *padfGeoTransform, int *pnGCPCount,
                           GDAL_GCP **ppasGCPList, int *pbPixelIsPoint,
                           char ***ppapszRPCMD);

CPL_C_END

#endif  // GT_WKT_SRS_FOR_GDAL_H_INCLUDED

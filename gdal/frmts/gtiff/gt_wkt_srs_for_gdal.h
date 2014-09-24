/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Read/Write in-memory GeoTIFF file
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GT_WKT_SRS_FOR_GDAL_H_INCLUDED
#define GT_WKT_SRS_FOR_GDAL_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"

CPL_C_START

CPLErr CPL_DLL GTIFMemBufFromWkt( const char *pszWKT,
                                  const double *padfGeoTransform,
                                  int nGCPCount, const GDAL_GCP *pasGCPList,
                                  int *pnSize, unsigned char **ppabyBuffer );

CPLErr GTIFMemBufFromWktEx( const char *pszWKT,
                            const double *padfGeoTransform,
                            int nGCPCount, const GDAL_GCP *pasGCPList,
                            int *pnSize, unsigned char **ppabyBuffer,
                            int bPixelIsPoint );

CPLErr CPL_DLL GTIFWktFromMemBuf( int nSize, unsigned char *pabyBuffer,
                          char **ppszWKT, double *padfGeoTransform,
                          int *pnGCPCount, GDAL_GCP **ppasGCPList );

CPLErr GTIFWktFromMemBufEx( int nSize, unsigned char *pabyBuffer, 
                            char **ppszWKT, double *padfGeoTransform,
                            int *pnGCPCount, GDAL_GCP **ppasGCPList,
                            int *pbPixelIsPoint );

CPL_C_END;

#endif // GT_WKT_SRS_FOR_GDAL_H_INCLUDED

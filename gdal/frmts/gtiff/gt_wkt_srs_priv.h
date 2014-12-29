/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Internal methods of gt_wkt_srs.cpp shared with gt_citation.cpp
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef GT_WKT_SRS_PRIV_H_INCLUDED
#define GT_WKT_SRS_PRIV_H_INCLUDED

#include "geotiff.h"

int GDALGTIFKeyGetASCII( GTIF *hGTIF, geokey_t key,
                                char* szStr,
                                int nIndex,
                                int szStrMaxLen );

int GDALGTIFKeyGetSHORT( GTIF *hGTIF, geokey_t key,
                                short* pnVal,
                                int nIndex,
                                int nCount );

int GDALGTIFKeyGetDOUBLE( GTIF *hGTIF, geokey_t key,
                                 double* pdfVal,
                                 int nIndex,
                                 int nCount );

#endif // GT_WKT_SRS_PRIV_H_INCLUDED

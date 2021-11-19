/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Code to build overviews of external databases as a TIFF file.
 *           Only used by the GDALDefaultOverviews::BuildOverviews() method.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GT_OVERVIEW_H_INCLUDED
#define GT_OVERVIEW_H_INCLUDED

#include <cstdint>

#include "gdal_priv.h"
#include "tiffio.h"

#include <utility>

toff_t GTIFFWriteDirectory( TIFF *hTIFF, int nSubfileType,
                            int nXSize, int nYSize,
                            int nBitsPerPixel, int nPlanarConfig, int nSamples,
                            int nBlockXSize, int nBlockYSize,
                            int bTiled, int nCompressFlag, int nPhotometric,
                            int nSampleFormat,
                            int nPredictor,
                            unsigned short *panRed,
                            unsigned short *panGreen,
                            unsigned short *panBlue,
                            int nExtraSamples,
                            unsigned short *panExtraSampleValues,
                            const char *pszMetadata,
                            const char* pszJPEGQuality,
                            const char* pszJPEGTablesMode,
                            const char* pszNoData,
                            const uint32_t* panLercAddCompressionAndVersion,
                            bool bDeferStrileArrayWriting );

void GTIFFBuildOverviewMetadata( const char *pszResampling,
                                 GDALDataset *poBaseDS,
                                 CPLString &osMetadata );

CPLErr
GTIFFBuildOverviewsEx( const char * pszFilename,
                       int nBands, GDALRasterBand **papoBandList,
                       int nOverviews,
                       const int * panOverviewList,
                       const std::pair<int, int>* pasOverviewSize,
                       const char * pszResampling,
                       const char* const* papszOptions,
                       GDALProgressFunc pfnProgress, void * pProgressData );

#endif

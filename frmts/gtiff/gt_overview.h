/******************************************************************************
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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GT_OVERVIEW_H_INCLUDED
#define GT_OVERVIEW_H_INCLUDED

#include <cstdint>

#include "gdal_priv.h"
#include "tiffio.h"

#include <utility>

toff_t GTIFFWriteDirectory(TIFF *hTIFF, int nSubfileType, int nXSize,
                           int nYSize, int nBitsPerPixel, int nPlanarConfig,
                           int nSamples, int nBlockXSize, int nBlockYSize,
                           int bTiled, int nCompressFlag, int nPhotometric,
                           int nSampleFormat, int nPredictor,
                           unsigned short *panRed, unsigned short *panGreen,
                           unsigned short *panBlue, int nExtraSamples,
                           unsigned short *panExtraSampleValues,
                           const char *pszMetadata, const char *pszJPEGQuality,
                           const char *pszJPEGTablesMode, const char *pszNoData,
                           const uint32_t *panLercAddCompressionAndVersion,
                           bool bDeferStrileArrayWriting);

void GTIFFBuildOverviewMetadata(const char *pszResampling,
                                GDALDataset *poBaseDS, bool bIsForMaskBand,
                                CPLString &osMetadata);

CPLErr GTIFFBuildOverviewsEx(const char *pszFilename, int nBands,
                             GDALRasterBand *const *papoBandList,
                             int nOverviews, const int *panOverviewList,
                             const std::pair<int, int> *pasOverviewSize,
                             const char *pszResampling,
                             const char *const *papszOptions,
                             GDALProgressFunc pfnProgress, void *pProgressData);

#endif

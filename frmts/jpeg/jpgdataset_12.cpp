/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#if defined(JPEG_DUAL_MODE_8_12)

#if !defined(HAVE_JPEGTURBO_DUAL_MODE_8_12)
#define LIBJPEG_12_PATH "libjpeg12/jpeglib.h"
#endif
#define JPGDataset JPGDataset12
#define GDALJPEGErrorStruct GDALJPEGErrorStruct12
#define jpeg_vsiio_src jpeg_vsiio_src_12
#define jpeg_vsiio_dest jpeg_vsiio_dest_12
#define GDALJPEGUserData GDALJPEGUserData12

#include "jpgdataset.cpp"

JPGDatasetCommon *JPEGDataset12Open(JPGDatasetOpenArgs *psArgs);
GDALDataset *JPEGDataset12CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int bStrict,
                                     char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData);

JPGDatasetCommon *JPEGDataset12Open(JPGDatasetOpenArgs *psArgs)
{
    return JPGDataset12::Open(psArgs);
}

GDALDataset *JPEGDataset12CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int bStrict,
                                     char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    return JPGDataset12::CreateCopy(pszFilename, poSrcDS, bStrict, papszOptions,
                                    pfnProgress, pProgressData);
}

#endif /* defined(JPEG_DUAL_MODE_8_12) */

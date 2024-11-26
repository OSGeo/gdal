/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Specialized copy of JPEG content into TIFF.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GT_JPEG_COPY_H_INCLUDED
#define GT_JPEG_COPY_H_INCLUDED

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal_priv.h"

#ifdef JPEG_DIRECT_COPY

int GTIFF_CanDirectCopyFromJPEG(GDALDataset *poSrcDS,
                                char **&papszCreateOptions);

CPLErr GTIFF_DirectCopyFromJPEG(GDALDataset *poDS, GDALDataset *poSrcDS,
                                GDALProgressFunc pfnProgress,
                                void *pProgressData,
                                bool &bShouldFallbackToNormalCopyIfFail);

#endif  // JPEG_DIRECT_COPY

#ifdef HAVE_LIBJPEG

#include "tiffio.h"

int GTIFF_CanCopyFromJPEG(GDALDataset *poSrcDS, char **&papszCreateOptions);

CPLErr GTIFF_CopyFromJPEG_WriteAdditionalTags(TIFF *hTIFF,
                                              GDALDataset *poSrcDS);

CPLErr GTIFF_CopyFromJPEG(GDALDataset *poDS, GDALDataset *poSrcDS,
                          GDALProgressFunc pfnProgress, void *pProgressData,
                          bool &bShouldFallbackToNormalCopyIfFail);

#endif  // HAVE_LIBJPEG

#endif  // GT_JPEG_COPY_H_INCLUDED

/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Specialized copy of JPEG content into TIFF.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GT_JPEG_COPY_H_INCLUDED
#define GT_JPEG_COPY_H_INCLUDED

#ifdef JPEG_DIRECT_COPY

#include "gdal_priv.h"
#include "cpl_vsi.h"

int GTIFF_CanDirectCopyFromJPEG(GDALDataset* poSrcDS, char** &papszCreateOptions);

CPLErr GTIFF_DirectCopyFromJPEG(GDALDataset* poDS, GDALDataset* poSrcDS,
                                GDALProgressFunc pfnProgress, void * pProgressData,
                                int& bShouldFallbackToNormalCopyIfFail);

#endif // JPEG_DIRECT_COPY

#ifdef HAVE_LIBJPEG

#include "gdal_priv.h"
#include "cpl_error.h"
#include "tiffio.h"

int GTIFF_CanCopyFromJPEG(GDALDataset* poSrcDS, char** &papszCreateOptions);

CPLErr GTIFF_CopyFromJPEG_WriteAdditionalTags(TIFF* hTIFF,
                                              GDALDataset* poSrcDS);

CPLErr GTIFF_CopyFromJPEG(GDALDataset* poDS, GDALDataset* poSrcDS,
                          GDALProgressFunc pfnProgress, void * pProgressData,
                          int& bShouldFallbackToNormalCopyIfFail);

#endif // HAVE_LIBJPEG

#endif // GT_JPEG_COPY_H_INCLUDED

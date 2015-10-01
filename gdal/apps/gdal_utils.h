/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Utilities Public Declarations.
 * Author:   Faza Mahamood, fazamhd at gmail dot com
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2015, Faza Mahamood
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

#ifndef _GDAL_UTILS_H_INCLUDED
#define _GDAL_UTILS_H_INCLUDED

/**
 * \file gdal_utils.h
 *
 * Public (C callable) GDAL Utilities entry points.
 *
 * @since GDAL 2.1
 */

#include "cpl_port.h"

CPL_C_START

/*! Options for GDALInfo(). Opaque type */
typedef struct GDALInfoOptions GDALInfoOptions;

typedef struct GDALInfoOptionsForBinary GDALInfoOptionsForBinary;

GDALInfoOptions CPL_DLL *GDALInfoOptionsNew(char** papszArgv, GDALInfoOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALInfoOptionsFree( GDALInfoOptions *psOptions );

char CPL_DLL *GDALInfo( GDALDatasetH hDataset, const GDALInfoOptions *psOptions );


/*! Options for GDALTranslate(). Opaque type */
typedef struct GDALTranslateOptions GDALTranslateOptions;

typedef struct GDALTranslateOptionsForBinary GDALTranslateOptionsForBinary;

GDALTranslateOptions CPL_DLL *GDALTranslateOptionsNew(char** papszArgv,
                                                      GDALTranslateOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALTranslateOptionsFree( GDALTranslateOptions *psOptions );

void CPL_DLL GDALTranslateOptionsSetProgress( GDALTranslateOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALTranslate(const char *pszDestFilename,
                                   GDALDatasetH hSrcDataset,
                                   const GDALTranslateOptions *psOptions,
                                   int *pbUsageError);

CPL_C_END

#endif /* _GDAL_UTILS_H_INCLUDED */

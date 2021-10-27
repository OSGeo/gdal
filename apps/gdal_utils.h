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

#ifndef GDAL_UTILS_H_INCLUDED
#define GDAL_UTILS_H_INCLUDED

/**
 * \file gdal_utils.h
 *
 * Public (C callable) GDAL Utilities entry points.
 *
 * @since GDAL 2.1
 */

#include "cpl_port.h"
#include "gdal.h"

CPL_C_START

/*! Options for GDALInfo(). Opaque type */
typedef struct GDALInfoOptions GDALInfoOptions;

/** Opaque type */
typedef struct GDALInfoOptionsForBinary GDALInfoOptionsForBinary;

GDALInfoOptions CPL_DLL *GDALInfoOptionsNew(char** papszArgv, GDALInfoOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALInfoOptionsFree( GDALInfoOptions *psOptions );

char CPL_DLL *GDALInfo( GDALDatasetH hDataset, const GDALInfoOptions *psOptions );

/*! Options for GDALTranslate(). Opaque type */
typedef struct GDALTranslateOptions GDALTranslateOptions;

/** Opaque type */
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

/*! Options for GDALWarp(). Opaque type */
typedef struct GDALWarpAppOptions GDALWarpAppOptions;

/** Opaque type */
typedef struct GDALWarpAppOptionsForBinary GDALWarpAppOptionsForBinary;

GDALWarpAppOptions CPL_DLL *GDALWarpAppOptionsNew(char** papszArgv,
                                                      GDALWarpAppOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALWarpAppOptionsFree( GDALWarpAppOptions *psOptions );

void CPL_DLL GDALWarpAppOptionsSetProgress( GDALWarpAppOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );
void CPL_DLL GDALWarpAppOptionsSetQuiet( GDALWarpAppOptions *psOptions,
                                         int bQuiet );
void CPL_DLL GDALWarpAppOptionsSetWarpOption( GDALWarpAppOptions *psOptions,
                                              const char* pszKey,
                                              const char* pszValue );

GDALDatasetH CPL_DLL GDALWarp( const char *pszDest, GDALDatasetH hDstDS,
                               int nSrcCount, GDALDatasetH *pahSrcDS,
                               const GDALWarpAppOptions *psOptions, int *pbUsageError );

/*! Options for GDALVectorTranslate(). Opaque type */
typedef struct GDALVectorTranslateOptions GDALVectorTranslateOptions;

/** Opaque type */
typedef struct GDALVectorTranslateOptionsForBinary GDALVectorTranslateOptionsForBinary;

GDALVectorTranslateOptions CPL_DLL *GDALVectorTranslateOptionsNew(char** papszArgv,
                                                      GDALVectorTranslateOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALVectorTranslateOptionsFree( GDALVectorTranslateOptions *psOptions );

void CPL_DLL GDALVectorTranslateOptionsSetProgress( GDALVectorTranslateOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALVectorTranslate( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                               GDALDatasetH *pahSrcDS,
                               const GDALVectorTranslateOptions *psOptions, int *pbUsageError );

/*! Options for GDALDEMProcessing(). Opaque type */
typedef struct GDALDEMProcessingOptions GDALDEMProcessingOptions;

/** Opaque type */
typedef struct GDALDEMProcessingOptionsForBinary GDALDEMProcessingOptionsForBinary;

GDALDEMProcessingOptions CPL_DLL *GDALDEMProcessingOptionsNew(char** papszArgv,
                                                      GDALDEMProcessingOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALDEMProcessingOptionsFree( GDALDEMProcessingOptions *psOptions );

void CPL_DLL GDALDEMProcessingOptionsSetProgress( GDALDEMProcessingOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALDEMProcessing(const char *pszDestFilename,
                                       GDALDatasetH hSrcDataset,
                                       const char* pszProcessing,
                                       const char* pszColorFilename,
                                       const GDALDEMProcessingOptions *psOptions,
                                       int *pbUsageError);

/*! Options for GDALNearblack(). Opaque type */
typedef struct GDALNearblackOptions GDALNearblackOptions;

/** Opaque type */
typedef struct GDALNearblackOptionsForBinary GDALNearblackOptionsForBinary;

GDALNearblackOptions CPL_DLL *GDALNearblackOptionsNew(char** papszArgv,
                                                      GDALNearblackOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALNearblackOptionsFree( GDALNearblackOptions *psOptions );

void CPL_DLL GDALNearblackOptionsSetProgress( GDALNearblackOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALNearblack( const char *pszDest, GDALDatasetH hDstDS,
                                    GDALDatasetH hSrcDS,
                                    const GDALNearblackOptions *psOptions, int *pbUsageError );

/*! Options for GDALGrid(). Opaque type */
typedef struct GDALGridOptions GDALGridOptions;

/** Opaque type */
typedef struct GDALGridOptionsForBinary GDALGridOptionsForBinary;

GDALGridOptions CPL_DLL *GDALGridOptionsNew(char** papszArgv,
                                                      GDALGridOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALGridOptionsFree( GDALGridOptions *psOptions );

void CPL_DLL GDALGridOptionsSetProgress( GDALGridOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALGrid( const char *pszDest,
                               GDALDatasetH hSrcDS,
                               const GDALGridOptions *psOptions, int *pbUsageError );

/*! Options for GDALRasterize(). Opaque type */
typedef struct GDALRasterizeOptions GDALRasterizeOptions;

/** Opaque type */
typedef struct GDALRasterizeOptionsForBinary GDALRasterizeOptionsForBinary;

GDALRasterizeOptions CPL_DLL *GDALRasterizeOptionsNew(char** papszArgv,
                                                      GDALRasterizeOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALRasterizeOptionsFree( GDALRasterizeOptions *psOptions );

void CPL_DLL GDALRasterizeOptionsSetProgress( GDALRasterizeOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALRasterize( const char *pszDest, GDALDatasetH hDstDS,
                                    GDALDatasetH hSrcDS,
                                    const GDALRasterizeOptions *psOptions, int *pbUsageError );

/*! Options for GDALBuildVRT(). Opaque type */
typedef struct GDALBuildVRTOptions GDALBuildVRTOptions;

/** Opaque type */
typedef struct GDALBuildVRTOptionsForBinary GDALBuildVRTOptionsForBinary;

GDALBuildVRTOptions CPL_DLL *GDALBuildVRTOptionsNew(char** papszArgv,
                                                      GDALBuildVRTOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALBuildVRTOptionsFree( GDALBuildVRTOptions *psOptions );

void CPL_DLL GDALBuildVRTOptionsSetProgress( GDALBuildVRTOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALBuildVRT( const char *pszDest,
                                   int nSrcCount, GDALDatasetH *pahSrcDS, const char* const* papszSrcDSNames,
                                   const GDALBuildVRTOptions *psOptions, int *pbUsageError );


/*! Options for GDALMultiDimInfo(). Opaque type */
typedef struct GDALMultiDimInfoOptions GDALMultiDimInfoOptions;

/** Opaque type */
typedef struct GDALMultiDimInfoOptionsForBinary GDALMultiDimInfoOptionsForBinary;

GDALMultiDimInfoOptions CPL_DLL *GDALMultiDimInfoOptionsNew(char** papszArgv, GDALMultiDimInfoOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALMultiDimInfoOptionsFree( GDALMultiDimInfoOptions *psOptions );

char CPL_DLL *GDALMultiDimInfo( GDALDatasetH hDataset, const GDALMultiDimInfoOptions *psOptions );


/*! Options for GDALMultiDimTranslate(). Opaque type */
typedef struct GDALMultiDimTranslateOptions GDALMultiDimTranslateOptions;

/** Opaque type */
typedef struct GDALMultiDimTranslateOptionsForBinary GDALMultiDimTranslateOptionsForBinary;

GDALMultiDimTranslateOptions CPL_DLL *GDALMultiDimTranslateOptionsNew(char** papszArgv, GDALMultiDimTranslateOptionsForBinary* psOptionsForBinary);

void CPL_DLL GDALMultiDimTranslateOptionsFree( GDALMultiDimTranslateOptions *psOptions );

void CPL_DLL GDALMultiDimTranslateOptionsSetProgress( GDALMultiDimTranslateOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData );

GDALDatasetH CPL_DLL GDALMultiDimTranslate( const char* pszDest,
                                            GDALDatasetH hDstDataset,
                                            int nSrcCount, GDALDatasetH *pahSrcDS,
                                            const GDALMultiDimTranslateOptions *psOptions,
                                            int *pbUsageError );

CPL_C_END

#endif /* GDAL_UTILS_H_INCLUDED */

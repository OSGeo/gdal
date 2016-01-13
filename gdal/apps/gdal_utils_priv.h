/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Utilities Private Declarations.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#ifndef GDAL_UTILS_PRIV_H_INCLUDED
#define GDAL_UTILS_PRIV_H_INCLUDED

#include "cpl_port.h"
#include "gdal_utils.h"

/* This file is only meant at being used by the XXXX_bin.cpp and XXXX_lib.cpp files */

CPL_C_START

struct GDALInfoOptionsForBinary
{
    /* Filename to open. */
    char* pszFilename;

    /* Open options. */
    char** papszOpenOptions;

    /* > for reporting on a particular subdataset */
    int nSubdataset;
};

struct GDALTranslateOptionsForBinary
{
    char* pszSource;
    char* pszDest;
    int bQuiet;
    int bCopySubDatasets;
    char** papszOpenOptions;
    int bFormatExplicitlySet;
    char* pszFormat;
};

struct GDALWarpAppOptionsForBinary
{
    char** papszSrcFiles;
    char* pszDstFilename;
    int bQuiet;
    char** papszOpenOptions;

    /*! output dataset open option (format specific) */
    char **papszDestOpenOptions;

    int bOverwrite;
    int bCreateOutput;
    int bFormatExplicitlySet;
    char* pszFormat;
};

/* Access modes */
typedef enum
{
    ACCESS_CREATION,
    ACCESS_UPDATE, /* open existing output datasource in update mode rather than trying to create a new one */
    ACCESS_APPEND, /* append to existing layer instead of creating new */
    ACCESS_OVERWRITE /*  delete the output layer and recreate it empty */
} GDALVectorTranslateAccessMode;

struct GDALVectorTranslateOptionsForBinary
{
    char* pszDataSource;
    char* pszDestDataSource;
    int bQuiet;
    char** papszOpenOptions;
    int bFormatExplicitlySet;
    char* pszFormat;
    GDALVectorTranslateAccessMode eAccessMode;
};

struct GDALDEMProcessingOptionsForBinary
{
    char* pszProcessing;
    char* pszSrcFilename;
    char* pszColorFilename;
    char* pszDstFilename;
    int bQuiet;
    int bFormatExplicitlySet;
    char* pszFormat;
};

struct GDALNearblackOptionsForBinary
{
    char* pszInFile;
    char* pszOutFile;
    int bQuiet;
    int bFormatExplicitlySet;
    char* pszFormat;
};

struct GDALGridOptionsForBinary
{
    char* pszSource;
    char* pszDest;
    int bQuiet;
    int bFormatExplicitlySet;
    char* pszFormat;
};

struct GDALRasterizeOptionsForBinary
{
    char* pszSource;
    char* pszDest;
    int bQuiet;
    int bFormatExplicitlySet;
    char* pszFormat;
    int bCreateOutput;
};

struct GDALBuildVRTOptionsForBinary
{
    int nSrcFiles;
    char** papszSrcFiles;
    char* pszDstFilename;
    int bQuiet;
    int bOverwrite;
};

CPL_C_END

#endif /* GDAL_UTILS_PRIV_H_INCLUDED */

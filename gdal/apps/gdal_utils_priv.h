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

#ifndef DOXYGEN_SKIP

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

    /* Allowed input drivers. */
    char** papszAllowInputDrivers;
};

struct GDALTranslateOptionsForBinary
{
    char* pszSource;
    char* pszDest;
    int bQuiet;
    int bCopySubDatasets;
    char** papszOpenOptions;
    char* pszFormat;

    /* Allowed input drivers. */
    char** papszAllowInputDrivers;
};

struct GDALWarpAppOptionsForBinary
{
    char** papszSrcFiles;
    char* pszDstFilename;
    int bQuiet;
    char** papszOpenOptions;

    /*! output dataset open option (format specific) */
    char **papszDestOpenOptions;

    char **papszCreateOptions;

    int bOverwrite;
    int bCreateOutput;

    /* Allowed input drivers. */
    char** papszAllowInputDrivers;
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
};

struct GDALNearblackOptionsForBinary
{
    char* pszInFile;
    char* pszOutFile;
    int bQuiet;
};

struct GDALGridOptionsForBinary
{
    char* pszSource;
    char* pszDest;
    int bQuiet;
    char* pszFormat;
};

struct GDALRasterizeOptionsForBinary
{
    char* pszSource;
    char* pszDest;
    int bQuiet;
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

struct GDALMultiDimInfoOptionsForBinary
{
    /* Filename to open. */
    char* pszFilename;

    /* Open options. */
    char** papszOpenOptions;
};

struct GDALMultiDimTranslateOptionsForBinary
{
    char* pszSource;
    char* pszDest;
    char* pszFormat;
    int   bQuiet;
    int   bUpdate;

    /* Open options. */
    char** papszOpenOptions;
};

CPL_C_END

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* GDAL_UTILS_PRIV_H_INCLUDED */

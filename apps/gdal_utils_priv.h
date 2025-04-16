/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Utilities Private Declarations.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_UTILS_PRIV_H_INCLUDED
#define GDAL_UTILS_PRIV_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal_utils.h"

/* This file is only meant at being used by the XXXX_bin.cpp and XXXX_lib.cpp */

CPL_C_START

struct GDALInfoOptionsForBinary
{
    /* Filename to open. */
    std::string osFilename{};

    /* Open options. */
    CPLStringList aosOpenOptions{};

    /* For reporting on a particular subdataset */
    int nSubdataset = 0;

    /* Allowed input drivers. */
    CPLStringList aosAllowedInputDrivers{};
};

struct GDALDEMProcessingOptionsForBinary
{
    std::string osProcessing{};
    std::string osSrcFilename{};
    std::string osColorFilename{};
    std::string osDstFilename{};
    bool bQuiet = false;
};

CPL_C_END

/* Access modes */
typedef enum
{
    ACCESS_CREATION,
    ACCESS_UPDATE, /* open existing output datasource in update mode rather than
                      trying to create a new one */
    ACCESS_APPEND, /* append to existing layer instead of creating new */
    ACCESS_OVERWRITE /*  delete the output layer and recreate it empty */
} GDALVectorTranslateAccessMode;

struct GDALVectorTranslateOptionsForBinary
{
    std::string osDataSource{};
    std::string osDestDataSource{};
    bool bQuiet = false;
    CPLStringList aosOpenOptions{};
    std::string osFormat{};
    GDALVectorTranslateAccessMode eAccessMode = ACCESS_CREATION;
    bool bShowUsageIfError = false;

    /* Allowed input drivers. */
    CPLStringList aosAllowInputDrivers{};
};

struct GDALContourOptionsForBinary
{
    CPLStringList aosOpenOptions{};
    CPLStringList aosCreationOptions{};
    bool bQuiet = false;
    std::string osDestDataSource{};
    std::string osSrcDataSource{};
};

struct GDALMultiDimInfoOptionsForBinary
{
    /* Filename to open. */
    std::string osFilename{};

    /* Allowed input drivers. */
    CPLStringList aosAllowInputDrivers{};

    /* Open options. */
    CPLStringList aosOpenOptions{};
};

struct GDALMultiDimTranslateOptionsForBinary
{
    std::string osSource{};
    std::string osDest{};
    std::string osFormat{};
    bool bQuiet = false;
    bool bUpdate = false;

    /* Allowed input drivers. */
    CPLStringList aosAllowInputDrivers{};

    /* Open options. */
    CPLStringList aosOpenOptions{};
};

struct GDALVectorInfoOptionsForBinary
{
    /* Filename to open. */
    std::string osFilename{};

    bool bVerbose = true;

    bool bReadOnly = false;

    bool bUpdate = false;

    std::string osSQLStatement{};

    /* Open options. */
    CPLStringList aosOpenOptions{};

    /* Allowed input drivers. */
    CPLStringList aosAllowInputDrivers{};
};

struct GDALGridOptionsForBinary
{
    std::string osSource{};
    std::string osDest{};
    bool bQuiet = false;
    CPLStringList aosOpenOptions{};
};

struct GDALRasterizeOptionsForBinary
{
    std::string osSource{};
    bool bDestSpecified = false;
    std::string osDest{};
    bool bQuiet = false;
    CPLStringList aosOpenOptions{};
    bool bCreateOutput = false;
    std::string osFormat{};
};

struct GDALFootprintOptionsForBinary
{
    std::string osSource{};
    bool bDestSpecified = false;
    std::string osDest{};
    bool bQuiet = false;
    CPLStringList aosOpenOptions{};
    bool bCreateOutput = false;
    std::string osFormat{};

    /*! whether to overwrite destination layer */
    bool bOverwrite = false;

    std::string osDestLayerName{};
};

struct GDALTileIndexOptionsForBinary
{
    CPLStringList aosSrcFiles{};
    bool bDestSpecified = false;
    std::string osDest{};
    bool bQuiet = false;
};

struct GDALNearblackOptionsForBinary
{
    std::string osInFile{};
    std::string osOutFile{};
    bool bQuiet = false;
};

struct GDALTranslateOptionsForBinary
{
    std::string osSource{};
    std::string osDest{};
    bool bQuiet = false;
    bool bCopySubDatasets = false;
    CPLStringList aosOpenOptions{};
    CPLStringList aosCreateOptions{};
    std::string osFormat{};

    /* Allowed input drivers. */
    CPLStringList aosAllowedInputDrivers{};
};

struct GDALWarpAppOptionsForBinary
{
    CPLStringList aosSrcFiles{};
    std::string osDstFilename{};
    bool bQuiet = false;
    CPLStringList aosOpenOptions{};

    /*! output dataset open option (format specific) */
    CPLStringList aosDestOpenOptions{};

    CPLStringList aosCreateOptions{};

    bool bOverwrite = false;
    bool bCreateOutput = false;

    /* Allowed input drivers. */
    CPLStringList aosAllowedInputDrivers{};
};

struct GDALBuildVRTOptionsForBinary
{
    CPLStringList aosSrcFiles{};
    std::string osDstFilename{};
    bool bQuiet = false;
    bool bOverwrite = false;
};

std::string CPL_DLL GDALNearblackGetParserUsage();

std::string CPL_DLL GDALVectorInfoGetParserUsage();

std::string CPL_DLL GDALTranslateGetParserUsage();

std::string CPL_DLL GDALMultiDimTranslateAppGetParserUsage();

std::string CPL_DLL GDALVectorTranslateGetParserUsage();

std::string CPL_DLL GDALWarpAppGetParserUsage();

std::string CPL_DLL GDALInfoAppGetParserUsage();

std::string CPL_DLL GDALMultiDimInfoAppGetParserUsage();

std::string CPL_DLL GDALGridGetParserUsage();

std::string CPL_DLL GDALContourGetParserUsage();

std::string CPL_DLL GDALBuildVRTGetParserUsage();

std::string CPL_DLL GDALTileIndexAppGetParserUsage();

std::string CPL_DLL GDALFootprintAppGetParserUsage();

std::string CPL_DLL GDALRasterizeAppGetParserUsage();

/**
 * Returns the gdaldem usage help string
 * @param osProcessingMode          Processing mode (subparser name)
 * @return                          gdaldem usage help string
 */
std::string CPL_DLL
GDALDEMAppGetParserUsage(const std::string &osProcessingMode);

GDALDatasetH GDALTileIndexInternal(const char *pszDest,
                                   GDALDatasetH hTileIndexDS, OGRLayerH hLayer,
                                   int nSrcCount,
                                   const char *const *papszSrcDSNames,
                                   const GDALTileIndexOptions *psOptionsIn,
                                   int *pbUsageError);

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* GDAL_UTILS_PRIV_H_INCLUDED */

/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL scattered data gridding (interpolation) tool
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_version.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    fprintf(stderr, "%s\n", GDALGridGetParserUsage().c_str());
    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)
{
    /* Check strict compilation and runtime library version as we use C++ API */
    if (!GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

    /* -------------------------------------------------------------------- */
    /*      Generic arg processing.                                         */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    GDALGridOptionsForBinary sOptionsForBinary;
    /* coverity[tainted_data] */
    GDALGridOptions *psOptions =
        GDALGridOptionsNew(argv + 1, &sOptionsForBinary);
    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage();
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALGridOptionsSetProgress(psOptions, GDALTermProgress, nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      Open input file.                                                */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hInDS = GDALOpenEx(sOptionsForBinary.osSource.c_str(),
                                    GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                                    /*papszAllowedDrivers=*/nullptr,
                                    sOptionsForBinary.aosOpenOptions.List(),
                                    /*papszSiblingFiles=*/nullptr);
    if (hInDS == nullptr)
        exit(1);

    int bUsageError = FALSE;
    GDALDatasetH hOutDS = GDALGrid(sOptionsForBinary.osDest.c_str(), hInDS,
                                   psOptions, &bUsageError);
    if (bUsageError == TRUE)
        Usage();
    int nRetCode = hOutDS ? 0 : 1;

    GDALClose(hInDS);
    GDALClose(hOutDS);
    GDALGridOptionsFree(psOptions);

    OGRCleanupAll();
    GDALDestroyDriverManager();

    return nRetCode;
}

MAIN_END

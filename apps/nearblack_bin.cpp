/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Convert nearly black or nearly white border to exact black/white.
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

static void Usage(const char *pszErrorMsg = nullptr)
{
    fprintf(stderr, "%s\n\n", GDALNearblackGetParserUsage().c_str());

    if (pszErrorMsg != nullptr)
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

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

    if (CPLGetConfigOption("GDAL_CACHEMAX", nullptr) == nullptr)
        GDALSetCacheMax(100000000);
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    GDALNearblackOptionsForBinary sOptionsForBinary;
    GDALNearblackOptions *psOptions =
        GDALNearblackOptionsNew(argv + 1, &sOptionsForBinary);
    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage();
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALNearblackOptionsSetProgress(psOptions, GDALTermProgress, nullptr);
    }

    if (sOptionsForBinary.osOutFile.empty())
        sOptionsForBinary.osOutFile = sOptionsForBinary.osInFile;

    /* -------------------------------------------------------------------- */
    /*      Open input file.                                                */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hInDS = nullptr;
    GDALDatasetH hOutDS = nullptr;
    bool bCloseRetDS = false;

    if (sOptionsForBinary.osOutFile == sOptionsForBinary.osInFile)
    {
        hInDS = GDALOpen(sOptionsForBinary.osInFile.c_str(), GA_Update);
        hOutDS = hInDS;
    }
    else
    {
        hInDS = GDALOpen(sOptionsForBinary.osInFile.c_str(), GA_ReadOnly);
        bCloseRetDS = true;
    }

    if (hInDS == nullptr)
        exit(1);

    int bUsageError = FALSE;
    GDALDatasetH hRetDS = GDALNearblack(sOptionsForBinary.osOutFile.c_str(),
                                        hOutDS, hInDS, psOptions, &bUsageError);
    if (bUsageError)
        Usage();
    int nRetCode = hRetDS ? 0 : 1;

    if (GDALClose(hInDS) != CE_None)
        nRetCode = 1;
    if (bCloseRetDS)
    {
        if (GDALClose(hRetDS) != CE_None)
            nRetCode = 1;
    }
    GDALNearblackOptionsFree(psOptions);

    GDALDestroyDriverManager();

    return nRetCode;
}

MAIN_END

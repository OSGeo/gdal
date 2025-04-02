/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Rasterize OGR shapes into a GDAL raster.
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
#include "gdal_priv.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    fprintf(stderr, "%s\n", GDALRasterizeAppGetParserUsage().c_str());
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
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    /* -------------------------------------------------------------------- */
    /*      Generic arg processing.                                         */
    /* -------------------------------------------------------------------- */

    GDALRasterizeOptionsForBinary sOptionsForBinary;
    std::unique_ptr<GDALRasterizeOptions, decltype(&GDALRasterizeOptionsFree)>
        psOptions{GDALRasterizeOptionsNew(argv + 1, &sOptionsForBinary),
                  GDALRasterizeOptionsFree};

    CSLDestroy(argv);

    if (!psOptions)
    {
        Usage();
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALRasterizeOptionsSetProgress(psOptions.get(), GDALTermProgress,
                                        nullptr);
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

    /* -------------------------------------------------------------------- */
    /*      Open output file if it exists.                                  */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = nullptr;
    if (!(sOptionsForBinary.bCreateOutput))
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hDstDS =
            GDALOpenEx(sOptionsForBinary.osDest.c_str(),
                       GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR | GDAL_OF_UPDATE,
                       nullptr, nullptr, nullptr);
        CPLPopErrorHandler();
    }

    if (!sOptionsForBinary.osFormat.empty() &&
        (sOptionsForBinary.bCreateOutput || hDstDS == nullptr))
    {
        GDALDriverManager *poDM = GetGDALDriverManager();
        GDALDriver *poDriver =
            poDM->GetDriverByName(sOptionsForBinary.osFormat.c_str());
        char **papszDriverMD = (poDriver) ? poDriver->GetMetadata() : nullptr;
        if (poDriver == nullptr ||
            !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_RASTER,
                                              "FALSE")) ||
            !CPLTestBool(
                CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
        {
            fprintf(stderr,
                    "Output driver `%s' not recognised or does not support "
                    "direct output file creation.\n",
                    sOptionsForBinary.osFormat.c_str());
            fprintf(stderr, "The following format drivers are enabled and "
                            "support direct writing:\n");

            for (int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++)
            {
                GDALDriver *poIter = poDM->GetDriver(iDriver);
                papszDriverMD = poIter->GetMetadata();
                if (CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_RASTER, "FALSE")) &&
                    CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
                {
                    fprintf(stderr, "  -> `%s'\n", poIter->GetDescription());
                }
            }
            exit(1);
        }
    }

    int bUsageError = FALSE;
    GDALDatasetH hRetDS =
        GDALRasterize(sOptionsForBinary.osDest.c_str(), hDstDS, hInDS,
                      psOptions.get(), &bUsageError);

    if (bUsageError == TRUE)
        Usage();

    int nRetCode = hRetDS ? 0 : 1;

    GDALClose(hInDS);

    if (GDALClose(hRetDS) != CE_None)
        nRetCode = 1;

    GDALDestroyDriverManager();

    return nRetCode;
}

MAIN_END

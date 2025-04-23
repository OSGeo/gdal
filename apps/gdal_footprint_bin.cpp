/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Computes the footprint of a GDAL raster
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_version.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

/**
 * @brief Makes sure the GDAL library is properly cleaned up before exiting.
 * @param nCode exit code
 * @todo Move to API
 */
static void GDALExit(int nCode)
{
    GDALDestroy();
    exit(nCode);
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    fprintf(stderr, "%s\n", GDALFootprintAppGetParserUsage().c_str());
    GDALExit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)
{
    /* Check strict compilation and runtime library version as we use C++ API */
    if (!GDAL_CHECK_VERSION(argv[0]))
        GDALExit(1);

    EarlySetConfigOptions(argc, argv);

    /* -------------------------------------------------------------------- */
    /*      Generic arg processing.                                         */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        GDALExit(-argc);

    /* -------------------------------------------------------------------- */
    /*      Parse command line                                              */
    /* -------------------------------------------------------------------- */

    GDALFootprintOptionsForBinary sOptionsForBinary;
    // coverity[tainted_data]
    std::unique_ptr<GDALFootprintOptions, decltype(&GDALFootprintOptionsFree)>
        psOptions{GDALFootprintOptionsNew(argv + 1, &sOptionsForBinary),
                  GDALFootprintOptionsFree};

    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage();
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALFootprintOptionsSetProgress(psOptions.get(), GDALTermProgress,
                                        nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      Open input file.                                                */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hInDS = GDALOpenEx(sOptionsForBinary.osSource.c_str(),
                                    GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                                    /*papszAllowedDrivers=*/nullptr,
                                    sOptionsForBinary.aosOpenOptions.List(),
                                    /*papszSiblingFiles=*/nullptr);

    if (hInDS == nullptr)
        GDALExit(1);

    /* -------------------------------------------------------------------- */
    /*      Open output file if it exists.                                  */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = nullptr;
    if (!(sOptionsForBinary.bCreateOutput))
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hDstDS =
            GDALOpenEx(sOptionsForBinary.osDest.c_str(),
                       GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR | GDAL_OF_UPDATE,
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
            !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_VECTOR,
                                              "FALSE")) ||
            !CPLTestBool(
                CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
        {
            fprintf(stderr,
                    "Output driver `%s' not recognised or does not support "
                    "direct output file creation.\n",
                    sOptionsForBinary.osFormat.c_str());
            fprintf(stderr, "The following format drivers are enable and "
                            "support direct writing:\n");

            for (int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++)
            {
                GDALDriver *poIter = poDM->GetDriver(iDriver);
                papszDriverMD = poIter->GetMetadata();
                if (CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_VECTOR, "FALSE")) &&
                    CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
                {
                    fprintf(stderr, "  -> `%s'\n", poIter->GetDescription());
                }
            }
            GDALExit(1);
        }
    }

    if (hDstDS && sOptionsForBinary.bOverwrite)
    {
        auto poDstDS = GDALDataset::FromHandle(hDstDS);
        int nLayerCount = poDstDS->GetLayerCount();
        int iLayerToDelete = -1;
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poLayer = poDstDS->GetLayer(i);
            if (poLayer &&
                poLayer->GetName() == sOptionsForBinary.osDestLayerName)
            {
                iLayerToDelete = i;
                break;
            }
        }
        bool bDeleteOK = false;
        if (iLayerToDelete >= 0 && poDstDS->TestCapability(ODsCDeleteLayer))
        {
            bDeleteOK = poDstDS->DeleteLayer(iLayerToDelete) == OGRERR_NONE;
        }
        if (!bDeleteOK && nLayerCount == 1)
        {
            GDALClose(hDstDS);
            hDstDS = nullptr;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            GDALDeleteDataset(nullptr, sOptionsForBinary.osDest.c_str());
            CPLPopErrorHandler();
            VSIUnlink(sOptionsForBinary.osDest.c_str());
        }
    }
    else if (sOptionsForBinary.bOverwrite)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDeleteDataset(nullptr, sOptionsForBinary.osDest.c_str());
        CPLPopErrorHandler();
        VSIUnlink(sOptionsForBinary.osDest.c_str());
    }

    int bUsageError = FALSE;
    GDALDatasetH hRetDS =
        GDALFootprint(sOptionsForBinary.osDest.c_str(), hDstDS, hInDS,
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

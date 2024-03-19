/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Computes the footprint of a GDAL raster
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_string.h"
#include "gdal_version.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(bool bIsError, const char *pszErrorMsg = nullptr)

{
    fprintf(bIsError ? stderr : stdout,
            "Usage: gdal_footprint [--help] [--help-general]\n"
            "       [-b <band>]... [-combine_bands union|intersection]\n"
            "       [-oo <NAME>=<VALUE>]... [-ovr <index>]\n"
            "       [-srcnodata \"<value>[ <value>]...\"]\n"
            "       [-t_cs pixel|georef] [-t_srs <srs_def>] [-split_polys]\n"
            "       [-convex_hull] [-densify <value>] [-simplify <value>]\n"
            "       [-min_ring_area <value>] [-max_points <value>|unlimited]\n"
            "       [-of <ogr_format>] [-lyr_name <dst_layername>]\n"
            "       [-location_field_name <field_name>] [-no_location]\n"
            "       [-write_absolute_path]\n"
            "       [-dsco <name>=<value>]... [-lco <name>=<value>]... "
            "[-overwrite] [-q]\n"
            "       <src_filename> <dst_filename>\n");

    if (pszErrorMsg != nullptr)
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);
    exit(bIsError ? 1 : 0);
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

    for (int i = 0; i < argc; i++)
    {
        if (EQUAL(argv[i], "--utility_version"))
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(argv);
            return 0;
        }
        else if (EQUAL(argv[i], "--help"))
        {
            Usage(false);
        }
    }

    GDALFootprintOptionsForBinary sOptionsForBinary;
    // coverity[tainted_data]
    GDALFootprintOptions *psOptions =
        GDALFootprintOptionsNew(argv + 1, &sOptionsForBinary);
    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage(true);
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALFootprintOptionsSetProgress(psOptions, GDALTermProgress, nullptr);
    }

    if (sOptionsForBinary.osSource.empty())
        Usage(true, "No input file specified.");

    if (!sOptionsForBinary.bDestSpecified)
        Usage(true, "No output file specified.");

    /* -------------------------------------------------------------------- */
    /*      Open input file.                                                */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hInDS = GDALOpenEx(sOptionsForBinary.osSource.c_str(),
                                    GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
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
            fprintf(stderr, "The following format drivers are configured and "
                            "support direct output:\n");

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
            exit(1);
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
    GDALDatasetH hRetDS = GDALFootprint(sOptionsForBinary.osDest.c_str(),
                                        hDstDS, hInDS, psOptions, &bUsageError);
    if (bUsageError == TRUE)
        Usage(true);
    int nRetCode = hRetDS ? 0 : 1;

    GDALClose(hInDS);
    if (GDALClose(hRetDS) != CE_None)
        nRetCode = 1;
    GDALFootprintOptionsFree(psOptions);

    GDALDestroyDriverManager();

    return nRetCode;
}

MAIN_END

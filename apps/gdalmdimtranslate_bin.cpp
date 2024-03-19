/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to convert a multidimensional raster
 * Author:   Even Rouault,<even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(bool bIsError, const char *pszErrorMsg = nullptr)

{
    fprintf(bIsError ? stderr : stdout,
            "Usage: gdalmdimtranslate [--help] [--help-general]\n"
            "                         [-if <format>]... [-of <format>]\n"
            "                         [-co <NAME>=<VALUE>]...\n"
            "                         [-array <array_spec>]...\n"
            "                         [-arrayoption <NAME>=<VALUE>]...\n"
            "                         [-group <group_spec>]...\n"
            "                         [-subset <subset_spec>]...\n"
            "                         [-scaleaxes <scaleaxes_spec>]\n"
            "                         [-oo <NAME>=<VALUE>]...\n"
            "                         <src_filename> <dst_filename>\n");

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

    GDALMultiDimTranslateOptionsForBinary sOptionsForBinary;
    // coverity[tainted_data]
    GDALMultiDimTranslateOptions *psOptions =
        GDALMultiDimTranslateOptionsNew(argv + 1, &sOptionsForBinary);
    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage(true);
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALMultiDimTranslateOptionsSetProgress(psOptions, GDALTermProgress,
                                                nullptr);
    }

    if (sOptionsForBinary.osSource.empty())
        Usage(true, "No input file specified.");

    if (sOptionsForBinary.osDest.empty())
        Usage(true, "No output file specified.");

    /* -------------------------------------------------------------------- */
    /*      Open input file.                                                */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hInDS = GDALOpenEx(
        sOptionsForBinary.osSource.c_str(),
        GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VERBOSE_ERROR,
        sOptionsForBinary.aosAllowInputDrivers.List(),
        sOptionsForBinary.aosOpenOptions.List(), nullptr);

    if (hInDS == nullptr)
        exit(1);

    /* -------------------------------------------------------------------- */
    /*      Open output file if in update mode.                             */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = nullptr;
    if (sOptionsForBinary.bUpdate)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hDstDS = GDALOpenEx(sOptionsForBinary.osDest.c_str(),
                            GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER |
                                GDAL_OF_VERBOSE_ERROR | GDAL_OF_UPDATE,
                            nullptr, nullptr, nullptr);
        CPLPopErrorHandler();
    }

    int bUsageError = FALSE;
    GDALDatasetH hRetDS =
        GDALMultiDimTranslate(sOptionsForBinary.osDest.c_str(), hDstDS, 1,
                              &hInDS, psOptions, &bUsageError);
    if (bUsageError == TRUE)
        Usage(true);
    int nRetCode = hRetDS ? 0 : 1;

    if (GDALClose(hRetDS) != CE_None)
        nRetCode = 1;

    GDALClose(hInDS);
    GDALMultiDimTranslateOptionsFree(psOptions);

    GDALDestroyDriverManager();

    return nRetCode;
}

MAIN_END

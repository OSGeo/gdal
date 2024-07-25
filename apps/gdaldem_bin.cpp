/******************************************************************************
 *
 * Project:  GDAL DEM Utilities
 * Purpose:
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_vsi.h"
#include <stdlib.h>
#include <math.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "commonutils.h"

/**
 * @brief Makes sure the GDAL library is properly cleaned up before exiting.
 * @param nCode exit code
 * @todo Move to API
 */
static void GDALExit(const int nCode)
{
    GDALDestroy();
    exit(nCode);
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const std::string &osProcessingMode = "")
{
    fprintf(stderr, "%s\n", GDALDEMAppGetParserUsage(osProcessingMode).c_str());
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

    GDALAllRegister();

    /* -------------------------------------------------------------------- */
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 2)
    {
        Usage();
    }

    GDALDEMProcessingOptionsForBinary sOptionsForBinary;

    const std::string osProcessingMode = argv[1];

    std::unique_ptr<GDALDEMProcessingOptions,
                    decltype(&GDALDEMProcessingOptionsFree)>
        psOptions{GDALDEMProcessingOptionsNew(argv + 1, &sOptionsForBinary),
                  GDALDEMProcessingOptionsFree};

    CSLDestroy(argv);

    if (!psOptions)
        Usage(osProcessingMode);

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALDEMProcessingOptionsSetProgress(psOptions.get(), GDALTermProgress,
                                            nullptr);
    }

    // Open Dataset and get raster band.
    GDALDatasetH hSrcDataset =
        GDALOpen(sOptionsForBinary.osSrcFilename.c_str(), GA_ReadOnly);

    if (hSrcDataset == nullptr)
    {
        fprintf(stderr, "GDALOpen failed - %d\n%s\n", CPLGetLastErrorNo(),
                CPLGetLastErrorMsg());
        GDALDestroyDriverManager();
        GDALExit(1);
    }

    int bUsageError = FALSE;
    GDALDatasetH hOutDS =
        GDALDEMProcessing(sOptionsForBinary.osDstFilename.c_str(), hSrcDataset,
                          sOptionsForBinary.osProcessing.c_str(),
                          sOptionsForBinary.osColorFilename.c_str(),
                          psOptions.get(), &bUsageError);

    if (bUsageError)
        Usage(osProcessingMode);

    const int nRetCode = hOutDS ? 0 : 1;

    GDALClose(hSrcDataset);
    GDALClose(hOutDS);

    GDALDestroy();

    return nRetCode;
}

MAIN_END

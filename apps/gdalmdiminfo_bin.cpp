/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to list info about a multidimensional
 *raster Author:   Even Rouault,<even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_version.h"
#include "gdal.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

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

static void Usage()

{
    fprintf(stderr, "%s\n", GDALMultiDimInfoAppGetParserUsage().c_str());
    GDALExit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    EarlySetConfigOptions(argc, argv);

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        GDALExit(-argc);

    argv = CSLAddString(argv, "-stdout");

    GDALMultiDimInfoOptionsForBinary sOptionsForBinary;

    std::unique_ptr<GDALMultiDimInfoOptions,
                    decltype(&GDALMultiDimInfoOptionsFree)>
        psOptions{GDALMultiDimInfoOptionsNew(argv + 1, &sOptionsForBinary),
                  GDALMultiDimInfoOptionsFree};

    CSLDestroy(argv);

    if (!psOptions)
        Usage();

    GDALDatasetH hDataset =
        GDALOpenEx(sOptionsForBinary.osFilename.c_str(),
                   GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VERBOSE_ERROR,
                   sOptionsForBinary.aosAllowInputDrivers.List(),
                   sOptionsForBinary.aosOpenOptions.List(), nullptr);
    if (!hDataset)
    {
        fprintf(stderr, "gdalmdiminfo failed - unable to open '%s'.\n",
                sOptionsForBinary.osFilename.c_str());
        GDALExit(1);
    }

    char *pszGDALInfoOutput = GDALMultiDimInfo(hDataset, psOptions.get());
    int nRet = pszGDALInfoOutput != nullptr ? 0 : 1;
    CPLFree(pszGDALInfoOutput);

    GDALClose(hDataset);

    GDALDestroy();

    return nRet;
}

MAIN_END

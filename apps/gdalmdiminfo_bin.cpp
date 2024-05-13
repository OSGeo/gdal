/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to list info about a multidimensional
 *raster Author:   Even Rouault,<even.rouault at spatialys.com>
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

    if (pszGDALInfoOutput)
        printf("%s", pszGDALInfoOutput);

    CPLFree(pszGDALInfoOutput);

    GDALClose(hDataset);

    GDALDestroy();

    return 0;
}

MAIN_END

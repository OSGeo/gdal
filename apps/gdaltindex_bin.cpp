/******************************************************************************
 *
 * Project:  MapServer
 * Purpose:  Commandline App to build tile index for raster files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam, DM Solutions Group Inc
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_error.h"
#include "commonutils.h"
#include "gdal_version.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    fprintf(stderr, "%s\n", GDALTileIndexAppGetParserUsage().c_str());
    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)
{

    EarlySetConfigOptions(argc, argv);

    /* -------------------------------------------------------------------- */
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    GDALTileIndexOptionsForBinary sOptionsForBinary;
    std::unique_ptr<GDALTileIndexOptions, decltype(&GDALTileIndexOptionsFree)>
        psOptions{GDALTileIndexOptionsNew(argv + 1, &sOptionsForBinary),
                  GDALTileIndexOptionsFree};
    CSLDestroy(argv);

    if (!psOptions)
    {
        Usage();
    }

    int bUsageError = FALSE;
    GDALDatasetH hOutDS = GDALTileIndex(
        sOptionsForBinary.osDest.c_str(), sOptionsForBinary.aosSrcFiles.size(),
        sOptionsForBinary.aosSrcFiles.List(), psOptions.get(), &bUsageError);

    int nRetCode = (hOutDS) ? 0 : 1;

    CPLErrorReset();
    // The flush to disk is only done at that stage, so check if any error has
    // happened
    if (GDALClose(hOutDS) != CE_None)
        nRetCode = 1;
    if (CPLGetLastErrorType() != CE_None)
        nRetCode = 1;

    GDALDumpOpenDatasets(stderr);

    GDALDestroyDriverManager();

    OGRCleanupAll();

    return nRetCode;
}

MAIN_END

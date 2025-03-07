/******************************************************************************
 *
 * Project:  Contour Generator
 * Purpose:  Contour Generator mainline.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Applied Coherent Technology (www.actgate.com).
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2018, Oslandia <infos at oslandia dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdalargumentparser.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "commonutils.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    fprintf(stderr, "%s\n", GDALContourGetParserUsage().c_str());
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

    GDALContourOptionsForBinary sOptionsForBinary;
    std::unique_ptr<GDALContourOptions, decltype(&GDALContourOptionsFree)>
        psOptions{GDALContourOptionsNew(argv + 1, &sOptionsForBinary),
                  GDALContourOptionsFree};

    CSLDestroy(argv);

    if (!psOptions)
    {
        Usage();
    }

    GDALProgressFunc pfnProgress = nullptr;

    if (!sOptionsForBinary.bQuiet)
        pfnProgress = GDALTermProgress;

    char **papszStringOptions = nullptr;

    GDALDatasetH hSrcDS{nullptr};
    GDALRasterBandH hBand{nullptr};
    GDALDatasetH hDstDS{nullptr};
    OGRLayerH hLayer{nullptr};
    CPLErr eErr =
        GDALContourProcessOptions(psOptions.get(), &papszStringOptions, &hSrcDS,
                                  &hBand, &hDstDS, &hLayer);

    if (eErr == CE_None)
    {
        eErr = GDALContourGenerateEx(hBand, hLayer, papszStringOptions,
                                     pfnProgress, nullptr);
    }

    if (GDALClose(hSrcDS) != CE_None)
        eErr = CE_Failure;

    GDALClose(hDstDS);

    CSLDestroy(papszStringOptions);
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return (eErr == CE_None) ? 0 : 1;
}

MAIN_END

/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  CLI front-end
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"
#include "commonutils.h"

#include "gdal.h"

#include <cassert>

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
    argc = GDALGeneralCmdLineProcessor(
        argc, &argv, GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_MULTIDIM_RASTER);
    if (argc < 1)
        return (-argc);

    auto alg = GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate(
        GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME);
    assert(alg);
    alg->SetCallPath(std::vector<std::string>{argv[0]});
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);
    CSLDestroy(argv);

    if (!alg->ParseCommandLineArguments(args))
    {
        fprintf(stderr, "%s", alg->GetUsageForCLI(true).c_str());
        return 1;
    }

    {
        const auto stdoutArg = alg->GetActualAlgorithm().GetArg("stdout");
        if (stdoutArg && stdoutArg->GetType() == GAAT_BOOLEAN)
            stdoutArg->Set(true);
    }

    GDALProgressFunc pfnProgress =
        alg->IsProgressBarRequested() ? GDALTermProgress : nullptr;
    void *pProgressData = nullptr;

    int ret = 0;
    if (alg->Run(pfnProgress, pProgressData) && alg->Finalize())
    {
        const auto outputArg =
            alg->GetActualAlgorithm().GetArg("output-string");
        if (outputArg && outputArg->GetType() == GAAT_STRING &&
            outputArg->IsOutput())
        {
            printf("%s", outputArg->Get<std::string>().c_str());
        }
    }
    else
    {
        ret = 1;
    }

    return ret;
}

MAIN_END

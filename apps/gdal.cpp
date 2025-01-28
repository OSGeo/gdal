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

// #define DEBUG_COMPLETION

/************************************************************************/
/*                           EmitCompletion()                           */
/************************************************************************/

/** Return on stdout a space-separated list of choices for bash completion */
static void EmitCompletion(std::unique_ptr<GDALAlgorithm> rootAlg,
                           const std::vector<std::string> &argsIn)
{
#ifdef DEBUG_COMPLETION
    for (size_t i = 0; i < argsIn.size(); ++i)
        fprintf(stderr, "arg[%d]='%s'\n", static_cast<int>(i),
                argsIn[i].c_str());
#endif

    std::vector<std::string> args = argsIn;

    std::string ret;
    const auto addSpace = [&ret]()
    {
        if (!ret.empty())
            ret += " ";
    };

    if (!args.empty() &&
        (args.back() == "--config" ||
         STARTS_WITH(args.back().c_str(), "--config=") ||
         (args.size() >= 2 && args[args.size() - 2] == "--config")))
    {
        if (args.back() == "--config=" || args.back().back() != '=')
        {
            CPLStringList aosConfigOptions(CPLGetKnownConfigOptions());
            for (const char *pszOpt : cpl::Iterate(aosConfigOptions))
            {
                addSpace();
                ret += pszOpt;
                ret += '=';
            }
            printf("%s", ret.c_str());
        }
        return;
    }

    // Get inner-most algorithm
    bool showAllOptions = true;
    auto curAlg = std::move(rootAlg);
    while (!args.empty() && !args.front().empty() && args.front()[0] != '-')
    {
        auto subAlg = curAlg->InstantiateSubAlgorithm(args.front());
        if (!subAlg)
            break;
        showAllOptions = false;
        args.erase(args.begin());
        curAlg = std::move(subAlg);
    }

    for (const auto &choice : curAlg->GetAutoComplete(args, showAllOptions))
    {
        addSpace();
        ret += CPLString(choice).replaceAll(" ", "\\ ");
    }

#ifdef DEBUG_COMPLETION
    fprintf(stderr, "ret = '%s'\n", ret.c_str());
#endif
    if (!ret.empty())
        printf("%s", ret.c_str());
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)
{
    auto alg = GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate(
        GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME);
    assert(alg);

    if (argc >= 3 && strcmp(argv[1], "completion") == 0)
    {
        GDALAllRegister();

        // Process lines like "gdal completion gdal raster"
        EmitCompletion(std::move(alg),
                       std::vector<std::string>(argv + 3, argv + argc));
        return 0;
    }

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

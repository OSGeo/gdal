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
#include "cpl_error.h"

#include "gdal.h"

#include <cassert>
#include <utility>

// #define DEBUG_COMPLETION

/************************************************************************/
/*                           EmitCompletion()                           */
/************************************************************************/

/** Return on stdout a space-separated list of choices for bash completion */
static void EmitCompletion(std::unique_ptr<GDALAlgorithm> rootAlg,
                           const std::vector<std::string> &argsIn,
                           bool lastWordIsComplete)
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

    for (const auto &choice : rootAlg->GetAutoComplete(
             args, lastWordIsComplete, /*showAllOptions = */ true))
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
    const bool bIsCompletion = argc >= 3 && strcmp(argv[1], "completion") == 0;

    if (bIsCompletion)
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        EarlySetConfigOptions(argc, argv);
    }
    else
    {
        EarlySetConfigOptions(argc, argv);
    }

    auto alg = GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate(
        GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME);
    assert(alg);

    // Register GDAL drivers
    GDALAllRegister();

    if (bIsCompletion)
    {
        const bool bLastWordIsComplete =
            EQUAL(argv[argc - 1], "last_word_is_complete=true");
        if (STARTS_WITH(argv[argc - 1], "last_word_is_complete="))
            --argc;

        // Process lines like "gdal completion gdal raster last_word_is_complete=true|false"
        EmitCompletion(std::move(alg),
                       std::vector<std::string>(argv + 3, argv + argc),
                       bLastWordIsComplete);
        return 0;
    }

    // Prevent GDALGeneralCmdLineProcessor() to process --format XXX, unless
    // "gdal" is invoked only with it. Cf #12411
    std::vector<std::pair<char **, char *>> apOrigFormat;
    constexpr const char *pszFormatReplaced = "--format-XXXX";
    if (!(argc == 3 && strcmp(argv[1], "--format") == 0))
    {
        for (int i = 1; i < argc; ++i)
        {
            if (strcmp(argv[i], "--format") == 0)
            {
                apOrigFormat.emplace_back(argv + i, argv[i]);
                argv[i] = const_cast<char *>(pszFormatReplaced);
            }
        }
    }

    // Process generic cmomand options
    argc = GDALGeneralCmdLineProcessor(
        argc, &argv, GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_MULTIDIM_RASTER);
    for (auto &pair : apOrigFormat)
    {
        *(pair.first) = pair.second;
    }

    if (argc < 1)
        return (-argc);

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(strcmp(argv[i], pszFormatReplaced) == 0 ? "--format"
                                                               : argv[i]);
    CSLDestroy(argv);

    alg->SetCalledFromCommandLine();

    if (!alg->ParseCommandLineArguments(args))
    {
        if (strstr(CPLGetLastErrorMsg(), "Do you mean") == nullptr)
        {
            fprintf(stderr, "%s", alg->GetUsageForCLI(true).c_str());
        }
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

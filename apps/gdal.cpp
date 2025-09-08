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
        for (int i = 1; i < argc; ++i)
        {
            // Used by gdal raster tile --parallel-method=spawn to pass
            // config options in a stealth way
            if (strcmp(argv[i], "--config-options-in-stdin") == 0)
            {
                std::string line;
                constexpr int LINE_SIZE = 10 * 1024;
                line.resize(LINE_SIZE);
                while (fgets(line.data(), LINE_SIZE, stdin))
                {
                    if (strcmp(line.c_str(), "--config\n") == 0 &&
                        fgets(line.data(), LINE_SIZE, stdin))
                    {
                        std::string osLine(line.c_str());
                        if (!osLine.empty() && osLine.back() == '\n')
                        {
                            osLine.pop_back();
                            char *pszUnescaped = CPLUnescapeString(
                                osLine.c_str(), nullptr, CPLES_URL);
                            char *pszKey = nullptr;
                            const char *pszValue =
                                CPLParseNameValue(pszUnescaped, &pszKey);
                            if (pszKey && pszValue)
                            {
                                CPLSetConfigOption(pszKey, pszValue);
                            }
                            CPLFree(pszKey);
                            CPLFree(pszUnescaped);
                        }
                    }
                    else if (strcmp(line.c_str(), "END_CONFIG\n") == 0)
                    {
                        break;
                    }
                }

                break;
            }
        }
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
        else if (argc >= 2 && STARTS_WITH(argv[argc - 2], "prev=") &&
                 STARTS_WITH(argv[argc - 1], "cur="))
        {
            const char *pszPrevVal = argv[argc - 2] + strlen("prev=");
            const char *pszCurVal = argv[argc - 1] + strlen("cur=");
            std::string osCurVal;
            const bool bIsPrevValEqual = (strcmp(pszPrevVal, "=") == 0);
            if (bIsPrevValEqual)
            {
                osCurVal = std::string("=").append(pszCurVal);
                pszCurVal = osCurVal.c_str();
            }
            int iMatch = 0;
            for (int i = 3; i < argc - 1; ++i)
            {
                if (bIsPrevValEqual ? (strstr(argv[i], pszCurVal) != nullptr)
                                    : (strcmp(argv[i], pszCurVal) == 0))
                {
                    if (iMatch == 0)
                        iMatch = i;
                    else
                        iMatch = -1;
                }
            }
            if (iMatch > 0)
                argc = iMatch + 1;
            else
                argc -= 2;
        }

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
        if (strstr(CPLGetLastErrorMsg(), "Do you mean") == nullptr &&
            strstr(CPLGetLastErrorMsg(), "Should be one among") == nullptr &&
            strstr(CPLGetLastErrorMsg(), "Potential values for argument") ==
                nullptr &&
            strstr(CPLGetLastErrorMsg(),
                   "Single potential value for argument") == nullptr)
        {
            fprintf(stderr, "%s", alg->GetUsageForCLI(true).c_str());
        }
        return 1;
    }

    {
        const auto stdoutArg =
            alg->GetActualAlgorithm().GetArg(GDAL_ARG_NAME_STDOUT);
        if (stdoutArg && stdoutArg->GetType() == GAAT_BOOLEAN)
            stdoutArg->Set(true);
    }

    GDALProgressFunc pfnProgress =
        alg->IsProgressBarRequested() ? GDALTermProgress : nullptr;
    void *pProgressData = nullptr;

    int ret = (alg->Run(pfnProgress, pProgressData) && alg->Finalize()) ? 0 : 1;

    const auto outputArg =
        alg->GetActualAlgorithm().GetArg(GDAL_ARG_NAME_OUTPUT_STRING);
    if (outputArg && outputArg->GetType() == GAAT_STRING &&
        outputArg->IsOutput())
    {
        printf("%s", outputArg->Get<std::string>().c_str());
    }

    const auto retCodeArg = alg->GetActualAlgorithm().GetArg("return-code");
    if (retCodeArg && retCodeArg->GetType() == GAAT_INTEGER &&
        retCodeArg->IsOutput())
    {
        ret = retCodeArg->Get<int>();
    }

    return ret;
}

MAIN_END

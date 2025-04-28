/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to build seek-optimized ZIP files
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdalalgorithm.h"
#include "commonutils.h"
#include "gdalargumentparser.h"

#include <cassert>

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(nArgc, papszArgv)
{
    EarlySetConfigOptions(nArgc, papszArgv);
    nArgc = GDALGeneralCmdLineProcessor(nArgc, &papszArgv, 0);
    CPLStringList aosArgv;
    aosArgv.Assign(papszArgv, /* bTakeOwnership= */ true);
    if (nArgc < 1)
        std::exit(-nArgc);

    GDALArgumentParser argParser(aosArgv[0], /* bForBinary=*/true);

    argParser.add_description(_("Generate a seek-optimized ZIP (SOZip) file."));

    argParser.add_epilog(
        _("For more details, consult https://gdal.org/programs/sozip.html"));

    std::string osZipFilename;
    argParser.add_argument("zip_filename")
        .metavar("<zip_filename>")
        .store_into(osZipFilename)
        .help(_("ZIP filename."));

    bool bRecurse = false;
    argParser.add_argument("-r", "--recurse-paths")
        .store_into(bRecurse)
        .help(_("Travels the directory structure of the specified directories "
                "recursively."));

    bool bOverwrite = false;
    {
        auto &group = argParser.add_mutually_exclusive_group();
        group.add_argument("-g", "--grow")
            .flag()  // Default mode. Nothing to do
            .help(
                _("Grow an existing zip file with the content of the specified "
                  "filename(s). Default mode."));
        group.add_argument("--overwrite")
            .store_into(bOverwrite)
            .help(_("Overwrite the target zip file if it already exists."));
    }

    bool bList = false;
    bool bValidate = false;
    std::string osOptimizeFrom;
    std::vector<std::string> aosFiles;
    {
        auto &group = argParser.add_mutually_exclusive_group();
        group.add_argument("-l", "--list")
            .store_into(bList)
            .help(_("List the files contained in the zip file."));
        group.add_argument("--validate")
            .store_into(bValidate)
            .help(_("Validates a ZIP/SOZip file."));
        group.add_argument("--optimize-from")
            .metavar("<input.zip>")
            .store_into(osOptimizeFrom)
            .help(
                _("Re-process {input.zip} to generate a SOZip-optimized .zip"));
        group.add_argument("input_files")
            .metavar("<input_files>")
            .store_into(aosFiles)
            .help(_("Filename of the file to add."))
            .nargs(argparse::nargs_pattern::any);
    }

    bool bQuiet = false;
    bool bVerbose = false;
    argParser.add_group("Advanced options");
    {
        auto &group = argParser.add_mutually_exclusive_group();
        group.add_argument("--quiet").store_into(bQuiet).help(
            _("Quiet mode. No progress message is emitted on the standard "
              "output."));
        group.add_argument("--verbose")
            .store_into(bVerbose)
            .help(_("Verbose mode."));
    }
    bool bJunkPaths = false;
    argParser.add_argument("-j", "--junk-paths")
        .store_into(bJunkPaths)
        .help(
            _("Store just the name of a saved file (junk the path), and do not "
              "store directory names."));

    CPLStringList aosOptions;
    argParser.add_argument("--enable-sozip")
        .choices("auto", "yes", "no")
        .metavar("auto|yes|no")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("SOZIP_ENABLED", s.c_str()); })
        .help(_("In auto mode, a file is seek-optimized only if its size is "
                "above the value of\n"
                "--sozip-chunk-size. In yes mode, all input files will be "
                "seek-optimized.\n"
                "In no mode, no input files will be seek-optimized."));
    argParser.add_argument("--sozip-chunk-size")
        .metavar("<value in bytes or with K/M suffix>")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("SOZIP_CHUNK_SIZE", s.c_str()); })
        .help(_(
            "Chunk size for a seek-optimized file. Defaults to 32768 bytes."));
    argParser.add_argument("--sozip-min-file-size")
        .metavar("<value in bytes or with K/M/G suffix>")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("SOZIP_MIN_FILE_SIZE", s.c_str()); })
        .help(
            _("Minimum file size to decide if a file should be seek-optimized. "
              "Defaults to 1 MB byte."));
    argParser.add_argument("--content-type")
        .metavar("<string>")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("CONTENT_TYPE", s.c_str()); })
        .help(_("Store the Content-Type for the file being added."));

    try
    {
        argParser.parse_args(aosArgv);
    }
    catch (const std::exception &err)
    {
        argParser.display_error_and_usage(err);
        std::exit(1);
    }

    if (!bList && !bValidate && osOptimizeFrom.empty() && aosFiles.empty())
    {
        std::cerr << _("Missing source filename(s)") << std::endl << std::endl;
        std::cerr << argParser << std::endl;
        std::exit(1);
    }

    const char *pszZipFilename = osZipFilename.c_str();
    if (!EQUAL(CPLGetExtensionSafe(pszZipFilename).c_str(), "zip"))
    {
        std::cerr << _("Extension of zip filename should be .zip") << std::endl
                  << std::endl;
        std::cerr << argParser << std::endl;
        std::exit(1);
    }

    auto alg = GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate(
        GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME);
    assert(alg);

    std::vector<std::string> args;
    args.push_back("vsi");
    args.push_back("sozip");

    if (bValidate)
    {
        args.push_back("validate");
        if (bVerbose)
            args.push_back("--verbose");
        args.push_back(pszZipFilename);
    }
    else if (bList)
    {
        args.push_back("list");
        args.push_back(pszZipFilename);
    }
    else
    {
        args.push_back(osOptimizeFrom.empty() ? "create" : "optimize");
        if (bRecurse)
            args.push_back("--recurse");
        if (bJunkPaths)
            args.push_back("--junk-paths");
        if (bOverwrite)
            args.push_back("--overwrite");
        if (const char *val = aosOptions.FetchNameValue("SOZIP_ENABLED"))
        {
            args.push_back("--enable-sozip");
            args.push_back(val);
        }
        if (const char *val = aosOptions.FetchNameValue("SOZIP_CHUNK_SIZE"))
        {
            args.push_back("--sozip-chunk-size");
            args.push_back(val);
        }
        if (const char *val = aosOptions.FetchNameValue("SOZIP_MIN_FILE_SIZE"))
        {
            args.push_back("--sozip-min-file-size");
            args.push_back(val);
        }
        if (const char *val = aosOptions.FetchNameValue("CONTENT_TYPE"))
        {
            args.push_back("--content-type");
            args.push_back(val);
        }
        if (osOptimizeFrom.empty())
        {
            for (const auto &s : aosFiles)
            {
                args.push_back(s);
            }
        }
        else
        {
            args.push_back(std::move(osOptimizeFrom));
        }
        args.push_back(pszZipFilename);
    }

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

    alg->SetCalledFromCommandLine();

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

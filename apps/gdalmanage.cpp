/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line utility for GDAL identify, delete, rename and copy
 *           (by file) operations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "cpl_conv.h"
#include "gdal_version.h"
#include "gdal.h"
#include "commonutils.h"
#include "gdalargumentparser.h"

/************************************************************************/
/*                     GDALManageOptions()                              */
/************************************************************************/

struct GDALManageOptions
{
    bool bRecursive = false;
    bool bForceRecurse = false;
    bool bReportFailures = false;
    std::string osNewName;
    std::string osDatasetName;
    std::vector<std::string> aosDatasetNames;
    std::string osDriverName;
};

/************************************************************************/
/*                       ProcessIdentifyTarget()                        */
/************************************************************************/

static void ProcessIdentifyTarget(const char *pszTarget,
                                  char **papszSiblingList, bool bRecursive,
                                  bool bReportFailures, bool bForceRecurse)

{
    GDALDriverH hDriver;
    VSIStatBufL sStatBuf;
    int i;

    hDriver = GDALIdentifyDriver(pszTarget, papszSiblingList);

    if (hDriver != nullptr)
        printf("%s: %s\n", pszTarget, GDALGetDriverShortName(hDriver));
    else if (bReportFailures)
        printf("%s: unrecognized\n", pszTarget);

    if (!bForceRecurse && (!bRecursive || hDriver != nullptr))
        return;

    if (VSIStatL(pszTarget, &sStatBuf) != 0 || !VSI_ISDIR(sStatBuf.st_mode))
        return;

    papszSiblingList = VSIReadDir(pszTarget);
    for (i = 0; papszSiblingList && papszSiblingList[i]; i++)
    {
        if (EQUAL(papszSiblingList[i], "..") || EQUAL(papszSiblingList[i], "."))
            continue;

        const CPLString osSubTarget =
            CPLFormFilenameSafe(pszTarget, papszSiblingList[i], nullptr);

        ProcessIdentifyTarget(osSubTarget, papszSiblingList, bRecursive,
                              bReportFailures, bForceRecurse);
    }
    CSLDestroy(papszSiblingList);
}

/************************************************************************/
/*                     GDALManageAppOptionsGetParser()                 */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALManageAppOptionsGetParser(GDALManageOptions *psOptions)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdalmanage", /* bForBinary */ true);

    argParser->add_description(
        _("Identify, delete, rename and copy raster data files."));
    argParser->add_epilog(_("For more details, consult the full documentation "
                            "for the gdalmanage utility "
                            "https://gdal.org/programs/gdalmanage.html"));

    auto addCommonOptions =
        [psOptions](GDALArgumentParser *subParser, const char *helpMessageSrc)
    {
        subParser->add_argument("-f")
            .metavar("<format>")
            .store_into(psOptions->osDriverName)
            .help(_("Specify format of raster file if unknown by the "
                    "application."));

        subParser->add_argument("datasetname")
            .metavar("<datasetname>")
            .store_into(psOptions->osDatasetName)
            .help(helpMessageSrc);

        subParser->add_argument("newdatasetname")
            .metavar("<newdatasetname>")
            .store_into(psOptions->osNewName)
            .help(_("Name of the new file."));
    };

    // Identify

    auto identifyParser =
        argParser->add_subparser("identify", /* bForBinary */ true);
    identifyParser->add_description(_("List data format of file(s)."));

    identifyParser->add_argument("-r")
        .flag()
        .store_into(psOptions->bRecursive)
        .help(_("Recursively scan files/folders for raster files."));

    identifyParser->add_argument("-fr")
        .flag()
        .store_into(psOptions->bRecursive)
        .store_into(psOptions->bForceRecurse)
        .help(_("Recursively scan folders for raster files, forcing "
                "recursion in folders recognized as valid formats."));

    identifyParser->add_argument("-u")
        .flag()
        .store_into(psOptions->bReportFailures)
        .help(_("Report failures if file type is unidentified."));

    // Note: this accepts multiple files
    identifyParser->add_argument("datasetname")
        .metavar("<datasetname>")
        .store_into(psOptions->aosDatasetNames)
        .remaining()
        .help(_("Name(s) of the file(s) to identify."));

    // Copy

    auto copyParser = argParser->add_subparser("copy", /* bForBinary */ true);
    copyParser->add_description(
        _("Create a copy of the raster file with a new name."));

    addCommonOptions(copyParser, _("Name of the file to copy."));

    // Rename

    auto renameParser =
        argParser->add_subparser("rename", /* bForBinary */ true);
    renameParser->add_description(_("Change the name of the raster file."));

    addCommonOptions(renameParser, _("Name of the file to rename."));

    // Delete

    auto deleteParser =
        argParser->add_subparser("delete", /* bForBinary */ true);
    deleteParser->add_description(_("Delete the raster file(s)."));

    // Note: this accepts multiple files
    deleteParser->add_argument("datasetname")
        .metavar("<datasetname>")
        .store_into(psOptions->aosDatasetNames)
        .remaining()
        .help(_("Name(s) of the file(s) to delete."));

    deleteParser->add_argument("-f")
        .metavar("<format>")
        .store_into(psOptions->osDriverName)
        .help(
            _("Specify format of raster file if unknown by the application."));

    return argParser;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{

    EarlySetConfigOptions(argc, argv);

    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    /* -------------------------------------------------------------------- */
    /*      Parse arguments.                                                */
    /* -------------------------------------------------------------------- */

    if (argc < 2)
    {
        try
        {
            GDALManageOptions sOptions;
            auto argParser = GDALManageAppOptionsGetParser(&sOptions);
            fprintf(stderr, "%s\n", argParser->usage().c_str());
        }
        catch (const std::exception &err)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                     err.what());
        }
        CSLDestroy(argv);
        exit(1);
    }

    GDALAllRegister();

    GDALManageOptions psOptions;
    auto argParser = GDALManageAppOptionsGetParser(&psOptions);

    try
    {
        argParser->parse_args_without_binary_name(argv + 1);
        CSLDestroy(argv);
    }
    catch (const std::exception &error)
    {
        argParser->display_error_and_usage(error);
        CSLDestroy(argv);
        exit(1);
    }

    // For some obscure reason datasetname is parsed as mandatory
    // if used with remaining() in a subparser
    if (psOptions.aosDatasetNames.empty() && psOptions.osDatasetName.empty())
    {
        std::invalid_argument error(
            _("No dataset name provided. At least one dataset "
              "name is required."));
        argParser->display_error_and_usage(error);
        exit(1);
    }

    GDALDriverH hDriver = nullptr;
    if (!psOptions.osDriverName.empty())
    {
        hDriver = GDALGetDriverByName(psOptions.osDriverName.c_str());
        if (hDriver == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to find driver '%s'.",
                     psOptions.osDriverName.c_str());
            exit(1);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Split out based on operation.                                   */
    /* -------------------------------------------------------------------- */

    if (argParser->is_subcommand_used("identify"))
    {
        // Process all files in aosDatasetName
        for (const auto &datasetName : psOptions.aosDatasetNames)
        {
            ProcessIdentifyTarget(
                datasetName.c_str(), nullptr, psOptions.bRecursive,
                psOptions.bReportFailures, psOptions.bForceRecurse);
        }
    }
    else if (argParser->is_subcommand_used("copy"))
    {
        GDALCopyDatasetFiles(hDriver, psOptions.osNewName.c_str(),
                             psOptions.osDatasetName.c_str());
    }
    else if (argParser->is_subcommand_used("rename"))
    {
        GDALRenameDataset(hDriver, psOptions.osNewName.c_str(),
                          psOptions.osDatasetName.c_str());
    }
    else if (argParser->is_subcommand_used("delete"))
    {
        // Process all files in aosDatasetName
        for (const auto &datasetName : psOptions.aosDatasetNames)
        {
            GDALDeleteDataset(hDriver, datasetName.c_str());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    GDALDestroy();

    exit(0);
}

MAIN_END

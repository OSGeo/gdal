/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Program to generate a UMN MapServer compatible tile index for a
 *           set of OGR data sources.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"

#include <vector>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdalargumentparser.h"

#include "gdalalg_vector_index.h"
#include "commonutils.h"

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(nArgc, papszArgv)

{
    EarlySetConfigOptions(nArgc, papszArgv);

    GDALAllRegister();

    nArgc = GDALGeneralCmdLineProcessor(nArgc, &papszArgv, 0);
    if (nArgc < 1)
        exit(-nArgc);

    CPLStringList aosArgv;
    for (int i = 0; i < nArgc; i++)
    {
        aosArgv.AddString(papszArgv[i]);
    }

    CSLDestroy(papszArgv);

    /* -------------------------------------------------------------------- */
    /*      Processing command line arguments.                              */
    /* -------------------------------------------------------------------- */
    std::string osOutputFormat;
    std::string osTileIndexField;
    std::string osOutputName;
    bool bWriteAbsolutePath{false};
    bool bSkipDifferentProjection{false};
    bool bAcceptDifferentSchemas{false};
    std::string osTargetSRS;
    std::string osSrcSRSName;
    std::string osSrcSRSFormat;
    std::vector<std::string> aosSrcDatasets;
    std::vector<std::string> aosLayerNames;
    std::vector<int> anLayerNumbers;

    GDALArgumentParser argParser{"ogrtindex", true};

    argParser.add_description(
        _("Program to generate a UMN MapServer compatible "
          "tile index for a set of OGR data sources."));

    argParser.add_epilog(
        _("For more details, see the full documentation for ogrtindex "
          "at\nhttps://gdal.org/programs/ogrtindex.html"));

    argParser.add_argument("-lnum")
        .metavar("<n>")
        .append()
        .scan<'i', int>()
        .store_into(anLayerNumbers)
        .help(
            _("Add layer number <n> from each source file in the tile index."));

    argParser.add_argument("-lname")
        .metavar("<name>")
        .append()
        .store_into(aosLayerNames)
        .help(_(
            "Add layer named <name> from each source file in the tile index."));

    argParser.add_output_format_argument(osOutputFormat);

    argParser.add_argument("-tileindex")
        .metavar("<tileindex>")
        .default_value("LOCATION")
        .nargs(1)
        .store_into(osTileIndexField)
        .help(_("Name to use for the dataset name."));

    argParser.add_argument("-write_absolute_path")
        .flag()
        .store_into(bWriteAbsolutePath)
        .help(_("Write absolute path of the source file in the tile index."));

    argParser.add_argument("-skip_different_projection")
        .flag()
        .store_into(bSkipDifferentProjection)
        .help(_("Skip layers that are not in the same projection as the first "
                "layer."));

    argParser.add_argument("-t_srs")
        .metavar("<srs_def>")
        .store_into(osTargetSRS)
        .help(
            _("Extent of input files will be transformed to the desired target "
              "coordinate reference system."));

    argParser.add_argument("-src_srs_name")
        .metavar("<field_name>")
        .store_into(osSrcSRSName)
        .help(_("Name of the field to store the SRS of each tile."));

    argParser.add_argument("-src_srs_format")
        .metavar("{AUTO|WKT|EPSG|PROJ}")
        .choices("AUTO", "WKT", "EPSG", "PROJ")
        .store_into(osSrcSRSFormat)
        .help(_("Format of the source SRS to store in the tile index file."));

    argParser.add_argument("-accept_different_schemas")
        .flag()
        .store_into(bAcceptDifferentSchemas)
        .help(_(
            "Disable check for identical schemas for layers in input files."));

    argParser.add_argument("output_dataset")
        .metavar("<output_dataset>")
        .store_into(osOutputName)
        .help(_("Name of the output dataset."));

    argParser.add_argument("src_dataset")
        .metavar("<src_dataset>")
        .nargs(nargs_pattern::at_least_one)
        .store_into(aosSrcDatasets)
        .help(_("Name of the source dataset(s)."));

    try
    {
        argParser.parse_args(aosArgv);
    }
    catch (const std::exception &e)
    {
        argParser.display_error_and_usage(e);
        GDALDestroy();
        return 1;
    }

    /* -------------------------------------------------------------------- */
    /*      Validate input                                                  */
    /* -------------------------------------------------------------------- */

    //srs_name must be specified when srs_format is specified.
    if (argParser.is_used("-src_srs_format") &&
        !argParser.is_used("-src_srs_name"))
    {
        fprintf(stderr, "-src_srs_name must be specified when -src_srs_format "
                        "is specified.\n");
        GDALDestroy();
        return 1;
    }

    GDALVectorIndexAlgorithm alg;
    alg["called-from-ogrtindex"] = true;
    alg["input"] = aosSrcDatasets;
    alg["output"] = osOutputName;
    if (!osOutputFormat.empty())
        alg["output-format"] = osOutputFormat;
    if (!aosLayerNames.empty())
        alg["source-layer-name"] = aosLayerNames;
    if (!anLayerNumbers.empty())
        alg["source-layer-index"] = anLayerNumbers;
    if (!osSrcSRSName.empty())
        alg["source-crs-field-name"] = osSrcSRSName;
    if (!osSrcSRSFormat.empty())
        alg["source-crs-format"] = osSrcSRSFormat;
    if (!osTargetSRS.empty())
        alg["dst-crs"] = osTargetSRS;
    alg["accept-different-schemas"] = bAcceptDifferentSchemas;
    if (bSkipDifferentProjection)
        alg["skip-different-crs"] = bSkipDifferentProjection;
    alg["absolute-path"] = bWriteAbsolutePath;
    alg["location-name"] = osTileIndexField;

    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        if (std::unique_ptr<GDALDataset>(
                GDALDataset::Open(osOutputName.c_str(), GDAL_OF_VECTOR)))
            alg["append"] = true;
    }

    const int nRetCode =
        CPLGetLastErrorType() == CE_None && alg.Run() && alg.Finalize() ? 0 : 1;

    GDALDestroy();

    return nRetCode;
}

MAIN_END

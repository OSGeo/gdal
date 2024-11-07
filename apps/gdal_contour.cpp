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

/************************************************************************/
/*                     GDALContourOptions                               */
/************************************************************************/

struct GDALContourOptions
{
    int nBand = 1;
    double dfInterval = 0.0;
    double dfNoData = 0.0;
    double dfOffset = 0.0;
    double dfExpBase = 0.0;
    bool b3D = false;
    bool bPolygonize = false;
    bool bNoDataSet = false;
    bool bIgnoreNoData = false;
    std::string osNewLayerName = "contour";
    std::string osFormat;
    std::string osElevAttrib;
    std::string osElevAttribMin;
    std::string osElevAttribMax;
    std::vector<double> adfFixedLevels;
    CPLStringList aosOpenOptions;
    CPLStringList aosCreationOptions;
    bool bQuiet = false;
    std::string aosDestFilename;
    std::string aosSrcFilename;
    GIntBig nGroupTransactions = 100 * 1000;
};

/************************************************************************/
/*                     GDALContourAppOptionsGetParser()                 */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALContourAppOptionsGetParser(GDALContourOptions *psOptions)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdal_contour", /* bForBinary */ true);

    argParser->add_description(_("Creates contour lines from a raster file."));
    argParser->add_epilog(_(
        "For more details, consult the full documentation for the gdal_contour "
        "utility: http://gdal.org/gdal_contour.html"));

    argParser->add_extra_usage_hint(
        _("One of -i, -fl or -e must be specified."));

    argParser->add_argument("-b")
        .metavar("<name>")
        .default_value(1)
        .nargs(1)
        .scan<'i', int>()
        .store_into(psOptions->nBand)
        .help(_("Select an input band band containing the DEM data."));

    argParser->add_argument("-a")
        .metavar("<name>")
        .store_into(psOptions->osElevAttrib)
        .help(_("Provides a name for the attribute in which to put the "
                "elevation."));

    argParser->add_argument("-amin")
        .metavar("<name>")
        .store_into(psOptions->osElevAttribMin)
        .help(_("Provides a name for the attribute in which to put the minimum "
                "elevation."));

    argParser->add_argument("-amax")
        .metavar("<name>")
        .store_into(psOptions->osElevAttribMax)
        .help(_("Provides a name for the attribute in which to put the maximum "
                "elevation."));

    argParser->add_argument("-3d")
        .flag()
        .store_into(psOptions->b3D)
        .help(_("Force production of 3D vectors instead of 2D."));

    argParser->add_argument("-inodata")
        .flag()
        .store_into(psOptions->bIgnoreNoData)
        .help(_("Ignore any nodata value implied in the dataset - treat all "
                "values as valid."));

    argParser->add_argument("-snodata")
        .metavar("<value>")
        .scan<'g', double>()
        .action(
            [psOptions](const auto &d)
            {
                psOptions->bNoDataSet = true;
                psOptions->dfNoData = CPLAtofM(d.c_str());
            })
        .help(_("Input pixel value to treat as \"nodata\"."));

    argParser->add_output_format_argument(psOptions->osFormat);

    argParser->add_dataset_creation_options_argument(psOptions->aosOpenOptions);

    argParser->add_layer_creation_options_argument(
        psOptions->aosCreationOptions);

    auto &group = argParser->add_mutually_exclusive_group();

    group.add_argument("-i")
        .metavar("<interval>")
        .scan<'g', double>()
        .store_into(psOptions->dfInterval)
        .help(_("Elevation interval between contours."));

    group.add_argument("-e")
        .metavar("<base>")
        .scan<'g', double>()
        .store_into(psOptions->dfExpBase)
        .help(_("Generate levels on an exponential scale: base ^ k, for k an "
                "integer."));

    // Dealt manually as argparse::nargs_pattern::at_least_one is problematic
    argParser->add_argument("-fl").scan<'g', double>().metavar("<level>").help(
        _("Name one or more \"fixed levels\" to extract."));

    argParser->add_argument("-off")
        .metavar("<offset>")
        .scan<'g', double>()
        .store_into(psOptions->dfOffset)
        .help(_("Offset from zero relative to which to interpret intervals."));

    argParser->add_argument("-nln")
        .metavar("<name>")
        .store_into(psOptions->osNewLayerName)
        .help(_("Provide a name for the output vector layer. Defaults to "
                "\"contour\"."));

    argParser->add_argument("-p")
        .flag()
        .store_into(psOptions->bPolygonize)
        .help(_("Generate contour polygons instead of lines."));

    argParser->add_argument("-gt")
        .metavar("<n>|unlimited")
        .action(
            [psOptions](const std::string &s)
            {
                if (EQUAL(s.c_str(), "unlimited"))
                    psOptions->nGroupTransactions = -1;
                else
                    psOptions->nGroupTransactions = atoi(s.c_str());
            })
        .help(_("Group <n> features per transaction."));

    argParser->add_quiet_argument(&psOptions->bQuiet);

    argParser->add_argument("src_filename")
        .store_into(psOptions->aosSrcFilename)
        .help("The source raster file.");

    argParser->add_argument("dst_filename")
        .store_into(psOptions->aosDestFilename)
        .help("The destination vector file.");

    return argParser;
}

static void CreateElevAttrib(const char *pszElevAttrib, OGRLayerH hLayer)
{
    OGRFieldDefnH hFld = OGR_Fld_Create(pszElevAttrib, OFTReal);
    OGRErr eErr = OGR_L_CreateField(hLayer, hFld, FALSE);
    OGR_Fld_Destroy(hFld);
    if (eErr == OGRERR_FAILURE)
    {
        exit(1);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{

    GDALProgressFunc pfnProgress = nullptr;

    EarlySetConfigOptions(argc, argv);

    /* -------------------------------------------------------------------- */
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */

    GDALAllRegister();
    OGRRegisterAll();

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
            GDALContourOptions sOptions;
            auto argParser = GDALContourAppOptionsGetParser(&sOptions);
            fprintf(stderr, "%s\n", argParser->usage().c_str());
        }
        catch (const std::exception &err)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                     err.what());
        }
        exit(1);
    }

    GDALContourOptions sOptions;
    CPLStringList aosArgv;

    try
    {
        /* -------------------------------------------------------------------- */
        /*      Pre-processing for custom syntax that ArgumentParser does not   */
        /*      support.                                                        */
        /* -------------------------------------------------------------------- */
        for (int i = 1; i < argc && argv != nullptr && argv[i] != nullptr; i++)
        {
            // argparser is confused by arguments that have at_least_one
            // cardinality, if they immediately precede positional arguments.
            if (EQUAL(argv[i], "-fl") && argv[i + 1])
            {
                if (strchr(argv[i + 1], ' '))
                {
                    const CPLStringList aosTokens(
                        CSLTokenizeString(argv[i + 1]));
                    for (const char *pszToken : aosTokens)
                    {
                        sOptions.adfFixedLevels.push_back(CPLAtof(pszToken));
                    }
                    i += 1;
                }
                else
                {
                    auto isNumeric = [](const char *pszArg) -> bool
                    {
                        char *pszEnd = nullptr;
                        CPLStrtod(pszArg, &pszEnd);
                        return pszEnd != nullptr && pszEnd[0] == '\0';
                    };

                    while (i < argc - 1 && isNumeric(argv[i + 1]))
                    {
                        sOptions.adfFixedLevels.push_back(CPLAtof(argv[i + 1]));
                        i += 1;
                    }
                }
            }
            else
            {
                aosArgv.AddString(argv[i]);
            }
        }

        auto argParser = GDALContourAppOptionsGetParser(&sOptions);
        argParser->parse_args_without_binary_name(aosArgv.List());

        if (sOptions.dfInterval == 0.0 && sOptions.adfFixedLevels.empty() &&
            sOptions.dfExpBase == 0.0)
        {
            fprintf(stderr, "%s\n", argParser->usage().c_str());
            exit(1);
        }
    }
    catch (const std::exception &error)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
        exit(1);
    }

    if (sOptions.aosSrcFilename.find("/vsistdout/") != std::string::npos ||
        sOptions.aosDestFilename.find("/vsistdout/") != std::string::npos)
    {
        sOptions.bQuiet = true;
    }

    if (!sOptions.bQuiet)
        pfnProgress = GDALTermProgress;

    /* -------------------------------------------------------------------- */
    /*      Open source raster file.                                        */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS =
        GDALOpen(sOptions.aosSrcFilename.c_str(), GA_ReadOnly);
    if (hSrcDS == nullptr)
        exit(2);

    GDALRasterBandH hBand = GDALGetRasterBand(hSrcDS, sOptions.nBand);
    if (hBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Band %d does not exist on dataset.", sOptions.nBand);
        exit(2);
    }

    if (!sOptions.bNoDataSet && !sOptions.bIgnoreNoData)
    {
        int bNoDataSet;
        sOptions.dfNoData = GDALGetRasterNoDataValue(hBand, &bNoDataSet);
        sOptions.bNoDataSet = bNoDataSet;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to get a coordinate system from the raster.                 */
    /* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hSRS = GDALGetSpatialRef(hSrcDS);

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    CPLString osFormat;
    if (sOptions.osFormat.empty())
    {
        const auto aoDrivers = GetOutputDriversFor(
            sOptions.aosDestFilename.c_str(), GDAL_OF_VECTOR);
        if (aoDrivers.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot guess driver for %s",
                     sOptions.aosDestFilename.c_str());
            exit(10);
        }
        else
        {
            if (aoDrivers.size() > 1)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Several drivers matching %s extension. Using %s",
                         CPLGetExtension(sOptions.aosDestFilename.c_str()),
                         aoDrivers[0].c_str());
            }
            osFormat = aoDrivers[0];
        }
    }
    else
    {
        osFormat = sOptions.osFormat;
    }

    OGRSFDriverH hDriver = OGRGetDriverByName(osFormat.c_str());

    if (hDriver == nullptr)
    {
        fprintf(stderr, "Unable to find format driver named %s.\n",
                osFormat.c_str());
        exit(10);
    }

    OGRDataSourceH hDS = OGR_Dr_CreateDataSource(
        hDriver, sOptions.aosDestFilename.c_str(), sOptions.aosCreationOptions);
    if (hDS == nullptr)
        exit(1);

    OGRLayerH hLayer = OGR_DS_CreateLayer(
        hDS, sOptions.osNewLayerName.c_str(), hSRS,
        sOptions.bPolygonize
            ? (sOptions.b3D ? wkbMultiPolygon25D : wkbMultiPolygon)
            : (sOptions.b3D ? wkbLineString25D : wkbLineString),
        sOptions.aosCreationOptions);
    if (hLayer == nullptr)
        exit(1);

    if (!OGR_L_TestCapability(hLayer, OLCTransactions))
    {
        sOptions.nGroupTransactions = 0;
    }

    OGRFieldDefnH hFld = OGR_Fld_Create("ID", OFTInteger);
    OGR_Fld_SetWidth(hFld, 8);
    OGR_L_CreateField(hLayer, hFld, FALSE);
    OGR_Fld_Destroy(hFld);

    if (sOptions.bPolygonize)
    {
        if (!sOptions.osElevAttrib.empty())
        {
            sOptions.osElevAttrib.clear();
            CPLError(CE_Warning, CPLE_NotSupported,
                     "-a is ignored in polygonal contouring mode. "
                     "Use -amin and/or -amax instead");
        }
    }
    else
    {
        if (!sOptions.osElevAttribMin.empty() ||
            !sOptions.osElevAttribMax.empty())
        {
            sOptions.osElevAttribMin.clear();
            sOptions.osElevAttribMax.clear();
            CPLError(CE_Warning, CPLE_NotSupported,
                     "-amin and/or -amax are ignored in line contouring mode. "
                     "Use -a instead");
        }
    }

    if (!sOptions.osElevAttrib.empty())
    {
        CreateElevAttrib(sOptions.osElevAttrib.c_str(), hLayer);
    }

    if (!sOptions.osElevAttribMin.empty())
    {
        CreateElevAttrib(sOptions.osElevAttribMin.c_str(), hLayer);
    }

    if (!sOptions.osElevAttribMax.empty())
    {
        CreateElevAttrib(sOptions.osElevAttribMax.c_str(), hLayer);
    }

    /* -------------------------------------------------------------------- */
    /*      Invoke.                                                         */
    /* -------------------------------------------------------------------- */
    int iIDField = OGR_FD_GetFieldIndex(OGR_L_GetLayerDefn(hLayer), "ID");
    int iElevField = (sOptions.osElevAttrib.empty())
                         ? -1
                         : OGR_FD_GetFieldIndex(OGR_L_GetLayerDefn(hLayer),
                                                sOptions.osElevAttrib.c_str());

    int iElevFieldMin =
        (sOptions.osElevAttribMin.empty())
            ? -1
            : OGR_FD_GetFieldIndex(OGR_L_GetLayerDefn(hLayer),
                                   sOptions.osElevAttribMin.c_str());

    int iElevFieldMax =
        (sOptions.osElevAttribMax.empty())
            ? -1
            : OGR_FD_GetFieldIndex(OGR_L_GetLayerDefn(hLayer),
                                   sOptions.osElevAttribMax.c_str());

    char **options = nullptr;
    if (!sOptions.adfFixedLevels.empty())
    {
        std::string values = "FIXED_LEVELS=";
        for (size_t i = 0; i < sOptions.adfFixedLevels.size(); i++)
        {
            const int sz = 32;
            char *newValue = new char[sz + 1];
            if (i == sOptions.adfFixedLevels.size() - 1)
            {
                CPLsnprintf(newValue, sz + 1, "%f", sOptions.adfFixedLevels[i]);
            }
            else
            {
                CPLsnprintf(newValue, sz + 1, "%f,",
                            sOptions.adfFixedLevels[i]);
            }
            values = values + std::string(newValue);
            delete[] newValue;
        }
        options = CSLAddString(options, values.c_str());
    }

    if (sOptions.dfExpBase != 0.0)
    {
        options =
            CSLAppendPrintf(options, "LEVEL_EXP_BASE=%f", sOptions.dfExpBase);
    }
    else if (sOptions.dfInterval != 0.0)
    {
        options =
            CSLAppendPrintf(options, "LEVEL_INTERVAL=%f", sOptions.dfInterval);
    }

    if (sOptions.dfOffset != 0.0)
    {
        options = CSLAppendPrintf(options, "LEVEL_BASE=%f", sOptions.dfOffset);
    }

    if (sOptions.bNoDataSet)
    {
        options = CSLAppendPrintf(options, "NODATA=%.19g", sOptions.dfNoData);
    }
    if (iIDField != -1)
    {
        options = CSLAppendPrintf(options, "ID_FIELD=%d", iIDField);
    }
    if (iElevField != -1)
    {
        options = CSLAppendPrintf(options, "ELEV_FIELD=%d", iElevField);
    }
    if (iElevFieldMin != -1)
    {
        options = CSLAppendPrintf(options, "ELEV_FIELD_MIN=%d", iElevFieldMin);
    }
    if (iElevFieldMax != -1)
    {
        options = CSLAppendPrintf(options, "ELEV_FIELD_MAX=%d", iElevFieldMax);
    }
    if (sOptions.bPolygonize)
    {
        options = CSLAppendPrintf(options, "POLYGONIZE=YES");
    }
    if (sOptions.nGroupTransactions)
    {
        options = CSLAppendPrintf(options, "COMMIT_INTERVAL=" CPL_FRMT_GIB,
                                  sOptions.nGroupTransactions);
    }

    CPLErr eErr =
        GDALContourGenerateEx(hBand, hLayer, options, pfnProgress, nullptr);

    CSLDestroy(options);
    if (GDALClose(hDS) != CE_None)
        eErr = CE_Failure;
    GDALClose(hSrcDS);

    CSLDestroy(argv);
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return (eErr == CE_None) ? 0 : 1;
}

MAIN_END

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
#include "gdal_utils_priv.h"

/************************************************************************/
/*                     GDALContourOptions                               */
/************************************************************************/

/** Options for use with GDALContour(). GDALContourOptions must be allocated
 *  with GDALContourOptionsNew() and deallocated with GDALContourOptionsFree().
 */
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
    std::string osFormat{};
    std::string osElevAttrib{};
    std::string osElevAttribMin{};
    std::string osElevAttribMax{};
    std::vector<std::string> aosFixedLevels{};
    CPLStringList aosOpenOptions{};
    CPLStringList aosCreationOptions{};
    CPLStringList aosLayerCreationOptions{};
    bool bQuiet = false;
    std::string osDestDataSource{};
    std::string osSrcDataSource{};
    GIntBig nGroupTransactions = 100 * 1000;
    GDALProgressFunc pfnProgress = GDALDummyProgress;
    void *pProgressData = nullptr;
};

/************************************************************************/
/*                 GDALContourOptionsSetDestDataSource                  */
/************************************************************************/
void GDALContourOptionsSetDestDataSource(GDALContourOptions *psOptions,
                                         const char *pszDestDatasource)
{
    psOptions->osDestDataSource = pszDestDatasource;
}

/************************************************************************/
/*                  GDALContourOptionsSetProgress()                   */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALContour().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 */

void GDALContourOptionsSetProgress(GDALContourOptions *psOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
}

///@cond Doxygen_Suppress

/************************************************************************/
/*                  CreateElevAttrib()                                  */
/************************************************************************/

static bool CreateElevAttrib(const char *pszElevAttrib, OGRLayerH hLayer)
{
    OGRFieldDefnH hFld = OGR_Fld_Create(pszElevAttrib, OFTReal);
    OGRErr eErr = OGR_L_CreateField(hLayer, hFld, FALSE);
    OGR_Fld_Destroy(hFld);
    return eErr == OGRERR_NONE;
}

/************************************************************************/
/*                  GDALContourProcessOptions()                      */
/************************************************************************/

CPLErr GDALContourProcessOptions(GDALContourOptions *psOptions,
                                 char ***ppapszStringOptions,
                                 GDALDatasetH *hSrcDS, GDALRasterBandH *hBand,
                                 GDALDatasetH *hDstDS, OGRLayerH *hLayer)
{

    /* -------------------------------------------------------------------- */
    /*      Open source raster file.                                        */
    /* -------------------------------------------------------------------- */
    if (!*hSrcDS)
    {
        *hSrcDS = GDALOpen(psOptions->osSrcDataSource.c_str(), GA_ReadOnly);
    }

    if (*hSrcDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open source raster file '%s'.",
                 psOptions->osSrcDataSource.c_str());
        return CE_Failure;
    }

    if (!*hBand)
    {
        *hBand = GDALGetRasterBand(*hSrcDS, psOptions->nBand);
    }

    if (*hBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Band %d does not exist on dataset.", psOptions->nBand);
        return CE_Failure;
    }

    if (!psOptions->bNoDataSet && !psOptions->bIgnoreNoData)
    {
        int bNoDataSet;
        psOptions->dfNoData = GDALGetRasterNoDataValue(*hBand, &bNoDataSet);
        psOptions->bNoDataSet = bNoDataSet;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to get a coordinate system from the raster.                 */
    /* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hSRS = GDALGetSpatialRef(*hSrcDS);

    // Dedup lambda to create the layer
    auto CreateLayer = [&]() -> OGRLayerH
    {
        return GDALDatasetCreateLayer(
            *hDstDS, psOptions->osNewLayerName.c_str(), hSRS,
            psOptions->bPolygonize
                ? (psOptions->b3D ? wkbMultiPolygon25D : wkbMultiPolygon)
                : (psOptions->b3D ? wkbLineString25D : wkbLineString),
            psOptions->aosLayerCreationOptions);
    };

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    if (!*hDstDS && !*hLayer)
    {
        CPLString osFormat;
        if (psOptions->osFormat.empty())
        {
            const auto aoDrivers = GetOutputDriversFor(
                psOptions->osDestDataSource.c_str(), GDAL_OF_VECTOR);
            if (aoDrivers.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot guess driver for %s",
                         psOptions->osDestDataSource.c_str());
                return CE_Failure;
            }
            else
            {
                if (aoDrivers.size() > 1)
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Several drivers matching %s extension. Using %s",
                        CPLGetExtensionSafe(psOptions->osDestDataSource.c_str())
                            .c_str(),
                        aoDrivers[0].c_str());
                }
                osFormat = aoDrivers[0];
            }
        }
        else
        {
            osFormat = psOptions->osFormat;
        }

        OGRSFDriverH hDriver = OGRGetDriverByName(osFormat.c_str());

        if (hDriver == nullptr)
        {
            fprintf(stderr, "Unable to find format driver named %s.\n",
                    osFormat.c_str());
            return CE_Failure;
        }

        if (!*hDstDS)
        {
            *hDstDS =
                GDALCreate(hDriver, psOptions->osDestDataSource.c_str(), 0, 0,
                           0, GDT_Unknown, psOptions->aosCreationOptions);
        }

        if (*hDstDS == nullptr)
        {
            return CE_Failure;
        }

        // Create the layer
        *hLayer = CreateLayer();
    }

    if (!*hLayer)
    {
        auto hDriver = GDALGetDatasetDriver(*hDstDS);
        // Try to load the layer if it already exists
        if (GDALGetMetadataItem(hDriver, GDAL_DCAP_MULTIPLE_VECTOR_LAYERS,
                                nullptr))
        {
            *hLayer = GDALDatasetGetLayerByName(
                *hDstDS, psOptions->osNewLayerName.c_str());
            if (!*hLayer &&
                GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE_LAYER, nullptr) &&
                !GDALDatasetTestCapability(*hDstDS, ODsCCreateLayer))
            {
                *hLayer = CreateLayer();
            }
        }
        else
        {
            *hLayer = GDALDatasetGetLayer(*hDstDS, 0);
        }
    }

    if (*hLayer == nullptr)
    {
        return CE_Failure;
    }

    if (!OGR_L_TestCapability(*hLayer, OLCTransactions))
    {
        psOptions->nGroupTransactions = 0;
    }

    OGRFieldDefnH hFld = OGR_Fld_Create("ID", OFTInteger);
    OGR_Fld_SetWidth(hFld, 8);
    OGR_L_CreateField(*hLayer, hFld, FALSE);
    OGR_Fld_Destroy(hFld);

    if (psOptions->bPolygonize)
    {
        if (!psOptions->osElevAttrib.empty())
        {
            psOptions->osElevAttrib.clear();
            CPLError(CE_Warning, CPLE_NotSupported,
                     "-a is ignored in polygonal contouring mode. "
                     "Use -amin and/or -amax instead");
        }
    }
    else
    {
        if (!psOptions->osElevAttribMin.empty() ||
            !psOptions->osElevAttribMax.empty())
        {
            psOptions->osElevAttribMin.clear();
            psOptions->osElevAttribMax.clear();
            CPLError(CE_Warning, CPLE_NotSupported,
                     "-amin and/or -amax are ignored in line contouring mode. "
                     "Use -a instead");
        }
    }

    OGRFeatureDefnH hFeatureDefn = OGR_L_GetLayerDefn(*hLayer);

    if (!psOptions->osElevAttrib.empty())
    {
        // Skip if field already exists
        if (OGR_FD_GetFieldIndex(hFeatureDefn,
                                 psOptions->osElevAttrib.c_str()) == -1)
        {
            if (!CreateElevAttrib(psOptions->osElevAttrib.c_str(), *hLayer))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create elevation field '%s'",
                         psOptions->osElevAttrib.c_str());
                return CE_Failure;
            }
        }
    }

    if (!psOptions->osElevAttribMin.empty())
    {
        // Skip if field already exists
        if (OGR_FD_GetFieldIndex(hFeatureDefn,
                                 psOptions->osElevAttribMin.c_str()) == -1)
        {
            if (!CreateElevAttrib(psOptions->osElevAttribMin.c_str(), *hLayer))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create elevation min field '%s'",
                         psOptions->osElevAttribMin.c_str());
                return CE_Failure;
            }
        }
    }

    if (!psOptions->osElevAttribMax.empty())
    {
        // Skip if field already exists
        if (OGR_FD_GetFieldIndex(hFeatureDefn,
                                 psOptions->osElevAttribMax.c_str()) == -1)
        {
            if (!CreateElevAttrib(psOptions->osElevAttribMax.c_str(), *hLayer))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create elevation max field '%s'",
                         psOptions->osElevAttribMax.c_str());
                return CE_Failure;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Invoke.                                                         */
    /* -------------------------------------------------------------------- */
    int iIDField = OGR_FD_GetFieldIndex(hFeatureDefn, "ID");
    int iElevField = (psOptions->osElevAttrib.empty())
                         ? -1
                         : OGR_FD_GetFieldIndex(
                               hFeatureDefn, psOptions->osElevAttrib.c_str());

    int iElevFieldMin =
        (psOptions->osElevAttribMin.empty())
            ? -1
            : OGR_FD_GetFieldIndex(hFeatureDefn,
                                   psOptions->osElevAttribMin.c_str());

    int iElevFieldMax =
        (psOptions->osElevAttribMax.empty())
            ? -1
            : OGR_FD_GetFieldIndex(hFeatureDefn,
                                   psOptions->osElevAttribMax.c_str());

    if (!psOptions->aosFixedLevels.empty())
    {
        std::string values = "FIXED_LEVELS=";
        for (size_t i = 0; i < psOptions->aosFixedLevels.size(); i++)
        {
            if (i == psOptions->aosFixedLevels.size() - 1)
            {
                values = values + psOptions->aosFixedLevels[i];
            }
            else
            {
                values = values + psOptions->aosFixedLevels[i] + ",";
            }
        }
        *ppapszStringOptions =
            CSLAddString(*ppapszStringOptions, values.c_str());
    }

    if (psOptions->dfExpBase != 0.0)
    {
        *ppapszStringOptions = CSLAppendPrintf(
            *ppapszStringOptions, "LEVEL_EXP_BASE=%f", psOptions->dfExpBase);
    }
    else if (psOptions->dfInterval != 0.0)
    {
        *ppapszStringOptions = CSLAppendPrintf(
            *ppapszStringOptions, "LEVEL_INTERVAL=%f", psOptions->dfInterval);
    }

    if (psOptions->dfOffset != 0.0)
    {
        *ppapszStringOptions = CSLAppendPrintf(
            *ppapszStringOptions, "LEVEL_BASE=%f", psOptions->dfOffset);
    }

    if (psOptions->bNoDataSet)
    {
        *ppapszStringOptions = CSLAppendPrintf(
            *ppapszStringOptions, "NODATA=%.19g", psOptions->dfNoData);
    }
    if (iIDField != -1)
    {
        *ppapszStringOptions =
            CSLAppendPrintf(*ppapszStringOptions, "ID_FIELD=%d", iIDField);
    }
    if (iElevField != -1)
    {
        *ppapszStringOptions =
            CSLAppendPrintf(*ppapszStringOptions, "ELEV_FIELD=%d", iElevField);
    }
    if (iElevFieldMin != -1)
    {
        *ppapszStringOptions = CSLAppendPrintf(
            *ppapszStringOptions, "ELEV_FIELD_MIN=%d", iElevFieldMin);
    }
    if (iElevFieldMax != -1)
    {
        *ppapszStringOptions = CSLAppendPrintf(
            *ppapszStringOptions, "ELEV_FIELD_MAX=%d", iElevFieldMax);
    }
    if (psOptions->bPolygonize)
    {
        *ppapszStringOptions =
            CSLAppendPrintf(*ppapszStringOptions, "POLYGONIZE=YES");
    }
    if (psOptions->nGroupTransactions)
    {
        *ppapszStringOptions = CSLAppendPrintf(*ppapszStringOptions,
                                               "COMMIT_INTERVAL=" CPL_FRMT_GIB,
                                               psOptions->nGroupTransactions);
    }

    return CE_None;
}

///@endcond

/************************************************************************/
/*                     GDALContourAppOptionsGetParser()                 */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALContourAppOptionsGetParser(GDALContourOptions *psOptions,
                               GDALContourOptionsForBinary *psOptionsForBinary)
{

    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdal_contour", /* bForBinary=*/psOptionsForBinary != nullptr);

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
    argParser->add_argument("-fl").metavar("<level>").help(
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

    // Written that way so that in library mode, users can still use the -q
    // switch, even if it has no effect
    argParser->add_quiet_argument(
        psOptionsForBinary ? &(psOptionsForBinary->bQuiet) : nullptr);

    if (psOptionsForBinary)
    {
        argParser->add_open_options_argument(
            psOptionsForBinary->aosOpenOptions);

        argParser->add_argument("src_filename")
            .store_into(psOptions->osSrcDataSource)
            .help("The source raster file.");

        argParser->add_dataset_creation_options_argument(
            psOptions->aosOpenOptions);

        argParser->add_argument("dst_filename")
            .store_into(psOptions->osDestDataSource)
            .help("The destination vector file.");

        argParser->add_output_format_argument(psOptions->osFormat);

        argParser->add_creation_options_argument(psOptions->aosCreationOptions);

        argParser->add_layer_creation_options_argument(
            psOptions->aosLayerCreationOptions);
    }

    return argParser;
}

/************************************************************************/
/*                         GDALContourGetParserUsage()                     */
/************************************************************************/

std::string GDALContourGetParserUsage()
{
    try
    {
        GDALContourOptions sOptions;
        auto argParser = GDALContourAppOptionsGetParser(&sOptions, nullptr);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                     GDALContourOptionsNew()                          */
/************************************************************************/

/**
 * Create a new GDALContourOptions object.
 *
 * @param papszArgv the command line arguments.
 * @param psOptionsForBinary the options for binary.
 *
 * @return the new GDALContourOptions object.
 */
GDALContourOptions *
GDALContourOptionsNew(char **papszArgv,
                      GDALContourOptionsForBinary *psOptionsForBinary)
{

    auto psOptions = std::make_unique<GDALContourOptions>();

    /*-------------------------------------------------------------------- */
    /*      Parse arguments.                                               */
    /*-------------------------------------------------------------------- */

    CPLStringList aosArgv;

    /* -------------------------------------------------------------------- */
    /*      Pre-processing for custom syntax that ArgumentParser does not   */
    /*      support.                                                        */
    /* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);

    /* -------------------------------------------------------------------- */
    /*      Pre-processing for custom syntax that ArgumentParser does not   */
    /*      support.                                                        */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < argc && papszArgv != nullptr && papszArgv[i] != nullptr;
         i++)
    {
        // argparser is confused by arguments that have at_least_one
        // cardinality, if they immediately precede positional arguments.
        if (EQUAL(papszArgv[i], "-fl") && papszArgv[i + 1])
        {
            if (strchr(papszArgv[i + 1], ' '))
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString(papszArgv[i + 1]));
                for (const char *pszToken : aosTokens)
                {
                    // Handle min/max special values
                    if (EQUAL(pszToken, "MIN"))
                    {
                        psOptions->aosFixedLevels.push_back("MIN");
                    }
                    else if (EQUAL(pszToken, "MAX"))
                    {
                        psOptions->aosFixedLevels.push_back("MAX");
                    }
                    else
                    {
                        psOptions->aosFixedLevels.push_back(
                            std::to_string(CPLAtof(pszToken)));
                    }
                }
                i += 1;
            }
            else
            {
                auto isNumericOrMinMax = [](const char *pszArg) -> bool
                {
                    if (EQUAL(pszArg, "MIN") || EQUAL(pszArg, "MAX"))
                        return true;
                    char *pszEnd = nullptr;
                    CPLStrtod(pszArg, &pszEnd);
                    return pszEnd != nullptr && pszEnd[0] == '\0';
                };

                while (i < argc - 1 && isNumericOrMinMax(papszArgv[i + 1]))
                {
                    if (EQUAL(papszArgv[i + 1], "MIN"))
                    {
                        psOptions->aosFixedLevels.push_back("MIN");
                    }
                    else if (EQUAL(papszArgv[i + 1], "MAX"))
                    {
                        psOptions->aosFixedLevels.push_back("MAX");
                    }
                    else
                    {
                        psOptions->aosFixedLevels.push_back(
                            std::to_string(CPLAtof(papszArgv[i + 1])));
                    }
                    i += 1;
                }
            }
        }
        else
        {
            aosArgv.AddString(papszArgv[i]);
        }
    }

    try
    {

        auto argParser =
            GDALContourAppOptionsGetParser(psOptions.get(), psOptionsForBinary);
        argParser->parse_args_without_binary_name(aosArgv.List());

        if (psOptions->dfInterval == 0.0 && psOptions->aosFixedLevels.empty() &&
            psOptions->dfExpBase == 0.0)
        {
            fprintf(stderr, "%s\n", argParser->usage().c_str());
            return nullptr;
        }

        if (psOptions->osSrcDataSource.find("/vsistdout/") !=
                std::string::npos ||
            psOptions->osDestDataSource.find("/vsistdout/") !=
                std::string::npos)
        {
            psOptions->bQuiet = true;
        }

        if (psOptionsForBinary)
        {
            psOptionsForBinary->bQuiet = psOptions->bQuiet;
            psOptionsForBinary->osDestDataSource = psOptions->osDestDataSource;
            psOptionsForBinary->osSrcDataSource = psOptions->osSrcDataSource;
            psOptionsForBinary->aosOpenOptions = psOptions->aosOpenOptions;
        }

        return psOptions.release();
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                     GDALContourOptionsFree()                         */
/************************************************************************/

/**
 * Free a GDALContourOptions object.
 *
 * @param psOptions the GDALContourOptions object to free.
 */
void GDALContourOptionsFree(GDALContourOptions *psOptions)
{
    delete psOptions;
}

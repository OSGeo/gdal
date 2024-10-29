/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Raster creation utility
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal_priv.h"
#include "gdal.h"
#include "commonutils.h"
#include "gdalargumentparser.h"
#include "ogr_spatialref.h"

#include <cstdlib>
#include <memory>
#include <vector>

/**
 * @brief Makes sure the GDAL library is properly cleaned up before exiting.
 * @param nCode exit code
 * @todo Move to API
 */
static void GDALExit(int nCode)
{
    GDALDestroy();
    exit(nCode);
}

/************************************************************************/
/*                     GDALCreateOptions                               */
/************************************************************************/

struct GDALCreateOptions
{
    int nBandCount = -1;
    int nPixels = 0;
    bool bPixelsSet{false};
    int nLines = 0;
    GDALDataType eDT = GDT_Unknown;
    double dfULX = 0;
    double dfULY = 0;
    double dfLRX = 0;
    double dfLRY = 0;
    int nULCounter{0};
    bool bGeoTransform = false;
    std::string osOutputSRS;
    CPLStringList aosMetadata;
    std::vector<double> adfBurnValues;
    bool bQuiet = false;
    bool bSetNoData = false;
    std::string osNoData;
    std::string osOutputFilename;
    std::string osInputFilename;
    std::string osFormat;
    CPLStringList aosCreateOptions;
};

/************************************************************************/
/*                   GDALCreateAppOptionsGetParser()                   */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALCreateAppOptionsGetParser(GDALCreateOptions *psOptions)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdal_create", /* bForBinary */ true);

    argParser->add_description(
        _("Create a raster file (without source dataset)."));

    argParser->add_epilog(_(
        "For more details, consult the full documentation for the gdal_create "
        "utility: http://gdal.org/gdal_create.html"));

    argParser->add_output_type_argument(psOptions->eDT);

    argParser->add_output_format_argument(psOptions->osFormat);

    argParser->add_argument("-outsize")
        .metavar("<xsize> <ysize>")
        .nargs(2)
        .scan<'i', int>()
        .action(
            [psOptions](const std::string &s)
            {
                if (!psOptions->bPixelsSet)
                {
                    psOptions->nPixels = atoi(s.c_str());
                    psOptions->bPixelsSet = true;
                }
                else
                {
                    psOptions->nLines = atoi(s.c_str());
                }
            })
        .help(_("Set the size of the output file."));

    argParser->add_argument("-bands")
        .metavar("<count>")
        .store_into(psOptions->nBandCount)
        .help(_("Set the number of bands in the output file."));

    argParser->add_argument("-burn").metavar("<value>").append().help(
        _("A fixed value to burn into a band. A list of "
          "-burn options can be supplied, one per band being written to."));

    argParser->add_argument("-a_srs")
        .metavar("<srs_def>")
        .store_into(psOptions->osOutputSRS)
        .help(_("Override the projection for the output file. "));

    argParser->add_argument("-a_ullr")
        .metavar("<ulx> <uly> <lrx> <lry>")
        .scan<'g', double>()
        .nargs(4)
        .action(
            [psOptions](const std::string &s)
            {
                switch (psOptions->nULCounter++)
                {
                    case 0:
                        psOptions->bGeoTransform = true;
                        psOptions->dfULX = CPLAtofM(s.c_str());
                        break;
                    case 1:
                        psOptions->dfULY = CPLAtofM(s.c_str());
                        break;
                    case 2:
                        psOptions->dfLRX = CPLAtofM(s.c_str());
                        break;
                    case 3:
                        psOptions->dfLRY = CPLAtof(s.c_str());
                        break;
                }
            })
        .help(_("Assign the georeferenced bounds of the output file. "));

    argParser->add_argument("-a_nodata")
        .metavar("<value>")
        .scan<'g', double>()
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->bSetNoData = true;
                psOptions->osNoData = s;
            })
        .help(_("Assign a specified nodata value to output bands."));

    argParser->add_metadata_item_options_argument(psOptions->aosMetadata);

    argParser->add_creation_options_argument(psOptions->aosCreateOptions);

    argParser->add_quiet_argument(&psOptions->bQuiet);

    argParser->add_argument("-if")
        .metavar("<input_dataset>")
        .store_into(psOptions->osInputFilename)
        .help(_("Name of GDAL input dataset that serves as a template for "
                "default values of options -outsize, -bands, -ot, -a_srs, "
                "-a_ullr and -a_nodata."));

    argParser->add_argument("out_dataset")
        .metavar("<out_dataset>")
        .store_into(psOptions->osOutputFilename)
        .help(_("Name of the output dataset to create."));

    return argParser;
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
        GDALExit(-argc);

    if (argc < 2)
    {
        try
        {
            GDALCreateOptions sOptions;
            auto argParser = GDALCreateAppOptionsGetParser(&sOptions);
            fprintf(stderr, "%s\n", argParser->usage().c_str());
        }
        catch (const std::exception &err)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                     err.what());
        }
        CSLDestroy(argv);
        GDALExit(1);
    }

    GDALCreateOptions sOptions;

    CPLStringList aosArgv;
    for (int iArg = 1; iArg < argc; iArg++)
    {
        if (iArg + 1 < argc && EQUAL(argv[iArg], "-burn"))
        {
            ++iArg;
            while (true)
            {
                if (strchr(argv[iArg], ' '))
                {
                    const CPLStringList aosTokens(
                        CSLTokenizeString(argv[iArg]));
                    for (int i = 0; i < aosTokens.size(); i++)
                    {
                        char *endptr = nullptr;
                        sOptions.adfBurnValues.push_back(
                            CPLStrtodM(aosTokens[i], &endptr));
                        if (endptr != aosTokens[i] + strlen(aosTokens[i]))
                        {
                            fprintf(stderr, "Invalid value for -burn\n");
                            CSLDestroy(argv);
                            GDALExit(1);
                        }
                    }
                }
                else
                {
                    char *endptr = nullptr;
                    sOptions.adfBurnValues.push_back(
                        CPLStrtodM(argv[iArg], &endptr));
                    if (endptr != argv[iArg] + strlen(argv[iArg]))
                    {
                        fprintf(stderr, "Invalid value for -burn\n");
                        CSLDestroy(argv);
                        GDALExit(1);
                    }
                }
                if (iArg + 1 < argc &&
                    CPLGetValueType(argv[iArg + 1]) != CPL_VALUE_STRING)
                {
                    ++iArg;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            aosArgv.AddString(argv[iArg]);
        }
    }
    CSLDestroy(argv);

    try
    {

        auto argParser = GDALCreateAppOptionsGetParser(&sOptions);
        argParser->parse_args_without_binary_name(aosArgv.List());
    }
    catch (const std::exception &error)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
        GDALExit(1);
    }

    double adfGeoTransform[6] = {0, 1, 0, 0, 0, 1};
    if (sOptions.bGeoTransform && sOptions.nPixels > 0 && sOptions.nLines > 0)
    {
        adfGeoTransform[0] = sOptions.dfULX;
        adfGeoTransform[1] =
            (sOptions.dfLRX - sOptions.dfULX) / sOptions.nPixels;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = sOptions.dfULY;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] =
            (sOptions.dfLRY - sOptions.dfULY) / sOptions.nLines;
    }

    std::unique_ptr<GDALDataset> poInputDS;
    if (!sOptions.osInputFilename.empty())
    {
        poInputDS.reset(
            GDALDataset::Open(sOptions.osInputFilename.c_str(),
                              GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
        if (poInputDS == nullptr)
        {
            GDALExit(1);
        }
        if (sOptions.nPixels == 0)
        {
            sOptions.nPixels = poInputDS->GetRasterXSize();
            sOptions.nLines = poInputDS->GetRasterYSize();
        }
        if (sOptions.nBandCount < 0)
        {
            sOptions.nBandCount = poInputDS->GetRasterCount();
        }
        if (sOptions.eDT == GDT_Unknown && poInputDS->GetRasterCount() > 0)
        {
            sOptions.eDT = poInputDS->GetRasterBand(1)->GetRasterDataType();
        }
        if (sOptions.osOutputSRS.empty())
        {
            sOptions.osOutputSRS = poInputDS->GetProjectionRef();
        }
        if (!(sOptions.bGeoTransform && sOptions.nPixels > 0 &&
              sOptions.nLines > 0))
        {
            if (poInputDS->GetGeoTransform(adfGeoTransform) == CE_None)
            {
                sOptions.bGeoTransform = true;
            }
        }
        if (!sOptions.bSetNoData && poInputDS->GetRasterCount() > 0)
        {
            if (sOptions.eDT == GDT_Int64)
            {
                int noData;
                const auto nNoDataValue =
                    poInputDS->GetRasterBand(1)->GetNoDataValueAsInt64(&noData);
                sOptions.bSetNoData = noData;
                if (sOptions.bSetNoData)
                    sOptions.osNoData = CPLSPrintf(
                        CPL_FRMT_GIB, static_cast<GIntBig>(nNoDataValue));
            }
            else if (sOptions.eDT == GDT_UInt64)
            {
                int noData;
                const auto nNoDataValue =
                    poInputDS->GetRasterBand(1)->GetNoDataValueAsUInt64(
                        &noData);
                sOptions.bSetNoData = noData;
                if (sOptions.bSetNoData)
                    sOptions.osNoData = CPLSPrintf(
                        CPL_FRMT_GUIB, static_cast<GUIntBig>(nNoDataValue));
            }
            else
            {
                int noData;
                const double dfNoDataValue =
                    poInputDS->GetRasterBand(1)->GetNoDataValue(&noData);
                sOptions.bSetNoData = noData;
                if (sOptions.bSetNoData)
                    sOptions.osNoData = CPLSPrintf("%.18g", dfNoDataValue);
            }
        }
    }

    GDALDriverH hDriver = GDALGetDriverByName(
        sOptions.osFormat.empty()
            ? GetOutputDriverForRaster(sOptions.osOutputFilename.c_str())
                  .c_str()
            : sOptions.osFormat.c_str());

    if (hDriver == nullptr)
    {
        fprintf(stderr, "Output driver not found.\n");
        GDALExit(1);
    }
    const bool bHasCreate =
        GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) != nullptr;
    if (!bHasCreate &&
        GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) == nullptr)
    {
        fprintf(stderr, "This driver has no creation capabilities.\n");
        GDALExit(1);
    }
    GDALDriverH hTmpDriver = GDALGetDriverByName("MEM");
    if (!bHasCreate && hTmpDriver == nullptr)
    {
        fprintf(stderr, "MEM driver not available.\n");
        GDALExit(1);
    }

    if (sOptions.nPixels != 0 && sOptions.eDT == GDT_Unknown)
    {
        sOptions.eDT = GDT_Byte;
    }
    if (sOptions.nBandCount < 0)
    {
        sOptions.nBandCount = sOptions.eDT == GDT_Unknown ? 0 : 1;
    }
    GDALDatasetH hDS = GDALCreate(
        bHasCreate ? hDriver : hTmpDriver, sOptions.osOutputFilename.c_str(),
        sOptions.nPixels, sOptions.nLines, sOptions.nBandCount, sOptions.eDT,
        bHasCreate ? sOptions.aosCreateOptions.List() : nullptr);

    if (hDS == nullptr)
    {
        GDALExit(1);
    }

    if (!sOptions.osOutputSRS.empty() &&
        !EQUAL(sOptions.osOutputSRS.c_str(), "NONE"))
    {
        OGRSpatialReference oSRS;
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        if (oSRS.SetFromUserInput(sOptions.osOutputSRS.c_str()) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to process SRS definition: %s",
                     sOptions.osOutputSRS.c_str());
            GDALExit(1);
        }

        char *pszSRS = nullptr;
        oSRS.exportToWkt(&pszSRS);

        if (GDALSetProjection(hDS, pszSRS) != CE_None)
        {
            CPLFree(pszSRS);
            GDALClose(hDS);
            GDALExit(1);
        }
        CPLFree(pszSRS);
    }
    if (sOptions.bGeoTransform)
    {
        if (sOptions.nPixels == 0)
        {
            fprintf(stderr,
                    "-outsize must be specified when -a_ullr is used.\n");
            GDALClose(hDS);
            GDALExit(1);
        }
        if (GDALSetGeoTransform(hDS, adfGeoTransform) != CE_None)
        {
            GDALClose(hDS);
            GDALExit(1);
        }
    }
    else if (poInputDS && poInputDS->GetGCPCount() > 0)
    {
        GDALDataset::FromHandle(hDS)->SetGCPs(poInputDS->GetGCPCount(),
                                              poInputDS->GetGCPs(),
                                              poInputDS->GetGCPSpatialRef());
    }

    if (!sOptions.aosMetadata.empty())
    {
        GDALSetMetadata(hDS, sOptions.aosMetadata.List(), nullptr);
    }
    const int nBands = GDALGetRasterCount(hDS);
    if (sOptions.bSetNoData)
    {
        for (int i = 0; i < nBands; i++)
        {
            auto hBand = GDALGetRasterBand(hDS, i + 1);
            if (sOptions.eDT == GDT_Int64)
            {
                GDALSetRasterNoDataValueAsInt64(
                    hBand, static_cast<int64_t>(std::strtoll(
                               sOptions.osNoData.c_str(), nullptr, 10)));
            }
            else if (sOptions.eDT == GDT_UInt64)
            {
                GDALSetRasterNoDataValueAsUInt64(
                    hBand, static_cast<uint64_t>(std::strtoull(
                               sOptions.osNoData.c_str(), nullptr, 10)));
            }
            else
            {
                GDALSetRasterNoDataValue(hBand,
                                         CPLAtofM(sOptions.osNoData.c_str()));
            }
        }
    }
    if (!sOptions.adfBurnValues.empty())
    {
        for (int i = 0; i < nBands; i++)
        {
            GDALFillRaster(GDALGetRasterBand(hDS, i + 1),
                           i < static_cast<int>(sOptions.adfBurnValues.size())
                               ? sOptions.adfBurnValues[i]
                               : sOptions.adfBurnValues.back(),
                           0);
        }
    }

    bool bHasGotErr = false;
    if (!bHasCreate)
    {
        GDALDatasetH hOutDS = GDALCreateCopy(
            hDriver, sOptions.osOutputFilename.c_str(), hDS, false,
            sOptions.aosCreateOptions.List(),
            sOptions.bQuiet ? GDALDummyProgress : GDALTermProgress, nullptr);
        if (hOutDS == nullptr)
        {
            GDALClose(hDS);
            GDALExit(1);
        }
        if (GDALClose(hOutDS) != CE_None)
        {
            bHasGotErr = true;
        }
    }

    const bool bWasFailureBefore = (CPLGetLastErrorType() == CE_Failure);
    if (GDALClose(hDS) != CE_None)
        bHasGotErr = true;
    if (!bWasFailureBefore && CPLGetLastErrorType() == CE_Failure)
    {
        bHasGotErr = true;
    }

    return bHasGotErr ? 1 : 0;
}

MAIN_END

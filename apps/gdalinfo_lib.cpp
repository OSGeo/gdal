/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to list info about a file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2015, Faza Mahamood
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "gdalargumentparser.h"

#include <cmath>
#include <limits>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <string>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_json_header.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonwriter.h"

using std::vector;

/*! output format */
typedef enum
{
    /*! output in text format */ GDALINFO_FORMAT_TEXT = 0,
    /*! output in json format */ GDALINFO_FORMAT_JSON = 1
} GDALInfoFormat;

/************************************************************************/
/*                           GDALInfoOptions                            */
/************************************************************************/

/** Options for use with GDALInfo(). GDALInfoOptions* must be allocated and
 * freed with GDALInfoOptionsNew() and GDALInfoOptionsFree() respectively.
 */
struct GDALInfoOptions
{
    /*! output format */
    GDALInfoFormat eFormat = GDALINFO_FORMAT_TEXT;

    bool bComputeMinMax = false;

    /*! report histogram information for all bands */
    bool bReportHistograms = false;

    /*! report a PROJ.4 string corresponding to the file's coordinate system */
    bool bReportProj4 = false;

    /*! read and display image statistics. Force computation if no statistics
        are stored in an image */
    bool bStats = false;

    /*! read and display image statistics. Force computation if no statistics
        are stored in an image.  However, they may be computed based on
        overviews or a subset of all tiles. Useful if you are in a hurry and
        don't want precise stats. */
    bool bApproxStats = true;

    bool bSample = false;

    /*! force computation of the checksum for each band in the dataset */
    bool bComputeChecksum = false;

    /*! allow or suppress ground control points list printing. It may be useful
        for datasets with huge amount of GCPs, such as L1B AVHRR or HDF4 MODIS
        which contain thousands of them. */
    bool bShowGCPs = true;

    /*! allow or suppress metadata printing. Some datasets may contain a lot of
        metadata strings. */
    bool bShowMetadata = true;

    /*! allow or suppress printing of raster attribute table */
    bool bShowRAT = true;

    /*! allow or suppress printing of color table */
    bool bShowColorTable = true;

    /*! list all metadata domains available for the dataset */
    bool bListMDD = false;

    /*! display the file list or the first file of the file list */
    bool bShowFileList = true;

    /*! report metadata for the specified domains. "all" can be used to report
        metadata in all domains.
        */
    CPLStringList aosExtraMDDomains;

    /*! WKT format used for SRS */
    std::string osWKTFormat = "WKT2";

    bool bStdoutOutput = false;
};

static int GDALInfoReportCorner(const GDALInfoOptions *psOptions,
                                GDALDatasetH hDataset,
                                OGRCoordinateTransformationH hTransform,
                                const char *corner_name, double x, double y,
                                bool bJson, json_object *poCornerCoordinates,
                                json_object *poLongLatExtentCoordinates,
                                CPLString &osStr);

static void GDALInfoReportMetadata(const GDALInfoOptions *psOptions,
                                   GDALMajorObjectH hObject, bool bIsBand,
                                   bool bJson, json_object *poMetadata,
                                   CPLString &osStr);

#ifndef Concat_defined
#define Concat_defined
static void Concat(CPLString &osRet, bool bStdoutOutput, const char *pszFormat,
                   ...) CPL_PRINT_FUNC_FORMAT(3, 4);

static void Concat(CPLString &osRet, bool bStdoutOutput, const char *pszFormat,
                   ...)
{
    va_list args;
    va_start(args, pszFormat);

    if (bStdoutOutput)
    {
        vfprintf(stdout, pszFormat, args);
    }
    else
    {
        try
        {
            CPLString osTarget;
            osTarget.vPrintf(pszFormat, args);

            osRet += osTarget;
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        }
    }

    va_end(args);
}
#endif

/************************************************************************/
/*           gdal_json_object_new_double_or_str_for_non_finite()        */
/************************************************************************/

static json_object *
gdal_json_object_new_double_or_str_for_non_finite(double dfVal, int nPrecision)
{
    if (std::isinf(dfVal))
        return json_object_new_string(dfVal < 0 ? "-Infinity" : "Infinity");
    else if (std::isnan(dfVal))
        return json_object_new_string("NaN");
    else
        return json_object_new_double_with_precision(dfVal, nPrecision);
}

/************************************************************************/
/*           gdal_json_object_new_double_significant_digits()           */
/************************************************************************/

static json_object *
gdal_json_object_new_double_significant_digits(double dfVal,
                                               int nSignificantDigits)
{
    if (std::isinf(dfVal))
        return json_object_new_string(dfVal < 0 ? "-Infinity" : "Infinity");
    else if (std::isnan(dfVal))
        return json_object_new_string("NaN");
    else
        return json_object_new_double_with_significant_figures(
            dfVal, nSignificantDigits);
}

/************************************************************************/
/*                     GDALWarpAppOptionsGetParser()                    */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALInfoAppOptionsGetParser(GDALInfoOptions *psOptions,
                            GDALInfoOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdalinfo", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(_("Raster dataset information utility."));

    argParser->add_epilog(
        _("For more details, consult https://gdal.org/programs/gdalinfo.html"));

    argParser->add_argument("-json")
        .flag()
        .action([psOptions](const auto &)
                { psOptions->eFormat = GDALINFO_FORMAT_JSON; })
        .help(_("Display the output in json format."));

    argParser->add_argument("-mm")
        .store_into(psOptions->bComputeMinMax)
        .help(_("Force computation of the actual min/max values for each band "
                "in the dataset."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-stats")
            .store_into(psOptions->bStats)
            .help(_("Read and display image statistics computing exact values "
                    "if required."));

        group.add_argument("-approx_stats")
            .store_into(psOptions->bApproxStats)
            .help(
                _("Read and display image statistics computing approximated "
                  "values on overviews or a subset of all tiles if required."));
    }

    argParser->add_argument("-hist")
        .store_into(psOptions->bReportHistograms)
        .help(_("Report histogram information for all bands."));

    argParser->add_inverted_logic_flag(
        "-nogcp", &psOptions->bShowGCPs,
        _("Suppress ground control points list printing."));

    argParser->add_inverted_logic_flag("-nomd", &psOptions->bShowMetadata,
                                       _("Suppress metadata printing."));

    argParser->add_inverted_logic_flag(
        "-norat", &psOptions->bShowRAT,
        _("Suppress printing of raster attribute table."));

    argParser->add_inverted_logic_flag("-noct", &psOptions->bShowColorTable,
                                       _("Suppress printing of color table."));

    argParser->add_inverted_logic_flag("-nofl", &psOptions->bShowFileList,
                                       _("Suppress display of the file list."));

    argParser->add_argument("-checksum")
        .flag()
        .store_into(psOptions->bComputeChecksum)
        .help(_(
            "Force computation of the checksum for each band in the dataset."));

    argParser->add_argument("-listmdd")
        .flag()
        .store_into(psOptions->bListMDD)
        .help(_("List all metadata domains available for the dataset."));

    argParser->add_argument("-proj4")
        .flag()
        .store_into(psOptions->bReportProj4)
        .help(_("Report a PROJ.4 string corresponding to the file's coordinate "
                "system."));

    argParser->add_argument("-wkt_format")
        .metavar("<WKT1|WKT2|WKT2_2015|WKT2_2018|WKT2_2019>")
        .choices("WKT1", "WKT2", "WKT2_2015", "WKT2_2018", "WKT2_2019")
        .store_into(psOptions->osWKTFormat)
        .help(_("WKT format used for SRS."));

    if (psOptionsForBinary)
    {
        argParser->add_argument("-sd")
            .metavar("<n>")
            .store_into(psOptionsForBinary->nSubdataset)
            .help(_(
                "Use subdataset of specified index (starting at 1), instead of "
                "the source dataset itself."));
    }

    argParser->add_argument("-oo")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action(
            [psOptionsForBinary](const std::string &s)
            {
                if (psOptionsForBinary)
                    psOptionsForBinary->aosOpenOptions.AddString(s.c_str());
            })
        .help(_("Open option(s) for dataset."));

    argParser->add_input_format_argument(
        psOptionsForBinary ? &psOptionsForBinary->aosAllowedInputDrivers
                           : nullptr);

    argParser->add_argument("-mdd")
        .metavar("<domain>|all")
        .action(
            [psOptions](const std::string &value)
            {
                psOptions->aosExtraMDDomains =
                    CSLAddString(psOptions->aosExtraMDDomains, value.c_str());
            })
        .help(_("Report metadata for the specified domains. 'all' can be used "
                "to report metadata in all domains."));

    /* Not documented: used by gdalinfo_bin.cpp only */
    argParser->add_argument("-stdout").flag().hidden().store_into(
        psOptions->bStdoutOutput);

    if (psOptionsForBinary)
    {
        argParser->add_argument("dataset_name")
            .metavar("<dataset_name>")
            .store_into(psOptionsForBinary->osFilename)
            .help("Input dataset.");
    }

    return argParser;
}

/************************************************************************/
/*                       GDALInfoAppGetParserUsage()                    */
/************************************************************************/

std::string GDALInfoAppGetParserUsage()
{
    try
    {
        GDALInfoOptions sOptions;
        GDALInfoOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALInfoAppOptionsGetParser(&sOptions, &sOptionsForBinary);
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
/*                             GDALInfo()                               */
/************************************************************************/

/**
 * Lists various information about a GDAL supported raster dataset.
 *
 * This is the equivalent of the <a href="/programs/gdalinfo.html">gdalinfo</a>
 * utility.
 *
 * GDALInfoOptions* must be allocated and freed with GDALInfoOptionsNew()
 * and GDALInfoOptionsFree() respectively.
 *
 * @param hDataset the dataset handle.
 * @param psOptions the options structure returned by GDALInfoOptionsNew() or
 * NULL.
 * @return string corresponding to the information about the raster dataset
 * (must be freed with CPLFree()), or NULL in case of error.
 *
 * @since GDAL 2.1
 */

char *GDALInfo(GDALDatasetH hDataset, const GDALInfoOptions *psOptions)
{
    if (hDataset == nullptr)
        return nullptr;

    GDALInfoOptions *psOptionsToFree = nullptr;
    if (psOptions == nullptr)
    {
        psOptionsToFree = GDALInfoOptionsNew(nullptr, nullptr);
        psOptions = psOptionsToFree;
    }

    CPLString osStr;
    json_object *poJsonObject = nullptr;
    json_object *poBands = nullptr;
    json_object *poMetadata = nullptr;
    json_object *poStac = nullptr;
    json_object *poStacRasterBands = nullptr;
    json_object *poStacEOBands = nullptr;

    const bool bJson = psOptions->eFormat == GDALINFO_FORMAT_JSON;

    /* -------------------------------------------------------------------- */
    /*      Report general info.                                            */
    /* -------------------------------------------------------------------- */
    GDALDriverH hDriver = GDALGetDatasetDriver(hDataset);
    if (bJson)
    {
        json_object *poDescription =
            json_object_new_string(GDALGetDescription(hDataset));
        json_object *poDriverShortName =
            json_object_new_string(GDALGetDriverShortName(hDriver));
        json_object *poDriverLongName =
            json_object_new_string(GDALGetDriverLongName(hDriver));
        poJsonObject = json_object_new_object();
        poBands = json_object_new_array();
        poMetadata = json_object_new_object();
        poStac = json_object_new_object();
        poStacRasterBands = json_object_new_array();
        poStacEOBands = json_object_new_array();

        json_object_object_add(poJsonObject, "description", poDescription);
        json_object_object_add(poJsonObject, "driverShortName",
                               poDriverShortName);
        json_object_object_add(poJsonObject, "driverLongName",
                               poDriverLongName);
    }
    else
    {
        Concat(osStr, psOptions->bStdoutOutput, "Driver: %s/%s\n",
               GDALGetDriverShortName(hDriver), GDALGetDriverLongName(hDriver));
    }

    if (psOptions->bShowFileList)
    {
        // The list of files of a raster FileGDB is not super useful and potentially
        // super long, so omit it, unless the -json mode is enabled
        char **papszFileList =
            (!bJson && EQUAL(GDALGetDriverShortName(hDriver), "OpenFileGDB"))
                ? nullptr
                : GDALGetFileList(hDataset);

        if (!papszFileList || *papszFileList == nullptr)
        {
            if (bJson)
            {
                json_object *poFiles = json_object_new_array();
                json_object_object_add(poJsonObject, "files", poFiles);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "Files: none associated\n");
            }
        }
        else
        {
            if (bJson)
            {
                json_object *poFiles = json_object_new_array();

                for (int i = 0; papszFileList[i] != nullptr; i++)
                {
                    json_object *poFile =
                        json_object_new_string(papszFileList[i]);

                    json_object_array_add(poFiles, poFile);
                }

                json_object_object_add(poJsonObject, "files", poFiles);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput, "Files: %s\n",
                       papszFileList[0]);
                for (int i = 1; papszFileList[i] != nullptr; i++)
                    Concat(osStr, psOptions->bStdoutOutput, "       %s\n",
                           papszFileList[i]);
            }
        }
        CSLDestroy(papszFileList);
    }

    if (bJson)
    {
        {
            json_object *poSize = json_object_new_array();
            json_object *poSizeX =
                json_object_new_int(GDALGetRasterXSize(hDataset));
            json_object *poSizeY =
                json_object_new_int(GDALGetRasterYSize(hDataset));

            // size is X, Y ordered
            json_object_array_add(poSize, poSizeX);
            json_object_array_add(poSize, poSizeY);

            json_object_object_add(poJsonObject, "size", poSize);
        }

        {
            json_object *poStacSize = json_object_new_array();
            json_object *poSizeX =
                json_object_new_int(GDALGetRasterXSize(hDataset));
            json_object *poSizeY =
                json_object_new_int(GDALGetRasterYSize(hDataset));

            // ... but ... proj:shape is Y, X ordered.
            json_object_array_add(poStacSize, poSizeY);
            json_object_array_add(poStacSize, poSizeX);

            json_object_object_add(poStac, "proj:shape", poStacSize);
        }
    }
    else
    {
        Concat(osStr, psOptions->bStdoutOutput, "Size is %d, %d\n",
               GDALGetRasterXSize(hDataset), GDALGetRasterYSize(hDataset));
    }

    CPLString osWKTFormat("FORMAT=");
    osWKTFormat += psOptions->osWKTFormat;
    const char *const apszWKTOptions[] = {osWKTFormat.c_str(), "MULTILINE=YES",
                                          nullptr};

    /* -------------------------------------------------------------------- */
    /*      Report projection.                                              */
    /* -------------------------------------------------------------------- */
    auto hSRS = GDALGetSpatialRef(hDataset);
    if (hSRS != nullptr)
    {
        json_object *poCoordinateSystem = nullptr;

        if (bJson)
            poCoordinateSystem = json_object_new_object();

        char *pszPrettyWkt = nullptr;

        OSRExportToWktEx(hSRS, &pszPrettyWkt, apszWKTOptions);

        int nAxesCount = 0;
        const int *panAxes = OSRGetDataAxisToSRSAxisMapping(hSRS, &nAxesCount);

        const double dfCoordinateEpoch = OSRGetCoordinateEpoch(hSRS);

        if (bJson)
        {
            json_object *poWkt = json_object_new_string(pszPrettyWkt);
            if (psOptions->osWKTFormat == "WKT2")
            {
                json_object *poStacWkt = nullptr;
                json_object_deep_copy(poWkt, &poStacWkt, nullptr);
                json_object_object_add(poStac, "proj:wkt2", poStacWkt);
            }
            json_object_object_add(poCoordinateSystem, "wkt", poWkt);

            const char *pszAuthCode = OSRGetAuthorityCode(hSRS, nullptr);
            const char *pszAuthName = OSRGetAuthorityName(hSRS, nullptr);
            if (pszAuthCode && pszAuthName && EQUAL(pszAuthName, "EPSG"))
            {
                json_object *poEPSG = json_object_new_int64(atoi(pszAuthCode));
                json_object_object_add(poStac, "proj:epsg", poEPSG);
            }
            else
            {
                // Setting it to null is mandated by the
                // https://github.com/stac-extensions/projection#projepsg
                // when setting proj:projjson or proj:wkt2
                json_object_object_add(poStac, "proj:epsg", nullptr);
            }
            {
                // PROJJSON requires PROJ >= 6.2
                CPLErrorStateBackuper oCPLErrorHandlerPusher(
                    CPLQuietErrorHandler);
                char *pszProjJson = nullptr;
                OGRErr result =
                    OSRExportToPROJJSON(hSRS, &pszProjJson, nullptr);
                if (result == OGRERR_NONE)
                {
                    json_object *poStacProjJson =
                        json_tokener_parse(pszProjJson);
                    json_object_object_add(poStac, "proj:projjson",
                                           poStacProjJson);
                    CPLFree(pszProjJson);
                }
            }

            json_object *poAxisMapping = json_object_new_array();
            for (int i = 0; i < nAxesCount; i++)
            {
                json_object_array_add(poAxisMapping,
                                      json_object_new_int(panAxes[i]));
            }
            json_object_object_add(poCoordinateSystem,
                                   "dataAxisToSRSAxisMapping", poAxisMapping);

            if (dfCoordinateEpoch > 0)
            {
                json_object_object_add(
                    poJsonObject, "coordinateEpoch",
                    json_object_new_double(dfCoordinateEpoch));
            }
        }
        else
        {
            Concat(osStr, psOptions->bStdoutOutput,
                   "Coordinate System is:\n%s\n", pszPrettyWkt);

            Concat(osStr, psOptions->bStdoutOutput,
                   "Data axis to CRS axis mapping: ");
            for (int i = 0; i < nAxesCount; i++)
            {
                if (i > 0)
                {
                    Concat(osStr, psOptions->bStdoutOutput, ",");
                }
                Concat(osStr, psOptions->bStdoutOutput, "%d", panAxes[i]);
            }
            Concat(osStr, psOptions->bStdoutOutput, "\n");

            if (dfCoordinateEpoch > 0)
            {
                std::string osCoordinateEpoch =
                    CPLSPrintf("%f", dfCoordinateEpoch);
                const size_t nDotPos = osCoordinateEpoch.find('.');
                if (nDotPos != std::string::npos)
                {
                    while (osCoordinateEpoch.size() > nDotPos + 2 &&
                           osCoordinateEpoch.back() == '0')
                        osCoordinateEpoch.pop_back();
                }
                Concat(osStr, psOptions->bStdoutOutput,
                       "Coordinate epoch: %s\n", osCoordinateEpoch.c_str());
            }
        }
        CPLFree(pszPrettyWkt);

        if (psOptions->bReportProj4)
        {
            char *pszProj4 = nullptr;
            OSRExportToProj4(hSRS, &pszProj4);

            if (bJson)
            {
                json_object *proj4 = json_object_new_string(pszProj4);
                json_object_object_add(poCoordinateSystem, "proj4", proj4);
            }
            else
                Concat(osStr, psOptions->bStdoutOutput,
                       "PROJ.4 string is:\n\'%s\'\n", pszProj4);
            CPLFree(pszProj4);
        }

        if (bJson)
            json_object_object_add(poJsonObject, "coordinateSystem",
                                   poCoordinateSystem);
    }

    /* -------------------------------------------------------------------- */
    /*      Report Geotransform.                                            */
    /* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    if (GDALGetGeoTransform(hDataset, adfGeoTransform) == CE_None)
    {
        if (bJson)
        {
            json_object *poGeoTransform = json_object_new_array();
            // Deep copy wasn't working on the array, for some reason, so we
            // build the geotransform STAC array at the same time.
            json_object *poStacGeoTransform = json_object_new_array();

            for (int i = 0; i < 6; i++)
            {
                json_object *poGeoTransformCoefficient =
                    json_object_new_double_with_precision(adfGeoTransform[i],
                                                          16);
                json_object *poStacGeoTransformCoefficient =
                    json_object_new_double_with_precision(adfGeoTransform[i],
                                                          16);

                json_object_array_add(poGeoTransform,
                                      poGeoTransformCoefficient);
                json_object_array_add(poStacGeoTransform,
                                      poStacGeoTransformCoefficient);
            }

            json_object_object_add(poJsonObject, "geoTransform",
                                   poGeoTransform);
            json_object_object_add(poStac, "proj:transform",
                                   poStacGeoTransform);
        }
        else
        {
            if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0)
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "Origin = (%.15f,%.15f)\n", adfGeoTransform[0],
                       adfGeoTransform[3]);

                Concat(osStr, psOptions->bStdoutOutput,
                       "Pixel Size = (%.15f,%.15f)\n", adfGeoTransform[1],
                       adfGeoTransform[5]);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "GeoTransform =\n"
                       "  %.16g, %.16g, %.16g\n"
                       "  %.16g, %.16g, %.16g\n",
                       adfGeoTransform[0], adfGeoTransform[1],
                       adfGeoTransform[2], adfGeoTransform[3],
                       adfGeoTransform[4], adfGeoTransform[5]);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Report GCPs.                                                    */
    /* -------------------------------------------------------------------- */
    if (psOptions->bShowGCPs && GDALGetGCPCount(hDataset) > 0)
    {
        json_object *const poGCPs = bJson ? json_object_new_object() : nullptr;

        hSRS = GDALGetGCPSpatialRef(hDataset);
        if (hSRS)
        {
            json_object *poGCPCoordinateSystem = nullptr;

            char *pszPrettyWkt = nullptr;

            int nAxesCount = 0;
            const int *panAxes =
                OSRGetDataAxisToSRSAxisMapping(hSRS, &nAxesCount);

            OSRExportToWktEx(hSRS, &pszPrettyWkt, apszWKTOptions);

            if (bJson)
            {
                json_object *poWkt = json_object_new_string(pszPrettyWkt);
                poGCPCoordinateSystem = json_object_new_object();

                json_object_object_add(poGCPCoordinateSystem, "wkt", poWkt);

                json_object *poAxisMapping = json_object_new_array();
                for (int i = 0; i < nAxesCount; i++)
                {
                    json_object_array_add(poAxisMapping,
                                          json_object_new_int(panAxes[i]));
                }
                json_object_object_add(poGCPCoordinateSystem,
                                       "dataAxisToSRSAxisMapping",
                                       poAxisMapping);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "GCP Projection = \n%s\n", pszPrettyWkt);

                Concat(osStr, psOptions->bStdoutOutput,
                       "Data axis to CRS axis mapping: ");
                for (int i = 0; i < nAxesCount; i++)
                {
                    if (i > 0)
                    {
                        Concat(osStr, psOptions->bStdoutOutput, ",");
                    }
                    Concat(osStr, psOptions->bStdoutOutput, "%d", panAxes[i]);
                }
                Concat(osStr, psOptions->bStdoutOutput, "\n");
            }
            CPLFree(pszPrettyWkt);

            if (bJson)
                json_object_object_add(poGCPs, "coordinateSystem",
                                       poGCPCoordinateSystem);
        }

        json_object *const poGCPList =
            bJson ? json_object_new_array() : nullptr;

        for (int i = 0; i < GDALGetGCPCount(hDataset); i++)
        {
            const GDAL_GCP *psGCP = GDALGetGCPs(hDataset) + i;
            if (bJson)
            {
                json_object *poGCP = json_object_new_object();
                json_object *poId = json_object_new_string(psGCP->pszId);
                json_object *poInfo = json_object_new_string(psGCP->pszInfo);
                json_object *poPixel = json_object_new_double_with_precision(
                    psGCP->dfGCPPixel, 15);
                json_object *poLine =
                    json_object_new_double_with_precision(psGCP->dfGCPLine, 15);
                json_object *poX =
                    json_object_new_double_with_precision(psGCP->dfGCPX, 15);
                json_object *poY =
                    json_object_new_double_with_precision(psGCP->dfGCPY, 15);
                json_object *poZ =
                    json_object_new_double_with_precision(psGCP->dfGCPZ, 15);

                json_object_object_add(poGCP, "id", poId);
                json_object_object_add(poGCP, "info", poInfo);
                json_object_object_add(poGCP, "pixel", poPixel);
                json_object_object_add(poGCP, "line", poLine);
                json_object_object_add(poGCP, "x", poX);
                json_object_object_add(poGCP, "y", poY);
                json_object_object_add(poGCP, "z", poZ);
                json_object_array_add(poGCPList, poGCP);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "GCP[%3d]: Id=%s, Info=%s\n"
                       "          (%.15g,%.15g) -> (%.15g,%.15g,%.15g)\n",
                       i, psGCP->pszId, psGCP->pszInfo, psGCP->dfGCPPixel,
                       psGCP->dfGCPLine, psGCP->dfGCPX, psGCP->dfGCPY,
                       psGCP->dfGCPZ);
            }
        }
        if (bJson)
        {
            json_object_object_add(poGCPs, "gcpList", poGCPList);
            json_object_object_add(poJsonObject, "gcps", poGCPs);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Report metadata.                                                */
    /* -------------------------------------------------------------------- */

    GDALInfoReportMetadata(psOptions, hDataset, false, bJson, poMetadata,
                           osStr);
    if (bJson)
    {
        if (psOptions->bShowMetadata)
            json_object_object_add(poJsonObject, "metadata", poMetadata);
        else
            json_object_put(poMetadata);

        // Include eo:cloud_cover in stac output
        const char *pszCloudCover =
            GDALGetMetadataItem(hDataset, "CLOUDCOVER", "IMAGERY");
        json_object *poValue = nullptr;
        if (pszCloudCover)
        {
            poValue = json_object_new_int(atoi(pszCloudCover));
            json_object_object_add(poStac, "eo:cloud_cover", poValue);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Setup projected to lat/long transform if appropriate.           */
    /* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hProj = nullptr;
    if (GDALGetGeoTransform(hDataset, adfGeoTransform) == CE_None)
        hProj = GDALGetSpatialRef(hDataset);

    OGRCoordinateTransformationH hTransform = nullptr;
    bool bTransformToWGS84 = false;

    if (hProj)
    {
        OGRSpatialReferenceH hLatLong = nullptr;

        if (bJson)
        {
            // Check that it looks like Earth before trying to reproject to wgs84...
            // OSRGetSemiMajor() may raise an error on CRS like Engineering CRS
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            OGRErr eErr = OGRERR_NONE;
            if (fabs(OSRGetSemiMajor(hProj, &eErr) - 6378137.0) < 10000.0 &&
                eErr == OGRERR_NONE)
            {
                bTransformToWGS84 = true;
                hLatLong = OSRNewSpatialReference(nullptr);
                OSRSetWellKnownGeogCS(hLatLong, "WGS84");
            }
        }
        else
        {
            hLatLong = OSRCloneGeogCS(hProj);
            if (hLatLong)
            {
                // Override GEOGCS|UNIT child to be sure to output as degrees
                OSRSetAngularUnits(hLatLong, SRS_UA_DEGREE,
                                   CPLAtof(SRS_UA_DEGREE_CONV));
            }
        }

        if (hLatLong != nullptr)
        {
            OSRSetAxisMappingStrategy(hLatLong, OAMS_TRADITIONAL_GIS_ORDER);
            CPLPushErrorHandler(CPLQuietErrorHandler);
            hTransform = OCTNewCoordinateTransformation(hProj, hLatLong);
            CPLPopErrorHandler();

            OSRDestroySpatialReference(hLatLong);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Report corners.                                                 */
    /* -------------------------------------------------------------------- */
    if (bJson && GDALGetRasterXSize(hDataset))
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);

        json_object *poLinearRing = json_object_new_array();
        json_object *poCornerCoordinates = json_object_new_object();
        json_object *poLongLatExtent = json_object_new_object();
        json_object *poLongLatExtentType = json_object_new_string("Polygon");
        json_object *poLongLatExtentCoordinates = json_object_new_array();

        GDALInfoReportCorner(psOptions, hDataset, hTransform, "upperLeft", 0.0,
                             0.0, bJson, poCornerCoordinates,
                             poLongLatExtentCoordinates, osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "lowerLeft", 0.0,
                             GDALGetRasterYSize(hDataset), bJson,
                             poCornerCoordinates, poLongLatExtentCoordinates,
                             osStr);
        GDALInfoReportCorner(
            psOptions, hDataset, hTransform, "lowerRight",
            GDALGetRasterXSize(hDataset), GDALGetRasterYSize(hDataset), bJson,
            poCornerCoordinates, poLongLatExtentCoordinates, osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "upperRight",
                             GDALGetRasterXSize(hDataset), 0.0, bJson,
                             poCornerCoordinates, poLongLatExtentCoordinates,
                             osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "center",
                             GDALGetRasterXSize(hDataset) / 2.0,
                             GDALGetRasterYSize(hDataset) / 2.0, bJson,
                             poCornerCoordinates, poLongLatExtentCoordinates,
                             osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "upperLeft", 0.0,
                             0.0, bJson, poCornerCoordinates,
                             poLongLatExtentCoordinates, osStr);

        json_object_object_add(poJsonObject, "cornerCoordinates",
                               poCornerCoordinates);
        json_object_object_add(poLongLatExtent, "type", poLongLatExtentType);
        json_object_array_add(poLinearRing, poLongLatExtentCoordinates);
        json_object_object_add(poLongLatExtent, "coordinates", poLinearRing);
        json_object_object_add(poJsonObject,
                               bTransformToWGS84 ? "wgs84Extent" : "extent",
                               poLongLatExtent);
    }
    else if (GDALGetRasterXSize(hDataset))
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);

        Concat(osStr, psOptions->bStdoutOutput, "Corner Coordinates:\n");
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "Upper Left", 0.0,
                             0.0, bJson, nullptr, nullptr, osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "Lower Left", 0.0,
                             GDALGetRasterYSize(hDataset), bJson, nullptr,
                             nullptr, osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "Upper Right",
                             GDALGetRasterXSize(hDataset), 0.0, bJson, nullptr,
                             nullptr, osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "Lower Right",
                             GDALGetRasterXSize(hDataset),
                             GDALGetRasterYSize(hDataset), bJson, nullptr,
                             nullptr, osStr);
        GDALInfoReportCorner(psOptions, hDataset, hTransform, "Center",
                             GDALGetRasterXSize(hDataset) / 2.0,
                             GDALGetRasterYSize(hDataset) / 2.0, bJson, nullptr,
                             nullptr, osStr);
    }

    if (hTransform != nullptr)
    {
        OCTDestroyCoordinateTransformation(hTransform);
        hTransform = nullptr;
    }

    /* ==================================================================== */
    /*      Loop over bands.                                                */
    /* ==================================================================== */
    for (int iBand = 0; iBand < GDALGetRasterCount(hDataset); iBand++)
    {
        json_object *poBand = nullptr;
        json_object *poBandMetadata = nullptr;
        json_object *poStacRasterBand = nullptr;
        json_object *poStacEOBand = nullptr;

        if (bJson)
        {
            poBand = json_object_new_object();
            poBandMetadata = json_object_new_object();
            poStacRasterBand = json_object_new_object();
            poStacEOBand = json_object_new_object();
        }

        GDALRasterBandH const hBand = GDALGetRasterBand(hDataset, iBand + 1);
        const auto eDT = GDALGetRasterDataType(hBand);

        if (psOptions->bSample)
        {
            vector<float> ofSample(10000, 0);
            float *const pafSample = &ofSample[0];
            const int nCount =
                GDALGetRandomRasterSample(hBand, 10000, pafSample);
            if (!bJson)
                Concat(osStr, psOptions->bStdoutOutput, "Got %d samples.\n",
                       nCount);
        }

        int nBlockXSize = 0;
        int nBlockYSize = 0;
        GDALGetBlockSize(hBand, &nBlockXSize, &nBlockYSize);
        if (bJson)
        {
            json_object *poBandNumber = json_object_new_int(iBand + 1);
            json_object *poBlock = json_object_new_array();
            json_object *poType =
                json_object_new_string(GDALGetDataTypeName(eDT));
            json_object *poColorInterp =
                json_object_new_string(GDALGetColorInterpretationName(
                    GDALGetRasterColorInterpretation(hBand)));

            json_object_array_add(poBlock, json_object_new_int(nBlockXSize));
            json_object_array_add(poBlock, json_object_new_int(nBlockYSize));
            json_object_object_add(poBand, "band", poBandNumber);
            json_object_object_add(poBand, "block", poBlock);
            json_object_object_add(poBand, "type", poType);
            json_object_object_add(poBand, "colorInterpretation",
                                   poColorInterp);

            const char *stacDataType = nullptr;
            switch (eDT)
            {
                case GDT_Byte:
                    stacDataType = "uint8";
                    break;
                case GDT_Int8:
                    stacDataType = "int8";
                    break;
                case GDT_UInt16:
                    stacDataType = "uint16";
                    break;
                case GDT_Int16:
                    stacDataType = "int16";
                    break;
                case GDT_UInt32:
                    stacDataType = "uint32";
                    break;
                case GDT_Int32:
                    stacDataType = "int32";
                    break;
                case GDT_UInt64:
                    stacDataType = "uint64";
                    break;
                case GDT_Int64:
                    stacDataType = "int64";
                    break;
                case GDT_Float32:
                    stacDataType = "float32";
                    break;
                case GDT_Float64:
                    stacDataType = "float64";
                    break;
                case GDT_CInt16:
                    stacDataType = "cint16";
                    break;
                case GDT_CInt32:
                    stacDataType = "cint32";
                    break;
                case GDT_CFloat32:
                    stacDataType = "cfloat32";
                    break;
                case GDT_CFloat64:
                    stacDataType = "cfloat64";
                    break;
                case GDT_Unknown:
                case GDT_TypeCount:
                    stacDataType = nullptr;
            }
            if (stacDataType)
                json_object_object_add(poStacRasterBand, "data_type",
                                       json_object_new_string(stacDataType));
        }
        else
        {
            Concat(osStr, psOptions->bStdoutOutput,
                   "Band %d Block=%dx%d Type=%s, ColorInterp=%s\n", iBand + 1,
                   nBlockXSize, nBlockYSize, GDALGetDataTypeName(eDT),
                   GDALGetColorInterpretationName(
                       GDALGetRasterColorInterpretation(hBand)));
        }

        if (bJson)
        {
            json_object *poBandName =
                json_object_new_string(CPLSPrintf("b%i", iBand + 1));
            json_object_object_add(poStacEOBand, "name", poBandName);
        }

        if (GDALGetDescription(hBand) != nullptr &&
            strlen(GDALGetDescription(hBand)) > 0)
        {
            if (bJson)
            {
                json_object *poBandDescription =
                    json_object_new_string(GDALGetDescription(hBand));
                json_object_object_add(poBand, "description",
                                       poBandDescription);

                json_object *poStacBandDescription =
                    json_object_new_string(GDALGetDescription(hBand));
                json_object_object_add(poStacEOBand, "description",
                                       poStacBandDescription);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput, "  Description = %s\n",
                       GDALGetDescription(hBand));
            }
        }
        else
        {
            if (bJson)
            {
                json_object *poColorInterp =
                    json_object_new_string(GDALGetColorInterpretationName(
                        GDALGetRasterColorInterpretation(hBand)));
                json_object_object_add(poStacEOBand, "description",
                                       poColorInterp);
            }
        }

        {
            int bGotMin = FALSE;
            int bGotMax = FALSE;
            const double dfMin = GDALGetRasterMinimum(hBand, &bGotMin);
            const double dfMax = GDALGetRasterMaximum(hBand, &bGotMax);
            if (bGotMin || bGotMax || psOptions->bComputeMinMax)
            {
                if (!bJson)
                    Concat(osStr, psOptions->bStdoutOutput, "  ");
                if (bGotMin)
                {
                    if (bJson)
                    {
                        json_object *poMin =
                            gdal_json_object_new_double_or_str_for_non_finite(
                                dfMin, 3);
                        json_object_object_add(poBand, "min", poMin);
                    }
                    else
                    {
                        Concat(osStr, psOptions->bStdoutOutput, "Min=%.3f ",
                               dfMin);
                    }
                }
                if (bGotMax)
                {
                    if (bJson)
                    {
                        json_object *poMax =
                            gdal_json_object_new_double_or_str_for_non_finite(
                                dfMax, 3);
                        json_object_object_add(poBand, "max", poMax);
                    }
                    else
                    {
                        Concat(osStr, psOptions->bStdoutOutput, "Max=%.3f ",
                               dfMax);
                    }
                }

                if (psOptions->bComputeMinMax)
                {
                    CPLErrorReset();
                    double adfCMinMax[2] = {0.0, 0.0};
                    GDALComputeRasterMinMax(hBand, FALSE, adfCMinMax);
                    if (CPLGetLastErrorType() == CE_None)
                    {
                        if (bJson)
                        {
                            json_object *poComputedMin =
                                gdal_json_object_new_double_or_str_for_non_finite(
                                    adfCMinMax[0], 3);
                            json_object *poComputedMax =
                                gdal_json_object_new_double_or_str_for_non_finite(
                                    adfCMinMax[1], 3);
                            json_object_object_add(poBand, "computedMin",
                                                   poComputedMin);
                            json_object_object_add(poBand, "computedMax",
                                                   poComputedMax);
                        }
                        else
                        {
                            Concat(osStr, psOptions->bStdoutOutput,
                                   "  Computed Min/Max=%.3f,%.3f",
                                   adfCMinMax[0], adfCMinMax[1]);
                        }
                    }
                }
                if (!bJson)
                    Concat(osStr, psOptions->bStdoutOutput, "\n");
            }
        }

        double dfMinStat = 0.0;
        double dfMaxStat = 0.0;
        double dfMean = 0.0;
        double dfStdDev = 0.0;
        CPLErr eErr = GDALGetRasterStatistics(hBand, psOptions->bApproxStats,
                                              psOptions->bStats, &dfMinStat,
                                              &dfMaxStat, &dfMean, &dfStdDev);
        if (eErr == CE_None)
        {
            if (bJson)
            {
                json_object *poStacStats = json_object_new_object();
                json_object *poMinimum =
                    gdal_json_object_new_double_or_str_for_non_finite(dfMinStat,
                                                                      3);
                json_object_object_add(poBand, "minimum", poMinimum);
                json_object *poStacMinimum =
                    gdal_json_object_new_double_or_str_for_non_finite(dfMinStat,
                                                                      3);
                json_object_object_add(poStacStats, "minimum", poStacMinimum);

                json_object *poMaximum =
                    gdal_json_object_new_double_or_str_for_non_finite(dfMaxStat,
                                                                      3);
                json_object_object_add(poBand, "maximum", poMaximum);
                json_object *poStacMaximum =
                    gdal_json_object_new_double_or_str_for_non_finite(dfMaxStat,
                                                                      3);
                json_object_object_add(poStacStats, "maximum", poStacMaximum);

                json_object *poMean =
                    gdal_json_object_new_double_or_str_for_non_finite(dfMean,
                                                                      3);
                json_object_object_add(poBand, "mean", poMean);
                json_object *poStacMean =
                    gdal_json_object_new_double_or_str_for_non_finite(dfMean,
                                                                      3);
                json_object_object_add(poStacStats, "mean", poStacMean);

                json_object *poStdDev =
                    gdal_json_object_new_double_or_str_for_non_finite(dfStdDev,
                                                                      3);
                json_object_object_add(poBand, "stdDev", poStdDev);
                json_object *poStacStdDev =
                    gdal_json_object_new_double_or_str_for_non_finite(dfStdDev,
                                                                      3);
                json_object_object_add(poStacStats, "stddev", poStacStdDev);

                json_object_object_add(poStacRasterBand, "stats", poStacStats);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f\n",
                       dfMinStat, dfMaxStat, dfMean, dfStdDev);
            }
        }

        if (psOptions->bReportHistograms)
        {
            int nBucketCount = 0;
            GUIntBig *panHistogram = nullptr;

            if (bJson)
                eErr = GDALGetDefaultHistogramEx(
                    hBand, &dfMinStat, &dfMaxStat, &nBucketCount, &panHistogram,
                    TRUE, GDALDummyProgress, nullptr);
            else
                eErr = GDALGetDefaultHistogramEx(
                    hBand, &dfMinStat, &dfMaxStat, &nBucketCount, &panHistogram,
                    TRUE, GDALTermProgress, nullptr);
            if (eErr == CE_None)
            {
                json_object *poHistogram = nullptr;
                json_object *poBuckets = nullptr;

                if (bJson)
                {
                    json_object *poCount = json_object_new_int(nBucketCount);
                    json_object *poMin = json_object_new_double(dfMinStat);
                    json_object *poMax = json_object_new_double(dfMaxStat);

                    poBuckets = json_object_new_array();
                    poHistogram = json_object_new_object();
                    json_object_object_add(poHistogram, "count", poCount);
                    json_object_object_add(poHistogram, "min", poMin);
                    json_object_object_add(poHistogram, "max", poMax);
                }
                else
                {
                    Concat(osStr, psOptions->bStdoutOutput,
                           "  %d buckets from %g to %g:\n  ", nBucketCount,
                           dfMinStat, dfMaxStat);
                }

                for (int iBucket = 0; iBucket < nBucketCount; iBucket++)
                {
                    if (bJson)
                    {
                        json_object *poBucket =
                            json_object_new_int64(panHistogram[iBucket]);
                        json_object_array_add(poBuckets, poBucket);
                    }
                    else
                        Concat(osStr, psOptions->bStdoutOutput,
                               CPL_FRMT_GUIB " ", panHistogram[iBucket]);
                }
                if (bJson)
                {
                    json_object_object_add(poHistogram, "buckets", poBuckets);
                    json_object *poStacHistogram = nullptr;
                    json_object_deep_copy(poHistogram, &poStacHistogram,
                                          nullptr);
                    json_object_object_add(poBand, "histogram", poHistogram);
                    json_object_object_add(poStacRasterBand, "histogram",
                                           poStacHistogram);
                }
                else
                {
                    Concat(osStr, psOptions->bStdoutOutput, "\n");
                }
                CPLFree(panHistogram);
            }
        }

        if (psOptions->bComputeChecksum)
        {
            const int nBandChecksum =
                GDALChecksumImage(hBand, 0, 0, GDALGetRasterXSize(hDataset),
                                  GDALGetRasterYSize(hDataset));
            if (bJson)
            {
                json_object *poChecksum = json_object_new_int(nBandChecksum);
                json_object_object_add(poBand, "checksum", poChecksum);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput, "  Checksum=%d\n",
                       nBandChecksum);
            }
        }

        int bGotNodata = FALSE;
        if (eDT == GDT_Int64)
        {
            const auto nNoData =
                GDALGetRasterNoDataValueAsInt64(hBand, &bGotNodata);
            if (bGotNodata)
            {
                if (bJson)
                {
                    json_object *poNoDataValue = json_object_new_int64(nNoData);
                    json_object *poStacNoDataValue = nullptr;
                    json_object_deep_copy(poNoDataValue, &poStacNoDataValue,
                                          nullptr);
                    json_object_object_add(poStacRasterBand, "nodata",
                                           poStacNoDataValue);
                    json_object_object_add(poBand, "noDataValue",
                                           poNoDataValue);
                }
                else
                {
                    Concat(osStr, psOptions->bStdoutOutput,
                           "  NoData Value=" CPL_FRMT_GIB "\n",
                           static_cast<GIntBig>(nNoData));
                }
            }
        }
        else if (eDT == GDT_UInt64)
        {
            const auto nNoData =
                GDALGetRasterNoDataValueAsUInt64(hBand, &bGotNodata);
            if (bGotNodata)
            {
                if (bJson)
                {
                    if (nNoData < static_cast<uint64_t>(
                                      std::numeric_limits<int64_t>::max()))
                    {
                        json_object *poNoDataValue = json_object_new_int64(
                            static_cast<int64_t>(nNoData));
                        json_object *poStacNoDataValue = nullptr;
                        json_object_deep_copy(poNoDataValue, &poStacNoDataValue,
                                              nullptr);
                        json_object_object_add(poStacRasterBand, "nodata",
                                               poStacNoDataValue);
                        json_object_object_add(poBand, "noDataValue",
                                               poNoDataValue);
                    }
                    else
                    {
                        // not pretty to serialize as a string but there's no
                        // way to serialize a uint64_t with libjson-c
                        json_object *poNoDataValue =
                            json_object_new_string(CPLSPrintf(
                                CPL_FRMT_GUIB, static_cast<GUIntBig>(nNoData)));
                        json_object_object_add(poBand, "noDataValue",
                                               poNoDataValue);
                    }
                }
                else
                {
                    Concat(osStr, psOptions->bStdoutOutput,
                           "  NoData Value=" CPL_FRMT_GUIB "\n",
                           static_cast<GUIntBig>(nNoData));
                }
            }
        }
        else
        {
            const double dfNoData =
                GDALGetRasterNoDataValue(hBand, &bGotNodata);
            if (bGotNodata)
            {
                const bool bIsNoDataFloat =
                    eDT == GDT_Float32 &&
                    static_cast<float>(dfNoData) == dfNoData;
                // Find the most compact decimal representation of the nodata
                // value that can be used to exactly represent the binary value
                int nSignificantDigits = bIsNoDataFloat ? 8 : 18;
                char szNoData[64] = {0};
                while (nSignificantDigits > 0)
                {
                    char szCandidateNoData[64];
                    char szFormat[16];
                    snprintf(szFormat, sizeof(szFormat), "%%.%dg",
                             nSignificantDigits);
                    CPLsnprintf(szCandidateNoData, sizeof(szCandidateNoData),
                                szFormat, dfNoData);
                    if (szNoData[0] == '\0' ||
                        (bIsNoDataFloat &&
                         static_cast<float>(CPLAtof(szCandidateNoData)) ==
                             static_cast<float>(dfNoData)) ||
                        (!bIsNoDataFloat &&
                         CPLAtof(szCandidateNoData) == dfNoData))
                    {
                        strcpy(szNoData, szCandidateNoData);
                        nSignificantDigits--;
                    }
                    else
                    {
                        nSignificantDigits++;
                        break;
                    }
                }

                if (bJson)
                {
                    json_object *poNoDataValue =
                        gdal_json_object_new_double_significant_digits(
                            dfNoData, nSignificantDigits);
                    json_object *poStacNoDataValue = nullptr;
                    json_object_deep_copy(poNoDataValue, &poStacNoDataValue,
                                          nullptr);
                    json_object_object_add(poStacRasterBand, "nodata",
                                           poStacNoDataValue);
                    json_object_object_add(poBand, "noDataValue",
                                           poNoDataValue);
                }
                else if (CPLIsNan(dfNoData))
                {
                    Concat(osStr, psOptions->bStdoutOutput,
                           "  NoData Value=nan\n");
                }
                else
                {
                    Concat(osStr, psOptions->bStdoutOutput,
                           "  NoData Value=%s\n", szNoData);
                }
            }
        }

        if (GDALGetOverviewCount(hBand) > 0)
        {
            json_object *poOverviews = nullptr;

            if (bJson)
                poOverviews = json_object_new_array();
            else
                Concat(osStr, psOptions->bStdoutOutput, "  Overviews: ");

            for (int iOverview = 0; iOverview < GDALGetOverviewCount(hBand);
                 iOverview++)
            {
                if (!bJson)
                    if (iOverview != 0)
                        Concat(osStr, psOptions->bStdoutOutput, ", ");

                GDALRasterBandH hOverview = GDALGetOverview(hBand, iOverview);
                if (hOverview != nullptr)
                {
                    if (bJson)
                    {
                        json_object *poOverviewSize = json_object_new_array();
                        json_object *poOverviewSizeX = json_object_new_int(
                            GDALGetRasterBandXSize(hOverview));
                        json_object *poOverviewSizeY = json_object_new_int(
                            GDALGetRasterBandYSize(hOverview));

                        json_object *poOverview = json_object_new_object();
                        json_object_array_add(poOverviewSize, poOverviewSizeX);
                        json_object_array_add(poOverviewSize, poOverviewSizeY);
                        json_object_object_add(poOverview, "size",
                                               poOverviewSize);

                        if (psOptions->bComputeChecksum)
                        {
                            const int nOverviewChecksum = GDALChecksumImage(
                                hOverview, 0, 0,
                                GDALGetRasterBandXSize(hOverview),
                                GDALGetRasterBandYSize(hOverview));
                            json_object *poOverviewChecksum =
                                json_object_new_int(nOverviewChecksum);
                            json_object_object_add(poOverview, "checksum",
                                                   poOverviewChecksum);
                        }
                        json_object_array_add(poOverviews, poOverview);
                    }
                    else
                    {
                        Concat(osStr, psOptions->bStdoutOutput, "%dx%d",
                               GDALGetRasterBandXSize(hOverview),
                               GDALGetRasterBandYSize(hOverview));
                    }

                    const char *pszResampling =
                        GDALGetMetadataItem(hOverview, "RESAMPLING", "");

                    if (pszResampling != nullptr && !bJson &&
                        STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2"))
                        Concat(osStr, psOptions->bStdoutOutput, "*");
                }
                else
                {
                    if (!bJson)
                        Concat(osStr, psOptions->bStdoutOutput, "(null)");
                }
            }
            if (bJson)
                json_object_object_add(poBand, "overviews", poOverviews);
            else
                Concat(osStr, psOptions->bStdoutOutput, "\n");

            if (psOptions->bComputeChecksum && !bJson)
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "  Overviews checksum: ");

                for (int iOverview = 0; iOverview < GDALGetOverviewCount(hBand);
                     iOverview++)
                {
                    GDALRasterBandH hOverview;

                    if (iOverview != 0)
                        Concat(osStr, psOptions->bStdoutOutput, ", ");

                    hOverview = GDALGetOverview(hBand, iOverview);
                    if (hOverview)
                    {
                        Concat(osStr, psOptions->bStdoutOutput, "%d",
                               GDALChecksumImage(
                                   hOverview, 0, 0,
                                   GDALGetRasterBandXSize(hOverview),
                                   GDALGetRasterBandYSize(hOverview)));
                    }
                    else
                    {
                        Concat(osStr, psOptions->bStdoutOutput, "(null)");
                    }
                }
                Concat(osStr, psOptions->bStdoutOutput, "\n");
            }
        }

        if (GDALHasArbitraryOverviews(hBand) && !bJson)
        {
            Concat(osStr, psOptions->bStdoutOutput, "  Overviews: arbitrary\n");
        }

        const int nMaskFlags = GDALGetMaskFlags(hBand);
        if ((nMaskFlags & (GMF_NODATA | GMF_ALL_VALID)) == 0 ||
            nMaskFlags == (GMF_NODATA | GMF_PER_DATASET))
        {
            GDALRasterBandH hMaskBand = GDALGetMaskBand(hBand);
            json_object *poMask = nullptr;
            json_object *poFlags = nullptr;
            json_object *poMaskOverviews = nullptr;

            if (bJson)
            {
                poMask = json_object_new_object();
                poFlags = json_object_new_array();
            }
            else
                Concat(osStr, psOptions->bStdoutOutput, "  Mask Flags: ");
            if (nMaskFlags & GMF_PER_DATASET)
            {
                if (bJson)
                {
                    json_object *poFlag = json_object_new_string("PER_DATASET");
                    json_object_array_add(poFlags, poFlag);
                }
                else
                    Concat(osStr, psOptions->bStdoutOutput, "PER_DATASET ");
            }
            if (nMaskFlags & GMF_ALPHA)
            {
                if (bJson)
                {
                    json_object *poFlag = json_object_new_string("ALPHA");
                    json_object_array_add(poFlags, poFlag);
                }
                else
                    Concat(osStr, psOptions->bStdoutOutput, "ALPHA ");
            }
            if (nMaskFlags & GMF_NODATA)
            {
                if (bJson)
                {
                    json_object *poFlag = json_object_new_string("NODATA");
                    json_object_array_add(poFlags, poFlag);
                }
                else
                {
                    Concat(osStr, psOptions->bStdoutOutput, "NODATA ");
                }
            }

            if (bJson)
                json_object_object_add(poMask, "flags", poFlags);
            else
                Concat(osStr, psOptions->bStdoutOutput, "\n");

            if (bJson)
                poMaskOverviews = json_object_new_array();

            if (hMaskBand != nullptr && GDALGetOverviewCount(hMaskBand) > 0)
            {
                if (!bJson)
                    Concat(osStr, psOptions->bStdoutOutput,
                           "  Overviews of mask band: ");

                for (int iOverview = 0;
                     iOverview < GDALGetOverviewCount(hMaskBand); iOverview++)
                {
                    GDALRasterBandH hOverview =
                        GDALGetOverview(hMaskBand, iOverview);
                    if (!hOverview)
                        break;
                    json_object *poMaskOverview = nullptr;
                    json_object *poMaskOverviewSize = nullptr;

                    if (bJson)
                    {
                        poMaskOverview = json_object_new_object();
                        poMaskOverviewSize = json_object_new_array();
                    }
                    else
                    {
                        if (iOverview != 0)
                            Concat(osStr, psOptions->bStdoutOutput, ", ");
                    }

                    if (bJson)
                    {
                        json_object *poMaskOverviewSizeX = json_object_new_int(
                            GDALGetRasterBandXSize(hOverview));
                        json_object *poMaskOverviewSizeY = json_object_new_int(
                            GDALGetRasterBandYSize(hOverview));

                        json_object_array_add(poMaskOverviewSize,
                                              poMaskOverviewSizeX);
                        json_object_array_add(poMaskOverviewSize,
                                              poMaskOverviewSizeY);
                        json_object_object_add(poMaskOverview, "size",
                                               poMaskOverviewSize);
                        json_object_array_add(poMaskOverviews, poMaskOverview);
                    }
                    else
                    {
                        Concat(osStr, psOptions->bStdoutOutput, "%dx%d",
                               GDALGetRasterBandXSize(hOverview),
                               GDALGetRasterBandYSize(hOverview));
                    }
                }
                if (!bJson)
                    Concat(osStr, psOptions->bStdoutOutput, "\n");
            }
            if (bJson)
            {
                json_object_object_add(poMask, "overviews", poMaskOverviews);
                json_object_object_add(poBand, "mask", poMask);
            }
        }

        if (strlen(GDALGetRasterUnitType(hBand)) > 0)
        {
            if (bJson)
            {
                json_object *poUnit =
                    json_object_new_string(GDALGetRasterUnitType(hBand));
                json_object *poStacUnit = nullptr;
                json_object_deep_copy(poUnit, &poStacUnit, nullptr);
                json_object_object_add(poStacRasterBand, "unit", poStacUnit);
                json_object_object_add(poBand, "unit", poUnit);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput, "  Unit Type: %s\n",
                       GDALGetRasterUnitType(hBand));
            }
        }

        if (GDALGetRasterCategoryNames(hBand) != nullptr)
        {
            char **papszCategories = GDALGetRasterCategoryNames(hBand);
            json_object *poCategories = nullptr;

            if (bJson)
                poCategories = json_object_new_array();
            else
                Concat(osStr, psOptions->bStdoutOutput, "  Categories:\n");

            for (int i = 0; papszCategories[i] != nullptr; i++)
            {
                if (bJson)
                {
                    json_object *poCategoryName =
                        json_object_new_string(papszCategories[i]);
                    json_object_array_add(poCategories, poCategoryName);
                }
                else
                    Concat(osStr, psOptions->bStdoutOutput, "    %3d: %s\n", i,
                           papszCategories[i]);
            }
            if (bJson)
                json_object_object_add(poBand, "categories", poCategories);
        }

        int bSuccess = FALSE;
        if (GDALGetRasterScale(hBand, &bSuccess) != 1.0 ||
            GDALGetRasterOffset(hBand, &bSuccess) != 0.0)
        {
            if (bJson)
            {
                json_object *poOffset = json_object_new_double_with_precision(
                    GDALGetRasterOffset(hBand, &bSuccess), 15);
                json_object *poScale = json_object_new_double_with_precision(
                    GDALGetRasterScale(hBand, &bSuccess), 15);
                json_object *poStacScale = nullptr;
                json_object *poStacOffset = nullptr;
                json_object_deep_copy(poScale, &poStacScale, nullptr);
                json_object_deep_copy(poOffset, &poStacOffset, nullptr);
                json_object_object_add(poStacRasterBand, "scale", poStacScale);
                json_object_object_add(poStacRasterBand, "offset",
                                       poStacOffset);
                json_object_object_add(poBand, "offset", poOffset);
                json_object_object_add(poBand, "scale", poScale);
            }
            else
            {
                Concat(osStr, psOptions->bStdoutOutput,
                       "  Offset: %.15g,   Scale:%.15g\n",
                       GDALGetRasterOffset(hBand, &bSuccess),
                       GDALGetRasterScale(hBand, &bSuccess));
            }
        }

        GDALInfoReportMetadata(psOptions, hBand, true, bJson, poBandMetadata,
                               osStr);
        if (bJson)
        {
            if (psOptions->bShowMetadata)
                json_object_object_add(poBand, "metadata", poBandMetadata);
            else
                json_object_put(poBandMetadata);
        }

        GDALColorTableH hTable;
        if (GDALGetRasterColorInterpretation(hBand) == GCI_PaletteIndex &&
            (hTable = GDALGetRasterColorTable(hBand)) != nullptr)
        {
            if (!bJson)
                Concat(osStr, psOptions->bStdoutOutput,
                       "  Color Table (%s with %d entries)\n",
                       GDALGetPaletteInterpretationName(
                           GDALGetPaletteInterpretation(hTable)),
                       GDALGetColorEntryCount(hTable));

            if (psOptions->bShowColorTable)
            {
                json_object *poEntries = nullptr;

                if (bJson)
                {
                    json_object *poPalette =
                        json_object_new_string(GDALGetPaletteInterpretationName(
                            GDALGetPaletteInterpretation(hTable)));
                    json_object *poCount =
                        json_object_new_int(GDALGetColorEntryCount(hTable));

                    json_object *poColorTable = json_object_new_object();

                    json_object_object_add(poColorTable, "palette", poPalette);
                    json_object_object_add(poColorTable, "count", poCount);

                    poEntries = json_object_new_array();
                    json_object_object_add(poColorTable, "entries", poEntries);
                    json_object_object_add(poBand, "colorTable", poColorTable);
                }

                for (int i = 0; i < GDALGetColorEntryCount(hTable); i++)
                {
                    GDALColorEntry sEntry;

                    GDALGetColorEntryAsRGB(hTable, i, &sEntry);

                    if (bJson)
                    {
                        json_object *poEntry = json_object_new_array();
                        json_object *poC1 = json_object_new_int(sEntry.c1);
                        json_object *poC2 = json_object_new_int(sEntry.c2);
                        json_object *poC3 = json_object_new_int(sEntry.c3);
                        json_object *poC4 = json_object_new_int(sEntry.c4);

                        json_object_array_add(poEntry, poC1);
                        json_object_array_add(poEntry, poC2);
                        json_object_array_add(poEntry, poC3);
                        json_object_array_add(poEntry, poC4);
                        json_object_array_add(poEntries, poEntry);
                    }
                    else
                    {
                        Concat(osStr, psOptions->bStdoutOutput,
                               "  %3d: %d,%d,%d,%d\n", i, sEntry.c1, sEntry.c2,
                               sEntry.c3, sEntry.c4);
                    }
                }
            }
        }

        if (psOptions->bShowRAT && GDALGetDefaultRAT(hBand) != nullptr)
        {
            GDALRasterAttributeTableH hRAT = GDALGetDefaultRAT(hBand);

            if (bJson)
            {
                json_object *poRAT =
                    static_cast<json_object *>(GDALRATSerializeJSON(hRAT));
                json_object_object_add(poJsonObject, "rat", poRAT);
            }
            else
            {
                CPLXMLNode *psTree =
                    static_cast<GDALRasterAttributeTable *>(hRAT)->Serialize();
                char *pszXMLText = CPLSerializeXMLTree(psTree);
                CPLDestroyXMLNode(psTree);
                Concat(osStr, psOptions->bStdoutOutput, "%s\n", pszXMLText);
                CPLFree(pszXMLText);
            }
        }
        if (bJson)
        {
            json_object_array_add(poBands, poBand);
            json_object_array_add(poStacRasterBands, poStacRasterBand);
            json_object_array_add(poStacEOBands, poStacEOBand);
        }
    }

    if (bJson)
    {
        json_object_object_add(poJsonObject, "bands", poBands);
        json_object_object_add(poStac, "raster:bands", poStacRasterBands);
        json_object_object_add(poStac, "eo:bands", poStacEOBands);
        json_object_object_add(poJsonObject, "stac", poStac);
        Concat(osStr, psOptions->bStdoutOutput, "%s",
               json_object_to_json_string_ext(
                   poJsonObject, JSON_C_TO_STRING_PRETTY
#ifdef JSON_C_TO_STRING_NOSLASHESCAPE
                                     | JSON_C_TO_STRING_NOSLASHESCAPE
#endif
                   ));
        json_object_put(poJsonObject);
        Concat(osStr, psOptions->bStdoutOutput, "\n");
    }

    if (psOptionsToFree != nullptr)
        GDALInfoOptionsFree(psOptionsToFree);

    return VSI_STRDUP_VERBOSE(osStr);
}

/************************************************************************/
/*                        GDALInfoReportCorner()                        */
/************************************************************************/

static int GDALInfoReportCorner(const GDALInfoOptions *psOptions,
                                GDALDatasetH hDataset,
                                OGRCoordinateTransformationH hTransform,
                                const char *corner_name, double x, double y,
                                bool bJson, json_object *poCornerCoordinates,
                                json_object *poLongLatExtentCoordinates,
                                CPLString &osStr)

{
    if (!bJson)
        Concat(osStr, psOptions->bStdoutOutput, "%-11s ", corner_name);

    /* -------------------------------------------------------------------- */
    /*      Transform the point into georeferenced coordinates.             */
    /* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double dfGeoX = 0.0;
    double dfGeoY = 0.0;

    if (GDALGetGeoTransform(hDataset, adfGeoTransform) == CE_None)
    {
        dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x +
                 adfGeoTransform[2] * y;
        dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x +
                 adfGeoTransform[5] * y;
    }
    else
    {
        if (bJson)
        {
            json_object *const poCorner = json_object_new_array();
            json_object *const poX =
                json_object_new_double_with_precision(x, 1);
            json_object *const poY =
                json_object_new_double_with_precision(y, 1);
            json_object_array_add(poCorner, poX);
            json_object_array_add(poCorner, poY);
            json_object_object_add(poCornerCoordinates, corner_name, poCorner);
        }
        else
        {
            Concat(osStr, psOptions->bStdoutOutput, "(%7.1f,%7.1f)\n", x, y);
        }
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Report the georeferenced coordinates.                           */
    /* -------------------------------------------------------------------- */
    if (std::abs(dfGeoX) < 181 && std::abs(dfGeoY) < 91)
    {
        if (bJson)
        {
            json_object *const poCorner = json_object_new_array();
            json_object *const poX =
                json_object_new_double_with_precision(dfGeoX, 7);
            json_object *const poY =
                json_object_new_double_with_precision(dfGeoY, 7);
            json_object_array_add(poCorner, poX);
            json_object_array_add(poCorner, poY);
            json_object_object_add(poCornerCoordinates, corner_name, poCorner);
        }
        else
        {
            Concat(osStr, psOptions->bStdoutOutput, "(%12.7f,%12.7f) ", dfGeoX,
                   dfGeoY);
        }
    }
    else
    {
        if (bJson)
        {
            json_object *const poCorner = json_object_new_array();
            json_object *const poX =
                json_object_new_double_with_precision(dfGeoX, 3);
            json_object *const poY =
                json_object_new_double_with_precision(dfGeoY, 3);
            json_object_array_add(poCorner, poX);
            json_object_array_add(poCorner, poY);
            json_object_object_add(poCornerCoordinates, corner_name, poCorner);
        }
        else
        {
            Concat(osStr, psOptions->bStdoutOutput, "(%12.3f,%12.3f) ", dfGeoX,
                   dfGeoY);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Transform to latlong and report.                                */
    /* -------------------------------------------------------------------- */
    if (bJson)
    {
        double dfZ = 0.0;
        if (hTransform != nullptr && !EQUAL(corner_name, "center") &&
            OCTTransform(hTransform, 1, &dfGeoX, &dfGeoY, &dfZ))
        {
            json_object *const poCorner = json_object_new_array();
            json_object *const poX =
                json_object_new_double_with_precision(dfGeoX, 7);
            json_object *const poY =
                json_object_new_double_with_precision(dfGeoY, 7);
            json_object_array_add(poCorner, poX);
            json_object_array_add(poCorner, poY);
            json_object_array_add(poLongLatExtentCoordinates, poCorner);
        }
    }
    else
    {
        double dfZ = 0.0;
        if (hTransform != nullptr &&
            OCTTransform(hTransform, 1, &dfGeoX, &dfGeoY, &dfZ))
        {
            Concat(osStr, psOptions->bStdoutOutput, "(%s,",
                   GDALDecToDMS(dfGeoX, "Long", 2));
            Concat(osStr, psOptions->bStdoutOutput, "%s)",
                   GDALDecToDMS(dfGeoY, "Lat", 2));
        }
        Concat(osStr, psOptions->bStdoutOutput, "\n");
    }

    return TRUE;
}

/************************************************************************/
/*                       GDALInfoPrintMetadata()                        */
/************************************************************************/
static void GDALInfoPrintMetadata(const GDALInfoOptions *psOptions,
                                  GDALMajorObjectH hObject,
                                  const char *pszDomain,
                                  const char *pszDisplayedname,
                                  const char *pszIndent, int bJsonOutput,
                                  json_object *poMetadata, CPLString &osStr)
{
    const bool bIsxml =
        pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "xml:");
    const bool bMDIsJson =
        pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "json:");

    char **papszMetadata = GDALGetMetadata(hObject, pszDomain);
    if (papszMetadata != nullptr && *papszMetadata != nullptr)
    {
        json_object *poDomain = (bJsonOutput && !bIsxml && !bMDIsJson)
                                    ? json_object_new_object()
                                    : nullptr;

        if (!bJsonOutput)
            Concat(osStr, psOptions->bStdoutOutput, "%s%s:\n", pszIndent,
                   pszDisplayedname);

        json_object *poValue = nullptr;

        for (int i = 0; papszMetadata[i] != nullptr; i++)
        {
            if (bJsonOutput)
            {
                if (bIsxml)
                {
                    poValue = json_object_new_string(papszMetadata[i]);
                    break;
                }
                else if (bMDIsJson)
                {
                    OGRJSonParse(papszMetadata[i], &poValue, true);
                    break;
                }
                else
                {
                    char *pszKey = nullptr;
                    const char *pszValue =
                        CPLParseNameValue(papszMetadata[i], &pszKey);
                    if (pszKey)
                    {
                        poValue = json_object_new_string(pszValue);
                        json_object_object_add(poDomain, pszKey, poValue);
                        CPLFree(pszKey);
                    }
                }
            }
            else
            {
                if (bIsxml || bMDIsJson)
                    Concat(osStr, psOptions->bStdoutOutput, "%s%s\n", pszIndent,
                           papszMetadata[i]);
                else
                    Concat(osStr, psOptions->bStdoutOutput, "%s  %s\n",
                           pszIndent, papszMetadata[i]);
            }
        }
        if (bJsonOutput)
        {
            if (bIsxml || bMDIsJson)
            {
                json_object_object_add(poMetadata, pszDomain, poValue);
            }
            else
            {
                if (pszDomain == nullptr)
                    json_object_object_add(poMetadata, "", poDomain);
                else
                    json_object_object_add(poMetadata, pszDomain, poDomain);
            }
        }
    }
}

/************************************************************************/
/*                       GDALInfoReportMetadata()                       */
/************************************************************************/
static void GDALInfoReportMetadata(const GDALInfoOptions *psOptions,
                                   GDALMajorObjectH hObject, bool bIsBand,
                                   bool bJson, json_object *poMetadata,
                                   CPLString &osStr)
{
    const char *const pszIndent = bIsBand ? "  " : "";

    /* -------------------------------------------------------------------- */
    /*      Report list of Metadata domains                                 */
    /* -------------------------------------------------------------------- */
    if (psOptions->bListMDD)
    {
        const CPLStringList aosDomainList(GDALGetMetadataDomainList(hObject));
        json_object *poMDD = nullptr;
        json_object *const poListMDD =
            bJson ? json_object_new_array() : nullptr;

        if (!aosDomainList.empty())
        {
            if (!bJson)
                Concat(osStr, psOptions->bStdoutOutput, "%sMetadata domains:\n",
                       pszIndent);
        }

        for (const char *pszDomain : aosDomainList)
        {
            if (EQUAL(pszDomain, ""))
            {
                if (bJson)
                    poMDD = json_object_new_string(pszDomain);
                else
                    Concat(osStr, psOptions->bStdoutOutput, "%s  (default)\n",
                           pszIndent);
            }
            else
            {
                if (bJson)
                    poMDD = json_object_new_string(pszDomain);
                else
                    Concat(osStr, psOptions->bStdoutOutput, "%s  %s\n",
                           pszIndent, pszDomain);
            }
            if (bJson)
                json_object_array_add(poListMDD, poMDD);
        }
        if (bJson)
            json_object_object_add(poMetadata, "metadataDomains", poListMDD);
    }

    if (!psOptions->bShowMetadata)
        return;

    /* -------------------------------------------------------------------- */
    /*      Report default Metadata domain.                                 */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata(psOptions, hObject, nullptr, "Metadata", pszIndent,
                          bJson, poMetadata, osStr);

    /* -------------------------------------------------------------------- */
    /*      Report extra Metadata domains                                   */
    /* -------------------------------------------------------------------- */
    if (!psOptions->aosExtraMDDomains.empty())
    {
        CPLStringList aosExtraMDDomainsExpanded;

        if (EQUAL(psOptions->aosExtraMDDomains[0], "all") &&
            psOptions->aosExtraMDDomains.Count() == 1)
        {
            const CPLStringList aosMDDList(GDALGetMetadataDomainList(hObject));
            for (const char *pszDomain : aosMDDList)
            {
                if (!EQUAL(pszDomain, "") &&
                    !EQUAL(pszDomain, "IMAGE_STRUCTURE") &&
                    !EQUAL(pszDomain, "TILING_SCHEME") &&
                    !EQUAL(pszDomain, "SUBDATASETS") &&
                    !EQUAL(pszDomain, "GEOLOCATION") &&
                    !EQUAL(pszDomain, "RPC"))
                {
                    aosExtraMDDomainsExpanded.AddString(pszDomain);
                }
            }
        }
        else
        {
            aosExtraMDDomainsExpanded = psOptions->aosExtraMDDomains;
        }

        for (const char *pszDomain : aosExtraMDDomainsExpanded)
        {
            if (bJson)
            {
                GDALInfoPrintMetadata(psOptions, hObject, pszDomain, pszDomain,
                                      pszIndent, bJson, poMetadata, osStr);
            }
            else
            {
                const std::string osDisplayedName =
                    std::string("Metadata (").append(pszDomain).append(")");

                GDALInfoPrintMetadata(psOptions, hObject, pszDomain,
                                      osDisplayedName.c_str(), pszIndent, bJson,
                                      poMetadata, osStr);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Report various named metadata domains.                          */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata(psOptions, hObject, "IMAGE_STRUCTURE",
                          "Image Structure Metadata", pszIndent, bJson,
                          poMetadata, osStr);

    if (!bIsBand)
    {
        GDALInfoPrintMetadata(psOptions, hObject, "TILING_SCHEME",
                              "Tiling Scheme", pszIndent, bJson, poMetadata,
                              osStr);
        GDALInfoPrintMetadata(psOptions, hObject, "SUBDATASETS", "Subdatasets",
                              pszIndent, bJson, poMetadata, osStr);
        GDALInfoPrintMetadata(psOptions, hObject, "GEOLOCATION", "Geolocation",
                              pszIndent, bJson, poMetadata, osStr);
        GDALInfoPrintMetadata(psOptions, hObject, "RPC", "RPC Metadata",
                              pszIndent, bJson, poMetadata, osStr);
    }
}

/************************************************************************/
/*                             GDALInfoOptionsNew()                     */
/************************************************************************/

/**
 * Allocates a GDALInfoOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdalinfo.html">gdalinfo</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdalinfo_bin.cpp use case) must be allocated with
 *                           GDALInfoOptionsForBinaryNew() prior to this
 * function. Will be filled with potentially present filename, open options,
 * subdataset number...
 * @return pointer to the allocated GDALInfoOptions struct. Must be freed with
 * GDALInfoOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALInfoOptions *
GDALInfoOptionsNew(char **papszArgv,
                   GDALInfoOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALInfoOptions>();

    /* -------------------------------------------------------------------- */
    /*      Parse arguments.                                                */
    /* -------------------------------------------------------------------- */

    CPLStringList aosArgv;

    if (papszArgv)
    {
        const int nArgc = CSLCount(papszArgv);
        for (int i = 0; i < nArgc; i++)
        {
            aosArgv.AddString(papszArgv[i]);
        }
    }

    try
    {
        auto argParser =
            GDALInfoAppOptionsGetParser(psOptions.get(), psOptionsForBinary);

        argParser->parse_args_without_binary_name(aosArgv.List());

        if (psOptions->bApproxStats)
            psOptions->bStats = true;
    }
    catch (const std::exception &error)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
        return nullptr;
    }

    return psOptions.release();
}

/************************************************************************/
/*                             GDALInfoOptionsFree()                    */
/************************************************************************/

/**
 * Frees the GDALInfoOptions struct.
 *
 * @param psOptions the options struct for GDALInfo().
 *
 * @since GDAL 2.1
 */

void GDALInfoOptionsFree(GDALInfoOptions *psOptions)
{
    delete psOptions;
}

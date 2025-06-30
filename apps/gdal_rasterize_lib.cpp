/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Rasterize OGR shapes into a GDAL raster.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"
#include "gdalargumentparser.h"

/************************************************************************/
/*                          GDALRasterizeOptions()                      */
/************************************************************************/

struct GDALRasterizeOptions
{
    std::vector<int> anBandList{};
    std::vector<double> adfBurnValues{};
    bool bInverse = false;
    std::string osFormat{};
    bool b3D = false;
    GDALProgressFunc pfnProgress = GDALDummyProgress;
    void *pProgressData = nullptr;
    std::vector<std::string> aosLayers{};
    std::string osSQL{};
    std::string osDialect{};
    std::string osBurnAttribute{};
    std::string osWHERE{};
    CPLStringList aosRasterizeOptions{};
    CPLStringList aosTO{};
    double dfXRes = 0;
    double dfYRes = 0;
    CPLStringList aosCreationOptions{};
    GDALDataType eOutputType = GDT_Unknown;
    std::vector<double> adfInitVals{};
    std::string osNoData{};
    OGREnvelope sEnvelop{};
    int nXSize = 0;
    int nYSize = 0;
    OGRSpatialReference oOutputSRS{};

    bool bTargetAlignedPixels = false;
    bool bCreateOutput = false;
};

/************************************************************************/
/*                     GDALRasterizeOptionsGetParser()                  */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALRasterizeOptionsGetParser(GDALRasterizeOptions *psOptions,
                              GDALRasterizeOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdal_rasterize", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(_("Burns vector geometries into a raster."));

    argParser->add_epilog(
        _("This program burns vector geometries (points, lines, and polygons) "
          "into the raster band(s) of a raster image."));

    // Dealt manually as argparse::nargs_pattern::at_least_one is problematic
    argParser->add_argument("-b")
        .metavar("<band>")
        .append()
        .scan<'i', int>()
        //.nargs(argparse::nargs_pattern::at_least_one)
        .help(_("The band(s) to burn values into."));

    argParser->add_argument("-i")
        .flag()
        .store_into(psOptions->bInverse)
        .help(_("Invert rasterization."));

    argParser->add_argument("-at")
        .flag()
        .action(
            [psOptions](const std::string &) {
                psOptions->aosRasterizeOptions.SetNameValue("ALL_TOUCHED",
                                                            "TRUE");
            })
        .help(_("Enables the ALL_TOUCHED rasterization option."));

    // Mutually exclusive options: -burn, -3d, -a
    {
        // Required if options for binary
        auto &group = argParser->add_mutually_exclusive_group(
            psOptionsForBinary != nullptr);

        // Dealt manually as argparse::nargs_pattern::at_least_one is problematic
        group.add_argument("-burn")
            .metavar("<value>")
            .scan<'g', double>()
            .append()
            //.nargs(argparse::nargs_pattern::at_least_one)
            .help(_("A fixed value to burn into the raster band(s)."));

        group.add_argument("-a")
            .metavar("<attribute_name>")
            .store_into(psOptions->osBurnAttribute)
            .help(_("Name of the field in the input layer to get the burn "
                    "values from."));

        group.add_argument("-3d")
            .flag()
            .store_into(psOptions->b3D)
            .action(
                [psOptions](const std::string &) {
                    psOptions->aosRasterizeOptions.SetNameValue(
                        "BURN_VALUE_FROM", "Z");
                })
            .help(_("Indicates that a burn value should be extracted from the "
                    "\"Z\" values of the feature."));
    }

    argParser->add_argument("-add")
        .flag()
        .action(
            [psOptions](const std::string &) {
                psOptions->aosRasterizeOptions.SetNameValue("MERGE_ALG", "ADD");
            })
        .help(_("Instead of burning a new value, this adds the new value to "
                "the existing raster."));

    // Undocumented
    argParser->add_argument("-chunkysize")
        .flag()
        .hidden()
        .action(
            [psOptions](const std::string &s) {
                psOptions->aosRasterizeOptions.SetNameValue("CHUNKYSIZE",
                                                            s.c_str());
            });

    // Mutually exclusive -l, -sql
    {
        auto &group = argParser->add_mutually_exclusive_group(false);

        group.add_argument("-l")
            .metavar("<layer_name>")
            .append()
            .store_into(psOptions->aosLayers)
            .help(_("Name of the layer(s) to process."));

        group.add_argument("-sql")
            .metavar("<sql_statement>")
            .store_into(psOptions->osSQL)
            .action(
                [psOptions](const std::string &sql)
                {
                    GByte *pabyRet = nullptr;
                    if (!sql.empty() && sql.at(0) == '@' &&
                        VSIIngestFile(nullptr, sql.substr(1).c_str(), &pabyRet,
                                      nullptr, 10 * 1024 * 1024))
                    {
                        GDALRemoveBOM(pabyRet);
                        char *pszSQLStatement =
                            reinterpret_cast<char *>(pabyRet);
                        psOptions->osSQL =
                            CPLRemoveSQLComments(pszSQLStatement);
                        VSIFree(pszSQLStatement);
                    }
                })
            .help(
                _("An SQL statement to be evaluated against the datasource to "
                  "produce a virtual layer of features to be burned in."));
    }

    argParser->add_argument("-where")
        .metavar("<expression>")
        .store_into(psOptions->osWHERE)
        .help(_("An optional SQL WHERE style query expression to be applied to "
                "select features "
                "to burn in from the input layer(s)."));

    argParser->add_argument("-dialect")
        .metavar("<sql_dialect>")
        .store_into(psOptions->osDialect)
        .help(_("The SQL dialect to use for the SQL expression."));

    // Store later
    argParser->add_argument("-a_nodata")
        .metavar("<value>")
        .help(_("Assign a specified nodata value to output bands."));

    // Dealt manually as argparse::nargs_pattern::at_least_one is problematic
    argParser->add_argument("-init")
        .metavar("<value>")
        .append()
        //.nargs(argparse::nargs_pattern::at_least_one)
        .scan<'g', double>()
        .help(_("Initialize the output bands to the specified value."));

    argParser->add_argument("-a_srs")
        .metavar("<srs_def>")
        .action(
            [psOptions](const std::string &osOutputSRSDef)
            {
                if (psOptions->oOutputSRS.SetFromUserInput(
                        osOutputSRSDef.c_str()) != OGRERR_NONE)
                {
                    throw std::invalid_argument(
                        std::string("Failed to process SRS definition: ")
                            .append(osOutputSRSDef));
                }
                psOptions->bCreateOutput = true;
            })
        .help(_("The spatial reference system to use for the output raster."));

    argParser->add_argument("-to")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosTO.AddString(s.c_str()); })
        .help(_("Set a transformer option."));

    // Store later
    argParser->add_argument("-te")
        .metavar("<xmin> <ymin> <xmax> <ymax>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("Set georeferenced extents of output file to be created."));

    // Mutex with tr
    {
        auto &group = argParser->add_mutually_exclusive_group(false);

        // Store later
        group.add_argument("-tr")
            .metavar("<xres> <yres>")
            .nargs(2)
            .scan<'g', double>()
            .help(
                _("Set output file resolution in target georeferenced units."));

        // Store later
        // Note: this is supposed to be int but for backward compatibility, we
        //       use double
        auto &arg = group.add_argument("-ts")
                        .metavar("<width> <height>")
                        .nargs(2)
                        .scan<'g', double>()
                        .help(_("Set output file size in pixels and lines."));

        argParser->add_hidden_alias_for(arg, "-outsize");
    }

    argParser->add_argument("-tap")
        .flag()
        .store_into(psOptions->bTargetAlignedPixels)
        .action([psOptions](const std::string &)
                { psOptions->bCreateOutput = true; })
        .help(_("Align the coordinates of the extent to the values of the "
                "output raster."));

    argParser->add_argument("-optim")
        .metavar("AUTO|VECTOR|RASTER")
        .action(
            [psOptions](const std::string &s) {
                psOptions->aosRasterizeOptions.SetNameValue("OPTIM", s.c_str());
            })
        .help(_("Force the algorithm used."));

    argParser->add_creation_options_argument(psOptions->aosCreationOptions)
        .action([psOptions](const std::string &)
                { psOptions->bCreateOutput = true; });

    argParser->add_output_type_argument(psOptions->eOutputType)
        .action([psOptions](const std::string &)
                { psOptions->bCreateOutput = true; });

    argParser->add_output_format_argument(psOptions->osFormat)
        .action([psOptions](const std::string &)
                { psOptions->bCreateOutput = true; });

    // Written that way so that in library mode, users can still use the -q
    // switch, even if it has no effect
    argParser->add_quiet_argument(
        psOptionsForBinary ? &(psOptionsForBinary->bQuiet) : nullptr);

    if (psOptionsForBinary)
    {

        argParser->add_open_options_argument(
            psOptionsForBinary->aosOpenOptions);

        argParser->add_argument("src_datasource")
            .metavar("<src_datasource>")
            .store_into(psOptionsForBinary->osSource)
            .help(_("Any vector supported readable datasource."));

        argParser->add_argument("dst_filename")
            .metavar("<dst_filename>")
            .store_into(psOptionsForBinary->osDest)
            .help(_("The GDAL raster supported output file."));
    }

    return argParser;
}

/************************************************************************/
/*                          GDALRasterizeAppGetParserUsage()            */
/************************************************************************/

std::string GDALRasterizeAppGetParserUsage()
{
    try
    {
        GDALRasterizeOptions sOptions;
        GDALRasterizeOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALRasterizeOptionsGetParser(&sOptions, &sOptionsForBinary);
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
/*                          InvertGeometries()                          */
/************************************************************************/

static void InvertGeometries(GDALDatasetH hDstDS,
                             std::vector<OGRGeometryH> &ahGeometries)

{
    OGRMultiPolygon *poInvertMP = new OGRMultiPolygon();

    /* -------------------------------------------------------------------- */
    /*      Create a ring that is a bit outside the raster dataset.         */
    /* -------------------------------------------------------------------- */
    const int brx = GDALGetRasterXSize(hDstDS) + 2;
    const int bry = GDALGetRasterYSize(hDstDS) + 2;

    double adfGeoTransform[6] = {};
    GDALGetGeoTransform(hDstDS, adfGeoTransform);

    auto poUniverseRing = std::make_unique<OGRLinearRing>();

    poUniverseRing->addPoint(
        adfGeoTransform[0] + -2 * adfGeoTransform[1] + -2 * adfGeoTransform[2],
        adfGeoTransform[3] + -2 * adfGeoTransform[4] + -2 * adfGeoTransform[5]);

    poUniverseRing->addPoint(adfGeoTransform[0] + brx * adfGeoTransform[1] +
                                 -2 * adfGeoTransform[2],
                             adfGeoTransform[3] + brx * adfGeoTransform[4] +
                                 -2 * adfGeoTransform[5]);

    poUniverseRing->addPoint(adfGeoTransform[0] + brx * adfGeoTransform[1] +
                                 bry * adfGeoTransform[2],
                             adfGeoTransform[3] + brx * adfGeoTransform[4] +
                                 bry * adfGeoTransform[5]);

    poUniverseRing->addPoint(adfGeoTransform[0] + -2 * adfGeoTransform[1] +
                                 bry * adfGeoTransform[2],
                             adfGeoTransform[3] + -2 * adfGeoTransform[4] +
                                 bry * adfGeoTransform[5]);

    poUniverseRing->addPoint(
        adfGeoTransform[0] + -2 * adfGeoTransform[1] + -2 * adfGeoTransform[2],
        adfGeoTransform[3] + -2 * adfGeoTransform[4] + -2 * adfGeoTransform[5]);

    auto poUniversePoly = std::make_unique<OGRPolygon>();
    poUniversePoly->addRing(std::move(poUniverseRing));
    poInvertMP->addGeometry(std::move(poUniversePoly));

    bool bFoundNonPoly = false;
    // If we have GEOS, use it to "subtract" each polygon from the universe
    // multipolygon
    if (OGRGeometryFactory::haveGEOS())
    {
        OGRGeometry *poInvertMPAsGeom = poInvertMP;
        poInvertMP = nullptr;
        CPL_IGNORE_RET_VAL(poInvertMP);
        for (unsigned int iGeom = 0; iGeom < ahGeometries.size(); iGeom++)
        {
            auto poGeom = OGRGeometry::FromHandle(ahGeometries[iGeom]);
            const auto eGType = OGR_GT_Flatten(poGeom->getGeometryType());
            if (eGType != wkbPolygon && eGType != wkbMultiPolygon)
            {
                if (!bFoundNonPoly)
                {
                    bFoundNonPoly = true;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Ignoring non-polygon geometries in -i mode");
                }
            }
            else
            {
                auto poNewGeom = poInvertMPAsGeom->Difference(poGeom);
                if (poNewGeom)
                {
                    delete poInvertMPAsGeom;
                    poInvertMPAsGeom = poNewGeom;
                }
            }

            delete poGeom;
        }

        ahGeometries.resize(1);
        ahGeometries[0] = OGRGeometry::ToHandle(poInvertMPAsGeom);
        return;
    }

    OGRPolygon &hUniversePoly =
        *poInvertMP->getGeometryRef(poInvertMP->getNumGeometries() - 1);

    /* -------------------------------------------------------------------- */
    /*      If we don't have GEOS, add outer rings of polygons as inner     */
    /*      rings of poUniversePoly and inner rings as sub-polygons. Note   */
    /*      that this only works properly if the polygons are disjoint, in  */
    /*      the sense that the outer ring of any polygon is not inside the  */
    /*      outer ring of another one. So the scenario of                   */
    /*      https://github.com/OSGeo/gdal/issues/8689 with an "island" in   */
    /*      the middle of a hole will not work properly.                    */
    /* -------------------------------------------------------------------- */
    for (unsigned int iGeom = 0; iGeom < ahGeometries.size(); iGeom++)
    {
        const auto eGType =
            OGR_GT_Flatten(OGR_G_GetGeometryType(ahGeometries[iGeom]));
        if (eGType != wkbPolygon && eGType != wkbMultiPolygon)
        {
            if (!bFoundNonPoly)
            {
                bFoundNonPoly = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Ignoring non-polygon geometries in -i mode");
            }
            OGR_G_DestroyGeometry(ahGeometries[iGeom]);
            continue;
        }

        const auto ProcessPoly =
            [&hUniversePoly, poInvertMP](OGRPolygon *poPoly)
        {
            for (int i = poPoly->getNumInteriorRings() - 1; i >= 0; --i)
            {
                auto poNewPoly = std::make_unique<OGRPolygon>();
                std::unique_ptr<OGRLinearRing> poRing(
                    poPoly->stealInteriorRing(i));
                poNewPoly->addRing(std::move(poRing));
                poInvertMP->addGeometry(std::move(poNewPoly));
            }
            std::unique_ptr<OGRLinearRing> poShell(poPoly->stealExteriorRing());
            hUniversePoly.addRing(std::move(poShell));
        };

        if (eGType == wkbPolygon)
        {
            auto poPoly =
                OGRGeometry::FromHandle(ahGeometries[iGeom])->toPolygon();
            ProcessPoly(poPoly);
            delete poPoly;
        }
        else
        {
            auto poMulti =
                OGRGeometry::FromHandle(ahGeometries[iGeom])->toMultiPolygon();
            for (auto *poPoly : *poMulti)
            {
                ProcessPoly(poPoly);
            }
            delete poMulti;
        }
    }

    ahGeometries.resize(1);
    ahGeometries[0] = OGRGeometry::ToHandle(poInvertMP);
}

/************************************************************************/
/*                            ProcessLayer()                            */
/*                                                                      */
/*      Process all the features in a layer selection, collecting       */
/*      geometries and burn values.                                     */
/************************************************************************/

static CPLErr ProcessLayer(OGRLayerH hSrcLayer, bool bSRSIsSet,
                           GDALDatasetH hDstDS,
                           const std::vector<int> &anBandList,
                           const std::vector<double> &adfBurnValues, bool b3D,
                           bool bInverse, const std::string &osBurnAttribute,
                           CSLConstList papszRasterizeOptions,
                           CSLConstList papszTO, GDALProgressFunc pfnProgress,
                           void *pProgressData)

{
    /* -------------------------------------------------------------------- */
    /*      Checkout that SRS are the same.                                 */
    /*      If -a_srs is specified, skip the test                           */
    /* -------------------------------------------------------------------- */
    OGRCoordinateTransformationH hCT = nullptr;
    if (!bSRSIsSet)
    {
        OGRSpatialReferenceH hDstSRS = GDALGetSpatialRef(hDstDS);

        if (hDstSRS)
            hDstSRS = OSRClone(hDstSRS);
        else if (GDALGetMetadata(hDstDS, "RPC") != nullptr)
        {
            hDstSRS = OSRNewSpatialReference(nullptr);
            CPL_IGNORE_RET_VAL(
                OSRSetFromUserInput(hDstSRS, SRS_WKT_WGS84_LAT_LONG));
            OSRSetAxisMappingStrategy(hDstSRS, OAMS_TRADITIONAL_GIS_ORDER);
        }

        OGRSpatialReferenceH hSrcSRS = OGR_L_GetSpatialRef(hSrcLayer);
        if (hDstSRS != nullptr && hSrcSRS != nullptr)
        {
            if (OSRIsSame(hSrcSRS, hDstSRS) == FALSE)
            {
                hCT = OCTNewCoordinateTransformation(hSrcSRS, hDstSRS);
                if (hCT == nullptr)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The output raster dataset and the input vector "
                             "layer do not have the same SRS.\n"
                             "And reprojection of input data did not work. "
                             "Results might be incorrect.");
                }
            }
        }
        else if (hDstSRS != nullptr && hSrcSRS == nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "The output raster dataset has a SRS, but the input "
                     "vector layer SRS is unknown.\n"
                     "Ensure input vector has the same SRS, otherwise results "
                     "might be incorrect.");
        }
        else if (hDstSRS == nullptr && hSrcSRS != nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "The input vector layer has a SRS, but the output raster "
                     "dataset SRS is unknown.\n"
                     "Ensure output raster dataset has the same SRS, otherwise "
                     "results might be incorrect.");
        }

        if (hDstSRS != nullptr)
        {
            OSRDestroySpatialReference(hDstSRS);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Get field index, and check.                                     */
    /* -------------------------------------------------------------------- */
    int iBurnField = -1;
    bool bUseInt64 = false;
    if (!osBurnAttribute.empty())
    {
        OGRFeatureDefnH hLayerDefn = OGR_L_GetLayerDefn(hSrcLayer);
        iBurnField = OGR_FD_GetFieldIndex(hLayerDefn, osBurnAttribute.c_str());
        if (iBurnField == -1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to find field %s on layer %s.",
                     osBurnAttribute.c_str(),
                     OGR_FD_GetName(OGR_L_GetLayerDefn(hSrcLayer)));
            if (hCT != nullptr)
                OCTDestroyCoordinateTransformation(hCT);
            return CE_Failure;
        }
        if (OGR_Fld_GetType(OGR_FD_GetFieldDefn(hLayerDefn, iBurnField)) ==
            OFTInteger64)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, anBandList[0]);
            if (hBand && GDALGetRasterDataType(hBand) == GDT_Int64)
            {
                bUseInt64 = true;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Collect the geometries from this layer, and build list of       */
    /*      burn values.                                                    */
    /* -------------------------------------------------------------------- */
    OGRFeatureH hFeat = nullptr;
    std::vector<OGRGeometryH> ahGeometries;
    std::vector<double> adfFullBurnValues;
    std::vector<int64_t> anFullBurnValues;

    OGR_L_ResetReading(hSrcLayer);

    while ((hFeat = OGR_L_GetNextFeature(hSrcLayer)) != nullptr)
    {
        OGRGeometryH hGeom = OGR_F_StealGeometry(hFeat);
        if (hGeom == nullptr)
        {
            OGR_F_Destroy(hFeat);
            continue;
        }

        if (hCT != nullptr)
        {
            if (OGR_G_Transform(hGeom, hCT) != OGRERR_NONE)
            {
                OGR_F_Destroy(hFeat);
                OGR_G_DestroyGeometry(hGeom);
                continue;
            }
        }
        ahGeometries.push_back(hGeom);

        for (unsigned int iBand = 0; iBand < anBandList.size(); iBand++)
        {
            if (!adfBurnValues.empty())
                adfFullBurnValues.push_back(adfBurnValues[std::min(
                    iBand,
                    static_cast<unsigned int>(adfBurnValues.size()) - 1)]);
            else if (!osBurnAttribute.empty())
            {
                if (bUseInt64)
                    anFullBurnValues.push_back(
                        OGR_F_GetFieldAsInteger64(hFeat, iBurnField));
                else
                    adfFullBurnValues.push_back(
                        OGR_F_GetFieldAsDouble(hFeat, iBurnField));
            }
            else if (b3D)
            {
                /* Points and Lines will have their "z" values collected at the
                   point and line levels respectively. Not implemented for
                   polygons */
                adfFullBurnValues.push_back(0.0);
            }
        }

        OGR_F_Destroy(hFeat);
    }

    if (hCT != nullptr)
        OCTDestroyCoordinateTransformation(hCT);

    /* -------------------------------------------------------------------- */
    /*      If we are in inverse mode, we add one extra ring around the     */
    /*      whole dataset to invert the concept of insideness and then      */
    /*      merge everything into one geometry collection.                  */
    /* -------------------------------------------------------------------- */
    if (bInverse)
    {
        if (ahGeometries.empty())
        {
            for (unsigned int iBand = 0; iBand < anBandList.size(); iBand++)
            {
                if (!adfBurnValues.empty())
                    adfFullBurnValues.push_back(adfBurnValues[std::min(
                        iBand,
                        static_cast<unsigned int>(adfBurnValues.size()) - 1)]);
                else /* FIXME? Not sure what to do exactly in the else case, but
                        we must insert a value */
                {
                    adfFullBurnValues.push_back(0.0);
                    anFullBurnValues.push_back(0);
                }
            }
        }

        InvertGeometries(hDstDS, ahGeometries);
    }

    /* -------------------------------------------------------------------- */
    /*      If we have transformer options, create the transformer here     */
    /*      Coordinate transformation to the target SRS has already been    */
    /*      done, so we just need to convert to target raster space.        */
    /*      Note: this is somewhat identical to what is done in             */
    /*      GDALRasterizeGeometries() itself, except we can pass transformer*/
    /*      options.                                                        */
    /* -------------------------------------------------------------------- */

    void *pTransformArg = nullptr;
    GDALTransformerFunc pfnTransformer = nullptr;
    CPLErr eErr = CE_None;
    if (papszTO != nullptr)
    {
        GDALDataset *poDS = GDALDataset::FromHandle(hDstDS);
        char **papszTransformerOptions = CSLDuplicate(papszTO);
        double adfGeoTransform[6] = {0.0};
        if (poDS->GetGeoTransform(adfGeoTransform) != CE_None &&
            poDS->GetGCPCount() == 0 && poDS->GetMetadata("RPC") == nullptr)
        {
            papszTransformerOptions = CSLSetNameValue(
                papszTransformerOptions, "DST_METHOD", "NO_GEOTRANSFORM");
        }

        pTransformArg = GDALCreateGenImgProjTransformer2(
            nullptr, hDstDS, papszTransformerOptions);
        CSLDestroy(papszTransformerOptions);

        pfnTransformer = GDALGenImgProjTransform;
        if (pTransformArg == nullptr)
        {
            eErr = CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Perform the burn.                                               */
    /* -------------------------------------------------------------------- */
    if (eErr == CE_None)
    {
        if (bUseInt64)
        {
            eErr = GDALRasterizeGeometriesInt64(
                hDstDS, static_cast<int>(anBandList.size()), anBandList.data(),
                static_cast<int>(ahGeometries.size()), ahGeometries.data(),
                pfnTransformer, pTransformArg, anFullBurnValues.data(),
                papszRasterizeOptions, pfnProgress, pProgressData);
        }
        else
        {
            eErr = GDALRasterizeGeometries(
                hDstDS, static_cast<int>(anBandList.size()), anBandList.data(),
                static_cast<int>(ahGeometries.size()), ahGeometries.data(),
                pfnTransformer, pTransformArg, adfFullBurnValues.data(),
                papszRasterizeOptions, pfnProgress, pProgressData);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */

    if (pTransformArg)
        GDALDestroyTransformer(pTransformArg);

    for (int iGeom = static_cast<int>(ahGeometries.size()) - 1; iGeom >= 0;
         iGeom--)
        OGR_G_DestroyGeometry(ahGeometries[iGeom]);

    return eErr;
}

/************************************************************************/
/*                  CreateOutputDataset()                               */
/************************************************************************/

static GDALDatasetH CreateOutputDataset(
    const std::vector<OGRLayerH> &ahLayers, OGRSpatialReferenceH hSRS,
    OGREnvelope sEnvelop, GDALDriverH hDriver, const char *pszDest, int nXSize,
    int nYSize, double dfXRes, double dfYRes, bool bTargetAlignedPixels,
    int nBandCount, GDALDataType eOutputType, CSLConstList papszCreationOptions,
    const std::vector<double> &adfInitVals, const char *pszNoData)
{
    bool bFirstLayer = true;
    char *pszWKT = nullptr;
    const bool bBoundsSpecifiedByUser = sEnvelop.IsInit();

    for (unsigned int i = 0; i < ahLayers.size(); i++)
    {
        OGRLayerH hLayer = ahLayers[i];

        if (!bBoundsSpecifiedByUser)
        {
            OGREnvelope sLayerEnvelop;

            if (OGR_L_GetExtent(hLayer, &sLayerEnvelop, TRUE) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot get layer extent");
                return nullptr;
            }

            /* Voluntarily increase the extent by a half-pixel size to avoid */
            /* missing points on the border */
            if (!bTargetAlignedPixels && dfXRes != 0 && dfYRes != 0)
            {
                sLayerEnvelop.MinX -= dfXRes / 2;
                sLayerEnvelop.MaxX += dfXRes / 2;
                sLayerEnvelop.MinY -= dfYRes / 2;
                sLayerEnvelop.MaxY += dfYRes / 2;
            }

            sEnvelop.Merge(sLayerEnvelop);
        }

        if (bFirstLayer)
        {
            if (hSRS == nullptr)
                hSRS = OGR_L_GetSpatialRef(hLayer);

            bFirstLayer = false;
        }
    }

    if (!sEnvelop.IsInit())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not determine bounds");
        return nullptr;
    }

    if (dfXRes == 0 && dfYRes == 0)
    {
        if (nXSize == 0 || nYSize == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Size and resolution are missing");
            return nullptr;
        }
        dfXRes = (sEnvelop.MaxX - sEnvelop.MinX) / nXSize;
        dfYRes = (sEnvelop.MaxY - sEnvelop.MinY) / nYSize;
    }
    else if (bTargetAlignedPixels && dfXRes != 0 && dfYRes != 0)
    {
        sEnvelop.MinX = floor(sEnvelop.MinX / dfXRes) * dfXRes;
        sEnvelop.MaxX = ceil(sEnvelop.MaxX / dfXRes) * dfXRes;
        sEnvelop.MinY = floor(sEnvelop.MinY / dfYRes) * dfYRes;
        sEnvelop.MaxY = ceil(sEnvelop.MaxY / dfYRes) * dfYRes;
    }

    if (dfXRes == 0 || dfYRes == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not determine bounds");
        return nullptr;
    }

    double adfProjection[6] = {sEnvelop.MinX, dfXRes, 0.0,
                               sEnvelop.MaxY, 0.0,    -dfYRes};

    if (nXSize == 0 && nYSize == 0)
    {
        // coverity[divide_by_zero]
        const double dfXSize = 0.5 + (sEnvelop.MaxX - sEnvelop.MinX) / dfXRes;
        // coverity[divide_by_zero]
        const double dfYSize = 0.5 + (sEnvelop.MaxY - sEnvelop.MinY) / dfYRes;
        if (dfXSize > std::numeric_limits<int>::max() ||
            dfXSize < std::numeric_limits<int>::min() ||
            dfYSize > std::numeric_limits<int>::max() ||
            dfYSize < std::numeric_limits<int>::min())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid computed output raster size: %f x %f", dfXSize,
                     dfYSize);
            return nullptr;
        }
        nXSize = static_cast<int>(dfXSize);
        nYSize = static_cast<int>(dfYSize);
    }

    GDALDatasetH hDstDS =
        GDALCreate(hDriver, pszDest, nXSize, nYSize, nBandCount, eOutputType,
                   papszCreationOptions);
    if (hDstDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s", pszDest);
        return nullptr;
    }

    GDALSetGeoTransform(hDstDS, adfProjection);

    if (hSRS)
        OSRExportToWkt(hSRS, &pszWKT);
    if (pszWKT)
        GDALSetProjection(hDstDS, pszWKT);
    CPLFree(pszWKT);

    /*if( nBandCount == 3 || nBandCount == 4 )
    {
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALSetRasterColorInterpretation(hBand,
    (GDALColorInterp)(GCI_RedBand + iBand));
        }
    }*/

    if (pszNoData)
    {
        for (int iBand = 0; iBand < nBandCount; iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            if (GDALGetRasterDataType(hBand) == GDT_Int64)
                GDALSetRasterNoDataValueAsInt64(hBand,
                                                CPLAtoGIntBig(pszNoData));
            else
                GDALSetRasterNoDataValue(hBand, CPLAtof(pszNoData));
        }
    }

    if (!adfInitVals.empty())
    {
        for (int iBand = 0;
             iBand < std::min(nBandCount, static_cast<int>(adfInitVals.size()));
             iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALFillRaster(hBand, adfInitVals[iBand], 0);
        }
    }

    return hDstDS;
}

/************************************************************************/
/*                             GDALRasterize()                          */
/************************************************************************/

/* clang-format off */
/**
 * Burns vector geometries into a raster
 *
 * This is the equivalent of the
 * <a href="/programs/gdal_rasterize.html">gdal_rasterize</a> utility.
 *
 * GDALRasterizeOptions* must be allocated and freed with
 * GDALRasterizeOptionsNew() and GDALRasterizeOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALRasterizeOptionsNew()
 * or NULL.
 * @param pbUsageError pointer to an integer output variable to store if any
 * usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */
/* clang-format on */

GDALDatasetH GDALRasterize(const char *pszDest, GDALDatasetH hDstDS,
                           GDALDatasetH hSrcDataset,
                           const GDALRasterizeOptions *psOptionsIn,
                           int *pbUsageError)
{
    if (pszDest == nullptr && hDstDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pszDest == NULL && hDstDS == NULL");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (hSrcDataset == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hSrcDataset== NULL");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (hDstDS != nullptr && psOptionsIn && psOptionsIn->bCreateOutput)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "hDstDS != NULL but options that imply creating a new dataset "
                 "have been set.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    std::unique_ptr<GDALRasterizeOptions, decltype(&GDALRasterizeOptionsFree)>
        psOptionsToFree(nullptr, GDALRasterizeOptionsFree);
    const GDALRasterizeOptions *psOptions = psOptionsIn;
    if (psOptions == nullptr)
    {
        psOptionsToFree.reset(GDALRasterizeOptionsNew(nullptr, nullptr));
        psOptions = psOptionsToFree.get();
    }

    const bool bCloseOutDSOnError = hDstDS == nullptr;
    if (pszDest == nullptr)
        pszDest = GDALGetDescription(hDstDS);

    if (psOptions->osSQL.empty() && psOptions->aosLayers.empty() &&
        GDALDatasetGetLayerCount(hSrcDataset) != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Neither -sql nor -l are specified, but the source dataset "
                 "has not one single layer.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Open target raster file.  Eventually we will add optional       */
    /*      creation.                                                       */
    /* -------------------------------------------------------------------- */
    const bool bCreateOutput = psOptions->bCreateOutput || hDstDS == nullptr;

    GDALDriverH hDriver = nullptr;
    if (bCreateOutput)
    {
        CPLString osFormat;
        if (psOptions->osFormat.empty())
        {
            osFormat = GetOutputDriverForRaster(pszDest);
            if (osFormat.empty())
            {
                return nullptr;
            }
        }
        else
        {
            osFormat = psOptions->osFormat;
        }

        /* --------------------------------------------------------------------
         */
        /*      Find the output driver. */
        /* --------------------------------------------------------------------
         */
        hDriver = GDALGetDriverByName(osFormat);
        char **papszDriverMD =
            hDriver ? GDALGetMetadata(hDriver, nullptr) : nullptr;
        if (hDriver == nullptr ||
            !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_RASTER,
                                              "FALSE")) ||
            !CPLTestBool(
                CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Output driver `%s' not recognised or does not support "
                     "direct output file creation.",
                     osFormat.c_str());
            return nullptr;
        }
    }

    const auto GetOutputDataType = [&](OGRLayerH hLayer)
    {
        CPLAssert(bCreateOutput);
        CPLAssert(hDriver);
        GDALDataType eOutputType = psOptions->eOutputType;
        if (eOutputType == GDT_Unknown && !psOptions->osBurnAttribute.empty())
        {
            OGRFeatureDefnH hLayerDefn = OGR_L_GetLayerDefn(hLayer);
            const int iBurnField = OGR_FD_GetFieldIndex(
                hLayerDefn, psOptions->osBurnAttribute.c_str());
            if (iBurnField >= 0 && OGR_Fld_GetType(OGR_FD_GetFieldDefn(
                                       hLayerDefn, iBurnField)) == OFTInteger64)
            {
                const char *pszMD = GDALGetMetadataItem(
                    hDriver, GDAL_DMD_CREATIONDATATYPES, nullptr);
                if (pszMD && CPLStringList(CSLTokenizeString2(pszMD, " ", 0))
                                     .FindString("Int64") >= 0)
                {
                    eOutputType = GDT_Int64;
                }
            }
        }
        if (eOutputType == GDT_Unknown)
        {
            eOutputType = GDT_Float64;
        }
        return eOutputType;
    };

    // Store SRS handle
    OGRSpatialReferenceH hSRS =
        psOptions->oOutputSRS.IsEmpty()
            ? nullptr
            : OGRSpatialReference::ToHandle(
                  const_cast<OGRSpatialReference *>(&psOptions->oOutputSRS));

    /* -------------------------------------------------------------------- */
    /*      Process SQL request.                                            */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_Failure;

    if (!psOptions->osSQL.empty())
    {
        OGRLayerH hLayer =
            GDALDatasetExecuteSQL(hSrcDataset, psOptions->osSQL.c_str(),
                                  nullptr, psOptions->osDialect.c_str());
        if (hLayer != nullptr)
        {
            if (bCreateOutput)
            {
                std::vector<OGRLayerH> ahLayers;
                ahLayers.push_back(hLayer);

                const GDALDataType eOutputType = GetOutputDataType(hLayer);
                hDstDS = CreateOutputDataset(
                    ahLayers, hSRS, psOptions->sEnvelop, hDriver, pszDest,
                    psOptions->nXSize, psOptions->nYSize, psOptions->dfXRes,
                    psOptions->dfYRes, psOptions->bTargetAlignedPixels,
                    static_cast<int>(psOptions->anBandList.size()), eOutputType,
                    psOptions->aosCreationOptions, psOptions->adfInitVals,
                    psOptions->osNoData.c_str());
                if (hDstDS == nullptr)
                {
                    GDALDatasetReleaseResultSet(hSrcDataset, hLayer);
                    return nullptr;
                }
            }

            eErr = ProcessLayer(
                hLayer, hSRS != nullptr, hDstDS, psOptions->anBandList,
                psOptions->adfBurnValues, psOptions->b3D, psOptions->bInverse,
                psOptions->osBurnAttribute.c_str(),
                psOptions->aosRasterizeOptions, psOptions->aosTO,
                psOptions->pfnProgress, psOptions->pProgressData);

            GDALDatasetReleaseResultSet(hSrcDataset, hLayer);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create output file if necessary.                                */
    /* -------------------------------------------------------------------- */
    const int nLayerCount =
        (psOptions->osSQL.empty() && psOptions->aosLayers.empty())
            ? 1
            : static_cast<int>(psOptions->aosLayers.size());

    if (bCreateOutput && hDstDS == nullptr)
    {
        std::vector<OGRLayerH> ahLayers;

        GDALDataType eOutputType = psOptions->eOutputType;

        for (int i = 0; i < nLayerCount; i++)
        {
            OGRLayerH hLayer;
            if (psOptions->aosLayers.size() > static_cast<size_t>(i))
                hLayer = GDALDatasetGetLayerByName(
                    hSrcDataset, psOptions->aosLayers[i].c_str());
            else
                hLayer = GDALDatasetGetLayer(hSrcDataset, 0);
            if (hLayer == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to find layer \"%s\".",
                         psOptions->aosLayers.size() > static_cast<size_t>(i)
                             ? psOptions->aosLayers[i].c_str()
                             : "0");
                return nullptr;
            }
            if (eOutputType == GDT_Unknown)
            {
                if (GetOutputDataType(hLayer) == GDT_Int64)
                    eOutputType = GDT_Int64;
            }

            ahLayers.push_back(hLayer);
        }

        if (eOutputType == GDT_Unknown)
        {
            eOutputType = GDT_Float64;
        }

        hDstDS = CreateOutputDataset(
            ahLayers, hSRS, psOptions->sEnvelop, hDriver, pszDest,
            psOptions->nXSize, psOptions->nYSize, psOptions->dfXRes,
            psOptions->dfYRes, psOptions->bTargetAlignedPixels,
            static_cast<int>(psOptions->anBandList.size()), eOutputType,
            psOptions->aosCreationOptions, psOptions->adfInitVals,
            psOptions->osNoData.c_str());
        if (hDstDS == nullptr)
        {
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Process each layer.                                             */
    /* -------------------------------------------------------------------- */

    for (int i = 0; i < nLayerCount; i++)
    {
        OGRLayerH hLayer;
        if (psOptions->aosLayers.size() > static_cast<size_t>(i))
            hLayer = GDALDatasetGetLayerByName(hSrcDataset,
                                               psOptions->aosLayers[i].c_str());
        else
            hLayer = GDALDatasetGetLayer(hSrcDataset, 0);
        if (hLayer == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to find layer \"%s\".",
                     psOptions->aosLayers.size() > static_cast<size_t>(i)
                         ? psOptions->aosLayers[i].c_str()
                         : "0");
            eErr = CE_Failure;
            break;
        }

        if (!psOptions->osWHERE.empty())
        {
            if (OGR_L_SetAttributeFilter(hLayer, psOptions->osWHERE.c_str()) !=
                OGRERR_NONE)
            {
                eErr = CE_Failure;
                break;
            }
        }

        void *pScaledProgress = GDALCreateScaledProgress(
            0.0, 1.0 * (i + 1) / nLayerCount, psOptions->pfnProgress,
            psOptions->pProgressData);

        eErr = ProcessLayer(hLayer, !psOptions->oOutputSRS.IsEmpty(), hDstDS,
                            psOptions->anBandList, psOptions->adfBurnValues,
                            psOptions->b3D, psOptions->bInverse,
                            psOptions->osBurnAttribute.c_str(),
                            psOptions->aosRasterizeOptions, psOptions->aosTO,
                            GDALScaledProgress, pScaledProgress);

        GDALDestroyScaledProgress(pScaledProgress);
        if (eErr != CE_None)
            break;
    }

    if (eErr != CE_None)
    {
        if (bCloseOutDSOnError)
            GDALClose(hDstDS);
        return nullptr;
    }

    return hDstDS;
}

/************************************************************************/
/*                       ArgIsNumericRasterize()                        */
/************************************************************************/

static bool ArgIsNumericRasterize(const char *pszArg)

{
    char *pszEnd = nullptr;
    CPLStrtod(pszArg, &pszEnd);
    return pszEnd != nullptr && pszEnd[0] == '\0';
}

/************************************************************************/
/*                           GDALRasterizeOptionsNew()                  */
/************************************************************************/

/**
 * Allocates a GDALRasterizeOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdal_rasterize.html">gdal_rasterize</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALRasterizeOptionsForBinaryNew() prior to this
 * function. Will be filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALRasterizeOptions struct. Must be freed
 * with GDALRasterizeOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALRasterizeOptions *
GDALRasterizeOptionsNew(char **papszArgv,
                        GDALRasterizeOptionsForBinary *psOptionsForBinary)
{

    auto psOptions = std::make_unique<GDALRasterizeOptions>();

    /*-------------------------------------------------------------------- */
    /*      Parse arguments.                                               */
    /*-------------------------------------------------------------------- */

    CPLStringList aosArgv;

    /* -------------------------------------------------------------------- */
    /*      Pre-processing for custom syntax that ArgumentParser does not   */
    /*      support.                                                        */
    /* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for (int i = 0; i < argc && papszArgv != nullptr && papszArgv[i] != nullptr;
         i++)
    {
        // argparser will be confused if the value of a string argument
        // starts with a negative sign.
        if (EQUAL(papszArgv[i], "-a_nodata") && papszArgv[i + 1])
        {
            ++i;
            const std::string s = papszArgv[i];
            psOptions->osNoData = s;
            psOptions->bCreateOutput = true;
        }

        // argparser is confused by arguments that have at_least_one
        // cardinality, if they immediately precede positional arguments.
        else if (EQUAL(papszArgv[i], "-burn") && papszArgv[i + 1])
        {
            if (strchr(papszArgv[i + 1], ' '))
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString(papszArgv[i + 1]));
                for (const char *pszToken : aosTokens)
                {
                    psOptions->adfBurnValues.push_back(CPLAtof(pszToken));
                }
                i += 1;
            }
            else
            {
                while (i < argc - 1 && ArgIsNumericRasterize(papszArgv[i + 1]))
                {
                    psOptions->adfBurnValues.push_back(
                        CPLAtof(papszArgv[i + 1]));
                    i += 1;
                }
            }

            // Dummy value to make argparse happy, as at least one of
            // -burn, -a or -3d is required
            aosArgv.AddString("-burn");
            aosArgv.AddString("0");
        }
        else if (EQUAL(papszArgv[i], "-init") && papszArgv[i + 1])
        {
            if (strchr(papszArgv[i + 1], ' '))
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString(papszArgv[i + 1]));
                for (const char *pszToken : aosTokens)
                {
                    psOptions->adfInitVals.push_back(CPLAtof(pszToken));
                }
                i += 1;
            }
            else
            {
                while (i < argc - 1 && ArgIsNumericRasterize(papszArgv[i + 1]))
                {
                    psOptions->adfInitVals.push_back(CPLAtof(papszArgv[i + 1]));
                    i += 1;
                }
            }
            psOptions->bCreateOutput = true;
        }
        else if (EQUAL(papszArgv[i], "-b") && papszArgv[i + 1])
        {
            if (strchr(papszArgv[i + 1], ' '))
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString(papszArgv[i + 1]));
                for (const char *pszToken : aosTokens)
                {
                    psOptions->anBandList.push_back(atoi(pszToken));
                }
                i += 1;
            }
            else
            {
                while (i < argc - 1 && ArgIsNumericRasterize(papszArgv[i + 1]))
                {
                    psOptions->anBandList.push_back(atoi(papszArgv[i + 1]));
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
            GDALRasterizeOptionsGetParser(psOptions.get(), psOptionsForBinary);
        argParser->parse_args_without_binary_name(aosArgv.List());

        // Check all no store_into args
        if (auto oTe = argParser->present<std::vector<double>>("-te"))
        {
            psOptions->sEnvelop.MinX = oTe.value()[0];
            psOptions->sEnvelop.MinY = oTe.value()[1];
            psOptions->sEnvelop.MaxX = oTe.value()[2];
            psOptions->sEnvelop.MaxY = oTe.value()[3];
            psOptions->bCreateOutput = true;
        }

        if (auto oTr = argParser->present<std::vector<double>>("-tr"))
        {
            psOptions->dfXRes = oTr.value()[0];
            psOptions->dfYRes = oTr.value()[1];

            if (psOptions->dfXRes <= 0 || psOptions->dfYRes <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for -tr parameter.");
                return nullptr;
            }

            psOptions->bCreateOutput = true;
        }

        if (auto oTs = argParser->present<std::vector<double>>("-ts"))
        {
            const int nXSize = static_cast<int>(oTs.value()[0]);
            const int nYSize = static_cast<int>(oTs.value()[1]);

            // Warn the user if the conversion to int looses precision
            if (nXSize != oTs.value()[0] || nYSize != oTs.value()[1])
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "-ts values parsed as %d %d.", nXSize, nYSize);
            }

            psOptions->nXSize = nXSize;
            psOptions->nYSize = nYSize;

            if (psOptions->nXSize <= 0 || psOptions->nYSize <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for -ts parameter.");
                return nullptr;
            }

            psOptions->bCreateOutput = true;
        }

        if (psOptions->bCreateOutput)
        {
            if (psOptions->dfXRes == 0 && psOptions->dfYRes == 0 &&
                psOptions->nXSize == 0 && psOptions->nYSize == 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "'-tr xres yres' or '-ts xsize ysize' is required.");
                return nullptr;
            }

            if (psOptions->bTargetAlignedPixels && psOptions->dfXRes == 0 &&
                psOptions->dfYRes == 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-tap option cannot be used without using -tr.");
                return nullptr;
            }

            if (!psOptions->anBandList.empty())
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "-b option cannot be used when creating a GDAL dataset.");
                return nullptr;
            }

            int nBandCount = 1;

            if (!psOptions->adfBurnValues.empty())
                nBandCount = static_cast<int>(psOptions->adfBurnValues.size());

            if (static_cast<int>(psOptions->adfInitVals.size()) > nBandCount)
                nBandCount = static_cast<int>(psOptions->adfInitVals.size());

            if (psOptions->adfInitVals.size() == 1)
            {
                for (int i = 1; i <= nBandCount - 1; i++)
                    psOptions->adfInitVals.push_back(psOptions->adfInitVals[0]);
            }

            for (int i = 1; i <= nBandCount; i++)
                psOptions->anBandList.push_back(i);
        }
        else
        {
            if (psOptions->anBandList.empty())
                psOptions->anBandList.push_back(1);
        }

        if (!psOptions->osDialect.empty() && !psOptions->osWHERE.empty() &&
            !psOptions->osSQL.empty())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-dialect is ignored with -where. Use -sql instead");
        }

        if (psOptionsForBinary)
        {
            psOptionsForBinary->bCreateOutput = psOptions->bCreateOutput;
            if (!psOptions->osFormat.empty())
                psOptionsForBinary->osFormat = psOptions->osFormat;
        }
        else if (psOptions->adfBurnValues.empty() &&
                 psOptions->osBurnAttribute.empty() && !psOptions->b3D)
        {
            psOptions->adfBurnValues.push_back(255);
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return nullptr;
    }

    return psOptions.release();
}

/************************************************************************/
/*                       GDALRasterizeOptionsFree()                     */
/************************************************************************/

/**
 * Frees the GDALRasterizeOptions struct.
 *
 * @param psOptions the options struct for GDALRasterize().
 *
 * @since GDAL 2.1
 */

void GDALRasterizeOptionsFree(GDALRasterizeOptions *psOptions)
{
    delete psOptions;
}

/************************************************************************/
/*                  GDALRasterizeOptionsSetProgress()                   */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALRasterize().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALRasterizeOptionsSetProgress(GDALRasterizeOptions *psOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
}

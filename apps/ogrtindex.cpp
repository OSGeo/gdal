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

#include <cassert>
#include <vector>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdalargumentparser.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "commonutils.h"

typedef enum
{
    FORMAT_AUTO,
    FORMAT_WKT,
    FORMAT_EPSG,
    FORMAT_PROJ
} SrcSRSFormat;

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
/*                                main()                                */
/************************************************************************/

MAIN_START(nArgc, papszArgv)

{

    // Check strict compilation and runtime library version as we use C++ API.
    if (!GDAL_CHECK_VERSION(papszArgv[0]))
        GDALExit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

    /* -------------------------------------------------------------------- */
    /*      Processing command line arguments.                              */
    /* -------------------------------------------------------------------- */
    bool bLayersWildcarded = true;
    std::string osOutputFormat;
    std::string osTileIndexField;
    std::string osOutputName;
    bool bWriteAbsolutePath{false};
    bool bSkipDifferentProjection{false};
    char *pszCurrentPath = nullptr;
    bool bAcceptDifferentSchemas{false};
    bool bFirstWarningForNonMatchingAttributes = true;
    std::string osTargetSRS;
    bool bSetTargetSRS = false;
    std::string osSrcSRSName;
    int i_SrcSRSName = -1;
    SrcSRSFormat eSrcSRSFormat = FORMAT_AUTO;
    size_t nMaxFieldSize = 254;
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
        .action(
            [&eSrcSRSFormat](const auto &f)
            {
                if (f == "WKT")
                    eSrcSRSFormat = FORMAT_WKT;
                else if (f == "EPSG")
                    eSrcSRSFormat = FORMAT_EPSG;
                else if (f == "PROJ")
                    eSrcSRSFormat = FORMAT_PROJ;
                else
                    eSrcSRSFormat = FORMAT_AUTO;
            })
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

    CPLStringList aosArgv;

    for (int i = 0; i < nArgc; i++)
    {
        aosArgv.AddString(papszArgv[i]);
    }

    try
    {
        argParser.parse_args(aosArgv);
    }
    catch (const std::exception &e)
    {
        argParser.display_error_and_usage(e);
        GDALExit(1);
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
        GDALExit(1);
    }

    bLayersWildcarded = aosLayerNames.empty() && anLayerNumbers.empty();

    /* -------------------------------------------------------------------- */
    /*      Register format(s).                                             */
    /* -------------------------------------------------------------------- */
    OGRRegisterAll();

    /* -------------------------------------------------------------------- */
    /*      Create and validate target SRS if given.                        */
    /* -------------------------------------------------------------------- */

    OGRSpatialReference *poTargetSRS = nullptr;

    if (!osTargetSRS.empty())
    {
        if (bSkipDifferentProjection)
        {
            fprintf(stderr,
                    "Warning : -skip_different_projection does not apply "
                    "when -t_srs is requested.\n");
        }
        poTargetSRS = new OGRSpatialReference();
        poTargetSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        // coverity[tainted_data]
        if (poTargetSRS->SetFromUserInput(osTargetSRS.c_str()) != CE_None)
        {
            delete poTargetSRS;
            fprintf(stderr, "Invalid target SRS `%s'.\n", osTargetSRS.c_str());
            GDALExit(1);
        }
        bSetTargetSRS = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to open as an existing dataset for update access.           */
    /* -------------------------------------------------------------------- */
    GDALDataset *poDstDS =
        GDALDataset::FromHandle(OGROpen(osOutputName.c_str(), TRUE, nullptr));

    /* -------------------------------------------------------------------- */
    /*      If that failed, find the driver so we can create the tile index.*/
    /* -------------------------------------------------------------------- */
    OGRLayer *poDstLayer = nullptr;

    if (poDstDS == nullptr)
    {
        CPLString osFormat;
        if (osOutputFormat.empty())
        {
            const auto aoDrivers =
                GetOutputDriversFor(osOutputName.c_str(), GDAL_OF_VECTOR);
            if (aoDrivers.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot guess driver for %s", osOutputName.c_str());
                GDALExit(10);
            }
            else
            {
                if (aoDrivers.size() > 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Several drivers matching %s extension. Using %s",
                             CPLGetExtensionSafe(osOutputName.c_str()).c_str(),
                             aoDrivers[0].c_str());
                }
                osFormat = aoDrivers[0];
            }
        }
        else
        {
            osFormat = osOutputFormat;
        }

        if (!EQUAL(osFormat, "ESRI Shapefile"))
            nMaxFieldSize = 0;

        GDALDriverH hDriver = GDALGetDriverByName(osFormat.c_str());
        if (hDriver == nullptr)
        {
            fprintf(stderr, "Unable to find driver `%s'.\n", osFormat.c_str());
            fprintf(stderr, "The following drivers are available:\n");
            GDALDriverManager *poDM = GetGDALDriverManager();
            for (int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++)
            {
                GDALDriver *poIter = poDM->GetDriver(iDriver);
                char **papszDriverMD = poIter->GetMetadata();
                if (CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_VECTOR, "FALSE")) &&
                    CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
                {
                    fprintf(stderr, "  -> `%s'\n", poIter->GetDescription());
                }
            }
            GDALExit(1);
        }

        if (!CPLTestBool(CSLFetchNameValueDef(GDALGetMetadata(hDriver, nullptr),
                                              GDAL_DCAP_CREATE, "FALSE")))
        {
            fprintf(stderr,
                    "%s driver does not support data source creation.\n",
                    osFormat.c_str());
            GDALExit(1);
        }

        /* --------------------------------------------------------------------
         */
        /*      Now create it. */
        /* --------------------------------------------------------------------
         */

        poDstDS = GDALDataset::FromHandle(GDALCreate(
            hDriver, osOutputName.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
        if (poDstDS == nullptr)
        {
            fprintf(stderr, "%s driver failed to create %s\n", osFormat.c_str(),
                    osOutputName.c_str());
            GDALExit(1);
        }

        if (poDstDS->GetLayerCount() == 0)
        {

            OGRSpatialReference *poSrcSpatialRef = nullptr;
            if (bSetTargetSRS)
            {
                // Fetches the SRS from target SRS (if set), or from the SRS of
                // the first layer and use it when creating the
                // tileindex layer.
                poSrcSpatialRef = poTargetSRS->Clone();
            }
            else if (!aosSrcDatasets.empty())
            {
                GDALDataset *poDS = GDALDataset::FromHandle(
                    OGROpen(aosSrcDatasets.front().c_str(), FALSE, nullptr));
                if (poDS != nullptr)
                {
                    for (int iLayer = 0; iLayer < poDS->GetLayerCount();
                         iLayer++)
                    {
                        bool bRequested = bLayersWildcarded;
                        OGRLayer *poLayer = poDS->GetLayer(iLayer);

                        if (!bRequested)
                        {
                            if (std::find(anLayerNumbers.cbegin(),
                                          anLayerNumbers.cend(),
                                          iLayer) != anLayerNumbers.cend())
                                bRequested = true;
                            else if (std::find(
                                         aosLayerNames.cbegin(),
                                         aosLayerNames.cend(),
                                         poLayer->GetLayerDefn()->GetName()) !=
                                     aosLayerNames.cend())
                                bRequested = true;
                        }

                        if (!bRequested)
                            continue;

                        if (poLayer->GetSpatialRef())
                            poSrcSpatialRef = poLayer->GetSpatialRef()->Clone();
                        break;
                    }
                }

                GDALClose(poDS);
            }

            poDstLayer = poDstDS->CreateLayer("tileindex", poSrcSpatialRef);

            OGRFieldDefn oLocation(osTileIndexField.c_str(), OFTString);
            oLocation.SetWidth(200);
            poDstLayer->CreateField(&oLocation);

            if (!osSrcSRSName.empty())
            {
                OGRFieldDefn oSrcSRSNameField(osSrcSRSName.c_str(), OFTString);
                poDstLayer->CreateField(&oSrcSRSNameField);
            }

            if (poSrcSpatialRef)
                poSrcSpatialRef->Release();
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Identify target layer and field.                                */
    /* -------------------------------------------------------------------- */

    poDstLayer = poDstDS->GetLayer(0);
    if (poDstLayer == nullptr)
    {
        fprintf(stderr, "Can't find any layer in output tileindex!\n");
        GDALExit(1);
    }

    const int iTileIndexField =
        poDstLayer->GetLayerDefn()->GetFieldIndex(osTileIndexField.c_str());
    if (iTileIndexField == -1)
    {
        fprintf(stderr, "Can't find %s field in tile index dataset.\n",
                osTileIndexField.c_str());
        GDALExit(1);
    }

    if (!osSrcSRSName.empty())
        i_SrcSRSName =
            poDstLayer->GetLayerDefn()->GetFieldIndex(osSrcSRSName.c_str());

    OGRFeatureDefn *poFeatureDefn = nullptr;

    // Load in memory existing file names in SHP.
    char **existingLayersTab = nullptr;
    OGRSpatialReference *alreadyExistingSpatialRef = nullptr;
    bool alreadyExistingSpatialRefValid = false;
    const int nExistingLayers = static_cast<int>(poDstLayer->GetFeatureCount());
    if (nExistingLayers)
    {
        existingLayersTab =
            static_cast<char **>(CPLMalloc(nExistingLayers * sizeof(char *)));
        for (int i = 0; i < nExistingLayers; i++)
        {
            OGRFeature *feature = poDstLayer->GetNextFeature();
            existingLayersTab[i] =
                CPLStrdup(feature->GetFieldAsString(iTileIndexField));
            if (i == 0)
            {
                char *filename = CPLStrdup(existingLayersTab[i]);
                // j used after for.
                int j = static_cast<int>(strlen(filename)) - 1;
                for (; j >= 0; j--)
                {
                    if (filename[j] == ',')
                        break;
                }
                GDALDataset *poDS = nullptr;
                if (j >= 0)
                {
                    const int iLayer = atoi(filename + j + 1);
                    filename[j] = 0;
                    poDS = GDALDataset::FromHandle(
                        OGROpen(filename, FALSE, nullptr));
                    if (poDS != nullptr)
                    {
                        OGRLayer *poLayer = poDS->GetLayer(iLayer);
                        if (poLayer)
                        {
                            alreadyExistingSpatialRefValid = true;
                            alreadyExistingSpatialRef =
                                poLayer->GetSpatialRef()
                                    ? poLayer->GetSpatialRef()->Clone()
                                    : nullptr;

                            if (poFeatureDefn == nullptr)
                                poFeatureDefn =
                                    poLayer->GetLayerDefn()->Clone();
                        }
                        GDALClose(poDS);
                    }
                }
            }
        }
    }

    if (bWriteAbsolutePath)
    {
        pszCurrentPath = CPLGetCurrentDir();
        if (pszCurrentPath == nullptr)
        {
            fprintf(stderr,
                    "This system does not support the CPLGetCurrentDir call. "
                    "The option -write_absolute_path will have no effect\n");
            bWriteAbsolutePath = false;
        }
    }
    /* ==================================================================== */
    /*      Process each input datasource in turn.                          */
    /* ==================================================================== */
    for (const auto &srcDataSet : aosSrcDatasets)
    {

        char *fileNameToWrite = nullptr;
        VSIStatBuf sStatBuf;

        if (bWriteAbsolutePath && CPLIsFilenameRelative(srcDataSet.c_str()) &&
            VSIStat(srcDataSet.c_str(), &sStatBuf) == 0)
        {
            fileNameToWrite = CPLStrdup(CPLProjectRelativeFilenameSafe(
                                            pszCurrentPath, srcDataSet.c_str())
                                            .c_str());
        }
        else
        {
            fileNameToWrite = CPLStrdup(srcDataSet.c_str());
        }

        GDALDataset *poDS = GDALDataset::FromHandle(
            OGROpen(srcDataSet.c_str(), FALSE, nullptr));

        if (poDS == nullptr)
        {
            fprintf(stderr, "Failed to open dataset %s, skipping.\n",
                    srcDataSet.c_str());
            CPLFree(fileNameToWrite);
            continue;
        }

        /* ----------------------------------------------------------------- */
        /*      Check all layers, and see if they match requests.            */
        /* ----------------------------------------------------------------- */

        for (int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++)
        {
            bool bRequested = bLayersWildcarded;
            OGRLayer *poLayer = poDS->GetLayer(iLayer);

            if (!bRequested)
            {
                if (std::find(anLayerNumbers.cbegin(), anLayerNumbers.cend(),
                              iLayer) != anLayerNumbers.cend())
                    bRequested = true;
                else if (std::find(aosLayerNames.cbegin(), aosLayerNames.cend(),
                                   poLayer->GetLayerDefn()->GetName()) !=
                         aosLayerNames.cend())
                    bRequested = true;
            }

            if (!bRequested)
                continue;

            // Checks that the layer is not already in tileindex.
            int i = 0;  // Used after for.
            for (; i < nExistingLayers; i++)
            {
                // TODO(schwehr): Move this off of the stack.
                char szLocation[5000] = {};
                snprintf(szLocation, sizeof(szLocation), "%s,%d",
                         fileNameToWrite, iLayer);
                if (EQUAL(szLocation, existingLayersTab[i]))
                {
                    fprintf(stderr,
                            "Layer %d of %s is already in tileindex. "
                            "Skipping it.\n",
                            iLayer, srcDataSet.c_str());
                    break;
                }
            }
            if (i != nExistingLayers)
            {
                continue;
            }

            OGRSpatialReference *spatialRef = poLayer->GetSpatialRef();
            // If not set target srs, test that the current file uses same
            // projection as others.
            if (!bSetTargetSRS)
            {
                if (alreadyExistingSpatialRefValid)
                {
                    if ((spatialRef != nullptr &&
                         alreadyExistingSpatialRef != nullptr &&
                         spatialRef->IsSame(alreadyExistingSpatialRef) ==
                             FALSE) ||
                        ((spatialRef != nullptr) !=
                         (alreadyExistingSpatialRef != nullptr)))
                    {
                        fprintf(
                            stderr,
                            "Warning : layer %d of %s is not using the same "
                            "projection system as other files in the "
                            "tileindex. This may cause problems when using it "
                            "in MapServer for example.%s\n",
                            iLayer, srcDataSet.c_str(),
                            bSkipDifferentProjection ? " Skipping it" : "");
                        if (bSkipDifferentProjection)
                        {
                            continue;
                        }
                    }
                }
                else
                {
                    alreadyExistingSpatialRefValid = true;
                    alreadyExistingSpatialRef =
                        spatialRef ? spatialRef->Clone() : nullptr;
                }
            }

            /* ------------------------------------------------------------------ */
            /* Check if all layers in dataset have the same attributes schema.    */
            /* ------------------------------------------------------------------ */

            if (poFeatureDefn == nullptr)
            {
                poFeatureDefn = poLayer->GetLayerDefn()->Clone();
            }
            else if (!bAcceptDifferentSchemas)
            {
                OGRFeatureDefn *poFeatureDefnCur = poLayer->GetLayerDefn();
                assert(nullptr != poFeatureDefnCur);

                const int fieldCount = poFeatureDefnCur->GetFieldCount();

                if (fieldCount != poFeatureDefn->GetFieldCount())
                {
                    fprintf(stderr,
                            "Number of attributes of layer %s of %s "
                            "does not match ... skipping it.\n",
                            poLayer->GetLayerDefn()->GetName(),
                            srcDataSet.c_str());
                    if (bFirstWarningForNonMatchingAttributes)
                    {
                        fprintf(
                            stderr,
                            "Note : you can override this "
                            "behavior with -accept_different_schemas option\n"
                            "but this may result in a tileindex incompatible "
                            "with MapServer\n");
                        bFirstWarningForNonMatchingAttributes = false;
                    }
                    continue;
                }

                bool bSkip = false;
                for (int fn = 0; fn < poFeatureDefnCur->GetFieldCount(); fn++)
                {
                    OGRFieldDefn *poField = poFeatureDefn->GetFieldDefn(fn);
                    OGRFieldDefn *poFieldCur =
                        poFeatureDefnCur->GetFieldDefn(fn);

                    // XXX - Should those pointers be checked against NULL?
                    assert(nullptr != poField);
                    assert(nullptr != poFieldCur);

                    if (poField->GetType() != poFieldCur->GetType() ||
                        poField->GetWidth() != poFieldCur->GetWidth() ||
                        poField->GetPrecision() != poFieldCur->GetPrecision() ||
                        !EQUAL(poField->GetNameRef(), poFieldCur->GetNameRef()))
                    {
                        fprintf(stderr,
                                "Schema of attributes of layer %s of %s "
                                "does not match. Skipping it.\n",
                                poLayer->GetLayerDefn()->GetName(),
                                srcDataSet.c_str());
                        if (bFirstWarningForNonMatchingAttributes)
                        {
                            fprintf(
                                stderr,
                                "Note : you can override this "
                                "behavior with -accept_different_schemas "
                                "option,\nbut this may result in a tileindex "
                                "incompatible with MapServer\n");
                            bFirstWarningForNonMatchingAttributes = false;
                        }
                        bSkip = true;
                        break;
                    }
                }

                if (bSkip)
                    continue;
            }

            /* ----------------------------------------------------------------- */
            /*      Get layer extents, and create a corresponding polygon        */
            /*      geometry.                                                    */
            /* ----------------------------------------------------------------- */
            OGREnvelope sExtents;

            if (poLayer->GetExtent(&sExtents, TRUE) != OGRERR_NONE)
            {
                fprintf(stderr,
                        "GetExtent() failed on layer %s of %s, skipping.\n",
                        poLayer->GetLayerDefn()->GetName(), srcDataSet.c_str());
                continue;
            }

            OGRLinearRing oRing;
            oRing.addPoint(sExtents.MinX, sExtents.MinY);
            oRing.addPoint(sExtents.MinX, sExtents.MaxY);
            oRing.addPoint(sExtents.MaxX, sExtents.MaxY);
            oRing.addPoint(sExtents.MaxX, sExtents.MinY);
            oRing.addPoint(sExtents.MinX, sExtents.MinY);

            OGRPolygon oRegion;
            oRegion.addRing(&oRing);

            // If set target srs, do the forward transformation of all points.
            if (bSetTargetSRS && spatialRef != nullptr)
            {
                if (!spatialRef->IsSame(poTargetSRS))
                {
                    auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                        OGRCreateCoordinateTransformation(spatialRef,
                                                          poTargetSRS));
                    if (poCT == nullptr ||
                        oRegion.transform(poCT.get()) == OGRERR_FAILURE)
                    {
                        char *pszSourceWKT = nullptr;
                        spatialRef->exportToWkt(&pszSourceWKT);
                        fprintf(
                            stderr,
                            "Warning : unable to transform points from source "
                            "SRS `%s' to target SRS `%s'\n"
                            "for file `%s' - file skipped\n",
                            pszSourceWKT, osTargetSRS.c_str(),
                            srcDataSet.c_str());
                        CPLFree(pszSourceWKT);
                        continue;
                    }
                }
            }

            /* ---------------------------------------------------------------- */
            /*      Add layer to tileindex.                                     */
            /* ---------------------------------------------------------------- */
            OGRFeature oTileFeat(poDstLayer->GetLayerDefn());

            // TODO(schwehr): Move this off of the stack.
            char szLocation[5000] = {};
            snprintf(szLocation, sizeof(szLocation), "%s,%d", fileNameToWrite,
                     iLayer);
            oTileFeat.SetGeometry(&oRegion);
            oTileFeat.SetField(iTileIndexField, szLocation);

            if (i_SrcSRSName >= 0 && spatialRef != nullptr)
            {
                const char *pszAuthorityCode =
                    spatialRef->GetAuthorityCode(nullptr);
                const char *pszAuthorityName =
                    spatialRef->GetAuthorityName(nullptr);
                char *pszWKT = nullptr;
                spatialRef->exportToWkt(&pszWKT);
                if (eSrcSRSFormat == FORMAT_AUTO)
                {
                    if (pszAuthorityName != nullptr &&
                        pszAuthorityCode != nullptr)
                    {
                        oTileFeat.SetField(i_SrcSRSName,
                                           CPLSPrintf("%s:%s", pszAuthorityName,
                                                      pszAuthorityCode));
                    }
                    else if (nMaxFieldSize == 0 ||
                             strlen(pszWKT) <= nMaxFieldSize)
                    {
                        oTileFeat.SetField(i_SrcSRSName, pszWKT);
                    }
                    else
                    {
                        char *pszProj4 = nullptr;
                        if (spatialRef->exportToProj4(&pszProj4) == OGRERR_NONE)
                        {
                            oTileFeat.SetField(i_SrcSRSName, pszProj4);
                            CPLFree(pszProj4);
                        }
                        else
                        {
                            oTileFeat.SetField(i_SrcSRSName, pszWKT);
                        }
                    }
                }
                else if (eSrcSRSFormat == FORMAT_WKT)
                {
                    if (nMaxFieldSize == 0 || strlen(pszWKT) <= nMaxFieldSize)
                    {
                        oTileFeat.SetField(i_SrcSRSName, pszWKT);
                    }
                    else
                    {
                        fprintf(
                            stderr,
                            "Cannot write WKT for file %s as it is too long!\n",
                            fileNameToWrite);
                    }
                }
                else if (eSrcSRSFormat == FORMAT_PROJ)
                {
                    char *pszProj4 = nullptr;
                    if (spatialRef->exportToProj4(&pszProj4) == OGRERR_NONE)
                    {
                        oTileFeat.SetField(i_SrcSRSName, pszProj4);
                        CPLFree(pszProj4);
                    }
                }
                else if (eSrcSRSFormat == FORMAT_EPSG)
                {
                    if (pszAuthorityName != nullptr &&
                        pszAuthorityCode != nullptr)
                        oTileFeat.SetField(i_SrcSRSName,
                                           CPLSPrintf("%s:%s", pszAuthorityName,
                                                      pszAuthorityCode));
                }
                CPLFree(pszWKT);
            }
            if (poDstLayer->CreateFeature(&oTileFeat) != OGRERR_NONE)
            {
                fprintf(stderr, "Failed to create feature on tile index. "
                                "Terminating.");
                GDALClose(poDstDS);
                exit(1);
            }
        }

        /* ---------------------------------------------------------------- */
        /*      Cleanup this data source.                                   */
        /* ---------------------------------------------------------------- */
        CPLFree(fileNameToWrite);
        GDALClose(poDS);
    }

    /* -------------------------------------------------------------------- */
    /*      Close tile index and clear buffers.                             */
    /* -------------------------------------------------------------------- */
    int nRetCode = 0;
    if (GDALClose(poDstDS) != CE_None)
        nRetCode = 1;
    OGRFeatureDefn::DestroyFeatureDefn(poFeatureDefn);

    if (alreadyExistingSpatialRef != nullptr)
        alreadyExistingSpatialRef->Release();
    delete poTargetSRS;

    CPLFree(pszCurrentPath);

    if (nExistingLayers)
    {
        for (int i = 0; i < nExistingLayers; i++)
        {
            CPLFree(existingLayersTab[i]);
        }
        CPLFree(existingLayersTab);
    }

    GDALDestroy();

    return nRetCode;
}

MAIN_END

/******************************************************************************
 *
 * Project:  MapServer
 * Purpose:  Commandline App to build tile index for raster files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam, DM Solutions Group Inc
 * Copyright (c) 2007-2023, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_priv.h"
#include "gdal_utils_priv.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "ogr_spatialref.h"
#include "commonutils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

typedef enum
{
    FORMAT_AUTO,
    FORMAT_WKT,
    FORMAT_EPSG,
    FORMAT_PROJ
} SrcSRSFormat;

/************************************************************************/
/*                          GDALTileIndexOptions                        */
/************************************************************************/

struct GDALTileIndexOptions
{
    bool bOverwrite = false;
    std::string osFormat{};
    std::string osIndexLayerName{};
    std::string osLocationField = "location";
    std::string osTargetSRS{};
    bool bWriteAbsolutePath = false;
    bool bSkipDifferentProjection = false;
    std::string osSrcSRSFieldName{};
    SrcSRSFormat eSrcSRSFormat = FORMAT_AUTO;
    double xres = std::numeric_limits<double>::quiet_NaN();
    double yres = std::numeric_limits<double>::quiet_NaN();
    double xmin = std::numeric_limits<double>::quiet_NaN();
    double ymin = std::numeric_limits<double>::quiet_NaN();
    double xmax = std::numeric_limits<double>::quiet_NaN();
    double ymax = std::numeric_limits<double>::quiet_NaN();
    std::string osBandCount{};
    std::string osNodata{};
    std::string osColorInterp{};
    std::string osDataType{};
    bool bMaskBand = false;
    std::vector<std::string> aosMetadata{};
    std::string osVRTTIFilename{};
};

/************************************************************************/
/*                           GDALTileIndex()                            */
/************************************************************************/

/* clang-format off */
/**
 * Build a tile index from a list of datasets.
 *
 * This is the equivalent of the
 * <a href="/programs/gdaltindex.html">gdaltindex</a> utility.
 *
 * GDALTileIndexOptions* must be allocated and freed with
 * GDALTileIndexOptionsNew() and GDALTileIndexOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param nSrcCount the number of input datasets.
 * @param papszSrcDSNames the list of input dataset names
 * @param psOptionsIn the options struct returned by GDALTileIndexOptionsNew() or
 * NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose()) or NULL in case of error.
 *
 * @since GDAL3.9
 */
/* clang-format on */

GDALDatasetH GDALTileIndex(const char *pszDest, int nSrcCount,
                           const char *const *papszSrcDSNames,
                           const GDALTileIndexOptions *psOptionsIn,
                           int *pbUsageError)
{
    if (nSrcCount == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No input dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    auto psOptions = psOptionsIn
                         ? std::make_unique<GDALTileIndexOptions>(*psOptionsIn)
                         : std::make_unique<GDALTileIndexOptions>();

    /* -------------------------------------------------------------------- */
    /*      Create and validate target SRS if given.                        */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference oTargetSRS;
    if (!psOptions->osTargetSRS.empty())
    {
        if (psOptions->bSkipDifferentProjection)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-skip_different_projections does not apply "
                     "when -t_srs is requested.");
        }
        oTargetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        // coverity[tainted_data]
        oTargetSRS.SetFromUserInput(psOptions->osTargetSRS.c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      Open or create the target datasource                            */
    /* -------------------------------------------------------------------- */

    if (psOptions->bOverwrite)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        auto hDriver = GDALIdentifyDriver(pszDest, nullptr);
        if (hDriver)
            GDALDeleteDataset(hDriver, pszDest);
        else
            VSIUnlink(pszDest);
        CPLPopErrorHandler();
    }

    auto poTileIndexDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        pszDest, GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr));
    OGRLayer *poLayer = nullptr;
    std::string osFormat;
    int nMaxFieldSize = 254;
    bool bExistingLayer = false;

    if (poTileIndexDS != nullptr)
    {
        auto poDriver = poTileIndexDS->GetDriver();
        if (poDriver)
            osFormat = poDriver->GetDescription();

        if (poTileIndexDS->GetLayerCount() == 1)
        {
            poLayer = poTileIndexDS->GetLayer(0);
        }
        else
        {
            if (psOptions->osIndexLayerName.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "-lyr_name must be specified.");
                if (pbUsageError)
                    *pbUsageError = true;
                return nullptr;
            }
            CPLPushErrorHandler(CPLQuietErrorHandler);
            poLayer = poTileIndexDS->GetLayerByName(
                psOptions->osIndexLayerName.c_str());
            CPLPopErrorHandler();
        }
    }
    else
    {
        if (psOptions->osFormat.empty())
        {
            std::vector<CPLString> aoDrivers =
                GetOutputDriversFor(pszDest, GDAL_OF_VECTOR);
            if (aoDrivers.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot guess driver for %s", pszDest);
                return nullptr;
            }
            else
            {
                if (aoDrivers.size() > 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Several drivers matching %s extension. Using %s",
                             CPLGetExtension(pszDest), aoDrivers[0].c_str());
                }
                osFormat = aoDrivers[0];
            }
        }
        else
        {
            osFormat = psOptions->osFormat;
        }
        if (!EQUAL(osFormat.c_str(), "ESRI Shapefile"))
            nMaxFieldSize = 0;

        auto poDriver =
            GetGDALDriverManager()->GetDriverByName(osFormat.c_str());
        if (poDriver == nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined, "%s driver not available.",
                     osFormat.c_str());
            return nullptr;
        }

        poTileIndexDS.reset(
            poDriver->Create(pszDest, 0, 0, 0, GDT_Unknown, nullptr));
        if (!poTileIndexDS)
            return nullptr;
    }

    if (poLayer)
    {
        bExistingLayer = true;
    }
    else
    {
        std::string osLayerName;
        if (psOptions->osIndexLayerName.empty())
        {
            VSIStatBuf sStat;
            if (EQUAL(osFormat.c_str(), "ESRI Shapefile") ||
                VSIStat(pszDest, &sStat) == 0)
            {
                osLayerName = CPLGetBasename(pszDest);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "-lyr_name must be specified.");
                if (pbUsageError)
                    *pbUsageError = true;
                return nullptr;
            }
        }
        else
        {
            if (psOptions->bOverwrite)
            {
                for (int i = 0; i < poTileIndexDS->GetLayerCount(); ++i)
                {
                    auto poExistingLayer = poTileIndexDS->GetLayer(i);
                    if (poExistingLayer && poExistingLayer->GetName() ==
                                               psOptions->osIndexLayerName)
                    {
                        poTileIndexDS->DeleteLayer(i);
                        break;
                    }
                }
            }

            osLayerName = psOptions->osIndexLayerName;
        }

        /* get spatial reference for output file from target SRS (if set) */
        /* or from first input file */
        OGRSpatialReference oSRS;
        if (!oTargetSRS.IsEmpty())
        {
            oSRS = oTargetSRS;
        }
        else
        {
            auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                papszSrcDSNames[0], GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                nullptr, nullptr, nullptr));
            if (!poSrcDS)
                return nullptr;

            auto poSrcSRS = poSrcDS->GetSpatialRef();
            if (poSrcSRS)
                oSRS = *poSrcSRS;
        }

        poLayer = poTileIndexDS->CreateLayer(osLayerName.c_str(),
                                             oSRS.IsEmpty() ? nullptr : &oSRS,
                                             wkbPolygon, nullptr);
        if (!poLayer)
            return nullptr;

        OGRFieldDefn oLocationField(psOptions->osLocationField.c_str(),
                                    OFTString);
        oLocationField.SetWidth(nMaxFieldSize);
        if (poLayer->CreateField(&oLocationField) != OGRERR_NONE)
            return nullptr;

        if (!psOptions->osSrcSRSFieldName.empty())
        {
            OGRFieldDefn oSrcSRSField(psOptions->osSrcSRSFieldName.c_str(),
                                      OFTString);
            oSrcSRSField.SetWidth(nMaxFieldSize);
            if (poLayer->CreateField(&oSrcSRSField) != OGRERR_NONE)
                return nullptr;
        }
    }

    if (!psOptions->osVRTTIFilename.empty())
    {
        if (!psOptions->aosMetadata.empty())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "-mo is not supported when -vrtti_filename is used");
            return nullptr;
        }
        CPLXMLNode *psRoot =
            CPLCreateXMLNode(nullptr, CXT_Element, "VRTTileIndexDataset");
        CPLCreateXMLElementAndValue(psRoot, "IndexDataset", pszDest);
        CPLCreateXMLElementAndValue(psRoot, "IndexLayer", poLayer->GetName());
        CPLCreateXMLElementAndValue(psRoot, "LocationField",
                                    psOptions->osLocationField.c_str());
        if (!std::isnan(psOptions->xres))
        {
            CPLCreateXMLElementAndValue(psRoot, "ResX",
                                        CPLSPrintf("%.18g", psOptions->xres));
            CPLCreateXMLElementAndValue(psRoot, "ResY",
                                        CPLSPrintf("%.18g", psOptions->yres));
        }
        if (!std::isnan(psOptions->xmin))
        {
            CPLCreateXMLElementAndValue(psRoot, "MinX",
                                        CPLSPrintf("%.18g", psOptions->xmin));
            CPLCreateXMLElementAndValue(psRoot, "MinY",
                                        CPLSPrintf("%.18g", psOptions->ymin));
            CPLCreateXMLElementAndValue(psRoot, "MaxX",
                                        CPLSPrintf("%.18g", psOptions->xmax));
            CPLCreateXMLElementAndValue(psRoot, "MaxY",
                                        CPLSPrintf("%.18g", psOptions->ymax));
        }

        int nBandCount = 0;
        if (!psOptions->osBandCount.empty())
        {
            nBandCount = atoi(psOptions->osBandCount.c_str());
        }
        else
        {
            if (!psOptions->osDataType.empty())
            {
                nBandCount = std::max(
                    nBandCount,
                    CPLStringList(CSLTokenizeString2(
                                      psOptions->osDataType.c_str(), ", ", 0))
                        .size());
            }
            if (!psOptions->osNodata.empty())
            {
                nBandCount = std::max(
                    nBandCount,
                    CPLStringList(CSLTokenizeString2(
                                      psOptions->osNodata.c_str(), ", ", 0))
                        .size());
            }
            if (!psOptions->osColorInterp.empty())
            {
                nBandCount =
                    std::max(nBandCount,
                             CPLStringList(
                                 CSLTokenizeString2(
                                     psOptions->osColorInterp.c_str(), ", ", 0))
                                 .size());
            }
        }

        for (int i = 0; i < nBandCount; ++i)
        {
            auto psBand = CPLCreateXMLNode(psRoot, CXT_Element, "Band");
            CPLAddXMLAttributeAndValue(psBand, "band", CPLSPrintf("%d", i + 1));
            if (!psOptions->osDataType.empty())
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString2(psOptions->osDataType.c_str(), ", ", 0));
                if (aosTokens.size() == 1)
                    CPLAddXMLAttributeAndValue(psBand, "dataType",
                                               aosTokens[0]);
                else if (i < aosTokens.size())
                    CPLAddXMLAttributeAndValue(psBand, "dataType",
                                               aosTokens[i]);
            }
            if (!psOptions->osNodata.empty())
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString2(psOptions->osNodata.c_str(), ", ", 0));
                if (aosTokens.size() == 1)
                    CPLCreateXMLElementAndValue(psBand, "NoDataValue",
                                                aosTokens[0]);
                else if (i < aosTokens.size())
                    CPLCreateXMLElementAndValue(psBand, "NoDataValue",
                                                aosTokens[i]);
            }
            if (!psOptions->osColorInterp.empty())
            {
                const CPLStringList aosTokens(CSLTokenizeString2(
                    psOptions->osColorInterp.c_str(), ", ", 0));
                if (aosTokens.size() == 1)
                    CPLCreateXMLElementAndValue(psBand, "ColorInterp",
                                                aosTokens[0]);
                else if (i < aosTokens.size())
                    CPLCreateXMLElementAndValue(psBand, "ColorInterp",
                                                aosTokens[i]);
            }
        }

        if (psOptions->bMaskBand)
        {
            CPLCreateXMLElementAndValue(psRoot, "MaskBand", "true");
        }
        int res = CPLSerializeXMLTreeToFile(psRoot,
                                            psOptions->osVRTTIFilename.c_str());
        CPLDestroyXMLNode(psRoot);
        if (!res)
            return nullptr;
    }
    else
    {
        poLayer->SetMetadataItem("LOCATION_FIELD",
                                 psOptions->osLocationField.c_str());
        if (!std::isnan(psOptions->xres))
        {
            poLayer->SetMetadataItem("RESX",
                                     CPLSPrintf("%.18g", psOptions->xres));
            poLayer->SetMetadataItem("RESY",
                                     CPLSPrintf("%.18g", psOptions->yres));
        }
        if (!std::isnan(psOptions->xmin))
        {
            poLayer->SetMetadataItem("MINX",
                                     CPLSPrintf("%.18g", psOptions->xmin));
            poLayer->SetMetadataItem("MINY",
                                     CPLSPrintf("%.18g", psOptions->ymin));
            poLayer->SetMetadataItem("MAXX",
                                     CPLSPrintf("%.18g", psOptions->xmax));
            poLayer->SetMetadataItem("MAXY",
                                     CPLSPrintf("%.18g", psOptions->ymax));
        }
        if (!psOptions->osBandCount.empty())
        {
            poLayer->SetMetadataItem("BAND_COUNT",
                                     psOptions->osBandCount.c_str());
        }
        if (!psOptions->osDataType.empty())
        {
            poLayer->SetMetadataItem("DATA_TYPE",
                                     psOptions->osDataType.c_str());
        }
        if (!psOptions->osNodata.empty())
        {
            poLayer->SetMetadataItem("NODATA", psOptions->osNodata.c_str());
        }
        if (!psOptions->osColorInterp.empty())
        {
            poLayer->SetMetadataItem("COLOR_INTERPRETATION",
                                     psOptions->osColorInterp.c_str());
        }
        if (psOptions->bMaskBand)
        {
            poLayer->SetMetadataItem("MASK_BAND", "YES");
        }
        for (const auto &osNameValue : psOptions->aosMetadata)
        {
            char *pszKey = nullptr;
            const char *pszValue =
                CPLParseNameValue(osNameValue.c_str(), &pszKey);
            if (pszKey && pszValue)
                poLayer->SetMetadataItem(pszKey, pszValue);
            CPLFree(pszKey);
        }
    }

    auto poLayerDefn = poLayer->GetLayerDefn();
    const int ti_field =
        poLayerDefn->GetFieldIndex(psOptions->osLocationField.c_str());
    if (ti_field < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find field `%s' in file `%s'.",
                 psOptions->osLocationField.c_str(), pszDest);
        return nullptr;
    }

    int i_SrcSRSName = -1;
    if (!psOptions->osSrcSRSFieldName.empty())
    {
        i_SrcSRSName =
            poLayerDefn->GetFieldIndex(psOptions->osSrcSRSFieldName.c_str());
        if (i_SrcSRSName < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to find field `%s' in file `%s'.",
                     psOptions->osSrcSRSFieldName.c_str(), pszDest);
            return nullptr;
        }
    }

    // Load in memory existing file names in tile index.
    std::set<std::string> oSetExistingFiles;
    OGRSpatialReference oAlreadyExistingSRS;
    if (bExistingLayer)
    {
        for (auto &&poFeature : poLayer)
        {
            if (poFeature->IsFieldSetAndNotNull(ti_field))
            {
                if (oSetExistingFiles.empty())
                {
                    auto poSrcDS =
                        std::unique_ptr<GDALDataset>(GDALDataset::Open(
                            poFeature->GetFieldAsString(ti_field),
                            GDAL_OF_RASTER, nullptr, nullptr, nullptr));
                    if (poSrcDS)
                    {
                        auto poSrcSRS = poSrcDS->GetSpatialRef();
                        if (poSrcSRS)
                            oAlreadyExistingSRS = *poSrcSRS;
                    }
                }
                oSetExistingFiles.insert(poFeature->GetFieldAsString(ti_field));
            }
        }
    }

    std::string osCurrentPath;
    if (psOptions->bWriteAbsolutePath)
    {
        char *pszCurrentPath = CPLGetCurrentDir();
        if (pszCurrentPath)
        {
            osCurrentPath = pszCurrentPath;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "This system does not support the CPLGetCurrentDir call. "
                     "The option -bWriteAbsolutePath will have no effect.");
        }
        CPLFree(pszCurrentPath);
    }

    const bool bIsVRTTIContext =
        !std::isnan(psOptions->xres) || !std::isnan(psOptions->xmin) ||
        !psOptions->osBandCount.empty() || !psOptions->osNodata.empty() ||
        !psOptions->osColorInterp.empty() || !psOptions->osDataType.empty() ||
        psOptions->bMaskBand || !psOptions->aosMetadata.empty() ||
        !psOptions->osVRTTIFilename.empty();

    /* -------------------------------------------------------------------- */
    /*      loop over GDAL files, processing.                               */
    /* -------------------------------------------------------------------- */
    for (int iSrc = 0; iSrc < nSrcCount; ++iSrc)
    {
        std::string osFileNameToWrite;
        VSIStatBuf sStatBuf;

        // Make sure it is a file before building absolute path name.
        if (!osCurrentPath.empty() &&
            CPLIsFilenameRelative(papszSrcDSNames[iSrc]) &&
            VSIStat(papszSrcDSNames[iSrc], &sStatBuf) == 0)
        {
            osFileNameToWrite = CPLProjectRelativeFilename(
                osCurrentPath.c_str(), papszSrcDSNames[iSrc]);
        }
        else
        {
            osFileNameToWrite = papszSrcDSNames[iSrc];
        }

        // Checks that file is not already in tileindex.
        if (oSetExistingFiles.find(osFileNameToWrite) !=
            oSetExistingFiles.end())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "File %s is already in tileindex. Skipping it.",
                     osFileNameToWrite.c_str());
            continue;
        }

        auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            papszSrcDSNames[iSrc], GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
            nullptr, nullptr, nullptr));
        if (poSrcDS == nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unable to open %s, skipping.", papszSrcDSNames[iSrc]);
            continue;
        }

        double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "It appears no georeferencing is available for\n"
                     "`%s', skipping.",
                     papszSrcDSNames[iSrc]);
            continue;
        }

        auto poSrcSRS = poSrcDS->GetSpatialRef();
        // If not set target srs, test that the current file uses same
        // projection as others.
        if (oTargetSRS.IsEmpty())
        {
            if (!oAlreadyExistingSRS.IsEmpty())
            {
                if (poSrcSRS == nullptr ||
                    !poSrcSRS->IsSame(&oAlreadyExistingSRS))
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "%s is not using the same projection system "
                        "as other files in the tileindex.\n"
                        "This may cause problems when using it in MapServer "
                        "for example.\n"
                        "Use -t_srs option to set target projection system. %s",
                        papszSrcDSNames[iSrc],
                        psOptions->bSkipDifferentProjection
                            ? "Skipping this file."
                            : "");
                    if (psOptions->bSkipDifferentProjection)
                    {
                        continue;
                    }
                }
            }
            else
            {
                if (poSrcSRS)
                    oAlreadyExistingSRS = *poSrcSRS;
            }
        }

        const int nXSize = poSrcDS->GetRasterXSize();
        const int nYSize = poSrcDS->GetRasterYSize();

        double adfX[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        double adfY[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        adfX[0] = adfGeoTransform[0] + 0 * adfGeoTransform[1] +
                  0 * adfGeoTransform[2];
        adfY[0] = adfGeoTransform[3] + 0 * adfGeoTransform[4] +
                  0 * adfGeoTransform[5];

        adfX[1] = adfGeoTransform[0] + nXSize * adfGeoTransform[1] +
                  0 * adfGeoTransform[2];
        adfY[1] = adfGeoTransform[3] + nXSize * adfGeoTransform[4] +
                  0 * adfGeoTransform[5];

        adfX[2] = adfGeoTransform[0] + nXSize * adfGeoTransform[1] +
                  nYSize * adfGeoTransform[2];
        adfY[2] = adfGeoTransform[3] + nXSize * adfGeoTransform[4] +
                  nYSize * adfGeoTransform[5];

        adfX[3] = adfGeoTransform[0] + 0 * adfGeoTransform[1] +
                  nYSize * adfGeoTransform[2];
        adfY[3] = adfGeoTransform[3] + 0 * adfGeoTransform[4] +
                  nYSize * adfGeoTransform[5];

        adfX[4] = adfGeoTransform[0] + 0 * adfGeoTransform[1] +
                  0 * adfGeoTransform[2];
        adfY[4] = adfGeoTransform[3] + 0 * adfGeoTransform[4] +
                  0 * adfGeoTransform[5];

        // If set target srs, do the forward transformation of all points.
        if (!oTargetSRS.IsEmpty() && poSrcSRS)
        {
            if (!poSrcSRS->IsSame(&oTargetSRS))
            {
                auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                    OGRCreateCoordinateTransformation(poSrcSRS, &oTargetSRS));
                if (!poCT || !poCT->Transform(5, adfX, adfY, nullptr))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "unable to transform points from source "
                             "SRS `%s' to target SRS `%s' for file `%s' - file "
                             "skipped",
                             poSrcDS->GetProjectionRef(),
                             psOptions->osTargetSRS.c_str(),
                             osFileNameToWrite.c_str());
                    continue;
                }
            }
        }
        else if (bIsVRTTIContext && !oAlreadyExistingSRS.IsEmpty() &&
                 (poSrcSRS == nullptr ||
                  !poSrcSRS->IsSame(&oAlreadyExistingSRS)))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "%s is not using the same projection system "
                "as other files in the tileindex. This is not compatible of "
                "VRTTI use. Use -t_srs option to reproject tile extents "
                "to a common SRS.",
                papszSrcDSNames[iSrc]);
            return nullptr;
        }

        auto poFeature = std::make_unique<OGRFeature>(poLayerDefn);
        poFeature->SetField(ti_field, osFileNameToWrite.c_str());

        if (i_SrcSRSName >= 0 && poSrcSRS)
        {
            const char *pszAuthorityCode = poSrcSRS->GetAuthorityCode(nullptr);
            const char *pszAuthorityName = poSrcSRS->GetAuthorityName(nullptr);
            if (psOptions->eSrcSRSFormat == FORMAT_AUTO)
            {
                if (pszAuthorityName != nullptr && pszAuthorityCode != nullptr)
                {
                    poFeature->SetField(i_SrcSRSName,
                                        CPLSPrintf("%s:%s", pszAuthorityName,
                                                   pszAuthorityCode));
                }
                else if (nMaxFieldSize == 0 ||
                         strlen(poSrcDS->GetProjectionRef()) <=
                             static_cast<size_t>(nMaxFieldSize))
                {
                    poFeature->SetField(i_SrcSRSName,
                                        poSrcDS->GetProjectionRef());
                }
                else
                {
                    char *pszProj4 = nullptr;
                    if (poSrcSRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                    {
                        poFeature->SetField(i_SrcSRSName, pszProj4);
                        CPLFree(pszProj4);
                    }
                    else
                    {
                        poFeature->SetField(i_SrcSRSName,
                                            poSrcDS->GetProjectionRef());
                    }
                }
            }
            else if (psOptions->eSrcSRSFormat == FORMAT_WKT)
            {
                if (nMaxFieldSize == 0 ||
                    strlen(poSrcDS->GetProjectionRef()) <=
                        static_cast<size_t>(nMaxFieldSize))
                {
                    poFeature->SetField(i_SrcSRSName,
                                        poSrcDS->GetProjectionRef());
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot write WKT for file %s as it is too long!",
                             osFileNameToWrite.c_str());
                }
            }
            else if (psOptions->eSrcSRSFormat == FORMAT_PROJ)
            {
                char *pszProj4 = nullptr;
                if (poSrcSRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                {
                    poFeature->SetField(i_SrcSRSName, pszProj4);
                    CPLFree(pszProj4);
                }
            }
            else if (psOptions->eSrcSRSFormat == FORMAT_EPSG)
            {
                if (pszAuthorityName != nullptr && pszAuthorityCode != nullptr)
                    poFeature->SetField(i_SrcSRSName,
                                        CPLSPrintf("%s:%s", pszAuthorityName,
                                                   pszAuthorityCode));
            }
        }

        auto poPoly = std::make_unique<OGRPolygon>();
        auto poRing = std::make_unique<OGRLinearRing>();
        for (int k = 0; k < 5; k++)
            poRing->addPoint(adfX[k], adfY[k]);
        poPoly->addRingDirectly(poRing.release());
        poFeature->SetGeometryDirectly(poPoly.release());

        if (poLayer->CreateFeature(poFeature.get()) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create feature in tile index.");
            return nullptr;
        }
    }

    return GDALDataset::ToHandle(poTileIndexDS.release());
}

/************************************************************************/
/*                   CHECK_HAS_ENOUGH_ADDITIONAL_ARGS()                 */
/************************************************************************/

#ifndef CheckHasEnoughAdditionalArgs_defined
#define CheckHasEnoughAdditionalArgs_defined
static bool CheckHasEnoughAdditionalArgs(CSLConstList papszArgv, int i,
                                         int nExtraArg, int nArgc)
{
    if (i + nExtraArg >= nArgc)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "%s option requires %d argument%s", papszArgv[i], nExtraArg,
                 nExtraArg == 1 ? "" : "s");
        return false;
    }
    return true;
}
#endif

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg)                            \
    if (!CheckHasEnoughAdditionalArgs(papszArgv, iArg, nExtraArg, argc))       \
    {                                                                          \
        return nullptr;                                                        \
    }

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

static char *SanitizeSRS(const char *pszUserInput)

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = nullptr;

    CPLErrorReset();

    hSRS = OSRNewSpatialReference(nullptr);
    if (OSRSetFromUserInput(hSRS, pszUserInput) == OGRERR_NONE)
        OSRExportToWkt(hSRS, &pszResult);
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Translating SRS failed:\n%s",
                 pszUserInput);
    }

    OSRDestroySpatialReference(hSRS);

    return pszResult;
}

/************************************************************************/
/*                          GDALTileIndexOptionsNew()                   */
/************************************************************************/

/**
 * Allocates a GDALTileIndexOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdaltindex.html">gdaltindex</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdaltindex_bin.cpp use case) must be allocated with
 * GDALTileIndexOptionsForBinaryNew() prior to this function. Will be filled
 * with potentially present filename, open options,...
 * @return pointer to the allocated GDALTileIndexOptions struct. Must be freed
 * with GDALTileIndexOptionsFree().
 *
 * @since GDAL 3.9
 */

GDALTileIndexOptions *
GDALTileIndexOptionsNew(char **papszArgv,
                        GDALTileIndexOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALTileIndexOptions>();

    bool bSrcSRSFormatSpecified = false;

    /* -------------------------------------------------------------------- */
    /*      Parse arguments.                                                */
    /* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for (int iArg = 0; papszArgv != nullptr && iArg < argc; iArg++)
    {
        if ((strcmp(papszArgv[iArg], "-f") == 0 ||
             strcmp(papszArgv[iArg], "-of") == 0))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osFormat = papszArgv[++iArg];
        }
        else if (strcmp(papszArgv[iArg], "-lyr_name") == 0)
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osIndexLayerName = papszArgv[++iArg];
        }
        else if (strcmp(papszArgv[iArg], "-tileindex") == 0)
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osLocationField = papszArgv[++iArg];
        }
        else if (strcmp(papszArgv[iArg], "-t_srs") == 0)
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            char *pszSRS = SanitizeSRS(papszArgv[++iArg]);
            if (pszSRS == nullptr)
            {
                return nullptr;
            }
            psOptions->osTargetSRS = pszSRS;
            CPLFree(pszSRS);
        }
        else if (strcmp(papszArgv[iArg], "-write_absolute_path") == 0)
        {
            psOptions->bWriteAbsolutePath = true;
        }
        else if (strcmp(papszArgv[iArg], "-skip_different_projection") == 0)
        {
            psOptions->bSkipDifferentProjection = true;
        }
        else if (strcmp(papszArgv[iArg], "-src_srs_name") == 0)
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osSrcSRSFieldName = papszArgv[++iArg];
        }
        else if (strcmp(papszArgv[iArg], "-src_srs_format") == 0)
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            const char *pszFormat = papszArgv[++iArg];
            bSrcSRSFormatSpecified = true;
            if (EQUAL(pszFormat, "AUTO"))
                psOptions->eSrcSRSFormat = FORMAT_AUTO;
            else if (EQUAL(pszFormat, "WKT"))
                psOptions->eSrcSRSFormat = FORMAT_WKT;
            else if (EQUAL(pszFormat, "EPSG"))
                psOptions->eSrcSRSFormat = FORMAT_EPSG;
            else if (EQUAL(pszFormat, "PROJ"))
                psOptions->eSrcSRSFormat = FORMAT_PROJ;
            else
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Unhandled value for -src_srs_format");
                return nullptr;
            }
        }
        else if (EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet"))
        {
            if (psOptionsForBinary)
            {
                psOptionsForBinary->bQuiet = true;
            }
        }
        else if (EQUAL(papszArgv[iArg], "-tr"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(2);
            psOptions->xres = CPLAtofM(papszArgv[++iArg]);
            psOptions->yres = CPLAtofM(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-te"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            psOptions->xmin = CPLAtofM(papszArgv[++iArg]);
            psOptions->ymin = CPLAtofM(papszArgv[++iArg]);
            psOptions->xmax = CPLAtofM(papszArgv[++iArg]);
            psOptions->ymax = CPLAtofM(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-ot"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osDataType = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-mo"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->aosMetadata.push_back(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-bandcount"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osBandCount = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-nodata"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osNodata = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-colorinterp"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osColorInterp = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-mask"))
        {
            psOptions->bMaskBand = true;
        }
        else if (EQUAL(papszArgv[iArg], "-vrtti_filename"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osVRTTIFilename = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-overwrite"))
        {
            psOptions->bOverwrite = true;
        }
        else if (papszArgv[iArg][0] == '-')
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unknown option name '%s'",
                     papszArgv[iArg]);
            return nullptr;
        }
        else
        {
            if (psOptionsForBinary)
            {
                if (!psOptionsForBinary->bDestSpecified)
                {
                    psOptionsForBinary->bDestSpecified = true;
                    psOptionsForBinary->osDest = papszArgv[iArg];
                }
                else
                {
                    psOptionsForBinary->aosSrcFiles.AddString(papszArgv[iArg]);
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unexpected argument");
                return nullptr;
            }
        }
    }

    if (bSrcSRSFormatSpecified && psOptions->osTargetSRS.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-src_srs_name must be specified when -src_srs_format is "
                 "specified.");
        return nullptr;
    }

    return psOptions.release();
}

/************************************************************************/
/*                        GDALTileIndexOptionsFree()                    */
/************************************************************************/

/**
 * Frees the GDALTileIndexOptions struct.
 *
 * @param psOptions the options struct for GDALTileIndex().
 *
 * @since GDAL 3.9
 */

void GDALTileIndexOptionsFree(GDALTileIndexOptions *psOptions)
{
    delete psOptions;
}

#undef CHECK_HAS_ENOUGH_ADDITIONAL_ARGS

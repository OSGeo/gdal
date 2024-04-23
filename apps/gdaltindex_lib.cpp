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

#include <ctype.h>

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
/*                        GDALTileIndexRasterMetadata                   */
/************************************************************************/

struct GDALTileIndexRasterMetadata
{
    OGRFieldType eType = OFTString;
    std::string osFieldName{};
    std::string osRasterItemName{};
};

/************************************************************************/
/*                          GDALTileIndexOptions                        */
/************************************************************************/

struct GDALTileIndexOptions
{
    bool bOverwrite = false;
    std::string osFormat{};
    std::string osIndexLayerName{};
    std::string osLocationField = "location";
    CPLStringList aosLCO{};
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
    std::string osGTIFilename{};
    bool bRecursive = false;
    double dfMinPixelSize = std::numeric_limits<double>::quiet_NaN();
    double dfMaxPixelSize = std::numeric_limits<double>::quiet_NaN();
    std::vector<GDALTileIndexRasterMetadata> aoFetchMD{};
    std::set<std::string> oSetFilenameFilters{};
};

/************************************************************************/
/*                               PatternMatch()                         */
/************************************************************************/

static bool PatternMatch(const char *input, const char *pattern)

{
    while (*input != '\0')
    {
        if (*pattern == '\0')
            return false;

        else if (*pattern == '?')
        {
            pattern++;
            if (static_cast<unsigned int>(*input) > 127)
            {
                // Continuation bytes of such characters are of the form
                // 10xxxxxx (0x80), whereas single-byte are 0xxxxxxx
                // and the start of a multi-byte is 11xxxxxx
                do
                {
                    input++;
                } while (static_cast<unsigned int>(*input) > 127);
            }
            else
            {
                input++;
            }
        }
        else if (*pattern == '*')
        {
            if (pattern[1] == '\0')
                return true;

            // Try eating varying amounts of the input till we get a positive.
            for (int eat = 0; input[eat] != '\0'; eat++)
            {
                if (PatternMatch(input + eat, pattern + 1))
                    return true;
            }

            return false;
        }
        else
        {
            if (CPLTolower(*pattern) != CPLTolower(*input))
            {
                return false;
            }
            else
            {
                input++;
                pattern++;
            }
        }
    }

    if (*pattern != '\0' && strcmp(pattern, "*") != 0)
        return false;
    else
        return true;
}

/************************************************************************/
/*                        GDALTileIndexTileIterator                     */
/************************************************************************/

struct GDALTileIndexTileIterator
{
    const GDALTileIndexOptions *psOptions = nullptr;
    int nSrcCount = 0;
    const char *const *papszSrcDSNames = nullptr;
    std::string osCurDir{};
    int iCurSrc = 0;
    VSIDIR *psDir = nullptr;

    GDALTileIndexTileIterator(const GDALTileIndexOptions *psOptionsIn,
                              int nSrcCountIn,
                              const char *const *papszSrcDSNamesIn)
        : psOptions(psOptionsIn), nSrcCount(nSrcCountIn),
          papszSrcDSNames(papszSrcDSNamesIn)
    {
    }

    void reset()
    {
        if (psDir)
            VSICloseDir(psDir);
        psDir = nullptr;
        iCurSrc = 0;
    }

    std::string next()
    {
        while (true)
        {
            if (!psDir)
            {
                if (iCurSrc == nSrcCount)
                {
                    break;
                }

                VSIStatBufL sStatBuf;
                const std::string osCurName = papszSrcDSNames[iCurSrc++];
                if (VSIStatL(osCurName.c_str(), &sStatBuf) == 0 &&
                    VSI_ISDIR(sStatBuf.st_mode))
                {
                    auto poSrcDS = std::unique_ptr<GDALDataset>(
                        GDALDataset::Open(osCurName.c_str(), GDAL_OF_RASTER,
                                          nullptr, nullptr, nullptr));
                    if (poSrcDS)
                        return osCurName;

                    osCurDir = osCurName;
                    psDir = VSIOpenDir(
                        osCurDir.c_str(),
                        /*nDepth=*/psOptions->bRecursive ? -1 : 0, nullptr);
                    if (!psDir)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot open directory %s", osCurDir.c_str());
                        return std::string();
                    }
                }
                else
                {
                    return osCurName;
                }
            }

            auto psEntry = VSIGetNextDirEntry(psDir);
            if (!psEntry)
            {
                VSICloseDir(psDir);
                psDir = nullptr;
                continue;
            }

            if (!psOptions->oSetFilenameFilters.empty())
            {
                bool bMatchFound = false;
                const std::string osFilenameOnly =
                    CPLGetFilename(psEntry->pszName);
                for (const auto &osFilter : psOptions->oSetFilenameFilters)
                {
                    if (PatternMatch(osFilenameOnly.c_str(), osFilter.c_str()))
                    {
                        bMatchFound = true;
                        break;
                    }
                }
                if (!bMatchFound)
                    continue;
            }

            const std::string osFilename =
                CPLFormFilename(osCurDir.c_str(), psEntry->pszName, nullptr);
            if (VSI_ISDIR(psEntry->nMode))
            {
                auto poSrcDS = std::unique_ptr<GDALDataset>(
                    GDALDataset::Open(osFilename.c_str(), GDAL_OF_RASTER,
                                      nullptr, nullptr, nullptr));
                if (poSrcDS)
                    return osFilename;
                continue;
            }

            return osFilename;
        }
        return std::string();
    }
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

    GDALTileIndexTileIterator oGDALTileIndexTileIterator(
        psOptions.get(), nSrcCount, papszSrcDSNames);

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
            const auto aoDrivers = GetOutputDriversFor(pszDest, GDAL_OF_VECTOR);
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
            std::string osFilename = oGDALTileIndexTileIterator.next();
            if (osFilename.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find any tile");
                return nullptr;
            }
            oGDALTileIndexTileIterator.reset();
            auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                osFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                nullptr, nullptr, nullptr));
            if (!poSrcDS)
                return nullptr;

            auto poSrcSRS = poSrcDS->GetSpatialRef();
            if (poSrcSRS)
                oSRS = *poSrcSRS;
        }

        poLayer = poTileIndexDS->CreateLayer(
            osLayerName.c_str(), oSRS.IsEmpty() ? nullptr : &oSRS, wkbPolygon,
            psOptions->aosLCO.List());
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

    auto poLayerDefn = poLayer->GetLayerDefn();

    for (const auto &oFetchMD : psOptions->aoFetchMD)
    {
        if (poLayerDefn->GetFieldIndex(oFetchMD.osFieldName.c_str()) < 0)
        {
            OGRFieldDefn oField(oFetchMD.osFieldName.c_str(), oFetchMD.eType);
            if (poLayer->CreateField(&oField) != OGRERR_NONE)
                return nullptr;
        }
    }

    if (!psOptions->osGTIFilename.empty())
    {
        if (!psOptions->aosMetadata.empty())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "-mo is not supported when -gti_filename is used");
            return nullptr;
        }
        CPLXMLNode *psRoot =
            CPLCreateXMLNode(nullptr, CXT_Element, "GDALTileIndexDataset");
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
        int res =
            CPLSerializeXMLTreeToFile(psRoot, psOptions->osGTIFilename.c_str());
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
        const CPLStringList aosMetadata(psOptions->aosMetadata);
        for (const auto &[pszKey, pszValue] :
             cpl::IterateNameValue(aosMetadata))
        {
            poLayer->SetMetadataItem(pszKey, pszValue);
        }
    }

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

    const bool bIsGTIContext =
        !std::isnan(psOptions->xres) || !std::isnan(psOptions->xmin) ||
        !psOptions->osBandCount.empty() || !psOptions->osNodata.empty() ||
        !psOptions->osColorInterp.empty() || !psOptions->osDataType.empty() ||
        psOptions->bMaskBand || !psOptions->aosMetadata.empty() ||
        !psOptions->osGTIFilename.empty();

    /* -------------------------------------------------------------------- */
    /*      loop over GDAL files, processing.                               */
    /* -------------------------------------------------------------------- */
    while (true)
    {
        const std::string osSrcFilename = oGDALTileIndexTileIterator.next();
        if (osSrcFilename.empty())
            break;

        std::string osFileNameToWrite;
        VSIStatBuf sStatBuf;

        // Make sure it is a file before building absolute path name.
        if (!osCurrentPath.empty() &&
            CPLIsFilenameRelative(osSrcFilename.c_str()) &&
            VSIStat(osSrcFilename.c_str(), &sStatBuf) == 0)
        {
            osFileNameToWrite = CPLProjectRelativeFilename(
                osCurrentPath.c_str(), osSrcFilename.c_str());
        }
        else
        {
            osFileNameToWrite = osSrcFilename.c_str();
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
            osSrcFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
            nullptr, nullptr, nullptr));
        if (poSrcDS == nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unable to open %s, skipping.", osSrcFilename.c_str());
            continue;
        }

        double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "It appears no georeferencing is available for\n"
                     "`%s', skipping.",
                     osSrcFilename.c_str());
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
                        osSrcFilename.c_str(),
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
        if (nXSize == 0 || nYSize == 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s has 0 width or height. Skipping",
                     osSrcFilename.c_str());
            continue;
        }

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
        else if (bIsGTIContext && !oAlreadyExistingSRS.IsEmpty() &&
                 (poSrcSRS == nullptr ||
                  !poSrcSRS->IsSame(&oAlreadyExistingSRS)))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "%s is not using the same projection system "
                "as other files in the tileindex. This is not compatible of "
                "GTI use. Use -t_srs option to reproject tile extents "
                "to a common SRS.",
                osSrcFilename.c_str());
            return nullptr;
        }

        const double dfMinX =
            std::min(std::min(adfX[0], adfX[1]), std::min(adfX[2], adfX[3]));
        const double dfMinY =
            std::min(std::min(adfY[0], adfY[1]), std::min(adfY[2], adfY[3]));
        const double dfMaxX =
            std::max(std::max(adfX[0], adfX[1]), std::max(adfX[2], adfX[3]));
        const double dfMaxY =
            std::max(std::max(adfY[0], adfY[1]), std::max(adfY[2], adfY[3]));
        const double dfRes =
            (dfMaxX - dfMinX) * (dfMaxY - dfMinY) / nXSize / nYSize;
        if (!std::isnan(psOptions->dfMinPixelSize) &&
            dfRes < psOptions->dfMinPixelSize)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s has %f as pixel size (< %f). Skipping",
                     osSrcFilename.c_str(), dfRes, psOptions->dfMinPixelSize);
            continue;
        }
        if (!std::isnan(psOptions->dfMaxPixelSize) &&
            dfRes > psOptions->dfMaxPixelSize)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s has %f as pixel size (> %f). Skipping",
                     osSrcFilename.c_str(), dfRes, psOptions->dfMaxPixelSize);
            continue;
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
                    }
                    else
                    {
                        poFeature->SetField(i_SrcSRSName,
                                            poSrcDS->GetProjectionRef());
                    }
                    CPLFree(pszProj4);
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
                }
                CPLFree(pszProj4);
            }
            else if (psOptions->eSrcSRSFormat == FORMAT_EPSG)
            {
                if (pszAuthorityName != nullptr && pszAuthorityCode != nullptr)
                    poFeature->SetField(i_SrcSRSName,
                                        CPLSPrintf("%s:%s", pszAuthorityName,
                                                   pszAuthorityCode));
            }
        }

        for (const auto &oFetchMD : psOptions->aoFetchMD)
        {
            if (EQUAL(oFetchMD.osRasterItemName.c_str(), "{PIXEL_SIZE}"))
            {
                poFeature->SetField(oFetchMD.osFieldName.c_str(), dfRes);
                continue;
            }

            const char *pszMD =
                poSrcDS->GetMetadataItem(oFetchMD.osRasterItemName.c_str());
            if (pszMD)
            {
                if (EQUAL(oFetchMD.osRasterItemName.c_str(),
                          "TIFFTAG_DATETIME"))
                {
                    int nYear, nMonth, nDay, nHour, nMin, nSec;
                    if (sscanf(pszMD, "%04d:%02d:%02d %02d:%02d:%02d", &nYear,
                               &nMonth, &nDay, &nHour, &nMin, &nSec) == 6)
                    {
                        poFeature->SetField(
                            oFetchMD.osFieldName.c_str(),
                            CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d", nYear,
                                       nMonth, nDay, nHour, nMin, nSec));
                        continue;
                    }
                }
                poFeature->SetField(oFetchMD.osFieldName.c_str(), pszMD);
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
        else if (strcmp(papszArgv[iArg], "-lco") == 0)
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->aosLCO.AddString(papszArgv[++iArg]);
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
            psOptions->yres = std::fabs(CPLAtofM(papszArgv[++iArg]));
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
        else if (EQUAL(papszArgv[iArg], "-gti_filename"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->osGTIFilename = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-overwrite"))
        {
            psOptions->bOverwrite = true;
        }
        else if (EQUAL(papszArgv[iArg], "-recursive"))
        {
            psOptions->bRecursive = true;
        }
        else if (EQUAL(papszArgv[iArg], "-min_pixel_size"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->dfMinPixelSize = CPLAtofM(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-max_pixel_size"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->dfMaxPixelSize = CPLAtofM(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-filename_filter"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            psOptions->oSetFilenameFilters.insert(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-fetch_md"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(3);
            GDALTileIndexRasterMetadata md;
            md.osRasterItemName = papszArgv[++iArg];
            md.osFieldName = papszArgv[++iArg];
            const char *pszType = papszArgv[++iArg];
            if (EQUAL(pszType, "String"))
                md.eType = OFTString;
            else if (EQUAL(pszType, "Integer"))
                md.eType = OFTInteger;
            else if (EQUAL(pszType, "Integer64"))
                md.eType = OFTInteger64;
            else if (EQUAL(pszType, "Real"))
                md.eType = OFTReal;
            else if (EQUAL(pszType, "Date"))
                md.eType = OFTDate;
            else if (EQUAL(pszType, "DateTime"))
                md.eType = OFTDateTime;
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Unknown type '%s'",
                         pszType);
                return nullptr;
            }
            psOptions->aoFetchMD.emplace_back(std::move(md));
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

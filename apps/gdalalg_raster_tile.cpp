/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster tile" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_tile.h"

#include "cpl_conv.h"
#include "cpl_mem_cache.h"
#include "cpl_worker_thread_pool.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "ogr_spatialref.h"
#include "memdataset.h"
#include "tilematrixset.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*           GDALRasterTileAlgorithm::GDALRasterTileAlgorithm()         */
/************************************************************************/

GDALRasterTileAlgorithm::GDALRasterTileAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_dataset, GDAL_OF_RASTER);
    AddOutputFormatArg(&m_outputFormat)
        .SetDefault(m_outputFormat)
        .AddMetadataItem(
            GAAMDI_REQUIRED_CAPABILITIES,
            {GDAL_DCAP_RASTER, GDAL_DCAP_CREATECOPY, GDAL_DMD_EXTENSIONS})
        .AddMetadataItem(GAAMDI_VRT_COMPATIBLE, {"false"});
    AddCreationOptionsArg(&m_creationOptions);

    AddArg("output", 'o', _("Output directory"), &m_outputDirectory)
        .SetRequired()
        .SetMinCharCount(1)
        .SetPositional();

    std::vector<std::string> tilingSchemes{"raster"};
    for (const std::string &scheme :
         gdal::TileMatrixSet::listPredefinedTileMatrixSets())
    {
        auto poTMS = gdal::TileMatrixSet::parse(scheme.c_str());
        OGRSpatialReference oSRS_TMS;
        if (poTMS && !poTMS->hasVariableMatrixWidth() &&
            oSRS_TMS.SetFromUserInput(poTMS->crs().c_str()) == OGRERR_NONE)
        {
            const std::string identifier = scheme == "GoogleMapsCompatible"
                                               ? "WebMercatorQuad"
                                               : poTMS->identifier();
            m_mapTileMatrixIdentifierToScheme[identifier] = scheme;
            tilingSchemes.push_back(identifier);
        }
    }
    AddArg("tiling-scheme", 0, _("Tiling scheme"), &m_tilingScheme)
        .SetDefault("WebMercatorQuad")
        .SetChoices(tilingSchemes)
        .SetHiddenChoices(
            "GoogleMapsCompatible",  // equivalent of WebMercatorQuad
            "mercator",              // gdal2tiles equivalent of WebMercatorQuad
            "geodetic"  // gdal2tiles (not totally) equivalent of WorldCRS84Quad
        );

    AddArg("min-zoom", 0, _("Minimum zoom level"), &m_minZoomLevel)
        .SetMinValueIncluded(0);
    AddArg("max-zoom", 0, _("Maximum zoom level"), &m_maxZoomLevel)
        .SetMinValueIncluded(0);

    AddArg("min-x", 0, _("Minimum tile X coordinate"), &m_minTileX)
        .SetMinValueIncluded(0);
    AddArg("max-x", 0, _("Maximum tile X coordinate"), &m_maxTileX)
        .SetMinValueIncluded(0);
    AddArg("min-y", 0, _("Minimum tile Y coordinate"), &m_minTileY)
        .SetMinValueIncluded(0);
    AddArg("max-y", 0, _("Maximum tile Y coordinate"), &m_maxTileY)
        .SetMinValueIncluded(0);
    AddArg("no-intersection-ok", 0,
           _("Whether dataset extent not intersecting tile matrix is only a "
             "warning"),
           &m_noIntersectionIsOK);

    AddArg("resampling", 'r', _("Resampling method for max zoom"),
           &m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum")
        .SetDefault("cubic")
        .SetHiddenChoices("near");
    AddArg("overview-resampling", 0, _("Resampling method for overviews"),
           &m_overviewResampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum")
        .SetHiddenChoices("near");

    AddArg("convention", 0,
           _("Tile numbering convention: xyz (from top) or tms (from bottom)"),
           &m_convention)
        .SetDefault(m_convention)
        .SetChoices("xyz", "tms");
    AddArg("tile-size", 0, _("Override default tile size"), &m_tileSize)
        .SetMinValueIncluded(64)
        .SetMaxValueIncluded(32768);
    AddArg("add-alpha", 0, _("Whether to force adding an alpha channel"),
           &m_addalpha)
        .SetMutualExclusionGroup("alpha");
    AddArg("no-alpha", 0, _("Whether to disable adding an alpha channel"),
           &m_noalpha)
        .SetMutualExclusionGroup("alpha");
    auto &dstNoDataArg =
        AddArg("dst-nodata", 0, _("Destination nodata value"), &m_dstNoData);
    AddArg("skip-blank", 0, _("Do not generate blank tiles"), &m_skipBlank);

    {
        auto &arg = AddArg("metadata", 0,
                           _("Add metadata item to output tiles"), &m_metadata)
                        .SetMetaVar("<KEY>=<VALUE>")
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }
    AddArg("copy-src-metadata", 0,
           _("Whether to copy metadata from source dataset"),
           &m_copySrcMetadata);

    AddArg("aux-xml", 0, _("Generate .aux.xml sidecar files when needed"),
           &m_auxXML);
    AddArg("kml", 0, _("Generate KML files"), &m_kml);
    AddArg("resume", 0, _("Generate only missing files"), &m_resume);

    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);

    constexpr const char *ADVANCED_RESAMPLING_CATEGORY = "Advanced Resampling";
    auto &excludedValuesArg =
        AddArg("excluded-values", 0,
               _("Tuples of values (e.g. <R>,<G>,<B> or (<R1>,<G1>,<B1>),"
                 "(<R2>,<G2>,<B2>)) that must beignored as contributing source "
                 "pixels during (average) resampling"),
               &m_excludedValues)
            .SetCategory(ADVANCED_RESAMPLING_CATEGORY);
    auto &excludedValuesPctThresholdArg =
        AddArg(
            "excluded-values-pct-threshold", 0,
            _("Minimum percentage of source pixels that must be set at one of "
              "the --excluded-values to cause the excluded value to be used as "
              "the target pixel value"),
            &m_excludedValuesPctThreshold)
            .SetDefault(m_excludedValuesPctThreshold)
            .SetMinValueIncluded(0)
            .SetMaxValueIncluded(100)
            .SetCategory(ADVANCED_RESAMPLING_CATEGORY);
    auto &nodataValuesPctThresholdArg =
        AddArg(
            "nodata-values-pct-threshold", 0,
            _("Minimum percentage of source pixels that must be set at one of "
              "nodata (or alpha=0 or any other way to express transparent pixel"
              "to cause the target pixel value to be transparent"),
            &m_nodataValuesPctThreshold)
            .SetDefault(m_nodataValuesPctThreshold)
            .SetMinValueIncluded(0)
            .SetMaxValueIncluded(100)
            .SetCategory(ADVANCED_RESAMPLING_CATEGORY);

    constexpr const char *PUBLICATION_CATEGORY = "Publication";
    AddArg("webviewer", 0, _("Web viewer to generate"), &m_webviewers)
        .SetDefault("all")
        .SetChoices("none", "all", "leaflet", "openlayers", "mapml")
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("url", 0,
           _("URL address where the generated tiles are going to be published"),
           &m_url)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("title", 0, _("Title of the map"), &m_title)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("copyright", 0, _("Copyright for the map"), &m_copyright)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("mapml-template", 0,
           _("Filename of a template mapml file where variables will be "
             "substituted"),
           &m_mapmlTemplate)
        .SetMinCharCount(1)
        .SetCategory(PUBLICATION_CATEGORY);

    AddValidationAction(
        [this, &dstNoDataArg, &excludedValuesArg,
         &excludedValuesPctThresholdArg, &nodataValuesPctThresholdArg]()
        {
            if (m_minTileX >= 0 && m_maxTileX >= 0 && m_minTileX > m_maxTileX)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "'min-x' must be lesser or equal to 'max-x'");
                return false;
            }

            if (m_minTileY >= 0 && m_maxTileY >= 0 && m_minTileY > m_maxTileY)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "'min-y' must be lesser or equal to 'max-y'");
                return false;
            }

            if (m_minZoomLevel >= 0 && m_maxZoomLevel >= 0 &&
                m_minZoomLevel > m_maxZoomLevel)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "'min-zoom' must be lesser or equal to 'max-zoom'");
                return false;
            }

            if (m_addalpha && dstNoDataArg.IsExplicitlySet())
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "'add-alpha' and 'dst-nodata' are mutually exclusive");
                return false;
            }

            for (const auto *arg :
                 {&excludedValuesArg, &excludedValuesPctThresholdArg,
                  &nodataValuesPctThresholdArg})
            {
                if (arg->IsExplicitlySet() && m_resampling != "average")
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "'%s' can only be specified if 'resampling' is "
                                "set to 'average'",
                                arg->GetName().c_str());
                    return false;
                }
                if (arg->IsExplicitlySet() && !m_overviewResampling.empty() &&
                    m_overviewResampling != "average")
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "'%s' can only be specified if "
                                "'overview-resampling' is set to 'average'",
                                arg->GetName().c_str());
                    return false;
                }
            }

            return true;
        });
}

/************************************************************************/
/*                          GetTileIndices()                            */
/************************************************************************/

static bool GetTileIndices(gdal::TileMatrixSet::TileMatrix &tileMatrix,
                           bool bInvertAxisTMS, int tileSize,
                           const double adfExtent[4], int &nMinTileX,
                           int &nMinTileY, int &nMaxTileX, int &nMaxTileY,
                           bool noIntersectionIsOK, bool &bIntersects,
                           bool checkRasterOverflow = true)
{
    if (tileSize > 0)
    {
        tileMatrix.mResX *=
            static_cast<double>(tileMatrix.mTileWidth) / tileSize;
        tileMatrix.mResY *=
            static_cast<double>(tileMatrix.mTileHeight) / tileSize;
        tileMatrix.mTileWidth = tileSize;
        tileMatrix.mTileHeight = tileSize;
    }

    if (bInvertAxisTMS)
        std::swap(tileMatrix.mTopLeftX, tileMatrix.mTopLeftY);

    const double dfTileWidth = tileMatrix.mResX * tileMatrix.mTileWidth;
    const double dfTileHeight = tileMatrix.mResY * tileMatrix.mTileHeight;

    constexpr double EPSILON = 1e-3;
    const double dfMinTileX =
        (adfExtent[0] - tileMatrix.mTopLeftX) / dfTileWidth;
    nMinTileX = static_cast<int>(
        std::clamp(std::floor(dfMinTileX + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixWidth - 1)));
    const double dfMinTileY =
        (tileMatrix.mTopLeftY - adfExtent[3]) / dfTileHeight;
    nMinTileY = static_cast<int>(
        std::clamp(std::floor(dfMinTileY + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixHeight - 1)));
    const double dfMaxTileX =
        (adfExtent[2] - tileMatrix.mTopLeftX) / dfTileWidth;
    nMaxTileX = static_cast<int>(
        std::clamp(std::floor(dfMaxTileX + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixWidth - 1)));
    const double dfMaxTileY =
        (tileMatrix.mTopLeftY - adfExtent[1]) / dfTileHeight;
    nMaxTileY = static_cast<int>(
        std::clamp(std::floor(dfMaxTileY + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixHeight - 1)));

    bIntersects = (dfMinTileX <= tileMatrix.mMatrixWidth && dfMaxTileX >= 0 &&
                   dfMinTileY <= tileMatrix.mMatrixHeight && dfMaxTileY >= 0);
    if (!bIntersects)
    {
        CPLDebug("gdal_raster_tile",
                 "dfMinTileX=%g dfMinTileY=%g dfMaxTileX=%g dfMaxTileY=%g",
                 dfMinTileX, dfMinTileY, dfMaxTileX, dfMaxTileY);
        CPLError(noIntersectionIsOK ? CE_Warning : CE_Failure, CPLE_AppDefined,
                 "Extent of source dataset is not compatible with extent of "
                 "tile matrix %s",
                 tileMatrix.mId.c_str());
        return noIntersectionIsOK;
    }
    if (checkRasterOverflow)
    {
        if (nMaxTileX - nMinTileX + 1 > INT_MAX / tileMatrix.mTileWidth ||
            nMaxTileY - nMinTileY + 1 > INT_MAX / tileMatrix.mTileHeight)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large zoom level");
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                           GetFileY()                                 */
/************************************************************************/

static int GetFileY(int iY, const gdal::TileMatrixSet::TileMatrix &tileMatrix,
                    const std::string &convention)
{
    return convention == "xyz" ? iY : tileMatrix.mMatrixHeight - 1 - iY;
}

/************************************************************************/
/*                          GenerateTile()                              */
/************************************************************************/

static bool GenerateTile(
    GDALDataset *poSrcDS, GDALDriver *poDstDriver, const char *pszExtension,
    CSLConstList creationOptions, GDALWarpOperation &oWO,
    const OGRSpatialReference &oSRS_TMS, GDALDataType eWorkingDataType,
    const gdal::TileMatrixSet::TileMatrix &tileMatrix,
    const std::string &outputDirectory, int nBands, const double *pdfDstNoData,
    int nZoomLevel, int iX, int iY, const std::string &convention,
    int nMinTileX, int nMinTileY, bool bSkipBlank, bool bUserAskedForAlpha,
    bool bAuxXML, bool bResume, const std::vector<std::string> &metadata,
    const GDALColorTable *poColorTable, std::vector<GByte> &dstBuffer)
{
    const std::string osDirZ = CPLFormFilenameSafe(
        outputDirectory.c_str(), CPLSPrintf("%d", nZoomLevel), nullptr);
    const std::string osDirX =
        CPLFormFilenameSafe(osDirZ.c_str(), CPLSPrintf("%d", iX), nullptr);
    const int iFileY = GetFileY(iY, tileMatrix, convention);
    const std::string osFilename = CPLFormFilenameSafe(
        osDirX.c_str(), CPLSPrintf("%d", iFileY), pszExtension);

    if (bResume)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
            return true;
    }

    const int nDstXOff = (iX - nMinTileX) * tileMatrix.mTileWidth;
    const int nDstYOff = (iY - nMinTileY) * tileMatrix.mTileHeight;
    memset(dstBuffer.data(), 0, dstBuffer.size());
    const CPLErr eErr = oWO.WarpRegionToBuffer(
        nDstXOff, nDstYOff, tileMatrix.mTileWidth, tileMatrix.mTileHeight,
        dstBuffer.data(), eWorkingDataType);
    if (eErr != CE_None)
        return false;

    const bool bDstHasAlpha =
        nBands > poSrcDS->GetRasterCount() ||
        (nBands == poSrcDS->GetRasterCount() &&
         poSrcDS->GetRasterBand(nBands)->GetColorInterpretation() ==
             GCI_AlphaBand);
    const size_t nBytesPerBand = static_cast<size_t>(tileMatrix.mTileWidth) *
                                 tileMatrix.mTileHeight *
                                 GDALGetDataTypeSizeBytes(eWorkingDataType);
    if (bDstHasAlpha && bSkipBlank)
    {
        bool bBlank = true;
        for (size_t i = 0; i < nBytesPerBand && bBlank; ++i)
        {
            bBlank = (dstBuffer[(nBands - 1) * nBytesPerBand + i] == 0);
        }
        if (bBlank)
            return true;
    }
    if (bDstHasAlpha && !bUserAskedForAlpha)
    {
        bool bAllOpaque = true;
        for (size_t i = 0; i < nBytesPerBand && bAllOpaque; ++i)
        {
            bAllOpaque = (dstBuffer[(nBands - 1) * nBytesPerBand + i] == 255);
        }
        if (bAllOpaque)
            nBands--;
    }

    VSIMkdir(osDirZ.c_str(), 0755);
    VSIMkdir(osDirX.c_str(), 0755);

    auto memDS = std::unique_ptr<GDALDataset>(
        MEMDataset::Create("", tileMatrix.mTileWidth, tileMatrix.mTileHeight, 0,
                           eWorkingDataType, nullptr));
    for (int i = 0; i < nBands; ++i)
    {
        char szBuffer[32] = {'\0'};
        int nRet = CPLPrintPointer(
            szBuffer, dstBuffer.data() + i * nBytesPerBand, sizeof(szBuffer));
        szBuffer[nRet] = 0;

        char szOption[64] = {'\0'};
        snprintf(szOption, sizeof(szOption), "DATAPOINTER=%s", szBuffer);

        char *apszOptions[] = {szOption, nullptr};

        memDS->AddBand(eWorkingDataType, apszOptions);
        auto poDstBand = memDS->GetRasterBand(i + 1);
        if (i + 1 <= poSrcDS->GetRasterCount())
            poDstBand->SetColorInterpretation(
                poSrcDS->GetRasterBand(i + 1)->GetColorInterpretation());
        else
            poDstBand->SetColorInterpretation(GCI_AlphaBand);
        if (pdfDstNoData)
            poDstBand->SetNoDataValue(*pdfDstNoData);
        if (i == 0 && poColorTable)
            poDstBand->SetColorTable(
                const_cast<GDALColorTable *>(poColorTable));
    }
    const CPLStringList aosMD(metadata);
    for (const auto [key, value] : cpl::IterateNameValue(aosMD))
    {
        memDS->SetMetadataItem(key, value);
    }

    double adfGT[6];
    adfGT[0] =
        tileMatrix.mTopLeftX + iX * tileMatrix.mResX * tileMatrix.mTileWidth;
    adfGT[1] = tileMatrix.mResX;
    adfGT[2] = 0;
    adfGT[3] =
        tileMatrix.mTopLeftY - iY * tileMatrix.mResY * tileMatrix.mTileHeight;
    adfGT[4] = 0;
    adfGT[5] = -tileMatrix.mResY;
    memDS->SetGeoTransform(adfGT);

    memDS->SetSpatialRef(&oSRS_TMS);

    CPLConfigOptionSetter oSetter("GDAL_PAM_ENABLED", bAuxXML ? "YES" : "NO",
                                  false);

    const std::string osTmpFilename = osFilename + ".tmp." + pszExtension;

    std::unique_ptr<GDALDataset> poOutDS(
        poDstDriver->CreateCopy(osTmpFilename.c_str(), memDS.get(), false,
                                creationOptions, nullptr, nullptr));
    bool bRet = poOutDS && poOutDS->Close() == CE_None;
    poOutDS.reset();
    if (bRet)
    {
        bRet = VSIRename(osTmpFilename.c_str(), osFilename.c_str()) == 0;
        if (bAuxXML)
        {
            VSIRename((osTmpFilename + ".aux.xml").c_str(),
                      (osFilename + ".aux.xml").c_str());
        }
    }
    else
    {
        VSIUnlink(osTmpFilename.c_str());
    }
    return bRet;
}

/************************************************************************/
/*                    GenerateOverviewTile()                            */
/************************************************************************/

static bool
GenerateOverviewTile(GDALDataset &oSrcDS, GDALDriver *poDstDriver,
                     const std::string &outputFormat, const char *pszExtension,
                     CSLConstList creationOptions,
                     CSLConstList papszWarpOptions,
                     const std::string &resampling,
                     const gdal::TileMatrixSet::TileMatrix &tileMatrix,
                     const std::string &outputDirectory, int nZoomLevel, int iX,
                     int iY, const std::string &convention, bool bSkipBlank,
                     bool bUserAskedForAlpha, bool bAuxXML, bool bResume)
{
    const std::string osDirZ = CPLFormFilenameSafe(
        outputDirectory.c_str(), CPLSPrintf("%d", nZoomLevel), nullptr);
    const std::string osDirX =
        CPLFormFilenameSafe(osDirZ.c_str(), CPLSPrintf("%d", iX), nullptr);

    const int iFileY = GetFileY(iY, tileMatrix, convention);
    const std::string osFilename = CPLFormFilenameSafe(
        osDirX.c_str(), CPLSPrintf("%d", iFileY), pszExtension);

    if (bResume)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
            return true;
    }

    VSIMkdir(osDirZ.c_str(), 0755);
    VSIMkdir(osDirX.c_str(), 0755);

    CPLStringList aosOptions;

    aosOptions.AddString("-of");
    aosOptions.AddString(outputFormat.c_str());

    for (const char *pszCO : cpl::Iterate(creationOptions))
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(pszCO);
    }
    CPLConfigOptionSetter oSetter("GDAL_PAM_ENABLED", bAuxXML ? "YES" : "NO",
                                  false);

    aosOptions.AddString("-r");
    aosOptions.AddString(resampling.c_str());

    std::unique_ptr<GDALDataset> poOutDS;
    const double dfMinX =
        tileMatrix.mTopLeftX + iX * tileMatrix.mResX * tileMatrix.mTileWidth;
    const double dfMaxY =
        tileMatrix.mTopLeftY - iY * tileMatrix.mResY * tileMatrix.mTileHeight;
    const double dfMaxX = dfMinX + tileMatrix.mResX * tileMatrix.mTileWidth;
    const double dfMinY = dfMaxY - tileMatrix.mResY * tileMatrix.mTileHeight;

    const bool resamplingCompatibleOfTranslate =
        papszWarpOptions == nullptr &&
        (resampling == "nearest" || resampling == "average" ||
         resampling == "bilinear" || resampling == "cubic" ||
         resampling == "cubicspline" || resampling == "lanczos" ||
         resampling == "mode");

    const std::string osTmpFilename = osFilename + ".tmp." + pszExtension;

    if (resamplingCompatibleOfTranslate)
    {
        double adfUpperGT[6];
        oSrcDS.GetGeoTransform(adfUpperGT);
        const double dfMinXUpper = adfUpperGT[0];
        const double dfMaxXUpper =
            dfMinXUpper + adfUpperGT[1] * oSrcDS.GetRasterXSize();
        const double dfMaxYUpper = adfUpperGT[3];
        const double dfMinYUpper =
            dfMaxYUpper + adfUpperGT[5] * oSrcDS.GetRasterYSize();
        if (dfMinX >= dfMinXUpper && dfMaxX <= dfMaxXUpper &&
            dfMinY >= dfMinYUpper && dfMaxY <= dfMaxYUpper)
        {
            // If the overview tile is fully within the extent of the
            // upper zoom level, we can use GDALDataset::RasterIO() directly.

            const auto eDT = oSrcDS.GetRasterBand(1)->GetRasterDataType();
            const size_t nBytesPerBand =
                static_cast<size_t>(tileMatrix.mTileWidth) *
                tileMatrix.mTileHeight * GDALGetDataTypeSizeBytes(eDT);
            std::vector<GByte> dstBuffer(nBytesPerBand *
                                         oSrcDS.GetRasterCount());

            const double dfXOff = (dfMinX - dfMinXUpper) / adfUpperGT[1];
            const double dfYOff = (dfMaxYUpper - dfMaxY) / -adfUpperGT[5];
            const double dfXSize = (dfMaxX - dfMinX) / adfUpperGT[1];
            const double dfYSize = (dfMaxY - dfMinY) / -adfUpperGT[5];
            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);
            CPL_IGNORE_RET_VAL(sExtraArg.eResampleAlg);
            sExtraArg.eResampleAlg =
                GDALRasterIOGetResampleAlg(resampling.c_str());
            sExtraArg.dfXOff = dfXOff;
            sExtraArg.dfYOff = dfYOff;
            sExtraArg.dfXSize = dfXSize;
            sExtraArg.dfYSize = dfYSize;
            sExtraArg.bFloatingPointWindowValidity =
                sExtraArg.eResampleAlg != GRIORA_NearestNeighbour;
            constexpr double EPSILON = 1e-3;
            if (oSrcDS.RasterIO(GF_Read, static_cast<int>(dfXOff + EPSILON),
                                static_cast<int>(dfYOff + EPSILON),
                                static_cast<int>(dfXSize + 0.5),
                                static_cast<int>(dfYSize + 0.5),
                                dstBuffer.data(), tileMatrix.mTileWidth,
                                tileMatrix.mTileHeight, eDT,
                                oSrcDS.GetRasterCount(), nullptr, 0, 0, 0,
                                &sExtraArg) == CE_None)
            {
                int nDstBands = oSrcDS.GetRasterCount();
                const bool bDstHasAlpha =
                    oSrcDS.GetRasterBand(nDstBands)->GetColorInterpretation() ==
                    GCI_AlphaBand;
                if (bDstHasAlpha && bSkipBlank)
                {
                    bool bBlank = true;
                    for (size_t i = 0; i < nBytesPerBand && bBlank; ++i)
                    {
                        bBlank =
                            (dstBuffer[(nDstBands - 1) * nBytesPerBand + i] ==
                             0);
                    }
                    if (bBlank)
                        return true;
                    bSkipBlank = false;
                }
                if (bDstHasAlpha && !bUserAskedForAlpha)
                {
                    bool bAllOpaque = true;
                    for (size_t i = 0; i < nBytesPerBand && bAllOpaque; ++i)
                    {
                        bAllOpaque =
                            (dstBuffer[(nDstBands - 1) * nBytesPerBand + i] ==
                             255);
                    }
                    if (bAllOpaque)
                        nDstBands--;
                }

                auto memDS = std::unique_ptr<GDALDataset>(MEMDataset::Create(
                    "", tileMatrix.mTileWidth, tileMatrix.mTileHeight, 0, eDT,
                    nullptr));
                for (int i = 0; i < nDstBands; ++i)
                {
                    char szBuffer[32] = {'\0'};
                    int nRet = CPLPrintPointer(
                        szBuffer, dstBuffer.data() + i * nBytesPerBand,
                        sizeof(szBuffer));
                    szBuffer[nRet] = 0;

                    char szOption[64] = {'\0'};
                    snprintf(szOption, sizeof(szOption), "DATAPOINTER=%s",
                             szBuffer);

                    char *apszOptions[] = {szOption, nullptr};

                    memDS->AddBand(eDT, apszOptions);
                    auto poSrcBand = oSrcDS.GetRasterBand(i + 1);
                    auto poDstBand = memDS->GetRasterBand(i + 1);
                    poDstBand->SetColorInterpretation(
                        poSrcBand->GetColorInterpretation());
                    int bHasNoData = false;
                    const double dfNoData =
                        poSrcBand->GetNoDataValue(&bHasNoData);
                    if (bHasNoData)
                        poDstBand->SetNoDataValue(dfNoData);
                    if (auto poCT = poSrcBand->GetColorTable())
                        poDstBand->SetColorTable(poCT);
                }
                memDS->SetMetadata(oSrcDS.GetMetadata());
                double adfGT[6];
                adfGT[0] = dfMinX;
                adfGT[1] = tileMatrix.mResX;
                adfGT[2] = 0;
                adfGT[3] = dfMaxY;
                adfGT[4] = 0;
                adfGT[5] = -tileMatrix.mResY;
                memDS->SetGeoTransform(adfGT);

                memDS->SetSpatialRef(oSrcDS.GetSpatialRef());

                poOutDS.reset(poDstDriver->CreateCopy(
                    osTmpFilename.c_str(), memDS.get(), false, creationOptions,
                    nullptr, nullptr));
            }
        }
        else
        {
            // If the overview tile is not fully within the extent of the
            // upper zoom level, use GDALTranslate() to use VRT padding

            aosOptions.AddString("-q");

            aosOptions.AddString("-projwin");
            aosOptions.AddString(CPLSPrintf("%.17g", dfMinX));
            aosOptions.AddString(CPLSPrintf("%.17g", dfMaxY));
            aosOptions.AddString(CPLSPrintf("%.17g", dfMaxX));
            aosOptions.AddString(CPLSPrintf("%.17g", dfMinY));

            aosOptions.AddString("-outsize");
            aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileWidth));
            aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileHeight));

            GDALTranslateOptions *psOptions =
                GDALTranslateOptionsNew(aosOptions.List(), nullptr);
            poOutDS.reset(GDALDataset::FromHandle(GDALTranslate(
                osTmpFilename.c_str(), GDALDataset::ToHandle(&oSrcDS),
                psOptions, nullptr)));
            GDALTranslateOptionsFree(psOptions);
        }
    }
    else
    {
        aosOptions.AddString("-te");
        aosOptions.AddString(CPLSPrintf("%.17g", dfMinX));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMinY));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMaxX));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMaxY));

        aosOptions.AddString("-ts");
        aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileWidth));
        aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileHeight));

        for (int i = 0; papszWarpOptions && papszWarpOptions[i]; ++i)
        {
            aosOptions.AddString("-wo");
            aosOptions.AddString(papszWarpOptions[i]);
        }

        GDALWarpAppOptions *psOptions =
            GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
        GDALDatasetH hSrcDS = GDALDataset::ToHandle(&oSrcDS);
        poOutDS.reset(GDALDataset::FromHandle(GDALWarp(
            osTmpFilename.c_str(), nullptr, 1, &hSrcDS, psOptions, nullptr)));
        GDALWarpAppOptionsFree(psOptions);
    }

    bool bRet = poOutDS != nullptr;
    if (bRet && bSkipBlank)
    {
        auto poLastBand = poOutDS->GetRasterBand(poOutDS->GetRasterCount());
        if (poLastBand->GetColorInterpretation() == GCI_AlphaBand)
        {
            std::vector<GByte> buffer(
                static_cast<size_t>(tileMatrix.mTileWidth) *
                tileMatrix.mTileHeight *
                GDALGetDataTypeSizeBytes(poLastBand->GetRasterDataType()));
            CPL_IGNORE_RET_VAL(poLastBand->RasterIO(
                GF_Read, 0, 0, tileMatrix.mTileWidth, tileMatrix.mTileHeight,
                buffer.data(), tileMatrix.mTileWidth, tileMatrix.mTileHeight,
                poLastBand->GetRasterDataType(), 0, 0, nullptr));
            bool bBlank = true;
            for (size_t i = 0; i < buffer.size() && bBlank; ++i)
            {
                bBlank = (buffer[i] == 0);
            }
            if (bBlank)
            {
                poOutDS.reset();
                VSIUnlink(osTmpFilename.c_str());
                if (bAuxXML)
                    VSIUnlink((osTmpFilename + ".aux.xml").c_str());
                return true;
            }
        }
    }
    bRet = bRet && poOutDS->Close() == CE_None;
    poOutDS.reset();
    if (bRet)
    {
        bRet = VSIRename(osTmpFilename.c_str(), osFilename.c_str()) == 0;
        if (bAuxXML)
        {
            VSIRename((osTmpFilename + ".aux.xml").c_str(),
                      (osFilename + ".aux.xml").c_str());
        }
    }
    else
    {
        VSIUnlink(osTmpFilename.c_str());
    }
    return bRet;
}

namespace
{

/************************************************************************/
/*                     FakeMaxZoomRasterBand                            */
/************************************************************************/

class FakeMaxZoomRasterBand : public GDALRasterBand
{
    void *m_pDstBuffer = nullptr;
    CPL_DISALLOW_COPY_ASSIGN(FakeMaxZoomRasterBand)

  public:
    FakeMaxZoomRasterBand(int nBandIn, int nWidth, int nHeight,
                          int nBlockXSizeIn, int nBlockYSizeIn,
                          GDALDataType eDT, void *pDstBuffer)
        : m_pDstBuffer(pDstBuffer)
    {
        nBand = nBandIn;
        nRasterXSize = nWidth;
        nRasterYSize = nHeight;
        nBlockXSize = nBlockXSizeIn;
        nBlockYSize = nBlockYSizeIn;
        eDataType = eDT;
    }

    CPLErr IReadBlock(int, int, void *) override
    {
        CPLAssert(false);
        return CE_Failure;
    }

#ifdef DEBUG
    CPLErr IWriteBlock(int, int, void *) override
    {
        CPLAssert(false);
        return CE_Failure;
    }
#endif

    CPLErr IRasterIO(GDALRWFlag eRWFlag, [[maybe_unused]] int nXOff,
                     [[maybe_unused]] int nYOff, [[maybe_unused]] int nXSize,
                     [[maybe_unused]] int nYSize, void *pData,
                     [[maybe_unused]] int nBufXSize,
                     [[maybe_unused]] int nBufYSize, GDALDataType eBufType,
                     GSpacing nPixelSpace, [[maybe_unused]] GSpacing nLineSpace,
                     GDALRasterIOExtraArg *) override
    {
        // For sake of implementation simplicity, check various assumptions of
        // how GDALAlphaMask code does I/O
        CPLAssert((nXOff % nBlockXSize) == 0);
        CPLAssert((nYOff % nBlockYSize) == 0);
        CPLAssert(nXSize == nBufXSize);
        CPLAssert(nXSize == nBlockXSize);
        CPLAssert(nYSize == nBufYSize);
        CPLAssert(nYSize == nBlockYSize);
        CPLAssert(nLineSpace == nBlockXSize * nPixelSpace);
        CPLAssert(
            nBand ==
            poDS->GetRasterCount());  // only alpha band is accessed this way
        if (eRWFlag == GF_Read)
        {
            double dfZero = 0;
            GDALCopyWords64(&dfZero, GDT_Float64, 0, pData, eBufType,
                            static_cast<int>(nPixelSpace),
                            static_cast<size_t>(nBlockXSize) * nBlockYSize);
        }
        else
        {
            GDALCopyWords64(pData, eBufType, static_cast<int>(nPixelSpace),
                            m_pDstBuffer, eDataType,
                            GDALGetDataTypeSizeBytes(eDataType),
                            static_cast<size_t>(nBlockXSize) * nBlockYSize);
        }
        return CE_None;
    }
};

/************************************************************************/
/*                       FakeMaxZoomDataset                             */
/************************************************************************/

// This class is used to create a fake output dataset for GDALWarpOperation.
// In particular we need to implement GDALRasterBand::IRasterIO(GF_Write, ...)
// to catch writes (of one single tile) to the alpha band and redirect them
// to the dstBuffer passed to FakeMaxZoomDataset constructor.

class FakeMaxZoomDataset : public GDALDataset
{
    const int m_nBlockXSize;
    const int m_nBlockYSize;
    const OGRSpatialReference m_oSRS;
    double m_adfGT[6];

  public:
    FakeMaxZoomDataset(int nWidth, int nHeight, int nBandsIn, int nBlockXSize,
                       int nBlockYSize, GDALDataType eDT, const double adfGT[6],
                       const OGRSpatialReference &oSRS,
                       std::vector<GByte> &dstBuffer)
        : m_nBlockXSize(nBlockXSize), m_nBlockYSize(nBlockYSize), m_oSRS(oSRS)
    {
        eAccess = GA_Update;
        nRasterXSize = nWidth;
        nRasterYSize = nHeight;
        memcpy(m_adfGT, adfGT, sizeof(double) * 6);
        for (int i = 1; i <= nBandsIn; ++i)
        {
            SetBand(i,
                    new FakeMaxZoomRasterBand(
                        i, nWidth, nHeight, nBlockXSize, nBlockYSize, eDT,
                        dstBuffer.data() + static_cast<size_t>(i - 1) *
                                               nBlockXSize * nBlockYSize *
                                               GDALGetDataTypeSizeBytes(eDT)));
        }
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    CPLErr GetGeoTransform(double *padfGT) override
    {
        memcpy(padfGT, m_adfGT, sizeof(double) * 6);
        return CE_None;
    }

    using GDALDataset::Clone;

    std::unique_ptr<FakeMaxZoomDataset>
    Clone(std::vector<GByte> &dstBuffer) const
    {
        return std::make_unique<FakeMaxZoomDataset>(
            nRasterXSize, nRasterYSize, nBands, m_nBlockXSize, m_nBlockYSize,
            GetRasterBand(1)->GetRasterDataType(), m_adfGT, m_oSRS, dstBuffer);
    }
};

/************************************************************************/
/*                          MosaicRasterBand                            */
/************************************************************************/

class MosaicRasterBand : public GDALRasterBand
{
    const int m_tileMinX;
    const int m_tileMinY;
    const GDALColorInterp m_eColorInterp;
    const gdal::TileMatrixSet::TileMatrix m_oTM;
    const std::string m_convention;
    const std::string m_directory;
    const std::string m_extension;
    const bool m_hasNoData;
    const double m_noData;
    std::unique_ptr<GDALColorTable> m_poColorTable{};

  public:
    MosaicRasterBand(GDALDataset *poDSIn, int nBandIn, int nWidth, int nHeight,
                     int nBlockXSizeIn, int nBlockYSizeIn, GDALDataType eDT,
                     GDALColorInterp eColorInterp, int nTileMinX, int nTileMinY,
                     const gdal::TileMatrixSet::TileMatrix &oTM,
                     const std::string &convention,
                     const std::string &directory, const std::string &extension,
                     const double *pdfDstNoData,
                     const GDALColorTable *poColorTable)
        : m_tileMinX(nTileMinX), m_tileMinY(nTileMinY),
          m_eColorInterp(eColorInterp), m_oTM(oTM), m_convention(convention),
          m_directory(directory), m_extension(extension),
          m_hasNoData(pdfDstNoData != nullptr),
          m_noData(pdfDstNoData ? *pdfDstNoData : 0),
          m_poColorTable(poColorTable ? poColorTable->Clone() : nullptr)
    {
        poDS = poDSIn;
        nBand = nBandIn;
        nRasterXSize = nWidth;
        nRasterYSize = nHeight;
        nBlockXSize = nBlockXSizeIn;
        nBlockYSize = nBlockYSizeIn;
        eDataType = eDT;
    }

    CPLErr IReadBlock(int nXBlock, int nYBlock, void *pData) override;

    GDALColorTable *GetColorTable() override
    {
        return m_poColorTable.get();
    }

    GDALColorInterp GetColorInterpretation() override
    {
        return m_eColorInterp;
    }

    double GetNoDataValue(int *pbHasNoData) override
    {
        if (pbHasNoData)
            *pbHasNoData = m_hasNoData;
        return m_noData;
    }
};

/************************************************************************/
/*                         MosaicDataset                                */
/************************************************************************/

// This class is to expose the tiles of a given level as a mosaic that
// can be used as a source to generate the immediately below zoom level.

class MosaicDataset : public GDALDataset
{
    friend class MosaicRasterBand;

    const std::string m_directory;
    const std::string m_extension;
    const std::string m_format;
    GDALDataset *const m_poSrcDS;
    const gdal::TileMatrixSet::TileMatrix &m_oTM;
    const OGRSpatialReference m_oSRS;
    const int m_nTileMinX;
    const int m_nTileMinY;
    const int m_nTileMaxX;
    const int m_nTileMaxY;
    const std::string m_convention;
    const GDALDataType m_eDT;
    const double *const m_pdfDstNoData;
    const std::vector<std::string> &m_metadata;
    const GDALColorTable *const m_poCT;

    double m_adfGT[6];
    lru11::Cache<std::string, std::shared_ptr<GDALDataset>> m_oCacheTile{};

    CPL_DISALLOW_COPY_ASSIGN(MosaicDataset)

  public:
    MosaicDataset(const std::string &directory, const std::string &extension,
                  const std::string &format, GDALDataset *poSrcDS,
                  const gdal::TileMatrixSet::TileMatrix &oTM,
                  const OGRSpatialReference &oSRS, int nTileMinX, int nTileMinY,
                  int nTileMaxX, int nTileMaxY, const std::string &convention,
                  int nBandsIn, GDALDataType eDT, const double *pdfDstNoData,
                  const std::vector<std::string> &metadata,
                  const GDALColorTable *poCT)
        : m_directory(directory), m_extension(extension), m_format(format),
          m_poSrcDS(poSrcDS), m_oTM(oTM), m_oSRS(oSRS), m_nTileMinX(nTileMinX),
          m_nTileMinY(nTileMinY), m_nTileMaxX(nTileMaxX),
          m_nTileMaxY(nTileMaxY), m_convention(convention), m_eDT(eDT),
          m_pdfDstNoData(pdfDstNoData), m_metadata(metadata), m_poCT(poCT)
    {
        nRasterXSize = (nTileMaxX - nTileMinX + 1) * oTM.mTileWidth;
        nRasterYSize = (nTileMaxY - nTileMinY + 1) * oTM.mTileHeight;
        m_adfGT[0] = oTM.mTopLeftX + nTileMinX * oTM.mResX * oTM.mTileWidth;
        m_adfGT[1] = oTM.mResX;
        m_adfGT[2] = 0;
        m_adfGT[3] = oTM.mTopLeftY - nTileMinY * oTM.mResY * oTM.mTileHeight;
        m_adfGT[4] = 0;
        m_adfGT[5] = -oTM.mResY;
        for (int i = 1; i <= nBandsIn; ++i)
        {
            const GDALColorInterp eColorInterp =
                (i <= poSrcDS->GetRasterCount())
                    ? poSrcDS->GetRasterBand(i)->GetColorInterpretation()
                    : GCI_AlphaBand;
            SetBand(i, new MosaicRasterBand(
                           this, i, nRasterXSize, nRasterYSize, oTM.mTileWidth,
                           oTM.mTileHeight, eDT, eColorInterp, nTileMinX,
                           nTileMinY, oTM, convention, directory, extension,
                           pdfDstNoData, poCT));
        }
        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        const CPLStringList aosMD(metadata);
        for (const auto [key, value] : cpl::IterateNameValue(aosMD))
        {
            SetMetadataItem(key, value);
        }
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    CPLErr GetGeoTransform(double *padfGT) override
    {
        memcpy(padfGT, m_adfGT, sizeof(double) * 6);
        return CE_None;
    }

    using GDALDataset::Clone;

    std::unique_ptr<MosaicDataset> Clone() const
    {
        return std::make_unique<MosaicDataset>(
            m_directory, m_extension, m_format, m_poSrcDS, m_oTM, m_oSRS,
            m_nTileMinX, m_nTileMinY, m_nTileMaxX, m_nTileMaxY, m_convention,
            nBands, m_eDT, m_pdfDstNoData, m_metadata, m_poCT);
    }
};

/************************************************************************/
/*                   MosaicRasterBand::IReadBlock()                     */
/************************************************************************/

CPLErr MosaicRasterBand::IReadBlock(int nXBlock, int nYBlock, void *pData)
{
    auto poThisDS = cpl::down_cast<MosaicDataset *>(poDS);
    std::string filename = CPLFormFilenameSafe(
        m_directory.c_str(), CPLSPrintf("%d", m_tileMinX + nXBlock), nullptr);
    const int iFileY = GetFileY(m_tileMinY + nYBlock, m_oTM, m_convention);
    filename = CPLFormFilenameSafe(filename.c_str(), CPLSPrintf("%d", iFileY),
                                   m_extension.c_str());

    std::shared_ptr<GDALDataset> poTileDS;
    if (!poThisDS->m_oCacheTile.tryGet(filename, poTileDS))
    {
        const char *const apszAllowedDrivers[] = {poThisDS->m_format.c_str(),
                                                  nullptr};
        const char *const apszAllowedDriversForCOG[] = {"GTiff", "LIBERTIFF",
                                                        nullptr};
        poTileDS.reset(GDALDataset::Open(
            filename.c_str(), GDAL_OF_RASTER | GDAL_OF_INTERNAL,
            EQUAL(poThisDS->m_format.c_str(), "COG") ? apszAllowedDriversForCOG
                                                     : apszAllowedDrivers));
        if (!poTileDS)
        {
            VSIStatBufL sStat;
            if (VSIStatL(filename.c_str(), &sStat) == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "File %s exists but cannot be opened with %s driver",
                         filename.c_str(), poThisDS->m_format.c_str());
                return CE_Failure;
            }
        }
        poThisDS->m_oCacheTile.insert(filename, poTileDS);
    }
    if (!poTileDS || nBand > poTileDS->GetRasterCount())
    {
        memset(pData,
               (poTileDS && (nBand == poTileDS->GetRasterCount() + 1)) ? 255
                                                                       : 0,
               static_cast<size_t>(nBlockXSize) * nBlockYSize *
                   GDALGetDataTypeSizeBytes(eDataType));
        return CE_None;
    }
    else
    {
        return poTileDS->GetRasterBand(nBand)->RasterIO(
            GF_Read, 0, 0, nBlockXSize, nBlockYSize, pData, nBlockXSize,
            nBlockYSize, eDataType, 0, 0, nullptr);
    }
}

}  // namespace

/************************************************************************/
/*                         ApplySubstitutions()                         */
/************************************************************************/

static void ApplySubstitutions(CPLString &s,
                               const std::map<std::string, std::string> &substs)
{
    for (const auto &[key, value] : substs)
    {
        s.replaceAll("%(" + key + ")s", value);
        s.replaceAll("%(" + key + ")d", value);
        s.replaceAll("%(" + key + ")f", value);
        s.replaceAll("${" + key + "}", value);
    }
}

/************************************************************************/
/*                           GenerateLeaflet()                          */
/************************************************************************/

static void GenerateLeaflet(const std::string &osDirectory,
                            const std::string &osTitle, double dfSouthLat,
                            double dfWestLon, double dfNorthLat,
                            double dfEastLon, int nMinZoom, int nMaxZoom,
                            int nTileSize, const std::string &osExtension,
                            const std::string &osURL,
                            const std::string &osCopyright, bool bXYZ)
{
    if (const char *pszTemplate = CPLFindFile("gdal", "leaflet_template.html"))
    {
        const std::string osFilename(pszTemplate);
        std::map<std::string, std::string> substs;

        // For tests
        const char *pszFmt =
            atoi(CPLGetConfigOption("GDAL_RASTER_TILE_HTML_PREC", "17")) == 10
                ? "%.10g"
                : "%.17g";

        substs["double_quote_escaped_title"] =
            CPLString(osTitle).replaceAll('"', "\\\"");
        char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
        substs["xml_escaped_title"] = pszStr;
        CPLFree(pszStr);
        substs["south"] = CPLSPrintf(pszFmt, dfSouthLat);
        substs["west"] = CPLSPrintf(pszFmt, dfWestLon);
        substs["north"] = CPLSPrintf(pszFmt, dfNorthLat);
        substs["east"] = CPLSPrintf(pszFmt, dfEastLon);
        substs["centerlon"] = CPLSPrintf(pszFmt, (dfNorthLat + dfSouthLat) / 2);
        substs["centerlat"] = CPLSPrintf(pszFmt, (dfWestLon + dfEastLon) / 2);
        substs["minzoom"] = CPLSPrintf("%d", nMinZoom);
        substs["maxzoom"] = CPLSPrintf("%d", nMaxZoom);
        substs["beginzoom"] = CPLSPrintf("%d", nMaxZoom);
        substs["tile_size"] = CPLSPrintf("%d", nTileSize);  // not used
        substs["tileformat"] = osExtension;
        substs["publishurl"] = osURL;  // not used
        substs["copyright"] = CPLString(osCopyright).replaceAll('"', "\\\"");
        substs["tms"] = bXYZ ? "0" : "1";

        GByte *pabyRet = nullptr;
        CPL_IGNORE_RET_VAL(VSIIngestFile(nullptr, osFilename.c_str(), &pabyRet,
                                         nullptr, 10 * 1024 * 1024));
        if (pabyRet)
        {
            CPLString osHTML(reinterpret_cast<char *>(pabyRet));
            CPLFree(pabyRet);

            ApplySubstitutions(osHTML, substs);

            VSILFILE *f = VSIFOpenL(CPLFormFilenameSafe(osDirectory.c_str(),
                                                        "leaflet.html", nullptr)
                                        .c_str(),
                                    "wb");
            if (f)
            {
                VSIFWriteL(osHTML.data(), 1, osHTML.size(), f);
                VSIFCloseL(f);
            }
        }
    }
}

/************************************************************************/
/*                           GenerateMapML()                            */
/************************************************************************/

static void
GenerateMapML(const std::string &osDirectory, const std::string &mapmlTemplate,
              const std::string &osTitle, int nMinTileX, int nMinTileY,
              int nMaxTileX, int nMaxTileY, int nMinZoom, int nMaxZoom,
              const std::string &osExtension, const std::string &osURL,
              const std::string &osCopyright, const gdal::TileMatrixSet &tms)
{
    if (const char *pszTemplate =
            (mapmlTemplate.empty() ? CPLFindFile("gdal", "template_tiles.mapml")
                                   : mapmlTemplate.c_str()))
    {
        const std::string osFilename(pszTemplate);
        std::map<std::string, std::string> substs;

        if (tms.identifier() == "GoogleMapsCompatible")
            substs["TILING_SCHEME"] = "OSMTILE";
        else if (tms.identifier() == "WorldCRS84Quad")
            substs["TILING_SCHEME"] = "WGS84";
        else
            substs["TILING_SCHEME"] = tms.identifier();

        substs["URL"] = osURL.empty() ? "./" : osURL;
        substs["MINTILEX"] = CPLSPrintf("%d", nMinTileX);
        substs["MINTILEY"] = CPLSPrintf("%d", nMinTileY);
        substs["MAXTILEX"] = CPLSPrintf("%d", nMaxTileX);
        substs["MAXTILEY"] = CPLSPrintf("%d", nMaxTileY);
        substs["CURZOOM"] = CPLSPrintf("%d", nMaxZoom);
        substs["MINZOOM"] = CPLSPrintf("%d", nMinZoom);
        substs["MAXZOOM"] = CPLSPrintf("%d", nMaxZoom);
        substs["TILEEXT"] = osExtension;
        char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
        substs["TITLE"] = pszStr;
        CPLFree(pszStr);
        substs["COPYRIGHT"] = osCopyright;

        GByte *pabyRet = nullptr;
        CPL_IGNORE_RET_VAL(VSIIngestFile(nullptr, osFilename.c_str(), &pabyRet,
                                         nullptr, 10 * 1024 * 1024));
        if (pabyRet)
        {
            CPLString osMAPML(reinterpret_cast<char *>(pabyRet));
            CPLFree(pabyRet);

            ApplySubstitutions(osMAPML, substs);

            VSILFILE *f = VSIFOpenL(
                CPLFormFilenameSafe(osDirectory.c_str(), "mapml.mapml", nullptr)
                    .c_str(),
                "wb");
            if (f)
            {
                VSIFWriteL(osMAPML.data(), 1, osMAPML.size(), f);
                VSIFCloseL(f);
            }
        }
    }
}

/************************************************************************/
/*                           GenerateOpenLayers()                       */
/************************************************************************/

static void GenerateOpenLayers(
    const std::string &osDirectory, const std::string &osTitle, double dfMinX,
    double dfMinY, double dfMaxX, double dfMaxY, int nMinZoom, int nMaxZoom,
    int nTileSize, const std::string &osExtension, const std::string &osURL,
    const std::string &osCopyright, const gdal::TileMatrixSet &tms,
    bool bInvertAxisTMS, const OGRSpatialReference &oSRS_TMS, bool bXYZ)
{
    std::map<std::string, std::string> substs;

    // For tests
    const char *pszFmt =
        atoi(CPLGetConfigOption("GDAL_RASTER_TILE_HTML_PREC", "17")) == 10
            ? "%.10g"
            : "%.17g";

    char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
    substs["xml_escaped_title"] = pszStr;
    CPLFree(pszStr);
    substs["ominx"] = CPLSPrintf(pszFmt, dfMinX);
    substs["ominy"] = CPLSPrintf(pszFmt, dfMinY);
    substs["omaxx"] = CPLSPrintf(pszFmt, dfMaxX);
    substs["omaxy"] = CPLSPrintf(pszFmt, dfMaxY);
    substs["center_x"] = CPLSPrintf(pszFmt, (dfMinX + dfMaxX) / 2);
    substs["center_y"] = CPLSPrintf(pszFmt, (dfMinY + dfMaxY) / 2);
    substs["minzoom"] = CPLSPrintf("%d", nMinZoom);
    substs["maxzoom"] = CPLSPrintf("%d", nMaxZoom);
    substs["tile_size"] = CPLSPrintf("%d", nTileSize);
    substs["tileformat"] = osExtension;
    substs["publishurl"] = osURL;
    substs["copyright"] = osCopyright;
    substs["sign_y"] = bXYZ ? "" : "-";

    CPLString s(R"raw(<!DOCTYPE html>
<html>
<head>
    <title>%(xml_escaped_title)s</title>
    <meta http-equiv="content-type" content="text/html; charset=utf-8"/>
    <meta http-equiv='imagetoolbar' content='no'/>
    <style type="text/css"> v\:* {behavior:url(#default#VML);}
        html, body { overflow: hidden; padding: 0; height: 100%; width: 100%; font-family: 'Lucida Grande',Geneva,Arial,Verdana,sans-serif; }
        body { margin: 10px; background: #fff; }
        h1 { margin: 0; padding: 6px; border:0; font-size: 20pt; }
        #header { height: 43px; padding: 0; background-color: #eee; border: 1px solid #888; }
        #subheader { height: 12px; text-align: right; font-size: 10px; color: #555;}
        #map { height: 90%; border: 1px solid #888; }
    </style>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/gh/openlayers/openlayers.github.io@main/dist/en/v7.0.0/legacy/ol.css" type="text/css">
    <script src="https://cdn.jsdelivr.net/gh/openlayers/openlayers.github.io@main/dist/en/v7.0.0/legacy/ol.js"></script>
    <script src="https://unpkg.com/ol-layerswitcher@4.1.1"></script>
    <link rel="stylesheet" href="https://unpkg.com/ol-layerswitcher@4.1.1/src/ol-layerswitcher.css" />
</head>
<body>
    <div id="header"><h1>%(xml_escaped_title)s</h1></div>
    <div id="subheader">Generated by <a href="https://gdal.org/programs/gdal_raster_tile.html">gdal raster tile</a>&nbsp;&nbsp;&nbsp;&nbsp;</div>
    <div id="map" class="map"></div>
    <div id="mouse-position"></div>
    <script type="text/javascript">
        var mousePositionControl = new ol.control.MousePosition({
            className: 'custom-mouse-position',
            target: document.getElementById('mouse-position'),
            undefinedHTML: '&nbsp;'
        });
        var map = new ol.Map({
            controls: ol.control.defaults.defaults().extend([mousePositionControl]),
            target: 'map',)raw");

    if (tms.identifier() == "GoogleMapsCompatible" ||
        tms.identifier() == "WorldCRS84Quad")
    {
        s += R"raw(
            layers: [
                new ol.layer.Group({
                        title: 'Base maps',
                        layers: [
                            new ol.layer.Tile({
                                title: 'OpenStreetMap',
                                type: 'base',
                                visible: true,
                                source: new ol.source.OSM()
                            }),
                        ]
                }),)raw";
    }

    if (tms.identifier() == "GoogleMapsCompatible")
    {
        s += R"raw(new ol.layer.Group({
                    title: 'Overlay',
                    layers: [
                        new ol.layer.Tile({
                            title: 'Overlay',
                            // opacity: 0.7,
                            extent: [%(ominx)f, %(ominy)f,%(omaxx)f, %(omaxy)f],
                            source: new ol.source.XYZ({
                                attributions: '%(copyright)s',
                                minZoom: %(minzoom)d,
                                maxZoom: %(maxzoom)d,
                                url: './{z}/{x}/{%(sign_y)sy}.%(tileformat)s',
                                tileSize: [%(tile_size)d, %(tile_size)d]
                            })
                        }),
                    ]
                }),)raw";
    }
    else if (tms.identifier() == "WorldCRS84Quad")
    {
        const double base_res = 180.0 / nTileSize;
        std::string resolutions = "[";
        for (int i = 0; i <= nMaxZoom; ++i)
        {
            if (i > 0)
                resolutions += ",";
            resolutions += CPLSPrintf(pszFmt, base_res / (1 << i));
        }
        resolutions += "]";
        substs["resolutions"] = std::move(resolutions);

        if (bXYZ)
        {
            substs["origin"] = "[-180,90]";
            substs["y_formula"] = "tileCoord[2]";
        }
        else
        {
            substs["origin"] = "[-180,-90]";
            substs["y_formula"] = "- 1 - tileCoord[2]";
        }

        s += R"raw(
                new ol.layer.Group({
                    title: 'Overlay',
                    layers: [
                        new ol.layer.Tile({
                            title: 'Overlay',
                            // opacity: 0.7,
                            extent: [%(ominx)f, %(ominy)f,%(omaxx)f, %(omaxy)f],
                            source: new ol.source.TileImage({
                                attributions: '%(copyright)s',
                                projection: 'EPSG:4326',
                                minZoom: %(minzoom)d,
                                maxZoom: %(maxzoom)d,
                                tileGrid: new ol.tilegrid.TileGrid({
                                    extent: [-180,-90,180,90],
                                    origin: %(origin)s,
                                    resolutions: %(resolutions)s,
                                    tileSize: [%(tile_size)d, %(tile_size)d]
                                }),
                                tileUrlFunction: function(tileCoord) {
                                    return ('./{z}/{x}/{y}.%(tileformat)s'
                                        .replace('{z}', String(tileCoord[0]))
                                        .replace('{x}', String(tileCoord[1]))
                                        .replace('{y}', String(%(y_formula)s)));
                                },
                            })
                        }),
                    ]
                }),)raw";
    }
    else
    {
        substs["maxres"] =
            CPLSPrintf(pszFmt, tms.tileMatrixList()[nMinZoom].mResX);
        std::string resolutions = "[";
        for (int i = 0; i <= nMaxZoom; ++i)
        {
            if (i > 0)
                resolutions += ",";
            resolutions += CPLSPrintf(pszFmt, tms.tileMatrixList()[i].mResX);
        }
        resolutions += "]";
        substs["resolutions"] = std::move(resolutions);

        std::string matrixsizes = "[";
        for (int i = 0; i <= nMaxZoom; ++i)
        {
            if (i > 0)
                matrixsizes += ",";
            matrixsizes +=
                CPLSPrintf("[%d,%d]", tms.tileMatrixList()[i].mMatrixWidth,
                           tms.tileMatrixList()[i].mMatrixHeight);
        }
        matrixsizes += "]";
        substs["matrixsizes"] = std::move(matrixsizes);

        double dfTopLeftX = tms.tileMatrixList()[0].mTopLeftX;
        double dfTopLeftY = tms.tileMatrixList()[0].mTopLeftY;
        if (bInvertAxisTMS)
            std::swap(dfTopLeftX, dfTopLeftY);

        if (bXYZ)
        {
            substs["origin"] =
                CPLSPrintf("[%.17g,%.17g]", dfTopLeftX, dfTopLeftY);
            substs["y_formula"] = "tileCoord[2]";
        }
        else
        {
            substs["origin"] = CPLSPrintf(
                "[%.17g,%.17g]", dfTopLeftX,
                dfTopLeftY - tms.tileMatrixList()[0].mResY *
                                 tms.tileMatrixList()[0].mTileHeight);
            substs["y_formula"] = "- 1 - tileCoord[2]";
        }

        substs["tilegrid_extent"] =
            CPLSPrintf("[%.17g,%.17g,%.17g,%.17g]", dfTopLeftX,
                       dfTopLeftY - tms.tileMatrixList()[0].mMatrixHeight *
                                        tms.tileMatrixList()[0].mResY *
                                        tms.tileMatrixList()[0].mTileHeight,
                       dfTopLeftX + tms.tileMatrixList()[0].mMatrixWidth *
                                        tms.tileMatrixList()[0].mResX *
                                        tms.tileMatrixList()[0].mTileWidth,
                       dfTopLeftY);

        s += R"raw(
            layers: [
                new ol.layer.Group({
                    title: 'Overlay',
                    layers: [
                        new ol.layer.Tile({
                            title: 'Overlay',
                            // opacity: 0.7,
                            extent: [%(ominx)f, %(ominy)f,%(omaxx)f, %(omaxy)f],
                            source: new ol.source.TileImage({
                                attributions: '%(copyright)s',
                                minZoom: %(minzoom)d,
                                maxZoom: %(maxzoom)d,
                                tileGrid: new ol.tilegrid.TileGrid({
                                    extent: %(tilegrid_extent)s,
                                    origin: %(origin)s,
                                    resolutions: %(resolutions)s,
                                    sizes: %(matrixsizes)s,
                                    tileSize: [%(tile_size)d, %(tile_size)d]
                                }),
                                tileUrlFunction: function(tileCoord) {
                                    return ('./{z}/{x}/{y}.%(tileformat)s'
                                        .replace('{z}', String(tileCoord[0]))
                                        .replace('{x}', String(tileCoord[1]))
                                        .replace('{y}', String(%(y_formula)s)));
                                },
                            })
                        }),
                    ]
                }),)raw";
    }

    s += R"raw(
            ],
            view: new ol.View({
                center: [%(center_x)f, %(center_y)f],)raw";

    if (tms.identifier() == "GoogleMapsCompatible" ||
        tms.identifier() == "WorldCRS84Quad")
    {
        substs["view_zoom"] = substs["minzoom"];
        if (tms.identifier() == "WorldCRS84Quad")
        {
            substs["view_zoom"] = CPLSPrintf("%d", nMinZoom + 1);
        }

        s += R"raw(
                zoom: %(view_zoom)d,)raw";
    }
    else
    {
        s += R"raw(
                resolution: %(maxres)f,)raw";
    }

    if (tms.identifier() == "WorldCRS84Quad")
    {
        s += R"raw(
                projection: 'EPSG:4326',)raw";
    }
    else if (!oSRS_TMS.IsEmpty() && tms.identifier() != "GoogleMapsCompatible")
    {
        const char *pszAuthName = oSRS_TMS.GetAuthorityName(nullptr);
        const char *pszAuthCode = oSRS_TMS.GetAuthorityCode(nullptr);
        if (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
        {
            substs["epsg_code"] = pszAuthCode;
            if (oSRS_TMS.IsGeographic())
            {
                substs["units"] = "deg";
            }
            else
            {
                const char *pszUnits = "";
                if (oSRS_TMS.GetLinearUnits(&pszUnits) == 1.0)
                    substs["units"] = "m";
                else
                    substs["units"] = pszUnits;
            }
            s += R"raw(
                projection: new ol.proj.Projection({code: 'EPSG:%(epsg_code)s', units:'%(units)s'}),)raw";
        }
    }

    s += R"raw(
            })
        });)raw";

    if (tms.identifier() == "GoogleMapsCompatible" ||
        tms.identifier() == "WorldCRS84Quad")
    {
        s += R"raw(
        map.addControl(new ol.control.LayerSwitcher());)raw";
    }

    s += R"raw(
    </script>
</body>
</html>)raw";

    ApplySubstitutions(s, substs);

    VSILFILE *f = VSIFOpenL(
        CPLFormFilenameSafe(osDirectory.c_str(), "openlayers.html", nullptr)
            .c_str(),
        "wb");
    if (f)
    {
        VSIFWriteL(s.data(), 1, s.size(), f);
        VSIFCloseL(f);
    }
}

/************************************************************************/
/*                           GetTileBoundingBox()                       */
/************************************************************************/

static void GetTileBoundingBox(int nTileX, int nTileY, int nTileZ,
                               const gdal::TileMatrixSet *poTMS,
                               bool bInvertAxisTMS,
                               OGRCoordinateTransformation *poCTToWGS84,
                               double &dfTLX, double &dfTLY, double &dfTRX,
                               double &dfTRY, double &dfLLX, double &dfLLY,
                               double &dfLRX, double &dfLRY)
{
    gdal::TileMatrixSet::TileMatrix tileMatrix =
        poTMS->tileMatrixList()[nTileZ];
    if (bInvertAxisTMS)
        std::swap(tileMatrix.mTopLeftX, tileMatrix.mTopLeftY);

    dfTLX = tileMatrix.mTopLeftX +
            nTileX * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfTLY = tileMatrix.mTopLeftY -
            nTileY * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfTLX, &dfTLY);

    dfTRX = tileMatrix.mTopLeftX +
            (nTileX + 1) * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfTRY = tileMatrix.mTopLeftY -
            nTileY * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfTRX, &dfTRY);

    dfLLX = tileMatrix.mTopLeftX +
            nTileX * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfLLY = tileMatrix.mTopLeftY -
            (nTileY + 1) * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfLLX, &dfLLY);

    dfLRX = tileMatrix.mTopLeftX +
            (nTileX + 1) * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfLRY = tileMatrix.mTopLeftY -
            (nTileY + 1) * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfLRX, &dfLRY);
}

/************************************************************************/
/*                           GenerateKML()                              */
/************************************************************************/

namespace
{
struct TileCoordinates
{
    int nTileX = 0;
    int nTileY = 0;
    int nTileZ = 0;
};
}  // namespace

static void GenerateKML(const std::string &osDirectory,
                        const std::string &osTitle, int nTileX, int nTileY,
                        int nTileZ, int nTileSize,
                        const std::string &osExtension,
                        const std::string &osURL,
                        const gdal::TileMatrixSet *poTMS, bool bInvertAxisTMS,
                        const std::string &convention,
                        OGRCoordinateTransformation *poCTToWGS84,
                        const std::vector<TileCoordinates> &children)
{
    std::map<std::string, std::string> substs;

    const bool bIsTileKML = nTileX >= 0;

    // For tests
    const char *pszFmt =
        atoi(CPLGetConfigOption("GDAL_RASTER_TILE_KML_PREC", "14")) == 10
            ? "%.10f"
            : "%.14f";

    substs["tx"] = CPLSPrintf("%d", nTileX);
    substs["tz"] = CPLSPrintf("%d", nTileZ);
    substs["tileformat"] = osExtension;
    substs["minlodpixels"] = CPLSPrintf("%d", nTileSize / 2);
    substs["maxlodpixels"] =
        children.empty() ? "-1" : CPLSPrintf("%d", nTileSize * 8);

    double dfTLX = 0;
    double dfTLY = 0;
    double dfTRX = 0;
    double dfTRY = 0;
    double dfLLX = 0;
    double dfLLY = 0;
    double dfLRX = 0;
    double dfLRY = 0;

    int nFileY = -1;
    if (!bIsTileKML)
    {
        char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
        substs["xml_escaped_title"] = pszStr;
        CPLFree(pszStr);
    }
    else
    {
        nFileY = GetFileY(nTileY, poTMS->tileMatrixList()[nTileZ], convention);
        substs["realtiley"] = CPLSPrintf("%d", nFileY);
        substs["xml_escaped_title"] =
            CPLSPrintf("%d/%d/%d.kml", nTileZ, nTileX, nFileY);

        GetTileBoundingBox(nTileX, nTileY, nTileZ, poTMS, bInvertAxisTMS,
                           poCTToWGS84, dfTLX, dfTLY, dfTRX, dfTRY, dfLLX,
                           dfLLY, dfLRX, dfLRY);
    }

    substs["drawOrder"] = CPLSPrintf("%d", nTileX == 0  ? 2 * nTileZ + 1
                                           : nTileX > 0 ? 2 * nTileZ
                                                        : 0);

    substs["url"] = osURL.empty() && bIsTileKML ? "../../" : "";

    const bool bIsRectangle =
        (dfTLX == dfLLX && dfTRX == dfLRX && dfTLY == dfTRY && dfLLY == dfLRY);
    const bool bUseGXNamespace = bIsTileKML && !bIsRectangle;

    substs["xmlns_gx"] = bUseGXNamespace
                             ? " xmlns:gx=\"http://www.google.com/kml/ext/2.2\""
                             : "";

    CPLString s(R"raw(<?xml version="1.0" encoding="utf-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2"%(xmlns_gx)s>
  <Document>
    <name>%(xml_escaped_title)s</name>
    <description></description>
    <Style>
      <ListStyle id="hideChildren">
        <listItemType>checkHideChildren</listItemType>
      </ListStyle>
    </Style>
)raw");
    ApplySubstitutions(s, substs);

    if (bIsTileKML)
    {
        CPLString s2(R"raw(    <Region>
      <LatLonAltBox>
        <north>%(north)f</north>
        <south>%(south)f</south>
        <east>%(east)f</east>
        <west>%(west)f</west>
      </LatLonAltBox>
      <Lod>
        <minLodPixels>%(minlodpixels)d</minLodPixels>
        <maxLodPixels>%(maxlodpixels)d</maxLodPixels>
      </Lod>
    </Region>
    <GroundOverlay>
      <drawOrder>%(drawOrder)d</drawOrder>
      <Icon>
        <href>%(realtiley)d.%(tileformat)s</href>
      </Icon>
      <LatLonBox>
        <north>%(north)f</north>
        <south>%(south)f</south>
        <east>%(east)f</east>
        <west>%(west)f</west>
      </LatLonBox>
)raw");

        if (!bIsRectangle)
        {
            s2 +=
                R"raw(      <gx:LatLonQuad><coordinates>%(LLX)f,%(LLY)f %(LRX)f,%(LRY)f %(TRX)f,%(TRY)f %(TLX)f,%(TLY)f</coordinates></gx:LatLonQuad>
)raw";
        }

        s2 += R"raw(    </GroundOverlay>
)raw";
        substs["north"] = CPLSPrintf(pszFmt, std::max(dfTLY, dfTRY));
        substs["south"] = CPLSPrintf(pszFmt, std::min(dfLLY, dfLRY));
        substs["east"] = CPLSPrintf(pszFmt, std::max(dfTRX, dfLRX));
        substs["west"] = CPLSPrintf(pszFmt, std::min(dfLLX, dfTLX));

        if (!bIsRectangle)
        {
            substs["TLX"] = CPLSPrintf(pszFmt, dfTLX);
            substs["TLY"] = CPLSPrintf(pszFmt, dfTLY);
            substs["TRX"] = CPLSPrintf(pszFmt, dfTRX);
            substs["TRY"] = CPLSPrintf(pszFmt, dfTRY);
            substs["LRX"] = CPLSPrintf(pszFmt, dfLRX);
            substs["LRY"] = CPLSPrintf(pszFmt, dfLRY);
            substs["LLX"] = CPLSPrintf(pszFmt, dfLLX);
            substs["LLY"] = CPLSPrintf(pszFmt, dfLLY);
        }

        ApplySubstitutions(s2, substs);
        s += s2;
    }

    for (const auto &child : children)
    {
        substs["tx"] = CPLSPrintf("%d", child.nTileX);
        substs["tz"] = CPLSPrintf("%d", child.nTileZ);
        substs["realtiley"] = CPLSPrintf(
            "%d", GetFileY(child.nTileY, poTMS->tileMatrixList()[child.nTileZ],
                           convention));

        GetTileBoundingBox(child.nTileX, child.nTileY, child.nTileZ, poTMS,
                           bInvertAxisTMS, poCTToWGS84, dfTLX, dfTLY, dfTRX,
                           dfTRY, dfLLX, dfLLY, dfLRX, dfLRY);

        CPLString s2(R"raw(    <NetworkLink>
      <name>%(tz)d/%(tx)d/%(realtiley)d.%(tileformat)s</name>
      <Region>
        <LatLonAltBox>
          <north>%(north)f</north>
          <south>%(south)f</south>
          <east>%(east)f</east>
          <west>%(west)f</west>
        </LatLonAltBox>
        <Lod>
          <minLodPixels>%(minlodpixels)d</minLodPixels>
          <maxLodPixels>-1</maxLodPixels>
        </Lod>
      </Region>
      <Link>
        <href>%(url)s%(tz)d/%(tx)d/%(realtiley)d.kml</href>
        <viewRefreshMode>onRegion</viewRefreshMode>
        <viewFormat/>
      </Link>
    </NetworkLink>
)raw");
        substs["north"] = CPLSPrintf(pszFmt, std::max(dfTLY, dfTRY));
        substs["south"] = CPLSPrintf(pszFmt, std::min(dfLLY, dfLRY));
        substs["east"] = CPLSPrintf(pszFmt, std::max(dfTRX, dfLRX));
        substs["west"] = CPLSPrintf(pszFmt, std::min(dfLLX, dfTLX));
        ApplySubstitutions(s2, substs);
        s += s2;
    }

    s += R"raw(</Document>
</kml>)raw";

    std::string osFilename(osDirectory);
    if (!bIsTileKML)
    {
        osFilename =
            CPLFormFilenameSafe(osFilename.c_str(), "doc.kml", nullptr);
    }
    else
    {
        osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                         CPLSPrintf("%d", nTileZ), nullptr);
        osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                         CPLSPrintf("%d", nTileX), nullptr);
        osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                         CPLSPrintf("%d.kml", nFileY), nullptr);
    }

    VSILFILE *f = VSIFOpenL(osFilename.c_str(), "wb");
    if (f)
    {
        VSIFWriteL(s.data(), 1, s.size(), f);
        VSIFCloseL(f);
    }
}

namespace
{

/************************************************************************/
/*                            ResourceManager                           */
/************************************************************************/

// Generic cache managing resources
template <class Resource> class ResourceManager /* non final */
{
  public:
    virtual ~ResourceManager() = default;

    std::unique_ptr<Resource> AcquireResources()
    {
        std::lock_guard oLock(m_oMutex);
        if (!m_oResources.empty())
        {
            auto ret = std::move(m_oResources.back());
            m_oResources.pop_back();
            return ret;
        }

        return CreateResources();
    }

    void ReleaseResources(std::unique_ptr<Resource> resources)
    {
        std::lock_guard oLock(m_oMutex);
        m_oResources.push_back(std::move(resources));
    }

    void SetError()
    {
        std::lock_guard oLock(m_oMutex);
        if (m_errorMsg.empty())
            m_errorMsg = CPLGetLastErrorMsg();
    }

    const std::string &GetErrorMsg() const
    {
        std::lock_guard oLock(m_oMutex);
        return m_errorMsg;
    }

  protected:
    virtual std::unique_ptr<Resource> CreateResources() = 0;

  private:
    mutable std::mutex m_oMutex{};
    std::vector<std::unique_ptr<Resource>> m_oResources{};
    std::string m_errorMsg{};
};

/************************************************************************/
/*                         PerThreadMaxZoomResources                    */
/************************************************************************/

// Per-thread resources for generation of tiles at full resolution
struct PerThreadMaxZoomResources
{
    struct GDALDatasetReleaser
    {
        void operator()(GDALDataset *poDS)
        {
            if (poDS)
                poDS->ReleaseRef();
        }
    };

    std::unique_ptr<GDALDataset, GDALDatasetReleaser> poSrcDS{};
    std::vector<GByte> dstBuffer{};
    std::unique_ptr<FakeMaxZoomDataset> poFakeMaxZoomDS{};
    std::unique_ptr<void, decltype(&GDALDestroyTransformer)> poTransformer{
        nullptr, GDALDestroyTransformer};
    std::unique_ptr<GDALWarpOperation> poWO{};
};

/************************************************************************/
/*                      PerThreadMaxZoomResourceManager                 */
/************************************************************************/

// Manage a cache of PerThreadMaxZoomResources instances
class PerThreadMaxZoomResourceManager final
    : public ResourceManager<PerThreadMaxZoomResources>
{
  public:
    PerThreadMaxZoomResourceManager(GDALDataset *poSrcDS,
                                    const GDALWarpOptions *psWO,
                                    void *pTransformerArg,
                                    const FakeMaxZoomDataset &oFakeMaxZoomDS,
                                    size_t nBufferSize)
        : m_poSrcDS(poSrcDS), m_psWOSource(psWO),
          m_pTransformerArg(pTransformerArg), m_oFakeMaxZoomDS(oFakeMaxZoomDS),
          m_nBufferSize(nBufferSize)
    {
    }

  protected:
    std::unique_ptr<PerThreadMaxZoomResources> CreateResources() override
    {
        auto ret = std::make_unique<PerThreadMaxZoomResources>();

        ret->poSrcDS.reset(GDALGetThreadSafeDataset(m_poSrcDS, GDAL_OF_RASTER));
        if (!ret->poSrcDS)
            return nullptr;

        try
        {
            ret->dstBuffer.resize(m_nBufferSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating temporary buffer");
            return nullptr;
        }

        ret->poFakeMaxZoomDS = m_oFakeMaxZoomDS.Clone(ret->dstBuffer);

        ret->poTransformer.reset(GDALCloneTransformer(m_pTransformerArg));
        if (!ret->poTransformer)
            return nullptr;

        auto psWO =
            std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)>(
                GDALCloneWarpOptions(m_psWOSource), GDALDestroyWarpOptions);
        if (!psWO)
            return nullptr;

        psWO->hSrcDS = GDALDataset::ToHandle(ret->poSrcDS.get());
        psWO->hDstDS = GDALDataset::ToHandle(ret->poFakeMaxZoomDS.get());
        psWO->pTransformerArg = ret->poTransformer.get();
        psWO->pfnTransformer = m_psWOSource->pfnTransformer;

        ret->poWO = std::make_unique<GDALWarpOperation>();
        if (ret->poWO->Initialize(psWO.get()) != CE_None)
            return nullptr;

        return ret;
    }

  private:
    GDALDataset *const m_poSrcDS;
    const GDALWarpOptions *const m_psWOSource;
    void *const m_pTransformerArg;
    const FakeMaxZoomDataset &m_oFakeMaxZoomDS;
    const size_t m_nBufferSize;

    CPL_DISALLOW_COPY_ASSIGN(PerThreadMaxZoomResourceManager)
};

/************************************************************************/
/*                       PerThreadLowerZoomResources                    */
/************************************************************************/

// Per-thread resources for generation of tiles at zoom level < max
struct PerThreadLowerZoomResources
{
    std::unique_ptr<GDALDataset> poSrcDS{};
};

/************************************************************************/
/*                   PerThreadLowerZoomResourceManager                  */
/************************************************************************/

// Manage a cache of PerThreadLowerZoomResources instances
class PerThreadLowerZoomResourceManager final
    : public ResourceManager<PerThreadLowerZoomResources>
{
  public:
    explicit PerThreadLowerZoomResourceManager(const MosaicDataset &oSrcDS)
        : m_oSrcDS(oSrcDS)
    {
    }

  protected:
    std::unique_ptr<PerThreadLowerZoomResources> CreateResources() override
    {
        auto ret = std::make_unique<PerThreadLowerZoomResources>();
        ret->poSrcDS = m_oSrcDS.Clone();
        return ret;
    }

  private:
    const MosaicDataset &m_oSrcDS;
};

}  // namespace

/************************************************************************/
/*                  GDALRasterTileAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALRasterTileAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    auto poSrcDS = m_dataset.GetDatasetRef();
    CPLAssert(poSrcDS);
    const int nSrcWidth = poSrcDS->GetRasterXSize();
    const int nSrcHeight = poSrcDS->GetRasterYSize();
    if (poSrcDS->GetRasterCount() == 0 || nSrcWidth == 0 || nSrcHeight == 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Invalid source dataset");
        return false;
    }

    if (m_resampling == "near")
        m_resampling = "nearest";
    if (m_overviewResampling == "near")
        m_overviewResampling = "nearest";
    else if (m_overviewResampling.empty())
        m_overviewResampling = m_resampling;

    CPLStringList aosWarpOptions;
    if (!m_excludedValues.empty() || m_nodataValuesPctThreshold < 100)
    {
        aosWarpOptions.SetNameValue(
            "NODATA_VALUES_PCT_THRESHOLD",
            CPLSPrintf("%g", m_nodataValuesPctThreshold));
        if (!m_excludedValues.empty())
        {
            aosWarpOptions.SetNameValue("EXCLUDED_VALUES",
                                        m_excludedValues.c_str());
            aosWarpOptions.SetNameValue(
                "EXCLUDED_VALUES_PCT_THRESHOLD",
                CPLSPrintf("%g", m_excludedValuesPctThreshold));
        }
    }

    if (poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
            GCI_PaletteIndex &&
        ((m_resampling != "nearest" && m_resampling != "mode") ||
         (m_overviewResampling != "nearest" && m_overviewResampling != "mode")))
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Datasets with color table not supported with non-nearest "
                    "or non-mode resampling. Run 'gdal raster "
                    "color-map' before or set the 'resampling' argument to "
                    "'nearest' or 'mode'.");
        return false;
    }

    const auto eSrcDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    auto poDstDriver =
        GetGDALDriverManager()->GetDriverByName(m_outputFormat.c_str());
    if (!poDstDriver)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Invalid value for argument 'output-format'. Driver '%s' "
                    "does not exist",
                    m_outputFormat.c_str());
        return false;
    }

    if (m_outputFormat == "PNG")
    {
        if (poSrcDS->GetRasterCount() > 4)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only up to 4 bands supported for PNG.");
            return false;
        }
        if (eSrcDT != GDT_Byte && eSrcDT != GDT_UInt16)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only Byte and UInt16 data types supported for PNG.");
            return false;
        }
    }
    else if (m_outputFormat == "JPEG")
    {
        if (poSrcDS->GetRasterCount() > 4)
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Only up to 4 bands supported for JPEG (with alpha ignored).");
            return false;
        }
        const bool bUInt16Supported = strstr(
            poDstDriver->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES), "UInt16");
        if (eSrcDT != GDT_Byte && !(eSrcDT == GDT_UInt16 && bUInt16Supported))
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                bUInt16Supported
                    ? "Only Byte and UInt16 data types supported for JPEG."
                    : "Only Byte data type supported for JPEG.");
            return false;
        }
        if (eSrcDT == GDT_UInt16)
        {
            if (const char *pszNBITS =
                    poSrcDS->GetRasterBand(1)->GetMetadataItem(
                        "NBITS", "IMAGE_STRUCTURE"))
            {
                if (atoi(pszNBITS) > 12)
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "JPEG output only supported up to 12 bits");
                    return false;
                }
            }
            else
            {
                double adfMinMax[2] = {0, 0};
                poSrcDS->GetRasterBand(1)->ComputeRasterMinMax(
                    /* bApproxOK = */ true, adfMinMax);
                if (adfMinMax[1] >= (1 << 12))
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "JPEG output only supported up to 12 bits");
                    return false;
                }
            }
        }
    }
    else if (m_outputFormat == "WEBP")
    {
        if (poSrcDS->GetRasterCount() != 3 && poSrcDS->GetRasterCount() != 4)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only 3 or 4 bands supported for WEBP.");
            return false;
        }
        if (eSrcDT != GDT_Byte)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only Byte data type supported for WEBP.");
            return false;
        }
    }

    const char *pszExtensions =
        poDstDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
    CPLAssert(pszExtensions && pszExtensions[0] != 0);
    const CPLStringList aosExtensions(
        CSLTokenizeString2(pszExtensions, " ", 0));
    const char *pszExtension = aosExtensions[0];
    std::array<double, 6> adfSrcGeoTransform;
    const bool bHasSrcGT =
        poSrcDS->GetGeoTransform(adfSrcGeoTransform.data()) == CE_None;
    const bool bHasNorthUpSrcGT = bHasSrcGT && adfSrcGeoTransform[2] == 0 &&
                                  adfSrcGeoTransform[4] == 0 &&
                                  adfSrcGeoTransform[5] < 0;
    OGRSpatialReference oSRS_TMS;

    if (m_tilingScheme == "raster")
    {
        if (const auto poSRS = poSrcDS->GetSpatialRef())
            oSRS_TMS = *poSRS;
    }
    else
    {
        if (!bHasSrcGT && poSrcDS->GetGCPCount() == 0 &&
            poSrcDS->GetMetadata("GEOLOCATION") == nullptr &&
            poSrcDS->GetMetadata("RPC") == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Ungeoreferenced datasets are not supported, unless "
                        "'tiling-scheme' is set to 'raster'");
            return false;
        }

        if (poSrcDS->GetMetadata("GEOLOCATION") == nullptr &&
            poSrcDS->GetMetadata("RPC") == nullptr &&
            poSrcDS->GetSpatialRef() == nullptr &&
            poSrcDS->GetGCPSpatialRef() == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Ungeoreferenced datasets are not supported, unless "
                        "'tiling-scheme' is set to 'raster'");
            return false;
        }
    }

    if (m_copySrcMetadata)
    {
        CPLStringList aosMD(CSLDuplicate(poSrcDS->GetMetadata()));
        const CPLStringList aosNewMD(m_metadata);
        for (const auto [key, value] : cpl::IterateNameValue(aosNewMD))
        {
            aosMD.SetNameValue(key, value);
        }
        m_metadata = aosMD;
    }

    std::array<double, 6> adfSrcGeoTransformModif{0, 1, 0, 0, 0, -1};

    if (m_tilingScheme == "mercator")
        m_tilingScheme = "WebMercatorQuad";
    else if (m_tilingScheme == "geodetic")
        m_tilingScheme = "WorldCRS84Quad";
    else if (m_tilingScheme == "raster")
    {
        if (m_tileSize == 0)
            m_tileSize = 256;
        if (m_maxZoomLevel < 0)
        {
            m_maxZoomLevel = static_cast<int>(std::ceil(std::log2(
                std::max(1, std::max(nSrcWidth, nSrcHeight) / m_tileSize))));
        }
        if (bHasNorthUpSrcGT)
        {
            adfSrcGeoTransformModif = adfSrcGeoTransform;
        }
    }

    auto poTMS =
        m_tilingScheme == "raster"
            ? gdal::TileMatrixSet::createRaster(
                  nSrcWidth, nSrcHeight, m_tileSize, 1 + m_maxZoomLevel,
                  adfSrcGeoTransformModif[0], adfSrcGeoTransformModif[3],
                  adfSrcGeoTransformModif[1], -adfSrcGeoTransformModif[5],
                  oSRS_TMS.IsEmpty() ? std::string() : oSRS_TMS.exportToWkt())
            : gdal::TileMatrixSet::parse(
                  m_mapTileMatrixIdentifierToScheme[m_tilingScheme].c_str());
    // Enforced by SetChoices() on the m_tilingScheme argument
    CPLAssert(poTMS && !poTMS->hasVariableMatrixWidth());

    CPLStringList aosTO;
    if (m_tilingScheme == "raster")
    {
        aosTO.SetNameValue("SRC_METHOD", "GEOTRANSFORM");
    }
    else
    {
        CPL_IGNORE_RET_VAL(oSRS_TMS.SetFromUserInput(poTMS->crs().c_str()));
        aosTO.SetNameValue("DST_SRS", oSRS_TMS.exportToWkt().c_str());
    }

    const char *pszAuthName = oSRS_TMS.GetAuthorityName(nullptr);
    const char *pszAuthCode = oSRS_TMS.GetAuthorityCode(nullptr);
    const int nEPSGCode =
        (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
            ? atoi(pszAuthCode)
            : 0;

    const bool bInvertAxisTMS =
        m_tilingScheme != "raster" &&
        (oSRS_TMS.EPSGTreatsAsLatLong() != FALSE ||
         oSRS_TMS.EPSGTreatsAsNorthingEasting() != FALSE);

    oSRS_TMS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    std::unique_ptr<void, decltype(&GDALDestroyTransformer)> hTransformArg(
        nullptr, GDALDestroyTransformer);

    // Hack to compensate for GDALSuggestedWarpOutput2() failure (or not
    // ideal suggestion with PROJ 8) when reprojecting latitude = +/- 90 to
    // EPSG:3857.
    std::unique_ptr<GDALDataset> poTmpDS;
    bool bEPSG3857Adjust = false;
    if (nEPSGCode == 3857 && bHasNorthUpSrcGT)
    {
        const auto poSrcSRS = poSrcDS->GetSpatialRef();
        if (poSrcSRS && poSrcSRS->IsGeographic())
        {
            double maxLat = adfSrcGeoTransform[3];
            double minLat =
                adfSrcGeoTransform[3] + nSrcHeight * adfSrcGeoTransform[5];
            // Corresponds to the latitude of below MAX_GM
            constexpr double MAX_LAT = 85.0511287798066;
            bool bModified = false;
            if (maxLat > MAX_LAT)
            {
                maxLat = MAX_LAT;
                bModified = true;
            }
            if (minLat < -MAX_LAT)
            {
                minLat = -MAX_LAT;
                bModified = true;
            }
            if (bModified)
            {
                CPLStringList aosOptions;
                aosOptions.AddString("-of");
                aosOptions.AddString("VRT");
                aosOptions.AddString("-projwin");
                aosOptions.AddString(
                    CPLSPrintf("%.17g", adfSrcGeoTransform[0]));
                aosOptions.AddString(CPLSPrintf("%.17g", maxLat));
                aosOptions.AddString(
                    CPLSPrintf("%.17g", adfSrcGeoTransform[0] +
                                            nSrcWidth * adfSrcGeoTransform[1]));
                aosOptions.AddString(CPLSPrintf("%.17g", minLat));
                auto psOptions =
                    GDALTranslateOptionsNew(aosOptions.List(), nullptr);
                poTmpDS.reset(GDALDataset::FromHandle(GDALTranslate(
                    "", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
                GDALTranslateOptionsFree(psOptions);
                if (poTmpDS)
                {
                    bEPSG3857Adjust = true;
                    hTransformArg.reset(GDALCreateGenImgProjTransformer2(
                        GDALDataset::FromHandle(poTmpDS.get()), nullptr,
                        aosTO.List()));
                }
            }
        }
    }

    std::array<double, 6> adfDstGeoTransform;
    double adfExtent[4];
    int nXSize, nYSize;

    bool bSuggestOK;
    if (m_tilingScheme == "raster")
    {
        bSuggestOK = true;
        nXSize = nSrcWidth;
        nYSize = nSrcHeight;
        adfDstGeoTransform = adfSrcGeoTransformModif;
        adfExtent[0] = adfDstGeoTransform[0];
        adfExtent[1] =
            adfDstGeoTransform[3] + nSrcHeight * adfDstGeoTransform[5];
        adfExtent[2] =
            adfDstGeoTransform[0] + nSrcWidth * adfDstGeoTransform[1];
        adfExtent[3] = adfDstGeoTransform[3];
    }
    else
    {
        if (!hTransformArg)
        {
            hTransformArg.reset(GDALCreateGenImgProjTransformer2(
                poSrcDS, nullptr, aosTO.List()));
        }
        if (!hTransformArg)
        {
            return false;
        }
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        bSuggestOK =
            (GDALSuggestedWarpOutput2(
                 poSrcDS,
                 static_cast<GDALTransformerInfo *>(hTransformArg.get())
                     ->pfnTransform,
                 hTransformArg.get(), adfDstGeoTransform.data(), &nXSize,
                 &nYSize, adfExtent, 0) == CE_None);
    }
    if (!bSuggestOK)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot determine extent of raster in target CRS");
        return false;
    }

    poTmpDS.reset();

    if (bEPSG3857Adjust)
    {
        constexpr double SPHERICAL_RADIUS = 6378137.0;
        constexpr double MAX_GM =
            SPHERICAL_RADIUS * M_PI;  // 20037508.342789244
        double maxNorthing = adfDstGeoTransform[3];
        double minNorthing =
            adfDstGeoTransform[3] + adfDstGeoTransform[5] * nYSize;
        bool bChanged = false;
        if (maxNorthing > MAX_GM)
        {
            bChanged = true;
            maxNorthing = MAX_GM;
        }
        if (minNorthing < -MAX_GM)
        {
            bChanged = true;
            minNorthing = -MAX_GM;
        }
        if (bChanged)
        {
            adfDstGeoTransform[3] = maxNorthing;
            nYSize = int(
                (maxNorthing - minNorthing) / (-adfDstGeoTransform[5]) + 0.5);
            adfExtent[1] = maxNorthing + nYSize * adfDstGeoTransform[5];
            adfExtent[3] = maxNorthing;
        }
    }

    const auto &tileMatrixList = poTMS->tileMatrixList();
    if (m_maxZoomLevel >= 0)
    {
        if (m_maxZoomLevel >= static_cast<int>(tileMatrixList.size()))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "max-zoom = %d is invalid. It must be in [0,%d] range",
                        m_maxZoomLevel,
                        static_cast<int>(tileMatrixList.size()) - 1);
            return false;
        }
    }
    else
    {
        const double dfComputedRes = adfDstGeoTransform[1];
        double dfPrevRes = 0.0;
        double dfRes = 0.0;
        constexpr double EPSILON = 1e-8;

        if (m_minZoomLevel >= 0)
            m_maxZoomLevel = m_minZoomLevel;
        else
            m_maxZoomLevel = 0;

        for (; m_maxZoomLevel < static_cast<int>(tileMatrixList.size());
             m_maxZoomLevel++)
        {
            dfRes = tileMatrixList[m_maxZoomLevel].mResX;
            if (dfComputedRes > dfRes ||
                fabs(dfComputedRes - dfRes) / dfRes <= EPSILON)
                break;
            dfPrevRes = dfRes;
        }
        if (m_maxZoomLevel >= static_cast<int>(tileMatrixList.size()))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Could not find an appropriate zoom level. Perhaps "
                        "min-zoom is too large?");
            return false;
        }

        if (m_maxZoomLevel > 0 && fabs(dfComputedRes - dfRes) / dfRes > EPSILON)
        {
            // Round to closest resolution
            if (dfPrevRes / dfComputedRes < dfComputedRes / dfRes)
                m_maxZoomLevel--;
        }
    }
    if (m_minZoomLevel < 0)
        m_minZoomLevel = m_maxZoomLevel;

    auto tileMatrix = tileMatrixList[m_maxZoomLevel];
    int nMinTileX = 0;
    int nMinTileY = 0;
    int nMaxTileX = 0;
    int nMaxTileY = 0;
    bool bIntersects = false;
    if (!GetTileIndices(tileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                        nMinTileX, nMinTileY, nMaxTileX, nMaxTileY,
                        m_noIntersectionIsOK, bIntersects,
                        /* checkRasterOverflow = */ false))
    {
        return false;
    }
    if (!bIntersects)
        return true;

    // Potentially restrict tiling to user specified coordinates
    if (m_minTileX >= tileMatrix.mMatrixWidth)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'min-x' value must be in [0,%d] range",
                    tileMatrix.mMatrixWidth - 1);
        return false;
    }
    if (m_maxTileX >= tileMatrix.mMatrixWidth)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'max-x' value must be in [0,%d] range",
                    tileMatrix.mMatrixWidth - 1);
        return false;
    }
    if (m_minTileY >= tileMatrix.mMatrixHeight)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'min-y' value must be in [0,%d] range",
                    tileMatrix.mMatrixHeight - 1);
        return false;
    }
    if (m_maxTileY >= tileMatrix.mMatrixHeight)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'max-y' value must be in [0,%d] range",
                    tileMatrix.mMatrixHeight - 1);
        return false;
    }

    if ((m_minTileX >= 0 && m_minTileX > nMaxTileX) ||
        (m_minTileY >= 0 && m_minTileY > nMaxTileY) ||
        (m_maxTileX >= 0 && m_maxTileX < nMinTileX) ||
        (m_maxTileY >= 0 && m_maxTileY < nMinTileY))
    {
        ReportError(
            m_noIntersectionIsOK ? CE_Warning : CE_Failure, CPLE_AppDefined,
            "Dataset extent not intersecting specified min/max X/Y tile "
            "coordinates");
        return m_noIntersectionIsOK;
    }
    if (m_minTileX >= 0 && m_minTileX > nMinTileX)
    {
        nMinTileX = m_minTileX;
        adfExtent[0] = tileMatrix.mTopLeftX +
                       nMinTileX * tileMatrix.mResX * tileMatrix.mTileWidth;
    }
    if (m_minTileY >= 0 && m_minTileY > nMinTileY)
    {
        nMinTileY = m_minTileY;
        adfExtent[3] = tileMatrix.mTopLeftY -
                       nMinTileY * tileMatrix.mResY * tileMatrix.mTileHeight;
    }
    if (m_maxTileX >= 0 && m_maxTileX < nMaxTileX)
    {
        nMaxTileX = m_maxTileX;
        adfExtent[2] = tileMatrix.mTopLeftX + (nMaxTileX + 1) *
                                                  tileMatrix.mResX *
                                                  tileMatrix.mTileWidth;
    }
    if (m_maxTileY >= 0 && m_maxTileY < nMaxTileY)
    {
        nMaxTileY = m_maxTileY;
        adfExtent[1] = tileMatrix.mTopLeftY - (nMaxTileY + 1) *
                                                  tileMatrix.mResY *
                                                  tileMatrix.mTileHeight;
    }

    if (nMaxTileX - nMinTileX + 1 > INT_MAX / tileMatrix.mTileWidth ||
        nMaxTileY - nMinTileY + 1 > INT_MAX / tileMatrix.mTileHeight)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Too large zoom level");
        return false;
    }

    adfDstGeoTransform[0] = tileMatrix.mTopLeftX + nMinTileX *
                                                       tileMatrix.mResX *
                                                       tileMatrix.mTileWidth;
    adfDstGeoTransform[1] = tileMatrix.mResX;
    adfDstGeoTransform[2] = 0;
    adfDstGeoTransform[3] = tileMatrix.mTopLeftY - nMinTileY *
                                                       tileMatrix.mResY *
                                                       tileMatrix.mTileHeight;
    adfDstGeoTransform[4] = 0;
    adfDstGeoTransform[5] = -tileMatrix.mResY;

    /* -------------------------------------------------------------------- */
    /*      Setup warp options.                                             */
    /* -------------------------------------------------------------------- */
    std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)> psWO(
        GDALCreateWarpOptions(), GDALDestroyWarpOptions);

    psWO->papszWarpOptions = CSLSetNameValue(nullptr, "OPTIMIZE_SIZE", "YES");
    psWO->papszWarpOptions =
        CSLSetNameValue(psWO->papszWarpOptions, "SAMPLE_GRID", "YES");
    psWO->papszWarpOptions =
        CSLMerge(psWO->papszWarpOptions, aosWarpOptions.List());

    int bHasSrcNoData = false;
    const double dfSrcNoDataValue =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasSrcNoData);

    const bool bLastSrcBandIsAlpha =
        (poSrcDS->GetRasterCount() > 1 &&
         poSrcDS->GetRasterBand(poSrcDS->GetRasterCount())
                 ->GetColorInterpretation() == GCI_AlphaBand);

    const bool bOutputSupportsAlpha = !EQUAL(m_outputFormat.c_str(), "JPEG");
    const bool bOutputSupportsNoData = EQUAL(m_outputFormat.c_str(), "GTiff");
    const bool bDstNoDataSpecified = GetArg("dst-nodata")->IsExplicitlySet();
    const GDALColorTable *poColorTable =
        poSrcDS->GetRasterBand(1)->GetColorTable();

    const bool bUserAskedForAlpha = m_addalpha;
    if (!m_noalpha && !m_addalpha)
    {
        m_addalpha = !(bHasSrcNoData && bOutputSupportsNoData) &&
                     !bDstNoDataSpecified && poColorTable == nullptr;
    }
    m_addalpha &= bOutputSupportsAlpha;

    psWO->nBandCount = poSrcDS->GetRasterCount();
    if (bLastSrcBandIsAlpha)
    {
        --psWO->nBandCount;
        psWO->nSrcAlphaBand = poSrcDS->GetRasterCount();
    }

    if (bHasSrcNoData)
    {
        psWO->padfSrcNoDataReal =
            static_cast<double *>(CPLCalloc(psWO->nBandCount, sizeof(double)));
        for (int i = 0; i < psWO->nBandCount; ++i)
        {
            psWO->padfSrcNoDataReal[i] = dfSrcNoDataValue;
        }
    }

    if ((bHasSrcNoData && !m_addalpha && bOutputSupportsNoData) ||
        bDstNoDataSpecified)
    {
        psWO->padfDstNoDataReal =
            static_cast<double *>(CPLCalloc(psWO->nBandCount, sizeof(double)));
        for (int i = 0; i < psWO->nBandCount; ++i)
        {
            psWO->padfDstNoDataReal[i] =
                bDstNoDataSpecified ? m_dstNoData : dfSrcNoDataValue;
        }
    }

    psWO->eWorkingDataType = eSrcDT;

    GDALGetWarpResampleAlg(m_resampling.c_str(), psWO->eResampleAlg);

    /* -------------------------------------------------------------------- */
    /*      Setup band mapping.                                             */
    /* -------------------------------------------------------------------- */

    psWO->panSrcBands =
        static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));
    psWO->panDstBands =
        static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));

    for (int i = 0; i < psWO->nBandCount; i++)
    {
        psWO->panSrcBands[i] = i + 1;
        psWO->panDstBands[i] = i + 1;
    }

    if (m_addalpha)
        psWO->nDstAlphaBand = psWO->nBandCount + 1;

    const int nDstBands =
        psWO->nDstAlphaBand ? psWO->nDstAlphaBand : psWO->nBandCount;

    std::vector<GByte> dstBuffer;
    const uint64_t dstBufferSize =
        static_cast<uint64_t>(tileMatrix.mTileWidth) * tileMatrix.mTileHeight *
        nDstBands * GDALGetDataTypeSizeBytes(psWO->eWorkingDataType);
    const uint64_t nUsableRAM =
        std::min<uint64_t>(INT_MAX, CPLGetUsablePhysicalRAM() / 4);
    if (dstBufferSize <=
        (nUsableRAM ? nUsableRAM : static_cast<uint64_t>(INT_MAX)))
    {
        try
        {
            dstBuffer.resize(static_cast<size_t>(dstBufferSize));
        }
        catch (const std::exception &)
        {
        }
    }
    if (dstBuffer.size() < dstBufferSize)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Tile size and/or number of bands too large compared to "
                    "available RAM");
        return false;
    }

    FakeMaxZoomDataset oFakeMaxZoomDS(
        (nMaxTileX - nMinTileX + 1) * tileMatrix.mTileWidth,
        (nMaxTileY - nMinTileY + 1) * tileMatrix.mTileHeight, nDstBands,
        tileMatrix.mTileWidth, tileMatrix.mTileHeight, psWO->eWorkingDataType,
        adfDstGeoTransform.data(), oSRS_TMS, dstBuffer);
    CPL_IGNORE_RET_VAL(oFakeMaxZoomDS.GetSpatialRef());

    psWO->hSrcDS = GDALDataset::ToHandle(poSrcDS);
    psWO->hDstDS = GDALDataset::ToHandle(&oFakeMaxZoomDS);

    std::unique_ptr<GDALDataset> tmpSrcDS;
    if (m_tilingScheme == "raster" && !bHasNorthUpSrcGT)
    {
        CPLStringList aosOptions;
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");
        aosOptions.AddString("-a_ullr");
        aosOptions.AddString(CPLSPrintf("%.17g", adfSrcGeoTransformModif[0]));
        aosOptions.AddString(CPLSPrintf("%.17g", adfSrcGeoTransformModif[3]));
        aosOptions.AddString(
            CPLSPrintf("%.17g", adfSrcGeoTransformModif[0] +
                                    nSrcWidth * adfSrcGeoTransformModif[1]));
        aosOptions.AddString(
            CPLSPrintf("%.17g", adfSrcGeoTransformModif[3] +
                                    nSrcHeight * adfSrcGeoTransformModif[5]));
        if (oSRS_TMS.IsEmpty())
        {
            aosOptions.AddString("-a_srs");
            aosOptions.AddString("none");
        }

        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);

        tmpSrcDS.reset(GDALDataset::FromHandle(GDALTranslate(
            "", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
        GDALTranslateOptionsFree(psOptions);
        if (!tmpSrcDS)
            return false;
    }
    hTransformArg.reset(GDALCreateGenImgProjTransformer2(
        tmpSrcDS ? tmpSrcDS.get() : poSrcDS, &oFakeMaxZoomDS, aosTO.List()));
    CPLAssert(hTransformArg);

    /* -------------------------------------------------------------------- */
    /*      Warp the transformer with a linear approximator                 */
    /* -------------------------------------------------------------------- */
    hTransformArg.reset(GDALCreateApproxTransformer(
        GDALGenImgProjTransform, hTransformArg.release(), 0.125));
    GDALApproxTransformerOwnsSubtransformer(hTransformArg.get(), TRUE);

    psWO->pfnTransformer = GDALApproxTransform;
    psWO->pTransformerArg = hTransformArg.get();

    /* -------------------------------------------------------------------- */
    /*      Determine total number of tiles                                 */
    /* -------------------------------------------------------------------- */
    uint64_t nTotalTiles = static_cast<uint64_t>(nMaxTileY - nMinTileY + 1) *
                           (nMaxTileX - nMinTileX + 1);
    const uint64_t nBaseTiles = nTotalTiles;
    std::atomic<uint64_t> nCurTile = 0;
    bool bRet = true;

    for (int iZ = m_maxZoomLevel - 1;
         bRet && bIntersects && iZ >= m_minZoomLevel; --iZ)
    {
        auto ovrTileMatrix = tileMatrixList[iZ];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        bRet =
            GetTileIndices(ovrTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                           nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX,
                           nOvrMaxTileY, m_noIntersectionIsOK, bIntersects);
        if (bIntersects)
        {
            nTotalTiles +=
                static_cast<uint64_t>(nOvrMaxTileY - nOvrMinTileY + 1) *
                (nOvrMaxTileX - nOvrMinTileX + 1);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Generate tiles at max zoom level                                */
    /* -------------------------------------------------------------------- */
    GDALWarpOperation oWO;

    bRet = oWO.Initialize(psWO.get()) == CE_None && bRet;

    const auto GetUpdatedCreationOptions =
        [this](const gdal::TileMatrixSet::TileMatrix &oTM)
    {
        CPLStringList aosCreationOptions(m_creationOptions);
        if (m_outputFormat == "GTiff")
        {
            if (aosCreationOptions.FetchNameValue("TILED") == nullptr &&
                aosCreationOptions.FetchNameValue("BLOCKYSIZE") == nullptr)
            {
                if (oTM.mTileWidth <= 512 && oTM.mTileHeight <= 512)
                {
                    aosCreationOptions.SetNameValue(
                        "BLOCKYSIZE", CPLSPrintf("%d", oTM.mTileHeight));
                }
                else
                {
                    aosCreationOptions.SetNameValue("TILED", "YES");
                }
            }
            if (aosCreationOptions.FetchNameValue("COMPRESS") == nullptr)
                aosCreationOptions.SetNameValue("COMPRESS", "LZW");
        }
        else if (m_outputFormat == "COG")
        {
            if (aosCreationOptions.FetchNameValue("OVERVIEW_RESAMPLING") ==
                nullptr)
            {
                aosCreationOptions.SetNameValue("OVERVIEW_RESAMPLING",
                                                m_overviewResampling.c_str());
            }
            if (aosCreationOptions.FetchNameValue("BLOCKSIZE") == nullptr &&
                oTM.mTileWidth <= 512 && oTM.mTileWidth == oTM.mTileHeight)
            {
                aosCreationOptions.SetNameValue(
                    "BLOCKSIZE", CPLSPrintf("%d", oTM.mTileWidth));
            }
        }
        return aosCreationOptions;
    };

    VSIMkdir(m_outputDirectory.c_str(), 0755);
    VSIStatBufL sStat;
    if (VSIStatL(m_outputDirectory.c_str(), &sStat) != 0 ||
        !VSI_ISDIR(sStat.st_mode))
    {
        ReportError(CE_Failure, CPLE_FileIO,
                    "Cannot create output directory %s",
                    m_outputDirectory.c_str());
        return false;
    }

    OGRSpatialReference oWGS84;
    oWGS84.importFromEPSG(4326);
    oWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    std::unique_ptr<OGRCoordinateTransformation> poCTToWGS84;
    if (!oSRS_TMS.IsEmpty())
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        poCTToWGS84.reset(
            OGRCreateCoordinateTransformation(&oSRS_TMS, &oWGS84));
    }

    const bool kmlCompatible = m_kml &&
                               [this, &poTMS, &poCTToWGS84, bInvertAxisTMS]()
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        double dfX = poTMS->tileMatrixList()[0].mTopLeftX;
        double dfY = poTMS->tileMatrixList()[0].mTopLeftY;
        if (bInvertAxisTMS)
            std::swap(dfX, dfY);
        return (m_minZoomLevel == m_maxZoomLevel ||
                (poTMS->haveAllLevelsSameTopLeft() &&
                 poTMS->haveAllLevelsSameTileSize() &&
                 poTMS->hasOnlyPowerOfTwoVaryingScales())) &&
               poCTToWGS84 && poCTToWGS84->Transform(1, &dfX, &dfY);
    }();
    const int kmlTileSize =
        m_tileSize > 0 ? m_tileSize : poTMS->tileMatrixList()[0].mTileWidth;
    if (m_kml && !kmlCompatible)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Tiling scheme not compatible with KML output");
        return false;
    }

    if (m_title.empty())
        m_title = CPLGetFilename(m_dataset.GetName().c_str());

    if (!m_url.empty())
    {
        if (m_url.back() != '/')
            m_url += '/';
        std::string out_path = m_outputDirectory;
        if (m_outputDirectory.back() == '/')
            out_path.pop_back();
        m_url += CPLGetFilename(out_path.c_str());
    }

    CPLWorkerThreadPool oThreadPool;

    {
        PerThreadMaxZoomResourceManager oResourceManager(
            poSrcDS, psWO.get(), hTransformArg.get(), oFakeMaxZoomDS,
            dstBuffer.size());

        const CPLStringList aosCreationOptions(
            GetUpdatedCreationOptions(tileMatrix));

        CPLDebug("gdal_raster_tile",
                 "Generating tiles z=%d, y=%d...%d, x=%d...%d", m_maxZoomLevel,
                 nMinTileY, nMaxTileY, nMinTileX, nMaxTileX);

        if (static_cast<uint64_t>(m_numThreads) > nBaseTiles)
            m_numThreads = static_cast<int>(nBaseTiles);

        if (bRet && m_numThreads > 1)
        {
            CPLDebug("gdal_raster_tile", "Using %d threads", m_numThreads);
            bRet = oThreadPool.Setup(m_numThreads, nullptr, nullptr);
        }

        std::atomic<bool> bFailure = false;
        std::atomic<int> nQueuedJobs = 0;

        for (int iY = nMinTileY; bRet && iY <= nMaxTileY; ++iY)
        {
            for (int iX = nMinTileX; bRet && iX <= nMaxTileX; ++iX)
            {
                if (m_numThreads > 1)
                {
                    auto job = [this, &oResourceManager, &bFailure, &nCurTile,
                                &nQueuedJobs, poDstDriver, pszExtension,
                                &aosCreationOptions, &psWO, &tileMatrix,
                                nDstBands, iX, iY, nMinTileX, nMinTileY,
                                poColorTable, bUserAskedForAlpha]()
                    {
                        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);

                        --nQueuedJobs;
                        auto resources = oResourceManager.AcquireResources();
                        if (resources &&
                            GenerateTile(
                                resources->poSrcDS.get(), poDstDriver,
                                pszExtension, aosCreationOptions.List(),
                                *(resources->poWO.get()),
                                *(resources->poFakeMaxZoomDS->GetSpatialRef()),
                                psWO->eWorkingDataType, tileMatrix,
                                m_outputDirectory, nDstBands,
                                psWO->padfDstNoDataReal
                                    ? &(psWO->padfDstNoDataReal[0])
                                    : nullptr,
                                m_maxZoomLevel, iX, iY, m_convention, nMinTileX,
                                nMinTileY, m_skipBlank, bUserAskedForAlpha,
                                m_auxXML, m_resume, m_metadata, poColorTable,
                                resources->dstBuffer))
                        {
                            oResourceManager.ReleaseResources(
                                std::move(resources));
                        }
                        else
                        {
                            oResourceManager.SetError();
                            bFailure = true;
                        }
                        ++nCurTile;
                    };

                    // Avoid queueing too many jobs at once
                    while (bRet && nQueuedJobs / 10 > m_numThreads)
                    {
                        oThreadPool.WaitEvent();

                        bRet &=
                            !bFailure &&
                            (!pfnProgress ||
                             pfnProgress(static_cast<double>(nCurTile) /
                                             static_cast<double>(nTotalTiles),
                                         "", pProgressData));
                    }

                    ++nQueuedJobs;
                    oThreadPool.SubmitJob(std::move(job));
                }
                else
                {
                    bRet = GenerateTile(
                        poSrcDS, poDstDriver, pszExtension,
                        aosCreationOptions.List(), oWO, oSRS_TMS,
                        psWO->eWorkingDataType, tileMatrix, m_outputDirectory,
                        nDstBands,
                        psWO->padfDstNoDataReal ? &(psWO->padfDstNoDataReal[0])
                                                : nullptr,
                        m_maxZoomLevel, iX, iY, m_convention, nMinTileX,
                        nMinTileY, m_skipBlank, bUserAskedForAlpha, m_auxXML,
                        m_resume, m_metadata, poColorTable, dstBuffer);

                    ++nCurTile;
                    bRet &= (!pfnProgress ||
                             pfnProgress(static_cast<double>(nCurTile) /
                                             static_cast<double>(nTotalTiles),
                                         "", pProgressData));
                }
            }
        }

        if (m_numThreads > 1)
        {
            // Wait for completion of all jobs
            while (bRet && nQueuedJobs > 0)
            {
                oThreadPool.WaitEvent();
                bRet &= !bFailure &&
                        (!pfnProgress ||
                         pfnProgress(static_cast<double>(nCurTile) /
                                         static_cast<double>(nTotalTiles),
                                     "", pProgressData));
            }
            oThreadPool.WaitCompletion();
            bRet &=
                !bFailure && (!pfnProgress ||
                              pfnProgress(static_cast<double>(nCurTile) /
                                              static_cast<double>(nTotalTiles),
                                          "", pProgressData));

            if (!oResourceManager.GetErrorMsg().empty())
            {
                // Re-emit error message from worker thread to main thread
                ReportError(CE_Failure, CPLE_AppDefined, "%s",
                            oResourceManager.GetErrorMsg().c_str());
            }
        }

        if (m_kml && bRet)
        {
            for (int iY = nMinTileY; iY <= nMaxTileY; ++iY)
            {
                for (int iX = nMinTileX; iX <= nMaxTileX; ++iX)
                {
                    const int nFileY =
                        GetFileY(iY, poTMS->tileMatrixList()[m_maxZoomLevel],
                                 m_convention);
                    std::string osFilename = CPLFormFilenameSafe(
                        m_outputDirectory.c_str(),
                        CPLSPrintf("%d", m_maxZoomLevel), nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(), CPLSPrintf("%d", iX), nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(),
                        CPLSPrintf("%d.%s", nFileY, pszExtension), nullptr);
                    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                    {
                        GenerateKML(m_outputDirectory, m_title, iX, iY,
                                    m_maxZoomLevel, kmlTileSize, pszExtension,
                                    m_url, poTMS.get(), bInvertAxisTMS,
                                    m_convention, poCTToWGS84.get(), {});
                    }
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Generate tiles at lower zoom levels                             */
    /* -------------------------------------------------------------------- */
    for (int iZ = m_maxZoomLevel - 1; bRet && iZ >= m_minZoomLevel; --iZ)
    {
        auto srcTileMatrix = tileMatrixList[iZ + 1];
        int nSrcMinTileX = 0;
        int nSrcMinTileY = 0;
        int nSrcMaxTileX = 0;
        int nSrcMaxTileY = 0;

        CPL_IGNORE_RET_VAL(
            GetTileIndices(srcTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                           nSrcMinTileX, nSrcMinTileY, nSrcMaxTileX,
                           nSrcMaxTileY, m_noIntersectionIsOK, bIntersects));

        MosaicDataset oSrcDS(
            CPLFormFilenameSafe(m_outputDirectory.c_str(),
                                CPLSPrintf("%d", iZ + 1), nullptr),
            pszExtension, m_outputFormat, poSrcDS, srcTileMatrix, oSRS_TMS,
            nSrcMinTileX, nSrcMinTileY, nSrcMaxTileX, nSrcMaxTileY,
            m_convention, nDstBands, psWO->eWorkingDataType,
            psWO->padfDstNoDataReal ? &(psWO->padfDstNoDataReal[0]) : nullptr,
            m_metadata, poColorTable);

        auto ovrTileMatrix = tileMatrixList[iZ];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        CPL_IGNORE_RET_VAL(
            GetTileIndices(ovrTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                           nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX,
                           nOvrMaxTileY, m_noIntersectionIsOK, bIntersects));
        bRet = bIntersects;

        if (bRet)
        {
            CPLDebug("gdal_raster_tile",
                     "Generating overview tiles z=%d, y=%d...%d, x=%d...%d", iZ,
                     nOvrMinTileY, nOvrMaxTileY, nOvrMinTileX, nOvrMaxTileX);
        }

        const CPLStringList aosCreationOptions(
            GetUpdatedCreationOptions(ovrTileMatrix));

        PerThreadLowerZoomResourceManager oResourceManager(oSrcDS);
        std::atomic<bool> bFailure = false;
        std::atomic<int> nQueuedJobs = 0;

        const bool bUseThreads =
            m_numThreads > 1 &&
            (nOvrMaxTileY > nOvrMinTileY || nOvrMaxTileX > nOvrMinTileX);

        for (int iY = nOvrMinTileY; bRet && iY <= nOvrMaxTileY; ++iY)
        {
            for (int iX = nOvrMinTileX; bRet && iX <= nOvrMaxTileX; ++iX)
            {
                if (bUseThreads)
                {
                    auto job = [this, &oResourceManager, poDstDriver, &bFailure,
                                &nCurTile, &nQueuedJobs, pszExtension,
                                &aosCreationOptions, &aosWarpOptions,
                                &ovrTileMatrix, iZ, iX, iY,
                                bUserAskedForAlpha]()
                    {
                        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);

                        --nQueuedJobs;
                        auto resources = oResourceManager.AcquireResources();
                        if (resources &&
                            GenerateOverviewTile(
                                *(resources->poSrcDS.get()), poDstDriver,
                                m_outputFormat, pszExtension,
                                aosCreationOptions.List(),
                                aosWarpOptions.List(), m_overviewResampling,
                                ovrTileMatrix, m_outputDirectory, iZ, iX, iY,
                                m_convention, m_skipBlank, bUserAskedForAlpha,
                                m_auxXML, m_resume))
                        {
                            oResourceManager.ReleaseResources(
                                std::move(resources));
                        }
                        else
                        {
                            oResourceManager.SetError();
                            bFailure = true;
                        }
                        ++nCurTile;
                    };

                    // Avoid queueing too many jobs at once
                    while (bRet && nQueuedJobs / 10 > m_numThreads)
                    {
                        oThreadPool.WaitEvent();

                        bRet &=
                            !bFailure &&
                            (!pfnProgress ||
                             pfnProgress(static_cast<double>(nCurTile) /
                                             static_cast<double>(nTotalTiles),
                                         "", pProgressData));
                    }

                    ++nQueuedJobs;
                    oThreadPool.SubmitJob(std::move(job));
                }
                else
                {
                    bRet = GenerateOverviewTile(
                        oSrcDS, poDstDriver, m_outputFormat, pszExtension,
                        aosCreationOptions.List(), aosWarpOptions.List(),
                        m_overviewResampling, ovrTileMatrix, m_outputDirectory,
                        iZ, iX, iY, m_convention, m_skipBlank,
                        bUserAskedForAlpha, m_auxXML, m_resume);

                    ++nCurTile;
                    bRet &= (!pfnProgress ||
                             pfnProgress(static_cast<double>(nCurTile) /
                                             static_cast<double>(nTotalTiles),
                                         "", pProgressData));
                }
            }
        }

        if (bUseThreads)
        {
            // Wait for completion of all jobs
            while (bRet && nQueuedJobs > 0)
            {
                oThreadPool.WaitEvent();
                bRet &= !bFailure &&
                        (!pfnProgress ||
                         pfnProgress(static_cast<double>(nCurTile) /
                                         static_cast<double>(nTotalTiles),
                                     "", pProgressData));
            }
            oThreadPool.WaitCompletion();
            bRet &=
                !bFailure && (!pfnProgress ||
                              pfnProgress(static_cast<double>(nCurTile) /
                                              static_cast<double>(nTotalTiles),
                                          "", pProgressData));

            if (!oResourceManager.GetErrorMsg().empty())
            {
                // Re-emit error message from worker thread to main thread
                ReportError(CE_Failure, CPLE_AppDefined, "%s",
                            oResourceManager.GetErrorMsg().c_str());
            }
        }

        if (m_kml && bRet)
        {
            for (int iY = nOvrMinTileY; bRet && iY <= nOvrMaxTileY; ++iY)
            {
                for (int iX = nOvrMinTileX; bRet && iX <= nOvrMaxTileX; ++iX)
                {
                    int nFileY =
                        GetFileY(iY, poTMS->tileMatrixList()[iZ], m_convention);
                    std::string osFilename =
                        CPLFormFilenameSafe(m_outputDirectory.c_str(),
                                            CPLSPrintf("%d", iZ), nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(), CPLSPrintf("%d", iX), nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(),
                        CPLSPrintf("%d.%s", nFileY, pszExtension), nullptr);
                    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                    {
                        std::vector<TileCoordinates> children;

                        for (int iChildY = 0; iChildY <= 1; ++iChildY)
                        {
                            for (int iChildX = 0; iChildX <= 1; ++iChildX)
                            {
                                nFileY =
                                    GetFileY(iY * 2 + iChildY,
                                             poTMS->tileMatrixList()[iZ + 1],
                                             m_convention);
                                osFilename = CPLFormFilenameSafe(
                                    m_outputDirectory.c_str(),
                                    CPLSPrintf("%d", iZ + 1), nullptr);
                                osFilename = CPLFormFilenameSafe(
                                    osFilename.c_str(),
                                    CPLSPrintf("%d", iX * 2 + iChildX),
                                    nullptr);
                                osFilename = CPLFormFilenameSafe(
                                    osFilename.c_str(),
                                    CPLSPrintf("%d.%s", nFileY, pszExtension),
                                    nullptr);
                                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                                {
                                    TileCoordinates tc;
                                    tc.nTileX = iX * 2 + iChildX;
                                    tc.nTileY = iY * 2 + iChildY;
                                    tc.nTileZ = iZ + 1;
                                    children.push_back(std::move(tc));
                                }
                            }
                        }

                        GenerateKML(m_outputDirectory, m_title, iX, iY, iZ,
                                    kmlTileSize, pszExtension, m_url,
                                    poTMS.get(), bInvertAxisTMS, m_convention,
                                    poCTToWGS84.get(), children);
                    }
                }
            }
        }
    }

    const auto IsWebViewerEnabled = [this](const char *name)
    {
        return std::find_if(m_webviewers.begin(), m_webviewers.end(),
                            [name](const std::string &s) {
                                return s == "all" || s == name;
                            }) != m_webviewers.end();
    };

    if (bRet && poTMS->identifier() == "GoogleMapsCompatible" &&
        IsWebViewerEnabled("leaflet"))
    {
        double dfSouthLat = -90;
        double dfWestLon = -180;
        double dfNorthLat = 90;
        double dfEastLon = 180;

        if (poCTToWGS84)
        {
            poCTToWGS84->TransformBounds(
                adfExtent[0], adfExtent[1], adfExtent[2], adfExtent[3],
                &dfWestLon, &dfSouthLat, &dfEastLon, &dfNorthLat, 21);
        }

        GenerateLeaflet(m_outputDirectory, m_title, dfSouthLat, dfWestLon,
                        dfNorthLat, dfEastLon, m_minZoomLevel, m_maxZoomLevel,
                        tileMatrix.mTileWidth, pszExtension, m_url, m_copyright,
                        m_convention == "xyz");
    }

    if (bRet && IsWebViewerEnabled("openlayers"))
    {
        GenerateOpenLayers(
            m_outputDirectory, m_title, adfExtent[0], adfExtent[1],
            adfExtent[2], adfExtent[3], m_minZoomLevel, m_maxZoomLevel,
            tileMatrix.mTileWidth, pszExtension, m_url, m_copyright,
            *(poTMS.get()), bInvertAxisTMS, oSRS_TMS, m_convention == "xyz");
    }

    if (bRet && IsWebViewerEnabled("mapml") &&
        poTMS->identifier() != "raster" && m_convention == "xyz")
    {
        GenerateMapML(m_outputDirectory, m_mapmlTemplate, m_title, nMinTileX,
                      nMinTileY, nMaxTileX, nMaxTileY, m_minZoomLevel,
                      m_maxZoomLevel, pszExtension, m_url, m_copyright,
                      *(poTMS.get()));
    }

    if (bRet && m_kml)
    {
        std::vector<TileCoordinates> children;

        auto ovrTileMatrix = tileMatrixList[m_minZoomLevel];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        CPL_IGNORE_RET_VAL(
            GetTileIndices(ovrTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                           nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX,
                           nOvrMaxTileY, m_noIntersectionIsOK, bIntersects));

        for (int iY = nOvrMinTileY; bRet && iY <= nOvrMaxTileY; ++iY)
        {
            for (int iX = nOvrMinTileX; bRet && iX <= nOvrMaxTileX; ++iX)
            {
                int nFileY = GetFileY(
                    iY, poTMS->tileMatrixList()[m_minZoomLevel], m_convention);
                std::string osFilename = CPLFormFilenameSafe(
                    m_outputDirectory.c_str(), CPLSPrintf("%d", m_minZoomLevel),
                    nullptr);
                osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                                 CPLSPrintf("%d", iX), nullptr);
                osFilename = CPLFormFilenameSafe(
                    osFilename.c_str(),
                    CPLSPrintf("%d.%s", nFileY, pszExtension), nullptr);
                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                {
                    TileCoordinates tc;
                    tc.nTileX = iX;
                    tc.nTileY = iY;
                    tc.nTileZ = m_minZoomLevel;
                    children.push_back(std::move(tc));
                }
            }
        }
        GenerateKML(m_outputDirectory, m_title, -1, -1, -1, kmlTileSize,
                    pszExtension, m_url, poTMS.get(), bInvertAxisTMS,
                    m_convention, poCTToWGS84.get(), children);
    }

    return bRet;
}

//! @endcond

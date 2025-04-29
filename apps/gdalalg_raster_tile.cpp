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
#include "gdal_priv.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "ogr_spatialref.h"
#include "tilematrixset.hpp"

#include <algorithm>
#include <cmath>

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

    std::vector<std::string> tilingSchemes;
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

    AddArg("resampling", 'r', _("Resampling method"), &m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum")
        .SetDefault("cubic")
        .SetHiddenChoices("near");

    AddArg("convention", 0,
           _("Tile numbering convention: xyz (from top) or tms (from bottom)"),
           &m_convention)
        .SetDefault(m_convention)
        .SetChoices("xyz", "tms");
    AddArg("tilesize", 0, _("Override default tile size"), &m_tileSize)
        .SetMinValueIncluded(64)
        .SetMaxValueIncluded(32768);
    AddArg("addalpha", 0, _("Whether to force adding an alpha channel"),
           &m_addalpha)
        .SetMutualExclusionGroup("alpha");
    AddArg("no-alpha", 0, _("Whether to disable adding an alpha channel"),
           &m_noalpha)
        .SetMutualExclusionGroup("alpha");
    AddArg("dstnodata", 0, _("Destination nodata value"), &m_dstNoData);
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
    AddArg("resume", 0, _("Generate only missing files"), &m_resume);

    constexpr const char *PUBLICATION_CATEGORY = "Publication";
    AddArg("webviewer", 0, _("Web viewer to generate"), &m_webviewers)
        .SetDefault("all")
        .SetChoices("none", "all", "leaflet", "openlayers")
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("url", 0,
           _("URL address where the generated tiles are going to be published"),
           &m_url)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("title", 0, _("Title of the map"), &m_title)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("copyright", 0, _("Copyright for the map"), &m_copyright)
        .SetCategory(PUBLICATION_CATEGORY);

    AddValidationAction(
        [this]()
        {
            if (m_minZoomLevel >= 0 && m_maxZoomLevel >= 0 &&
                m_minZoomLevel > m_maxZoomLevel)
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "'min-zoom' should be lesser or equal to 'max-zoom'");
                return false;
            }

            if (m_addalpha && GetArg("dstnodata")->IsExplicitlySet())
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "'addalpha' and 'dstnodata' are mutually exclusive");
                return false;
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
                           int &nMinTileY, int &nMaxTileX, int &nMaxTileY)
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

    if (!((dfMinTileX <= tileMatrix.mMatrixWidth && dfMaxTileX >= 0 &&
           dfMinTileY <= tileMatrix.mMatrixHeight && dfMaxTileY >= 0)))
    {
        CPLDebug("gdal_raster_tile",
                 "dfMinTileX=%g dfMinTileY=%g dfMaxTileX=%g dfMaxTileY=%g",
                 dfMinTileX, dfMinTileY, dfMaxTileX, dfMaxTileY);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Extent of source dataset is not compatible with extent of "
                 "tiling scheme");
        return false;
    }
    if (nMaxTileX - nMinTileX + 1 > INT_MAX / tileMatrix.mTileWidth ||
        nMaxTileY - nMinTileY + 1 > INT_MAX / tileMatrix.mTileHeight)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too large zoom level");
        return false;
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

static bool GenerateTile(GDALDataset *poSrcDS, GDALDriver *poMemDriver,
                         GDALDriver *poDstDriver, const char *pszExtension,
                         CSLConstList creationOptions, GDALWarpOperation &oWO,
                         const OGRSpatialReference &oSRS_TMS,
                         GDALDataType eWorkingDataType,
                         const gdal::TileMatrixSet::TileMatrix &tileMatrix,
                         const std::string &outputDirectory, int nBands,
                         const double *pdfDstNoData, int nZoomLevel, int iX,
                         int iY, const std::string &convention, int nMinTileX,
                         int nMinTileY, bool bSkipBlank, bool bAuxXML,
                         bool bResume, const std::vector<std::string> &metadata,
                         const GDALColorTable *poColorTable,
                         std::vector<GByte> &dstBuffer)
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

    VSIMkdir(osDirZ.c_str(), 0755);
    VSIMkdir(osDirX.c_str(), 0755);

    std::unique_ptr<GDALDataset> memDS(
        poMemDriver->Create("", tileMatrix.mTileWidth, tileMatrix.mTileHeight,
                            0, eWorkingDataType, nullptr));
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
GenerateOverviewTile(GDALDataset &oSrcDS, const std::string &outputFormat,
                     const char *pszExtension, CSLConstList creationOptions,
                     const std::string &resampling,
                     const gdal::TileMatrixSet::TileMatrix &tileMatrix,
                     const std::string &outputDirectory, int nZoomLevel, int iX,
                     int iY, const std::string &convention, bool bSkipBlank,
                     bool bAuxXML, bool bResume)
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

    CPLStringList aosOptions;

    aosOptions.AddString("-q");

    aosOptions.AddString("-of");
    aosOptions.AddString(outputFormat.c_str());

    for (const char *pszCO : cpl::Iterate(creationOptions))
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(pszCO);
    }

    aosOptions.AddString("-projwin");
    const double dfMinX =
        tileMatrix.mTopLeftX + iX * tileMatrix.mResX * tileMatrix.mTileWidth;
    aosOptions.AddString(CPLSPrintf("%.17g", dfMinX));
    const double dfMaxY =
        tileMatrix.mTopLeftY - iY * tileMatrix.mResY * tileMatrix.mTileHeight;
    aosOptions.AddString(CPLSPrintf("%.17g", dfMaxY));
    const double dfMaxX = dfMinX + tileMatrix.mResX * tileMatrix.mTileWidth;
    aosOptions.AddString(CPLSPrintf("%.17g", dfMaxX));
    const double dfMinY = dfMaxY - tileMatrix.mResY * tileMatrix.mTileHeight;
    aosOptions.AddString(CPLSPrintf("%.17g", dfMinY));

    aosOptions.AddString("-outsize");
    aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileWidth));
    aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileHeight));

    aosOptions.AddString("-r");
    aosOptions.AddString(resampling == "nearest" || resampling == "average" ||
                                 resampling == "bilinear" ||
                                 resampling == "cubic" ||
                                 resampling == "cubicspline" ||
                                 resampling == "lanczos" || resampling == "mode"
                             ? resampling.c_str()
                             : "average");

    VSIMkdir(osDirZ.c_str(), 0755);
    VSIMkdir(osDirX.c_str(), 0755);

    CPLConfigOptionSetter oSetter("GDAL_PAM_ENABLED", bAuxXML ? "YES" : "NO",
                                  false);
    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);
    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALTranslate((osFilename + ".tmp").c_str(),
                      GDALDataset::ToHandle(&oSrcDS), psOptions, nullptr)));
    GDALTranslateOptionsFree(psOptions);
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
                bRet = poOutDS->Close() == CE_None;
                poOutDS.reset();
                if (bRet)
                {
                    VSIUnlink((osFilename + ".tmp").c_str());
                    if (bAuxXML)
                        VSIUnlink((osFilename + ".tmp.aux.xml").c_str());
                    return true;
                }
            }
        }
    }
    bRet = bRet && poOutDS && poOutDS->Close() == CE_None;
    poOutDS.reset();
    if (bRet)
    {
        bRet =
            VSIRename((osFilename + ".tmp").c_str(), osFilename.c_str()) == 0;
        if (bAuxXML)
        {
            VSIRename((osFilename + ".tmp.aux.xml").c_str(),
                      (osFilename + ".aux.xml").c_str());
        }
    }
    else
    {
        VSIUnlink((osFilename + ".tmp").c_str());
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
    const OGRSpatialReference m_oSRS;
    double m_adfGT[6];

  public:
    FakeMaxZoomDataset(int nWidth, int nHeight, int nBandsIn, int nBlockXSize,
                       int nBlockYSize, GDALDataType eDT, double adfGT[6],
                       const OGRSpatialReference &oSRS,
                       std::vector<GByte> &dstBuffer)
        : m_oSRS(oSRS)
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
        return &m_oSRS;
    }

    CPLErr GetGeoTransform(double *padfGT) override
    {
        memcpy(padfGT, m_adfGT, sizeof(double) * 6);
        return CE_None;
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
                     double *pdfDstNoData, const GDALColorTable *poColorTable)
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

    const OGRSpatialReference m_oSRS;
    double m_adfGT[6];
    lru11::Cache<std::string, std::shared_ptr<GDALDataset>> m_oCacheTile{};

  public:
    MosaicDataset(const std::string &directory, const std::string &extension,
                  GDALDataset *poSrcDS,
                  const gdal::TileMatrixSet::TileMatrix &oTM,
                  const OGRSpatialReference &oSRS, int nTileMinX, int nTileMinY,
                  int nTileMaxX, int nTileMaxY, const std::string &convention,
                  int nBandsIn, GDALDataType eDT, double *pdfDstNoData,
                  const std::vector<std::string> &metadata,
                  const GDALColorTable *poCT)
        : m_oSRS(oSRS)
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
        return &m_oSRS;
    }

    CPLErr GetGeoTransform(double *padfGT) override
    {
        memcpy(padfGT, m_adfGT, sizeof(double) * 6);
        return CE_None;
    }
};

/************************************************************************/
/*                  MosaicRasterBand::IReadBlock()                     */
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
        poTileDS.reset(GDALDataset::Open(filename.c_str(), GDAL_OF_RASTER));
        poThisDS->m_oCacheTile.insert(filename, poTileDS);
    }
    if (!poTileDS || nBand > poTileDS->GetRasterCount())
    {
        memset(pData, 0,
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
        const double base_res = tms.tileMatrixList()[0].mResX;
        substs["maxres"] = CPLSPrintf("%d", nMinZoom);
        std::string resolutions = "[";
        for (int i = 0; i <= nMaxZoom; ++i)
        {
            if (i > 0)
                resolutions += ",";
            resolutions += CPLSPrintf(pszFmt, base_res / (1 << i));
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
                resolution: %(maxres)f)raw";
    }

    if (tms.identifier() == "WorldCRS84Quad")
    {
        s += R"raw(
                projection: 'EPSG:4326',)raw";
    }
    else if (tms.identifier() != "GoogleMapsCompatible")
    {
        const char *pszAuthName = oSRS_TMS.GetAuthorityName(nullptr);
        const char *pszAuthCode = oSRS_TMS.GetAuthorityCode(nullptr);
        if (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
        {
            substs["epsg_code"] = pszAuthCode;
            s += R"raw(
                projection: new ol.proj.Projection({code: 'EPSG:%(epsg_code)s', units:'m'}),)raw";
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
/*                  GDALRasterTileAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALRasterTileAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    auto poSrcDS = m_dataset.GetDatasetRef();
    CPLAssert(poSrcDS);
    if (poSrcDS->GetRasterCount() == 0 || poSrcDS->GetRasterXSize() == 0 ||
        poSrcDS->GetRasterYSize() == 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Invalid source dataset");
        return false;
    }

    auto poMemDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!poMemDriver)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find MEM driver");
        return false;
    }

    if (m_resampling == "near")
        m_resampling = "nearest";

    if (poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
            GCI_PaletteIndex &&
        m_resampling != "nearest" && m_resampling != "mode")
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

    {
        double adfSrcGeoTransform[6];
        if (poSrcDS->GetGeoTransform(adfSrcGeoTransform) != CE_None &&
            poSrcDS->GetGCPCount() == 0 &&
            poSrcDS->GetMetadata("GEOLOCATION") == nullptr &&
            poSrcDS->GetMetadata("RPC") == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Ungeoreferenced datasets are not supported");
            return false;
        }

        if (poSrcDS->GetMetadata("GEOLOCATION") == nullptr &&
            poSrcDS->GetMetadata("RPC") == nullptr &&
            poSrcDS->GetSpatialRef() == nullptr &&
            poSrcDS->GetGCPSpatialRef() == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Ungeoreferenced datasets are not supported");
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

    if (m_tilingScheme == "mercator")
        m_tilingScheme = "WebMercatorQuad";
    else if (m_tilingScheme == "geodetic")
        m_tilingScheme = "WorldCRS84Quad";

    auto poTMS = gdal::TileMatrixSet::parse(
        m_mapTileMatrixIdentifierToScheme[m_tilingScheme].c_str());
    // Enforced by SetChoices() on the m_tilingScheme argument
    CPLAssert(poTMS && !poTMS->hasVariableMatrixWidth());

    OGRSpatialReference oSRS_TMS;
    CPL_IGNORE_RET_VAL(oSRS_TMS.SetFromUserInput(poTMS->crs().c_str()));

    const char *pszAuthName = oSRS_TMS.GetAuthorityName(nullptr);
    const char *pszAuthCode = oSRS_TMS.GetAuthorityCode(nullptr);
    const int nEPSGCode =
        (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
            ? atoi(pszAuthCode)
            : 0;

    const bool bInvertAxisTMS = oSRS_TMS.EPSGTreatsAsLatLong() != FALSE ||
                                oSRS_TMS.EPSGTreatsAsNorthingEasting() != FALSE;

    oSRS_TMS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    CPLStringList aosTO;
    aosTO.SetNameValue("DST_SRS", oSRS_TMS.exportToWkt().c_str());

    std::unique_ptr<void, decltype(&GDALDestroyTransformer)> hTransformArg(
        nullptr, GDALDestroyTransformer);

    // Hack to compensate for GDALSuggestedWarpOutput2() failure (or not
    // ideal suggestion with PROJ 8) when reprojecting latitude = +/- 90 to
    // EPSG:3857.
    double adfSrcGeoTransform[6];
    std::unique_ptr<GDALDataset> poTmpDS;
    bool bEPSG3857Adjust = false;
    if (nEPSGCode == 3857 &&
        poSrcDS->GetGeoTransform(adfSrcGeoTransform) == CE_None &&
        adfSrcGeoTransform[2] == 0 && adfSrcGeoTransform[4] == 0 &&
        adfSrcGeoTransform[5] < 0)
    {
        const auto poSrcSRS = poSrcDS->GetSpatialRef();
        if (poSrcSRS && poSrcSRS->IsGeographic())
        {
            double maxLat = adfSrcGeoTransform[3];
            double minLat = adfSrcGeoTransform[3] +
                            poSrcDS->GetRasterYSize() * adfSrcGeoTransform[5];
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
                                            poSrcDS->GetRasterXSize() *
                                                adfSrcGeoTransform[1]));
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
    if (!hTransformArg)
    {
        hTransformArg.reset(
            GDALCreateGenImgProjTransformer2(poSrcDS, nullptr, aosTO.List()));
    }
    if (!hTransformArg)
    {
        return false;
    }

    double adfGeoTransform[6];
    double adfExtent[4];
    int nXSize, nYSize;

    bool bSuggestOK;
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        bSuggestOK =
            (GDALSuggestedWarpOutput2(
                 poSrcDS,
                 static_cast<GDALTransformerInfo *>(hTransformArg.get())
                     ->pfnTransform,
                 hTransformArg.get(), adfGeoTransform, &nXSize, &nYSize,
                 adfExtent, 0) == CE_None);
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
        double maxNorthing = adfGeoTransform[3];
        double minNorthing = adfGeoTransform[3] + adfGeoTransform[5] * nYSize;
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
            adfGeoTransform[3] = maxNorthing;
            nYSize =
                int((maxNorthing - minNorthing) / (-adfGeoTransform[5]) + 0.5);
            adfExtent[1] = maxNorthing + nYSize * adfGeoTransform[5];
            adfExtent[3] = maxNorthing;
        }
    }

    const auto &tileMatrixList = poTMS->tileMatrixList();
    if (m_maxZoomLevel >= 0)
    {
        if (m_maxZoomLevel >= static_cast<int>(tileMatrixList.size()))
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "max-zoom = %d is invalid. It should be in [0,%d] range",
                m_maxZoomLevel, static_cast<int>(tileMatrixList.size()) - 1);
            return false;
        }
    }
    else
    {
        const double dfComputedRes = adfGeoTransform[1];
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
    if (!GetTileIndices(tileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                        nMinTileX, nMinTileY, nMaxTileX, nMaxTileY))
    {
        return false;
    }

    double adfDstGeoTransform[6];
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

    int bHasSrcNoData = false;
    const double dfSrcNoDataValue =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasSrcNoData);

    const bool bLastSrcBandIsAlpha =
        (poSrcDS->GetRasterCount() > 1 &&
         poSrcDS->GetRasterBand(poSrcDS->GetRasterCount())
                 ->GetColorInterpretation() == GCI_AlphaBand);

    const bool bOutputSupportsAlpha = !EQUAL(m_outputFormat.c_str(), "JPEG");
    const bool bOutputSupportsNoData = EQUAL(m_outputFormat.c_str(), "GTiff");
    const bool bDstNoDataSpecified = GetArg("dstnodata")->IsExplicitlySet();
    const GDALColorTable *poColorTable =
        poSrcDS->GetRasterBand(1)->GetColorTable();

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
        adfDstGeoTransform, oSRS_TMS, dstBuffer);
    CPL_IGNORE_RET_VAL(oFakeMaxZoomDS.GetSpatialRef());

    psWO->hSrcDS = GDALDataset::ToHandle(poSrcDS);
    psWO->hDstDS = GDALDataset::ToHandle(&oFakeMaxZoomDS);

    hTransformArg.reset(GDALCreateGenImgProjTransformer2(
        poSrcDS, &oFakeMaxZoomDS, aosTO.List()));
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
    uint64_t nCurTile = 0;

    for (int iZ = m_minZoomLevel; iZ < m_maxZoomLevel; ++iZ)
    {
        auto ovrTileMatrix = tileMatrixList[iZ];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        if (!GetTileIndices(ovrTileMatrix, bInvertAxisTMS, m_tileSize,
                            adfExtent, nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX,
                            nOvrMaxTileY))
        {
            return false;
        }
        nTotalTiles += static_cast<uint64_t>(nOvrMaxTileY - nOvrMinTileY + 1) *
                       (nOvrMaxTileX - nOvrMinTileX + 1);
    }

    /* -------------------------------------------------------------------- */
    /*      Generate tiles at max zoom level                                */
    /* -------------------------------------------------------------------- */
    GDALWarpOperation oWO;

    bool bRet = oWO.Initialize(psWO.get()) == CE_None;

    const auto GetUpdatedCreationOptions =
        [this](const gdal::TileMatrixSet::TileMatrix &oTM)
    {
        CPLStringList aosCreationOptions(m_creationOptions);
        if (m_outputFormat == "GTiff")
        {
            if (aosCreationOptions.FetchNameValue("TILED") == nullptr &&
                aosCreationOptions.FetchNameValue("BLOCKYSIZE") == nullptr)
            {
                aosCreationOptions.SetNameValue(
                    "BLOCKYSIZE", CPLSPrintf("%d", oTM.mTileHeight));
            }
            if (aosCreationOptions.FetchNameValue("COMPRESS") == nullptr)
                aosCreationOptions.SetNameValue("COMPRESS", "LZW");
        }
        else if (m_outputFormat == "COG" &&
                 aosCreationOptions.FetchNameValue("OVERVIEW_RESAMPLING") ==
                     nullptr)
        {
            aosCreationOptions.SetNameValue("OVERVIEW_RESAMPLING",
                                            m_resampling.c_str());
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

    {
        const CPLStringList aosCreationOptions(
            GetUpdatedCreationOptions(tileMatrix));

        CPLDebug("gdal_raster_tile",
                 "Generating tiles z=%d, y=%d...%d, x=%d...%d", m_maxZoomLevel,
                 nMinTileY, nMaxTileY, nMinTileX, nMaxTileX);

        for (int iY = nMinTileY; bRet && iY <= nMaxTileY; ++iY)
        {
            for (int iX = nMinTileX; bRet && iX <= nMaxTileX; ++iX)
            {
                bRet = GenerateTile(
                    poSrcDS, poMemDriver, poDstDriver, pszExtension,
                    aosCreationOptions.List(), oWO, oSRS_TMS,
                    psWO->eWorkingDataType, tileMatrix, m_outputDirectory,
                    nDstBands,
                    psWO->padfDstNoDataReal ? &(psWO->padfDstNoDataReal[0])
                                            : nullptr,
                    m_maxZoomLevel, iX, iY, m_convention, nMinTileX, nMinTileY,
                    m_skipBlank, m_auxXML, m_resume, m_metadata, poColorTable,
                    dstBuffer);

                ++nCurTile;
                bRet &= (!pfnProgress ||
                         pfnProgress(static_cast<double>(nCurTile) /
                                         static_cast<double>(nTotalTiles),
                                     "", pProgressData));
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

        CPL_IGNORE_RET_VAL(GetTileIndices(
            srcTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent, nSrcMinTileX,
            nSrcMinTileY, nSrcMaxTileX, nSrcMaxTileY));

        MosaicDataset oSrcDS(
            CPLFormFilenameSafe(m_outputDirectory.c_str(),
                                CPLSPrintf("%d", iZ + 1), nullptr),
            pszExtension, poSrcDS, srcTileMatrix, oSRS_TMS, nSrcMinTileX,
            nSrcMinTileY, nSrcMaxTileX, nSrcMaxTileY, m_convention, nDstBands,
            psWO->eWorkingDataType,
            psWO->padfDstNoDataReal ? &(psWO->padfDstNoDataReal[0]) : nullptr,
            m_metadata, poColorTable);

        auto ovrTileMatrix = tileMatrixList[iZ];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        CPL_IGNORE_RET_VAL(GetTileIndices(
            ovrTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent, nOvrMinTileX,
            nOvrMinTileY, nOvrMaxTileX, nOvrMaxTileY));

        CPLDebug("gdal_raster_tile",
                 "Generating overview tiles z=%d, y=%d...%d, x=%d...%d", iZ,
                 nOvrMinTileY, nOvrMaxTileY, nOvrMinTileX, nOvrMaxTileX);

        const CPLStringList aosCreationOptions(
            GetUpdatedCreationOptions(ovrTileMatrix));
        for (int iY = nOvrMinTileY; bRet && iY <= nOvrMaxTileY; ++iY)
        {
            for (int iX = nOvrMinTileX; bRet && iX <= nOvrMaxTileX; ++iX)
            {
                bRet = GenerateOverviewTile(
                    oSrcDS, m_outputFormat, pszExtension,
                    aosCreationOptions.List(), m_resampling, ovrTileMatrix,
                    m_outputDirectory, iZ, iX, iY, m_convention, m_skipBlank,
                    m_auxXML, m_resume);

                ++nCurTile;
                bRet &= (!pfnProgress ||
                         pfnProgress(static_cast<double>(nCurTile) /
                                         static_cast<double>(nTotalTiles),
                                     "", pProgressData));
            }
        }
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

        OGRSpatialReference oWGS84;
        oWGS84.importFromEPSG(4326);
        oWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        auto poCTToWGS84 = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(&oSRS_TMS, &oWGS84));
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

    return bRet;
}

//! @endcond

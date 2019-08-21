/******************************************************************************
 *
 * Project:  COG Driver
 * Purpose:  Cloud optimized GeoTIFF write support.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
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

#include "gdal_priv.h"
#include "gtiff.h"
#include "gt_overview.h"
#include "gdal_utils.h"
#include "gdalwarper.h"
#include "cogdriver.h"

#include <algorithm>
#include <memory>
#include <vector>

extern "C" CPL_DLL void GDALRegister_COG();

/************************************************************************/
/*                        HasZSTDCompression()                          */
/************************************************************************/

static bool HasZSTDCompression()
{
    TIFFCodec *codecs = TIFFGetConfiguredCODECs();
    bool bHasZSTD = false;
    for( TIFFCodec *c = codecs; c->name; ++c )
    {
        if( c->scheme == COMPRESSION_ZSTD )
        {
            bHasZSTD = true;
            break;
        }
    }
    _TIFFfree( codecs );
    return bHasZSTD;
}

/************************************************************************/
/*                           GetTmpFilename()                           */
/************************************************************************/

static CPLString GetTmpFilename(const char* pszFilename,
                                const char* pszExt)
{
    CPLString osTmpFilename;
    osTmpFilename.Printf("%s.%s", pszFilename, pszExt);
    VSIUnlink(osTmpFilename);
    return osTmpFilename;
}

/************************************************************************/
/*                             GetResampling()                          */
/************************************************************************/

static const char* GetResampling(GDALDataset* poSrcDS)
{
    return poSrcDS->GetRasterBand(1)->GetColorTable() ? "NEAREST" : "CUBIC";
}

/************************************************************************/
/*                     COGGetWarpingCharacteristics()                   */
/************************************************************************/

bool COGGetWarpingCharacteristics(GDALDataset* poSrcDS,
                                  const char * const* papszOptions,
                                  CPLString& osResampling,
                                  CPLString& osTargetSRS,
                                  int& nXSize,
                                  int& nYSize,
                                  double& dfMinX,
                                  double& dfMinY,
                                  double& dfMaxX,
                                  double& dfMaxY)
{
    osTargetSRS = CSLFetchNameValueDef(papszOptions, "TARGET_SRS", "");
    CPLString osTilingScheme(CSLFetchNameValueDef(papszOptions,
                                                  "TILING_SCHEME", "CUSTOM"));
    if( EQUAL(osTargetSRS, "") && EQUAL(osTilingScheme, "CUSTOM") )
        return false;

    CPLString osExtent(CSLFetchNameValueDef(papszOptions, "EXTENT", ""));
    CPLString osRes(CSLFetchNameValueDef(papszOptions, "RES", ""));
    if( EQUAL(osTilingScheme, "GoogleMapsCompatible") )
        osTargetSRS = "EPSG:3857";

    CPLStringList aosTO;
    aosTO.SetNameValue( "DST_SRS", osTargetSRS );
    void* hTransformArg =
            GDALCreateGenImgProjTransformer2( poSrcDS, nullptr, aosTO.List() );
    if( hTransformArg == nullptr )
    {
        return false;
    }

    GDALTransformerInfo* psInfo = static_cast<GDALTransformerInfo*>(hTransformArg);
    double adfGeoTransform[6];
    double adfExtent[4];

    if ( GDALSuggestedWarpOutput2( poSrcDS,
                                  psInfo->pfnTransform, hTransformArg,
                                  adfGeoTransform,
                                  &nXSize, &nYSize,
                                  adfExtent, 0 ) != CE_None )
    {
        GDALDestroyGenImgProjTransformer( hTransformArg );
        return false;
    }

    GDALDestroyGenImgProjTransformer( hTransformArg );
    hTransformArg = nullptr;


    // Hack to compensate for GDALSuggestedWarpOutput2() failure when
    // reprojection latitude = +/- 90 to EPSG:3857.
    double adfSrcGeoTransform[6];
    OGRSpatialReference oTargetSRS;
    oTargetSRS.SetFromUserInput(osTargetSRS);
    const char* pszAuthCode = oTargetSRS.GetAuthorityCode(nullptr);
    const int nEPSGCode = pszAuthCode ? atoi(pszAuthCode) : 0;
    if( nEPSGCode == 3857 && poSrcDS->GetGeoTransform(adfSrcGeoTransform) == CE_None )
    {
        const char* pszSrcWKT = poSrcDS->GetProjectionRef();
        if( pszSrcWKT != nullptr && pszSrcWKT[0] != '\0' )
        {
            OGRSpatialReference oSrcSRS;
            if( oSrcSRS.SetFromUserInput( pszSrcWKT ) == OGRERR_NONE &&
                oSrcSRS.IsGeographic() )
            {
                const double minLat =
                    std::min(adfSrcGeoTransform[3],
                             adfSrcGeoTransform[3] +
                             poSrcDS->GetRasterYSize() *
                             adfSrcGeoTransform[5]);
                const double maxLat =
                    std::max(adfSrcGeoTransform[3],
                             adfSrcGeoTransform[3] +
                             poSrcDS->GetRasterYSize() *
                             adfSrcGeoTransform[5]);
                double maxNorthing = adfGeoTransform[3];
                double minNorthing =
                    adfGeoTransform[3] + adfGeoTransform[5] * nYSize;
                bool bChanged = false;
                const double SPHERICAL_RADIUS = 6378137.0;
                const double MAX_GM =
                    SPHERICAL_RADIUS * M_PI;  // 20037508.342789244
                if( maxLat > 89.9999999 )
                {
                    bChanged = true;
                    maxNorthing = MAX_GM;
                }
                if( minLat <= -89.9999999 )
                {
                    bChanged = true;
                    minNorthing = -MAX_GM;
                }
                if( bChanged )
                {
                    adfGeoTransform[3] = maxNorthing;
                    nYSize = int((maxNorthing - minNorthing) / (-adfGeoTransform[5]) + 0.5);
                    adfExtent[1] = maxNorthing + nYSize * adfGeoTransform[5];
                    adfExtent[3] = maxNorthing;
                }
            }
        }
    }

    dfMinX = adfExtent[0];
    dfMinY = adfExtent[1];
    dfMaxX = adfExtent[2];
    dfMaxY = adfExtent[3];
    double dfRes = adfGeoTransform[1];

    if( EQUAL(osTilingScheme, "GoogleMapsCompatible") )
    {
        const int nBlockSize = atoi(CSLFetchNameValueDef(
            papszOptions, "BLOCKSIZE", "256"));
        const double dfPixelXSizeZoomLevel0 = 156543.0339280410 * 256 / nBlockSize;
        const double dfOriX = -(dfPixelXSizeZoomLevel0 * nBlockSize) / 2.0;
        const double dfOriY = -dfOriX;
        double dfComputedRes = adfGeoTransform[1];
        double dfPrevRes = 0.0;
        dfRes = 0.0;
        int nZoomLevel = 0;  // Used after for.
        for( ; nZoomLevel < 25; nZoomLevel++ )
        {
            dfRes = dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);
            if( dfComputedRes > dfRes )
                break;
            dfPrevRes = dfRes;
        }
        if( nZoomLevel == 25 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Could not find an appropriate zoom level");
            return false;
        }
        if( nZoomLevel > 0 )
        {
            if( dfPrevRes / dfComputedRes < dfComputedRes / dfRes )
            {
                nZoomLevel --;
                dfRes = dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);
            }
        }
        CPLDebug("COG", "Using ZOOM_LEVEL %d", nZoomLevel);

        const double dfTileExtent = dfRes * nBlockSize;
        int nTLTileX = static_cast<int>(std::floor((dfMinX - dfOriX) / dfTileExtent));
        int nTLTileY = static_cast<int>(std::floor((dfOriY - dfMaxY) / dfTileExtent));
        int nBRTileX = static_cast<int>(std::ceil((dfMaxX - dfOriX) / dfTileExtent));
        int nBRTileY = static_cast<int>(std::ceil((dfOriY - dfMinY) / dfTileExtent));

        const int nAlignedLevels = std::min(10, atoi(
        CSLFetchNameValueDef(papszOptions, "ALIGNED_LEVELS", "0")));
        if( nAlignedLevels >= 1 )
        {
            const int nAlignment = ((1 << (nAlignedLevels-1))-1);
            nTLTileX &= ~nAlignment;
            nTLTileY &= ~nAlignment;
            nBRTileY = (nBRTileY + nAlignment) & ~nAlignment;
            nBRTileX = (nBRTileX + nAlignment) & ~nAlignment;
        }
        nTLTileX = std::max(0, nTLTileX);
        nTLTileY = std::max(0, nTLTileY);
        nBRTileX = std::min(1 << nZoomLevel, nBRTileX);
        nBRTileY = std::min(1 << nZoomLevel, nBRTileY);

        dfMinX = dfOriX + nTLTileX * dfTileExtent;
        dfMinY = dfOriY - nBRTileY * dfTileExtent;
        dfMaxX = dfOriX + nBRTileX * dfTileExtent;
        dfMaxY = dfOriY - nTLTileY * dfTileExtent;
        nXSize = static_cast<int>(std::round((dfMaxX - dfMinX) / dfRes));
        nYSize = static_cast<int>(std::round((dfMaxY - dfMinY) / dfRes));
    }
    else if( !osExtent.empty() || !osRes.empty() )
    {
        CPLStringList aosTokens(CSLTokenizeString2(osExtent, ",", 0));
        if( aosTokens.size() != 4 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value for EXTENT");
            return false;
        }
        dfMinX = CPLAtof(aosTokens[0]);
        dfMinY = CPLAtof(aosTokens[1]);
        dfMaxX = CPLAtof(aosTokens[2]);
        dfMaxY = CPLAtof(aosTokens[3]);
        if( !osRes.empty() )
            dfRes = CPLAtof(osRes);
    }

    nXSize = static_cast<int>(std::round((dfMaxX - dfMinX) / dfRes));
    nYSize = static_cast<int>(std::round((dfMaxY - dfMinY) / dfRes));

    osResampling = CSLFetchNameValueDef(papszOptions,
        "RESAMPLING", GetResampling(poSrcDS));

    return true;
}

/************************************************************************/
/*                        COGHasWarpingOptions()                        */
/************************************************************************/

bool COGHasWarpingOptions(CSLConstList papszOptions)
{
    return CSLFetchNameValue(papszOptions, "TARGET_SRS") != nullptr ||
           !EQUAL(CSLFetchNameValueDef(papszOptions, "TILING_SCHEME", "CUSTOM"),
                  "CUSTOM");
}

/************************************************************************/
/*                      COGRemoveWarpingOptions()                       */
/************************************************************************/

void COGRemoveWarpingOptions(CPLStringList& aosOptions)
{
    aosOptions.SetNameValue("TARGET_SRS", nullptr);
    aosOptions.SetNameValue("TILING_SCHEME", nullptr);
    aosOptions.SetNameValue("EXTENT", nullptr);
    aosOptions.SetNameValue("RES", nullptr);
    aosOptions.SetNameValue("ALIGNED_LEVELS", nullptr);
}

/************************************************************************/
/*                        CreateReprojectedDS()                         */
/************************************************************************/

static std::unique_ptr<GDALDataset> CreateReprojectedDS(
                                const char* pszDstFilename,
                                GDALDataset *poSrcDS,
                                const char * const* papszOptions,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData,
                                double& dfCurPixels,
                                double& dfTotalPixelsToProcess)
{
    CPLString osResampling;
    CPLString osTargetSRS;
    int nXSize = 0;
    int nYSize = 0;
    double dfMinX = 0;
    double dfMinY = 0;
    double dfMaxX = 0;
    double dfMaxY = 0;
    if( !COGGetWarpingCharacteristics(poSrcDS, papszOptions,
                                      osResampling,
                                      osTargetSRS,
                                      nXSize, nYSize,
                                      dfMinX, dfMinY, dfMaxX, dfMaxY) )
    {
        return nullptr;
    }

    char** papszArg = nullptr;
    // We could have done a warped VRT, but overview building on it might be
    // slow, so materialize as GTiff
    papszArg = CSLAddString(papszArg, "-of");
    papszArg = CSLAddString(papszArg, "GTiff");
    papszArg = CSLAddString(papszArg, "-co");
    papszArg = CSLAddString(papszArg, "TILED=YES");
    papszArg = CSLAddString(papszArg, "-co");
    papszArg = CSLAddString(papszArg, "SPARSE_OK=YES");
    const char* pszBIGTIFF = CSLFetchNameValue(papszOptions, "BIGTIFF");
    if( pszBIGTIFF )
    {
        papszArg = CSLAddString(papszArg, "-co");
        papszArg = CSLAddString(papszArg, (CPLString("BIGTIFF=") + pszBIGTIFF).c_str());
    }
    papszArg = CSLAddString(papszArg, "-co");
    papszArg = CSLAddString(papszArg,
                    HasZSTDCompression() ? "COMPRESS=ZSTD" : "COMPRESS=LZW");
    papszArg = CSLAddString(papszArg, "-t_srs");
    papszArg = CSLAddString(papszArg, osTargetSRS);
    papszArg = CSLAddString(papszArg, "-te");
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMinX));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMinY));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMaxX));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMaxY));
    papszArg = CSLAddString(papszArg, "-ts");
    papszArg = CSLAddString(papszArg, CPLSPrintf("%d", nXSize));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%d", nYSize));
    int bHasNoData = FALSE;
    poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoData);
    if( !bHasNoData && CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "ADD_ALPHA", "YES")) )
    {
        papszArg = CSLAddString(papszArg, "-dstalpha");
    }
    papszArg = CSLAddString(papszArg, "-r");
    papszArg = CSLAddString(papszArg, osResampling);
    const char* pszNumThreads = CSLFetchNameValue(papszOptions, "NUM_THREADS");
    if( pszNumThreads )
    {
        papszArg = CSLAddString(papszArg, "-wo");
        papszArg = CSLAddString(papszArg, (CPLString("NUM_THREADS=") + pszNumThreads).c_str());
    }

    const auto poFirstBand = poSrcDS->GetRasterBand(1);
    const bool bHasMask = poFirstBand->GetMaskFlags() == GMF_PER_DATASET;

    const int nBands = poSrcDS->GetRasterCount();
    const char* pszOverviews = CSLFetchNameValueDef(
        papszOptions, "OVERVIEWS", "AUTO");
    const bool bRecreateOvr = EQUAL(pszOverviews, "FORCE_USE_EXISTING") ||
                              EQUAL(pszOverviews, "NONE");
    dfTotalPixelsToProcess =
        double(nXSize) * nYSize * (nBands + (bHasMask ? 1 : 0)) +
        ((bHasMask && !bRecreateOvr) ? double(nXSize) * nYSize / 3 : 0) +
        (!bRecreateOvr ? double(nXSize) * nYSize * nBands / 3: 0) +
        double(nXSize) * nYSize * (nBands + (bHasMask ? 1 : 0)) * 4. / 3;

    auto psOptions = GDALWarpAppOptionsNew(papszArg, nullptr);
    CSLDestroy(papszArg);
    const double dfNextPixels =
        double(nXSize) * nYSize * (nBands + (bHasMask ? 1 : 0));
    void* pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixelsToProcess,
                dfNextPixels / dfTotalPixelsToProcess,
                pfnProgress, pProgressData );
    dfCurPixels = dfNextPixels;

    CPLDebug("COG", "Reprojecting source dataset");
    GDALWarpAppOptionsSetProgress(psOptions, GDALScaledProgress, pScaledProgress );
    CPLString osTmpFile(GetTmpFilename(pszDstFilename, "warped.tif.tmp"));
    auto hSrcDS = GDALDataset::ToHandle(poSrcDS);
    auto hRet = GDALWarp( osTmpFile, nullptr,
                          1, &hSrcDS,
                          psOptions, nullptr);
    GDALWarpAppOptionsFree(psOptions);

    GDALDestroyScaledProgress(pScaledProgress);

    return std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(hRet));
}

/************************************************************************/
/*                            GDALCOGCreator                            */
/************************************************************************/

struct GDALCOGCreator final
{
    std::unique_ptr<GDALDataset> m_poReprojectedDS{};
    std::unique_ptr<GDALDataset> m_poRGBMaskDS{};
    CPLString                    m_osTmpOverviewFilename{};
    CPLString                    m_osTmpMskOverviewFilename{};

    ~GDALCOGCreator();

    GDALDataset* Create(const char * pszFilename,
                        GDALDataset * const poSrcDS,
                        char ** papszOptions,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData );
};

/************************************************************************/
/*                    GDALCOGCreator::~GDALCOGCreator()                 */
/************************************************************************/

GDALCOGCreator::~GDALCOGCreator()
{
    if( m_poReprojectedDS )
    {
        CPLString osProjectedDSName(m_poReprojectedDS->GetDescription());
        // Destroy m_poRGBMaskDS before m_poReprojectedDS since the former
        // references the later
        m_poRGBMaskDS.reset();
        m_poReprojectedDS.reset();
        VSIUnlink(osProjectedDSName);
    }
    if( !m_osTmpOverviewFilename.empty() )
    {
        VSIUnlink(m_osTmpOverviewFilename);
    }
    if( !m_osTmpMskOverviewFilename.empty() )
    {
        VSIUnlink(m_osTmpMskOverviewFilename);
    }
}

/************************************************************************/
/*                    GDALCOGCreator::Create()                          */
/************************************************************************/

GDALDataset* GDALCOGCreator::Create(const char * pszFilename,
                                    GDALDataset * const poSrcDS,
                                    char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData )
{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    if( poSrcDS->GetRasterCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "COG driver does not support 0-band source raster");
        return nullptr;
    }

    CPLConfigOptionSetter oSetterReportDirtyBlockFlushing(
        "GDAL_REPORT_DIRTY_BLOCK_FLUSHING", "NO", true);

    double dfCurPixels = 0;
    double dfTotalPixelsToProcess = 0;
    GDALDataset* poCurDS = poSrcDS;

    if( COGHasWarpingOptions(papszOptions) )
    {
        m_poReprojectedDS =
            CreateReprojectedDS(pszFilename, poCurDS,
                                papszOptions, pfnProgress, pProgressData,
                                dfCurPixels, dfTotalPixelsToProcess);
        if( !m_poReprojectedDS )
            return nullptr;
        poCurDS = m_poReprojectedDS.get();
    }

    CPLString osCompress = CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    if( EQUAL(osCompress, "JPEG") &&
        poCurDS->GetRasterCount() == 4 )
    {
        char** papszArg = nullptr;
        papszArg = CSLAddString(papszArg, "-of");
        papszArg = CSLAddString(papszArg, "VRT");
        papszArg = CSLAddString(papszArg, "-b");
        papszArg = CSLAddString(papszArg, "1");
        papszArg = CSLAddString(papszArg, "-b");
        papszArg = CSLAddString(papszArg, "2");
        papszArg = CSLAddString(papszArg, "-b");
        papszArg = CSLAddString(papszArg, "3");
        papszArg = CSLAddString(papszArg, "-mask");
        papszArg = CSLAddString(papszArg, "4");
        GDALTranslateOptions* psOptions = GDALTranslateOptionsNew(papszArg, nullptr);
        CSLDestroy(papszArg);
        GDALDatasetH hRGBMaskDS = GDALTranslate("",
                                                GDALDataset::ToHandle(poCurDS),
                                                psOptions,
                                                nullptr);
        GDALTranslateOptionsFree(psOptions);
        if( !hRGBMaskDS )
        {
            return nullptr;
        }
        m_poRGBMaskDS.reset( GDALDataset::FromHandle(hRGBMaskDS) );
        poCurDS = m_poRGBMaskDS.get();
    }

    const int nBands = poCurDS->GetRasterCount();
    const int nXSize = poCurDS->GetRasterXSize();
    const int nYSize = poCurDS->GetRasterYSize();

    CPLString osBlockSize(CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", ""));
    if( osBlockSize.empty() )
    {
        if( EQUAL(CSLFetchNameValueDef(papszOptions, "TILING_SCHEME", "CUSTOM"),
            "GoogleMapsCompatible") )
            osBlockSize = "256";
        else
            osBlockSize = "512";
    }

    const int nOvrThresholdSize = atoi(osBlockSize);

    const auto poFirstBand = poCurDS->GetRasterBand(1);
    const bool bHasMask = poFirstBand->GetMaskFlags() == GMF_PER_DATASET;

    CPLString osOverviews = CSLFetchNameValueDef(
        papszOptions, "OVERVIEWS", "AUTO");
    const bool bRecreateOvr = EQUAL(osOverviews, "FORCE_USE_EXISTING") ||
                              EQUAL(osOverviews, "NONE");
    const bool bGenerateMskOvr = 
        !bRecreateOvr &&
        bHasMask &&
        (nXSize > nOvrThresholdSize || nYSize > nOvrThresholdSize) &&
        (EQUAL(osOverviews, "IGNORE_EXISTING") ||
         poFirstBand->GetMaskBand()->GetOverviewCount() == 0);
    const bool bGenerateOvr =
        !bRecreateOvr &&
        (nXSize > nOvrThresholdSize || nYSize > nOvrThresholdSize) &&
        (EQUAL(osOverviews, "IGNORE_EXISTING") ||
         poFirstBand->GetOverviewCount() == 0);

    std::vector<int> anOverviewLevels;
    int nTmpXSize = nXSize;
    int nTmpYSize = nYSize;
    int nOvrFactor = 2;
    while( nTmpXSize > nOvrThresholdSize || nTmpYSize > nOvrThresholdSize )
    {
        anOverviewLevels.push_back(nOvrFactor);
        nOvrFactor *= 2;
        nTmpXSize /= 2;
        nTmpYSize /= 2;
    }

    if( dfTotalPixelsToProcess == 0.0 )
    {
        dfTotalPixelsToProcess =
            (bGenerateMskOvr ? double(nXSize) * nYSize / 3 : 0) +
            (bGenerateOvr ? double(nXSize) * nYSize * nBands / 3: 0) +
            double(nXSize) * nYSize * (nBands + (bHasMask ? 1 : 0)) * 4. / 3;
    }

    CPLStringList aosOverviewOptions;
    aosOverviewOptions.SetNameValue("COMPRESS",
        CPLGetConfigOption("COG_TMP_COMPRESSION", // only for debug purposes
                        HasZSTDCompression() ? "ZSTD" : "LZW"));
    aosOverviewOptions.SetNameValue("NUM_THREADS",
                        CSLFetchNameValue(papszOptions, "NUM_THREADS"));
    aosOverviewOptions.SetNameValue("BIGTIFF", "YES");

    if( bGenerateMskOvr )
    {
        CPLDebug("COG", "Generating overviews of the mask");
        m_osTmpMskOverviewFilename = GetTmpFilename(pszFilename, "msk.ovr.tmp");
        GDALRasterBand* poSrcMask = poFirstBand->GetMaskBand();
        const char* pszResampling = CSLFetchNameValueDef(papszOptions,
            "RESAMPLING", GetResampling(poSrcDS));

        double dfNextPixels = dfCurPixels + double(nXSize) * nYSize / 3;
        void* pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixelsToProcess,
                dfNextPixels / dfTotalPixelsToProcess,
                pfnProgress, pProgressData );
        dfCurPixels = dfNextPixels;

        // Used by GDALRegenerateOverviews() and GDALRegenerateOverviewsMultiBand()
        CPLConfigOptionSetter oSetterRegeneratedBandIsMask(
            "GDAL_REGENERATED_BAND_IS_MASK", "YES", true);

        CPLErr eErr = GTIFFBuildOverviewsEx(
            m_osTmpMskOverviewFilename,
            1, &poSrcMask,
            static_cast<int>(anOverviewLevels.size()),
            &anOverviewLevels[0],
            pszResampling,
            aosOverviewOptions.List(),
            GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress(pScaledProgress);
        if( eErr != CE_None )
        {
            return nullptr;
        }
    }

    if( bGenerateOvr )
    {
        CPLDebug("COG", "Generating overviews of the imagery");
        m_osTmpOverviewFilename = GetTmpFilename(pszFilename, "ovr.tmp");
        std::vector<GDALRasterBand*> apoSrcBands;
        for( int i = 0; i < nBands; i++ )
            apoSrcBands.push_back( poCurDS->GetRasterBand(i+1) );
        const char* pszResampling = CSLFetchNameValueDef(papszOptions,
            "RESAMPLING", GetResampling(poSrcDS));

        double dfNextPixels = dfCurPixels + double(nXSize) * nYSize * nBands / 3;
        void* pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixelsToProcess,
                dfNextPixels / dfTotalPixelsToProcess,
                pfnProgress, pProgressData );
        dfCurPixels = dfNextPixels;

        if( nBands > 1 )
        {
            aosOverviewOptions.SetNameValue("INTERLEAVE", "PIXEL");
        }
        if( !m_osTmpMskOverviewFilename.empty() )
        {
            aosOverviewOptions.SetNameValue("MASK_OVERVIEW_DATASET",
                                            m_osTmpMskOverviewFilename);
        }
        CPLErr eErr = GTIFFBuildOverviewsEx(
            m_osTmpOverviewFilename,
            nBands, &apoSrcBands[0],
            static_cast<int>(anOverviewLevels.size()),
            &anOverviewLevels[0],
            pszResampling,
            aosOverviewOptions.List(),
            GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress(pScaledProgress);
        if( eErr != CE_None )
        {
            return nullptr;
        }
    }

    CPLStringList aosOptions;
    aosOptions.SetNameValue("COPY_SRC_OVERVIEWS", "YES");
    aosOptions.SetNameValue("COMPRESS", osCompress);
    aosOptions.SetNameValue("TILED", "YES");
    aosOptions.SetNameValue("BLOCKXSIZE", osBlockSize);
    aosOptions.SetNameValue("BLOCKYSIZE", osBlockSize);
    if( CPLTestBool(CSLFetchNameValueDef(papszOptions, "PREDICTOR", "FALSE")) )
        aosOptions.SetNameValue("PREDICTOR", "2");
    const char* pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    if( EQUAL(osCompress, "JPEG") )
    {
        aosOptions.SetNameValue("JPEG_QUALITY", pszQuality);
        if( nBands == 3 )
            aosOptions.SetNameValue("PHOTOMETRIC", "YCBCR");
    }
    else if( EQUAL(osCompress, "WEBP") )
    {
        if( pszQuality && atoi(pszQuality) == 100 )
            aosOptions.SetNameValue("WEBP_LOSSLESS", "YES");
        aosOptions.SetNameValue("WEBP_LEVEL", pszQuality);
    }
    else if( EQUAL(osCompress, "DEFLATE") )
    {
        aosOptions.SetNameValue("ZLEVEL",
                                CSLFetchNameValue(papszOptions, "LEVEL"));
    }
    else if( EQUAL(osCompress, "ZSTD") )
    {
        aosOptions.SetNameValue("ZSTD_LEVEL",
                                CSLFetchNameValue(papszOptions, "LEVEL"));
    }
    aosOptions.SetNameValue("BIGTIFF",
                                CSLFetchNameValue(papszOptions, "BIGTIFF"));
    aosOptions.SetNameValue("NUM_THREADS",
                                CSLFetchNameValue(papszOptions, "NUM_THREADS"));

    if( EQUAL( osOverviews, "NONE") )
    {
        aosOptions.SetNameValue("@OVERVIEW_DATASET", "");
    }
    else
    {
        if( !m_osTmpOverviewFilename.empty() )
        {
            aosOptions.SetNameValue("@OVERVIEW_DATASET", m_osTmpOverviewFilename);
        }
        if( !m_osTmpMskOverviewFilename.empty() )
        {
            aosOptions.SetNameValue("@MASK_OVERVIEW_DATASET", m_osTmpMskOverviewFilename);
        }
    }

    GDALDriver* poGTiffDrv = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
    if( !poGTiffDrv )
        return nullptr;
    void* pScaledProgress = GDALCreateScaledProgress(
            dfCurPixels / dfTotalPixelsToProcess,
            1.0,
            pfnProgress, pProgressData );

    CPLConfigOptionSetter oSetterInternalMask(
        "GDAL_TIFF_INTERNAL_MASK", "YES", false);

    auto poRet = poGTiffDrv->CreateCopy(pszFilename, poCurDS, false,
                                        aosOptions.List(),
                                        GDALScaledProgress, pScaledProgress);

    GDALDestroyScaledProgress(pScaledProgress);

    return poRet;
}

/************************************************************************/
/*                            COGCreateCopy()                           */
/************************************************************************/

static GDALDataset* COGCreateCopy( const char * pszFilename,
                                   GDALDataset *poSrcDS,
                                   int /*bStrict*/, char ** papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData )
{
    return GDALCOGCreator().Create(pszFilename, poSrcDS,
                                   papszOptions, pfnProgress, pProgressData);
}

/************************************************************************/
/*                          GDALRegister_COG()                          */
/************************************************************************/

void GDALRegister_COG()

{
    if( GDALGetDriverByName( "COG" ) != nullptr )
        return;

    bool bHasLZW = false;
    bool bHasDEFLATE = false;
    bool bHasLZMA = false;
    bool bHasZSTD = false;
    bool bHasJPEG = false;
    bool bHasWebP = false;
    CPLString osCompressValues(GTiffGetCompressValues(
        bHasLZW, bHasDEFLATE, bHasLZMA, bHasZSTD, bHasJPEG, bHasWebP,
        true /* bForCOG */));

    CPLString osOptions;
    osOptions = "<CreationOptionList>"
                "   <Option name='COMPRESS' type='string-select'>";
    osOptions += osCompressValues;
    osOptions += "   </Option>";
    if( bHasLZW || bHasDEFLATE || bHasZSTD )
    {
        osOptions += "   <Option name='LEVEL' type='int' "
            "description='DEFLATE/ZSTD compression level: 1 (fastest)'/>";
        osOptions += "   <Option name='PREDICTOR' type='boolean' default='FALSE'/>";
    }
    if( bHasJPEG || bHasWebP )
    {
        osOptions += "   <Option name='QUALITY' type='int' "
                     "description='JPEG/WEBP quality 1-100' default='75'/>";
    }
    osOptions +=
"   <Option name='NUM_THREADS' type='string' "
        "description='Number of worker threads for compression. "
        "Can be set to ALL_CPUS' default='1'/>"
"   <Option name='BLOCKSIZE' type='int' "
        "description='Tile size in pixels' min='128' default='512'/>"
"   <Option name='BIGTIFF' type='string-select' description='"
        "Force creation of BigTIFF file'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"     <Value>IF_SAFER</Value>"
"   </Option>"
"   <Option name='RESAMPLING' type='string' "
        "description='Resampling method for overviews or warping'/>"
"   <Option name='OVERVIEWS' type='string-select' description='"
        "Behaviour regarding overviews'>"
"     <Value>AUTO</Value>"
"     <Value>IGNORE_EXISTING</Value>"
"     <Value>FORCE_USE_EXISTING</Value>"
"     <Value>NONE</Value>"
"   </Option>"
"  <Option name='TILING_SCHEME' type='string-select' description='"
        "Which tiling scheme to use' default='CUSTOM'>"
"    <Value>CUSTOM</Value>"
"    <Value>GoogleMapsCompatible</Value>"
"  </Option>"
"   <Option name='TARGET_SRS' type='string' "
        "description='Target SRS as EPSG:XXXX, WKT or PROJ string for reprojection'/>"
"  <Option name='RES' type='float' description='"
        "Target resolution for reprojection'/>"
"  <Option name='EXTENT' type='string' description='"
        "Target extent as minx,miny,maxx,maxy for reprojection'/>"
"  <Option name='ALIGNED_LEVELS' type='int' description='"
        "Number of overview levels for which GeoTIFF tile and WebMercator "
        "tiles match'/>"
"  <Option name='ADD_ALPHA' type='boolean' description='Can be set to NO to "
        "disable the addition of an alpha band in case of reprojection' default='YES'/>"
"</CreationOptionList>";

    auto poDriver = new GDALDriver();
    poDriver->SetDescription( "COG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Cloud optimized GeoTIFF generator" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "cog.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, osOptions );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64 CInt16 CInt32 CFloat32 CFloat64" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnCreateCopy = COGCreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}

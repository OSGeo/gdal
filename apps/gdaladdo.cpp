/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to build overviews.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal_priv.h"
#include "commonutils.h"
#include "vrtdataset.h"

#include <algorithm>

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(bool bIsError, const char *pszErrorMsg = nullptr)

{
    fprintf(
        bIsError ? stderr : stdout,
        "Usage: gdaladdo [--help] [--help-general]\n"
        "                [-r "
        "{nearest|average|rms|gauss|cubic|cubicspline|lanczos|average_mp|"
        "average_magphase|mode}]\n"
        "                [-ro] [-clean] [-q] [-oo <NAME>=<VALUE>]... [-minsize "
        "<val>]\n"
        "                [--partial-refresh-from-source-timestamp]\n"
        "                [--partial-refresh-from-projwin <ulx> <uly> <lrx> "
        "<lry>]\n"
        "                [--partial-refresh-from-source-extent "
        "<filename1>[,<filenameN>]...]\n"
        "                <filename> [<levels>]...\n"
        "\n"
        "  -r : choice of resampling method (default: nearest)\n"
        "  -ro : open the dataset in read-only mode, in order to generate\n"
        "        external overview (for GeoTIFF datasets especially)\n"
        "  -clean : remove all overviews\n"
        "  -q : turn off progress display\n"
        "  -b : band to create overview (if not set overviews will be created "
        "for all bands)\n"
        "  filename: The file to build overviews for (or whose overviews must "
        "be removed).\n"
        "  levels: A list of integral overview levels to build. Ignored with "
        "-clean option.\n"
        "\n"
        "Useful configuration variables :\n"
        "  --config USE_RRD YES : Use Erdas Imagine format (.aux) as overview "
        "format.\n"
        "Below, only for external overviews in GeoTIFF format:\n"
        "  --config COMPRESS_OVERVIEW {JPEG,LZW,PACKBITS,DEFLATE} : TIFF "
        "compression\n"
        "  --config PHOTOMETRIC_OVERVIEW {RGB,YCBCR,...} : TIFF photometric "
        "interp.\n"
        "  --config INTERLEAVE_OVERVIEW {PIXEL|BAND} : TIFF interleaving "
        "method\n"
        "  --config BIGTIFF_OVERVIEW {IF_NEEDED|IF_SAFER|YES|NO} : is BigTIFF "
        "used\n"
        "\n"
        "Examples:\n"
        " %% gdaladdo -r average abc.tif\n"
        " %% gdaladdo --config COMPRESS_OVERVIEW JPEG\n"
        "            --config PHOTOMETRIC_OVERVIEW YCBCR\n"
        "            --config INTERLEAVE_OVERVIEW PIXEL -ro abc.tif\n");

    if (pszErrorMsg != nullptr)
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(bIsError ? 1 : 0);
}

/************************************************************************/
/*                        GDALAddoErrorHandler()                        */
/************************************************************************/

class GDALError
{
  public:
    CPLErr m_eErr;
    CPLErrorNum m_errNum;
    CPLString m_osMsg;

    explicit GDALError(CPLErr eErr = CE_None, CPLErrorNum errNum = CPLE_None,
                       const char *pszMsg = "")
        : m_eErr(eErr), m_errNum(errNum), m_osMsg(pszMsg ? pszMsg : "")
    {
    }
};

std::vector<GDALError> aoErrors;

static void CPL_STDCALL GDALAddoErrorHandler(CPLErr eErr, CPLErrorNum errNum,
                                             const char *pszMsg)
{
    aoErrors.push_back(GDALError(eErr, errNum, pszMsg));
}

/************************************************************************/
/*                              PartialRefresh()                        */
/************************************************************************/

static bool PartialRefresh(GDALDataset *poDS,
                           const std::vector<int> &anOvrIndices, int nBandCount,
                           const int *panBandList, const char *pszResampling,
                           int nXOff, int nYOff, int nXSize, int nYSize,
                           GDALProgressFunc pfnProgress, void *pProgressArg)
{
    std::vector<int> anBandList;
    if (nBandCount == 0)
    {
        for (int i = 0; i < poDS->GetRasterCount(); ++i)
            anBandList.push_back(i + 1);
        nBandCount = poDS->GetRasterCount();
        panBandList = anBandList.data();
    }

    int nOvCount = 0;
    for (int i = 0; i < nBandCount; ++i)
    {
        auto poSrcBand = poDS->GetRasterBand(panBandList[i]);
        if (i == 0)
            nOvCount = poSrcBand->GetOverviewCount();
        else if (nOvCount != poSrcBand->GetOverviewCount())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Not same number of overviews on all bands");
            return false;
        }
    }

    std::vector<GDALRasterBand *> apoSrcBands;
    std::vector<GDALRasterBand **> apapoOverviewBands;
    for (int i = 0; i < nBandCount; ++i)
    {
        auto poSrcBand = poDS->GetRasterBand(panBandList[i]);
        apoSrcBands.push_back(poSrcBand);
        apapoOverviewBands.push_back(static_cast<GDALRasterBand **>(
            CPLMalloc(sizeof(GDALRasterBand *) * anOvrIndices.size())));
        int j = 0;
        for (int nOvrIdx : anOvrIndices)
        {
            apapoOverviewBands[i][j] = poSrcBand->GetOverview(nOvrIdx);
            ++j;
        }
    }

    CPLStringList aosOptions;
    aosOptions.SetNameValue("XOFF", CPLSPrintf("%d", nXOff));
    aosOptions.SetNameValue("YOFF", CPLSPrintf("%d", nYOff));
    aosOptions.SetNameValue("XSIZE", CPLSPrintf("%d", nXSize));
    aosOptions.SetNameValue("YSIZE", CPLSPrintf("%d", nYSize));
    bool bOK = GDALRegenerateOverviewsMultiBand(
                   nBandCount, apoSrcBands.data(),
                   static_cast<int>(anOvrIndices.size()),
                   apapoOverviewBands.data(), pszResampling, pfnProgress,
                   pProgressArg, aosOptions.List()) == CE_None;
    for (auto papoOverviewBands : apapoOverviewBands)
        CPLFree(papoOverviewBands);
    return bOK;
}

/************************************************************************/
/*                           GetOvrIndices()                            */
/************************************************************************/

static bool GetOvrIndices(GDALDataset *poDS, int nLevelCount,
                          const int *panLevels, bool bMinSizeSpecified,
                          int nMinSize, std::vector<int> &anOvrIndices)
{
    auto poBand = poDS->GetRasterBand(1);
    if (!poBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has no bands");
        return false;
    }
    const int nOvCount = poBand->GetOverviewCount();
    if (nOvCount == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has no overviews");
        return false;
    }

    if (nLevelCount == 0)
    {
        if (!bMinSizeSpecified)
        {
            for (int i = 0; i < nOvCount; ++i)
                anOvrIndices.push_back(i);
        }
        else
        {
            for (int i = 0; i < nOvCount; i++)
            {
                GDALRasterBand *poOverview = poBand->GetOverview(i);
                if (poOverview == nullptr)
                    continue;
                if (poOverview->GetXSize() >= nMinSize ||
                    poOverview->GetYSize() >= nMinSize)
                {
                    anOvrIndices.push_back(i);
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < nLevelCount; ++i)
        {
            const int nLevel = panLevels[i];
            int nIdx = -1;
            for (int j = 0; j < nOvCount; j++)
            {
                GDALRasterBand *poOverview = poBand->GetOverview(j);
                if (poOverview == nullptr)
                    continue;

                int nOvFactor = GDALComputeOvFactor(
                    poOverview->GetXSize(), poBand->GetXSize(),
                    poOverview->GetYSize(), poBand->GetYSize());

                if (nOvFactor == nLevel ||
                    nOvFactor == GDALOvLevelAdjust2(nLevel, poBand->GetXSize(),
                                                    poBand->GetYSize()))
                {
                    nIdx = j;
                    break;
                }
            }
            if (nIdx < 0)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Cannot find overview level with subsampling factor of %d",
                    nLevel);
                return false;
            }
            anOvrIndices.push_back(nIdx);
        }
    }
    return true;
}

/************************************************************************/
/*                   PartialRefreshFromSourceTimestamp()                */
/************************************************************************/

static bool PartialRefreshFromSourceTimestamp(
    GDALDataset *poDS, const char *pszResampling, int nLevelCount,
    const int *panLevels, int nBandCount, const int *panBandList,
    bool bMinSizeSpecified, int nMinSize, GDALProgressFunc pfnProgress,
    void *pProgressArg)
{
    auto poVRTDS = dynamic_cast<VRTDataset *>(poDS);
    if (!poVRTDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "--partial-refresh-from-source-timestamp only works on a VRT "
                 "dataset");
        return false;
    }

    std::vector<int> anOvrIndices;
    if (!GetOvrIndices(poDS, nLevelCount, panLevels, bMinSizeSpecified,
                       nMinSize, anOvrIndices))
        return false;

    VSIStatBufL sStatVRTOvr;
    std::string osVRTOvr(std::string(poDS->GetDescription()) + ".ovr");
    if (VSIStatL(osVRTOvr.c_str(), &sStatVRTOvr) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s\n",
                 osVRTOvr.c_str());
        return false;
    }
    if (sStatVRTOvr.st_mtime == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot get modification time of %s\n", osVRTOvr.c_str());
        return false;
    }

    auto poVRTBand =
        dynamic_cast<VRTSourcedRasterBand *>(poDS->GetRasterBand(1));
    if (!poVRTBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Band is not a VRTSourcedRasterBand");
        return false;
    }

    struct Region
    {
        std::string osFileName;
        int nXOff;
        int nYOff;
        int nXSize;
        int nYSize;
    };
    std::vector<Region> regions;

    double dfTotalPixels = 0;
    for (int i = 0; i < poVRTBand->nSources; ++i)
    {
        auto poSource =
            dynamic_cast<VRTSimpleSource *>(poVRTBand->papoSources[i]);
        if (poSource)
        {
            VSIStatBufL sStatSource;
            if (VSIStatL(poSource->GetSourceDatasetName().c_str(),
                         &sStatSource) == 0)
            {
                if (sStatSource.st_mtime > sStatVRTOvr.st_mtime)
                {
                    double dfXOff, dfYOff, dfXSize, dfYSize;
                    poSource->GetDstWindow(dfXOff, dfYOff, dfXSize, dfYSize);
                    constexpr double EPS = 1e-8;
                    int nXOff = static_cast<int>(dfXOff + EPS);
                    int nYOff = static_cast<int>(dfYOff + EPS);
                    int nXSize = static_cast<int>(dfXSize + 0.5);
                    int nYSize = static_cast<int>(dfYSize + 0.5);
                    if (nXOff > poDS->GetRasterXSize() ||
                        nYOff > poDS->GetRasterYSize() || nXSize <= 0 ||
                        nYSize <= 0)
                    {
                        continue;
                    }
                    if (nXOff < 0)
                    {
                        nXSize += nXOff;
                        nXOff = 0;
                    }
                    if (nXOff > poDS->GetRasterXSize() - nXSize)
                    {
                        nXSize = poDS->GetRasterXSize() - nXOff;
                    }
                    if (nYOff < 0)
                    {
                        nYSize += nYOff;
                        nYOff = 0;
                    }
                    if (nYOff > poDS->GetRasterYSize() - nYSize)
                    {
                        nYSize = poDS->GetRasterYSize() - nYOff;
                    }

                    dfTotalPixels += static_cast<double>(nXSize) * nYSize;
                    Region region;
                    region.osFileName = poSource->GetSourceDatasetName();
                    region.nXOff = nXOff;
                    region.nYOff = nYOff;
                    region.nXSize = nXSize;
                    region.nYSize = nYSize;
                    regions.push_back(std::move(region));
                }
            }
        }
    }

    if (!regions.empty())
    {
        double dfCurPixels = 0;
        for (const auto &region : regions)
        {
            if (pfnProgress == GDALDummyProgress)
            {
                CPLDebug("GDAL", "Refresh from source %s",
                         region.osFileName.c_str());
            }
            else
            {
                printf("Refresh from source %s.\n", region.osFileName.c_str());
            }
            double dfNextCurPixels =
                dfCurPixels +
                static_cast<double>(region.nXSize) * region.nYSize;
            void *pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixels, dfNextCurPixels / dfTotalPixels,
                pfnProgress, pProgressArg);
            bool bRet = PartialRefresh(
                poDS, anOvrIndices, nBandCount, panBandList, pszResampling,
                region.nXOff, region.nYOff, region.nXSize, region.nYSize,
                GDALScaledProgress, pScaledProgress);
            GDALDestroyScaledProgress(pScaledProgress);
            if (!bRet)
                return false;
            dfCurPixels = dfNextCurPixels;
        }
    }
    else
    {
        if (pfnProgress == GDALDummyProgress)
        {
            CPLDebug("GDAL", "No source is more recent than the overviews");
        }
        else
        {
            printf("No source is more recent than the overviews.\n");
        }
    }

    return true;
}

/************************************************************************/
/*                   PartialRefreshFromSourceExtent()                   */
/************************************************************************/

static bool PartialRefreshFromSourceExtent(
    GDALDataset *poDS, const CPLStringList &aosSources,
    const char *pszResampling, int nLevelCount, const int *panLevels,
    int nBandCount, const int *panBandList, bool bMinSizeSpecified,
    int nMinSize, GDALProgressFunc pfnProgress, void *pProgressArg)
{
    std::vector<int> anOvrIndices;
    if (!GetOvrIndices(poDS, nLevelCount, panLevels, bMinSizeSpecified,
                       nMinSize, anOvrIndices))
        return false;

    double adfGeoTransform[6];
    if (poDS->GetGeoTransform(adfGeoTransform) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has no geotransform");
        return false;
    }
    double adfInvGT[6];
    if (!GDALInvGeoTransform(adfGeoTransform, adfInvGT))
    {
        return false;
    }

    struct Region
    {
        std::string osFileName;
        int nXOff;
        int nYOff;
        int nXSize;
        int nYSize;
    };
    std::vector<Region> regions;

    double dfTotalPixels = 0;
    for (int i = 0; i < aosSources.size(); ++i)
    {
        auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            aosSources[i], GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
        if (!poSrcDS)
            return false;

        double adfSrcGT[6];
        if (poSrcDS->GetGeoTransform(adfSrcGT) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Source dataset has no geotransform");
            return false;
        }

        const double dfULX = adfSrcGT[0];
        const double dfULY = adfSrcGT[3];
        const double dfLRX = adfSrcGT[0] +
                             poSrcDS->GetRasterXSize() * adfSrcGT[1] +
                             poSrcDS->GetRasterYSize() * adfSrcGT[2];
        const double dfLRY = adfSrcGT[3] +
                             poSrcDS->GetRasterXSize() * adfSrcGT[4] +
                             poSrcDS->GetRasterYSize() * adfSrcGT[5];
        const double dfX1 =
            adfInvGT[0] + adfInvGT[1] * dfULX + adfInvGT[2] * dfULY;
        const double dfY1 =
            adfInvGT[3] + adfInvGT[4] * dfULX + adfInvGT[5] * dfULY;
        const double dfX2 =
            adfInvGT[0] + adfInvGT[1] * dfLRX + adfInvGT[2] * dfLRY;
        const double dfY2 =
            adfInvGT[3] + adfInvGT[4] * dfLRX + adfInvGT[5] * dfLRY;
        constexpr double EPS = 1e-8;
        const int nXOff =
            static_cast<int>(std::max(0.0, std::min(dfX1, dfX2)) + EPS);
        const int nYOff =
            static_cast<int>(std::max(0.0, std::min(dfY1, dfY2)) + EPS);
        const int nXSize =
            static_cast<int>(
                std::ceil(std::min(static_cast<double>(poDS->GetRasterXSize()),
                                   std::max(dfX1, dfX2)) -
                          EPS)) -
            nXOff;
        const int nYSize =
            static_cast<int>(
                std::ceil(std::min(static_cast<double>(poDS->GetRasterYSize()),
                                   std::max(dfY1, dfY2)) -
                          EPS)) -
            nYOff;

        dfTotalPixels += static_cast<double>(nXSize) * nYSize;
        Region region;
        region.osFileName = aosSources[i];
        region.nXOff = nXOff;
        region.nYOff = nYOff;
        region.nXSize = nXSize;
        region.nYSize = nYSize;
        regions.push_back(std::move(region));
    }

    double dfCurPixels = 0;
    for (const auto &region : regions)
    {
        if (pfnProgress == GDALDummyProgress)
        {
            CPLDebug("GDAL", "Refresh from source %s",
                     region.osFileName.c_str());
        }
        else
        {
            printf("Refresh from source %s.\n", region.osFileName.c_str());
        }
        double dfNextCurPixels =
            dfCurPixels + static_cast<double>(region.nXSize) * region.nYSize;
        void *pScaledProgress = GDALCreateScaledProgress(
            dfCurPixels / dfTotalPixels, dfNextCurPixels / dfTotalPixels,
            pfnProgress, pProgressArg);
        bool bRet = PartialRefresh(poDS, anOvrIndices, nBandCount, panBandList,
                                   pszResampling, region.nXOff, region.nYOff,
                                   region.nXSize, region.nYSize,
                                   GDALScaledProgress, pScaledProgress);
        GDALDestroyScaledProgress(pScaledProgress);
        if (!bRet)
            return false;
        dfCurPixels = dfNextCurPixels;
    }

    return true;
}

/************************************************************************/
/*                     PartialRefreshFromProjWin()                      */
/************************************************************************/

static bool PartialRefreshFromProjWin(
    GDALDataset *poDS, double dfULX, double dfULY, double dfLRX, double dfLRY,
    const char *pszResampling, int nLevelCount, const int *panLevels,
    int nBandCount, const int *panBandList, bool bMinSizeSpecified,
    int nMinSize, GDALProgressFunc pfnProgress, void *pProgressArg)
{
    std::vector<int> anOvrIndices;
    if (!GetOvrIndices(poDS, nLevelCount, panLevels, bMinSizeSpecified,
                       nMinSize, anOvrIndices))
        return false;

    double adfGeoTransform[6];
    if (poDS->GetGeoTransform(adfGeoTransform) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has no geotransform");
        return false;
    }
    double adfInvGT[6];
    if (!GDALInvGeoTransform(adfGeoTransform, adfInvGT))
    {
        return false;
    }
    const double dfX1 = adfInvGT[0] + adfInvGT[1] * dfULX + adfInvGT[2] * dfULY;
    const double dfY1 = adfInvGT[3] + adfInvGT[4] * dfULX + adfInvGT[5] * dfULY;
    const double dfX2 = adfInvGT[0] + adfInvGT[1] * dfLRX + adfInvGT[2] * dfLRY;
    const double dfY2 = adfInvGT[3] + adfInvGT[4] * dfLRX + adfInvGT[5] * dfLRY;
    constexpr double EPS = 1e-8;
    const int nXOff =
        static_cast<int>(std::max(0.0, std::min(dfX1, dfX2)) + EPS);
    const int nYOff =
        static_cast<int>(std::max(0.0, std::min(dfY1, dfY2)) + EPS);
    const int nXSize = static_cast<int>(std::ceil(
                           std::min(static_cast<double>(poDS->GetRasterXSize()),
                                    std::max(dfX1, dfX2)) -
                           EPS)) -
                       nXOff;
    const int nYSize = static_cast<int>(std::ceil(
                           std::min(static_cast<double>(poDS->GetRasterYSize()),
                                    std::max(dfY1, dfY2)) -
                           EPS)) -
                       nYOff;
    return PartialRefresh(poDS, anOvrIndices, nBandCount, panBandList,
                          pszResampling, nXOff, nYOff, nXSize, nYSize,
                          pfnProgress, pProgressArg);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg)                            \
    do                                                                         \
    {                                                                          \
        if (iArg + nExtraArg >= nArgc)                                         \
            Usage(true, CPLSPrintf("%s option requires %d argument(s)",        \
                                   papszArgv[iArg], nExtraArg));               \
    } while (false)

MAIN_START(nArgc, papszArgv)

{
    // Check that we are running against at least GDAL 1.7.
    // Note to developers: if we use newer API, please change the requirement.
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1700)
    {
        fprintf(stderr,
                "At least, GDAL >= 1.7.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n",
                papszArgv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    GDALAllRegister();

    nArgc = GDALGeneralCmdLineProcessor(nArgc, &papszArgv, 0);
    if (nArgc < 1)
        exit(-nArgc);

    std::string osResampling;
    const char *pszFilename = nullptr;
    std::vector<int> anLevels;
    int nResultStatus = 0;
    bool bReadOnly = false;
    bool bClean = false;
    GDALProgressFunc pfnProgress = GDALTermProgress;
    void *pProgressArg = nullptr;
    int *panBandList = nullptr;
    int nBandCount = 0;
    char **papszOpenOptions = nullptr;
    bool bMinSizeSpecified = false;
    int nMinSize = 256;
    bool bPartialRefreshFromSourceTimestamp = false;
    bool bPartialRefreshFromProjWin = false;
    double dfULX = 0;
    double dfULY = 0;
    double dfLRX = 0;
    double dfLRY = 0;
    bool bPartialRefreshFromSourceExtent = false;
    CPLStringList aosSources;

    /* -------------------------------------------------------------------- */
    /*      Parse command line.                                              */
    /* -------------------------------------------------------------------- */
    for (int iArg = 1; iArg < nArgc; iArg++)
    {
        if (EQUAL(papszArgv[iArg], "--utility_version"))
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(papszArgv);
            return 0;
        }
        else if (EQUAL(papszArgv[iArg], "--help"))
        {
            Usage(false);
        }
        else if (EQUAL(papszArgv[iArg], "-r"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            osResampling = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-ro"))
        {
            bReadOnly = true;
        }
        else if (EQUAL(papszArgv[iArg], "-clean"))
        {
            bClean = true;
        }
        else if (EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet"))
        {
            pfnProgress = GDALDummyProgress;
        }
        else if (EQUAL(papszArgv[iArg], "-b"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            const char *pszBand = papszArgv[iArg + 1];
            const int nBand = atoi(pszBand);
            if (nBand < 1)
            {
                Usage(true, CPLSPrintf("Unrecognizable band number (%s).\n",
                                       papszArgv[iArg + 1]));
            }
            iArg++;

            nBandCount++;
            panBandList = static_cast<int *>(
                CPLRealloc(panBandList, sizeof(int) * nBandCount));
            panBandList[nBandCount - 1] = nBand;
        }
        else if (EQUAL(papszArgv[iArg], "-oo"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszOpenOptions =
                CSLAddString(papszOpenOptions, papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-minsize"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nMinSize = atoi(papszArgv[++iArg]);
            bMinSizeSpecified = true;
        }
        else if (EQUAL(papszArgv[iArg],
                       "--partial-refresh-from-source-timestamp"))
        {
            bPartialRefreshFromSourceTimestamp = true;
        }
        else if (EQUAL(papszArgv[iArg], "--partial-refresh-from-projwin"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            bPartialRefreshFromProjWin = true;
            dfULX = CPLAtof(papszArgv[++iArg]);
            dfULY = CPLAtof(papszArgv[++iArg]);
            dfLRX = CPLAtof(papszArgv[++iArg]);
            dfLRY = CPLAtof(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "--partial-refresh-from-source-extent"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            bPartialRefreshFromSourceExtent = true;
            aosSources = CSLTokenizeString2(papszArgv[++iArg], ",", 0);
        }
        else if (papszArgv[iArg][0] == '-')
        {
            Usage(true,
                  CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        }
        else if (pszFilename == nullptr)
        {
            pszFilename = papszArgv[iArg];
        }
        else if (atoi(papszArgv[iArg]) > 0)
        {
            anLevels.push_back(atoi(papszArgv[iArg]));
            if (anLevels.back() == 1)
            {
                printf(
                    "Warning: Overview with subsampling factor of 1 requested. "
                    "This will copy the full resolution dataset in the "
                    "overview!\n");
            }
        }
        else
        {
            Usage(true, "Too many command options.");
        }
    }

    if (pszFilename == nullptr)
        Usage(true, "No datasource specified.");

    if (((bClean) ? 1 : 0) + ((bPartialRefreshFromSourceTimestamp) ? 1 : 0) +
            ((bPartialRefreshFromProjWin) ? 1 : 0) +
            ((bPartialRefreshFromSourceExtent) ? 1 : 0) >
        1)
    {
        Usage(true, "Mutually exclusive options used");
    }

    /* -------------------------------------------------------------------- */
    /*      Open data file.                                                 */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDataset = nullptr;
    if (!bReadOnly)
    {
        CPLPushErrorHandler(GDALAddoErrorHandler);
        CPLSetCurrentErrorHandlerCatchDebug(FALSE);
        hDataset = GDALOpenEx(pszFilename, GDAL_OF_RASTER | GDAL_OF_UPDATE,
                              nullptr, papszOpenOptions, nullptr);
        CPLPopErrorHandler();
        if (hDataset != nullptr)
        {
            for (size_t i = 0; i < aoErrors.size(); i++)
            {
                CPLError(aoErrors[i].m_eErr, aoErrors[i].m_errNum, "%s",
                         aoErrors[i].m_osMsg.c_str());
            }
        }
    }

    if (hDataset == nullptr)
        hDataset =
            GDALOpenEx(pszFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                       nullptr, papszOpenOptions, nullptr);

    CSLDestroy(papszOpenOptions);
    papszOpenOptions = nullptr;

    if (hDataset == nullptr)
        exit(2);

    if (!bClean && osResampling.empty())
    {
        auto poDS = GDALDataset::FromHandle(hDataset);
        if (poDS->GetRasterCount() > 0)
        {
            auto poBand = poDS->GetRasterBand(1);
            if (poBand->GetOverviewCount() > 0)
            {
                const char *pszResampling =
                    poBand->GetOverview(0)->GetMetadataItem("RESAMPLING");
                if (pszResampling)
                {
                    osResampling = pszResampling;
                    if (pfnProgress == GDALDummyProgress)
                        CPLDebug("GDAL",
                                 "Reusing resampling method %s from existing "
                                 "overview",
                                 pszResampling);
                    else
                        printf("Info: reusing resampling method %s from "
                               "existing overview.\n",
                               pszResampling);
                }
            }
        }
        if (osResampling.empty())
            osResampling = "nearest";
    }

    /* -------------------------------------------------------------------- */
    /*      Clean overviews.                                                */
    /* -------------------------------------------------------------------- */
    if (bClean)
    {
        if (GDALBuildOverviews(hDataset, "NONE", 0, nullptr, 0, nullptr,
                               pfnProgress, pProgressArg) != CE_None)
        {
            fprintf(stderr, "Cleaning overviews failed.\n");
            nResultStatus = 200;
        }
    }
    else if (bPartialRefreshFromSourceTimestamp)
    {
        if (!PartialRefreshFromSourceTimestamp(
                GDALDataset::FromHandle(hDataset), osResampling.c_str(),
                static_cast<int>(anLevels.size()), anLevels.data(), nBandCount,
                panBandList, bMinSizeSpecified, nMinSize, pfnProgress,
                pProgressArg))
        {
            nResultStatus = 1;
        }
    }
    else if (bPartialRefreshFromProjWin)
    {
        if (!PartialRefreshFromProjWin(
                GDALDataset::FromHandle(hDataset), dfULX, dfULY, dfLRX, dfLRY,
                osResampling.c_str(), static_cast<int>(anLevels.size()),
                anLevels.data(), nBandCount, panBandList, bMinSizeSpecified,
                nMinSize, pfnProgress, pProgressArg))
        {
            nResultStatus = 1;
        }
    }
    else if (bPartialRefreshFromSourceExtent)
    {
        if (!PartialRefreshFromSourceExtent(
                GDALDataset::FromHandle(hDataset), aosSources,
                osResampling.c_str(), static_cast<int>(anLevels.size()),
                anLevels.data(), nBandCount, panBandList, bMinSizeSpecified,
                nMinSize, pfnProgress, pProgressArg))
        {
            nResultStatus = 1;
        }
    }
    else
    {
        /* --------------------------------------------------------------------
         */
        /*      Generate overviews. */
        /* --------------------------------------------------------------------
         */

        // If no levels are specified, reuse the potentially existing ones.
        if (anLevels.empty())
        {
            auto poDS = GDALDataset::FromHandle(hDataset);
            if (poDS->GetRasterCount() > 0)
            {
                auto poBand = poDS->GetRasterBand(1);
                const int nExistingCount = poBand->GetOverviewCount();
                if (nExistingCount > 0)
                {
                    for (int iOvr = 0; iOvr < nExistingCount; ++iOvr)
                    {
                        auto poOverview = poBand->GetOverview(iOvr);
                        if (poOverview)
                        {
                            const int nOvFactor = GDALComputeOvFactor(
                                poOverview->GetXSize(), poBand->GetXSize(),
                                poOverview->GetYSize(), poBand->GetYSize());
                            anLevels.push_back(nOvFactor);
                        }
                    }
                }
            }
        }

        if (anLevels.empty())
        {
            const int nXSize = GDALGetRasterXSize(hDataset);
            const int nYSize = GDALGetRasterYSize(hDataset);
            int nOvrFactor = 1;
            while (DIV_ROUND_UP(nXSize, nOvrFactor) > nMinSize ||
                   DIV_ROUND_UP(nYSize, nOvrFactor) > nMinSize)
            {
                nOvrFactor *= 2;
                anLevels.push_back(nOvrFactor);
            }
        }

        // Only HFA supports selected layers
        if (nBandCount > 0)
            CPLSetConfigOption("USE_RRD", "YES");

        if (!anLevels.empty() &&
            GDALBuildOverviews(hDataset, osResampling.c_str(),
                               static_cast<int>(anLevels.size()),
                               anLevels.data(), nBandCount, panBandList,
                               pfnProgress, pProgressArg) != CE_None)
        {
            fprintf(stderr, "Overview building failed.\n");
            nResultStatus = 100;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    if (GDALClose(hDataset) != CE_None)
    {
        if (nResultStatus == 0)
            nResultStatus = 1;
    }

    CSLDestroy(papszArgv);
    CPLFree(panBandList);
    GDALDestroyDriverManager();

    return nResultStatus;
}
MAIN_END

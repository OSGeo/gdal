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
#include "vrt_priv.h"
#include "gdalargumentparser.h"

#include <algorithm>

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

    std::vector<GTISourceDesc> regions;

    double dfTotalPixels = 0;

    if (dynamic_cast<VRTDataset *>(poDS))
    {
        auto poVRTBand =
            dynamic_cast<VRTSourcedRasterBand *>(poDS->GetRasterBand(1));
        if (!poVRTBand)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Band is not a VRTSourcedRasterBand");
            return false;
        }

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
                        poSource->GetDstWindow(dfXOff, dfYOff, dfXSize,
                                               dfYSize);
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
                        GTISourceDesc region;
                        region.osFilename = poSource->GetSourceDatasetName();
                        region.nDstXOff = nXOff;
                        region.nDstYOff = nYOff;
                        region.nDstXSize = nXSize;
                        region.nDstYSize = nYSize;
                        regions.push_back(std::move(region));
                    }
                }
            }
        }
    }
    else if (auto poGTIDS = GDALDatasetCastToGTIDataset(poDS))
    {
        regions = GTIGetSourcesMoreRecentThan(poGTIDS, sStatVRTOvr.st_mtime);
        for (const auto &region : regions)
        {
            dfTotalPixels +=
                static_cast<double>(region.nDstXSize) * region.nDstYSize;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "--partial-refresh-from-source-timestamp only works on a VRT "
                 "dataset");
        return false;
    }

    if (!regions.empty())
    {
        double dfCurPixels = 0;
        for (const auto &region : regions)
        {
            if (pfnProgress == GDALDummyProgress)
            {
                CPLDebug("GDAL", "Refresh from source %s",
                         region.osFilename.c_str());
            }
            else
            {
                printf("Refresh from source %s.\n", region.osFilename.c_str());
            }
            double dfNextCurPixels =
                dfCurPixels +
                static_cast<double>(region.nDstXSize) * region.nDstYSize;
            void *pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixels, dfNextCurPixels / dfTotalPixels,
                pfnProgress, pProgressArg);
            bool bRet = PartialRefresh(
                poDS, anOvrIndices, nBandCount, panBandList, pszResampling,
                region.nDstXOff, region.nDstYOff, region.nDstXSize,
                region.nDstYSize, GDALScaledProgress, pScaledProgress);
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

MAIN_START(nArgc, papszArgv)

{
    EarlySetConfigOptions(nArgc, papszArgv);
    GDALAllRegister();

    nArgc = GDALGeneralCmdLineProcessor(nArgc, &papszArgv, 0);
    if (nArgc < 1)
        exit(-nArgc);
    CPLStringList aosArgv;
    aosArgv.Assign(papszArgv, /* bAssign = */ true);

    GDALArgumentParser argParser(aosArgv[0], /* bForBinary=*/true);

    argParser.add_description(_("Builds or rebuilds overview images."));

    const char *pszEpilog = _(
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
        "            --config INTERLEAVE_OVERVIEW PIXEL -ro abc.tif\n"
        "\n"
        "For more details, consult https://gdal.org/programs/gdaladdo.html");
    argParser.add_epilog(pszEpilog);

    std::string osResampling;
    argParser.add_argument("-r")
        .store_into(osResampling)
        .metavar("nearest|average|rms|gauss|cubic|cubicspline|lanczos|average_"
                 "magphase|mode")
        .help(_("Select a resampling algorithm."));

    bool bReadOnly = false;
    argParser.add_argument("-ro").store_into(bReadOnly).help(
        _("Open the dataset in read-only mode, in order to generate external "
          "overview."));

    bool bQuiet = false;
    argParser.add_quiet_argument(&bQuiet);

    std::vector<int> anBandList;
    argParser.add_argument("-b")
        .append()
        .metavar("<band>")
        .action(
            [&anBandList](const std::string &s)
            {
                const int nBand = atoi(s.c_str());
                if (nBand < 1)
                {
                    throw std::invalid_argument(CPLSPrintf(
                        "Unrecognizable band number (%s).", s.c_str()));
                }
                anBandList.push_back(nBand);
            })
        .help(_("Select input band(s) for overview generation."));

    CPLStringList aosOpenOptions;
    argParser.add_argument("-oo")
        .append()
        .metavar("<NAME=VALUE>")
        .action([&aosOpenOptions](const std::string &s)
                { aosOpenOptions.AddString(s.c_str()); })
        .help(_("Dataset open option (format-specific)."));

    int nMinSize = 256;
    argParser.add_argument("-minsize")
        .default_value(nMinSize)
        .metavar("<val>")
        .store_into(nMinSize)
        .help(_("Maximum width or height of the smallest overview level."));

    bool bClean = false;
    bool bPartialRefreshFromSourceTimestamp = false;
    std::string osPartialRefreshFromSourceExtent;

    {
        auto &group = argParser.add_mutually_exclusive_group();
        group.add_argument("-clean").store_into(bClean).help(
            _("Remove all overviews."));

        group.add_argument("--partial-refresh-from-source-timestamp")
            .store_into(bPartialRefreshFromSourceTimestamp)
            .help(_("Performs a partial refresh of existing overviews, when "
                    "<filename> is a VRT file with an external overview."));

        group.add_argument("--partial-refresh-from-projwin")
            .metavar("<ulx> <uly> <lrx> <lry>")
            .nargs(4)
            .scan<'g', double>()
            .help(
                _("Performs a partial refresh of existing overviews, in the "
                  "region of interest specified by georeference coordinates."));

        group.add_argument("--partial-refresh-from-source-extent")
            .metavar("<filename1>[,<filenameN>]...")
            .store_into(osPartialRefreshFromSourceExtent)
            .help(
                _("Performs a partial refresh of existing overviews, in the "
                  "region of interest specified by one or several filename."));
    }

    std::string osFilename;
    argParser.add_argument("filename")
        .store_into(osFilename)
        .help(_("The file to build overviews for (or whose overviews must be "
                "removed)."));

    argParser.add_argument("level").remaining().metavar("<level>").help(
        _("A list of integral overview levels to build."));

    try
    {
        argParser.parse_args(aosArgv);
    }
    catch (const std::exception &err)
    {
        argParser.display_error_and_usage(err);
        std::exit(1);
    }

    std::vector<int> anLevels;
    auto levels = argParser.present<std::vector<std::string>>("level");
    if (levels)
    {
        for (const auto &level : *levels)
        {
            anLevels.push_back(atoi(level.c_str()));
            if (anLevels.back() == 1)
            {
                printf(
                    "Warning: Overview with subsampling factor of 1 requested. "
                    "This will copy the full resolution dataset in the "
                    "overview!\n");
            }
        }
    }

    GDALProgressFunc pfnProgress =
        bQuiet ? GDALDummyProgress : GDALTermProgress;
    const bool bMinSizeSpecified = argParser.is_used("-minsize");

    CPLStringList aosSources;
    if (!osPartialRefreshFromSourceExtent.empty())
    {
        aosSources = CSLTokenizeString2(
            osPartialRefreshFromSourceExtent.c_str(), ",", 0);
    }

    bool bPartialRefreshFromProjWin = false;
    double dfULX = 0;
    double dfULY = 0;
    double dfLRX = 0;
    double dfLRY = 0;
    if (auto oProjWin = argParser.present<std::vector<double>>(
            "--partial-refresh-from-projwin"))
    {
        bPartialRefreshFromProjWin = true;
        dfULX = (*oProjWin)[0];
        dfULY = (*oProjWin)[1];
        dfLRX = (*oProjWin)[2];
        dfLRY = (*oProjWin)[3];
    }

    /* -------------------------------------------------------------------- */
    /*      Open data file.                                                 */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDataset = nullptr;
    if (!bReadOnly)
    {
        CPLPushErrorHandler(GDALAddoErrorHandler);
        CPLSetCurrentErrorHandlerCatchDebug(FALSE);
        hDataset =
            GDALOpenEx(osFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_UPDATE,
                       nullptr, aosOpenOptions.List(), nullptr);
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
        hDataset = GDALOpenEx(osFilename.c_str(),
                              GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, nullptr,
                              aosOpenOptions.List(), nullptr);
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
    int nResultStatus = 0;
    void *pProgressArg = nullptr;
    const int nBandCount = static_cast<int>(anBandList.size());
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
                anBandList.data(), bMinSizeSpecified, nMinSize, pfnProgress,
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
                anLevels.data(), nBandCount, anBandList.data(),
                bMinSizeSpecified, nMinSize, pfnProgress, pProgressArg))
        {
            nResultStatus = 1;
        }
    }
    else if (!aosSources.empty())
    {
        if (!PartialRefreshFromSourceExtent(
                GDALDataset::FromHandle(hDataset), aosSources,
                osResampling.c_str(), static_cast<int>(anLevels.size()),
                anLevels.data(), nBandCount, anBandList.data(),
                bMinSizeSpecified, nMinSize, pfnProgress, pProgressArg))
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
                               anLevels.data(), nBandCount, anBandList.data(),
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

    GDALDestroyDriverManager();

    return nResultStatus;
}

MAIN_END

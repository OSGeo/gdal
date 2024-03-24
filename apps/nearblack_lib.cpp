/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Convert nearly black or nearly white border to exact black/white.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2006, MapShots Inc (www.mapshots.com)
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "commonutils.h"
#include "gdalargumentparser.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <memory>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_priv.h"

#include "nearblack_lib.h"

static void ProcessLine(GByte *pabyLine, GByte *pabyMask, int iStart, int iEnd,
                        int nSrcBands, int nDstBands, int nNearDist,
                        int nMaxNonBlack, bool bNearWhite,
                        const Colors &oColors, int *panLastLineCounts,
                        bool bDoHorizontalCheck, bool bDoVerticalCheck,
                        bool bBottomUp, int iLineFromTopOrBottom);

/************************************************************************/
/*                            GDALNearblack()                           */
/************************************************************************/

/* clang-format off */
/**
 * Convert nearly black/white borders to exact value.
 *
 * This is the equivalent of the
 * <a href="/programs/nearblack.html">nearblack</a> utility.
 *
 * GDALNearblackOptions* must be allocated and freed with
 * GDALNearblackOptionsNew() and GDALNearblackOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * In-place update (i.e. hDstDS == hSrcDataset) is possible for formats that
 * support it, and if the dataset is opened in update mode.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL. Might be equal to hSrcDataset.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALNearblackOptionsNew()
 * or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose(), or hDstDS when it is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */
/* clang-format on */

GDALDatasetH CPL_DLL GDALNearblack(const char *pszDest, GDALDatasetH hDstDS,
                                   GDALDatasetH hSrcDataset,
                                   const GDALNearblackOptions *psOptionsIn,
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

    // to keep in that scope
    std::unique_ptr<GDALNearblackOptions> psTmpOptions;
    const GDALNearblackOptions *psOptions = psOptionsIn;
    if (!psOptionsIn)
    {
        psTmpOptions = std::make_unique<GDALNearblackOptions>();
        psOptions = psTmpOptions.get();
    }

    const bool bCloseOutDSOnError = hDstDS == nullptr;
    if (pszDest == nullptr)
        pszDest = GDALGetDescription(hDstDS);

    const int nXSize = GDALGetRasterXSize(hSrcDataset);
    const int nYSize = GDALGetRasterYSize(hSrcDataset);
    int nBands = GDALGetRasterCount(hSrcDataset);
    int nDstBands = nBands;

    const bool bNearWhite = psOptions->bNearWhite;
    const bool bSetAlpha = psOptions->bSetAlpha;
    bool bSetMask = psOptions->bSetMask;
    Colors oColors = psOptions->oColors;

    /* -------------------------------------------------------------------- */
    /*      Do we need to create output file?                               */
    /* -------------------------------------------------------------------- */

    if (hDstDS == nullptr)
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

        GDALDriverH hDriver = GDALGetDriverByName(osFormat);
        if (hDriver == nullptr)
        {
            return nullptr;
        }

        if (bSetAlpha)
        {
            // TODO(winkey): There should be a way to preserve alpha
            // band data not in the collar.
            if (nBands == 4)
                nBands--;
            else
                nDstBands++;
        }

        if (bSetMask)
        {
            if (nBands == 4)
            {
                nDstBands = 3;
                nBands = 3;
            }
        }

        hDstDS = GDALCreate(hDriver, pszDest, nXSize, nYSize, nDstBands,
                            GDT_Byte, psOptions->aosCreationOptions.List());
        if (hDstDS == nullptr)
        {
            return nullptr;
        }

        double adfGeoTransform[6] = {};

        if (GDALGetGeoTransform(hSrcDataset, adfGeoTransform) == CE_None)
        {
            GDALSetGeoTransform(hDstDS, adfGeoTransform);
            GDALSetProjection(hDstDS, GDALGetProjectionRef(hSrcDataset));
        }
    }
    else
    {
        if (!psOptions->aosCreationOptions.empty())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Warning: creation options are ignored when writing to "
                     "an existing file.");
        }

        /***** check the input and output datasets are the same size *****/
        if (GDALGetRasterXSize(hDstDS) != nXSize ||
            GDALGetRasterYSize(hDstDS) != nYSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The dimensions of the output dataset don't match "
                     "the dimensions of the input dataset.");
            return nullptr;
        }

        if (bSetAlpha)
        {
            if (nBands != 4 &&
                (nBands < 2 ||
                 GDALGetRasterColorInterpretation(
                     GDALGetRasterBand(hDstDS, nBands)) != GCI_AlphaBand))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Last band is not an alpha band.");
                return nullptr;
            }

            nBands--;
        }

        if (bSetMask)
        {
            if (nBands == 4)
            {
                nDstBands = 3;
                nBands = 3;
            }
        }
    }

    /***** set a color if there are no colors set? *****/

    if (oColors.empty())
    {
        Color oColor;

        /***** loop over the bands to get the right number of values *****/
        for (int iBand = 0; iBand < nBands; iBand++)
        {
            // black or white?
            oColor.push_back(bNearWhite ? 255 : 0);
        }

        /***** add the color to the colors *****/
        oColors.push_back(oColor);
        assert(!oColors.empty());
    }

    /***** does the number of bands match the number of color values? *****/

    if (static_cast<int>(oColors.front().size()) != nBands)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "-color args must have the same number of values as "
                 "the non alpha input band count.\n");
        if (bCloseOutDSOnError)
            GDALClose(hDstDS);
        return nullptr;
    }

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        GDALRasterBandH hBand = GDALGetRasterBand(hSrcDataset, iBand + 1);
        if (GDALGetRasterDataType(hBand) != GDT_Byte)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Band %d is not of type GDT_Byte. "
                     "It can lead to unexpected results.",
                     iBand + 1);
        }
        if (GDALGetRasterColorTable(hBand) != nullptr)
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Band %d has a color table, which is ignored by nearblack. "
                "It can lead to unexpected results.",
                iBand + 1);
        }
    }

    GDALRasterBandH hMaskBand = nullptr;

    if (bSetMask)
    {
        // If there isn't already a mask band on the output file create one.
        if (GMF_PER_DATASET != GDALGetMaskFlags(GDALGetRasterBand(hDstDS, 1)))
        {

            if (CE_None != GDALCreateDatasetMaskBand(hDstDS, GMF_PER_DATASET))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create mask band on output DS");
                bSetMask = false;
            }
        }

        if (bSetMask)
        {
            hMaskBand = GDALGetMaskBand(GDALGetRasterBand(hDstDS, 1));
        }
    }

    bool bRet;
    if (psOptions->bFloodFill)
    {
        bRet = GDALNearblackFloodFill(psOptions, hSrcDataset, hDstDS, hMaskBand,
                                      nBands, nDstBands, bSetMask, oColors);
    }
    else
    {
        bRet = GDALNearblackTwoPassesAlgorithm(psOptions, hSrcDataset, hDstDS,
                                               hMaskBand, nBands, nDstBands,
                                               bSetMask, oColors);
    }
    if (!bRet)
    {
        if (bCloseOutDSOnError)
            GDALClose(hDstDS);
        hDstDS = nullptr;
    }

    return hDstDS;
}

/************************************************************************/
/*                   GDALNearblackTwoPassesAlgorithm()                  */
/*                                                                      */
/* Do a top-to-bottom pass, followed by a bottom-to-top one.            */
/************************************************************************/

bool GDALNearblackTwoPassesAlgorithm(const GDALNearblackOptions *psOptions,
                                     GDALDatasetH hSrcDataset,
                                     GDALDatasetH hDstDS,
                                     GDALRasterBandH hMaskBand, int nBands,
                                     int nDstBands, bool bSetMask,
                                     const Colors &oColors)
{
    const int nXSize = GDALGetRasterXSize(hSrcDataset);
    const int nYSize = GDALGetRasterYSize(hSrcDataset);

    const int nMaxNonBlack = psOptions->nMaxNonBlack;
    const int nNearDist = psOptions->nNearDist;
    const bool bNearWhite = psOptions->bNearWhite;
    const bool bSetAlpha = psOptions->bSetAlpha;

    /* -------------------------------------------------------------------- */
    /*      Allocate a line buffer.                                         */
    /* -------------------------------------------------------------------- */

    std::vector<GByte> abyLine(static_cast<size_t>(nXSize) * nDstBands);
    GByte *pabyLine = abyLine.data();

    std::vector<GByte> abyMask;
    GByte *pabyMask = nullptr;
    if (bSetMask)
    {
        abyMask.resize(nXSize);
        pabyMask = abyMask.data();
    }

    std::vector<int> anLastLineCounts(nXSize);
    int *panLastLineCounts = anLastLineCounts.data();

    /* -------------------------------------------------------------------- */
    /*      Processing data one line at a time.                             */
    /* -------------------------------------------------------------------- */
    int iLine;

    for (iLine = 0; iLine < nYSize; iLine++)
    {
        CPLErr eErr = GDALDatasetRasterIO(
            hSrcDataset, GF_Read, 0, iLine, nXSize, 1, pabyLine, nXSize, 1,
            GDT_Byte, nBands, nullptr, nDstBands, nXSize * nDstBands, 1);
        if (eErr != CE_None)
        {
            return false;
        }

        if (bSetAlpha)
        {
            for (int iCol = 0; iCol < nXSize; iCol++)
            {
                pabyLine[iCol * nDstBands + nDstBands - 1] = 255;
            }
        }

        if (bSetMask)
        {
            for (int iCol = 0; iCol < nXSize; iCol++)
            {
                pabyMask[iCol] = 255;
            }
        }

        ProcessLine(pabyLine, pabyMask, 0, nXSize - 1, nBands, nDstBands,
                    nNearDist, nMaxNonBlack, bNearWhite, oColors,
                    panLastLineCounts,
                    true,   // bDoHorizontalCheck
                    true,   // bDoVerticalCheck
                    false,  // bBottomUp
                    iLine);
        ProcessLine(pabyLine, pabyMask, nXSize - 1, 0, nBands, nDstBands,
                    nNearDist, nMaxNonBlack, bNearWhite, oColors,
                    panLastLineCounts,
                    true,   // bDoHorizontalCheck
                    false,  // bDoVerticalCheck
                    false,  // bBottomUp
                    iLine);

        eErr = GDALDatasetRasterIO(hDstDS, GF_Write, 0, iLine, nXSize, 1,
                                   pabyLine, nXSize, 1, GDT_Byte, nDstBands,
                                   nullptr, nDstBands, nXSize * nDstBands, 1);

        if (eErr != CE_None)
        {
            return false;
        }

        /***** write out the mask band line *****/

        if (bSetMask)
        {
            eErr = GDALRasterIO(hMaskBand, GF_Write, 0, iLine, nXSize, 1,
                                pabyMask, nXSize, 1, GDT_Byte, 0, 0);
            if (eErr != CE_None)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "ERROR writing out line to mask band.");
                return false;
            }
        }

        if (!(psOptions->pfnProgress(
                0.5 * ((iLine + 1) / static_cast<double>(nYSize)), nullptr,
                psOptions->pProgressData)))
        {
            return false;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Now process from the bottom back up                            .*/
    /* -------------------------------------------------------------------- */
    memset(panLastLineCounts, 0, sizeof(int) * nXSize);

    for (iLine = nYSize - 1; hDstDS != nullptr && iLine >= 0; iLine--)
    {
        CPLErr eErr = GDALDatasetRasterIO(
            hDstDS, GF_Read, 0, iLine, nXSize, 1, pabyLine, nXSize, 1, GDT_Byte,
            nDstBands, nullptr, nDstBands, nXSize * nDstBands, 1);
        if (eErr != CE_None)
        {
            return false;
        }

        /***** read the mask band line back in *****/

        if (bSetMask)
        {
            eErr = GDALRasterIO(hMaskBand, GF_Read, 0, iLine, nXSize, 1,
                                pabyMask, nXSize, 1, GDT_Byte, 0, 0);
            if (eErr != CE_None)
            {
                return false;
            }
        }

        ProcessLine(pabyLine, pabyMask, 0, nXSize - 1, nBands, nDstBands,
                    nNearDist, nMaxNonBlack, bNearWhite, oColors,
                    panLastLineCounts,
                    true,  // bDoHorizontalCheck
                    true,  // bDoVerticalCheck
                    true,  // bBottomUp
                    nYSize - 1 - iLine);
        ProcessLine(pabyLine, pabyMask, nXSize - 1, 0, nBands, nDstBands,
                    nNearDist, nMaxNonBlack, bNearWhite, oColors,
                    panLastLineCounts,
                    true,   // bDoHorizontalCheck
                    false,  // bDoVerticalCheck
                    true,   // bBottomUp
                    nYSize - 1 - iLine);

        eErr = GDALDatasetRasterIO(hDstDS, GF_Write, 0, iLine, nXSize, 1,
                                   pabyLine, nXSize, 1, GDT_Byte, nDstBands,
                                   nullptr, nDstBands, nXSize * nDstBands, 1);
        if (eErr != CE_None)
        {
            return false;
        }

        /***** write out the mask band line *****/

        if (bSetMask)
        {
            eErr = GDALRasterIO(hMaskBand, GF_Write, 0, iLine, nXSize, 1,
                                pabyMask, nXSize, 1, GDT_Byte, 0, 0);
            if (eErr != CE_None)
            {
                return false;
            }
        }

        if (!(psOptions->pfnProgress(0.5 + 0.5 * (nYSize - iLine) /
                                               static_cast<double>(nYSize),
                                     nullptr, psOptions->pProgressData)))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                            ProcessLine()                             */
/*                                                                      */
/*      Process a single scanline of image data.                        */
/************************************************************************/

static void ProcessLine(GByte *pabyLine, GByte *pabyMask, int iStart, int iEnd,
                        int nSrcBands, int nDstBands, int nNearDist,
                        int nMaxNonBlack, bool bNearWhite,
                        const Colors &oColors, int *panLastLineCounts,
                        bool bDoHorizontalCheck, bool bDoVerticalCheck,
                        bool bBottomUp, int iLineFromTopOrBottom)
{
    const GByte nReplacevalue = bNearWhite ? 255 : 0;

    /* -------------------------------------------------------------------- */
    /*      Vertical checking.                                              */
    /* -------------------------------------------------------------------- */

    if (bDoVerticalCheck)
    {
        const int nXSize = std::max(iStart + 1, iEnd + 1);

        for (int i = 0; i < nXSize; i++)
        {
            // are we already terminated for this column?
            if (panLastLineCounts[i] > nMaxNonBlack)
                continue;

            /***** is the pixel valid data? ****/

            bool bIsNonBlack = false;

            /***** loop over the colors *****/

            for (int iColor = 0; iColor < static_cast<int>(oColors.size());
                 iColor++)
            {

                const Color &oColor = oColors[iColor];

                bIsNonBlack = false;

                /***** loop over the bands *****/

                for (int iBand = 0; iBand < nSrcBands; iBand++)
                {
                    const int nPix = pabyLine[i * nDstBands + iBand];

                    if (oColor[iBand] - nPix > nNearDist ||
                        nPix > nNearDist + oColor[iBand])
                    {
                        bIsNonBlack = true;
                        break;
                    }
                }

                if (!bIsNonBlack)
                    break;
            }

            if (bIsNonBlack)
            {
                panLastLineCounts[i]++;

                if (panLastLineCounts[i] > nMaxNonBlack)
                    continue;

                if (iLineFromTopOrBottom == 0 && nMaxNonBlack > 0)
                {
                    // if there's a valid value just at the top or bottom
                    // of the raster, then ignore the nMaxNonBlack setting
                    panLastLineCounts[i] = nMaxNonBlack + 1;
                    continue;
                }
            }
            // else
            //   panLastLineCounts[i] = 0; // not sure this even makes sense

            /***** replace the pixel values *****/
            for (int iBand = 0; iBand < nSrcBands; iBand++)
                pabyLine[i * nDstBands + iBand] = nReplacevalue;

            /***** alpha *****/
            if (nDstBands > nSrcBands)
                pabyLine[i * nDstBands + nDstBands - 1] = 0;

            /***** mask *****/
            if (pabyMask != nullptr)
                pabyMask[i] = 0;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Horizontal Checking.                                            */
    /* -------------------------------------------------------------------- */

    if (bDoHorizontalCheck)
    {
        int nNonBlackPixels = 0;

        /***** on a bottom up pass assume nMaxNonBlack is 0 *****/

        if (bBottomUp)
            nMaxNonBlack = 0;

        const int iDir = iStart < iEnd ? 1 : -1;

        bool bDoTest = TRUE;

        for (int i = iStart; i != iEnd; i += iDir)
        {
            /***** not seen any valid data? *****/

            if (bDoTest)
            {
                /***** is the pixel valid data? ****/

                bool bIsNonBlack = false;

                /***** loop over the colors *****/

                for (int iColor = 0; iColor < static_cast<int>(oColors.size());
                     iColor++)
                {

                    const Color &oColor = oColors[iColor];

                    bIsNonBlack = false;

                    /***** loop over the bands *****/

                    for (int iBand = 0; iBand < nSrcBands; iBand++)
                    {
                        const int nPix = pabyLine[i * nDstBands + iBand];

                        if (oColor[iBand] - nPix > nNearDist ||
                            nPix > nNearDist + oColor[iBand])
                        {
                            bIsNonBlack = true;
                            break;
                        }
                    }

                    if (bIsNonBlack == false)
                        break;
                }

                if (bIsNonBlack)
                {
                    /***** use nNonBlackPixels in grey areas  *****/
                    /***** from the vertical pass's grey areas ****/

                    if (panLastLineCounts[i] <= nMaxNonBlack)
                        nNonBlackPixels = panLastLineCounts[i];
                    else
                        nNonBlackPixels++;
                }

                if (nNonBlackPixels > nMaxNonBlack)
                {
                    bDoTest = false;
                    continue;
                }

                if (bIsNonBlack && nMaxNonBlack > 0 && i == iStart)
                {
                    // if there's a valid value just at the left or right
                    // of the raster, then ignore the nMaxNonBlack setting
                    bDoTest = false;
                    continue;
                }

                /***** replace the pixel values *****/

                for (int iBand = 0; iBand < nSrcBands; iBand++)
                    pabyLine[i * nDstBands + iBand] = nReplacevalue;

                /***** alpha *****/

                if (nDstBands > nSrcBands)
                    pabyLine[i * nDstBands + nDstBands - 1] = 0;

                /***** mask *****/

                if (pabyMask != nullptr)
                    pabyMask[i] = 0;
            }

            /***** seen valid data but test if the *****/
            /***** vertical pass saw any non valid data *****/

            else if (panLastLineCounts[i] == 0)
            {
                bDoTest = true;
                nNonBlackPixels = 0;
            }
        }
    }
}

/************************************************************************/
/*                            IsInt()                                   */
/************************************************************************/

static bool IsInt(const char *pszArg)
{
    if (pszArg[0] == '-')
        pszArg++;

    if (*pszArg == '\0')
        return false;

    while (*pszArg != '\0')
    {
        if (*pszArg < '0' || *pszArg > '9')
            return false;
        pszArg++;
    }

    return true;
}

/************************************************************************/
/*                    GDALNearblackOptionsGetParser()                   */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALNearblackOptionsGetParser(GDALNearblackOptions *psOptions,
                              GDALNearblackOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "nearblack", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(
        _("Convert nearly black/white borders to black."));

    argParser->add_epilog(_(
        "For more details, consult https://gdal.org/programs/nearblack.html"));

    argParser->add_output_format_argument(psOptions->osFormat);

    // Written that way so that in library mode, users can still use the -q
    // switch, even if it has no effect
    argParser->add_quiet_argument(
        psOptionsForBinary ? &(psOptionsForBinary->bQuiet) : nullptr);

    argParser->add_creation_options_argument(psOptions->aosCreationOptions);

    auto &oOutputFileArg =
        argParser->add_argument("-o")
            .metavar("<output_file>")
            .help(_("The name of the output file to be created."));
    if (psOptionsForBinary)
        oOutputFileArg.store_into(psOptionsForBinary->osOutFile);

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-white")
            .store_into(psOptions->bNearWhite)
            .help(_("Search for nearly white (255) pixels instead of nearly "
                    "black pixels."));

        group.add_argument("-color")
            .append()
            .metavar("<c1,c2,c3...cn>")
            .action(
                [psOptions](const std::string &s)
                {
                    Color oColor;

                    /***** tokenize the arg on , *****/

                    const CPLStringList aosTokens(
                        CSLTokenizeString2(s.c_str(), ",", 0));

                    /***** loop over the tokens *****/

                    for (int iToken = 0; iToken < aosTokens.size(); iToken++)
                    {

                        /***** ensure the token is an int and add it to the color *****/

                        if (IsInt(aosTokens[iToken]))
                        {
                            oColor.push_back(atoi(aosTokens[iToken]));
                        }
                        else
                        {
                            throw std::invalid_argument(
                                "Colors must be valid integers.");
                        }
                    }

                    /***** check if the number of bands is consistent *****/

                    if (!psOptions->oColors.empty() &&
                        psOptions->oColors.front().size() != oColor.size())
                    {
                        throw std::invalid_argument(
                            "all -color args must have the same number of "
                            "values.\n");
                    }

                    /***** add the color to the colors *****/

                    psOptions->oColors.push_back(oColor);
                })
            .help(_("Search for pixels near the specified color."));
    }

    argParser->add_argument("-nb")
        .store_into(psOptions->nMaxNonBlack)
        .metavar("<non_black_pixels>")
        .default_value(psOptions->nMaxNonBlack)
        .nargs(1)
        .help(_("Number of consecutive non-black pixels."));

    argParser->add_argument("-near")
        .store_into(psOptions->nNearDist)
        .metavar("<dist>")
        .default_value(psOptions->nNearDist)
        .nargs(1)
        .help(_("Select how far from black, white or custom colors the pixel "
                "values can be and still considered."));

    argParser->add_argument("-setalpha")
        .store_into(psOptions->bSetAlpha)
        .help(_("Adds an alpha band if needed."));

    argParser->add_argument("-setmask")
        .store_into(psOptions->bSetMask)
        .help(_("Adds a mask band to the output file if -o is used, or to the "
                "input file otherwise."));

    argParser->add_argument("-alg")
        .choices("floodfill", "twopasses")
        .metavar("floodfill|twopasses")
        .action([psOptions](const std::string &s)
                { psOptions->bFloodFill = EQUAL(s.c_str(), "floodfill"); })
        .help(_("Selects the algorithm to apply."));

    if (psOptionsForBinary)
    {
        argParser->add_argument("input_file")
            .metavar("<input_file>")
            .store_into(psOptionsForBinary->osInFile)
            .help(_("The input file. Any GDAL supported format, any number of "
                    "bands, normally 8bit Byte bands."));
    }

    return argParser;
}

/************************************************************************/
/*                      GDALNearblackGetParserUsage()                   */
/************************************************************************/

std::string GDALNearblackGetParserUsage()
{
    try
    {
        GDALNearblackOptions sOptions;
        GDALNearblackOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALNearblackOptionsGetParser(&sOptions, &sOptionsForBinary);
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
/*                           GDALNearblackOptionsNew()                  */
/************************************************************************/

/**
 * Allocates a GDALNearblackOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/nearblack.html">nearblack</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALNearblackOptionsForBinaryNew() prior to this
 * function. Will be filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALNearblackOptions struct. Must be freed
 * with GDALNearblackOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALNearblackOptions *
GDALNearblackOptionsNew(char **papszArgv,
                        GDALNearblackOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALNearblackOptions>();

    try
    {

        auto argParser =
            GDALNearblackOptionsGetParser(psOptions.get(), psOptionsForBinary);

        argParser->parse_args_without_binary_name(papszArgv);

        return psOptions.release();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", err.what());
        return nullptr;
    }
}

/************************************************************************/
/*                       GDALNearblackOptionsFree()                     */
/************************************************************************/

/**
 * Frees the GDALNearblackOptions struct.
 *
 * @param psOptions the options struct for GDALNearblack().
 *
 * @since GDAL 2.1
 */

void GDALNearblackOptionsFree(GDALNearblackOptions *psOptions)
{
    delete psOptions;
}

/************************************************************************/
/*                  GDALNearblackOptionsSetProgress()                   */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALNearblack().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALNearblackOptionsSetProgress(GDALNearblackOptions *psOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
}

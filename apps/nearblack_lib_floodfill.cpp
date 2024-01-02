/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Convert nearly black or nearly white border to exact black/white
 *           using the flood fill algorithm.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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
#include "nearblack_lib.h"

#include <algorithm>
#include <memory>
#include <queue>

/************************************************************************/
/*                    GDALNearblackFloodFillAlg                         */
/************************************************************************/

// Implements the "final, combined-scan-and-fill span filler was then published
// in 1990" algorithm of https://en.wikipedia.org/wiki/Flood_fill#Span_filling

struct GDALNearblackFloodFillAlg
{
    // Input arguments of the algorithm
    const GDALNearblackOptions *m_psOptions = nullptr;
    GDALDataset *m_poSrcDataset = nullptr;
    GDALDataset *m_poDstDS = nullptr;
    GDALRasterBand *m_poMaskBand = nullptr;
    int m_nSrcBands = 0;
    int m_nDstBands = 0;
    bool m_bSetMask = false;
    Colors m_oColors{};
    GByte m_nReplacevalue = 0;

    // As we (generally) do not modify the value of pixels that are "black"
    // we need to keep track of the pixels we visited
    // Cf https://en.wikipedia.org/wiki/Flood_fill#Disadvantages_2
    // and https://en.wikipedia.org/wiki/Flood_fill#Adding_pattern_filling_support
    // for the requirement to add that extra sentinel
    std::unique_ptr<GDALDataset> m_poVisitedDS = nullptr;

    // Active line for the m_abyLine, m_abyLineMustSet, m_abyMask buffers
    int m_nLoadedLine = -1;

    // Whether Set(..., m_nLoadedLine) has been called
    bool m_bLineModified = true;

    // Content of m_poSrcDataset/m_poDstDS for m_nLoadedLine
    // Contains m_nDstBands * nXSize values in the order (R,G,B),(R,G,B),...
    std::vector<GByte> m_abyLine{};

    static constexpr GByte MUST_FILL_UNINIT = 0;  // must be 0
    static constexpr GByte MUST_FILL_FALSE = 1;
    static constexpr GByte MUST_FILL_TRUE = 2;
    // Content of m_poVisitedDS for m_nLoadedLine
    std::vector<GByte> m_abyLineMustSet{};

    // Only use if m_bSetMask
    std::vector<GByte> m_abyMask{};

    // Used for progress bar. Incremented the first time a line ifs loaded
    int m_nCountLoadedOnce = 0;

    // m_abLineLoadedOnce[line] is set to true after the first time the line
    // of m_poSrcDataset is loaded by LoadLine(line)
    std::vector<bool> m_abLineLoadedOnce{};

    // m_abLineSavedOnce[line] is set to true after the first time the line
    // of m_poDstDS is written by LoadLine()
    std::vector<bool> m_abLineSavedOnce{};

#ifdef DEBUG
    size_t m_nMaxQueueSize = 0;
#endif

    // Entry point
    bool Process();

  private:
    bool Fill(int iX, int iY);
    bool LoadLine(int iY);
    bool MustSet(int iX, int iY);
    void Set(int iX, int iY);
};

/************************************************************************/
/*              GDALNearblackFloodFillAlg::MustSet()                    */
/*                                                                      */
/* Called Inside() in https://en.wikipedia.org/wiki/Flood_fill          */
/************************************************************************/

// Returns true if the pixel (iX, iY) is "black" (or more generally transparent
// according to m_oColors)
bool GDALNearblackFloodFillAlg::MustSet(int iX, int iY)
{
    CPLAssert(iX >= 0);
    CPLAssert(iX < m_poSrcDataset->GetRasterXSize());

    CPLAssert(iY >= 0);
    CPLAssert(iY < m_poSrcDataset->GetRasterYSize());
    CPLAssert(iY == m_nLoadedLine);
    CPL_IGNORE_RET_VAL(iY);

    if (m_abyLineMustSet[iX] != MUST_FILL_UNINIT)
    {
        return m_abyLineMustSet[iX] == MUST_FILL_TRUE;
    }

    /***** loop over the colors *****/

    for (int iColor = 0; iColor < static_cast<int>(m_oColors.size()); iColor++)
    {
        const Color &oColor = m_oColors[iColor];

        /***** loop over the bands *****/
        bool bIsNonBlack = false;

        for (int iBand = 0; iBand < m_nSrcBands; iBand++)
        {
            const int nPix = m_abyLine[iX * m_nDstBands + iBand];

            if (oColor[iBand] - nPix > m_psOptions->nNearDist ||
                nPix > m_psOptions->nNearDist + oColor[iBand])
            {
                bIsNonBlack = true;
                break;
            }
        }

        if (!bIsNonBlack)
        {
            m_abyLineMustSet[iX] = MUST_FILL_TRUE;
            return true;
        }
    }

    m_abyLineMustSet[iX] = MUST_FILL_FALSE;
    return false;
}

/************************************************************************/
/*             GDALNearblackFloodFillAlg::LoadLine()                    */
/************************************************************************/

// Load the new line iY, and saves if needed buffer of the previous loaded
// line (m_nLoadedLine).
// Returns true if no error
bool GDALNearblackFloodFillAlg::LoadLine(int iY)
{
    if (iY != m_nLoadedLine)
    {
#ifdef DEBUG
        // CPLDebug("GDAL", "GDALNearblackFloodFillAlg::LoadLine(%d)", iY);
#endif
        const int nXSize = m_poSrcDataset->GetRasterXSize();

        if (m_nLoadedLine >= 0)
        {
            if (m_bLineModified || (m_poDstDS != m_poSrcDataset &&
                                    !m_abLineSavedOnce[m_nLoadedLine]))
            {
                if (m_poDstDS->RasterIO(
                        GF_Write, 0, m_nLoadedLine, nXSize, 1, m_abyLine.data(),
                        nXSize, 1, GDT_Byte, m_nDstBands, nullptr, m_nDstBands,
                        static_cast<GSpacing>(nXSize) * m_nDstBands, 1,
                        nullptr) != CE_None)
                {
                    return false;
                }
            }

            if (m_bSetMask &&
                (m_bLineModified || !m_abLineSavedOnce[m_nLoadedLine]))
            {
                if (m_poMaskBand->RasterIO(GF_Write, 0, m_nLoadedLine, nXSize,
                                           1, m_abyMask.data(), nXSize, 1,
                                           GDT_Byte, 0, 0, nullptr) != CE_None)
                {
                    return false;
                }
            }

            m_abLineSavedOnce[m_nLoadedLine] = true;
        }

        if (iY >= 0)
        {
            if (m_poDstDS != m_poSrcDataset && m_abLineSavedOnce[iY])
            {
                // If the output dataset is different from the source one,
                // load from the output dataset if we have already written the
                // line of interest
                if (m_poDstDS->RasterIO(
                        GF_Read, 0, iY, nXSize, 1, m_abyLine.data(), nXSize, 1,
                        GDT_Byte, m_nDstBands, nullptr, m_nDstBands,
                        static_cast<GSpacing>(nXSize) * m_nDstBands, 1,
                        nullptr) != CE_None)
                {
                    return false;
                }
            }
            else
            {
                // Otherwise load from the source data
                if (m_poSrcDataset->RasterIO(
                        GF_Read, 0, iY, nXSize, 1, m_abyLine.data(), nXSize, 1,
                        GDT_Byte,
                        // m_nSrcBands intended
                        m_nSrcBands,
                        // m_nDstBands intended
                        nullptr, m_nDstBands,
                        static_cast<GSpacing>(nXSize) * m_nDstBands, 1,
                        nullptr) != CE_None)
                {
                    return false;
                }

                // Initialize the alpha component to 255 if it is the first time
                // we load that line.
                if (m_psOptions->bSetAlpha && !m_abLineLoadedOnce[iY])
                {
                    for (int iCol = 0; iCol < nXSize; iCol++)
                    {
                        m_abyLine[iCol * m_nDstBands + m_nDstBands - 1] = 255;
                    }
                }
            }

            if (m_bSetMask)
            {
                if (!m_abLineLoadedOnce[iY])
                {
                    for (int iCol = 0; iCol < nXSize; iCol++)
                    {
                        m_abyMask[iCol] = 255;
                    }
                }
                else
                {
                    if (m_poMaskBand->RasterIO(
                            GF_Read, 0, iY, nXSize, 1, m_abyMask.data(), nXSize,
                            1, GDT_Byte, 0, 0, nullptr) != CE_None)
                    {
                        return false;
                    }
                }
            }

            if (!m_abLineLoadedOnce[iY])
            {
                m_nCountLoadedOnce++;
                // Very rough progression report based on the first time
                // we load a line...
                // We arbitrarily consider that it's 90% of the processing time
                const int nYSize = m_poSrcDataset->GetRasterYSize();
                if (!(m_psOptions->pfnProgress(
                        0.9 *
                            (m_nCountLoadedOnce / static_cast<double>(nYSize)),
                        nullptr, m_psOptions->pProgressData)))
                {
                    return false;
                }
                m_abLineLoadedOnce[iY] = true;
            }
        }

        if (m_nLoadedLine >= 0)
        {
            if (m_poVisitedDS->GetRasterBand(1)->RasterIO(
                    GF_Write, 0, m_nLoadedLine, nXSize, 1,
                    m_abyLineMustSet.data(), nXSize, 1, GDT_Byte, 0, 0,
                    nullptr) != CE_None)
            {
                return false;
            }
        }

        if (iY >= 0)
        {
            if (m_poVisitedDS->GetRasterBand(1)->RasterIO(
                    GF_Read, 0, iY, nXSize, 1, m_abyLineMustSet.data(), nXSize,
                    1, GDT_Byte, 0, 0, nullptr) != CE_None)
            {
                return false;
            }
        }

        m_bLineModified = false;
        m_nLoadedLine = iY;
    }
    return true;
}

/************************************************************************/
/*              GDALNearblackFloodFillAlg::Set()                        */
/************************************************************************/

// Mark the pixel as transparent
void GDALNearblackFloodFillAlg::Set(int iX, int iY)
{
    CPLAssert(iY == m_nLoadedLine);
    CPL_IGNORE_RET_VAL(iY);

    m_bLineModified = true;
    m_abyLineMustSet[iX] = MUST_FILL_FALSE;

    for (int iBand = 0; iBand < m_nSrcBands; iBand++)
        m_abyLine[iX * m_nDstBands + iBand] = m_nReplacevalue;

    /***** alpha *****/
    if (m_nDstBands > m_nSrcBands)
        m_abyLine[iX * m_nDstBands + m_nDstBands - 1] = 0;

    if (m_bSetMask)
        m_abyMask[iX] = 0;
}

/************************************************************************/
/*              GDALNearblackFloodFillAlg::Fill()                       */
/************************************************************************/

/* Implements the "final, combined-scan-and-fill span filler was then published
 * in 1990" algorithm of https://en.wikipedia.org/wiki/Flood_fill#Span_filling
 * with the following enhancements:
 * - extra bound checking to avoid calling MustSet() outside the raster
 * - extra bound checking to avoid pushing spans outside the raster
 *
 * Returns true if no error.
 */

bool GDALNearblackFloodFillAlg::Fill(int iXInit, int iYInit)
{
    const int nXSize = m_poSrcDataset->GetRasterXSize();
    const int nYSize = m_poSrcDataset->GetRasterYSize();

    struct Span
    {
        int x1;
        int x2;
        int y;
        int dy;

        Span(int x1In, int x2In, int yIn, int dyIn)
            : x1(x1In), x2(x2In), y(yIn), dy(dyIn)
        {
        }
    };

    if (!LoadLine(iYInit))
        return false;

    if (!MustSet(iXInit, iYInit))
    {
        // nothing to do
        return true;
    }

    std::queue<Span> queue;
    queue.emplace(Span(iXInit, iXInit, iYInit, 1));
    if (iYInit > 0)
    {
        queue.emplace(Span(iXInit, iXInit, iYInit - 1, -1));
    }

    while (!queue.empty())
    {
#ifdef DEBUG
        m_nMaxQueueSize = std::max(m_nMaxQueueSize, queue.size());
#endif

        const Span s = queue.front();
        queue.pop();

        CPLAssert(s.x1 >= 0);
        CPLAssert(s.x1 < nXSize);
        CPLAssert(s.x2 >= 0);
        CPLAssert(s.x2 < nXSize);
        CPLAssert(s.x2 >= s.x1);
        CPLAssert(s.y >= 0);
        CPLAssert(s.y < nYSize);

        int iX = s.x1;
        const int iY = s.y;

        if (!LoadLine(iY))
            return false;

        if (iX > 0 && MustSet(iX, iY))
        {
            while (MustSet(iX - 1, iY))
            {
                Set(iX - 1, iY);
                iX--;
                if (iX == 0)
                    break;
            }
        }
        if (iX >= 0 && iX <= s.x1 - 1 && iY - s.dy >= 0 && iY - s.dy < nYSize)
        {
            queue.emplace(Span(iX, s.x1 - 1, iY - s.dy, -s.dy));
        }
        int iX1 = s.x1;
        const int iX2 = s.x2;
        while (iX1 <= iX2)
        {
            while (MustSet(iX1, iY))
            {
                Set(iX1, iY);
                iX1++;
                if (iX1 == nXSize)
                    break;
            }
            if (iX <= iX1 - 1 && iY + s.dy >= 0 && iY + s.dy < nYSize)
            {
                queue.emplace(Span(iX, iX1 - 1, iY + s.dy, s.dy));
            }
            if (iX1 - 1 > iX2 && iY - s.dy >= 0 && iY - s.dy < nYSize)
            {
                queue.emplace(Span(iX2 + 1, iX1 - 1, iY - s.dy, -s.dy));
            }
            iX1++;
            while (iX1 < iX2 && !MustSet(iX1, iY))
                iX1++;
            iX = iX1;
        }
    }

    return true;
}

/************************************************************************/
/*              GDALNearblackFloodFillAlg::Process()                    */
/************************************************************************/

// Entry point.
// Returns true if no error.

bool GDALNearblackFloodFillAlg::Process()
{
    const int nXSize = m_poSrcDataset->GetRasterXSize();
    const int nYSize = m_poSrcDataset->GetRasterYSize();

    /* -------------------------------------------------------------------- */
    /*      Allocate working buffers.                                       */
    /* -------------------------------------------------------------------- */
    try
    {
        m_abyLine.resize(static_cast<size_t>(nXSize) * m_nDstBands);
        m_abyLineMustSet.resize(nXSize);
        if (m_bSetMask)
            m_abyMask.resize(nXSize);

        if (m_psOptions->nMaxNonBlack > 0)
        {
            m_abLineLoadedOnce.resize(nYSize, true);
            m_abLineSavedOnce.resize(nYSize, true);
        }
        else
        {
            m_abLineLoadedOnce.resize(nYSize);
            m_abLineSavedOnce.resize(nYSize);
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate working buffers: %s", e.what());
        return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a temporary dataset to save visited state                */
    /* -------------------------------------------------------------------- */

    // For debugging / testing purposes only
    const char *pszTmpDriver =
        CPLGetConfigOption("GDAL_TEMP_DRIVER_NAME", nullptr);
    if (!pszTmpDriver)
    {
        pszTmpDriver =
            (nXSize < 100 * 1024 * 1024 / nYSize ||
             (m_poDstDS->GetDriver() &&
              strcmp(m_poDstDS->GetDriver()->GetDescription(), "MEM") == 0))
                ? "MEM"
                : "GTiff";
    }
    GDALDriverH hDriver = GDALGetDriverByName(pszTmpDriver);
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find driver %s for temporary file", pszTmpDriver);
        return false;
    }
    std::string osVisitedDataset = m_poDstDS->GetDescription();
    VSIStatBuf sStat;
    if (strcmp(pszTmpDriver, "MEM") == 0 ||
        STARTS_WITH(osVisitedDataset.c_str(), "/vsimem/") ||
        // Regular VSIStat() (not VSIStatL()) intended to check this is
        // a real file
        VSIStat(osVisitedDataset.c_str(), &sStat) == 0)
    {
        osVisitedDataset += ".visited";
    }
    else
    {
        osVisitedDataset = CPLGenerateTempFilename(osVisitedDataset.c_str());
    }
    CPLStringList aosOptions;
    if (strcmp(pszTmpDriver, "GTiff") == 0)
    {
        aosOptions.SetNameValue("SPARSE_OK", "YES");
        aosOptions.SetNameValue("COMPRESS", "LZW");
        osVisitedDataset += ".tif";
    }
    m_poVisitedDS.reset(GDALDataset::FromHandle(
        GDALCreate(hDriver, osVisitedDataset.c_str(), nXSize, nYSize, 1,
                   GDT_Byte, aosOptions.List())));
    if (!m_poVisitedDS)
        return false;
    if (strcmp(pszTmpDriver, "MEM") != 0)
    {
        VSIUnlink(osVisitedDataset.c_str());
    }
    m_poVisitedDS->MarkSuppressOnClose();

    /* -------------------------------------------------------------------- */
    /*      Iterate over the border of the raster                           */
    /* -------------------------------------------------------------------- */
    // Fill from top line
    for (int iX = 0; iX < nXSize; iX++)
    {
        if (!Fill(iX, 0))
            return false;
    }

    // Fill from left and right side
    for (int iY = 1; iY < nYSize - 1; iY++)
    {
        if (!Fill(0, iY))
            return false;
        if (!Fill(nXSize - 1, iY))
            return false;
    }

    // Fill from bottom line
    for (int iX = 0; iX < nXSize; iX++)
    {
        if (!Fill(iX, nYSize - 1))
            return false;
    }

    if (!(m_psOptions->pfnProgress(1.0, nullptr, m_psOptions->pProgressData)))
    {
        return false;
    }

#ifdef DEBUG
    CPLDebug("GDAL", "flood fill max queue size = %u",
             unsigned(m_nMaxQueueSize));
#endif

    // Force update of last visited line
    return LoadLine(-1);
}

/************************************************************************/
/*                    GDALNearblackFloodFill()                          */
/************************************************************************/

// Entry point.
// Returns true if no error.

bool GDALNearblackFloodFill(const GDALNearblackOptions *psOptions,
                            GDALDatasetH hSrcDataset, GDALDatasetH hDstDS,
                            GDALRasterBandH hMaskBand, int nSrcBands,
                            int nDstBands, bool bSetMask, const Colors &oColors)
{
    GDALNearblackFloodFillAlg alg;
    alg.m_psOptions = psOptions;
    alg.m_poSrcDataset = GDALDataset::FromHandle(hSrcDataset);
    alg.m_poDstDS = GDALDataset::FromHandle(hDstDS);
    alg.m_poMaskBand = GDALRasterBand::FromHandle(hMaskBand);
    alg.m_nSrcBands = nSrcBands;
    alg.m_nDstBands = nDstBands;
    alg.m_bSetMask = bSetMask;
    alg.m_oColors = oColors;
    alg.m_nReplacevalue = psOptions->bNearWhite ? 255 : 0;

    if (psOptions->nMaxNonBlack > 0)
    {
        // First pass: use the TwoPasses algorithm to deal with nMaxNonBlack
        GDALNearblackOptions sOptionsTmp(*psOptions);
        sOptionsTmp.pProgressData = GDALCreateScaledProgress(
            0, 0.5, psOptions->pfnProgress, psOptions->pProgressData);
        sOptionsTmp.pfnProgress = GDALScaledProgress;
        bool bRet = GDALNearblackTwoPassesAlgorithm(
            &sOptionsTmp, hSrcDataset, hDstDS, hMaskBand, nSrcBands, nDstBands,
            bSetMask, oColors);
        GDALDestroyScaledProgress(sOptionsTmp.pProgressData);
        if (!bRet)
            return false;

        // Second pass: use flood fill
        sOptionsTmp.pProgressData = GDALCreateScaledProgress(
            0.5, 1, psOptions->pfnProgress, psOptions->pProgressData);
        sOptionsTmp.pfnProgress = GDALScaledProgress;
        alg.m_psOptions = &sOptionsTmp;
        bRet = alg.Process();
        GDALDestroyScaledProgress(sOptionsTmp.pProgressData);
        return bRet;
    }
    else
    {
        return alg.Process();
    }
}

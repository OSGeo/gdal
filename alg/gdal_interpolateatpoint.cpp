/******************************************************************************
 * $Id$
 *
 * Project:  GDAL DEM Interpolation
 * Purpose:  Interpolation algorithms with cache
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2024, Javier Jimenez Shaw
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

#include "gdal_interpolateatpoint.h"

#include "gdalresamplingkernels.h"

#include <algorithm>

static bool
GDALInterpExtractDEMWindow(GDALRasterBand *pBand,
                           std::unique_ptr<DoublePointsCache> &cacheDEM, int nX,
                           int nY, int nWidth, int nHeight, double *padfOut)
{
    constexpr int BLOCK_SIZE = 64;

    // Request the DEM by blocks of BLOCK_SIZE * BLOCK_SIZE and put them
    // in cacheDEM
    if (!cacheDEM)
        cacheDEM.reset(new DoublePointsCache{});

    const int nXIters = (nX + nWidth - 1) / BLOCK_SIZE - nX / BLOCK_SIZE + 1;
    const int nYIters = (nY + nHeight - 1) / BLOCK_SIZE - nY / BLOCK_SIZE + 1;
    const int nRasterXSize = pBand->GetXSize();
    const int nRasterYSize = pBand->GetYSize();
    for (int iY = 0; iY < nYIters; iY++)
    {
        const int nBlockY = nY / BLOCK_SIZE + iY;
        const int nReqYSize =
            std::min(nRasterYSize - nBlockY * BLOCK_SIZE, BLOCK_SIZE);
        const int nFirstLineInCachedBlock = (iY == 0) ? nY % BLOCK_SIZE : 0;
        const int nFirstLineInOutput =
            (iY == 0) ? 0
                      : BLOCK_SIZE - (nY % BLOCK_SIZE) + (iY - 1) * BLOCK_SIZE;
        const int nLinesToCopy = (nYIters == 1) ? nHeight
                                 : (iY == 0)    ? BLOCK_SIZE - (nY % BLOCK_SIZE)
                                 : (iY == nYIters - 1)
                                     ? 1 + (nY + nHeight - 1) % BLOCK_SIZE
                                     : BLOCK_SIZE;
        for (int iX = 0; iX < nXIters; iX++)
        {
            const int nBlockX = nX / BLOCK_SIZE + iX;
            const int nReqXSize =
                std::min(nRasterXSize - nBlockX * BLOCK_SIZE, BLOCK_SIZE);
            const uint64_t nKey =
                (static_cast<uint64_t>(nBlockY) << 32) | nBlockX;
            const int nFirstColInCachedBlock = (iX == 0) ? nX % BLOCK_SIZE : 0;
            const int nFirstColInOutput =
                (iX == 0)
                    ? 0
                    : BLOCK_SIZE - (nX % BLOCK_SIZE) + (iX - 1) * BLOCK_SIZE;
            const int nColsToCopy = (nXIters == 1) ? nWidth
                                    : (iX == 0) ? BLOCK_SIZE - (nX % BLOCK_SIZE)
                                    : (iX == nXIters - 1)
                                        ? 1 + (nX + nWidth - 1) % BLOCK_SIZE
                                        : BLOCK_SIZE;

#if 0
            CPLDebug("RPC", "nY=%d nX=%d nBlockY=%d nBlockX=%d "
                     "nFirstLineInCachedBlock=%d nFirstLineInOutput=%d nLinesToCopy=%d "
                     "nFirstColInCachedBlock=%d nFirstColInOutput=%d nColsToCopy=%d",
                     nY, nX, nBlockY, nBlockX, nFirstLineInCachedBlock, nFirstLineInOutput, nLinesToCopy,
                     nFirstColInCachedBlock, nFirstColInOutput, nColsToCopy);
#endif

            std::shared_ptr<std::vector<double>> poValue;
            if (!cacheDEM->tryGet(nKey, poValue))
            {
                poValue = std::make_shared<std::vector<double>>(nReqXSize *
                                                                nReqYSize);
                CPLErr eErr = pBand->RasterIO(
                    GF_Read, nBlockX * BLOCK_SIZE, nBlockY * BLOCK_SIZE,
                    nReqXSize, nReqYSize, poValue->data(), nReqXSize, nReqYSize,
                    GDT_Float64, 0, 0, nullptr);
                if (eErr != CE_None)
                {
                    return false;
                }
                cacheDEM->insert(nKey, poValue);
            }

            // Compose the cached block to the final buffer
            for (int j = 0; j < nLinesToCopy; j++)
            {
                memcpy(padfOut + (nFirstLineInOutput + j) * nWidth +
                           nFirstColInOutput,
                       poValue->data() +
                           (nFirstLineInCachedBlock + j) * nReqXSize +
                           nFirstColInCachedBlock,
                       nColsToCopy * sizeof(double));
            }
        }
    }

#if 0
    CPLDebug("RPC_DEM", "DEM for %d,%d,%d,%d", nX, nY, nWidth, nHeight);
    for(int j = 0; j < nHeight; j++)
    {
        std::string osLine;
        for(int i = 0; i < nWidth; ++i )
        {
            if( !osLine.empty() )
                osLine += ", ";
            osLine += std::to_string(padfOut[j * nWidth + i]);
        }
        CPLDebug("RPC_DEM", "%s", osLine.c_str());
    }
#endif

    return true;
}

/************************************************************************/
/*                        GDALInterpolateAtPoint()                      */
/************************************************************************/

bool GDALInterpolateAtPoint(GDALRasterBand *pBand,
                            GDALRIOResampleAlg eResampleAlg,
                            std::unique_ptr<DoublePointsCache> &cache,
                            const double dfXIn, const double dfYIn,
                            double *pdfOutputValue)
{
    const int nRasterXSize = pBand->GetXSize();
    const int nRasterYSize = pBand->GetYSize();
    int bGotNoDataValue = FALSE;
    const double dfNoDataValue = pBand->GetNoDataValue(&bGotNoDataValue);

    if (dfXIn < 0 || dfXIn > nRasterXSize || dfYIn < 0 || dfYIn > nRasterYSize)
    {
        return FALSE;
    }

    // Downgrade the interpolation algorithm if the image is too small
    if ((nRasterXSize < 4 || nRasterYSize < 4) &&
        (eResampleAlg == GRIORA_CubicSpline || eResampleAlg == GRIORA_Cubic))
    {
        eResampleAlg = GRIORA_Bilinear;
    }
    if ((nRasterXSize < 2 || nRasterYSize < 2) &&
        eResampleAlg == GRIORA_Bilinear)
    {
        eResampleAlg = GRIORA_NearestNeighbour;
    }

    auto outOfBorderCorrection = [](int dNew, int nRasterSize, int nKernelsize)
    {
        int dOutOfBorder = 0;
        if (dNew < 0)
        {
            dOutOfBorder = dNew;
        }
        if (dNew + nKernelsize >= nRasterSize)
        {
            dOutOfBorder = dNew + nKernelsize - nRasterSize;
        }
        return dOutOfBorder;
    };

    auto dragElevDataInBorder =
        [](double *adfElevData, int dOutOfBorder, int nKernelSize, bool bIsX)
    {
        while (dOutOfBorder < 0)
        {
            for (int j = 0; j < nKernelSize; j++)
                for (int ii = 0; ii < nKernelSize - 1; ii++)
                {
                    const int i = nKernelSize - ii - 2;  // iterate in reverse
                    const int row_src = bIsX ? j : i;
                    const int row_dst = bIsX ? j : i + 1;
                    const int col_src = bIsX ? i : j;
                    const int col_dst = bIsX ? i + 1 : j;
                    adfElevData[nKernelSize * row_dst + col_dst] =
                        adfElevData[nKernelSize * row_src + col_src];
                }
            dOutOfBorder++;
        }
        while (dOutOfBorder > 0)
        {
            for (int j = 0; j < nKernelSize; j++)
                for (int i = 0; i < nKernelSize - 1; i++)
                {
                    const int row_src = bIsX ? j : i + 1;
                    const int row_dst = bIsX ? j : i;
                    const int col_src = bIsX ? i + 1 : j;
                    const int col_dst = bIsX ? i : j;
                    adfElevData[nKernelSize * row_dst + col_dst] =
                        adfElevData[nKernelSize * row_src + col_src];
                }
            dOutOfBorder--;
        }
    };

    if (eResampleAlg == GDALRIOResampleAlg::GRIORA_CubicSpline ||
        eResampleAlg == GDALRIOResampleAlg::GRIORA_Cubic)
    {
        // Convert from upper left corner of pixel coordinates to center of
        // pixel coordinates:
        const double dfX = dfXIn - 0.5;
        const double dfY = dfYIn - 0.5;
        const int dX = static_cast<int>(std::floor(dfX));
        const int dY = static_cast<int>(std::floor(dfY));
        const double dfDeltaX = dfX - dX;
        const double dfDeltaY = dfY - dY;

        const int dXNew = dX - 1;
        const int dYNew = dY - 1;
        const int nKernelSize = 4;
        const int dXOutOfBorder =
            outOfBorderCorrection(dXNew, nRasterXSize, nKernelSize);
        const int dYOutOfBorder =
            outOfBorderCorrection(dYNew, nRasterYSize, nKernelSize);

        // CubicSpline interpolation.
        double adfElevData[16] = {0.0};
        if (!GDALInterpExtractDEMWindow(pBand, cache, dXNew - dXOutOfBorder,
                                        dYNew - dYOutOfBorder, nKernelSize,
                                        nKernelSize, adfElevData))
        {
            return FALSE;
        }
        dragElevDataInBorder(adfElevData, dXOutOfBorder, nKernelSize, true);
        dragElevDataInBorder(adfElevData, dYOutOfBorder, nKernelSize, false);

        double dfSumH = 0.0;
        double dfSumWeight = 0.0;
        for (int k_i = 0; k_i < 4; k_i++)
        {
            // Loop across the X axis.
            for (int k_j = 0; k_j < 4; k_j++)
            {
                // Calculate the weight for the specified pixel according
                // to the bicubic b-spline kernel we're using for
                // interpolation.
                const int dKernIndX = k_j - 1;
                const int dKernIndY = k_i - 1;
                const double dfPixelWeight =
                    eResampleAlg == GDALRIOResampleAlg::GRIORA_CubicSpline
                        ? CubicSplineKernel(dKernIndX - dfDeltaX) *
                              CubicSplineKernel(dKernIndY - dfDeltaY)
                        : CubicKernel(dKernIndX - dfDeltaX) *
                              CubicKernel(dKernIndY - dfDeltaY);

                // Create a sum of all values
                // adjusted for the pixel's calculated weight.
                const double dfElev = adfElevData[k_j + k_i * 4];
                if (bGotNoDataValue && ARE_REAL_EQUAL(dfNoDataValue, dfElev))
                    continue;

                dfSumH += dfElev * dfPixelWeight;
                dfSumWeight += dfPixelWeight;
            }
        }
        if (dfSumWeight == 0.0)
        {
            return FALSE;
        }

        *pdfOutputValue = dfSumH / dfSumWeight;

        return TRUE;
    }
    else if (eResampleAlg == GDALRIOResampleAlg::GRIORA_Bilinear)
    {
        // Convert from upper left corner of pixel coordinates to center of
        // pixel coordinates:
        const double dfX = dfXIn - 0.5;
        const double dfY = dfYIn - 0.5;
        const int dX = static_cast<int>(std::floor(dfX));
        const int dY = static_cast<int>(std::floor(dfY));
        const double dfDeltaX = dfX - dX;
        const double dfDeltaY = dfY - dY;

        const int nKernelSize = 2;
        const int dXOutOfBorder =
            outOfBorderCorrection(dX, nRasterXSize, nKernelSize);
        const int dYOutOfBorder =
            outOfBorderCorrection(dY, nRasterYSize, nKernelSize);

        // Bilinear interpolation.
        double adfElevData[4] = {0.0, 0.0, 0.0, 0.0};
        if (!GDALInterpExtractDEMWindow(pBand, cache, dX - dXOutOfBorder,
                                        dY - dYOutOfBorder, nKernelSize,
                                        nKernelSize, adfElevData))
        {
            return FALSE;
        }
        dragElevDataInBorder(adfElevData, dXOutOfBorder, nKernelSize, true);
        dragElevDataInBorder(adfElevData, dYOutOfBorder, nKernelSize, false);

        if (bGotNoDataValue)
        {
            // TODO: We could perhaps use a valid sample if there's one.
            bool bFoundNoDataElev = false;
            for (int k_i = 0; k_i < 4; k_i++)
            {
                if (ARE_REAL_EQUAL(dfNoDataValue, adfElevData[k_i]))
                    bFoundNoDataElev = true;
            }
            if (bFoundNoDataElev)
            {
                return FALSE;
            }
        }
        const double dfDeltaX1 = 1.0 - dfDeltaX;
        const double dfDeltaY1 = 1.0 - dfDeltaY;

        const double dfXZ1 =
            adfElevData[0] * dfDeltaX1 + adfElevData[1] * dfDeltaX;
        const double dfXZ2 =
            adfElevData[2] * dfDeltaX1 + adfElevData[3] * dfDeltaX;
        const double dfYZ = dfXZ1 * dfDeltaY1 + dfXZ2 * dfDeltaY;

        *pdfOutputValue = dfYZ;

        return TRUE;
    }
    else
    {
        const int dX = static_cast<int>(dfXIn);
        const int dY = static_cast<int>(dfYIn);
        double dfDEMH = 0.0;
        if (!GDALInterpExtractDEMWindow(pBand, cache, dX, dY, 1, 1, &dfDEMH) ||
            (bGotNoDataValue && ARE_REAL_EQUAL(dfNoDataValue, dfDEMH)))
        {
            return FALSE;
        }

        *pdfOutputValue = dfDEMH;

        return TRUE;
    }
}
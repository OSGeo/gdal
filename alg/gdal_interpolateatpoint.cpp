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
#include <complex>

template <typename T> bool areEqualReal(double dfNoDataValue, T dfOut);

template <> bool areEqualReal(double dfNoDataValue, double dfOut)
{
    return ARE_REAL_EQUAL(dfNoDataValue, dfOut);
}

template <> bool areEqualReal(double dfNoDataValue, std::complex<double> dfOut)
{
    return ARE_REAL_EQUAL(dfNoDataValue, dfOut.real());
}

// Only valid for T = double or std::complex<double>
template <typename T>
bool GDALInterpExtractValuesWindow(GDALRasterBand *pBand,
                                   std::unique_ptr<DoublePointsCache> &cache,
                                   int nX, int nY, int nWidth, int nHeight,
                                   T *padfOut)
{
    constexpr int BLOCK_SIZE = 64;

    // Request the DEM by blocks of BLOCK_SIZE * BLOCK_SIZE and put them
    // in cache
    if (!cache)
        cache.reset(new DoublePointsCache{});

    const int nXIters = (nX + nWidth - 1) / BLOCK_SIZE - nX / BLOCK_SIZE + 1;
    const int nYIters = (nY + nHeight - 1) / BLOCK_SIZE - nY / BLOCK_SIZE + 1;
    const int nRasterXSize = pBand->GetXSize();
    const int nRasterYSize = pBand->GetYSize();
    const bool bIsComplex =
        CPL_TO_BOOL(GDALDataTypeIsComplex(pBand->GetRasterDataType()));

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

            constexpr int nTypeFactor = sizeof(T) / sizeof(double);
            std::shared_ptr<std::vector<double>> poValue;
            if (!cache->tryGet(nKey, poValue))
            {
                const GDALDataType eDataType =
                    bIsComplex ? GDT_CFloat64 : GDT_Float64;
                const size_t nVectorSize =
                    size_t(nReqXSize) * nReqYSize * nTypeFactor;
                poValue = std::make_shared<std::vector<double>>(nVectorSize);
                CPLErr eErr = pBand->RasterIO(
                    GF_Read, nBlockX * BLOCK_SIZE, nBlockY * BLOCK_SIZE,
                    nReqXSize, nReqYSize, poValue->data(), nReqXSize, nReqYSize,
                    eDataType, 0, 0, nullptr);
                if (eErr != CE_None)
                {
                    return false;
                }
                cache->insert(nKey, poValue);
            }

            double *padfAsDouble = reinterpret_cast<double *>(padfOut);
            // Compose the cached block to the final buffer
            for (int j = 0; j < nLinesToCopy; j++)
            {
                memcpy(padfAsDouble + ((nFirstLineInOutput + j) * nWidth +
                                       nFirstColInOutput) *
                                          nTypeFactor,
                       poValue->data() +
                           ((nFirstLineInCachedBlock + j) * nReqXSize +
                            nFirstColInCachedBlock) *
                               nTypeFactor,
                       nColsToCopy * sizeof(T));
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

template <typename T>
bool GDALInterpolateAtPointImpl(GDALRasterBand *pBand,
                                GDALRIOResampleAlg eResampleAlg,
                                std::unique_ptr<DoublePointsCache> &cache,
                                const double dfXIn, const double dfYIn, T &out)
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

    auto dragReadDataInBorder =
        [](T *adfElevData, int dOutOfBorder, int nKernelSize, bool bIsX)
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

    auto applyBilinearKernel = [&](double dfDeltaX, double dfDeltaY,
                                   T *adfValues, T &pdfRes) -> bool
    {
        if (bGotNoDataValue)
        {
            // TODO: We could perhaps use a valid sample if there's one.
            bool bFoundNoDataElev = false;
            for (int k_i = 0; k_i < 4; k_i++)
            {
                if (areEqualReal(dfNoDataValue, adfValues[k_i]))
                    bFoundNoDataElev = true;
            }
            if (bFoundNoDataElev)
            {
                return FALSE;
            }
        }
        const double dfDeltaX1 = 1.0 - dfDeltaX;
        const double dfDeltaY1 = 1.0 - dfDeltaY;

        const T dfXZ1 = adfValues[0] * dfDeltaX1 + adfValues[1] * dfDeltaX;
        const T dfXZ2 = adfValues[2] * dfDeltaX1 + adfValues[3] * dfDeltaX;
        const T dfYZ = dfXZ1 * dfDeltaY1 + dfXZ2 * dfDeltaY;

        pdfRes = dfYZ;
        return TRUE;
    };

    auto apply4x4Kernel = [&](double dfDeltaX, double dfDeltaY, T *adfValues,
                              T &pdfRes) -> bool
    {
        T dfSumH = 0.0;
        T dfSumWeight = 0.0;
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
                const T dfElev = adfValues[k_j + k_i * 4];
                if (bGotNoDataValue && areEqualReal(dfNoDataValue, dfElev))
                    continue;

                dfSumH += dfElev * dfPixelWeight;
                dfSumWeight += dfPixelWeight;
            }
        }
        if (dfSumWeight == 0.0)
        {
            return FALSE;
        }

        pdfRes = dfSumH / dfSumWeight;

        return TRUE;
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
        T adfReadData[16] = {0.0};
        if (!GDALInterpExtractValuesWindow(pBand, cache, dXNew - dXOutOfBorder,
                                           dYNew - dYOutOfBorder, nKernelSize,
                                           nKernelSize, adfReadData))
        {
            return FALSE;
        }
        dragReadDataInBorder(adfReadData, dXOutOfBorder, nKernelSize, true);
        dragReadDataInBorder(adfReadData, dYOutOfBorder, nKernelSize, false);
        if (!apply4x4Kernel(dfDeltaX, dfDeltaY, adfReadData, out))
            return FALSE;

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
        T adfReadData[4] = {0.0};
        if (!GDALInterpExtractValuesWindow(pBand, cache, dX - dXOutOfBorder,
                                           dY - dYOutOfBorder, nKernelSize,
                                           nKernelSize, adfReadData))
        {
            return FALSE;
        }
        dragReadDataInBorder(adfReadData, dXOutOfBorder, nKernelSize, true);
        dragReadDataInBorder(adfReadData, dYOutOfBorder, nKernelSize, false);
        if (!applyBilinearKernel(dfDeltaX, dfDeltaY, adfReadData, out))
            return FALSE;

        return TRUE;
    }
    else
    {
        const int dX = static_cast<int>(dfXIn);
        const int dY = static_cast<int>(dfYIn);
        T dfOut{};
        if (!GDALInterpExtractValuesWindow(pBand, cache, dX, dY, 1, 1,
                                           &dfOut) ||
            (bGotNoDataValue && areEqualReal(dfNoDataValue, dfOut)))
        {
            return FALSE;
        }

        out = dfOut;

        return TRUE;
    }
}

/************************************************************************/
/*                        GDALInterpolateAtPoint()                      */
/************************************************************************/

bool GDALInterpolateAtPoint(GDALRasterBand *pBand,
                            GDALRIOResampleAlg eResampleAlg,
                            std::unique_ptr<DoublePointsCache> &cache,
                            const double dfXIn, const double dfYIn,
                            double *pdfOutputReal, double *pdfOutputImag)
{
    const bool bIsComplex =
        CPL_TO_BOOL(GDALDataTypeIsComplex(pBand->GetRasterDataType()));
    bool res = TRUE;
    if (bIsComplex)
    {
        std::complex<double> out{};
        res = GDALInterpolateAtPointImpl(pBand, eResampleAlg, cache, dfXIn,
                                         dfYIn, out);
        *pdfOutputReal = out.real();
        if (pdfOutputImag)
            *pdfOutputImag = out.imag();
    }
    else
    {
        double out{};
        res = GDALInterpolateAtPointImpl(pBand, eResampleAlg, cache, dfXIn,
                                         dfYIn, out);
        *pdfOutputReal = out;
        if (pdfOutputImag)
            *pdfOutputImag = 0;
    }
    return res;
}

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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_interpolateatpoint.h"

#include "gdalresamplingkernels.h"

#include "gdal_vectorx.h"

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
                                   gdal::Vector2i point,
                                   gdal::Vector2i dimensions, T *padfOut)
{
    constexpr int BLOCK_SIZE = 64;

    const int nX = point.x();
    const int nY = point.y();
    const int nWidth = dimensions.x();
    const int nHeight = dimensions.y();

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
                const size_t dstOffset =
                    (static_cast<size_t>(nFirstLineInOutput + j) * nWidth +
                     nFirstColInOutput) *
                    nTypeFactor;
                const size_t srcOffset =
                    (static_cast<size_t>(nFirstLineInCachedBlock + j) *
                         nReqXSize +
                     nFirstColInCachedBlock) *
                    nTypeFactor;
                if (srcOffset + nColsToCopy * nTypeFactor > poValue->size())
                {
                    return false;
                }
                memcpy(padfAsDouble + dstOffset, poValue->data() + srcOffset,
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
                                double dfXIn, double dfYIn, T &out)
{
    const gdal::Vector2i rasterSize{pBand->GetXSize(), pBand->GetYSize()};

    if (eResampleAlg == GRIORA_NearestNeighbour)
    {
        // Allow input coordinates right at the bottom or right edge
        // with GRIORA_NearestNeighbour.
        // "introduce" them in the pixel of the image.
        if (dfXIn >= rasterSize.x() && dfXIn <= rasterSize.x() + 1e-5)
            dfXIn -= 0.25;
        if (dfYIn >= rasterSize.y() && dfYIn <= rasterSize.y() + 1e-5)
            dfYIn -= 0.25;
    }
    const gdal::Vector2d inLoc{dfXIn, dfYIn};

    int bGotNoDataValue = FALSE;
    const double dfNoDataValue = pBand->GetNoDataValue(&bGotNoDataValue);

    if (inLoc.x() < 0 || inLoc.x() > rasterSize.x() || inLoc.y() < 0 ||
        inLoc.y() > rasterSize.y())
    {
        return FALSE;
    }

    // Downgrade the interpolation algorithm if the image is too small
    if ((rasterSize.x() < 4 || rasterSize.y() < 4) &&
        (eResampleAlg == GRIORA_CubicSpline || eResampleAlg == GRIORA_Cubic))
    {
        eResampleAlg = GRIORA_Bilinear;
    }
    if ((rasterSize.x() < 2 || rasterSize.y() < 2) &&
        eResampleAlg == GRIORA_Bilinear)
    {
        eResampleAlg = GRIORA_NearestNeighbour;
    }

    auto outOfBorderCorrectionSimple =
        [](int dNew, int nRasterSize, int nKernelsize)
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

    auto outOfBorderCorrection =
        [&outOfBorderCorrectionSimple,
         &rasterSize](gdal::Vector2i input, int nKernelsize) -> gdal::Vector2i
    {
        return {
            outOfBorderCorrectionSimple(input.x(), rasterSize.x(), nKernelsize),
            outOfBorderCorrectionSimple(input.y(), rasterSize.y(),
                                        nKernelsize)};
    };

    auto dragReadDataInBorderSimple =
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
    auto dragReadDataInBorder = [&dragReadDataInBorderSimple](
                                    T *adfElevData, gdal::Vector2i dOutOfBorder,
                                    int nKernelSize) -> void
    {
        dragReadDataInBorderSimple(adfElevData, dOutOfBorder.x(), nKernelSize,
                                   true);
        dragReadDataInBorderSimple(adfElevData, dOutOfBorder.y(), nKernelSize,
                                   false);
    };

    auto applyBilinearKernel = [&](gdal::Vector2d dfDelta, T *adfValues,
                                   T &pdfRes) -> bool
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
        const gdal::Vector2d dfDelta1 = 1.0 - dfDelta;

        const T dfXZ1 =
            adfValues[0] * dfDelta1.x() + adfValues[1] * dfDelta.x();
        const T dfXZ2 =
            adfValues[2] * dfDelta1.x() + adfValues[3] * dfDelta.x();
        const T dfYZ = dfXZ1 * dfDelta1.y() + dfXZ2 * dfDelta.y();

        pdfRes = dfYZ;
        return TRUE;
    };

    auto apply4x4Kernel = [&](gdal::Vector2d dfDelta, T *adfValues,
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
                const gdal::Vector2i dKernInd = {k_j - 1, k_i - 1};
                const gdal::Vector2d fPoint = dKernInd.cast<double>() - dfDelta;
                const double dfPixelWeight =
                    eResampleAlg == GDALRIOResampleAlg::GRIORA_CubicSpline
                        ? CubicSplineKernel(fPoint.x()) *
                              CubicSplineKernel(fPoint.y())
                        : CubicKernel(fPoint.x()) * CubicKernel(fPoint.y());

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
        const gdal::Vector2d df = inLoc - 0.5;
        const gdal::Vector2i d = df.floor().template cast<int>();
        const gdal::Vector2d delta = df - d.cast<double>();
        const gdal::Vector2i dNew = d - 1;
        const int nKernelSize = 4;
        const gdal::Vector2i dOutOfBorder =
            outOfBorderCorrection(dNew, nKernelSize);

        // CubicSpline interpolation.
        T adfReadData[16] = {0.0};
        if (!GDALInterpExtractValuesWindow(pBand, cache, dNew - dOutOfBorder,
                                           {nKernelSize, nKernelSize},
                                           adfReadData))
        {
            return FALSE;
        }
        dragReadDataInBorder(adfReadData, dOutOfBorder, nKernelSize);
        if (!apply4x4Kernel(delta, adfReadData, out))
            return FALSE;

        return TRUE;
    }
    else if (eResampleAlg == GDALRIOResampleAlg::GRIORA_Bilinear)
    {
        // Convert from upper left corner of pixel coordinates to center of
        // pixel coordinates:
        const gdal::Vector2d df = inLoc - 0.5;
        const gdal::Vector2i d = df.floor().template cast<int>();
        const gdal::Vector2d delta = df - d.cast<double>();
        const int nKernelSize = 2;
        const gdal::Vector2i dOutOfBorder =
            outOfBorderCorrection(d, nKernelSize);

        // Bilinear interpolation.
        T adfReadData[4] = {0.0};
        if (!GDALInterpExtractValuesWindow(pBand, cache, d - dOutOfBorder,
                                           {nKernelSize, nKernelSize},
                                           adfReadData))
        {
            return FALSE;
        }
        dragReadDataInBorder(adfReadData, dOutOfBorder, nKernelSize);
        if (!applyBilinearKernel(delta, adfReadData, out))
            return FALSE;

        return TRUE;
    }
    else
    {
        const gdal::Vector2i d = inLoc.cast<int>();
        T adfOut[1] = {};
        if (!GDALInterpExtractValuesWindow(pBand, cache, d, {1, 1}, adfOut) ||
            (bGotNoDataValue && areEqualReal(dfNoDataValue, adfOut[0])))
        {
            return FALSE;
        }

        out = adfOut[0];

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

/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Helper code to implement overview support in different drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_priv.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "gdal.h"
// TODO(schwehr): Fix warning: Software emulation of SSE2.
// #include "gdalsse_priv.h"
#include "gdalwarper.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                     GDALResampleChunk32R_Near()                      */
/************************************************************************/

template <class T>
static CPLErr
GDALResampleChunk32R_NearT( double dfXRatioDstToSrc,
                            double dfYRatioDstToSrc,
                            GDALDataType eWrkDataType,
                            T * pChunk,
                            int nChunkXOff, int nChunkXSize,
                            int nChunkYOff,
                            int nDstXOff, int nDstXOff2,
                            int nDstYOff, int nDstYOff2,
                            GDALRasterBand * poOverview )

{
    const int nDstXWidth = nDstXOff2 - nDstXOff;

/* -------------------------------------------------------------------- */
/*      Allocate scanline buffer.                                       */
/* -------------------------------------------------------------------- */

    T* pDstScanline = static_cast<T *>(
        VSI_MALLOC_VERBOSE(
            nDstXWidth * GDALGetDataTypeSizeBytes(eWrkDataType) ) );
    int* panSrcXOff = static_cast<int *>(
        VSI_MALLOC_VERBOSE(nDstXWidth * sizeof(int)) );

    if( pDstScanline == NULL || panSrcXOff == NULL )
    {
        VSIFree(pDstScanline);
        VSIFree(panSrcXOff);
        return CE_Failure;
    }

/* ==================================================================== */
/*      Precompute inner loop constants.                                */
/* ==================================================================== */
    for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
    {
        int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
        if( nSrcXOff < nChunkXOff )
            nSrcXOff = nChunkXOff;

        panSrcXOff[iDstPixel - nDstXOff] = nSrcXOff;
    }

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    CPLErr eErr = CE_None;
    for( int iDstLine = nDstYOff;
         iDstLine < nDstYOff2 && eErr == CE_None;
         ++iDstLine )
    {
        int   nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        const T * const pSrcScanline =
            pChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize) - nChunkXOff;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel )
        {
            pDstScanline[iDstPixel] = pSrcScanline[panSrcXOff[iDstPixel]];
        }

        eErr = poOverview->RasterIO(
            GF_Write, nDstXOff, iDstLine, nDstXWidth, 1,
            pDstScanline, nDstXWidth, 1, eWrkDataType,
            0, 0, NULL );
    }

    CPLFree( pDstScanline );
    CPLFree( panSrcXOff );

    return eErr;
}

static CPLErr
GDALResampleChunk32R_Near( double dfXRatioDstToSrc,
                           double dfYRatioDstToSrc,
                           double /* dfSrcXDelta */,
                           double /* dfSrcYDelta */,
                           GDALDataType eWrkDataType,
                           void * pChunk,
                           GByte * /* pabyChunkNodataMask_unused */,
                           int nChunkXOff, int nChunkXSize,
                           int nChunkYOff, int /* nChunkYSize */,
                           int nDstXOff, int nDstXOff2,
                           int nDstYOff, int nDstYOff2,
                           GDALRasterBand * poOverview,
                           const char * /* pszResampling_unused */,
                           int /* bHasNoData_unused */,
                           float /* fNoDataValue_unused */,
                           GDALColorTable* /* poColorTable_unused */,
                           GDALDataType /* eSrcDataType */,
                           bool /* bPropagateNoData */ )
{
    if( eWrkDataType == GDT_Byte )
        return GDALResampleChunk32R_NearT(
            dfXRatioDstToSrc,
            dfYRatioDstToSrc,
            eWrkDataType,
            static_cast<GByte *>( pChunk ),
            nChunkXOff, nChunkXSize,
            nChunkYOff,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview );
    else if( eWrkDataType == GDT_UInt16 )
        return GDALResampleChunk32R_NearT(
            dfXRatioDstToSrc,
            dfYRatioDstToSrc,
            eWrkDataType,
            static_cast<GInt16 *>( pChunk ),
            nChunkXOff, nChunkXSize,
            nChunkYOff,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview );
    else if( eWrkDataType == GDT_Float32 )
        return GDALResampleChunk32R_NearT(
            dfXRatioDstToSrc,
            dfYRatioDstToSrc,
            eWrkDataType,
            static_cast<float *>( pChunk ),
            nChunkXOff, nChunkXSize,
            nChunkYOff,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview);

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                          GDALFindBestEntry()                         */
/************************************************************************/

// Find in the color table the entry whose (c1,c2,c3) value is the closest
// (using quadratic distance) to the passed (nR,nG,nB) triplet, ignoring
// transparent entries.
static int GDALFindBestEntry( int nEntryCount, const GDALColorEntry* aEntries,
                              int nR, int nG, int nB )
{
    int nMinDist = std::numeric_limits<int>::max();
    int iBestEntry = 0;
    for( int i = 0; i < nEntryCount; ++i )
    {
        // Ignore transparent entries
        if( aEntries[i].c4 == 0 )
            continue;
        int nDist = (nR - aEntries[i].c1) *  (nR - aEntries[i].c1) +
            (nG - aEntries[i].c2) *  (nG - aEntries[i].c2) +
            (nB - aEntries[i].c3) *  (nB - aEntries[i].c3);
        if( nDist < nMinDist )
        {
            nMinDist = nDist;
            iBestEntry = i;
        }
    }
    return iBestEntry;
}

/************************************************************************/
/*                      ReadColorTableAsArray()                         */
/************************************************************************/

static bool ReadColorTableAsArray( const GDALColorTable* poColorTable,
                                   int& nEntryCount,
                                   GDALColorEntry*& aEntries,
                                   int& nTransparentIdx )
{
    nEntryCount = poColorTable->GetColorEntryCount();
    aEntries = static_cast<GDALColorEntry *>(
        VSI_MALLOC2_VERBOSE(sizeof(GDALColorEntry), nEntryCount) );
    nTransparentIdx = -1;
    if( aEntries == NULL )
        return false;
    for( int i = 0; i < nEntryCount; ++i )
    {
        poColorTable->GetColorEntryAsRGB(i, &aEntries[i]);
        if( nTransparentIdx < 0 && aEntries[i].c4 == 0 )
            nTransparentIdx = i;
    }
    return true;
}

/************************************************************************/
/*                    GDALResampleChunk32R_Average()                    */
/************************************************************************/

template <class T, class Tsum>
static CPLErr
GDALResampleChunk32R_AverageT( double dfXRatioDstToSrc,
                               double dfYRatioDstToSrc,
                               double dfSrcXDelta,
                               double dfSrcYDelta,
                               GDALDataType eWrkDataType,
                               T* pChunk,
                               GByte * pabyChunkNodataMask,
                               int nChunkXOff, int nChunkXSize,
                               int nChunkYOff, int nChunkYSize,
                               int nDstXOff, int nDstXOff2,
                               int nDstYOff, int nDstYOff2,
                               GDALRasterBand * poOverview,
                               const char * pszResampling,
                               int bHasNoData,  // TODO(schwehr): bool.
                               float fNoDataValue,
                               GDALColorTable* poColorTable,
                               bool bPropagateNoData )
{
    // AVERAGE_BIT2GRAYSCALE
    const bool bBit2Grayscale =
        CPL_TO_BOOL( STARTS_WITH_CI( pszResampling, "AVERAGE_BIT2G" ) );
    if( bBit2Grayscale )
        poColorTable = NULL;

    T tNoDataValue;
    if( !bHasNoData )
        tNoDataValue = 0;
    else
        tNoDataValue = (T)fNoDataValue;

    int nChunkRightXOff = nChunkXOff + nChunkXSize;
    int nChunkBottomYOff = nChunkYOff + nChunkYSize;
    int nDstXWidth = nDstXOff2 - nDstXOff;

/* -------------------------------------------------------------------- */
/*      Allocate scanline buffer.                                       */
/* -------------------------------------------------------------------- */

    T *pDstScanline = static_cast<T *>(
        VSI_MALLOC_VERBOSE(
            nDstXWidth * GDALGetDataTypeSizeBytes(eWrkDataType) ) );
    int* panSrcXOffShifted = static_cast<int *>(
        VSI_MALLOC_VERBOSE(2 * nDstXWidth * sizeof(int) ) );

    if( pDstScanline == NULL || panSrcXOffShifted == NULL )
    {
        VSIFree(pDstScanline);
        VSIFree(panSrcXOffShifted);
        return CE_Failure;
    }

    int nEntryCount = 0;
    GDALColorEntry* aEntries = NULL;
    int nTransparentIdx = -1;

    if( poColorTable &&
        !ReadColorTableAsArray(poColorTable, nEntryCount, aEntries,
                               nTransparentIdx) )
    {
        VSIFree(pDstScanline);
        VSIFree(panSrcXOffShifted);
        return CE_Failure;
    }

    // Force c4 of nodata entry to 0 so that GDALFindBestEntry() identifies
    // it as nodata value
    if( bHasNoData && fNoDataValue >= 0.0f && tNoDataValue < nEntryCount )
    {
        if( aEntries == NULL )
        {
            CPLError(CE_Failure, CPLE_ObjectNull, "No aEntries.");
            VSIFree(pDstScanline);
            VSIFree(panSrcXOffShifted);
            return CE_Failure;
        }
        aEntries[static_cast<int>(tNoDataValue)].c4 = 0;
    }
    // Or if we have no explicit nodata, but a color table entry that is
    // transparent, consider it as the nodata value
    else if( !bHasNoData && nTransparentIdx >= 0 )
    {
        bHasNoData = TRUE;
        tNoDataValue = static_cast<T>(nTransparentIdx);
    }

/* ==================================================================== */
/*      Precompute inner loop constants.                                */
/* ==================================================================== */
    bool bSrcXSpacingIsTwo = true;
    for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
    {
        double dfSrcXOff = dfSrcXDelta + iDstPixel * dfXRatioDstToSrc;
        // Apply some epsilon to avoid numerical precision issues
        int nSrcXOff = static_cast<int>(dfSrcXOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfXRatioDstToSrc > 1 && dfSrcXOff - nSrcXOff > 0.9 &&
            nSrcXOff < nChunkRightXOff)
            nSrcXOff ++;
#endif
        if( nSrcXOff < nChunkXOff )
            nSrcXOff = nChunkXOff;

        double dfSrcXOff2 = dfSrcXDelta + (iDstPixel+1)* dfXRatioDstToSrc;
        int nSrcXOff2 = static_cast<int>(ceil(dfSrcXOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfXRatioDstToSrc > 1 && nSrcXOff2 - dfSrcXOff2 > 0.9 &&
            nSrcXOff2 > nChunkXOff)
            nSrcXOff2 --;
#endif
        if( nSrcXOff2 == nSrcXOff )
            nSrcXOff2 ++;
        if( nSrcXOff2 > nChunkRightXOff )
            nSrcXOff2 = nChunkRightXOff;

        panSrcXOffShifted[2 * (iDstPixel - nDstXOff)] = nSrcXOff - nChunkXOff;
        panSrcXOffShifted[2 * (iDstPixel - nDstXOff) + 1] =
            nSrcXOff2 - nChunkXOff;
        if( nSrcXOff2 - nSrcXOff != 2 )
            bSrcXSpacingIsTwo = false;
    }

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    CPLErr eErr = CE_None;
    for( int iDstLine = nDstYOff;
         iDstLine < nDstYOff2 && eErr == CE_None;
         ++iDstLine )
    {
        double dfSrcYOff = dfSrcYDelta + iDstLine * dfYRatioDstToSrc;
        int nSrcYOff = static_cast<int>(dfSrcYOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfYRatioDstToSrc > 1 && dfSrcYOff - nSrcYOff > 0.9 &&
            nSrcYOff < nChunkBottomYOff)
            nSrcYOff ++;
#endif
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        double dfSrcYOff2 = dfSrcYDelta + (iDstLine+1) * dfYRatioDstToSrc;
        int nSrcYOff2 = static_cast<int>(ceil(dfSrcYOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfYRatioDstToSrc > 1 && nSrcYOff2 - dfSrcYOff2 > 0.9 &&
            nSrcYOff2 > nChunkYOff)
            nSrcYOff2 --;
#endif
        if( nSrcYOff2 == nSrcYOff )
            ++nSrcYOff2;
        if( nSrcYOff2 > nChunkBottomYOff )
            nSrcYOff2 = nChunkBottomYOff;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        if( poColorTable == NULL )
        {
            if( bSrcXSpacingIsTwo && nSrcYOff2 == nSrcYOff + 2 &&
                pabyChunkNodataMask == NULL &&
                (eWrkDataType == GDT_Byte || eWrkDataType == GDT_UInt16) )
            {
                // Optimized case : no nodata, overview by a factor of 2 and
                // regular x and y src spacing.
                const T* pSrcScanlineShifted =
                    pChunk + panSrcXOffShifted[0] +
                    (nSrcYOff - nChunkYOff) * nChunkXSize;
                for( int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel )
                {
                    const Tsum nTotal =
                        pSrcScanlineShifted[0]
                        + pSrcScanlineShifted[1]
                        + pSrcScanlineShifted[nChunkXSize]
                        + pSrcScanlineShifted[1+nChunkXSize];

                    pDstScanline[iDstPixel] = (T) ((nTotal + 2) / 4);
                    pSrcScanlineShifted += 2;
                }
            }
            else
            {
                nSrcYOff -= nChunkYOff;
                nSrcYOff2 -= nChunkYOff;

                for( int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel )
                {
                    const int nSrcXOff = panSrcXOffShifted[2 * iDstPixel];
                    const int nSrcXOff2 = panSrcXOffShifted[2 * iDstPixel + 1];

                    Tsum dfTotal = 0;
                    int nCount = 0;

                    for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                    {
                        for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                        {
                            const T val = pChunk[iX + iY *nChunkXSize];
                            if( pabyChunkNodataMask == NULL ||
                                pabyChunkNodataMask[iX + iY *nChunkXSize] )
                            {
                                dfTotal += val;
                                ++nCount;
                            }
                        }
                    }

                    if( nCount == 0 ||
                        (bPropagateNoData && nCount <
                            (nSrcYOff2 - nSrcYOff) * (nSrcXOff2 - nSrcXOff)))
                    {
                        pDstScanline[iDstPixel] = tNoDataValue;
                    }
                    else if( eWrkDataType == GDT_Byte ||
                             eWrkDataType == GDT_UInt16)
                        pDstScanline[iDstPixel] =
                            static_cast<T>((dfTotal + nCount / 2) / nCount);
                    else
                        pDstScanline[iDstPixel] =
                            static_cast<T>(dfTotal / nCount);
                }
            }
        }
        else
        {
            nSrcYOff -= nChunkYOff;
            nSrcYOff2 -= nChunkYOff;

            for( int iDstPixel = 0; iDstPixel < nDstXWidth; ++iDstPixel )
            {
                const int nSrcXOff = panSrcXOffShifted[2 * iDstPixel];
                const int nSrcXOff2 = panSrcXOffShifted[2 * iDstPixel + 1];

                int nTotalR = 0;
                int nTotalG = 0;
                int nTotalB = 0;
                int nCount = 0;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        const T val = pChunk[iX + iY *nChunkXSize];
                        int nVal = static_cast<int>(val);
                        if( nVal >= 0 && nVal < nEntryCount &&
                            aEntries[nVal].c4 )
                        {
                            nTotalR += aEntries[nVal].c1;
                            nTotalG += aEntries[nVal].c2;
                            nTotalB += aEntries[nVal].c3;
                            ++nCount;
                        }
                    }
                }

                if( nCount == 0 ||
                    (bPropagateNoData && nCount <
                        (nSrcYOff2 - nSrcYOff) * (nSrcXOff2 - nSrcXOff)) )
                {
                    pDstScanline[iDstPixel] = tNoDataValue;
                }
                else
                {
                    int nR = (nTotalR + nCount / 2) / nCount,
                        nG = (nTotalG + nCount / 2) / nCount,
                        nB = (nTotalB + nCount / 2) / nCount;
                    pDstScanline[iDstPixel] = (T)GDALFindBestEntry(
                        nEntryCount, aEntries, nR, nG, nB);
                }
            }
        }

        eErr = poOverview->RasterIO(
            GF_Write, nDstXOff, iDstLine, nDstXWidth, 1,
            pDstScanline, nDstXWidth, 1, eWrkDataType,
            0, 0, NULL );
    }

    CPLFree( pDstScanline );
    CPLFree( aEntries );
    CPLFree( panSrcXOffShifted );

    return eErr;
}

static CPLErr
GDALResampleChunk32R_Average( double dfXRatioDstToSrc, double dfYRatioDstToSrc,
                              double dfSrcXDelta,
                              double dfSrcYDelta,
                              GDALDataType eWrkDataType,
                              void * pChunk,
                              GByte * pabyChunkNodataMask,
                              int nChunkXOff, int nChunkXSize,
                              int nChunkYOff, int nChunkYSize,
                              int nDstXOff, int nDstXOff2,
                              int nDstYOff, int nDstYOff2,
                              GDALRasterBand * poOverview,
                              const char * pszResampling,
                              int bHasNoData, float fNoDataValue,
                              GDALColorTable* poColorTable,
                              GDALDataType /* eSrcDataType */,
                              bool bPropagateNoData )
{
    if( eWrkDataType == GDT_Byte )
        return GDALResampleChunk32R_AverageT<GByte, int>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            eWrkDataType,
            static_cast<GByte *>( pChunk ),
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            pszResampling,
            bHasNoData, fNoDataValue,
            poColorTable,
            bPropagateNoData );
    else if( eWrkDataType == GDT_UInt16 &&
             dfXRatioDstToSrc * dfYRatioDstToSrc < 65536 )
        return GDALResampleChunk32R_AverageT<GUInt16, GUInt32>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            eWrkDataType,
            static_cast<GUInt16 *>( pChunk ),
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            pszResampling,
            bHasNoData, fNoDataValue,
            poColorTable,
            bPropagateNoData );
    else if( eWrkDataType == GDT_Float32 )
        return GDALResampleChunk32R_AverageT<float, double>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            eWrkDataType,
            static_cast<float *>( pChunk ),
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            poOverview,
            pszResampling,
            bHasNoData, fNoDataValue,
            poColorTable,
            bPropagateNoData );

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                    GDALResampleChunk32R_Gauss()                      */
/************************************************************************/

static CPLErr
GDALResampleChunk32R_Gauss( double dfXRatioDstToSrc, double dfYRatioDstToSrc,
                            double /* dfSrcXDelta */,
                            double /* dfSrcYDelta */,
                            GDALDataType /* eWrkDataType */,
                            void * pChunk,
                            GByte * pabyChunkNodataMask,
                            int nChunkXOff, int nChunkXSize,
                            int nChunkYOff, int nChunkYSize,
                            int nDstXOff, int nDstXOff2,
                            int nDstYOff, int nDstYOff2,
                            GDALRasterBand * poOverview,
                            const char * /* pszResampling */,
                            int bHasNoData, float fNoDataValue,
                            GDALColorTable* poColorTable,
                            GDALDataType /* eSrcDataType */,
                            bool /* bPropagateNoData */ )

{
    float * pafChunk = static_cast<float *>( pChunk );

/* -------------------------------------------------------------------- */
/*      Create the filter kernel and allocate scanline buffer.          */
/* -------------------------------------------------------------------- */
    int nGaussMatrixDim = 3;
    const int *panGaussMatrix;
    static const int anGaussMatrix3x3[] ={
        1, 2, 1,
        2, 4, 2,
        1, 2, 1
    };
    static const int anGaussMatrix5x5[] = {
        1, 4, 6, 4, 1,
        4, 16, 24, 16, 4,
        6, 24, 36, 24, 6,
        4, 16, 24, 16, 4,
        1, 4, 6, 4, 1};
    static const int anGaussMatrix7x7[] = {
        1, 6, 15, 20, 15, 6, 1,
        6, 36, 90, 120, 90, 36, 6,
        15, 90, 225, 300, 225, 90, 15,
        20, 120, 300, 400, 300, 120, 20,
        15, 90, 225, 300, 225, 90, 15,
        6, 36, 90, 120, 90, 36, 6,
        1, 6, 15, 20, 15, 6, 1};

    const int nOXSize = poOverview->GetXSize();
    const int nOYSize = poOverview->GetYSize();
    const int nResYFactor = static_cast<int>(0.5 + dfYRatioDstToSrc);

    // matrix for gauss filter
    if(nResYFactor <= 2 )
    {
        panGaussMatrix = anGaussMatrix3x3;
        nGaussMatrixDim=3;
    }
    else if( nResYFactor <= 4 )
    {
        panGaussMatrix = anGaussMatrix5x5;
        nGaussMatrixDim=5;
    }
    else
    {
        panGaussMatrix = anGaussMatrix7x7;
        nGaussMatrixDim=7;
    }

    float *pafDstScanline = static_cast<float *>(
        VSI_MALLOC_VERBOSE((nDstXOff2 - nDstXOff) * sizeof(float)) );
    if( pafDstScanline == NULL )
    {
        return CE_Failure;
    }

    if( !bHasNoData )
        fNoDataValue = 0.0f;

    int nEntryCount = 0;
    GDALColorEntry* aEntries = NULL;
    int nTransparentIdx = -1;
    if( poColorTable &&
        !ReadColorTableAsArray(poColorTable, nEntryCount, aEntries,
                               nTransparentIdx) )
    {
        VSIFree(pafDstScanline);
        return CE_Failure;
    }

    // Force c4 of nodata entry to 0 so that GDALFindBestEntry() identifies
    // it as nodata value.
    if( bHasNoData && fNoDataValue >= 0.0f && fNoDataValue < nEntryCount )
    {
        if( aEntries == NULL )
        {
            CPLError(CE_Failure, CPLE_ObjectNull, "No aEntries");
            VSIFree(pafDstScanline);
            return CE_Failure;
        }
        aEntries[static_cast<int>(fNoDataValue)].c4 = 0;
    }
    // Or if we have no explicit nodata, but a color table entry that is
    // transparent, consider it as the nodata value.
    else if( !bHasNoData && nTransparentIdx >= 0 )
    {
        fNoDataValue = static_cast<float>(nTransparentIdx);
    }

    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    CPLErr eErr = CE_None;
    for( int iDstLine = nDstYOff;
         iDstLine < nDstYOff2 && eErr == CE_None;
         ++iDstLine )
    {
        int nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        int nSrcYOff2 =
            static_cast<int>(0.5 + (iDstLine+1) * dfYRatioDstToSrc) + 1;

        if( nSrcYOff < nChunkYOff )
        {
            nSrcYOff = nChunkYOff;
            nSrcYOff2++;
        }

        const int iSizeY = nSrcYOff2 - nSrcYOff;
        nSrcYOff = nSrcYOff + iSizeY/2 - nGaussMatrixDim/2;
        nSrcYOff2 = nSrcYOff + nGaussMatrixDim;
        int nYShiftGaussMatrix = 0;
        if(nSrcYOff < 0)
        {
            nYShiftGaussMatrix = -nSrcYOff;
            nSrcYOff = 0;
        }

        if( nSrcYOff2 > nChunkBottomYOff ||
            (dfYRatioDstToSrc > 1 && iDstLine == nOYSize-1) )
            nSrcYOff2 = nChunkBottomYOff;

        const float * const pafSrcScanline =
            pafChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        GByte *pabySrcScanlineNodataMask = NULL;
        if( pabyChunkNodataMask != NULL )
            pabySrcScanlineNodataMask =
                pabyChunkNodataMask + ((nSrcYOff-nChunkYOff) * nChunkXSize);

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
        {
            int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
            int nSrcXOff2 =
                static_cast<int>(0.5 + (iDstPixel+1) * dfXRatioDstToSrc) + 1;

            const int iSizeX = nSrcXOff2 - nSrcXOff;
            nSrcXOff = nSrcXOff + iSizeX/2 - nGaussMatrixDim/2;
            nSrcXOff2 = nSrcXOff + nGaussMatrixDim;
            int nXShiftGaussMatrix = 0;
            if(nSrcXOff < 0)
            {
                nXShiftGaussMatrix = -nSrcXOff;
                nSrcXOff = 0;
            }

            if( nSrcXOff2 > nChunkRightXOff ||
                (dfXRatioDstToSrc > 1 && iDstPixel == nOXSize-1) )
                nSrcXOff2 = nChunkRightXOff;

            if( poColorTable == NULL )
            {
                double dfTotal = 0.0;
                int nCount = 0;
                const int *panLineWeight = panGaussMatrix +
                    nYShiftGaussMatrix * nGaussMatrixDim + nXShiftGaussMatrix;

                for( int j=0, iY = nSrcYOff;
                     iY < nSrcYOff2;
                     ++iY, ++j, panLineWeight += nGaussMatrixDim )
                {
                    for( int i=0, iX = nSrcXOff; iX < nSrcXOff2; ++iX, ++i )
                    {
                        const double val =
                            pafSrcScanline[iX-nChunkXOff+(iY-nSrcYOff)
                                           * nChunkXSize];
                        if( pabySrcScanlineNodataMask == NULL ||
                            pabySrcScanlineNodataMask[iX - nChunkXOff
                                                      +(iY - nSrcYOff)
                                                      * nChunkXSize] )
                        {
                            const int nWeight = panLineWeight[i];
                            dfTotal += val * nWeight;
                            nCount += nWeight;
                        }
                    }
                }

                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                }
                else
                {
                    pafDstScanline[iDstPixel - nDstXOff] =
                        static_cast<float>(dfTotal / nCount);
                }
            }
            else
            {
                int nTotalR = 0;
                int nTotalG = 0;
                int nTotalB = 0;
                int nTotalWeight = 0;
                const int *panLineWeight =
                    panGaussMatrix + nYShiftGaussMatrix * nGaussMatrixDim +
                    nXShiftGaussMatrix;

                for( int j=0, iY = nSrcYOff; iY < nSrcYOff2;
                        ++iY, ++j, panLineWeight += nGaussMatrixDim )
                {
                    for( int i=0, iX = nSrcXOff; iX < nSrcXOff2; ++iX, ++i )
                    {
                        const double val =
                            pafSrcScanline[iX - nChunkXOff +
                                           (iY-nSrcYOff) * nChunkXSize];
                        int nVal = static_cast<int>(val);
                        if( nVal >= 0 && nVal < nEntryCount &&
                            aEntries[nVal].c4 )
                        {
                            const int nWeight = panLineWeight[i];
                            nTotalR += aEntries[nVal].c1 * nWeight;
                            nTotalG += aEntries[nVal].c2 * nWeight;
                            nTotalB += aEntries[nVal].c3 * nWeight;
                            nTotalWeight += nWeight;
                        }
                    }
                }

                if( nTotalWeight == 0 )
                {
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                }
                else
                {
                    const int nR =
                        (nTotalR + nTotalWeight / 2) / nTotalWeight;
                    const int nG =
                        (nTotalG + nTotalWeight / 2) / nTotalWeight;
                    const int nB =
                        (nTotalB + nTotalWeight / 2) / nTotalWeight;
                    pafDstScanline[iDstPixel - nDstXOff] =
                        static_cast<float>( GDALFindBestEntry(
                            nEntryCount, aEntries, nR, nG, nB ) );
                }
            }
        }

        eErr = poOverview->RasterIO(
            GF_Write, nDstXOff, iDstLine, nDstXOff2 - nDstXOff, 1,
            pafDstScanline, nDstXOff2 - nDstXOff, 1, GDT_Float32,
            0, 0, NULL );
    }

    CPLFree( pafDstScanline );
    CPLFree( aEntries );

    return eErr;
}

/************************************************************************/
/*                    GDALResampleChunk32R_Mode()                       */
/************************************************************************/

static CPLErr
GDALResampleChunk32R_Mode( double dfXRatioDstToSrc, double dfYRatioDstToSrc,
                           double dfSrcXDelta,
                           double dfSrcYDelta,
                           GDALDataType /* eWrkDataType */,
                           void * pChunk,
                           GByte * pabyChunkNodataMask,
                           int nChunkXOff, int nChunkXSize,
                           int nChunkYOff, int nChunkYSize,
                           int nDstXOff, int nDstXOff2,
                           int nDstYOff, int nDstYOff2,
                           GDALRasterBand * poOverview,
                           const char * /* pszResampling */,
                           int bHasNoData, float fNoDataValue,
                           GDALColorTable* poColorTable,
                           GDALDataType eSrcDataType,
                           bool /* bPropagateNoData */ )

{
    float * pafChunk = static_cast<float*>( pChunk );

/* -------------------------------------------------------------------- */
/*      Create the filter kernel and allocate scanline buffer.          */
/* -------------------------------------------------------------------- */
    float *pafDstScanline = static_cast<float *>(
        VSI_MALLOC_VERBOSE((nDstXOff2 - nDstXOff) * sizeof(float)) );
    if( pafDstScanline == NULL )
    {
        return CE_Failure;
    }

    if( !bHasNoData )
        fNoDataValue = 0.0f;
    int nEntryCount = 0;
    GDALColorEntry* aEntries = NULL;
    int nTransparentIdx = -1;
    if( poColorTable &&
        !ReadColorTableAsArray(poColorTable, nEntryCount,
                               aEntries, nTransparentIdx) )
    {
        VSIFree(pafDstScanline);
        return CE_Failure;
    }

    int nMaxNumPx = 0;
    float *pafVals = NULL;
    int *panSums = NULL;

    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    CPLErr eErr = CE_None;
    for( int iDstLine = nDstYOff;
         iDstLine < nDstYOff2 && eErr == CE_None;
         ++iDstLine )
    {
        double dfSrcYOff = dfSrcYDelta + iDstLine * dfYRatioDstToSrc;
        int nSrcYOff = static_cast<int>(dfSrcYOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfYRatioDstToSrc > 1 && dfSrcYOff - nSrcYOff > 0.9 &&
            nSrcYOff < nChunkBottomYOff)
            nSrcYOff ++;
#endif
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        double dfSrcYOff2 = dfSrcYDelta + (iDstLine+1) * dfYRatioDstToSrc;
        int nSrcYOff2 = static_cast<int>(ceil(dfSrcYOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
        // When oversampling, don't take into account pixels that have a tiny
        // participation in the resulting pixel
        if( dfYRatioDstToSrc > 1 && nSrcYOff2 - dfSrcYOff2 > 0.9 &&
            nSrcYOff2 > nChunkYOff)
            nSrcYOff2 --;
#endif
        if( nSrcYOff2 == nSrcYOff )
            ++nSrcYOff2;
        if( nSrcYOff2 > nChunkBottomYOff )
            nSrcYOff2 = nChunkBottomYOff;

        const float * const pafSrcScanline =
            pafChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        GByte *pabySrcScanlineNodataMask = NULL;
        if( pabyChunkNodataMask != NULL )
            pabySrcScanlineNodataMask =
                pabyChunkNodataMask + (nSrcYOff-nChunkYOff) * nChunkXSize;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
        {
            double dfSrcXOff = dfSrcXDelta + iDstPixel * dfXRatioDstToSrc;
            // Apply some epsilon to avoid numerical precision issues
            int nSrcXOff = static_cast<int>(dfSrcXOff + 1e-8);
#ifdef only_pixels_with_more_than_10_pct_participation
            // When oversampling, don't take into account pixels that have a tiny
            // participation in the resulting pixel
            if( dfXRatioDstToSrc > 1 && dfSrcXOff - nSrcXOff > 0.9 &&
                nSrcXOff < nChunkRightXOff)
                nSrcXOff ++;
#endif
            if( nSrcXOff < nChunkXOff )
                nSrcXOff = nChunkXOff;

            double dfSrcXOff2 = dfSrcXDelta + (iDstPixel+1)* dfXRatioDstToSrc;
            int nSrcXOff2 = static_cast<int>(ceil(dfSrcXOff2 - 1e-8));
#ifdef only_pixels_with_more_than_10_pct_participation
            // When oversampling, don't take into account pixels that have a tiny
            // participation in the resulting pixel
            if( dfXRatioDstToSrc > 1 && nSrcXOff2 - dfSrcXOff2 > 0.9 &&
                nSrcXOff2 > nChunkXOff)
                nSrcXOff2 --;
#endif
            if( nSrcXOff2 == nSrcXOff )
                nSrcXOff2 ++;
            if( nSrcXOff2 > nChunkRightXOff )
                nSrcXOff2 = nChunkRightXOff;

            if( eSrcDataType != GDT_Byte || nEntryCount > 256 )
            {
                // Not sure how much sense it makes to run a majority
                // filter on floating point data, but here it is for the sake
                // of compatibility. It won't look right on RGB images by the
                // nature of the filter.
                int nNumPx = (nSrcYOff2-nSrcYOff)*(nSrcXOff2-nSrcXOff);
                int iMaxInd = 0;
                int iMaxVal = -1;

                if( pafVals == NULL || nNumPx > nMaxNumPx )
                {
                    pafVals = static_cast<float *>(
                        CPLRealloc(pafVals, nNumPx * sizeof(float)) );
                    panSums = static_cast<int *>(
                        CPLRealloc(panSums, nNumPx * sizeof(int)) );
                    nMaxNumPx = nNumPx;
                }

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    const int iTotYOff = (iY-nSrcYOff)*nChunkXSize-nChunkXOff;
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        if( pabySrcScanlineNodataMask == NULL ||
                            pabySrcScanlineNodataMask[iX+iTotYOff] )
                        {
                            const float fVal = pafSrcScanline[iX+iTotYOff];
                            int i = 0;  // Used after for.

                            // Check array for existing entry.
                            for( ; i < iMaxInd; ++i )
                                if( pafVals[i] == fVal
                                    && ++panSums[i] > panSums[iMaxVal] )
                                {
                                    iMaxVal = i;
                                    break;
                                }

                            // Add to arr if entry not already there.
                            if( i == iMaxInd )
                            {
                                pafVals[iMaxInd] = fVal;
                                panSums[iMaxInd] = 1;

                                if( iMaxVal < 0 )
                                    iMaxVal = iMaxInd;

                                ++iMaxInd;
                            }
                        }
                    }
                }

                if( iMaxVal == -1 )
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                else
                    pafDstScanline[iDstPixel - nDstXOff] = pafVals[iMaxVal];
            }
            else // if( eSrcDataType == GDT_Byte && nEntryCount < 256 )
            {
                // So we go here for a paletted or non-paletted byte band.
                // The input values are then between 0 and 255.
                std::vector<int> anVals(256, 0);
                int nMaxVal = 0;
                int iMaxInd = -1;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    const int iTotYOff =
                        (iY - nSrcYOff) * nChunkXSize - nChunkXOff;
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        const float val = pafSrcScanline[iX+iTotYOff];
                        if( bHasNoData == FALSE || val != fNoDataValue )
                        {
                            int nVal = static_cast<int>(val);
                            if( ++anVals[nVal] > nMaxVal)
                            {
                                // Sum the density.
                                // Is it the most common value so far?
                                iMaxInd = nVal;
                                nMaxVal = anVals[nVal];
                            }
                        }
                    }
                }

                if( iMaxInd == -1 )
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                else
                    pafDstScanline[iDstPixel - nDstXOff] =
                        static_cast<float>(iMaxInd);
            }
        }

        eErr = poOverview->RasterIO(
            GF_Write, nDstXOff, iDstLine, nDstXOff2 - nDstXOff, 1,
            pafDstScanline, nDstXOff2 - nDstXOff, 1, GDT_Float32,
            0, 0, NULL );
    }

    CPLFree( pafDstScanline );
    CPLFree( aEntries );
    CPLFree( pafVals );
    CPLFree( panSums );

    return eErr;
}

/************************************************************************/
/*                  GDALResampleConvolutionHorizontal()                 */
/************************************************************************/

template<class T> static inline double GDALResampleConvolutionHorizontal(
    const T* pChunk, const double* padfWeights, int nSrcPixelCount )
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    int i = 0;  // Used after for.
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        dfVal1 += pChunk[i] * padfWeights[i];
        dfVal1 += pChunk[i+1] * padfWeights[i+1];
        dfVal2 += pChunk[i+2] * padfWeights[i+2];
        dfVal2 += pChunk[i+3] * padfWeights[i+3];
    }
    for( ; i < nSrcPixelCount; ++i )
    {
        dfVal1 += pChunk[i] * padfWeights[i];
    }
    return dfVal1 + dfVal2;
}

template<class T> static inline void GDALResampleConvolutionHorizontalWithMask(
    const T* pChunk, const GByte* pabyMask,
    const double* padfWeights, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum)
{
    dfVal = 0;
    dfWeightSum = 0;
    int i = 0;
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        const double dfWeight0 = padfWeights[i] * pabyMask[i];
        const double dfWeight1 = padfWeights[i+1] * pabyMask[i+1];
        const double dfWeight2 = padfWeights[i+2] * pabyMask[i+2];
        const double dfWeight3 = padfWeights[i+3] * pabyMask[i+3];
        dfVal += pChunk[i] * dfWeight0;
        dfVal += pChunk[i+1] * dfWeight1;
        dfVal += pChunk[i+2] * dfWeight2;
        dfVal += pChunk[i+3] * dfWeight3;
        dfWeightSum += dfWeight0 + dfWeight1 + dfWeight2 + dfWeight3;
    }
    for( ; i < nSrcPixelCount; ++i )
    {
        const double dfWeight = padfWeights[i] * pabyMask[i];
        dfVal += pChunk[i] * dfWeight;
        dfWeightSum += dfWeight;
    }
}

template<class T> static inline void GDALResampleConvolutionHorizontal_3rows(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeights, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3)
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    double dfVal3 = 0.0;
    double dfVal4 = 0.0;
    double dfVal5 = 0.0;
    double dfVal6 = 0.0;
    int i = 0;  // Used after for.
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        dfVal1 += pChunkRow1[i] * padfWeights[i];
        dfVal1 += pChunkRow1[i+1] * padfWeights[i+1];
        dfVal2 += pChunkRow1[i+2] * padfWeights[i+2];
        dfVal2 += pChunkRow1[i+3] * padfWeights[i+3];
        dfVal3 += pChunkRow2[i] * padfWeights[i];
        dfVal3 += pChunkRow2[i+1] * padfWeights[i+1];
        dfVal4 += pChunkRow2[i+2] * padfWeights[i+2];
        dfVal4 += pChunkRow2[i+3] * padfWeights[i+3];
        dfVal5 += pChunkRow3[i] * padfWeights[i];
        dfVal5 += pChunkRow3[i+1] * padfWeights[i+1];
        dfVal6 += pChunkRow3[i+2] * padfWeights[i+2];
        dfVal6 += pChunkRow3[i+3] * padfWeights[i+3];
    }
    for( ; i < nSrcPixelCount; ++i )
    {
        dfVal1 += pChunkRow1[i] * padfWeights[i];
        dfVal3 += pChunkRow2[i] * padfWeights[i];
        dfVal5 += pChunkRow3[i] * padfWeights[i];
    }
    dfRes1 = dfVal1 + dfVal2;
    dfRes2 = dfVal3 + dfVal4;
    dfRes3 = dfVal5 + dfVal6;
}

template<class T> static inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeights, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontal_3rows(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeights, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

/************************************************************************/
/*                  GDALResampleConvolutionVertical()                   */
/************************************************************************/

template<class T> static inline double GDALResampleConvolutionVertical(
    const T* pChunk, int nStride, const double* padfWeights, int nSrcLineCount )
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    int i = 0;
    int j = 0;
    for( ; i + 3 < nSrcLineCount; i+=4, j+=4*nStride)
    {
        dfVal1 += pChunk[j] * padfWeights[i];
        dfVal1 += pChunk[j + nStride] * padfWeights[i+1];
        dfVal2 += pChunk[j + 2 * nStride] * padfWeights[i+2];
        dfVal2 += pChunk[j + 3 * nStride] * padfWeights[i+3];
    }
    for( ; i < nSrcLineCount; ++i, j += nStride)
    {
        dfVal1 += pChunk[j] * padfWeights[i];
    }
    return dfVal1 + dfVal2;
}

template<class T> static inline void GDALResampleConvolutionVertical_2cols(
    const T* pChunk, int nStride, const double* padfWeights, int nSrcLineCount,
    double& dfRes1, double& dfRes2 )
{
    double dfVal1 = 0.0;
    double dfVal2 = 0.0;
    double dfVal3 = 0.0;
    double dfVal4 = 0.0;
    int i = 0;
    int j = 0;
    for(;i+3<nSrcLineCount;i+=4, j+=4*nStride)
    {
        dfVal1 += pChunk[j] * padfWeights[i];
        dfVal3 += pChunk[j+1] * padfWeights[i];
        dfVal1 += pChunk[j + nStride] * padfWeights[i+1];
        dfVal3 += pChunk[j+1 + nStride] * padfWeights[i+1];
        dfVal2 += pChunk[j + 2 * nStride] * padfWeights[i+2];
        dfVal4 += pChunk[j+1 + 2 * nStride] * padfWeights[i+2];
        dfVal2 += pChunk[j + 3 * nStride] * padfWeights[i+3];
        dfVal4 += pChunk[j+1 + 3 * nStride] * padfWeights[i+3];
    }
    for( ; i < nSrcLineCount; ++i, j += nStride )
    {
        dfVal1 += pChunk[j] * padfWeights[i];
        dfVal3 += pChunk[j+1] * padfWeights[i];
    }
    dfRes1 = dfVal1 + dfVal2;
    dfRes2 = dfVal3 + dfVal4;
}

// TODO(schwehr): Move define of USE_SSE2 and include to the top of the file.
// Restrict to 64bit processors because they are guaranteed to have SSE2.
// Could possibly be used too on 32bit, but we would need to check at runtime.
#if defined(__x86_64) || defined(_M_X64)
#define USE_SSE2
#endif

#ifdef USE_SSE2
#include <gdalsse_priv.h>

/************************************************************************/
/*              GDALResampleConvolutionHorizontalSSE2<T>                */
/************************************************************************/

template<class T> static inline double GDALResampleConvolutionHorizontalSSE2(
    const T* pChunk, const double* padfWeightsAligned, int nSrcPixelCount )
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    int i = 0;  // Used after for.
    for( ; i + 7 < nSrcPixelCount; i += 8 )
    {
        // Retrieve the pixel & accumulate
        const XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunk+i);
        const XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunk+i+4);
        const XMMReg4Double v_weight1 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i);
        const XMMReg4Double v_weight2 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i+4);

        v_acc1 += v_pixels1 * v_weight1;
        v_acc2 += v_pixels2 * v_weight2;
    }

    v_acc1 += v_acc2;
    v_acc1.AddLowAndHigh();

    double dfVal = static_cast<double>(v_acc1.GetLow());
    for( ; i < nSrcPixelCount; ++i )
    {
        dfVal += pChunk[i] * padfWeightsAligned[i];
    }
    return dfVal;
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontal<GByte>                */
/************************************************************************/

template<> inline double GDALResampleConvolutionHorizontal<GByte>(
    const GByte* pChunk, const double* padfWeightsAligned, int nSrcPixelCount )
{
    return GDALResampleConvolutionHorizontalSSE2( pChunk, padfWeightsAligned,
                                                  nSrcPixelCount );
}

template<> inline double GDALResampleConvolutionHorizontal<GUInt16>(
    const GUInt16* pChunk, const double* padfWeightsAligned,
    int nSrcPixelCount )
{
    return GDALResampleConvolutionHorizontalSSE2( pChunk, padfWeightsAligned,
                                                  nSrcPixelCount) ;
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontalWithMaskSSE2<T>        */
/************************************************************************/

template<class T> static inline void
GDALResampleConvolutionHorizontalWithMaskSSE2(
    const T* pChunk, const GByte* pabyMask,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum )
{
    int i = 0;  // Used after for.
    XMMReg4Double v_acc = XMMReg4Double::Zero();
    XMMReg4Double v_acc_weight = XMMReg4Double::Zero();
    for( ; i + 3 < nSrcPixelCount; i += 4 )
    {
        const XMMReg4Double v_pixels = XMMReg4Double::Load4Val(pChunk+i);
        const XMMReg4Double v_mask = XMMReg4Double::Load4Val(pabyMask+i);
        XMMReg4Double v_weight =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i);
        v_weight *= v_mask;
        v_acc += v_pixels * v_weight;
        v_acc_weight += v_weight;
    }
    v_acc.AddLowAndHigh();
    v_acc_weight.AddLowAndHigh();
    dfVal = static_cast<double>(v_acc.GetLow());
    dfWeightSum = static_cast<double>(v_acc_weight.GetLow());
    for( ; i < nSrcPixelCount; ++i )
    {
        const double dfWeight = padfWeightsAligned[i] * pabyMask[i];
        dfVal += pChunk[i] * dfWeight;
        dfWeightSum += dfWeight;
    }
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontalWithMask<GByte>        */
/************************************************************************/

template<> inline void GDALResampleConvolutionHorizontalWithMask<GByte>(
    const GByte* pChunk, const GByte* pabyMask,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum)
{
    GDALResampleConvolutionHorizontalWithMaskSSE2(pChunk, pabyMask,
                                                  padfWeightsAligned,
                                                  nSrcPixelCount,
                                                  dfVal, dfWeightSum);
}

template<> inline void GDALResampleConvolutionHorizontalWithMask<GUInt16>(
    const GUInt16* pChunk, const GByte* pabyMask,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfVal, double &dfWeightSum )
{
    GDALResampleConvolutionHorizontalWithMaskSSE2( pChunk, pabyMask,
                                                   padfWeightsAligned,
                                                   nSrcPixelCount,
                                                   dfVal, dfWeightSum );
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontal_3rows_SSE2<T>         */
/************************************************************************/

template<class T> static inline void
GDALResampleConvolutionHorizontal_3rows_SSE2(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero(),
                  v_acc2 = XMMReg4Double::Zero(),
                  v_acc3 = XMMReg4Double::Zero();
    int i = 0;
    for( ; i + 7 < nSrcPixelCount; i += 8 )
    {
        // Retrieve the pixel & accumulate.
        XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunkRow1+i);
        XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunkRow1+i+4);
        const XMMReg4Double v_weight1 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i);
        const XMMReg4Double v_weight2 =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned+i+4);

        v_acc1 += v_pixels1 * v_weight1;
        v_acc1 += v_pixels2 * v_weight2;

        v_pixels1 = XMMReg4Double::Load4Val(pChunkRow2+i);
        v_pixels2 = XMMReg4Double::Load4Val(pChunkRow2+i+4);
        v_acc2 += v_pixels1 * v_weight1;
        v_acc2 += v_pixels2 * v_weight2;

        v_pixels1 = XMMReg4Double::Load4Val(pChunkRow3+i);
        v_pixels2 = XMMReg4Double::Load4Val(pChunkRow3+i+4);
        v_acc3 += v_pixels1 * v_weight1;
        v_acc3 += v_pixels2 * v_weight2;
    }

    v_acc1.AddLowAndHigh();
    v_acc2.AddLowAndHigh();
    v_acc3.AddLowAndHigh();

    dfRes1 = static_cast<double>(v_acc1.GetLow());
    dfRes2 = static_cast<double>(v_acc2.GetLow());
    dfRes3 = static_cast<double>(v_acc3.GetLow());
    for( ; i < nSrcPixelCount; ++i )
    {
        dfRes1 += pChunkRow1[i] * padfWeightsAligned[i];
        dfRes2 += pChunkRow2[i] * padfWeightsAligned[i];
        dfRes3 += pChunkRow3[i] * padfWeightsAligned[i];
    }
}

/************************************************************************/
/*              GDALResampleConvolutionHorizontal_3rows<GByte>          */
/************************************************************************/

template<> inline void GDALResampleConvolutionHorizontal_3rows<GByte>(
    const GByte* pChunkRow1, const GByte* pChunkRow2, const GByte* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontal_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

template<> inline void GDALResampleConvolutionHorizontal_3rows<GUInt16>(
    const GUInt16* pChunkRow1, const GUInt16* pChunkRow2,
    const GUInt16* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontal_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3);
}

/************************************************************************/
/*     GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2<T>   */
/************************************************************************/

template<class T> static inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
    const T* pChunkRow1, const T* pChunkRow2, const T* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3)
{
    XMMReg4Double v_acc1 = XMMReg4Double::Zero();
    XMMReg4Double v_acc2 = XMMReg4Double::Zero();
    XMMReg4Double v_acc3 = XMMReg4Double::Zero();
    int i = 0;  // Use after for.
    for( ; i + 3 < nSrcPixelCount; i += 4)
    {
        // Retrieve the pixel & accumulate.
        const XMMReg4Double v_pixels1 = XMMReg4Double::Load4Val(pChunkRow1+i);
        const XMMReg4Double v_pixels2 = XMMReg4Double::Load4Val(pChunkRow2+i);
        const XMMReg4Double v_pixels3 = XMMReg4Double::Load4Val(pChunkRow3+i);
        const XMMReg4Double v_weight =
            XMMReg4Double::Load4ValAligned(padfWeightsAligned + i);

        v_acc1 += v_pixels1 * v_weight;
        v_acc2 += v_pixels2 * v_weight;
        v_acc3 += v_pixels3 * v_weight;
    }

    v_acc1.AddLowAndHigh();
    v_acc2.AddLowAndHigh();
    v_acc3.AddLowAndHigh();

    dfRes1 = static_cast<double>(v_acc1.GetLow());
    dfRes2 = static_cast<double>(v_acc2.GetLow());
    dfRes3 = static_cast<double>(v_acc3.GetLow());

    for( ; i < nSrcPixelCount; ++i )
    {
        dfRes1 += pChunkRow1[i] * padfWeightsAligned[i];
        dfRes2 += pChunkRow2[i] * padfWeightsAligned[i];
        dfRes3 += pChunkRow3[i] * padfWeightsAligned[i];
    }
}

/************************************************************************/
/*     GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GByte>    */
/************************************************************************/

template<> inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GByte>(
    const GByte* pChunkRow1, const GByte* pChunkRow2, const GByte* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

template<> inline void
GDALResampleConvolutionHorizontalPixelCountLess8_3rows<GUInt16>(
    const GUInt16* pChunkRow1, const GUInt16* pChunkRow2,
    const GUInt16* pChunkRow3,
    const double* padfWeightsAligned, int nSrcPixelCount,
    double& dfRes1, double& dfRes2, double& dfRes3 )
{
    GDALResampleConvolutionHorizontalPixelCountLess8_3rows_SSE2(
        pChunkRow1, pChunkRow2, pChunkRow3,
        padfWeightsAligned, nSrcPixelCount,
        dfRes1, dfRes2, dfRes3 );
}

#endif  // USE_SSE2

/************************************************************************/
/*                   GDALResampleChunk32R_Convolution()                 */
/************************************************************************/

// TODO(schwehr): Does bMultipleBands really have to be a part of the template?

// class MSVCPedanticBool fails with bMultipleBands being a bool.
template<class T, EMULATED_BOOL bMultipleBands> static CPLErr
GDALResampleChunk32R_ConvolutionT( double dfXRatioDstToSrc,
                                   double dfYRatioDstToSrc,
                                   double dfSrcXDelta,
                                   double dfSrcYDelta,
                                   const T * pChunk, int nBands,
                                   GByte * pabyChunkNodataMask,
                                   int nChunkXOff, int nChunkXSize,
                                   int nChunkYOff, int nChunkYSize,
                                   int nDstXOff, int nDstXOff2,
                                   int nDstYOff, int nDstYOff2,
                                   GDALRasterBand ** papoDstBands,
                                   int bHasNoData,
                                   float fNoDataValue,
                                   FilterFuncType pfnFilterFunc,
                                   FilterFunc4ValuesType pfnFilterFunc4Values,
                                   int nKernelRadius,
                                   float fMaxVal )

{
    if( !bHasNoData )
        fNoDataValue = 0.0f;

/* -------------------------------------------------------------------- */
/*      Allocate work buffers.                                          */
/* -------------------------------------------------------------------- */
    const int nDstXSize = nDstXOff2 - nDstXOff;

    const double dfXScale = 1.0 / dfXRatioDstToSrc;
    const double dfXScaleWeight = ( dfXScale >= 1.0 ) ? 1.0 : dfXScale;
    const double dfXScaledRadius = nKernelRadius / dfXScaleWeight;
    const double dfYScale = 1.0 / dfYRatioDstToSrc;
    const double dfYScaleWeight = ( dfYScale >= 1.0 ) ? 1.0 : dfYScale;
    const double dfYScaledRadius = nKernelRadius / dfYScaleWeight;

    float* pafDstScanline = static_cast<float *>(
        VSI_MALLOC_VERBOSE(nDstXSize * sizeof(float)) );

    // Temporary array to store result of horizontal filter.
    double* padfHorizontalFiltered = static_cast<double*>(
        VSI_MALLOC_VERBOSE(nChunkYSize * nDstXSize * sizeof(double) * nBands) );

    // To store convolution coefficients.
    double* padfWeights = static_cast<double *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            static_cast<int>(
                2 + 2 * std::max(dfXScaledRadius, dfYScaledRadius) +
                0.5) * sizeof(double) ) );

    GByte* pabyChunkNodataMaskHorizontalFiltered = NULL;
    if( pabyChunkNodataMask )
        pabyChunkNodataMaskHorizontalFiltered = static_cast<GByte*>(
            VSI_MALLOC_VERBOSE(nChunkYSize * nDstXSize) );
    if( pafDstScanline == NULL ||
        padfHorizontalFiltered == NULL ||
        padfWeights == NULL ||
        (pabyChunkNodataMask != NULL &&
         pabyChunkNodataMaskHorizontalFiltered == NULL) )
    {
        VSIFree(pafDstScanline);
        VSIFree(padfHorizontalFiltered);
        VSIFreeAligned(padfWeights);
        VSIFree(pabyChunkNodataMaskHorizontalFiltered);
        return CE_Failure;
    }

/* ==================================================================== */
/*      First pass: horizontal filter                                   */
/* ==================================================================== */
    const int nChunkRightXOff = nChunkXOff + nChunkXSize;
#ifdef USE_SSE2
    bool bSrcPixelCountLess8 = dfXScaledRadius < 4;
#endif
    for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; ++iDstPixel )
    {
        const double dfSrcPixel =
            (iDstPixel + 0.5) * dfXRatioDstToSrc + dfSrcXDelta;
        int nSrcPixelStart =
            static_cast<int>(floor(dfSrcPixel - dfXScaledRadius + 0.5));
        if( nSrcPixelStart < nChunkXOff )
            nSrcPixelStart = nChunkXOff;
        int nSrcPixelStop =
            static_cast<int>(dfSrcPixel + dfXScaledRadius + 0.5);
        if( nSrcPixelStop > nChunkRightXOff )
            nSrcPixelStop = nChunkRightXOff;
#if 0
        if( nSrcPixelStart < nChunkXOff && nChunkXOff > 0 )
        {
            printf( "truncated iDstPixel = %d\n", iDstPixel );/*ok*/
        }
        if( nSrcPixelStop > nChunkRightXOff && nChunkRightXOff < nSrcWidth )
        {
            printf( "truncated iDstPixel = %d\n", iDstPixel );/*ok*/
        }
#endif
        const int nSrcPixelCount = nSrcPixelStop - nSrcPixelStart;
        double dfWeightSum = 0.0;

        // Compute convolution coefficients.
        int nSrcPixel = nSrcPixelStart;
        double dfX = dfXScaleWeight * (nSrcPixel - dfSrcPixel + 0.5);
        for( ; nSrcPixel + 3 < nSrcPixelStop; nSrcPixel+=4)
        {
            padfWeights[nSrcPixel - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel+1 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel+2 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            padfWeights[nSrcPixel+3 - nSrcPixelStart] = dfX;
            dfX += dfXScaleWeight;
            dfWeightSum +=
                pfnFilterFunc4Values(padfWeights + nSrcPixel - nSrcPixelStart);
        }
        for( ; nSrcPixel < nSrcPixelStop; ++nSrcPixel, dfX += dfXScaleWeight)
        {
            const double dfWeight = pfnFilterFunc(dfX);
            padfWeights[nSrcPixel - nSrcPixelStart] = dfWeight;
            dfWeightSum += dfWeight;
        }

        const int nHeight = nChunkYSize * nBands;
        if( pabyChunkNodataMask == NULL )
        {
            if( dfWeightSum != 0 )
            {
                const double dfInvWeightSum = 1.0 / dfWeightSum;
                for( int i = 0; i < nSrcPixelCount; ++i )
                    padfWeights[i] *= dfInvWeightSum;
            }
            int iSrcLineOff = 0;
#ifdef USE_SSE2
            if( bSrcPixelCountLess8 )
            {
                for( ; iSrcLineOff+2 < nHeight; iSrcLineOff +=3 )
                {
                    const int j =
                        iSrcLineOff * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontalPixelCountLess8_3rows(
                        pChunk + j, pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize,
                        padfWeights, nSrcPixelCount, dfVal1, dfVal2, dfVal3);
                    padfHorizontalFiltered[iSrcLineOff * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal1;
                    padfHorizontalFiltered[(iSrcLineOff + 1) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal2;
                    padfHorizontalFiltered[(iSrcLineOff + 2) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal3;
                }
            }
            else
#endif
            {
                for( ; iSrcLineOff+2 < nHeight; iSrcLineOff +=3 )
                {
                    const int j =
                        iSrcLineOff * nChunkXSize +
                        (nSrcPixelStart - nChunkXOff);
                    double dfVal1 = 0.0;
                    double dfVal2 = 0.0;
                    double dfVal3 = 0.0;
                    GDALResampleConvolutionHorizontal_3rows(
                        pChunk + j,
                        pChunk + j + nChunkXSize,
                        pChunk + j + 2 * nChunkXSize,
                        padfWeights, nSrcPixelCount, dfVal1, dfVal2, dfVal3);
                    padfHorizontalFiltered[iSrcLineOff * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal1;
                    padfHorizontalFiltered[(iSrcLineOff + 1) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal2;
                    padfHorizontalFiltered[(iSrcLineOff + 2) * nDstXSize +
                                           iDstPixel - nDstXOff] = dfVal3;
                }
            }
            for( ; iSrcLineOff < nHeight; ++iSrcLineOff )
            {
                const int j =
                    iSrcLineOff * nChunkXSize + (nSrcPixelStart - nChunkXOff);
                const double dfVal =
                    GDALResampleConvolutionHorizontal(pChunk + j,
                                                padfWeights, nSrcPixelCount);
                padfHorizontalFiltered[iSrcLineOff * nDstXSize +
                                       iDstPixel - nDstXOff] = dfVal;
            }
        }
        else
        {
            for( int iSrcLineOff = 0; iSrcLineOff < nHeight; ++iSrcLineOff )
            {
                double dfVal = 0.0;
                const int j =
                    iSrcLineOff * nChunkXSize + (nSrcPixelStart - nChunkXOff);
                GDALResampleConvolutionHorizontalWithMask(
                    pChunk + j, pabyChunkNodataMask + j,
                    padfWeights, nSrcPixelCount,
                    dfVal, dfWeightSum );
                const int nTempOffset =
                    iSrcLineOff * nDstXSize + iDstPixel - nDstXOff;
                if( dfWeightSum > 0.0 )
                {
                    padfHorizontalFiltered[nTempOffset] = dfVal / dfWeightSum;
                    pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 1;
                }
                else
                {
                    padfHorizontalFiltered[nTempOffset] = 0.0;
                    pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 0;
                }
            }
        }
    }

/* ==================================================================== */
/*      Second pass: vertical filter                                    */
/* ==================================================================== */
    const int nChunkBottomYOff = nChunkYOff + nChunkYSize;

    CPLErr eErr = CE_None;

  for( int iBand = 0; iBand < (bMultipleBands ? nBands : 1); ++iBand )
  {
    const double* padfHorizontalFilteredBand =
        padfHorizontalFiltered + iBand * nChunkYSize * nDstXSize;
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; ++iDstLine )
    {
        const double dfSrcLine =
            (iDstLine + 0.5) * dfYRatioDstToSrc + dfSrcYDelta;
        int nSrcLineStart =
            static_cast<int>(floor(dfSrcLine - dfYScaledRadius + 0.5));
        int nSrcLineStop = static_cast<int>(dfSrcLine + dfYScaledRadius + 0.5);
        if( nSrcLineStart < nChunkYOff )
            nSrcLineStart = nChunkYOff;
        if( nSrcLineStop > nChunkBottomYOff )
            nSrcLineStop = nChunkBottomYOff;
#if 0
        if( nSrcLineStart < nChunkYOff &&
            nChunkYOff > 0 )
        {
            printf( "truncated iDstLine = %d\n", iDstLine );/*ok*/
        }
        if( nSrcLineStop > nChunkBottomYOff && nChunkBottomYOff < nSrcHeight )
        {
            printf( "truncated iDstLine = %d\n", iDstLine );/*ok*/
        }
#endif
        const int nSrcLineCount = nSrcLineStop - nSrcLineStart;
        double dfWeightSum = 0.0;

        // Compute convolution coefficients.
        int nSrcLine = nSrcLineStart;  // Used after for.
        double dfY = dfYScaleWeight * (nSrcLine - dfSrcLine + 0.5);
        for( ;
             nSrcLine + 3 < nSrcLineStop;
             nSrcLine += 4, dfY += 4 * dfYScaleWeight)
        {
            padfWeights[nSrcLine - nSrcLineStart] = dfY;
            padfWeights[nSrcLine+1 - nSrcLineStart] = dfY + dfYScaleWeight;
            padfWeights[nSrcLine+2 - nSrcLineStart] = dfY + 2 * dfYScaleWeight;
            padfWeights[nSrcLine+3 - nSrcLineStart] = dfY + 3 * dfYScaleWeight;
            dfWeightSum +=
                pfnFilterFunc4Values(padfWeights + nSrcLine - nSrcLineStart);
        }
        for( ; nSrcLine < nSrcLineStop; ++nSrcLine, dfY += dfYScaleWeight )
        {
            const double dfWeight = pfnFilterFunc(dfY);
            padfWeights[nSrcLine - nSrcLineStart] = dfWeight;
            dfWeightSum += dfWeight;
        }

        if( pabyChunkNodataMask == NULL )
        {
            if( dfWeightSum != 0 )
            {
                const double dfInvWeightSum = 1.0 / dfWeightSum;
                for( int i = 0; i < nSrcLineCount; ++i )
                    padfWeights[i] *= dfInvWeightSum;
            }
        }

        if( pabyChunkNodataMask == NULL )
        {
            int iFilteredPixelOff = 0;  // Used after for.
            // j used after for.
            int j = (nSrcLineStart - nChunkYOff) * nDstXSize;
            for( ;
                 iFilteredPixelOff+1 < nDstXSize;
                 iFilteredPixelOff += 2, j += 2 )
            {
                double dfVal1 = 0.0;
                double dfVal2 = 0.0;
                GDALResampleConvolutionVertical_2cols(
                    padfHorizontalFilteredBand + j, nDstXSize, padfWeights,
                    nSrcLineCount, dfVal1, dfVal2 );
                pafDstScanline[iFilteredPixelOff] = static_cast<float>(dfVal1);
                pafDstScanline[iFilteredPixelOff+1] =
                    static_cast<float>(dfVal2);
            }
            if( iFilteredPixelOff < nDstXSize )
            {
                const double dfVal =
                    GDALResampleConvolutionVertical(
                        padfHorizontalFilteredBand + j,
                        nDstXSize, padfWeights, nSrcLineCount );
                pafDstScanline[iFilteredPixelOff] = static_cast<float>(dfVal);
            }
        }
        else
        {
            for( int iFilteredPixelOff = 0;
                 iFilteredPixelOff < nDstXSize;
                 ++iFilteredPixelOff )
            {
                double dfVal = 0.0;
                dfWeightSum = 0.0;
                for( int i = 0,
                         j = (nSrcLineStart - nChunkYOff) * nDstXSize
                             + iFilteredPixelOff;
                    i < nSrcLineCount;
                    ++i, j += nDstXSize)
                {
                    const double dfWeight =
                        padfWeights[i]
                        * pabyChunkNodataMaskHorizontalFiltered[j];
                    dfVal += padfHorizontalFilteredBand[j] * dfWeight;
                    dfWeightSum += dfWeight;
                }
                if( dfWeightSum > 0.0 )
                {
                    pafDstScanline[iFilteredPixelOff] =
                        static_cast<float>(dfVal / dfWeightSum);
                }
                else
                {
                    pafDstScanline[iFilteredPixelOff] = fNoDataValue;
                }
            }
        }

        if( fMaxVal != 0.0f )
        {
            for( int i = 0; i < nDstXSize; ++i )
            {
                if( pafDstScanline[i] > fMaxVal )
                    pafDstScanline[i] = fMaxVal;
            }
        }

        eErr = papoDstBands[iBand]->RasterIO(
            GF_Write, nDstXOff, iDstLine, nDstXSize, 1,
            pafDstScanline, nDstXSize, 1, GDT_Float32,
            0, 0, NULL );
    }
  }

    VSIFreeAligned( padfWeights );
    VSIFree( padfHorizontalFiltered );
    VSIFree( pafDstScanline );
    VSIFree( pabyChunkNodataMaskHorizontalFiltered );

    return eErr;
}

static CPLErr GDALResampleChunk32R_Convolution(
    double dfXRatioDstToSrc, double dfYRatioDstToSrc,
    double dfSrcXDelta,
    double dfSrcYDelta,
    GDALDataType eWrkDataType,
    void * pChunk,
    GByte * pabyChunkNodataMask,
    int nChunkXOff, int nChunkXSize,
    int nChunkYOff, int nChunkYSize,
    int nDstXOff, int nDstXOff2,
    int nDstYOff, int nDstYOff2,
    GDALRasterBand * poOverview,
    const char * pszResampling,
    int bHasNoData, float fNoDataValue,
    GDALColorTable* /* poColorTable_unused */,
    GDALDataType /* eSrcDataType */,
    bool /* bPropagateNoData */ )
{
    GDALResampleAlg eResample;
    if( EQUAL(pszResampling, "BILINEAR") )
        eResample = GRA_Bilinear;
    else if( EQUAL(pszResampling, "CUBIC") )
        eResample = GRA_Cubic;
    else if( EQUAL(pszResampling, "CUBICSPLINE") )
        eResample = GRA_CubicSpline;
    else if( EQUAL(pszResampling, "LANCZOS") )
        eResample = GRA_Lanczos;
    else
    {
        CPLAssert(false);
        return CE_Failure;
    }
    const int nKernelRadius = GWKGetFilterRadius(eResample);
    FilterFuncType pfnFilterFunc = GWKGetFilterFunc(eResample);
    const FilterFunc4ValuesType pfnFilterFunc4Values =
        GWKGetFilterFunc4Values(eResample);

    float fMaxVal = 0.f;
    // Cubic, etc... can have overshoots, so make sure we clamp values to the
    // maximum value if NBITS is set.
    const char* pszNBITS =
        poOverview->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    GDALDataType eBandDT = poOverview->GetRasterDataType();
    if( eResample != GRA_Bilinear && pszNBITS != NULL &&
        (eBandDT == GDT_Byte || eBandDT == GDT_UInt16 ||
         eBandDT == GDT_UInt32) )
    {
        int nBits = atoi(pszNBITS);
        if( nBits == GDALGetDataTypeSize(eBandDT) )
            nBits = 0;
        if( nBits )
            fMaxVal = static_cast<float>((1 << nBits) -1);
    }

    if( eWrkDataType == GDT_Byte )
        return GDALResampleChunk32R_ConvolutionT<GByte, false>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<GByte *>( pChunk ),
            1,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            &poOverview,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            fMaxVal );
    else if( eWrkDataType == GDT_UInt16 )
        return GDALResampleChunk32R_ConvolutionT<GUInt16, false>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<GUInt16 *>( pChunk ),
            1,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            &poOverview,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            fMaxVal );
    else if( eWrkDataType == GDT_Float32 )
        return GDALResampleChunk32R_ConvolutionT<float, false>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<float *>( pChunk ),
            1,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            &poOverview,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            fMaxVal );

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                       GDALResampleChunkC32R()                        */
/************************************************************************/

static CPLErr
GDALResampleChunkC32R( int nSrcWidth, int nSrcHeight,
                       float * pafChunk, int nChunkYOff, int nChunkYSize,
                       int nDstYOff, int nDstYOff2,
                       GDALRasterBand * poOverview,
                       const char * pszResampling )

{
    const int nOXSize = poOverview->GetXSize();

    float * const pafDstScanline
        = static_cast<float *>(VSI_MALLOC_VERBOSE(nOXSize * sizeof(float) * 2));
    if( pafDstScanline == NULL )
    {
        return CE_Failure;
    }

    const int nOYSize = poOverview->GetYSize();
    const double dfXRatioDstToSrc = static_cast<double>(nSrcWidth) / nOXSize;
    const double dfYRatioDstToSrc = static_cast<double>(nSrcHeight) / nOYSize;

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    CPLErr eErr = CE_None;
    for( int iDstLine = nDstYOff;
         iDstLine < nDstYOff2 && eErr == CE_None;
         ++iDstLine )
    {
        int nSrcYOff = static_cast<int>(0.5 + iDstLine * dfYRatioDstToSrc);
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        int nSrcYOff2 = static_cast<int>(0.5 + (iDstLine+1) * dfYRatioDstToSrc);
        if( nSrcYOff2 == nSrcYOff )
            nSrcYOff2 ++;

        if( nSrcYOff2 > nSrcHeight || iDstLine == nOYSize-1 )
        {
            if( nSrcYOff == nSrcHeight && nSrcHeight - 1 >= nChunkYOff )
                nSrcYOff = nSrcHeight - 1;
            nSrcYOff2 = nSrcHeight;
        }
        if( nSrcYOff2 > nChunkYOff + nChunkYSize )
            nSrcYOff2 = nChunkYOff + nChunkYSize;

        float * const pafSrcScanline =
            pafChunk + ((nSrcYOff - nChunkYOff) * nSrcWidth) * 2;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( int iDstPixel = 0; iDstPixel < nOXSize; ++iDstPixel )
        {
            int nSrcXOff = static_cast<int>(0.5 + iDstPixel * dfXRatioDstToSrc);
            int nSrcXOff2 = static_cast<int>(
                0.5 + (iDstPixel+1) * dfXRatioDstToSrc);
            if( nSrcXOff2 == nSrcXOff )
                nSrcXOff2 ++;
            if( nSrcXOff2 > nSrcWidth || iDstPixel == nOXSize-1 )
            {
                if( nSrcXOff == nSrcWidth && nSrcWidth - 1 >= 0 )
                    nSrcXOff = nSrcWidth - 1;
                nSrcXOff2 = nSrcWidth;
            }

            if( STARTS_WITH_CI(pszResampling, "NEAR") )
            {
                pafDstScanline[iDstPixel*2] = pafSrcScanline[nSrcXOff*2];
                pafDstScanline[iDstPixel*2+1] = pafSrcScanline[nSrcXOff*2+1];
            }
            else if( EQUAL(pszResampling, "AVERAGE_MAGPHASE") )
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                double dfTotalM = 0.0;
                int nCount = 0;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        const double dfR =
                            pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2];
                        const double dfI =
                            pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2+1];
                        dfTotalR += dfR;
                        dfTotalI += dfI;
                        dfTotalM += sqrt( dfR*dfR + dfI*dfI );
                        ++nCount;
                    }
                }

                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel*2] = 0.0;
                    pafDstScanline[iDstPixel*2+1] = 0.0;
                }
                else
                {
                    pafDstScanline[iDstPixel*2  ] =
                        static_cast<float>(dfTotalR/nCount);
                    pafDstScanline[iDstPixel*2+1] =
                        static_cast<float>(dfTotalI/nCount);

                    const double dfM =
                        sqrt( pafDstScanline[iDstPixel*2]
                                  * pafDstScanline[iDstPixel*2]
                              + pafDstScanline[iDstPixel*2+1]
                                  * pafDstScanline[iDstPixel*2+1] );
                    const double dfDesiredM = dfTotalM / nCount;
                    double dfRatio = 1.0;
                    if( dfM != 0.0 )
                        dfRatio = dfDesiredM / dfM;

                    pafDstScanline[iDstPixel*2] *=
                        static_cast<float>(dfRatio);
                    pafDstScanline[iDstPixel*2+1] *=
                        static_cast<float>(dfRatio);
                }
            }
            else if( STARTS_WITH_CI(pszResampling, "AVER") )
            {
                double dfTotalR = 0.0;
                double dfTotalI = 0.0;
                int nCount = 0;

                for( int iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    for( int iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        // TODO(schwehr): Maybe use std::complex?
                        dfTotalR +=
                            pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2];
                        dfTotalI +=
                            pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2+1];
                        ++nCount;
                    }
                }

                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel*2] = 0.0;
                    pafDstScanline[iDstPixel*2+1] = 0.0;
                }
                else
                {
                    pafDstScanline[iDstPixel*2  ] =
                        static_cast<float>(dfTotalR/nCount);
                    pafDstScanline[iDstPixel*2+1] =
                        static_cast<float>(dfTotalI/nCount);
                }
            }
        }

        eErr = poOverview->RasterIO( GF_Write, 0, iDstLine, nOXSize, 1,
                                     pafDstScanline, nOXSize, 1, GDT_CFloat32,
                                     0, 0, NULL );
    }

    CPLFree( pafDstScanline );

    return eErr;
}

/************************************************************************/
/*                  GDALRegenerateCascadingOverviews()                  */
/*                                                                      */
/*      Generate a list of overviews in order from largest to           */
/*      smallest, computing each from the next larger.                  */
/************************************************************************/

static CPLErr
GDALRegenerateCascadingOverviews(
    GDALRasterBand *poSrcBand, int nOverviews, GDALRasterBand **papoOvrBands,
    const char * pszResampling,
    GDALProgressFunc pfnProgress, void * pProgressData )

{
/* -------------------------------------------------------------------- */
/*      First, we must put the overviews in order from largest to       */
/*      smallest.                                                       */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nOverviews-1; ++i )
    {
        for( int j = 0; j < nOverviews - i - 1; ++j )
        {
            if( papoOvrBands[j]->GetXSize()
                * static_cast<float>( papoOvrBands[j]->GetYSize() ) <
                papoOvrBands[j+1]->GetXSize()
                * static_cast<float>( papoOvrBands[j+1]->GetYSize() ) )
            {
                GDALRasterBand *poTempBand = papoOvrBands[j];
                papoOvrBands[j] = papoOvrBands[j+1];
                papoOvrBands[j+1] = poTempBand;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Count total pixels so we can prepare appropriate scaled         */
/*      progress functions.                                             */
/* -------------------------------------------------------------------- */
    double dfTotalPixels = 0.0;

    for( int i = 0; i < nOverviews; ++i )
    {
        dfTotalPixels += papoOvrBands[i]->GetXSize()
            * static_cast<double>( papoOvrBands[i]->GetYSize() );
    }

/* -------------------------------------------------------------------- */
/*      Generate all the bands.                                         */
/* -------------------------------------------------------------------- */
    double dfPixelsProcessed = 0.0;

    for( int i = 0; i < nOverviews; ++i )
    {
        GDALRasterBand *poBaseBand = poSrcBand;
        if( i != 0 )
            poBaseBand = papoOvrBands[i-1];

        double dfPixels =
            papoOvrBands[i]->GetXSize()
            * static_cast<double>( papoOvrBands[i]->GetYSize() );

        void *pScaledProgressData =
            GDALCreateScaledProgress(
            dfPixelsProcessed / dfTotalPixels,
            (dfPixelsProcessed + dfPixels) / dfTotalPixels,
            pfnProgress, pProgressData );

        const CPLErr eErr =
            GDALRegenerateOverviews(
                poBaseBand,
                1,
                reinterpret_cast<GDALRasterBandH *>( papoOvrBands ) + i,
                pszResampling,
                GDALScaledProgress,
                pScaledProgressData );
        GDALDestroyScaledProgress( pScaledProgressData );

        if( eErr != CE_None )
            return eErr;

        dfPixelsProcessed += dfPixels;

        // Only do the bit2grayscale promotion on the base band.
        if( STARTS_WITH_CI( pszResampling,
                            "AVERAGE_BIT2G" /* AVERAGE_BIT2GRAYSCALE */) )
            pszResampling = "AVERAGE";
    }

    return CE_None;
}

/************************************************************************/
/*                    GDALGetResampleFunction()                         */
/************************************************************************/

GDALResampleFunction GDALGetResampleFunction( const char* pszResampling,
                                              int* pnRadius )
{
    if( pnRadius ) *pnRadius = 0;
    if( STARTS_WITH_CI(pszResampling, "NEAR") )
        return GDALResampleChunk32R_Near;
    else if( STARTS_WITH_CI(pszResampling, "AVER") )
        return GDALResampleChunk32R_Average;
    else if( STARTS_WITH_CI(pszResampling, "GAUSS") )
    {
        if( pnRadius ) *pnRadius = 1;
        return GDALResampleChunk32R_Gauss;
    }
    else if( STARTS_WITH_CI(pszResampling, "MODE") )
        return GDALResampleChunk32R_Mode;
    else if( EQUAL(pszResampling,"CUBIC") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Cubic);
        return GDALResampleChunk32R_Convolution;
    }
    else if( EQUAL(pszResampling,"CUBICSPLINE") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_CubicSpline);
        return GDALResampleChunk32R_Convolution;
    }
    else if( EQUAL(pszResampling,"LANCZOS") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Lanczos);
        return GDALResampleChunk32R_Convolution;
    }
    else if( EQUAL(pszResampling,"BILINEAR") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Bilinear);
        return GDALResampleChunk32R_Convolution;
    }
    else
    {
       CPLError(
           CE_Failure, CPLE_AppDefined,
           "GDALGetResampleFunction: Unsupported resampling method \"%s\".",
           pszResampling );
        return NULL;
    }
}

#ifdef GDAL_ENABLE_RESAMPLING_MULTIBAND

// For some reason, this does not perform better, and sometimes it performs
// worse that when operating band after band. Probably due to cache misses.

/************************************************************************/
/*             GDALResampleChunk32RMultiBands_Convolution()             */
/************************************************************************/

static CPLErr GDALResampleChunk32RMultiBands_Convolution(
    double dfXRatioDstToSrc, double dfYRatioDstToSrc,
    double dfSrcXDelta,
    double dfSrcYDelta,
    GDALDataType eWrkDataType,
    void * pChunk, int nBands,
    GByte * pabyChunkNodataMask,
    int nChunkXOff, int nChunkXSize,
    int nChunkYOff, int nChunkYSize,
    int nDstXOff, int nDstXOff2,
    int nDstYOff, int nDstYOff2,
    GDALRasterBand **papoDstBands,
    const char * pszResampling,
    int bHasNoData, float fNoDataValue,
    GDALColorTable* /* poColorTable_unused */,
    GDALDataType /* eSrcDataType */ )
{
    GDALResampleAlg eResample;
    if( EQUAL(pszResampling, "BILINEAR") )
        eResample = GRA_Bilinear;
    else if( EQUAL(pszResampling, "CUBIC") )
        eResample = GRA_Cubic;
    else if( EQUAL(pszResampling, "CUBICSPLINE") )
        eResample = GRA_CubicSpline;
    else if( EQUAL(pszResampling, "LANCZOS") )
        eResample = GRA_Lanczos;
    else
    {
        CPLAssert(false);
        return CE_Failure;
    }
    int nKernelRadius = GWKGetFilterRadius(eResample);
    FilterFuncType pfnFilterFunc = GWKGetFilterFunc(eResample);
    FilterFunc4ValuesType pfnFilterFunc4Values =
        GWKGetFilterFunc4Values(eResample);

    float fMaxVal = 0.0f;
    // Cubic, etc... can have overshoots, so make sure we clamp values to the
    // maximum value if NBITS is set
    const char* pszNBITS =
        papoDstBands[0]->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    GDALDataType eBandDT = papoDstBands[0]->GetRasterDataType();
    if( eResample != GRA_Bilinear && pszNBITS != NULL &&
        (eBandDT == GDT_Byte || eBandDT == GDT_UInt16 ||
         eBandDT == GDT_UInt32) )
    {
        int nBits = atoi(pszNBITS);
        if( nBits == GDALGetDataTypeSize(eBandDT) )
            nBits = 0;
        if( nBits )
            fMaxVal = static_cast<float>((1 << nBits) -1);
    }

    if( eWrkDataType == GDT_Byte )
        return GDALResampleChunk32R_ConvolutionT<GByte, true>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<GByte *>( pChunk ), nBands,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            papoDstBands,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            fMaxVal );
    else if( eWrkDataType == GDT_UInt16 )
        return GDALResampleChunk32R_ConvolutionT<GUInt16, true>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<GUInt16 *>( pChunk ), nBands,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            papoDstBands,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            fMaxVal );
    else if( eWrkDataType == GDT_Float32 )
        return GDALResampleChunk32R_ConvolutionT<float, true>(
            dfXRatioDstToSrc, dfYRatioDstToSrc,
            dfSrcXDelta, dfSrcYDelta,
            static_cast<float *>( pChunk ), nBands,
            pabyChunkNodataMask,
            nChunkXOff, nChunkXSize,
            nChunkYOff, nChunkYSize,
            nDstXOff, nDstXOff2,
            nDstYOff, nDstYOff2,
            papoDstBands,
            bHasNoData, fNoDataValue,
            pfnFilterFunc,
            pfnFilterFunc4Values,
            nKernelRadius,
            fMaxVal );

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                    GDALGetResampleFunctionMultiBands()               */
/************************************************************************/

GDALResampleFunctionMultiBands GDALGetResampleFunctionMultiBands(
    const char* pszResampling, int* pnRadius )
{
    if( pnRadius ) *pnRadius = 0;
    if( EQUAL(pszResampling, "CUBIC") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Cubic);
        return GDALResampleChunk32RMultiBands_Convolution;
    }
    else if( EQUAL(pszResampling, "CUBICSPLINE") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_CubicSpline);
        return GDALResampleChunk32RMultiBands_Convolution;
    }
    else if( EQUAL(pszResampling, "LANCZOS") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Lanczos);
        return GDALResampleChunk32RMultiBands_Convolution;
    }
    else if( EQUAL(pszResampling, "BILINEAR") )
    {
        if( pnRadius ) *pnRadius = GWKGetFilterRadius(GRA_Bilinear);
        return GDALResampleChunk32RMultiBands_Convolution;
    }

    return NULL;
}
#endif

/************************************************************************/
/*                      GDALGetOvrWorkDataType()                        */
/************************************************************************/

GDALDataType GDALGetOvrWorkDataType( const char* pszResampling,
                                     GDALDataType eSrcDataType )
{
    if( (STARTS_WITH_CI(pszResampling, "NEAR") ||
         STARTS_WITH_CI(pszResampling, "AVER") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) &&
        eSrcDataType == GDT_Byte )
    {
        return GDT_Byte;
    }
    else if( (STARTS_WITH_CI(pszResampling, "NEAR") ||
         STARTS_WITH_CI(pszResampling, "AVER") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) &&
        eSrcDataType == GDT_UInt16 )
    {
        return GDT_UInt16;
    }

    return GDT_Float32;
}

/************************************************************************/
/*                      GDALRegenerateOverviews()                       */
/************************************************************************/

/**
 * \brief Generate downsampled overviews.
 *
 * This function will generate one or more overview images from a base image
 * using the requested downsampling algorithm.  Its primary use is for
 * generating overviews via GDALDataset::BuildOverviews(), but it can also be
 * used to generate downsampled images in one file from another outside the
 * overview architecture.
 *
 * The output bands need to exist in advance.
 *
 * The full set of resampling algorithms is documented in
 * GDALDataset::BuildOverviews().
 *
 * This function will honour properly NODATA_VALUES tuples (special dataset
 * metadata) so that only a given RGB triplet (in case of a RGB image) will be
 * considered as the nodata value and not each value of the triplet
 * independently per band.
 *
 * @param hSrcBand the source (base level) band.
 * @param nOverviewCount the number of downsampled bands being generated.
 * @param pahOvrBands the list of downsampled bands to be generated.
 * @param pszResampling Resampling algorithm (e.g. "AVERAGE").
 * @param pfnProgress progress report function.
 * @param pProgressData progress function callback data.
 * @return CE_None on success or CE_Failure on failure.
 */
CPLErr
GDALRegenerateOverviews( GDALRasterBandH hSrcBand,
                         int nOverviewCount, GDALRasterBandH *pahOvrBands,
                         const char * pszResampling,
                         GDALProgressFunc pfnProgress, void * pProgressData )

{
    GDALRasterBand *poSrcBand = static_cast<GDALRasterBand *>( hSrcBand );
    GDALRasterBand **papoOvrBands =
        reinterpret_cast<GDALRasterBand **>( pahOvrBands );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( EQUAL(pszResampling,"NONE") )
        return CE_None;

    int nKernelRadius = 0;
    GDALResampleFunction pfnResampleFn
        = GDALGetResampleFunction(pszResampling, &nKernelRadius);

    if( pfnResampleFn == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Check color tables...                                           */
/* -------------------------------------------------------------------- */
    GDALColorTable* poColorTable = NULL;

    if( (STARTS_WITH_CI(pszResampling, "AVER")
         || STARTS_WITH_CI(pszResampling, "MODE")
         || STARTS_WITH_CI(pszResampling, "GAUSS")) &&
        poSrcBand->GetColorInterpretation() == GCI_PaletteIndex )
    {
        poColorTable = poSrcBand->GetColorTable();
        if( poColorTable != NULL )
        {
            if( poColorTable->GetPaletteInterpretation() != GPI_RGB )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Computing overviews on palette index raster bands "
                    "with a palette whose color interpretation is not RGB "
                    "will probably lead to unexpected results." );
                poColorTable = NULL;
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Computing overviews on palette index raster bands "
                      "without a palette will probably lead to unexpected "
                      "results." );
        }
    }
    // Not ready yet
    else if( (EQUAL(pszResampling, "CUBIC") ||
              EQUAL(pszResampling, "CUBICSPLINE") ||
              EQUAL(pszResampling, "LANCZOS") ||
              EQUAL(pszResampling, "BILINEAR") )
        && poSrcBand->GetColorInterpretation() == GCI_PaletteIndex )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Computing %s overviews on palette index raster bands "
                  "will probably lead to unexpected results.", pszResampling );
    }

    // If we have a nodata mask and we are doing something more complicated
    // than nearest neighbouring, we have to fetch to nodata mask.

    GDALRasterBand* poMaskBand = NULL;
    int nMaskFlags = 0;
    bool bUseNoDataMask = false;

    if( !STARTS_WITH_CI(pszResampling, "NEAR") )
    {
        // Special case if we are the alpha band. We want it to be considered
        // as the mask band to avoid alpha=0 to be taken into account in average
        // computation.
        if( poSrcBand->GetColorInterpretation() == GCI_AlphaBand )
        {
            poMaskBand = poSrcBand;
            nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
        }
        else
        {
            poMaskBand = poSrcBand->GetMaskBand();
            nMaskFlags = poSrcBand->GetMaskFlags();
        }

        bUseNoDataMask = (nMaskFlags & GMF_ALL_VALID) == 0;
    }

/* -------------------------------------------------------------------- */
/*      If we are operating on multiple overviews, and using            */
/*      averaging, lets do them in cascading order to reduce the        */
/*      amount of computation.                                          */
/* -------------------------------------------------------------------- */

    // In case the mask made be computed from another band of the dataset,
    // we can't use cascaded generation, as the computation of the overviews
    // of the band used for the mask band may not have yet occurred (#3033).
    if( (STARTS_WITH_CI(pszResampling, "AVER") |
         STARTS_WITH_CI(pszResampling, "GAUSS") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) && nOverviewCount > 1
         && !(bUseNoDataMask && nMaskFlags != GMF_NODATA))
        return GDALRegenerateCascadingOverviews( poSrcBand,
                                                 nOverviewCount, papoOvrBands,
                                                 pszResampling,
                                                 pfnProgress,
                                                 pProgressData );

/* -------------------------------------------------------------------- */
/*      Setup one horizontal swath to read from the raw buffer.         */
/* -------------------------------------------------------------------- */
    int nFRXBlockSize = 0;
    int nFRYBlockSize = 0;
    poSrcBand->GetBlockSize( &nFRXBlockSize, &nFRYBlockSize );

    int nFullResYChunk = 0;
    if( nFRYBlockSize < 16 || nFRYBlockSize > 256 )
        nFullResYChunk = 64;
    else
        nFullResYChunk = nFRYBlockSize;

    GDALDataType eType = GDT_Unknown;
    if( GDALDataTypeIsComplex( poSrcBand->GetRasterDataType() ) )
        eType = GDT_CFloat32;
    else
        eType = GDALGetOvrWorkDataType( pszResampling,
                                        poSrcBand->GetRasterDataType() );

    const int nWidth = poSrcBand->GetXSize();
    const int nHeight = poSrcBand->GetYSize();

    int nMaxOvrFactor = 1;
    for( int iOverview = 0; iOverview < nOverviewCount; ++iOverview )
    {
        const int nDstWidth = papoOvrBands[iOverview]->GetXSize();
        const int nDstHeight = papoOvrBands[iOverview]->GetYSize();
        nMaxOvrFactor = std::max(
            nMaxOvrFactor,
            static_cast<int>(static_cast<double>(nWidth) / nDstWidth + 0.5) );
        nMaxOvrFactor = std::max(
            nMaxOvrFactor,
            static_cast<int>(static_cast<double>(nHeight) / nDstHeight + 0.5) );
    }
    const int nMaxChunkYSizeQueried =
        nFullResYChunk + 2 * nKernelRadius * nMaxOvrFactor;

    GByte *pabyChunkNodataMask = NULL;
    void *pChunk =
        VSI_MALLOC3_VERBOSE(
            GDALGetDataTypeSizeBytes(eType), nMaxChunkYSizeQueried, nWidth );
    if( bUseNoDataMask )
    {
        pabyChunkNodataMask =
            (GByte*) VSI_MALLOC2_VERBOSE( nMaxChunkYSizeQueried, nWidth );
    }

    if( pChunk == NULL || (bUseNoDataMask && pabyChunkNodataMask == NULL))
    {
        CPLFree(pChunk);
        CPLFree(pabyChunkNodataMask);
        return CE_Failure;
    }

    int bHasNoData = FALSE;
    const float fNoDataValue =
        static_cast<float>( poSrcBand->GetNoDataValue(&bHasNoData) );
    const bool bPropagateNoData =
        CPLTestBool( CPLGetConfigOption("GDAL_OVR_PROPAGATE_NODATA", "NO") );

/* -------------------------------------------------------------------- */
/*      Loop over image operating on chunks.                            */
/* -------------------------------------------------------------------- */
    int nChunkYOff = 0;
    CPLErr eErr = CE_None;

    for( nChunkYOff = 0;
         nChunkYOff < nHeight && eErr == CE_None;
         nChunkYOff += nFullResYChunk )
    {
        if( !pfnProgress( nChunkYOff / static_cast<double>( nHeight ),
                          NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }

        if( nFullResYChunk + nChunkYOff > nHeight )
            nFullResYChunk = nHeight - nChunkYOff;

        int nChunkYOffQueried = nChunkYOff - nKernelRadius * nMaxOvrFactor;
        int nChunkYSizeQueried =
            nFullResYChunk + 2 * nKernelRadius * nMaxOvrFactor;
        if( nChunkYOffQueried < 0 )
        {
            nChunkYSizeQueried += nChunkYOffQueried;
            nChunkYOffQueried = 0;
        }
        if( nChunkYOffQueried + nChunkYSizeQueried > nHeight )
            nChunkYSizeQueried = nHeight - nChunkYOffQueried;

        // Read chunk.
        if( eErr == CE_None )
            eErr = poSrcBand->RasterIO(
                GF_Read, 0, nChunkYOffQueried, nWidth, nChunkYSizeQueried,
                pChunk, nWidth, nChunkYSizeQueried, eType,
                0, 0, NULL );
        if( eErr == CE_None && bUseNoDataMask )
            eErr = poMaskBand->RasterIO(
                GF_Read, 0, nChunkYOffQueried, nWidth, nChunkYSizeQueried,
                pabyChunkNodataMask, nWidth, nChunkYSizeQueried, GDT_Byte,
                0, 0, NULL );

        // Special case to promote 1bit data to 8bit 0/255 values.
        if( EQUAL(pszResampling, "AVERAGE_BIT2GRAYSCALE") )
        {
            if( eType == GDT_Float32 )
            {
                float* pafChunk = static_cast<float*>(pChunk);
                for( int i = nChunkYSizeQueried*nWidth - 1; i >= 0; --i )
                {
                    if( pafChunk[i] == 1.0 )
                        pafChunk[i] = 255.0;
                }
            }
            else if( eType == GDT_Byte )
            {
              GByte* pabyChunk = static_cast<GByte*>(pChunk);
                for( int i = nChunkYSizeQueried*nWidth - 1; i >= 0; --i )
                {
                    if( pabyChunk[i] == 1 )
                        pabyChunk[i] = 255;
                }
            }
            else if( eType == GDT_UInt16 )
            {
                GUInt16* pasChunk = static_cast<GUInt16*>(pChunk);
                for( int i = nChunkYSizeQueried*nWidth - 1; i >= 0; --i )
                {
                    if( pasChunk[i] == 1 )
                        pasChunk[i] = 255;
                }
            }
            else {
                CPLAssert(false);
            }
        }
        else if( EQUAL(pszResampling, "AVERAGE_BIT2GRAYSCALE_MINISWHITE") )
        {
            if( eType == GDT_Float32 )
            {
                float* pafChunk = static_cast<float*>(pChunk);
                for( int i = nChunkYSizeQueried*nWidth - 1; i >= 0; --i )
                {
                    if( pafChunk[i] == 1.0 )
                        pafChunk[i] = 0.0;
                    else if( pafChunk[i] == 0.0 )
                        pafChunk[i] = 255.0;
                }
            }
            else if( eType == GDT_Byte )
            {
                GByte* pabyChunk = static_cast<GByte*>(pChunk);
                for( int i = nChunkYSizeQueried*nWidth - 1; i >= 0; --i )
                {
                    if( pabyChunk[i] == 1 )
                        pabyChunk[i] = 0;
                    else if( pabyChunk[i] == 0 )
                        pabyChunk[i] = 255;
                }
            }
            else if( eType == GDT_UInt16 )
            {
                GUInt16* pasChunk = static_cast<GUInt16*>(pChunk);
                for( int i = nChunkYSizeQueried*nWidth - 1; i >= 0; --i )
                {
                    if( pasChunk[i] == 1 )
                        pasChunk[i] = 0;
                    else if( pasChunk[i] == 0 )
                        pasChunk[i] = 255;
                }
            }
            else {
                CPLAssert(false);
            }
        }

        for( int iOverview = 0;
             iOverview < nOverviewCount && eErr == CE_None;
             ++iOverview )
        {
            const int nDstWidth = papoOvrBands[iOverview]->GetXSize();
            const int nDstHeight = papoOvrBands[iOverview]->GetYSize();

            const double dfXRatioDstToSrc =
                static_cast<double>(nWidth) / nDstWidth;
            const double dfYRatioDstToSrc =
                static_cast<double>(nHeight) / nDstHeight;

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
            int nDstYOff = static_cast<int>(0.5 + nChunkYOff/dfYRatioDstToSrc);
            int nDstYOff2 = static_cast<int>(
                0.5 + (nChunkYOff+nFullResYChunk)/dfYRatioDstToSrc);

            if( nChunkYOff + nFullResYChunk == nHeight )
                nDstYOff2 = nDstHeight;
#if DEBUG_VERBOSE
            CPLDebug( "GDAL",
                      "nDstYOff=%d, nDstYOff2=%d", nDstYOff, nDstYOff2 );
#endif

            if( eType == GDT_Byte ||
                eType == GDT_UInt16 ||
                eType == GDT_Float32 )
                eErr = pfnResampleFn(
                    dfXRatioDstToSrc, dfYRatioDstToSrc,
                    0.0, 0.0,
                    eType,
                    pChunk,
                    pabyChunkNodataMask,
                    0, nWidth,
                    nChunkYOffQueried, nChunkYSizeQueried,
                    0, nDstWidth,
                    nDstYOff, nDstYOff2,
                    papoOvrBands[iOverview], pszResampling,
                    bHasNoData, fNoDataValue, poColorTable,
                    poSrcBand->GetRasterDataType(),
                    bPropagateNoData);
            else
                eErr = GDALResampleChunkC32R(
                    nWidth, nHeight,
                    static_cast<float*>(pChunk),
                    nChunkYOffQueried, nChunkYSizeQueried,
                    nDstYOff, nDstYOff2,
                    papoOvrBands[iOverview], pszResampling);
        }
    }

    VSIFree( pChunk );
    VSIFree( pabyChunkNodataMask );

/* -------------------------------------------------------------------- */
/*      Renormalized overview mean / stddev if needed.                  */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && EQUAL(pszResampling,"AVERAGE_MP") )
    {
        GDALOverviewMagnitudeCorrection(
            poSrcBand,
            nOverviewCount,
            reinterpret_cast<GDALRasterBandH *>( papoOvrBands ),
            GDALDummyProgress, NULL );
    }

/* -------------------------------------------------------------------- */
/*      It can be important to flush out data to overviews.             */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0;
         eErr == CE_None && iOverview < nOverviewCount;
         ++iOverview )
    {
        eErr = papoOvrBands[iOverview]->FlushCache();
    }

    if( eErr == CE_None )
        pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}

/************************************************************************/
/*            GDALRegenerateOverviewsMultiBand()                        */
/************************************************************************/

/**
 * \brief Variant of GDALRegenerateOverviews, specially dedicated for generating
 * compressed pixel-interleaved overviews (JPEG-IN-TIFF for example)
 *
 * This function will generate one or more overview images from a base
 * image using the requested downsampling algorithm.  Its primary use
 * is for generating overviews via GDALDataset::BuildOverviews(), but it
 * can also be used to generate downsampled images in one file from another
 * outside the overview architecture.
 *
 * The output bands need to exist in advance and share the same characteristics
 * (type, dimensions)
 *
 * The resampling algorithms supported for the moment are "NEAREST", "AVERAGE"
 * and "GAUSS"
 *
 * The pseudo-algorithm used by the function is :
 *    for each overview
 *       iterate on lines of the source by a step of deltay
 *           iterate on columns of the source  by a step of deltax
 *               read the source data of size deltax * deltay for all the bands
 *               generate the corresponding overview block for all the bands
 *
 * This function will honour properly NODATA_VALUES tuples (special dataset
 * metadata) so that only a given RGB triplet (in case of a RGB image) will be
 * considered as the nodata value and not each value of the triplet
 * independently per band.
 *
 * @param nBands the number of bands, size of papoSrcBands and size of
 *               first dimension of papapoOverviewBands
 * @param papoSrcBands the list of source bands to downsample
 * @param nOverviews the number of downsampled overview levels being generated.
 * @param papapoOverviewBands bidimension array of bands. First dimension is
 *                            indexed by nBands. Second dimension is indexed by
 *                            nOverviews.
 * @param pszResampling Resampling algorithm ("NEAREST", "AVERAGE" or "GAUSS").
 * @param pfnProgress progress report function.
 * @param pProgressData progress function callback data.
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr
GDALRegenerateOverviewsMultiBand( int nBands, GDALRasterBand** papoSrcBands,
                                  int nOverviews,
                                  GDALRasterBand*** papapoOverviewBands,
                                  const char * pszResampling,
                                  GDALProgressFunc pfnProgress,
                                  void * pProgressData )
{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( EQUAL(pszResampling,"NONE") )
        return CE_None;

    // Sanity checks.
    if( !STARTS_WITH_CI(pszResampling, "NEAR") &&
        !EQUAL(pszResampling, "AVERAGE") &&
        !EQUAL(pszResampling, "GAUSS") &&
        !EQUAL(pszResampling, "CUBIC") &&
        !EQUAL(pszResampling, "CUBICSPLINE") &&
        !EQUAL(pszResampling, "LANCZOS") &&
        !EQUAL(pszResampling, "BILINEAR") )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "GDALRegenerateOverviewsMultiBand: pszResampling='%s' "
            "not supported", pszResampling );
        return CE_Failure;
    }

    int nKernelRadius = 0;
    GDALResampleFunction pfnResampleFn =
        GDALGetResampleFunction(pszResampling, &nKernelRadius);
    if( pfnResampleFn == NULL )
        return CE_Failure;

    int nSrcWidth = papoSrcBands[0]->GetXSize();
    int nSrcHeight = papoSrcBands[0]->GetYSize();
    GDALDataType eDataType = papoSrcBands[0]->GetRasterDataType();
    for( int iBand = 1; iBand < nBands; ++iBand )
    {
        if( papoSrcBands[iBand]->GetXSize() != nSrcWidth ||
            papoSrcBands[iBand]->GetYSize() != nSrcHeight )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "GDALRegenerateOverviewsMultiBand: all the source bands must "
                "have the same dimensions" );
            return CE_Failure;
        }
        if( papoSrcBands[iBand]->GetRasterDataType() != eDataType )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "GDALRegenerateOverviewsMultiBand: all the source bands must "
                "have the same data type" );
            return CE_Failure;
        }
    }

    for( int iOverview = 0; iOverview < nOverviews; ++iOverview )
    {
        const int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        const int nDstHeight = papapoOverviewBands[0][iOverview]->GetYSize();
        for( int iBand = 1; iBand < nBands; ++iBand )
        {
            if( papapoOverviewBands[iBand][iOverview]->GetXSize() !=
                nDstWidth ||
                papapoOverviewBands[iBand][iOverview]->GetYSize()
                != nDstHeight )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the overviews bands "
                    "of the same level must have the same dimensions" );
                return CE_Failure;
            }
            if( papapoOverviewBands[iBand][iOverview]->GetRasterDataType() !=
                eDataType )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the overviews bands "
                    "must have the same data type as the source bands" );
                return CE_Failure;
            }
        }
    }

    // First pass to compute the total number of pixels to read.
    double dfTotalPixelCount = 0;
    for( int iOverview = 0; iOverview < nOverviews; ++iOverview )
    {
        nSrcWidth = papoSrcBands[0]->GetXSize();
        nSrcHeight = papoSrcBands[0]->GetYSize();

        const int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        // Try to use previous level of overview as the source to compute
        // the next level.
        if( iOverview > 0 &&
            papapoOverviewBands[0][iOverview - 1]->GetXSize() > nDstWidth )
        {
            nSrcWidth = papapoOverviewBands[0][iOverview - 1]->GetXSize();
            nSrcHeight = papapoOverviewBands[0][iOverview - 1]->GetYSize();
        }

        dfTotalPixelCount += static_cast<double>(nSrcWidth) * nSrcHeight;
    }

    nSrcWidth = papoSrcBands[0]->GetXSize();
    nSrcHeight = papoSrcBands[0]->GetYSize();

    GDALDataType eWrkDataType =
        GDALGetOvrWorkDataType(pszResampling, eDataType);

    // If we have a nodata mask and we are doing something more complicated
    // than nearest neighbouring, we have to fetch to nodata mask.
    const bool bUseNoDataMask =
        !STARTS_WITH_CI(pszResampling, "NEAR") &&
        (papoSrcBands[0]->GetMaskFlags() & GMF_ALL_VALID) == 0;

    int* const pabHasNoData = static_cast<int *>(
        VSI_MALLOC_VERBOSE(nBands * sizeof(int)) );
    float* const pafNoDataValue = static_cast<float *>(
        VSI_MALLOC_VERBOSE(nBands * sizeof(float)) );
    if( pabHasNoData == NULL || pafNoDataValue == NULL )
    {
        CPLFree(pabHasNoData);
        CPLFree(pafNoDataValue);
        return CE_Failure;
    }

    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        pabHasNoData[iBand] = FALSE;
        pafNoDataValue[iBand] = static_cast<float>(
            papoSrcBands[iBand]->GetNoDataValue(&pabHasNoData[iBand]) );
    }
    const bool bPropagateNoData =
        CPLTestBool( CPLGetConfigOption("GDAL_OVR_PROPAGATE_NODATA", "NO") );

    // Second pass to do the real job.
    double dfCurPixelCount = 0;
    CPLErr eErr = CE_None;
    for( int iOverview = 0;
         iOverview < nOverviews && eErr == CE_None;
         ++iOverview )
    {
        int iSrcOverview = -1;  // -1 means the source bands.

        int nDstBlockXSize = 0;
        int nDstBlockYSize = 0;
        papapoOverviewBands[0][iOverview]->GetBlockSize( &nDstBlockXSize,
                                                         &nDstBlockYSize );

        const int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        const int nDstHeight = papapoOverviewBands[0][iOverview]->GetYSize();

        // Try to use previous level of overview as the source to compute
        // the next level.
        if( iOverview > 0 &&
            papapoOverviewBands[0][iOverview - 1]->GetXSize() > nDstWidth )
        {
            nSrcWidth = papapoOverviewBands[0][iOverview - 1]->GetXSize();
            nSrcHeight = papapoOverviewBands[0][iOverview - 1]->GetYSize();
            iSrcOverview = iOverview - 1;
        }

        const double dfXRatioDstToSrc =
            static_cast<double>(nSrcWidth) / nDstWidth;
        const double dfYRatioDstToSrc =
            static_cast<double>(nSrcHeight) / nDstHeight;

        // Compute the maximum chunk size of the source such as it will match
        // the size of a block of the overview.
        const int nFullResXChunk =
            1 + static_cast<int>(nDstBlockXSize * dfXRatioDstToSrc);
        const int nFullResYChunk =
            1 + static_cast<int>(nDstBlockYSize * dfYRatioDstToSrc);

        int nOvrFactor = std::max( static_cast<int>(0.5 + dfXRatioDstToSrc),
                                   static_cast<int>(0.5 + dfYRatioDstToSrc) );
        if( nOvrFactor == 0 ) nOvrFactor = 1;
        const int nFullResXChunkQueried =
            nFullResXChunk + 2 * nKernelRadius * nOvrFactor;
        const int nFullResYChunkQueried =
            nFullResYChunk + 2 * nKernelRadius * nOvrFactor;

        void** papaChunk = static_cast<void **>(
            VSI_MALLOC_VERBOSE(nBands * sizeof(void*)) );
        if( papaChunk == NULL )
        {
            CPLFree(pabHasNoData);
            CPLFree(pafNoDataValue);
            return CE_Failure;
        }
        GByte* pabyChunkNoDataMask = NULL;
        for( int iBand = 0; iBand < nBands; ++iBand )
        {
            papaChunk[iBand] = VSI_MALLOC3_VERBOSE(
                nFullResXChunkQueried,
                nFullResYChunkQueried,
                GDALGetDataTypeSizeBytes(eWrkDataType) );
            if( papaChunk[iBand] == NULL )
            {
                while ( --iBand >= 0)
                    CPLFree(papaChunk[iBand]);
                CPLFree(papaChunk);
                CPLFree(pabHasNoData);
                CPLFree(pafNoDataValue);
                return CE_Failure;
            }
        }
        if( bUseNoDataMask )
        {
            pabyChunkNoDataMask = static_cast<GByte *>(
                VSI_MALLOC2_VERBOSE( nFullResXChunkQueried,
                                     nFullResYChunkQueried ) );
            if( pabyChunkNoDataMask == NULL )
            {
                for( int iBand = 0; iBand < nBands; ++iBand )
                {
                    CPLFree(papaChunk[iBand]);
                }
                CPLFree(papaChunk);
                CPLFree(pabHasNoData);
                CPLFree(pafNoDataValue);
                return CE_Failure;
            }
        }

        int nDstYOff = 0;
        // Iterate on destination overview, block by block.
        for( nDstYOff = 0;
             nDstYOff < nDstHeight && eErr == CE_None;
             nDstYOff += nDstBlockYSize )
        {
            int nDstYCount;
            if( nDstYOff + nDstBlockYSize <= nDstHeight )
                nDstYCount = nDstBlockYSize;
            else
                nDstYCount = nDstHeight - nDstYOff;

            int nChunkYOff =
                static_cast<int>(0.5 + nDstYOff * dfYRatioDstToSrc);
            int nChunkYOff2 =
                static_cast<int>(
                    0.5 + (nDstYOff + nDstYCount) * dfYRatioDstToSrc );
            if( nChunkYOff2 > nSrcHeight || nDstYOff + nDstYCount == nDstHeight)
                nChunkYOff2 = nSrcHeight;
            int nYCount = nChunkYOff2 - nChunkYOff;
            CPLAssert(nYCount <= nFullResYChunk);

            int nChunkYOffQueried = nChunkYOff - nKernelRadius * nOvrFactor;
            int nChunkYSizeQueried = nYCount + 2 * nKernelRadius * nOvrFactor;
            if( nChunkYOffQueried < 0 )
            {
                nChunkYSizeQueried += nChunkYOffQueried;
                nChunkYOffQueried = 0;
            }
            if( nChunkYSizeQueried + nChunkYOffQueried > nSrcHeight )
                nChunkYSizeQueried = nSrcHeight - nChunkYOffQueried;
            CPLAssert(nChunkYSizeQueried <= nFullResYChunkQueried);

            if( !pfnProgress( dfCurPixelCount / dfTotalPixelCount,
                              NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }

            int nDstXOff = 0;
            // Iterate on destination overview, block by block.
            for( nDstXOff = 0;
                 nDstXOff < nDstWidth && eErr == CE_None;
                 nDstXOff += nDstBlockXSize )
            {
                int nDstXCount = 0;
                if( nDstXOff + nDstBlockXSize <= nDstWidth )
                    nDstXCount = nDstBlockXSize;
                else
                    nDstXCount = nDstWidth - nDstXOff;

                int nChunkXOff =
                    static_cast<int>(0.5 + nDstXOff * dfXRatioDstToSrc);
                int nChunkXOff2 =
                    static_cast<int>(
                        0.5 + (nDstXOff + nDstXCount) * dfXRatioDstToSrc );
                if( nChunkXOff2 > nSrcWidth ||
                    nDstXOff + nDstXCount == nDstWidth )
                    nChunkXOff2 = nSrcWidth;
                const int nXCount = nChunkXOff2 - nChunkXOff;
                CPLAssert(nXCount <= nFullResXChunk);

                int nChunkXOffQueried = nChunkXOff - nKernelRadius * nOvrFactor;
                int nChunkXSizeQueried =
                    nXCount + 2 * nKernelRadius * nOvrFactor;
                if( nChunkXOffQueried < 0 )
                {
                    nChunkXSizeQueried += nChunkXOffQueried;
                    nChunkXOffQueried = 0;
                }
                if( nChunkXSizeQueried + nChunkXOffQueried > nSrcWidth )
                    nChunkXSizeQueried = nSrcWidth - nChunkXOffQueried;
                CPLAssert(nChunkXSizeQueried <= nFullResXChunkQueried);
#if DEBUG_VERBOSE
                CPLDebug(
                    "GDAL",
                    "Reading (%dx%d -> %dx%d) for output (%dx%d -> %dx%d)",
                    nChunkXOff, nChunkYOff, nXCount, nYCount,
                    nDstXOff, nDstYOff, nDstXCount, nDstYCount );
#endif

                // Read the source buffers for all the bands.
                for( int iBand = 0; iBand < nBands && eErr == CE_None; ++iBand )
                {
                    GDALRasterBand* poSrcBand = NULL;
                    if( iSrcOverview == -1 )
                        poSrcBand = papoSrcBands[iBand];
                    else
                        poSrcBand = papapoOverviewBands[iBand][iSrcOverview];
                    eErr = poSrcBand->RasterIO(
                        GF_Read,
                        nChunkXOffQueried, nChunkYOffQueried,
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        papaChunk[iBand],
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        eWrkDataType, 0, 0, NULL );
                }

                if( bUseNoDataMask && eErr == CE_None )
                {
                    GDALRasterBand* poSrcBand = NULL;
                    if( iSrcOverview == -1 )
                        poSrcBand = papoSrcBands[0];
                    else
                        poSrcBand = papapoOverviewBands[0][iSrcOverview];
                    eErr = poSrcBand->GetMaskBand()->RasterIO(
                        GF_Read,
                        nChunkXOffQueried, nChunkYOffQueried,
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        pabyChunkNoDataMask,
                        nChunkXSizeQueried, nChunkYSizeQueried,
                        GDT_Byte, 0, 0, NULL );
                }

                // Compute the resulting overview block.
                for( int iBand = 0; iBand < nBands && eErr == CE_None; ++iBand )
                {
                    eErr = pfnResampleFn(
                        dfXRatioDstToSrc, dfYRatioDstToSrc,
                        0.0, 0.0,
                        eWrkDataType,
                        papaChunk[iBand],
                        pabyChunkNoDataMask,
                        nChunkXOffQueried, nChunkXSizeQueried,
                        nChunkYOffQueried, nChunkYSizeQueried,
                        nDstXOff, nDstXOff + nDstXCount,
                        nDstYOff, nDstYOff + nDstYCount,
                        papapoOverviewBands[iBand][iOverview],
                        pszResampling,
                        pabHasNoData[iBand],
                        pafNoDataValue[iBand],
                        /*poColorTable*/ NULL,
                        eDataType,
                        bPropagateNoData);
                }
            }

            dfCurPixelCount += static_cast<double>(nYCount) * nSrcWidth;
        }

        // Flush the data to overviews.
        for( int iBand = 0; iBand < nBands; ++iBand )
        {
            CPLFree(papaChunk[iBand]);
            papapoOverviewBands[iBand][iOverview]->FlushCache();
        }
        CPLFree(papaChunk);
        CPLFree(pabyChunkNoDataMask);
    }

    CPLFree(pabHasNoData);
    CPLFree(pafNoDataValue);

    if( eErr == CE_None )
        pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}

/************************************************************************/
/*                        GDALComputeBandStats()                        */
/************************************************************************/

/** Undocumented
 * @param hSrcBand undocumented.
 * @param nSampleStep undocumented.
 * @param pdfMean undocumented.
 * @param pdfStdDev undocumented.
 * @param pfnProgress undocumented.
 * @param pProgressData undocumented.
 * @return undocumented
 */
CPLErr CPL_STDCALL
GDALComputeBandStats( GDALRasterBandH hSrcBand,
                      int nSampleStep,
                      double *pdfMean, double *pdfStdDev,
                      GDALProgressFunc pfnProgress,
                      void *pProgressData )

{
    VALIDATE_POINTER1( hSrcBand, "GDALComputeBandStats", CE_Failure );

    GDALRasterBand *poSrcBand = static_cast<GDALRasterBand *>( hSrcBand );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    const int nWidth = poSrcBand->GetXSize();
    const int nHeight = poSrcBand->GetYSize();

    if( nSampleStep >= nHeight || nSampleStep < 1 )
        nSampleStep = 1;

    GDALDataType eWrkType = GDT_Unknown;
    float *pafData = NULL;
    GDALDataType eType = poSrcBand->GetRasterDataType();
    const bool bComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eType));
    if( bComplex )
    {
        pafData = static_cast<float *>(
            VSI_MALLOC_VERBOSE(nWidth * 2 * sizeof(float)) );
        eWrkType = GDT_CFloat32;
    }
    else
    {
        pafData = static_cast<float *>(
            VSI_MALLOC_VERBOSE(nWidth * sizeof(float)) );
        eWrkType = GDT_Float32;
    }

    if( nWidth == 0 || pafData == NULL )
    {
        VSIFree(pafData);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Loop over all sample lines.                                     */
/* -------------------------------------------------------------------- */
    double dfSum = 0.0;
    double dfSum2 = 0.0;
    int iLine = 0;
    int nSamples = 0;

    do
    {
        if( !pfnProgress( iLine / static_cast<double>( nHeight ),
                          NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            CPLFree( pafData );
            return CE_Failure;
        }

        const CPLErr eErr =
            poSrcBand->RasterIO( GF_Read, 0, iLine, nWidth, 1,
                                 pafData, nWidth, 1, eWrkType,
                                 0, 0, NULL );
        if( eErr != CE_None )
        {
            CPLFree( pafData );
            return eErr;
        }

        for( int iPixel = 0; iPixel < nWidth; ++iPixel )
        {
            float fValue = 0.0f;

            if( bComplex )
            {
                // Compute the magnitude of the complex value.
                fValue = static_cast<float>(
                    sqrt(pafData[iPixel*2  ] * pafData[iPixel*2  ]
                         + pafData[iPixel*2+1] * pafData[iPixel*2+1]) );
            }
            else
            {
                fValue = pafData[iPixel];
            }

            dfSum  += fValue;
            dfSum2 += fValue * fValue;
        }

        nSamples += nWidth;
        iLine += nSampleStep;
    } while( iLine < nHeight );

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        CPLFree( pafData );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Produce the result values.                                      */
/* -------------------------------------------------------------------- */
    if( pdfMean != NULL )
        *pdfMean = dfSum / nSamples;

    if( pdfStdDev != NULL )
    {
        const double dfMean = dfSum / nSamples;

        *pdfStdDev = sqrt((dfSum2 / nSamples) - (dfMean * dfMean));
    }

    CPLFree( pafData );

    return CE_None;
}

/************************************************************************/
/*                  GDALOverviewMagnitudeCorrection()                   */
/*                                                                      */
/*      Correct the mean and standard deviation of the overviews of     */
/*      the given band to match the base layer approximately.           */
/************************************************************************/

/** Undocumented
 * @param hBaseBand undocumented.
 * @param nOverviewCount undocumented.
 * @param pahOverviews undocumented.
 * @param pfnProgress undocumented.
 * @param pProgressData undocumented.
 * @return undocumented
 */
CPLErr
GDALOverviewMagnitudeCorrection( GDALRasterBandH hBaseBand,
                                 int nOverviewCount,
                                 GDALRasterBandH *pahOverviews,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData )

{
    VALIDATE_POINTER1( hBaseBand, "GDALOverviewMagnitudeCorrection",
                       CE_Failure );

/* -------------------------------------------------------------------- */
/*      Compute mean/stddev for source raster.                          */
/* -------------------------------------------------------------------- */
    double dfOrigMean = 0.0;
    double dfOrigStdDev = 0.0;
    {
        const CPLErr eErr
            = GDALComputeBandStats( hBaseBand, 2, &dfOrigMean, &dfOrigStdDev,
                                    pfnProgress, pProgressData );

        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Loop on overview bands.                                         */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0; iOverview < nOverviewCount; ++iOverview )
    {
        GDALRasterBand *poOverview = (GDALRasterBand *)pahOverviews[iOverview];
        double  dfOverviewMean, dfOverviewStdDev;

        const CPLErr eErr =
            GDALComputeBandStats( pahOverviews[iOverview], 1,
                                  &dfOverviewMean, &dfOverviewStdDev,
                                  pfnProgress, pProgressData );

        if( eErr != CE_None )
            return eErr;

        double dfGain = 1.0;
        if( dfOrigStdDev >= 0.0001 )
            dfGain = dfOrigStdDev / dfOverviewStdDev;

/* -------------------------------------------------------------------- */
/*      Apply gain and offset.                                          */
/* -------------------------------------------------------------------- */
        const int nWidth = poOverview->GetXSize();
        const int nHeight = poOverview->GetYSize();

        GDALDataType eWrkType = GDT_Unknown;
        float *pafData = NULL;
        const GDALDataType eType = poOverview->GetRasterDataType();
        const bool bComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eType));
        if( bComplex )
        {
            pafData = static_cast<float *>(
                VSI_MALLOC2_VERBOSE(nWidth, 2 * sizeof(float)) );
            eWrkType = GDT_CFloat32;
        }
        else
        {
            pafData = static_cast<float *>(
                VSI_MALLOC2_VERBOSE(nWidth, sizeof(float)) );
            eWrkType = GDT_Float32;
        }

        if( pafData == NULL )
        {
            return CE_Failure;
        }

        for( int iLine = 0; iLine < nHeight; ++iLine )
        {
            if( !pfnProgress( iLine / static_cast<double>( nHeight ),
                              NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                CPLFree( pafData );
                return CE_Failure;
            }

            if( poOverview->RasterIO( GF_Read, 0, iLine, nWidth, 1,
                                      pafData, nWidth, 1, eWrkType,
                                      0, 0, NULL ) != CE_None )
            {
                CPLFree( pafData );
                return CE_Failure;
            }

            for( int iPixel = 0; iPixel < nWidth; ++iPixel )
            {
                if( bComplex )
                {
                    pafData[iPixel*2] *= static_cast<float>( dfGain );
                    pafData[iPixel*2+1] *= static_cast<float>( dfGain );
                }
                else
                {
                    pafData[iPixel] = static_cast<float>(
                        (pafData[iPixel] - dfOverviewMean)
                        * dfGain + dfOrigMean );
                }
            }

            if( poOverview->RasterIO( GF_Write, 0, iLine, nWidth, 1,
                                      pafData, nWidth, 1, eWrkType,
                                      0, 0, NULL ) != CE_None )
            {
                CPLFree( pafData );
                return CE_Failure;
            }
        }

        if( !pfnProgress( 1.0, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            CPLFree( pafData );
            return CE_Failure;
        }

        CPLFree( pafData );
    }

    return CE_None;
}

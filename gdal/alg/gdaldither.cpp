/******************************************************************************
 *
 * Project:  CIETMap Phase 2
 * Purpose:  Convert RGB (24bit) to a pseudo-colored approximation using
 *           Floyd-Steinberg dithering (error diffusion).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007, Even Rouault <even dot rouault at mines-paris dot org>
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
 ******************************************************************************
 *
 * Notes:
 *
 * [1] Floyd-Steinberg dither:
 *  I should point out that the actual fractions we used were, assuming
 *  you are at X, moving left to right:
 *
 *                    X     7/16
 *             3/16   5/16  1/16
 *
 *  Note that the error goes to four neighbors, not three.  I think this
 *  will probably do better (at least for black and white) than the
 *  3/8-3/8-1/4 distribution, at the cost of greater processing.  I have
 *  seen the 3/8-3/8-1/4 distribution described as "our" algorithm before,
 *  but I have no idea who the credit really belongs to.
 *  --
 *                                          Lou Steinberg
 */

#include "cpl_port.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

#if defined(__x86_64) || defined(_M_X64)
#define USE_SSE2
#endif

#ifdef USE_SSE2

#include <emmintrin.h>
#define CAST_PCT(x) ((GByte*)x)
#define ALIGN_INT_ARRAY_ON_16_BYTE(x) \
    ( (((GUIntptr_t)(x) % 16) != 0 ) \
      ? (int*)((GByte*)(x) + 16 - ((GUIntptr_t)(x) % 16)) \
      : (x) )
#else
#define CAST_PCT(x) x
#endif

CPL_CVSID("$Id$")

static int MAKE_COLOR_CODE( int r, int g, int b )
{
  return r | (g << 8) | (b << 16);
}

static void FindNearestColor( int nColors, int *panPCT, GByte *pabyColorMap,
                              int nCLevels );
static int FindNearestColor( int nColors, int *panPCT,
                             int nRedValue, int nGreenValue, int nBlueValue );

// Structure for a hashmap from a color code to a color index of the
// color table.

// NOTE: if changing the size of this structure, edit
// MEDIAN_CUT_AND_DITHER_BUFFER_SIZE_65536 in gdal_alg_priv.h and take
// into account HashHistogram in gdalmediancut.cpp.
typedef struct
{
    GUInt32 nColorCode;
    GUInt32 nColorCode2;
    GUInt32 nColorCode3;
    GByte   nIndex;
    GByte   nIndex2;
    GByte   nIndex3;
    GByte   nPadding;
} ColorIndex;

/************************************************************************/
/*                         GDALDitherRGB2PCT()                          */
/************************************************************************/

/**
 * 24bit to 8bit conversion with dithering.
 *
 * This functions utilizes Floyd-Steinberg dithering in the process of
 * converting a 24bit RGB image into a pseudocolored 8bit image using a
 * provided color table.
 *
 * The red, green and blue input bands do not necessarily need to come
 * from the same file, but they must be the same width and height.  They will
 * be clipped to 8bit during reading, so non-eight bit bands are generally
 * inappropriate.  Likewise the hTarget band will be written with 8bit values
 * and must match the width and height of the source bands.
 *
 * The color table cannot have more than 256 entries.
 *
 * @param hRed Red input band.
 * @param hGreen Green input band.
 * @param hBlue Blue input band.
 * @param hTarget Output band.
 * @param hColorTable the color table to use with the output band.
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

int CPL_STDCALL
GDALDitherRGB2PCT( GDALRasterBandH hRed,
                   GDALRasterBandH hGreen,
                   GDALRasterBandH hBlue,
                   GDALRasterBandH hTarget,
                   GDALColorTableH hColorTable,
                   GDALProgressFunc pfnProgress,
                   void * pProgressArg )

{
    return GDALDitherRGB2PCTInternal( hRed, hGreen, hBlue, hTarget,
                                      hColorTable, 5, NULL, TRUE,
                                      pfnProgress, pProgressArg );
}

int GDALDitherRGB2PCTInternal(
    GDALRasterBandH hRed,
    GDALRasterBandH hGreen,
    GDALRasterBandH hBlue,
    GDALRasterBandH hTarget,
    GDALColorTableH hColorTable,
    int nBits,
    // NULL or at least 256 * 256 * 256 * sizeof(GInt16) bytes.
    GInt16* pasDynamicColorMap,
    int bDither,
    GDALProgressFunc pfnProgress,
    void* pProgressArg )
{
    VALIDATE_POINTER1( hRed, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hGreen, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hBlue, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hTarget, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hColorTable, "GDALDitherRGB2PCT", CE_Failure );

/* -------------------------------------------------------------------- */
/*      Validate parameters.                                            */
/* -------------------------------------------------------------------- */
    const int nXSize = GDALGetRasterBandXSize( hRed );
    const int nYSize = GDALGetRasterBandYSize( hRed );

    if( GDALGetRasterBandXSize( hGreen ) != nXSize
        || GDALGetRasterBandYSize( hGreen ) != nYSize
        || GDALGetRasterBandXSize( hBlue ) != nXSize
        || GDALGetRasterBandYSize( hBlue ) != nYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Green or blue band doesn't match size of red band." );

        return CE_Failure;
    }

    if( GDALGetRasterBandXSize( hTarget ) != nXSize
        || GDALGetRasterBandYSize( hTarget ) != nYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALDitherRGB2PCT(): "
                  "Target band doesn't match size of source bands." );

        return CE_Failure;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Setup more direct colormap.                                     */
/* -------------------------------------------------------------------- */
    int iColor;
#ifdef USE_SSE2
    int anPCTUnaligned[256+4];  // 4 for alignment on 16-byte boundary.
    int* anPCT = ALIGN_INT_ARRAY_ON_16_BYTE(anPCTUnaligned);
#else
    int anPCT[256*4] = {};
#endif
    const int nColors = GDALGetColorEntryCount( hColorTable );

    if( nColors == 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALDitherRGB2PCT(): "
                  "Color table must not be empty." );

        return CE_Failure;
    }
    else if( nColors > 256 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALDitherRGB2PCT(): "
                  "Color table cannot have more than 256 entries." );

        return CE_Failure;
    }

    iColor = 0;
    do
    {
        GDALColorEntry sEntry;

        GDALGetColorEntryAsRGB( hColorTable, iColor, &sEntry );
        CAST_PCT(anPCT)[4*iColor+0] = static_cast<GByte>(sEntry.c1);
        CAST_PCT(anPCT)[4*iColor+1] = static_cast<GByte>(sEntry.c2);
        CAST_PCT(anPCT)[4*iColor+2] = static_cast<GByte>(sEntry.c3);
        CAST_PCT(anPCT)[4*iColor+3] = 0;

        iColor++;
    } while( iColor < nColors );

#ifdef USE_SSE2
    // Pad to multiple of 8 colors.
    const int nColorsMod8 = nColors % 8;
    if( nColorsMod8 )
    {
        int iDest = nColors;
        for( iColor = 0; iColor < 8 - nColorsMod8 &&
                         iDest < 256; iColor ++, iDest++)
        {
            anPCT[iDest] = anPCT[nColors-1];
        }
    }
#endif

/* -------------------------------------------------------------------- */
/*      Setup various variables.                                        */
/* -------------------------------------------------------------------- */
    int nCLevels = 1 << nBits;
    ColorIndex* psColorIndexMap = NULL;

    GByte *pabyRed = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));
    GByte *pabyGreen = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));
    GByte *pabyBlue = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));

    GByte *pabyIndex = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));

    int *panError = static_cast<int *>(
        VSI_CALLOC_VERBOSE(sizeof(int), (nXSize + 2) * 3));

    if( pabyRed == NULL ||
        pabyGreen == NULL ||
        pabyBlue == NULL ||
        pabyIndex == NULL ||
        panError == NULL )
    {
        CPLFree( pabyRed );
        CPLFree( pabyGreen );
        CPLFree( pabyBlue );
        CPLFree( pabyIndex );
        CPLFree( panError );

        return  CE_Failure;
    }

    GByte *pabyColorMap = NULL;
    if( pasDynamicColorMap == NULL )
    {
/* -------------------------------------------------------------------- */
/*      Build a 24bit to 8 bit color mapping.                           */
/* -------------------------------------------------------------------- */

        pabyColorMap = static_cast<GByte *>(
            VSI_MALLOC_VERBOSE(nCLevels * nCLevels * nCLevels * sizeof(GByte)));
        if( pabyColorMap == NULL )
        {
            CPLFree( pabyRed );
            CPLFree( pabyGreen );
            CPLFree( pabyBlue );
            CPLFree( pabyIndex );
            CPLFree( panError );
            CPLFree( pabyColorMap );

            return CE_Failure;
        }

        FindNearestColor( nColors, anPCT, pabyColorMap, nCLevels);
    }
    else
    {
        pabyColorMap = NULL;
        if( nBits == 8 && static_cast<GIntBig>(nXSize) * nYSize <= 65536 )
        {
            // If the image is small enough, then the number of colors
            // will be limited and using a hashmap, rather than a full table
            // will be more efficient.
            psColorIndexMap = (ColorIndex*)pasDynamicColorMap;
            memset(psColorIndexMap, 0xFF, sizeof(ColorIndex) * PRIME_FOR_65536);
        }
        else
        {
            memset(pasDynamicColorMap, 0xFF, 256 * 256 * 256 * sizeof(GInt16));
        }
    }

/* ==================================================================== */
/*      Loop over all scanlines of data to process.                     */
/* ==================================================================== */
    CPLErr err = CE_None;

    for( int iScanline = 0; iScanline < nYSize; iScanline++ )
    {
/* -------------------------------------------------------------------- */
/*      Report progress                                                 */
/* -------------------------------------------------------------------- */
        if( !pfnProgress( iScanline / static_cast<double>(nYSize),
                          NULL, pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User Terminated" );
            CPLFree( pabyRed );
            CPLFree( pabyGreen );
            CPLFree( pabyBlue );
            CPLFree( pabyIndex );
            CPLFree( panError );
            CPLFree( pabyColorMap );

            return CE_Failure;
        }

/* -------------------------------------------------------------------- */
/*      Read source data.                                               */
/* -------------------------------------------------------------------- */
        CPLErr err1 =
            GDALRasterIO( hRed, GF_Read, 0, iScanline, nXSize, 1,
                          pabyRed, nXSize, 1, GDT_Byte, 0, 0 );
        if( err1 == CE_None )
            err1 = GDALRasterIO( hGreen, GF_Read, 0, iScanline, nXSize, 1,
                      pabyGreen, nXSize, 1, GDT_Byte, 0, 0 );
        if( err1 == CE_None )
            err1 = GDALRasterIO( hBlue, GF_Read, 0, iScanline, nXSize, 1,
                      pabyBlue, nXSize, 1, GDT_Byte, 0, 0 );
        if( err1 != CE_None )
        {
            CPLFree( pabyRed );
            CPLFree( pabyGreen );
            CPLFree( pabyBlue );
            CPLFree( pabyIndex );
            CPLFree( panError );
            CPLFree( pabyColorMap );

            return err1;
        }

/* -------------------------------------------------------------------- */
/*      Apply the error from the previous line to this one.             */
/* -------------------------------------------------------------------- */
        if( bDither )
        {
          for( int i = 0; i < nXSize; i++ )
          {
              pabyRed[i] = static_cast<GByte>(
                  std::max(0, std::min(255, (pabyRed[i] + panError[i*3+0+3]))));
              pabyGreen[i] = static_cast<GByte>(
                  std::max(0,
                           std::min(255, (pabyGreen[i] + panError[i*3+1+3]))));
              pabyBlue[i] = static_cast<GByte>(
                  std::max(0, std::min(255,
                                       (pabyBlue[i] + panError[i*3+2+3]))));
          }

          memset( panError, 0, sizeof(int) * (nXSize+2) * 3 );
        }

/* -------------------------------------------------------------------- */
/*      Figure out the nearest color to the RGB value.                  */
/* -------------------------------------------------------------------- */
        int nLastRedError = 0;
        int nLastGreenError = 0;
        int nLastBlueError = 0;

        for( int i = 0; i < nXSize; i++ )
        {
            const int nRedValue =
                std::max(0, std::min(255, pabyRed[i] + nLastRedError));
            const int nGreenValue =
                std::max(0, std::min(255, pabyGreen[i] + nLastGreenError));
            const int nBlueValue =
                std::max(0, std::min(255, pabyBlue[i] + nLastBlueError));

            int iIndex = 0;
            int nError = 0;
            int nSixth = 0;
            if( psColorIndexMap )
            {
                const GUInt32 nColorCode =
                    MAKE_COLOR_CODE(nRedValue, nGreenValue, nBlueValue);
                GUInt32 nIdx = nColorCode % PRIME_FOR_65536;
                while( true )
                {
                    if( psColorIndexMap[nIdx].nColorCode == nColorCode )
                    {
                        iIndex = psColorIndexMap[nIdx].nIndex;
                        break;
                    }
                    if( static_cast<int>(psColorIndexMap[nIdx].nColorCode) < 0 )
                    {
                        psColorIndexMap[nIdx].nColorCode = nColorCode;
                        iIndex = FindNearestColor(
                            nColors, anPCT, nRedValue, nGreenValue, nBlueValue);
                        psColorIndexMap[nIdx].nIndex =
                            static_cast<GByte>(iIndex);
                        break;
                    }
                    if( psColorIndexMap[nIdx].nColorCode2 == nColorCode )
                    {
                        iIndex = psColorIndexMap[nIdx].nIndex2;
                        break;
                    }
                    if( static_cast<int>(psColorIndexMap[nIdx].nColorCode2) <
                        0 )
                    {
                        psColorIndexMap[nIdx].nColorCode2 = nColorCode;
                        iIndex = FindNearestColor(
                            nColors, anPCT, nRedValue, nGreenValue, nBlueValue);
                        psColorIndexMap[nIdx].nIndex2 =
                            static_cast<GByte>(iIndex);
                        break;
                    }
                    if( psColorIndexMap[nIdx].nColorCode3 == nColorCode )
                    {
                        iIndex = psColorIndexMap[nIdx].nIndex3;
                        break;
                    }
                    if( static_cast<int>(psColorIndexMap[nIdx].nColorCode3) <
                        0 )
                    {
                        psColorIndexMap[nIdx].nColorCode3 = nColorCode;
                        iIndex = FindNearestColor( nColors, anPCT,
                                                   nRedValue, nGreenValue,
                                                   nBlueValue );
                        psColorIndexMap[nIdx].nIndex3 =
                            static_cast<GByte>(iIndex);
                        break;
                    }

                    do
                    {
                        nIdx+=257;
                        if( nIdx >= PRIME_FOR_65536 )
                            nIdx -= PRIME_FOR_65536;
                    }
                    while( static_cast<int>(psColorIndexMap[nIdx].nColorCode)
                           >= 0 &&
                           psColorIndexMap[nIdx].nColorCode != nColorCode &&
                           static_cast<int>(psColorIndexMap[nIdx].nColorCode2)
                           >= 0 &&
                           psColorIndexMap[nIdx].nColorCode2 != nColorCode&&
                           static_cast<int>(psColorIndexMap[nIdx].nColorCode3)
                           >= 0 &&
                           psColorIndexMap[nIdx].nColorCode3 != nColorCode );
                }
            }
            else if( pasDynamicColorMap == NULL )
            {
                const int iRed   = nRedValue *   nCLevels / 256;
                const int iGreen = nGreenValue * nCLevels / 256;
                const int iBlue  = nBlueValue *  nCLevels / 256;

                iIndex = pabyColorMap[iRed + iGreen * nCLevels
                                      + iBlue * nCLevels * nCLevels];
            }
            else
            {
                const GUInt32 nColorCode =
                    MAKE_COLOR_CODE(nRedValue, nGreenValue, nBlueValue);
                GInt16* psIndex = &pasDynamicColorMap[nColorCode];
                if( *psIndex < 0 )
                {
                    *psIndex = static_cast<GInt16>(
                        FindNearestColor( nColors, anPCT,
                                          nRedValue,
                                          nGreenValue,
                                          nBlueValue ));
                    iIndex = *psIndex;
                }
                else
                {
                    iIndex = *psIndex;
                }
            }

            pabyIndex[i] = static_cast<GByte>(iIndex);
            if( !bDither )
                continue;

/* -------------------------------------------------------------------- */
/*      Compute Red error, and carry it on to the next error line.      */
/* -------------------------------------------------------------------- */
            nError = nRedValue - CAST_PCT(anPCT)[4 * iIndex + 0];
            nSixth = nError / 6;

            panError[i * 3    ] += nSixth;
            panError[i * 3 + 6] = nSixth;
            panError[i * 3 + 3] += nError - 5 * nSixth;

            nLastRedError = 2 * nSixth;

/* -------------------------------------------------------------------- */
/*      Compute Green error, and carry it on to the next error line.    */
/* -------------------------------------------------------------------- */
            nError = nGreenValue - CAST_PCT(anPCT)[4*iIndex+1];
            nSixth = nError / 6;

            panError[i * 3 + 1] += nSixth;
            panError[i * 3 + 6 + 1] = nSixth;
            panError[i * 3 + 3 + 1] += nError - 5 * nSixth;

            nLastGreenError = 2 * nSixth;

/* -------------------------------------------------------------------- */
/*      Compute Blue error, and carry it on to the next error line.     */
/* -------------------------------------------------------------------- */
            nError = nBlueValue - CAST_PCT(anPCT)[4*iIndex+2];
            nSixth = nError / 6;

            panError[i * 3 + 2] += nSixth;
            panError[i * 3 + 6 + 2] = nSixth;
            panError[i * 3 + 3 + 2] += nError - 5 * nSixth;

            nLastBlueError = 2 * nSixth;
        }

/* -------------------------------------------------------------------- */
/*      Write results.                                                  */
/* -------------------------------------------------------------------- */
        err = GDALRasterIO( hTarget, GF_Write, 0, iScanline, nXSize, 1,
                      pabyIndex, nXSize, 1, GDT_Byte, 0, 0 );
        if( err != CE_None )
            break;
    }

    pfnProgress( 1.0, NULL, pProgressArg );

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pabyRed );
    CPLFree( pabyGreen );
    CPLFree( pabyBlue );
    CPLFree( pabyIndex );
    CPLFree( panError );
    CPLFree( pabyColorMap );

    return err;
}

static int FindNearestColor( int nColors, int *panPCT,
                             int nRedValue, int nGreenValue, int nBlueValue )

{
#ifdef USE_SSE2
    int nBestDist = 768;
    int nBestIndex = 0;

    int anDistanceUnaligned[16+4] = {};  // 4 for alignment on 16-byte boundary.
    int* anDistance = ALIGN_INT_ARRAY_ON_16_BYTE(anDistanceUnaligned);

    const __m128i ff = _mm_set1_epi32(0xFFFFFFFF);
    const __m128i mask_low = _mm_srli_epi64(ff, 32);
    const __m128i mask_high = _mm_slli_epi64(ff, 32);

    const unsigned int nColorVal =
        MAKE_COLOR_CODE(nRedValue, nGreenValue, nBlueValue);
    const __m128i thisColor = _mm_set1_epi32(nColorVal);
    const __m128i thisColor_low = _mm_srli_epi64(thisColor, 32);
    const __m128i thisColor_high = _mm_slli_epi64(thisColor, 32);

    for( int iColor = 0; iColor < nColors; iColor+=8 )
    {
        const __m128i pctColor = _mm_load_si128((__m128i*)&panPCT[iColor]);
        const __m128i pctColor2 = _mm_load_si128((__m128i*)&panPCT[iColor+4]);

        _mm_store_si128(
            (__m128i*)anDistance,
            _mm_sad_epu8(_mm_and_si128(pctColor, mask_low), thisColor_low));
        _mm_store_si128(
            (__m128i*)(anDistance+4),
            _mm_sad_epu8(_mm_and_si128(pctColor, mask_high), thisColor_high));
        _mm_store_si128(
            (__m128i*)(anDistance+8),
            _mm_sad_epu8(_mm_and_si128(pctColor2, mask_low), thisColor_low));
        _mm_store_si128(
            (__m128i*)(anDistance+12),
            _mm_sad_epu8(_mm_and_si128(pctColor2, mask_high), thisColor_high));

        if( anDistance[0] < nBestDist )
        {
            nBestIndex = iColor;
            nBestDist = anDistance[0];
        }
        if( anDistance[4] < nBestDist )
        {
            nBestIndex = iColor + 1;
            nBestDist = anDistance[4];
        }
        if( anDistance[2] < nBestDist )
        {
            nBestIndex = iColor + 2;
            nBestDist = anDistance[2];
        }
        if( anDistance[6] < nBestDist )
        {
            nBestIndex = iColor + 3;
            nBestDist = anDistance[6];
        }
        if( anDistance[8 + 0] < nBestDist )
        {
            nBestIndex = iColor + 4;
            nBestDist = anDistance[8 + 0];
        }
        if( anDistance[8 + 4] < nBestDist )
        {
            nBestIndex = iColor + 4 + 1;
            nBestDist = anDistance[8 + 4];
        }
        if( anDistance[8 + 2] < nBestDist )
        {
            nBestIndex = iColor + 4 + 2;
            nBestDist = anDistance[8 + 2];
        }
        if( anDistance[8 + 6] < nBestDist )
        {
            nBestIndex = iColor + 4 + 3;
            nBestDist = anDistance[8  +  6];
        }
    }
    return nBestIndex;
#else
    int nBestDist = 768;
    int nBestIndex = 0;

    for( int iColor = 0; iColor < nColors; iColor++ )
    {
        const int nThisDist =
            std::abs(nRedValue - panPCT[4*iColor + 0]) +
            std::abs(nGreenValue - panPCT[4*iColor + 1]) +
            std::abs(nBlueValue - panPCT[4*iColor + 2]);

        if( nThisDist < nBestDist )
        {
            nBestIndex = iColor;
            nBestDist = nThisDist;
        }
    }
    return nBestIndex;
#endif
}

/************************************************************************/
/*                          FindNearestColor()                          */
/*                                                                      */
/*      Finear near PCT color for any RGB color.                        */
/************************************************************************/

static void FindNearestColor( int nColors, int *panPCT, GByte *pabyColorMap,
                              int nCLevels )

{
/* -------------------------------------------------------------------- */
/*  Loop over all the cells in the high density cube.                   */
/* -------------------------------------------------------------------- */
    for( int iBlue = 0; iBlue < nCLevels; iBlue++ )
    {
        for( int iGreen = 0; iGreen < nCLevels; iGreen++ )
        {
            for( int iRed = 0; iRed < nCLevels; iRed++ )
            {
                const int nRedValue   = (iRed * 255) / (nCLevels - 1);
                const int nGreenValue = (iGreen * 255) / (nCLevels - 1);
                const int nBlueValue  = (iBlue * 255) / (nCLevels - 1);

                const int nBestIndex =
                    FindNearestColor( nColors, panPCT,
                                      nRedValue, nGreenValue, nBlueValue );
                pabyColorMap[iRed + iGreen*nCLevels
                             + iBlue*nCLevels*nCLevels] =
                    static_cast<GByte>(nBestIndex);
            }
        }
    }
}

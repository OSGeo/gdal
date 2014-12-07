/******************************************************************************
 * $Id$
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
 *					    Lou Steinberg
 *
 */

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"

#if defined(__x86_64) || defined(_M_X64)
#define USE_SSE2
#endif

#ifdef USE_SSE2

#include <emmintrin.h>
#define CAST_PCT(x) ((GByte*)x)
#define ALIGN_INT_ARRAY_ON_16_BYTE(x) ( (((GPtrDiff_t)(x) % 16) != 0 ) ? (int*)((GByte*)(x) + 16 - ((GPtrDiff_t)(x) % 16)) : (x) )

#else

#define CAST_PCT(x) x

#endif

#define MAKE_COLOR_CODE(r,g,b) ((r)|((g)<<8)|((b)<<16))

CPL_CVSID("$Id$");

static void FindNearestColor( int nColors, int *panPCT, GByte *pabyColorMap,
                              int nCLevels );
static int FindNearestColor( int nColors, int *panPCT,
                             int nRedValue, int nGreenValue, int nBlueValue );

/* Structure for a hashmap from a color code to a color index of the color table */
typedef struct  /* NOTE: if changing the size of this structure, edit MEDIAN_CUT_AND_DITHER_BUFFER_SIZE_65536 */
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

int GDALDitherRGB2PCTInternal( GDALRasterBandH hRed, 
                         GDALRasterBandH hGreen, 
                         GDALRasterBandH hBlue, 
                         GDALRasterBandH hTarget, 
                         GDALColorTableH hColorTable,
                         int nBits,
                         GInt16* pasDynamicColorMap, /* NULL or at least 256 * 256 * 256 * sizeof(GInt16) bytes */
                         int bDither,
                         GDALProgressFunc pfnProgress, 
                         void * pProgressArg )
{
    VALIDATE_POINTER1( hRed, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hGreen, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hBlue, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hTarget, "GDALDitherRGB2PCT", CE_Failure );
    VALIDATE_POINTER1( hColorTable, "GDALDitherRGB2PCT", CE_Failure );

    int		nXSize, nYSize;
    CPLErr err = CE_None;
    
/* -------------------------------------------------------------------- */
/*      Validate parameters.                                            */
/* -------------------------------------------------------------------- */
    nXSize = GDALGetRasterBandXSize( hRed );
    nYSize = GDALGetRasterBandYSize( hRed );

    if( GDALGetRasterBandXSize( hGreen ) != nXSize 
        || GDALGetRasterBandYSize( hGreen ) != nYSize 
        || GDALGetRasterBandXSize( hBlue ) != nXSize 
        || GDALGetRasterBandYSize( hBlue ) != nYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Green or blue band doesn't match size of red band.\n" );

        return CE_Failure;
    }

    if( GDALGetRasterBandXSize( hTarget ) != nXSize 
        || GDALGetRasterBandYSize( hTarget ) != nYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALDitherRGB2PCT(): "
                  "Target band doesn't match size of source bands.\n" );

        return CE_Failure;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Setup more direct colormap.                                     */
/* -------------------------------------------------------------------- */
    int		nColors, iColor;
#ifdef USE_SSE2
    int anPCTUnaligned[256+4]; /* 4 for alignment on 16-byte boundary */
    int* anPCT = ALIGN_INT_ARRAY_ON_16_BYTE(anPCTUnaligned);
#else
    int anPCT[256*4];
#endif
    nColors = GDALGetColorEntryCount( hColorTable );
    
    if (nColors == 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALDitherRGB2PCT(): "
                  "Color table must not be empty.\n" );

        return CE_Failure;
    }
    else if (nColors > 256)
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALDitherRGB2PCT(): "
                  "Color table cannot have more than 256 entries.\n" );

        return CE_Failure;
    }
    
    for( iColor = 0; iColor < nColors; iColor++ )
    {
        GDALColorEntry	sEntry;

        GDALGetColorEntryAsRGB( hColorTable, iColor, &sEntry );
        CAST_PCT(anPCT)[4*iColor+0] = sEntry.c1;
        CAST_PCT(anPCT)[4*iColor+1] = sEntry.c2;
        CAST_PCT(anPCT)[4*iColor+2] = sEntry.c3;
        CAST_PCT(anPCT)[4*iColor+3] = 0;
    }
#ifdef USE_SSE2
    /* Pad to multiple of 8 colors */
    int nColorsMod8 = nColors % 8;
    if( nColorsMod8 )
    {
        for( iColor = 0; iColor < 8 - nColorsMod8; iColor ++)
        {
            anPCT[nColors+iColor] = anPCT[nColors-1];
        }
    }
#endif

/* -------------------------------------------------------------------- */
/*      Setup various variables.                                        */
/* -------------------------------------------------------------------- */
    GByte   *pabyRed, *pabyGreen, *pabyBlue, *pabyIndex;
    GByte   *pabyColorMap = NULL;
    int     *panError;
    int nCLevels = 1 << nBits;
    ColorIndex* psColorIndexMap = NULL;

    pabyRed = (GByte *) VSIMalloc(nXSize);
    pabyGreen = (GByte *) VSIMalloc(nXSize);
    pabyBlue = (GByte *) VSIMalloc(nXSize);

    pabyIndex = (GByte *) VSIMalloc(nXSize);

    panError = (int *) VSICalloc(sizeof(int),(nXSize+2) * 3);
    
    if (pabyRed == NULL ||
        pabyGreen == NULL ||
        pabyBlue == NULL ||
        pabyIndex == NULL ||
        panError == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "VSIMalloc(): Out of memory in GDALDitherRGB2PCT" );
        err = CE_Failure;
        goto end_and_cleanup;
    }

    if( pasDynamicColorMap == NULL )
    {
/* -------------------------------------------------------------------- */
/*      Build a 24bit to 8 bit color mapping.                           */
/* -------------------------------------------------------------------- */

        pabyColorMap = (GByte *) VSIMalloc(nCLevels * nCLevels * nCLevels 
                                        * sizeof(GByte));
        if( pabyColorMap == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                  "VSIMalloc(): Out of memory in GDALDitherRGB2PCT" );
            err = CE_Failure;
            goto end_and_cleanup;
        }

        FindNearestColor( nColors, anPCT, pabyColorMap, nCLevels);
    }
    else
    {
        pabyColorMap = NULL;
        if( nBits == 8 && (GIntBig)nXSize * nYSize <= 65536 )
        {
            /* If the image is small enough, then the number of colors */
            /* will be limited and using a hashmap, rather than a full table */
            /* will be more efficient */
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
    int		iScanline;

    for( iScanline = 0; iScanline < nYSize; iScanline++ )
    {
        int	nLastRedError, nLastGreenError, nLastBlueError, i;

/* -------------------------------------------------------------------- */
/*      Report progress                                                 */
/* -------------------------------------------------------------------- */
        if( !pfnProgress( iScanline / (double) nYSize, NULL, pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User Terminated" );
            err = CE_Failure;
            goto end_and_cleanup;
        }

/* -------------------------------------------------------------------- */
/*      Read source data.                                               */
/* -------------------------------------------------------------------- */
        GDALRasterIO( hRed, GF_Read, 0, iScanline, nXSize, 1, 
                      pabyRed, nXSize, 1, GDT_Byte, 0, 0 );
        GDALRasterIO( hGreen, GF_Read, 0, iScanline, nXSize, 1, 
                      pabyGreen, nXSize, 1, GDT_Byte, 0, 0 );
        GDALRasterIO( hBlue, GF_Read, 0, iScanline, nXSize, 1, 
                      pabyBlue, nXSize, 1, GDT_Byte, 0, 0 );

/* -------------------------------------------------------------------- */
/*	Apply the error from the previous line to this one.		*/
/* -------------------------------------------------------------------- */
        if( bDither )
        {
          for( i = 0; i < nXSize; i++ )
          {
            pabyRed[i] = (GByte)
                MAX(0,MIN(255,(pabyRed[i]   + panError[i*3+0+3])));
            pabyGreen[i] = (GByte)
                MAX(0,MIN(255,(pabyGreen[i] + panError[i*3+1+3])));
            pabyBlue[i] =  (GByte)
                MAX(0,MIN(255,(pabyBlue[i]  + panError[i*3+2+3])));
          }

          memset( panError, 0, sizeof(int) * (nXSize+2) * 3 );
        }

/* -------------------------------------------------------------------- */
/*	Figure out the nearest color to the RGB value.			*/
/* -------------------------------------------------------------------- */
        nLastRedError = 0;
        nLastGreenError = 0;
        nLastBlueError = 0;

        for( i = 0; i < nXSize; i++ )
        {
            int		iIndex, nError, nSixth;
            int		nRedValue, nGreenValue, nBlueValue;

            nRedValue =   MAX(0,MIN(255, pabyRed[i]   + nLastRedError));
            nGreenValue = MAX(0,MIN(255, pabyGreen[i] + nLastGreenError));
            nBlueValue =  MAX(0,MIN(255, pabyBlue[i]  + nLastBlueError));

            if( psColorIndexMap )
            {
                GUInt32 nColorCode = MAKE_COLOR_CODE(nRedValue, nGreenValue, nBlueValue);
                GUInt32 nIdx = nColorCode % PRIME_FOR_65536;
                //int nCollisions = 0;
                //static int nMaxCollisions = 0;
                while( TRUE )
                {
                    if( psColorIndexMap[nIdx].nColorCode == nColorCode )
                    {
                        iIndex = psColorIndexMap[nIdx].nIndex;
                        break;
                    }
                    if( (int)psColorIndexMap[nIdx].nColorCode < 0 )
                    {
                        psColorIndexMap[nIdx].nColorCode = nColorCode;
                        iIndex = FindNearestColor( nColors, anPCT,
                                                   nRedValue, nGreenValue, nBlueValue );
                        psColorIndexMap[nIdx].nIndex = (GByte) iIndex;
                        break;
                    }
                    if( psColorIndexMap[nIdx].nColorCode2 == nColorCode )
                    {
                        iIndex = psColorIndexMap[nIdx].nIndex2;
                        break;
                    }
                    if( (int)psColorIndexMap[nIdx].nColorCode2 < 0 )
                    {
                        psColorIndexMap[nIdx].nColorCode2 = nColorCode;
                        iIndex = FindNearestColor( nColors, anPCT,
                                                   nRedValue, nGreenValue, nBlueValue );
                        psColorIndexMap[nIdx].nIndex2 = (GByte) iIndex;
                        break;
                    }
                    if( psColorIndexMap[nIdx].nColorCode3 == nColorCode )
                    {
                        iIndex = psColorIndexMap[nIdx].nIndex3;
                        break;
                    }
                    if( (int)psColorIndexMap[nIdx].nColorCode3 < 0 )
                    {
                        psColorIndexMap[nIdx].nColorCode3 = nColorCode;
                        iIndex = FindNearestColor( nColors, anPCT,
                                                   nRedValue, nGreenValue, nBlueValue );
                        psColorIndexMap[nIdx].nIndex3 = (GByte) iIndex;
                        break;
                    }

                    do
                    {
                        //nCollisions ++;
                        nIdx+=257;
                        if( nIdx >= PRIME_FOR_65536 )
                            nIdx -= PRIME_FOR_65536;
                    }
                    while( (int)psColorIndexMap[nIdx].nColorCode >= 0 &&
                            psColorIndexMap[nIdx].nColorCode != nColorCode &&
                            (int)psColorIndexMap[nIdx].nColorCode2 >= 0 &&
                            psColorIndexMap[nIdx].nColorCode2 != nColorCode&&
                            (int)psColorIndexMap[nIdx].nColorCode3 >= 0 &&
                            psColorIndexMap[nIdx].nColorCode3 != nColorCode );
                    /*if( nCollisions > nMaxCollisions )
                    {
                        nMaxCollisions = nCollisions;
                        printf("nCollisions = %d for R=%d,G=%d,B=%d\n",
                                nCollisions, nRedValue, nGreenValue, nBlueValue);
                    }*/
                }
            }
            else if( pasDynamicColorMap == NULL )
            {
                int iRed   = nRedValue *   nCLevels   / 256;
                int iGreen = nGreenValue * nCLevels / 256;
                int iBlue  = nBlueValue *  nCLevels  / 256;
                
                iIndex = pabyColorMap[iRed + iGreen * nCLevels 
                                    + iBlue * nCLevels * nCLevels];
            }
            else
            {
                GUInt32 nColorCode = MAKE_COLOR_CODE(nRedValue, nGreenValue, nBlueValue);
                GInt16* psIndex = &pasDynamicColorMap[nColorCode];
                if( *psIndex < 0 )
                    iIndex = *psIndex = FindNearestColor( nColors, anPCT,
                                                          nRedValue,
                                                          nGreenValue,
                                                          nBlueValue );
                else
                    iIndex = *psIndex;
            }

            pabyIndex[i] = (GByte) iIndex;
            if( !bDither )
                continue;

/* -------------------------------------------------------------------- */
/*      Compute Red error, and carry it on to the next error line.      */
/* -------------------------------------------------------------------- */
            nError = nRedValue - CAST_PCT(anPCT)[4*iIndex+0];
            nSixth = nError / 6;
            
            panError[i*3    ] += nSixth;
            panError[i*3+6  ] = nSixth;
            panError[i*3+3  ] += nError - 5 * nSixth;
            
            nLastRedError = 2 * nSixth;

/* -------------------------------------------------------------------- */
/*      Compute Green error, and carry it on to the next error line.    */
/* -------------------------------------------------------------------- */
            nError = nGreenValue - CAST_PCT(anPCT)[4*iIndex+1];
            nSixth = nError / 6;

            panError[i*3  +1] += nSixth;
            panError[i*3+6+1] = nSixth;
            panError[i*3+3+1] += nError - 5 * nSixth;
            
            nLastGreenError = 2 * nSixth;

/* -------------------------------------------------------------------- */
/*      Compute Blue error, and carry it on to the next error line.     */
/* -------------------------------------------------------------------- */
            nError = nBlueValue - CAST_PCT(anPCT)[4*iIndex+2];
            nSixth = nError / 6;
            
            panError[i*3  +2] += nSixth;
            panError[i*3+6+2] = nSixth;
            panError[i*3+3+2] += nError - 5 * nSixth;
            
            nLastBlueError = 2 * nSixth;
        }

/* -------------------------------------------------------------------- */
/*      Write results.                                                  */
/* -------------------------------------------------------------------- */
        GDALRasterIO( hTarget, GF_Write, 0, iScanline, nXSize, 1, 
                      pabyIndex, nXSize, 1, GDT_Byte, 0, 0 );
    }

    pfnProgress( 1.0, NULL, pProgressArg );

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
end_and_cleanup:
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
    int     iColor;

    int nBestDist = 768, nBestIndex = 0;

    int     anDistanceUnaligned[16+4]; /* 4 for alignment on 16-byte boundary */
    int* anDistance = ALIGN_INT_ARRAY_ON_16_BYTE(anDistanceUnaligned);

    const __m128i ff = _mm_set1_epi32(0xFFFFFFFF);
    const __m128i mask_low = _mm_srli_epi64(ff, 32);
    const __m128i mask_high = _mm_slli_epi64(ff, 32);

    unsigned int nColorVal = MAKE_COLOR_CODE(nRedValue, nGreenValue, nBlueValue);
    const __m128i thisColor = _mm_set1_epi32(nColorVal);
    const __m128i thisColor_low = _mm_srli_epi64(thisColor, 32);
    const __m128i thisColor_high = _mm_slli_epi64(thisColor, 32);

    for( iColor = 0; iColor < nColors; iColor+=8 )
    {
        __m128i pctColor = _mm_load_si128((__m128i*)&panPCT[iColor]);
        __m128i pctColor2 = _mm_load_si128((__m128i*)&panPCT[iColor+4]);

        _mm_store_si128((__m128i*)anDistance,
                        _mm_sad_epu8(_mm_and_si128(pctColor,mask_low),thisColor_low));
        _mm_store_si128((__m128i*)(anDistance+4),
                        _mm_sad_epu8(_mm_and_si128(pctColor,mask_high),thisColor_high));
        _mm_store_si128((__m128i*)(anDistance+8),
                        _mm_sad_epu8(_mm_and_si128(pctColor2,mask_low),thisColor_low));
        _mm_store_si128((__m128i*)(anDistance+12),
                        _mm_sad_epu8(_mm_and_si128(pctColor2,mask_high),thisColor_high));

        if( anDistance[0] < nBestDist )
        {
            nBestIndex = iColor;
            nBestDist = anDistance[0];
        }
        if( anDistance[4] < nBestDist )
        {
            nBestIndex = iColor+1;
            nBestDist = anDistance[4];
        }
        if( anDistance[2] < nBestDist )
        {
            nBestIndex = iColor+2;
            nBestDist = anDistance[2];
        }
        if( anDistance[6] < nBestDist )
        {
            nBestIndex = iColor+3;
            nBestDist = anDistance[6];
        }
        if( anDistance[8+0] < nBestDist )
        {
            nBestIndex = iColor+4;
            nBestDist = anDistance[8+0];
        }
        if( anDistance[8+4] < nBestDist )
        {
            nBestIndex = iColor+4+1;
            nBestDist = anDistance[8+4];
        }
        if( anDistance[8+2] < nBestDist )
        {
            nBestIndex = iColor+4+2;
            nBestDist = anDistance[8+2];
        }
        if( anDistance[8+6] < nBestDist )
        {
            nBestIndex = iColor+4+3;
            nBestDist = anDistance[8+6];
        }
    }
    return nBestIndex;
#else
    int     iColor;

    int nBestDist = 768, nBestIndex = 0;

    for( iColor = 0; iColor < nColors; iColor++ )
    {
        int     nThisDist;

        nThisDist = ABS(nRedValue   - panPCT[4*iColor+0]) 
                  + ABS(nGreenValue - panPCT[4*iColor+1])
                  + ABS(nBlueValue  - panPCT[4*iColor+2]);

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
    int     iBlue, iGreen, iRed;

/* -------------------------------------------------------------------- */
/*  Loop over all the cells in the high density cube.       */
/* -------------------------------------------------------------------- */
    for( iBlue = 0; iBlue < nCLevels; iBlue++ )
    {
        for( iGreen = 0; iGreen < nCLevels; iGreen++ )
        {
            for( iRed = 0; iRed < nCLevels; iRed++ )
            {
                int     nRedValue, nGreenValue, nBlueValue;

                nRedValue   = (iRed * 255) / (nCLevels-1);
                nGreenValue = (iGreen * 255) / (nCLevels-1);
                nBlueValue  = (iBlue * 255) / (nCLevels-1);

                int nBestIndex = FindNearestColor( nColors, panPCT,
                                        nRedValue, nGreenValue, nBlueValue );
                pabyColorMap[iRed + iGreen*nCLevels 
                                    + iBlue*nCLevels*nCLevels] = (GByte)nBestIndex;
            }
        }
    }
}

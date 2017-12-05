/******************************************************************************
 *
 * Project:  CIETMap Phase 2
 * Purpose:  Use median cut algorithm to generate an near-optimal PCT for a
 *           given RGB image.  Implemented as function GDALComputeMedianCutPCT.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 ******************************************************************************
 *
 * This code was based on the tiffmedian.c code from libtiff (www.libtiff.org)
 * which was based on a paper by Paul Heckbert:
 *
 *      "Color  Image Quantization for Frame Buffer Display", Paul
 *      Heckbert, SIGGRAPH proceedings, 1982, pp. 297-307.
 *
 */

#include "cpl_port.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"

#include <climits>
#include <cstring>

#include <algorithm>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

template<typename T> static T* HISTOGRAM( T *h, int n, int r, int g, int b )
{
    const int index = (r * n + g) * n + b;
    return &h[index];
}

static int MAKE_COLOR_CODE( int r, int g, int b )
{
    return r + g * 256 + b * 256 * 256;
}

// NOTE: If changing the size of this structure, edit
// MEDIAN_CUT_AND_DITHER_BUFFER_SIZE_65536 in gdal_alg_priv.h and take into
// account ColorIndex in gdaldither.cpp.
typedef struct
{
    GUInt32 nColorCode;
    int     nCount;
    GUInt32 nColorCode2;
    int     nCount2;
    GUInt32 nColorCode3;
    int     nCount3;
} HashHistogram;

typedef struct colorbox {
    struct colorbox *next, *prev;
    int rmin, rmax;
    int gmin, gmax;
    int bmin, bmax;
    GUIntBig total;
} Colorbox;

template<class T>
static void splitbox( Colorbox* ptr, const T* histogram,
                      const HashHistogram* psHashHistogram,
                      int nCLevels,
                      Colorbox **pfreeboxes, Colorbox **pusedboxes,
                      GByte* pabyRedBand,
                      GByte* pabyGreenBand,
                      GByte* pabyBlueBand, T nPixels );

template<class T>
static void shrinkbox( Colorbox* box,
                       const T* histogram,
                       int nCLevels );

static Colorbox* largest_box( Colorbox *usedboxes );

/************************************************************************/
/*                      GDALComputeMedianCutPCT()                       */
/************************************************************************/

/**
 * Compute optimal PCT for RGB image.
 *
 * This function implements a median cut algorithm to compute an "optimal"
 * pseudocolor table for representing an input RGB image.  This PCT could
 * then be used with GDALDitherRGB2PCT() to convert a 24bit RGB image into
 * an eightbit pseudo-colored image.
 *
 * This code was based on the tiffmedian.c code from libtiff (www.libtiff.org)
 * which was based on a paper by Paul Heckbert:
 *
 * \verbatim
 *   "Color  Image Quantization for Frame Buffer Display", Paul
 *   Heckbert, SIGGRAPH proceedings, 1982, pp. 297-307.
 * \endverbatim
 *
 * The red, green and blue input bands do not necessarily need to come
 * from the same file, but they must be the same width and height.  They will
 * be clipped to 8bit during reading, so non-eight bit bands are generally
 * inappropriate.
 *
 * @param hRed Red input band.
 * @param hGreen Green input band.
 * @param hBlue Blue input band.
 * @param pfnIncludePixel function used to test which pixels should be included
 * in the analysis.  At this time this argument is ignored and all pixels are
 * utilized.  This should normally be NULL.
 * @param nColors the desired number of colors to be returned (2-256).
 * @param hColorTable the colors will be returned in this color table object.
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 *
 * @return returns CE_None on success or CE_Failure if an error occurs.
 */

extern "C" int CPL_STDCALL
GDALComputeMedianCutPCT( GDALRasterBandH hRed,
                         GDALRasterBandH hGreen,
                         GDALRasterBandH hBlue,
                         int (*pfnIncludePixel)(int, int, void*),
                         int nColors,
                         GDALColorTableH hColorTable,
                         GDALProgressFunc pfnProgress,
                         void * pProgressArg )

{
    VALIDATE_POINTER1( hRed, "GDALComputeMedianCutPCT", CE_Failure );
    const int nXSize = GDALGetRasterBandXSize( hRed );
    const int nYSize = GDALGetRasterBandYSize( hRed );
    if( nYSize == 0 )
        return CE_Failure;
    if( static_cast<GUInt32>(nXSize) < UINT_MAX / static_cast<GUInt32>(nYSize) )
    {
        return GDALComputeMedianCutPCTInternal(hRed, hGreen, hBlue,
                                               NULL, NULL, NULL,
                                               pfnIncludePixel, nColors,
                                               5,
                                               static_cast<GUInt32 *>(NULL),
                                               hColorTable,
                                               pfnProgress, pProgressArg);
    }
    else
    {
#ifdef CPL_HAS_GINT64
        return GDALComputeMedianCutPCTInternal(hRed, hGreen, hBlue,
                                               NULL, NULL, NULL,
                                               pfnIncludePixel, nColors,
                                               5,
                                               static_cast<GUIntBig * >(NULL),
                                               hColorTable,
                                               pfnProgress, pProgressArg);
#else
        return CE_Failure;
#endif
    }
}

static inline int FindColorCount( const HashHistogram* psHashHistogram,
                                  GUInt32 nColorCode )
{

    GUInt32 nIdx = nColorCode % PRIME_FOR_65536;
    while( true )
    {
        if( static_cast<int>(psHashHistogram[nIdx].nColorCode) < 0 )
        {
            return 0;
        }
        if( psHashHistogram[nIdx].nColorCode == nColorCode )
        {
            return psHashHistogram[nIdx].nCount;
        }
        if( static_cast<int>(psHashHistogram[nIdx].nColorCode2) < 0 )
        {
            return 0;
        }
        if( psHashHistogram[nIdx].nColorCode2 == nColorCode )
        {
            return psHashHistogram[nIdx].nCount2;
        }
        if( static_cast<int>(psHashHistogram[nIdx].nColorCode3) < 0 )
        {
            return 0;
        }
        if( psHashHistogram[nIdx].nColorCode3 == nColorCode )
        {
            return psHashHistogram[nIdx].nCount3;
        }

        do
        {
            nIdx += 257;
            if( nIdx >= PRIME_FOR_65536 )
                nIdx -= PRIME_FOR_65536;
        }
        while( static_cast<int>(psHashHistogram[nIdx].nColorCode) >= 0 &&
               psHashHistogram[nIdx].nColorCode != nColorCode &&
               static_cast<int>(psHashHistogram[nIdx].nColorCode2) >= 0 &&
               psHashHistogram[nIdx].nColorCode2 != nColorCode&&
               static_cast<int>(psHashHistogram[nIdx].nColorCode3) >= 0 &&
               psHashHistogram[nIdx].nColorCode3 != nColorCode );
    }
}

static inline int*
FindAndInsertColorCount( HashHistogram* psHashHistogram, GUInt32 nColorCode )
{
    GUInt32 nIdx = nColorCode % PRIME_FOR_65536;
    while( true )
    {
        if( psHashHistogram[nIdx].nColorCode == nColorCode )
        {
            return &(psHashHistogram[nIdx].nCount);
        }
        if( static_cast<int>(psHashHistogram[nIdx].nColorCode) < 0 )
        {
            psHashHistogram[nIdx].nColorCode = nColorCode;
            psHashHistogram[nIdx].nCount = 0;
            return &(psHashHistogram[nIdx].nCount);
        }
        if( psHashHistogram[nIdx].nColorCode2 == nColorCode )
        {
            return &(psHashHistogram[nIdx].nCount2);
        }
        if( static_cast<int>(psHashHistogram[nIdx].nColorCode2) < 0 )
        {
            psHashHistogram[nIdx].nColorCode2 = nColorCode;
            psHashHistogram[nIdx].nCount2 = 0;
            return &(psHashHistogram[nIdx].nCount2);
        }
        if( psHashHistogram[nIdx].nColorCode3 == nColorCode )
        {
            return &(psHashHistogram[nIdx].nCount3);
        }
        if( static_cast<int>(psHashHistogram[nIdx].nColorCode3) < 0 )
        {
            psHashHistogram[nIdx].nColorCode3 = nColorCode;
            psHashHistogram[nIdx].nCount3 = 0;
            return &(psHashHistogram[nIdx].nCount3);
        }

        do
        {
            nIdx+=257;
            if( nIdx >= PRIME_FOR_65536 )
                nIdx -= PRIME_FOR_65536;
        }
        while( static_cast<int>(psHashHistogram[nIdx].nColorCode) >= 0 &&
               psHashHistogram[nIdx].nColorCode != nColorCode &&
               static_cast<int>(psHashHistogram[nIdx].nColorCode2) >= 0 &&
               psHashHistogram[nIdx].nColorCode2 != nColorCode&&
               static_cast<int>(psHashHistogram[nIdx].nColorCode3) >= 0 &&
               psHashHistogram[nIdx].nColorCode3 != nColorCode );
    }
}

template<class T> int
GDALComputeMedianCutPCTInternal(
    GDALRasterBandH hRed,
    GDALRasterBandH hGreen,
    GDALRasterBandH hBlue,
    GByte* pabyRedBand,
    GByte* pabyGreenBand,
    GByte* pabyBlueBand,
    int (*pfnIncludePixel)(int, int, void*),
    int nColors,
    int nBits,
    T* panHistogram,  // NULL, or >= size (1<<nBits)^3 * sizeof(T) bytes.
    GDALColorTableH hColorTable,
    GDALProgressFunc pfnProgress,
    void * pProgressArg )

{
    VALIDATE_POINTER1( hRed, "GDALComputeMedianCutPCT", CE_Failure );
    VALIDATE_POINTER1( hGreen, "GDALComputeMedianCutPCT", CE_Failure );
    VALIDATE_POINTER1( hBlue, "GDALComputeMedianCutPCT", CE_Failure );

    CPLErr err = CE_None;

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
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Green or blue band doesn't match size of red band.");

        return CE_Failure;
    }

    if( pfnIncludePixel != NULL )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GDALComputeMedianCutPCT() doesn't currently support "
                 "pfnIncludePixel function.");

        return CE_Failure;
    }

    if( nColors <= 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GDALComputeMedianCutPCT(): "
                 "nColors must be strictly greater than 1.");

        return CE_Failure;
    }

    if( nColors > 256 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GDALComputeMedianCutPCT(): "
                 "nColors must be lesser than or equal to 256.");

        return CE_Failure;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* ==================================================================== */
/*      STEP 1: create empty boxes.                                     */
/* ==================================================================== */
    if( static_cast<GUInt32>(nXSize) >
        std::numeric_limits<T>::max() / static_cast<GUInt32>(nYSize) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GDALComputeMedianCutPCTInternal() not called "
                 "with large enough type");
    }

    T nPixels = 0;
    if( nBits == 8 && pabyRedBand != NULL && pabyGreenBand != NULL &&
        pabyBlueBand != NULL &&
        static_cast<GUInt32>(nXSize) <=
        std::numeric_limits<T>::max() / static_cast<GUInt32>(nYSize) )
    {
      nPixels = static_cast<T>(nXSize) * static_cast<T>(nYSize);
    }

    const int nCLevels = 1 << nBits;
    T* histogram = NULL;
    HashHistogram* psHashHistogram = NULL;
    if( panHistogram )
    {
        if( nBits == 8 && static_cast<GUIntBig>(nXSize) * nYSize <= 65536 )
        {
            // If the image is small enough, then the number of colors
            // will be limited and using a hashmap, rather than a full table
            // will be more efficient.
            histogram = NULL;
            psHashHistogram = (HashHistogram*)panHistogram;
            memset(psHashHistogram,
                   0xFF,
                   sizeof(HashHistogram) * PRIME_FOR_65536);
        }
        else
        {
            histogram = panHistogram;
            memset(histogram, 0, nCLevels*nCLevels*nCLevels * sizeof(T));
        }
    }
    else
    {
        histogram = static_cast<T*>(
            VSI_CALLOC_VERBOSE(nCLevels * nCLevels * nCLevels, sizeof(T)));
        if( histogram == NULL )
        {
            return CE_Failure;
        }
    }
    Colorbox *box_list =
        static_cast<Colorbox *>(CPLMalloc(nColors*sizeof (Colorbox)));
    Colorbox *freeboxes = box_list;
    freeboxes[0].next = &freeboxes[1];
    freeboxes[0].prev = NULL;
    for( int i = 1; i < nColors-1; ++i )
    {
        freeboxes[i].next = &freeboxes[i+1];
        freeboxes[i].prev = &freeboxes[i-1];
    }
    freeboxes[nColors-1].next = NULL;
    freeboxes[nColors-1].prev = &freeboxes[nColors-2];

/* ==================================================================== */
/*      Build histogram.                                                */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Initialize the box datastructures.                              */
/* -------------------------------------------------------------------- */
    Colorbox *ptr = freeboxes;
    freeboxes = ptr->next;
    if( freeboxes )
        freeboxes->prev = NULL;
    Colorbox *usedboxes = NULL;  // TODO(schwehr): What?
    ptr->next = usedboxes;
    usedboxes = ptr;
    if( ptr->next )
        ptr->next->prev = ptr;

    ptr->rmin = 999;
    ptr->gmin = 999;
    ptr->bmin = 999;
    ptr->rmax = -1;
    ptr->gmax = -1;
    ptr->bmax = -1;
    ptr->total = static_cast<GUIntBig>(nXSize) * static_cast<GUIntBig>(nYSize);

/* -------------------------------------------------------------------- */
/*      Collect histogram.                                              */
/* -------------------------------------------------------------------- */

    // TODO(schwehr): Move these closer to usage after removing gotos.
    const int nColorShift = 8 - nBits;
    int nColorCounter = 0;
    GByte anRed[256] = {};
    GByte anGreen[256] = {};
    GByte anBlue[256] = {};

    GByte *pabyRedLine = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));
    GByte *pabyGreenLine = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));
    GByte *pabyBlueLine = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));

    if( pabyRedLine == NULL ||
        pabyGreenLine == NULL ||
        pabyBlueLine == NULL )
    {
        err = CE_Failure;
        goto end_and_cleanup;
    }

    for( int iLine = 0; iLine < nYSize; iLine++ )
    {
        if( !pfnProgress( iLine / static_cast<double>(nYSize),
                          "Generating Histogram", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User Terminated" );
            err = CE_Failure;
            goto end_and_cleanup;
        }

        err = GDALRasterIO( hRed, GF_Read, 0, iLine, nXSize, 1,
                      pabyRedLine, nXSize, 1, GDT_Byte, 0, 0 );
        if( err == CE_None )
            err = GDALRasterIO( hGreen, GF_Read, 0, iLine, nXSize, 1,
                      pabyGreenLine, nXSize, 1, GDT_Byte, 0, 0 );
        if( err == CE_None )
            err = GDALRasterIO( hBlue, GF_Read, 0, iLine, nXSize, 1,
                      pabyBlueLine, nXSize, 1, GDT_Byte, 0, 0 );
        if( err != CE_None )
            goto end_and_cleanup;

        for( int iPixel = 0; iPixel < nXSize; iPixel++ )
        {
            const int nRed = pabyRedLine[iPixel] >> nColorShift;
            const int nGreen = pabyGreenLine[iPixel] >> nColorShift;
            const int nBlue = pabyBlueLine[iPixel] >> nColorShift;

            ptr->rmin = std::min(ptr->rmin, nRed);
            ptr->gmin = std::min(ptr->gmin, nGreen);
            ptr->bmin = std::min(ptr->bmin, nBlue);
            ptr->rmax = std::max(ptr->rmax, nRed);
            ptr->gmax = std::max(ptr->gmax, nGreen);
            ptr->bmax = std::max(ptr->bmax, nBlue);

            bool bFirstOccurrence;
            if( psHashHistogram )
            {
                int* pnColor = FindAndInsertColorCount(psHashHistogram,
                                         MAKE_COLOR_CODE(nRed, nGreen, nBlue));
                bFirstOccurrence = ( *pnColor == 0 );
                (*pnColor)++;
            }
            else
            {
                T* pnColor =
                    HISTOGRAM(histogram, nCLevels, nRed, nGreen, nBlue);
                bFirstOccurrence = ( *pnColor == 0 );
                (*pnColor)++;
            }
            if( bFirstOccurrence)
            {
                if( nColorShift == 0 && nColorCounter < nColors )
                {
                    anRed[nColorCounter] = static_cast<GByte>(nRed);
                    anGreen[nColorCounter] = static_cast<GByte>(nGreen);
                    anBlue[nColorCounter] = static_cast<GByte>(nBlue);
                }
                nColorCounter++;
            }
        }
    }

    if( !pfnProgress( 1.0, "Generating Histogram", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User Terminated" );
        err = CE_Failure;
        goto end_and_cleanup;
    }

    if( nColorShift == 0 && nColorCounter <= nColors )
    {
#if DEBUG_VERBOSE
        CPLDebug("MEDIAN_CUT", "%d colors found <= %d", nColorCounter, nColors);
#endif
        for( int iColor = 0; iColor < nColorCounter; iColor++ )
        {
            const GDALColorEntry sEntry =
            {
                static_cast<GByte>(anRed[iColor]),
                static_cast<GByte>(anGreen[iColor]),
                static_cast<GByte>(anBlue[iColor]),
                255
            };
            GDALSetColorEntry( hColorTable, iColor, &sEntry );
        }
        goto end_and_cleanup;
    }

/* ==================================================================== */
/*      STEP 3: continually subdivide boxes until no more free          */
/*      boxes remain or until all colors assigned.                      */
/* ==================================================================== */
    while( freeboxes != NULL )
    {
        ptr = largest_box(usedboxes);
        if( ptr != NULL )
            splitbox(ptr, histogram, psHashHistogram, nCLevels,
                     &freeboxes, &usedboxes,
                     pabyRedBand, pabyGreenBand, pabyBlueBand, nPixels);
        else
            freeboxes = NULL;
    }

/* ==================================================================== */
/*      STEP 4: assign colors to all boxes                              */
/* ==================================================================== */
    ptr = usedboxes;
    for( int i = 0; ptr != NULL; ++i, ptr = ptr->next )
    {
        const GDALColorEntry sEntry = {
            static_cast<GByte>(((ptr->rmin + ptr->rmax) << nColorShift) / 2),
            static_cast<GByte>(((ptr->gmin + ptr->gmax) << nColorShift) / 2),
            static_cast<GByte>(((ptr->bmin + ptr->bmax) << nColorShift) / 2),
            255
        };
        GDALSetColorEntry( hColorTable, i, &sEntry );
    }

end_and_cleanup:
    CPLFree( pabyRedLine );
    CPLFree( pabyGreenLine );
    CPLFree( pabyBlueLine );

    // We're done with the boxes now.
    CPLFree(box_list);
    freeboxes = NULL;
    usedboxes = NULL;

    if( panHistogram == NULL )
        CPLFree( histogram );

    return err;
}

/************************************************************************/
/*                            largest_box()                             */
/************************************************************************/

static Colorbox *
largest_box( Colorbox *usedboxes )
{
    Colorbox *b = NULL;

    for( Colorbox* p = usedboxes; p != NULL; p = p->next )
    {
        if( (p->rmax > p->rmin || p->gmax > p->gmin ||
             p->bmax > p->bmin) && (b == NULL || p->total > b->total) )
        {
            b = p;
        }
    }
    return b;
}

static void shrinkboxFromBand( Colorbox* ptr,
                               const GByte* pabyRedBand,
                               const GByte* pabyGreenBand,
                               const GByte* pabyBlueBand, GUIntBig nPixels )
{
    int rmin_new = 255;
    int rmax_new = 0;
    int gmin_new = 255;
    int gmax_new = 0;
    int bmin_new = 255;
    int bmax_new = 0;
    for( GUIntBig i = 0; i < nPixels; i++ )
    {
        const int iR = pabyRedBand[i];
        const int iG = pabyGreenBand[i];
        const int iB = pabyBlueBand[i];
        if( iR >= ptr->rmin && iR <= ptr->rmax &&
            iG >= ptr->gmin && iG <= ptr->gmax &&
            iB >= ptr->bmin && iB <= ptr->bmax )
        {
            if( iR < rmin_new ) rmin_new = iR;
            if( iR > rmax_new ) rmax_new = iR;
            if( iG < gmin_new ) gmin_new = iG;
            if( iG > gmax_new ) gmax_new = iG;
            if( iB < bmin_new ) bmin_new = iB;
            if( iB > bmax_new ) bmax_new = iB;
        }
    }

    CPLAssert(rmin_new >= ptr->rmin && rmin_new <= rmax_new &&
              rmax_new <= ptr->rmax);
    CPLAssert(gmin_new >= ptr->gmin && gmin_new <= gmax_new &&
              gmax_new <= ptr->gmax);
    CPLAssert(bmin_new >= ptr->bmin && bmin_new <= bmax_new &&
              bmax_new <= ptr->bmax);
    ptr->rmin = rmin_new;
    ptr->rmax = rmax_new;
    ptr->gmin = gmin_new;
    ptr->gmax = gmax_new;
    ptr->bmin = bmin_new;
    ptr->bmax = bmax_new;
}

static void shrinkboxFromHashHistogram( Colorbox* box,
                                        const HashHistogram* psHashHistogram )
{
    if( box->rmax > box->rmin )
    {
        for( int ir = box->rmin; ir <= box->rmax; ++ir )
        {
            for( int ig = box->gmin; ig <= box->gmax; ++ig )
            {
                for( int ib = box->bmin; ib <= box->bmax; ++ib )
                {
                    if( FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0 )
                    {
                        box->rmin = ir;
                        goto have_rmin;
                    }
                }
            }
        }
    }
    have_rmin:
    if( box->rmax > box->rmin )
    {
        for( int ir = box->rmax; ir >= box->rmin; --ir )
        {
            for( int ig = box->gmin; ig <= box->gmax; ++ig )
            {
                for( int ib = box->bmin; ib <= box->bmax; ++ib )
                {
                    if( FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0 )
                    {
                        box->rmax = ir;
                        goto have_rmax;
                    }
                }
            }
        }
    }

    have_rmax:
    if( box->gmax > box->gmin )
    {
        for( int ig = box->gmin; ig <= box->gmax; ++ig )
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                for( int ib = box->bmin; ib <= box->bmax; ++ib )
                {
                    if( FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0 )
                    {
                        box->gmin = ig;
                        goto have_gmin;
                    }
                }
            }
        }
    }

    have_gmin:
    if( box->gmax > box->gmin )
    {
        for( int ig = box->gmax; ig >= box->gmin; --ig)
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                int ib = box->bmin;
                for( ; ib <= box->bmax; ++ib )
                {
                    if( FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0 )
                    {
                        box->gmax = ig;
                        goto have_gmax;
                    }
                }
            }
        }
    }

    have_gmax:
    if( box->bmax > box->bmin )
    {
        for( int ib = box->bmin; ib <= box->bmax; ++ib )
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                for( int ig = box->gmin; ig <= box->gmax; ++ig )
                {
                    if( FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0 )
                    {
                        box->bmin = ib;
                        goto have_bmin;
                    }
                }
            }
        }
    }

    have_bmin:
    if( box->bmax > box->bmin )
    {
        for( int ib = box->bmax; ib >= box->bmin; --ib )
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                for( int ig = box->gmin; ig <= box->gmax; ++ig )
                {
                    if( FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0 )
                    {
                        box->bmax = ib;
                        goto have_bmax;
                    }
                }
            }
        }
    }

    have_bmax:
    ;
}

/************************************************************************/
/*                              splitbox()                              */
/************************************************************************/
template<class T> static void
splitbox(Colorbox* ptr, const T* histogram,
         const HashHistogram* psHashHistogram,
         int nCLevels,
         Colorbox **pfreeboxes, Colorbox **pusedboxes,
         GByte* pabyRedBand,
         GByte* pabyGreenBand,
         GByte* pabyBlueBand, T nPixels)
{
    T hist2[256] = {};
    int first = 0;
    int last = 0;
    enum { RED, GREEN, BLUE } axis;

    // See which axis is the largest, do a histogram along that axis.  Split at
    // median point.  Contract both new boxes to fit points and return.
    {
        int i = ptr->rmax - ptr->rmin;
        if( i >= ptr->gmax - ptr->gmin  && i >= ptr->bmax - ptr->bmin )
            axis = RED;
        else if( ptr->gmax - ptr->gmin >= ptr->bmax - ptr->bmin )
            axis = GREEN;
        else
            axis = BLUE;
    }
    // Get histogram along longest axis.
    const GUInt32 nIters =
        (ptr->rmax - ptr->rmin + 1) *
        (ptr->gmax - ptr->gmin + 1) *
        (ptr->bmax - ptr->bmin + 1);

    switch( axis )
    {
      case RED:
      {
        if( nPixels != 0 && nIters > nPixels )
        {
            const int rmin = ptr->rmin;
            const int rmax = ptr->rmax;
            const int gmin = ptr->gmin;
            const int gmax = ptr->gmax;
            const int bmin = ptr->bmin;
            const int bmax = ptr->bmax;
            for( T iPixel = 0; iPixel < nPixels; iPixel++ )
            {
                int iR = pabyRedBand[iPixel];
                int iG = pabyGreenBand[iPixel];
                int iB = pabyBlueBand[iPixel];
                if( iR >= rmin && iR <= rmax &&
                    iG >= gmin && iG <= gmax &&
                    iB >= bmin && iB <= bmax )
                {
                    hist2[iR]++;
                }
            }
        }
        else if( psHashHistogram )
        {
            T *histp = &hist2[ptr->rmin];
            for( int ir = ptr->rmin; ir <= ptr->rmax; ++ir )
            {
                *histp = 0;
                for( int ig = ptr->gmin; ig <= ptr->gmax; ++ig )
                {
                    for( int ib = ptr->bmin; ib <= ptr->bmax; ++ib )
                    {
                        *histp += FindColorCount(psHashHistogram,
                                                 MAKE_COLOR_CODE(ir, ig, ib));
                    }
                }
                histp++;
            }
        }
        else
        {
            T *histp = &hist2[ptr->rmin];
            for( int ir = ptr->rmin; ir <= ptr->rmax; ++ir )
            {
                *histp = 0;
                for( int ig = ptr->gmin; ig <= ptr->gmax; ++ig )
                {
                    const T *iptr =
                        HISTOGRAM(histogram, nCLevels, ir, ig, ptr->bmin);
                    for( int ib = ptr->bmin; ib <= ptr->bmax; ++ib )
                        *histp += *iptr++;
                }
                histp++;
            }
        }
        first = ptr->rmin;
        last = ptr->rmax;
        break;
      }
      case GREEN:
      {
        if( nPixels != 0 && nIters > nPixels )
        {
            const int rmin = ptr->rmin;
            const int rmax = ptr->rmax;
            const int gmin = ptr->gmin;
            const int gmax = ptr->gmax;
            const int bmin = ptr->bmin;
            const int bmax = ptr->bmax;
            for( T iPixel = 0; iPixel < nPixels; iPixel++ )
            {
                const int iR = pabyRedBand[iPixel];
                const int iG = pabyGreenBand[iPixel];
                const int iB = pabyBlueBand[iPixel];
                if( iR >= rmin && iR <= rmax &&
                    iG >= gmin && iG <= gmax &&
                    iB >= bmin && iB <= bmax )
                {
                    hist2[iG]++;
                }
            }
        }
        else if( psHashHistogram )
        {
            T *histp = &hist2[ptr->gmin];
            for( int ig = ptr->gmin; ig <= ptr->gmax; ++ig )
            {
                *histp = 0;
                for( int ir = ptr->rmin; ir <= ptr->rmax; ++ir )
                {
                    for( int ib = ptr->bmin; ib <= ptr->bmax; ++ib )
                    {
                        *histp += FindColorCount(psHashHistogram,
                                                 MAKE_COLOR_CODE(ir, ig, ib));
                    }
                }
                histp++;
            }
        }
        else
        {
            T *histp = &hist2[ptr->gmin];
            for( int ig = ptr->gmin; ig <= ptr->gmax; ++ig )
            {
                *histp = 0;
                for( int ir = ptr->rmin; ir <= ptr->rmax; ++ir )
                {
                    const T *iptr =
                        HISTOGRAM(histogram, nCLevels, ir, ig, ptr->bmin);
                    for( int ib = ptr->bmin; ib <= ptr->bmax; ++ib )
                        *histp += *iptr++;
                }
                histp++;
            }
        }
        first = ptr->gmin;
        last = ptr->gmax;
        break;
      }
      case BLUE:
      {
        if( nPixels != 0 && nIters > nPixels )
        {
            const int rmin = ptr->rmin;
            const int rmax = ptr->rmax;
            const int gmin = ptr->gmin;
            const int gmax = ptr->gmax;
            const int bmin = ptr->bmin;
            const int bmax = ptr->bmax;
            for( T iPixel = 0; iPixel < nPixels; iPixel++ )
            {
                const int iR = pabyRedBand[iPixel];
                const int iG = pabyGreenBand[iPixel];
                const int iB = pabyBlueBand[iPixel];
                if( iR >= rmin && iR <= rmax &&
                    iG >= gmin && iG <= gmax &&
                    iB >= bmin && iB <= bmax )
                {
                    hist2[iB]++;
                }
            }
        }
        else if( psHashHistogram )
        {
            T *histp = &hist2[ptr->bmin];
            for( int ib = ptr->bmin; ib <= ptr->bmax; ++ib )
            {
                *histp = 0;
                for( int ir = ptr->rmin; ir <= ptr->rmax; ++ir )
                {
                    for( int ig = ptr->gmin; ig <= ptr->gmax; ++ig )
                    {
                        *histp += FindColorCount(psHashHistogram,
                                                 MAKE_COLOR_CODE(ir, ig, ib));
                    }
                }
                histp++;
            }
        }
        else
        {
            T *histp = &hist2[ptr->bmin];
            for( int ib = ptr->bmin; ib <= ptr->bmax; ++ib )
            {
                *histp = 0;
                for( int ir = ptr->rmin; ir <= ptr->rmax; ++ir )
                {
                    const T *iptr =
                        HISTOGRAM(histogram, nCLevels, ir, ptr->gmin, ib);
                    for( int ig = ptr->gmin; ig <= ptr->gmax; ++ig )
                    {
                        *histp += *iptr;
                        iptr += nCLevels;
                    }
                }
                histp++;
            }
        }
        first = ptr->bmin;
        last = ptr->bmax;
        break;
      }
    }
    // Find median point.
    T *histp = &hist2[first];
    int i = first;  // TODO(schwehr): Rename i.
    {
        T sum = 0;
        T sum2 = static_cast<T>(ptr->total / 2);
        for( ; i <= last && (sum += *histp++) < sum2; ++i )
            {}
    }
    if( i == first )
        i++;

    // Create new box, re-allocate points.
    Colorbox *new_cb = *pfreeboxes;
    *pfreeboxes = new_cb->next;
    if( *pfreeboxes )
        (*pfreeboxes)->prev = NULL;
    if( *pusedboxes )
        (*pusedboxes)->prev = new_cb;
    new_cb->next = *pusedboxes;
    *pusedboxes = new_cb;

    histp = &hist2[first];
    {
        T sum1 = 0;
        for( int j = first; j < i; j++ )
            sum1 += *histp++;
        T sum2 = 0;
        for( int j = i; j <= last; j++ )
            sum2 += *histp++;
        new_cb->total = sum1;
        ptr->total = sum2;
    }
    new_cb->rmin = ptr->rmin;
    new_cb->rmax = ptr->rmax;
    new_cb->gmin = ptr->gmin;
    new_cb->gmax = ptr->gmax;
    new_cb->bmin = ptr->bmin;
    new_cb->bmax = ptr->bmax;
    switch( axis )
    {
      case RED:
        new_cb->rmax = i - 1;
        ptr->rmin = i;
        break;
      case GREEN:
        new_cb->gmax = i - 1;
        ptr->gmin = i;
        break;
      case BLUE:
        new_cb->bmax = i - 1;
        ptr->bmin = i;
        break;
    }

    if( nPixels != 0 &&
        static_cast<T>(new_cb->rmax - new_cb->rmin + 1) *
        static_cast<T>(new_cb->gmax - new_cb->gmin + 1) *
        static_cast<T>(new_cb->bmax - new_cb->bmin + 1) > nPixels )
    {
        shrinkboxFromBand(new_cb, pabyRedBand, pabyGreenBand, pabyBlueBand,
                          nPixels);
    }
    else if( psHashHistogram != NULL )
    {
        shrinkboxFromHashHistogram(new_cb, psHashHistogram);
    }
    else
    {
        shrinkbox(new_cb, histogram, nCLevels);
    }

    if( nPixels != 0 &&
        static_cast<T>(ptr->rmax - ptr->rmin + 1) *
        static_cast<T>(ptr->gmax - ptr->gmin + 1) *
        static_cast<T>(ptr->bmax - ptr->bmin + 1) > nPixels )
    {
        shrinkboxFromBand(ptr, pabyRedBand, pabyGreenBand, pabyBlueBand,
                          nPixels);
    }
    else if( psHashHistogram != NULL )
    {
        shrinkboxFromHashHistogram(ptr, psHashHistogram);
    }
    else
    {
        shrinkbox(ptr, histogram, nCLevels);
    }
}

/************************************************************************/
/*                             shrinkbox()                              */
/************************************************************************/
template<class T> static void
shrinkbox( Colorbox* box, const T* histogram, int nCLevels )
{
    if( box->rmax > box->rmin )
    {
        for( int ir = box->rmin; ir <= box->rmax; ++ir )
        {
            for( int ig = box->gmin; ig <= box->gmax; ++ig )
            {
                const T *histp =
                    HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                for( int ib = box->bmin; ib <= box->bmax; ++ib )
                {
                    if( *histp++ != 0 )
                    {
                        box->rmin = ir;
                        goto have_rmin;
                    }
                }
            }
        }
    }
    have_rmin:
    if( box->rmax > box->rmin )
    {
        for( int ir = box->rmax; ir >= box->rmin; --ir )
        {
            for( int ig = box->gmin; ig <= box->gmax; ++ig )
            {
                const T *histp =
                    HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                for( int ib = box->bmin; ib <= box->bmax; ++ib )
                {
                    if( *histp++ != 0 )
                    {
                        box->rmax = ir;
                        goto have_rmax;
                    }
                }
            }
        }
    }

    have_rmax:
    if( box->gmax > box->gmin )
    {
        for( int ig = box->gmin; ig <= box->gmax; ++ig )
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                const T *histp =
                    HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                for( int ib = box->bmin; ib <= box->bmax; ++ib )
                {
                    if( *histp++ != 0 )
                    {
                        box->gmin = ig;
                        goto have_gmin;
                    }
                }
            }
        }
    }

    have_gmin:
    if( box->gmax > box->gmin )
    {
        for( int ig = box->gmax; ig >= box->gmin; --ig )
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                const T *histp =
                    HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                for( int ib = box->bmin; ib <= box->bmax; ++ib )
                {
                    if( *histp++ != 0 )
                    {
                        box->gmax = ig;
                        goto have_gmax;
                    }
                }
            }
        }
    }

    have_gmax:
    if( box->bmax > box->bmin )
    {
        for( int ib = box->bmin; ib <= box->bmax; ++ib )
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                const T *histp =
                    HISTOGRAM(histogram, nCLevels, ir, box->gmin, ib);
                for( int ig = box->gmin; ig <= box->gmax; ++ig )
                {
                    if( *histp != 0 )
                    {
                        box->bmin = ib;
                        goto have_bmin;
                    }
                    histp += nCLevels;
                }
            }
        }
    }

    have_bmin:
    if( box->bmax > box->bmin )
    {
        for( int ib = box->bmax; ib >= box->bmin; --ib )
        {
            for( int ir = box->rmin; ir <= box->rmax; ++ir )
            {
                const T *histp =
                    HISTOGRAM(histogram, nCLevels, ir, box->gmin, ib);
                for( int ig = box->gmin; ig <= box->gmax; ++ig )
                {
                    if( *histp != 0 )
                    {
                        box->bmax = ib;
                        goto have_bmax;
                    }
                    histp += nCLevels;
                }
            }
        }
    }

    have_bmax:
    ;
}

// Explicitly instantiate template functions.
template int
GDALComputeMedianCutPCTInternal<GUInt32>(
    GDALRasterBandH hRed,
    GDALRasterBandH hGreen,
    GDALRasterBandH hBlue,
    GByte* pabyRedBand,
    GByte* pabyGreenBand,
    GByte* pabyBlueBand,
    int (*pfnIncludePixel)(int, int, void*),
    int nColors,
    int nBits,
    GUInt32* panHistogram,
    GDALColorTableH hColorTable,
    GDALProgressFunc pfnProgress,
    void * pProgressArg );

template int
GDALComputeMedianCutPCTInternal<GUIntBig>(
    GDALRasterBandH hRed,
    GDALRasterBandH hGreen,
    GDALRasterBandH hBlue,
    GByte* pabyRedBand,
    GByte* pabyGreenBand,
    GByte* pabyBlueBand,
    int (*pfnIncludePixel)(int, int, void*),
    int nColors,
    int nBits,
    GUIntBig* panHistogram,
    GDALColorTableH hColorTable,
    GDALProgressFunc pfnProgress,
    void * pProgressArg );

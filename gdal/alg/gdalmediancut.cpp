/******************************************************************************
 * $Id$
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

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"

CPL_CVSID("$Id$");

#define HISTOGRAM(h,n,r,g,b) h[((r)*(n)+(g))*(n)+(b)]

#define MAKE_COLOR_CODE(r,g,b) ((r)+(g)*256+(b)*256*256)

typedef struct /* NOTE: if changing the size of this structure, edit MEDIAN_CUT_AND_DITHER_BUFFER_SIZE_65536 */
{
    GUInt32 nColorCode;
    int     nCount;
    GUInt32 nColorCode2;
    int     nCount2;
    GUInt32 nColorCode3;
    int     nCount3;
} HashHistogram;

typedef	struct colorbox {
	struct	colorbox *next, *prev;
	int	rmin, rmax;
	int	gmin, gmax;
	int	bmin, bmax;
	int	total;
} Colorbox;

static void splitbox(Colorbox* ptr, const int* histogram,
                     const HashHistogram* psHashHistogram,
                     int nCLevels,
                     Colorbox **pfreeboxes, Colorbox **pusedboxes,
                     GByte* pabyRedBand,
                     GByte* pabyGreenBand,
                     GByte* pabyBlueBand, int nPixels);
static void shrinkbox(Colorbox* box,
                      const int* histogram,
                      int nCLevels);
static Colorbox* largest_box(Colorbox *usedboxes);

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
                         int (*pfnIncludePixel)(int,int,void*),
                         int nColors, 
                         GDALColorTableH hColorTable,
                         GDALProgressFunc pfnProgress, 
                         void * pProgressArg )

{
    return GDALComputeMedianCutPCTInternal(hRed, hGreen, hBlue,
                                     NULL, NULL, NULL,
                                     pfnIncludePixel, nColors,
                                     5,
                                     NULL,
                                     hColorTable,
                                     pfnProgress, pProgressArg);
}

/*static int nMaxCollisions = 0;*/

static inline int FindColorCount(const HashHistogram* psHashHistogram,
                                 GUInt32 nColorCode)
{
    GUInt32 nIdx = nColorCode % PRIME_FOR_65536;
    /*int nCollisions = 0; */
    while( TRUE )
    {
        if( (int)psHashHistogram[nIdx].nColorCode < 0 )
        {
            return 0;
        }
        if( psHashHistogram[nIdx].nColorCode == nColorCode )
        {
            return psHashHistogram[nIdx].nCount;
        }
        if( (int)psHashHistogram[nIdx].nColorCode2 < 0 )
        {
            return 0;
        }
        if( psHashHistogram[nIdx].nColorCode2 == nColorCode )
        {
            return psHashHistogram[nIdx].nCount2;
        }
        if( (int)psHashHistogram[nIdx].nColorCode3 < 0 )
        {
            return 0;
        }
        if( psHashHistogram[nIdx].nColorCode3 == nColorCode )
        {
            return psHashHistogram[nIdx].nCount3;
        }

        do
        {
            /*nCollisions ++;*/
            nIdx+=257;
            if( nIdx >= PRIME_FOR_65536 )
                nIdx -= PRIME_FOR_65536;
        }
        while( (int)psHashHistogram[nIdx].nColorCode >= 0 &&
                psHashHistogram[nIdx].nColorCode != nColorCode &&
                (int)psHashHistogram[nIdx].nColorCode2 >= 0 &&
                psHashHistogram[nIdx].nColorCode2 != nColorCode&&
                (int)psHashHistogram[nIdx].nColorCode3 >= 0 &&
                psHashHistogram[nIdx].nColorCode3 != nColorCode );
        /*if( nCollisions > nMaxCollisions )
        {
            nMaxCollisions = nCollisions;
            printf("median cut: nCollisions = %d for R=%d,G=%d,B=%d\n",
                    nCollisions, nColorCode&0xFF, (nColorCode>>8)&0xFF, (nColorCode>>16)&0xFF);
        }*/
    }
}

static inline int* FindAndInsertColorCount(HashHistogram* psHashHistogram,
                           GUInt32 nColorCode)
{
    GUInt32 nIdx = nColorCode % PRIME_FOR_65536;
    /*int nCollisions = 0;*/
    while( TRUE )
    {
        if( psHashHistogram[nIdx].nColorCode == nColorCode )
        {
            return &(psHashHistogram[nIdx].nCount);
        }
        if( (int)psHashHistogram[nIdx].nColorCode < 0 )
        {
            psHashHistogram[nIdx].nColorCode = nColorCode;
            psHashHistogram[nIdx].nCount = 0;
            return &(psHashHistogram[nIdx].nCount);
        }
        if( psHashHistogram[nIdx].nColorCode2 == nColorCode )
        {
            return &(psHashHistogram[nIdx].nCount2);
        }
        if( (int)psHashHistogram[nIdx].nColorCode2 < 0 )
        {
            psHashHistogram[nIdx].nColorCode2 = nColorCode;
            psHashHistogram[nIdx].nCount2 = 0;
            return &(psHashHistogram[nIdx].nCount2);
        }
        if( psHashHistogram[nIdx].nColorCode3 == nColorCode )
        {
            return &(psHashHistogram[nIdx].nCount3);
        }
        if( (int)psHashHistogram[nIdx].nColorCode3 < 0 )
        {
            psHashHistogram[nIdx].nColorCode3 = nColorCode;
            psHashHistogram[nIdx].nCount3 = 0;
            return &(psHashHistogram[nIdx].nCount3);
        }

        do
        {
            /*nCollisions ++;*/
            nIdx+=257;
            if( nIdx >= PRIME_FOR_65536 )
                nIdx -= PRIME_FOR_65536;
        }
        while( (int)psHashHistogram[nIdx].nColorCode >= 0 &&
                psHashHistogram[nIdx].nColorCode != nColorCode &&
                (int)psHashHistogram[nIdx].nColorCode2 >= 0 &&
                psHashHistogram[nIdx].nColorCode2 != nColorCode&&
                (int)psHashHistogram[nIdx].nColorCode3 >= 0 &&
                psHashHistogram[nIdx].nColorCode3 != nColorCode );
        /*if( nCollisions > nMaxCollisions )
        {
            nMaxCollisions = nCollisions;
            printf("median cut: nCollisions = %d for R=%d,G=%d,B=%d\n",
                    nCollisions, nColorCode&0xFF, (nColorCode>>8)&0xFF, (nColorCode>>16)&0xFF);
        }*/
    }
}

int
GDALComputeMedianCutPCTInternal( GDALRasterBandH hRed, 
                           GDALRasterBandH hGreen, 
                           GDALRasterBandH hBlue, 
                           GByte* pabyRedBand,
                           GByte* pabyGreenBand,
                           GByte* pabyBlueBand,
                           int (*pfnIncludePixel)(int,int,void*),
                           int nColors, 
                           int nBits,
                           int* panHistogram, /* NULL, or at least of size (1<<nBits)^3 * sizeof(int) bytes */
                           GDALColorTableH hColorTable,
                           GDALProgressFunc pfnProgress, 
                           void * pProgressArg )

{
    VALIDATE_POINTER1( hRed, "GDALComputeMedianCutPCT", CE_Failure );
    VALIDATE_POINTER1( hGreen, "GDALComputeMedianCutPCT", CE_Failure );
    VALIDATE_POINTER1( hBlue, "GDALComputeMedianCutPCT", CE_Failure );

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

    if( pfnIncludePixel != NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALComputeMedianCutPCT() doesn't currently support "
                  " pfnIncludePixel function." );

        return CE_Failure;
    }

    if ( nColors <= 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALComputeMedianCutPCT() : nColors must be strictly greater than 1." );

        return CE_Failure;
    }

    if ( nColors > 256 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALComputeMedianCutPCT() : nColors must be lesser than or equal to 256." );

        return CE_Failure;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* ==================================================================== */
/*      STEP 1: create empty boxes.                                     */
/* ==================================================================== */
    int	     i;
    Colorbox *box_list, *ptr;
    int* histogram;
    Colorbox *freeboxes;
    Colorbox *usedboxes;
    int nCLevels = 1 << nBits;
    int nColorShift = 8 - nBits;
    int nColorCounter = 0;
    GByte anRed[256], anGreen[256], anBlue[256];
    int nPixels = 0;
    HashHistogram* psHashHistogram = NULL;
    
    if( nBits == 8 && pabyRedBand != NULL && pabyGreenBand != NULL &&
        pabyBlueBand != NULL && nXSize < INT_MAX / nYSize )
    {
        nPixels = nXSize * nYSize;
    }

    if( panHistogram )
    {
        if( nBits == 8 && (GIntBig)nXSize * nYSize <= 65536 )
        {
            /* If the image is small enough, then the number of colors */
            /* will be limited and using a hashmap, rather than a full table */
            /* will be more efficient */
            histogram = NULL;
            psHashHistogram = (HashHistogram*)panHistogram;
            memset(psHashHistogram, 0xFF, sizeof(HashHistogram) * PRIME_FOR_65536);
        }
        else
        {
            histogram = panHistogram;
            memset(histogram, 0, nCLevels*nCLevels*nCLevels * sizeof(int));
        }
    }
    else
    {
        histogram = (int*) VSICalloc(nCLevels*nCLevels*nCLevels,sizeof(int));
        if( histogram == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                  "VSICalloc(): Out of memory in GDALComputeMedianCutPCT" );
            return CE_Failure;
        }
    }
    usedboxes = NULL;
    box_list = freeboxes = (Colorbox *)CPLMalloc(nColors*sizeof (Colorbox));
    freeboxes[0].next = &freeboxes[1];
    freeboxes[0].prev = NULL;
    for (i = 1; i < nColors-1; ++i) {
        freeboxes[i].next = &freeboxes[i+1];
        freeboxes[i].prev = &freeboxes[i-1];
    }
    freeboxes[nColors-1].next = NULL;
    freeboxes[nColors-1].prev = &freeboxes[nColors-2];

/* ==================================================================== */
/*      Build histogram.                                                */
/* ==================================================================== */
    GByte	*pabyRedLine, *pabyGreenLine, *pabyBlueLine;
    int		iLine, iPixel;

/* -------------------------------------------------------------------- */
/*      Initialize the box datastructures.                              */
/* -------------------------------------------------------------------- */
    ptr = freeboxes;
    freeboxes = ptr->next;
    if (freeboxes)
        freeboxes->prev = NULL;
    ptr->next = usedboxes;
    usedboxes = ptr;
    if (ptr->next)
        ptr->next->prev = ptr;

    ptr->rmin = ptr->gmin = ptr->bmin = 999;
    ptr->rmax = ptr->gmax = ptr->bmax = -1;
    ptr->total = nXSize * nYSize;

/* -------------------------------------------------------------------- */
/*      Collect histogram.                                              */
/* -------------------------------------------------------------------- */
    pabyRedLine = (GByte *) VSIMalloc(nXSize);
    pabyGreenLine = (GByte *) VSIMalloc(nXSize);
    pabyBlueLine = (GByte *) VSIMalloc(nXSize);
    
    if (pabyRedLine == NULL ||
        pabyGreenLine == NULL ||
        pabyBlueLine == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "VSIMalloc(): Out of memory in GDALComputeMedianCutPCT" );
        err = CE_Failure;
        goto end_and_cleanup;
    }

    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        if( !pfnProgress( iLine / (double) nYSize, 
                          "Generating Histogram", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User Terminated" );
            err = CE_Failure;
            goto end_and_cleanup;
        }

        GDALRasterIO( hRed, GF_Read, 0, iLine, nXSize, 1, 
                      pabyRedLine, nXSize, 1, GDT_Byte, 0, 0 );
        GDALRasterIO( hGreen, GF_Read, 0, iLine, nXSize, 1, 
                      pabyGreenLine, nXSize, 1, GDT_Byte, 0, 0 );
        GDALRasterIO( hBlue, GF_Read, 0, iLine, nXSize, 1, 
                      pabyBlueLine, nXSize, 1, GDT_Byte, 0, 0 );

        for( iPixel = 0; iPixel < nXSize; iPixel++ )
        {
            int	nRed, nGreen, nBlue;
            
            nRed = pabyRedLine[iPixel] >> nColorShift;
            nGreen = pabyGreenLine[iPixel] >> nColorShift;
            nBlue = pabyBlueLine[iPixel] >> nColorShift;

            ptr->rmin = MIN(ptr->rmin, nRed);
            ptr->gmin = MIN(ptr->gmin, nGreen);
            ptr->bmin = MIN(ptr->bmin, nBlue);
            ptr->rmax = MAX(ptr->rmax, nRed);
            ptr->gmax = MAX(ptr->gmax, nGreen);
            ptr->bmax = MAX(ptr->bmax, nBlue);

            int* pnColor;
            if( psHashHistogram )
            {
                pnColor = FindAndInsertColorCount(psHashHistogram,
                                         MAKE_COLOR_CODE(nRed, nGreen, nBlue));
            }
            else
            {
                pnColor = &HISTOGRAM(histogram, nCLevels, nRed, nGreen, nBlue);
            }
            if( *pnColor == 0 )
            {
                if( nColorShift == 0 && nColorCounter < nColors )
                {
                    anRed[nColorCounter] = nRed;
                    anGreen[nColorCounter] = nGreen;
                    anBlue[nColorCounter] = nBlue;
                }
                nColorCounter++;
            }
            (*pnColor) ++;
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
        //CPLDebug("MEDIAN_CUT", "%d colors found <= %d", nColorCounter, nColors);
        for(int iColor = 0;iColor<nColorCounter;iColor++)
        {
            GDALColorEntry  sEntry;
            sEntry.c1 = (GByte) anRed[iColor];
            sEntry.c2 = (GByte) anGreen[iColor];
            sEntry.c3 = (GByte) anBlue[iColor];
            sEntry.c4 = 255;
            GDALSetColorEntry( hColorTable, iColor, &sEntry );
        }
        goto end_and_cleanup;
    }

/* ==================================================================== */
/*      STEP 3: continually subdivide boxes until no more free          */
/*      boxes remain or until all colors assigned.                      */
/* ==================================================================== */
    while (freeboxes != NULL) {
        ptr = largest_box(usedboxes);
        if (ptr != NULL)
            splitbox(ptr, histogram, psHashHistogram, nCLevels, &freeboxes, &usedboxes,
                     pabyRedBand, pabyGreenBand, pabyBlueBand, nPixels);
        else
            freeboxes = NULL;
    }

/* ==================================================================== */
/*      STEP 4: assign colors to all boxes                              */
/* ==================================================================== */
    for (i = 0, ptr = usedboxes; ptr != NULL; ++i, ptr = ptr->next) 
    {
        GDALColorEntry	sEntry;

        sEntry.c1 = (GByte) (((ptr->rmin + ptr->rmax) << nColorShift) / 2);
        sEntry.c2 = (GByte) (((ptr->gmin + ptr->gmax) << nColorShift) / 2);
        sEntry.c3 = (GByte) (((ptr->bmin + ptr->bmax) << nColorShift) / 2);
        sEntry.c4 = 255;
        GDALSetColorEntry( hColorTable, i, &sEntry );
    }

end_and_cleanup:
    CPLFree( pabyRedLine );
    CPLFree( pabyGreenLine );
    CPLFree( pabyBlueLine );

    /* We're done with the boxes now */
    CPLFree(box_list);
    freeboxes = usedboxes = NULL;

    if( panHistogram == NULL )
        CPLFree( histogram );
    
    return err;
}

/************************************************************************/
/*                            largest_box()                             */
/************************************************************************/

static Colorbox *
largest_box(Colorbox *usedboxes)
{
    Colorbox *p, *b;
    int size;

    b = NULL;
    size = -1;
    for (p = usedboxes; p != NULL; p = p->next)
        if ((p->rmax > p->rmin || p->gmax > p->gmin ||
             p->bmax > p->bmin) &&  p->total > size)
            size = (b = p)->total;
    return (b);
}

static void shrinkboxFromBand(Colorbox* ptr,
                              const GByte* pabyRedBand,
                              const GByte* pabyGreenBand,
                              const GByte* pabyBlueBand, int nPixels)
{
    int rmin_new = 255, rmax_new = 0,
        gmin_new = 255, gmax_new = 0,
        bmin_new = 255, bmax_new = 0;
    for(int i=0;i<nPixels;i++)
    {
        int iR = pabyRedBand[i];
        int iG = pabyGreenBand[i];
        int iB = pabyBlueBand[i];
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

    CPLAssert(rmin_new >= ptr->rmin && rmin_new <= rmax_new && rmax_new <= ptr->rmax);
    CPLAssert(gmin_new >= ptr->gmin && gmin_new <= gmax_new && gmax_new <= ptr->gmax);
    CPLAssert(bmin_new >= ptr->bmin && bmin_new <= bmax_new && bmax_new <= ptr->bmax);
    ptr->rmin = rmin_new;
    ptr->rmax = rmax_new;
    ptr->gmin = gmin_new;
    ptr->gmax = gmax_new;
    ptr->bmin = bmin_new;
    ptr->bmax = bmax_new;
}

static void shrinkboxFromHashHistogram(Colorbox* box,
                                       const HashHistogram* psHashHistogram)
{
    int ir, ig, ib;
    //int count_iter;

    if (box->rmax > box->rmin) {
        //count_iter = 0;
        for (ir = box->rmin; ir <= box->rmax; ++ir) {
            for (ig = box->gmin; ig <= box->gmax; ++ig) {
                for (ib = box->bmin; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0) {
                        //if( count_iter > 65536 ) printf("iter rmin=%d\n", count_iter);
                        box->rmin = ir;
                        goto have_rmin;
                    }
                }
            }
        }
    }
    have_rmin:
    if (box->rmax > box->rmin) {
        //count_iter = 0;
        for (ir = box->rmax; ir >= box->rmin; --ir) {
            for (ig = box->gmin; ig <= box->gmax; ++ig) {
                ib = box->bmin;
                for (; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0) {
                        //if( count_iter > 65536 ) printf("iter rmax=%d\n", count_iter);
                        box->rmax = ir;
                        goto have_rmax;
                    }
                }
            }
        }
    }

    have_rmax:
    if (box->gmax > box->gmin) {
        //count_iter = 0;
        for (ig = box->gmin; ig <= box->gmax; ++ig) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                for (ib = box->bmin; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0) {
                        //if( count_iter > 65536 ) printf("iter gmin=%d\n", count_iter);
                        box->gmin = ig;
                        goto have_gmin;
                    }
                }
            }
        }
    }

    have_gmin:
    if (box->gmax > box->gmin) {
        //count_iter = 0;
        for (ig = box->gmax; ig >= box->gmin; --ig) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                ib = box->bmin;
                for (; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0) {
                        //if( count_iter > 65536 ) printf("iter gmax=%d\n", count_iter);
                        box->gmax = ig;
                        goto have_gmax;
                    }
                }
            }
        }
    }

    have_gmax:
    if (box->bmax > box->bmin) {
        //count_iter = 0;
        for (ib = box->bmin; ib <= box->bmax; ++ib) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                for (ig = box->gmin; ig <= box->gmax; ++ig) {
                    //count_iter ++;
                    if (FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0) {
                        //if( count_iter > 65536 ) printf("iter bmin=%d\n", count_iter);
                        box->bmin = ib;
                        goto have_bmin;
                    }
                }
            }
        }
    }
    
    have_bmin:
    if (box->bmax > box->bmin) {
        //count_iter = 0;
        for (ib = box->bmax; ib >= box->bmin; --ib) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                ig = box->gmin;
                for (; ig <= box->gmax; ++ig) {
                    //count_iter ++;
                    if (FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib)) != 0) {
                        //if( count_iter > 65536 ) printf("iter bmax=%d\n", count_iter);
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
static void
splitbox(Colorbox* ptr, const int* histogram,
         const HashHistogram* psHashHistogram,
         int nCLevels,
         Colorbox **pfreeboxes, Colorbox **pusedboxes,
         GByte* pabyRedBand,
         GByte* pabyGreenBand,
         GByte* pabyBlueBand, int nPixels)
{
    int		hist2[256];
    int		first=0, last=0;
    Colorbox	*new_cb;
    const int	*iptr;
    int *histp;
    int	i, j;
    int	ir,ig,ib;
    int sum, sum1, sum2;
    enum { RED, GREEN, BLUE } axis;

    /*
     * See which axis is the largest, do a histogram along that
     * axis.  Split at median point.  Contract both new boxes to
     * fit points and return
     */
    i = ptr->rmax - ptr->rmin;
    if (i >= ptr->gmax - ptr->gmin  && i >= ptr->bmax - ptr->bmin)
        axis = RED;
    else if (ptr->gmax - ptr->gmin >= ptr->bmax - ptr->bmin)
        axis = GREEN;
    else
        axis = BLUE;
    /* get histogram along longest axis */
    int nIters = (ptr->rmax - ptr->rmin + 1) * (ptr->gmax - ptr->gmin + 1) *
                 (ptr->bmax - ptr->bmin + 1);
    //printf("nIters = %d\n", nIters);
    switch (axis) {
      case RED:
      {
        if( nPixels != 0 && nIters > nPixels )
        {
            memset(hist2, 0, sizeof(hist2));
            const int           rmin = ptr->rmin,
                                rmax = ptr->rmax,
                                gmin = ptr->gmin,
                                gmax = ptr->gmax,
                                bmin = ptr->bmin,
                                bmax = ptr->bmax;
            for(int i=0;i<nPixels;i++)
            {
                int iR = pabyRedBand[i];
                int iG = pabyGreenBand[i];
                int iB = pabyBlueBand[i];
                if( iR >= rmin && iR <= rmax &&
                    iG >= gmin && iG <= gmax &&
                    iB >= bmin && iB <= bmax )
                {
                    hist2[iR] ++;
                }
            }
        }
        else if( psHashHistogram )
        {
            histp = &hist2[ptr->rmin];
            for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                *histp = 0;
                for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
                    for (ib = ptr->bmin; ib <= ptr->bmax; ++ib)
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
            histp = &hist2[ptr->rmin];
            for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                *histp = 0;
                for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
                    iptr = &HISTOGRAM(histogram,nCLevels,ir,ig,ptr->bmin);
                    for (ib = ptr->bmin; ib <= ptr->bmax; ++ib)
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
            memset(hist2, 0, sizeof(hist2));
            const int           rmin = ptr->rmin,
                                rmax = ptr->rmax,
                                gmin = ptr->gmin,
                                gmax = ptr->gmax,
                                bmin = ptr->bmin,
                                bmax = ptr->bmax;
            for(int i=0;i<nPixels;i++)
            {
                int iR = pabyRedBand[i];
                int iG = pabyGreenBand[i];
                int iB = pabyBlueBand[i];
                if( iR >= rmin && iR <= rmax &&
                    iG >= gmin && iG <= gmax &&
                    iB >= bmin && iB <= bmax )
                {
                    hist2[iG] ++;
                }
            }
        }
        else if( psHashHistogram )
        {
            histp = &hist2[ptr->gmin];
            for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
                *histp = 0;
                for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                    for (ib = ptr->bmin; ib <= ptr->bmax; ++ib)
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
            histp = &hist2[ptr->gmin];
            for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
                *histp = 0;
                for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                    iptr = &HISTOGRAM(histogram,nCLevels,ir,ig,ptr->bmin);
                    for (ib = ptr->bmin; ib <= ptr->bmax; ++ib)
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
            memset(hist2, 0, sizeof(hist2));
            const int           rmin = ptr->rmin,
                                rmax = ptr->rmax,
                                gmin = ptr->gmin,
                                gmax = ptr->gmax,
                                bmin = ptr->bmin,
                                bmax = ptr->bmax;
            for(int i=0;i<nPixels;i++)
            {
                int iR = pabyRedBand[i];
                int iG = pabyGreenBand[i];
                int iB = pabyBlueBand[i];
                if( iR >= rmin && iR <= rmax &&
                    iG >= gmin && iG <= gmax &&
                    iB >= bmin && iB <= bmax )
                {
                    hist2[iB] ++;
                }
            }
        }
        else if( psHashHistogram )
        {
            histp = &hist2[ptr->bmin];
            for (ib = ptr->bmin; ib <= ptr->bmax; ++ib) {
                *histp = 0;
                for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                    for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
                        *histp += FindColorCount(psHashHistogram,
                                       MAKE_COLOR_CODE(ir, ig, ib));
                    }
                }
                histp++;
            }
        }
        else
        {
            histp = &hist2[ptr->bmin];
            for (ib = ptr->bmin; ib <= ptr->bmax; ++ib) {
                *histp = 0;
                for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                    iptr = &HISTOGRAM(histogram,nCLevels,ir,ptr->gmin,ib);
                    for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
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
    /* find median point */
    sum2 = ptr->total / 2;
    histp = &hist2[first];
    sum = 0;
    for (i = first; i <= last && (sum += *histp++) < sum2; ++i)
        ;
    if (i == first)
        i++;

    /* Create new box, re-allocate points */
    new_cb = *pfreeboxes;
    *pfreeboxes = new_cb->next;
    if (*pfreeboxes)
        (*pfreeboxes)->prev = NULL;
    if (*pusedboxes)
        (*pusedboxes)->prev = new_cb;
    new_cb->next = *pusedboxes;
    *pusedboxes = new_cb;

    histp = &hist2[first];
    for (sum1 = 0, j = first; j < i; j++)
        sum1 += *histp++;
    for (sum2 = 0, j = i; j <= last; j++)
        sum2 += *histp++;
    new_cb->total = sum1;
    ptr->total = sum2;

    new_cb->rmin = ptr->rmin;
    new_cb->rmax = ptr->rmax;
    new_cb->gmin = ptr->gmin;
    new_cb->gmax = ptr->gmax;
    new_cb->bmin = ptr->bmin;
    new_cb->bmax = ptr->bmax;
    switch (axis) {
      case RED:
        new_cb->rmax = i-1;
        ptr->rmin = i;
        break;
      case GREEN:
        new_cb->gmax = i-1;
        ptr->gmin = i;
        break;
      case BLUE:
        new_cb->bmax = i-1;
        ptr->bmin = i;
        break;
    }
    if( nPixels != 0 &&
        (new_cb->rmax - new_cb->rmin + 1) * (new_cb->gmax - new_cb->gmin + 1) *
        (new_cb->bmax - new_cb->bmin + 1) > nPixels )
    {
        shrinkboxFromBand(new_cb, pabyRedBand, pabyGreenBand, pabyBlueBand, nPixels);
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
        (ptr->rmax - ptr->rmin + 1) * (ptr->gmax - ptr->gmin + 1) *
        (ptr->bmax - ptr->bmin + 1) > nPixels )
    {
        shrinkboxFromBand(ptr, pabyRedBand, pabyGreenBand, pabyBlueBand, nPixels);
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
static void
shrinkbox(Colorbox* box, const int* histogram, int nCLevels)
{
    const int *histp;
    int ir, ig, ib;
    //int count_iter;

    if (box->rmax > box->rmin) {
        //count_iter = 0;
        for (ir = box->rmin; ir <= box->rmax; ++ir) {
            for (ig = box->gmin; ig <= box->gmax; ++ig) {
                histp = &HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                for (ib = box->bmin; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (*histp++ != 0) {
                        //if( count_iter > 65536 ) printf("iter rmin=%d\n", count_iter);
                        box->rmin = ir;
                        goto have_rmin;
                    }
                }
            }
        }
    }
    have_rmin:
    if (box->rmax > box->rmin) {
        //count_iter = 0;
        for (ir = box->rmax; ir >= box->rmin; --ir) {
            for (ig = box->gmin; ig <= box->gmax; ++ig) {
                histp = &HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                ib = box->bmin;
                for (; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (*histp++ != 0) {
                        //if( count_iter > 65536 ) printf("iter rmax=%d\n", count_iter);
                        box->rmax = ir;
                        goto have_rmax;
                    }
                }
            }
        }
    }

    have_rmax:
    if (box->gmax > box->gmin) {
        //count_iter = 0;
        for (ig = box->gmin; ig <= box->gmax; ++ig) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                histp = &HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                for (ib = box->bmin; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (*histp++ != 0) {
                        //if( count_iter > 65536 ) printf("iter gmin=%d\n", count_iter);
                        box->gmin = ig;
                        goto have_gmin;
                    }
                }
            }
        }
    }

    have_gmin:
    if (box->gmax > box->gmin) {
        //count_iter = 0;
        for (ig = box->gmax; ig >= box->gmin; --ig) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                histp = &HISTOGRAM(histogram, nCLevels, ir, ig, box->bmin);
                ib = box->bmin;
                for (; ib <= box->bmax; ++ib) {
                    //count_iter ++;
                    if (*histp++ != 0) {
                        //if( count_iter > 65536 ) printf("iter gmax=%d\n", count_iter);
                        box->gmax = ig;
                        goto have_gmax;
                    }
                }
            }
        }
    }

    have_gmax:
    if (box->bmax > box->bmin) {
        //count_iter = 0;
        for (ib = box->bmin; ib <= box->bmax; ++ib) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                histp = &HISTOGRAM(histogram, nCLevels, ir, box->gmin, ib);
                for (ig = box->gmin; ig <= box->gmax; ++ig) {
                    //count_iter ++;
                    if (*histp != 0) {
                        //if( count_iter > 65536 ) printf("iter bmin=%d\n", count_iter);
                        box->bmin = ib;
                        goto have_bmin;
                    }
                    histp += nCLevels;
                }
            }
        }
    }
    
    have_bmin:
    if (box->bmax > box->bmin) {
        //count_iter = 0;
        for (ib = box->bmax; ib >= box->bmin; --ib) {
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                histp = &HISTOGRAM(histogram, nCLevels, ir, box->gmin, ib);
                ig = box->gmin;
                for (; ig <= box->gmax; ++ig) {
                    //count_iter ++;
                    if (*histp != 0) {
                        //if( count_iter > 65536 ) printf("iter bmax=%d\n", count_iter);
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

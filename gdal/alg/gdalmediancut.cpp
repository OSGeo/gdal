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

CPL_CVSID("$Id$");

#define	MAX_CMAP_SIZE	256

#define	COLOR_DEPTH	8
#define	MAX_COLOR	256

#define	GMC_B_DEPTH		5		/* # bits/pixel to use */
#define	GMC_B_LEN		(1L<<GMC_B_DEPTH)

#define	C_DEPTH		2
#define	C_LEN		(1L<<C_DEPTH)	/* # cells/color to use */

#define	COLOR_SHIFT	(COLOR_DEPTH-GMC_B_DEPTH)

typedef	struct colorbox {
	struct	colorbox *next, *prev;
	int	rmin, rmax;
	int	gmin, gmax;
	int	bmin, bmax;
	int	total;
} Colorbox;

static int	num_colors;
static int	(*histogram)[GMC_B_LEN][GMC_B_LEN];
static Colorbox *freeboxes;
static Colorbox *usedboxes;

static	void splitbox(Colorbox*);
static	void shrinkbox(Colorbox*);
static	Colorbox* largest_box(void);

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

int CPL_STDCALL
GDALComputeMedianCutPCT( GDALRasterBandH hRed, 
                         GDALRasterBandH hGreen, 
                         GDALRasterBandH hBlue, 
                         int (*pfnIncludePixel)(int,int,void*),
                         int nColors, 
                         GDALColorTableH hColorTable,
                         GDALProgressFunc pfnProgress, 
                         void * pProgressArg )

{
    int		nXSize, nYSize;

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

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    histogram = (int (*)[GMC_B_LEN][GMC_B_LEN]) 
        CPLCalloc(GMC_B_LEN * GMC_B_LEN * GMC_B_LEN,sizeof(int));

/* ==================================================================== */
/*      STEP 1: crate empty boxes.                                      */
/* ==================================================================== */
    int	     i;
    Colorbox *box_list, *ptr;

    num_colors = nColors;
    usedboxes = NULL;
    box_list = freeboxes = (Colorbox *)CPLMalloc(num_colors*sizeof (Colorbox));
    freeboxes[0].next = &freeboxes[1];
    freeboxes[0].prev = NULL;
    for (i = 1; i < num_colors-1; ++i) {
        freeboxes[i].next = &freeboxes[i+1];
        freeboxes[i].prev = &freeboxes[i-1];
    }
    freeboxes[num_colors-1].next = NULL;
    freeboxes[num_colors-1].prev = &freeboxes[num_colors-2];

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

    memset( histogram, 0, sizeof(int) * GMC_B_LEN * GMC_B_LEN * GMC_B_LEN );

/* -------------------------------------------------------------------- */
/*      Collect histogram.                                              */
/* -------------------------------------------------------------------- */
    pabyRedLine = (GByte *) CPLMalloc(nXSize);
    pabyGreenLine = (GByte *) CPLMalloc(nXSize);
    pabyBlueLine = (GByte *) CPLMalloc(nXSize);

    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        if( !pfnProgress( iLine / (double) nYSize, 
                          "Generating Histogram", pProgressArg ) )
        {
            CPLFree( pabyRedLine );
            CPLFree( pabyGreenLine );
            CPLFree( pabyBlueLine );

            CPLError( CE_Failure, CPLE_UserInterrupt, "User Terminated" );
            return CE_Failure;
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
            
            nRed = pabyRedLine[iPixel] >> COLOR_SHIFT;
            nGreen = pabyGreenLine[iPixel] >> COLOR_SHIFT;
            nBlue = pabyBlueLine[iPixel] >> COLOR_SHIFT;

            ptr->rmin = MIN(ptr->rmin, nRed);
            ptr->gmin = MIN(ptr->gmin, nGreen);
            ptr->bmin = MIN(ptr->bmin, nBlue);
            ptr->rmax = MAX(ptr->rmax, nRed);
            ptr->gmax = MAX(ptr->gmax, nGreen);
            ptr->bmax = MAX(ptr->bmax, nBlue);

            histogram[nRed][nGreen][nBlue]++;
        }
    }

    CPLFree( pabyRedLine );
    CPLFree( pabyGreenLine );
    CPLFree( pabyBlueLine );

    if( !pfnProgress( 1.0, "Generating Histogram", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User Terminated" );
        return CE_Failure;
    }

/* ==================================================================== */
/*      STEP 3: continually subdivide boxes until no more free          */
/*      boxes remain or until all colors assigned.                      */
/* ==================================================================== */
    while (freeboxes != NULL) {
        ptr = largest_box();
        if (ptr != NULL)
            splitbox(ptr);
        else
            freeboxes = NULL;
    }

/* ==================================================================== */
/*      STEP 4: assign colors to all boxes                              */
/* ==================================================================== */
    for (i = 0, ptr = usedboxes; ptr != NULL; ++i, ptr = ptr->next) 
    {
        GDALColorEntry	sEntry;

        sEntry.c1 = (GByte) (((ptr->rmin + ptr->rmax) << COLOR_SHIFT) / 2);
        sEntry.c2 = (GByte) (((ptr->gmin + ptr->gmax) << COLOR_SHIFT) / 2);
        sEntry.c3 = (GByte) (((ptr->bmin + ptr->bmax) << COLOR_SHIFT) / 2);
        sEntry.c4 = 255;
        GDALSetColorEntry( hColorTable, i, &sEntry );
    }
    
    /* We're done with the boxes now */
    CPLFree(box_list);
    freeboxes = usedboxes = NULL;

    CPLFree( histogram );
    
    return CE_None;
}

/************************************************************************/
/*                            largest_box()                             */
/************************************************************************/

static Colorbox *
largest_box(void)
{
    register Colorbox *p, *b;
    register int size;

    b = NULL;
    size = -1;
    for (p = usedboxes; p != NULL; p = p->next)
        if ((p->rmax > p->rmin || p->gmax > p->gmin ||
             p->bmax > p->bmin) &&  p->total > size)
            size = (b = p)->total;
    return (b);
}

/************************************************************************/
/*                              splitbox()                              */
/************************************************************************/
static void
splitbox(Colorbox* ptr)
{
    int		hist2[GMC_B_LEN];
    int		first=0, last=0;
    register Colorbox	*new_cb;
    register int	*iptr, *histp;
    register int	i, j;
    register int	ir,ig,ib;
    register int sum, sum1, sum2;
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
    switch (axis) {
      case RED:
        histp = &hist2[ptr->rmin];
        for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
            *histp = 0;
            for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
                iptr = &histogram[ir][ig][ptr->bmin];
                for (ib = ptr->bmin; ib <= ptr->bmax; ++ib)
                    *histp += *iptr++;
            }
            histp++;
        }
        first = ptr->rmin;
        last = ptr->rmax;
        break;
      case GREEN:
        histp = &hist2[ptr->gmin];
        for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
            *histp = 0;
            for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                iptr = &histogram[ir][ig][ptr->bmin];
                for (ib = ptr->bmin; ib <= ptr->bmax; ++ib)
                    *histp += *iptr++;
            }
            histp++;
        }
        first = ptr->gmin;
        last = ptr->gmax;
        break;
      case BLUE:
        histp = &hist2[ptr->bmin];
        for (ib = ptr->bmin; ib <= ptr->bmax; ++ib) {
            *histp = 0;
            for (ir = ptr->rmin; ir <= ptr->rmax; ++ir) {
                iptr = &histogram[ir][ptr->gmin][ib];
                for (ig = ptr->gmin; ig <= ptr->gmax; ++ig) {
                    *histp += *iptr;
                    iptr += GMC_B_LEN;
                }
            }
            histp++;
        }
        first = ptr->bmin;
        last = ptr->bmax;
        break;
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
    new_cb = freeboxes;
    freeboxes = new_cb->next;
    if (freeboxes)
        freeboxes->prev = NULL;
    if (usedboxes)
        usedboxes->prev = new_cb;
    new_cb->next = usedboxes;
    usedboxes = new_cb;

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
    shrinkbox(new_cb);
    shrinkbox(ptr);
}

/************************************************************************/
/*                             shrinkbox()                              */
/************************************************************************/
static void
shrinkbox(Colorbox* box)
{
    register int *histp, ir, ig, ib;

    if (box->rmax > box->rmin) {
        for (ir = box->rmin; ir <= box->rmax; ++ir)
            for (ig = box->gmin; ig <= box->gmax; ++ig) {
                histp = &histogram[ir][ig][box->bmin];
                for (ib = box->bmin; ib <= box->bmax; ++ib)
                    if (*histp++ != 0) {
                        box->rmin = ir;
                        goto have_rmin;
                    }
            }
      have_rmin:
        if (box->rmax > box->rmin)
            for (ir = box->rmax; ir >= box->rmin; --ir)
                for (ig = box->gmin; ig <= box->gmax; ++ig) {
                    histp = &histogram[ir][ig][box->bmin];
                    ib = box->bmin;
                    for (; ib <= box->bmax; ++ib)
                        if (*histp++ != 0) {
                            box->rmax = ir;
                            goto have_rmax;
                        }
                }
    }
  have_rmax:
    if (box->gmax > box->gmin) {
        for (ig = box->gmin; ig <= box->gmax; ++ig)
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                histp = &histogram[ir][ig][box->bmin];
                for (ib = box->bmin; ib <= box->bmax; ++ib)
                    if (*histp++ != 0) {
                        box->gmin = ig;
                        goto have_gmin;
                    }
            }
      have_gmin:
        if (box->gmax > box->gmin)
            for (ig = box->gmax; ig >= box->gmin; --ig)
                for (ir = box->rmin; ir <= box->rmax; ++ir) {
                    histp = &histogram[ir][ig][box->bmin];
                    ib = box->bmin;
                    for (; ib <= box->bmax; ++ib)
                        if (*histp++ != 0) {
                            box->gmax = ig;
                            goto have_gmax;
                        }
                }
    }
  have_gmax:
    if (box->bmax > box->bmin) {
        for (ib = box->bmin; ib <= box->bmax; ++ib)
            for (ir = box->rmin; ir <= box->rmax; ++ir) {
                histp = &histogram[ir][box->gmin][ib];
                for (ig = box->gmin; ig <= box->gmax; ++ig) {
                    if (*histp != 0) {
                        box->bmin = ib;
                        goto have_bmin;
                    }
                    histp += GMC_B_LEN;
                }
            }
      have_bmin:
        if (box->bmax > box->bmin)
            for (ib = box->bmax; ib >= box->bmin; --ib)
                for (ir = box->rmin; ir <= box->rmax; ++ir) {
                    histp = &histogram[ir][box->gmin][ib];
                    ig = box->gmin;
                    for (; ig <= box->gmax; ++ig) {
                        if (*histp != 0) {
                            box->bmax = ib;
                            goto have_bmax;
                        }
                        histp += GMC_B_LEN;
                    }
                }
    }
  have_bmax:
    ;
}

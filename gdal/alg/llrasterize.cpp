/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Vector polygon rasterization code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
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
 * $Log$
 * Revision 1.1  2005/10/28 17:46:08  fwarmerdam
 * New
 *
 */

/*
 * NOTE: This code was originally adapted from the gdImageFilledPolygon() 
 * function in libgd.  
 * 
 * http://www.boutell.com/gd/
 *
 * It was later adapted for direct inclusion in GDAL and relicensed under
 * the GDAL MIT/X license (pulled from the OpenEV distribution). 
 */

#include "gdal_alg.h"

static int llCompareInt(const void *a, const void *b)
{
	return (*(const int *)a) - (*(const int *)b);
}

/************************************************************************/
/*                       dllImageFilledPolygon()                        */
/*                                                                      */
/*      Perform scanline conversion of the passed multi-ring            */
/*      polygon.  Note the polygon does not need to be explicitly       */
/*      closed.  The scanline function will be called with              */
/*      horizontal scanline chunks which may not be entirely            */
/*      contained within the valid raster area (in the X                */
/*      direction).                                                     */
/*                                                                      */
/*      NEW: Nodes' coordinate are kept as double  in order             */
/*      to compute accurately the intersections with the lines          */
/*                                                                      */
/*      Two methods for determining the border pixels:                  */
/*                                                                      */
/*      1) method = 0                                                   */
/*         Inherits algorithm from version above but with several bugs  */
/*         fixed except for the cone facing down.                       */
/*         A pixel on which a line intersects a segment of a            */
/*         polygon will always be considered as inside the shape.       */
/*         Note that we only compute intersections with lines that      */
/*         passes through the middle of a pixel (line coord = 0.5,      */
/*         1.5, 2.5, etc.)                                              */
/*                                                                      */
/*      2) method = 1:                                                  */
/*         A pixel is considered inside a polygon if its center         */
/*         falls inside the polygon. This is somehow more robust unless */
/*         the nodes are placed in the center of the pixels in which    */
/*         case, due to numerical inaccuracies, it's hard to predict    */
/*         if the pixel will be considered inside or outside the shape. */
/************************************************************************/

void GDALdllImageFilledPolygon(int nRasterXSize, int nRasterYSize, 
                               int nPartCount, int *panPartSize,
                               double *padfX, double *padfY,
                               llScanlineFunc pfnScanlineFunc, void *pCBData )
{
/*************************************************************************
2nd Method (method=1):
=====================
No known bug
*************************************************************************/

    int i;
    int y;
    int miny, maxy,minx,maxx;
    double dminy, dmaxy;
    double dx1, dy1;
    double dx2, dy2;
    double dy;
    double intersect;
    

    int ind1, ind2;
    int ints, n, part;
    int *polyInts, polyAllocated;

  
    int horizontal_x1, horizontal_x2;


    if (!nPartCount) {
        return;
    }

    n = 0;
    for( part = 0; part < nPartCount; part++ )
        n += panPartSize[part];
    
    polyInts = (int *) malloc(sizeof(int) * n);
    polyAllocated = n;
    
    dminy = padfY[0];
    dmaxy = padfY[0];
    for (i=1; (i < n); i++) {

        if (padfY[i] < dminy) {
            dminy = padfY[i];
        }
        if (padfY[i] > dmaxy) {
            dmaxy = padfY[i];
        }
    }
    miny = (int) dminy;
    maxy = (int) dmaxy;
    
    
    if( miny < 0 )
        miny = 0;
    if( maxy >= nRasterYSize )
        maxy = nRasterYSize-1;
   
    
    minx = 0;
    maxx = nRasterXSize - 1;

    /* Fix in 1.3: count a vertex only once */
    for (y=miny; y <= maxy; y++) {
        int	partoffset = 0;

        dy = y +0.5; /* center height of line*/
         

        part = 0;
        ints = 0;


        /*Initialize polyInts, otherwise it can sometimes causes a seg fault */
        for (i=0; (i < n); i++) {
            polyInts[i] = -1;
        }


        for (i=0; (i < n); i++) {
        
            
            if( i == partoffset + panPartSize[part] ) {
                partoffset += panPartSize[part];
                part++;
            }

            if( i == partoffset ) {
                ind1 = partoffset + panPartSize[part] - 1;
                ind2 = partoffset;
            } else {
                ind1 = i-1;
                ind2 = i;
            }
	    

            dy1 = padfY[ind1];
            dy2 = padfY[ind2];
            

            if( (dy1 < dy && dy2 < dy) || (dy1 > dy && dy2 > dy) )
                continue;

            if (dy1 < dy2) {
                dx1 = padfX[ind1];
                dx2 = padfX[ind2];
            } else if (dy1 > dy2) {
                dy2 = padfY[ind1];
                dy1 = padfY[ind2];
                dx2 = padfX[ind1];
                dx1 = padfX[ind2];
            } else /* if (fabs(dy1-dy2)< 1.e-6) */
	    {
                
                /*AE: DO NOT skip bottom horizontal segments 
		  -Fill them separately- 
		  They are not taken into account twice.*/
		if (padfX[ind1] > padfX[ind2])
		{
		    horizontal_x1 = (int) floor(padfX[ind2]+0.5);
		    horizontal_x2 = (int) floor(padfX[ind1]+0.5);
		
                    if  ( (horizontal_x1 >  maxx) ||  (horizontal_x2 < minx) )
                        continue;

		    /*fill the horizontal segment (separately from the rest)*/
		    pfnScanlineFunc( pCBData, y, horizontal_x1, horizontal_x2 - 1 );
		    continue;
		}
		else /*skip top horizontal segments (they are already filled in the regular loop)*/
		    continue;

	    }

            if(( dy < dy2 ) && (dy >= dy1))
            {
                
                intersect = (dy-dy1) * (dx2-dx1) / (dy2-dy1) + dx1;

		polyInts[ints++] = (int) floor(intersect+0.5);
	    }
	}

        /* 
         * It would be more efficient to do this inline, to avoid 
         * a function call for each comparison.
         */
        qsort(polyInts, ints, sizeof(int), llCompareInt);


        for (i=0; (i < (ints)); i+=2) {

            if( polyInts[i] <= maxx && polyInts[i+1] >= minx )
            {
                pfnScanlineFunc( pCBData, y, polyInts[i], polyInts[i+1] - 1 );
                
	    }
            
        }
    }

    free( polyInts );
}




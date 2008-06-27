/******************************************************************************
 * $Id: gdalchecksum.cpp 13893 2008-02-28 21:08:37Z rouault $
 *
 * Project:  GDAL
 * Purpose:  Compute each pixel's proximity to a set of target pixels.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
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

#include "gdal_alg.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id: gdalchecksum.cpp 13893 2008-02-28 21:08:37Z rouault $");

static CPLErr LoadTargetLine( GUInt32 *panTargetMask, int iLine,
                              GDALRasterBandH hSrcBand, 
                              int nTargetValues, int *panTargetValues );
static CPLErr 
LineProximitySearch( int nXSize, int nYSize, int iLine, int nMaxDist,
                     float *pafLastProximityLine,
                     float *pafThisProximityLine,
                     int iFirstTargetLine, int iFirstTargetLineLoc,
                     int nTargetMaskLines, int nTargetMaskLineWords,
                     GUInt32 *panTargetMaskBuffer );


/************************************************************************/
/*                        GDALComputeProximity()                        */
/************************************************************************/

/*

This function attempts to compute the proximity of all pixels in
the image to a set of pixels in the source image.  The following
options are used to define the behavior of the function.  By
default all non-zero pixels in hSrcBand will be considered the
"target", and all proximities will be computed in pixels.  Note
that target pixels are set to the value corresponding to a distance
of zero.

The progress function args may be NULL or a valid progress reporting function
such as GDALTermProgress/NULL. 

Options:

  VALUES=n[,n]*

A list of target pixel values to measure the distance from.  If this
option is not provided proximity will be computed from non-zero
pixel values.  Currently pixel values are internally processed as
integers.

  DISTUNITS=[PIXEL]/GEO

Indicates whether distances will be computed in pixel units or
in georeferenced units.  The default is pixel units.  This also 
determines the interpretation of MAXDIST.

  MAXDIST=n

The maximum distance to search.  Proximity distances greater than
this value will not be computed.  Instead output pixels will be
set to a nodata value. 

  NODATA=n

The NODATA value to use on the output band for pixels that are
beyond MAXDIST.  If not provided, the hProximityBand will be
queried for a nodata value.  If one is not found, 255 will be used.

  FIXED_BUF_VAL=n

If this option is set, all pixels within the MAXDIST threadhold are
set to this fixed value instead of to a proximity distance.  
*/


CPLErr CPL_STDCALL 
GDALComputeProximity( GDALRasterBandH hSrcBand, 
                      GDALRasterBandH hProximityBand,
                      char **papszOptions,
                      GDALProgressFunc pfnProgress, 
                      void * pProgressArg )

{
    int nMaxDist, nXSize, nYSize, i;
    const char *pszOpt;

    VALIDATE_POINTER1( hSrcBand, "GDALComputeProximity", CE_Failure );
    VALIDATE_POINTER1( hProximityBand, "GDALComputeProximity", CE_Failure );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      What is our maxdist value?                                      */
/* -------------------------------------------------------------------- */
    pszOpt = CSLFetchNameValue( papszOptions, "MAXDIST" );
    if( pszOpt )
        nMaxDist = atoi(pszOpt);
    else
        nMaxDist = GDALGetRasterXSize(hSrcBand) + GDALGetRasterYSize(hSrcBand);

/* -------------------------------------------------------------------- */
/*      Verify the source and destination are compatible.               */
/* -------------------------------------------------------------------- */
    nXSize = GDALGetRasterXSize( hSrcBand );
    nYSize = GDALGetRasterYSize( hSrcBand );
    if( nXSize != GDALGetRasterXSize( hProximityBand )
        || nYSize != GDALGetRasterYSize( hProximityBand ))
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Source and proximity bands are not the same size." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get the target value(s).                                        */
/* -------------------------------------------------------------------- */
    int *panTargetValues = NULL;
    int  nTargetValues = 0;
    
    pszOpt = CSLFetchNameValue( papszOptions, "VALUES" );
    if( pszOpt != NULL )
    {
        char **papszValuesTokens;

        papszValuesTokens = CSLTokenizeStringComplex( pszOpt, ",", FALSE,FALSE);
        
        nTargetValues = CSLCount(papszValuesTokens);
        panTargetValues = (int *) CPLCalloc(sizeof(int),nTargetValues);
        
        for( i = 0; i < nTargetValues; i++ )
            panTargetValues[i] = atoi(papszValuesTokens[i]);
        CSLDestroy( papszValuesTokens );
    }

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, "", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate buffer for two scanlines of distances as floats        */
/*      (the current and last line).                                    */
/* -------------------------------------------------------------------- */
    float *pafThisProximityLine;
    float *pafLastProximityLine;
    float *pafWriteBuffer;

    pafThisProximityLine = (float *) VSIMalloc(4 * nXSize);
    pafLastProximityLine = (float *) VSIMalloc(4 * nXSize);
    pafWriteBuffer = (float *) VSIMalloc(4 * nXSize);

    if( pafLastProximityLine == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory allocating %d byte buffer.", 
                  4 * nXSize );
        return CE_Failure;
    }

    for( i = 0; i < nXSize; i++ )
    {
        pafThisProximityLine[i] = -2.0;
        pafLastProximityLine[i] = -2.0;
    }

/* -------------------------------------------------------------------- */
/*      Allocate the target identification buffer.  We use one bit      */
/*      per pixel and allocate a number of lines determined by the      */
/*      maximum distance we need to search.                             */
/* -------------------------------------------------------------------- */
    GUInt32 *panTargetMaskBuffer;
    int      nTargetMaskLines;
    int      nTargetMaskLineWords;
    
    nTargetMaskLines = MIN(nMaxDist * 2 + 1,nYSize);
    nTargetMaskLineWords = (nXSize+31)/32;
    panTargetMaskBuffer = (GUInt32 *) 
        VSIMalloc2(4*nTargetMaskLineWords,nTargetMaskLines);
   
    if( panTargetMaskBuffer == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory allocating %g byte buffer.", 
                  nTargetMaskLineWords * (double) nTargetMaskLines );
        return CE_Failure;
    }

    memset( panTargetMaskBuffer, 0, 
            4 * nTargetMaskLineWords * nTargetMaskLines );

/* -------------------------------------------------------------------- */
/*      Prefill the target buffer.                                      */
/* -------------------------------------------------------------------- */
    int iLine;
    int iFirstTargetLine = 0;
    int iFirstTargetLineLoc = 0;
    CPLErr eErr;

    for( iLine = 0; iLine < nTargetMaskLines; iLine++ )
    {
        eErr = 
            LoadTargetLine( panTargetMaskBuffer + iLine * nTargetMaskLineWords, 
                            iLine, hSrcBand, nTargetValues, panTargetValues );
        if( eErr != CE_None )
        {
            CPLFree( panTargetMaskBuffer );
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop over the lines to process.                                 */
/* -------------------------------------------------------------------- */
    for( iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
    {
        // Switch this/last proximity buffers.
        {
            float *pafTemp = pafThisProximityLine;
            pafThisProximityLine = pafLastProximityLine;
            pafLastProximityLine = pafTemp;
        }

        // Do we need to load a new target line?
        if( (iFirstTargetLine + nTargetMaskLines) < nYSize
            && (iFirstTargetLine + nTargetMaskLines) < iLine + nMaxDist )
        {
            eErr = 
                LoadTargetLine( panTargetMaskBuffer 
                                + iFirstTargetLineLoc * nTargetMaskLineWords, 
                                iFirstTargetLine + nTargetMaskLines,
                                hSrcBand, nTargetValues, panTargetValues );

            iFirstTargetLine++;
            iFirstTargetLineLoc = (iFirstTargetLineLoc+1) % nTargetMaskLines;
        }

        // Perform proximity search for this scanline.
        eErr = 
            LineProximitySearch( nXSize, nYSize, iLine, nMaxDist,
                                 pafLastProximityLine, pafThisProximityLine,
                                 iFirstTargetLine, iFirstTargetLineLoc,
                                 nTargetMaskLines, nTargetMaskLineWords,
                                 panTargetMaskBuffer );
        if( eErr != CE_None )
            break;

        // We need to turn the -1.0's into >maxdist value.
        for( i = 0; i < nXSize; i++ )
        {
            pafWriteBuffer[i] = pafThisProximityLine[i];
            if( pafWriteBuffer[i] < 0.0 )
                pafWriteBuffer[i] = nMaxDist;
        }

        // Write out results.
        eErr = 
            GDALRasterIO( hProximityBand, GF_Write, 0, iLine, nXSize, 1, 
                          pafWriteBuffer, nXSize, 1, GDT_Float32, 0, 0 );

        
        if( eErr != CE_None )
            break;

        if( !pfnProgress( (iLine+1) / (double) nYSize, "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pafThisProximityLine );
    CPLFree( pafLastProximityLine );
    CPLFree( panTargetMaskBuffer );
    CPLFree( pafWriteBuffer );

    return eErr;
}

/************************************************************************/
/*                           LoadTargetLine()                           */
/************************************************************************/
static CPLErr LoadTargetLine( GUInt32 *panTargetMask, int iLine,
                              GDALRasterBandH hSrcBand, 
                              int nTargetValues, int *panTargetValues )

{
    int i;
    int nXSize = GDALGetRasterXSize( hSrcBand );
    GInt32 *panScanline;
    CPLErr eErr;

    memset( panTargetMask, 0, 4 * ((nXSize+31)/32) );
    panScanline = (GInt32 *) CPLMalloc(4 * nXSize);

    eErr = GDALRasterIO( hSrcBand, GF_Read, 0, iLine, nXSize, 1, 
                         panScanline, nXSize, 1, GDT_Int32, 0, 0 );
    for( i = 0; i < nXSize; i++ )
    {
        int bIsTarget;

        if( panTargetValues == NULL )
            bIsTarget = (panScanline[i] != 0);
        else
        {
            int j;
            bIsTarget = FALSE;
            for( j = 0; j < nTargetValues; j++ )
            {
                bIsTarget |= (panScanline[i] == panTargetValues[i]);
            }
        }

        if( bIsTarget )
            panTargetMask[i>>5] |= (0x1 << (i & 0x1f));
    }

    CPLFree( panScanline );

    return eErr;
}

#define MASK_LINE_INDEX(iReqLine) \
    ((iReqLine - iFirstTargetLine + iFirstTargetLineLoc) % nTargetMaskLines)

#define MASK_LINE(iReqLine) \
    (panTargetMaskBuffer + nTargetMaskLineWords * MASK_LINE_INDEX(iReqLine))

#define MASK_BIT(panMaskLine,iPixel) \
    (panMaskLine[iPixel>>5] & (0x1 << (iPixel & 0x1f)))

#define MASK_TEST(iPixel,iLine) MASK_BIT(MASK_LINE(iLine),iPixel)
  
/************************************************************************/
/*                        LineProximitySearch()                         */
/************************************************************************/

static CPLErr 
LineProximitySearch( int nXSize, int nYSize, int iLine, int nMaxDist,
                     float *pafLastProximityLine,
                     float *pafThisProximityLine,
                     int iFirstTargetLine, int iFirstTargetLineLoc,
                     int nTargetMaskLines, int nTargetMaskLineWords,
                     GUInt32 *panTargetMaskBuffer )
    
{
    int iPixel;
    GUInt32 *panThisTargetMaskLine = MASK_LINE(iLine);

    for( iPixel = 0; iPixel < nXSize; iPixel++ )
    {
        // Is this pixel a target pixel?  Distance zero.
        if( MASK_BIT(panThisTargetMaskLine,iPixel) )
        {
            pafThisProximityLine[iPixel] = 0.0;
            continue;
        }

        // Were the left and above pixels outside our last search region?
        // If so, we only need to check one new pixel in the bottom right 
        // corner of our search area. 
        if( iPixel > 0 
            && pafThisProximityLine[iPixel-1] == -1.0
            && iLine > 1 
            && pafLastProximityLine[iPixel] == -1.0 )
        {
            if( iPixel < nXSize - nMaxDist
                && iLine < nYSize - nMaxDist 
                && MASK_TEST(iPixel+nMaxDist,iLine+nMaxDist) )
            {
                pafThisProximityLine[iPixel] = 
                    sqrt( nMaxDist * nMaxDist * 2 );
            }
            else
                pafThisProximityLine[iPixel] = -1.0;
        }

        // Add case where we search the whole right column and bottom row. 
#ifdef optimization
#endif

        // We will do a full search, in concentric boxes out from our center 
        // pixel.  This is the most expensive case. 
        float fLeastDistSq = (nMaxDist*2) * (nMaxDist*2);
        int iLeastDist = nMaxDist*2;
        int iDistOut;

        for( iDistOut = 1; 
             iDistOut < nMaxDist && iDistOut < iLeastDist*1.42 /*sqrt(2)*/;
             iDistOut++ )
        {
            int nEdgeLen = iDistOut*2 + 1;
            int iBaseX, iBaseY, iOff;
            float fDistSq;

            iBaseX = iPixel - iDistOut;
            iBaseY = iLine - iDistOut;
            
            for( iOff = 0; iOff < nEdgeLen; iOff++ )
            {
                if( iBaseX+iOff >= 0 && iBaseX+iOff < nXSize )
                {
                    // top edge.
                    if( iBaseY >= 0 )
                    {
                        if( MASK_TEST(iBaseX+iOff,iBaseY) )
                        {
                            iLeastDist = MIN(iLeastDist,iDistOut);
                            fDistSq = (iOff - iDistOut) * (iOff - iDistOut) 
                                + iDistOut * iDistOut;
                            if( fDistSq < fLeastDistSq )
                                fLeastDistSq = fDistSq;
                        }
                    }
                    // bottom edge
                    if( (iBaseY + nEdgeLen - 1) < nYSize )
                    {
                        if( MASK_TEST(iBaseX+iOff,iBaseY+nEdgeLen-1) )
                        {
                            iLeastDist = MIN(iLeastDist,iDistOut);
                            fDistSq = (iOff - iDistOut) * (iOff - iDistOut) 
                                + iDistOut * iDistOut;
                            if( fDistSq < fLeastDistSq )
                                fLeastDistSq = fDistSq;
                        }
                    }
                }

                if( iBaseY+iOff >= 0 && iBaseY+iOff < nYSize )
                {
                    // left edge
                    if( iBaseX >= 0 )
                    {
                        if( MASK_TEST(iBaseX,iBaseY+iOff) )
                        {
                            iLeastDist = MIN(iLeastDist,iDistOut);
                            fDistSq = (iOff - iDistOut) * (iOff - iDistOut) 
                                + iDistOut * iDistOut;
                            if( fDistSq < fLeastDistSq )
                                fLeastDistSq = fDistSq;
                        }
                    }
                    // right edge.
                    if( (iBaseX + nEdgeLen - 1) < nXSize )
                    {
                        if( MASK_TEST(iBaseX+nEdgeLen-1,iBaseY+iOff) )
                        {
                            iLeastDist = MIN(iLeastDist,iDistOut);
                            fDistSq = (iOff - iDistOut) * (iOff - iDistOut) 
                                + iDistOut * iDistOut;
                            if( fDistSq < fLeastDistSq )
                                fLeastDistSq = fDistSq;
                        }
                    }
                }
            }
        } /* next ring (box) out in search */

        if( fLeastDistSq <= nMaxDist * nMaxDist )
            pafThisProximityLine[iPixel] = sqrt(fLeastDistSq);
        else if( iLeastDist < nMaxDist*2 )
            pafThisProximityLine[iPixel] = -2.0; // hit in box, but >maxdist
        else
            pafThisProximityLine[iPixel] = -1.0; // no hit in box.
    } /* next pixel on scanline */

    return CE_None;
}
                       

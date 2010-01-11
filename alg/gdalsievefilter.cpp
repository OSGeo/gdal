/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Raster to Polygon Converter
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

#include "gdal_alg_priv.h"
#include "cpl_conv.h"
#include <vector>

CPL_CVSID("$Id$");

#define GP_NODATA_MARKER -51502112

/*
 * General Plan
 *
 * 1) make a pass with the polygon enumerator to build up the 
 *    polygon map array.  Also accumulate polygon size information.
 *
 * 2) Identify the polygons that need to be merged.
 * 
 * 3) Make a pass with the polygon enumerator.  For each "to be merged" 
 *    polygon keep track of it's largest neighbour. 
 * 
 * 4) Fix up remappings that would go to polygons smaller than the seive
 *    size.  Ensure these in term map to the largest neighbour of the 
 *    "to be seieved" polygons. 
 * 
 * 5) Make another pass with the polygon enumerator. This time we remap
 *    the actual pixel values of all polygons to be merged.
 * 
 */

/************************************************************************/
/*                          GPMaskImageData()                           */
/*                                                                      */
/*      Mask out image pixels to a special nodata value if the mask     */
/*      band is zero.                                                   */
/************************************************************************/

static CPLErr 
GPMaskImageData( GDALRasterBandH hMaskBand, GByte *pabyMaskLine, int iY, int nXSize, 
                 GInt32 *panImageLine )

{
    CPLErr eErr;

    eErr = GDALRasterIO( hMaskBand, GF_Read, 0, iY, nXSize, 1, 
                         pabyMaskLine, nXSize, 1, GDT_Byte, 0, 0 );
    if( eErr == CE_None )
    {
        int i;
        for( i = 0; i < nXSize; i++ )
        {
            if( pabyMaskLine[i] == 0 )
                panImageLine[i] = GP_NODATA_MARKER;
        }
    }

    return eErr;
}

/************************************************************************/
/*                          CompareNeighbour()                          */
/*                                                                      */
/*      Compare two neighbouring polygons, and update eaches            */
/*      "biggest neighbour" if the other is larger than it's current    */
/*      largest neighbour.                                              */
/*                                                                      */
/*      Note that this should end up with each polygon knowing the      */
/*      id of it's largest neighbour.  No attempt is made to            */
/*      restrict things to small polygons that we will be merging,      */
/*      nor to exclude assigning "biggest neighbours" that are still    */
/*      smaller than our sieve threshold.                               */
/************************************************************************/

static inline void CompareNeighbour( int nPolyId1, int nPolyId2, 
                                     int *panPolyIdMap, 
                                     int *panPolyValue,
                                     std::vector<int> &anPolySizes,
                                     std::vector<int> &anBigNeighbour )

{
    // make sure we are working with the final merged polygon ids. 
    nPolyId1 = panPolyIdMap[nPolyId1];
    nPolyId2 = panPolyIdMap[nPolyId2];

    if( nPolyId1 == nPolyId2 )
        return;

    // nodata polygon do not need neighbours, and cannot be neighbours
    // to valid polygons. 
    if( panPolyValue[nPolyId1] == GP_NODATA_MARKER
        || panPolyValue[nPolyId2] == GP_NODATA_MARKER )
        return;

    if( anBigNeighbour[nPolyId1] == -1
        || anPolySizes[anBigNeighbour[nPolyId1]] < anPolySizes[nPolyId2] )
        anBigNeighbour[nPolyId1] = nPolyId2;

    if( anBigNeighbour[nPolyId2] == -1
        || anPolySizes[anBigNeighbour[nPolyId2]] < anPolySizes[nPolyId1] )
        anBigNeighbour[nPolyId2] = nPolyId1;
}

/************************************************************************/
/*                          GDALSieveFilter()                           */
/************************************************************************/

/** 
 * Removes small raster polygons. 
 *
 * The function removes raster polygons smaller than a provided
 * threshold size (in pixels) and replaces replaces them with the pixel value 
 * of the largest neighbour polygon.  
 *
 * Polygon are determined (per GDALRasterPolygonEnumerator) as regions of
 * the raster where the pixels all have the same value, and that are contiguous
 * (connected).  
 *
 * Pixels determined to be "nodata" per hMaskBand will not be treated as part
 * of a polygon regardless of their pixel values.  Nodata areas will never be
 * changed nor affect polygon sizes. 
 *
 * Polygons smaller than the threshold with no neighbours that are as large
 * as the threshold will not be altered.  Polygons surrounded by nodata areas
 * will therefore not be altered.  
 *
 * The algorithm makes three passes over the input file to enumerate the
 * polygons and collect limited information about them.  Memory use is 
 * proportional to the number of polygons (roughly 24 bytes per polygon), but
 * is not directly related to the size of the raster.  So very large raster
 * files can be processed effectively if there aren't too many polygons.  But
 * extremely noisy rasters with many one pixel polygons will end up being 
 * expensive (in memory) to process.
 * 
 * @param hSrcBand the source raster band to be processed.
 * @param hMaskBand an optional mask band.  All pixels in the mask band with a 
 * value other than zero will be considered suitable for inclusion in polygons.
 * @param hDstBand the output raster band.  It may be the same as hSrcBand
 * to update the source in place. 
 * @param nSizeThreshold raster polygons with sizes smaller than this will
 * be merged into their largest neighbour.
 * @param nConnectedness either 4 indicating that diagonal pixels are not
 * considered directly adjacent for polygon membership purposes or 8
 * indicating they are. 
 * @param papszOption algorithm options in name=value list form.  None currently
 * supported.
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr CPL_STDCALL
GDALSieveFilter( GDALRasterBandH hSrcBand, GDALRasterBandH hMaskBand,
                 GDALRasterBandH hDstBand,
                 int nSizeThreshold, int nConnectedness,
                 char **papszOptions,
                 GDALProgressFunc pfnProgress, 
                 void * pProgressArg )

{
    VALIDATE_POINTER1( hSrcBand, "GDALSieveFilter", CE_Failure );
    VALIDATE_POINTER1( hDstBand, "GDALSieveFilter", CE_Failure );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Allocate working buffers.                                       */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    int nXSize = GDALGetRasterBandXSize( hSrcBand );
    int nYSize = GDALGetRasterBandYSize( hSrcBand );
    GInt32 *panLastLineVal = (GInt32 *) VSIMalloc2(sizeof(GInt32), nXSize);
    GInt32 *panThisLineVal = (GInt32 *) VSIMalloc2(sizeof(GInt32), nXSize);
    GInt32 *panLastLineId =  (GInt32 *) VSIMalloc2(sizeof(GInt32), nXSize);
    GInt32 *panThisLineId =  (GInt32 *) VSIMalloc2(sizeof(GInt32), nXSize);
    GInt32 *panThisLineWriteVal = (GInt32 *) VSIMalloc2(sizeof(GInt32), nXSize);
    GByte *pabyMaskLine = (hMaskBand != NULL) ? (GByte *) VSIMalloc(nXSize) : NULL;
    if (panLastLineVal == NULL || panThisLineVal == NULL ||
        panLastLineId == NULL || panThisLineId == NULL ||
        panThisLineWriteVal == NULL ||
        (hMaskBand != NULL && pabyMaskLine == NULL))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Could not allocate enough memory for temporary buffers");
        CPLFree( panThisLineId );
        CPLFree( panLastLineId );
        CPLFree( panThisLineVal );
        CPLFree( panLastLineVal );
        CPLFree( panThisLineWriteVal );
        CPLFree( pabyMaskLine );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      The first pass over the raster is only used to build up the     */
/*      polygon id map so we will know in advance what polygons are     */
/*      what on the second pass.                                        */
/* -------------------------------------------------------------------- */
    int iY, iX, iPoly;
    GDALRasterPolygonEnumerator oFirstEnum( nConnectedness );
    std::vector<int> anPolySizes;

    for( iY = 0; eErr == CE_None && iY < nYSize; iY++ )
    {
        eErr = GDALRasterIO( 
            hSrcBand,
            GF_Read, 0, iY, nXSize, 1, 
            panThisLineVal, nXSize, 1, GDT_Int32, 0, 0 );
        
        if( eErr == CE_None && hMaskBand != NULL )
            eErr = GPMaskImageData( hMaskBand, pabyMaskLine, iY, nXSize, panThisLineVal );

        if( iY == 0 )
            oFirstEnum.ProcessLine( 
                NULL, panThisLineVal, NULL, panThisLineId, nXSize );
        else
            oFirstEnum.ProcessLine(
                panLastLineVal, panThisLineVal, 
                panLastLineId,  panThisLineId, 
                nXSize );

/* -------------------------------------------------------------------- */
/*      Accumulate polygon sizes.                                       */
/* -------------------------------------------------------------------- */
        if( oFirstEnum.nNextPolygonId > (int) anPolySizes.size() )
            anPolySizes.resize( oFirstEnum.nNextPolygonId );

        for( iX = 0; iX < nXSize; iX++ )
        {
            iPoly = panThisLineId[iX]; 

            CPLAssert( iPoly >= 0 );
            anPolySizes[iPoly] += 1;
        }

/* -------------------------------------------------------------------- */
/*      swap this/last lines.                                           */
/* -------------------------------------------------------------------- */
        GInt32 *panTmp = panLastLineVal;
        panLastLineVal = panThisLineVal;
        panThisLineVal = panTmp;

        panTmp = panThisLineId;
        panThisLineId = panLastLineId;
        panLastLineId = panTmp;

/* -------------------------------------------------------------------- */
/*      Report progress, and support interrupts.                        */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None 
            && !pfnProgress( 0.25 * ((iY+1) / (double) nYSize), 
                             "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Make a pass through the maps, ensuring every polygon id         */
/*      points to the final id it should use, not an intermediate       */
/*      value.                                                          */
/* -------------------------------------------------------------------- */
    oFirstEnum.CompleteMerges();

/* -------------------------------------------------------------------- */
/*      Push the sizes of merged polygon fragments into the the         */
/*      merged polygon id's count.                                      */
/* -------------------------------------------------------------------- */
    for( iPoly = 0; iPoly < oFirstEnum.nNextPolygonId; iPoly++ )
    {
        if( oFirstEnum.panPolyIdMap[iPoly] != iPoly )
        {
            anPolySizes[oFirstEnum.panPolyIdMap[iPoly]] += anPolySizes[iPoly];
            anPolySizes[iPoly] = 0;
        }
    }

/* -------------------------------------------------------------------- */
/*      We will use a new enumerator for the second pass primariliy     */
/*      so we can preserve the first pass map.                          */
/* -------------------------------------------------------------------- */
    GDALRasterPolygonEnumerator oSecondEnum( nConnectedness );

    std::vector<int> anBigNeighbour;
    anBigNeighbour.resize( anPolySizes.size() );

    for( iPoly = 0; iPoly < (int) anPolySizes.size(); iPoly++ )
        anBigNeighbour[iPoly] = -1;

/* ==================================================================== */
/*      Second pass ... identify the largest neighbour for each         */
/*      polygon.                                                        */
/* ==================================================================== */
    for( iY = 0; eErr == CE_None && iY < nYSize; iY++ )
    {
/* -------------------------------------------------------------------- */
/*      Read the image data.                                            */
/* -------------------------------------------------------------------- */
        eErr = GDALRasterIO( hSrcBand, GF_Read, 0, iY, nXSize, 1, 
                             panThisLineVal, nXSize, 1, GDT_Int32, 0, 0 );

        if( eErr == CE_None && hMaskBand != NULL )
            eErr = GPMaskImageData( hMaskBand, pabyMaskLine, iY, nXSize, panThisLineVal );

        if( eErr != CE_None )
            continue;

/* -------------------------------------------------------------------- */
/*      Determine what polygon the various pixels belong to (redoing    */
/*      the same thing done in the first pass above).                   */
/* -------------------------------------------------------------------- */
        if( iY == 0 )
            oSecondEnum.ProcessLine( 
                NULL, panThisLineVal, NULL, panThisLineId, nXSize );
        else
            oSecondEnum.ProcessLine(
                panLastLineVal, panThisLineVal, 
                panLastLineId,  panThisLineId, 
                nXSize );

/* -------------------------------------------------------------------- */
/*      Check our neighbours, and update our biggest neighbour map      */
/*      as appropriate.                                                 */
/* -------------------------------------------------------------------- */
        for( iX = 0; iX < nXSize; iX++ )
        {
            if( iY > 0 )
            {
                CompareNeighbour( panThisLineId[iX], 
                                  panLastLineId[iX],
                                  oFirstEnum.panPolyIdMap,
                                  oFirstEnum.panPolyValue,
                                  anPolySizes, anBigNeighbour );

                if( iX > 0 && nConnectedness == 8 )
                    CompareNeighbour( panThisLineId[iX], 
                                      panLastLineId[iX-1],
                                      oFirstEnum.panPolyIdMap,
                                      oFirstEnum.panPolyValue,
                                      anPolySizes, anBigNeighbour );
                    
                if( iX < nXSize-1 && nConnectedness == 8 )
                    CompareNeighbour( panThisLineId[iX], 
                                      panLastLineId[iX+1],
                                      oFirstEnum.panPolyIdMap,
                                      oFirstEnum.panPolyValue,
                                      anPolySizes, anBigNeighbour );
                    
            }
            
            if( iX > 0 )
                CompareNeighbour( panThisLineId[iX], 
                                  panThisLineId[iX-1],
                                  oFirstEnum.panPolyIdMap,
                                  oFirstEnum.panPolyValue,
                                  anPolySizes, anBigNeighbour );

            // We don't need to compare to next pixel or next line
            // since they will be compared to us.
        }                     

/* -------------------------------------------------------------------- */
/*      Swap pixel value, and polygon id lines to be ready for the      */
/*      next line.                                                      */
/* -------------------------------------------------------------------- */
        GInt32 *panTmp = panLastLineVal;
        panLastLineVal = panThisLineVal;
        panThisLineVal = panTmp;

        panTmp = panThisLineId;
        panThisLineId = panLastLineId;
        panLastLineId = panTmp;

/* -------------------------------------------------------------------- */
/*      Report progress, and support interrupts.                        */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None 
            && !pfnProgress( 0.25 + 0.25 * ((iY+1) / (double) nYSize), 
                             "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      If our biggest neighbour is still smaller than the              */
/*      threshold, then try tracking to that polygons biggest           */
/*      neighbour, and so forth.                                        */
/* -------------------------------------------------------------------- */
    int nFailedMerges = 0;
    int nIsolatedSmall = 0;
    int nSieveTargets = 0;

    for( iPoly = 0; iPoly < (int) anPolySizes.size(); iPoly++ )
    {
        if( oFirstEnum.panPolyIdMap[iPoly] != iPoly )
            continue;

        // Ignore nodata polygons. 
        if( oFirstEnum.panPolyValue[iPoly] == GP_NODATA_MARKER )
            continue;

        // Don't try to merge polygons larger than the threshold.
        if( anPolySizes[iPoly] >= nSizeThreshold )
        {
            anBigNeighbour[iPoly] = -1;
            continue;
        }

        nSieveTargets++;

        // if we have no neighbours but we are small, what shall we do?
        if( anBigNeighbour[iPoly] == -1 )
        {
            nIsolatedSmall++;
            continue;
        }

        // If our biggest neighbour is larger than the threshold
        // then we are golden. 
        if( anPolySizes[anBigNeighbour[iPoly]] >= nSizeThreshold )
            continue;

#ifdef notdef
        // Will our neighbours biggest neighbour do?  
        // Eventually we need something sort of recursive here with
        // loop detection.
        if( anPolySizes[anBigNeighbour[anBigNeighbour[iPoly]]] 
            >= nSizeThreshold )
        {
            anBigNeighbour[iPoly] = anBigNeighbour[anBigNeighbour[iPoly]];
            continue;
        }
#endif

        nFailedMerges++;
        anBigNeighbour[iPoly] = -1;
    }									

    CPLDebug( "GDALSieveFilter", 
              "Small Polygons: %d, Isolated: %d, Unmergable: %d",
              nSieveTargets, nIsolatedSmall, nFailedMerges );

/* ==================================================================== */
/*      Make a third pass over the image, actually applying the         */
/*      merges.  We reuse the second enumerator but preserve the        */
/*      "final maps" from the first.                                    */
/* ==================================================================== */
    oSecondEnum.Clear();
    

    for( iY = 0; eErr == CE_None && iY < nYSize; iY++ )
    {
/* -------------------------------------------------------------------- */
/*      Read the image data.                                            */
/* -------------------------------------------------------------------- */
        eErr = GDALRasterIO( hSrcBand, GF_Read, 0, iY, nXSize, 1, 
                             panThisLineVal, nXSize, 1, GDT_Int32, 0, 0 );

        memcpy( panThisLineWriteVal, panThisLineVal, 4 * nXSize );

        if( eErr == CE_None && hMaskBand != NULL )
            eErr = GPMaskImageData( hMaskBand, pabyMaskLine, iY, nXSize, panThisLineVal );

        if( eErr != CE_None )
            continue;

/* -------------------------------------------------------------------- */
/*      Determine what polygon the various pixels belong to (redoing    */
/*      the same thing done in the first pass above).                   */
/* -------------------------------------------------------------------- */
        if( iY == 0 )
            oSecondEnum.ProcessLine( 
                NULL, panThisLineVal, NULL, panThisLineId, nXSize );
        else
            oSecondEnum.ProcessLine(
                panLastLineVal, panThisLineVal, 
                panLastLineId,  panThisLineId, 
                nXSize );

/* -------------------------------------------------------------------- */
/*      Reprocess the actual pixel values according to the polygon      */
/*      merging, and write out this line of image data.                 */
/* -------------------------------------------------------------------- */
        for( iX = 0; iX < nXSize; iX++ )
        {
            int iThisPoly = oFirstEnum.panPolyIdMap[panThisLineId[iX]];

            if( anBigNeighbour[iThisPoly] != -1 )
            {
                panThisLineWriteVal[iX] = 
                    oFirstEnum.panPolyValue[
                        anBigNeighbour[iThisPoly]];
            }
        }

/* -------------------------------------------------------------------- */
/*      Write the update data out.                                      */
/* -------------------------------------------------------------------- */
        eErr = GDALRasterIO( hDstBand, GF_Write, 0, iY, nXSize, 1, 
                             panThisLineWriteVal, nXSize, 1, GDT_Int32, 0, 0 );

/* -------------------------------------------------------------------- */
/*      Swap pixel value, and polygon id lines to be ready for the      */
/*      next line.                                                      */
/* -------------------------------------------------------------------- */
        GInt32 *panTmp = panLastLineVal;
        panLastLineVal = panThisLineVal;
        panThisLineVal = panTmp;

        panTmp = panThisLineId;
        panThisLineId = panLastLineId;
        panLastLineId = panTmp;

/* -------------------------------------------------------------------- */
/*      Report progress, and support interrupts.                        */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None 
            && !pfnProgress( 0.5 + 0.5 * ((iY+1) / (double) nYSize), 
                             "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( panThisLineId );
    CPLFree( panLastLineId );
    CPLFree( panThisLineVal );
    CPLFree( panLastLineVal );
    CPLFree( panThisLineWriteVal );
    CPLFree( pabyMaskLine );

    return eErr;
}


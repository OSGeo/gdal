/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Interpolate in nodata areas.
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GDALFilterLine()                           */
/*                                                                      */
/*      Apply 3x3 filtering one one scanline with masking for which     */
/*      pixels are to be interpolated (ThisFMask) and which window      */
/*      pixels are valid to include in the interpolation (TMask).       */
/************************************************************************/

static void
GDALFilterLine( float *pafLastLine, float *pafThisLine, float *pafNextLine,
                float *pafOutLine,
                GByte *pabyLastTMask, GByte *pabyThisTMask, GByte*pabyNextTMask,
                GByte *pabyThisFMask, int nXSize )

{
    int iX;

    for( iX = 0; iX < nXSize; iX++ )
    {
        if( !pabyThisFMask[iX] )
        {
            pafOutLine[iX] = pafThisLine[iX];
            continue;
        }

        CPLAssert( pabyThisTMask[iX] );

        double dfValSum = 0.0;
        double dfWeightSum = 0.0;

        // Previous line
        if( pafLastLine != NULL )
        {
            if( iX > 0 && pabyLastTMask[iX-1] )
            {
                dfValSum += pafLastLine[iX-1];
                dfWeightSum += 1.0;
            }
            if( pabyLastTMask[iX] )
            {
                dfValSum += pafLastLine[iX];
                dfWeightSum += 1.0;
            }
            if( iX < nXSize-1 && pabyLastTMask[iX+1] )
            {
                dfValSum += pafLastLine[iX+1];
                dfWeightSum += 1.0;
            }
        }

        // Current Line
        if( iX > 0 && pabyThisTMask[iX-1] )
        {
            dfValSum += pafThisLine[iX-1];
            dfWeightSum += 1.0;
        }
        if( pabyThisTMask[iX] )
        {
            dfValSum += pafThisLine[iX];
            dfWeightSum += 1.0;
        }
        if( iX < nXSize-1 && pabyThisTMask[iX+1] )
        {
            dfValSum += pafThisLine[iX+1];
            dfWeightSum += 1.0;
        }

        // Next line
        if( pafNextLine != NULL )
        {
            if( iX > 0 && pabyNextTMask[iX-1] )
            {
                dfValSum += pafNextLine[iX-1];
                dfWeightSum += 1.0;
            }
            if( pabyNextTMask[iX] )
            {
                dfValSum += pafNextLine[iX];
                dfWeightSum += 1.0;
            }
            if( iX < nXSize-1 && pabyNextTMask[iX+1] )
            {
                dfValSum += pafNextLine[iX+1];
                dfWeightSum += 1.0;
            }
        }

        pafOutLine[iX] = (float) (dfValSum / dfWeightSum);
    }
}

/************************************************************************/
/*                          GDALMultiFilter()                           */
/*                                                                      */
/*      Apply multiple iterations of a 3x3 smoothing filter over a      */
/*      band with masking controlling what pixels should be             */
/*      filtered (FiltMaskBand non zero) and which pixels can be        */
/*      considered valid contributors to the filter                     */
/*      (TargetMaskBand non zero).                                      */
/*                                                                      */
/*      This implementation attempts to apply many iterations in        */
/*      one IO pass by managing the filtering over a rolling buffer     */
/*      of nIternations+2 scanlines.  While possibly clever this        */
/*      makes the algorithm implementation largely                      */
/*      incomprehensible.                                               */
/************************************************************************/

static CPLErr
GDALMultiFilter( GDALRasterBandH hTargetBand, 
                 GDALRasterBandH hTargetMaskBand, 
                 GDALRasterBandH hFiltMaskBand,
                 int nIterations,
                 GDALProgressFunc pfnProgress, 
                 void * pProgressArg )

{
    float *paf3PassLineBuf;
    GByte *pabyTMaskBuf;
    GByte *pabyFMaskBuf;
    float *pafThisPass, *pafLastPass, *pafSLastPass;

    int   nBufLines = nIterations + 2;
    int   iPassCounter = 0;
    int   nNewLine; // the line being loaded this time (zero based scanline)
    int   nXSize = GDALGetRasterBandXSize( hTargetBand );
    int   nYSize = GDALGetRasterBandYSize( hTargetBand );
    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Report starting progress value.                                 */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, "Smoothing Filter...", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate rotating buffers.                                      */
/* -------------------------------------------------------------------- */
    pabyTMaskBuf = (GByte *) VSIMalloc2(nXSize, nBufLines);
    pabyFMaskBuf = (GByte *) VSIMalloc2(nXSize, nBufLines);

    paf3PassLineBuf = (float *) VSIMalloc3(nXSize, nBufLines, 3 * sizeof(float));
    if (pabyTMaskBuf == NULL || pabyFMaskBuf == NULL || paf3PassLineBuf == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Could not allocate enough memory for temporary buffers");
        eErr = CE_Failure;
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Process rotating buffers.                                       */
/* -------------------------------------------------------------------- */
    for( nNewLine = 0; 
         eErr == CE_None && nNewLine < nYSize+nIterations; 
         nNewLine++ )
    {
/* -------------------------------------------------------------------- */
/*      Rotate pass buffers.                                            */
/* -------------------------------------------------------------------- */
        iPassCounter = (iPassCounter + 1) % 3;

        pafSLastPass = paf3PassLineBuf 
            + ((iPassCounter+0)%3) * nXSize*nBufLines;
        pafLastPass = paf3PassLineBuf 
            + ((iPassCounter+1)%3) * nXSize*nBufLines;
        pafThisPass = paf3PassLineBuf 
            + ((iPassCounter+2)%3) * nXSize*nBufLines;

/* -------------------------------------------------------------------- */
/*      Where does the new line go in the rotating buffer?              */
/* -------------------------------------------------------------------- */
        int iBufOffset = nNewLine % nBufLines;

/* -------------------------------------------------------------------- */
/*      Read the new data line if it is't off the bottom of the         */
/*      image.                                                          */
/* -------------------------------------------------------------------- */
        if( nNewLine < nYSize )
        {
            eErr = 
                GDALRasterIO( hTargetMaskBand, GF_Read, 
                              0, nNewLine, nXSize, 1, 
                              pabyTMaskBuf + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Byte, 0, 0 );
            
            if( eErr != CE_None )
                break;

            eErr = 
                GDALRasterIO( hFiltMaskBand, GF_Read, 
                              0, nNewLine, nXSize, 1, 
                              pabyFMaskBuf + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Byte, 0, 0 );
            
            if( eErr != CE_None )
                break;

            eErr = 
                GDALRasterIO( hTargetBand, GF_Read, 
                              0, nNewLine, nXSize, 1, 
                              pafThisPass + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Float32, 0, 0 );
            
            if( eErr != CE_None )
                break;
        }

/* -------------------------------------------------------------------- */
/*      Loop over the loaded data, applying the filter to all loaded    */
/*      lines with neighbours.                                          */
/* -------------------------------------------------------------------- */
        int iFLine;

        for( iFLine = nNewLine-1;
             eErr == CE_None && iFLine >= nNewLine-nIterations;
             iFLine-- )
        {
            int iLastOffset, iThisOffset, iNextOffset;

            iLastOffset = (iFLine-1) % nBufLines; 
            iThisOffset = (iFLine  ) % nBufLines;
            iNextOffset = (iFLine+1) % nBufLines;

            // default to preserving the old value.
            if( iFLine >= 0 )
                memcpy( pafThisPass + iThisOffset * nXSize, 
                        pafLastPass + iThisOffset * nXSize, 
                        sizeof(float) * nXSize );

            // currently this skips the first and last line.  Eventually 
            // we will enable these too.  TODO
            if( iFLine < 1 || iFLine >= nYSize-1 )
            {
                continue;
            }

            GDALFilterLine( 
                pafSLastPass + iLastOffset * nXSize,
                pafLastPass  + iThisOffset * nXSize, 
                pafThisPass  + iNextOffset * nXSize, 
                pafThisPass  + iThisOffset * nXSize,
                pabyTMaskBuf + iLastOffset * nXSize,
                pabyTMaskBuf + iThisOffset * nXSize,
                pabyTMaskBuf + iNextOffset * nXSize,
                pabyFMaskBuf + iThisOffset * nXSize, 
                nXSize );
        }

/* -------------------------------------------------------------------- */
/*      Write out the top data line that will be rolling out of our     */
/*      buffer.                                                         */
/* -------------------------------------------------------------------- */
        int iLineToSave = nNewLine - nIterations;

        if( iLineToSave >= 0 && eErr == CE_None )
        {
            iBufOffset = iLineToSave % nBufLines;

            eErr = 
                GDALRasterIO( hTargetBand, GF_Write, 
                              0, iLineToSave, nXSize, 1, 
                              pafThisPass + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Float32, 0, 0 );
        }

/* -------------------------------------------------------------------- */
/*      Report progress.                                                */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None
            && !pfnProgress( (nNewLine+1) / (double) (nYSize+nIterations), 
                             "Smoothing Filter...", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
end:
    CPLFree( pabyTMaskBuf );
    CPLFree( pabyFMaskBuf );
    CPLFree( paf3PassLineBuf );

    return eErr;
}
 
/************************************************************************/
/*                             QUAD_CHECK()                             */
/*                                                                      */
/*      macro for checking whether a point is nearer than the           */
/*      existing closest point.                                         */
/************************************************************************/
#define QUAD_CHECK(quad_dist, quad_value, 				\
target_x, target_y, origin_x, origin_y, target_value )			\
									\
if( quad_value != nNoDataVal ) 						\
{									\
    double dfDistSq = ((target_x-origin_x) * (target_x-origin_x))       \
                    + ((target_y-origin_y) * (target_y-origin_y));      \
    									\
    if( dfDistSq < quad_dist*quad_dist )				\
    {									\
	CPLAssert( dfDistSq > 0.0 );                                    \
        quad_dist = sqrt(dfDistSq); 					\
        quad_value = target_value;					\
    }									\
}

/************************************************************************/
/*                           GDALFillNodata()                           */
/************************************************************************/

/**
 * Fill selected raster regions by interpolation from the edges.
 *
 * This algorithm will interpolate values for all designated 
 * nodata pixels (marked by zeros in hMaskBand).  For each pixel
 * a four direction conic search is done to find values to interpolate
 * from (using inverse distance weighting).  Once all values are
 * interpolated, zero or more smoothing iterations (3x3 average
 * filters on interpolated pixels) are applied to smooth out 
 * artifacts. 
 *
 * This algorithm is generally suitable for interpolating missing
 * regions of fairly continuously varying rasters (such as elevation
 * models for instance).  It is also suitable for filling small holes
 * and cracks in more irregularly varying images (like airphotos).  It
 * is generally not so great for interpolating a raster from sparse 
 * point data - see the algorithms defined in gdal_grid.h for that case.
 *
 * @param hTargetBand the raster band to be modified in place. 
 * @param hMaskBand a mask band indicating pixels to be interpolated (zero valued
 * @param dfMaxSearchDist the maximum number of pixels to search in all 
 * directions to find values to interpolate from.
 * @param bDeprecatedOption unused argument, should be zero.
 * @param nSmoothingIterations the number of 3x3 smoothing filter passes to 
 * run (0 or more).
 * @param papszOptions additional name=value options in a string list (none 
 * supported at this time - just pass NULL).
 * @param pfnProgress the progress function to report completion.
 * @param pProgressArg callback data for progress function.
 * 
 * @return CE_None on success or CE_Failure if something goes wrong. 
 */

CPLErr CPL_STDCALL
GDALFillNodata( GDALRasterBandH hTargetBand, 
                GDALRasterBandH hMaskBand,
                double dfMaxSearchDist, 
                int bDeprecatedOption,
                int nSmoothingIterations,
                char **papszOptions,
                GDALProgressFunc pfnProgress, 
                void * pProgressArg )

{
    VALIDATE_POINTER1( hTargetBand, "GDALFillNodata", CE_Failure );

    int nXSize = GDALGetRasterBandXSize( hTargetBand );
    int nYSize = GDALGetRasterBandYSize( hTargetBand );
    CPLErr eErr = CE_None;

    // Special "x" pixel values identifying pixels as special.
    GUInt32 nNoDataVal;
    GDALDataType eType;

    if( dfMaxSearchDist == 0.0 )
        dfMaxSearchDist = MAX(nXSize,nYSize) + 1;

    int nMaxSearchDist = (int) floor(dfMaxSearchDist);

    if( nXSize > 65533 || nYSize > 65533 )
    {
        eType = GDT_UInt32;
        nNoDataVal = 4000002;
    }
    else
    {
        eType = GDT_UInt16;
        nNoDataVal = 65535;
    }

    if( hMaskBand == NULL )
        hMaskBand = GDALGetMaskBand( hTargetBand );

    /* If there are smoothing iterations, reserve 10% of the progress for them */
    double dfProgressRatio = (nSmoothingIterations > 0) ? 0.9 : 1.0;

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( !pfnProgress( 0.0, "Filling...", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Create a work file to hold the Y "last value" indices.          */
/* -------------------------------------------------------------------- */
    GDALDriverH  hDriver = GDALGetDriverByName( "GTiff" );
    if (hDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALFillNodata needs GTiff driver");
        return CE_Failure;
    }
    
    GDALDatasetH hYDS;
    GDALRasterBandH hYBand;
    static const char *apszOptions[] = { "COMPRESS=LZW", "BIGTIFF=IF_SAFER", 
                                         NULL };
    CPLString osTmpFile = CPLGenerateTempFilename("");
    CPLString osYTmpFile = osTmpFile + "fill_y_work.tif";
    
    hYDS = GDALCreate( hDriver, osYTmpFile, nXSize, nYSize, 1, 
                       eType, (char **) apszOptions );
    
    if( hYDS == NULL )
        return CE_Failure;

    hYBand = GDALGetRasterBand( hYDS, 1 );

/* -------------------------------------------------------------------- */
/*      Create a work file to hold the pixel value associated with      */
/*      the "last xy value" pixel.                                      */
/* -------------------------------------------------------------------- */
    GDALDatasetH hValDS;
    GDALRasterBandH hValBand;
    CPLString osValTmpFile = osTmpFile + "fill_val_work.tif";

    hValDS = GDALCreate( hDriver, osValTmpFile, nXSize, nYSize, 1,
                         GDALGetRasterDataType( hTargetBand ), 
                         (char **) apszOptions );
    
    if( hValDS == NULL )
        return CE_Failure;

    hValBand = GDALGetRasterBand( hValDS, 1 );

/* -------------------------------------------------------------------- */
/*      Create a mask file to make it clear what pixels can be filtered */
/*      on the filtering pass.                                          */
/* -------------------------------------------------------------------- */
    GDALDatasetH hFiltMaskDS;
    GDALRasterBandH hFiltMaskBand;
    CPLString osFiltMaskTmpFile = osTmpFile + "fill_filtmask_work.tif";
    
    hFiltMaskDS = 
        GDALCreate( hDriver, osFiltMaskTmpFile, nXSize, nYSize, 1,
                    GDT_Byte, (char **) apszOptions );
    
    if( hFiltMaskDS == NULL )
        return CE_Failure;

    hFiltMaskBand = GDALGetRasterBand( hFiltMaskDS, 1 );

/* -------------------------------------------------------------------- */
/*      Allocate buffers for last scanline and this scanline.           */
/* -------------------------------------------------------------------- */
    GUInt32 *panLastY, *panThisY, *panTopDownY;
    float   *pafLastValue, *pafThisValue, *pafScanline, *pafTopDownValue;
    GByte   *pabyMask, *pabyFiltMask;
    int     iX;
    int     iY;

    panLastY = (GUInt32 *) VSICalloc(nXSize,sizeof(GUInt32));
    panThisY = (GUInt32 *) VSICalloc(nXSize,sizeof(GUInt32));
    panTopDownY = (GUInt32 *) VSICalloc(nXSize,sizeof(GUInt32));
    pafLastValue = (float *) VSICalloc(nXSize,sizeof(float));
    pafThisValue = (float *) VSICalloc(nXSize,sizeof(float));
    pafTopDownValue = (float *) VSICalloc(nXSize,sizeof(float));
    pafScanline = (float *) VSICalloc(nXSize,sizeof(float));
    pabyMask = (GByte *) VSICalloc(nXSize,1);
    pabyFiltMask = (GByte *) VSICalloc(nXSize,1);
    if (panLastY == NULL || panThisY == NULL || panTopDownY == NULL ||
        pafLastValue == NULL || pafThisValue == NULL || pafTopDownValue == NULL ||
        pafScanline == NULL || pabyMask == NULL || pabyFiltMask == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Could not allocate enough memory for temporary buffers");

        eErr = CE_Failure;
        goto end;
    }

    for( iX = 0; iX < nXSize; iX++ )
    {
        panLastY[iX] = nNoDataVal;
    }

/* ==================================================================== */
/*      Make first pass from top to bottom collecting the "last         */
/*      known value" for each column and writing it out to the work     */
/*      files.                                                          */
/* ==================================================================== */
    
    for( iY = 0; iY < nYSize && eErr == CE_None; iY++ )
    {
/* -------------------------------------------------------------------- */
/*      Read data and mask for this line.                               */
/* -------------------------------------------------------------------- */
        eErr = 
            GDALRasterIO( hMaskBand, GF_Read, 0, iY, nXSize, 1, 
                          pabyMask, nXSize, 1, GDT_Byte, 0, 0 );

        if( eErr != CE_None )
            break;

        eErr = 
            GDALRasterIO( hTargetBand, GF_Read, 0, iY, nXSize, 1, 
                          pafScanline, nXSize, 1, GDT_Float32, 0, 0 );
        
        if( eErr != CE_None )
            break;
        
/* -------------------------------------------------------------------- */
/*      Figure out the most recent pixel for each column.               */
/* -------------------------------------------------------------------- */
        
        for( iX = 0; iX < nXSize; iX++ )
        {
            if( pabyMask[iX] )
            {
                pafThisValue[iX] = pafScanline[iX];
                panThisY[iX] = iY;
            }
            else if( iY - panLastY[iX] <= dfMaxSearchDist )
            {
                pafThisValue[iX] = pafLastValue[iX];
                panThisY[iX] = panLastY[iX];
            }
            else
            {
                panThisY[iX] = nNoDataVal;
            }
        }
        
/* -------------------------------------------------------------------- */
/*      Write out best index/value to working files.                    */
/* -------------------------------------------------------------------- */
        eErr = GDALRasterIO( hYBand, GF_Write, 0, iY, nXSize, 1, 
                             panThisY, nXSize, 1, GDT_UInt32, 0, 0 );
        if( eErr != CE_None )
            break;

        eErr = GDALRasterIO( hValBand, GF_Write, 0, iY, nXSize, 1, 
                             pafThisValue, nXSize, 1, GDT_Float32, 0, 0 );
        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Flip this/last buffers.                                         */
/* -------------------------------------------------------------------- */
        {
            float *pafTmp = pafThisValue;
            pafThisValue = pafLastValue;
            pafLastValue = pafTmp;

            GUInt32 *panTmp = panThisY;
            panThisY = panLastY;
            panLastY = panTmp;
        }

/* -------------------------------------------------------------------- */
/*      report progress.                                                */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None
            && !pfnProgress( dfProgressRatio * (0.5*(iY+1) / (double)nYSize), 
                             "Filling...", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* ==================================================================== */
/*      Now we will do collect similar this/last information from       */
/*      bottom to top and use it in combination with the top to         */
/*      bottom search info to interpolate.                              */
/* ==================================================================== */
    for( iY = nYSize-1; iY >= 0 && eErr == CE_None; iY-- )
    {
        eErr = 
            GDALRasterIO( hMaskBand, GF_Read, 0, iY, nXSize, 1, 
                          pabyMask, nXSize, 1, GDT_Byte, 0, 0 );

        if( eErr != CE_None )
            break;

        eErr = 
            GDALRasterIO( hTargetBand, GF_Read, 0, iY, nXSize, 1, 
                          pafScanline, nXSize, 1, GDT_Float32, 0, 0 );
        
        if( eErr != CE_None )
            break;
        
/* -------------------------------------------------------------------- */
/*      Figure out the most recent pixel for each column.               */
/* -------------------------------------------------------------------- */
        
        for( iX = 0; iX < nXSize; iX++ )
        {
            if( pabyMask[iX] )
            {
                pafThisValue[iX] = pafScanline[iX];
                panThisY[iX] = iY;
            }
            else if( panLastY[iX] - iY <= dfMaxSearchDist )
            {
                pafThisValue[iX] = pafLastValue[iX];
                panThisY[iX] = panLastY[iX];
            }
            else
            {
                panThisY[iX] = nNoDataVal;
            }
        }
        
/* -------------------------------------------------------------------- */
/*      Load the last y and corresponding value from the top down pass. */
/* -------------------------------------------------------------------- */
        eErr = 
            GDALRasterIO( hYBand, GF_Read, 0, iY, nXSize, 1, 
                          panTopDownY, nXSize, 1, GDT_UInt32, 0, 0 );

        if( eErr != CE_None )
            break;

        eErr = 
            GDALRasterIO( hValBand, GF_Read, 0, iY, nXSize, 1, 
                          pafTopDownValue, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Attempt to interpolate any pixels that are nodata.              */
/* -------------------------------------------------------------------- */
        memset( pabyFiltMask, 0, nXSize );
        for( iX = 0; iX < nXSize; iX++ )
        {
            int iStep, iQuad;
            int nThisMaxSearchDist = nMaxSearchDist;

            // If this was a valid target - no change.
            if( pabyMask[iX] )
                continue;

            // Quadrants 0:topleft, 1:bottomleft, 2:topright, 3:bottomright
            double adfQuadDist[4];
            double adfQuadValue[4];

            for( iQuad = 0; iQuad < 4; iQuad++ )
                adfQuadDist[iQuad] = dfMaxSearchDist + 1.0;
            
            // Step left and right by one pixel searching for the closest 
            // target value for each quadrant. 
            for( iStep = 0; iStep < nThisMaxSearchDist; iStep++ )
            {
                int iLeftX = MAX(0,iX - iStep);
                int iRightX = MIN(nXSize-1,iX + iStep);
                
                // top left includes current line 
                QUAD_CHECK(adfQuadDist[0],adfQuadValue[0], 
                           iLeftX, panTopDownY[iLeftX], iX, iY,
                           pafTopDownValue[iLeftX] );

                // bottom left 
                QUAD_CHECK(adfQuadDist[1],adfQuadValue[1], 
                           iLeftX, panLastY[iLeftX], iX, iY, 
                           pafLastValue[iLeftX] );

                // top right and bottom right do no include center pixel.
                if( iStep == 0 )
                     continue;
                    
                // top right includes current line 
                QUAD_CHECK(adfQuadDist[2],adfQuadValue[2], 
                           iRightX, panTopDownY[iRightX], iX, iY,
                           pafTopDownValue[iRightX] );

                // bottom right
                QUAD_CHECK(adfQuadDist[3],adfQuadValue[3], 
                           iRightX, panLastY[iRightX], iX, iY,
                           pafLastValue[iRightX] );

                // every four steps, recompute maximum distance.
                if( (iStep & 0x3) == 0 )
                    nThisMaxSearchDist = (int) floor(
                        MAX(MAX(adfQuadDist[0],adfQuadDist[1]),
                            MAX(adfQuadDist[2],adfQuadDist[3])) );
            }

            double dfWeightSum = 0.0;
            double dfValueSum = 0.0;
            
            for( iQuad = 0; iQuad < 4; iQuad++ )
            {
                if( adfQuadDist[iQuad] <= dfMaxSearchDist )
                {
                    double dfWeight = 1.0 / adfQuadDist[iQuad];

                    dfWeightSum += dfWeight;
                    dfValueSum += adfQuadValue[iQuad] * dfWeight;
                }
            }

            if( dfWeightSum > 0.0 )
            {
                pabyMask[iX] = 255;
                pabyFiltMask[iX] = 255;
                pafScanline[iX] = (float) (dfValueSum / dfWeightSum);
            }

        }

/* -------------------------------------------------------------------- */
/*      Write out the updated data and mask information.                */
/* -------------------------------------------------------------------- */
        eErr = 
            GDALRasterIO( hTargetBand, GF_Write, 0, iY, nXSize, 1, 
                          pafScanline, nXSize, 1, GDT_Float32, 0, 0 );
        
        if( eErr != CE_None )
            break;

        eErr = 
            GDALRasterIO( hFiltMaskBand, GF_Write, 0, iY, nXSize, 1, 
                          pabyFiltMask, nXSize, 1, GDT_Byte, 0, 0 );
        
        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Flip this/last buffers.                                         */
/* -------------------------------------------------------------------- */
        {
            float *pafTmp = pafThisValue;
            pafThisValue = pafLastValue;
            pafLastValue = pafTmp;
            
            GUInt32 *panTmp = panThisY;
            panThisY = panLastY;
            panLastY = panTmp;
        }

/* -------------------------------------------------------------------- */
/*      report progress.                                                */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None
            && !pfnProgress( dfProgressRatio*(0.5+0.5*(nYSize-iY) / (double)nYSize), 
                             "Filling...", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }        

/* ==================================================================== */
/*      Now we will do iterative average filters over the               */
/*      interpolated values to smooth things out and make linear        */
/*      artifacts less obvious.                                         */
/* ==================================================================== */
    if( eErr == CE_None && nSmoothingIterations > 0 )
    {
        // force masks to be to flushed and recomputed.
        GDALFlushRasterCache( hMaskBand );

        void *pScaledProgress;
        pScaledProgress =
            GDALCreateScaledProgress( dfProgressRatio, 1.0, pfnProgress, NULL );

        eErr = GDALMultiFilter( hTargetBand, hMaskBand, hFiltMaskBand, 
                                nSmoothingIterations,
                                GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress( pScaledProgress );
    }

/* -------------------------------------------------------------------- */
/*      Close and clean up temporary files. Free working buffers        */
/* -------------------------------------------------------------------- */
end:
    CPLFree(panLastY);
    CPLFree(panThisY);
    CPLFree(panTopDownY);
    CPLFree(pafLastValue);
    CPLFree(pafThisValue);
    CPLFree(pafTopDownValue);
    CPLFree(pafScanline);
    CPLFree(pabyMask);
    CPLFree(pabyFiltMask);

    GDALClose( hYDS );
    GDALClose( hValDS );
    GDALClose( hFiltMaskDS );

    GDALDeleteDataset( hDriver, osYTmpFile );
    GDALDeleteDataset( hDriver, osValTmpFile );
    GDALDeleteDataset( hDriver, osFiltMaskTmpFile );

    return eErr;
}

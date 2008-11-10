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

        pafOutLine[iX] = dfValSum / dfWeightSum;
    }
}

/************************************************************************/
/*                          GDALMultiFilter()                           */
/*                                                                      */
/*      Apply multiple iterations of a 3x3 smoothing filter.            */
/************************************************************************/

static CPLErr
GDALMultiFilter( GDALRasterBandH hTargetBand, 
                 GDALRasterBandH hTargetMaskBand, 
                 GDALRasterBandH hFiltMaskBand,
                 int nIterations,
                 GDALProgressFunc pfnProgress, 
                 void * pProgressArg )

{
    float *pafLineBuf;
    GByte *pabyTMaskBuf;
    GByte *pabyFMaskBuf;
    float *pafThisLineOut, *pafLastLineOut;

    int   nBufLines = nIterations + 2;
    int   nFirstBufLine;  
    int   nXSize = GDALGetRasterBandXSize( hTargetBand );
    int   nYSize = GDALGetRasterBandYSize( hTargetBand );
    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Allocate rotating buffers.                                      */
/* -------------------------------------------------------------------- */
    pabyTMaskBuf = (GByte *) CPLMalloc(nXSize * nBufLines);
    pabyFMaskBuf = (GByte *) CPLMalloc(nXSize * nBufLines);
    pafLineBuf = (float *) CPLCalloc(nXSize * nBufLines,sizeof(float));
    pafThisLineOut = (float *) CPLCalloc(nXSize,sizeof(float));
    pafLastLineOut = (float *) CPLCalloc(nXSize,sizeof(float));

/* -------------------------------------------------------------------- */
/*      Process rotating buffers.                                       */
/* -------------------------------------------------------------------- */
    for( nFirstBufLine = 0 - nBufLines + 1;
         nFirstBufLine < nYSize && eErr == CE_None; 
         nFirstBufLine++ )
    {
        int nFirstBufLineOffset = nFirstBufLine % nBufLines;

        if( nFirstBufLineOffset < 0 )
            nFirstBufLineOffset += nBufLines;

/* -------------------------------------------------------------------- */
/*      Figure out what line we want to load, and where it goes in      */
/*      the rotating buffer.                                            */
/* -------------------------------------------------------------------- */
        int iLineToLoad = nFirstBufLine + nBufLines - 1;
        int iBufOffset = (iLineToLoad - nFirstBufLine + nFirstBufLineOffset)
            % nBufLines;

/* -------------------------------------------------------------------- */
/*      Read the new data.                                              */
/* -------------------------------------------------------------------- */
        if( iLineToLoad < nYSize )
        {
            eErr = 
                GDALRasterIO( hTargetMaskBand, GF_Read, 
                              0, iLineToLoad, nXSize, 1, 
                              pabyTMaskBuf + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Byte, 0, 0 );
            
            if( eErr != CE_None )
                break;

            eErr = 
                GDALRasterIO( hFiltMaskBand, GF_Read, 
                              0, iLineToLoad, nXSize, 1, 
                              pabyFMaskBuf + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Byte, 0, 0 );
            
            if( eErr != CE_None )
                break;

            eErr = 
                GDALRasterIO( hTargetBand, GF_Read, 
                              0, iLineToLoad, nXSize, 1, 
                              pafLineBuf + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Float32, 0, 0 );
            
            if( eErr != CE_None )
                break;
        }

/* -------------------------------------------------------------------- */
/*      Loop over the loaded data, applying the filter to all loaded    */
/*      lines with neighbours.                                          */
/* -------------------------------------------------------------------- */
        int iFLine;
        int bHaveModifiedLastLine = FALSE;

        for( iFLine = nFirstBufLine+1; 
             iFLine < nFirstBufLine + nBufLines - 1;
             iFLine++ )
        {
            int iLastOffset, iThisOffset, iNextOffset;

            // currently this skips the first and last line.  Eventually 
            // we will enable these too.  TODO
            if( iFLine < 1 || iFLine >= nYSize-1 )
                continue;

            iLastOffset = (iFLine-1 - nFirstBufLine + nFirstBufLineOffset)
                % nBufLines;
            iThisOffset = (iFLine - nFirstBufLine + nFirstBufLineOffset)
                % nBufLines;
            iNextOffset = (iFLine+1 - nFirstBufLine + nFirstBufLineOffset)
                % nBufLines;

            GDALFilterLine( 
                pafLineBuf + iLastOffset * nXSize, 
                pafLineBuf + iThisOffset * nXSize,
                pafLineBuf + iNextOffset * nXSize,
                pafThisLineOut, 
                pabyTMaskBuf + iLastOffset * nXSize,
                pabyTMaskBuf + iThisOffset * nXSize,
                pabyTMaskBuf + iNextOffset * nXSize,
                pabyFMaskBuf + iThisOffset * nXSize, 
                nXSize );

            if( bHaveModifiedLastLine )
            {
                memcpy( pafLineBuf+ iLastOffset * nXSize, 
                        pafLastLineOut, 
                        sizeof(float) * nXSize );
            }

            if( iFLine == nFirstBufLine + nBufLines - 2 ) 
            {
                // last line to process?  If so, we push it back into the
                // line data buffer too.
                memcpy( pafLineBuf+ iThisOffset * nXSize, 
                        pafThisLineOut, 
                        sizeof(float) * nXSize );
            }
            else // flip this/last line buffers.
            {
                float *pafTmp = pafLastLineOut;
                pafLastLineOut = pafThisLineOut;
                pafThisLineOut = pafTmp;
                bHaveModifiedLastLine = TRUE;
            }
        }

/* -------------------------------------------------------------------- */
/*      Write out the top data line that will be rolling out of our     */
/*      buffer.                                                         */
/* -------------------------------------------------------------------- */
        int iLineToSave = nFirstBufLine;
        
        iBufOffset = (iLineToSave - nFirstBufLine + nFirstBufLineOffset)
            % nBufLines;

        if( iLineToSave >= 0 && iLineToSave < nYSize )
        {
            eErr = 
                GDALRasterIO( hTargetBand, GF_Write, 
                              0, iLineToSave, nXSize, 1, 
                              pafLineBuf + nXSize * iBufOffset, nXSize, 1, 
                              GDT_Float32, 0, 0 );
            
            if( eErr != CE_None )
                break;
        }
    }

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

CPLErr
GDALFillNodata( GDALRasterBandH hTargetBand, 
                GDALRasterBandH hMaskBand,
                double dfMaxSearchDist, 
                int bConicSearch, 
                int nSmoothingIterations,
                char **papszOptions,
                GDALProgressFunc pfnProgress, 
                void * pProgressArg )

{
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

/* -------------------------------------------------------------------- */
/*      Create a work file to hold the Y "last value" indices.          */
/* -------------------------------------------------------------------- */
    GDALDriverH  hDriver = GDALGetDriverByName( "GTiff" );
    GDALDatasetH hYDS;
    GDALRasterBandH hYBand;
    static const char *apszOptions[] = { "COMPRESS=LZW", NULL };
    CPLString osTmpFile = CPLGenerateTempFilename("");
    
    hYDS = GDALCreate( hDriver, (osTmpFile+"fill_y_work.tif").c_str(), 
                       nXSize, nYSize, 1, 
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

    hValDS = GDALCreate( hDriver, (osTmpFile+"fill_val_work.tif").c_str(), 
                         nXSize, nYSize, 1,
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
    
    hFiltMaskDS = 
        GDALCreate( hDriver, (osTmpFile+"fill_filtmask_work.tif").c_str(), 
                    nXSize, nYSize, 1,
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

    panLastY = (GUInt32 *) CPLCalloc(nXSize,4);
    panThisY = (GUInt32 *) CPLCalloc(nXSize,4);
    panTopDownY = (GUInt32 *) CPLCalloc(nXSize,4);
    pafLastValue = (float *) CPLCalloc(nXSize,sizeof(float));
    pafThisValue = (float *) CPLCalloc(nXSize,sizeof(float));
    pafTopDownValue = (float *) CPLCalloc(nXSize,sizeof(float));
    pafScanline = (float *) CPLCalloc(nXSize,sizeof(float));
    pabyMask = (GByte *) CPLCalloc(nXSize,1);
    pabyFiltMask = (GByte *) CPLCalloc(nXSize,1);

    for( iX = 0; iX < nXSize; iX++ )
    {
        panLastY[iX] = nNoDataVal;
    }

/* ==================================================================== */
/*      Make first pass from top to bottom collecting the "last         */
/*      known value" for each column and writing it out to the work     */
/*      files.                                                          */
/* ==================================================================== */
    int     iY;
    
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
                pafScanline[iX] = dfValueSum / dfWeightSum;
            }
        }

/* -------------------------------------------------------------------- */
/*      Write out the updated data and mask information.                */
/* -------------------------------------------------------------------- */
#ifdef notdef
        eErr = 
            GDALRasterIO( hMaskBand, GF_Write, 0, iY, nXSize, 1, 
                          pabyMask, nXSize, 1, GDT_Byte, 0, 0 );

        if( eErr != CE_None )
            break;
#endif

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

#ifdef notdef
        while( nSmoothingIterations-- > 0 )
            eErr = GDALMultiFilter( hTargetBand, hMaskBand, hFiltMaskBand, 
                                    1,
                                    pfnProgress, pProgressArg );
#else
        eErr = GDALMultiFilter( hTargetBand, hMaskBand, hFiltMaskBand, 
                                nSmoothingIterations,
                                pfnProgress, pProgressArg );
#endif
        
    }

    GDALClose( hYDS );
    GDALClose( hValDS );
    GDALClose( hFiltMaskDS );

    return eErr;
}

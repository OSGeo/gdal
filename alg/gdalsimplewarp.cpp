/******************************************************************************
 * $Id$
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Simple (source in memory) warp algorithm.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging 
 *                          Fort Collin, CO
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
 * Revision 1.2  2002/12/05 21:44:19  warmerda
 * initial implementation
 *
 * Revision 1.1  2002/12/05 05:43:23  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "gdal_alg.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GDALSimpleImageWarp()                         */
/************************************************************************/

int GDALSimpleImageWarp( GDALDatasetH hSrcDS, GDALDatasetH hDstDS, 
                         int nBandCount, int *panBandList,
                         GByte *pabyRedLUT, 
                         GByte *pabyGreenLUT,
                         GByte *pabyBlueLUT,
                         GDALTransformerFunc pfnTransform, void *pTransformArg,
                         GDALProgressFunc pfnProgress, void *pProgressArg, 
                         const char **papszWarpOptions )

{
    int		iBand, bCancelled = FALSE;

/* -------------------------------------------------------------------- */
/*      If no bands provided assume we should process all bands.        */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 )
    {
        int nResult;

        nBandCount = GDALGetRasterCount( hSrcDS ); 
        panBandList = (int *) CPLCalloc(sizeof(int),nBandCount);

        for( iBand = 0; iBand < nBandCount; iBand++ )
            panBandList[iBand] = iBand+1;

        nResult = GDALSimpleImageWarp( hSrcDS, hDstDS, nBandCount, panBandList,
                                       pabyRedLUT, pabyGreenLUT, pabyBlueLUT,
                                       pfnTransform, pTransformArg, 
                                       pfnProgress, pProgressArg, 
                                       papszWarpOptions );
        CPLFree( panBandList );
        return nResult;
    }

/* -------------------------------------------------------------------- */
/*      Post initial progress.                                          */
/* -------------------------------------------------------------------- */
    if( pfnProgress )
    {
        if( !pfnProgress( 0.0, "Loading source image", pProgressArg ) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Load the source image band(s).                                  */
/* -------------------------------------------------------------------- */
    int   nSrcXSize = GDALGetRasterXSize(hSrcDS);
    int   nSrcYSize = GDALGetRasterYSize(hSrcDS);
    GByte **papabySrcData;

    papabySrcData = (GByte **) CPLCalloc(nBandCount,sizeof(int));
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        papabySrcData[iBand] = (GByte *) VSIMalloc(nSrcXSize*nSrcYSize);
        
        GDALRasterIO( GDALGetRasterBand(hSrcDS,panBandList[iBand]), GF_Read,
                      0, 0, nSrcXSize, nSrcYSize, 
                      papabySrcData[iBand], nSrcXSize, nSrcYSize, GDT_Byte, 
                      0, 0 );
    }

/* -------------------------------------------------------------------- */
/*      Allocate scanline buffers for output image.                     */
/* -------------------------------------------------------------------- */
    int nDstXSize = GDALGetRasterXSize( hDstDS );
    int nDstYSize = GDALGetRasterYSize( hDstDS );
    GByte **papabyDstLine;

    papabyDstLine = (GByte **) CPLCalloc(nBandCount,sizeof(int));
    
    for( iBand = 0; iBand < nBandCount; iBand++ )
        papabyDstLine[iBand] = (GByte *) CPLMalloc( nDstXSize );

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* -------------------------------------------------------------------- */
/*      Loop over all the scanlines in the output image.                */
/* -------------------------------------------------------------------- */
    int iDstY;

    for( iDstY = 0; iDstY < nDstYSize; iDstY++ )
    {
        int iDstX;

        // Clear output buffer to "transparent" value.  Shouldn't we
        // really be reading from the destination file to support overlay?
        for( iBand = 0; iBand < nBandCount; iBand++ )
            memset( papabyDstLine[iBand], 0, nDstXSize );

        // Set point to transform.
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5;
            padfY[iDstX] = iDstY + 0.5;
            padfZ[iDstX] = 0.0;
        }

        // Transform the points from destination pixel/line coordinates
        // to source pixel/line coordinates.
        pfnTransform( pTransformArg, TRUE, nDstXSize, 
                      padfX, padfY, padfZ, pabSuccess );

        // Loop over the output scanline.
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < 0.0 || padfY[iDstX] < 0.0 )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = (int) padfX[iDstX];
            iSrcY = (int) padfY[iDstX];

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;
            
            for( iBand = 0; iBand < nBandCount; iBand++ )
                papabyDstLine[iBand][iDstX] = papabySrcData[iBand][iSrcOffset];
        }

        // Write scanline to disk. 
        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            GDALRasterIO( GDALGetRasterBand(hDstDS,iBand+1), GF_Write,
                          0, iDstY, nDstXSize, 1, 
                          papabyDstLine[iBand], nDstXSize, 1, GDT_Byte, 0, 0 );
        }

        if( pfnProgress != NULL )
        {
            if( !pfnProgress( (iDstY+1) / (double) nDstYSize, 
                              "", pProgressArg ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                bCancelled = TRUE;
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup working buffers.                                        */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        CPLFree( papabyDstLine[iBand] );
        CPLFree( papabySrcData[iBand] );
    }

    CPLFree( papabyDstLine );
    CPLFree( papabySrcData );
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
    
    return !bCancelled;
}



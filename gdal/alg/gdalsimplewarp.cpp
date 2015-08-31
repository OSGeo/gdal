/******************************************************************************
 * $Id$
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Simple (source in memory) warp algorithm.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging, Fort Collin,CO
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

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static void 
GDALSimpleWarpRemapping( int nBandCount, GByte **papabySrcData, 
                         int nSrcXSize, int nSrcYSize,
                         char **papszWarpOptions );

/************************************************************************/
/*                        GDALSimpleImageWarp()                         */
/************************************************************************/

/**
 * Perform simple image warp.
 *
 * Copies an image from a source dataset to a destination dataset applying
 * an application defined transformation.   This algorithm is called simple
 * because it lacks many options such as resampling kernels (other than 
 * nearest neighbour), support for data types other than 8bit, and the
 * ability to warp images without holding the entire source and destination
 * image in memory.
 *
 * The following option(s) may be passed in papszWarpOptions. 
 * <ul>
 * <li> "INIT=v[,v...]": This option indicates that the output dataset should
 * be initialized to the indicated value in any area valid data is not written.
 * Distinct values may be listed for each band separated by columns. 
 * </ul>
 *
 * @param hSrcDS the source image dataset. 
 * @param hDstDS the destination image dataset. 
 * @param nBandCount the number of bands to be warped.  If zero, all bands
 * will be processed.
 * @param panBandList the list of bands to translate. 
 * @param pfnTransform the transformation function to call.  See 
 * GDALTransformerFunc(). 
 * @param pTransformArg the callback handle to pass to pfnTransform.
 * @param pfnProgress the function used to report progress.  See 
 * GDALProgressFunc(). 
 * @param pProgressArg the callback handle to pass to pfnProgress. 
 * @param papszWarpOptions additional options controlling the warp. 
 * 
 * @return TRUE if the operation completes, or FALSE if an error occurs. 
 */

int CPL_STDCALL
GDALSimpleImageWarp( GDALDatasetH hSrcDS, GDALDatasetH hDstDS, 
                     int nBandCount, int *panBandList, 
                     GDALTransformerFunc pfnTransform, void *pTransformArg,
                     GDALProgressFunc pfnProgress, void *pProgressArg, 
                     char **papszWarpOptions )
    
{
    VALIDATE_POINTER1( hSrcDS, "GDALSimpleImageWarp", 0 );
    VALIDATE_POINTER1( hDstDS, "GDALSimpleImageWarp", 0 );

    int		iBand, bCancelled = FALSE;

/* -------------------------------------------------------------------- */
/*      If no bands provided assume we should process all bands.        */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 )
    {
        int nResult;

        nBandCount = GDALGetRasterCount( hSrcDS ); 
        if (nBandCount == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No raster band in source dataset");
            return FALSE;
        }

        panBandList = (int *) CPLCalloc(sizeof(int),nBandCount);

        for( iBand = 0; iBand < nBandCount; iBand++ )
            panBandList[iBand] = iBand+1;

        nResult = GDALSimpleImageWarp( hSrcDS, hDstDS, nBandCount, panBandList,
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
        if( !pfnProgress( 0.0, "", pProgressArg ) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Load the source image band(s).                                  */
/* -------------------------------------------------------------------- */
    int   nSrcXSize = GDALGetRasterXSize(hSrcDS);
    int   nSrcYSize = GDALGetRasterYSize(hSrcDS);
    GByte **papabySrcData;

    papabySrcData = (GByte **) CPLCalloc(nBandCount,sizeof(GByte*));
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        papabySrcData[iBand] = (GByte *) VSIMalloc(nSrcXSize*nSrcYSize);
        
        GDALRasterIO( GDALGetRasterBand(hSrcDS,panBandList[iBand]), GF_Read,
                      0, 0, nSrcXSize, nSrcYSize, 
                      papabySrcData[iBand], nSrcXSize, nSrcYSize, GDT_Byte, 
                      0, 0 );
    }

/* -------------------------------------------------------------------- */
/*      Check for remap request(s).                                     */
/* -------------------------------------------------------------------- */
    GDALSimpleWarpRemapping( nBandCount, papabySrcData, nSrcXSize, nSrcYSize,
                             papszWarpOptions );

/* -------------------------------------------------------------------- */
/*      Allocate scanline buffers for output image.                     */
/* -------------------------------------------------------------------- */
    int nDstXSize = GDALGetRasterXSize( hDstDS );
    int nDstYSize = GDALGetRasterYSize( hDstDS );
    GByte **papabyDstLine;

    papabyDstLine = (GByte **) CPLCalloc(nBandCount,sizeof(GByte*));
    
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
/*      Establish the value we will use to initialize the bands.  We    */
/*      default to -1 indicating the initial value should be read       */
/*      and preserved from the source file, but allow this to be        */
/*      overridden by passed                                            */
/*      option(s).                                                      */
/* -------------------------------------------------------------------- */
    int         *panBandInit;

    panBandInit = (int *) CPLCalloc(sizeof(int),nBandCount);
    if( CSLFetchNameValue( papszWarpOptions, "INIT" ) )
    {
        int  iBand, nTokenCount;
        char **papszTokens = 
            CSLTokenizeStringComplex( CSLFetchNameValue( papszWarpOptions, 
                                                         "INIT" ),
                                      " ,", FALSE, FALSE );

        nTokenCount = CSLCount(papszTokens);

        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            if( nTokenCount == 0 )
                panBandInit[iBand] = 0;
            else
                panBandInit[iBand] = 
                    atoi(papszTokens[MIN(iBand,nTokenCount-1)]);
        }

        CSLDestroy(papszTokens);
    }

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
        {
            if( panBandInit[iBand] == -1 )
                GDALRasterIO( GDALGetRasterBand(hDstDS,iBand+1), GF_Read,
                              0, iDstY, nDstXSize, 1, 
                              papabyDstLine[iBand], nDstXSize, 1, GDT_Byte, 
                              0, 0 );
            else
                memset( papabyDstLine[iBand], panBandInit[iBand], nDstXSize );
        }

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

    CPLFree( panBandInit );
    CPLFree( papabyDstLine );
    CPLFree( papabySrcData );
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
    
    return !bCancelled;
}

/************************************************************************/
/*                      GDALSimpleWarpRemapping()                       */
/*                                                                      */
/*      This function implements any raster remapping requested in      */
/*      the options list.  The remappings are applied to the source     */
/*      data before warping.  Two kinds are support ... REMAP           */
/*      commands which remap selected pixel values for any band and     */
/*      REMAP_MULTI which only remap pixels matching the input in       */
/*      all bands at once (ie. to remap an RGB value to another).       */
/************************************************************************/

static void 
GDALSimpleWarpRemapping( int nBandCount, GByte **papabySrcData, 
                         int nSrcXSize, int nSrcYSize,
                         char **papszWarpOptions )

{

/* ==================================================================== */
/*      Process any and all single value REMAP commands.                */
/* ==================================================================== */
    int  iRemap;
    char **papszRemaps = CSLFetchNameValueMultiple( papszWarpOptions, 
                                                    "REMAP" );

    for( iRemap = 0; iRemap < CSLCount(papszRemaps); iRemap++ )
    {

/* -------------------------------------------------------------------- */
/*      What are the pixel values to map from and to?                   */
/* -------------------------------------------------------------------- */
        char **papszTokens = CSLTokenizeString( papszRemaps[iRemap] );
        int  nFromValue, nToValue;
        
        if( CSLCount(papszTokens) != 2 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Ill formed REMAP `%s' ignored in GDALSimpleWarpRemapping()", 
                      papszRemaps[iRemap] );
            CSLDestroy( papszTokens );
            continue;
        }

        nFromValue = atoi(papszTokens[0]);
        nToValue = atoi(papszTokens[1]);
        
        CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Pass over each band searching for matches.                      */
/* -------------------------------------------------------------------- */
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            GByte *pabyData = papabySrcData[iBand];
            int   nPixelCount = nSrcXSize * nSrcYSize;
            
            while( nPixelCount != 0 )
            {
                if( *pabyData == nFromValue )
                    *pabyData = (GByte) nToValue;

                pabyData++;
                nPixelCount--;
            }
        }
    }

    CSLDestroy( papszRemaps );

/* ==================================================================== */
/*      Process any and all REMAP_MULTI commands.                       */
/* ==================================================================== */
    papszRemaps = CSLFetchNameValueMultiple( papszWarpOptions, 
                                             "REMAP_MULTI" );

    for( iRemap = 0; iRemap < CSLCount(papszRemaps); iRemap++ )
    {
/* -------------------------------------------------------------------- */
/*      What are the pixel values to map from and to?                   */
/* -------------------------------------------------------------------- */
        char **papszTokens = CSLTokenizeString( papszRemaps[iRemap] );
        int *panFromValue, *panToValue;
        int  nMapBandCount, iBand;

        if( CSLCount(papszTokens) % 2 == 1 
            || CSLCount(papszTokens) == 0 
            || CSLCount(papszTokens) > nBandCount * 2 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Ill formed REMAP_MULTI `%s' ignored in GDALSimpleWarpRemapping()", 
                      papszRemaps[iRemap] );
            continue;
        }

        nMapBandCount = CSLCount(papszTokens) / 2;
        
        panFromValue = (int *) CPLMalloc(sizeof(int) * nMapBandCount );
        panToValue = (int *) CPLMalloc(sizeof(int) * nMapBandCount );

        for( iBand = 0; iBand < nMapBandCount; iBand++ )
        {
            panFromValue[iBand] = atoi(papszTokens[iBand]);
            panToValue[iBand] = atoi(papszTokens[iBand+nMapBandCount]);
        }

        CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Search for matching values to replace.                          */
/* -------------------------------------------------------------------- */
        int   nPixelCount = nSrcXSize * nSrcYSize;
        int   iPixel;

        for( iPixel = 0; iPixel < nPixelCount; iPixel++ )
        {
            if( papabySrcData[0][iPixel] != panFromValue[0] )
                continue;

            int bMatch = TRUE;

            for( iBand = 1; iBand < nMapBandCount; iBand++ )
            {
                if( papabySrcData[iBand][iPixel] != panFromValue[iBand] )
                    bMatch = FALSE;
            }

            if( !bMatch )
                continue;

            for( iBand = 0; iBand < nMapBandCount; iBand++ )
                papabySrcData[iBand][iPixel] = (GByte) panToValue[iBand];
        }

        CPLFree( panFromValue );
        CPLFree( panToValue );
    }

    CSLDestroy( papszRemaps );
}

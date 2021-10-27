/******************************************************************************
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Simple (source in memory) warp algorithm.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging, Fort Collin, CO
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

#include "cpl_port.h"
#include "gdal_alg.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      GDALSimpleWarpRemapping()                       */
/*                                                                      */
/*      This function implements any raster remapping requested in      */
/*      the options list.  The remappings are applied to the source     */
/*      data before warping.  Two kinds are support ... REMAP           */
/*      commands which remap selected pixel values for any band and     */
/*      REMAP_MULTI which only remap pixels matching the input in       */
/*      all bands at once (i.e. to remap an RGB value to another).      */
/************************************************************************/

static void
GDALSimpleWarpRemapping( int nBandCount, GByte **papabySrcData,
                         int nSrcXSize, int nSrcYSize,
                         char **papszWarpOptions )

{

/* ==================================================================== */
/*      Process any and all single value REMAP commands.                */
/* ==================================================================== */
    char **papszRemaps = CSLFetchNameValueMultiple( papszWarpOptions,
                                                    "REMAP" );

    const int nRemaps = CSLCount(papszRemaps);
    for( int iRemap = 0; iRemap < nRemaps; iRemap++ )
    {

/* -------------------------------------------------------------------- */
/*      What are the pixel values to map from and to?                   */
/* -------------------------------------------------------------------- */
        char **papszTokens = CSLTokenizeString( papszRemaps[iRemap] );

        if( CSLCount(papszTokens) != 2 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ill formed REMAP `%s' ignored in "
                     "GDALSimpleWarpRemapping()",
                     papszRemaps[iRemap] );
            CSLDestroy( papszTokens );
            continue;
        }

        const int nFromValue = atoi(papszTokens[0]);
        const int nToValue = atoi(papszTokens[1]);
        // TODO(schwehr): Why is it ok to narrow ints to byte without checking?
        const GByte byToValue = static_cast<GByte>(nToValue);

        CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Pass over each band searching for matches.                      */
/* -------------------------------------------------------------------- */
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            GByte *pabyData = papabySrcData[iBand];
            int nPixelCount = nSrcXSize * nSrcYSize;

            while( nPixelCount != 0 )
            {
                if( *pabyData == nFromValue )
                    *pabyData = byToValue;

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

    const int nRemapsMulti = CSLCount(papszRemaps);
    for( int iRemap = 0; iRemap < nRemapsMulti; iRemap++ )
    {
/* -------------------------------------------------------------------- */
/*      What are the pixel values to map from and to?                   */
/* -------------------------------------------------------------------- */
        char **papszTokens = CSLTokenizeString( papszRemaps[iRemap] );

        const int nTokens = CSLCount(papszTokens);
        if( nTokens % 2 == 1 ||
            nTokens == 0 ||
            nTokens > nBandCount * 2 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ill formed REMAP_MULTI `%s' ignored in "
                     "GDALSimpleWarpRemapping()",
                     papszRemaps[iRemap]);
            CSLDestroy( papszTokens );
            continue;
        }

        const int nMapBandCount = nTokens / 2;

        int *panFromValue = static_cast<int *>(
            CPLMalloc(sizeof(int) * nMapBandCount ) );
        int *panToValue = static_cast<int *>(
            CPLMalloc(sizeof(int) * nMapBandCount ) );

        for( int iBand = 0; iBand < nMapBandCount; iBand++ )
        {
            panFromValue[iBand] = atoi(papszTokens[iBand]);
            panToValue[iBand] = atoi(papszTokens[iBand+nMapBandCount]);
        }

        CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Search for matching values to replace.                          */
/* -------------------------------------------------------------------- */
        const int nPixelCount = nSrcXSize * nSrcYSize;

        for( int iPixel = 0; iPixel < nPixelCount; iPixel++ )
        {
            bool bMatch = true;

            // Always check band 0.
            for( int iBand = 0; bMatch && iBand < std::max(1, nMapBandCount);
                 iBand++ )
            {
                if( papabySrcData[iBand][iPixel] != panFromValue[iBand] )
                    bMatch = false;
            }

            if( !bMatch )
                continue;

            for( int iBand = 0; iBand < nMapBandCount; iBand++ )
                papabySrcData[iBand][iPixel] =
                    static_cast<GByte>( panToValue[iBand] );
        }

        CPLFree( panFromValue );
        CPLFree( panToValue );
    }

    CSLDestroy( papszRemaps );
}

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

    bool bError = false;

/* -------------------------------------------------------------------- */
/*      If no bands provided assume we should process all bands.        */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 )
    {
        nBandCount = GDALGetRasterCount( hSrcDS );
        if( nBandCount == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No raster band in source dataset");
            return FALSE;
        }

        panBandList = static_cast<int *>(CPLCalloc(sizeof(int), nBandCount));

        for( int iBand = 0; iBand < nBandCount; iBand++ )
            panBandList[iBand] = iBand + 1;

        const int nResult =
            GDALSimpleImageWarp( hSrcDS, hDstDS, nBandCount, panBandList,
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
    const int nSrcXSize = GDALGetRasterXSize(hSrcDS);
    const int nSrcYSize = GDALGetRasterYSize(hSrcDS);
    GByte **papabySrcData = static_cast<GByte **>(
        CPLCalloc(nBandCount, sizeof(GByte*)) );

    bool ok = true;
    for( int iBand = 0; iBand < nBandCount; iBand++ )
    {
        papabySrcData[iBand] = static_cast<GByte *>(
            VSI_MALLOC2_VERBOSE(nSrcXSize, nSrcYSize) );
        if( papabySrcData[iBand] == nullptr )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "GDALSimpleImageWarp out of memory." );
            ok = false;
            break;
        }

        if( GDALRasterIO(
                GDALGetRasterBand(hSrcDS,panBandList[iBand]), GF_Read,
                0, 0, nSrcXSize, nSrcYSize,
                papabySrcData[iBand], nSrcXSize, nSrcYSize, GDT_Byte,
                0, 0 ) != CE_None )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "GDALSimpleImageWarp GDALRasterIO failure %s",
                      CPLGetLastErrorMsg() );
            ok = false;
            break;
        }
    }
    if( !ok )
    {
        for( int i=0; i <= nBandCount; i++ )
        {
            VSIFree(papabySrcData[i]);
        }
        CPLFree(papabySrcData);
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Check for remap request(s).                                     */
/* -------------------------------------------------------------------- */
    GDALSimpleWarpRemapping( nBandCount, papabySrcData, nSrcXSize, nSrcYSize,
                             papszWarpOptions );

/* -------------------------------------------------------------------- */
/*      Allocate scanline buffers for output image.                     */
/* -------------------------------------------------------------------- */
    const int nDstXSize = GDALGetRasterXSize( hDstDS );
    const int nDstYSize = GDALGetRasterYSize( hDstDS );
    GByte **papabyDstLine = static_cast<GByte **>(
        CPLCalloc(nBandCount, sizeof(GByte*)) );

    for( int iBand = 0; iBand < nBandCount; iBand++ )
        papabyDstLine[iBand] = static_cast<GByte *>(CPLMalloc( nDstXSize ));

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX = static_cast<double *>(
        CPLMalloc(sizeof(double) * nDstXSize) );
    double *padfY = static_cast<double *>(
        CPLMalloc(sizeof(double) * nDstXSize) );
    double *padfZ = static_cast<double *>(
        CPLMalloc(sizeof(double) * nDstXSize) );
    int *pabSuccess = static_cast<int *>( CPLMalloc(sizeof(int) * nDstXSize) );

/* -------------------------------------------------------------------- */
/*      Establish the value we will use to initialize the bands.  We    */
/*      default to -1 indicating the initial value should be read       */
/*      and preserved from the source file, but allow this to be        */
/*      overridden by passed                                            */
/*      option(s).                                                      */
/* -------------------------------------------------------------------- */
    int * const panBandInit =
        static_cast<int *>( CPLCalloc(sizeof(int), nBandCount) );
    if( CSLFetchNameValue( papszWarpOptions, "INIT" ) )
    {
        char **papszTokens =
            CSLTokenizeStringComplex( CSLFetchNameValue( papszWarpOptions,
                                                         "INIT" ),
                                      " ,", FALSE, FALSE );

        const int nTokenCount = CSLCount(papszTokens);

        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            if( nTokenCount == 0 )
                panBandInit[iBand] = 0;
            else
                panBandInit[iBand] =
                    atoi(papszTokens[std::min(iBand, nTokenCount- 1)]);
        }

        CSLDestroy(papszTokens);
    }

/* -------------------------------------------------------------------- */
/*      Loop over all the scanlines in the output image.                */
/* -------------------------------------------------------------------- */
    for( int iDstY = 0; iDstY < nDstYSize; iDstY++ )
    {
        // Clear output buffer to "transparent" value.  Should not we
        // really be reading from the destination file to support overlay?
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            if( panBandInit[iBand] == -1 )
            {
                if( GDALRasterIO(
                        GDALGetRasterBand(hDstDS,iBand + 1), GF_Read,
                        0, iDstY, nDstXSize, 1,
                        papabyDstLine[iBand], nDstXSize, 1, GDT_Byte,
                        0, 0 ) != CE_None )
                {
                    bError = TRUE;
                    break;
                }
            }
            else
            {
                memset( papabyDstLine[iBand], panBandInit[iBand], nDstXSize );
            }
        }

        // Set point to transform.
        for( int iDstX = 0; iDstX < nDstXSize; iDstX++ )
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
        for( int iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int.
            if( padfX[iDstX] < 0.0 || padfY[iDstX] < 0.0 )
                continue;

            const int iSrcX = static_cast<int>( padfX[iDstX] );
            const int iSrcY = static_cast<int>( padfY[iDstX] );

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            const int iSrcOffset = iSrcX + iSrcY * nSrcXSize;

            for( int iBand = 0; iBand < nBandCount; iBand++ )
                papabyDstLine[iBand][iDstX] = papabySrcData[iBand][iSrcOffset];
        }

        // Write scanline to disk.
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            if( GDALRasterIO(
                    GDALGetRasterBand(hDstDS,iBand+1), GF_Write,
                    0, iDstY, nDstXSize, 1,
                    papabyDstLine[iBand], nDstXSize,
                    1, GDT_Byte, 0, 0 ) != CE_None )
            {
                bError = TRUE;
                break;
            }
        }

        if( pfnProgress != nullptr )
        {
            if( !pfnProgress( (iDstY + 1) / static_cast<double>(nDstYSize),
                              "", pProgressArg ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                bError = TRUE;
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup working buffers.                                        */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBandCount; iBand++ )
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

    return !bError;
}

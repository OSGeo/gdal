/******************************************************************************
 * $Id$
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Implementation of one or more GDALTrasformerFunc types, including
 *           the GenImgProj (general image reprojector) transformer.
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
 ****************************************************************************/

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "gdal_alg_priv.h"
#include "cpl_list.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");
CPL_C_START
void *GDALDeserializeGCPTransformer( CPLXMLNode *psTree );
void *GDALDeserializeTPSTransformer( CPLXMLNode *psTree );
void *GDALDeserializeGeoLocTransformer( CPLXMLNode *psTree );
void *GDALDeserializeRPCTransformer( CPLXMLNode *psTree );
CPL_C_END

static CPLXMLNode *GDALSerializeReprojectionTransformer( void *pTransformArg );
static void *GDALDeserializeReprojectionTransformer( CPLXMLNode *psTree );

static CPLXMLNode *GDALSerializeGenImgProjTransformer( void *pTransformArg );
static void *GDALDeserializeGenImgProjTransformer( CPLXMLNode *psTree );

static void GDALRefreshGenImgProjTransformer(void* hTransformArg);

/************************************************************************/
/*                          GDALTransformFunc                           */
/*                                                                      */
/*      Documentation for GDALTransformFunc typedef.                    */
/************************************************************************/

/*!

\typedef int GDALTransformerFunc

Generic signature for spatial point transformers.

This function signature is used for a variety of functions that accept 
passed in functions used to transform point locations between two coordinate
spaces.  

The GDALCreateGenImgProjTransformer(), GDALCreateReprojectionTransformer(), 
GDALCreateGCPTransformer() and GDALCreateApproxTransformer() functions can
be used to prepare argument data for some built-in transformers.  As well,
applications can implement their own transformers to the following signature.

\code
typedef int 
(*GDALTransformerFunc)( void *pTransformerArg, 
                        int bDstToSrc, int nPointCount, 
                        double *x, double *y, double *z, int *panSuccess );
\endcode

@param pTransformerArg application supplied callback data used by the
transformer.  

@param bDstToSrc if TRUE the transformation will be from the destination
coordinate space to the source coordinate system, otherwise the transformation
will be from the source coordinate system to the destination coordinate system.

@param nPointCount number of points in the x, y and z arrays.

@param x input X coordinates.  Results returned in same array.

@param y input Y coordinates.  Results returned in same array.

@param z input Z coordinates.  Results returned in same array.

@param panSuccess array of ints in which success (TRUE) or failure (FALSE)
flags are returned for the translation of each point.

@return TRUE if the overall transformation succeeds (though some individual
points may have failed) or FALSE if the overall transformation fails.

*/

/************************************************************************/
/*                      GDALSuggestedWarpOutput()                       */
/************************************************************************/

/**
 * Suggest output file size.
 *
 * This function is used to suggest the size, and georeferenced extents
 * appropriate given the indicated transformation and input file.  It walks
 * the edges of the input file (approximately 20 sample points along each
 * edge) transforming into output coordinates in order to get an extents box.
 *
 * Then a resolution is computed with the intent that the length of the
 * distance from the top left corner of the output imagery to the bottom right
 * corner would represent the same number of pixels as in the source image. 
 * Note that if the image is somewhat rotated the diagonal taken isnt of the
 * whole output bounding rectangle, but instead of the locations where the
 * top/left and bottom/right corners transform.  The output pixel size is 
 * always square.  This is intended to approximately preserve the resolution
 * of the input data in the output file. 
 * 
 * The values returned in padfGeoTransformOut, pnPixels and pnLines are
 * the suggested number of pixels and lines for the output file, and the
 * geotransform relating those pixels to the output georeferenced coordinates.
 *
 * The trickiest part of using the function is ensuring that the 
 * transformer created is from source file pixel/line coordinates to 
 * output file georeferenced coordinates.  This can be accomplished with 
 * GDALCreateGenImgProjTransformer() by passing a NULL for the hDstDS.  
 *
 * @param hSrcDS the input image (it is assumed the whole input images is
 * being transformed). 
 * @param pfnTransformer the transformer function.
 * @param pTransformArg the callback data for the transformer function.
 * @param padfGeoTransformOut the array of six doubles in which the suggested
 * geotransform is returned. 
 * @param pnPixels int in which the suggest pixel width of output is returned.
 * @param pnLines int in which the suggest pixel height of output is returned.
 *
 * @return CE_None if successful or CE_Failure otherwise. 
 */


CPLErr CPL_STDCALL
GDALSuggestedWarpOutput( GDALDatasetH hSrcDS, 
                         GDALTransformerFunc pfnTransformer, 
                         void *pTransformArg, 
                         double *padfGeoTransformOut, 
                         int *pnPixels, int *pnLines )

{
    VALIDATE_POINTER1( hSrcDS, "GDALSuggestedWarpOutput", CE_Failure );

    double adfExtent[4] = { 0 };

    return GDALSuggestedWarpOutput2( hSrcDS, pfnTransformer, pTransformArg, 
                                     padfGeoTransformOut, pnPixels, pnLines, 
                                     adfExtent, 0 );
}


 static int GDALSuggestedWarpOutput2_MustAdjustForRightBorder(
                     GDALTransformerFunc pfnTransformer, void *pTransformArg,
                     double* padfExtent, int nPixels, int nLines,
                     double dfPixelSizeX, double dfPixelSizeY)
 {
    int nSamplePoints;
    double dfRatio;
    int bErr;
    int nBadCount;
    int    abSuccess[21] = { 0 };
    double adfX[21] = { 0 };
    double adfY[21] = { 0 };
    double adfZ[21] = { 0 };
    
    //double dfMinXOut = padfExtent[0];
    //double dfMinYOut = padfExtent[1];
    double dfMaxXOut = padfExtent[2];
    double dfMaxYOut = padfExtent[3];
    
    // Take 20 steps 
    nSamplePoints = 0;
    for( dfRatio = 0.0; dfRatio <= 1.01; dfRatio += 0.05 )
    {
        // Ensure we end exactly at the end.
        if( dfRatio > 0.99 )
            dfRatio = 1.0;

        // Along right
        adfX[nSamplePoints]   = dfMaxXOut;
        adfY[nSamplePoints]   = dfMaxYOut - dfPixelSizeY * dfRatio * nLines;
        adfZ[nSamplePoints++] = 0.0;
    }
    
    bErr = FALSE;
    if( !pfnTransformer( pTransformArg, TRUE, nSamplePoints, 
                             adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = TRUE;
    }
    
    if( !bErr && !pfnTransformer( pTransformArg, FALSE, nSamplePoints, 
                             adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = TRUE;
    }
    
    nSamplePoints = 0;
    nBadCount = 0;
    for( dfRatio = 0.0; !bErr && dfRatio <= 1.01; dfRatio += 0.05 )
    {
        double expected_x = dfMaxXOut;
        double expected_y = dfMaxYOut - dfPixelSizeY * dfRatio * nLines;
        if (fabs(adfX[nSamplePoints] -  expected_x) > dfPixelSizeX ||
            fabs(adfY[nSamplePoints] -  expected_y) > dfPixelSizeY)
            nBadCount ++;
        nSamplePoints ++;
    }
    
    return (nBadCount == nSamplePoints);
}


 static int GDALSuggestedWarpOutput2_MustAdjustForBottomBorder(
                     GDALTransformerFunc pfnTransformer, void *pTransformArg,
                     double* padfExtent, int nPixels, int nLines,
                     double dfPixelSizeX, double dfPixelSizeY)
 {
    int nSamplePoints;
    double dfRatio;
    int bErr;
    int nBadCount;
    int    abSuccess[21] = { 0 };
    double adfX[21] = { 0 };
    double adfY[21] = { 0 };
    double adfZ[21] = { 0 };
    
    double dfMinXOut = padfExtent[0];
    double dfMinYOut = padfExtent[1];
    //double dfMaxXOut = padfExtent[2];
    //double dfMaxYOut = padfExtent[3];
    
    // Take 20 steps 
    nSamplePoints = 0;
    for( dfRatio = 0.0; dfRatio <= 1.01; dfRatio += 0.05 )
    {
        // Ensure we end exactly at the end.
        if( dfRatio > 0.99 )
            dfRatio = 1.0;

        // Along right
        adfX[nSamplePoints]   = dfMinXOut + dfPixelSizeX * dfRatio * nPixels;
        adfY[nSamplePoints]   = dfMinYOut;
        adfZ[nSamplePoints++] = 0.0;
    }
    
    bErr = FALSE;
    if( !pfnTransformer( pTransformArg, TRUE, nSamplePoints, 
                             adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = TRUE;
    }
    
    if( !bErr && !pfnTransformer( pTransformArg, FALSE, nSamplePoints, 
                             adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = TRUE;
    }
    
    nSamplePoints = 0;
    nBadCount = 0;
    for( dfRatio = 0.0; !bErr && dfRatio <= 1.01; dfRatio += 0.05 )
    {
        double expected_x = dfMinXOut + dfPixelSizeX * dfRatio * nPixels;
        double expected_y = dfMinYOut;
        if (fabs(adfX[nSamplePoints] -  expected_x) > dfPixelSizeX ||
            fabs(adfY[nSamplePoints] -  expected_y) > dfPixelSizeY)
            nBadCount ++;
        nSamplePoints ++;
    }
    
    return (nBadCount == nSamplePoints);
}

/************************************************************************/
/*                      GDALSuggestedWarpOutput2()                      */
/************************************************************************/

/**
 * Suggest output file size.
 *
 * This function is used to suggest the size, and georeferenced extents
 * appropriate given the indicated transformation and input file.  It walks
 * the edges of the input file (approximately 20 sample points along each
 * edge) transforming into output coordinates in order to get an extents box.
 *
 * Then a resolution is computed with the intent that the length of the
 * distance from the top left corner of the output imagery to the bottom right
 * corner would represent the same number of pixels as in the source image. 
 * Note that if the image is somewhat rotated the diagonal taken isnt of the
 * whole output bounding rectangle, but instead of the locations where the
 * top/left and bottom/right corners transform.  The output pixel size is 
 * always square.  This is intended to approximately preserve the resolution
 * of the input data in the output file. 
 * 
 * The values returned in padfGeoTransformOut, pnPixels and pnLines are
 * the suggested number of pixels and lines for the output file, and the
 * geotransform relating those pixels to the output georeferenced coordinates.
 *
 * The trickiest part of using the function is ensuring that the 
 * transformer created is from source file pixel/line coordinates to 
 * output file georeferenced coordinates.  This can be accomplished with 
 * GDALCreateGenImgProjTransformer() by passing a NULL for the hDstDS.  
 *
 * @param hSrcDS the input image (it is assumed the whole input images is
 * being transformed). 
 * @param pfnTransformer the transformer function.
 * @param pTransformArg the callback data for the transformer function.
 * @param padfGeoTransformOut the array of six doubles in which the suggested
 * geotransform is returned. 
 * @param pnPixels int in which the suggest pixel width of output is returned.
 * @param pnLines int in which the suggest pixel height of output is returned.
 * @param padfExtent Four entry array to return extents as (xmin, ymin, xmax, ymax). 
 * @param nOptions Options, currently always zero.
 *
 * @return CE_None if successful or CE_Failure otherwise. 
 */

CPLErr CPL_STDCALL
GDALSuggestedWarpOutput2( GDALDatasetH hSrcDS, 
                          GDALTransformerFunc pfnTransformer, 
                          void *pTransformArg, 
                          double *padfGeoTransformOut, 
                          int *pnPixels, int *pnLines,
                          double *padfExtent, int nOptions )

{
    VALIDATE_POINTER1( hSrcDS, "GDALSuggestedWarpOutput2", CE_Failure );

/* -------------------------------------------------------------------- */
/*      Setup sample points all around the edge of the input raster.    */
/* -------------------------------------------------------------------- */
    int    nSamplePoints = 0;
    int    nInXSize = GDALGetRasterXSize( hSrcDS );
    int    nInYSize = GDALGetRasterYSize( hSrcDS );

    if (pfnTransformer == GDALGenImgProjTransform)
    {
        /* In case CHECK_WITH_INVERT_PROJ has been modified */
        GDALRefreshGenImgProjTransformer(pTransformArg);
    }

#define N_PIXELSTEP 50
    int nSteps = (int) (double(MIN(nInYSize, nInXSize)) / N_PIXELSTEP + .5);
    if (nSteps < 20)
        nSteps = 20;

retry:
    int nSampleMax = (nSteps + 1)*(nSteps + 1);
    int *pabSuccess = NULL;
    double *padfX, *padfY, *padfZ;
    double *padfXRevert, *padfYRevert, *padfZRevert;

    double dfRatio = 0.0;
    double dfStep = 1. / nSteps;

    pabSuccess = (int *) VSIMalloc3(sizeof(int), nSteps + 1, nSteps + 1);
    padfX = (double *) VSIMalloc3(sizeof(double) * 3, nSteps + 1, nSteps + 1);
    padfXRevert = (double *) VSIMalloc3(sizeof(double) * 3, nSteps + 1, nSteps + 1);
    if (pabSuccess == NULL || padfX == NULL || padfXRevert == NULL)
    {
        CPLFree( padfX );
        CPLFree( padfXRevert );
        CPLFree( pabSuccess );
        if (nSteps > 20)
        {
            nSteps = 20;
            goto retry;
        }
        return CE_Failure;
    }
    padfY = padfX + nSampleMax;
    padfZ = padfX + nSampleMax * 2;
    padfYRevert = padfXRevert + nSampleMax;
    padfZRevert = padfXRevert + nSampleMax * 2;


    // Take N_STEPS steps
    int iStep;
    for( iStep = 0; iStep <= nSteps; iStep ++ )
    {
        dfRatio = (iStep == nSteps) ? 1.0 : iStep * dfStep;

        // Along top 
        padfX[nSamplePoints]   = dfRatio * nInXSize;
        padfY[nSamplePoints]   = 0.0;
        padfZ[nSamplePoints++] = 0.0;

        // Along bottom 
        padfX[nSamplePoints]   = dfRatio * nInXSize;
        padfY[nSamplePoints]   = nInYSize;
        padfZ[nSamplePoints++] = 0.0;

        // Along left
        padfX[nSamplePoints]   = 0.0;
        padfY[nSamplePoints] = dfRatio * nInYSize;
        padfZ[nSamplePoints++] = 0.0;

        // Along right
        padfX[nSamplePoints]   = nInXSize;
        padfY[nSamplePoints] = dfRatio * nInYSize;
        padfZ[nSamplePoints++] = 0.0;
    }

    CPLAssert( nSamplePoints == 4 * (nSteps + 1) );

    memset( pabSuccess, 1, sizeof(int) * nSampleMax );

/* -------------------------------------------------------------------- */
/*      Transform them to the output coordinate system.                 */
/* -------------------------------------------------------------------- */
    int    nFailedCount = 0, i;

    if( !pfnTransformer( pTransformArg, FALSE, nSamplePoints, 
                         padfX, padfY, padfZ, pabSuccess ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GDALSuggestedWarpOutput() failed because the passed\n"
                  "transformer failed." );
        CPLFree( padfX );
        CPLFree( padfXRevert );
        CPLFree( pabSuccess );
        return CE_Failure;
    }

    for( i = 0; i < nSamplePoints; i++ )
    {
        if( !pabSuccess[i] )
            nFailedCount++;
    }
    
/* -------------------------------------------------------------------- */
/*      Check if the computed target coordinates are revertable.        */
/*      If not, try the detailed grid sampling.                         */
/* -------------------------------------------------------------------- */
    if (nFailedCount == 0 )
    {
        memcpy(padfXRevert, padfX, nSamplePoints * sizeof(double));
        memcpy(padfYRevert, padfY, nSamplePoints * sizeof(double));
        memcpy(padfZRevert, padfZ, nSamplePoints * sizeof(double));
        if( !pfnTransformer( pTransformArg, TRUE, nSamplePoints, 
                             padfXRevert, padfYRevert, padfZRevert, pabSuccess ) )
        {
            nFailedCount = 1;
        }
        else
        {
            for( i = 0; nFailedCount == 0 && i < nSamplePoints; i++ )
            {
                if( !pabSuccess[i] )
                    nFailedCount++;

                dfRatio = 0.0 + (i/4) * dfStep;
                if (dfRatio>0.99)
                    dfRatio = 1.0;

                double dfExpectedX, dfExpectedY;
                if ((i % 4) == 0)
                {
                    dfExpectedX   = dfRatio * nInXSize;
                    dfExpectedY   = 0.0;
                }
                else if ((i % 4) == 1)
                {
                    dfExpectedX   = dfRatio * nInXSize;
                    dfExpectedY   = nInYSize;
                }
                else if ((i % 4) == 2)
                {
                    dfExpectedX   = 0.0;
                    dfExpectedY   = dfRatio * nInYSize;
                }
                else
                {
                    dfExpectedX   = nInXSize;
                    dfExpectedY   = dfRatio * nInYSize;
                }
                
                if (fabs(padfXRevert[i] - dfExpectedX) > nInXSize / nSteps ||
                    fabs(padfYRevert[i] - dfExpectedY) > nInYSize / nSteps)
                    nFailedCount ++;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If any of the edge points failed to transform, we need to       */
/*      build a fairly detailed internal grid of points instead to      */
/*      help identify the area that is transformable.                   */
/* -------------------------------------------------------------------- */
    if( nFailedCount > 0 )
    {
        int iStep2;
        double dfRatio2;
        nSamplePoints = 0;

        // Take N_STEPS steps 
        for( iStep = 0; iStep <= nSteps; iStep ++ )
        {
            dfRatio = (iStep == nSteps) ? 1.0 : iStep * dfStep;

            for( iStep2 = 0; iStep2 <= nSteps; iStep2 ++ )
            {
                dfRatio2 = (iStep2 == nSteps) ? 1.0 : iStep2 * dfStep;

                // Along top 
                padfX[nSamplePoints]   = dfRatio2 * nInXSize;
                padfY[nSamplePoints]   = dfRatio * nInYSize;
                padfZ[nSamplePoints++] = 0.0;
            }
        }

        CPLAssert( nSamplePoints == nSampleMax );

        if( !pfnTransformer( pTransformArg, FALSE, nSamplePoints, 
                             padfX, padfY, padfZ, pabSuccess ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "GDALSuggestedWarpOutput() failed because the passed\n"
                      "transformer failed." );

            CPLFree( padfX );
            CPLFree( padfXRevert );
            CPLFree( pabSuccess );

            return CE_Failure;
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Collect the bounds, ignoring any failed points.                 */
/* -------------------------------------------------------------------- */
    double dfMinXOut=0, dfMinYOut=0, dfMaxXOut=0, dfMaxYOut=0;
    int    bGotInitialPoint = FALSE;

    nFailedCount = 0;
    for( i = 0; i < nSamplePoints; i++ )
    {
        
        int x_i = i % (nSteps + 1);
        int y_i = i / (nSteps + 1);

        if (x_i > 0 && (pabSuccess[i-1] || pabSuccess[i]))
        {
            double x_out_before = padfX[i-1];
            double x_out_after = padfX[i];
            int nIter = 0;
            double x_in_before = (x_i - 1) * nInXSize * 1.0 / nSteps;
            double x_in_after = x_i * nInXSize * 1.0 / nSteps;
            int valid_before = pabSuccess[i-1];
            int valid_after = pabSuccess[i];
            
            /* Detect discontinuity in target coordinates when the target x coordinates */
            /* change sign. This may be a false positive when the targe tx is around 0 */
            /* Dichotomic search to reduce the interval to near the discontinuity and */
            /* get a better out extent */
            while ( (!valid_before || !valid_after ||
                     x_out_before * x_out_after < 0) && nIter < 16 )
            {
                double x = (x_in_before + x_in_after) / 2;
                double y = y_i * nInYSize * 1.0 / nSteps;
                double z= 0;
                //fprintf(stderr, "[%d] (%f, %f) -> ", nIter, x, y);
                int bSuccess = TRUE;
                if( !pfnTransformer( pTransformArg, FALSE, 1, 
                                     &x, &y, &z, &bSuccess ) || !bSuccess )
                {
                    //fprintf(stderr, "invalid\n");
                    if (!valid_before)
                    {
                        x_in_before = (x_in_before + x_in_after) / 2;
                    }
                    else if (!valid_after)
                    {
                        x_in_after = (x_in_before + x_in_after) / 2;
                    }
                    else
                        break;
                }
                else
                {
                    //fprintf(stderr, "(%f, %f)\n", x, y);
                    
                    if( !bGotInitialPoint )
                    {
                        bGotInitialPoint = TRUE;
                        dfMinXOut = dfMaxXOut = x;
                        dfMinYOut = dfMaxYOut = y;
                    }
                    else
                    {
                        dfMinXOut = MIN(dfMinXOut,x);
                        dfMinYOut = MIN(dfMinYOut,y);
                        dfMaxXOut = MAX(dfMaxXOut,x);
                        dfMaxYOut = MAX(dfMaxYOut,y);
                    }
                    
                    if (!valid_before || x_out_before * x < 0)
                    {
                        valid_after = TRUE;
                        x_in_after = (x_in_before + x_in_after) / 2;
                        x_out_after = x;
                    }
                    else
                    {
                        valid_before = TRUE;
                        x_out_before = x;
                        x_in_before = (x_in_before + x_in_after) / 2;
                    }
                }
                nIter ++;
            }
        }
        
        if( !pabSuccess[i] )
        {
            nFailedCount++;
            continue;
        }

        if( !bGotInitialPoint )
        {
            bGotInitialPoint = TRUE;
            dfMinXOut = dfMaxXOut = padfX[i];
            dfMinYOut = dfMaxYOut = padfY[i];
        }
        else
        {
            dfMinXOut = MIN(dfMinXOut, padfX[i]);
            dfMinYOut = MIN(dfMinYOut, padfY[i]);
            dfMaxXOut = MAX(dfMaxXOut, padfX[i]);
            dfMaxYOut = MAX(dfMaxYOut, padfY[i]);
        }
    }

    if( nFailedCount > nSamplePoints - 10 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Too many points (%d out of %d) failed to transform,\n"
                  "unable to compute output bounds.",
                  nFailedCount, nSamplePoints );

        CPLFree( padfX );
        CPLFree( padfXRevert );
        CPLFree( pabSuccess );

        return CE_Failure;
    }

    if( nFailedCount > 0 )
        CPLDebug( "GDAL", 
                  "GDALSuggestedWarpOutput(): %d out of %d points failed to transform.", 
                  nFailedCount, nSamplePoints );

/* -------------------------------------------------------------------- */
/*      Compute the distance in "georeferenced" units from the top      */
/*      corner of the transformed input image to the bottom left        */
/*      corner of the transformed input.  Use this distance to          */
/*      compute an approximate pixel size in the output                 */
/*      georeferenced coordinates.                                      */
/* -------------------------------------------------------------------- */
    double dfDiagonalDist, dfDeltaX, dfDeltaY;

    if( pabSuccess[0] && pabSuccess[nSamplePoints - 1] )
    {
        dfDeltaX = padfX[nSamplePoints-1] - padfX[0];
        dfDeltaY = padfY[nSamplePoints-1] - padfY[0];
    }
    else
    {
        dfDeltaX = dfMaxXOut - dfMinXOut;
        dfDeltaY = dfMaxYOut - dfMinYOut;
    }

    dfDiagonalDist = sqrt( dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY );
    
/* -------------------------------------------------------------------- */
/*      Compute a pixel size from this.                                 */
/* -------------------------------------------------------------------- */
    double dfPixelSize;

    dfPixelSize = dfDiagonalDist 
        / sqrt(((double)nInXSize)*nInXSize + ((double)nInYSize)*nInYSize);

    *pnPixels = (int) ((dfMaxXOut - dfMinXOut) / dfPixelSize + 0.5);
    *pnLines = (int) ((dfMaxYOut - dfMinYOut) / dfPixelSize + 0.5);
    
    double dfPixelSizeX = dfPixelSize;
    double dfPixelSizeY = dfPixelSize;
   
    double adfExtent[4];
    const double adfRatioArray[] = { 0, 0.001, 0.01, 0.1, 1 };
    size_t nRetry;
    
#define N_ELEMENTS(x) (sizeof(x) / sizeof(x[0]))

/* -------------------------------------------------------------------- */
/*      Check that the right border is not completely out of source     */
/*      image. If so, adjust the x pixel size a bit in the hope it will */
/*      fit.                                                            */
/* -------------------------------------------------------------------- */
    for( nRetry = 0; nRetry < N_ELEMENTS(adfRatioArray); nRetry ++ )
    {
        double dfTryPixelSizeX =
            dfPixelSizeX - dfPixelSizeX * adfRatioArray[nRetry] / *pnPixels;
        adfExtent[0] = dfMinXOut;
        adfExtent[1] = dfMaxYOut - (*pnLines) * dfPixelSizeY;
        adfExtent[2] = dfMinXOut + (*pnPixels) * dfTryPixelSizeX;
        adfExtent[3] = dfMaxYOut;
        if (!GDALSuggestedWarpOutput2_MustAdjustForRightBorder(
                                            pfnTransformer, pTransformArg,
                                            adfExtent, *pnPixels,  *pnLines,
                                            dfTryPixelSizeX, dfPixelSizeY))
        {
            dfPixelSizeX = dfTryPixelSizeX;
            break;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Check that the bottom border is not completely out of source    */
/*      image. If so, adjust the y pixel size a bit in the hope it will */
/*      fit.                                                            */
/* -------------------------------------------------------------------- */
    for( nRetry = 0; nRetry < N_ELEMENTS(adfRatioArray); nRetry ++ )
    {
        double dfTryPixelSizeY =
            dfPixelSizeY - dfPixelSizeY * adfRatioArray[nRetry] / *pnLines;
        adfExtent[0] = dfMinXOut;
        adfExtent[1] = dfMaxYOut - (*pnLines) * dfTryPixelSizeY;
        adfExtent[2] = dfMinXOut + (*pnPixels) * dfPixelSizeX;
        adfExtent[3] = dfMaxYOut;
        if (!GDALSuggestedWarpOutput2_MustAdjustForBottomBorder(
                                            pfnTransformer, pTransformArg,
                                            adfExtent, *pnPixels,  *pnLines,
                                            dfPixelSizeX, dfTryPixelSizeY))
        {
            dfPixelSizeY = dfTryPixelSizeY;
            break;
        }
    }
    
    
/* -------------------------------------------------------------------- */
/*      Recompute some bounds so that all return values are consistant  */
/* -------------------------------------------------------------------- */
    dfMaxXOut = dfMinXOut + (*pnPixels) * dfPixelSizeX;
    dfMinYOut = dfMaxYOut - (*pnLines) * dfPixelSizeY;
    
    /* -------------------------------------------------------------------- */
    /*      Return raw extents.                                             */
    /* -------------------------------------------------------------------- */
    padfExtent[0] = dfMinXOut;
    padfExtent[1] = dfMinYOut;
    padfExtent[2] = dfMaxXOut;
    padfExtent[3] = dfMaxYOut;

    /* -------------------------------------------------------------------- */
    /*      Set the output geotransform.                                    */
    /* -------------------------------------------------------------------- */
    padfGeoTransformOut[0] = dfMinXOut;
    padfGeoTransformOut[1] = dfPixelSizeX;
    padfGeoTransformOut[2] = 0.0;
    padfGeoTransformOut[3] = dfMaxYOut;
    padfGeoTransformOut[4] = 0.0;
    padfGeoTransformOut[5] = - dfPixelSizeY;
    
    CPLFree( padfX );
    CPLFree( padfXRevert );
    CPLFree( pabSuccess );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*			 GDALGenImgProjTransformer                      */
/* ==================================================================== */
/************************************************************************/

typedef struct {

    GDALTransformerInfo sTI;

    double   adfSrcGeoTransform[6];
    double   adfSrcInvGeoTransform[6];

    void     *pSrcGCPTransformArg;
    void     *pSrcRPCTransformArg;
    void     *pSrcTPSTransformArg;
    void     *pSrcGeoLocTransformArg;

    void     *pReprojectArg;

    double   adfDstGeoTransform[6];
    double   adfDstInvGeoTransform[6];
    
    void     *pDstGCPTransformArg;

} GDALGenImgProjTransformInfo;

/************************************************************************/
/*                  GDALCreateGenImgProjTransformer()                   */
/************************************************************************/

/**
 * Create image to image transformer.
 *
 * This function creates a transformation object that maps from pixel/line
 * coordinates on one image to pixel/line coordinates on another image.  The
 * images may potentially be georeferenced in different coordinate systems, 
 * and may used GCPs to map between their pixel/line coordinates and 
 * georeferenced coordinates (as opposed to the default assumption that their
 * geotransform should be used). 
 *
 * This transformer potentially performs three concatenated transformations.
 *
 * The first stage is from source image pixel/line coordinates to source
 * image georeferenced coordinates, and may be done using the geotransform, 
 * or if not defined using a polynomial model derived from GCPs.  If GCPs
 * are used this stage is accomplished using GDALGCPTransform(). 
 *
 * The second stage is to change projections from the source coordinate system
 * to the destination coordinate system, assuming they differ.  This is 
 * accomplished internally using GDALReprojectionTransform().
 *
 * The third stage is converting from destination image georeferenced
 * coordinates to destination image coordinates.  This is done using the
 * destination image geotransform, or if not available, using a polynomial 
 * model derived from GCPs. If GCPs are used this stage is accomplished using 
 * GDALGCPTransform().  This stage is skipped if hDstDS is NULL when the
 * transformation is created. 
 * 
 * @param hSrcDS source dataset, or NULL.
 * @param pszSrcWKT the coordinate system for the source dataset.  If NULL, 
 * it will be read from the dataset itself. 
 * @param hDstDS destination dataset (or NULL). 
 * @param pszDstWKT the coordinate system for the destination dataset.  If
 * NULL, and hDstDS not NULL, it will be read from the destination dataset.
 * @param bGCPUseOK TRUE if GCPs should be used if the geotransform is not
 * available on the source dataset (not destination).
 * @param dfGCPErrorThreshold ignored/deprecated.
 * @param nOrder the maximum order to use for GCP derived polynomials if 
 * possible.  Use 0 to autoselect, or -1 for thin plate splines.
 * 
 * @return handle suitable for use GDALGenImgProjTransform(), and to be
 * deallocated with GDALDestroyGenImgProjTransformer().
 */

void *
GDALCreateGenImgProjTransformer( GDALDatasetH hSrcDS, const char *pszSrcWKT,
                                 GDALDatasetH hDstDS, const char *pszDstWKT,
                                 int bGCPUseOK, double dfGCPErrorThreshold,
                                 int nOrder )

{
    char **papszOptions = NULL;
    void *pRet;

    if( pszSrcWKT != NULL )
        papszOptions = CSLSetNameValue( papszOptions, "SRC_SRS", pszSrcWKT );
    if( pszDstWKT != NULL )
        papszOptions = CSLSetNameValue( papszOptions, "DST_SRS", pszDstWKT );
    if( !bGCPUseOK )
        papszOptions = CSLSetNameValue( papszOptions, "GCPS_OK", "FALSE" );
    if( nOrder != 0 )
        papszOptions = CSLSetNameValue( papszOptions, "MAX_GCP_ORDER", 
                                        CPLString().Printf("%d",nOrder) );

    pRet = GDALCreateGenImgProjTransformer2( hSrcDS, hDstDS, papszOptions );
    CSLDestroy( papszOptions );

    return pRet;
}



/************************************************************************/
/*                          InsertCenterLong()                          */
/*                                                                      */
/*      Insert a CENTER_LONG Extension entry on a GEOGCS to indicate    */
/*      the center longitude of the dataset for wrapping purposes.      */
/************************************************************************/

static CPLString InsertCenterLong( GDALDatasetH hDS, CPLString osWKT )

{								        
    if( !EQUALN(osWKT.c_str(), "GEOGCS[", 7) )
        return osWKT;
    
    if( strstr(osWKT,"EXTENSION[\"CENTER_LONG") != NULL )
        return osWKT;

/* -------------------------------------------------------------------- */
/*      For now we only do this if we have a geotransform since         */
/*      other forms require a bunch of extra work.                      */
/* -------------------------------------------------------------------- */
    double   adfGeoTransform[6];

    if( GDALGetGeoTransform( hDS, adfGeoTransform ) != CE_None )
        return osWKT;

/* -------------------------------------------------------------------- */
/*      Compute min/max longitude based on testing the four corners.    */
/* -------------------------------------------------------------------- */
    double dfMinLong, dfMaxLong;
    int nXSize = GDALGetRasterXSize( hDS );
    int nYSize = GDALGetRasterYSize( hDS );

    dfMinLong = 
        MIN(MIN(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + 0 * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + 0 * adfGeoTransform[2]),
            MIN(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2]));
    dfMaxLong = 
        MAX(MAX(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + 0 * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + 0 * adfGeoTransform[2]),
            MAX(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2]));

    if( dfMaxLong - dfMinLong > 360.0 )
        return osWKT;

/* -------------------------------------------------------------------- */
/*      Insert center long.                                             */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS( osWKT );
    double dfCenterLong = (dfMaxLong + dfMinLong) / 2.0;
    OGR_SRSNode *poExt;

    poExt  = new OGR_SRSNode( "EXTENSION" );
    poExt->AddChild( new OGR_SRSNode( "CENTER_LONG" ) );
    poExt->AddChild( new OGR_SRSNode( CPLString().Printf("%g",dfCenterLong) ));
    
    oSRS.GetRoot()->AddChild( poExt->Clone() );
    delete poExt;

/* -------------------------------------------------------------------- */
/*      Convert back to wkt.                                            */
/* -------------------------------------------------------------------- */
    char *pszWKT = NULL;
    oSRS.exportToWkt( &pszWKT );
    
    osWKT = pszWKT;
    CPLFree( pszWKT );

    return osWKT;
}

/************************************************************************/
/*                  GDALCreateGenImgProjTransformer2()                  */
/************************************************************************/

/**
 * Create image to image transformer.
 *
 * This function creates a transformation object that maps from pixel/line
 * coordinates on one image to pixel/line coordinates on another image.  The
 * images may potentially be georeferenced in different coordinate systems, 
 * and may used GCPs to map between their pixel/line coordinates and 
 * georeferenced coordinates (as opposed to the default assumption that their
 * geotransform should be used). 
 *
 * This transformer potentially performs three concatenated transformations.
 *
 * The first stage is from source image pixel/line coordinates to source
 * image georeferenced coordinates, and may be done using the geotransform, 
 * or if not defined using a polynomial model derived from GCPs.  If GCPs
 * are used this stage is accomplished using GDALGCPTransform(). 
 *
 * The second stage is to change projections from the source coordinate system
 * to the destination coordinate system, assuming they differ.  This is 
 * accomplished internally using GDALReprojectionTransform().
 *
 * The third stage is converting from destination image georeferenced
 * coordinates to destination image coordinates.  This is done using the
 * destination image geotransform, or if not available, using a polynomial 
 * model derived from GCPs. If GCPs are used this stage is accomplished using 
 * GDALGCPTransform().  This stage is skipped if hDstDS is NULL when the
 * transformation is created. 
 *
 * Supported Options:
 * <ul>
 * <li> SRC_SRS: WKT SRS to be used as an override for hSrcDS.
 * <li> DST_SRS: WKT SRS to be used as an override for hDstDS.
 * <li> GCPS_OK: If false, GCPs will not be used, default is TRUE. 
 * <li> REFINE_MINIMUM_GCPS: The minimum amount of GCPs that should be available after the refinement.
 * <li> REFINE_TOLERANCE: The tolernace that specifies when a GCP will be eliminated.
 * <li> MAX_GCP_ORDER: the maximum order to use for GCP derived polynomials if
 * possible.  The default is to autoselect based on the number of GCPs.  
 * A value of -1 triggers use of Thin Plate Spline instead of polynomials.
 * <li> METHOD: may have a value which is one of GEOTRANSFORM, GCP_POLYNOMIAL,
 * GCP_TPS, GEOLOC_ARRAY, RPC to force only one geolocation method to be
 * considered on the source dataset. 
 * <li> RPC_HEIGHT: A fixed height to be used with RPC calculations.
 * <li> RPC_DEM: The name of a DEM file to be used with RPC calculations.
 * <li> INSERT_CENTER_LONG: May be set to FALSE to disable setting up a 
 * CENTER_LONG value on the coordinate system to rewrap things around the
 * center of the image.  
 * </ul>
 * 
 * @param hSrcDS source dataset, or NULL.
 * @param hDstDS destination dataset (or NULL).
 * @param papszOptions NULL-terminated list of string options (or NULL).
 * 
 * @return handle suitable for use GDALGenImgProjTransform(), and to be
 * deallocated with GDALDestroyGenImgProjTransformer() or NULL on failure.
 */

void *
GDALCreateGenImgProjTransformer2( GDALDatasetH hSrcDS, GDALDatasetH hDstDS, 
                                  char **papszOptions )

{
    GDALGenImgProjTransformInfo *psInfo;
    char **papszMD;
    GDALRPCInfo sRPCInfo;
    const char *pszMethod = CSLFetchNameValue( papszOptions, "METHOD" );
    const char *pszValue;
    int nOrder = 0, bGCPUseOK = TRUE, nMinimumGcps = -1, bRefine = FALSE;
    double dfTolerance = 0.0;
    const char *pszSrcWKT = CSLFetchNameValue( papszOptions, "SRC_SRS" );
    const char *pszDstWKT = CSLFetchNameValue( papszOptions, "DST_SRS" );

    pszValue = CSLFetchNameValue( papszOptions, "MAX_GCP_ORDER" );
    if( pszValue )
        nOrder = atoi(pszValue);

    pszValue = CSLFetchNameValue( papszOptions, "GCPS_OK" );
    if( pszValue )
        bGCPUseOK = CSLTestBoolean(pszValue);

    pszValue = CSLFetchNameValue( papszOptions, "REFINE_MINIMUM_GCPS" );
    if( pszValue )
    {
        if( atoi(pszValue) != -1)
            nMinimumGcps = atoi(pszValue);
    }

    pszValue = CSLFetchNameValue( papszOptions, "REFINE_TOLERANCE" );
    if( pszValue )
    {
        dfTolerance = atof(pszValue);
        bRefine = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    psInfo = (GDALGenImgProjTransformInfo *) 
        CPLCalloc(sizeof(GDALGenImgProjTransformInfo),1);

    strcpy( psInfo->sTI.szSignature, "GTI" );
    psInfo->sTI.pszClassName = "GDALGenImgProjTransformer";
    psInfo->sTI.pfnTransform = GDALGenImgProjTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyGenImgProjTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeGenImgProjTransformer;

/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for the source image.      */
/* -------------------------------------------------------------------- */
    if( hSrcDS == NULL )
    {
        psInfo->adfSrcGeoTransform[0] = 0.0;
        psInfo->adfSrcGeoTransform[1] = 1.0;
        psInfo->adfSrcGeoTransform[2] = 0.0;
        psInfo->adfSrcGeoTransform[3] = 0.0;
        psInfo->adfSrcGeoTransform[4] = 0.0;
        psInfo->adfSrcGeoTransform[5] = 1.0;
        memcpy( psInfo->adfSrcInvGeoTransform, psInfo->adfSrcGeoTransform,
                sizeof(double) * 6 );
    }

    else if( (pszMethod == NULL || EQUAL(pszMethod,"GEOTRANSFORM"))
             && GDALGetGeoTransform( hSrcDS, psInfo->adfSrcGeoTransform ) 
             == CE_None
             && (psInfo->adfSrcGeoTransform[0] != 0.0
                 || psInfo->adfSrcGeoTransform[1] != 1.0
                 || psInfo->adfSrcGeoTransform[2] != 0.0
                 || psInfo->adfSrcGeoTransform[3] != 0.0
                 || psInfo->adfSrcGeoTransform[4] != 0.0
                 || ABS(psInfo->adfSrcGeoTransform[5]) != 1.0) )
    {
        GDALInvGeoTransform( psInfo->adfSrcGeoTransform, 
                             psInfo->adfSrcInvGeoTransform );
        if( pszSrcWKT == NULL )
            pszSrcWKT = GDALGetProjectionRef( hSrcDS );
    }

    else if( bGCPUseOK 
             && (pszMethod == NULL || EQUAL(pszMethod,"GCP_POLYNOMIAL") )
             && GDALGetGCPCount( hSrcDS ) > 0 && nOrder >= 0 )
    {
        if(bRefine)
        {
                psInfo->pSrcGCPTransformArg = 
                    GDALCreateGCPRefineTransformer( GDALGetGCPCount( hSrcDS ),
                                                    GDALGetGCPs( hSrcDS ), nOrder, 
                                                    FALSE, dfTolerance, nMinimumGcps );
        }
        else
        {
            psInfo->pSrcGCPTransformArg = 
                GDALCreateGCPTransformer( GDALGetGCPCount( hSrcDS ),
                                          GDALGetGCPs( hSrcDS ), nOrder, 
                                          FALSE );
        }

        if( psInfo->pSrcGCPTransformArg == NULL )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return NULL;
        }

        if( pszSrcWKT == NULL )
            pszSrcWKT = GDALGetGCPProjection( hSrcDS );
    }

    else if( bGCPUseOK 
             && GDALGetGCPCount( hSrcDS ) > 0 
             && nOrder <= 0
             && (pszMethod == NULL || EQUAL(pszMethod,"GCP_TPS")) )
    {
        psInfo->pSrcTPSTransformArg = 
            GDALCreateTPSTransformer( GDALGetGCPCount( hSrcDS ),
                                      GDALGetGCPs( hSrcDS ), FALSE );
        if( psInfo->pSrcTPSTransformArg == NULL )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return NULL;
        }

        if( pszSrcWKT == NULL )
            pszSrcWKT = GDALGetGCPProjection( hSrcDS );
    }

    else if( (pszMethod == NULL || EQUAL(pszMethod,"RPC"))
             && (papszMD = GDALGetMetadata( hSrcDS, "RPC" )) != NULL
             && GDALExtractRPCInfo( papszMD, &sRPCInfo ) )
    {
        psInfo->pSrcRPCTransformArg = 
            GDALCreateRPCTransformer( &sRPCInfo, FALSE, 0.1, papszOptions );
        if( psInfo->pSrcRPCTransformArg == NULL )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return NULL;
        }
        if( pszSrcWKT == NULL )
            pszSrcWKT = SRS_WKT_WGS84;
    }

    else if( (pszMethod == NULL || EQUAL(pszMethod,"GEOLOC_ARRAY"))
             && (papszMD = GDALGetMetadata( hSrcDS, "GEOLOCATION" )) != NULL )
    {
        psInfo->pSrcGeoLocTransformArg = 
            GDALCreateGeoLocTransformer( hSrcDS, papszMD, FALSE );

        if( psInfo->pSrcGeoLocTransformArg == NULL )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return NULL;
        }
        if( pszSrcWKT == NULL )
            pszSrcWKT = CSLFetchNameValue( papszMD, "SRS" );
    }

    else if( pszMethod != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to compute a %s based transformation between pixel/line\n"
                  "and georeferenced coordinates for %s.\n", 
                  pszMethod, GDALGetDescription( hSrcDS ) );

        GDALDestroyGenImgProjTransformer( psInfo );
        return NULL;
    }

    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to compute a transformation between pixel/line\n"
                  "and georeferenced coordinates for %s.\n"
                  "There is no affine transformation and no GCPs.", 
                  GDALGetDescription( hSrcDS ) );

        GDALDestroyGenImgProjTransformer( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup reprojection.                                             */
/* -------------------------------------------------------------------- */
    if( pszDstWKT == NULL && hDstDS != NULL )
        pszDstWKT = GDALGetProjectionRef( hDstDS );

    if( pszSrcWKT != NULL && strlen(pszSrcWKT) > 0 
        && pszDstWKT != NULL && strlen(pszDstWKT) > 0 
        && !EQUAL(pszSrcWKT,pszDstWKT) )
    {
        CPLString osSrcWKT = pszSrcWKT;
        if (hSrcDS 
            && CSLFetchBoolean( papszOptions, "INSERT_CENTER_LONG", TRUE ) )
            osSrcWKT = InsertCenterLong( hSrcDS, osSrcWKT );
        
        psInfo->pReprojectArg = 
            GDALCreateReprojectionTransformer( osSrcWKT.c_str(), pszDstWKT );
    }
        
/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for destination image.     */
/*      If we have no destination use a unit transform.                 */
/* -------------------------------------------------------------------- */
    if( hDstDS )
    {
        GDALGetGeoTransform( hDstDS, psInfo->adfDstGeoTransform );
        GDALInvGeoTransform( psInfo->adfDstGeoTransform, 
                             psInfo->adfDstInvGeoTransform );
    }
    else
    {
        psInfo->adfDstGeoTransform[0] = 0.0;
        psInfo->adfDstGeoTransform[1] = 1.0;
        psInfo->adfDstGeoTransform[2] = 0.0;
        psInfo->adfDstGeoTransform[3] = 0.0;
        psInfo->adfDstGeoTransform[4] = 0.0;
        psInfo->adfDstGeoTransform[5] = 1.0;
        memcpy( psInfo->adfDstInvGeoTransform, psInfo->adfDstGeoTransform,
                sizeof(double) * 6 );
    }
    
    return psInfo;
}

/************************************************************************/
/*                  GDALRefreshGenImgProjTransformer()                  */
/************************************************************************/

void GDALRefreshGenImgProjTransformer(void* hTransformArg)
{
    GDALGenImgProjTransformInfo *psInfo =
        static_cast<GDALGenImgProjTransformInfo *>( hTransformArg );

    if (psInfo->pReprojectArg)
    {
        CPLXMLNode* psXML = GDALSerializeReprojectionTransformer(psInfo->pReprojectArg);
        GDALDestroyReprojectionTransformer(psInfo->pReprojectArg);
        psInfo->pReprojectArg = GDALDeserializeReprojectionTransformer(psXML);
        CPLDestroyXMLNode(psXML);
    }
}

/************************************************************************/
/*                  GDALCreateGenImgProjTransformer3()                  */
/************************************************************************/

/**
 * Create image to image transformer.
 *
 * This function creates a transformation object that maps from pixel/line
 * coordinates on one image to pixel/line coordinates on another image.  The
 * images may potentially be georeferenced in different coordinate systems, 
 * and may used GCPs to map between their pixel/line coordinates and 
 * georeferenced coordinates (as opposed to the default assumption that their
 * geotransform should be used). 
 *
 * This transformer potentially performs three concatenated transformations.
 *
 * The first stage is from source image pixel/line coordinates to source
 * image georeferenced coordinates, and may be done using the geotransform, 
 * or if not defined using a polynomial model derived from GCPs.  If GCPs
 * are used this stage is accomplished using GDALGCPTransform(). 
 *
 * The second stage is to change projections from the source coordinate system
 * to the destination coordinate system, assuming they differ.  This is 
 * accomplished internally using GDALReprojectionTransform().
 *
 * The third stage is converting from destination image georeferenced
 * coordinates to destination image coordinates.  This is done using the
 * destination image geotransform, or if not available, using a polynomial 
 * model derived from GCPs. If GCPs are used this stage is accomplished using 
 * GDALGCPTransform().  This stage is skipped if hDstDS is NULL when the
 * transformation is created. 
 *
 * @param pszSrcWKT source WKT (or NULL).
 * @param padfSrcGeoTransform source geotransform (or NULL).
 * @param pszDstWKT destination WKT (or NULL).
 * @param padfDstGeoTransform destination geotransform (or NULL).
 * 
 * @return handle suitable for use GDALGenImgProjTransform(), and to be
 * deallocated with GDALDestroyGenImgProjTransformer() or NULL on failure.
 */

void *
GDALCreateGenImgProjTransformer3( const char *pszSrcWKT,
                                  const double *padfSrcGeoTransform,
                                  const char *pszDstWKT,
                                  const double *padfDstGeoTransform )

{
    GDALGenImgProjTransformInfo *psInfo;

/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    psInfo = (GDALGenImgProjTransformInfo *) 
        CPLCalloc(sizeof(GDALGenImgProjTransformInfo),1);

    strcpy( psInfo->sTI.szSignature, "GTI" );
    psInfo->sTI.pszClassName = "GDALGenImgProjTransformer";
    psInfo->sTI.pfnTransform = GDALGenImgProjTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyGenImgProjTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeGenImgProjTransformer;

/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for the source image.      */
/* -------------------------------------------------------------------- */
    if( padfSrcGeoTransform )
    {
        memcpy( psInfo->adfSrcGeoTransform, padfSrcGeoTransform,
                sizeof(psInfo->adfSrcGeoTransform) );
        GDALInvGeoTransform( psInfo->adfSrcGeoTransform, 
                             psInfo->adfSrcInvGeoTransform );
    }
    else
    {
        psInfo->adfSrcGeoTransform[0] = 0.0;
        psInfo->adfSrcGeoTransform[1] = 1.0;
        psInfo->adfSrcGeoTransform[2] = 0.0;
        psInfo->adfSrcGeoTransform[3] = 0.0;
        psInfo->adfSrcGeoTransform[4] = 0.0;
        psInfo->adfSrcGeoTransform[5] = 1.0;
        memcpy( psInfo->adfSrcInvGeoTransform, psInfo->adfSrcGeoTransform,
                sizeof(double) * 6 );
    }

/* -------------------------------------------------------------------- */
/*      Setup reprojection.                                             */
/* -------------------------------------------------------------------- */
    if( pszSrcWKT != NULL && strlen(pszSrcWKT) > 0 
        && pszDstWKT != NULL && strlen(pszDstWKT) > 0 
        && !EQUAL(pszSrcWKT, pszDstWKT) )
    {
        psInfo->pReprojectArg = 
            GDALCreateReprojectionTransformer( pszSrcWKT, pszDstWKT );
    }
        
/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for destination image.     */
/*      If we have no destination matrix use a unit transform.          */
/* -------------------------------------------------------------------- */
    if( padfDstGeoTransform )
    {
        memcpy( psInfo->adfDstGeoTransform, padfDstGeoTransform,
                sizeof(psInfo->adfDstGeoTransform) );
        GDALInvGeoTransform( psInfo->adfDstGeoTransform, 
                             psInfo->adfDstInvGeoTransform );
    }
    else
    {
        psInfo->adfDstGeoTransform[0] = 0.0;
        psInfo->adfDstGeoTransform[1] = 1.0;
        psInfo->adfDstGeoTransform[2] = 0.0;
        psInfo->adfDstGeoTransform[3] = 0.0;
        psInfo->adfDstGeoTransform[4] = 0.0;
        psInfo->adfDstGeoTransform[5] = 1.0;
        memcpy( psInfo->adfDstInvGeoTransform, psInfo->adfDstGeoTransform,
                sizeof(double) * 6 );
    }
    
    return psInfo;
}

/************************************************************************/
/*            GDALSetGenImgProjTransformerDstGeoTransform()             */
/************************************************************************/

/**
 * Set GenImgProj output geotransform.
 *
 * Normally the "destination geotransform", or transformation between 
 * georeferenced output coordinates and pixel/line coordinates on the
 * destination file is extracted from the destination file by 
 * GDALCreateGenImgProjTransformer() and stored in the GenImgProj private
 * info.  However, sometimes it is inconvenient to have an output file
 * handle with appropriate geotransform information when creating the
 * transformation.  For these cases, this function can be used to apply
 * the destination geotransform. 
 *
 * @param hTransformArg the handle to update.
 * @param padfGeoTransform the destination geotransform to apply (six doubles).
 */

void GDALSetGenImgProjTransformerDstGeoTransform( 
    void *hTransformArg, const double *padfGeoTransform )

{
    VALIDATE_POINTER0( hTransformArg, "GDALSetGenImgProjTransformerDstGeoTransform" );

    GDALGenImgProjTransformInfo *psInfo = 
        static_cast<GDALGenImgProjTransformInfo *>( hTransformArg );

    memcpy( psInfo->adfDstGeoTransform, padfGeoTransform, sizeof(double) * 6 );
    GDALInvGeoTransform( psInfo->adfDstGeoTransform, 
                         psInfo->adfDstInvGeoTransform );
}

/************************************************************************/
/*                  GDALDestroyGenImgProjTransformer()                  */
/************************************************************************/

/**
 * GenImgProjTransformer deallocator.
 *
 * This function is used to deallocate the handle created with
 * GDALCreateGenImgProjTransformer().
 *
 * @param hTransformArg the handle to deallocate. 
 */

void GDALDestroyGenImgProjTransformer( void *hTransformArg )

{
    VALIDATE_POINTER0( hTransformArg, "GDALDestroyGenImgProjTransformer" );

    GDALGenImgProjTransformInfo *psInfo = 
        (GDALGenImgProjTransformInfo *) hTransformArg;

    if( psInfo->pSrcGCPTransformArg != NULL )
        GDALDestroyGCPTransformer( psInfo->pSrcGCPTransformArg );

    if( psInfo->pSrcTPSTransformArg != NULL )
        GDALDestroyTPSTransformer( psInfo->pSrcTPSTransformArg );

    if( psInfo->pSrcRPCTransformArg != NULL )
        GDALDestroyRPCTransformer( psInfo->pSrcRPCTransformArg );

    if( psInfo->pSrcGeoLocTransformArg != NULL )
        GDALDestroyGeoLocTransformer( psInfo->pSrcGeoLocTransformArg );

    if( psInfo->pDstGCPTransformArg != NULL )
        GDALDestroyGCPTransformer( psInfo->pDstGCPTransformArg );

    if( psInfo->pReprojectArg != NULL )
        GDALDestroyReprojectionTransformer( psInfo->pReprojectArg );

    CPLFree( psInfo );
}

/************************************************************************/
/*                      GDALGenImgProjTransform()                       */
/************************************************************************/

/**
 * Perform general image reprojection transformation.
 *
 * Actually performs the transformation setup in 
 * GDALCreateGenImgProjTransformer().  This function matches the signature
 * required by the GDALTransformerFunc(), and more details on the arguments
 * can be found in that topic. 
 */

int GDALGenImgProjTransform( void *pTransformArg, int bDstToSrc, 
                             int nPointCount, 
                             double *padfX, double *padfY, double *padfZ,
                             int *panSuccess )
{
    GDALGenImgProjTransformInfo *psInfo = 
        (GDALGenImgProjTransformInfo *) pTransformArg;
    int   i;
    double *padfGeoTransform;
    void *pGCPTransformArg;
    void *pRPCTransformArg;
    void *pTPSTransformArg;
    void *pGeoLocTransformArg;

/* -------------------------------------------------------------------- */
/*      Convert from src (dst) pixel/line to src (dst)                  */
/*      georeferenced coordinates.                                      */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
    {
        padfGeoTransform = psInfo->adfDstGeoTransform;
        pGCPTransformArg = psInfo->pDstGCPTransformArg;
        pRPCTransformArg = NULL;
        pTPSTransformArg = NULL;
        pGeoLocTransformArg = NULL;
    }
    else
    {
        padfGeoTransform = psInfo->adfSrcGeoTransform;
        pGCPTransformArg = psInfo->pSrcGCPTransformArg;
        pRPCTransformArg = psInfo->pSrcRPCTransformArg;
        pTPSTransformArg = psInfo->pSrcTPSTransformArg;
        pGeoLocTransformArg = psInfo->pSrcGeoLocTransformArg;
    }

    if( pGCPTransformArg != NULL )
    {
        if( !GDALGCPTransform( pGCPTransformArg, FALSE, 
                               nPointCount, padfX, padfY, padfZ,
                               panSuccess ) )
            return FALSE;
    }
    else if( pTPSTransformArg != NULL )
    {
        if( !GDALTPSTransform( pTPSTransformArg, FALSE, 
                               nPointCount, padfX, padfY, padfZ,
                               panSuccess ) )
            return FALSE;
    }
    else if( pRPCTransformArg != NULL )
    {
        if( !GDALRPCTransform( pRPCTransformArg, FALSE, 
                               nPointCount, padfX, padfY, padfZ,
                               panSuccess ) )
            return FALSE;
    }
    else if( pGeoLocTransformArg != NULL )
    {
        if( !GDALGeoLocTransform( pGeoLocTransformArg, FALSE, 
                                  nPointCount, padfX, padfY, padfZ,
                                  panSuccess ) )
            return FALSE;
    }
    else 
    {
        for( i = 0; i < nPointCount; i++ )
        {
            double dfNewX, dfNewY;
            
            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            dfNewX = padfGeoTransform[0]
                + padfX[i] * padfGeoTransform[1]
                + padfY[i] * padfGeoTransform[2];
            dfNewY = padfGeoTransform[3]
                + padfX[i] * padfGeoTransform[4]
                + padfY[i] * padfGeoTransform[5];
            
            padfX[i] = dfNewX;
            padfY[i] = dfNewY;
        }
    }

/* -------------------------------------------------------------------- */
/*      Reproject if needed.                                            */
/* -------------------------------------------------------------------- */
    if( psInfo->pReprojectArg )
    {
        if( !GDALReprojectionTransform( psInfo->pReprojectArg, bDstToSrc, 
                                        nPointCount, padfX, padfY, padfZ,
                                        panSuccess ) )
            return FALSE;
    }
    else
    {
        for( i = 0; i < nPointCount; i++ )
            panSuccess[i] = 1;
    }

/* -------------------------------------------------------------------- */
/*      Convert dst (src) georef coordinates back to pixel/line.        */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
    {
        padfGeoTransform = psInfo->adfSrcInvGeoTransform;
        pGCPTransformArg = psInfo->pSrcGCPTransformArg;
        pRPCTransformArg = psInfo->pSrcRPCTransformArg;
        pTPSTransformArg = psInfo->pSrcTPSTransformArg;
        pGeoLocTransformArg = psInfo->pSrcGeoLocTransformArg;
    }
    else
    {
        padfGeoTransform = psInfo->adfDstInvGeoTransform;
        pGCPTransformArg = psInfo->pDstGCPTransformArg;
        pRPCTransformArg = NULL;
        pTPSTransformArg = NULL;
        pGeoLocTransformArg = NULL;
    }
        
    if( pGCPTransformArg != NULL )
    {
        if( !GDALGCPTransform( pGCPTransformArg, TRUE,
                               nPointCount, padfX, padfY, padfZ,
                               panSuccess ) )
            return FALSE;
    }
    else if( pTPSTransformArg != NULL )
    {
        if( !GDALTPSTransform( pTPSTransformArg, TRUE,
                               nPointCount, padfX, padfY, padfZ,
                               panSuccess ) )
            return FALSE;
    }
    else if( pRPCTransformArg != NULL )
    {
        if( !GDALRPCTransform( pRPCTransformArg, TRUE,
                               nPointCount, padfX, padfY, padfZ,
                               panSuccess ) )
            return FALSE;
    }
    else if( pGeoLocTransformArg != NULL )
    {
        if( !GDALGeoLocTransform( pGeoLocTransformArg, TRUE,
                                  nPointCount, padfX, padfY, padfZ,
                                  panSuccess ) )
            return FALSE;
    }
    else
    {
        for( i = 0; i < nPointCount; i++ )
        {
            double dfNewX, dfNewY;

            if( !panSuccess[i] )
                continue;
            
            dfNewX = padfGeoTransform[0]
                + padfX[i] * padfGeoTransform[1]
                + padfY[i] * padfGeoTransform[2];
            dfNewY = padfGeoTransform[3]
                + padfX[i] * padfGeoTransform[4]
                + padfY[i] * padfGeoTransform[5];
            
            padfX[i] = dfNewX;
            padfY[i] = dfNewY;
        }
    }
        
    return TRUE;
}

/************************************************************************/
/*                 GDALSerializeGenImgProjTransformer()                 */
/************************************************************************/

static CPLXMLNode *
GDALSerializeGenImgProjTransformer( void *pTransformArg )

{
    char szWork[200];
    CPLXMLNode *psTree;
    GDALGenImgProjTransformInfo *psInfo = 
        (GDALGenImgProjTransformInfo *) pTransformArg;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "GenImgProjTransformer" );

/* -------------------------------------------------------------------- */
/*      Handle GCP transformation.                                      */
/* -------------------------------------------------------------------- */
    if( psInfo->pSrcGCPTransformArg != NULL )
    {
        CPLXMLNode *psTransformerContainer;
        CPLXMLNode *psTransformer;

        psTransformerContainer = 
            CPLCreateXMLNode( psTree, CXT_Element, "SrcGCPTransformer" );

        psTransformer = GDALSerializeTransformer( GDALGCPTransform,
                                                  psInfo->pSrcGCPTransformArg);
        if( psTransformer != NULL )
            CPLAddXMLChild( psTransformerContainer, psTransformer );
    }

/* -------------------------------------------------------------------- */
/*      Handle TPS transformation.                                      */
/* -------------------------------------------------------------------- */
    else if( psInfo->pSrcTPSTransformArg != NULL )
    {
        CPLXMLNode *psTransformerContainer;
        CPLXMLNode *psTransformer;

        psTransformerContainer = 
            CPLCreateXMLNode( psTree, CXT_Element, "SrcTPSTransformer" );

        psTransformer = 
            GDALSerializeTransformer( NULL, psInfo->pSrcTPSTransformArg);
        if( psTransformer != NULL )
            CPLAddXMLChild( psTransformerContainer, psTransformer );
    }

/* -------------------------------------------------------------------- */
/*      Handle GeoLoc transformation.                                   */
/* -------------------------------------------------------------------- */
    else if( psInfo->pSrcGeoLocTransformArg != NULL )
    {
        CPLXMLNode *psTransformerContainer;
        CPLXMLNode *psTransformer;

        psTransformerContainer = 
            CPLCreateXMLNode( psTree, CXT_Element, "SrcGeoLocTransformer" );

        psTransformer = 
            GDALSerializeTransformer( NULL, psInfo->pSrcGeoLocTransformArg);
        if( psTransformer != NULL )
            CPLAddXMLChild( psTransformerContainer, psTransformer );
    }

/* -------------------------------------------------------------------- */
/*      Handle RPC transformation.                                      */
/* -------------------------------------------------------------------- */
    else if( psInfo->pSrcRPCTransformArg != NULL )
    {
        CPLXMLNode *psTransformerContainer;
        CPLXMLNode *psTransformer;

        psTransformerContainer = 
            CPLCreateXMLNode( psTree, CXT_Element, "SrcRPCTransformer" );

        psTransformer = 
            GDALSerializeTransformer( NULL, psInfo->pSrcRPCTransformArg);
        if( psTransformer != NULL )
            CPLAddXMLChild( psTransformerContainer, psTransformer );
    }

/* -------------------------------------------------------------------- */
/*      Handle source geotransforms.                                    */
/* -------------------------------------------------------------------- */
    else
    {
        sprintf( szWork, "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
                 psInfo->adfSrcGeoTransform[0],
                 psInfo->adfSrcGeoTransform[1],
                 psInfo->adfSrcGeoTransform[2],
                 psInfo->adfSrcGeoTransform[3],
                 psInfo->adfSrcGeoTransform[4],
                 psInfo->adfSrcGeoTransform[5] );
        CPLCreateXMLElementAndValue( psTree, "SrcGeoTransform", szWork );
        
        sprintf( szWork, "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
                 psInfo->adfSrcInvGeoTransform[0],
                 psInfo->adfSrcInvGeoTransform[1],
                 psInfo->adfSrcInvGeoTransform[2],
                 psInfo->adfSrcInvGeoTransform[3],
                 psInfo->adfSrcInvGeoTransform[4],
                 psInfo->adfSrcInvGeoTransform[5] );
        CPLCreateXMLElementAndValue( psTree, "SrcInvGeoTransform", szWork );
    }
    
/* -------------------------------------------------------------------- */
/*      Handle destination geotransforms.                               */
/* -------------------------------------------------------------------- */
    sprintf( szWork, "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
             psInfo->adfDstGeoTransform[0],
             psInfo->adfDstGeoTransform[1],
             psInfo->adfDstGeoTransform[2],
             psInfo->adfDstGeoTransform[3],
             psInfo->adfDstGeoTransform[4],
             psInfo->adfDstGeoTransform[5] );
    CPLCreateXMLElementAndValue( psTree, "DstGeoTransform", szWork );
    
    sprintf( szWork, "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
             psInfo->adfDstInvGeoTransform[0],
             psInfo->adfDstInvGeoTransform[1],
             psInfo->adfDstInvGeoTransform[2],
             psInfo->adfDstInvGeoTransform[3],
             psInfo->adfDstInvGeoTransform[4],
             psInfo->adfDstInvGeoTransform[5] );
    CPLCreateXMLElementAndValue( psTree, "DstInvGeoTransform", szWork );

/* -------------------------------------------------------------------- */
/*      Do we have a reprojection transformer?                          */
/* -------------------------------------------------------------------- */
    if( psInfo->pReprojectArg != NULL )
    {
        CPLXMLNode *psTransformerContainer;
        CPLXMLNode *psTransformer;

        psTransformerContainer = 
            CPLCreateXMLNode( psTree, CXT_Element, "ReprojectTransformer" );

        psTransformer = GDALSerializeTransformer( GDALReprojectionTransform,
                                                  psInfo->pReprojectArg );
        if( psTransformer != NULL )
            CPLAddXMLChild( psTransformerContainer, psTransformer );
    }
    
    return psTree;
}

/************************************************************************/
/*                GDALDeserializeGenImgProjTransformer()                */
/************************************************************************/

void *GDALDeserializeGenImgProjTransformer( CPLXMLNode *psTree )

{
    GDALGenImgProjTransformInfo *psInfo;
    CPLXMLNode *psSubtree;

/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    psInfo = (GDALGenImgProjTransformInfo *) 
        CPLCalloc(sizeof(GDALGenImgProjTransformInfo),1);

    strcpy( psInfo->sTI.szSignature, "GTI" );
    psInfo->sTI.pszClassName = "GDALGenImgProjTransformer";
    psInfo->sTI.pfnTransform = GDALGenImgProjTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyGenImgProjTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeGenImgProjTransformer;

/* -------------------------------------------------------------------- */
/*      SrcGeotransform                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "SrcGeoTransform" ) != NULL )
    {
        sscanf( CPLGetXMLValue( psTree, "SrcGeoTransform", "" ), 
                "%lg,%lg,%lg,%lg,%lg,%lg", 
                psInfo->adfSrcGeoTransform + 0,
                psInfo->adfSrcGeoTransform + 1,
                psInfo->adfSrcGeoTransform + 2,
                psInfo->adfSrcGeoTransform + 3,
                psInfo->adfSrcGeoTransform + 4,
                psInfo->adfSrcGeoTransform + 5 );

        if( CPLGetXMLNode( psTree, "SrcInvGeoTransform" ) != NULL )
        {
            sscanf( CPLGetXMLValue( psTree, "SrcInvGeoTransform", "" ), 
                    "%lg,%lg,%lg,%lg,%lg,%lg", 
                    psInfo->adfSrcInvGeoTransform + 0,
                    psInfo->adfSrcInvGeoTransform + 1,
                    psInfo->adfSrcInvGeoTransform + 2,
                    psInfo->adfSrcInvGeoTransform + 3,
                    psInfo->adfSrcInvGeoTransform + 4,
                    psInfo->adfSrcInvGeoTransform + 5 );
            
        }
        else
            GDALInvGeoTransform( psInfo->adfSrcGeoTransform,
                                 psInfo->adfSrcInvGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Src GCP Transform                                               */
/* -------------------------------------------------------------------- */
    psSubtree = CPLGetXMLNode( psTree, "SrcGCPTransformer" );
    if( psSubtree != NULL && psSubtree->psChild != NULL )
    {
        psInfo->pSrcGCPTransformArg = 
            GDALDeserializeGCPTransformer( psSubtree->psChild );
    }

/* -------------------------------------------------------------------- */
/*      Src TPS Transform                                               */
/* -------------------------------------------------------------------- */
    psSubtree = CPLGetXMLNode( psTree, "SrcTPSTransformer" );
    if( psSubtree != NULL && psSubtree->psChild != NULL )
    {
        psInfo->pSrcTPSTransformArg = 
            GDALDeserializeTPSTransformer( psSubtree->psChild );
    }

/* -------------------------------------------------------------------- */
/*      Src GeoLoc Transform                                            */
/* -------------------------------------------------------------------- */
    psSubtree = CPLGetXMLNode( psTree, "SrcGeoLocTransformer" );
    if( psSubtree != NULL && psSubtree->psChild != NULL )
    {
        psInfo->pSrcGeoLocTransformArg = 
            GDALDeserializeGeoLocTransformer( psSubtree->psChild );
    }

/* -------------------------------------------------------------------- */
/*      Src RPC Transform                                               */
/* -------------------------------------------------------------------- */
    psSubtree = CPLGetXMLNode( psTree, "SrcRPCTransformer" );
    if( psSubtree != NULL && psSubtree->psChild != NULL )
    {
        psInfo->pSrcRPCTransformArg = 
            GDALDeserializeRPCTransformer( psSubtree->psChild );
    }

/* -------------------------------------------------------------------- */
/*      DstGeotransform                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "DstGeoTransform" ) != NULL )
    {
        sscanf( CPLGetXMLValue( psTree, "DstGeoTransform", "" ), 
                "%lg,%lg,%lg,%lg,%lg,%lg", 
                psInfo->adfDstGeoTransform + 0,
                psInfo->adfDstGeoTransform + 1,
                psInfo->adfDstGeoTransform + 2,
                psInfo->adfDstGeoTransform + 3,
                psInfo->adfDstGeoTransform + 4,
                psInfo->adfDstGeoTransform + 5 );

        if( CPLGetXMLNode( psTree, "DstInvGeoTransform" ) != NULL )
        {
            sscanf( CPLGetXMLValue( psTree, "DstInvGeoTransform", "" ), 
                    "%lg,%lg,%lg,%lg,%lg,%lg", 
                    psInfo->adfDstInvGeoTransform + 0,
                    psInfo->adfDstInvGeoTransform + 1,
                    psInfo->adfDstInvGeoTransform + 2,
                    psInfo->adfDstInvGeoTransform + 3,
                    psInfo->adfDstInvGeoTransform + 4,
                    psInfo->adfDstInvGeoTransform + 5 );
            
        }
        else
            GDALInvGeoTransform( psInfo->adfDstGeoTransform,
                                 psInfo->adfDstInvGeoTransform );
    }
    
/* -------------------------------------------------------------------- */
/*      Reproject transformer                                           */
/* -------------------------------------------------------------------- */
    psSubtree = CPLGetXMLNode( psTree, "ReprojectTransformer" );
    if( psSubtree != NULL && psSubtree->psChild != NULL )
    {
        psInfo->pReprojectArg = 
            GDALDeserializeReprojectionTransformer( psSubtree->psChild );
    }

    return psInfo;
}

/************************************************************************/
/* ==================================================================== */
/*			 GDALReprojectionTransformer                    */
/* ==================================================================== */
/************************************************************************/

typedef struct {
    GDALTransformerInfo sTI;

    OGRCoordinateTransformation *poForwardTransform;
    OGRCoordinateTransformation *poReverseTransform;
} GDALReprojectionTransformInfo;

/************************************************************************/
/*                 GDALCreateReprojectionTransformer()                  */
/************************************************************************/

/**
 * Create reprojection transformer.
 *
 * Creates a callback data structure suitable for use with 
 * GDALReprojectionTransformation() to represent a transformation from
 * one geographic or projected coordinate system to another.  On input
 * the coordinate systems are described in OpenGIS WKT format. 
 *
 * Internally the OGRCoordinateTransformation object is used to implement
 * the reprojection.
 *
 * @param pszSrcWKT the coordinate system for the source coordinate system.
 * @param pszDstWKT the coordinate system for the destination coordinate 
 * system.
 *
 * @return Handle for use with GDALReprojectionTransform(), or NULL if the 
 * system fails to initialize the reprojection. 
 **/

void *GDALCreateReprojectionTransformer( const char *pszSrcWKT, 
                                         const char *pszDstWKT )

{
    OGRSpatialReference oSrcSRS, oDstSRS;
    OGRCoordinateTransformation *poForwardTransform;

/* -------------------------------------------------------------------- */
/*      Ingest the SRS definitions.                                     */
/* -------------------------------------------------------------------- */
    if( oSrcSRS.importFromWkt( (char **) &pszSrcWKT ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to import coordinate system `%s'.", 
                  pszSrcWKT );
        return NULL;
    }
    if( oDstSRS.importFromWkt( (char **) &pszDstWKT ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to import coordinate system `%s'.", 
                  pszSrcWKT );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Build the forward coordinate transformation.                    */
/* -------------------------------------------------------------------- */
    poForwardTransform = OGRCreateCoordinateTransformation(&oSrcSRS,&oDstSRS);

    if( poForwardTransform == NULL )
        // OGRCreateCoordinateTransformation() will report errors on its own.
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a structure to hold the transform info, and also         */
/*      build reverse transform.  We assume that if the forward         */
/*      transform can be created, then so can the reverse one.          */
/* -------------------------------------------------------------------- */
    GDALReprojectionTransformInfo *psInfo;

    psInfo = (GDALReprojectionTransformInfo *) 
        CPLCalloc(sizeof(GDALReprojectionTransformInfo),1);

    psInfo->poForwardTransform = poForwardTransform;
    psInfo->poReverseTransform = 
        OGRCreateCoordinateTransformation(&oDstSRS,&oSrcSRS);

    strcpy( psInfo->sTI.szSignature, "GTI" );
    psInfo->sTI.pszClassName = "GDALReprojectionTransformer";
    psInfo->sTI.pfnTransform = GDALReprojectionTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyReprojectionTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeReprojectionTransformer;

    return psInfo;
}

/************************************************************************/
/*                 GDALDestroyReprojectionTransformer()                 */
/************************************************************************/

/**
 * Destroy reprojection transformation.
 *
 * @param pTransformArg the transformation handle returned by
 * GDALCreateReprojectionTransformer().
 */

void GDALDestroyReprojectionTransformer( void *pTransformArg )

{
    VALIDATE_POINTER0( pTransformArg, "GDALDestroyReprojectionTransformer" );

    GDALReprojectionTransformInfo *psInfo = 
        (GDALReprojectionTransformInfo *) pTransformArg;		

    if( psInfo->poForwardTransform )
        delete psInfo->poForwardTransform;

    if( psInfo->poReverseTransform )
        delete psInfo->poReverseTransform;

    CPLFree( psInfo );
}

/************************************************************************/
/*                     GDALReprojectionTransform()                      */
/************************************************************************/

/**
 * Perform reprojection transformation.
 *
 * Actually performs the reprojection transformation described in 
 * GDALCreateReprojectionTransformer().  This function matches the 
 * GDALTransformerFunc() signature.  Details of the arguments are described
 * there. 
 */

int GDALReprojectionTransform( void *pTransformArg, int bDstToSrc, 
                                int nPointCount, 
                                double *padfX, double *padfY, double *padfZ,
                                int *panSuccess )

{
    GDALReprojectionTransformInfo *psInfo = 
        (GDALReprojectionTransformInfo *) pTransformArg;		
    int bSuccess;

    if( bDstToSrc )
        bSuccess = psInfo->poReverseTransform->TransformEx( 
            nPointCount, padfX, padfY, padfZ, panSuccess );
    else
        bSuccess = psInfo->poForwardTransform->TransformEx( 
            nPointCount, padfX, padfY, padfZ, panSuccess );

    return bSuccess;
}

/************************************************************************/
/*                GDALSerializeReprojectionTransformer()                */
/************************************************************************/

static CPLXMLNode *
GDALSerializeReprojectionTransformer( void *pTransformArg )

{
    CPLXMLNode *psTree;
    GDALReprojectionTransformInfo *psInfo = 
        (GDALReprojectionTransformInfo *) pTransformArg;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "ReprojectionTransformer" );

/* -------------------------------------------------------------------- */
/*      Handle SourceCS.                                                */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS;
    char *pszWKT = NULL;

    poSRS = psInfo->poForwardTransform->GetSourceCS();
    poSRS->exportToWkt( &pszWKT );
    CPLCreateXMLElementAndValue( psTree, "SourceSRS", pszWKT );
    CPLFree( pszWKT );

/* -------------------------------------------------------------------- */
/*      Handle DestinationCS.                                           */
/* -------------------------------------------------------------------- */
    poSRS = psInfo->poForwardTransform->GetTargetCS();
    poSRS->exportToWkt( &pszWKT );
    CPLCreateXMLElementAndValue( psTree, "TargetSRS", pszWKT );
    CPLFree( pszWKT );

    return psTree;
}

/************************************************************************/
/*               GDALDeserializeReprojectionTransformer()               */
/************************************************************************/

static void *
GDALDeserializeReprojectionTransformer( CPLXMLNode *psTree )

{
    const char *pszSourceSRS = CPLGetXMLValue( psTree, "SourceSRS", NULL );
    const char *pszTargetSRS= CPLGetXMLValue( psTree, "TargetSRS", NULL );
    char *pszSourceWKT = NULL, *pszTargetWKT = NULL;
    void *pResult = NULL;

    if( pszSourceSRS != NULL )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( pszSourceSRS ) == OGRERR_NONE )
            oSRS.exportToWkt( &pszSourceWKT );
    }

    if( pszTargetSRS != NULL )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( pszTargetSRS ) == OGRERR_NONE )
            oSRS.exportToWkt( &pszTargetWKT );
    }

    if( pszSourceWKT != NULL && pszTargetWKT != NULL )
    {
        pResult = GDALCreateReprojectionTransformer( pszSourceWKT,
                                                     pszTargetWKT );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "ReprojectionTransformer definition missing either\n"
                  "SourceSRS or TargetSRS definition." );
    }

    CPLFree( pszSourceWKT );
    CPLFree( pszTargetWKT );

    return pResult;
}

/************************************************************************/
/* ==================================================================== */
/*      Approximate transformer.                                        */
/* ==================================================================== */
/************************************************************************/

typedef struct 
{
    GDALTransformerInfo sTI;

    GDALTransformerFunc pfnBaseTransformer;
    void             *pBaseCBData;
    double	      dfMaxError;

    int               bOwnSubtransformer;
} ApproxTransformInfo;

/************************************************************************/
/*                   GDALSerializeApproxTransformer()                   */
/************************************************************************/

static CPLXMLNode *
GDALSerializeApproxTransformer( void *pTransformArg )

{
    CPLXMLNode *psTree;
    ApproxTransformInfo *psInfo = (ApproxTransformInfo *) pTransformArg;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "ApproxTransformer" );

/* -------------------------------------------------------------------- */
/*      Attach max error.                                               */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( psTree, "MaxError", 
                                 CPLString().Printf("%g",psInfo->dfMaxError) );

/* -------------------------------------------------------------------- */
/*      Capture underlying transformer.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTransformerContainer;
    CPLXMLNode *psTransformer;

    psTransformerContainer = 
        CPLCreateXMLNode( psTree, CXT_Element, "BaseTransformer" );
    
    psTransformer = GDALSerializeTransformer( psInfo->pfnBaseTransformer,
                                              psInfo->pBaseCBData );
    if( psTransformer != NULL )
        CPLAddXMLChild( psTransformerContainer, psTransformer );

    return psTree;
}

/************************************************************************/
/*                    GDALCreateApproxTransformer()                     */
/************************************************************************/

/**
 * Create an approximating transformer.
 *
 * This function creates a context for an approximated transformer.  Basically
 * a high precision transformer is supplied as input and internally linear
 * approximations are computed to generate results to within a defined
 * precision. 
 *
 * The approximation is actually done at the point where GDALApproxTransform()
 * calls are made, and depend on the assumption that the roughly linear.  The
 * first and last point passed in must be the extreme values and the 
 * intermediate values should describe a curve between the end points.  The
 * approximator transforms and center using the approximate transformer, and
 * then compares the true middle transformed value to a linear approximation
 * based on the end points.  If the error is within the supplied threshold
 * then the end points are used to linearly approximate all the values 
 * otherwise the inputs points are split into two smaller sets, and the
 * function recursively called till a sufficiently small set of points if found
 * that the linear approximation is OK, or that all the points are exactly
 * computed. 
 *
 * This function is very suitable for approximating transformation results
 * from output pixel/line space to input coordinates for warpers that operate
 * on one input scanline at a time.  Care should be taken using it in other
 * circumstances as little internal validation is done, in order to keep things
 * fast. 
 *
 * @param pfnBaseTransformer the high precision transformer which should be
 * approximated. 
 * @param pBaseTransformArg the callback argument for the high precision 
 * transformer. 
 * @param dfMaxError the maximum cartesian error in the "output" space that
 * is to be accepted in the linear approximation.
 * 
 * @return callback pointer suitable for use with GDALApproxTransform().  It
 * should be deallocated with GDALDestroyApproxTransformer().
 */

void *GDALCreateApproxTransformer( GDALTransformerFunc pfnBaseTransformer,
                                   void *pBaseTransformArg, double dfMaxError)

{
    ApproxTransformInfo	*psATInfo;

    psATInfo = (ApproxTransformInfo*) CPLMalloc(sizeof(ApproxTransformInfo));
    psATInfo->pfnBaseTransformer = pfnBaseTransformer;
    psATInfo->pBaseCBData = pBaseTransformArg;
    psATInfo->dfMaxError = dfMaxError;
    psATInfo->bOwnSubtransformer = FALSE;

    strcpy( psATInfo->sTI.szSignature, "GTI" );
    psATInfo->sTI.pszClassName = "GDALApproxTransformer";
    psATInfo->sTI.pfnTransform = GDALApproxTransform;
    psATInfo->sTI.pfnCleanup = GDALDestroyApproxTransformer;
    psATInfo->sTI.pfnSerialize = GDALSerializeApproxTransformer;

    return psATInfo;
}

/************************************************************************/
/*              GDALApproxTransformerOwnsSubtransformer()               */
/************************************************************************/

void GDALApproxTransformerOwnsSubtransformer( void *pCBData, int bOwnFlag )

{
    ApproxTransformInfo	*psATInfo = (ApproxTransformInfo *) pCBData;

    psATInfo->bOwnSubtransformer = bOwnFlag;
}

/************************************************************************/
/*                    GDALDestroyApproxTransformer()                    */
/************************************************************************/

/**
 * Cleanup approximate transformer.
 *
 * Deallocates the resources allocated by GDALCreateApproxTransformer().
 * 
 * @param pCBData callback data originally returned by 
 * GDALCreateApproxTransformer().
 */

void GDALDestroyApproxTransformer( void * pCBData )

{
    VALIDATE_POINTER0( pCBData, "GDALDestroyApproxTransformer" );

    ApproxTransformInfo	*psATInfo = (ApproxTransformInfo *) pCBData;

    if( psATInfo->bOwnSubtransformer ) 
        GDALDestroyTransformer( psATInfo->pBaseCBData );

    CPLFree( pCBData );
}

/************************************************************************/
/*                        GDALApproxTransform()                         */
/************************************************************************/

/**
 * Perform approximate transformation.
 *
 * Actually performs the approximate transformation described in 
 * GDALCreateApproxTransformer().  This function matches the 
 * GDALTransformerFunc() signature.  Details of the arguments are described
 * there. 
 */
 
int GDALApproxTransform( void *pCBData, int bDstToSrc, int nPoints, 
                         double *x, double *y, double *z, int *panSuccess )

{
    ApproxTransformInfo *psATInfo = (ApproxTransformInfo *) pCBData;
    double x2[3], y2[3], z2[3], dfDeltaX, dfDeltaY, dfError, dfDist, dfDeltaZ;
    int nMiddle, anSuccess2[3], i, bSuccess;

    nMiddle = (nPoints-1)/2;

/* -------------------------------------------------------------------- */
/*      Bail if our preconditions are not met, or if error is not       */
/*      acceptable.                                                     */
/* -------------------------------------------------------------------- */
    if( y[0] != y[nPoints-1] || y[0] != y[nMiddle]
        || x[0] == x[nPoints-1] || x[0] == x[nMiddle]
        || psATInfo->dfMaxError == 0.0 || nPoints <= 5 )
    {
        return psATInfo->pfnBaseTransformer( psATInfo->pBaseCBData, bDstToSrc,
                                             nPoints, x, y, z, panSuccess );
    }

/* -------------------------------------------------------------------- */
/*      Transform first, last and middle point.                         */
/* -------------------------------------------------------------------- */
    x2[0] = x[0];
    y2[0] = y[0];
    z2[0] = z[0];
    x2[1] = x[nMiddle];
    y2[1] = y[nMiddle];
    z2[1] = z[nMiddle];
    x2[2] = x[nPoints-1];
    y2[2] = y[nPoints-1];
    z2[2] = z[nPoints-1];

    bSuccess = 
        psATInfo->pfnBaseTransformer( psATInfo->pBaseCBData, bDstToSrc, 3, 
                                      x2, y2, z2, anSuccess2 );
    if( !bSuccess || !anSuccess2[0] || !anSuccess2[1] || !anSuccess2[2] )
        return psATInfo->pfnBaseTransformer( psATInfo->pBaseCBData, bDstToSrc,
                                             nPoints, x, y, z, panSuccess );
    
/* -------------------------------------------------------------------- */
/*      Is the error at the middle acceptable relative to an            */
/*      interpolation of the middle position?                           */
/* -------------------------------------------------------------------- */
    dfDeltaX = (x2[2] - x2[0]) / (x[nPoints-1] - x[0]);
    dfDeltaY = (y2[2] - y2[0]) / (x[nPoints-1] - x[0]);
    dfDeltaZ = (z2[2] - z2[0]) / (x[nPoints-1] - x[0]);

    dfError = fabs((x2[0] + dfDeltaX * (x[nMiddle] - x[0])) - x2[1])
        + fabs((y2[0] + dfDeltaY * (x[nMiddle] - x[0])) - y2[1]);

    if( dfError > psATInfo->dfMaxError )
    {
#ifdef notdef
        CPLDebug( "GDAL", "ApproxTransformer - "
                  "error %g over threshold %g, subdivide %d points.",
                  dfError, psATInfo->dfMaxError, nPoints );
#endif

        bSuccess = 
            GDALApproxTransform( psATInfo, bDstToSrc, nMiddle, 
                                 x, y, z, panSuccess );
            
        if( !bSuccess )
            return FALSE;

        bSuccess = 
            GDALApproxTransform( psATInfo, bDstToSrc, nPoints - nMiddle,
                                 x+nMiddle, y+nMiddle, z+nMiddle,
                                 panSuccess+nMiddle );

        if( !bSuccess )
            return FALSE;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Error is OK since this is just used to compute output bounds    */
/*      of newly created file for gdalwarper.  So just use affine       */
/*      approximation of the reverse transform.  Eventually we          */
/*      should implement iterative searching to find a result within    */
/*      our error threshold.                                            */
/* -------------------------------------------------------------------- */
    for( i = nPoints-1; i >= 0; i-- )
    {
        dfDist = (x[i] - x[0]);
        y[i] = y2[0] + dfDeltaY * dfDist;
        x[i] = x2[0] + dfDeltaX * dfDist;
        z[i] = z2[0] + dfDeltaZ * dfDist;
        panSuccess[i] = TRUE;
    }
    
    return TRUE;
}

/************************************************************************/
/*                  GDALDeserializeApproxTransformer()                  */
/************************************************************************/

static void *
GDALDeserializeApproxTransformer( CPLXMLNode *psTree )

{
    double dfMaxError = atof(CPLGetXMLValue( psTree, "MaxError",  "0.25" ));
    CPLXMLNode *psContainer;
    GDALTransformerFunc pfnBaseTransform = NULL;
    void *pBaseCBData = NULL;

    psContainer = CPLGetXMLNode( psTree, "BaseTransformer" );

    if( psContainer != NULL && psContainer->psChild != NULL )
    {
        GDALDeserializeTransformer( psContainer->psChild, 
                                    &pfnBaseTransform, 
                                    &pBaseCBData );
        
    }
    
    if( pfnBaseTransform == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot get base transform for approx transformer." );
        return NULL;
    }
    else
    {
        void *pApproxCBData = GDALCreateApproxTransformer( pfnBaseTransform,
                                                           pBaseCBData, 
                                                           dfMaxError );
        GDALApproxTransformerOwnsSubtransformer( pApproxCBData, TRUE );

        return pApproxCBData;
    }
}

/************************************************************************/
/*                       GDALApplyGeoTransform()                        */
/************************************************************************/

/**
 * Apply GeoTransform to x/y coordinate.
 *
 * Applies the following computation, converting a (pixel,line) coordinate
 * into a georeferenced (geo_x,geo_y) location. 
 *
 *  *pdfGeoX = padfGeoTransform[0] + dfPixel * padfGeoTransform[1]
 *                                 + dfLine  * padfGeoTransform[2];
 *  *pdfGeoY = padfGeoTransform[3] + dfPixel * padfGeoTransform[4]
 *                                 + dfLine  * padfGeoTransform[5];
 *
 * @param padfGeoTransform Six coefficient GeoTransform to apply.
 * @param dfPixel Input pixel position.
 * @param dfLine Input line position. 
 * @param *pdfGeoX output location where geo_x (easting/longitude) location is placed.
 * @param *pdfGeoY output location where geo_y (northing/latitude) location is placed.
 */

void CPL_STDCALL GDALApplyGeoTransform( double *padfGeoTransform, 
                            double dfPixel, double dfLine, 
                            double *pdfGeoX, double *pdfGeoY )
{
    *pdfGeoX = padfGeoTransform[0] + dfPixel * padfGeoTransform[1]
                                   + dfLine  * padfGeoTransform[2];
    *pdfGeoY = padfGeoTransform[3] + dfPixel * padfGeoTransform[4]
                                   + dfLine  * padfGeoTransform[5];
}

/************************************************************************/
/*                        GDALInvGeoTransform()                         */
/************************************************************************/

/**
 * Invert Geotransform.
 *
 * This function will invert a standard 3x2 set of GeoTransform coefficients.
 * This converts the equation from being pixel to geo to being geo to pixel. 
 *
 * @param gt_in Input geotransform (six doubles - unaltered).
 * @param gt_out Output geotransform (six doubles - updated). 
 *
 * @return TRUE on success or FALSE if the equation is uninvertable. 
 */

int CPL_STDCALL GDALInvGeoTransform( double *gt_in, double *gt_out )

{
    double	det, inv_det;

    /* we assume a 3rd row that is [1 0 0] */

    /* Compute determinate */

    det = gt_in[1] * gt_in[5] - gt_in[2] * gt_in[4];

    if( fabs(det) < 0.000000000000001 )
        return 0;

    inv_det = 1.0 / det;

    /* compute adjoint, and devide by determinate */

    gt_out[1] =  gt_in[5] * inv_det;
    gt_out[4] = -gt_in[4] * inv_det;

    gt_out[2] = -gt_in[2] * inv_det;
    gt_out[5] =  gt_in[1] * inv_det;

    gt_out[0] = ( gt_in[2] * gt_in[3] - gt_in[0] * gt_in[5]) * inv_det;
    gt_out[3] = (-gt_in[1] * gt_in[3] + gt_in[0] * gt_in[4]) * inv_det;

    return 1;
}

/************************************************************************/
/*                      GDALSerializeTransformer()                      */
/************************************************************************/

CPLXMLNode *GDALSerializeTransformer( GDALTransformerFunc pfnFunc,
                                      void *pTransformArg )

{
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeTransformer", NULL );

    GDALTransformerInfo *psInfo = static_cast<GDALTransformerInfo *>(pTransformArg);

    if( psInfo == NULL || !EQUAL(psInfo->szSignature,"GTI") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to serialize non-GTI transformer." );
        return NULL;
    }
    else if ( psInfo->pfnSerialize == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No serialization function available for this transformer." );
        return NULL;
    }
    else
    {
        return psInfo->pfnSerialize( pTransformArg );
    }
}

/************************************************************************/
/*                  GDALRegisterTransformDeserializer()                 */
/************************************************************************/

static CPLList* psListDeserializer = NULL;
static void* hDeserializerMutex = NULL;

typedef struct
{
    char* pszTransformName;
    GDALTransformerFunc pfnTransformerFunc;
    GDALTransformDeserializeFunc pfnDeserializeFunc;
} TransformDeserializerInfo;

void* GDALRegisterTransformDeserializer(const char* pszTransformName,
                                       GDALTransformerFunc pfnTransformerFunc,
                                       GDALTransformDeserializeFunc pfnDeserializeFunc)
{
    TransformDeserializerInfo* psInfo =
        (TransformDeserializerInfo*)CPLMalloc(sizeof(TransformDeserializerInfo));
    psInfo->pszTransformName = CPLStrdup(pszTransformName);
    psInfo->pfnTransformerFunc = pfnTransformerFunc;
    psInfo->pfnDeserializeFunc = pfnDeserializeFunc;

    CPLMutexHolderD(&hDeserializerMutex);
    psListDeserializer = CPLListInsert(psListDeserializer, psInfo, 0);

    return psInfo;
}

/************************************************************************/
/*                GDALUnregisterTransformDeserializer()                 */
/************************************************************************/

void GDALUnregisterTransformDeserializer(void* pData)
{
    CPLMutexHolderD(&hDeserializerMutex);
    CPLList* psList = psListDeserializer;
    CPLList* psLast = NULL;
    while(psList)
    {
        if (psList->pData == pData)
        {
            TransformDeserializerInfo* psInfo =
                    (TransformDeserializerInfo*)pData;
            CPLFree(psInfo->pszTransformName);
            CPLFree(pData);
            if (psLast)
                psLast->psNext = psList->psNext;
            else
                psListDeserializer = NULL;
            CPLFree(psList);
            break;
        }
        psLast = psList;
        psList = psList->psNext;
    }
}


/************************************************************************/
/*                     GDALDeserializeTransformer()                     */
/************************************************************************/

CPLErr GDALDeserializeTransformer( CPLXMLNode *psTree, 
                                   GDALTransformerFunc *ppfnFunc,
                                   void **ppTransformArg )

{
    *ppfnFunc = NULL;
    *ppTransformArg = NULL;

    CPLErrorReset();

    if( psTree == NULL || psTree->eType != CXT_Element )
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Malformed element in GDALDeserializeTransformer" );
    else if( EQUAL(psTree->pszValue,"GenImgProjTransformer") )
    {
        *ppfnFunc = GDALGenImgProjTransform;
        *ppTransformArg = GDALDeserializeGenImgProjTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue,"ReprojectionTransformer") )
    {
        *ppfnFunc = GDALReprojectionTransform;
        *ppTransformArg = GDALDeserializeReprojectionTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue,"GCPTransformer") )
    {
        *ppfnFunc = GDALGCPTransform;
        *ppTransformArg = GDALDeserializeGCPTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue,"TPSTransformer") )
    {
        *ppfnFunc = GDALTPSTransform;
        *ppTransformArg = GDALDeserializeTPSTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue,"GeoLocTransformer") )
    {
        *ppfnFunc = GDALGeoLocTransform;
        *ppTransformArg = GDALDeserializeGeoLocTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue,"RPCTransformer") )
    {
        *ppfnFunc = GDALRPCTransform;
        *ppTransformArg = GDALDeserializeRPCTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue,"ApproxTransformer") )
    {
        *ppfnFunc = GDALApproxTransform;
        *ppTransformArg = GDALDeserializeApproxTransformer( psTree );
    }
    else
    {
        GDALTransformDeserializeFunc pfnDeserializeFunc = NULL;
        {
            CPLMutexHolderD(&hDeserializerMutex);
            CPLList* psList = psListDeserializer;
            while(psList)
            {
                TransformDeserializerInfo* psInfo =
                            (TransformDeserializerInfo*)psList->pData;
                if (strcmp(psInfo->pszTransformName, psTree->pszValue) == 0)
                {
                    *ppfnFunc = psInfo->pfnTransformerFunc;
                    pfnDeserializeFunc = psInfo->pfnDeserializeFunc;
                    break;
                }
                psList = psList->psNext;
            }
        }

        if (pfnDeserializeFunc != NULL)
        {
            *ppTransformArg = pfnDeserializeFunc( psTree );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unrecognised element '%s' GDALDeserializeTransformer",
                    psTree->pszValue );
        }
    }

    return CPLGetLastErrorType();
}

/************************************************************************/
/*                       GDALDestroyTransformer()                       */
/************************************************************************/

void GDALDestroyTransformer( void *pTransformArg )

{
    GDALTransformerInfo *psInfo = (GDALTransformerInfo *) pTransformArg;

    if( psInfo == NULL || !EQUAL(psInfo->szSignature,"GTI") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to destroy non-GTI transformer." );
    }
    else
        psInfo->pfnCleanup( pTransformArg );
}

/************************************************************************/
/*                         GDALUseTransformer()                         */
/************************************************************************/

int GDALUseTransformer( void *pTransformArg,
                        int bDstToSrc, int nPointCount, 
                        double *x, double *y, double *z, 
                        int *panSuccess )
{
    GDALTransformerInfo *psInfo = (GDALTransformerInfo *) pTransformArg;

    if( psInfo == NULL || !EQUAL(psInfo->szSignature,"GTI") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to use non-GTI transformer." );
        return FALSE;
    }
    else
        return psInfo->pfnTransform( pTransformArg, bDstToSrc, nPointCount, 
                                     x, y, z, panSuccess );
}


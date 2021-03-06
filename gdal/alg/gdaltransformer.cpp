/******************************************************************************
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Implementation of one or more GDALTrasformerFunc types, including
 *           the GenImgProj (general image reprojector) transformer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging
 *                          Fort Collin, CO
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#define DO_NOT_USE_DEBUG_BOOL  // See TODO for bGCPUseOK.

#include "cpl_port.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_list.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"


CPL_CVSID("$Id$")

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

static void *
GDALCreateApproxTransformer2( GDALTransformerFunc pfnRawTransformer,
                              void *pRawTransformerArg,
                              double dfMaxErrorForward,
                              double dfMaxErrorReverse );

/************************************************************************/
/*                          GDALTransformFunc                           */
/*                                                                      */
/*      Documentation for GDALTransformFunc typedef.                    */
/************************************************************************/

/*!

\typedef typedef int (*GDALTransformerFunc)( void *pTransformerArg, int bDstToSrc, int nPointCount, double *x, double *y, double *z, int *panSuccess );

Generic signature for spatial point transformers.

This function signature is used for a variety of functions that accept
passed in functions used to transform point locations between two coordinate
spaces.

The GDALCreateGenImgProjTransformer(), GDALCreateReprojectionTransformerEx(),
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
 * Note that if the image is somewhat rotated the diagonal taken isn't of the
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

    double adfExtent[4] = {};

    return GDALSuggestedWarpOutput2( hSrcDS, pfnTransformer, pTransformArg,
                                     padfGeoTransformOut, pnPixels, pnLines,
                                     adfExtent, 0 );
}

static int GDALSuggestedWarpOutput2_MustAdjustForRightBorder(
    GDALTransformerFunc pfnTransformer, void *pTransformArg,
    double* padfExtent,
    CPL_UNUSED int nPixels,
    int nLines,
    double dfPixelSizeX, double dfPixelSizeY )
{
    double adfX[21] = {};
    double adfY[21] = {};

    const double dfMaxXOut = padfExtent[2];
    const double dfMaxYOut = padfExtent[3];

    // Take 20 steps.
    int nSamplePoints = 0;
    for( double dfRatio = 0.0; dfRatio <= 1.01; dfRatio += 0.05 )
    {
        // Ensure we end exactly at the end.
        if( dfRatio > 0.99 )
            dfRatio = 1.0;

        // Along right.
        adfX[nSamplePoints] = dfMaxXOut;
        adfY[nSamplePoints] = dfMaxYOut - dfPixelSizeY * dfRatio * nLines;
        nSamplePoints++;
    }
    double adfZ[21] = {};

    int abSuccess[21] = {};

    bool bErr = false;
    if( !pfnTransformer( pTransformArg, TRUE, nSamplePoints,
                         adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = true;
    }

    if( !bErr && !pfnTransformer( pTransformArg, FALSE, nSamplePoints,
                                  adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = true;
    }

    nSamplePoints = 0;
    int nBadCount = 0;
    for( double dfRatio = 0.0; !bErr && dfRatio <= 1.01; dfRatio += 0.05 )
    {
        const double expected_x = dfMaxXOut;
        const double expected_y = dfMaxYOut - dfPixelSizeY * dfRatio * nLines;
        if( fabs(adfX[nSamplePoints] -  expected_x) > dfPixelSizeX ||
            fabs(adfY[nSamplePoints] -  expected_y) > dfPixelSizeY )
            nBadCount++;
        nSamplePoints++;
    }

    return nBadCount == nSamplePoints;
}

static int GDALSuggestedWarpOutput2_MustAdjustForBottomBorder(
     GDALTransformerFunc pfnTransformer, void *pTransformArg,
     double* padfExtent, int nPixels,
     CPL_UNUSED int nLines,
     double dfPixelSizeX, double dfPixelSizeY)
{
    double adfX[21] = {};
    double adfY[21] = {};

    const double dfMinXOut = padfExtent[0];
    const double dfMinYOut = padfExtent[1];

    // Take 20 steps.
    int nSamplePoints = 0;
    for( double dfRatio = 0.0; dfRatio <= 1.01; dfRatio += 0.05 )
    {
        // Ensure we end exactly at the end.
        if( dfRatio > 0.99 )
            dfRatio = 1.0;

        // Along right.
        adfX[nSamplePoints] = dfMinXOut + dfPixelSizeX * dfRatio * nPixels;
        adfY[nSamplePoints] = dfMinYOut;
        nSamplePoints++;
    }
    double adfZ[21] = {};

    int abSuccess[21] = {};

    bool bErr = false;
    if( !pfnTransformer( pTransformArg, TRUE, nSamplePoints,
                             adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = true;
    }

    if( !bErr && !pfnTransformer( pTransformArg, FALSE, nSamplePoints,
                             adfX, adfY, adfZ, abSuccess ) )
    {
        bErr = true;
    }

    nSamplePoints = 0;
    int nBadCount = 0;
    for( double dfRatio = 0.0; !bErr && dfRatio <= 1.01; dfRatio += 0.05 )
    {
        const double expected_x = dfMinXOut + dfPixelSizeX * dfRatio * nPixels;
        const double expected_y = dfMinYOut;
        if( fabs(adfX[nSamplePoints] -  expected_x) > dfPixelSizeX ||
            fabs(adfY[nSamplePoints] -  expected_y) > dfPixelSizeY )
            nBadCount++;
        nSamplePoints++;
    }

    return nBadCount == nSamplePoints;
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
 * Note that if the image is somewhat rotated the diagonal taken isn't of the
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
 * @param padfExtent Four entry array to return extents as (xmin, ymin, xmax,
 * ymax).
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
                          double *padfExtent,
                          CPL_UNUSED int nOptions )
{
    VALIDATE_POINTER1( hSrcDS, "GDALSuggestedWarpOutput2", CE_Failure );

/* -------------------------------------------------------------------- */
/*      Setup sample points all around the edge of the input raster.    */
/* -------------------------------------------------------------------- */
    if( pfnTransformer == GDALGenImgProjTransform )
    {
        // In case CHECK_WITH_INVERT_PROJ has been modified.
        GDALRefreshGenImgProjTransformer(pTransformArg);
    }
    else if( pfnTransformer == GDALApproxTransform )
    {
        // In case CHECK_WITH_INVERT_PROJ has been modified.
        GDALRefreshApproxTransformer(pTransformArg);
    }

    const int nInXSize = GDALGetRasterXSize( hSrcDS );
    const int nInYSize = GDALGetRasterYSize( hSrcDS );

    const int N_PIXELSTEP = 50;
    int nSteps = static_cast<int>(
        static_cast<double>(std::min(nInYSize, nInXSize)) / N_PIXELSTEP + 0.5);
    if( nSteps < 20 )
        nSteps = 20;
    nSteps = std::min(nSteps, 100);

    // TODO(rouault): How is this goto retry supposed to work?  Added in r20537.
    // Does redoing the same malloc multiple times work?  If it is needed, can
    // it be converted to a tigher while loop around the MALLOC3s and free?  Is
    // the point to try with the full requested steps.  Then, if there is not
    // enough memory, back off and try with just 20 steps?
 retry:
    int nSampleMax = (nSteps + 1)*(nSteps + 1);

    double dfStep = 1.0 / nSteps;
    double *padfY = nullptr;
    double *padfZ = nullptr;
    double *padfYRevert = nullptr;
    double *padfZRevert = nullptr;

    int* pabSuccess = static_cast<int *>(
        VSI_MALLOC3_VERBOSE(sizeof(int), nSteps + 1, nSteps + 1));
    double* padfX = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double) * 3, nSteps + 1, nSteps + 1));
    double* padfXRevert = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double) * 3, nSteps + 1, nSteps + 1));
    if( pabSuccess == nullptr || padfX == nullptr || padfXRevert == nullptr )
    {
        CPLFree( padfX );
        CPLFree( padfXRevert );
        CPLFree( pabSuccess );
        if( nSteps > 20 )
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

    // Take N_STEPS steps.
    for( int iStep = 0; iStep <= nSteps; iStep++ )
    {
        double dfRatio = (iStep == nSteps) ? 1.0 : iStep * dfStep;

        // Along top.
        padfX[iStep] = dfRatio * nInXSize;
        padfY[iStep] = 0.0;
        padfZ[iStep] = 0.0;

        // Along bottom.
        padfX[nSteps + 1 + iStep] = dfRatio * nInXSize;
        padfY[nSteps + 1 + iStep] = nInYSize;
        padfZ[nSteps + 1 + iStep] = 0.0;

        // Along left.
        padfX[2 * (nSteps + 1) + iStep] = 0.0;
        padfY[2 * (nSteps + 1) + iStep] = dfRatio * nInYSize;
        padfZ[2 * (nSteps + 1) + iStep] = 0.0;

        // Along right.
        padfX[3 * (nSteps + 1) + iStep] = nInXSize;
        padfY[3 * (nSteps + 1) + iStep] = dfRatio * nInYSize;
        padfZ[3 * (nSteps + 1) + iStep] = 0.0;
    }

    int nSamplePoints = 4 * (nSteps + 1);

    memset( pabSuccess, 1, sizeof(int) * nSampleMax );

/* -------------------------------------------------------------------- */
/*      Transform them to the output coordinate system.                 */
/* -------------------------------------------------------------------- */
    int nFailedCount = 0;

    if( !pfnTransformer( pTransformArg, FALSE, nSamplePoints,
                         padfX, padfY, padfZ, pabSuccess ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GDALSuggestedWarpOutput() failed because the passed "
                  "transformer failed." );
        CPLFree( padfX );
        CPLFree( padfXRevert );
        CPLFree( pabSuccess );
        return CE_Failure;
    }

    constexpr int SIGN_FINAL_UNINIT = -2;
    constexpr int SIGN_FINAL_INVALID = 0;
    int iSignDiscontinuity = SIGN_FINAL_UNINIT;
    for( int i = 0; i < nSamplePoints; i++ )
    {
        if( !pabSuccess[i] )
            nFailedCount++;
        else
        {
            // Fix for https://trac.osgeo.org/gdal/ticket/7243
            // where echo "-2050000.000 2050000.000" |
            //              gdaltransform -s_srs EPSG:3411 -t_srs EPSG:4326
            // gives "-180 63.691332898492"
            // but we would rather like 180
            if( iSignDiscontinuity == 1 || iSignDiscontinuity == -1 )
            {
                if( !((iSignDiscontinuity * padfX[i] > 0 &&
                       iSignDiscontinuity * padfX[i] <= 180.0) ||
                      (fabs(padfX[i] - iSignDiscontinuity * -180.0) < 1e-8) ) )
                {
                    iSignDiscontinuity = SIGN_FINAL_INVALID;
                }
            }
            else if( iSignDiscontinuity == SIGN_FINAL_UNINIT )
            {
                for( int iSign = -1; iSign <= 1; iSign += 2 )
                {
                    if( (iSign * padfX[i] > 0 && iSign * padfX[i] <= 180.0) ||
                        (fabs(padfX[i] - iSign * -180.0) < 1e-8) )
                    {
                        iSignDiscontinuity = iSign;
                        break;
                    }
                }
                if( iSignDiscontinuity == SIGN_FINAL_UNINIT )
                {
                    iSignDiscontinuity = SIGN_FINAL_INVALID;
                }
            }
        }
    }

    if( iSignDiscontinuity == 1 || iSignDiscontinuity == -1 )
    {
        for( int i = 0; i < nSamplePoints; i++ )
        {
            if( pabSuccess[i] )
            {
                if( fabs(padfX[i] - iSignDiscontinuity * -180.0) < 1e-8 )
                {
                    double axTemp[2] = { iSignDiscontinuity * -180.0,
                                         iSignDiscontinuity * 180.0 };
                    double ayTemp[2] = { padfY[i], padfY[i] };
                    double azTemp[2] = { padfZ[i], padfZ[i] };
                    int abSuccess[2] = {FALSE, FALSE};
                    if( pfnTransformer(pTransformArg, TRUE, 2,
                                       axTemp, ayTemp, azTemp, abSuccess) &&
                        fabs(axTemp[0] - axTemp[1]) < 1e-8 &&
                        fabs(ayTemp[0] - ayTemp[1]) < 1e-8 )
                    {
                        padfX[i] = iSignDiscontinuity * 180.0;
                    }
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if the computed target coordinates are revertable.        */
/*      If not, try the detailed grid sampling.                         */
/* -------------------------------------------------------------------- */
    if( nFailedCount == 0 )
    {
        memcpy(padfXRevert, padfX, nSamplePoints * sizeof(double));
        memcpy(padfYRevert, padfY, nSamplePoints * sizeof(double));
        memcpy(padfZRevert, padfZ, nSamplePoints * sizeof(double));
        if( !pfnTransformer(pTransformArg, TRUE, nSamplePoints,
                            padfXRevert, padfYRevert, padfZRevert, pabSuccess) )
        {
            nFailedCount = 1;
        }
        else
        {
            for( int i = 0; nFailedCount == 0 && i < nSamplePoints; i++ )
            {
                if( !pabSuccess[i] )
                {
                    nFailedCount++;
                    break;
                }

                double dfRatio = (i % (nSteps + 1)) * dfStep;
                if( dfRatio > 0.99 )
                    dfRatio = 1.0;

                double dfExpectedX = 0.0;
                double dfExpectedY = 0.0;
                if( i < nSteps + 1 )
                {
                    dfExpectedX = dfRatio * nInXSize;
                }
                else if( i < 2 * (nSteps + 1) )
                {
                    dfExpectedX = dfRatio * nInXSize;
                    dfExpectedY = nInYSize;
                }
                else if( i < 3 * (nSteps + 1) )
                {
                    dfExpectedY   = dfRatio * nInYSize;
                }
                else
                {
                    dfExpectedX   = nInXSize;
                    dfExpectedY   = dfRatio * nInYSize;
                }

                if( fabs(padfXRevert[i] - dfExpectedX) > nInXSize /
                    static_cast<double>(nSteps) ||
                    fabs(padfYRevert[i] - dfExpectedY) > nInYSize /
                    static_cast<double>(nSteps) )
                    nFailedCount++;
            }
            if( nFailedCount != 0 )
                CPLDebug("WARP",
                         "At least one point failed after revert transform");
        }
    }
    else
    {
        CPLDebug("WARP", "At least one point failed after direct transform");
    }

/* -------------------------------------------------------------------- */
/*      If any of the edge points failed to transform, we need to       */
/*      build a fairly detailed internal grid of points instead to      */
/*      help identify the area that is transformable.                   */
/* -------------------------------------------------------------------- */
    if( nFailedCount > 0 )
    {
        nSamplePoints = 0;

        // Take N_STEPS steps.
        for( int iStep = 0; iStep <= nSteps; iStep++ )
        {
            double dfRatio = (iStep == nSteps) ? 1.0 : iStep * dfStep;

            for( int iStep2 = 0; iStep2 <= nSteps; iStep2++ )
            {
                const double dfRatio2 =
                    iStep2 == nSteps ? 1.0 : iStep2 * dfStep;

                // From top to bottom, from left to right.
                padfX[nSamplePoints] = dfRatio2 * nInXSize;
                padfY[nSamplePoints] = dfRatio * nInYSize;
                padfZ[nSamplePoints] = 0.0;
                nSamplePoints++;
            }
        }

        CPLAssert( nSamplePoints == nSampleMax );

        if( !pfnTransformer( pTransformArg, FALSE, nSamplePoints,
                             padfX, padfY, padfZ, pabSuccess ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALSuggestedWarpOutput() failed because the passed"
                     "transformer failed.");

            CPLFree( padfX );
            CPLFree( padfXRevert );
            CPLFree( pabSuccess );

            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect the bounds, ignoring any failed points.                 */
/* -------------------------------------------------------------------- */
    double dfMinXOut = 0.0;
    double dfMinYOut = 0.0;
    double dfMaxXOut = 0.0;
    double dfMaxYOut = 0.0;
    bool bGotInitialPoint = false;

    nFailedCount = 0;
    for( int i = 0; i < nSamplePoints; i++ )
    {
        int x_i = 0;
        int y_i = 0;

        if( nSamplePoints == nSampleMax )
        {
            x_i = i % (nSteps + 1);
            y_i = i / (nSteps + 1);
        }
        else
        {
            if( i < 2 * (nSteps + 1 ) )
            {
                x_i = i % (nSteps + 1);
                y_i = (i < nSteps + 1) ? 0 : nSteps;
            }
            else
                x_i = y_i = 0;
        }

        if( x_i > 0 && (pabSuccess[i-1] || pabSuccess[i]) )
        {
            double x_out_before = padfX[i-1];
            double x_out_after = padfX[i];
            int nIter = 0;
            double x_in_before =
                static_cast<double>(x_i - 1) * nInXSize / nSteps;
            double x_in_after =
                static_cast<double>(x_i) * nInXSize / nSteps;
            int valid_before = pabSuccess[i-1];
            int valid_after = pabSuccess[i];

            // Detect discontinuity in target coordinates when the target x
            // coordinates change sign. This may be a false positive when the
            // target tx is around 0 Dichotomic search to reduce the interval
            // to near the discontinuity and get a better out extent.
            while( (!valid_before || !valid_after ||
                    x_out_before * x_out_after < 0.0) && nIter < 16 )
            {
                double x = (x_in_before + x_in_after) / 2.0;
                double y = static_cast<double>(y_i) * nInYSize / nSteps;
                double z = 0.0;
                int bSuccess = TRUE;
                if( !pfnTransformer( pTransformArg, FALSE, 1,
                                     &x, &y, &z, &bSuccess ) || !bSuccess )
                {
                    if( !valid_before )
                    {
                        x_in_before = (x_in_before + x_in_after) / 2.0;
                    }
                    else if( !valid_after )
                    {
                        x_in_after = (x_in_before + x_in_after) / 2.0;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    if( !bGotInitialPoint )
                    {
                        bGotInitialPoint = true;
                        dfMinXOut = x;
                        dfMaxXOut = x;
                        dfMinYOut = y;
                        dfMaxYOut = y;
                    }
                    else
                    {
                        dfMinXOut = std::min(dfMinXOut, x);
                        dfMinYOut = std::min(dfMinYOut, y);
                        dfMaxXOut = std::max(dfMaxXOut, x);
                        dfMaxYOut = std::max(dfMaxYOut, y);
                    }

                    if( !valid_before || x_out_before * x < 0 )
                    {
                        valid_after = TRUE;
                        x_in_after = (x_in_before + x_in_after) / 2.0;
                        x_out_after = x;
                    }
                    else
                    {
                        valid_before = TRUE;
                        x_out_before = x;
                        x_in_before = (x_in_before + x_in_after) / 2.0;
                    }
                }
                nIter++;
            }
        }

        if( !pabSuccess[i] )
        {
            nFailedCount++;
            continue;
        }

        if( !bGotInitialPoint )
        {
            bGotInitialPoint = true;
            dfMinXOut = padfX[i];
            dfMaxXOut = padfX[i];
            dfMinYOut = padfY[i];
            dfMaxYOut = padfY[i];
        }
        else
        {
            dfMinXOut = std::min(dfMinXOut, padfX[i]);
            dfMinYOut = std::min(dfMinYOut, padfY[i]);
            dfMaxXOut = std::max(dfMaxXOut, padfX[i]);
            dfMaxYOut = std::max(dfMaxYOut, padfY[i]);
        }
    }

    if( nFailedCount > nSamplePoints - 10 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many points (%d out of %d) failed to transform, "
                 "unable to compute output bounds.",
                 nFailedCount, nSamplePoints);

        CPLFree( padfX );
        CPLFree( padfXRevert );
        CPLFree( pabSuccess );

        return CE_Failure;
    }

    if( nFailedCount > 0 )
        CPLDebug("GDAL",
                 "GDALSuggestedWarpOutput(): %d out of %d points failed to "
                 "transform.",
                 nFailedCount, nSamplePoints);

/* -------------------------------------------------------------------- */
/*      Compute the distance in "georeferenced" units from the top      */
/*      corner of the transformed input image to the bottom left        */
/*      corner of the transformed input.  Use this distance to          */
/*      compute an approximate pixel size in the output                 */
/*      georeferenced coordinates.                                      */
/* -------------------------------------------------------------------- */
    double dfDiagonalDist = 0.0;
    double dfDeltaX = 0.0;
    double dfDeltaY = 0.0;

    if( pabSuccess[0] && pabSuccess[nSamplePoints - 1] )
    {
        dfDeltaX = padfX[nSamplePoints-1] - padfX[0];
        dfDeltaY = padfY[nSamplePoints-1] - padfY[0];
        // In some cases this can result in 0 values. See #5980
        // Fallback to safer method in that case.
    }
    if( dfDeltaX == 0.0 || dfDeltaY == 0.0 )
    {
        dfDeltaX = dfMaxXOut - dfMinXOut;
        dfDeltaY = dfMaxYOut - dfMinYOut;
    }

    dfDiagonalDist = sqrt( dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY );

/* -------------------------------------------------------------------- */
/*      Compute a pixel size from this.                                 */
/* -------------------------------------------------------------------- */
    const double dfPixelSize =
        dfDiagonalDist /
        sqrt(static_cast<double>(nInXSize) * nInXSize +
             static_cast<double>(nInYSize) * nInYSize);

    const double dfPixels = (dfMaxXOut - dfMinXOut) / dfPixelSize;
    const double dfLines = (dfMaxYOut - dfMinYOut) / dfPixelSize;

    const int knIntMax = std::numeric_limits<int>::max();
    if( dfPixels > knIntMax - 1 || dfLines > knIntMax - 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Computed dimensions are too big : %.0f x %.0f",
                  dfPixels + 0.5, dfLines + 0.5 );

        CPLFree( padfX );
        CPLFree( padfXRevert );
        CPLFree( pabSuccess );

        return CE_Failure;
    }

    *pnPixels = static_cast<int>(dfPixels + 0.5);
    *pnLines = static_cast<int>(dfLines + 0.5);

    double dfPixelSizeX = dfPixelSize;
    double dfPixelSizeY = dfPixelSize;

    const double adfRatioArray[] = { 0.000, 0.001, 0.010, 0.100, 1.000 };

/* -------------------------------------------------------------------- */
/*      Check that the right border is not completely out of source     */
/*      image. If so, adjust the x pixel size a bit in the hope it will */
/*      fit.                                                            */
/* -------------------------------------------------------------------- */
    for( const auto& dfRatio : adfRatioArray )
    {
        const double dfTryPixelSizeX =
            dfPixelSizeX - dfPixelSizeX * dfRatio / *pnPixels;
        double adfExtent[4] = {
            dfMinXOut,
            dfMaxYOut - (*pnLines) * dfPixelSizeY,
            dfMinXOut + (*pnPixels) * dfTryPixelSizeX,
            dfMaxYOut
        };
        if( !GDALSuggestedWarpOutput2_MustAdjustForRightBorder(
                                            pfnTransformer, pTransformArg,
                                            adfExtent, *pnPixels,  *pnLines,
                                            dfTryPixelSizeX, dfPixelSizeY) )
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
    for( const auto& dfRatio : adfRatioArray )
    {
        const double dfTryPixelSizeY =
            dfPixelSizeY - dfPixelSizeY * dfRatio / *pnLines;
        double adfExtent[4] = {
            dfMinXOut,
            dfMaxYOut - (*pnLines) * dfTryPixelSizeY,
            dfMinXOut + (*pnPixels) * dfPixelSizeX,
            dfMaxYOut
        };
        if( !GDALSuggestedWarpOutput2_MustAdjustForBottomBorder(
                                            pfnTransformer, pTransformArg,
                                            adfExtent, *pnPixels,  *pnLines,
                                            dfPixelSizeX, dfTryPixelSizeY) )
        {
            dfPixelSizeY = dfTryPixelSizeY;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Recompute some bounds so that all return values are consistent  */
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
/*                       GDALGenImgProjTransformer                      */
/* ==================================================================== */
/************************************************************************/

typedef struct {

    GDALTransformerInfo sTI;

    double   adfSrcGeoTransform[6];
    double   adfSrcInvGeoTransform[6];

    void     *pSrcTransformArg;
    GDALTransformerFunc pSrcTransformer;

    void     *pReprojectArg;
    GDALTransformerFunc pReproject;

    double   adfDstGeoTransform[6];
    double   adfDstInvGeoTransform[6];

    void     *pDstTransformArg;
    GDALTransformerFunc pDstTransformer;

} GDALGenImgProjTransformInfo;

/************************************************************************/
/*                GDALCreateSimilarGenImgProjTransformer()              */
/************************************************************************/

static void *
GDALCreateSimilarGenImgProjTransformer( void *hTransformArg,
                                        double dfRatioX, double dfRatioY )
{
    VALIDATE_POINTER1( hTransformArg,
                       "GDALCreateSimilarGenImgProjTransformer", nullptr );

    GDALGenImgProjTransformInfo *psInfo =
        static_cast<GDALGenImgProjTransformInfo *>(hTransformArg);

    GDALGenImgProjTransformInfo *psClonedInfo =
        static_cast<GDALGenImgProjTransformInfo *>(
            CPLMalloc(sizeof(GDALGenImgProjTransformInfo)));

    memcpy(psClonedInfo, psInfo, sizeof(GDALGenImgProjTransformInfo));

    if( psClonedInfo->pSrcTransformArg )
        psClonedInfo->pSrcTransformArg =
            GDALCreateSimilarTransformer( psInfo->pSrcTransformArg,
                                          dfRatioX, dfRatioY );
    else if( dfRatioX != 1.0 || dfRatioY != 1.0 )
    {
        if( psClonedInfo->adfSrcGeoTransform[2] == 0.0 &&
            psClonedInfo->adfSrcGeoTransform[4] == 0.0 )
        {
            psClonedInfo->adfSrcGeoTransform[1] *= dfRatioX;
            psClonedInfo->adfSrcGeoTransform[5] *= dfRatioY;
        }
        else
        {
            // If the x and y ratios are not equal, then we cannot really
            // compute a geotransform.
            psClonedInfo->adfSrcGeoTransform[1] *= dfRatioX;
            psClonedInfo->adfSrcGeoTransform[2] *= dfRatioX;
            psClonedInfo->adfSrcGeoTransform[4] *= dfRatioX;
            psClonedInfo->adfSrcGeoTransform[5] *= dfRatioX;
        }
        if( !GDALInvGeoTransform( psClonedInfo->adfSrcGeoTransform,
                                  psClonedInfo->adfSrcInvGeoTransform ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            GDALDestroyGenImgProjTransformer( psClonedInfo );
            return nullptr;
        }
    }

    if( psClonedInfo->pReprojectArg )
        psClonedInfo->pReprojectArg =
            GDALCloneTransformer( psInfo->pReprojectArg );

    if( psClonedInfo->pDstTransformArg )
        psClonedInfo->pDstTransformArg =
            GDALCloneTransformer( psInfo->pDstTransformArg );

    return psClonedInfo;
}

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
                                 int bGCPUseOK,
                                 CPL_UNUSED double dfGCPErrorThreshold,
                                 int nOrder )
{
    char **papszOptions = nullptr;

    if( pszSrcWKT != nullptr )
        papszOptions = CSLSetNameValue( papszOptions, "SRC_SRS", pszSrcWKT );
    if( pszDstWKT != nullptr )
        papszOptions = CSLSetNameValue( papszOptions, "DST_SRS", pszDstWKT );
    if( !bGCPUseOK )
        papszOptions = CSLSetNameValue( papszOptions, "GCPS_OK", "FALSE" );
    if( nOrder != 0 )
        papszOptions = CSLSetNameValue( papszOptions, "MAX_GCP_ORDER",
                                        CPLString().Printf("%d", nOrder) );

    void *pRet =
        GDALCreateGenImgProjTransformer2( hSrcDS, hDstDS, papszOptions );
    CSLDestroy( papszOptions );

    return pRet;
}

/************************************************************************/
/*                          InsertCenterLong()                          */
/*                                                                      */
/*      Insert a CENTER_LONG Extension entry on a GEOGCS to indicate    */
/*      the center longitude of the dataset for wrapping purposes.      */
/************************************************************************/

static void InsertCenterLong( GDALDatasetH hDS, OGRSpatialReference* poSRS,
                              CPLStringList& aosOptions )

{
    if( !poSRS->IsGeographic())
        return;

    if( poSRS->GetExtension(nullptr, "CENTER_LONG") )
        return;

/* -------------------------------------------------------------------- */
/*      For now we only do this if we have a geotransform since         */
/*      other forms require a bunch of extra work.                      */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {};

    if( GDALGetGeoTransform( hDS, adfGeoTransform ) != CE_None )
        return;

/* -------------------------------------------------------------------- */
/*      Compute min/max longitude based on testing the four corners.    */
/* -------------------------------------------------------------------- */
    const int nXSize = GDALGetRasterXSize( hDS );
    const int nYSize = GDALGetRasterYSize( hDS );

    const double dfMinLong =
        std::min(std::min(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + 0 * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + 0 * adfGeoTransform[2]),
            std::min(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2]));
    const double dfMaxLong =
        std::max(std::max(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + 0 * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + 0 * adfGeoTransform[2]),
            std::max(adfGeoTransform[0] + 0 * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2],
                adfGeoTransform[0] + nXSize * adfGeoTransform[1]
                + nYSize * adfGeoTransform[2]));

    if( dfMaxLong - dfMinLong > 360.0 )
        return;

/* -------------------------------------------------------------------- */
/*      Insert center long.                                             */
/* -------------------------------------------------------------------- */
    const double dfCenterLong = (dfMaxLong + dfMinLong) / 2.0;
    aosOptions.SetNameValue("CENTER_LONG",
                            CPLSPrintf("%g", dfCenterLong));
}

/************************************************************************/
/*               GDALCreateGenImgProjTransformerInternal()              */
/************************************************************************/

static GDALGenImgProjTransformInfo* GDALCreateGenImgProjTransformerInternal()
{
/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    GDALGenImgProjTransformInfo* psInfo =
        static_cast<GDALGenImgProjTransformInfo *>(
            CPLCalloc(sizeof(GDALGenImgProjTransformInfo), 1));

    memcpy( psInfo->sTI.abySignature,
            GDAL_GTI2_SIGNATURE,
            strlen(GDAL_GTI2_SIGNATURE) );
    psInfo->sTI.pszClassName = "GDALGenImgProjTransformer";
    psInfo->sTI.pfnTransform = GDALGenImgProjTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyGenImgProjTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeGenImgProjTransformer;
    psInfo->sTI.pfnCreateSimilar = GDALCreateSimilarGenImgProjTransformer;

    return psInfo;
}

/************************************************************************/
/*                      GDALComputeAreaOfInterest()                     */
/************************************************************************/

bool GDALComputeAreaOfInterest(OGRSpatialReference* poSRS,
                               double adfGT[6],
                               int nXSize,
                               int nYSize,
                               double& dfWestLongitudeDeg,
                               double& dfSouthLatitudeDeg,
                               double& dfEastLongitudeDeg,
                               double& dfNorthLatitudeDeg )
{
    bool ret = false;

    if( !poSRS )
        return false;

    OGRSpatialReference oSrcSRSHoriz(*poSRS);
    if( oSrcSRSHoriz.IsCompound() )
    {
        oSrcSRSHoriz.StripVertical();
    }

    OGRSpatialReference* poGeog = oSrcSRSHoriz.CloneGeogCS();
    if( poGeog )
    {
        poGeog->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poGeog->SetAngularUnits(SRS_UA_DEGREE, CPLAtof(SRS_UA_DEGREE_CONV));

        auto poCT = OGRCreateCoordinateTransformation(&oSrcSRSHoriz, poGeog);
        if( poCT )
        {
            double x[4], y[4];
            x[0] = adfGT[0];
            y[0] = adfGT[3];
            x[1] = adfGT[0] + nXSize * adfGT[1];
            y[1] = adfGT[3];
            x[2] = adfGT[0];
            y[2] = adfGT[3] + nYSize * adfGT[5];
            x[3] = x[1];
            y[3] = y[2];
            int validity[4] = {false,false,false,false};
            poCT->Transform(4, x, y, nullptr, validity);
            dfWestLongitudeDeg = std::numeric_limits<double>::max();
            dfSouthLatitudeDeg = std::numeric_limits<double>::max();
            dfEastLongitudeDeg = -std::numeric_limits<double>::max();
            dfNorthLatitudeDeg = -std::numeric_limits<double>::max();
            for( int i = 0; i < 4; i++ )
            {
                if( validity[i] )
                {
                    ret = true;
                    dfWestLongitudeDeg = std::min(dfWestLongitudeDeg, x[i]);
                    dfSouthLatitudeDeg = std::min(dfSouthLatitudeDeg, y[i]);
                    dfEastLongitudeDeg = std::max(dfEastLongitudeDeg, x[i]);
                    dfNorthLatitudeDeg = std::max(dfNorthLatitudeDeg, y[i]);
                }
            }
            if( validity[0] && validity[1] && x[0] > x[1] )
            {
                dfWestLongitudeDeg = x[0];
                dfEastLongitudeDeg = x[1];
            }
            if( ret &&
                std::fabs(dfWestLongitudeDeg) <= 180 &&
                std::fabs(dfEastLongitudeDeg) <= 180 &&
                std::fabs(dfSouthLatitudeDeg) <= 90 &&
                std::fabs(dfNorthLatitudeDeg) <= 90 )
            {
                CPLDebug("GDAL", "Computing area of interest: %g, %g, %g, %g",
                        dfWestLongitudeDeg, dfSouthLatitudeDeg,
                        dfEastLongitudeDeg, dfNorthLatitudeDeg);
            }
            else
            {
                CPLDebug("GDAL", "Could not compute area of interest");
                dfWestLongitudeDeg = 0;
                dfSouthLatitudeDeg = 0;
                dfEastLongitudeDeg = 0;
                dfNorthLatitudeDeg = 0;
            }
            delete poCT;
        }

        delete poGeog;
    }

    return ret;
}

bool GDALComputeAreaOfInterest(OGRSpatialReference* poSRS,
                               double dfX1,
                               double dfY1,
                               double dfX2,
                               double dfY2,
                               double& dfWestLongitudeDeg,
                               double& dfSouthLatitudeDeg,
                               double& dfEastLongitudeDeg,
                               double& dfNorthLatitudeDeg )
{
    bool ret = false;

    if( !poSRS )
        return false;

    OGRSpatialReference oSrcSRSHoriz(*poSRS);
    if( oSrcSRSHoriz.IsCompound() )
    {
        oSrcSRSHoriz.StripVertical();
    }

    OGRSpatialReference* poGeog = oSrcSRSHoriz.CloneGeogCS();
    if( poGeog )
    {
        poGeog->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        auto poCT = OGRCreateCoordinateTransformation(&oSrcSRSHoriz, poGeog);
        if( poCT )
        {
            double x[4], y[4];
            x[0] = dfX1;
            y[0] = dfY1;
            x[1] = dfX2;
            y[1] = dfY1;
            x[2] = dfX1;
            y[2] = dfY2;
            x[3] = dfX2;
            y[3] = dfY2;
            int validity[4] = {false,false,false,false};
            poCT->Transform(4, x, y, nullptr, validity);
            dfWestLongitudeDeg = std::numeric_limits<double>::max();
            dfSouthLatitudeDeg = std::numeric_limits<double>::max();
            dfEastLongitudeDeg = -std::numeric_limits<double>::max();
            dfNorthLatitudeDeg = -std::numeric_limits<double>::max();
            for( int i = 0; i < 4; i++ )
            {
                if( validity[i] )
                {
                    ret = true;
                    dfWestLongitudeDeg = std::min(dfWestLongitudeDeg, x[i]);
                    dfSouthLatitudeDeg = std::min(dfSouthLatitudeDeg, y[i]);
                    dfEastLongitudeDeg = std::max(dfEastLongitudeDeg, x[i]);
                    dfNorthLatitudeDeg = std::max(dfNorthLatitudeDeg, y[i]);
                }
            }
            if( validity[0] && validity[1] && (dfX1 - dfX2) * (x[0] - x[1]) < 0 )
            {
                dfWestLongitudeDeg = x[0];
                dfEastLongitudeDeg = x[1];
            }
            if( ret )
            {
                CPLDebug("GDAL", "Computing area of interest: %g, %g, %g, %g",
                        dfWestLongitudeDeg, dfSouthLatitudeDeg,
                        dfEastLongitudeDeg, dfNorthLatitudeDeg);
            }
            else
            {
                CPLDebug("GDAL", "Could not compute area of interest");
                dfWestLongitudeDeg = 0;
                dfSouthLatitudeDeg = 0;
                dfEastLongitudeDeg = 0;
                dfNorthLatitudeDeg = 0;
            }
            delete poCT;
        }

        delete poGeog;
    }

    return ret;
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
 * Supported Options (specified with the -to switch of gdalwarp for example):
 * <ul>
 * <li> SRC_SRS: WKT SRS, or any string recognized by
 * OGRSpatialReference::SetFromUserInput(), to be used as an override for hSrcDS.
 * <li> DST_SRS: WKT SRS, or any string recognized by
 * OGRSpatialReference::SetFromUserInput(),  to be used as an override for hDstDS.
 * <li> COORDINATE_OPERATION: (GDAL &gt;= 3.0) Coordinate operation, as a
 * PROJ or WKT string, used as an override over the normally computed pipeline.
 * The pipeline must take into account the axis order of the source and target
 * SRS.
 * <li> COORDINATE_EPOCH: (GDAL &gt;= 3.0) Coordinate epoch, expressed as a
 * decimal year. Useful for time-dependant coordinate operations.
 * <li> GCPS_OK: If false, GCPs will not be used, default is TRUE.
 * <li> REFINE_MINIMUM_GCPS: The minimum amount of GCPs that should be available
 * after the refinement.
 * <li> REFINE_TOLERANCE: The tolerance that specifies when a GCP will be
 * eliminated.
 * <li> MAX_GCP_ORDER: the maximum order to use for GCP derived polynomials if
 * possible.  The default is to autoselect based on the number of GCPs.
 * A value of -1 triggers use of Thin Plate Spline instead of polynomials.
 * <li> SRC_METHOD: may have a value which is one of GEOTRANSFORM,
 * GCP_POLYNOMIAL, GCP_TPS, GEOLOC_ARRAY, RPC to force only one geolocation
 * method to be considered on the source dataset. Will be used for pixel/line
 * to georef transformation on the source dataset. NO_GEOTRANSFORM can be
 * used to specify the identity geotransform (ungeoreference image)
 * <li> DST_METHOD: may have a value which is one of GEOTRANSFORM,
 * GCP_POLYNOMIAL, GCP_TPS, GEOLOC_ARRAY, RPC to force only one geolocation
 * method to be considered on the target dataset.  Will be used for pixel/line
 * to georef transformation on the destination dataset. NO_GEOTRANSFORM can be
 * used to specify the identity geotransform (ungeoreference image)
 * <li> RPC_HEIGHT: A fixed height to be used with RPC calculations.
 * <li> RPC_DEM: The name of a DEM file to be used with RPC calculations.
 * <li> Other RPC related options. See GDALCreateRPCTransformer()
 * <li> INSERT_CENTER_LONG: May be set to FALSE to disable setting up a
 * CENTER_LONG value on the coordinate system to rewrap things around the
 * center of the image.
 * <li> SRC_APPROX_ERROR_IN_SRS_UNIT=err_threshold_in_SRS_units. (GDAL &gt;= 2.2) Use an
 * approximate transformer for the source transformer. Must be defined together
 * with SRC_APPROX_ERROR_IN_PIXEL to be taken into account.
 * <li> SRC_APPROX_ERROR_IN_PIXEL=err_threshold_in_pixel. (GDAL &gt;= 2.2) Use an
 * approximate transformer for the source transformer.. Must be defined together
 * with SRC_APPROX_ERROR_IN_SRS_UNIT to be taken into account.
 * <li> DST_APPROX_ERROR_IN_SRS_UNIT=err_threshold_in_SRS_units. (GDAL &gt;= 2.2) Use an
 * approximate transformer for the destination transformer. Must be defined together
 * with DST_APPROX_ERROR_IN_PIXEL to be taken into account.
 * <li> DST_APPROX_ERROR_IN_PIXEL=err_threshold_in_pixel. (GDAL &gt;= 2.2) Use an
 * approximate transformer for the destination transformer. Must be defined together
 * with DST_APPROX_ERROR_IN_SRS_UNIT to be taken into account.
 * <li> REPROJECTION_APPROX_ERROR_IN_SRC_SRS_UNIT=err_threshold_in_src_SRS_units.
 * (GDAL &gt;= 2.2) Use an approximate transformer for the coordinate reprojection.
 * Must be used together with REPROJECTION_APPROX_ERROR_IN_DST_SRS_UNIT to be taken
 * into account.
 * <li> REPROJECTION_APPROX_ERROR_IN_DST_SRS_UNIT=err_threshold_in_dst_SRS_units.
 * (GDAL &gt;= 2.2) Use an approximate transformer for the coordinate reprojection.
 * Must be used together with REPROJECTION_APPROX_ERROR_IN_SRC_SRS_UNIT to be taken
 * into account.
 * <li> AREA_OF_INTEREST=west_lon_deg,south_lat_deg,east_lon_deg,north_lat_deg.
 * (GDAL &gt;= 3.0) Area of interest, used to compute the best coordinate operation
 * between the source and target SRS. If not specified, the bounding box of the
 * source raster will be used.
 * </ul>
 *
 * The use case for the *_APPROX_ERROR_* options is when defining an approximate
 * transformer on top of the GenImgProjTransformer globally is not practical.
 * Such a use case is when the source dataset has RPC with a RPC DEM. In such
 * case we don't want to use the approximate transformer on the RPC transformation,
 * as the RPC DEM generally involves non-linearities that the approximate
 * transformer will not detect. In such case, we must a non-approximated
 * GenImgProjTransformer, but it might be worthwhile to use approximate sub-
 * transformers, for example on coordinate reprojection. For example if
 * warping from a source dataset with RPC to a destination dataset with
 * a UTM projection, since the inverse UTM transformation is rather costly.
 * In which case, one can use the REPROJECTION_APPROX_ERROR_IN_SRC_SRS_UNIT and
 * REPROJECTION_APPROX_ERROR_IN_DST_SRS_UNIT options.
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
    char **papszMD = nullptr;
    GDALRPCInfoV2 sRPCInfo;
    const char *pszMethod = CSLFetchNameValue( papszOptions, "SRC_METHOD" );
    if( pszMethod == nullptr )
        pszMethod = CSLFetchNameValue( papszOptions, "METHOD" );
    const char *pszSrcSRS = CSLFetchNameValue( papszOptions, "SRC_SRS" );
    const char *pszDstSRS = CSLFetchNameValue( papszOptions, "DST_SRS" );

    const char *pszValue = CSLFetchNameValue( papszOptions, "MAX_GCP_ORDER" );
    const int nOrder = pszValue ? atoi(pszValue) : 0;

    pszValue = CSLFetchNameValue( papszOptions, "GCPS_OK" );
    // TODO(schwehr): Why does this upset DEBUG_BOOL?
    const bool bGCPUseOK = pszValue ? CPLTestBool(pszValue) : true;

    pszValue = CSLFetchNameValue( papszOptions, "REFINE_MINIMUM_GCPS" );
    const int nMinimumGcps =  pszValue ? atoi(pszValue) : -1;

    pszValue = CSLFetchNameValue( papszOptions, "REFINE_TOLERANCE" );
    const bool bRefine = pszValue != nullptr;
    const double dfTolerance = pszValue ? CPLAtof(pszValue) : 0.0;

    double dfWestLongitudeDeg = 0.0;
    double dfSouthLatitudeDeg = 0.0;
    double dfEastLongitudeDeg = 0.0;
    double dfNorthLatitudeDeg = 0.0;
    bool bHasAreaOfInterest = false;
    pszValue = CSLFetchNameValue( papszOptions, "AREA_OF_INTEREST" );
    if( pszValue )
    {
        char** papszTokens = CSLTokenizeString2( pszValue, ", ", 0 );
        if( CSLCount(papszTokens) == 4 )
        {
            dfWestLongitudeDeg = CPLAtof(papszTokens[0]);
            dfSouthLatitudeDeg = CPLAtof(papszTokens[1]);
            dfEastLongitudeDeg = CPLAtof(papszTokens[2]);
            dfNorthLatitudeDeg = CPLAtof(papszTokens[3]);
            bHasAreaOfInterest = true;
        }
        CSLDestroy(papszTokens);
    }

    const char* pszCO = CSLFetchNameValue(papszOptions, "COORDINATE_OPERATION");

    OGRSpatialReference oSrcSRS;
    if( pszSrcSRS )
    {
        oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( pszSrcSRS[0] != '\0' &&
            oSrcSRS.SetFromUserInput( pszSrcSRS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to import coordinate system `%s'.",
                    pszSrcSRS );
            return nullptr;
        }
    }

    OGRSpatialReference oDstSRS;
    if( pszDstSRS )
    {
        oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( pszDstSRS[0] != '\0' &&
            oDstSRS.SetFromUserInput( pszDstSRS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to import coordinate system `%s'.",
                    pszDstSRS );
            return nullptr;
        }
    }
/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    GDALGenImgProjTransformInfo *psInfo =
        GDALCreateGenImgProjTransformerInternal();

    bool bCanUseSrcGeoTransform = false;

/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for the source image.      */
/* -------------------------------------------------------------------- */
    if( hSrcDS == nullptr ||
        (pszMethod != nullptr && EQUAL(pszMethod, "NO_GEOTRANSFORM")) )
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
    else if( (pszMethod == nullptr || EQUAL(pszMethod, "GEOTRANSFORM"))
             && GDALGetGeoTransform( hSrcDS, psInfo->adfSrcGeoTransform )
             == CE_None
             && (psInfo->adfSrcGeoTransform[0] != 0.0
                 || psInfo->adfSrcGeoTransform[1] != 1.0
                 || psInfo->adfSrcGeoTransform[2] != 0.0
                 || psInfo->adfSrcGeoTransform[3] != 0.0
                 || psInfo->adfSrcGeoTransform[4] != 0.0
                 || std::abs(psInfo->adfSrcGeoTransform[5]) != 1.0) )
    {
        if( !GDALInvGeoTransform( psInfo->adfSrcGeoTransform,
                                  psInfo->adfSrcInvGeoTransform ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        if( pszSrcSRS == nullptr )
        {
            auto hSRS = GDALGetSpatialRef( hSrcDS );
            if( hSRS )
                oSrcSRS = *(OGRSpatialReference::FromHandle(hSRS));
        }
        if( !bHasAreaOfInterest && pszCO == nullptr &&
            !oSrcSRS.IsEmpty() )
        {
            GDALComputeAreaOfInterest(&oSrcSRS,
                                      psInfo->adfSrcGeoTransform,
                                      GDALGetRasterXSize(hSrcDS),
                                      GDALGetRasterYSize(hSrcDS),
                                      dfWestLongitudeDeg,
                                      dfSouthLatitudeDeg,
                                      dfEastLongitudeDeg,
                                      dfNorthLatitudeDeg);
        }
        bCanUseSrcGeoTransform = true;
    }
    else if( bGCPUseOK
             && (pszMethod == nullptr || EQUAL(pszMethod, "GCP_POLYNOMIAL") )
             && GDALGetGCPCount( hSrcDS ) > 0 && nOrder >= 0 )
    {
        if( bRefine )
        {
                psInfo->pSrcTransformArg =
                    GDALCreateGCPRefineTransformer(
                        GDALGetGCPCount( hSrcDS ),
                        GDALGetGCPs( hSrcDS ), nOrder,
                        FALSE, dfTolerance, nMinimumGcps );
        }
        else
        {
            psInfo->pSrcTransformArg =
                GDALCreateGCPTransformer( GDALGetGCPCount( hSrcDS ),
                                          GDALGetGCPs( hSrcDS ), nOrder,
                                          FALSE );
        }

        if( psInfo->pSrcTransformArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pSrcTransformer = GDALGCPTransform;

        if( pszSrcSRS == nullptr )
        {
            auto hSRS = GDALGetGCPSpatialRef( hSrcDS );
            if( hSRS )
                oSrcSRS = *(OGRSpatialReference::FromHandle(hSRS));
        }
    }

    else if( bGCPUseOK
             && GDALGetGCPCount( hSrcDS ) > 0
             && nOrder <= 0
             && (pszMethod == nullptr || EQUAL(pszMethod, "GCP_TPS")) )
    {
        psInfo->pSrcTransformArg =
            GDALCreateTPSTransformerInt( GDALGetGCPCount( hSrcDS ),
                                         GDALGetGCPs( hSrcDS ), FALSE,
                                         papszOptions);
        if( psInfo->pSrcTransformArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pSrcTransformer = GDALTPSTransform;

        if( pszSrcSRS == nullptr )
        {
            auto hSRS = GDALGetGCPSpatialRef( hSrcDS );
            if( hSRS )
                oSrcSRS = *(OGRSpatialReference::FromHandle(hSRS));
        }
    }

    else if( (pszMethod == nullptr || EQUAL(pszMethod, "RPC"))
             && (papszMD = GDALGetMetadata( hSrcDS, "RPC" )) != nullptr
             && GDALExtractRPCInfoV2( papszMD, &sRPCInfo ) )
    {
        psInfo->pSrcTransformArg =
            GDALCreateRPCTransformerV2( &sRPCInfo, FALSE, 0, papszOptions );
        if( psInfo->pSrcTransformArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pSrcTransformer = GDALRPCTransform;
        if( pszSrcSRS == nullptr )
        {
            oSrcSRS.SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
            oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
    }

    else if( (pszMethod == nullptr || EQUAL(pszMethod, "GEOLOC_ARRAY"))
             && (papszMD = GDALGetMetadata( hSrcDS, "GEOLOCATION" )) != nullptr )
    {
        psInfo->pSrcTransformArg =
            GDALCreateGeoLocTransformer( hSrcDS, papszMD, FALSE );

        if( psInfo->pSrcTransformArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pSrcTransformer = GDALGeoLocTransform;
        if( pszSrcSRS == nullptr )
        {
            pszSrcSRS = CSLFetchNameValue( papszMD, "SRS" );
            if( pszSrcSRS )
            {
                oSrcSRS.SetFromUserInput(pszSrcSRS);
                oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            }
        }
    }

    else if( pszMethod != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to compute a %s based transformation between "
                 "pixel/line and georeferenced coordinates for %s.",
                 pszMethod, GDALGetDescription(hSrcDS));

        GDALDestroyGenImgProjTransformer( psInfo );
        return nullptr;
    }

    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The transformation is already \"north up\" or "
                 "a transformation between pixel/line and georeferenced "
                 "coordinates cannot be computed for %s. "
                 "There is no affine transformation and no GCPs. "
                 "Specify transformation option SRC_METHOD=NO_GEOTRANSFORM to "
                 "bypass this check.",
                 GDALGetDescription(hSrcDS));

        GDALDestroyGenImgProjTransformer( psInfo );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Handle optional source approximation transformer.               */
/* -------------------------------------------------------------------- */
    if( psInfo->pSrcTransformer )
    {
        const char* pszSrcApproxErrorFwd = CSLFetchNameValue( papszOptions,
                                                "SRC_APPROX_ERROR_IN_SRS_UNIT" );
        const char* pszSrcApproxErrorReverse = CSLFetchNameValue( papszOptions,
                                                "SRC_APPROX_ERROR_IN_PIXEL" );
        if( pszSrcApproxErrorFwd &&pszSrcApproxErrorReverse  )
        {
            void* pArg = GDALCreateApproxTransformer2( psInfo->pSrcTransformer,
                                            psInfo->pSrcTransformArg,
                                            CPLAtof(pszSrcApproxErrorFwd),
                                            CPLAtof(pszSrcApproxErrorReverse) );
            if( pArg == nullptr )
            {
                GDALDestroyGenImgProjTransformer( psInfo );
                return nullptr;
            }
            psInfo->pSrcTransformArg = pArg;
            psInfo->pSrcTransformer = GDALApproxTransform;
            GDALApproxTransformerOwnsSubtransformer(psInfo->pSrcTransformArg,
                                                    TRUE);
        }
    }

/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for destination image.     */
/*      If we have no destination use a unit transform.                 */
/* -------------------------------------------------------------------- */
    const char *pszDstMethod = CSLFetchNameValue( papszOptions, "DST_METHOD" );

    if( !hDstDS || (pszDstMethod != nullptr &&
                    EQUAL(pszDstMethod, "NO_GEOTRANSFORM"))  )
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
    else if( (pszDstMethod == nullptr || EQUAL(pszDstMethod, "GEOTRANSFORM"))
        && GDALGetGeoTransform( hDstDS, psInfo->adfDstGeoTransform ) == CE_None)
    {
        if( pszDstSRS == nullptr )
        {
            auto hSRS = GDALGetSpatialRef( hDstDS );
            if( hSRS )
                oDstSRS = *(OGRSpatialReference::FromHandle(hSRS));
        }
        if( !GDALInvGeoTransform( psInfo->adfDstGeoTransform,
                                  psInfo->adfDstInvGeoTransform ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
    }
    else if( bGCPUseOK
             && (pszDstMethod == nullptr || EQUAL(pszDstMethod, "GCP_POLYNOMIAL") )
             && GDALGetGCPCount( hDstDS ) > 0 && nOrder >= 0 )
    {
        if( bRefine )
        {
            psInfo->pDstTransformArg =
                GDALCreateGCPRefineTransformer( GDALGetGCPCount( hDstDS ),
                                                GDALGetGCPs( hDstDS ), nOrder,
                                                FALSE, dfTolerance,
                                                nMinimumGcps );
        }
        else
        {
            psInfo->pDstTransformArg =
                GDALCreateGCPTransformer( GDALGetGCPCount( hDstDS ),
                                          GDALGetGCPs( hDstDS ), nOrder,
                                          FALSE );
        }

        if( psInfo->pDstTransformArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pDstTransformer = GDALGCPTransform;

        if( pszDstSRS == nullptr )
        {
            auto hSRS = GDALGetGCPSpatialRef( hDstDS );
            if( hSRS )
                oDstSRS = *(OGRSpatialReference::FromHandle(hSRS));
        }
    }
    else if( bGCPUseOK
             && GDALGetGCPCount( hDstDS ) > 0
             && nOrder <= 0
             && (pszDstMethod == nullptr || EQUAL(pszDstMethod, "GCP_TPS")) )
    {
        psInfo->pDstTransformArg =
            GDALCreateTPSTransformerInt( GDALGetGCPCount( hDstDS ),
                                         GDALGetGCPs( hDstDS ), FALSE,
                                         papszOptions );
        if( psInfo->pDstTransformArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pDstTransformer = GDALTPSTransform;

        if( pszDstSRS == nullptr )
        {
            auto hSRS = GDALGetGCPSpatialRef( hDstDS );
            if( hSRS )
                oDstSRS = *(OGRSpatialReference::FromHandle(hSRS));
        }
    }
    else if( (pszDstMethod == nullptr || EQUAL(pszDstMethod, "RPC"))
             && (papszMD = GDALGetMetadata( hDstDS, "RPC" )) != nullptr
             && GDALExtractRPCInfoV2( papszMD, &sRPCInfo ) )
    {
        psInfo->pDstTransformArg =
            GDALCreateRPCTransformerV2( &sRPCInfo, FALSE, 0, papszOptions );
        if( psInfo->pDstTransformArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pDstTransformer = GDALRPCTransform;
        if( pszDstSRS == nullptr )
        {
            oDstSRS.SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
            oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
    }

    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to compute a transformation between pixel/line "
                 "and georeferenced coordinates for %s. "
                 "There is no affine transformation and no GCPs. "
                 "Specify transformation option DST_METHOD=NO_GEOTRANSFORM "
                 "to bypass this check.",
                 GDALGetDescription(hDstDS));

        GDALDestroyGenImgProjTransformer( psInfo );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Handle optional destination approximation transformer.          */
/* -------------------------------------------------------------------- */
    if( psInfo->pDstTransformer )
    {
        const char* pszDstApproxErrorFwd = CSLFetchNameValue( papszOptions,
                                                "DST_APPROX_ERROR_IN_PIXEL" );
        const char* pszDstApproxErrorReverse = CSLFetchNameValue( papszOptions,
                                                "DST_APPROX_ERROR_IN_SRS_UNIT" );
        if( pszDstApproxErrorFwd &&pszDstApproxErrorReverse  )
        {
            void* pArg = GDALCreateApproxTransformer2( psInfo->pDstTransformer,
                                            psInfo->pDstTransformArg,
                                            CPLAtof(pszDstApproxErrorFwd),
                                            CPLAtof(pszDstApproxErrorReverse) );
            if( pArg == nullptr )
            {
                GDALDestroyGenImgProjTransformer( psInfo );
                return nullptr;
            }
            psInfo->pDstTransformArg = pArg;
            psInfo->pDstTransformer = GDALApproxTransform;
            GDALApproxTransformerOwnsSubtransformer(psInfo->pDstTransformArg,
                                                    TRUE);
        }
    }

/* -------------------------------------------------------------------- */
/*      Setup reprojection.                                             */
/* -------------------------------------------------------------------- */

    if( CPLFetchBool( papszOptions, "STRIP_VERT_CS", false ) )
    {
        if( oSrcSRS.IsCompound() )
        {
            oSrcSRS.StripVertical();
        }
        if( oDstSRS.IsCompound() )
        {
            oDstSRS.StripVertical();
        }
    }

    const bool bMayInsertCenterLong = (bCanUseSrcGeoTransform
            && !oSrcSRS.IsEmpty() && hSrcDS
            && CPLFetchBool( papszOptions, "INSERT_CENTER_LONG", true ));
    if( (!oSrcSRS.IsEmpty() && !oDstSRS.IsEmpty() &&
         (!oSrcSRS.IsSame(&oDstSRS) ||
          (oSrcSRS.IsGeographic() && bMayInsertCenterLong))) || pszCO )
    {
        CPLStringList aosOptions;

        if( bMayInsertCenterLong )
        {
            InsertCenterLong( hSrcDS, &oSrcSRS, aosOptions );
        }

        if( !(dfWestLongitudeDeg == 0.0 && dfSouthLatitudeDeg == 0.0 &&
              dfEastLongitudeDeg == 0.0 && dfNorthLatitudeDeg == 0.0) )
        {
            aosOptions.SetNameValue(
                "AREA_OF_INTEREST",
                CPLSPrintf("%.16g,%.16g,%.16g,%.16g",
                           dfWestLongitudeDeg,
                           dfSouthLatitudeDeg,
                           dfEastLongitudeDeg,
                           dfNorthLatitudeDeg));
        }
        if( pszCO )
        {
            aosOptions.SetNameValue("COORDINATE_OPERATION", pszCO);
        }
        const char* pszCoordEpoch = CSLFetchNameValue(papszOptions,
                                                      "COORDINATE_EPOCH");
        if( pszCoordEpoch )
        {
            aosOptions.SetNameValue("COORDINATE_EPOCH", pszCoordEpoch);
        }

        psInfo->pReprojectArg =
            GDALCreateReprojectionTransformerEx(
                !oSrcSRS.IsEmpty() ? OGRSpatialReference::ToHandle(&oSrcSRS) : nullptr,
                !oDstSRS.IsEmpty() ? OGRSpatialReference::ToHandle(&oDstSRS) : nullptr,
                aosOptions.List());

        if( psInfo->pReprojectArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pReproject = GDALReprojectionTransform;

/* -------------------------------------------------------------------- */
/*      Handle optional reprojection approximation transformer.         */
/* -------------------------------------------------------------------- */
        const char* psApproxErrorFwd = CSLFetchNameValue( papszOptions,
                                    "REPROJECTION_APPROX_ERROR_IN_DST_SRS_UNIT" );
        const char* psApproxErrorReverse = CSLFetchNameValue( papszOptions,
                                    "REPROJECTION_APPROX_ERROR_IN_SRC_SRS_UNIT" );
        if( psApproxErrorFwd && psApproxErrorReverse )
        {
            void* pArg = GDALCreateApproxTransformer2( psInfo->pReproject,
                                                psInfo->pReprojectArg,
                                                CPLAtof(psApproxErrorFwd),
                                                CPLAtof(psApproxErrorReverse) );
            if( pArg == nullptr )
            {
                GDALDestroyGenImgProjTransformer( psInfo );
                return nullptr;
            }
            psInfo->pReprojectArg = pArg;
            psInfo->pReproject = GDALApproxTransform;
            GDALApproxTransformerOwnsSubtransformer(psInfo->pReprojectArg,
                                                    TRUE);
        }
    }

    return psInfo;
}

/************************************************************************/
/*                  GDALRefreshGenImgProjTransformer()                  */
/************************************************************************/

void GDALRefreshGenImgProjTransformer( void* hTransformArg )
{
    GDALGenImgProjTransformInfo *psInfo =
        static_cast<GDALGenImgProjTransformInfo *>( hTransformArg );

    if( psInfo->pReprojectArg )
    {
        CPLXMLNode* psXML =
            GDALSerializeTransformer(psInfo->pReproject,
                                     psInfo->pReprojectArg);
        GDALDestroyTransformer(psInfo->pReprojectArg);
        GDALDeserializeTransformer(psXML,
                                   &psInfo->pReproject,
                                   &psInfo->pReprojectArg);
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
    OGRSpatialReference oSrcSRS;
    if( pszSrcWKT )
    {
        oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( pszSrcWKT[0] != '\0' &&
            oSrcSRS.importFromWkt( pszSrcWKT ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to import coordinate system `%s'.",
                    pszSrcWKT );
            return nullptr;
        }
    }

    OGRSpatialReference oDstSRS;
    if( pszDstWKT )
    {
        oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( pszDstWKT[0] != '\0' &&
            oDstSRS.importFromWkt( pszDstWKT ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to import coordinate system `%s'.",
                    pszDstWKT );
            return nullptr;
        }
    }
    return GDALCreateGenImgProjTransformer4(
        OGRSpatialReference::ToHandle(&oSrcSRS),
        padfSrcGeoTransform,
        OGRSpatialReference::ToHandle(&oDstSRS),
        padfDstGeoTransform,
        nullptr );
}

/************************************************************************/
/*                  GDALCreateGenImgProjTransformer4()                  */
/************************************************************************/

/**
 * Create image to image transformer.
 *
 * Similar to GDALCreateGenImgProjTransformer3(), except that it takes
 * OGRSpatialReferenceH objects and options.
 * The options are the ones supported by GDALCreateReprojectionTransformerEx()
 *
 * @since GDAL 3.0
 */
void *
GDALCreateGenImgProjTransformer4( OGRSpatialReferenceH hSrcSRS,
                                  const double *padfSrcGeoTransform,
                                  OGRSpatialReferenceH hDstSRS,
                                  const double *padfDstGeoTransform,
                                  const char* const *papszOptions )
{
/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    GDALGenImgProjTransformInfo *psInfo =
        GDALCreateGenImgProjTransformerInternal();

/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for the source image.      */
/* -------------------------------------------------------------------- */
    if( padfSrcGeoTransform )
    {
        memcpy( psInfo->adfSrcGeoTransform, padfSrcGeoTransform,
                sizeof(psInfo->adfSrcGeoTransform) );
        if( !GDALInvGeoTransform( psInfo->adfSrcGeoTransform,
                                  psInfo->adfSrcInvGeoTransform ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
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
    OGRSpatialReference* poSrcSRS = OGRSpatialReference::FromHandle(hSrcSRS);
    OGRSpatialReference* poDstSRS = OGRSpatialReference::FromHandle(hDstSRS);
    if( !poSrcSRS->IsEmpty() && !poDstSRS->IsEmpty() &&
        !poSrcSRS->IsSame(poDstSRS) )
    {
        psInfo->pReprojectArg =
            GDALCreateReprojectionTransformerEx( hSrcSRS, hDstSRS,
                                                 papszOptions );
        if( psInfo->pReprojectArg == nullptr )
        {
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
        psInfo->pReproject = GDALReprojectionTransform;
    }

/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for destination image.     */
/*      If we have no destination matrix use a unit transform.          */
/* -------------------------------------------------------------------- */
    if( padfDstGeoTransform )
    {
        memcpy( psInfo->adfDstGeoTransform, padfDstGeoTransform,
                sizeof(psInfo->adfDstGeoTransform) );
        if( !GDALInvGeoTransform( psInfo->adfDstGeoTransform,
                                  psInfo->adfDstInvGeoTransform ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            GDALDestroyGenImgProjTransformer( psInfo );
            return nullptr;
        }
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
    VALIDATE_POINTER0( hTransformArg,
                       "GDALSetGenImgProjTransformerDstGeoTransform" );

    GDALGenImgProjTransformInfo *psInfo =
        static_cast<GDALGenImgProjTransformInfo *>( hTransformArg );

    memcpy( psInfo->adfDstGeoTransform, padfGeoTransform, sizeof(double) * 6 );
    if( !GDALInvGeoTransform( psInfo->adfDstGeoTransform,
                              psInfo->adfDstInvGeoTransform ) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
    }
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
    if( hTransformArg == nullptr )
        return;

    GDALGenImgProjTransformInfo *psInfo =
        static_cast<GDALGenImgProjTransformInfo *>(hTransformArg);

    if( psInfo->pSrcTransformArg != nullptr )
        GDALDestroyTransformer( psInfo->pSrcTransformArg );

    if( psInfo->pDstTransformArg != nullptr )
        GDALDestroyTransformer( psInfo->pDstTransformArg );

    if( psInfo->pReprojectArg != nullptr )
        GDALDestroyTransformer( psInfo->pReprojectArg );

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

#ifdef DEBUG_APPROX_TRANSFORMER
int countGDALGenImgProjTransform = 0;
#endif

int GDALGenImgProjTransform( void *pTransformArgIn, int bDstToSrc,
                             int nPointCount,
                             double *padfX, double *padfY, double *padfZ,
                             int *panSuccess )
{
    GDALGenImgProjTransformInfo *psInfo =
        static_cast<GDALGenImgProjTransformInfo *>(pTransformArgIn);

#ifdef DEBUG_APPROX_TRANSFORMER
    CPLAssert(nPointCount > 0);
    countGDALGenImgProjTransform += nPointCount;
#endif

    for( int i = 0; i < nPointCount; i++ )
    {
        panSuccess[i] = ( padfX[i] != HUGE_VAL && padfY[i] != HUGE_VAL );
    }

/* -------------------------------------------------------------------- */
/*      Convert from src (dst) pixel/line to src (dst)                  */
/*      georeferenced coordinates.                                      */
/* -------------------------------------------------------------------- */
    double *padfGeoTransform = nullptr;
    void *pTransformArg = nullptr;
    GDALTransformerFunc pTransformer = nullptr;
    if( bDstToSrc )
    {
        padfGeoTransform = psInfo->adfDstGeoTransform;
        pTransformArg = psInfo->pDstTransformArg;
        pTransformer = psInfo->pDstTransformer;
    }
    else
    {
        padfGeoTransform = psInfo->adfSrcGeoTransform;
        pTransformArg = psInfo->pSrcTransformArg;
        pTransformer = psInfo->pSrcTransformer;
    }

    if( pTransformArg != nullptr )
    {
        if( !pTransformer( pTransformArg, FALSE,
                           nPointCount, padfX, padfY, padfZ,
                           panSuccess ) )
            return FALSE;
    }
    else
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            if( !panSuccess[i] )
                continue;

            const double dfNewX = padfGeoTransform[0]
                + padfX[i] * padfGeoTransform[1]
                + padfY[i] * padfGeoTransform[2];
            const double dfNewY = padfGeoTransform[3]
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
        if( !psInfo->pReproject( psInfo->pReprojectArg, bDstToSrc,
                                 nPointCount, padfX, padfY, padfZ,
                                 panSuccess ) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Convert dst (src) georef coordinates back to pixel/line.        */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
    {
        padfGeoTransform = psInfo->adfSrcInvGeoTransform;
        pTransformArg = psInfo->pSrcTransformArg;
        pTransformer = psInfo->pSrcTransformer;
    }
    else
    {
        padfGeoTransform = psInfo->adfDstInvGeoTransform;
        pTransformArg = psInfo->pDstTransformArg;
        pTransformer = psInfo->pDstTransformer;
    }


    if( pTransformArg != nullptr )
    {
        if( !pTransformer( pTransformArg, TRUE,
                           nPointCount, padfX, padfY, padfZ,
                           panSuccess ) )
            return FALSE;
    }
    else
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            if( !panSuccess[i] )
                continue;

            const double dfNewX = padfGeoTransform[0]
                + padfX[i] * padfGeoTransform[1]
                + padfY[i] * padfGeoTransform[2];
            const double dfNewY = padfGeoTransform[3]
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
    GDALGenImgProjTransformInfo *psInfo =
        static_cast<GDALGenImgProjTransformInfo *>(pTransformArg);

    CPLXMLNode *psTree =
        CPLCreateXMLNode( nullptr, CXT_Element, "GenImgProjTransformer" );

    char szWork[200] = {};

/* -------------------------------------------------------------------- */
/*      Handle source transformation.                                   */
/* -------------------------------------------------------------------- */
    if( psInfo->pSrcTransformArg != nullptr )
    {
        CPLXMLNode *psTransformer =
            GDALSerializeTransformer( psInfo->pSrcTransformer,
                                      psInfo->pSrcTransformArg);
        if( psTransformer != nullptr )
        {
            CPLXMLNode *psTransformerContainer =
                CPLCreateXMLNode( psTree, CXT_Element,
                              CPLSPrintf("Src%s", psTransformer->pszValue) );

            CPLAddXMLChild( psTransformerContainer, psTransformer );
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle source geotransforms.                                    */
/* -------------------------------------------------------------------- */
    else
    {
        CPLsnprintf( szWork, sizeof(szWork),
                     "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
                     psInfo->adfSrcGeoTransform[0],
                     psInfo->adfSrcGeoTransform[1],
                     psInfo->adfSrcGeoTransform[2],
                     psInfo->adfSrcGeoTransform[3],
                     psInfo->adfSrcGeoTransform[4],
                     psInfo->adfSrcGeoTransform[5] );
        CPLCreateXMLElementAndValue( psTree, "SrcGeoTransform", szWork );

        CPLsnprintf( szWork, sizeof(szWork),
                     "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
                     psInfo->adfSrcInvGeoTransform[0],
                     psInfo->adfSrcInvGeoTransform[1],
                     psInfo->adfSrcInvGeoTransform[2],
                     psInfo->adfSrcInvGeoTransform[3],
                     psInfo->adfSrcInvGeoTransform[4],
                     psInfo->adfSrcInvGeoTransform[5] );
        CPLCreateXMLElementAndValue( psTree, "SrcInvGeoTransform", szWork );
    }


/* -------------------------------------------------------------------- */
/*      Handle dest transformation.                                     */
/* -------------------------------------------------------------------- */
    if( psInfo->pDstTransformArg != nullptr )
    {
        CPLXMLNode *psTransformer =
            GDALSerializeTransformer( psInfo->pDstTransformer,
                                      psInfo->pDstTransformArg);
        if( psTransformer != nullptr )
        {
            CPLXMLNode *psTransformerContainer =
                CPLCreateXMLNode( psTree, CXT_Element,
                              CPLSPrintf("Dst%s", psTransformer->pszValue) );

            CPLAddXMLChild( psTransformerContainer, psTransformer );
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle destination geotransforms.                               */
/* -------------------------------------------------------------------- */
    else
    {
        CPLsnprintf( szWork, sizeof(szWork),
                     "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
                     psInfo->adfDstGeoTransform[0],
                     psInfo->adfDstGeoTransform[1],
                     psInfo->adfDstGeoTransform[2],
                     psInfo->adfDstGeoTransform[3],
                     psInfo->adfDstGeoTransform[4],
                     psInfo->adfDstGeoTransform[5] );
        CPLCreateXMLElementAndValue( psTree, "DstGeoTransform", szWork );

        CPLsnprintf( szWork, sizeof(szWork),
                     "%.18g,%.18g,%.18g,%.18g,%.18g,%.18g",
                     psInfo->adfDstInvGeoTransform[0],
                     psInfo->adfDstInvGeoTransform[1],
                     psInfo->adfDstInvGeoTransform[2],
                     psInfo->adfDstInvGeoTransform[3],
                     psInfo->adfDstInvGeoTransform[4],
                     psInfo->adfDstInvGeoTransform[5] );
        CPLCreateXMLElementAndValue( psTree, "DstInvGeoTransform", szWork );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a reprojection transformer?                          */
/* -------------------------------------------------------------------- */
    if( psInfo->pReprojectArg != nullptr )
    {

        CPLXMLNode *psTransformerContainer
            = CPLCreateXMLNode( psTree, CXT_Element, "ReprojectTransformer" );

        CPLXMLNode *psTransformer
            = GDALSerializeTransformer( psInfo->pReproject,
                                        psInfo->pReprojectArg );
        if( psTransformer != nullptr )
            CPLAddXMLChild( psTransformerContainer, psTransformer );
    }

    return psTree;
}

/************************************************************************/
/*                    GDALDeserializeGeoTransform()                     */
/************************************************************************/

static void GDALDeserializeGeoTransform(const char* pszGT,
                                        double adfGeoTransform[6])
{
    CPLsscanf( pszGT, "%lf,%lf,%lf,%lf,%lf,%lf",
               adfGeoTransform + 0,
               adfGeoTransform + 1,
               adfGeoTransform + 2,
               adfGeoTransform + 3,
               adfGeoTransform + 4,
               adfGeoTransform + 5 );
}

/************************************************************************/
/*                GDALDeserializeGenImgProjTransformer()                */
/************************************************************************/

void *GDALDeserializeGenImgProjTransformer( CPLXMLNode *psTree )

{
/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    GDALGenImgProjTransformInfo *psInfo =
        GDALCreateGenImgProjTransformerInternal();

/* -------------------------------------------------------------------- */
/*      SrcGeotransform                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "SrcGeoTransform" ) != nullptr )
    {
        GDALDeserializeGeoTransform(
            CPLGetXMLValue( psTree, "SrcGeoTransform", "" ),
            psInfo->adfSrcGeoTransform );

        if( CPLGetXMLNode( psTree, "SrcInvGeoTransform" ) != nullptr )
        {
            GDALDeserializeGeoTransform(
                CPLGetXMLValue( psTree, "SrcInvGeoTransform", "" ),
                psInfo->adfSrcInvGeoTransform );
        }
        else
        {
            if( !GDALInvGeoTransform( psInfo->adfSrcGeoTransform,
                                      psInfo->adfSrcInvGeoTransform ) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot invert geotransform");
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Src Transform                                                   */
/* -------------------------------------------------------------------- */
    else
    {
        for( CPLXMLNode* psIter = psTree->psChild; psIter != nullptr;
                                                   psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element &&
                STARTS_WITH_CI(psIter->pszValue, "Src") )
            {
                GDALDeserializeTransformer( psIter->psChild,
                                            &psInfo->pSrcTransformer,
                                            &psInfo->pSrcTransformArg );
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      DstGeotransform                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "DstGeoTransform" ) != nullptr )
    {
        GDALDeserializeGeoTransform(
            CPLGetXMLValue( psTree, "DstGeoTransform", "" ),
            psInfo->adfDstGeoTransform);

        if( CPLGetXMLNode( psTree, "DstInvGeoTransform" ) != nullptr )
        {
            GDALDeserializeGeoTransform(
                CPLGetXMLValue( psTree, "DstInvGeoTransform", "" ),
                psInfo->adfDstInvGeoTransform);
        }
        else
        {
            if( !GDALInvGeoTransform( psInfo->adfDstGeoTransform,
                                      psInfo->adfDstInvGeoTransform ) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot invert geotransform");
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Dst Transform                                                   */
/* -------------------------------------------------------------------- */
    else
    {
        for( CPLXMLNode* psIter = psTree->psChild; psIter != nullptr;
                                                   psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element &&
                STARTS_WITH_CI(psIter->pszValue, "Dst") )
            {
                GDALDeserializeTransformer( psIter->psChild,
                                            &psInfo->pDstTransformer,
                                            &psInfo->pDstTransformArg );
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Reproject transformer                                           */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psSubtree = CPLGetXMLNode( psTree, "ReprojectTransformer" );
    if( psSubtree != nullptr && psSubtree->psChild != nullptr )
    {
        GDALDeserializeTransformer( psSubtree->psChild,
                                    &psInfo->pReproject,
                                    &psInfo->pReprojectArg );
    }

    return psInfo;
}

/************************************************************************/
/* ==================================================================== */
/*                       GDALReprojectionTransformer                    */
/* ==================================================================== */
/************************************************************************/

struct GDALReprojectionTransformInfo
{
    GDALTransformerInfo sTI;
    char** papszOptions = nullptr;
    double dfTime = 0.0;

    OGRCoordinateTransformation *poForwardTransform = nullptr;
    OGRCoordinateTransformation *poReverseTransform = nullptr;

    GDALReprojectionTransformInfo(): sTI()
    {
        memset(&sTI, 0, sizeof(sTI));
    }

    GDALReprojectionTransformInfo(const GDALReprojectionTransformInfo&) = delete;
    GDALReprojectionTransformInfo& operator= (const GDALReprojectionTransformInfo&) = delete;
};

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
/* -------------------------------------------------------------------- */
/*      Ingest the SRS definitions.                                     */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSrcSRS;
    oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( oSrcSRS.importFromWkt( pszSrcWKT ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to import coordinate system `%s'.",
                  pszSrcWKT );
        return nullptr;
    }

    OGRSpatialReference oDstSRS;
    oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( oDstSRS.importFromWkt( pszDstWKT ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to import coordinate system `%s'.",
                  pszSrcWKT );
        return nullptr;
    }

    return GDALCreateReprojectionTransformerEx(
        OGRSpatialReference::ToHandle(&oSrcSRS),
        OGRSpatialReference::ToHandle(&oDstSRS),
        nullptr);
}

/************************************************************************/
/*                 GDALCreateReprojectionTransformerEx()                */
/************************************************************************/

/**
 * Create reprojection transformer.
 *
 * Creates a callback data structure suitable for use with
 * GDALReprojectionTransformation() to represent a transformation from
 * one geographic or projected coordinate system to another.
 *
 * Internally the OGRCoordinateTransformation object is used to implement
 * the reprojection.
 *
 * @param hSrcSRS the coordinate system for the source coordinate system.
 * @param hDstSRS the coordinate system for the destination coordinate
 * system.
 * @param papszOptions NULL-terminated list of options, or NULL. Currently
 * supported options are:
 * <ul>
 * <li>AREA_OF_INTEREST=west_long,south_lat,east_long,north_lat: Values in
 * degrees. longitudes in [-180,180], latitudes in [-90,90].</li>
 * <li>COORDINATE_OPERATION=string: PROJ or WKT string representing a
 * coordinate operation, overriding the default computed transformation.</li>
 * <li>COORDINATE_EPOCH=decimal_year: Coordinate epoch, expressed as a
 * decimal year. Useful for time-dependant coordinate operations.</li>
 * </ul>
 *
 * @return Handle for use with GDALReprojectionTransform(), or NULL if the
 * system fails to initialize the reprojection.
 *
 * @since GDAL 3.0
 **/

void *GDALCreateReprojectionTransformerEx(
                                   OGRSpatialReferenceH hSrcSRS,
                                   OGRSpatialReferenceH hDstSRS,
                                   const char* const *papszOptions)
{
    OGRSpatialReference* poSrcSRS = OGRSpatialReference::FromHandle(hSrcSRS);
    OGRSpatialReference* poDstSRS = OGRSpatialReference::FromHandle(hDstSRS);

/* -------------------------------------------------------------------- */
/*      Build the forward coordinate transformation.                    */
/* -------------------------------------------------------------------- */
    double dfWestLongitudeDeg = 0.0;
    double dfSouthLatitudeDeg = 0.0;
    double dfEastLongitudeDeg = 0.0;
    double dfNorthLatitudeDeg = 0.0;
    const char* pszBBOX = CSLFetchNameValue(papszOptions, "AREA_OF_INTEREST");
    if( pszBBOX )
    {
        char** papszTokens = CSLTokenizeString2(pszBBOX, ",", 0);
        if( CSLCount(papszTokens) == 4 )
        {
            dfWestLongitudeDeg = CPLAtof(papszTokens[0]);
            dfSouthLatitudeDeg = CPLAtof(papszTokens[1]);
            dfEastLongitudeDeg = CPLAtof(papszTokens[2]);
            dfNorthLatitudeDeg = CPLAtof(papszTokens[3]);
        }
        CSLDestroy(papszTokens);
    }
    const char* pszCO = CSLFetchNameValue(papszOptions, "COORDINATE_OPERATION");

    OGRCoordinateTransformationOptions optionsFwd;
    OGRCoordinateTransformationOptions optionsInv;
    if( !(dfWestLongitudeDeg == 0.0 && dfSouthLatitudeDeg == 0.0 &&
          dfEastLongitudeDeg == 0.0 && dfNorthLatitudeDeg == 0.0) )
    {
        optionsFwd.SetAreaOfInterest(dfWestLongitudeDeg,
                                  dfSouthLatitudeDeg,
                                  dfEastLongitudeDeg,
                                  dfNorthLatitudeDeg);
        optionsInv.SetAreaOfInterest(dfWestLongitudeDeg,
                                  dfSouthLatitudeDeg,
                                  dfEastLongitudeDeg,
                                  dfNorthLatitudeDeg);
    }
    if( pszCO )
    {
        optionsFwd.SetCoordinateOperation(pszCO, false);
        optionsInv.SetCoordinateOperation(pszCO, true);
    }

    const char* pszCENTER_LONG = CSLFetchNameValue(papszOptions, "CENTER_LONG");
    if( pszCENTER_LONG )
    {
        optionsFwd.SetSourceCenterLong(CPLAtof(pszCENTER_LONG));
        optionsInv.SetTargetCenterLong(CPLAtof(pszCENTER_LONG));
    }

    OGRCoordinateTransformation *poForwardTransform =
        OGRCreateCoordinateTransformation(poSrcSRS, poDstSRS, optionsFwd);

    if( poForwardTransform == nullptr )
        // OGRCreateCoordinateTransformation() will report errors on its own.
        return nullptr;

    poForwardTransform->SetEmitErrors(false);

/* -------------------------------------------------------------------- */
/*      Create a structure to hold the transform info, and also         */
/*      build reverse transform.  We assume that if the forward         */
/*      transform can be created, then so can the reverse one.          */
/* -------------------------------------------------------------------- */
    GDALReprojectionTransformInfo *psInfo = new GDALReprojectionTransformInfo();

    psInfo->papszOptions = CSLDuplicate(papszOptions);
    psInfo->poForwardTransform = poForwardTransform;
    psInfo->dfTime = CPLAtof(CSLFetchNameValueDef(papszOptions,
                                                  "COORDINATE_EPOCH", "0"));
    CPLPushErrorHandler(CPLQuietErrorHandler);
    psInfo->poReverseTransform =
        OGRCreateCoordinateTransformation(poDstSRS, poSrcSRS, optionsInv);
    CPLPopErrorHandler();

    if( psInfo->poReverseTransform )
        psInfo->poReverseTransform->SetEmitErrors(false);

    memcpy( psInfo->sTI.abySignature,
            GDAL_GTI2_SIGNATURE,
            strlen(GDAL_GTI2_SIGNATURE) );
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
    if( pTransformArg == nullptr )
        return;

    GDALReprojectionTransformInfo *psInfo =
        static_cast<GDALReprojectionTransformInfo *>(pTransformArg);

    if( psInfo->poForwardTransform )
        delete psInfo->poForwardTransform;

    if( psInfo->poReverseTransform )
        delete psInfo->poReverseTransform;

    CSLDestroy( psInfo->papszOptions );

    delete psInfo;
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
        static_cast<GDALReprojectionTransformInfo *>(pTransformArg);
    int bSuccess;

    std::vector<double> adfTime;
    double* padfT = nullptr;
    if( psInfo->dfTime != 0.0 && nPointCount > 0 )
    {
        adfTime.resize( nPointCount, psInfo->dfTime );
        padfT = &adfTime[0];
    }

    if( bDstToSrc )
    {
        if( psInfo->poReverseTransform == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inverse coordinate transformation cannot be instantiated");
            if( panSuccess )
            {
                for( int i = 0; i < nPointCount; i++ )
                    panSuccess[i] = FALSE;
            }
            bSuccess = false;
        }
        else
        {
            bSuccess = psInfo->poReverseTransform->Transform(
                nPointCount, padfX, padfY, padfZ, padfT, panSuccess );
        }
    }
    else
        bSuccess = psInfo->poForwardTransform->Transform(
            nPointCount, padfX, padfY, padfZ, padfT, panSuccess );

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
        static_cast<GDALReprojectionTransformInfo *>(pTransformArg);

    psTree = CPLCreateXMLNode( nullptr, CXT_Element, "ReprojectionTransformer" );

/* -------------------------------------------------------------------- */
/*      Handle SourceCS.                                                */
/* -------------------------------------------------------------------- */
    char *pszWKT = nullptr;

    auto poSRS = psInfo->poForwardTransform->GetSourceCS();
    if( poSRS )
    {
        poSRS->exportToWkt( &pszWKT );
        CPLCreateXMLElementAndValue( psTree, "SourceSRS", pszWKT );
        CPLFree( pszWKT );
    }

/* -------------------------------------------------------------------- */
/*      Handle DestinationCS.                                           */
/* -------------------------------------------------------------------- */
    poSRS = psInfo->poForwardTransform->GetTargetCS();
    if( poSRS )
    {
        poSRS->exportToWkt( &pszWKT );
        CPLCreateXMLElementAndValue( psTree, "TargetSRS", pszWKT );
        CPLFree( pszWKT );
    }

/* -------------------------------------------------------------------- */
/*      Serialize options.                                              */
/* -------------------------------------------------------------------- */
    if( psInfo->papszOptions )
    {
        CPLXMLNode* psOptions = CPLCreateXMLNode( psTree, CXT_Element,
                                                  "Options" );
        for( auto iter = psInfo->papszOptions; *iter != nullptr; ++iter )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(*iter, &pszKey);
            if( pszKey && pszValue )
            {
                auto elt = CPLCreateXMLElementAndValue( psOptions, "Option", pszValue );
                CPLAddXMLAttributeAndValue( elt, "key", pszKey );
            }
            CPLFree(pszKey);
        }
    }

    return psTree;
}

/************************************************************************/
/*               GDALDeserializeReprojectionTransformer()               */
/************************************************************************/

static void *
GDALDeserializeReprojectionTransformer( CPLXMLNode *psTree )

{
    const char *pszSourceSRS = CPLGetXMLValue( psTree, "SourceSRS", nullptr );
    const char *pszTargetSRS = CPLGetXMLValue( psTree, "TargetSRS", nullptr );
    char *pszSourceWKT = nullptr, *pszTargetWKT = nullptr;
    void *pResult = nullptr;

    OGRSpatialReference oSrcSRS;
    OGRSpatialReference oDstSRS;

    oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( pszSourceSRS != nullptr )
    {
        oSrcSRS.SetFromUserInput( pszSourceSRS );
    }

    if( pszTargetSRS != nullptr )
    {
        oDstSRS.SetFromUserInput( pszTargetSRS );
    }

    CPLStringList aosList;
    const CPLXMLNode* psOptions = CPLGetXMLNode( psTree, "Options" );
    if( psOptions )
    {
        for( auto iter = psOptions->psChild; iter; iter = iter->psNext )
        {
            if( iter->eType == CXT_Element &&
                strcmp(iter->pszValue, "Option") == 0 )
            {
                const char* pszKey = CPLGetXMLValue(iter, "key", nullptr);
                const char* pszValue = CPLGetXMLValue(iter, nullptr, nullptr);
                if( pszKey && pszValue )
                {
                    aosList.SetNameValue(pszKey, pszValue);
                }
            }
        }
    }

    pResult = GDALCreateReprojectionTransformerEx(
        !oSrcSRS.IsEmpty() ? OGRSpatialReference::ToHandle(&oSrcSRS) : nullptr,
        !oDstSRS.IsEmpty() ? OGRSpatialReference::ToHandle(&oDstSRS) : nullptr,
        aosList.List());

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
    void *pBaseCBData;
    double dfMaxErrorForward;
    double dfMaxErrorReverse;

    int bOwnSubtransformer;
} ApproxTransformInfo;

/************************************************************************/
/*                  GDALCreateSimilarApproxTransformer()                */
/************************************************************************/

static void *
GDALCreateSimilarApproxTransformer( void *hTransformArg,
                                    double dfSrcRatioX, double dfSrcRatioY )
{
    VALIDATE_POINTER1( hTransformArg,
                       "GDALCreateSimilarApproxTransformer", nullptr );

    ApproxTransformInfo *psInfo =
      static_cast<ApproxTransformInfo *>(hTransformArg);

    ApproxTransformInfo *psClonedInfo = static_cast<ApproxTransformInfo *>(
        CPLMalloc(sizeof(ApproxTransformInfo)));

    memcpy(psClonedInfo, psInfo, sizeof(ApproxTransformInfo));
    if( psClonedInfo->pBaseCBData )
    {
        psClonedInfo->pBaseCBData =
            GDALCreateSimilarTransformer( psInfo->pBaseCBData,
                                          dfSrcRatioX,
                                          dfSrcRatioY );
        if( psClonedInfo->pBaseCBData == nullptr )
        {
            CPLFree(psClonedInfo);
            return nullptr;
        }
    }
    psClonedInfo->bOwnSubtransformer = TRUE;

    return psClonedInfo;
}

/************************************************************************/
/*                   GDALSerializeApproxTransformer()                   */
/************************************************************************/

static CPLXMLNode *
GDALSerializeApproxTransformer( void *pTransformArg )

{
    CPLXMLNode *psTree;
    ApproxTransformInfo *psInfo =
        static_cast<ApproxTransformInfo *>(pTransformArg);

    psTree = CPLCreateXMLNode( nullptr, CXT_Element, "ApproxTransformer" );

/* -------------------------------------------------------------------- */
/*      Attach max error.                                               */
/* -------------------------------------------------------------------- */
    if( psInfo->dfMaxErrorForward == psInfo->dfMaxErrorReverse )
    {
        CPLCreateXMLElementAndValue( psTree, "MaxError",
                        CPLString().Printf("%g", psInfo->dfMaxErrorForward) );
    }
    else
    {
        CPLCreateXMLElementAndValue( psTree, "MaxErrorForward",
                        CPLString().Printf("%g", psInfo->dfMaxErrorForward) );
        CPLCreateXMLElementAndValue( psTree, "MaxErrorReverse",
                        CPLString().Printf("%g", psInfo->dfMaxErrorReverse) );
    }

/* -------------------------------------------------------------------- */
/*      Capture underlying transformer.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTransformerContainer =
        CPLCreateXMLNode( psTree, CXT_Element, "BaseTransformer" );

    CPLXMLNode *psTransformer =
        GDALSerializeTransformer( psInfo->pfnBaseTransformer,
                                  psInfo->pBaseCBData );
    if( psTransformer != nullptr )
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
    return GDALCreateApproxTransformer2(pfnBaseTransformer,
                                        pBaseTransformArg,
                                        dfMaxError,
                                        dfMaxError);
}


static
void *GDALCreateApproxTransformer2( GDALTransformerFunc pfnBaseTransformer,
                                    void *pBaseTransformArg,
                                    double dfMaxErrorForward,
                                    double dfMaxErrorReverse)

{
    ApproxTransformInfo *psATInfo = static_cast<ApproxTransformInfo *>(
        CPLMalloc(sizeof(ApproxTransformInfo)));
    psATInfo->pfnBaseTransformer = pfnBaseTransformer;
    psATInfo->pBaseCBData = pBaseTransformArg;
    psATInfo->dfMaxErrorForward = dfMaxErrorForward;
    psATInfo->dfMaxErrorReverse = dfMaxErrorReverse;
    psATInfo->bOwnSubtransformer = FALSE;

    memcpy(psATInfo->sTI.abySignature,
           GDAL_GTI2_SIGNATURE,
           strlen(GDAL_GTI2_SIGNATURE));
    psATInfo->sTI.pszClassName = "GDALApproxTransformer";
    psATInfo->sTI.pfnTransform = GDALApproxTransform;
    psATInfo->sTI.pfnCleanup = GDALDestroyApproxTransformer;
    psATInfo->sTI.pfnSerialize = GDALSerializeApproxTransformer;
    psATInfo->sTI.pfnCreateSimilar = GDALCreateSimilarApproxTransformer;

    return psATInfo;
}

/************************************************************************/
/*              GDALApproxTransformerOwnsSubtransformer()               */
/************************************************************************/

/** Set bOwnSubtransformer flag */
void GDALApproxTransformerOwnsSubtransformer( void *pCBData, int bOwnFlag )

{
    ApproxTransformInfo *psATInfo = static_cast<ApproxTransformInfo *>(pCBData);

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
    if( pCBData == nullptr)
        return;

    ApproxTransformInfo *psATInfo = static_cast<ApproxTransformInfo *>(pCBData);

    if( psATInfo->bOwnSubtransformer )
        GDALDestroyTransformer( psATInfo->pBaseCBData );

    CPLFree( pCBData );
}

/************************************************************************/
/*                  GDALRefreshApproxTransformer()                      */
/************************************************************************/

void GDALRefreshApproxTransformer( void* hTransformArg )
{
    ApproxTransformInfo *psInfo =
        static_cast<ApproxTransformInfo *>( hTransformArg );

    if( psInfo->pfnBaseTransformer == GDALGenImgProjTransform )
    {
        GDALRefreshGenImgProjTransformer( psInfo->pBaseCBData );
    }
}

/************************************************************************/
/*                      GDALApproxTransformInternal()                   */
/************************************************************************/

static int GDALApproxTransformInternal( void *pCBData, int bDstToSrc,
                                        int nPoints,
                                        double *x, double *y, double *z,
                                        int *panSuccess,
                                        // SME = Start, Middle, End.
                                        const double xSMETransformed[3],
                                        const double ySMETransformed[3],
                                        const double zSMETransformed[3] )
{
    ApproxTransformInfo *psATInfo = static_cast<ApproxTransformInfo *>(pCBData);
    const int nMiddle = (nPoints - 1) / 2;

#ifdef notdef_sanify_check
    {
        double x2[3] = { x[0], x[nMiddle], x[nPoints-1] };
        double y2[3] = { y[0], y[nMiddle], y[nPoints-1] };
        double z2[3] = { z[0], z[nMiddle], z[nPoints-1] };
        int anSuccess2[3] = {};

        const int bSuccess =
            psATInfo->pfnBaseTransformer( psATInfo->pBaseCBData, bDstToSrc, 3,
                                        x2, y2, z2, anSuccess2 );
        CPLAssert(bSuccess);
        CPLAssert(anSuccess2[0]);
        CPLAssert(anSuccess2[1]);
        CPLAssert(anSuccess2[2]);
        CPLAssert(x2[0] == xSMETransformed[0]);
        CPLAssert(y2[0] == ySMETransformed[0]);
        CPLAssert(z2[0] == zSMETransformed[0]);
        CPLAssert(x2[1] == xSMETransformed[1]);
        CPLAssert(y2[1] == ySMETransformed[1]);
        CPLAssert(z2[1] == zSMETransformed[1]);
        CPLAssert(x2[2] == xSMETransformed[2]);
        CPLAssert(y2[2] == ySMETransformed[2]);
        CPLAssert(z2[2] == zSMETransformed[2]);
    }
#endif

#ifdef DEBUG_APPROX_TRANSFORMER
    fprintf(stderr, "start (%.3f,%.3f) -> (%.3f,%.3f)\n",/*ok*/
            x[0], y[0], xSMETransformed[0], ySMETransformed[0]);
    fprintf(stderr, "middle (%.3f,%.3f) -> (%.3f,%.3f)\n",/*ok*/
            x[nMiddle], y[nMiddle], xSMETransformed[1], ySMETransformed[1]);
    fprintf(stderr, "end (%.3f,%.3f) -> (%.3f,%.3f)\n",/*ok*/
            x[nPoints-1], y[nPoints-1], xSMETransformed[2], ySMETransformed[2]);
#endif

/* -------------------------------------------------------------------- */
/*      Is the error at the middle acceptable relative to an            */
/*      interpolation of the middle position?                           */
/* -------------------------------------------------------------------- */
    const double dfDeltaX =
        (xSMETransformed[2] - xSMETransformed[0]) / (x[nPoints-1] - x[0]);
    const double dfDeltaY =
        (ySMETransformed[2] - ySMETransformed[0]) / (x[nPoints-1] - x[0]);
    const double dfDeltaZ =
        (zSMETransformed[2] - zSMETransformed[0]) / (x[nPoints-1] - x[0]);

    const double dfError =
        fabs((xSMETransformed[0] + dfDeltaX * (x[nMiddle] - x[0])) -
             xSMETransformed[1]) +
        fabs((ySMETransformed[0] + dfDeltaY * (x[nMiddle] - x[0])) -
             ySMETransformed[1]);

    const double dfMaxError = (bDstToSrc) ? psATInfo->dfMaxErrorReverse :
                                            psATInfo->dfMaxErrorForward;
    if( dfError > dfMaxError )
    {
#if DEBUG_VERBOSE
        CPLDebug( "GDAL", "ApproxTransformer - "
                  "error %g over threshold %g, subdivide %d points.",
                  dfError, dfMaxError, nPoints );
#endif

        double xMiddle[3] = {
            x[(nMiddle - 1) / 2],
            x[nMiddle - 1],
            x[nMiddle + (nPoints - nMiddle - 1) / 2]
        };
        double yMiddle[3] = {
            y[(nMiddle - 1) / 2],
            y[nMiddle - 1],
            y[nMiddle + (nPoints - nMiddle - 1) / 2]
        };
        double zMiddle[3] = {
            z[(nMiddle - 1) / 2],
            z[nMiddle - 1 ],
            z[nMiddle + (nPoints - nMiddle - 1) / 2]
        };

        const bool bUseBaseTransformForHalf1 =
            nMiddle <= 5 ||
            y[0] != y[nMiddle-1] ||
            y[0] != y[(nMiddle - 1) / 2] ||
            x[0] == x[nMiddle-1] ||
            x[0] == x[(nMiddle - 1) / 2];
        const bool bUseBaseTransformForHalf2 =
            nPoints - nMiddle <= 5 ||
            y[nMiddle] != y[nPoints-1] ||
            y[nMiddle] != y[nMiddle + (nPoints - nMiddle - 1) / 2] ||
            x[nMiddle] == x[nPoints-1] ||
            x[nMiddle] == x[nMiddle + (nPoints - nMiddle - 1) / 2];

        int anSuccess2[3] = {};
        int bSuccess = FALSE;
        if( !bUseBaseTransformForHalf1 && !bUseBaseTransformForHalf2 )
            bSuccess =
                psATInfo->pfnBaseTransformer(psATInfo->pBaseCBData,
                                             bDstToSrc, 3,
                                             xMiddle, yMiddle, zMiddle,
                                             anSuccess2 );
        else if( !bUseBaseTransformForHalf1 )
        {
            bSuccess =
                psATInfo->pfnBaseTransformer(psATInfo->pBaseCBData,
                                             bDstToSrc, 2,
                                             xMiddle, yMiddle, zMiddle,
                                             anSuccess2 );
            anSuccess2[2] = TRUE;
        }
        else if( !bUseBaseTransformForHalf2 )
        {
            bSuccess =
                psATInfo->pfnBaseTransformer(psATInfo->pBaseCBData,
                                             bDstToSrc, 1,
                                             xMiddle + 2,
                                             yMiddle + 2,
                                             zMiddle + 2,
                                             anSuccess2 + 2 );
            anSuccess2[0] = TRUE;
            anSuccess2[1] = TRUE;
        }

        if( !bSuccess || !anSuccess2[0] || !anSuccess2[1] || !anSuccess2[2] )
        {
            bSuccess = psATInfo->pfnBaseTransformer(psATInfo->pBaseCBData,
                                                    bDstToSrc,
                                                    nMiddle - 1,
                                                    x + 1, y + 1, z + 1,
                                                    panSuccess + 1);
            bSuccess &= psATInfo->pfnBaseTransformer(psATInfo->pBaseCBData,
                                                     bDstToSrc,
                                                     nPoints - nMiddle - 2,
                                                     x + nMiddle + 1,
                                                     y + nMiddle + 1,
                                                     z + nMiddle + 1,
                                                     panSuccess + nMiddle + 1);

            x[0] = xSMETransformed[0];
            y[0] = ySMETransformed[0];
            z[0] = zSMETransformed[0];
            panSuccess[0] = TRUE;
            x[nMiddle] = xSMETransformed[1];
            y[nMiddle] = ySMETransformed[1];
            z[nMiddle] = zSMETransformed[1];
            panSuccess[nMiddle] = TRUE;
            x[nPoints-1] = xSMETransformed[2];
            y[nPoints-1] = ySMETransformed[2];
            z[nPoints-1] = zSMETransformed[2];
            panSuccess[nPoints-1] = TRUE;
            return bSuccess;
        }

        double x2[3] = {};
        double y2[3] = {};
        double z2[3] = {};
        if( !bUseBaseTransformForHalf1 )
        {
            x2[0] = xSMETransformed[0];
            y2[0] = ySMETransformed[0];
            z2[0] = zSMETransformed[0];
            x2[1] = xMiddle[0];
            y2[1] = yMiddle[0];
            z2[1] = zMiddle[0];
            x2[2] = xMiddle[1];
            y2[2] = yMiddle[1];
            z2[2] = zMiddle[1];

            bSuccess =
                GDALApproxTransformInternal( psATInfo, bDstToSrc, nMiddle,
                                            x, y, z, panSuccess,
                                            x2, y2, z2);
        }
        else
        {
            bSuccess = psATInfo->pfnBaseTransformer(psATInfo->pBaseCBData,
                                                    bDstToSrc,
                                                    nMiddle - 1,
                                                    x + 1, y + 1, z + 1,
                                                    panSuccess + 1 );
            x[0] = xSMETransformed[0];
            y[0] = ySMETransformed[0];
            z[0] = zSMETransformed[0];
            panSuccess[0] = TRUE;
        }

        if( !bSuccess )
            return FALSE;

        if( !bUseBaseTransformForHalf2 )
        {
            x2[0] = xSMETransformed[1];
            y2[0] = ySMETransformed[1];
            z2[0] = zSMETransformed[1];
            x2[1] = xMiddle[2];
            y2[1] = yMiddle[2];
            z2[1] = zMiddle[2];
            x2[2] = xSMETransformed[2];
            y2[2] = ySMETransformed[2];
            z2[2] = zSMETransformed[2];

            bSuccess =
                GDALApproxTransformInternal(psATInfo, bDstToSrc,
                                            nPoints - nMiddle,
                                            x+nMiddle, y+nMiddle, z+nMiddle,
                                            panSuccess+nMiddle,
                                            x2, y2, z2);
        }
        else
        {
            bSuccess = psATInfo->pfnBaseTransformer(psATInfo->pBaseCBData,
                                                    bDstToSrc,
                                                    nPoints - nMiddle - 2,
                                                    x + nMiddle + 1,
                                                    y + nMiddle + 1,
                                                    z + nMiddle + 1,
                                                    panSuccess+nMiddle+1 );

            x[nMiddle] = xSMETransformed[1];
            y[nMiddle] = ySMETransformed[1];
            z[nMiddle] = zSMETransformed[1];
            panSuccess[nMiddle] = TRUE;
            x[nPoints-1] = xSMETransformed[2];
            y[nPoints-1] = ySMETransformed[2];
            z[nPoints-1] = zSMETransformed[2];
            panSuccess[nPoints-1] = TRUE;
        }

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
/*      NOTE: the above comment is not true: gdalwarp uses approximator */
/*      also to compute the source pixel of each target pixel.          */
/* -------------------------------------------------------------------- */
    for( int i = nPoints-1; i >= 0; i-- )
    {
#ifdef check_error
        double xtemp = x[i];
        double ytemp = y[i];
        double ztemp = z[i];
        double x_ori = xtemp;
        double y_ori = ytemp;
        int btemp = FALSE;
        psATInfo->pfnBaseTransformer( psATInfo->pBaseCBData, bDstToSrc,
                                      1, &xtemp, &ytemp, &ztemp, &btemp);
#endif
        const double dfDist = (x[i] - x[0]);
        x[i] = xSMETransformed[0] + dfDeltaX * dfDist;
        y[i] = ySMETransformed[0] + dfDeltaY * dfDist;
        z[i] = zSMETransformed[0] + dfDeltaZ * dfDist;
#ifdef check_error
        const double dfError2 = fabs(x[i] - xtemp) + fabs(y[i] - ytemp);
        if( dfError2 > 4 /*10 * dfMaxError*/ )
        {
            /*ok*/printf("Error = %f on (%f, %f)\n", dfError2,  x_ori, y_ori);
        }
#endif
        panSuccess[i] = TRUE;
    }

    return TRUE;
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
    ApproxTransformInfo *psATInfo = static_cast<ApproxTransformInfo *>(pCBData);
    double x2[3] = {};
    double y2[3] = {};
    double z2[3] = {};
    int anSuccess2[3] = {};
    int bSuccess;

    const int nMiddle = (nPoints - 1) / 2;

/* -------------------------------------------------------------------- */
/*      Bail if our preconditions are not met, or if error is not       */
/*      acceptable.                                                     */
/* -------------------------------------------------------------------- */
    int bRet = FALSE;
    if( y[0] != y[nPoints-1] || y[0] != y[nMiddle]
        || x[0] == x[nPoints-1] || x[0] == x[nMiddle]
        || (psATInfo->dfMaxErrorForward == 0.0 &&
            psATInfo->dfMaxErrorReverse == 0.0) || nPoints <= 5 )
    {
        bRet = psATInfo->pfnBaseTransformer( psATInfo->pBaseCBData, bDstToSrc,
                                             nPoints, x, y, z, panSuccess );
        goto end;
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
    {
        bRet = psATInfo->pfnBaseTransformer( psATInfo->pBaseCBData, bDstToSrc,
                                             nPoints, x, y, z, panSuccess );
        goto end;
    }

    bRet = GDALApproxTransformInternal( pCBData, bDstToSrc, nPoints,
                                        x, y, z, panSuccess,
                                        x2,
                                        y2,
                                        z2 );

end:
#ifdef DEBUG_APPROX_TRANSFORMER
    for( int i = 0; i < nPoints; i++ )
        fprintf(stderr, "[%d] (%.10f,%.10f) %d\n",/*ok*/
                i, x[i], y[i], panSuccess[i]);
#endif

    return bRet;
}

/************************************************************************/
/*                  GDALDeserializeApproxTransformer()                  */
/************************************************************************/

static void *
GDALDeserializeApproxTransformer( CPLXMLNode *psTree )

{
    double dfMaxErrorForward = 0.25;
    double dfMaxErrorReverse = 0.25;
    const char* pszMaxError = CPLGetXMLValue( psTree, "MaxError", nullptr);
    if( pszMaxError != nullptr )
    {
        dfMaxErrorForward = CPLAtof(pszMaxError);
        dfMaxErrorReverse = dfMaxErrorForward;
    }
    const char* pszMaxErrorForward =
                    CPLGetXMLValue( psTree, "MaxErrorForward", nullptr);
    if( pszMaxErrorForward != nullptr )
    {
        dfMaxErrorForward = CPLAtof(pszMaxErrorForward);
    }
    const char* pszMaxErrorReverse =
                    CPLGetXMLValue( psTree, "MaxErrorReverse", nullptr);
    if( pszMaxErrorReverse != nullptr )
    {
        dfMaxErrorReverse = CPLAtof(pszMaxErrorReverse);
    }

    GDALTransformerFunc pfnBaseTransform = nullptr;
    void *pBaseCBData = nullptr;

    CPLXMLNode *psContainer = CPLGetXMLNode( psTree, "BaseTransformer" );

    if( psContainer != nullptr && psContainer->psChild != nullptr )
    {
        GDALDeserializeTransformer( psContainer->psChild,
                                    &pfnBaseTransform,
                                    &pBaseCBData );
    }

    if( pfnBaseTransform == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot get base transform for approx transformer." );
        return nullptr;
    }

    void *pApproxCBData = GDALCreateApproxTransformer2( pfnBaseTransform,
                                                        pBaseCBData,
                                                        dfMaxErrorForward,
                                                        dfMaxErrorReverse );
    GDALApproxTransformerOwnsSubtransformer( pApproxCBData, TRUE );

    return pApproxCBData;
}

/************************************************************************/
/*                       GDALApplyGeoTransform()                        */
/************************************************************************/

/**
 * Apply GeoTransform to x/y coordinate.
 *
 * Applies the following computation, converting a (pixel, line) coordinate
 * into a georeferenced (geo_x, geo_y) location.
 * \code{.c}
 *  *pdfGeoX = padfGeoTransform[0] + dfPixel * padfGeoTransform[1]
 *                                 + dfLine  * padfGeoTransform[2];
 *  *pdfGeoY = padfGeoTransform[3] + dfPixel * padfGeoTransform[4]
 *                                 + dfLine  * padfGeoTransform[5];
 * \endcode
 *
 * @param padfGeoTransform Six coefficient GeoTransform to apply.
 * @param dfPixel Input pixel position.
 * @param dfLine Input line position.
 * @param pdfGeoX output location where geo_x (easting/longitude)
 * location is placed.
 * @param pdfGeoY output location where geo_y (northing/latitude)
 * location is placed.
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
    // Special case - no rotation - to avoid computing determinate
    // and potential precision issues.
    if( gt_in[2] == 0.0 && gt_in[4] == 0.0 &&
        gt_in[1] != 0.0 && gt_in[5] != 0.0 )
    {
        /*X = gt_in[0] + x * gt_in[1]
          Y = gt_in[3] + y * gt_in[5]
          -->
          x = -gt_in[0] / gt_in[1] + (1 / gt_in[1]) * X
          y = -gt_in[3] / gt_in[5] + (1 / gt_in[5]) * Y
        */
        gt_out[0] = -gt_in[0] / gt_in[1];
        gt_out[1] = 1.0 / gt_in[1];
        gt_out[2] = 0.0;
        gt_out[3] = -gt_in[3] / gt_in[5];
        gt_out[4] = 0.0;
        gt_out[5] = 1.0 / gt_in[5];
        return 1;
    }

    // Assume a 3rd row that is [1 0 0].

    // Compute determinate.

    const double det = gt_in[1] * gt_in[5] - gt_in[2] * gt_in[4];
    const double magnitude = std::max(
            std::max(fabs(gt_in[1]), fabs(gt_in[2])),
            std::max(fabs(gt_in[4]), fabs(gt_in[5])));

    if( fabs(det) <= 1e-10 * magnitude * magnitude )
        return 0;

    const double inv_det = 1.0 / det;

    // Compute adjoint, and divide by determinate.

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

CPLXMLNode *GDALSerializeTransformer( GDALTransformerFunc /* pfnFunc */,
                                      void *pTransformArg )
{
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeTransformer", nullptr );

    GDALTransformerInfo *psInfo =
        static_cast<GDALTransformerInfo *>(pTransformArg);

    if( psInfo == nullptr ||
        memcmp(psInfo->abySignature,
               GDAL_GTI2_SIGNATURE,
               strlen(GDAL_GTI2_SIGNATURE)) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to serialize non-GTI2 transformer." );
        return nullptr;
    }
    else if( psInfo->pfnSerialize == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No serialization function available for this transformer." );
        return nullptr;
    }

    return psInfo->pfnSerialize( pTransformArg );
}

/************************************************************************/
/*                  GDALRegisterTransformDeserializer()                 */
/************************************************************************/

static CPLList* psListDeserializer = nullptr;
static CPLMutex* hDeserializerMutex = nullptr;

typedef struct
{
    char* pszTransformName;
    GDALTransformerFunc pfnTransformerFunc;
    GDALTransformDeserializeFunc pfnDeserializeFunc;
} TransformDeserializerInfo;

void *
GDALRegisterTransformDeserializer(
    const char* pszTransformName,
    GDALTransformerFunc pfnTransformerFunc,
    GDALTransformDeserializeFunc pfnDeserializeFunc )
{
    TransformDeserializerInfo* psInfo =
        static_cast<TransformDeserializerInfo *>(
            CPLMalloc(sizeof(TransformDeserializerInfo)));
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

void GDALUnregisterTransformDeserializer( void* pData )
{
    CPLMutexHolderD(&hDeserializerMutex);
    CPLList* psList = psListDeserializer;
    CPLList* psLast = nullptr;
    while( psList )
    {
        if( psList->pData == pData )
        {
            TransformDeserializerInfo* psInfo =
                static_cast<TransformDeserializerInfo *>(pData);
            CPLFree(psInfo->pszTransformName);
            CPLFree(pData);
            if( psLast )
                psLast->psNext = psList->psNext;
            else
                psListDeserializer = nullptr;
            CPLFree(psList);
            break;
        }
        psLast = psList;
        psList = psList->psNext;
    }
}

/************************************************************************/
/*                GDALUnregisterTransformDeserializer()                 */
/************************************************************************/

void GDALCleanupTransformDeserializerMutex()
{
    if( hDeserializerMutex != nullptr )
    {
        CPLDestroyMutex(hDeserializerMutex);
        hDeserializerMutex = nullptr;
    }
}

/************************************************************************/
/*                     GDALDeserializeTransformer()                     */
/************************************************************************/

CPLErr GDALDeserializeTransformer( CPLXMLNode *psTree,
                                   GDALTransformerFunc *ppfnFunc,
                                   void **ppTransformArg )

{
    *ppfnFunc = nullptr;
    *ppTransformArg = nullptr;

    CPLErrorReset();

    if( psTree == nullptr || psTree->eType != CXT_Element )
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Malformed element in GDALDeserializeTransformer" );
    else if( EQUAL(psTree->pszValue, "GenImgProjTransformer") )
    {
        *ppfnFunc = GDALGenImgProjTransform;
        *ppTransformArg = GDALDeserializeGenImgProjTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue, "ReprojectionTransformer") )
    {
        *ppfnFunc = GDALReprojectionTransform;
        *ppTransformArg = GDALDeserializeReprojectionTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue, "GCPTransformer") )
    {
        *ppfnFunc = GDALGCPTransform;
        *ppTransformArg = GDALDeserializeGCPTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue, "TPSTransformer") )
    {
        *ppfnFunc = GDALTPSTransform;
        *ppTransformArg = GDALDeserializeTPSTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue, "GeoLocTransformer") )
    {
        *ppfnFunc = GDALGeoLocTransform;
        *ppTransformArg = GDALDeserializeGeoLocTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue, "RPCTransformer") )
    {
        *ppfnFunc = GDALRPCTransform;
        *ppTransformArg = GDALDeserializeRPCTransformer( psTree );
    }
    else if( EQUAL(psTree->pszValue, "ApproxTransformer") )
    {
        *ppfnFunc = GDALApproxTransform;
        *ppTransformArg = GDALDeserializeApproxTransformer( psTree );
    }
    else
    {
        GDALTransformDeserializeFunc pfnDeserializeFunc = nullptr;
        {
            CPLMutexHolderD(&hDeserializerMutex);
            CPLList* psList = psListDeserializer;
            while( psList )
            {
                TransformDeserializerInfo* psInfo =
                    static_cast<TransformDeserializerInfo *>(psList->pData);
                if( strcmp(psInfo->pszTransformName, psTree->pszValue) == 0 )
                {
                    *ppfnFunc = psInfo->pfnTransformerFunc;
                    pfnDeserializeFunc = psInfo->pfnDeserializeFunc;
                    break;
                }
                psList = psList->psNext;
            }
        }

        if( pfnDeserializeFunc != nullptr )
        {
            *ppTransformArg = pfnDeserializeFunc( psTree );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unrecognized element '%s' GDALDeserializeTransformer",
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
    if( pTransformArg == nullptr )
        return;

    GDALTransformerInfo *psInfo =
        static_cast<GDALTransformerInfo *>(pTransformArg);

    if( memcmp(psInfo->abySignature,
               GDAL_GTI2_SIGNATURE,
               strlen(GDAL_GTI2_SIGNATURE)) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to destroy non-GTI2 transformer." );
        return;
    }

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
    GDALTransformerInfo *psInfo =
        static_cast<GDALTransformerInfo *>(pTransformArg);

    if( psInfo == nullptr ||
        memcmp(psInfo->abySignature,
               GDAL_GTI2_SIGNATURE,
               strlen(GDAL_GTI2_SIGNATURE)) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to use non-GTI2 transformer." );
        return FALSE;
    }

    return psInfo->pfnTransform(pTransformArg, bDstToSrc, nPointCount,
                                x, y, z, panSuccess);
}

/************************************************************************/
/*                        GDALCloneTransformer()                        */
/************************************************************************/

void *GDALCloneTransformer( void *pTransformArg )
{
    VALIDATE_POINTER1( pTransformArg, "GDALCloneTransformer", nullptr );

    GDALTransformerInfo *psInfo =
        static_cast<GDALTransformerInfo *>(pTransformArg);

    if( psInfo == nullptr ||
        memcmp(psInfo->abySignature,
               GDAL_GTI2_SIGNATURE,
               strlen(GDAL_GTI2_SIGNATURE)) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to clone non-GTI2 transformer." );
        return nullptr;
    }

    if( psInfo->pfnCreateSimilar != nullptr )
    {
        return psInfo->pfnCreateSimilar(psInfo, 1.0, 1.0);
    }

    if( psInfo->pfnSerialize == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No serialization function available for this transformer." );
        return nullptr;
    }

    CPLXMLNode* pSerialized = psInfo->pfnSerialize( pTransformArg );
    if( pSerialized == nullptr )
        return nullptr;
    GDALTransformerFunc pfnTransformer = nullptr;
    void *pClonedTransformArg = nullptr;
    if( GDALDeserializeTransformer( pSerialized, &pfnTransformer,
                                    &pClonedTransformArg ) !=
        CE_None )
    {
        CPLDestroyXMLNode(pSerialized);
        CPLFree(pClonedTransformArg);
        return nullptr;
    }

    CPLDestroyXMLNode(pSerialized);
    return pClonedTransformArg;
}

/************************************************************************/
/*                   GDALCreateSimilarTransformer()                     */
/************************************************************************/

void* GDALCreateSimilarTransformer( void* pTransformArg,
                                    double dfRatioX, double dfRatioY )
{
    VALIDATE_POINTER1( pTransformArg, "GDALCreateSimilarTransformer", nullptr );

    GDALTransformerInfo *psInfo =
        static_cast<GDALTransformerInfo *>(pTransformArg);

    if( psInfo == nullptr ||
        memcmp(psInfo->abySignature,
               GDAL_GTI2_SIGNATURE,
               strlen(GDAL_GTI2_SIGNATURE)) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to call CreateSimilar on a non-GTI2 transformer." );
        return nullptr;
    }

    if( psInfo->pfnCreateSimilar == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No CreateSimilar function available for this transformer.");
        return nullptr;
    }

    return psInfo->pfnCreateSimilar(psInfo, dfRatioX, dfRatioY);
}

/************************************************************************/
/*                      GetGenImgProjTransformInfo()                    */
/************************************************************************/

static GDALTransformerInfo* GetGenImgProjTransformInfo( const char* pszFunc,
                                                        void *pTransformArg )
{
    GDALTransformerInfo *psInfo =
        static_cast<GDALTransformerInfo *>(pTransformArg);

    if( psInfo == nullptr ||
        memcmp(psInfo->abySignature,
               GDAL_GTI2_SIGNATURE,
               strlen(GDAL_GTI2_SIGNATURE)) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to call %s on "
                 "a non-GTI2 transformer.", pszFunc);
        return nullptr;
    }

    if( EQUAL(psInfo->pszClassName, "GDALApproxTransformer") )
    {
        ApproxTransformInfo *psATInfo =
            static_cast<ApproxTransformInfo *>(pTransformArg);
        psInfo = static_cast<GDALTransformerInfo *>(psATInfo->pBaseCBData);

        if( psInfo == nullptr ||
            memcmp(psInfo->abySignature,
                   GDAL_GTI2_SIGNATURE,
                   strlen(GDAL_GTI2_SIGNATURE)) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to call %s on "
                     "a non-GTI2 transformer.", pszFunc);
            return nullptr;
        }
    }

    if( EQUAL(psInfo->pszClassName, "GDALGenImgProjTransformer") )
    {
        return psInfo;
    }

    return nullptr;
}

/************************************************************************/
/*                 GDALSetTransformerDstGeoTransform()                  */
/************************************************************************/

/**
 * Set ApproxTransformer or GenImgProj output geotransform.
 *
 * This is a layer above GDALSetGenImgProjTransformerDstGeoTransform() that
 * checks that the passed hTransformArg is compatible.
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
 * @param pTransformArg the handle to update.
 * @param padfGeoTransform the destination geotransform to apply (six doubles).
 */

void GDALSetTransformerDstGeoTransform( void *pTransformArg,
                                        const double *padfGeoTransform )
{
    VALIDATE_POINTER0( pTransformArg, "GDALSetTransformerDstGeoTransform" );

    GDALTransformerInfo* psInfo = GetGenImgProjTransformInfo(
        "GDALSetTransformerDstGeoTransform", pTransformArg );
    if( psInfo )
    {
        GDALSetGenImgProjTransformerDstGeoTransform(psInfo, padfGeoTransform);
    }
}

/************************************************************************/
/*                 GDALGetTransformerDstGeoTransform()                  */
/************************************************************************/

/**
 * Get ApproxTransformer or GenImgProj output geotransform.
 *
 * @param pTransformArg transformer handle.
 * @param padfGeoTransform (output) the destination geotransform to return (six doubles).
 */

void GDALGetTransformerDstGeoTransform( void *pTransformArg,
                                        double *padfGeoTransform )
{
    VALIDATE_POINTER0( pTransformArg, "GDALGetTransformerDstGeoTransform" );

    GDALTransformerInfo* psInfo = GetGenImgProjTransformInfo(
        "GDALGetTransformerDstGeoTransform", pTransformArg );
    if( psInfo )
    {
        GDALGenImgProjTransformInfo *psGenImgProjInfo =
            reinterpret_cast<GDALGenImgProjTransformInfo *>( psInfo );

        memcpy( padfGeoTransform, psGenImgProjInfo->adfDstGeoTransform,
                sizeof(double) * 6 );
    }
}

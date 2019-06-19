/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Compute each pixel's proximity to a set of target pixels.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"

CPL_CVSID("$Id$")

static CPLErr
ProcessProximityLine( GInt32 *panSrcScanline, int *panNearX, int *panNearY,
                      int bForward, int iLine, int nXSize, double nMaxDist,
                      float *pafProximity, double *pdfSrcNoDataValue,
                      int nTargetValues, int *panTargetValues );

/************************************************************************/
/*                        GDALComputeProximity()                        */
/************************************************************************/

/**
Compute the proximity of all pixels in the image to a set of pixels in
the source image.

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
queried for a nodata value.  If one is not found, 65535 will be used.

  USE_INPUT_NODATA=YES/NO

If this option is set, the input data set no-data value will be
respected. Leaving no data pixels in the input as no data pixels in
the proximity output.

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
    VALIDATE_POINTER1( hSrcBand, "GDALComputeProximity", CE_Failure );
    VALIDATE_POINTER1( hProximityBand, "GDALComputeProximity", CE_Failure );

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Are we using pixels or georeferenced coordinates for distances? */
/* -------------------------------------------------------------------- */
    double dfDistMult = 1.0;
    const char *pszOpt = CSLFetchNameValue( papszOptions, "DISTUNITS" );
    if( pszOpt )
    {
        if( EQUAL(pszOpt, "GEO") )
        {
            GDALDatasetH hSrcDS = GDALGetBandDataset( hSrcBand );
            if( hSrcDS )
            {
                double adfGeoTransform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

                GDALGetGeoTransform( hSrcDS, adfGeoTransform );
                if( std::abs(adfGeoTransform[1]) !=
                    std::abs(adfGeoTransform[5]) )
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Pixels not square, distances will be inaccurate." );
                dfDistMult = std::abs(adfGeoTransform[1]);
            }
        }
        else if( !EQUAL(pszOpt, "PIXEL") )
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Unrecognized DISTUNITS value '%s', should be GEO or PIXEL.",
                pszOpt );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      What is our maxdist value?                                      */
/* -------------------------------------------------------------------- */
    pszOpt = CSLFetchNameValue( papszOptions, "MAXDIST" );
    const double dfMaxDist = pszOpt ?
        CPLAtof(pszOpt) / dfDistMult :
        GDALGetRasterBandXSize(hSrcBand) + GDALGetRasterBandYSize(hSrcBand);

    CPLDebug( "GDAL", "MAXDIST=%g, DISTMULT=%g", dfMaxDist, dfDistMult );

/* -------------------------------------------------------------------- */
/*      Verify the source and destination are compatible.               */
/* -------------------------------------------------------------------- */
    const int nXSize = GDALGetRasterBandXSize( hSrcBand );
    const int nYSize = GDALGetRasterBandYSize( hSrcBand );
    if( nXSize != GDALGetRasterBandXSize( hProximityBand )
        || nYSize != GDALGetRasterBandYSize( hProximityBand ))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Source and proximity bands are not the same size." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get input NODATA value.                                         */
/* -------------------------------------------------------------------- */
    double dfSrcNoDataValue = 0.0;
    double *pdfSrcNoData = nullptr;
    if( CPLFetchBool( papszOptions, "USE_INPUT_NODATA", false ) )
    {
        int bSrcHasNoData = 0;
        dfSrcNoDataValue = GDALGetRasterNoDataValue( hSrcBand, &bSrcHasNoData );
        if( bSrcHasNoData )
            pdfSrcNoData = &dfSrcNoDataValue;
    }

/* -------------------------------------------------------------------- */
/*      Get output NODATA value.                                        */
/* -------------------------------------------------------------------- */
    float fNoDataValue = 0.0f;
    pszOpt = CSLFetchNameValue( papszOptions, "NODATA" );
    if( pszOpt != nullptr )
    {
        fNoDataValue = static_cast<float>(CPLAtof(pszOpt));
    }
    else
    {
        int bSuccess = FALSE;

        fNoDataValue = static_cast<float>(
            GDALGetRasterNoDataValue( hProximityBand, &bSuccess ) );
        if( !bSuccess )
            fNoDataValue = 65535.0;
    }

/* -------------------------------------------------------------------- */
/*      Is there a fixed value we wish to force the buffer area to?     */
/* -------------------------------------------------------------------- */
    double dfFixedBufVal = 0.0;
    bool bFixedBufVal = false;
    pszOpt = CSLFetchNameValue( papszOptions, "FIXED_BUF_VAL" );
    if( pszOpt )
    {
        dfFixedBufVal = CPLAtof(pszOpt);
        bFixedBufVal = true;
    }

/* -------------------------------------------------------------------- */
/*      Get the target value(s).                                        */
/* -------------------------------------------------------------------- */
    int *panTargetValues = nullptr;
    int nTargetValues = 0;

    pszOpt = CSLFetchNameValue( papszOptions, "VALUES" );
    if( pszOpt != nullptr )
    {
        char **papszValuesTokens =
            CSLTokenizeStringComplex( pszOpt, ",", FALSE, FALSE);

        nTargetValues = CSLCount(papszValuesTokens);
        panTargetValues = static_cast<int *>(
            CPLCalloc(sizeof(int), nTargetValues) );

        for( int i = 0; i < nTargetValues; i++ )
            panTargetValues[i] = atoi(papszValuesTokens[i]);
        CSLDestroy( papszValuesTokens );
    }

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, "", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        CPLFree(panTargetValues);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      We need a signed type for the working proximity values kept     */
/*      on disk.  If our proximity band is not signed, then create a    */
/*      temporary file for this purpose.                                */
/* -------------------------------------------------------------------- */
    GDALRasterBandH hWorkProximityBand = hProximityBand;
    GDALDatasetH hWorkProximityDS = nullptr;
    const GDALDataType eProxType = GDALGetRasterDataType(hProximityBand);
    CPLErr eErr = CE_None;

    // TODO(schwehr): Localize after removing gotos.
    float *pafProximity = nullptr;
    int *panNearX = nullptr;
    int *panNearY = nullptr;
    GInt32 *panSrcScanline = nullptr;
    bool bTempFileAlreadyDeleted = false;

    if( eProxType == GDT_Byte
        || eProxType == GDT_UInt16
        || eProxType == GDT_UInt32 )
    {
        GDALDriverH hDriver = GDALGetDriverByName("GTiff");
        if( hDriver == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GDALComputeProximity needs GTiff driver" );
            eErr = CE_Failure;
            goto end;
        }
        CPLString osTmpFile = CPLGenerateTempFilename( "proximity" );
        hWorkProximityDS =
            GDALCreate( hDriver, osTmpFile,
                        nXSize, nYSize, 1, GDT_Float32, nullptr );
        if( hWorkProximityDS == nullptr )
        {
            eErr = CE_Failure;
            goto end;
        }
        // On Unix, attempt at deleting the temporary file now, so that
        // if the process gets interrupted, it is automatically destroyed
        // by the operating system.
        bTempFileAlreadyDeleted = VSIUnlink( osTmpFile ) == 0;
        hWorkProximityBand = GDALGetRasterBand( hWorkProximityDS, 1 );
    }

/* -------------------------------------------------------------------- */
/*      Allocate buffer for two scanlines of distances as floats        */
/*      (the current and last line).                                    */
/* -------------------------------------------------------------------- */
    pafProximity =
        static_cast<float *>(VSI_MALLOC2_VERBOSE(sizeof(float), nXSize));
    panNearX =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nXSize));
    panNearY =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nXSize));
    panSrcScanline =
        static_cast<GInt32 *>(VSI_MALLOC2_VERBOSE(sizeof(GInt32), nXSize));

    if( pafProximity == nullptr
        || panNearX == nullptr
        || panNearY == nullptr
        || panSrcScanline == nullptr)
    {
        eErr = CE_Failure;
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Loop from top to bottom of the image.                           */
/* -------------------------------------------------------------------- */

    for( int i = 0; i < nXSize; i++ )
    {
        panNearX[i] = -1;
        panNearY[i] = -1;
    }

    for( int iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
    {
        // Read for target values.
        eErr = GDALRasterIO( hSrcBand, GF_Read, 0, iLine, nXSize, 1,
                             panSrcScanline, nXSize, 1, GDT_Int32, 0, 0 );
        if( eErr != CE_None )
            break;

        for( int i = 0; i < nXSize; i++ )
            pafProximity[i] = -1.0;

        // Left to right.
        ProcessProximityLine( panSrcScanline, panNearX, panNearY,
                              TRUE, iLine, nXSize, dfMaxDist, pafProximity,
                              pdfSrcNoData, nTargetValues, panTargetValues );

        // Right to Left.
        ProcessProximityLine( panSrcScanline, panNearX, panNearY,
                              FALSE, iLine, nXSize, dfMaxDist, pafProximity,
                              pdfSrcNoData, nTargetValues, panTargetValues );

        // Write out results.
        eErr =
            GDALRasterIO( hWorkProximityBand, GF_Write, 0, iLine, nXSize, 1,
                          pafProximity, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

        if( !pfnProgress( 0.5 * (iLine+1) / static_cast<double>(nYSize),
                          "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop from bottom to top of the image.                           */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nXSize; i++ )
    {
        panNearX[i] = -1;
        panNearY[i] = -1;
    }

    for( int iLine = nYSize-1; eErr == CE_None && iLine >= 0; iLine-- )
    {
        // Read first pass proximity.
        eErr =
            GDALRasterIO( hWorkProximityBand, GF_Read, 0, iLine, nXSize, 1,
                          pafProximity, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

        // Read pixel values.

        eErr = GDALRasterIO( hSrcBand, GF_Read, 0, iLine, nXSize, 1,
                             panSrcScanline, nXSize, 1, GDT_Int32, 0, 0 );
        if( eErr != CE_None )
            break;

        // Right to left.
        ProcessProximityLine( panSrcScanline, panNearX, panNearY,
                              FALSE, iLine, nXSize, dfMaxDist, pafProximity,
                              pdfSrcNoData, nTargetValues, panTargetValues );

        // Left to right.
        ProcessProximityLine( panSrcScanline, panNearX, panNearY,
                              TRUE, iLine, nXSize, dfMaxDist, pafProximity,
                              pdfSrcNoData, nTargetValues, panTargetValues );

        // Final post processing of distances.
        for( int i = 0; i < nXSize; i++ )
        {
            if( pafProximity[i] < 0.0 )
                pafProximity[i] = fNoDataValue;
            else if( pafProximity[i] > 0.0 )
            {
                if( bFixedBufVal )
                    pafProximity[i] = static_cast<float>( dfFixedBufVal );
                else
                    pafProximity[i] =
                        static_cast<float>(pafProximity[i] * dfDistMult);
            }
        }

        // Write out results.
        eErr =
            GDALRasterIO( hProximityBand, GF_Write, 0, iLine, nXSize, 1,
                          pafProximity, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

        if( !pfnProgress( 0.5 +
                          0.5 * (nYSize-iLine) / static_cast<double>( nYSize ),
                          "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
end:
    CPLFree( panNearX );
    CPLFree( panNearY );
    CPLFree( panSrcScanline );
    CPLFree( pafProximity );
    CPLFree( panTargetValues );

    if( hWorkProximityDS != nullptr )
    {
        CPLString osProxFile = GDALGetDescription( hWorkProximityDS );
        GDALClose( hWorkProximityDS );
        if( !bTempFileAlreadyDeleted )
        {
            GDALDeleteDataset( GDALGetDriverByName( "GTiff" ), osProxFile );
        }
    }

    return eErr;
}

/************************************************************************/
/*                         SquareDistance()                             */
/************************************************************************/

static double SquareDistance(double dfX1, double dfX2,
                             double dfY1, double dfY2)
{
    const double dfDX = dfX1 - dfX2;
    const double dfDY = dfY1 - dfY2;
    return dfDX * dfDX + dfDY * dfDY;
}

/************************************************************************/
/*                        ProcessProximityLine()                        */
/************************************************************************/

static CPLErr
ProcessProximityLine( GInt32 *panSrcScanline, int *panNearX, int *panNearY,
                      int bForward, int iLine, int nXSize, double dfMaxDist,
                      float *pafProximity, double *pdfSrcNoDataValue,
                      int nTargetValues, int *panTargetValues )

{
    const int iStart = bForward ? 0 : nXSize - 1;
    const int iEnd = bForward ? nXSize : -1;
    const int iStep = bForward ? 1 : -1;

    for( int iPixel = iStart; iPixel != iEnd; iPixel += iStep )
    {
        bool bIsTarget = false;

/* -------------------------------------------------------------------- */
/*      Is the current pixel a target pixel?                            */
/* -------------------------------------------------------------------- */
        if( nTargetValues == 0 )
        {
            bIsTarget = panSrcScanline[iPixel] != 0;
        }
        else
        {
            for( int i = 0; i < nTargetValues; i++ )
            {
                if( panSrcScanline[iPixel] == panTargetValues[i] )
                    bIsTarget = TRUE;
            }
        }

        if( bIsTarget )
        {
            pafProximity[iPixel] = 0.0;
            panNearX[iPixel] = iPixel;
            panNearY[iPixel] = iLine;
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Are we near(er) to the closest target to the above (below)      */
/*      pixel?                                                          */
/* -------------------------------------------------------------------- */
        double dfNearDistSq =
                std::max(dfMaxDist, static_cast<double>(nXSize)) *
                std::max(dfMaxDist, static_cast<double>(nXSize)) * 2.0;

        if( panNearX[iPixel] != -1 )
        {
            const double dfDistSq =
                SquareDistance(panNearX[iPixel], iPixel,
                               panNearY[iPixel], iLine);

            if( dfDistSq < dfNearDistSq )
            {
                dfNearDistSq = dfDistSq;
            }
            else
            {
                panNearX[iPixel] = -1;
                panNearY[iPixel] = -1;
            }
        }

/* -------------------------------------------------------------------- */
/*      Are we near(er) to the closest target to the left (right)       */
/*      pixel?                                                          */
/* -------------------------------------------------------------------- */
        const int iLast = iPixel - iStep;

        if( iPixel != iStart && panNearX[iLast] != -1 )
        {
            const double dfDistSq =
                SquareDistance(panNearX[iLast], iPixel,
                               panNearY[iLast], iLine);

            if( dfDistSq < dfNearDistSq )
            {
                dfNearDistSq = dfDistSq;
                panNearX[iPixel] = panNearX[iLast];
                panNearY[iPixel] = panNearY[iLast];
            }
        }

/* -------------------------------------------------------------------- */
/*      Are we near(er) to the closest target to the topright           */
/*      (bottom left) pixel?                                            */
/* -------------------------------------------------------------------- */
        const int iTR = iPixel + iStep;

        if( iTR != iEnd && panNearX[iTR] != -1 )
        {
            const double dfDistSq =
                SquareDistance(panNearX[iTR], iPixel,
                               panNearY[iTR], iLine);

            if( dfDistSq < dfNearDistSq )
            {
                dfNearDistSq = dfDistSq;
                panNearX[iPixel] = panNearX[iTR];
                panNearY[iPixel] = panNearY[iTR];
            }
        }

/* -------------------------------------------------------------------- */
/*      Update our proximity value.                                     */
/* -------------------------------------------------------------------- */
        if( panNearX[iPixel] != -1
            && (pdfSrcNoDataValue == nullptr
                || panSrcScanline[iPixel] != *pdfSrcNoDataValue)
            && dfNearDistSq <= dfMaxDist * dfMaxDist
            && (pafProximity[iPixel] < 0
                || dfNearDistSq < pafProximity[iPixel] * pafProximity[iPixel]) )
            pafProximity[iPixel] = static_cast<float>(sqrt(dfNearDistSq));
    }

    return CE_None;
}

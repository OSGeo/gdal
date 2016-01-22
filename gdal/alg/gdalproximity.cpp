/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Compute each pixel's proximity to a set of target pixels.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

static CPLErr
ProcessProximityLine( GInt32 *panSrcScanline, int *panNearX, int *panNearY, 
                      int bForward, int iLine, int nXSize, double nMaxDist,
                      float *pafProximity,
                      int nTargetValues, int *panTargetValues );

/************************************************************************/
/*                        GDALComputeProximity()                        */
/************************************************************************/

/**
Compute the proximity of all pixels in the image to a set of pixels in the source image.

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
    int nXSize, nYSize, i, bFixedBufVal = FALSE;
    const char *pszOpt;
    double dfMaxDist;
    double dfFixedBufVal = 0.0;

    VALIDATE_POINTER1( hSrcBand, "GDALComputeProximity", CE_Failure );
    VALIDATE_POINTER1( hProximityBand, "GDALComputeProximity", CE_Failure );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Are we using pixels or georeferenced coordinates for distances? */
/* -------------------------------------------------------------------- */
    double dfDistMult = 1.0;
    pszOpt = CSLFetchNameValue( papszOptions, "DISTUNITS" );
    if( pszOpt )
    {
        if( EQUAL(pszOpt,"GEO") )
        {
            GDALDatasetH hSrcDS = GDALGetBandDataset( hSrcBand );
            if( hSrcDS )
            {
                double adfGeoTransform[6];

                GDALGetGeoTransform( hSrcDS, adfGeoTransform );
                if( ABS(adfGeoTransform[1]) != ABS(adfGeoTransform[5]) )
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Pixels not square, distances will be inaccurate." );
                dfDistMult = ABS(adfGeoTransform[1]);
            }
        }
        else if( !EQUAL(pszOpt,"PIXEL") )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unrecognised DISTUNITS value '%s', should be GEO or PIXEL.",
                      pszOpt );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      What is our maxdist value?                                      */
/* -------------------------------------------------------------------- */
    pszOpt = CSLFetchNameValue( papszOptions, "MAXDIST" );
    if( pszOpt )
        dfMaxDist = atof(pszOpt) / dfDistMult;
    else
        dfMaxDist = GDALGetRasterBandXSize(hSrcBand) + GDALGetRasterBandYSize(hSrcBand);

    CPLDebug( "GDAL", "MAXDIST=%g, DISTMULT=%g", dfMaxDist, dfDistMult );

/* -------------------------------------------------------------------- */
/*      Verify the source and destination are compatible.               */
/* -------------------------------------------------------------------- */
    nXSize = GDALGetRasterBandXSize( hSrcBand );
    nYSize = GDALGetRasterBandYSize( hSrcBand );
    if( nXSize != GDALGetRasterBandXSize( hProximityBand )
        || nYSize != GDALGetRasterBandYSize( hProximityBand ))
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Source and proximity bands are not the same size." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get output NODATA value.                                        */
/* -------------------------------------------------------------------- */
    float fNoDataValue;
    pszOpt = CSLFetchNameValue( papszOptions, "NODATA" );
    if( pszOpt != NULL )
        fNoDataValue = (float) atof(pszOpt);
    else
    {
        int bSuccess;

        fNoDataValue = (float) GDALGetRasterNoDataValue( hProximityBand, &bSuccess );
        if( !bSuccess )
            fNoDataValue = 65535.0;
    }
    
/* -------------------------------------------------------------------- */
/*      Is there a fixed value we wish to force the buffer area to?     */
/* -------------------------------------------------------------------- */
    pszOpt = CSLFetchNameValue( papszOptions, "FIXED_BUF_VAL" );
    if( pszOpt )
    {
        dfFixedBufVal = atof(pszOpt);
        bFixedBufVal = TRUE;
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
        CPLFree(panTargetValues);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      We need a signed type for the working proximity values kept     */
/*      on disk.  If our proximity band is not signed, then create a    */
/*      temporary file for this purpose.                                */
/* -------------------------------------------------------------------- */
    GDALRasterBandH hWorkProximityBand = hProximityBand;
    GDALDatasetH hWorkProximityDS = NULL;
    GDALDataType eProxType = GDALGetRasterDataType( hProximityBand );
    int   *panNearX = NULL, *panNearY = NULL;
    float *pafProximity = NULL;
    GInt32 *panSrcScanline = NULL;
    int iLine;
    CPLErr eErr = CE_None;

    if( eProxType == GDT_Byte 
        || eProxType == GDT_UInt16
        || eProxType == GDT_UInt32 )
    {
        GDALDriverH hDriver = GDALGetDriverByName("GTiff");
        if (hDriver == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALComputeProximity needs GTiff driver");
            eErr = CE_Failure;
            goto end;
        }
        CPLString osTmpFile = CPLGenerateTempFilename( "proximity" );
        hWorkProximityDS = 
            GDALCreate( hDriver, osTmpFile,
                        nXSize, nYSize, 1, GDT_Float32, NULL );
        if (hWorkProximityDS == NULL)
        {
            eErr = CE_Failure;
            goto end;
        }
        hWorkProximityBand = GDALGetRasterBand( hWorkProximityDS, 1 );
    }

/* -------------------------------------------------------------------- */
/*      Allocate buffer for two scanlines of distances as floats        */
/*      (the current and last line).                                    */
/* -------------------------------------------------------------------- */
    pafProximity = (float *) VSIMalloc2(sizeof(float), nXSize);
    panNearX = (int *) VSIMalloc2(sizeof(int), nXSize);
    panNearY = (int *) VSIMalloc2(sizeof(int), nXSize);
    panSrcScanline = (GInt32 *) VSIMalloc2(sizeof(GInt32), nXSize);

    if( pafProximity== NULL 
        || panNearX == NULL 
        || panNearY == NULL
        || panSrcScanline == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory allocating working buffers.");
        eErr = CE_Failure;
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Loop from top to bottom of the image.                           */
/* -------------------------------------------------------------------- */

    for( i = 0; i < nXSize; i++ )
        panNearX[i] = panNearY[i] = -1;

    for( iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
    {
        // Read for target values.
        eErr = GDALRasterIO( hSrcBand, GF_Read, 0, iLine, nXSize, 1, 
                             panSrcScanline, nXSize, 1, GDT_Int32, 0, 0 );
        if( eErr != CE_None )
            break;

        for( i = 0; i < nXSize; i++ )
            pafProximity[i] = -1.0;

        // Left to right
        ProcessProximityLine( panSrcScanline, panNearX, panNearY, 
                              TRUE, iLine, nXSize, dfMaxDist,
                              pafProximity, nTargetValues, panTargetValues );

        // Right to Left
        ProcessProximityLine( panSrcScanline, panNearX, panNearY, 
                              FALSE, iLine, nXSize, dfMaxDist,
                              pafProximity, nTargetValues, panTargetValues );

        // Write out results.
        eErr = 
            GDALRasterIO( hWorkProximityBand, GF_Write, 0, iLine, nXSize, 1, 
                          pafProximity, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

        if( !pfnProgress( 0.5 * (iLine+1) / (double) nYSize, 
                          "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop from bottom to top of the image.                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nXSize; i++ )
        panNearX[i] = panNearY[i] = -1;

    for( iLine = nYSize-1; eErr == CE_None && iLine >= 0; iLine-- )
    {
        // Read first pass proximity
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

        // Right to left
        ProcessProximityLine( panSrcScanline, panNearX, panNearY, 
                              FALSE, iLine, nXSize, dfMaxDist,
                              pafProximity, nTargetValues, panTargetValues );

        // Left to right
        ProcessProximityLine( panSrcScanline, panNearX, panNearY, 
                              TRUE, iLine, nXSize, dfMaxDist,
                              pafProximity, nTargetValues, panTargetValues );

        // Final post processing of distances. 
        for( i = 0; i < nXSize; i++ )
        {
            if( pafProximity[i] < 0.0 )
                pafProximity[i] = fNoDataValue;
            else if( pafProximity[i] > 0.0 )
            {
                if( bFixedBufVal )
                    pafProximity[i] = (float) dfFixedBufVal;
                else 
                    pafProximity[i] = (float)(pafProximity[i] * dfDistMult);
            }
        }
  
        // Write out results.
        eErr = 
            GDALRasterIO( hProximityBand, GF_Write, 0, iLine, nXSize, 1, 
                          pafProximity, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

        if( !pfnProgress( 0.5 + 0.5 * (nYSize-iLine) / (double) nYSize, 
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
    CPLFree(panTargetValues);

    if( hWorkProximityDS != NULL )
    {
        CPLString osProxFile = GDALGetDescription( hWorkProximityDS );
        GDALClose( hWorkProximityDS );
        GDALDeleteDataset( GDALGetDriverByName( "GTiff" ), osProxFile );
    }

    return eErr;
}

/************************************************************************/
/*                        ProcessProximityLine()                        */
/************************************************************************/
                      
static CPLErr
ProcessProximityLine( GInt32 *panSrcScanline, int *panNearX, int *panNearY, 
                      int bForward, int iLine, int nXSize, double dfMaxDist,
                      float *pafProximity,
                      int nTargetValues, int *panTargetValues )

{
    int iStart, iEnd, iStep, iPixel;

    if( bForward )
    {
        iStart = 0;
        iEnd = nXSize; 
        iStep = 1;
    }
    else
    {
        iStart = nXSize-1;
        iEnd = -1; 
        iStep = -1;
    }

    for( iPixel = iStart; iPixel != iEnd; iPixel += iStep )
    {
        int bIsTarget = FALSE;

/* -------------------------------------------------------------------- */
/*      Is the current pixel a target pixel?                            */
/* -------------------------------------------------------------------- */
        if( nTargetValues == 0 )
            bIsTarget = (panSrcScanline[iPixel] != 0);
        else
        {
            int i;

            for( i = 0; i < nTargetValues; i++ )
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
        float fNearDistSq = (float) (MAX(dfMaxDist,nXSize) * MAX(dfMaxDist,nXSize) * 2);
        float fDistSq;

        if( panNearX[iPixel] != -1 )
        {
            fDistSq = (float)
                ((panNearX[iPixel] - iPixel) * (panNearX[iPixel] - iPixel)
                 + (panNearY[iPixel] - iLine) * (panNearY[iPixel] - iLine));

            if( fDistSq < fNearDistSq )
            {
                fNearDistSq = fDistSq;
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
        int iLast = iPixel-iStep;

        if( iPixel != iStart && panNearX[iLast] != -1 )
        {
            fDistSq = (float)
                ((panNearX[iLast] - iPixel) * (panNearX[iLast] - iPixel)
                 + (panNearY[iLast] - iLine) * (panNearY[iLast] - iLine));

            if( fDistSq < fNearDistSq )
            {
                fNearDistSq = fDistSq;
                panNearX[iPixel] = panNearX[iLast];
                panNearY[iPixel] = panNearY[iLast];
            }
        }

/* -------------------------------------------------------------------- */
/*      Are we near(er) to the closest target to the topright           */
/*      (bottom left) pixel?                                            */
/* -------------------------------------------------------------------- */
        int iTR = iPixel+iStep;

        if( iTR != iEnd && panNearX[iTR] != -1 )
        {
            fDistSq = (float)
                ((panNearX[iTR] - iPixel) * (panNearX[iTR] - iPixel)
                 + (panNearY[iTR] - iLine) * (panNearY[iTR] - iLine));

            if( fDistSq < fNearDistSq )
            {
                fNearDistSq = fDistSq;
                panNearX[iPixel] = panNearX[iTR];
                panNearY[iPixel] = panNearY[iTR];
            }
        }

/* -------------------------------------------------------------------- */
/*      Update our proximity value.                                     */
/* -------------------------------------------------------------------- */
        if( panNearX[iPixel] != -1 
            && fNearDistSq <= dfMaxDist * dfMaxDist
            && (pafProximity[iPixel] < 0 
                || fNearDistSq < pafProximity[iPixel] * pafProximity[iPixel]) )
            pafProximity[iPixel] = sqrt(fNearDistSq);
    }

    return CE_None;
}

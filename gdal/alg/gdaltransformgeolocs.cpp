/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Algorithm to apply a transformer to geolocation style bands.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Frank Warmerdam
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

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "gdal.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                     GDALTransformGeolocations()                      */
/************************************************************************/

/**
 * Transform locations held in bands.
 *
 * The X/Y and possibly Z values in the identified bands are transformed
 * using a spatial transformer.  The changes values are written back to the
 * source bands so they need to updatable.
 *
 * @param hXBand the band containing the X locations (usually long/easting).
 * @param hYBand the band containing the Y locations (usually lat/northing).
 * @param hZBand the band containing the Z locations (may be NULL).
 * @param pfnTransformer the transformer function.
 * @param pTransformArg the callback data for the transformer function.
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 * @param papszOptions list of name/value options - none currently supported.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr
GDALTransformGeolocations( GDALRasterBandH hXBand,
                           GDALRasterBandH hYBand,
                           GDALRasterBandH hZBand,
                           GDALTransformerFunc pfnTransformer,
                           void *pTransformArg,
                           GDALProgressFunc pfnProgress,
                           void *pProgressArg,
                           CPL_UNUSED char **papszOptions )

{
    VALIDATE_POINTER1( hXBand, "GDALTransformGeolocations", CE_Failure );
    VALIDATE_POINTER1( hYBand, "GDALTransformGeolocations", CE_Failure );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Ensure the bands are matching in size.                          */
/* -------------------------------------------------------------------- */
    GDALRasterBand *poXBand = reinterpret_cast<GDALRasterBand *>(hXBand);
    GDALRasterBand *poYBand = reinterpret_cast<GDALRasterBand *>(hYBand);
    GDALRasterBand *poZBand = reinterpret_cast<GDALRasterBand *>(hZBand);
    const int nXSize = poXBand->GetXSize();
    const int nYSize = poXBand->GetYSize();

    if( nXSize != poYBand->GetXSize()
        || nYSize != poYBand->GetYSize()
        || (poZBand != NULL && nXSize != poZBand->GetXSize())
        || (poZBand != NULL && nYSize != poZBand->GetYSize()) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Size of X, Y and/or Z bands do not match." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a buffer large enough to hold one whole row.           */
/* -------------------------------------------------------------------- */
    double *padfX = static_cast<double *>(CPLMalloc(sizeof(double) * nXSize));
    double *padfY = static_cast<double *>(CPLMalloc(sizeof(double) * nXSize));
    double *padfZ = static_cast<double *>(CPLMalloc(sizeof(double) * nXSize));
    int *panSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nXSize));
    CPLErr eErr = CE_None;

    pfnProgress( 0.0, "", pProgressArg );
    for( int iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
    {
        eErr = poXBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                                  padfX, nXSize, 1, GDT_Float64, 0, 0, NULL );
        if( eErr == CE_None )
            eErr = poYBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                                      padfY, nXSize, 1, GDT_Float64,
                                      0, 0, NULL );
        if( eErr == CE_None && poZBand != NULL )
            eErr = poZBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                                      padfZ, nXSize, 1, GDT_Float64,
                                      0, 0, NULL );
        else
            memset( padfZ, 0, sizeof(double) * nXSize);

        if( eErr == CE_None )
        {
            pfnTransformer( pTransformArg, FALSE, nXSize,
                            padfX, padfY, padfZ, panSuccess );
        }

        if( eErr == CE_None )
            eErr = poXBand->RasterIO( GF_Write, 0, iLine, nXSize, 1,
                                      padfX, nXSize, 1, GDT_Float64,
                                      0, 0, NULL );
        if( eErr == CE_None )
            eErr = poYBand->RasterIO( GF_Write, 0, iLine, nXSize, 1,
                                      padfY, nXSize, 1, GDT_Float64,
                                      0, 0, NULL );
        if( eErr == CE_None && poZBand != NULL )
            eErr = poZBand->RasterIO( GF_Write, 0, iLine, nXSize, 1,
                                      padfZ, nXSize, 1, GDT_Float64,
                                      0, 0, NULL );

        if( eErr == CE_None )
            pfnProgress( (iLine+1) /
                         static_cast<double>(nYSize), "", pProgressArg );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( panSuccess );

    return eErr;
}

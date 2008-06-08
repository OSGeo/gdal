/******************************************************************************
 * $Id: gdalwarper.cpp 13803 2008-02-17 05:28:42Z warmerdam $
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implement cutline/blend mask generator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdalwarper.h"
#include "gdal_alg.h"
#include "ogr_api.h"
#include "memdataset.h"

CPL_CVSID("$Id: gdalwarper.cpp 13803 2008-02-17 05:28:42Z warmerdam $");

/************************************************************************/
/*                       GDALWarpCutlineMasker()                        */
/*                                                                      */
/*      This function will generate a source mask based on a            */
/*      provided cutline, and optional blend distance.                  */
/************************************************************************/

CPLErr 
GDALWarpCutlineMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       GByte ** /*ppImageData */,
                       int bMaskIsFloat, void *pValidityMask )

{
    GDALWarpOptions *psWO = (GDALWarpOptions *) pMaskFuncArg;
    float *pafMask = (float *) pValidityMask;
    CPLErr eErr;

/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( !bMaskIsFloat )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    if( psWO == NULL || psWO->hCutline == NULL )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Check the polygon.                                              */
/* -------------------------------------------------------------------- */
    OGRGeometryH hPolygon = (OGRGeometryH) psWO->hCutline;
    OGREnvelope  sEnvelope;

    if( wkbFlatten(OGR_G_GetGeometryType(hPolygon)) != wkbPolygon
        && wkbFlatten(OGR_G_GetGeometryType(hPolygon)) != wkbMultiPolygon )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    OGR_G_GetEnvelope( hPolygon, &sEnvelope );
    if( sEnvelope.MaxX + psWO->dfCutlineBlendDist < nXOff
        || sEnvelope.MinX - psWO->dfCutlineBlendDist > nXOff + nXSize
        || sEnvelope.MaxY + psWO->dfCutlineBlendDist < nYOff
        || sEnvelope.MinY - psWO->dfCutlineBlendDist > nYOff + nYSize )
    {
        // We are far from the blend line - everything is masked to zero.
        // It would be nice to realize no work is required for this whole
        // chunk!
        memset( pafMask, 0, sizeof(float) * nXSize * nYSize );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Create a byte buffer into which we can burn the                 */
/*      mask polygon and wrap it up as a memory dataset.                */
/* -------------------------------------------------------------------- */
    GByte *pabyPolyMask = (GByte *) CPLCalloc( nXSize, nYSize );
    GDALDatasetH hMemDS;
    CPLString osDPOption;
    char *apszOptions[2] = { NULL, NULL };
    double adfGeoTransform[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

    osDPOption.Printf( "DATAPOINTER=%p", pabyPolyMask );
    apszOptions[0] = (char *) osDPOption.c_str();

    hMemDS = GDALCreate( GDALGetDriverByName("MEM"), "warp_temp", 
                         nXSize, nYSize, 0, GDT_Byte, NULL );
    GDALAddBand( hMemDS, GDT_Byte, apszOptions );
    GDALSetGeoTransform( hMemDS, adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Burn the polygon into the mask with 1.0 values.                 */
/* -------------------------------------------------------------------- */
    int nTargetBand = 1;
    double dfBurnValue = 255.0;

    eErr = 
        GDALRasterizeGeometries( hMemDS, 1, &nTargetBand, 
                                 1, &hPolygon, 
                                 NULL, NULL, &dfBurnValue, NULL, NULL, NULL );

    // Close and ensure data flushed to underlying array.
    GDALClose( hMemDS );
/* -------------------------------------------------------------------- */
/*      In the case with no blend distance, we just apply this as a     */
/*      mask, zeroing out everything outside the polygon.               */
/* -------------------------------------------------------------------- */
    if( psWO->dfCutlineBlendDist == 0.0 )
    {
        int i;

        for( i = nXSize * nYSize - 1; i >= 0; i-- )
        {
            if( pabyPolyMask[i] == 0 )
                ((float *) pValidityMask)[i] = 0.0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Clean up.                                                       */
/* -------------------------------------------------------------------- */
    CPLFree( pabyPolyMask );

    return eErr;
}


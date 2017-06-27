/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implement cutline/blend mask generator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdalwarper.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_geos.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         BlendMaskGenerator()                         */
/************************************************************************/

#ifndef HAVE_GEOS

static CPLErr
BlendMaskGenerator( int /* nXOff */, int /* nYOff */,
                    int /* nXSize */, int /* nYSize */,
                    GByte * /* pabyPolyMask */,
                    float * /* pafValidityMask */,
                    OGRGeometryH /* hPolygon */,
                    double /* dfBlendDist */ )
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "Blend distance support not available without the GEOS library.");
    return CE_Failure;
}
#else
static CPLErr
BlendMaskGenerator( int nXOff, int nYOff, int nXSize, int nYSize,
                    GByte *pabyPolyMask, float *pafValidityMask,
                    OGRGeometryH hPolygon, double dfBlendDist )
{

/* -------------------------------------------------------------------- */
/*      Convert the polygon into a collection of lines so that we       */
/*      measure distance from the edge even on the inside.              */
/* -------------------------------------------------------------------- */
    OGRGeometry *poLines =
        OGRGeometryFactory::forceToMultiLineString(
            reinterpret_cast<OGRGeometry *>(hPolygon)->clone() );

/* -------------------------------------------------------------------- */
/*      Prepare a clipping polygon a bit bigger than the area of        */
/*      interest in the hopes of simplifying the cutline down to        */
/*      stuff that will be relevant for this area of interest.          */
/* -------------------------------------------------------------------- */
    CPLString osClipRectWKT;

    osClipRectWKT.Printf( "POLYGON((%g %g,%g %g,%g %g,%g %g,%g %g))",
                          nXOff - (dfBlendDist + 1),
                          nYOff - (dfBlendDist + 1),
                          nXOff + nXSize + (dfBlendDist + 1),
                          nYOff - (dfBlendDist + 1),
                          nXOff + nXSize + (dfBlendDist + 1),
                          nYOff + nYSize + (dfBlendDist + 1),
                          nXOff - (dfBlendDist + 1),
                          nYOff + nYSize + (dfBlendDist + 1),
                          nXOff - (dfBlendDist + 1),
                          nYOff - (dfBlendDist + 1) );

    OGRPolygon *poClipRect = NULL;
    char *pszWKT = const_cast<char *>(osClipRectWKT.c_str());

    OGRGeometryFactory::createFromWkt( &pszWKT, NULL,
                                       (OGRGeometry**) (&poClipRect) );

    if( poClipRect )
    {
        // If it does not intersect the polym, zero the mask and return.
        if( !reinterpret_cast<OGRGeometry *>(hPolygon)->Intersects(poClipRect) )
        {
            memset( pafValidityMask, 0, sizeof(float) * nXSize * nYSize );

            delete poLines;
            delete poClipRect;

            return CE_None;
        }

        // If it does not intersect the line at all, just return.
        else if( !static_cast<OGRGeometry *>(poLines)->Intersects(poClipRect) )
        {
            delete poLines;
            delete poClipRect;

            return CE_None;
        }

        OGRGeometry *poClippedLines = poLines->Intersection(poClipRect);
        delete poLines;
        poLines = poClippedLines;
        delete poClipRect;
    }

/* -------------------------------------------------------------------- */
/*      Convert our polygon into GEOS format, and compute an            */
/*      envelope to accelerate later distance operations.               */
/* -------------------------------------------------------------------- */
    OGREnvelope sEnvelope;
    GEOSContextHandle_t hGEOSCtxt = OGRGeometry::createGEOSContext();
    GEOSGeom poGEOSPoly = poLines->exportToGEOS(hGEOSCtxt);
    OGR_G_GetEnvelope( hPolygon, &sEnvelope );

    delete poLines;

    // This check was already done in the calling
    // function and should never be true.

    // if( sEnvelope.MinY - dfBlendDist > nYOff+nYSize
    //     || sEnvelope.MaxY + dfBlendDist < nYOff
    //     || sEnvelope.MinX - dfBlendDist > nXOff+nXSize
    //     || sEnvelope.MaxX + dfBlendDist < nXOff )
    //     return CE_None;

    const int iXMin =
        std::max(0,
                 static_cast<int>(floor(sEnvelope.MinX - dfBlendDist - nXOff)));
    const int iXMax =
        std::min(nXSize,
                 static_cast<int>(ceil(sEnvelope.MaxX + dfBlendDist - nXOff)));
    const int iYMin =
        std::max(0,
                 static_cast<int>(floor(sEnvelope.MinY - dfBlendDist - nYOff)));
    const int iYMax =
        std::min(nYSize,
                 static_cast<int>(ceil(sEnvelope.MaxY + dfBlendDist - nYOff)));

/* -------------------------------------------------------------------- */
/*      Loop over potential area within blend line distance,            */
/*      processing each pixel.                                          */
/* -------------------------------------------------------------------- */
    for( int iY = 0; iY < nYSize; iY++ )
    {
        double dfLastDist = 0.0;

        for( int iX = 0; iX < nXSize; iX++ )
        {
            if( iX < iXMin || iX >= iXMax
                || iY < iYMin || iY > iYMax
                || dfLastDist > dfBlendDist + 1.5 )
            {
                if( pabyPolyMask[iX + iY * nXSize] == 0 )
                    pafValidityMask[iX + iY * nXSize] = 0.0;

                dfLastDist -= 1.0;
                continue;
            }

            CPLString osPointWKT;
            osPointWKT.Printf( "POINT(%d.5 %d.5)", iX + nXOff, iY + nYOff );

            GEOSGeom poGEOSPoint = GEOSGeomFromWKT_r( hGEOSCtxt, osPointWKT );

            double dfDist = 0.0;
            GEOSDistance_r( hGEOSCtxt, poGEOSPoly, poGEOSPoint, &dfDist );
            GEOSGeom_destroy_r( hGEOSCtxt, poGEOSPoint );

            dfLastDist = dfDist;

            if( dfDist > dfBlendDist )
            {
                if( pabyPolyMask[iX + iY * nXSize] == 0 )
                    pafValidityMask[iX + iY * nXSize] = 0.0;

                continue;
            }

            const double dfRatio =
                pabyPolyMask[iX + iY * nXSize] == 0
                ? 0.5 - (dfDist / dfBlendDist) * 0.5   // Outside.
                : 0.5 + (dfDist / dfBlendDist) * 0.5;  // Inside.

            pafValidityMask[iX + iY * nXSize] *= static_cast<float>(dfRatio);
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    GEOSGeom_destroy_r( hGEOSCtxt, poGEOSPoly );
    OGRGeometry::freeGEOSContext( hGEOSCtxt );

    return CE_None;

}
#endif  // HAVE_GEOS

/************************************************************************/
/*                         CutlineTransformer()                         */
/*                                                                      */
/*      A simple transformer for the cutline that just offsets          */
/*      relative to the current chunk.                                  */
/************************************************************************/

static int CutlineTransformer( void *pTransformArg,
                               int bDstToSrc,
                               int nPointCount,
                               double *x,
                               double *y,
                               double * /* z */,
                               int * /* panSuccess */ )
{
    int nXOff = static_cast<int *>(pTransformArg)[0];
    int nYOff = static_cast<int *>(pTransformArg)[1];

    if( bDstToSrc )
    {
        nXOff *= -1;
        nYOff *= -1;
    }

    for( int i = 0; i < nPointCount; i++ )
    {
        x[i] -= nXOff;
        y[i] -= nYOff;
    }

    return TRUE;
}

/************************************************************************/
/*                       GDALWarpCutlineMasker()                        */
/*                                                                      */
/*      This function will generate a source mask based on a            */
/*      provided cutline, and optional blend distance.                  */
/************************************************************************/

CPLErr
GDALWarpCutlineMasker( void *pMaskFuncArg,
                       int /* nBandCount */,
                       GDALDataType /* eType */,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       GByte ** /*ppImageData */,
                       int bMaskIsFloat, void *pValidityMask )

{
    if( nXSize < 1 || nYSize < 1 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( !bMaskIsFloat )
    {
        CPLAssert( false );
        return CE_Failure;
    }

    GDALWarpOptions *psWO = static_cast<GDALWarpOptions *>(pMaskFuncArg);

    if( psWO == NULL || psWO->hCutline == NULL )
    {
        CPLAssert( false );
        return CE_Failure;
    }

    GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
    if( hMemDriver == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALWarpCutlineMasker needs MEM driver");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Check the polygon.                                              */
/* -------------------------------------------------------------------- */
    OGRGeometryH hPolygon = static_cast<OGRGeometryH>(psWO->hCutline);

    if( wkbFlatten(OGR_G_GetGeometryType(hPolygon)) != wkbPolygon
        && wkbFlatten(OGR_G_GetGeometryType(hPolygon)) != wkbMultiPolygon )
    {
        CPLAssert( false );
        return CE_Failure;
    }

    OGREnvelope sEnvelope;
    OGR_G_GetEnvelope( hPolygon, &sEnvelope );

    float *pafMask = static_cast<float *>(pValidityMask);

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
    GByte *pabyPolyMask = static_cast<GByte *>(CPLCalloc(nXSize, nYSize));

    char szDataPointer[100] = {};

    // cppcheck-suppress redundantCopy
    snprintf( szDataPointer, sizeof(szDataPointer), "DATAPOINTER=" );
    CPLPrintPointer(
        szDataPointer+strlen(szDataPointer),
        pabyPolyMask,
        static_cast<int>(sizeof(szDataPointer) - strlen(szDataPointer)) );

    GDALDatasetH hMemDS = GDALCreate( hMemDriver, "warp_temp",
                                      nXSize, nYSize, 0, GDT_Byte, NULL );
    char *apszOptions[] = { szDataPointer, NULL };
    GDALAddBand( hMemDS, GDT_Byte, apszOptions );

    double adfGeoTransform[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    GDALSetGeoTransform( hMemDS, adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Burn the polygon into the mask with 1.0 values.                 */
/* -------------------------------------------------------------------- */
    int nTargetBand = 1;
    double dfBurnValue = 255.0;
    char **papszRasterizeOptions = NULL;

    if( CPLFetchBool( psWO->papszWarpOptions, "CUTLINE_ALL_TOUCHED", false ))
        papszRasterizeOptions =
            CSLSetNameValue( papszRasterizeOptions, "ALL_TOUCHED", "TRUE" );

    int anXYOff[2] = { nXOff, nYOff };

    CPLErr eErr =
        GDALRasterizeGeometries( hMemDS, 1, &nTargetBand,
                                 1, &hPolygon,
                                 CutlineTransformer, anXYOff,
                                 &dfBurnValue, papszRasterizeOptions,
                                 NULL, NULL );

    CSLDestroy( papszRasterizeOptions );

    // Close and ensure data flushed to underlying array.
    GDALClose( hMemDS );

/* -------------------------------------------------------------------- */
/*      In the case with no blend distance, we just apply this as a     */
/*      mask, zeroing out everything outside the polygon.               */
/* -------------------------------------------------------------------- */
    if( psWO->dfCutlineBlendDist == 0.0 )
    {
        for( int i = nXSize * nYSize - 1; i >= 0; i-- )
        {
            if( pabyPolyMask[i] == 0 )
                static_cast<float *>(pValidityMask)[i] = 0.0;
        }
    }
    else
    {
        eErr = BlendMaskGenerator( nXOff, nYOff, nXSize, nYSize,
                                   pabyPolyMask,
                                   static_cast<float *>(pValidityMask),
                                   hPolygon, psWO->dfCutlineBlendDist );
    }

/* -------------------------------------------------------------------- */
/*      Clean up.                                                       */
/* -------------------------------------------------------------------- */
    CPLFree( pabyPolyMask );

    return eErr;
}

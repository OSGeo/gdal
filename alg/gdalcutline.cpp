/******************************************************************************
 * $Id$
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
#include "ogr_geos.h"
#include "ogr_geometry.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         BlendMaskGenerator()                         */
/************************************************************************/

static CPLErr
BlendMaskGenerator( int nXOff, int nYOff, int nXSize, int nYSize, 
                    GByte *pabyPolyMask, float *pafValidityMask,
                    OGRGeometryH hPolygon, double dfBlendDist )

{
#ifndef HAVE_GEOS 
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Blend distance support not available without the GEOS library.");
    return CE_Failure;

#else /* HAVE_GEOS */

/* -------------------------------------------------------------------- */
/*      Convert the polygon into a collection of lines so that we       */
/*      measure distance from the edge even on the inside.              */
/* -------------------------------------------------------------------- */
    OGRGeometry *poLines
        = OGRGeometryFactory::forceToMultiLineString( 
            ((OGRGeometry *) hPolygon)->clone() );

/* -------------------------------------------------------------------- */
/*      Prepare a clipping polygon a bit bigger than the area of        */
/*      interest in the hopes of simplifying the cutline down to        */
/*      stuff that will be relavent for this area of interest.          */
/* -------------------------------------------------------------------- */
    CPLString osClipRectWKT;

    osClipRectWKT.Printf( "POLYGON((%g %g,%g %g,%g %g,%g %g,%g %g))", 
                          nXOff - (dfBlendDist+1), 
                          nYOff - (dfBlendDist+1), 
                          nXOff + nXSize + (dfBlendDist+1), 
                          nYOff - (dfBlendDist+1), 
                          nXOff + nXSize + (dfBlendDist+1), 
                          nYOff + nYSize + (dfBlendDist+1), 
                          nXOff - (dfBlendDist+1), 
                          nYOff + nYSize + (dfBlendDist+1), 
                          nXOff - (dfBlendDist+1), 
                          nYOff - (dfBlendDist+1) );
    
    OGRPolygon *poClipRect = NULL;
    char *pszWKT = (char *) osClipRectWKT.c_str();
    
    OGRGeometryFactory::createFromWkt( &pszWKT, NULL, 
                                       (OGRGeometry**) (&poClipRect) );

    if( poClipRect )
    {
        OGRGeometry *poClippedLines = 
            poLines->Intersection( poClipRect );
        delete poLines;
        poLines = poClippedLines;
        delete poClipRect;
    }

/* -------------------------------------------------------------------- */
/*      Convert our polygon into GEOS format, and compute an            */
/*      envelope to accelerate later distance operations.               */
/* -------------------------------------------------------------------- */
    OGREnvelope sEnvelope;
    int iXMin, iYMin, iXMax, iYMax;
    GEOSGeom poGEOSPoly;

    poGEOSPoly = poLines->exportToGEOS();
    OGR_G_GetEnvelope( hPolygon, &sEnvelope );

    delete poLines;

    if( sEnvelope.MinY - dfBlendDist > nYOff+nYSize 
        || sEnvelope.MaxY + dfBlendDist < nYOff 
        || sEnvelope.MinX - dfBlendDist > nXOff+nXSize
        || sEnvelope.MaxX + dfBlendDist < nXOff )
        return CE_None;
    
    iXMin = MAX(0,(int) floor(sEnvelope.MinX - dfBlendDist - nXOff));
    iXMax = MIN(nXSize, (int) ceil(sEnvelope.MaxX + dfBlendDist - nXOff));
    iYMin = MAX(0,(int) floor(sEnvelope.MinY - dfBlendDist - nYOff));
    iYMax = MIN(nYSize, (int) ceil(sEnvelope.MaxY + dfBlendDist - nYOff));

/* -------------------------------------------------------------------- */
/*      Loop over potential area within blend line distance,            */
/*      processing each pixel.                                          */
/* -------------------------------------------------------------------- */
    int iY, iX;
    double dfLastDist;
    
    for( iY = 0; iY < nYSize; iY++ )
    {
        dfLastDist = 0.0;

        for( iX = 0; iX < nXSize; iX++ )
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
            
            double dfDist, dfRatio;
            CPLString osPointWKT;
            GEOSGeom poGEOSPoint;

            osPointWKT.Printf( "POINT(%d.5 %d.5)", iX + nXOff, iY + nYOff );
            poGEOSPoint = GEOSGeomFromWKT( osPointWKT );

            GEOSDistance( poGEOSPoly, poGEOSPoint, &dfDist );
            GEOSGeom_destroy( poGEOSPoint );

            dfLastDist = dfDist;

            if( dfDist > dfBlendDist )
            {
                if( pabyPolyMask[iX + iY * nXSize] == 0 )
                    pafValidityMask[iX + iY * nXSize] = 0.0;

                continue;
            }

            if( pabyPolyMask[iX + iY * nXSize] == 0 )
            {
                /* outside */
                dfRatio = 0.5 - (dfDist / dfBlendDist) * 0.5;
            }
            else 
            {
                /* inside */
                dfRatio = 0.5 + (dfDist / dfBlendDist) * 0.5;
            }                

            pafValidityMask[iX + iY * nXSize] *= (float)dfRatio;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    GEOSGeom_destroy( poGEOSPoly );

    return CE_None;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                         CutlineTransformer()                         */
/*                                                                      */
/*      A simple transformer for the cutline that just offsets          */
/*      relative to the current chunk.                                  */
/************************************************************************/

static int CutlineTransformer( void *pTransformArg, int bDstToSrc, 
                               int nPointCount, 
                               double *x, double *y, double *z, 
                               int *panSuccess )

{
    int nXOff = ((int *) pTransformArg)[0];
    int nYOff = ((int *) pTransformArg)[1];				
    int i;

    if( bDstToSrc )
    {
        nXOff *= -1;
        nYOff *= -1;
    }

    for( i = 0; i < nPointCount; i++ )
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
GDALWarpCutlineMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       GByte ** /*ppImageData */,
                       int bMaskIsFloat, void *pValidityMask )

{
    GDALWarpOptions *psWO = (GDALWarpOptions *) pMaskFuncArg;
    float *pafMask = (float *) pValidityMask;
    CPLErr eErr;
    GDALDriverH hMemDriver;

    if( nXSize < 1 || nYSize < 1 )
        return CE_None;

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

    hMemDriver = GDALGetDriverByName("MEM");
    if (hMemDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALWarpCutlineMasker needs MEM driver");
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
    double adfGeoTransform[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

    char szDataPointer[100];
    char *apszOptions[] = { szDataPointer, NULL };

    memset( szDataPointer, 0, sizeof(szDataPointer) );
    sprintf( szDataPointer, "DATAPOINTER=" );
    CPLPrintPointer( szDataPointer+strlen(szDataPointer), 
                    pabyPolyMask, 
                     sizeof(szDataPointer) - strlen(szDataPointer) );

    hMemDS = GDALCreate( hMemDriver, "warp_temp", 
                         nXSize, nYSize, 0, GDT_Byte, NULL );
    GDALAddBand( hMemDS, GDT_Byte, apszOptions );
    GDALSetGeoTransform( hMemDS, adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Burn the polygon into the mask with 1.0 values.                 */
/* -------------------------------------------------------------------- */
    int nTargetBand = 1;
    double dfBurnValue = 255.0;
    int    anXYOff[2];
    char   **papszRasterizeOptions = NULL;
    

    if( CSLFetchBoolean( psWO->papszWarpOptions, "CUTLINE_ALL_TOUCHED", FALSE ))
        papszRasterizeOptions = 
            CSLSetNameValue( papszRasterizeOptions, "ALL_TOUCHED", "TRUE" );

    anXYOff[0] = nXOff;
    anXYOff[1] = nYOff;

    eErr = 
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
        int i;

        for( i = nXSize * nYSize - 1; i >= 0; i-- )
        {
            if( pabyPolyMask[i] == 0 )
                ((float *) pValidityMask)[i] = 0.0;
        }
    }
    else
    {
        eErr = BlendMaskGenerator( nXOff, nYOff, nXSize, nYSize, 
                                   pabyPolyMask, (float *) pValidityMask,
                                   hPolygon, psWO->dfCutlineBlendDist );
    }

/* -------------------------------------------------------------------- */
/*      Clean up.                                                       */
/* -------------------------------------------------------------------- */
    CPLFree( pabyPolyMask );

    return eErr;
}


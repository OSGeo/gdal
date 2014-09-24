/******************************************************************************
 * $Id$
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Implemention of the GDALTransformer wrapper around CRS.C functions
 *           to build a polynomial transformation based on ground control 
 *           points.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ***************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
#include "cpl_minixml.h"

extern int TwoDPolyFit( double *, double *, int, int, double *, double *, double * );
extern double TwoDPolyEval( double *, int, double, double );

typedef struct
{
    double adfToGeoX[20];
    double adfToGeoY[20];
    
    double adfFromGeoX[20];
    double adfFromGeoY[20];

    int    nOrder;
    int    bReversed;
    
} GCPTransformInfo;


/************************************************************************/
/*                      GDALCreateGCPTransformer()                      */
/************************************************************************/

/**
 * Create GCP based polynomial transformer.
 *
 * Computes least squares fit polynomials from a provided set of GCPs,
 * and stores the coefficients for later transformation of points between
 * pixel/line and georeferenced coordinates. 
 *
 * The return value should be used as a TransformArg in combination with
 * the transformation function GDALGCPTransform which fits the 
 * GDALTransformerFunc signature.  The returned transform argument should
 * be deallocated with GDALDestroyGCPTransformer when no longer needed.
 *
 * This function may fail (returning NULL) if the provided set of GCPs
 * are inadequate for the requested order, the determinate is zero or they
 * are otherwise "ill conditioned".  
 *
 * Note that 2nd order requires at least 6 GCPs, and 3rd order requires at
 * least 10 gcps.  If nReqOrder is 0 the highest order possible with the
 * provided gcp count will be used.
 *
 * @param nGCPCount the number of GCPs in pasGCPList.
 * @param pasGCPList an array of GCPs to be used as input.
 * @param nReqOrder the requested polynomial order.  It should be 1, 2 or 3.
 * @param bReversed set it to TRUE to compute the reversed transformation.
 * 
 * @return the transform argument or NULL if creation fails. 
 */

void *GDALCreateGCPTransformer( int nGCPCount, const GDAL_GCP *pasGCPList, 
                                int nReqOrder, int bReversed )

{
    GCPTransformInfo *psInfo;
    double *padfGeoX, *padfGeoY, *padfRasterX, *padfRasterY;
    int    *panStatus, iGCP;
    double rms_err;

    if( nReqOrder == 0 )
    {
        if( nGCPCount >= 10 )
            nReqOrder = 3;
        else if( nGCPCount >= 6 )
            nReqOrder = 2;
        else
            nReqOrder = 1;
    }
    
    psInfo = (GCPTransformInfo *) CPLCalloc(sizeof(GCPTransformInfo),1);
    psInfo->bReversed = bReversed;
    psInfo->nOrder = nReqOrder;

/* -------------------------------------------------------------------- */
/*      Allocate and initialize the working points list.                */
/* -------------------------------------------------------------------- */
    padfGeoX = (double *) CPLCalloc(sizeof(double),nGCPCount);
    padfGeoY = (double *) CPLCalloc(sizeof(double),nGCPCount);
    padfRasterX = (double *) CPLCalloc(sizeof(double),nGCPCount);
    padfRasterY = (double *) CPLCalloc(sizeof(double),nGCPCount);
    panStatus = (int *) CPLCalloc(sizeof(int),nGCPCount);
    
    for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        panStatus[iGCP] = 1;
        padfGeoX[iGCP] = pasGCPList[iGCP].dfGCPX;
        padfGeoY[iGCP] = pasGCPList[iGCP].dfGCPY;
        padfRasterX[iGCP] = pasGCPList[iGCP].dfGCPPixel;
        padfRasterY[iGCP] = pasGCPList[iGCP].dfGCPLine;
    }

/* -------------------------------------------------------------------- */
/*      Compute the forward and reverse polynomials.                    */
/* -------------------------------------------------------------------- */
    if ( TwoDPolyFit( &rms_err, psInfo->adfFromGeoX, nReqOrder, nGCPCount,
		      padfRasterX, padfGeoX, padfGeoY ) < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to compute polynomial equations of desired order\n"
                  "for provided control points." );
        goto CleanupAfterError;
    }
    if ( TwoDPolyFit( &rms_err, psInfo->adfFromGeoY, nReqOrder, nGCPCount,
		      padfRasterY, padfGeoX, padfGeoY ) < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to compute polynomial equations of desired order\n"
                  "for provided control points." );
        goto CleanupAfterError;
    }
    if ( TwoDPolyFit( &rms_err, psInfo->adfToGeoX, nReqOrder, nGCPCount,
		      padfGeoX, padfRasterX, padfRasterY ) < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to compute polynomial equations of desired order\n"
                  "for provided control points." );
        goto CleanupAfterError;
    }
    if ( TwoDPolyFit( &rms_err, psInfo->adfToGeoY, nReqOrder, nGCPCount,
		      padfGeoY, padfRasterX, padfRasterY ) < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to compute polynomial equations of desired order\n"
                  "for provided control points." );
        goto CleanupAfterError;
    }

/* -------------------------------------------------------------------- */
/*      Dump residuals.                                                 */
/* -------------------------------------------------------------------- */
    CPLDebug( "GDALCreateGCPTransformer",
	      "Number of GCPs %d, transformation order %d",
	      nGCPCount, psInfo->nOrder );
    
    for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
	double x = pasGCPList[iGCP].dfGCPX;
	double y = pasGCPList[iGCP].dfGCPY;
	double z = pasGCPList[iGCP].dfGCPZ;
	int bSuccess;
	GDALGCPTransform( psInfo, TRUE, 1, &x, &y, &z, &bSuccess );
	CPLDebug( "GDALCreateGCPTransformer",
		  "GCP %d. Residuals: X: %f, Y: %f", iGCP,
		  pasGCPList[iGCP].dfGCPPixel - x, pasGCPList[iGCP].dfGCPLine - y );
    }
    
    return psInfo;

  CleanupAfterError:
    CPLFree( padfGeoX );
    CPLFree( padfGeoY );
    CPLFree( padfRasterX );
    CPLFree( padfRasterX );
    CPLFree( panStatus );
    
    CPLFree( psInfo );
    return NULL;
}

/************************************************************************/
/*                     GDALDestroyGCPTransformer()                      */
/************************************************************************/

/**
 * Destroy GCP transformer.
 *
 * This function is used to destroy information about a GCP based
 * polynomial transformation created with GDALCreateGCPTransformer(). 
 *
 * @param pTransformArg the transform arg previously returned by 
 * GDALCreateGCPTransformer(). 
 */

void GDALDestroyGCPTransformer( void *pTransformArg )

{
    CPLFree( pTransformArg );
}

/************************************************************************/
/*                          GDALGCPTransform()                          */
/************************************************************************/

/**
 * Transforms point based on GCP derived polynomial model.
 *
 * This function matches the GDALTransformerFunc signature, and can be
 * used to transform one or more points from pixel/line coordinates to
 * georeferenced coordinates (SrcToDst) or vice versa (DstToSrc). 
 *
 * @param pTransformArg return value from GDALCreateGCPTransformer(). 
 * @param bDstToSrc TRUE if transformation is from the destination 
 * (georeferenced) coordinates to pixel/line or FALSE when transforming
 * from pixel/line to georeferenced coordinates.
 * @param nPointCount the number of values in the x, y and z arrays.
 * @param x array containing the X values to be transformed.
 * @param y array containing the Y values to be transformed.
 * @param z array containing the Z values to be transformed.
 * @param panSuccess array in which a flag indicating success (TRUE) or
 * failure (FALSE) of the transformation are placed.
 *
 * @return TRUE.
 */

int GDALGCPTransform( void *pTransformArg, int bDstToSrc, 
                      int nPointCount, 
                      double *x, double *y, double *z, 
                      int *panSuccess )

{
    int    i;
    double X, Y;
    GCPTransformInfo *psInfo = (GCPTransformInfo *) pTransformArg;

    if( psInfo->bReversed )
        bDstToSrc = !bDstToSrc;
    
    for( i = 0; i < nPointCount; i++ )
    {
	X = x[i];
	Y = y[i];
        if( bDstToSrc )
        {
            x[i] = TwoDPolyEval( psInfo->adfFromGeoX, psInfo->nOrder, X, Y );
	    y[i] = TwoDPolyEval( psInfo->adfFromGeoY, psInfo->nOrder, X, Y );
        }
        else
        {
            x[i] = TwoDPolyEval( psInfo->adfToGeoX, psInfo->nOrder, X, Y );
	    y[i] = TwoDPolyEval( psInfo->adfToGeoY, psInfo->nOrder, X, Y );
        }
	z[i] = 0;
        panSuccess[i] = TRUE;
    }

    return TRUE;
}

CPLXMLNode *GDALSerializeGCPTransformer( void *pTransformArg )

{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "serialization not supported for this type of gcp transformer.");
    return NULL;
}

void *GDALDeserializeGCPTransformer( CPLXMLNode *psTree )

{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "deserialization not supported for this type of gcp transformer.");
    return NULL;
}

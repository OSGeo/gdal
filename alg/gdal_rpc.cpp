/******************************************************************************
 * $Id$
 *
 * Project:  Image Warper
 * Purpose:  Implements a rational polynomail (RPC) based transformer. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2003/06/03 17:36:43  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*			     GDALRPCTransformer                         */
/* ==================================================================== */
/************************************************************************/

typedef struct {

    double      LINE_OFF;
    double      SAMP_OFF;
    double      LAT_OFF;
    double      LONG_OFF;
    double      HEIGHT_OFF;

    double      LINE_SCALE;
    double      SAMP_SCALE;
    double      LAT_SCALE;
    double      LONG_SCALE;
    double      HEIGHT_SCALE;

    double      LINE_NUM_COEFF[20];
    double      LINE_DEN_COEFF[20];
    double      SAMP_NUM_COEFF[20];
    double      SAMP_DEN_COEFF[20];

    int         bReversed;

} GDALRPCTransformInfo;

/************************************************************************/
/*                      GDALCreateRPCTransformer()                      */
/************************************************************************/

void *GDALCreateRPCTransformer( char **papszRPCMetadata, int bReversed )

{
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

    return psInfo;
}

/************************************************************************/
/*                 GDALDestroyReprojectionTransformer()                 */
/************************************************************************/

void GDALDestroyRPCTransformer( void *pTransformAlg )

{
    GDALReprojectionTransformInfo *psInfo = 
        (GDALReprojectionTransformInfo *) pTransformAlg;		

    if( psInfo->poForwardTransform )
        delete psInfo->poForwardTransform;

    if( psInfo->poReverseTransform )
    delete psInfo->poReverseTransform;

    CPLFree( psInfo );
}

/************************************************************************/
/*                          RPCComputeTerms()                           */
/************************************************************************/

static void RPCComputeTerms( double dfLong, double dfLat, double dfHeight,
                             double *padfTerms )

{
    padfTerms[0] = 1.0;
    padfTerms[1] = dfLong;
    padfTerms[2] = dfLat;
    padfTerms[3] = dfHeight;
    padfTerms[4] = dfLong * dfLat;
    padfTerms[5] = dfLong * dfHeight;
    padfTerms[6] = dfLat * dfHeight;
    padfTerms[7] = dfLong * dfLong;
    padfTerms[8] = dfLat * dfLat;
    padfTerms[9] = dfHeight * dfHeight;

    padfTerms[10] = dfLong * dfLat * dfHeight;
    padfTerms[11] = dfLong * dfLong * dfLong;
    padfTerms[12] = dfLong * dfLat * dfLat;
    padfTerms[13] = dfLong * dfHeight * dfHeight;
    padfTerms[14] = dfLong * dfLong * dfLat;
    padfTerms[15] = dfLat * dfLat * dfLat;
    padfTerms[16] = dfLat * dfHeight * dfHeight;
    padfTerms[17] = dfLong * dfLong * dfHeight;
    padfTerms[18] = dfLat * dfLat * dfHeight;
    padfTerms[19] = dfHeight * dfHeight * dfHeight;
}

/************************************************************************/
/*                            RPCEvaluate()                             */
/************************************************************************/

static double RPCEvaluate( double *padfTerms, *padfCoefs )

{
    double dfSum = 0.0;
    int i;

    for( i = 0; i < 20; i++ )
        dfSum += padfTerms[i] * padfCoefs[i];

    return dfSum;
}


/************************************************************************/
/*                          GDALRPCTransform()                          */
/************************************************************************/

int GDALRPCTransform( void *pTransformArg, int bDstToSrc, 
                      int nPointCount, 
                      double *padfX, double *padfY, double *padfZ,
                      int *panSuccess )

{
    GDALRPCTransformInfo *psRPC = (GDALRPCTransformInfo *) pTransformArg;
    int i;
    double adfTerms[20];

    if( psRPC->bReverse )
        bDstToSrc = !bDstToSrc;

/* -------------------------------------------------------------------- */
/*      The simple case is transforming from lat/long to pixel/line.    */
/*      Just apply the equations directly.                              */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            double dfResultX, dfResultY;

            RPCComputeTerms( (padfX[i]-psRPC->LONG_OFF) / psRPC->LONG_SCALE, 
                             (padfY[i]-psRPC->LAT_OFF) / psRPC->LAT_SCALE, 
                             (padfZ[i]-psRPC->HEIGHT_OFF)/psRPC->HEIGHT_SCALE,
                             adfTerms );

            dfResultX = RPCEvaluate( adfTerms, psRPC->SAMP_NUM_COEFFS )
                / RPCEvaluate( adfTerms, psRPC->SAMP_DEN_COEFFS );
            
            dfResultY = RPCEvaluate( adfTerms, psRPC->LINE_NUM_COEFFS )
                / RPCEvaluate( adfTerms, psRPC->LINE_DEN_COEFFS );
            
            padfX[i] = dfResultX * psRPC->SAMP_SCALE + psRPC->SAMP_OFF;
            padfY[i] = dfResultY * psRPC->LINE_SCALE + psRPC->LINE_OFF;
            panSuccess[i] = TRUE;
        }

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      The more complicated issue is how to reverse this.  For now     */
/*      we use a dead simple linear approximation.  In the case of      */
/*      image warping, this direction is only used to pick bound for    */
/*      the newly created output file anyways.                          */
/* -------------------------------------------------------------------- */
    

    return bSuccess;
}

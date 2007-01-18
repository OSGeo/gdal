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
 ****************************************************************************/

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

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

static double RPCEvaluate( double *padfTerms, double *padfCoefs )

{
    double dfSum = 0.0;
    int i;

    for( i = 0; i < 20; i++ )
        dfSum += padfTerms[i] * padfCoefs[i];

    return dfSum;
}

/************************************************************************/
/*                         RPCTransformPoint()                          */
/************************************************************************/

static void RPCTransformPoint( GDALRPCInfo *psRPC, 
                               double dfLong, double dfLat, double dfHeight, 
                               double *pdfPixel, double *pdfLine )

{
    double dfResultX, dfResultY;
    double adfTerms[20];
   
    RPCComputeTerms( 
        (dfLong   - psRPC->dfLONG_OFF) / psRPC->dfLONG_SCALE, 
        (dfLat    - psRPC->dfLAT_OFF) / psRPC->dfLAT_SCALE, 
        (dfHeight - psRPC->dfHEIGHT_OFF) / psRPC->dfHEIGHT_SCALE,
        adfTerms );
    
    dfResultX = RPCEvaluate( adfTerms, psRPC->adfSAMP_NUM_COEFF )
        / RPCEvaluate( adfTerms, psRPC->adfSAMP_DEN_COEFF );
    
    dfResultY = RPCEvaluate( adfTerms, psRPC->adfLINE_NUM_COEFF )
        / RPCEvaluate( adfTerms, psRPC->adfLINE_DEN_COEFF );
    
    *pdfPixel = dfResultX * psRPC->dfSAMP_SCALE + psRPC->dfSAMP_OFF;
    *pdfLine = dfResultY * psRPC->dfLINE_SCALE + psRPC->dfLINE_OFF;
}

/************************************************************************/
/* ==================================================================== */
/*			     GDALRPCTransformer                         */
/* ==================================================================== */
/************************************************************************/

typedef struct {

    GDALTransformerInfo sTI;

    GDALRPCInfo sRPC;

    double      adfPLToLatLongGeoTransform[6];

    int         bReversed;

    double      dfPixErrThreshold;

} GDALRPCTransformInfo;

/************************************************************************/
/*                      GDALCreateRPCTransformer()                      */
/************************************************************************/

void *GDALCreateRPCTransformer( GDALRPCInfo *psRPCInfo, int bReversed, 
                                double dfPixErrThreshold )

{
    GDALRPCTransformInfo *psTransform;

/* -------------------------------------------------------------------- */
/*      Initialize core info.                                           */
/* -------------------------------------------------------------------- */
    psTransform = (GDALRPCTransformInfo *) 
        CPLCalloc(sizeof(GDALRPCTransformInfo),1);

    memcpy( &(psTransform->sRPC), psRPCInfo, sizeof(GDALRPCInfo) );
    psTransform->bReversed = bReversed;
    psTransform->dfPixErrThreshold = dfPixErrThreshold;

    strcpy( psTransform->sTI.szSignature, "GTI" );
    psTransform->sTI.pszClassName = "GDALRPCTransformer";
    psTransform->sTI.pfnTransform = GDALRPCTransform;
    psTransform->sTI.pfnCleanup = GDALDestroyRPCTransformer;
    psTransform->sTI.pfnSerialize = NULL;

/* -------------------------------------------------------------------- */
/*      Establish a reference point for calcualating an affine          */
/*      geotransform approximate transformation.                        */
/* -------------------------------------------------------------------- */
    double adfGTFromLL[6], dfRefPixel, dfRefLine;

    double dfRefLong = (psRPCInfo->dfMIN_LONG + psRPCInfo->dfMAX_LONG) * 0.5;
    double dfRefLat  = (psRPCInfo->dfMIN_LAT  + psRPCInfo->dfMAX_LAT ) * 0.5;

    RPCTransformPoint( psRPCInfo, dfRefLong, dfRefLat, 0.0, 
                       &dfRefPixel, &dfRefLine );

/* -------------------------------------------------------------------- */
/*      Transform nearby locations to establish affine direction        */
/*      vectors.                                                        */
/* -------------------------------------------------------------------- */
    double dfRefPixelDelta, dfRefLineDelta, dfLLDelta = 0.0001;
    
    RPCTransformPoint( psRPCInfo, dfRefLong+dfLLDelta, dfRefLat, 0.0, 
                       &dfRefPixelDelta, &dfRefLineDelta );
    adfGTFromLL[1] = (dfRefPixelDelta - dfRefPixel) / dfLLDelta;
    adfGTFromLL[2] = (dfRefLineDelta - dfRefLine) / dfLLDelta;
    
    RPCTransformPoint( psRPCInfo, dfRefLong, dfRefLat+dfLLDelta, 0.0, 
                       &dfRefPixelDelta, &dfRefLineDelta );
    adfGTFromLL[4] = (dfRefPixelDelta - dfRefPixel) / dfLLDelta;
    adfGTFromLL[5] = (dfRefLineDelta - dfRefLine) / dfLLDelta;

    adfGTFromLL[0] = dfRefPixel 
        - adfGTFromLL[1] * dfRefLong - adfGTFromLL[2] * dfRefLat;
    adfGTFromLL[3] = dfRefLine 
        - adfGTFromLL[4] * dfRefLong - adfGTFromLL[5] * dfRefLat;

    GDALInvGeoTransform( adfGTFromLL, psTransform->adfPLToLatLongGeoTransform);
    
    return psTransform;
}

/************************************************************************/
/*                 GDALDestroyReprojectionTransformer()                 */
/************************************************************************/

void GDALDestroyRPCTransformer( void *pTransformAlg )

{
    CPLFree( pTransformAlg );
}

/************************************************************************/
/*                          GDALRPCTransform()                          */
/************************************************************************/

int GDALRPCTransform( void *pTransformArg, int bDstToSrc, 
                      int nPointCount, 
                      double *padfX, double *padfY, double *padfZ,
                      int *panSuccess )

{
    GDALRPCTransformInfo *psTransform = (GDALRPCTransformInfo *) pTransformArg;
    GDALRPCInfo *psRPC = &(psTransform->sRPC);
    int i;

    if( psTransform->bReversed )
        bDstToSrc = !bDstToSrc;

/* -------------------------------------------------------------------- */
/*      The simple case is transforming from lat/long to pixel/line.    */
/*      Just apply the equations directly.                              */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
    {
        for( i = 0; i < nPointCount; i++ )
        {
            RPCTransformPoint( psRPC, padfX[i], padfY[i], padfZ[i], 
                               padfX + i, padfY + i );
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
    for( i = 0; i < nPointCount; i++ )
    {
        double dfResultX, dfResultY;

        dfResultX = psTransform->adfPLToLatLongGeoTransform[0]
            + psTransform->adfPLToLatLongGeoTransform[1] * padfX[i]
            + psTransform->adfPLToLatLongGeoTransform[2] * padfY[i];

        dfResultY = psTransform->adfPLToLatLongGeoTransform[3]
            + psTransform->adfPLToLatLongGeoTransform[4] * padfX[i]
            + psTransform->adfPLToLatLongGeoTransform[5] * padfY[i];

        padfX[i] = dfResultX;
        padfY[i] = dfResultY;

        panSuccess[i] = TRUE;
    }

    return TRUE;
}

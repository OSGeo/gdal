/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  GCP / RPC Georeferencing Model (custom by/for ESRI)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, ESRI 
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
#include "nitflib.h"

CPL_CVSID("$Id");

/* Unused in normal builds. Caller code in nitfdataset.cpp is protected by #ifdef ESRI_BUILD */
#ifdef ESRI_BUILD

/************************************************************************/
/*                               Apply()                                */
/************************************************************************/
static double Apply( double *C, double P, double L, double H )
{
    // Polynomial equation for RPC00B.

    double H2 = H * H;
    double L2 = L * L;
    double P2 = P * P;

    return  C[0]
        + C[1]*L     + C[2]*P     + C[3]*H     + C[4]*L*P   + C[5]*L*H  
        + C[6]*P*H   + C[7]*L2    + C[8]*P2    + C[9]*H2    + C[10]*P*L*H
        + C[11]*L*L2 + C[12]*L*P2 + C[13]*L*H2 + C[14]*L2*P + C[15]*P*P2
        + C[16]*P*H2 + C[17]*L2*H + C[18]*P2*H + C[19]*H*H2;
} 

/************************************************************************/
/*                          NITFDensifyGCPs()                           */
/************************************************************************/
void NITFDensifyGCPs( GDAL_GCP **psGCPs, int *pnGCPCount )
{
    // Given the four corner points of an extent (UL, UR, LR, LL), this method
    // will add three points to each line segment and return a total of 16 points
    // including the four original corner points.

    if ( (*pnGCPCount != 4) || (psGCPs == NULL) ) return;

    const int  nDensifiedGCPs  = 16;
    GDAL_GCP  *psDensifiedGCPs = (GDAL_GCP*) CPLMalloc(sizeof(GDAL_GCP)*nDensifiedGCPs);

    GDALInitGCPs( nDensifiedGCPs, psDensifiedGCPs );

    bool   ok          = true;
    double xLeftPt     = 0.0;
    double xMidPt      = 0.0;
    double xRightPt    = 0.0;
    double yLeftPt     = 0.0;
    double yMidPt      = 0.0;
    double yRightPt    = 0.0;
    int    count       = 0;
    int    idx         = 0;

    for( int ii = 0; ( (ii < *pnGCPCount) && (ok) ) ; ++ii )
    {
        idx = ( ii != 3 ) ? ii+1 : 0;

        try
        {
            psDensifiedGCPs[count].dfGCPX = (*psGCPs)[ii].dfGCPX;
            psDensifiedGCPs[count].dfGCPY = (*psGCPs)[ii].dfGCPY;

            xMidPt = ((*psGCPs)[ii].dfGCPX+(*psGCPs)[idx].dfGCPX) * 0.5;
            yMidPt = ((*psGCPs)[ii].dfGCPY+(*psGCPs)[idx].dfGCPY) * 0.5;

            xLeftPt = ((*psGCPs)[ii].dfGCPX+xMidPt) * 0.5;
            yLeftPt = ((*psGCPs)[ii].dfGCPY+yMidPt) * 0.5;

            xRightPt = (xMidPt+(*psGCPs)[idx].dfGCPX) * 0.5;
            yRightPt = (yMidPt+(*psGCPs)[idx].dfGCPY) * 0.5;

            psDensifiedGCPs[count+1].dfGCPX = xLeftPt;
            psDensifiedGCPs[count+1].dfGCPY = yLeftPt;
            psDensifiedGCPs[count+2].dfGCPX = xMidPt;
            psDensifiedGCPs[count+2].dfGCPY = yMidPt;
            psDensifiedGCPs[count+3].dfGCPX = xRightPt;
            psDensifiedGCPs[count+3].dfGCPY = yRightPt;

            count += *pnGCPCount;

        }
        catch (...)
        {
            ok = false;
        }
    }

    if( !ok )
    {
        GDALDeinitGCPs( nDensifiedGCPs, psDensifiedGCPs );
        CPLFree( psDensifiedGCPs );
        psDensifiedGCPs = NULL;

        return;
    }

    GDALDeinitGCPs( *pnGCPCount, *psGCPs );
    CPLFree( *psGCPs );

    *psGCPs         = psDensifiedGCPs;
    *pnGCPCount     = nDensifiedGCPs;
    psDensifiedGCPs = NULL;
}

/************************************************************************/
/*                            RPCTransform()                            */
/************************************************************************/

static bool RPCTransform( NITFRPC00BInfo *psRPCInfo, 
                          double         *pGCPXCoord,
                          double         *pGCPYCoord,
                          int             nGCPCount )
{
    if( (psRPCInfo == NULL) || (pGCPXCoord == NULL) ||
        (pGCPYCoord == NULL) || (nGCPCount <= 0) ) return (false);

    bool   ok = true;
    double H  = 0.0;
    double L  = 0.0;
    double P  = 0.0;
    double u  = 0.0;
    double v  = 0.0;
    double z  = psRPCInfo->HEIGHT_OFF;

    double heightScaleInv = 1.0/psRPCInfo->HEIGHT_SCALE;
    double latScaleInv    = 1.0/psRPCInfo->LAT_SCALE;
    double longScaleInv   = 1.0/psRPCInfo->LONG_SCALE;

    for( int ii = 0; ( (ii < nGCPCount) && (ok) ); ++ii )
    {
        try
        {
            P = ( pGCPYCoord[ii] - psRPCInfo->LAT_OFF )    * latScaleInv;
            L = ( pGCPXCoord[ii] - psRPCInfo->LONG_OFF)    * longScaleInv;
            H = ( z              - psRPCInfo->HEIGHT_OFF ) * heightScaleInv;

            u = Apply( psRPCInfo->SAMP_NUM_COEFF, P, L, H )/Apply( psRPCInfo->SAMP_DEN_COEFF, P, L, H );
            v = Apply( psRPCInfo->LINE_NUM_COEFF, P, L, H )/Apply( psRPCInfo->LINE_DEN_COEFF, P, L, H );

            pGCPXCoord[ii] = u*psRPCInfo->SAMP_SCALE + psRPCInfo->SAMP_OFF;
            pGCPYCoord[ii] = v*psRPCInfo->LINE_SCALE + psRPCInfo->LINE_OFF;
        }
        catch (...)
        {
            ok = false;
        }
    }

    return (ok);
}

/************************************************************************/
/*                       NITFUpdateGCPsWithRPC()                        */
/************************************************************************/

void NITFUpdateGCPsWithRPC( NITFRPC00BInfo *psRPCInfo,
                            GDAL_GCP       *psGCPs,
                            int            *pnGCPCount )
{
    if( (psRPCInfo == NULL) || (!psRPCInfo->SUCCESS) ||
        (psGCPs == NULL) || (*pnGCPCount < 4) ) return;

    double *pGCPXCoord = NULL;
    double *pGCPYCoord = NULL;

    try
    {
        pGCPXCoord = new double[*pnGCPCount];
        pGCPYCoord = new double[*pnGCPCount];
    }
    catch (...)
    {
        if( pGCPXCoord != NULL )
        {
            delete [] (pGCPXCoord);
            pGCPXCoord = NULL;
        }

        if( pGCPYCoord != NULL )
        {
            delete [] (pGCPYCoord);
            pGCPYCoord = NULL;
        }
    }

    if( (pGCPXCoord == NULL) || (pGCPYCoord == NULL) ) return;

    bool ok = true;

    for( int ii = 0; ( (ii < *pnGCPCount) && (ok) ); ++ii )
    {
        try
        {
            pGCPXCoord[ii] = psGCPs[ii].dfGCPX;
            pGCPYCoord[ii] = psGCPs[ii].dfGCPY;
        }
        catch (...)
        {
            ok = false;
        }
    }

    if( (ok) && (RPCTransform( psRPCInfo, pGCPXCoord, pGCPYCoord, *pnGCPCount )) )
    {
        // Replace the image coordinates of the input GCPs.

        for( int jj = 0; jj < *pnGCPCount; ++jj )
        {
            psGCPs[jj].dfGCPPixel = pGCPXCoord[jj];
            psGCPs[jj].dfGCPLine  = pGCPYCoord[jj];
        }
    }

    delete [] (pGCPXCoord);
    delete [] (pGCPYCoord);

    pGCPXCoord = NULL;
    pGCPYCoord = NULL;
}

#endif

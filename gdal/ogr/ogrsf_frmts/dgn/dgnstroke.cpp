/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Code to stroke Arcs/Ellipses into polylines.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Avenza Systems Inc, http://www.avenza.com/
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

#include "dgnlibp.h"
#include <math.h>

CPL_CVSID("$Id$");

#define DEG_TO_RAD (PI/180.0)

/************************************************************************/
/*                         ComputePointOnArc()                          */
/************************************************************************/

static void ComputePointOnArc2D( double dfPrimary, double dfSecondary, 
                                 double dfAxisRotation, double dfAngle,
                                 double *pdfX, double *pdfY )

{
    //dfAxisRotation and dfAngle are suposed to be in Radians
    double      dfCosRotation = cos(dfAxisRotation);
    double      dfSinRotation = sin(dfAxisRotation);
    double      dfEllipseX = dfPrimary * cos(dfAngle);
    double      dfEllipseY = dfSecondary * sin(dfAngle);

    *pdfX = dfEllipseX * dfCosRotation - dfEllipseY * dfSinRotation;
    *pdfY = dfEllipseX * dfSinRotation + dfEllipseY * dfCosRotation;
}

/************************************************************************/
/*                            DGNStrokeArc()                            */
/************************************************************************/

/**
 * Generate a polyline approximation of an arc.
 *
 * Produce a series of equidistant (actually equi-angle) points along
 * an arc.  Currently this only works for 2D arcs (and ellipses). 
 *
 * @param hFile the DGN file to which the arc belongs (currently not used).
 * @param psArc the arc to be approximated.
 * @param nPoints the number of points to use to approximate the arc.
 * @param pasPoints the array of points into which to put the results. 
 * There must be room for at least nPoints points.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DGNStrokeArc( DGNHandle hFile, DGNElemArc *psArc, 
                  int nPoints, DGNPoint * pasPoints )

{
    double      dfAngleStep, dfAngle;
    int         i;

    if( nPoints < 2 )
        return FALSE;

    if( psArc->primary_axis == 0.0 || psArc->secondary_axis == 0.0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Zero primary or secondary axis in DGNStrokeArc()." );
        return FALSE;
    }

    dfAngleStep = psArc->sweepang / (nPoints - 1);
    for( i = 0; i < nPoints; i++ )
    {
        dfAngle = (psArc->startang + dfAngleStep * i) * DEG_TO_RAD;
        
        ComputePointOnArc2D( psArc->primary_axis, 
                             psArc->secondary_axis,
                             psArc->rotation * DEG_TO_RAD,
                             dfAngle,
                             &(pasPoints[i].x),
                             &(pasPoints[i].y) );
        pasPoints[i].x += psArc->origin.x;
        pasPoints[i].y += psArc->origin.y;
        pasPoints[i].z = psArc->origin.z;
    }

    return TRUE;
}

/************************************************************************/
/*                           DGNStrokeCurve()                           */
/************************************************************************/

/**
 * Generate a polyline approximation of an curve.
 *
 * Produce a series of equidistant points along a microstation curve element.
 * Currently this only works for 2D.
 *
 * @param hFile the DGN file to which the arc belongs (currently not used).
 * @param psCurve the curve to be approximated.
 * @param nPoints the number of points to use to approximate the curve.
 * @param pasPoints the array of points into which to put the results. 
 * There must be room for at least nPoints points.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DGNStrokeCurve( DGNHandle hFile, DGNElemMultiPoint *psCurve, 
                    int nPoints, DGNPoint * pasPoints )

{
    int         k, nDGNPoints, iOutPoint;
    double      *padfMx, *padfMy, *padfD, dfTotalD = 0, dfStepSize, dfD;
    double      *padfTx, *padfTy;
    DGNPoint    *pasDGNPoints = psCurve->vertices;

    nDGNPoints = psCurve->num_vertices;

    if( nDGNPoints < 6 )
        return FALSE;

    if( nPoints < nDGNPoints - 4 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compute the Compute the slopes/distances of the segments.       */
/* -------------------------------------------------------------------- */
    padfMx = (double *) CPLMalloc(sizeof(double) * nDGNPoints);
    padfMy = (double *) CPLMalloc(sizeof(double) * nDGNPoints);
    padfD  = (double *) CPLMalloc(sizeof(double) * nDGNPoints);
    padfTx = (double *) CPLMalloc(sizeof(double) * nDGNPoints);
    padfTy = (double *) CPLMalloc(sizeof(double) * nDGNPoints);

    for( k = 0; k < nDGNPoints-1; k++ )
    {
        padfD[k] = sqrt( (pasDGNPoints[k+1].x-pasDGNPoints[k].x)
                           * (pasDGNPoints[k+1].x-pasDGNPoints[k].x)
                         + (pasDGNPoints[k+1].y-pasDGNPoints[k].y)
                           * (pasDGNPoints[k+1].y-pasDGNPoints[k].y) );
        if( padfD[k] == 0.0 )
        {
            padfD[k] = 0.0001;
            padfMx[k] = 0.0;
            padfMy[k] = 0.0;
        }
        else
        {
            padfMx[k] = (pasDGNPoints[k+1].x - pasDGNPoints[k].x) / padfD[k];
            padfMy[k] = (pasDGNPoints[k+1].y - pasDGNPoints[k].y) / padfD[k];
        }

        if( k > 1 && k < nDGNPoints - 3 )
            dfTotalD += padfD[k];
    }

/* -------------------------------------------------------------------- */
/*      Compute the Tx, and Ty coefficients for each segment.           */
/* -------------------------------------------------------------------- */
    for( k = 2; k < nDGNPoints - 2; k++ )
    {
        if( fabs(padfMx[k+1] - padfMx[k]) == 0.0
            && fabs(padfMx[k-1] - padfMx[k-2]) == 0.0 )
        {
            padfTx[k] = (padfMx[k] + padfMx[k-1]) / 2;
        }
        else
        {
            padfTx[k] = (padfMx[k-1] * fabs( padfMx[k+1] - padfMx[k])
                    + padfMx[k] * fabs( padfMx[k-1] - padfMx[k-2] ))
           / (ABS(padfMx[k+1] - padfMx[k]) + ABS(padfMx[k-1] - padfMx[k-2]));
        }

        if( fabs(padfMy[k+1] - padfMy[k]) == 0.0
            && fabs(padfMy[k-1] - padfMy[k-2]) == 0.0 )
        {
            padfTy[k] = (padfMy[k] + padfMy[k-1]) / 2;
        }
        else
        {
            padfTy[k] = (padfMy[k-1] * fabs( padfMy[k+1] - padfMy[k])
                    + padfMy[k] * fabs( padfMy[k-1] - padfMy[k-2] ))
            / (ABS(padfMy[k+1] - padfMy[k]) + ABS(padfMy[k-1] - padfMy[k-2]));
        }
    }

/* -------------------------------------------------------------------- */
/*      Determine a step size in D.  We scale things so that we have    */
/*      roughly equidistant steps in D, but assume we also want to      */
/*      include every node along the way.                               */
/* -------------------------------------------------------------------- */
    dfStepSize = dfTotalD / (nPoints - (nDGNPoints - 4) - 1);

/* ==================================================================== */
/*      Process each of the segments.                                   */
/* ==================================================================== */
    dfD = dfStepSize;
    iOutPoint = 0;

    for( k = 2; k < nDGNPoints - 3; k++ )
    {
        double  dfAx, dfAy, dfBx, dfBy, dfCx, dfCy;

/* -------------------------------------------------------------------- */
/*      Compute the "x" coefficients for this segment.                  */
/* -------------------------------------------------------------------- */
        dfCx = padfTx[k];
        dfBx = (3.0 * (pasDGNPoints[k+1].x - pasDGNPoints[k].x) / padfD[k]
                - 2.0 * padfTx[k] - padfTx[k+1]) / padfD[k];
        dfAx = (padfTx[k] + padfTx[k+1] 
                - 2 * (pasDGNPoints[k+1].x - pasDGNPoints[k].x) / padfD[k])
            / (padfD[k] * padfD[k]);

/* -------------------------------------------------------------------- */
/*      Compute the Y coefficients for this segment.                    */
/* -------------------------------------------------------------------- */
        dfCy = padfTy[k];
        dfBy = (3.0 * (pasDGNPoints[k+1].y - pasDGNPoints[k].y) / padfD[k]
                - 2.0 * padfTy[k] - padfTy[k+1]) / padfD[k];
        dfAy = (padfTy[k] + padfTy[k+1] 
                - 2 * (pasDGNPoints[k+1].y - pasDGNPoints[k].y) / padfD[k])
            / (padfD[k] * padfD[k]);

/* -------------------------------------------------------------------- */
/*      Add the start point for this segment.                           */
/* -------------------------------------------------------------------- */
        pasPoints[iOutPoint].x = pasDGNPoints[k].x;
        pasPoints[iOutPoint].y = pasDGNPoints[k].y;
        pasPoints[iOutPoint].z = 0.0;
        iOutPoint++;

/* -------------------------------------------------------------------- */
/*      Step along, adding intermediate points.                         */
/* -------------------------------------------------------------------- */
        while( dfD < padfD[k] && iOutPoint < nPoints - (nDGNPoints-k-1) )
        {
            pasPoints[iOutPoint].x = dfAx * dfD * dfD * dfD
                                   + dfBx * dfD * dfD
                                   + dfCx * dfD 
                                   + pasDGNPoints[k].x;
            pasPoints[iOutPoint].y = dfAy * dfD * dfD * dfD
                                   + dfBy * dfD * dfD
                                   + dfCy * dfD 
                                   + pasDGNPoints[k].y;
            pasPoints[iOutPoint].z = 0.0;
            iOutPoint++;

            dfD += dfStepSize;
        }

        dfD -= padfD[k];
    }

/* -------------------------------------------------------------------- */
/*      Add the start point for this segment.                           */
/* -------------------------------------------------------------------- */
    while( iOutPoint < nPoints )
    {
        pasPoints[iOutPoint].x = pasDGNPoints[nDGNPoints-3].x;
        pasPoints[iOutPoint].y = pasDGNPoints[nDGNPoints-3].y;
        pasPoints[iOutPoint].z = 0.0;
        iOutPoint++;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CPLFree( padfMx );
    CPLFree( padfMy );
    CPLFree( padfD );
    CPLFree( padfTx );
    CPLFree( padfTy );

    return TRUE;
}

/************************************************************************/
/*                                main()                                */
/*                                                                      */
/*      test mainline                                                   */
/************************************************************************/
#ifdef notdef
int main( int argc, char ** argv )

{
    if( argc != 5 )
    {
        printf( "Usage: stroke primary_axis secondary_axis axis_rotation angle\n" );
        exit( 1 );
    }

    double      dfX, dfY, dfPrimary, dfSecondary, dfAxisRotation, dfAngle;

    dfPrimary = atof(argv[1]);
    dfSecondary = atof(argv[2]);
    dfAxisRotation = atof(argv[3]) / 180 * PI;
    dfAngle = atof(argv[4]) / 180 * PI;

    ComputePointOnArc2D( dfPrimary, dfSecondary, dfAxisRotation, dfAngle, 
                         &dfX, &dfY );

    printf( "X=%.2f, Y=%.2f\n", dfX, dfY );

    exit( 0 );
}

#endif


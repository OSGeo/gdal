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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2001/03/07 13:56:44  warmerda
 * updated copyright to be held by Avenza Systems
 *
 * Revision 1.1  2001/01/22 14:54:51  warmerda
 * New
 *
 */

#include "dgnlibp.h"
#include <math.h>

#ifndef PI
#define PI  3.14159265358979323846
#endif

#define DEG_TO_RAD (PI/180.0)

/************************************************************************/
/*                         ComputePointOnArc()                          */
/************************************************************************/

static void ComputePointOnArc2D( double dfPrimary, double dfSecondary, 
                                 double dfAxisRotation, double dfAngle,
                                 double *pdfX, double *pdfY )

{
    double	dfRadiusSquared, dfRadius, dfX2, dfY2;
    double	dfCosAngle = cos(dfAngle);
    double	dfSinAngle = sin(dfAngle);
    double	dfPrimarySquared = dfPrimary * dfPrimary;
    double	dfSecondarySquared = dfSecondary * dfSecondary;

    dfRadiusSquared = (dfPrimarySquared * dfSecondarySquared)
        / (dfSecondarySquared * dfCosAngle * dfCosAngle
           + dfPrimarySquared * dfSinAngle * dfSinAngle);

    dfRadius = sqrt(dfRadiusSquared);

    dfX2 = dfRadius * cos(dfAngle);
    dfY2 = dfRadius * sin(dfAngle);

    *pdfX = dfX2 * cos(dfAxisRotation) - dfY2 * sin(dfAxisRotation);
    *pdfY = dfX2 * sin(dfAxisRotation) + dfY2 * cos(dfAxisRotation);
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
    double	dfAngleStep, dfAngle;
    int		i;

    if( nPoints < 2 )
        return FALSE;

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

    double	dfX, dfY, dfPrimary, dfSecondary, dfAxisRotation, dfAngle;

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


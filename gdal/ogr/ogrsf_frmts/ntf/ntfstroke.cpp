/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  NTF Arc to polyline stroking code.  This code is really generic,
 *           and might be moved into an OGR module at some point in the
 *           future.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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

#include <stdarg.h>
#include "ntf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

#ifndef PI
#define PI  3.14159265358979323846
#endif

/************************************************************************/
/*                     NTFArcCenterFromEdgePoints()                     */
/*                                                                      */
/*      Compute the center of an arc/circle from three edge points.     */
/************************************************************************/

int NTFArcCenterFromEdgePoints( double x_c0, double y_c0,
                                double x_c1, double y_c1, 
                                double x_c2, double y_c2, 
                                double *x_center, double *y_center )

{

/* -------------------------------------------------------------------- */
/*      Handle a degenerate case that occurs in OSNI products by        */
/*      making some assumptions.  If the first and third points are     */
/*      the same assume they are intended to define a full circle,      */
/*      and that the second point is on the opposite side of the        */
/*      circle.                                                         */
/* -------------------------------------------------------------------- */
    if( x_c0 == x_c2 && y_c0 == y_c2 )
    {
        *x_center = (x_c0 + x_c1) * 0.5;
        *y_center = (y_c0 + y_c1) * 0.5;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Compute the inverse of the slopes connecting the first and      */
/*      second points.  Also compute the center point of the two        */
/*      lines ... the point our crossing line will go through.          */
/* -------------------------------------------------------------------- */
    double m1, x1, y1;

    if( (y_c1 - y_c0) != 0.0 )
        m1 = (x_c0 - x_c1) / (y_c1 - y_c0);
    else
        m1 = 1e+10;

    x1 = (x_c0 + x_c1) * 0.5;
    y1 = (y_c0 + y_c1) * 0.5;

/* -------------------------------------------------------------------- */
/*      Compute the same for the second point compared to the third     */
/*      point.                                                          */
/* -------------------------------------------------------------------- */
    double m2, x2, y2;

    if( (y_c2 - y_c1) != 0.0 )
        m2 = (x_c1 - x_c2) / (y_c2 - y_c1);
    else
        m2 = 1e+10;

    x2 = (x_c1 + x_c2) * 0.5;
    y2 = (y_c1 + y_c2) * 0.5;

/* -------------------------------------------------------------------- */
/*      Turn these into the Ax+By+C = 0 form of the lines.              */
/* -------------------------------------------------------------------- */
    double      a1, a2, b1, b2, c1, c2;

    a1 = m1;
    a2 = m2;

    b1 = -1.0;
    b2 = -1.0;
    
    c1 = (y1 - m1*x1);
    c2 = (y2 - m2*x2);
    
/* -------------------------------------------------------------------- */
/*      Compute the intersection of the two lines through the center    */
/*      of the circle, using Kramers rule.                              */
/* -------------------------------------------------------------------- */
    double      det_inv;

    if( a1*b2 - a2*b1 == 0.0 )
        return FALSE;

    det_inv = 1 / (a1*b2 - a2*b1);

    *x_center = (b1*c2 - b2*c1) * det_inv;
    *y_center = (a2*c1 - a1*c2) * det_inv;

    return TRUE;
}

/************************************************************************/
/*                  NTFStrokeArcToOGRGeometry_Points()                  */
/************************************************************************/

OGRGeometry *
NTFStrokeArcToOGRGeometry_Points( double dfStartX, double dfStartY,
                                  double dfAlongX, double dfAlongY,
                                  double dfEndX, double dfEndY,
                                  int nVertexCount )
    
{
    double      dfStartAngle, dfEndAngle, dfAlongAngle;
    double      dfCenterX, dfCenterY, dfRadius;

    if( !NTFArcCenterFromEdgePoints( dfStartX, dfStartY, dfAlongX, dfAlongY, 
                                     dfEndX, dfEndY, &dfCenterX, &dfCenterY ) )
        return NULL;

    if( dfStartX == dfEndX && dfStartY == dfEndY )
    {
        dfStartAngle = 0.0;
        dfEndAngle = 360.0;
    }
    else
    {
        double  dfDeltaX, dfDeltaY;

        dfDeltaX = dfStartX - dfCenterX;
        dfDeltaY = dfStartY - dfCenterY;
        dfStartAngle = atan2(dfDeltaY,dfDeltaX) * 180.0 / PI;

        dfDeltaX = dfAlongX - dfCenterX;
        dfDeltaY = dfAlongY - dfCenterY;
        dfAlongAngle = atan2(dfDeltaY,dfDeltaX) * 180.0 / PI;

        dfDeltaX = dfEndX - dfCenterX;
        dfDeltaY = dfEndY - dfCenterY;
        dfEndAngle = atan2(dfDeltaY,dfDeltaX) * 180.0 / PI;

#ifdef notdef
        if( dfStartAngle > dfAlongAngle && dfAlongAngle > dfEndAngle )
        {
            double dfTempAngle;

            dfTempAngle = dfStartAngle;
            dfStartAngle = dfEndAngle;
            dfEndAngle = dfTempAngle;
        }
#endif

        while( dfAlongAngle < dfStartAngle )
            dfAlongAngle += 360.0;

        while( dfEndAngle < dfAlongAngle )
            dfEndAngle += 360.0;

        if( dfEndAngle - dfStartAngle > 360.0 )
        {
            double dfTempAngle;

            dfTempAngle = dfStartAngle;
            dfStartAngle = dfEndAngle;
            dfEndAngle = dfTempAngle;

            while( dfEndAngle < dfStartAngle )
                dfStartAngle -= 360.0;
        }
    }

    dfRadius = sqrt( (dfCenterX - dfStartX) * (dfCenterX - dfStartX)
                     + (dfCenterY - dfStartY) * (dfCenterY - dfStartY) );
    
    return NTFStrokeArcToOGRGeometry_Angles( dfCenterX, dfCenterY, 
                                             dfRadius, 
                                             dfStartAngle, dfEndAngle,
                                             nVertexCount );
}

/************************************************************************/
/*                  NTFStrokeArcToOGRGeometry_Angles()                  */
/************************************************************************/

OGRGeometry *
NTFStrokeArcToOGRGeometry_Angles( double dfCenterX, double dfCenterY, 
                                  double dfRadius, 
                                  double dfStartAngle, double dfEndAngle,
                                  int nVertexCount )

{
    OGRLineString      *poLine = new OGRLineString;
    double             dfArcX, dfArcY, dfSlice;
    int                iPoint;

    nVertexCount = MAX(2,nVertexCount);
    dfSlice = (dfEndAngle-dfStartAngle)/(nVertexCount-1);

    poLine->setNumPoints( nVertexCount );
        
    for( iPoint=0; iPoint < nVertexCount; iPoint++ )
    {
        double      dfAngle;

        dfAngle = (dfStartAngle + iPoint * dfSlice) * PI / 180.0;
            
        dfArcX = dfCenterX + cos(dfAngle) * dfRadius;
        dfArcY = dfCenterY + sin(dfAngle) * dfRadius;

        poLine->setPoint( iPoint, dfArcX, dfArcY );
    }

    return poLine;
}



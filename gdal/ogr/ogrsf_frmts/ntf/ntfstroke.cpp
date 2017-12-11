/******************************************************************************
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

#include <algorithm>
#include <utility>

CPL_CVSID("$Id$")

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
    const double m1 =
        (y_c1 - y_c0) != 0.0
        ? (x_c0 - x_c1) / (y_c1 - y_c0)
        : 1e+10;

    const double x1 = (x_c0 + x_c1) * 0.5;
    const double y1 = (y_c0 + y_c1) * 0.5;

/* -------------------------------------------------------------------- */
/*      Compute the same for the second point compared to the third     */
/*      point.                                                          */
/* -------------------------------------------------------------------- */
    const double m2 =
        (y_c2 - y_c1) != 0.0
        ? (x_c1 - x_c2) / (y_c2 - y_c1)
        : 1e+10;

    const double x2 = (x_c1 + x_c2) * 0.5;
    const double y2 = (y_c1 + y_c2) * 0.5;

/* -------------------------------------------------------------------- */
/*      Turn these into the Ax+By+C = 0 form of the lines.              */
/* -------------------------------------------------------------------- */
    const double a1 = m1;
    const double a2 = m2;

    const double b1 = -1.0;
    const double b2 = -1.0;

    const double c1 = (y1 - m1*x1);
    const double c2 = (y2 - m2*x2);

/* -------------------------------------------------------------------- */
/*      Compute the intersection of the two lines through the center    */
/*      of the circle, using Kramers rule.                              */
/* -------------------------------------------------------------------- */
    if( a1*b2 - a2*b1 == 0.0 )
        return FALSE;

    const double det_inv = 1 / (a1*b2 - a2*b1);

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
    double dfStartAngle = 0.0;
    double dfEndAngle = 0.0;
    double dfCenterX = 0.0;
    double dfCenterY = 0.0;
    double dfRadius = 0.0;

    if( !NTFArcCenterFromEdgePoints( dfStartX, dfStartY, dfAlongX, dfAlongY,
                                     dfEndX, dfEndY, &dfCenterX, &dfCenterY ) )
        return nullptr;

    if( dfStartX == dfEndX && dfStartY == dfEndY )
    {
        dfStartAngle = 0.0;
        dfEndAngle = 360.0;
    }
    else
    {
        double dfDeltaX = dfStartX - dfCenterX;
        double dfDeltaY = dfStartY - dfCenterY;
        dfStartAngle = atan2(dfDeltaY, dfDeltaX) * 180.0 / M_PI;

        dfDeltaX = dfAlongX - dfCenterX;
        dfDeltaY = dfAlongY - dfCenterY;
        double dfAlongAngle = atan2(dfDeltaY, dfDeltaX) * 180.0 / M_PI;

        dfDeltaX = dfEndX - dfCenterX;
        dfDeltaY = dfEndY - dfCenterY;
        dfEndAngle = atan2(dfDeltaY,dfDeltaX) * 180.0 / M_PI;

        while( dfAlongAngle < dfStartAngle )
            dfAlongAngle += 360.0;

        while( dfEndAngle < dfAlongAngle )
            dfEndAngle += 360.0;

        if( dfEndAngle - dfStartAngle > 360.0 )
        {
            std::swap(dfStartAngle, dfEndAngle);

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
    OGRLineString *poLine = new OGRLineString;

    nVertexCount = std::max(2, nVertexCount);
    const double dfSlice = (dfEndAngle-dfStartAngle)/(nVertexCount-1);

    poLine->setNumPoints( nVertexCount );

    for( int iPoint = 0; iPoint < nVertexCount; iPoint++ )
    {
        const double dfAngle = (dfStartAngle + iPoint * dfSlice) * M_PI / 180.0;

        const double dfArcX = dfCenterX + cos(dfAngle) * dfRadius;
        const double dfArcY = dfCenterY + sin(dfAngle) * dfRadius;

        poLine->setPoint( iPoint, dfArcX, dfArcY );
    }

    return poLine;
}

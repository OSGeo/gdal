/******************************************************************************
 * $Id: ogr_xplane_geo_utils.cpp
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Geo-computations
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_xplane_geo_utils.h"
#include <math.h>
#include "cpl_port.h"

CPL_CVSID("$Id$");

#ifndef M_PI
# define M_PI  3.1415926535897932384626433832795
#endif

#define RAD2METER            ((180./M_PI)*60.*1852.)
#define METER2RAD            (1/RAD2METER)

#define DEG2RAD              (M_PI/180.)
#define RAD2DEG              (1/DEG2RAD)

static
double OGRXPlane_Safe_acos(double x)
{
    if (x > 1)
        x = 1;
    else if (x < -1)
        x = -1;
    return acos(x);
}

/************************************************************************/
/*                         OGRXPlane_Distance()                         */
/************************************************************************/

double OGRXPlane_Distance(double LatA_deg, double LonA_deg,
                          double LatB_deg, double LonB_deg)
{
    double LatA_rad, LatB_rad;
    double cosa, cosb, sina, sinb, cosP;
    double cos_angle;

    cosP = cos((LonB_deg - LonA_deg) * DEG2RAD);
    LatA_rad = LatA_deg * DEG2RAD;
    LatB_rad = LatB_deg * DEG2RAD;
    cosa = cos(LatA_rad);
    sina = sin(LatA_rad);
    cosb = cos(LatB_rad);
    sinb = sin(LatB_rad);
    cos_angle = sina*sinb + cosa*cosb*cosP;
    return OGRXPlane_Safe_acos(cos_angle) * RAD2METER;
}

/************************************************************************/
/*                           OGRXPlane_Track()                          */
/************************************************************************/

double OGRXPlane_Track(double LatA_deg, double LonA_deg,
                       double LatB_deg, double LonB_deg)
{
    if (fabs (LatA_deg - 90) < 1e-10 || fabs (LatB_deg + 90) < 1e-10)
    {
        return 180;
    }
    else  if (fabs (LatA_deg + 90) < 1e-10 || fabs (LatB_deg - 90) < 1e-10)
    {
        return 0;
    }
    else
    {
        double cos_LatA, sin_LatA, diffG, cos_diffG, sin_diffG;
        double denom;
        double track;
        double LatA_rad = LatA_deg * DEG2RAD;
        double LatB_rad = LatB_deg * DEG2RAD;

        cos_LatA = cos(LatA_rad);
        sin_LatA = sin(LatA_rad);

        diffG = (LonA_deg - LonB_deg) * DEG2RAD;
        cos_diffG = cos(diffG);
        sin_diffG = sin(diffG);

        denom = sin_LatA * cos_diffG - cos_LatA * tan(LatB_rad);

        track = atan (sin_diffG / denom) * RAD2DEG;

        if (denom > 0.0)
        {
            track = 180 + track;
        }
        else if (track < 0)
        {
            track = 360 + track;
        }

        return track;
    }
}

/************************************************************************/
/*                       OGRXPlane_ExtendPosition()                     */
/************************************************************************/

int OGRXPlane_ExtendPosition(double dfLatA_deg, double dfLonA_deg,
                             double dfDistance, double dfHeading,
                             double* pdfLatB_deg, double* pdfLonB_deg)
{
    double dfHeadingRad, cos_Heading, sin_Heading;
    double dfDistanceRad, cos_Distance, sin_Distance;
    double dfLatA_rad, cos_complement_LatA, sin_complement_LatA;
    double cos_complement_latB, complement_latB;
    double Cos_dG, dG_deg;

    dfHeadingRad = dfHeading * DEG2RAD;
    cos_Heading = cos (dfHeadingRad);
    sin_Heading = sin (dfHeadingRad);

    dfDistanceRad = dfDistance * METER2RAD;
    cos_Distance = cos (dfDistanceRad);
    sin_Distance = sin (dfDistanceRad);

    dfLatA_rad = dfLatA_deg * DEG2RAD;
    cos_complement_LatA = sin(dfLatA_rad);
    sin_complement_LatA = cos(dfLatA_rad);

    cos_complement_latB = cos_Distance * cos_complement_LatA +
                          sin_Distance * sin_complement_LatA * cos_Heading;

    complement_latB  = OGRXPlane_Safe_acos(cos_complement_latB);

    Cos_dG = (cos_Distance - cos_complement_latB * cos_complement_LatA) /
                    (sin(complement_latB) * sin_complement_LatA);
    *pdfLatB_deg = 90 - complement_latB * RAD2DEG;

    dG_deg  = OGRXPlane_Safe_acos(Cos_dG) * RAD2DEG;

    if (sin_Heading < 0)
        *pdfLonB_deg = dfLonA_deg - dG_deg;
    else
        *pdfLonB_deg = dfLonA_deg + dG_deg;

    if (*pdfLonB_deg > 180)
        *pdfLonB_deg -= 360;
    else if (*pdfLonB_deg <= -180)
        *pdfLonB_deg += 360;

    return 1;
}

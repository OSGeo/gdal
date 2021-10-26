/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Geo-computations
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_geo_utils.h"
#include "cpl_port.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

constexpr double RAD2METER = (180.0 / M_PI) * 60.0 * 1852.0;
constexpr double METER2RAD = 1.0 / RAD2METER;

constexpr double DEG2RAD = M_PI / 180.0;
constexpr double RAD2DEG = 1.0 / DEG2RAD;

static
double OGR_Safe_acos( double x )
{
    if( x > 1 )
        x = 1;
    else if( x < -1 )
        x = -1;
    return acos(x);
}

/************************************************************************/
/*                      OGR_GreatCircle_Distance()                      */
/************************************************************************/

double OGR_GreatCircle_Distance( double LatA_deg, double LonA_deg,
                                 double LatB_deg, double LonB_deg )
{
    const double cosP = cos((LonB_deg - LonA_deg) * DEG2RAD);
    const double LatA_rad = LatA_deg * DEG2RAD;
    const double LatB_rad = LatB_deg * DEG2RAD;
    const double cosa = cos(LatA_rad);
    const double sina = sin(LatA_rad);
    const double cosb = cos(LatB_rad);
    const double sinb = sin(LatB_rad);
    const double cos_angle = sina*sinb + cosa*cosb*cosP;
    return OGR_Safe_acos(cos_angle) * RAD2METER;
}

/************************************************************************/
/*                      OGR_GreatCircle_InitialHeading()                */
/************************************************************************/

double OGR_GreatCircle_InitialHeading( double LatA_deg, double LonA_deg,
                                       double LatB_deg, double LonB_deg )
{
    if( fabs (LatA_deg - 90) < 1e-10 || fabs (LatB_deg + 90) < 1e-10 )
    {
        return 180;
    }
    else if( fabs (LatA_deg + 90) < 1e-10 || fabs (LatB_deg - 90) < 1e-10 )
    {
        return 0;
    }
    else if( fabs(fmod(LonA_deg - LonB_deg, 360.0)) < 1e-10 &&
             fabs(LatA_deg - LatB_deg) < 1e-10 )
    {
        return 0; // Arbitrary number
    }
    else if( fabs(LatA_deg) < 1e-10 && fabs(LatB_deg) < 1e-10 )
    {
        return (LonB_deg > LonA_deg) ? 90.0 : 270.0;
    }
    else if( fabs(fmod(LonA_deg - LonB_deg, 360.0)) < 1e-10 )
    {
        return (LatA_deg > LatB_deg) ? 180.0 : 0.0;
    }
    else
    {
        const double LatA_rad = LatA_deg * DEG2RAD;
        const double LatB_rad = LatB_deg * DEG2RAD;

        const double cos_LatA = cos(LatA_rad);
        const double sin_LatA = sin(LatA_rad);

        const double diffG = (LonA_deg - LonB_deg) * DEG2RAD;
        const double cos_diffG = cos(diffG);
        const double sin_diffG = sin(diffG);

        const double denom = sin_LatA * cos_diffG - cos_LatA * tan(LatB_rad);
        if( denom == 0.0 )
        {
            // Can be the the case if Lat_A = -Lat_B and abs(LonA - LonB) = 180
            return 0.0;
        }

        double track = atan (sin_diffG / denom) * RAD2DEG;

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
/*                     OGR_GreatCircle_ExtendPosition()                 */
/************************************************************************/

int OGR_GreatCircle_ExtendPosition(double dfLatA_deg, double dfLonA_deg,
                                   double dfDistance, double dfHeadingInA,
                                   double* pdfLatB_deg, double* pdfLonB_deg)
{
    const double dfHeadingRad = dfHeadingInA * DEG2RAD;
    const double cos_Heading = cos (dfHeadingRad);
    const double sin_Heading = sin (dfHeadingRad);

    const double dfDistanceRad = dfDistance * METER2RAD;
    const double cos_Distance = cos (dfDistanceRad);
    const double sin_Distance = sin (dfDistanceRad);

    const double dfLatA_rad = dfLatA_deg * DEG2RAD;
    const double cos_complement_LatA = sin(dfLatA_rad);
    const double sin_complement_LatA = cos(dfLatA_rad);

    if( dfDistance == 0.0 )
    {
        *pdfLatB_deg = dfLatA_deg;
        *pdfLonB_deg = dfLonA_deg;
        return 1;
    }

    if( fabs(dfLatA_deg) >= 90.0 )
    {
        *pdfLatB_deg = dfLatA_deg;
        *pdfLonB_deg = dfLonA_deg;
        return 0;
    }

    if( fabs(sin_Heading) < 1e-8 )
    {
        *pdfLonB_deg = dfLonA_deg;
        if( fabs(fmod(dfHeadingInA+360.0,360.0)) < 1e-8 )
        {
            *pdfLatB_deg = dfLatA_deg + dfDistanceRad * RAD2DEG;
        }
        else
        {
            *pdfLatB_deg = dfLatA_deg - dfDistanceRad * RAD2DEG;
        }
        return 1;
    }

    if( fabs(cos_complement_LatA) < 1e-8 && fabs(cos_Heading) < 1e-8 )
    {
        *pdfLatB_deg = dfLatA_deg;
        if( fabs(dfHeadingInA - 90.0) < 1e-8 )
        {
            *pdfLonB_deg = dfLonA_deg + dfDistanceRad * RAD2DEG;
        }
        else
        {
            *pdfLonB_deg = dfLonA_deg - dfDistanceRad * RAD2DEG;
        }
        return 1;
    }

    const double cos_complement_latB =
        cos_Distance * cos_complement_LatA +
        sin_Distance * sin_complement_LatA * cos_Heading;

    const double complement_latB  = OGR_Safe_acos(cos_complement_latB);

    const double dfDenomin = sin(complement_latB) * sin_complement_LatA;
    if( dfDenomin == 0.0 )
        CPLDebug("OGR", "OGR_GreatCircle_Distance: dfDenomin == 0.0");
    const double Cos_dG =
        (cos_Distance - cos_complement_latB * cos_complement_LatA) /
        dfDenomin;
    *pdfLatB_deg = 90 - complement_latB * RAD2DEG;

    const double dG_deg  = OGR_Safe_acos(Cos_dG) * RAD2DEG;

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

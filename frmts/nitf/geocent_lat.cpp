/***************************************************************************
 * $Id$
 *
 * Project:  NITF Reader
 * Purpose:  Geocentric Latitude to Geodetic Latitude Converter
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *           Parts derived from Geotrans GEOCENTRIC module. 
 *
 ***************************************************************************
 *
 *   Notes on Geodetic vs. Geocentric Latitude
 *   -----------------------------------------
 *
 * "The angle L' is called "geocentric latitude" and is defined as the
 * angle between the equatorial plane and the radius from the geocenter.
 * 
 * The angle L is called "geodetic latitude" and is defined as the angle
 * between the equatorial plane and the normal to the surface of the
 * ellipsoid.  The word "latitude" usually means geodetic latitude.  This
 * is the basis for most of the maps and charts we use.  The normal to the
 * surface is the direction that a plumb bob would hang were it not for
 * local anomalies in the earth's gravitational field."
 *
 ***************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 ***************************************************************************
 * $Log$
 * Revision 1.1  2004/04/28 15:18:27  warmerda
 * New
 *
 */

#include <math.h>
#include <assert.h>
#include "nitflib.h"


/***************************************************************************/
/*
 *                               DEFINES
 */
#define PI         3.14159265358979323e0
#define PI_OVER_2  (PI / 2.0e0)
#define FALSE      0
#define TRUE       1
#define COS_67P5   0.38268343236508977  /* cosine of 67.5 degrees */
#define AD_C       1.0026000            /* Toms region 1 constant */


/***************************************************************************/
/*
 *                              GLOBAL DECLARATIONS
 */

/* Ellipsoid parameters, default to WGS 84 */
static double Geocent_a = 6378137.0;     /* Semi-major axis of ellipsoid in meters */
static double Geocent_b = 6356752.3142;  /* Semi-minor axis of ellipsoid           */

static double Geocent_a2 = 40680631590769.0;       /* Square of semi-major axis*/
static double Geocent_b2 = 40408299984087.05;      /* Square of semi-minor axis*/
static double Geocent_e2 = 0.0066943799901413800;  /* Eccentricity squared   */
static double Geocent_ep2 = 0.00673949675658690300;/* 2nd eccentricity squared*/
/*
 * These state variables are for optimization purposes.  The only function
 * that should modify them is Set_Geocentric_Parameters.
 */

/************************************************************************/
/*                   Convert_Geocentric_To_Geodetic()                   */
/*                                                                      */
/*      This is essentially unchanged from the GeoTrans GEOCENTRIC      */
/*      module.                                                         */
/************************************************************************/

static void Convert_Geocentric_To_Geodetic (double X,
                                            double Y, 
                                            double Z,
                                            double *Latitude,
                                            double *Longitude,
                                            double *Height)
{ /* BEGIN Convert_Geocentric_To_Geodetic */
/*
 * The function Convert_Geocentric_To_Geodetic converts geocentric
 * coordinates (X, Y, Z) to geodetic coordinates (latitude, longitude, 
 * and height), according to the current ellipsoid parameters.
 *
 *    X         : Geocentric X coordinate, in meters.         (input)
 *    Y         : Geocentric Y coordinate, in meters.         (input)
 *    Z         : Geocentric Z coordinate, in meters.         (input)
 *    Latitude  : Calculated latitude value in radians.       (output)
 *    Longitude : Calculated longitude value in radians.      (output)
 *    Height    : Calculated height value, in meters.         (output)
 *
 * The method used here is derived from 'An Improved Algorithm for
 * Geocentric to Geodetic Coordinate Conversion', by Ralph Toms, Feb 1996
 */

/* Note: Variable names follow the notation used in Toms, Feb 1996 */

  double W;        /* distance from Z axis */
  double W2;       /* square of distance from Z axis */
  double T0;       /* initial estimate of vertical component */
  double T1;       /* corrected estimate of vertical component */
  double S0;       /* initial estimate of horizontal component */
  double S1;       /* corrected estimate of horizontal component */
  double Sin_B0;   /* sin(B0), B0 is estimate of Bowring aux variable */
  double Sin3_B0;  /* cube of sin(B0) */
  double Cos_B0;   /* cos(B0) */
  double Sin_p1;   /* sin(phi1), phi1 is estimated latitude */
  double Cos_p1;   /* cos(phi1) */
  double Rn;       /* Earth radius at location */
  double Sum;      /* numerator of cos(phi1) */
  int At_Pole;     /* indicates location is in polar region */

  At_Pole = FALSE;
  if (X != 0.0)
  {
    *Longitude = atan2(Y,X);
  }
  else
  {
    if (Y > 0)
    {
      *Longitude = PI_OVER_2;
    }
    else if (Y < 0)
    {
      *Longitude = -PI_OVER_2;
    }
    else
    {
      At_Pole = TRUE;
      *Longitude = 0.0;
      if (Z > 0.0)
      {  /* north pole */
        *Latitude = PI_OVER_2;
      }
      else if (Z < 0.0)
      {  /* south pole */
        *Latitude = -PI_OVER_2;
      }
      else
      {  /* center of earth */
        *Latitude = PI_OVER_2;
        *Height = -Geocent_b;
        return;
      } 
    }
  }
  W2 = X*X + Y*Y;
  W = sqrt(W2);
  T0 = Z * AD_C;
  S0 = sqrt(T0 * T0 + W2);
  Sin_B0 = T0 / S0;
  Cos_B0 = W / S0;
  Sin3_B0 = Sin_B0 * Sin_B0 * Sin_B0;
  T1 = Z + Geocent_b * Geocent_ep2 * Sin3_B0;
  Sum = W - Geocent_a * Geocent_e2 * Cos_B0 * Cos_B0 * Cos_B0;
  S1 = sqrt(T1*T1 + Sum * Sum);
  Sin_p1 = T1 / S1;
  Cos_p1 = Sum / S1;
  Rn = Geocent_a / sqrt(1.0 - Geocent_e2 * Sin_p1 * Sin_p1);
  if (Cos_p1 >= COS_67P5)
  {
    *Height = W / Cos_p1 - Rn;
  }
  else if (Cos_p1 <= -COS_67P5)
  {
    *Height = W / -Cos_p1 - Rn;
  }
  else
  {
    *Height = Z / Sin_p1 + Rn * (Geocent_e2 - 1.0);
  }
  if (At_Pole == FALSE)
  {
    *Latitude = atan(Sin_p1 / Cos_p1);
  }
} /* END OF Convert_Geocentric_To_Geodetic */

/************************************************************************/
/*        NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude()         */
/*                                                                      */
/*      Input latitude is in geocentric degress and is returned in      */
/*      geodetic degrees.                                               */
/************************************************************************/

double NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( double dfLat )

{
    double dfRadius, dfGeocent_x, dfGeocent_y, dfGeocent_z; 
    double dfGeodeticLat, dfGeodeticLong, dfHeight;

    dfLat = dfLat * PI / 180.0;

/* -------------------------------------------------------------------- */
/*      Compute radius to ellipsoid surface from geocentric latitude    */
/*      at this latitude.                                               */
/* -------------------------------------------------------------------- */
    dfRadius = sqrt( (Geocent_a2*Geocent_b2) 
                     / (Geocent_b2 * cos(dfLat) * cos(dfLat)
                        + Geocent_a2 * sin(dfLat) * sin(dfLat)) );

/* -------------------------------------------------------------------- */
/*      Compute geocentric x/y/z (we assume the location is on the      */
/*      ellipsoid, not above or below it).  We also assume we are at    */
/*      longitude 0 though it won't matter to the final latitude        */
/*      computation.                                                    */
/* -------------------------------------------------------------------- */
    dfGeocent_x = cos( dfLat ) * dfRadius;
    dfGeocent_y = 0;
    dfGeocent_z = sin( dfLat ) * dfRadius;

/* -------------------------------------------------------------------- */
/*      Convert this to a geodetic location.                            */
/* -------------------------------------------------------------------- */
    Convert_Geocentric_To_Geodetic( dfGeocent_x, dfGeocent_y, dfGeocent_z,
                                    &dfGeodeticLat, &dfGeodeticLong,&dfHeight);
    
/* -------------------------------------------------------------------- */
/*      We should be on the surface, at the prime meridian.             */
/* -------------------------------------------------------------------- */
    assert( fabs(dfGeodeticLong) < 0.001 );
    assert( fabs(dfHeight) < 1 );

    return dfGeodeticLat * 180.0 / PI;
}

#ifdef TESTME

#include <stdio.h>
#include <stdlib.h>

int main()

{
    char  szInLine[1000];

    while( fgets( szInLine, sizeof(szInLine), stdin ) != NULL
           && !feof(stdin) )
    {
        double dfInLat, dfOutLat;
        
        dfInLat = atof(szInLine);
        dfOutLat = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( dfInLat );

        
        printf( "%.16f -> %.16f\n", dfInLat, dfOutLat);
    }
}

#endif

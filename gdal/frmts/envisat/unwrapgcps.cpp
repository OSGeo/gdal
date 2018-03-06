/******************************************************************************
 *
 * Project:  APP ENVISAT Support
 * Purpose:  GCPs Unwrapping for products crossing the WGS84 date-line
 * Author:   Martin Paces martin.paces@eox.at
 *
 ******************************************************************************
 * Copyright (c) 2013, EOX IT Services, GmbH
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

#include "gdal.h"
#include <cmath>
#include <cstdio>

CPL_CVSID("$Id$")

// number of histogram bins (36 a 10dg)
constexpr int NBIN = 36;
// number of empty bins to guess the flip-point
constexpr int NEMPY = 7;

// WGS84 bounds
constexpr double XMIN = -180.0;
// constexpr double XMAX = 180.0;
constexpr double XDIF = 360.0;
constexpr double XCNT = 0.0;

// max. allowed longitude extent of the GCP set
constexpr double XLIM = XDIF*(1.0-NEMPY*(1.0/NBIN));

/* used by envisatdataset.cpp */
extern void EnvisatUnwrapGCPs( int cnt, GDAL_GCP *gcp );

// The algorithm is based on assumption that the unwrapped
// GCPs ('flipped' values) have smaller extent along the longitude.
// We further assume that the length of the striplines is limited
// to one orbit and does not exceeded given limit along the longitude,
// e.i., the wrapped-around coordinates have significantly larger
// extent the unwrapped. If the smaller extend exceeds the limit
// the original tiepoints are returned.

static double _suggest_flip_point( const int cnt, GDAL_GCP *gcp )
{
    // the histogram array - it is expected to fit the stack
    int hist[NBIN] ;

    // reset the histogram counters
    for( int i = 0 ; i < NBIN ; i++ ) hist[i] = 0 ;

    // accumulate the histogram
    for( int i = 0 ; i < cnt ; i++ )
    {
        double x = (gcp[i].dfGCPX-XMIN)/XDIF;
        int idx = (int)(NBIN*(x-floor(x)));

        // The latitudes should lay in the +/-180 bounds
        // although it should never happen we check the outliers
        if (idx < 0) idx = 0 ;
        if (idx >= NBIN ) idx = NBIN-1;

        hist[idx] += 1 ;
    }

    // Find middle of at least NEMPTY consecutive empty bins and get its middle.
    int i0 = -1;
    int i1 = -1;
    int last_is_empty = 0;
    for( int i = 0; i < (2*NBIN-1); i++ )
    {
        if ( 0 == hist[i%NBIN] ) // empty
        {
            if ( !last_is_empty ) // re-start counter
            {
                i0 = i ;
                last_is_empty = 1 ;
            }
        }
        else // non-empty
        {
            if ( last_is_empty )
            {
                i1 = i ;
                last_is_empty = 0 ;

                // if the segment is long enough -> terminate
                if (( i1 - i0 )>=NEMPY) break ;
            }
        }
    }

    // if all full or all empty the returning default value
    if ( i1 < 0 ) return XCNT ;

    // return the flip-centre

    double tmp = ((i1-i0)*0.5+i0)/((float)NBIN) ;

    return (tmp-floor(tmp))*XDIF + XMIN;
}

void EnvisatUnwrapGCPs( int cnt, GDAL_GCP *gcp )
{
    if ( cnt < 1 ) return ;

    // suggest right flip-point
    double x_flip = _suggest_flip_point( cnt, gcp );

    // Find the limits along the longitude (x) for flipped and unflipped values.

    int cnt_flip = 0 ; // flipped values' counter
    double x0_dif , x1_dif ;

    {
        double x0_min;
        double x0_max;
        double x1_min;
        double x1_max;

        {
            double x0 = gcp[0].dfGCPX;
            int  flip = (x0>x_flip) ;
            x0_min = x0;
            x0_max = x0;
            x1_min = x0 - flip*XDIF;
            x1_max = x1_min;
            cnt_flip += flip ; // count the flipped values
        }

        for ( int i = 1 ; i < cnt ; ++i )
        {
            double x0 = gcp[i].dfGCPX ;
            int  flip = (x0>x_flip) ;
            double x1 = x0 - flip*XDIF ; // flipped value
            cnt_flip += flip ; // count the flipped values

            if ( x0 > x0_max ) x0_max = x0 ;
            if ( x0 < x0_min ) x0_min = x0 ;
            if ( x1 > x1_max ) x1_max = x1 ;
            if ( x1 < x1_min ) x1_min = x1 ;
        }

        x0_dif = x0_max - x0_min ;
        x1_dif = x1_max - x1_min ;
    }

    // in case all values either flipped or non-flipped
    // nothing is to be done
    if (( cnt_flip == 0 ) || ( cnt_flip == cnt )) return ;

    // check whether we need to split the segment
    // i.e., segment is too long decide the best option

    if (( x0_dif > XLIM ) && ( x1_dif > XLIM ))
    {
        // this should not happen
        // we give-up and return the original tie-point set

        CPLError(CE_Warning,CPLE_AppDefined,"GCPs' set is too large"
            " to perform the unwrapping! The unwrapping is not performed!");

        return ;
    }
    else if ( x1_dif < x0_dif )
    {
        // flipped GCPs' set has smaller extent -> unwrapping is performed
        for ( int i = 1 ; i < cnt ; ++i )
        {
            double x0 = gcp[i].dfGCPX ;

            gcp[i].dfGCPX = x0 - (x0>XCNT)*XDIF ;
        }
    }
}

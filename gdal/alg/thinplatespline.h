/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Warp API
 * Purpose:  Declarations for 2D Thin Plate Spline transformer. 
 * Author:   VIZRT Development Team.
 *
 * This code was provided by Gilad Ronnen (gro at visrt dot com) with 
 * permission to reuse under the following license.
 * 
 ******************************************************************************
 * Copyright (c) 2004, VIZRT Inc.
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

#include "gdal_alg.h"
#include "cpl_conv.h"

typedef enum
{
	VIZ_GEOREF_SPLINE_ZERO_POINTS,
	VIZ_GEOREF_SPLINE_ONE_POINT,
	VIZ_GEOREF_SPLINE_TWO_POINTS,
	VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL,
	VIZ_GEOREF_SPLINE_FULL,
	
	VIZ_GEOREF_SPLINE_POINT_WAS_ADDED,
	VIZ_GEOREF_SPLINE_POINT_WAS_DELETED

} vizGeorefInterType;

//#define VIZ_GEOREF_SPLINE_MAX_POINTS 40
#define VIZGEOREF_MAX_VARS 2

class VizGeorefSpline2D
{
  public:

    VizGeorefSpline2D(int nof_vars = 1){
        x = y = u = NULL;
        unused = index = NULL;
        for( int i = 0; i < nof_vars; i++ )
        {
            rhs[i] = NULL;
            coef[i] = NULL;
        }
          
        _tx = _ty = 0.0;		
        _ta = 10.0;
        _nof_points = 0;
        _nof_vars = nof_vars;
        _max_nof_points = 0;
        grow_points();
        type = VIZ_GEOREF_SPLINE_ZERO_POINTS;
    }

    ~VizGeorefSpline2D(){
        CPLFree( x );
        CPLFree( y );
        CPLFree( u );
        CPLFree( unused );
        CPLFree( index );
        for( int i = 0; i < _nof_vars; i++ )
        {
            CPLFree( rhs[i] );
            CPLFree( coef[i] );
        }
    }

#if 0
    int get_nof_points(){
        return _nof_points;
    }

    void set_toler( double tx, double ty ){
        _tx = tx;
        _ty = ty;
    }

    void get_toler( double& tx, double& ty) {
        tx = _tx;
        ty = _ty;
    }

    vizGeorefInterType get_interpolation_type ( ){
        return type;
    }

    void dump_data_points()
	{
            for ( int i = 0; i < _nof_points; i++ )
            {
                fprintf(stderr, "X = %f Y = %f Vars = ", x[i], y[i]);
                for ( int v = 0; v < _nof_vars; v++ )
                    fprintf(stderr, "%f ", rhs[v][i+3]);
                fprintf(stderr, "\n");
            }
	}
	
    int delete_list()
	{
            _nof_points = 0;
            type = VIZ_GEOREF_SPLINE_ZERO_POINTS;
            if ( _AA )
            {
                CPLFree(_AA);
                _AA = NULL;
            }
            if ( _Ainv )
            {
                CPLFree(_Ainv);
                _Ainv = NULL;
            }
            return _nof_points;
	}
#endif

    void grow_points();
    int add_point( const double Px, const double Py, const double *Pvars );
    int get_point( const double Px, const double Py, double *Pvars );
#if 0
    int delete_point(const double Px, const double Py );
    bool get_xy(int index, double& x, double& y);
    bool change_point(int index, double x, double y, double* Pvars);
    void reset(void) { _nof_points = 0; }
#endif
    int solve(void);

  private:	

    vizGeorefInterType type;

    int _nof_vars;
    int _nof_points;
    int _max_nof_points;
    int _nof_eqs;

    double _tx, _ty;
    double _ta;
    double _dx, _dy;

    double *x; // [VIZ_GEOREF_SPLINE_MAX_POINTS+3];
    double *y; // [VIZ_GEOREF_SPLINE_MAX_POINTS+3];

//    double rhs[VIZ_GEOREF_SPLINE_MAX_POINTS+3][VIZGEOREF_MAX_VARS];
//    double coef[VIZ_GEOREF_SPLINE_MAX_POINTS+3][VIZGEOREF_MAX_VARS];
    double *rhs[VIZGEOREF_MAX_VARS];
    double *coef[VIZGEOREF_MAX_VARS];

    double *u; // [VIZ_GEOREF_SPLINE_MAX_POINTS];
    int *unused; // [VIZ_GEOREF_SPLINE_MAX_POINTS];
    int *index; // [VIZ_GEOREF_SPLINE_MAX_POINTS];
};

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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2004/11/14 04:16:44  fwarmerdam
 * New
 *
 */

#include "gdal_alg.h"

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

#define VIZ_GEOREF_SPLINE_MAX_POINTS 40
#define VIZGEOREF_MAX_VARS 2

class VizGeorefSpline2D
{
 public:

	VizGeorefSpline2D(int nof_vars = 1){
		_tx = _ty = 0.0;		
		_ta = 10.0;
		_nof_points = 0;
		_nof_vars = nof_vars;
		_AA = NULL;
		_Ainv = NULL;
		for ( int v = 0; v < _nof_vars; v++ )
			for ( int i = 0; i < 3; i++ )
				rhs[i][v] = 0.0;
		type = VIZ_GEOREF_SPLINE_ZERO_POINTS;
	}

	~VizGeorefSpline2D(){
		if ( _AA )
			delete _AA;
		if ( _Ainv )
			delete _Ainv;
	}

	int get_nof_points(){
		return _nof_points;
	}

	void set_toler( float tx, float ty ){
		_tx = tx;
		_ty = ty;
	}

	void get_toler( float& tx, float& ty) {
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
				fprintf(stderr, "%f ", rhs[i+3][v]);
			fprintf(stderr, "\n");
		}
	}
	int delete_list()
	{
		_nof_points = 0;
		type = VIZ_GEOREF_SPLINE_ZERO_POINTS;
		if ( _AA )
		{
			delete _AA;
			_AA = NULL;
		}
		if ( _Ainv )
		{
			delete _Ainv;
			_Ainv = NULL;
		}
		return _nof_points;
	}

	int add_point( const float Px, const float Py, const float *Pvars );
	int delete_point(const float Px, const float Py );
	int get_point( const float Px, const float Py, float *Pvars );
	bool get_xy(int index, float& x, float& y);
	bool change_point(int index, float x, float y, float* Pvars);
	void reset(void) { _nof_points = 0; }
	int solve(void);

private:	
	float base_func( const float x1, const float y1,
					 const float x2, const float y2 );

	vizGeorefInterType type;

	int _nof_vars;
	int _nof_points;
	int _nof_eqs;

	float _tx, _ty;
	float _ta;
	float _dx, _dy;

	float x[VIZ_GEOREF_SPLINE_MAX_POINTS+3];
	float y[VIZ_GEOREF_SPLINE_MAX_POINTS+3];

	float rhs[VIZ_GEOREF_SPLINE_MAX_POINTS+3][VIZGEOREF_MAX_VARS];
	float coef[VIZ_GEOREF_SPLINE_MAX_POINTS+3][VIZGEOREF_MAX_VARS];

	float u[VIZ_GEOREF_SPLINE_MAX_POINTS];
	int unused[VIZ_GEOREF_SPLINE_MAX_POINTS];
	int index[VIZ_GEOREF_SPLINE_MAX_POINTS];
	
	float *_AA, *_Ainv;
};



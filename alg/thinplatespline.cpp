/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Warp API
 * Purpose:  Implemenentation of 2D Thin Plate Spline transformer. 
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

#include "thinplatespline.h"
#include <values.h>

VizGeorefSpline2D* viz_xy2llz;
VizGeorefSpline2D* viz_llz2xy;

#ifdef notdef
bool PICTURE_GEO_DATA::importVizSplineFile(char* vizSplineFileName, 
	long image_xsize, long image_ysize)
{
	bool ok = false;
	VizGeorefSpline2D *viz_xy2llz = NULL;
	VizGeorefSpline2D *viz_llz2xy = NULL;

	if ( !vizGeorefSpline_xy2llz )
	{
		viz_xy2llz = new VizGeorefSpline2D(VIZGEOREF_MAX_VARS);
		vizGeorefSpline_xy2llz = (void *)viz_xy2llz;
	}
	else
		viz_xy2llz = (VizGeorefSpline2D *)vizGeorefSpline_xy2llz;

	if ( !vizGeorefSpline_llz2xy )
	{
		viz_llz2xy = new VizGeorefSpline2D(VIZGEOREF_MAX_VARS);
		vizGeorefSpline_llz2xy = (void *)viz_llz2xy;
	}
	else
		viz_llz2xy = (VizGeorefSpline2D *)vizGeorefSpline_llz2xy;


	// Add the points from the .vizspline file (x, y, lon, lat)
	FILE* fp = fopen(vizSplineFileName, "r");
	if ( fp )
	{
		ok = true;

		while ( !feof(fp) )
		{
			char line[1000];
			float xy[2], lonlat[2];

			fgets(line, sizeof(line), fp);
			int nargs = sscanf(line, "%f %f %f %f", &xy[0], &xy[1], &lonlat[0], &lonlat[1]);
			if ( nargs == 4 )
			{
				// Add the point
				viz_xy2llz->add_point(xy[0], xy[1], lonlat);
				viz_llz2xy->add_point(lonlat[0], lonlat[1], xy);
			}
		}

		// The file must have at least 3 points!
		int nof_points = viz_xy2llz->get_nof_points();
		if ( nof_points >= 3 )
		{
			ok = true;
			viz_xy2llz->solve();
			viz_llz2xy->solve();
		}

		fclose(fp);
	}

	return( ok );
}

bool PICTURE_GEO_DATA::llz2xyVizSpline(long image_xsize, long image_ysize, 
		double longitude, double latitude, double height, float& x, float& y, float& z)
{
	bool ok = false;

	if ( vizGeorefSpline_llz2xy )
	{
		// Translate lon/lat to xy. xx,yy are in [0,1]
		float xx, yy, xy[VIZGEOREF_MAX_VARS];

		VizGeorefSpline2D* viz_llz2xy = (VizGeorefSpline2D*)vizGeorefSpline_llz2xy;
		viz_llz2xy->get_point(longitude, latitude, xy);
		xx = xy[0];
		yy = xy[1];

		// We are using "built-in image geometry" from viz :
		//   ar = height/width; bbox = (-50,-50*ar, 50, 50*ar)
		float aspectRatio = ((float)image_ysize)/((float)image_xsize);
		float minx = -50.0;
		float maxx =  50.0;
		float sizex = maxx - minx;
		float miny = -50 * aspectRatio;
		float maxy =  50 * aspectRatio;
		float sizey = maxy - miny;

		// Map from xx in [0,1] to x in [-50,50] 
		x = (xx - 0.5) * sizex;

		// Map from yy in [0,1] to y in [50*ar,-50*ar] 
		y = (0.5 - yy) * sizey;

		z = 0;
		ok = true;
	}


	return( ok );
}

void PICTURE_GEO_DATA::create_new_custom(void)
{
	VizGeorefSpline2D *viz_xy2llz;
	if (!vizGeorefSpline_xy2llz)
	{
		viz_xy2llz = new VizGeorefSpline2D(VIZGEOREF_MAX_VARS);
		vizGeorefSpline_xy2llz = (void *)viz_xy2llz;
	}
	else
	{
		viz_xy2llz = (VizGeorefSpline2D*)vizGeorefSpline_xy2llz;
		viz_xy2llz->reset();
	}

	VizGeorefSpline2D *viz_llz2xy;
	if (!vizGeorefSpline_llz2xy)
	{
		viz_llz2xy = new VizGeorefSpline2D(VIZGEOREF_MAX_VARS);
		vizGeorefSpline_llz2xy = (void *)viz_llz2xy;
	}
	else
	{
		viz_llz2xy = (VizGeorefSpline2D*)vizGeorefSpline_llz2xy;
		viz_llz2xy->reset();
	}

	// Add 4 points
	float xy[VIZGEOREF_MAX_VARS], lonlat[VIZGEOREF_MAX_VARS];
	xy[0] = 0.0f; xy[1] = 0.0f; lonlat[0] = 0.0f; lonlat[1] = 0.0f;
	viz_xy2llz->add_point(xy[0], xy[1], lonlat);
	viz_llz2xy->add_point(lonlat[0], lonlat[1], xy);

	xy[0] = 0.0f; xy[1] = 1.0f; lonlat[0] = 0.0f; lonlat[1] = 10.0f;
	viz_xy2llz->add_point(xy[0], xy[1], lonlat);
	viz_llz2xy->add_point(lonlat[0], lonlat[1], xy);

	xy[0] = 1.0f; xy[1] = 0.0f; lonlat[0] = 10.0f; lonlat[1] = 0.0f;
	viz_xy2llz->add_point(xy[0], xy[1], lonlat);
	viz_llz2xy->add_point(lonlat[0], lonlat[1], xy);

	xy[0] = 1.0f; xy[1] = 1.0f; lonlat[0] = 10.0f; lonlat[1] = 10.0f;
	viz_xy2llz->add_point(xy[0], xy[1], lonlat);
	viz_llz2xy->add_point(lonlat[0], lonlat[1], xy);

	viz_llz2xy->solve();
	viz_xy2llz->solve();
}

bool PICTURE_GEO_DATA::xy2llzVizSpline(long image_xsize, long image_ysize, 
		float x, float y, float z, double& longitude, double& latitude, double& height)
{
	bool ok = false;

	// Should be the inverse from rs2llzVizSpline() above 
	if ( vizGeorefSpline_xy2llz )
	{
		VizGeorefSpline2D* viz_xy2llz = (VizGeorefSpline2D*)vizGeorefSpline_xy2llz;

		// We are using "built-in image geometry" from viz :
		//   ar = height/width; bbox = (-50,-50*ar, 50, 50*ar)
		float aspectRatio = ((float)image_ysize)/((float)image_xsize);
		float minx = -50.0;
		float maxx =  50.0;
		float sizex = maxx - minx;
		float miny = -50 * aspectRatio;
		float maxy =  50 * aspectRatio;
		float sizey = maxy - miny;

		// Map from x in [-50,50] to xx in [0,1]
		float xx = (x - minx) / sizex;

		// Map from y in [50*ar,-50*ar] to yy in [0,1]
		float yy = (-y - miny) / sizey;

		// Translate xx,yy in [0,1] to lon/lat.
		float lonlat[VIZGEOREF_MAX_VARS];
		viz_xy2llz->get_point(xx, yy, lonlat);
		longitude = lonlat[0];
		latitude = lonlat[1];
		height = 0.0f;
		ok = true;
	}

	return( ok );
}

bool PICTURE_GEO_DATA::viz_custom_cmd(long cmd, int index, double& x, double& y, double& lon, double& lat, long& status)
{
	float xy[VIZGEOREF_MAX_VARS], lonlat[VIZGEOREF_MAX_VARS];
	float fx, fy, flon, flat;
	bool isCustomGeoref = false;
	bool isSolve = status;
	status = 0;

	if ( vizGeorefSpline_llz2xy && vizGeorefSpline_llz2xy )
	{
		isCustomGeoref = true;
		VizGeorefSpline2D* viz_xy2llz = (VizGeorefSpline2D*)vizGeorefSpline_xy2llz;
		VizGeorefSpline2D* viz_llz2xy = (VizGeorefSpline2D*)vizGeorefSpline_llz2xy;

		switch (cmd)
		{
			case 'GETN': // Get number of points
				status = viz_xy2llz->get_nof_points();
			break;

			case 'GETP' : // Get point
				viz_xy2llz->get_xy(index, fx, fy);
				x = fx;
				y = fy;
				viz_llz2xy->get_xy(index, flon, flat);
				lon = flon;
				lat = flat;
			break;

			case 'ADDP' : // Add point
				xy[0] = x;
				xy[1] = y;
				lonlat[0] = lon;
				lonlat[1] = lat;
				viz_xy2llz->add_point(xy[0], xy[1], lonlat);
				if ( isSolve )
					viz_xy2llz->solve();
				viz_llz2xy->add_point(lonlat[0], lonlat[1], xy);
				if ( isSolve )
					viz_llz2xy->solve();
			break;

			case 'CHNG' : // Change point
				xy[0] = x;
				xy[1] = y;
				lonlat[0] = lon;
				lonlat[1] = lat;
				viz_xy2llz->change_point(index, xy[0], xy[1], lonlat);
				if ( isSolve )
				viz_xy2llz->solve();

				viz_llz2xy->change_point(index, lonlat[0], lonlat[1], xy);
				if ( isSolve )
					viz_llz2xy->solve();
			break;

			case 'DELP' : // Delete point
				float x, y;
				viz_xy2llz->get_xy(index, x, y);
				viz_xy2llz->delete_point(x, y);
				if ( isSolve )
					viz_xy2llz->solve();
				float lon, lat;
				viz_llz2xy->get_xy(index, lon, lat);
				viz_llz2xy->delete_point(lon, lat);
				if ( isSolve )
					viz_llz2xy->solve();
			break;

			case 'SOLV' : // Recompute points
				viz_xy2llz->solve();
				viz_llz2xy->solve();
			break;

			case 'RESE' : // Reset all points
				viz_xy2llz->reset();
				viz_llz2xy->reset();
			break;
		}
	}

	return( isCustomGeoref );
}

#endif

/////////////////////////////////////////////////////////////////////////////////////
//// vizGeorefSpline2D
/////////////////////////////////////////////////////////////////////////////////////

#define A(r,c) _AA[ _nof_eqs * (r) + (c) ]
#define Ainv(r,c) _Ainv[ _nof_eqs * (r) + (c) ]


#define VIZ_GEOREF_SPLINE_DEBUG 0

int matrixInvert( int N, float input[], float output[] );

int VizGeorefSpline2D::add_point( const float Px, const float Py, const float *Pvars )
{
	type = VIZ_GEOREF_SPLINE_POINT_WAS_ADDED;
	int i;
#if 0
	for ( i = 0; i < _nof_points; i++ )
	{
		if ( ( fabs(Px - x[i]) <= _tx ) && ( fabs(Py - y[i]) <= _ty ) )
		{
			// there is such point, replace it
			x[i] = Px;
			y[i] = Py;
			for ( int j = 0; j < _nof_vars; j++ )
				rhs[i+3][j] = Pvars[j];
			return 0;
		}
	}
#endif
	i = _nof_points;
	//A new point is added
	x[i] = Px;
	y[i] = Py;
	for ( int j = 0; j < _nof_vars; j++ )
		rhs[i+3][j] = Pvars[j];
	_nof_points++;
	if ( _nof_points >= VIZ_GEOREF_SPLINE_MAX_POINTS )
		_nof_points = VIZ_GEOREF_SPLINE_MAX_POINTS - 1;
	return 1;
}

bool VizGeorefSpline2D::change_point(int index, float Px, float Py, float* Pvars)
{
	if ( index < _nof_points )
	{
		int i = index;
		x[i] = Px;
		y[i] = Py;
		for ( int j = 0; j < _nof_vars; j++ )
			rhs[i+3][j] = Pvars[j];
	}

	return( true );
}

bool VizGeorefSpline2D::get_xy(int index, float& outX, float& outY)
{
	bool ok;

	if ( index < _nof_points && index < VIZ_GEOREF_SPLINE_MAX_POINTS )
	{
		ok = true;
		outX = x[index];
		outY = y[index];
	}
	else
	{
		ok = false;
		outX = outY = 0.0f;
	}

	return(ok);
}

int VizGeorefSpline2D::delete_point(const float Px, const float Py )
{
	for ( int i = 0; i < _nof_points; i++ )
	{
		if ( ( fabs(Px - x[i]) <= _tx ) && ( fabs(Py - y[i]) <= _ty ) )
		{
			for ( int j = i; j < _nof_points - 1; j++ )
			{
				x[j] = x[j+1];
				y[j] = y[j+1];
				for ( int k = 0; k < _nof_vars; k++ )
					rhs[j+3][k] = rhs[j+3+1][k];
			}
			_nof_points--;
			type = VIZ_GEOREF_SPLINE_POINT_WAS_DELETED;
			return(1);
		}
	}
	return(0);
}

int VizGeorefSpline2D::solve(void)
{
	int r, c, v;
	int p;
	
	//	No points at all
	if ( _nof_points < 1 )
	{
		type = VIZ_GEOREF_SPLINE_ZERO_POINTS;
		return(0);
	}
	
	// Only one point
	if ( _nof_points == 1 )
	{
		type = VIZ_GEOREF_SPLINE_ONE_POINT;
		return(1);
	}
	// Just 2 points - it is necessarily 1D case
	if ( _nof_points == 2 )
	{
		_dx = x[1] - x[0];
		_dy = y[1] - y[0];	 
		float fact = 1.0 / ( _dx * _dx + _dy * _dy );
		_dx *= fact;
		_dy *= fact;
		
		type = VIZ_GEOREF_SPLINE_TWO_POINTS;
		return(2);
	}
	
	// More than 2 points - first we have to check if it is 1D or 2D case
		
	float xmax = FLT_MIN, xmin = FLT_MAX, ymax = FLT_MIN, ymin = FLT_MAX;
	float delx, dely;
	float xx, yy;
	float sumx = 0.0f, sumy= 0.0f, sumx2 = 0.0f, sumy2 = 0.0f, sumxy = 0.0f;
	float SSxx, SSyy, SSxy;
	
	for ( p = 0; p < _nof_points; p++ )
	{
		xx = x[p];
		yy = y[p];
		
		xmax = MAX( xmax, xx );
		xmin = MIN( xmin, xx );
		ymax = MAX( ymax, yy );
		ymin = MIN( ymin, yy );
		
		sumx  += xx;
		sumx2 += xx * xx;
		sumy  += yy;
		sumy2 += yy * yy;
		sumxy += xx * yy;
	}
	delx = xmax - xmin;
	dely = ymax - ymin;
	
	SSxx = sumx2 - sumx * sumx / _nof_points;
	SSyy = sumy2 - sumy * sumy / _nof_points;
	SSxy = sumxy - sumx * sumy / _nof_points;
	
	if ( delx < 0.001 * dely || dely < 0.001 * delx || 
		fabs ( SSxy * SSxy / ( SSxx * SSyy ) ) > 0.99 )
	{
		int p1;
		
		type = VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL;
		
		_dx = _nof_points * sumx2 - sumx * sumx;
		_dy = _nof_points * sumy2 - sumy * sumy;
		float fact = 1.0 / sqrt( _dx * _dx + _dy * _dy );
		_dx *= fact;
		_dy *= fact;
		
		for ( p = 0; p < _nof_points; p++ )
		{
			float dxp = x[p] - x[0];
			float dyp = y[p] - y[0];
			u[p] = _dx * dxp + _dy * dyp;
			unused[p] = 1;
		}
		
		for ( p = 0; p < _nof_points; p++ )
		{
			int min_index = -1;
			float min_u;
			for ( p1 = 0; p1 < _nof_points; p1++ )
			{
				if ( unused[p1] )
				{
					if ( min_index < 0 || u[p1] < min_u )
					{
						min_index = p1;
						min_u = u[p1];
					}
				}
			}
			index[p] = min_index;
			unused[min_index] = 0;
		}
		
		return(3);
	}
	
	type = VIZ_GEOREF_SPLINE_FULL;
	// Make the necessary memory allocations
	if ( _AA )
		delete _AA;
	if ( _Ainv )
		delete _Ainv;
	
	_nof_eqs = _nof_points + 3;
	
	_AA = ( float * )calloc( _nof_eqs * _nof_eqs, sizeof( float ) );
	_Ainv = ( float * )calloc( _nof_eqs * _nof_eqs, sizeof( float ) );
	
	// Calc the values of the matrix A
	for ( r = 0; r < 3; r++ )
		for ( c = 0; c < 3; c++ )
			A(r,c) = 0.0;
		
		for ( c = 0; c < _nof_points; c++ )
		{
			A(0,c+3) = 1.0;
			A(1,c+3) = x[c];
			A(2,c+3) = y[c];
			
			A(c+3,0) = 1.0;
			A(c+3,1) = x[c];
			A(c+3,2) = y[c];
		}
		
		for ( r = 0; r < _nof_points; r++ )
			for ( c = r; c < _nof_points; c++ )
			{
				A(r+3,c+3) = base_func( x[r], y[r], x[c], y[c] );
				if ( r != c )
					A(c+3,r+3 ) = A(r+3,c+3);
			}
			
#if VIZ_GEOREF_SPLINE_DEBUG
			
			for ( r = 0; r < _nof_eqs; r++ )
			{
				for ( c = 0; c < _nof_eqs; c++ )
					fprintf(stderr, "%f", A(r,c));
				fprintf(stderr, "\n");
			}
			
#endif
			
			// Invert the matrix
			int status = matrixInvert( _nof_eqs, _AA, _Ainv );
			
			if ( !status )
			{
				fprintf(stderr, " There is a problem to invert the interpolation matrix\n");
				return 0;
			}
			
			// calc the coefs
			for ( v = 0; v < _nof_vars; v++ )
				for ( r = 0; r < _nof_eqs; r++ )
				{
					coef[r][v] = 0.0;
					for ( c = 0; c < _nof_eqs; c++ )
						coef[r][v] += Ainv(r,c) * rhs[c][v];
				}
				
				return(4);
}

int VizGeorefSpline2D::get_point( const float Px, const float Py, float *vars )
{
	int v, r;
	float tmp, Pu;
	float fact;
	int leftP, rightP, found = 0;
	
	switch ( type )
	{
	case VIZ_GEOREF_SPLINE_ZERO_POINTS :
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = 0.0;
		break;
	case VIZ_GEOREF_SPLINE_ONE_POINT :
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = rhs[3][v];
		break;
	case VIZ_GEOREF_SPLINE_TWO_POINTS :
		fact = _dx * ( Px - x[0] ) + _dy * ( Py - y[0] );
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = ( 1 - fact ) * rhs[3][v] + fact * rhs[4][v];
		break;
	case VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL :
		Pu = _dx * ( Px - x[0] ) + _dy * ( Py - y[0] );
		if ( Pu <= u[index[0]] )
		{
			leftP = index[0];
			rightP = index[1];
		}
		else if ( Pu >= u[index[_nof_points-1]] )
		{
			leftP = index[_nof_points-2];
			rightP = index[_nof_points-1];
		}
		else
		{
			for ( r = 1; !found && r < _nof_points; r++ )
			{
				leftP = index[r-1];
				rightP = index[r];					
				if ( Pu >= u[leftP] && Pu <= u[rightP] )
					found = 1;
			}
		}
		
		fact = ( Pu - u[leftP] ) / ( u[rightP] - u[leftP] );
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = ( 1.0 - fact ) * rhs[leftP+3][v] +
			fact * rhs[rightP+3][v];
		break;
	case VIZ_GEOREF_SPLINE_FULL :
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = coef[0][v] + coef[1][v] * Px + coef[2][v] * Py;
		
		for ( r = 0; r < _nof_points; r++ )
		{
			tmp = base_func( Px, Py, x[r], y[r] );
			for ( v= 0; v < _nof_vars; v++ )
				vars[v] += coef[r+3][v] * tmp;
		}
		break;
	case VIZ_GEOREF_SPLINE_POINT_WAS_ADDED :
		fprintf(stderr, " A point was added after the last solve\n");
		fprintf(stderr, " NO interpolation - return values are zero\n");
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = 0.0;
		return(0);
		break;
	case VIZ_GEOREF_SPLINE_POINT_WAS_DELETED :
		fprintf(stderr, " A point was deleted after the last solve\n");
		fprintf(stderr, " NO interpolation - return values are zero\n");
		for ( v = 0; v < _nof_vars; v++ )
			vars[v] = 0.0;
		return(0);
		break;
	default :
		return(0);
		break;
	}
	return(1);
}

float VizGeorefSpline2D::base_func( const float x1, const float y1,
						  const float x2, const float y2 )
{
	if ( ( x1 == x2 ) && (y1 == y2 ) )
		return 0.0;
	
	float dist  = ( x2 - x1 ) * ( x2 - x1 ) + ( y2 - y1 ) * ( y2 - y1 );
	
	return dist * log( dist );	
}

int matrixInvert( int N, float input[], float output[] )
{
	// Receives an array of dimension NxN as input.  This is passed as a one-
	// dimensional array of N-squared size.  It produces the inverse of the
	// input matrix, returned as output, also of size N-squared.  The Gauss-
	// Jordan Elimination method is used.  (Adapted from a BASIC routine in
	// "Basic Scientific Subroutines Vol. 1", courtesy of Scott Edwards.)
	
	// Array elements 0...N-1 are for the first row, N...2N-1 are for the
	// second row, etc.
	
	// We need to have a temporary array of size N x 2N.  We'll refer to the
	// "left" and "right" halves of this array.
	
	int row, col;
	
#if 0
	fprintf(stderr, "Matrix Inversion input matrix (N=%d)\n", N);
	for ( row=0; row<N; row++ )
	{
		for ( col=0; col<N; col++ )
		{
			fprintf(stderr, "%5.2f ", input[row*N + col ]  );
		}
		fprintf(stderr, "\n");
	}
#endif
	
	int tempSize = 2 * N * N;
	float* temp = (float*) new float[ tempSize ];
	float ftemp;
	
	if (temp == 0) {
		
		fprintf(stderr, "matrixInvert(): ERROR - memory allocation failed.\n");
                return false;
	}
	
	// First create a double-width matrix with the input array on the left
	// and the identity matrix on the right.
	
	for ( row=0; row<N; row++ )
	{
		for ( col=0; col<N; col++ )
		{
			// Our index into the temp array is X2 because it's twice as wide
			// as the input matrix.
			
			temp[ 2*row*N + col ] = input[ row*N+col ];	// left = input matrix
			temp[ 2*row*N + col + N ] = 0.0f;			// right = 0
		}
		temp[ 2*row*N + row + N ] = 1.0f;		// 1 on the diagonal of RHS
	}
	
	// Now perform row-oriented operations to convert the left hand side
	// of temp to the identity matrix.  The inverse of input will then be
	// on the right.
	
	int max;
	int k=0;
    for (k = 0; k < N; k++)
	{
        if (k+1 < N)	// if not on the last row
		{              
            max = k;
            for (row = k+1; row < N; row++) // find the maximum element
			{  
                if (fabs( temp[row*2*N + k] ) > fabs( temp[max*2*N + k] ))
				{
                    max = row;
                }
            }
			
            if (max != k)	// swap all the elements in the two rows
			{        
                for (col=k; col<2*N; col++)
				{
                    ftemp = temp[k*2*N + col];
                    temp[k*2*N + col] = temp[max*2*N + col];
                    temp[max*2*N + col] = ftemp;
                }
            }
        }
		
		ftemp = temp[ k*2*N + k ];
		if ( ftemp == 0.0f ) // matrix cannot be inverted
		{
			delete temp;
			return false;
		}
		
		for ( col=k; col<2*N; col++ )
		{
			temp[ k*2*N + col ] /= ftemp;
		}
		
		for ( row=0; row<N; row++ )
		{
			if ( row != k )
			{
				ftemp = temp[ row*2*N + k ];
				for ( col=k; col<2*N; col++ ) 
				{
					temp[ row*2*N + col ] -= ftemp * temp[ k*2*N + col ];
				}
			}
		}
	}
	
	// Retrieve inverse from the right side of temp
	
    for (row = 0; row < N; row++)
	{
        for (col = 0; col < N; col++)
		{
            output[row*N + col] = temp[row*2*N + col + N ];
        }
    }
	
#if 0
	fprintf(stderr, "Matrix Inversion result matrix:\n");
	for ( row=0; row<N; row++ )
	{
		for ( col=0; col<N; col++ )
		{
			fprintf(stderr, "%5.2f ", output[row*N + col ]  );
		}
		fprintf(stderr, "\n");
	}
#endif
	
	delete [] temp;       // free memory
	return true;
}

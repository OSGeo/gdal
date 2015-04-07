#include "csf.h"
#include "csfimpl.h"
#include <math.h>

/* round up and down come up with a number such
 * we have rounded to integer multiplication
 * anyway this should hold:
	POSTCOND(RoundUp( 5.3, 4) == 8 );
	POSTCOND(RoundUp( 4  , 4) == 8 );
	POSTCOND(RoundUp(-5.3, 4) == -4 );
	POSTCOND(RoundUp(-4, 4) == 0 );
	POSTCOND(RoundDown( 5.3, 4)  ==  4 );
	POSTCOND(RoundDown( 4  , 4)  ==  0 );
	POSTCOND(RoundDown(-5.3, 4)  == -8 );
	POSTCOND(RoundDown(-4  , 4)  == -8 );
 */

static double RoundDown(
	double v,
	double round)
{
	double rVal = fmod(v, round);
	double x;
	if(rVal == 0)
		return v-round;
	if (v < 0)
		x = v-round-rVal;
	else
		x = v-rVal;
	return  x;
}

static double RoundUp(
	double v,
	double round)
{
	double rVal = fmod(v, round);
	if(rVal == 0)
		return v+round;
	if (v < 0)
	  return v-rVal;
	else
	  return v+round-rVal; 
}

/* compute (xUL,yUL) and nrRows, nrCols from some coordinates
 * RcomputeExtend computes parameters to create a raster maps
 * from minimum and maximum x and y coordinates, projection information,
 * cellsize and units. The resulting parameters are computed that the 
 * smallest raster map can be created that will include the two 
 * coordinates given, assuming a default angle of 0.
 * Which coordinates are the maximum or minimum are
 * determined by the function itself.
 */
void RcomputeExtend(
	REAL8 *xUL,     /* write-only, resulting xUL */
	REAL8 *yUL,     /* write-only, resulting yUL */
	size_t *nrRows, /* write-only, resulting nrRows */
	size_t *nrCols, /* write-only, resulting nrCols */
	double x_1,      /* first x-coordinate */ 
	double y_1,      /* first y-coordinate */
	double x_2,      /* second x-coordinate */
	double y_2,      /* second y-coordinate */
	CSF_PT projection, /* required projection */
	REAL8 cellSize, /* required cellsize, > 0 */
	double rounding) /* assure that (xUL/rounding), (yUL/rouding)
	                  * (xLL/rounding) and (yLL/rounding) will
	                 * will all be an integers values > 0 
	                 */
{
    /*
     * xUL ______
	   |    |
	   |    |
	   |    |
	   ------

     */
	double yLL,xUR = x_1 > x_2 ? x_1 : x_2;
	*xUL = x_1 < x_2 ? x_1 : x_2;
	*xUL = RoundDown(*xUL, rounding); /* Round down */
	xUR  = RoundUp(   xUR, rounding); /* Round up */
	POSTCOND(*xUL <= xUR);
	*nrCols = (size_t)ceil((xUR - *xUL)/cellSize);
	if (projection == PT_YINCT2B)
	{
		 yLL = y_1 > y_2 ? y_1 : y_2;  /* highest value at bottom */
		*yUL = y_1 < y_2 ? y_1 : y_2;  /* lowest value at top */
	        *yUL = RoundDown(*yUL, rounding);
	         yLL = RoundUp(   yLL, rounding);
	}
	else
	{
		 yLL = y_1 < y_2 ? y_1 : y_2;  /* lowest value at bottom */
		*yUL = y_1 > y_2 ? y_1 : y_2;  /* highest value at top */
	        *yUL = RoundUp(  *yUL, rounding);
	         yLL = RoundDown( yLL, rounding);
	}
	*nrRows = (size_t)ceil(fabs(yLL - *yUL)/cellSize);
}


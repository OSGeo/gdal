
#include "gvgcpfit.h"

/****************************************************************

static 
void *AllocVector( int nl, int nh, int num_chars_per_element )

This function allocates a block of memory and returns a generic ptr to it. If
m is the pointer returned, the memory can be referenced as m[ nl ] to m[  nh ].

INPUTS:     int nl -- minimum valid index for memory corresponding to the ptr
                returned.
            int nh -- maximum valid index for memory corresponding to the ptr
                returned.
            int num_chars_per_element -- vector element size (in bytes).
            
OUTPUTS:    none.

RETURNS:    void * -- generic ptr to the memory allocated, using the indexing
                requested through nl and nh.

CALLS:      handle_error( )
            
CALLED BY:  InitializeSphere( )
            SaveScatMatrix( )

****************************************************************/

static void *AllocVector( int nl, int nh, int num_chars_per_element )
{
    unsigned char *v;

    if ( nh < nl )
    {
        return NULL;
    }

    if ( num_chars_per_element <= 0 )
    {
        return NULL;
    }

    v = ( unsigned char * ) malloc( ( unsigned ) ( nh - nl + 1 ) * num_chars_per_element );
    if ( !v )
    {
        return NULL;
    }
    else
    {
        return ( void * ) ( v - nl * num_chars_per_element );
    }
}

/**********************************************************************

void DeallocVector( void *v, int nl, int nh, int num_chars_per_element )

This function frees memory previously allocated by AllocVector( ).

INPUTS:     void *v -- ptr to the memory to be deallocated.
            int nl -- lower bound of index of the ptr v.
            int nh -- upper bound of index of the ptr v.
            int num_chars_per_element -- vector element size (in bytes).

OUTPUTS:    none.
            
RETURNS:    none.

CALLS:      none.

CALLED BY:  SaveScatMatrix( )
            DisposeSphere( )

**********************************************************************/
         
static void DeallocVector( void *v, int nl, int nh, int num_chars_per_element )
{
    if ( nh >= nl && v != NULL )
    {
        free( ( unsigned char* ) v + nl * num_chars_per_element );
    }
}


static
void **AllocMatrix( int nrl, int nrh, int ncl, int nch, int num_chars_per_element )
{
    int i, j;
    unsigned char **m;

    if ( nrh < nrl || nch < ncl )
    {
        return NULL;
    }

    if ( num_chars_per_element <= 0 )
    {
        return NULL;
    }

    m = ( unsigned char ** ) malloc( ( unsigned ) ( nrh - nrl + 1 ) * sizeof( unsigned char * ) );
    if ( !m )
    {
        return NULL;
    }
    m -= nrl;

    for( i = nrl ; i <= nrh ; i++ )
    {
        m[ i ] = AllocVector( ncl, nch, num_chars_per_element );
        if ( !m[ i ] )
        {
            for ( j = i - 1 ; j >= nrl ; j-- )
            {
                DeallocVector( m[ j ], ncl, nch, num_chars_per_element );
            }
            return NULL;
        }
    }
    return ( void ** ) m;
}



/**********************************************************************

void DeallocMatrix( void **m, int nrl, int nrh, int ncl, int nch, int num_chars_per_element )

This function frees memory previously allocated by AllocMatrix( ).

INPUTS:     void **m -- ptr to the array of ptrs to memory to be deallocated.
            int nrl -- lower bound of row index of the ptr m.
            int nrh -- upper bound of row index of the ptr m.
            int ncl -- lower bound of column index of the ptr m.
            int nch -- upper bound of column index of the ptr m.
            int num_chars_per_element -- matrix element size (in bytes).

OUTPUTS:    none.
            
RETURNS:    none.

CALLS:      none.

CALLED BY:  main( )
            DeallocScatterData( )
            InvokePoincareSpherePlot( )

**********************************************************************/
static          
void DeallocMatrix( void **m, int nrl, int nrh, int ncl, int nch, int num_chars_per_element )
{
    int i;

    if ( nrh >= nrl && nch >= ncl && m != NULL )
    {
        for ( i = nrh ; i >= nrl ; i-- )
        {
            free( ( unsigned char * ) m[ i ] + ncl * num_chars_per_element );
        }
        free( ( unsigned char* ) ( m + nrl ) );
    }
}

/*------------------------- Start of file polyfit.c ----------------------*/


/* -----------------------------------------------------------------


        Function: Polynomial fit library utilities.

        Created : 12 September 1995
        Author : Jim Ehrismann, Andy Smith
        Revised: Bernie Armour January 14 1996

        General comments: ALL data & functions are DOUBLE PRECISION.

             - Bernie Armour March 15 1996:  The TwoDPolyGradFit()
               function does not have normalization of f,x,y.  This
               will effect accuracy for values of f,x, y outside the
               nominal range of f = [0,100], x,y = [0, 10000].

   -----------------------------------------------------------------
*/


/*
 *  Local defines.
 */

#define SVD_TOL_POLY_GRAD_FIT 1.0e-15  /* Used in back substitution routines. */
#define SVD_TOL_POLY_FIT      1.0e-9   /* For 2-D f(x,y) functions. */
#define SVD_TOL_POLY_FIT_3D   1.0e-9   /* For 3-D f(x,y) functions. */

#define TINY  1.0e-30  /* Tiny value for min. float value. */

/*
 *  Local function prototypes.
 */
LOCAL void GetOneDPowerCoefficients(double *coeff, int order, double x );
LOCAL void GetTwoDPowerCoefficients(double *coeff, int order, double x, double y);
LOCAL void GetGradxCoefficients(double *coeff, int order, double x, double y);
LOCAL void GetGradyCoefficients(double *coeff, int order, double x, double y);
LOCAL void GetThreeDPowerCoefficients(double *coeff, int order, double x, double y, double z);

/* -------------------------------------------------------------------
 *
 * Name: OneDPolyFit
 *
 * Purpose: Generate a least squares fit of the f(x) samples to
 *  a 1-D polynomial of order 1=linear, 2=quadratic, 3=cubic etc..
 *
 * Description:
 *
 *   This routine fits a line of order <fit_order>
 *   - (fit order = 1 - linear, 2 - quadratic, 3 - cubic etc.)
 *   to the function values provided in f-array, as a function of x.
 *
 *   The fitted surface is defined as:
 *
 *   f(x) = a0 + a1.x + a2.(x*x) + a3.(x*x*x) + etc.
 *
 *   An rms error value is computed and returned.
 *
 *   A status of ABORT is returned if a fit is infeasible or unsatisfactory.
 *
 *   The least square fit is achieved by use of svdcmp
 *   - we regard the coefficients 'a' as the variables to be solved for,
 *   and provide svdcmp with the overdetermined set of equations
 *
 *   f1 = a0 + a1x1 + a2x1 ...
 *   f2 = a0 + a1x2 + a2x2 ...
 *   f3 = a0 + a1x3 + a2x3 ...
 *   etc..
 *
 *   The singular value decomposition algorithm provides a robust least
 *   square fit as a solution when asked to solve
 *   an overdetermined problem Ax = b. See section 2.6 of Numerical Recipes.
 *
 *
 * Input Parameters:
 *
 *      fit_order  - order of polynomial fit 1=bilinear, 2=biquadratic,...
 *      no_samples - number of samples in the f, x, and y arrays
 *      f_array - points to array of function values in f(x,y)
 *      x_array - points to array of x values
 *      y_array - points to array of y values
 *
 * Output Parameters:
 *      rms_error - the RMS error in the polynomial surface fit.
 *      coeffs_array - the array of polynomial coefficients
 *
 * Returns: SUCCESS or ABORT in the case of an error.
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Jim Ehrismann
 *
 *     April 7 1997 : created  (based on the as-of-today current TwoDPolyFit())
 *
 * -------------------------------------------------------------------
 */
EXTERNAL int OneDPolyFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *f_array, double *x_array )
{

    int i, j;
    int l_num_coeff;
    int l_status;

    double max_f, max_abs_x;  /* Max f, |x| */
    double min_f;             /* Min value of function f. */

    double f_scale;           /* Scale and shift f such that it is on [0,1]. */
    double f_shift;           /* Thus f' = f_scale * f + f_shift.            */


    double l_wmax, l_thresh;
    double l_tol=SVD_TOL_POLY_FIT;
    double l_x, l_f, l_ff;
    double l_sumsq_err;

    double **l_u = NULL;
    double **l_v = NULL;
    double *l_w = NULL;

    /*
     *  Initialization.
     */
    l_status = SUCCESS;
    l_num_coeff = fit_order + 1;

    if ( no_samples < l_num_coeff )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "INTERNAL ERROR: Bad call to polyfit - too few sample points." );
        return (ABORT);
    }

    l_u = (double **) AllocMatrix( 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
    l_v = (double **) AllocMatrix( 1, l_num_coeff, 1, l_num_coeff, sizeof( **l_v ) );
    l_w = (double *) AllocVector( 1, l_num_coeff, sizeof( *l_w )  );


    if ( l_u == NULL || l_v == NULL || l_w == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INTERNAL ERROR: Not enough memory to determine "
                  "polynomial coefficients in polyfit." );
        if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
        if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff,1, l_num_coeff, sizeof( **l_v ) );
        if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff, sizeof( *l_w ) );

        return (ABORT);
    }

    /*
     *   Normalize the f and x values to fall in [-1.0, 1.0].
     */
    min_f = f_array[0];
    max_f = f_array[0];
    max_abs_x = 0.0;
    for ( i = 0 ; i < no_samples ; i++ )
    {
        if ( f_array[i] > max_f ) max_f = f_array[i];
        if ( f_array[i] < min_f ) min_f = f_array[i];
        if ( fabs(x_array[i]) > max_abs_x ) max_abs_x = fabs(x_array[i]);
    }

    /*
     *   f_scale = 1.0 / ( max(f) - min(f) )  and f_shift = - min(f) * f_scale
     *   f' = f_scale * f + f_shift
     */
    if ( (max_f - min_f) < TINY )
    {
        f_scale = 1.0;
    }
    else
    {
        f_scale = 1.0 / (max_f - min_f);
    }
    f_shift = -min_f * f_scale;
    for ( i = 0 ; i < no_samples ; i++ )
    {
        f_array[i] = f_scale * f_array[i] + f_shift;
        x_array[i] /= max_abs_x;
    }

    /*-                        2            -*/
    /*- fi = a0 + a1 xi + a2 xi  + ...      -*/

    for ( i = 0 ; i < no_samples ; i++ )
    {
        GetOneDPowerCoefficients( l_u[ i + 1 ], fit_order,  x_array[ i ] );
    }

    /*
     *  perform the svd least square fit
     */
    svdcmp( l_u, no_samples, l_num_coeff, l_w, l_v );

    l_wmax=0.0;
    for ( j = 1 ; j <= l_num_coeff ; j++ )
    {
        if ( l_w[ j ] > l_wmax ) l_wmax = l_w[ j ];
    }
    l_thresh = l_tol * l_wmax;

    for ( j = 1 ; j <= l_num_coeff ; j++ )
    {
        if ( l_w[ j ] < l_thresh )
        {
            l_w[ j ] = 0.0;
        }
    }
    svbksb( l_u, l_w, l_v, no_samples, l_num_coeff, f_array - 1, coeffs_array - 1 );

    /*
     *  Return data and coefficients to their correctly non-scaled values.
     *  For the coefficients:
     *  1. Undo the shift by shifting the constant coefficient c[0] by -f_shift.
     *  2. Undo the scaling by dividing by f_scale.
     */
    for ( i = 0 ; i < no_samples ; i++ )
    {
        f_array[i] = (f_array[i] - f_shift)/f_scale;
        x_array[i] *= max_abs_x;
    }
    GetOneDPowerCoefficients( l_w, fit_order, (1.0/max_abs_x) );
    coeffs_array[0] -= f_shift;
    for ( i = 0; i < l_num_coeff; i++ ) coeffs_array[i] *=  l_w[i+1] / f_scale;

    /*
     *  compute the sum square error between fitted & supplied values
     */
    l_sumsq_err = 0.0;

    for (i = 0; i<no_samples; i++)
    {
            l_x = *(x_array+i);
            l_f = *(f_array+i);
            l_ff = OneDPolyEval(coeffs_array, fit_order, l_x );

            l_sumsq_err += (l_f - l_ff)*(l_f - l_ff);
    }
    *rms_err = sqrt(l_sumsq_err/no_samples);

    /*
     *  tidy up
     */
    if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
    if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff,1, l_num_coeff, sizeof( **l_v ) );
    if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff, sizeof( *l_w ) );

    return (l_status);
}

/* -------------------------------------------------------------------
 *
 * Name: GetOneDPowerCoefficients
 *
 * Purpose: Generates an array of values for each term x^m
 *      in the one-D polynomial.  This is a precalculation done
 *      for one of the samples f(x).
 *
 * Description: generates the x terms in the line fit for
 *      each of the samples.
 *
 * Input Parameters:
 *      x - x sample value
 *      order - polynomial order 1=linear, 2=quadratic, ...
 *
 * Output Parameters:
 *      coeff - pointer to the array containing the x^m values for the
 *          polynomial for the current x values.
 *
 * Returns: none
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Jim Ehrismann
 *
 *     April 7 1997 : created (based on the as-of-today current GetTwoDPowerCoefficients())
 *
 * -------------------------------------------------------------------
 */
LOCAL void GetOneDPowerCoefficients(double *coeff, int order, double x )
{
    int coeff_index;
    int i;


    coeff_index = 1;    /*- watch out! unit offset as required by SVD routines. -*/

    coeff[ coeff_index++ ] = 1.0;

    for ( i = 1 ; i <= order ; i++ )
    {
        coeff[ coeff_index++ ] = coeff[ coeff_index - 1 ] * x;
    }
}



/* -------------------------------------------------------------------
 *
 * Name: OneDPolyEval
 *
 * Purpose: One dimensional polynomial evaluation for the specified (x)
 *      input.
 * Description:
 *
 *  This routine evaluates a one-dimensional polynomial at a given location.
 *  Input to the routine are the coefficients calculated by polyfit, the
 *  fitting order, and the x location.
 *
 * Input Parameters:
 *      x - x sample value
 *      order - polynomial order 1=linear, 2=quadratic, 3=cubic
 *      coeff - array of polynomial coefficients.
 *
 * Output Parameters: none
 *
 * Returns: f(x), the value of the polynomial evaluated at (x).
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Jim Ehrismann
 *
 *     April 7 1997 : created (based on the as-of-today current TwoDPolyEval())
 *
 * -------------------------------------------------------------------
 */
EXTERNAL double OneDPolyEval( double *coeff, int order, double x )
{
    double  term;
    double  ans;

    int i;


    term = 1.0;
    ans = coeff[ 0 ];
    for ( i = 1 ; i <= order ; i++ )
    {
        term *= x;
        ans += coeff[ i ] * term;
    }

    return (ans);
}


/* -------------------------------------------------------------------
 *
 * Name: [K1A10-1] TwoDPolyFit
 *
 * Purpose: Generate a least squares fit of the f(x,y) samples to
 *  a 2-D polynomial of order 1=bilinear, 2=biquadratic, 3=bicubic etc..
 *
 * Description:
 *
 *   This routine fits a surface of order <fit_order>
 *   - (fit order = 1 - bilinear, 2 - biquadratic, 3 - bicubic etc.)
 *   to the function values provided in f-array, as a function of x & y.
 *
 *   The fitted surface is defined as:
 *
 *   f(x,y) = a0 + a1.x + a2.y + a3.(x*x) + a4.(x*y) + a5.(y*y)
 *              + a6.(x*x*x) + a7.(x*x*y) + a8.(x*y*y) + a9.(y*y*y) etc.
 *
 *   An rms error value is computed and returned.
 *
 *   A status of ABORT is returned if a fit is infeasible or unsatisfactory.
 *
 *   The least square fit is achieved by use of svdcmp
 *   - we regard the coefficients 'a' as the variables to be solved for,
 *   and provide svdcmp with the overdetermined set of equations
 *
 *   f1 = a0 + a1x1 + a2y1 ...
 *   f2 = a0 + a1x2 + a2y2 ...
 *   f3 = a0 + a1x3 + a2y3 ...
 *   etc..
 *
 *   The singular value decomposition algorithm provides a robust least
 *   square fit as a solution when asked to solve
 *   an overdetermined problem Ax = b. See section 2.6 of Numerical Recipes.
 *
 *   The coefficients are most easily envisaged as being stored in a binomial pyramid
 *   of rows of different order:
 *
 *                                1
 *                              x   y
 *                            xx  xy  yy
 *                          xxx xxy xyy yyy
 *                      xxxx xxxy xxyy xyyy yyyy etc.
 *
 *
 *
 * Input Parameters:
 *
 *      fit_order  - order of polynomial fit 1=bilinear, 2=biquadratic,...
 *      no_samples - number of samples in the f, x, and y arrays
 *      f_array - points to array of function values in f(x,y)
 *      x_array - points to array of x values
 *      y_array - points to array of y values
 *
 * Output Parameters:
 *      rms_error - the RMS error in the polynomial surface fit.
 *      coeffs_array - the array of polynomial coefficients
 *
 * Returns: SUCCESS or ABORT in the case of an error.
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *     March 15 1996: Bernie Armour added normalization of f, x, y such that
 *               fitting accuracy will be independent of the magnitudes of f,x,y.
 *     May 30 1996: Bernie Armour added a new form of normalization of f such
 *               that f is rescaled to be on [0,1] before doing the curve fit.
 *               The x and y rescaling is not changed.
 *
 * -------------------------------------------------------------------
 */
EXTERNAL int TwoDPolyFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *f_array, double *x_array,
    double *y_array )
{

    int i, j;
    int l_num_coeff;
    int l_status;

    double max_f, max_abs_x, max_abs_y;  /* Max f, |x|, |y| */
    double min_f;                        /* Min value of function f. */

    double f_scale;                      /* Scale and shift f such that it is on [0,1]. */
    double f_shift;                      /* Thus f' = f_scale * f + f_shift.            */


    double l_wmax, l_thresh;
    double l_tol=SVD_TOL_POLY_FIT;
    double l_x, l_y, l_f, l_ff;
    double l_sumsq_err;

    double **l_u = NULL;
    double **l_v = NULL;
    double *l_w = NULL;

    /*
     *  Initialization.
     */
    l_status = SUCCESS;
    l_num_coeff = ( fit_order + 1 ) * ( fit_order + 2 ) / 2;

    if ( no_samples < l_num_coeff )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INTERNAL ERROR: Bad call to polyfit - too few sample points." );
        return (ABORT);
    }

    l_u = (double **) AllocMatrix( 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
    l_v = (double **) AllocMatrix( 1, l_num_coeff, 1, l_num_coeff, sizeof( **l_v ) );
    l_w = (double *) AllocVector( 1, l_num_coeff, sizeof( *l_w )  );


    if ( l_u == NULL || l_v == NULL || l_w == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INTERNAL ERROR: Not enough memory to determine "
                  "polynomial coefficients in polyfit." );
        if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
        if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff,1, l_num_coeff, sizeof( **l_v ) );
        if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff, sizeof( *l_w ) );

        return (ABORT);
    }

    /*
     *   Normalize the f, x, and y values to fall in [-1.0, 1.0].
     */
    min_f = f_array[0];
    max_f = f_array[0];
    max_abs_x = 0.0;
    max_abs_y = 0.0;
    for ( i = 0 ; i < no_samples ; i++ )
    {
        if ( f_array[i] > max_f ) max_f = f_array[i];
        if ( f_array[i] < min_f ) min_f = f_array[i];
        if ( fabs(x_array[i]) > max_abs_x ) max_abs_x = fabs(x_array[i]);
        if ( fabs(y_array[i]) > max_abs_y ) max_abs_y = fabs(y_array[i]);
    }

    /*
     *   f_scale = 1.0 / ( max(f) - min(f) )  and f_shift = - min(f) * f_scale
     *   f' = f_scale * f + f_shift
     */
    if ( (max_f - min_f) < TINY )
    {
        f_scale = 1.0;
    }
    else
    {
        f_scale = 1.0 / (max_f - min_f);
    }
    f_shift = -min_f * f_scale;
    for ( i = 0 ; i < no_samples ; i++ )
    {
        f_array[i] = f_scale * f_array[i] + f_shift;
        x_array[i] /= max_abs_x;
        y_array[i] /= max_abs_y;
    }

    /*-                                2                   2            -*/
    /*- fi = a0 + a1 xi + a2 yi + a3 xi  + a4 xi yi + a5 yi  + ...      -*/

    for ( i = 0 ; i < no_samples ; i++ )
    {
        GetTwoDPowerCoefficients( l_u[ i + 1 ], fit_order,  x_array[ i ],
                y_array[ i ] );
    }

    /*
     *  perform the svd least square fit
     */
    svdcmp( l_u, no_samples, l_num_coeff, l_w, l_v );

    l_wmax=0.0;
    for ( j = 1 ; j <= l_num_coeff ; j++ )
    {
        if ( l_w[ j ] > l_wmax ) l_wmax = l_w[ j ];
    }
    l_thresh = l_tol * l_wmax;

    for ( j = 1 ; j <= l_num_coeff ; j++ )
    {
        if ( l_w[ j ] < l_thresh )
        {
            l_w[ j ] = 0.0;
        }
    }


    svbksb( l_u, l_w, l_v, no_samples, l_num_coeff, f_array - 1, coeffs_array - 1 );

    /*
     *  Return data and coefficients to their correctly non-scaled values.
     *  For the coefficients:
     *  1. Undo the shift by shifting the constant coefficient c[0] by -f_shift.
     *  2. Undo the scaling by dividing by f_scale.
     */
    for ( i = 0 ; i < no_samples ; i++ )
    {
        f_array[i] = (f_array[i] - f_shift)/f_scale;
        x_array[i] *= max_abs_x;
        y_array[i] *= max_abs_y;
    }
    GetTwoDPowerCoefficients( l_w, fit_order, (1.0/max_abs_x), (1.0/max_abs_y) );
    coeffs_array[0] -= f_shift;
    for ( i = 0; i < l_num_coeff; i++ ) coeffs_array[i] *=  l_w[i+1] / f_scale;

    /*
     *  compute the sum square error between fitted & supplied values
     */
    l_sumsq_err = 0.0;

    for (i = 0; i<no_samples; i++)
    {
            l_x = *(x_array+i);
            l_y = *(y_array+i);
            l_f = *(f_array+i);
            l_ff = TwoDPolyEval(coeffs_array, fit_order, l_x, l_y);

            l_sumsq_err += (l_f - l_ff)*(l_f - l_ff);
    }
    *rms_err = sqrt(l_sumsq_err/no_samples);

    /*
     *  tidy up
     */
    if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
    if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff,1, l_num_coeff, sizeof( **l_v ) );
    if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff, sizeof( *l_w ) );

    return (l_status);
}

/* -------------------------------------------------------------------
 *
 * Name: GetTwoDPowerCoefficients
 *
 * Purpose: Generates an array of values for each term x^m*y^n
 *      in the two-D polynomial.  This is a precalculation done
 *      for one of the samples f(x,y).
 *
 * Description: generates the x,y terms in the surface fit for
 *      each of the samples.
 *
 * Input Parameters:
 *      x - x sample value
 *      y - y sample value
 *      order - polynomial order 1=bilinear, 2=biquadratic, ...
 *
 * Output Parameters:
 *      coeff - pointer to the array containing the x^m*y^n values for the
 *          polynomial for the current x,y values.
 *
 * Returns: none
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *
 *     April 7 1997    :  Jim Ehrismann
 *                  renamed from GetPowerCoefficients() since one-d and
 *                  three-d routines now exist too.
 *
 * -------------------------------------------------------------------
 */
LOCAL void GetTwoDPowerCoefficients(double *coeff, int order, double x, double y)
{
    int coeff_index;
    double term;

    int i, j, k;

    coeff_index = 1;    /*- watch out! unit offset as required by SVD routines. -*/

    coeff[ coeff_index++ ] = 1.0;

    for ( i = 1 ; i <= order ; i++ ) /*- rows in the binomial pyramid -*/
    {
        for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
        {
            term = 1.0;
            for ( k = 0 ; k < i - j ; k++ ) /*- power of x for this term -*/
            {
                term *= x;
            }
            for ( ; k < i ; k++ ) /*- power of y for this term -*/
            {
                term *= y;
            }

            coeff[ coeff_index++ ] = term;
        }
    }
}



/* -------------------------------------------------------------------
 *
 * Name: TwoDPolyEval
 *
 * Purpose: Two dimensional polynomial evaluation for the specified (x,y)
 *      input.
 * Description:
 *
 *  This routine evaluates a two-dimensional polynomial at a given location.
 *  Input to the routine are the coefficients calculated by polyfit, the
 *  fitting order, and the x,y location.
 *
 * Input Parameters:
 *      x - x sample value
 *      y - y sample value
 *      order - polynomial order 1=bilinear, 2=biquadratic, 3=bicubic
 *      coeff - array of polynomial coefficients.
 *
 * Output Parameters: none
 *
 * Returns: f(x,y), the value of the polynomial evaluated at (x,y).
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *
 * -------------------------------------------------------------------
 */
EXTERNAL double TwoDPolyEval( double *coeff, int order, double x, double y )
{
    int     coeff_index;
    double  term;
    double  ans;

    int i, j, k;


    coeff_index = 0;
    ans = coeff[ coeff_index++ ];
    for ( i = 1 ; i <= order ; i++ ) /*- rows in the binomial pyramid -*/
    {
        for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
        {
            term = 1.0;
            for ( k = 0 ; k < i - j ; k++ ) /*- power of x for this term -*/
            {
                term *= x;
            }
            for ( ; k < i ; k++ ) /*- power of y for this term -*/
            {
                term *= y;
            }

            ans += coeff[ coeff_index++ ] * term;
        }
    }
    return (ans);
}


/* -------------------------------------------------------------------
 *
 * Name: [K1A10-2] TwoDPolyGradFit
 *
 * Purpose: Calculates the coefficients of the polynomial such that the
 *      gradient is a least squares fit to the supplied samples
 *      gradf(x_i,y_i)=gradf_i.  Returns the RMS error of the fit.
 *
 * Description:
 *
 *   This routine fits a surface of order <fit_order>
 *   - (fit order = 1 - bilinear, 2 - biquadratic, 3 - bicubic etc.)
 *   such that its gradient is a least square fit to spot values provided
 *   as a function of x  & y. See U9A1.
 *
 *   The fitted surface is defined as:
 *
 *   f(x,y) = a0 + a1.x + a2.y + a3.(x*x) + a4.(x*y) + a5.(y*y)
 *            + a6.(x*x*x) + a7.(x*x*y) + a8.(x*y*y) + a9.(y*y*y) etc.
 *
 *   An rms error value is computed and returned.
 *
 *   A status of ABORT is returned if a fit is infeasible or unsatisfactory.
 *
 *   The least square fit is achieved by use of svdcmp - we regard
 *   the coefficients 'a' as the variables to be solved for,
 *   and provide svdcmp with the overdetermined set of equations
 *
 *   g1x = a1x1 + 0a2y1 + 2a3x1 + a4y1 +0a5y1y1...
 *   g1y = 0a1x1 + a2y1 + 0a3x1 + a4x1 + 2a5y1...
 *
 *   etc..
 *
 *
 * Input Parameters:
 *
 *      fit_order  - order of polynomial fit 1=bilinear, 2=biquadratic,...
 *      no_samples - number of samples in the f, x, and y arrays
 *      gradxy_array - points to array containing gradient function
 *          values, order as [(df/dx)(i), (df/dy)(i)] for each ith sample.
 *          This array is of length 2*no_samples.
 *      x_array - points to array of x values
 *      y_array - points to array of y values
 *
 * Output Parameters:
 *      rms_error - the RMS error in the polynomial surface fit.
 *      coeffs_array - the array of polynomial coefficients
 *
 * Returns: SUCCESS or ABORT in the case of a failure.
 *
 * Original Author:   Andy Smith
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *
 *     March 18 1996: Shinya Sato made some change to memory allocation/deallocation.
 *
 * -------------------------------------------------------------------
 */
EXTERNAL int TwoDPolyGradFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *gradxy_array,
    double *x_array, double *y_array )
{
    int i, j;
    int l_num_coeff, l_status;

    double l_wmax, l_thresh;
    double l_tol=SVD_TOL_POLY_GRAD_FIT;
    double l_x, l_y, l_fgradx, l_fgrady, l_gradx, l_grady;
    double l_sumsq_err;

    double **l_u = NULL;
    double **l_v = NULL;
    double *l_w = NULL;


    /*
     *  Initialization.
     */
    l_status = SUCCESS;
    l_num_coeff = ( fit_order + 1 ) * ( fit_order + 2 ) / 2;

    if ( no_samples <= l_num_coeff/2 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INTERNAL ERROR: bad call to TwoDPolyGradFit "
                  "- too few sample points." );
        return ABORT;
    }


    l_u = (double **) AllocMatrix( 1, no_samples*2, 1, l_num_coeff-1, sizeof( **l_u ) );
    l_v = (double **) AllocMatrix( 1, l_num_coeff*2, 1, l_num_coeff-1, sizeof( **l_v ) );
    l_w = (double *) AllocVector( 1, l_num_coeff-1, sizeof( *l_w )  );

    if ( l_u == NULL || l_v == NULL || l_w == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INTERNAL ERROR: Not enough memory to determine "
                  "polynomial coefficients in TwoDPolyGradFit." );
        if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples*2, 1, l_num_coeff-1, sizeof( **l_u ));
        if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff*2,1, l_num_coeff-1, sizeof( **l_v ));
        if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff-1, sizeof( *l_w ) );

        return ABORT;
    }

    /*-                              2            -*/
    /*- gradfx = a1 + 2a3 xi  + a4 yi +  ...  etc -*/

    for ( i = 0 ; i < no_samples ; i++ )
    {
        GetGradxCoefficients( l_u[ 2*i + 1 ], fit_order,  x_array[ i ],
                y_array[ i ] );
        GetGradyCoefficients( l_u[ 2*i + 2 ], fit_order,  x_array[ i ],
                y_array[ i ] );
    }

    /*
     *  Perform the svd least square fit.
     */
    svdcmp( l_u, 2*no_samples, l_num_coeff-1, l_w, l_v );

    l_wmax=0.0;
    for ( j = 1 ; j <= l_num_coeff-1 ; j++ )
    {
        if ( l_w[ j ] > l_wmax ) l_wmax = l_w[ j ];
    }

    l_thresh = l_tol * l_wmax;

    for ( j = 1 ; j <= l_num_coeff-1 ; j++ )
    {
        if ( l_w[ j ] < l_thresh )
        {
            l_w[ j ] = 0.0;
        }
    }

    *coeffs_array = 0.0; /* a0 is arbitrary */
    svbksb( l_u, l_w, l_v, 2*no_samples, l_num_coeff-1, gradxy_array - 1, coeffs_array );

    /*
     *  Compute the sum square error between fitted & supplied values.
     */
    l_sumsq_err = 0.0;

    for (i = 0; i<no_samples; i++)
    {
        l_x = *(x_array+i);
        l_y = *(y_array+i);
        l_gradx = *(gradxy_array + 2*i);
        l_grady = *(gradxy_array + 2*i + 1);
        TwoDPolyGradEval(&l_fgradx, &l_fgrady, coeffs_array, fit_order, l_x, l_y);

        l_sumsq_err += (l_gradx - l_fgradx)*(l_gradx - l_fgradx) +
                        (l_grady - l_fgrady)*(l_grady - l_fgrady);
    }

    *rms_err = sqrt(l_sumsq_err/(2*no_samples));

    /*
     *  tidy up
     */
    if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples*2, 1, l_num_coeff-1, sizeof( **l_u ));
    if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff*2,1, l_num_coeff-1, sizeof( **l_v ));
    if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff-1, sizeof( *l_w ) );


    return (l_status);
}

/* -------------------------------------------------------------------
 *
 * Name: GetGradxCoefficients
 *
 * Purpose: Generates an array of values for each term x^m*y^n
 *      in the x component of the gradient of the two-D polynomial.
 *      This is a precalculation done for one of the samples {df/dx(x,y)}_i.
 *
 * Description: generates the x,y terms in the surface fit for
 *      each of the samples.
 *
 * Input Parameters:
 *      x - x sample value
 *      y - y sample value
 *      order - polynomial order 1=bilinear, 2=biquadratic, ...
 *
 * Output Parameters:
 *      coeff - pointer to the array containing the x^m*y^n values for the
 *          df/dx polynomial for the current x,y values.
 *
 * Returns: none
 *
 * Original Author:   Andy Smith
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *
 * -------------------------------------------------------------------
 */
LOCAL void GetGradxCoefficients( double *coeff, int order, double x, double y )
{
    int coeff_index;
    double term;

    int i, j, k;

    coeff_index = 1;    /*- watch out! unit offset as required by SVD routines. -*/

    coeff[ coeff_index++ ] = 1.0;
    coeff[ coeff_index++ ] = 0.0;

    for ( i = 2 ; i <= order ; i++ ) /*- rows in the binomial pyramid -*/
    {
        for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
        {
            term = i - j ;        /*- power of x for this term -*/
            for ( k = 0 ; k < i - j - 1 ; k++ )
            {
                term *= x;
            }
            for ( k = 0; k < j ; k++ ) /*- power of y for this term -*/
            {
                term *= y;
            }

            coeff[ coeff_index++ ] = term;
        }
    }
}

/* -------------------------------------------------------------------
 *
 * Name: GetGradyCoefficients
 *
 * Purpose: Generates an array of values for each term x^m*y^n
 *      in the y component of the gradient of the two-D polynomial.
 *      This is a precalculation done for one of the samples {df/dy(x,y)}_i.
 *
 * Description: generates the x,y terms in the surface fit for
 *      each of the samples.
 *
 * Input Parameters:
 *      x - x sample value
 *      y - y sample value
 *      order - polynomial order 1=bilinear, 2=biquadratic, ...
 *
 * Output Parameters:
 *      coeff - pointer to the array containing the x^m*y^n values for the
 *          df/dy polynomial for the current x,y values.
 *
 * Returns: none
 *
 * Original Author:   Andy Smith
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *
 * -------------------------------------------------------------------
 */
LOCAL void GetGradyCoefficients( double *coeff, int order, double x, double y )
{
    int coeff_index;
    double term;

    int i, j, k;

    coeff_index = 1;    /*- watch out! unit offset as required by SVD routines. -*/

    coeff[ coeff_index++ ] = 0.0;
    coeff[ coeff_index++ ] = 1.0;

    for ( i = 2 ; i <= order ; i++ ) /*- rows in the binomial pyramid -*/
    {
        for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
        {
            term = 1.0 ;
            for ( k = 0 ; k < i - j ; k++ ) /*- power of x for this term -*/
            {
                term *= x;
            }
            term *= j ; /*- power of y for this term -*/
            for ( k = 0; k < j - 1; k++ )
            {
                term *= y;
            }

            coeff[ coeff_index++ ] = term;
        }
    }
}

/* -------------------------------------------------------------------
 *
 * Name: TwoDPolyGradEval
 *
 * Purpose: calculate df/dx, df/dy given coefficients for f(x,y) and
 *      function inputs (x,y).
 *
 * Description:
 *  This routine evaluates the x and y components of the gradient of the
 *  two-dimensional polynomial f(x,y) at a given location.
 *  Input to the routine are the coefficients calculated by polyfit, the
 *  fitting order, and the x,y location.
 *
 * Input Parameters:
 *      x - x sample value
 *      y - y sample value
 *      order - polynomial order 1=bilinear, 2=biquadratic, 3=bicubic
 *      coeff - array of polynomial coefficients.
 *
 * Output Parameters:
 *  fgradx - df/dx at (x,y)
 *  fgrady - df/dy at (x,y)
 *
 * Returns: none
 *
 * Original Author:   Andy Smith
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *
 * -------------------------------------------------------------------
 */
EXTERNAL void TwoDPolyGradEval(double *fgradx, double *fgrady,
    double *coeff, int order, double x, double y)
{
    int coeff_index;
    double term;

    int i, j, k;

    coeff_index = 1;

    *fgradx = coeff[ coeff_index++ ];
    coeff_index++;

    for ( i = 2 ; i <= order ; i++ ) /*- rows in the binomial pyramid -*/
    {
        for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
        {
            term = i - j ;        /*- power of x for this term -*/
            for ( k = 0 ; k < i - j - 1 ; k++ )
            {
                term *= x;
            }
            for ( k = 0; k < j ; k++ ) /*- power of y for this term -*/
            {
                term *= y;
            }

            *fgradx += coeff[ coeff_index++ ] * term;
        }
    }

    coeff_index = 1;

    coeff_index++;
    *fgrady = coeff[ coeff_index++ ];

    for ( i = 2 ; i <= order ; i++ ) /*- rows in the binomial pyramid -*/
    {
        for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
        {
            term = 1.0 ;
            for ( k = 0 ; k < i - j ; k++ ) /*- power of x for this term -*/
            {
                term *= x;
            }
            term *= j ; /*- power of y for this term -*/
            for ( k = 0; k < j - 1; k++ )
            {
                term *= y;
            }

            *fgrady += coeff[ coeff_index++ ] * term;
        }
    }
}


/* -------------------------------------------------------------------
 *
 * Name: GetPolyInX
 *
 * Purpose: use to calculate the coeffients of the corresponding 1-D
 *      polynomial given the coefficients for a 2-D polynomial and
 *      the value of y.
 *
 * Description:
 *
 * This routine generates the (order + 1) 1-dimensional polynomial
 * coefficients describing behaviour as a function of x, at a given y.
 *
 * Input Parameters:
 *      xycoeffs - array of polynomial coefficients.
 *      order - polynomial order 1=bilinear, 2=biquadratic, 3=bicubic
 *      y - y sample value
 *
 * Output Parameters:
 *      xcoeffs - array of 1-D polynomial in x coefficients.
 *
 * Returns: none
 *
 * Original Author:   Andy Smith
 *
 * Revision History: Bernie Armour
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : updated code format
 *
 * -------------------------------------------------------------------
 */
EXTERNAL void GetPolyInX (double *xcoeffs, double *xycoeffs, int order,
                          double y)
{
    int i, j, k, n;
    double l_term;

    for ( i = 0; i <= order; i++)               /* power of x */
    {
        xcoeffs[i] = 0;

        for ( j = 0; j <= (order - i); j++)     /* power of y */
        {
            n = i + j;                          /* order of binomial row of interest */
            l_term = xycoeffs[ (n*(n+1)/2) + j];
            for ( k = 0; k < j; k++)
            {
                l_term *= y;
            }

            xcoeffs[i] += l_term;
        }
    }
}


/* -------------------------------------------------------------------
 *
 * Name: GetPolyInY
 *
 * Purpose: use to calculate the coeffients of the corresponding 1-D
 *      polynomial given the coefficients for a 2-D polynomial and
 *      the value of x.
 *
 * Description:
 *
 * This routine generates the (order + 1) 1-dimensional polynomial
 * coefficients describing behaviour as a function of y, at a given x.
 *
 *
 * Input Parameters:
 *      xycoeffs - array of polynomial coefficients.
 *      order - polynomial order 1=bilinear, 2=biquadratic, 3=bicubic
 *      x - x sample value
 *
 * Output Parameters:
 *      ycoeffs - array of 1-D polynomial in y coefficients.
 *
 * Returns: none
 *
 * Original Author:   Andy Smith
 *
 * Revision History:
 *
 *     September 12 1996 : created
 *
 *     January 16 1996 : Bernie Armour - updated code format
 *     February 27 1996: Bernie Armour - corrected bug in xycoefs[] index, -1 added in.
 *
 * -------------------------------------------------------------------
 */
EXTERNAL void GetPolyInY(double *ycoeffs, double *xycoeffs, int order,
                           double x)
{
    int i, j, k, n;
    double l_term;

    for ( i = 0; i <= order; i++)               /* power of y */
    {
        ycoeffs[i] = 0;

        for ( j = 0; j <= (order - i); j++)     /* power of x */
        {
            n = i + j;                          /* order of binomial row of interest */
            l_term = xycoeffs[ ((n+1)*(n+2)/2) - j - 1];
            for ( k = 0; k < j; k++)
            {
                l_term *= x;
            }

            ycoeffs[i] += l_term;
        }
    }
}


/* -------------------------------------------------------------------
 *
 * Name: ThreeDPolyFit
 *
 * Purpose: Generate a least squares fit of the f(x,y,z) samples to
 *  a 3-D polynomial of order 1=trilinear, 2=triquadratic, 3=tricubic etc..
 *
 * Description:
 *
 *   This routine fits a surface of order <fit_order>
 *   - (fit order = 1 - trilinear, 2 - triquadratic, 3 - tricubic etc.)
 *   to the function values provided in f-array, as a function of x, y & z.
 *
 *   The fitted surface is defined as:
 *
 *   f(x,y) = a0 + a1.x + a2.y + a3.(x*x) + a4.(x*y) + a5.(y*y)
 *              + a6.(x*x*x) + a7.(x*x*y) + a8.(x*y*y) + a9.(y*y*y) etc.
 *          + ( b0 + b1.x + b2.y + b3.(x*x) + b4.(x*y) + b5.(y*y)
 *              + b6.(x*x*x) + b7.(x*x*y) + b8.(x*y*y) + b9.(y*y*y) etc. ) * z
 *          + ( c0 + c1.x + c2.y + c3.(x*x) + c4.(x*y) + c5.(y*y)
 *              + c6.(x*x*x) + c7.(x*x*y) + c8.(x*y*y) + c9.(y*y*y) etc. ) * z*z
 *          + etc.
 *
 *   An rms error value is computed and returned.
 *
 *   A status of ABORT is returned if a fit is infeasible or unsatisfactory.
 *
 *   The least square fit is achieved by use of svdcmp
 *   - we regard the coefficients 'a' as the variables to be solved for,
 *   and provide svdcmp with the overdetermined set of equations
 *
 *   f1 = a0 + a1x1 + a2y1 + ... + (b0 + b1x1 + b2y1 + ... )z1 + ...
 *   f2 = a0 + a1x2 + a2y2 + ... + (b0 + b1x2 + b2y2 + ... )z2 + ...
 *   f3 = a0 + a1x3 + a2y3 + ... + (b0 + b1x3 + b2y3 + ... )z3 + ...
 *   etc..
 *
 *   The singular value decomposition algorithm provides a robust least
 *   square fit as a solution when asked to solve
 *   an overdetermined problem Ax = b. See section 2.6 of Numerical Recipes.
 *
 *   The coefficients are most easily envisaged as being stored in binomial pyramids
 *   of rows of different order:
 *
 *                                1
 *                              x   y
 *                            xx  xy  yy
 *                          xxx xxy xyy yyy
 *                      xxxx xxxy xxyy xyyy yyyy etc. to O(n)(x,y)
 *                                z
 *                             zx   zy
 *                           zxx zxy zyy
 *                         zxxx zxxy zxyy zyyy
 *                     zxxxx zxxxy zxxyy zxyyy zyyyy etc. to O(n-1)(x,y)
 *                               zz
 *                            zzx  zzy
 *                          zzxx zzxy zzyy
 *                        zzxxx zzxxy zzxyy zzyyy
 *                  zzxxxx zzxxxy zzxxyy zzxyyy zzyyyy etc. to O(n-2)(x,y)
 *
 *                                  .
 *                                  .
 *                                  .
 *
 *                                 n
 *                                z
 *
 * Input Parameters:
 *
 *      fit_order  - order of polynomial fit 1=trilinear, 2=triquadratic,...
 *      no_samples - number of samples in the f, x, y, and z arrays
 *      f_array - points to array of function values in f(x,y,z)
 *      x_array - points to array of x values
 *      y_array - points to array of y values
 *      z_array - points to array of z values
 *
 * Output Parameters:
 *      rms_error - the RMS error in the polynomial surface fit.
 *      coeffs_array - the array of polynomial coefficients
 *
 * Returns: SUCCESS or ABORT in the case of an error.
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Jim Ehrismann
 *
 *     Wednesday March 5 1997 : created
 *               Enhanced existing TwoDPolyFit() to support third dimension.
 *
 * -------------------------------------------------------------------
 */
EXTERNAL int ThreeDPolyFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *f_array, double *x_array,
    double *y_array, double *z_array )
{

    int i, j;
    int l_num_coeff;
    int l_status;

    double max_f, max_abs_x, max_abs_y, max_abs_z;  /* Max f, |x|, |y|, |z| */
    double min_f;                                   /* Min value of function f. */

    double f_scale;                                 /* Scale and shift f such that it is on [0,1]. */
    double f_shift;                                 /* Thus f' = f_scale * f + f_shift.            */


    double l_wmax, l_thresh;
    double l_tol=SVD_TOL_POLY_FIT_3D;
    double l_x, l_y, l_f, l_z, l_ff;
    double l_sumsq_err;

    double **l_u = NULL;
    double **l_v = NULL;
    double *l_w = NULL;

    /*
     *  Initialization.
     */
    l_status = SUCCESS;

    /* number of coefficients is sum (from i = 0 to fit_order) of (i+1)(i+2)/2 */
    l_num_coeff = ( fit_order + 1 ) * ( fit_order + 2 ) * ( 2 * fit_order + 6 ) / 12;

    if ( no_samples <= l_num_coeff )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INTERNAL ERROR: Bad call to polyfit "
                  "- too few sample points." );
        return (ABORT);
    }

    l_u = (double **) AllocMatrix( 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
    l_v = (double **) AllocMatrix( 1, l_num_coeff, 1, l_num_coeff, sizeof( **l_v ) );
    l_w = (double *) AllocVector( 1, l_num_coeff, sizeof( *l_w )  );


    if ( l_u == NULL || l_v == NULL || l_w == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INTERNAL ERROR: Not enough memory to determine "
                  "polynomial coefficients in polyfit." );
        if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
        if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff,1, l_num_coeff, sizeof( **l_v ) );
        if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff, sizeof( *l_w ) );

        return (ABORT);
    }

    /*
     *   Normalize the f, x, y, and z values to fall in [-1.0, 1.0].
     */
    min_f = f_array[0];
    max_f = f_array[0];
    max_abs_x = 0.0;
    max_abs_y = 0.0;
    max_abs_z = 0.0;
    for ( i = 0 ; i < no_samples ; i++ )
    {
        if ( f_array[i] > max_f ) max_f = f_array[i];
        if ( f_array[i] < min_f ) min_f = f_array[i];
        if ( fabs(x_array[i]) > max_abs_x ) max_abs_x = fabs(x_array[i]);
        if ( fabs(y_array[i]) > max_abs_y ) max_abs_y = fabs(y_array[i]);
        if ( fabs(z_array[i]) > max_abs_z ) max_abs_z = fabs(z_array[i]);
    }

    /*
     *   f_scale = 1.0 / ( max(f) - min(f) )  and f_shift = - min(f) * f_scale
     *   f' = f_scale * f + f_shift
     */
    if ( (max_f - min_f) < TINY )
    {
        f_scale = 1.0;
    }
    else
    {
        f_scale = 1.0 / (max_f - min_f);
    }
    f_shift = -min_f * f_scale;
    for ( i = 0 ; i < no_samples ; i++ )
    {
        f_array[i] = f_scale * f_array[i] + f_shift;
        x_array[i] /= max_abs_x;
        y_array[i] /= max_abs_y;
        z_array[i] /= max_abs_z;
    }

    /*-                                2                   2                             -*/
    /*- fi = a0 + a1 xi + a2 yi + a3 xi  + a4 xi yi + a5 yi  + ... + O(n)(x,y)           -*/
    /*-                                2                   2                             -*/
    /*-  + ( b0 + b1 xi + b2 yi + b3 xi  + b4 xi yi + b5 yi  + ... + O(n-1)(x,y) ) * zi  -*/
    /*-                                2                   2                           2 -*/
    /*-  + ( c0 + c1 xi + c2 yi + c3 xi  + c4 xi yi + c5 yi  + ... + O(n-2)(x,y) ) * zi  -*/
    /*-               n                                                                  -*/
    /*-  + ... + n0 zi                                                                   -*/

    for ( i = 0 ; i < no_samples ; i++ )
    {
        GetThreeDPowerCoefficients( l_u[ i + 1 ], fit_order,  x_array[ i ],
                y_array[ i ], z_array[ i ] );
    }

    /*
     *  perform the svd least square fit
     */
    svdcmp( l_u, no_samples, l_num_coeff, l_w, l_v );

    l_wmax=0.0;
    for ( j = 1 ; j <= l_num_coeff ; j++ )
    {
        if ( l_w[ j ] > l_wmax ) l_wmax = l_w[ j ];
    }
    l_thresh = l_tol * l_wmax;

    for ( j = 1 ; j <= l_num_coeff ; j++ )
    {
        if ( l_w[ j ] < l_thresh )
        {
            l_w[ j ] = 0.0;    
        }
    }

    svbksb( l_u, l_w, l_v, no_samples, l_num_coeff, f_array - 1, coeffs_array - 1 );

    /*
     *  Return data and coefficients to their correctly non-scaled values.
     *  For the coefficients:
     *  1. Undo the shift by shifting the constant coefficient c[0] by -f_shift.
     *  2. Undo the scaling by dividing by f_scale.
     */
    for ( i = 0 ; i < no_samples ; i++ )
    {
        f_array[i] = (f_array[i] - f_shift)/f_scale;
        x_array[i] *= max_abs_x;
        y_array[i] *= max_abs_y;
        z_array[i] *= max_abs_z;
    }
    GetThreeDPowerCoefficients( l_w, fit_order, (1.0/max_abs_x), (1.0/max_abs_y), (1.0/max_abs_z) );
    coeffs_array[0] -= f_shift;
    for ( i = 0; i < l_num_coeff; i++ ) coeffs_array[i] *=  l_w[i+1] / f_scale;

    /*
     *  compute the sum square error between fitted & supplied values
     */
    l_sumsq_err = 0.0;

    for (i = 0; i<no_samples; i++)
    {
            l_x = *(x_array+i);
            l_y = *(y_array+i);
            l_z = *(z_array+i);
            l_f = *(f_array+i);
            l_ff = ThreeDPolyEval(coeffs_array, fit_order, l_x, l_y, l_z);

            l_sumsq_err += (l_f - l_ff)*(l_f - l_ff);
    }
    *rms_err = sqrt(l_sumsq_err/no_samples);

    /*
     *  tidy up
     */
    if (l_u != NULL) DeallocMatrix( (void **) l_u, 1, no_samples, 1, l_num_coeff, sizeof( **l_u ) );
    if (l_v != NULL) DeallocMatrix( (void **) l_v, 1, l_num_coeff,1, l_num_coeff, sizeof( **l_v ) );
    if (l_w != NULL) DeallocVector( (void *) l_w, 1, l_num_coeff, sizeof( *l_w ) );

    return (l_status);
}


/* -------------------------------------------------------------------
 *
 * Name: GetThreeDPowerCoefficients
 *
 * Purpose: Generates an array of values for each term x^m*y^n*z^n
 *      in the three-D polynomial.  This is a precalculation done
 *      for one of the samples f(x,y,z).
 *
 * Description: generates the x,y,z terms in the surface fit for
 *      each of the samples.
 *
 * Input Parameters:
 *      x - x sample value
 *      y - y sample value
 *      z - z sample value
 *      order - polynomial order 1=trilinear, 2=triquadratic, ...
 *
 * Output Parameters:
 *      coeff - pointer to the array containing the x^m*y^n*z^n values for the
 *          polynomial for the current x,y,z values.
 *
 * Returns: none
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Jim Ehrismann
 *
 *     Wednesday March 5 1997 : created
 *               Enhanced existing GetTwoDPowerCoefficients() to support third dimension.
 *
 * -------------------------------------------------------------------
 */
LOCAL void GetThreeDPowerCoefficients(double *coeff, int order, double x, double y, double z)
{
    int coeff_index;
    double term;

    int i, j, k, l;
    int current_order;  /* for a given order of z, the binomial order for (x,y) */


    coeff_index = 1;    /*- watch out! unit offset as required by SVD routines. -*/

    for ( l = 0, current_order = order ; l <= order ; l++, current_order-- )    /*- sets of binomial pyramids -*/
    {
        /* handle the constant (in (x,y)) term first */
        term = 1.0;
        for ( k = 0 ; k < l ; k++ ) /*- power of z for this constant (in (x,y)) term -*/
        {
            term *= z;
        }
        coeff[ coeff_index++ ] = term;

        for ( i = 1 ; i <= current_order ; i++ ) /*- rows in the binomial pyramid -*/
        {
            for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
            {
                term = 1.0;
                for ( k = 0 ; k < i - j ; k++ ) /*- power of x for this term -*/
                {
                    term *= x;
                }
                for ( ; k < i ; k++ ) /*- power of y for this term -*/
                {
                    term *= y;
                }

                for ( k = 0 ; k < l ; k++ ) /*- power of z for this term -*/
                {
                    term *= z;
                }

                coeff[ coeff_index++ ] = term;
            }
        }
    }
}



/* -------------------------------------------------------------------
 *
 * Name: ThreeDPolyEval
 *
 * Purpose: Three dimensional polynomial evaluation for the specified (x,y,z)
 *      input.
 * Description:
 *
 *  This routine evaluates a three-dimensional polynomial at a given location.
 *  Input to the routine are the coefficients calculated by polyfit, the
 *  fitting order, and the x,y,z location.
 *
 * Input Parameters:
 *      x - x sample value
 *      y - y sample value
 *      z - z sample value
 *      order - polynomial order 1=trilinear, 2=triquadratic, 3=tricubic
 *      coeff - array of polynomial coefficients.
 *
 * Output Parameters: none
 *
 * Returns: f(x,y,z), the value of the polynomial evaluated at (x,y,z).
 *
 * Original Author:   Jim Ehrismann
 *
 * Revision History: Jim Ehrismann
 *
 *     Wednesday March 5 1997 : created
 *               Enhanced existing TwoDPolyEval() to support third dimension.
 *
 * -------------------------------------------------------------------
 */
EXTERNAL double ThreeDPolyEval( double *coeff, int order, double x, double y, double z )
{
    int     coeff_index;
    double  term;
    double  ans = 0.0;  /* accumulated sum of terms to result in the answer */

    int i, j, k, l;
    int current_order;  /* for a given order of z, the binomial order for (x,y) */


    coeff_index = 0;
    for ( l = 0, current_order = order ; l <= order ; l++, current_order-- )    /*- sets of binomial pyramids -*/
    {
        /* handle the constant (in (x,y)) term first */
        term = 1.0;
        for ( k = 0 ; k < l ; k++ ) /*- power of z for this constant (in (x,y)) term -*/
        {
            term *= z;
        }
        ans += coeff[ coeff_index++ ] * term;

        for ( i = 1 ; i <= current_order ; i++ ) /*- rows in the binomial pyramid -*/
        {
            for ( j = 0 ; j <= i ; j++ ) /*- terms in each row -*/
            {
                term = 1.0;
                for ( k = 0 ; k < i - j ; k++ ) /*- power of x for this term -*/
                {
                    term *= x;
                }
                for ( ; k < i ; k++ ) /*- power of y for this term -*/
                {
                    term *= y;
                }
                for ( k = 0 ; k < l ; k++ ) /*- power of z for this term -*/
                {
                    term *= z;
                }

                ans += coeff[ coeff_index++ ] * term;
            }
        }
    }
    return (ans);
}



/*------------------------- End of file polyfit.c ----------------------*/


/*---------------------------- Start of file CURVEFIT.C --------------------------*/

/*
******************************************************************************************************
*                                                                                                    *
*                            COMMAND MODULE:   CURVEFIT.C                                               *
*                            ================================                                        *
*                                                                                                    *
******************************************************************************************************
*/


/*- USER REQUIRED INCLUDE FILES: these are all the .H files required for the application. -*/
/*- ============================                                                          -*/



/*- LOCAL DEFINES: for this command module only. -*/
/*- ==============                               -*/

#define MAX_MULT_FACTOR 4.0     /* used to avoid numerical errors */
#define MAX_OFFSET_FACTOR 2.0

#define CURVEFIT_SYNTAX    "Syntax: CURVEFIT polynomial_order [/yx /num_points=val]"
#define DEFAULT_NUM_POINTS  256

/*- COMMAND PROTOTYPE: this function executes the command.  It is called by the SAR workstation. -*/
/*- ==================                                                                           -*/
/*-                                                                                              -*/


/*- LOCAL FUNCTION PROTOTYPES: these functions are called in this command module only.           -*/
/*- ==========================                                                                   -*/
/*-                                                                                              -*/

/* The following routines are used to find the SVD least squares fit to */
/* a data curve.                                                        */


/************************************************************************
 *
 *  svdfit is used to fit a curve to a bunch of points.
 *
 ************************************************************************/

#define TOL 1.0e-5

EXTERNAL int svdfit(float x[], float y[], int ndata,
            double a[], int ma, double **u, double **v, double w[],
            double *chisq, void (*funcs)(double, double *, int))
{
	int     j,i;
	double  wmax,tmp,thresh,sum,*b, *afunc;

    b=AllocVector(1, ndata, sizeof(double));    
    afunc=AllocVector(1, ma, sizeof(double));

	for (i=1;i<=ndata;i++) {
		(*funcs)(x[i], afunc, ma);
		tmp=1.0;
		for (j=1;j<=ma;j++) u[i][j]=afunc[j]*tmp;
		b[i]= (double) y[i - 1] * tmp;
	}
    if (svdcmp(u,ndata,ma,w,v) != FALSE)
    {
	    wmax=0.0;
	    for (j=1;j<=ma;j++)
		    if (w[j] > wmax) wmax=w[j];
	    thresh=TOL*wmax;
	    for (j=1;j<=ma;j++)
		    if (w[j] < thresh) w[j]=0.0;
	    svbksb(u,w,v,ndata,ma,b,a);
	    *chisq=0.0;
	    for (i=1;i<=ndata;i++) {
		    (*funcs)(x[i], afunc, ma);
		    for (sum=0.0,j=1;j<=ma;j++) sum += a[j]*afunc[j];
		    *chisq += (tmp=(y[i - 1]-sum),tmp*tmp);
	    }
    }
    else
    {
    	DeallocVector(afunc, 1, ma, sizeof(double));
	    DeallocVector(b, 1, ndata, sizeof(double));

        return FALSE;
    }

	DeallocVector(afunc, 1, ma, sizeof(double));
	DeallocVector(b, 1, ndata, sizeof(double));

    return TRUE;
}

#undef TOL


/************************************************************************
 *
 *  svbksb is used to fit a curve to a bunch of points.
 *
 ************************************************************************/

EXTERNAL void svbksb(double **u, double w[], double **v, int m,int n,
            double b[], double x[])
{
	int     jj,j,i;
	double  s,*tmp;

	tmp=AllocVector(1, n, sizeof(double));

	for (j=1;j<=n;j++) {
		s=0.0;
		if (w[j]) {
			for (i=1;i<=m;i++) s += u[i][j]*b[i];
			s /= w[j];
		}
		tmp[j]=s;
	}

	for (j=1;j<=n;j++) {
		s=0.0;
		for (jj=1;jj<=n;jj++) s += v[j][jj]*tmp[jj];
		x[j]=s;
	}

	DeallocVector(tmp, 1, n, sizeof(double));
}


/************************************************************************
 *
 *  svdvar is used to fit a curve to a bunch of points.
 *
 ************************************************************************/

EXTERNAL void svdvar(double **v, int ma, double w[], double **cvm)
{
	int     k,j,i;
	double  sum,*wti;

	wti=AllocVector(1, ma, sizeof(double));

	for (i=1;i<=ma;i++) {
		wti[i]=0.0;
		if (w[i]) wti[i]=1.0/(w[i]*w[i]);
	}
	for (i=1;i<=ma;i++) {
		for (j=1;j<=i;j++) {
			for (sum=0.0,k=1;k<=ma;k++) sum += v[i][k]*v[j][k]*wti[k];
			cvm[j][i]=cvm[i][j]=sum;
		}
	}

	DeallocVector( wti, 1, ma, sizeof(double));
}


/************************************************************************
 *
 *  svdcmp is used to find the Singular Value Decomposition of a matrix.
 *
 ************************************************************************/

static double at,bt,ct;
#define PYTHAG(a,b) ((at=fabs(a)) > (bt=fabs(b)) ? \
(ct=bt/at,at*sqrt(1.0+ct*ct)) : (bt ? (ct=at/bt,bt*sqrt(1.0+ct*ct)): 0.0))

static double maxarg1,maxarg2;

#ifdef MAX
#  undef MAX
#endif

#define MAX(a,b) (maxarg1=(a),maxarg2=(b),(maxarg1) > (maxarg2) ?\
	(maxarg1) : (maxarg2))

#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

EXTERNAL int svdcmp(double **a, int m, int n, double *w, double **v)
{
	int     flag,i,its,j,jj,k,l=0,nm=0;
	double  c,f,h,s,x,y,z;
	double  anorm=0.0,g=0.0,scale=0.0;
	double  *rv1;

	if (m < n) return FALSE;
	rv1=AllocVector( 1, n, sizeof(double));
	for (i=1;i<=n;i++) {
		l=i+1;
		rv1[i]=scale*g;
		g=s=scale=0.0;
		if (i <= m) {
			for (k=i;k<=m;k++) scale += fabs(a[k][i]);
			if (scale) {
				for (k=i;k<=m;k++) {
					a[k][i] /= scale;
					s += a[k][i]*a[k][i];
				}
				f=a[i][i];
				g = -SIGN(sqrt(s),f);
				h=f*g-s;
				a[i][i]=f-g;
				if (i != n) {
					for (j=l;j<=n;j++) {
						for (s=0.0,k=i;k<=m;k++) s += a[k][i]*a[k][j];
						f=s/h;
						for (k=i;k<=m;k++) a[k][j] += f*a[k][i];
					}
				}
				for (k=i;k<=m;k++) a[k][i] *= scale;
			}
		}
		w[i]=scale*g;
		g=s=scale=0.0;
		if (i <= m && i != n) {
			for (k=l;k<=n;k++) scale += fabs(a[i][k]);
			if (scale) {
				for (k=l;k<=n;k++) {
					a[i][k] /= scale;
					s += a[i][k]*a[i][k];
				}
				f=a[i][l];
				g = -SIGN(sqrt(s),f);
				h=f*g-s;
				a[i][l]=f-g;
				for (k=l;k<=n;k++) rv1[k]=a[i][k]/h;
				if (i != m) {
					for (j=l;j<=m;j++) {
						for (s=0.0,k=l;k<=n;k++) s += a[j][k]*a[i][k];
						for (k=l;k<=n;k++) a[j][k] += s*rv1[k];
					}
				}
				for (k=l;k<=n;k++) a[i][k] *= scale;
			}
		}
		anorm=MAX(anorm,(fabs(w[i])+fabs(rv1[i])));
	}
	for (i=n;i>=1;i--) {
		if (i < n) {
			if (g) {
				for (j=l;j<=n;j++)
					v[j][i]=(a[i][j]/a[i][l])/g;
				for (j=l;j<=n;j++) {
					for (s=0.0,k=l;k<=n;k++) s += a[i][k]*v[k][j];
					for (k=l;k<=n;k++) v[k][j] += s*v[k][i];
				}
			}
			for (j=l;j<=n;j++) v[i][j]=v[j][i]=0.0;
		}
		v[i][i]=1.0;
		g=rv1[i];
		l=i;
	}
	for (i=n;i>=1;i--) {
		l=i+1;
		g=w[i];
		if (i < n)
			for (j=l;j<=n;j++) a[i][j]=0.0;
		if (g) {
			g=1.0/g;
			if (i != n) {
				for (j=l;j<=n;j++) {
					for (s=0.0,k=l;k<=m;k++) s += a[k][i]*a[k][j];
					f=(s/a[i][i])*g;
					for (k=i;k<=m;k++) a[k][j] += f*a[k][i];
				}
			}
			for (j=i;j<=m;j++) a[j][i] *= g;
		} else {
			for (j=i;j<=m;j++) a[j][i]=0.0;
		}
		++a[i][i];
	}
	for (k=n;k>=1;k--) {
		for (its=1;its<=30;its++) {
			flag=1;
			for (l=k;l>=1;l--) {
				nm=l-1;
				if (fabs(rv1[l])+anorm == anorm) {
					flag=0;
					break;
				}
				if (fabs(w[nm])+anorm == anorm) break;
			}
			if (flag) {
				c=0.0;
				s=1.0;
				for (i=l;i<=k;i++) {
					f=s*rv1[i];
					if (fabs(f)+anorm != anorm) {
						g=w[i];
						h=PYTHAG(f,g);
						w[i]=h;
						h=1.0/h;
						c=g*h;
						s=(-f*h);
						for (j=1;j<=m;j++) {
							y=a[j][nm];
							z=a[j][i];
							a[j][nm]=y*c+z*s;
							a[j][i]=z*c-y*s;
						}
					}
				}
			}
			z=w[k];
			if (l == k) {
				if (z < 0.0) {
					w[k] = -z;
					for (j=1;j<=n;j++) v[j][k]=(-v[j][k]);
				}
				break;
			}

			if (its == 30)
            {
              	DeallocVector(rv1 ,1 ,n, sizeof(double));
                return FALSE;
            }

			x=w[l];
			nm=k-1;
			y=w[nm];
			g=rv1[nm];
			h=rv1[k];
			f=((y-z)*(y+z)+(g-h)*(g+h))/(2.0*h*y);
			g=PYTHAG(f,1.0);
			f=((x-z)*(x+z)+h*((y/(f+SIGN(g,f)))-h))/x;
			c=s=1.0;
			for (j=l;j<=nm;j++) {
				i=j+1;
				g=rv1[i];
				y=w[i];
				h=s*g;
				g=c*g;
				z=PYTHAG(f,h);
				rv1[j]=z;
				c=f/z;
				s=h/z;
				f=x*c+g*s;
				g=g*c-x*s;
				h=y*s;
				y=y*c;
				for (jj=1;jj<=n;jj++) {
					x=v[jj][j];
					z=v[jj][i];
					v[jj][j]=x*c+z*s;
					v[jj][i]=z*c-x*s;
				}
				z=PYTHAG(f,h);
				w[j]=z;
				if (z) {
					z=1.0/z;
					c=f*z;
					s=h*z;
				}
				f=(c*g)+(s*y);
				x=(c*y)-(s*g);
				for (jj=1;jj<=m;jj++) {
					y=a[jj][j];
					z=a[jj][i];
					a[jj][j]=y*c+z*s;
					a[jj][i]=z*c-y*s;
				}
			}
			rv1[l]=0.0;
			rv1[k]=f;
			w[k]=x;
		}
	}

	DeallocVector(rv1 ,1 ,n, sizeof(double));

    return TRUE;
}

#undef SIGN
#undef MAX
#undef PYTHAG

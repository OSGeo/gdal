#ifndef GVGCPFIT_H_INCLUDED
#define GVGCPFIT_H_INCLUDED

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_error.h"

#define EXTERNAL
#define LOCAL static

#define SUCCESS 0
#define ABORT -1


/*------------------------ Start of file CURVEFIT.H -----------------------*/

/*
******************************************************************************
*                                                                            *
*                                 CURVEFIT.H                                 *
*                                 =========                                  *
*                                                                            *
*   This file contains the function prototype for CURVEFIT.C.                *
******************************************************************************
*/


#ifndef CURVEFIT_H
#define CURVEFIT_H

/*- Function prototypes in CURVEFIT.C. -*/

EXTERNAL int svdfit(float x[], float y[], int ndata,
            double a[], int ma, double **u, double **v, double w[],
            double *chisq, void (*funcs)(double, double *, int));

EXTERNAL void svbksb(double **u, double w[], double **v, int m,int n,
            double b[], double x[]);

EXTERNAL void svdvar(double **v, int ma, double w[], double **cvm);

EXTERNAL int svdcmp(double **a, int m, int n, double *w, double **v);


#endif


/*-------------------------- End of file CURVEFIT.H -----------------------*/




/*----------------------------- FILE polyfit.h ----------------------------*/
#ifndef POLYFIT_H
#define POLYFIT_H

EXTERNAL int OneDPolyFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *f_array, double *x_array );

EXTERNAL double OneDPolyEval( double *coeff, int order, double x );

EXTERNAL int TwoDPolyFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *f_array, double *x_array,
    double *y_array );

EXTERNAL double TwoDPolyEval( double *coeff, int order, double x, double y );

EXTERNAL int TwoDPolyGradFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *gradxy_array,
    double *x_array, double *y_array );

EXTERNAL void TwoDPolyGradEval(double *fgradx, double *fgrady,
    double *coeff, int order, double x, double y);

EXTERNAL void GetPolyInX (double *xcoeffs, double *xycoeffs, int order,
    double y);

EXTERNAL void GetPolyInY(double *ycoeffs, double *xycoeffs, int order,
    double x);

EXTERNAL int ThreeDPolyFit( double *rms_err, double *coeffs_array,
    int fit_order, int no_samples, double *f_array, double *x_array,
    double *y_array, double *z_array );

EXTERNAL double ThreeDPolyEval( double *coeff, int order, double x, double y, double z );



#endif /* POLYFIT_H */


/*---------------------- End of FILE polyfit.h ----------------------------*/

#endif /* ndef _GVGCPFIT_INCLUDED */

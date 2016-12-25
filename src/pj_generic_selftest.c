/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Generic regression test for PROJ.4 projection algorithms.
 * Author:   Thomas Knudsen
 *
 ******************************************************************************
 * Copyright (c) 2016, Thomas Knudsen
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
 *****************************************************************************/


#include <stdio.h>
#define PJ_LIB__
#include <projects.h>


static int deviates_xy (XY expected, XY got, double tolerance);
static int deviates_lp (LP expected, LP got, double tolerance);
static XY pj_fwd_deg (LP in, PJ *P);


/**********************************************************************/
int pj_generic_selftest (
/**********************************************************************/
    char *e_args,
    char *s_args,
    double tolerance_xy,
    double tolerance_lp,
    int n_fwd,
    int n_inv,
    LP *fwd_in,
    XY *e_fwd_expect,
    XY *s_fwd_expect,
    XY *inv_in,
    LP *e_inv_expect,
    LP *s_inv_expect
) {
/***********************************************************************

Generic regression test for PROJ.4 projection algorithms, testing both
ellipsoidal ("e_") and spheroidal ("s_") versions of the projection
algorithms in both forward ("_fwd_") and inverse ("_inv_") mode.

Compares the "known good" results in <e_fwd_expect> and <s_fwd_expect>
with the actual results obtained by transforming the forward input data
set in <fwd_in> with pj_fwd() using setup arguments <e_args> and
<s_args>, respectively.

Then

Compares the "known good" results in <e_inv_expect> and <s_inv_expect>
with the actual results obtained by transforming the inverse input data
set in <inv_in> with pj_inv() using setup arguments <e_args> and
<s_args>, respectively.

Any of the pointers passed may be set to 0, indicating "don't test this
part".

Returns 0 if all data agree to within the accuracy specified in
<tolerance_xy> and <tolerance_lp>. Non-zero otherwise.

***********************************************************************/
    int i;

    PJ *P;

    if (e_args) {
        P = pj_init_plus(e_args);
        if (0==P)
            return 2;

        /* Test forward ellipsoidal */
        if (e_fwd_expect) {
            for (i = 0; i < n_fwd; i++)
                if (deviates_xy (e_fwd_expect[i], pj_fwd_deg ( fwd_in[i], P ), tolerance_xy))
                    break;
            if ( i != n_fwd )
                return 100 + i;
        }

        /* Test inverse ellipsoidal */
        if (e_inv_expect)  {
            for (i = 0; i < n_inv; i++)
                if (deviates_lp (e_inv_expect[i], pj_inv ( inv_in[i], P ), tolerance_lp))
                    break;
            if ( i != n_inv )
                return 200 + i;
        }

        pj_free (P);
    }


    if (s_args) {
        P = pj_init_plus(s_args);
        if (0==P)
            return 3;

        /* Test forward spherical */
        if (s_fwd_expect)  {
            for (i = 0; i < n_fwd; i++)
                if (deviates_xy (s_fwd_expect[i], pj_fwd_deg ( fwd_in[i], P ), tolerance_xy))
                    break;
            if ( i != n_fwd )
                return 300 + i;
        }

        /* Test inverse spherical */
        if (s_inv_expect)  {
            for (i = 0; i < n_inv; i++)
                if (deviates_lp (s_inv_expect[i], pj_inv ( inv_in[i], P ), tolerance_lp))
                    break;
            if ( i != n_inv )
                return 400 + i;
        }

        pj_free (P);
    }

    return 0;
}



/**********************************************************************/
static int deviates_xy (XY expected, XY got, double tolerance) {
/***********************************************************************

    Determine whether two XYs deviate by more than <tolerance>.

    The test material ("expected" values) may contain coordinates that
    are indeterminate. For those cases, we test the other coordinate
    only by forcing expected and actual ("got") coordinates to 0.

***********************************************************************/
    if (HUGE_VAL== expected.x)
        return 0;
    if (HUGE_VAL== expected.y)
        return 0;
    if (hypot ( expected.x - got.x, expected.y - got.y ) > tolerance)
        return 1;
    return 0;
}


/**********************************************************************/
static int deviates_lp (LP expected, LP got, double tolerance) {
/***********************************************************************

    Determine whether two LPs deviate by more than <tolerance>.

    This one is slightly tricky, since the <expected> LP is
    supposed to be represented as degrees (since it was at some
    time written down by a real human), whereas the <got> LP is
    represented in radians (since it is supposed to be the result
    output from pj_inv)

***********************************************************************/
    if (HUGE_VAL== expected.lam)
        return 0;
    if (HUGE_VAL== expected.phi)
        return 0;
    if (hypot ( DEG_TO_RAD * expected.lam - got.lam, DEG_TO_RAD * expected.phi - got.phi ) > tolerance)
        return 1;
    return 0;
}


/**********************************************************************/
static XY pj_fwd_deg (LP in, PJ *P) {
/***********************************************************************

    Wrapper for pj_fwd, accepting input in degrees.

***********************************************************************/
    LP in_rad;
    in_rad.lam = DEG_TO_RAD * in.lam;
    in_rad.phi = DEG_TO_RAD * in.phi;
    return  pj_fwd (in_rad, P);
}

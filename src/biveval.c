/* procedures for evaluating Tseries */
# include <projects.h>
# define NEAR_ONE	1.00001
static double ceval(struct PW_COEF *C, int n, projUV w, projUV w2) {
    double d=0, dd=0, vd, vdd, tmp, *c;
    int j;

    for (C += n ; n-- ; --C ) {
        if ( (j = C->m) != 0) {
            vd = vdd = 0.;
            for (c = C->c + --j; j ; --j ) {
                vd = w2.v * (tmp = vd) - vdd + *c--;
                vdd = tmp;
            }
            d = w2.u * (tmp = d) - dd + w.v * vd - vdd + 0.5 * *c;
        } else
            d = w2.u * (tmp = d) - dd;
        dd = tmp;
    }
    if ( (j = C->m) != 0 ) {
        vd = vdd = 0.;
        for (c = C->c + --j; j ; --j ) {
            vd = w2.v * (tmp = vd) - vdd + *c--;
            vdd = tmp;
        }
        return (w.u * d - dd + 0.5 * ( w.v * vd - vdd + 0.5 * *c ));
    } else
        return (w.u * d - dd);
}

projUV /* bivariate Chebyshev polynomial entry point */
bcheval(projUV in, Tseries *T) {
    projUV w2, w;
    projUV out;
    /* scale to +-1 */
    w.u = ( in.u + in.u - T->a.u ) * T->b.u;
    w.v = ( in.v + in.v - T->a.v ) * T->b.v;
    if (fabs(w.u) > NEAR_ONE || fabs(w.v) > NEAR_ONE) {
        out.u = out.v = HUGE_VAL;
        pj_errno = -36;
    } else { /* double evaluation */
        w2.u = w.u + w.u;
        w2.v = w.v + w.v;
        out.u = ceval(T->cu, T->mu, w, w2);
        out.v = ceval(T->cv, T->mv, w, w2);
    }
    return out;
}

projUV /* bivariate power polynomial entry point */
bpseval(projUV in, Tseries *T) {
    projUV out;
    double *c, row;
    int i, m;

    out.u = out.v = 0.;
    for (i = T->mu; i >= 0; --i) {
        row = 0.;
        if ((m = T->cu[i].m) != 0) {
            c = T->cu[i].c + m;
            while (m--)
                row = *--c + in.v * row;
        }
        out.u = row + in.u * out.u;
    }
    for (i = T->mv; i >= 0; --i) {
        row = 0.;
        if ((m = T->cv[i].m) != 0) {
            c = T->cv[i].c + m;
            while (m--)
                row = *--c + in.v * row;
        }
        out.v = row + in.u * out.v;
    }
    return out;
}

projUV /* general entry point selecting evaluation mode */
biveval(projUV in, Tseries *T) {

    if (T->power) {
        return bpseval(in, T);
    } else {
        return bcheval(in, T);
    }
}


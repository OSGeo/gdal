#define PJ_LIB__
#include <projects.h>
#define MAX_TRY 9
#define TOL 1e-12
	LP
nad_cvt(LP in, int inverse, struct CTABLE *ct) {
	LP t, tb;

	if (in.lam == HUGE_VAL)
		return in;
	/* normalize input to ll origin */
	tb = in;
	tb.lam -= ct->ll.lam;
	tb.phi -= ct->ll.phi;
	tb.lam = adjlon(tb.lam - M_PI) + M_PI;
	t = nad_intr(tb, ct);
	if (inverse) {
		LP del, dif;
		int i = MAX_TRY;

		if (t.lam == HUGE_VAL) return t;
		t.lam = tb.lam + t.lam;
		t.phi = tb.phi - t.phi;

		do {
			del = nad_intr(t, ct);

                        /* This case used to return failure, but I have
                           changed it to return the first order approximation
                           of the inverse shift.  This avoids cases where the
                           grid shift *into* this grid came from another grid.
                           While we aren't returning optimally correct results
                           I feel a close result in this case is better than
                           no result.  NFW
                           To demonstrate use -112.5839956 49.4914451 against
                           the NTv2 grid shift file from Canada. */
			if (del.lam == HUGE_VAL) 
                        {
                            if( getenv( "PROJ_DEBUG" ) != NULL )
                                fprintf( stderr, 
                                         "Inverse grid shift iteration failed, presumably at grid edge.\n"
                                         "Using first approximation.\n" );
                            /* return del */;
                            break;
                        }

			t.lam -= dif.lam = t.lam - del.lam - tb.lam;
			t.phi -= dif.phi = t.phi + del.phi - tb.phi;
		} while (i-- && fabs(dif.lam) > TOL && fabs(dif.phi) > TOL);
		if (i < 0) {
                    if( getenv( "PROJ_DEBUG" ) != NULL )
                        fprintf( stderr, 
                                 "Inverse grid shift iterator failed to converge.\n" );
                    t.lam = t.phi = HUGE_VAL;
                    return t;
		}
		in.lam = adjlon(t.lam + ct->ll.lam);
		in.phi = t.phi + ct->ll.phi;
	} else {
		if (t.lam == HUGE_VAL)
			in = t;
		else {
			in.lam -= t.lam;
			in.phi += t.phi;
		}
	}
	return in;
}

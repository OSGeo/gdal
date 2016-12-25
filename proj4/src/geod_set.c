
#define _IN_GEOD_SET

#include <string.h>
#include "projects.h"
#include "geod_interface.h"
#include "emess.h"
	void
geod_set(int argc, char **argv) {
	paralist *start = 0, *curr;
	double es;
	char *name;
	int i;

    /* put arguments into internal linked list */
	if (argc <= 0)
		emess(1, "no arguments in initialization list");
	start = curr = pj_mkparam(argv[0]);
	for (i = 1; i < argc; ++i) {
		curr->next = pj_mkparam(argv[i]);
		curr = curr->next;
	}
	/* set elliptical parameters */
	if (pj_ell_set(pj_get_default_ctx(),start, &geod_a, &es)) emess(1,"ellipse setup failure");
	/* set units */
	if ((name = pj_param(NULL,start, "sunits").s) != NULL) {
		char *s;
                struct PJ_UNITS *unit_list = pj_get_units_ref();
		for (i = 0; (s = unit_list[i].id) && strcmp(name, s) ; ++i) ;
		if (!s)
			emess(1,"%s unknown unit conversion id", name);
		fr_meter = 1. / (to_meter = atof(unit_list[i].to_meter));
	} else
		to_meter = fr_meter = 1.;
	geod_f = es/(1 + sqrt(1 - es));
	geod_ini();
	/* check if line or arc mode */
	if (pj_param(NULL,start, "tlat_1").i) {
		double del_S;
#undef f
		phi1 = pj_param(NULL,start, "rlat_1").f;
		lam1 = pj_param(NULL,start, "rlon_1").f;
		if (pj_param(NULL,start, "tlat_2").i) {
			phi2 = pj_param(NULL,start, "rlat_2").f;
			lam2 = pj_param(NULL,start, "rlon_2").f;
			geod_inv();
			geod_pre();
		} else if ((geod_S = pj_param(NULL,start, "dS").f) != 0.) {
			al12 = pj_param(NULL,start, "rA").f;
			geod_pre();
			geod_for();
		} else emess(1,"incomplete geodesic/arc info");
		if ((n_alpha = pj_param(NULL,start, "in_A").i) > 0) {
			if (!(del_alpha = pj_param(NULL,start, "rdel_A").f))
				emess(1,"del azimuth == 0");
		} else if ((del_S = fabs(pj_param(NULL,start, "ddel_S").f)) != 0.) {
			n_S = (int)(geod_S / del_S + .5);
		} else if ((n_S = pj_param(NULL,start, "in_S").i) <= 0)
			emess(1,"no interval divisor selected");
	}
	/* free up linked list */
	for ( ; start; start = curr) {
		curr = start->next;
		pj_dalloc(start);
	}
}

/* Determine nad table correction value */
#define PJ_LIB__
#include <projects.h>
	LP
nad_intr(LP t, struct CTABLE *ct) {
	LP val, frct;
	ILP indx;
	double m00, m10, m01, m11;
	FLP *f00, *f10, *f01, *f11;
	long index;
	int in;

	indx.lam = floor(t.lam /= ct->del.lam);
	indx.phi = floor(t.phi /= ct->del.phi);
	frct.lam = t.lam - indx.lam;
	frct.phi = t.phi - indx.phi;
	val.lam = val.phi = HUGE_VAL;
	if (indx.lam < 0) {
		if (indx.lam == -1 && frct.lam > 0.99999999999) {
			++indx.lam;
			frct.lam = 0.;
		} else
			return val;
	} else if ((in = indx.lam + 1) >= ct->lim.lam) {
		if (in == ct->lim.lam && frct.lam < 1e-11) {
			--indx.lam;
			frct.lam = 1.;
		} else
			return val;
	}
	if (indx.phi < 0) {
		if (indx.phi == -1 && frct.phi > 0.99999999999) {
			++indx.phi;
			frct.phi = 0.;
		} else
			return val;
	} else if ((in = indx.phi + 1) >= ct->lim.phi) {
		if (in == ct->lim.phi && frct.phi < 1e-11) {
			--indx.phi;
			frct.phi = 1.;
		} else
			return val;
	}
	index = indx.phi * ct->lim.lam + indx.lam;
	f00 = ct->cvs + index++;
	f10 = ct->cvs + index;
	index += ct->lim.lam;
	f11 = ct->cvs + index--;
	f01 = ct->cvs + index;
	m11 = m10 = frct.lam;
	m00 = m01 = 1. - frct.lam;
	m11 *= frct.phi;
	m01 *= frct.phi;
	frct.phi = 1. - frct.phi;
	m00 *= frct.phi;
	m10 *= frct.phi;
	val.lam = m00 * f00->lam + m10 * f10->lam +
			  m01 * f01->lam + m11 * f11->lam;
	val.phi = m00 * f00->phi + m10 * f10->phi +
			  m01 * f01->phi + m11 * f11->phi;
	return val;
}

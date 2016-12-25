/* Convert DMS string to radians */
#include <projects.h>
#include <string.h>
#include <ctype.h>

static double proj_strtod(char *nptr, char **endptr);

/* following should be sufficient for all but the rediculous */
#define MAX_WORK 64
	static const char
*sym = "NnEeSsWw";
	static const double
vm[] = {
	DEG_TO_RAD,
	.0002908882086657216,
	.0000048481368110953599
};
	double
dmstor(const char *is, char **rs) {
	return dmstor_ctx( pj_get_default_ctx(), is, rs );
}

	double
dmstor_ctx(projCtx ctx, const char *is, char **rs) {
	int sign, n, nl;
	char *p, *s, work[MAX_WORK];
	double v, tv;

	if (rs)
		*rs = (char *)is;
	/* copy sting into work space */
	while (isspace(sign = *is)) ++is;
	for (n = MAX_WORK, s = work, p = (char *)is; isgraph(*p) && --n ; )
		*s++ = *p++;
	*s = '\0';
	/* it is possible that a really odd input (like lots of leading
		zeros) could be truncated in copying into work.  But ... */
	sign = *(s = work);
	if (sign == '+' || sign == '-') s++;
	else sign = '+';
	for (v = 0., nl = 0 ; nl < 3 ; nl = n + 1 ) {
		if (!(isdigit(*s) || *s == '.')) break;
		if ((tv = proj_strtod(s, &s)) == HUGE_VAL)
			return tv;
		switch (*s) {
		case 'D': case 'd':
			n = 0; break;
		case '\'':
			n = 1; break;
		case '"':
			n = 2; break;
		case 'r': case 'R':
			if (nl) {
				pj_ctx_set_errno( ctx, -16 );
				return HUGE_VAL;
			}
			++s;
			v = tv;
			goto skip;
		default:
			v += tv * vm[nl];
		skip:	n = 4;
			continue;
		}
		if (n < nl) {
			pj_ctx_set_errno( ctx, -16 );
			return HUGE_VAL;
		}
		v += tv * vm[n];
		++s;
	}
		/* postfix sign */
	if (*s && (p = strchr(sym, *s))) {
		sign = (p - sym) >= 4 ? '-' : '+';
		++s;
	}
	if (sign == '-')
		v = -v;
	if (rs) /* return point of next char after valid string */
		*rs = (char *)is + (s - work);
	return v;
}

static double
proj_strtod(char *nptr, char **endptr) 

{
    char c, *cp = nptr;
    double result;

    /*
     * Scan for characters which cause problems with VC++ strtod()
     */
    while ((c = *cp) != '\0') {
        if (c == 'd' || c == 'D') {

            /*
             * Found one, so NUL it out, call strtod(),
             * then restore it and return
             */
            *cp = '\0';
            result = strtod(nptr, endptr);
            *cp = c;
            return result;
        }
        ++cp;
    }

    /* no offending characters, just handle normally */

    return pj_strtod(nptr, endptr);
}


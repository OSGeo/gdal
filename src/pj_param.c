/* put parameters in linked list and retrieve */
#include <projects.h>
#include <stdio.h>
#include <string.h>
	paralist * /* create parameter list entry */
pj_mkparam(char *str) {
	paralist *newitem;

	if((newitem = (paralist *)pj_malloc(sizeof(paralist) + strlen(str))) != NULL) {
		newitem->used = 0;
		newitem->next = 0;
		if (*str == '+')
			++str;
		(void)strcpy(newitem->param, str);
	}
	return newitem;
}

/************************************************************************/
/*                              pj_param()                              */
/*                                                                      */
/*      Test for presence or get parameter value.  The first            */
/*      character in `opt' is a parameter type which can take the       */
/*      values:                                                         */
/*                                                                      */
/*       `t' - test for presence, return TRUE/FALSE in PROJVALUE.i         */
/*       `i' - integer value returned in PROJVALUE.i                       */
/*       `d' - simple valued real input returned in PROJVALUE.f            */
/*       `r' - degrees (DMS translation applied), returned as           */
/*             radians in PROJVALUE.f                                      */
/*       `s' - string returned in PROJVALUE.s                              */
/*       `b' - test for t/T/f/F, return in PROJVALUE.i                     */
/*                                                                      */
/************************************************************************/

	PROJVALUE /* test for presence or get parameter value */
pj_param(projCtx ctx, paralist *pl, const char *opt) {

	int type;
	unsigned l;
	PROJVALUE value;

	if( ctx == NULL )
		ctx = pj_get_default_ctx();

	type = *opt++;
	/* simple linear lookup */
	l = strlen(opt);
	while (pl && !(!strncmp(pl->param, opt, l) &&
	  (!pl->param[l] || pl->param[l] == '=')))
		pl = pl->next;
	if (type == 't')
		value.i = pl != 0;
	else if (pl) {
		pl->used |= 1;
		opt = pl->param + l;
		if (*opt == '=')
			++opt;
		switch (type) {
		case 'i':	/* integer input */
			value.i = atoi(opt);
			break;
		case 'd':	/* simple real input */
			value.f = pj_atof(opt);
			break;
		case 'r':	/* degrees input */
			value.f = dmstor_ctx(ctx, opt, 0);
			break;
		case 's':	/* char string */
                        value.s = (char *) opt;
			break;
		case 'b':	/* boolean */
			switch (*opt) {
			case 'F': case 'f':
				value.i = 0;
				break;
			case '\0': case 'T': case 't':
				value.i = 1;
				break;
			default:
				pj_ctx_set_errno(ctx, -8);
				value.i = 0;
				break;
			}
			break;
		default:
bum_type:	/* note: this is an error in parameter, not a user error */
			fprintf(stderr, "invalid request to pj_param, fatal\n");
			exit(1);
		}
	} else /* not given */
		switch (type) {
		case 'b':
		case 'i':
			value.i = 0;
			break;
		case 'd':
		case 'r':
			value.f = 0.;
			break;
		case 's':
			value.s = 0;
			break;
		default:
			goto bum_type;
		}
	return value;
}

/* <<<< Geodesic filter program >>>> */
# include "projects.h"
# include "geod_interface.h"
# include "emess.h"
# include <ctype.h>
# include <stdio.h>
# include <string.h>

# define MAXLINE 200
# define MAX_PARGS 50
# define TAB putchar('\t')
	static int
fullout = 0,	/* output full set of geodesic values */
tag = '#',	/* beginning of line tag character */
pos_azi = 0,	/* output azimuths as positive values */
inverse = 0;	/* != 0 then inverse geodesic */
	static char
*oform = (char *)0,	/* output format for decimal degrees */
*osform = "%.3f",	/* output format for S */
pline[50],		/* work string */
*usage =
"%s\nusage: %s [ -afFIptTwW [args] ] [ +opts[=arg] ] [ files ]\n";
	static void
printLL(double p, double l) {
	if (oform) {
		(void)printf(oform, p * RAD_TO_DEG); TAB;
		(void)printf(oform, l * RAD_TO_DEG);
	} else {
		(void)fputs(rtodms(pline, p, 'N', 'S'),stdout); TAB;
		(void)fputs(rtodms(pline, l, 'E', 'W'),stdout);
	}
}
	static void
do_arc(void) {
	double az;

	printLL(phi2, lam2); putchar('\n');
	for (az = al12; n_alpha--; ) {
		al12 = az = adjlon(az + del_alpha);
		geod_pre();
		geod_for();
		printLL(phi2, lam2); putchar('\n');
	}
}
	static void	/* generate intermediate geodesic coordinates */
do_geod(void) {
	double phil, laml, del_S;

	phil = phi2;
	laml = lam2;
	printLL(phi1, lam1); putchar('\n');
	for ( geod_S = del_S = geod_S / n_S; --n_S; geod_S += del_S) {
		geod_for();
		printLL(phi2, lam2); putchar('\n');
	}
	printLL(phil, laml); putchar('\n');
}
	static void	/* file processing function */
process(FILE *fid) {
	char line[MAXLINE+3], *s;

	for (;;) {
		++emess_dat.File_line;
		if (!(s = fgets(line, MAXLINE, fid)))
			break;
		if (!strchr(s, '\n')) { /* overlong line */
			int c;
			strcat(s, "\n");
			/* gobble up to newline */
			while ((c = fgetc(fid)) != EOF && c != '\n') ;
		}
		if (*s == tag) {
			fputs(line, stdout);
			continue;
		}
		phi1 = dmstor(s, &s);
		lam1 = dmstor(s, &s);
		if (inverse) {
			phi2 = dmstor(s, &s);
			lam2 = dmstor(s, &s);
			geod_inv();
		} else {
			al12 = dmstor(s, &s);
			geod_S = strtod(s, &s) * to_meter;
			geod_pre();
			geod_for();
		}
		if (!*s && (s > line)) --s; /* assumed we gobbled \n */
		if (pos_azi) {
			if (al12 < 0.) al12 += M_TWOPI;
			if (al21 < 0.) al21 += M_TWOPI;
		}
		if (fullout) {
			printLL(phi1, lam1); TAB;
			printLL(phi2, lam2); TAB;
			if (oform) {
				(void)printf(oform, al12 * RAD_TO_DEG); TAB;
				(void)printf(oform, al21 * RAD_TO_DEG); TAB;
				(void)printf(osform, geod_S * fr_meter);
			}  else {
				(void)fputs(rtodms(pline, al12, 0, 0), stdout); TAB;
				(void)fputs(rtodms(pline, al21, 0, 0), stdout); TAB;
				(void)printf(osform, geod_S * fr_meter);
			}
		} else if (inverse)
			if (oform) {
				(void)printf(oform, al12 * RAD_TO_DEG); TAB;
				(void)printf(oform, al21 * RAD_TO_DEG); TAB;
				(void)printf(osform, geod_S * fr_meter);
			} else {
				(void)fputs(rtodms(pline, al12, 0, 0), stdout); TAB;
				(void)fputs(rtodms(pline, al21, 0, 0), stdout); TAB;
				(void)printf(osform, geod_S * fr_meter);
			}
		else {
			printLL(phi2, lam2); TAB;
			if (oform)
				(void)printf(oform, al21 * RAD_TO_DEG);
			else
				(void)fputs(rtodms(pline, al21, 0, 0), stdout);
		}
		(void)fputs(s, stdout);
	}
}

static char *pargv[MAX_PARGS];
static int   pargc = 0;

int main(int argc, char **argv) {
	char *arg, **eargv = argv, *strnchr();
	FILE *fid;
	static int eargc = 0, c;

	if ((emess_dat.Prog_name = strrchr(*argv,'/')) != NULL) ++emess_dat.Prog_name;
	else emess_dat.Prog_name = *argv;
	inverse = ! strncmp(emess_dat.Prog_name, "inv", 3);
	if (argc <= 1 ) {
		(void)fprintf(stderr, usage, pj_get_release(),
                              emess_dat.Prog_name);
		exit (0);
	}
		/* process run line arguments */
	while (--argc > 0) { /* collect run line arguments */
		if(**++argv == '-') for(arg = *argv;;) {
			switch(*++arg) {
			case '\0': /* position of "stdin" */
				if (arg[-1] == '-') eargv[eargc++] = "-";
				break;
			case 'a': /* output full set of values */
				fullout = 1;
				continue;
			case 'I': /* alt. inverse spec. */
				inverse = 1;
				continue;
			case 't': /* set col. one char */
				if (arg[1]) tag = *++arg;
				else emess(1,"missing -t col. 1 tag");
				continue;
			case 'W': /* specify seconds precision */
			case 'w': /* -W for constant field width */
				if ((c = arg[1]) && isdigit(c)) {
					set_rtodms(c - '0', *arg == 'W');
					++arg;
				} else
				    emess(1,"-W argument missing or non-digit");
				continue;
			case 'f': /* alternate output format degrees or xy */
				if (--argc <= 0)
noargument:		   emess(1,"missing argument for -%c",*arg);
				oform = *++argv;
				continue;
			case 'F': /* alternate output format degrees or xy */
				if (--argc <= 0) goto noargument;
				osform = *++argv;
				continue;
			case 'l':
				if (!arg[1] || arg[1] == 'e') { /* list of ellipsoids */
                                    struct PJ_ELLPS *le;
                                    
                                    for (le=pj_get_ellps_ref(); le->id ; ++le)
                                        (void)printf("%9s %-16s %-16s %s\n",
                                                     le->id, le->major, le->ell, le->name);
				} else if (arg[1] == 'u') { /* list of units */
                                    struct PJ_UNITS *lu;
                                    
                                    for (lu = pj_get_units_ref();lu->id ; ++lu)
                                        (void)printf("%12s %-20s %s\n",
                                                     lu->id, lu->to_meter, lu->name);
				} else
                                    emess(1,"invalid list option: l%c",arg[1]);
                                exit( 0 );
			case 'p': /* output azimuths as positive */
				pos_azi = 1;
				continue;
			default:
				emess(1, "invalid option: -%c",*arg);
				break;
			}
			break;
		} else if (**argv == '+') /* + argument */
			if (pargc < MAX_PARGS)
				pargv[pargc++] = *argv + 1;
			else
				emess(1,"overflowed + argument table");
		else /* assumed to be input file name(s) */
			eargv[eargc++] = *argv;
	}
	/* done with parameter and control input */
	geod_set(pargc, pargv); /* setup projection */
	if ((n_alpha || n_S) && eargc)
		emess(1,"files specified for arc/geodesic mode");
	if (n_alpha)
		do_arc();
	else if (n_S)
		do_geod();
	else { /* process input file list */
		if (eargc == 0) /* if no specific files force sysin */
			eargv[eargc++] = "-";
		for ( ; eargc-- ; ++eargv) {
			if (**eargv == '-') {
				fid = stdin;
				emess_dat.File_name = "<stdin>";
			} else {
				if ((fid = fopen(*eargv, "r")) == NULL) {
					emess(-2, *eargv, "input file");
					continue;
				}
				emess_dat.File_name = *eargv;
			}
			emess_dat.File_line = 0;
			process(fid);
			(void)fclose(fid);
			emess_dat.File_name = (char *)0;
		}
	}
	exit(0); /* normal completion */
}

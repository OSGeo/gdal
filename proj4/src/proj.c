/* <<<< Cartographic projection filter program >>>> */
#include "projects.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "emess.h"

/* TK 1999-02-13 */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__WIN32__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif
/* ! TK 1999-02-13 */

#define MAX_LINE 1000
#define MAX_PARGS 100
#define PJ_INVERS(P) (P->inv ? 1 : 0)
	static PJ
*Proj;
	static projUV
(*proj)(projUV, PJ *);
	static int
reversein = 0,	/* != 0 reverse input arguments */
reverseout = 0,	/* != 0 reverse output arguments */
bin_in = 0,	/* != 0 then binary input */
bin_out = 0,	/* != 0 then binary output */
echoin = 0,	/* echo input data to output line */
tag = '#',	/* beginning of line tag character */
inverse = 0,	/* != 0 then inverse projection */
prescale = 0,	/* != 0 apply cartesian scale factor */
dofactors = 0,	/* determine scale factors */
facs_bad = 0,	/* return condition from pj_factors */
very_verby = 0, /* very verbose mode */
postscale = 0;
	static char
*cheby_str,		/* string controlling Chebychev evaluation */
*oform = (char *)0,	/* output format for x-y or decimal degrees */
*oterr = "*\t*",	/* output line for unprojectable input */
*usage =
"%s\nusage: %s [ -bCeEfiIlormsStTvVwW [args] ] [ +opts[=arg] ] [ files ]\n";
	static struct FACTORS
facs;
	static double
(*informat)(const char *, char **),	/* input data deformatter function */
fscale = 0.;	/* cartesian scale factor */
	static projUV
int_proj(projUV data) {
	if (prescale) { data.u *= fscale; data.v *= fscale; }
	data = (*proj)(data, Proj);
	if (postscale && data.u != HUGE_VAL)
		{ data.u *= fscale; data.v *= fscale; }
	return(data);
}
	static void	/* file processing function */
process(FILE *fid) {
	char line[MAX_LINE+3], *s, pline[40];
	projUV data;

	for (;;) {
		++emess_dat.File_line;
		if (bin_in) {	/* binary input */
			if (fread(&data, sizeof(projUV), 1, fid) != 1)
				break;
		} else {	/* ascii input */
			if (!(s = fgets(line, MAX_LINE, fid)))
				break;
			if (!strchr(s, '\n')) { /* overlong line */
				int c;
				(void)strcat(s, "\n");
				/* gobble up to newline */
				while ((c = fgetc(fid)) != EOF && c != '\n') ;
			}
			if (*s == tag) {
				if (!bin_out)
					(void)fputs(line, stdout);
				continue;
			}
			if (reversein) {
				data.v = (*informat)(s, &s);
				data.u = (*informat)(s, &s);
			} else {
				data.u = (*informat)(s, &s);
				data.v = (*informat)(s, &s);
			}
			if (data.v == HUGE_VAL)
				data.u = HUGE_VAL;
			if (!*s && (s > line)) --s; /* assumed we gobbled \n */
			if (!bin_out && echoin) {
				int t;
				t = *s;
				*s = '\0';
				(void)fputs(line, stdout);
				*s = t;
				putchar('\t');
			}
		}
		if (data.u != HUGE_VAL) {
			if (prescale) { data.u *= fscale; data.v *= fscale; }
			if (dofactors && !inverse)
				facs_bad = pj_factors(data, Proj, 0., &facs);
			data = (*proj)(data, Proj);
			if (dofactors && inverse)
				facs_bad = pj_factors(data, Proj, 0., &facs);
			if (postscale && data.u != HUGE_VAL)
				{ data.u *= fscale; data.v *= fscale; }
		}
		if (bin_out) { /* binary output */
			(void)fwrite(&data, sizeof(projUV), 1, stdout);
			continue;
		} else if (data.u == HUGE_VAL) /* error output */
			(void)fputs(oterr, stdout);
		else if (inverse && !oform) {	/*ascii DMS output */
			if (reverseout) {
				(void)fputs(rtodms(pline, data.v, 'N', 'S'), stdout);
				putchar('\t');
				(void)fputs(rtodms(pline, data.u, 'E', 'W'), stdout);
			} else {
				(void)fputs(rtodms(pline, data.u, 'E', 'W'), stdout);
				putchar('\t');
				(void)fputs(rtodms(pline, data.v, 'N', 'S'), stdout);
			}
		} else {	/* x-y or decimal degree ascii output */
			if (inverse) {
				data.v *= RAD_TO_DEG;
				data.u *= RAD_TO_DEG;
			}
			if (reverseout) {
				(void)printf(oform,data.v); putchar('\t');
				(void)printf(oform,data.u);
			} else {
				(void)printf(oform,data.u); putchar('\t');
				(void)printf(oform,data.v);
			}
		}
		if (dofactors) /* print scale factor data */
                {
			if (!facs_bad)
				(void)printf("\t<%g %g %g %g %g %g>",
					facs.h, facs.k, facs.s,
					facs.omega * RAD_TO_DEG, facs.a, facs.b);
			else
				(void)fputs("\t<* * * * * *>", stdout);
                }
		(void)fputs(bin_in ? "\n" : s, stdout);
	}
}
	static void	/* file processing function --- verbosely */
vprocess(FILE *fid) {
	char line[MAX_LINE+3], *s, pline[40];
	projUV dat_ll, dat_xy;
	int linvers;

	if (!oform)
		oform = "%.3f";
	if (bin_in || bin_out)
		emess(1,"binary I/O not available in -V option");
	for (;;) {
		++emess_dat.File_line;
		if (!(s = fgets(line, MAX_LINE, fid)))
			break;
		if (!strchr(s, '\n')) { /* overlong line */
			int c;
			(void)strcat(s, "\n");
			/* gobble up to newline */
			while ((c = fgetc(fid)) != EOF && c != '\n') ;
		}
		if (*s == tag) { /* pass on data */
			(void)fputs(s, stdout);
			continue;
		}
		/* check to override default input mode */
		if (*s == 'I' || *s == 'i') {
			linvers = 1;
			++s;
		} else if (*s == 'I' || *s == 'i') {
			linvers = 0;
			++s;
		} else
			linvers = inverse;
		if (linvers) {
			if (!PJ_INVERS(Proj)) {
				emess(-1,"inverse for this projection not avail.\n");
				continue;
			}
			dat_xy.u = strtod(s, &s);
			dat_xy.v = strtod(s, &s);
			if (dat_xy.u == HUGE_VAL || dat_xy.v == HUGE_VAL) {
				emess(-1,"lon-lat input conversion failure\n");
				continue;
			}
			if (prescale) { dat_xy.u *= fscale; dat_xy.v *= fscale; }
			dat_ll = pj_inv(dat_xy, Proj);
		} else {
			dat_ll.u = dmstor(s, &s);
			dat_ll.v = dmstor(s, &s);
			if (dat_ll.u == HUGE_VAL || dat_ll.v == HUGE_VAL) {
				emess(-1,"lon-lat input conversion failure\n");
				continue;
			}
			dat_xy = pj_fwd(dat_ll, Proj);
			if (postscale) { dat_xy.u *= fscale; dat_xy.v *= fscale; }
		}
		if (pj_errno) {
			emess(-1, pj_strerrno(pj_errno));
			continue;
		}
		if (!*s && (s > line)) --s; /* assumed we gobbled \n */
		if (pj_factors(dat_ll, Proj, 0., &facs)) {
			emess(-1,"failed to conpute factors\n\n");
			continue;
		}
		if (*s != '\n')
			(void)fputs(s, stdout);
		(void)fputs("Longitude: ", stdout);
		(void)fputs(rtodms(pline, dat_ll.u, 'E', 'W'), stdout);
		(void)printf(" [ %.11g ]\n", dat_ll.u * RAD_TO_DEG);
		(void)fputs("Latitude:  ", stdout);
		(void)fputs(rtodms(pline, dat_ll.v, 'N', 'S'), stdout);
		(void)printf(" [ %.11g ]\n", dat_ll.v * RAD_TO_DEG);
		(void)fputs("Easting (x):   ", stdout);
		(void)printf(oform, dat_xy.u); putchar('\n');
		(void)fputs("Northing (y):  ", stdout);
		(void)printf(oform, dat_xy.v); putchar('\n');
		(void)printf("Meridian scale (h)%c: %.8f  ( %.4g %% error )\n",
			facs.code & IS_ANAL_HK ? '*' : ' ', facs.h, (facs.h-1.)*100.);
		(void)printf("Parallel scale (k)%c: %.8f  ( %.4g %% error )\n",
			facs.code & IS_ANAL_HK ? '*' : ' ', facs.k, (facs.k-1.)*100.);
		(void)printf("Areal scale (s):     %.8f  ( %.4g %% error )\n",
			facs.s, (facs.s-1.)*100.);
		(void)printf("Angular distortion (w): %.3f\n", facs.omega *
			RAD_TO_DEG);
		(void)printf("Meridian/Parallel angle: %.5f\n",
			facs.thetap * RAD_TO_DEG);
		(void)printf("Convergence%c: ",facs.code & IS_ANAL_CONV ? '*' : ' ');
		(void)fputs(rtodms(pline, facs.conv, 0, 0), stdout);
		(void)printf(" [ %.8f ]\n", facs.conv * RAD_TO_DEG);
		(void)printf("Max-min (Tissot axis a-b) scale error: %.5f %.5f\n\n",
			facs.a, facs.b);
	}
}

int main(int argc, char **argv) {
    char *arg, **eargv = argv, *pargv[MAX_PARGS], **iargv = argv;
    FILE *fid;
    int pargc = 0, iargc = argc, eargc = 0, c, mon = 0;

    if ( (emess_dat.Prog_name = strrchr(*argv,DIR_CHAR)) != NULL)
        ++emess_dat.Prog_name;
    else emess_dat.Prog_name = *argv;
    inverse = ! strncmp(emess_dat.Prog_name, "inv", 3);
    if (argc <= 1 ) {
        (void)fprintf(stderr, usage, pj_get_release(), emess_dat.Prog_name);
        exit (0);
    }
    /* process run line arguments */
    while (--argc > 0) { /* collect run line arguments */
        if(**++argv == '-') for(arg = *argv;;) {
            switch(*++arg) {
              case '\0': /* position of "stdin" */
                if (arg[-1] == '-') eargv[eargc++] = "-";
                break;
              case 'b': /* binary I/O */
                bin_in = bin_out = 1;
                continue;
              case 'C': /* Check - run internal regression tests */
                return pj_run_selftests (very_verby);
                continue;
              case 'v': /* monitor dump of initialization */
                mon = 1;
                continue;
              case 'i': /* input binary */
                bin_in = 1;
                continue;
              case 'o': /* output binary */
                bin_out = 1;
                continue;
              case 'I': /* alt. method to spec inverse */
                inverse = 1;
                continue;
              case 'E': /* echo ascii input to ascii output */
                echoin = 1;
                continue;
              case 'V': /* very verbose processing mode */
                very_verby = 1;
                mon = 1;
              case 'S': /* compute scale factors */
                dofactors = 1;
                continue;
              case 't': /* set col. one char */
                if (arg[1]) tag = *++arg;
                else emess(1,"missing -t col. 1 tag");
                continue;
              case 'l': /* list projections, ellipses or units */
                if (!arg[1] || arg[1] == 'p' || arg[1] == 'P') {
                    /* list projections */
                    struct PJ_LIST *lp;
                    int do_long = arg[1] == 'P', c;
                    char *str;

                    for (lp = pj_get_list_ref() ; lp->id ; ++lp) {
                        if( strcmp(lp->id,"latlong") == 0
                            || strcmp(lp->id,"longlat") == 0
                            || strcmp(lp->id,"geocent") == 0 )
                            continue;

                        (void)printf("%s : ", lp->id);
                        if (do_long)  /* possibly multiline description */
                            (void)puts(*lp->descr);
                        else { /* first line, only */
                            str = *lp->descr;
                            while ((c = *str++) && c != '\n')
                                putchar(c);
                            putchar('\n');
                        }
                    }
                } else if (arg[1] == '=') { /* list projection 'descr' */
                    struct PJ_LIST *lp;

                    arg += 2;
                    for (lp = pj_get_list_ref(); lp->id ; ++lp)
                        if (!strcmp(lp->id, arg)) {
                            (void)printf("%9s : %s\n", lp->id, *lp->descr);
                            break;
                        }
                } else if (arg[1] == 'e') { /* list ellipses */
                    struct PJ_ELLPS *le;

                    for (le = pj_get_ellps_ref(); le->id ; ++le)
                        (void)printf("%9s %-16s %-16s %s\n",
                                     le->id, le->major, le->ell, le->name);
                } else if (arg[1] == 'u') { /* list units */
                    struct PJ_UNITS *lu;

                    for (lu = pj_get_units_ref(); lu->id ; ++lu)
                        (void)printf("%12s %-20s %s\n",
                                     lu->id, lu->to_meter, lu->name);
                } else if (arg[1] == 'd') { /* list datums */
                    struct PJ_DATUMS *ld;

                    printf("__datum_id__ __ellipse___ __definition/comments______________________________\n" );
                    for (ld = pj_get_datums_ref(); ld->id ; ++ld)
                    {
                        printf("%12s %-12s %-30s\n",
                               ld->id, ld->ellipse_id, ld->defn);
                        if( ld->comments != NULL && strlen(ld->comments) > 0 )
                            printf( "%25s %s\n", " ", ld->comments );
                    }
                } else
                    emess(1,"invalid list option: l%c",arg[1]);
                exit(0);
                continue; /* artificial */
              case 'e': /* error line alternative */
                if (--argc <= 0)
                    noargument:
                emess(1,"missing argument for -%c",*arg);
                oterr = *++argv;
                continue;
              case 'T': /* generate Chebyshev coefficients */
                if (--argc <= 0) goto noargument;
                cheby_str = *++argv;
                continue;
              case 'm': /* cartesian multiplier */
                if (--argc <= 0) goto noargument;
                postscale = 1;
                if (!strncmp("1/",*++argv,2) ||
                    !strncmp("1:",*argv,2)) {
                    if((fscale = atof((*argv)+2)) == 0.)
                        goto badscale;
                    fscale = 1. / fscale;
                } else
                    if ((fscale = atof(*argv)) == 0.) {
                      badscale:
                        emess(1,"invalid scale argument");
                    }
                continue;
              case 'W': /* specify seconds precision */
              case 'w': /* -W for constant field width */
                if ((c = arg[1]) != 0 && isdigit(c)) {
                    set_rtodms(c - '0', *arg == 'W');
                    ++arg;
                } else
                    emess(1,"-W argument missing or non-digit");
                continue;
              case 'f': /* alternate output format degrees or xy */
                if (--argc <= 0) goto noargument;
                oform = *++argv;
                continue;
              case 'r': /* reverse input */
                reversein = 1;
                continue;
              case 's': /* reverse output */
                reverseout = 1;
                continue;
              default:
                emess(1, "invalid option: -%c",*arg);
                break;
            }
            break;
        } else if (**argv == '+') { /* + argument */
            if (pargc < MAX_PARGS)
                pargv[pargc++] = *argv + 1;
            else
                emess(1,"overflowed + argument table");
        } else /* assumed to be input file name(s) */
            eargv[eargc++] = *argv;
    }
    if (eargc == 0 && !cheby_str) /* if no specific files force sysin */
        eargv[eargc++] = "-";
    else if (eargc > 0 && cheby_str) /* warning */
        emess(4, "data files when generating Chebychev prohibited");
    /* done with parameter and control input */
    if (inverse && postscale) {
        prescale = 1;
        postscale = 0;
        fscale = 1./fscale;
    }
    if (!(Proj = pj_init(pargc, pargv)))
        emess(3,"projection initialization failure\ncause: %s",
              pj_strerrno(pj_errno));

    if( pj_is_latlong( Proj ) )
    {
        emess( 3, "+proj=latlong unsuitable for use with proj program." );
        exit( 0 );
    }

    if (inverse) {
        if (!Proj->inv)
            emess(3,"inverse projection not available");
        proj = pj_inv;
    } else
        proj = pj_fwd;
    if (cheby_str) {
        extern void gen_cheb(int, projUV(*)(projUV), char *, PJ *, int, char **);

        gen_cheb(inverse, int_proj, cheby_str, Proj, iargc, iargv);
        exit(0);
    }
    /* set input formating control */
    if (mon) {
        pj_pr_list(Proj);
        if (very_verby) {
            (void)printf("#Final Earth figure: ");
            if (Proj->es) {
                (void)printf("ellipsoid\n#  Major axis (a): ");
                (void)printf(oform ? oform : "%.3f", Proj->a);
                (void)printf("\n#  1/flattening: %.6f\n",
                             1./(1. - sqrt(1. - Proj->es)));
                (void)printf("#  squared eccentricity: %.12f\n", Proj->es);
            } else {
                (void)printf("sphere\n#  Radius: ");
                (void)printf(oform ? oform : "%.3f", Proj->a);
                (void)putchar('\n');
            }
        }
    }
    if (inverse)
        informat = strtod;
    else {
        informat = dmstor;
        if (!oform)
            oform = "%.2f";
    }

    if (bin_out)
    {
        SET_BINARY_MODE(stdout);
    }

    /* process input file list */
    for ( ; eargc-- ; ++eargv) {
        if (**eargv == '-') {
            fid = stdin;
            emess_dat.File_name = "<stdin>";

            if (bin_in)
            {
                SET_BINARY_MODE(stdin);
            }

        } else {
            if ((fid = fopen(*eargv, "rb")) == NULL) {
                emess(-2, *eargv, "input file");
                continue;
            }
            emess_dat.File_name = *eargv;
        }
        emess_dat.File_line = 0;
        if (very_verby)
            vprocess(fid);
        else
            process(fid);
        (void)fclose(fid);
        emess_dat.File_name = 0;
    }
    if( Proj )
        pj_free(Proj);
    exit(0); /* normal completion */
}

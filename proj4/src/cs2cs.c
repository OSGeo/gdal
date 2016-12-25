/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Mainline program sort of like ``proj'' for converting between
 *           two coordinate systems.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include "projects.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "emess.h"
#include <locale.h>

#define MAX_LINE 1000
#define MAX_PARGS 100

static projPJ   fromProj, toProj;

static int
reversein = 0,	/* != 0 reverse input arguments */
reverseout = 0,	/* != 0 reverse output arguments */
echoin = 0,	/* echo input data to output line */
tag = '#';	/* beginning of line tag character */
	static char
*oform = (char *)0,	/* output format for x-y or decimal degrees */
*oterr = "*\t*",	/* output line for unprojectable input */
*usage =
"%s\nusage: %s [ -eEfIlrstvwW [args] ] [ +opts[=arg] ]\n"
"                   [+to [+opts[=arg] [ files ]\n";

static double (*informat)(const char *, 
                          char **); /* input data deformatter function */


/************************************************************************/
/*                              process()                               */
/*                                                                      */
/*      File processing function.                                       */
/************************************************************************/
static void process(FILE *fid) 

{
    char line[MAX_LINE+3], *s, pline[40];
    projUV data;

    for (;;) {
        double z;

        ++emess_dat.File_line;
        if (!(s = fgets(line, MAX_LINE, fid)))
            break;
        if (!strchr(s, '\n')) { /* overlong line */
            int c;
            (void)strcat(s, "\n");
				/* gobble up to newline */
            while ((c = fgetc(fid)) != EOF && c != '\n') ;
        }
        if (*s == tag) {
            fputs(line, stdout);
            continue;
        }

        if (reversein) {
            data.v = (*informat)(s, &s);
            data.u = (*informat)(s, &s);
        } else {
            data.u = (*informat)(s, &s);
            data.v = (*informat)(s, &s);
        }

        z = strtod( s, &s );

        if (data.v == HUGE_VAL)
            data.u = HUGE_VAL;

        if (!*s && (s > line)) --s; /* assumed we gobbled \n */

        if ( echoin) {
            int t;
            t = *s;
            *s = '\0';
            (void)fputs(line, stdout);
            *s = t;
            putchar('\t');
        }

        if (data.u != HUGE_VAL) {
            if( pj_transform( fromProj, toProj, 1, 0, 
                              &(data.u), &(data.v), &z ) != 0 )
            {
                data.u = HUGE_VAL;
                data.v = HUGE_VAL;
                emess(-3,"pj_transform(): %s", pj_strerrno(pj_errno));
            }
        }

        if (data.u == HUGE_VAL) /* error output */
            fputs(oterr, stdout);

        else if (pj_is_latlong(toProj) && !oform) {	/*ascii DMS output */
            if (reverseout) {
                fputs(rtodms(pline, data.v, 'N', 'S'), stdout);
                putchar('\t');
                fputs(rtodms(pline, data.u, 'E', 'W'), stdout);
            } else {
                fputs(rtodms(pline, data.u, 'E', 'W'), stdout);
                putchar('\t');
                fputs(rtodms(pline, data.v, 'N', 'S'), stdout);
            }

        } else {	/* x-y or decimal degree ascii output */
            if ( pj_is_latlong(toProj) ) {
                data.v *= RAD_TO_DEG;
                data.u *= RAD_TO_DEG;
            }
            if (reverseout) {
                printf(oform,data.v); putchar('\t');
                printf(oform,data.u);
            } else {
                printf(oform,data.u); putchar('\t');
                printf(oform,data.v);
            }
        }

        putchar(' ');
        if( oform != NULL )
            printf( oform, z );
        else
            printf( "%.3f", z );
        if( s )
            printf( "%s", s );
        else
            printf( "\n" );
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char **argv) 
{
    char *arg, **eargv = argv, *from_argv[MAX_PARGS], *to_argv[MAX_PARGS];
    FILE *fid;
    int from_argc=0, to_argc=0, eargc = 0, c, mon = 0;
    int have_to_flag = 0, inverse = 0, i;
    int use_env_locale = 0;

    /* This is just to check that pj_init() is locale-safe */
    /* Used by nad/testvarious */
    if( getenv("PROJ_USE_ENV_LOCALE") != NULL )
        use_env_locale = 1;

    if ((emess_dat.Prog_name = strrchr(*argv,DIR_CHAR)) != NULL)
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
              case 'v': /* monitor dump of initialization */
                mon = 1;
                continue;
              case 'I': /* alt. method to spec inverse */
                inverse = 1;
                continue;
              case 'E': /* echo ascii input to ascii output */
                echoin = 1;
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
                    for (lp = pj_get_list_ref() ; lp->id ; ++lp)
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
                } else if( arg[1] == 'm') { /* list prime meridians */
                    struct PJ_PRIME_MERIDIANS *lpm;

                    for (lpm = pj_get_prime_meridians_ref(); lpm->id ; ++lpm)
                        (void)printf("%12s %-30s\n",
                                     lpm->id, lpm->defn);
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
              case 'd': /* set debug level */
                if (--argc <= 0) goto noargument;
                pj_ctx_set_debug( pj_get_default_ctx(), atoi(*++argv));
                continue;
              default:
                emess(1, "invalid option: -%c",*arg);
                break;
            }
            break;

        } else if (strcmp(*argv,"+to") == 0 ) {
            have_to_flag = 1;

        } else if (**argv == '+') { /* + argument */
            if( have_to_flag )
            {
                if( to_argc < MAX_PARGS )
                    to_argv[to_argc++] = *argv + 1;
                else
                    emess(1,"overflowed + argument table");
            }
            else 
            {
                if (from_argc < MAX_PARGS)
                    from_argv[from_argc++] = *argv + 1;
                else
                    emess(1,"overflowed + argument table");
            }
        } else /* assumed to be input file name(s) */
            eargv[eargc++] = *argv;
    }
    if (eargc == 0 ) /* if no specific files force sysin */
        eargv[eargc++] = "-";

    /* 
     * If the user has requested inverse, then just reverse the
     * coordinate systems.
     */
    if( inverse )
    {
        int     argcount;
        
        for( i = 0; i < MAX_PARGS; i++ )
        {
            char *arg;

            arg = from_argv[i];
            from_argv[i] = to_argv[i];
            to_argv[i] = arg;
        }

        argcount = from_argc;
        from_argc = to_argc;
        to_argc = argcount;
    }

    if( use_env_locale )
    {
        /* Set locale from environment */
        setlocale(LC_ALL, "");
    }

    if( from_argc == 0 && to_argc != 0 )
    {
        /* we will generate the from proj as the latlong of the +to in a bit */
    }
    else if (!(fromProj = pj_init(from_argc, from_argv)))
    {
        printf( "Using from definition: " );
        for( i = 0; i < from_argc; i++ )
            printf( "%s ", from_argv[i] );
        printf( "\n" );

        emess(3,"projection initialization failure\ncause: %s",
              pj_strerrno(pj_errno));
    }

    if( to_argc == 0 )
    {
        if (!(toProj = pj_latlong_from_proj( fromProj )))
        {
            printf( "Using to definition: " );
            for( i = 0; i < to_argc; i++ )
                printf( "%s ", to_argv[i] );
            printf( "\n" );
            
            emess(3,"projection initialization failure\ncause: %s",
                  pj_strerrno(pj_errno));
        }   
    }
    else if (!(toProj = pj_init(to_argc, to_argv)))
    {
        printf( "Using to definition: " );
        for( i = 0; i < to_argc; i++ )
            printf( "%s ", to_argv[i] );
        printf( "\n" );

        emess(3,"projection initialization failure\ncause: %s",
              pj_strerrno(pj_errno));
    }

    if( from_argc == 0 && toProj != NULL) 
    {
        if (!(fromProj = pj_latlong_from_proj( toProj )))
        {
            printf( "Using to definition: " );
            for( i = 0; i < to_argc; i++ )
                printf( "%s ", to_argv[i] );
            printf( "\n" );
            
            emess(3,"projection initialization failure\ncause: %s",
                  pj_strerrno(pj_errno));
        }   
    }

    if( use_env_locale )
    {
        /* Restore C locale to avoid issues in parsing/outputing numbers*/
        setlocale(LC_ALL, "C");
    }

    if (mon) {
        printf( "%c ---- From Coordinate System ----\n", tag );
        pj_pr_list(fromProj);
        printf( "%c ---- To Coordinate System ----\n", tag );
        pj_pr_list(toProj);
    }

    /* set input formating control */
    if( !fromProj->is_latlong )
        informat = strtod;
    else {
        informat = dmstor;
    }

    if( !toProj->is_latlong && !oform )
        oform = "%.2f";

    /* process input file list */
    for ( ; eargc-- ; ++eargv) {
        if (**eargv == '-') {
            fid = stdin;
            emess_dat.File_name = "<stdin>";

        } else {
            if ((fid = fopen(*eargv, "rt")) == NULL) {
                emess(-2, *eargv, "input file");
                continue;
            }
            emess_dat.File_name = *eargv;
        }
        emess_dat.File_line = 0;
        process(fid);
        fclose(fid);
        emess_dat.File_name = 0;
    }

    if( fromProj != NULL )
        pj_free( fromProj );
    if( toProj != NULL )
        pj_free( toProj );

    pj_deallocate_grids();

    exit(0); /* normal completion */
}

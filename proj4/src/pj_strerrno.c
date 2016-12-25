/* list of projection system pj_errno values */
#include "projects.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

	static char *
pj_err_list[] = {
	"no arguments in initialization list",	/*  -1 */
	"no options found in 'init' file",		/*  -2 */
	"no colon in init= string",			/*  -3 */
	"projection not named",				/*  -4 */
	"unknown projection id",			/*  -5 */
	"effective eccentricity = 1.",			/*  -6 */
	"unknown unit conversion id",			/*  -7 */
	"invalid boolean param argument",		/*  -8 */
	"unknown elliptical parameter name",	        /*  -9 */
	"reciprocal flattening (1/f) = 0",		/* -10 */
	"|radius reference latitude| > 90",		/* -11 */
	"squared eccentricity < 0",			/* -12 */
	"major axis or radius = 0 or not given",	/* -13 */
	"latitude or longitude exceeded limits",	/* -14 */
	"invalid x or y",				/* -15 */
	"improperly formed DMS value",			/* -16 */
	"non-convergent inverse meridional dist",	/* -17 */
	"non-convergent inverse phi2",			/* -18 */
	"acos/asin: |arg| >1.+1e-14",			/* -19 */
	"tolerance condition error",			/* -20 */
	"conic lat_1 = -lat_2",				/* -21 */
	"lat_1 >= 90",					/* -22 */
	"lat_1 = 0",					/* -23 */
	"lat_ts >= 90",					/* -24 */
	"no distance between control points",		/* -25 */
	"projection not selected to be rotated",	/* -26 */
	"W <= 0 or M <= 0",				/* -27 */
	"lsat not in 1-5 range",			/* -28 */
	"path not in range",				/* -29 */
	"h <= 0",					/* -30 */
	"k <= 0",					/* -31 */
	"lat_0 = 0 or 90 or alpha = 90",		/* -32 */
	"lat_1=lat_2 or lat_1=0 or lat_2=90",		/* -33 */
	"elliptical usage required",			/* -34 */
	"invalid UTM zone number",			/* -35 */
	"arg(s) out of range for Tcheby eval",		/* -36 */
	"failed to find projection to be rotated",	/* -37 */
	"failed to load datum shift file",  	        /* -38 */
	"both n & m must be spec'd and > 0",		/* -39 */
	"n <= 0, n > 1 or not specified",		/* -40 */
	"lat_1 or lat_2 not specified",			/* -41 */
	"|lat_1| == |lat_2|",				/* -42 */
	"lat_0 is pi/2 from mean lat",			/* -43 */
	"unparseable coordinate system definition",	/* -44 */
	"geocentric transformation missing z or ellps",	/* -45 */
	"unknown prime meridian conversion id",		/* -46 */
	"illegal axis orientation combination",		/* -47 */
	"point not within available datum shift grids", /* -48 */
	"invalid sweep axis, choose x or y",            /* -49 */
};
	char *
pj_strerrno(int err) 
{
    static char note[50];

    if (err > 0)
#ifdef HAVE_STRERROR
        return strerror(err);
#else
    {   
        sprintf(note,"no system list, errno: %d\n", err);
        return note;
    }
#endif
    else if (err < 0) {
        size_t adjusted_err = - err - 1;
        if (adjusted_err < (sizeof(pj_err_list) / sizeof(char *)))
            return(pj_err_list[adjusted_err]);
        else
        {
            sprintf( note, "invalid projection system error (%d)",
                     err );
            return note;
        }
    } else
        return NULL;
}

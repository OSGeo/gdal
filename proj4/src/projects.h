/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Primary (private) include file for PROJ.4 library.
 * Author:   Gerald Evenden
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

/* General projections header file */
#ifndef PROJECTS_H
#define PROJECTS_H

#ifdef _MSC_VER
#  ifndef _CRT_SECURE_NO_DEPRECATE
#    define _CRT_SECURE_NO_DEPRECATE
#  endif
#  ifndef _CRT_NONSTDC_NO_DEPRECATE
#    define _CRT_NONSTDC_NO_DEPRECATE
#  endif
/* enable predefined math constants M_* for MS Visual Studio workaround */
#  define _USE_MATH_DEFINES
#endif

/* standard inclusions */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#define C_NAMESPACE extern "C"
#define C_NAMESPACE_VAR extern "C"
extern "C" {
#else
#define C_NAMESPACE extern
#define C_NAMESPACE_VAR
#endif

#ifndef NULL
#  define NULL 0
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE  1
#endif

#ifndef MAX
#  define MIN(a,b)      ((a<b) ? a : b)
#  define MAX(a,b)      ((a>b) ? a : b)
#endif

#ifndef ABS
#  define ABS(x)        ((x<0) ? (-1*(x)) : x)
#endif

/* maximum path/filename */
#ifndef MAX_PATH_FILENAME
#define MAX_PATH_FILENAME 1024
#endif

/* prototype hypot for systems where absent */
#ifndef _WIN32
extern double hypot(double, double);
#endif

#ifdef _WIN32_WCE
#  include <wce_stdlib.h>
#  include <wce_stdio.h>
#  define rewind wceex_rewind
#  define getenv wceex_getenv
#  define strdup _strdup
#  define hypot _hypot
#endif

/* If we still haven't got M_PI*, we rely on our own defines.
 * For example, this is necessary when compiling with gcc and
 * the -ansi flag.
 */
#ifndef M_PI
#define M_PI            3.14159265358979310
#define M_PI_2          1.57079632679489660
#define M_PI_4          0.78539816339744828
#endif

/* some more useful math constants and aliases */
#define M_FORTPI        M_PI_4                   /* pi/4 */
#define M_HALFPI        M_PI_2                   /* pi/2 */
#define M_PI_HALFPI     4.71238898038468985769   /* 1.5*pi */
#define M_TWOPI         6.28318530717958647693   /* 2*pi */
#define M_TWO_D_PI      M_2_PI                   /* 2/pi */
#define M_TWOPI_HALFPI  7.85398163397448309616   /* 2.5*pi */


/* maximum tag id length for +init and default files */
#ifndef ID_TAG_MAX
#define ID_TAG_MAX 50
#endif

/* Use WIN32 as a standard windows 32 bit declaration */
#if defined(_WIN32) && !defined(WIN32) && !defined(_WIN32_WCE)
#  define WIN32
#endif

#if defined(_WINDOWS) && !defined(WIN32) && !defined(_WIN32_WCE)
#  define WIN32
#endif

/* directory delimiter for DOS support */
#ifdef WIN32
#define DIR_CHAR '\\'
#else
#define DIR_CHAR '/'
#endif

struct projFileAPI_t;

/* proj thread context */
typedef struct {
    int     last_errno;
    int     debug_level;
    void    (*logger)(void *, int, const char *);
    void    *app_data;
    struct projFileAPI_t *fileapi;
} projCtx_t;

/* datum_type values */
#define PJD_UNKNOWN   0
#define PJD_3PARAM    1
#define PJD_7PARAM    2
#define PJD_GRIDSHIFT 3
#define PJD_WGS84     4   /* WGS84 (or anything considered equivelent) */

/* library errors */
#define PJD_ERR_GEOCENTRIC          -45
#define PJD_ERR_AXIS                -47
#define PJD_ERR_GRID_AREA           -48
#define PJD_ERR_CATALOG             -49

#define USE_PROJUV

typedef struct { double u, v; } projUV;
typedef struct { double r, i; } COMPLEX;
typedef struct { double u, v, w; } projUVW;

#ifndef PJ_LIB__
#define XY projUV
#define LP projUV
#define XYZ projUVW
#define LPZ projUVW
#else
typedef struct { double x, y; }     XY;
typedef struct { double lam, phi; } LP;
typedef struct { double x, y, z; } XYZ;
typedef struct { double lam, phi, z; } LPZ;
#endif

typedef union { double  f; int  i; char *s; } PROJVALUE;
struct PJconsts;

struct PJ_LIST {
    char             *id;                         /* projection keyword */
    struct PJconsts  *(*proj)(struct PJconsts*);  /* projection entry point */
    char * const     *descr;                      /* description text */
};

/* Merging this into the PJ_LIST infrastructure is tempting, but may imply ABI breakage. Perhaps at next major version? */
struct PJ_SELFTEST_LIST {
    char    *id;                    /* projection keyword */
    int     (* testfunc)(void);     /* projection entry point */
};

struct PJ_ELLPS {
    char    *id;        /* ellipse keyword name */
    char    *major;     /* a= value */
    char    *ell;       /* elliptical parameter */
    char    *name;      /* comments */
};
struct PJ_UNITS {
    char    *id;        /* units keyword */
    char    *to_meter;  /* multiply by value to get meters */
    char    *name;      /* comments */
};

struct PJ_DATUMS {
    char    *id;        /* datum keyword */
    char    *defn;      /* ie. "to_wgs84=..." */
    char    *ellipse_id;/* ie from ellipse table */
    char    *comments;  /* EPSG code, etc */
};

struct PJ_PRIME_MERIDIANS {
    char    *id;        /* prime meridian keyword */
    char    *defn;      /* offset from greenwich in DMS format. */
};

typedef struct {
    double ll_long;      /* lower left corner coordinates (radians) */
    double ll_lat;
    double ur_long;      /* upper right corner coordinates (radians) */
    double ur_lat;
} PJ_Region;

struct DERIVS {
    double x_l, x_p;    /* derivatives of x for lambda-phi */
    double y_l, y_p;    /* derivatives of y for lambda-phi */
};

struct FACTORS {
    struct DERIVS der;
    double h, k;        /* meridinal, parallel scales */
    double omega, thetap;   /* angular distortion, theta prime */
    double conv;        /* convergence */
    double s;           /* areal scale factor */
    double a, b;        /* max-min scale error */
    int code;           /* info as to analytics, see following */
};

#define IS_ANAL_XL_YL 01    /* derivatives of lon analytic */
#define IS_ANAL_XP_YP 02    /* derivatives of lat analytic */
#define IS_ANAL_HK    04    /* h and k analytic */
#define IS_ANAL_CONV 010    /* convergence analytic */

/* parameter list struct */
typedef struct ARG_list {
    struct ARG_list *next;
    char used;
    char param[1]; } paralist;

/* base projection data structure */
#ifdef PJ_LIB__
    /* we need this forward declaration in order to be able to add a
       pointer to struct opaque to the typedef struct PJconsts below */
    struct pj_opaque;
#endif

typedef struct PJconsts {
    projCtx_t *ctx;
    XY  (*fwd)(LP, struct PJconsts *);
    LP  (*inv)(XY, struct PJconsts *);
    XYZ (*fwd3d)(LPZ, struct PJconsts *);
    LPZ (*inv3d)(XYZ, struct PJconsts *);
    void (*spc)(LP, struct PJconsts *, struct FACTORS *);
    void (*pfree)(struct PJconsts *);

    const char *descr;
    paralist *params;           /* parameter list */
    int over;                   /* over-range flag */
    int geoc;                   /* geocentric latitude flag */
    int is_latlong;             /* proj=latlong ... not really a projection at all */
    int is_geocent;             /* proj=geocent ... not really a projection at all */
    double a;                   /* major axis or radius if es==0 */
    double a_orig;              /* major axis before any +proj related adjustment */
    double es;                  /* e ^ 2 */
    double es_orig;             /* es before any +proj related adjustment */
    double e;                   /* eccentricity */
    double ra;                  /* 1/A */
    double one_es;              /* 1 - e^2 */
    double rone_es;             /* 1/one_es */
    double lam0, phi0;          /* central longitude, latitude */
    double x0, y0;              /* easting and northing */
    double k0;                  /* general scaling factor */
    double to_meter, fr_meter;  /* cartesian scaling */

    int     datum_type;         /* PJD_UNKNOWN/3PARAM/7PARAM/GRIDSHIFT/WGS84 */
    double  datum_params[7];
    struct _pj_gi **gridlist;
    int     gridlist_count;

    int     has_geoid_vgrids;
    struct _pj_gi **vgridlist_geoid;
    int     vgridlist_geoid_count;
    double  vto_meter, vfr_meter;

    double  from_greenwich;     /* prime meridian offset (in radians) */
    double  long_wrap_center;   /* 0.0 for -180 to 180, actually in radians*/
    int     is_long_wrap_set;
    char    axis[4];

    /* New Datum Shift Grid Catalogs */
    char   *catalog_name;
    struct _PJ_GridCatalog *catalog;

    double   datum_date;

    struct _pj_gi *last_before_grid;
    PJ_Region     last_before_region;
    double        last_before_date;

    struct _pj_gi *last_after_grid;
    PJ_Region     last_after_region;
    double        last_after_date;

#ifdef PJ_LIB__
        struct pj_opaque *opaque;
#endif

#ifdef PROJ_PARMS__
PROJ_PARMS__
#endif /* end of optional extensions */
} PJ;

/* public API */
#include "proj_api.h"


/* Generate pj_list external or make list from include file */

#ifndef USE_PJ_LIST_H
extern struct PJ_LIST pj_list[];
extern struct PJ_SELFTEST_LIST pj_selftest_list[];
#endif



#ifndef PJ_ELLPS__
extern struct PJ_ELLPS pj_ellps[];
#endif

#ifndef PJ_UNITS__
extern struct PJ_UNITS pj_units[];
#endif

#ifndef PJ_DATUMS__
extern struct PJ_DATUMS pj_datums[];
extern struct PJ_PRIME_MERIDIANS pj_prime_meridians[];
#endif

#ifdef PJ_LIB__
/* repetitive projection code */
#define PROJ_HEAD(id, name) static const char des_##id [] = name
#define ENTRYA(name) \
    C_NAMESPACE_VAR const char * const pj_s_##name = des_##name; \
    C_NAMESPACE PJ *pj_##name(PJ *P) { if (!P) { \
    if( (P = (PJ*) pj_malloc(sizeof(PJ))) != NULL) { \
        memset( P, 0, sizeof(PJ) ); \
    P->pfree = freeup; P->fwd = 0; P->inv = 0; \
        P->fwd3d = 0; P->inv3d = 0; \
    P->spc = 0; P->descr = des_##name;
#define ENTRYX } return P; } else {
#define ENTRY0(name) ENTRYA(name) ENTRYX
#define ENTRY1(name, a) ENTRYA(name) P->a = 0; ENTRYX
#define ENTRY2(name, a, b) ENTRYA(name) P->a = 0; P->b = 0; ENTRYX
#define ENDENTRY(p) } return (p); }
#define E_ERROR(err) { pj_ctx_set_errno( P->ctx, err); freeup(P); return(0); }
#define E_ERROR_0 { freeup(P); return(0); }
#define F_ERROR { pj_ctx_set_errno( P->ctx, -20); return(xy); }
#define F3_ERROR { pj_ctx_set_errno( P->ctx, -20); return(xyz); }
#define I_ERROR { pj_ctx_set_errno( P->ctx, -20); return(lp); }
#define I3_ERROR { pj_ctx_set_errno( P->ctx, -20); return(lpz); }
#define FORWARD(name) static XY name(LP lp, PJ *P) { XY xy = {0.0,0.0}
#define INVERSE(name) static LP name(XY xy, PJ *P) { LP lp = {0.0,0.0}
#define FORWARD3D(name) static XYZ name(LPZ lpz, PJ *P) {XYZ xyz = {0.0, 0.0, 0.0}
#define INVERSE3D(name) static LPZ name(XYZ xyz, PJ *P) {LPZ lpz = {0.0, 0.0, 0.0}
#define FREEUP static void freeup(PJ *P) {
#define SPECIAL(name) static void name(LP lp, PJ *P, struct FACTORS *fac)
#define ELLIPSOIDAL(P) ((P->es==0)? (FALSE): (TRUE))

/* cleaned up alternative to most of the "repetitive projection code" macros */
#define PROJECTION(name)                                     \
pj_projection_specific_setup_##name (PJ *P);                 \
C_NAMESPACE_VAR const char * const pj_s_##name = des_##name; \
C_NAMESPACE PJ *pj_##name (PJ *P) {                          \
    if (P)                                                   \
        return pj_projection_specific_setup_##name (P);      \
    P = (PJ*) pj_calloc (1, sizeof(PJ));                     \
    if (0==P)                                                \
        return 0;                                            \
    P->pfree = freeup;                                       \
    P->descr = des_##name;                                   \
    return P;                                                \
}                                                            \
PJ *pj_projection_specific_setup_##name (PJ *P)

#endif


int pj_generic_selftest (
    char *e_args,
    char *s_args,
    double tolerance_xy,
    double tolerance_lp,
    int n_fwd,
    int n_inv,
    LP *fwd_in,
    XY *e_fwd_expect,
    XY *s_fwd_expect,
    XY *inv_in,
    LP *e_inv_expect,
    LP *s_inv_expect
);




#define MAX_TAB_ID 80
typedef struct { float lam, phi; } FLP;
typedef struct { int lam, phi; } ILP;

struct CTABLE {
    char id[MAX_TAB_ID];    /* ascii info */
    LP ll;                  /* lower left corner coordinates */
    LP del;                 /* size of cells */
    ILP lim;                /* limits of conversion matrix */
    FLP *cvs;               /* conversion matrix */
};

typedef struct _pj_gi {
    char *gridname;     /* identifying name of grid, eg "conus" or ntv2_0.gsb */
    char *filename;     /* full path to filename */

    const char *format; /* format of this grid, ie "ctable", "ntv1",
                           "ntv2" or "missing". */

    int   grid_offset;  /* offset in file, for delayed loading */
    int   must_swap;    /* only for NTv2 */

    struct CTABLE *ct;

    struct _pj_gi *next;
    struct _pj_gi *child;
} PJ_GRIDINFO;

typedef struct {
    PJ_Region region;
    int  priority;      /* higher used before lower */
    double date;        /* year.fraction */
    char *definition;   /* usually the gridname */

    PJ_GRIDINFO  *gridinfo;
    int available;      /* 0=unknown, 1=true, -1=false */
} PJ_GridCatalogEntry;

typedef struct _PJ_GridCatalog {
    char *catalog_name;

    PJ_Region region;   /* maximum extent of catalog data */

    int entry_count;
    PJ_GridCatalogEntry *entries;

    struct _PJ_GridCatalog *next;
} PJ_GridCatalog;


/* procedure prototypes */
double dmstor(const char *, char **);
double dmstor_ctx(projCtx ctx, const char *, char **);
void set_rtodms(int, int);
char *rtodms(char *, double, int, int);
double adjlon(double);
double aacos(projCtx,double), aasin(projCtx,double), asqrt(double), aatan2(double, double);
PROJVALUE pj_param(projCtx ctx, paralist *, const char *);
paralist *pj_mkparam(char *);
int pj_ell_set(projCtx ctx, paralist *, double *, double *);
int pj_datum_set(projCtx,paralist *, PJ *);
int pj_prime_meridian_set(paralist *, PJ *);
int pj_angular_units_set(paralist *, PJ *);
void pj_prepare (PJ *P, const char *description, void (*freeup)(struct PJconsts *), size_t sizeof_struct_opaque);

paralist *pj_clone_paralist( const paralist* );
paralist*pj_search_initcache( const char *filekey );
void pj_insert_initcache( const char *filekey, const paralist *list);

double *pj_enfn(double);
double pj_mlfn(double, double, double, double *);
double pj_inv_mlfn(projCtx, double, double, double *);
double pj_qsfn(double, double, double);
double pj_tsfn(double, double, double);
double pj_msfn(double, double, double);
double pj_phi2(projCtx, double, double);
double pj_qsfn_(double, PJ *);
double *pj_authset(double);
double pj_authlat(double, double *);
COMPLEX pj_zpoly1(COMPLEX, COMPLEX *, int);
COMPLEX pj_zpolyd1(COMPLEX, COMPLEX *, int, COMPLEX *);

int pj_deriv(LP, double, PJ *, struct DERIVS *);
int pj_factors(LP, PJ *, double, struct FACTORS *);

struct PW_COEF {    /* row coefficient structure */
    int m;          /* number of c coefficients (=0 for none) */
    double *c;      /* power coefficients */
};

/* Approximation structures and procedures */
typedef struct {    /* Chebyshev or Power series structure */
    projUV a, b;    /* power series range for evaluation */
                    /* or Chebyshev argument shift/scaling */
    struct PW_COEF *cu, *cv;
    int mu, mv;     /* maximum cu and cv index (+1 for count) */
    int power;      /* != 0 if power series, else Chebyshev */
} Tseries;
Tseries *mk_cheby(projUV, projUV, double, projUV *, projUV (*)(projUV), int, int, int);
projUV bpseval(projUV, Tseries *);
projUV bcheval(projUV, Tseries *);
projUV biveval(projUV, Tseries *);
void *vector1(int, int);
void **vector2(int, int, int);
void freev2(void **v, int nrows);
int bchgen(projUV, projUV, int, int, projUV **, projUV(*)(projUV));
int bch2bps(projUV, projUV, projUV **, int, int);

/* nadcon related protos */
LP nad_intr(LP, struct CTABLE *);
LP nad_cvt(LP, int, struct CTABLE *);
struct CTABLE *nad_init(projCtx ctx, char *);
struct CTABLE *nad_ctable_init( projCtx ctx, PAFile fid );
int nad_ctable_load( projCtx ctx, struct CTABLE *, PAFile fid );
struct CTABLE *nad_ctable2_init( projCtx ctx, PAFile fid );
int nad_ctable2_load( projCtx ctx, struct CTABLE *, PAFile fid );
void nad_free(struct CTABLE *);

/* higher level handling of datum grid shift files */

int pj_apply_vgridshift( PJ *defn, const char *listname,
                         PJ_GRIDINFO ***gridlist_p,
                         int *gridlist_count_p,
                         int inverse,
                         long point_count, int point_offset,
                         double *x, double *y, double *z );
int pj_apply_gridshift_2( PJ *defn, int inverse,
                          long point_count, int point_offset,
                          double *x, double *y, double *z );
int pj_apply_gridshift_3( projCtx ctx,
                          PJ_GRIDINFO **gridlist, int gridlist_count,
                          int inverse, long point_count, int point_offset,
                          double *x, double *y, double *z );

PJ_GRIDINFO **pj_gridlist_from_nadgrids( projCtx, const char *, int * );
void pj_deallocate_grids();

PJ_GRIDINFO *pj_gridinfo_init( projCtx, const char * );
int pj_gridinfo_load( projCtx, PJ_GRIDINFO * );
void pj_gridinfo_free( projCtx, PJ_GRIDINFO * );

PJ_GridCatalog *pj_gc_findcatalog( projCtx, const char * );
PJ_GridCatalog *pj_gc_readcatalog( projCtx, const char * );
void pj_gc_unloadall( projCtx );
int pj_gc_apply_gridshift( PJ *defn, int inverse,
                           long point_count, int point_offset,
                           double *x, double *y, double *z );
int pj_gc_apply_gridshift( PJ *defn, int inverse,
                           long point_count, int point_offset,
                           double *x, double *y, double *z );

PJ_GRIDINFO *pj_gc_findgrid( projCtx ctx,
                             PJ_GridCatalog *catalog, int after,
                             LP location, double date,
                             PJ_Region *optional_region,
                             double *grid_date );

double pj_gc_parsedate( projCtx, const char * );

void *proj_mdist_ini(double);
double proj_mdist(double, double, double, const void *);
double proj_inv_mdist(projCtx ctx, double, const void *);
void *pj_gauss_ini(double, double, double *,double *);
LP pj_gauss(projCtx, LP, const void *);
LP pj_inv_gauss(projCtx, LP, const void *);

extern char const pj_release[];

struct PJ_ELLPS *pj_get_ellps_ref( void );
struct PJ_DATUMS *pj_get_datums_ref( void );
struct PJ_UNITS *pj_get_units_ref( void );
struct PJ_LIST  *pj_get_list_ref( void );
struct PJ_SELFTEST_LIST  *pj_get_selftest_list_ref ( void );
struct PJ_PRIME_MERIDIANS  *pj_get_prime_meridians_ref( void );

double pj_atof( const char* nptr );
double pj_strtod( const char *nptr, char **endptr );

#ifdef __cplusplus
}
#endif

#endif /* end of basic projections header */

#if !defined(GEOD_INTERFACE_H)
#define GEOD_INTERFACE_H

#include "geodesic.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _IN_GEOD_SET
#  define GEOD_EXTERN extern
#else
#  define GEOD_EXTERN
#endif

GEOD_EXTERN struct geodesic {
  double A, FLAT, LAM1, PHI1, ALPHA12, LAM2, PHI2, ALPHA21, DIST;
} GEODESIC;

# define geod_a	GEODESIC.A
# define geod_f	GEODESIC.FLAT
# define lam1	GEODESIC.LAM1
# define phi1	GEODESIC.PHI1
# define al12	GEODESIC.ALPHA12
# define lam2	GEODESIC.LAM2
# define phi2	GEODESIC.PHI2
# define al21	GEODESIC.ALPHA21
# define geod_S	GEODESIC.DIST
    
GEOD_EXTERN struct geod_geodesic GlobalGeodesic;
GEOD_EXTERN struct geod_geodesicline GlobalGeodesicLine;
GEOD_EXTERN int n_alpha, n_S;
GEOD_EXTERN double to_meter, fr_meter, del_alpha;
	
void geod_set(int, char **);
void geod_ini(void);
void geod_pre(void);
void geod_for(void);
void geod_inv(void);

#ifdef __cplusplus
}
#endif

#endif

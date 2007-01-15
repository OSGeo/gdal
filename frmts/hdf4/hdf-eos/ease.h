#ifndef EASE_H_
#define EASE_H_

#include <math.h>
#define BCEA_COLS25 1383 /* total number of columns for EASE grid */
#define BCEA_ROWS25 586  /* total number of rows for EASE grid */
#define BCEA_CELL_M 25067.525 /* Cell size for EASE grid */
#define BCEA_RE_M 6371228.0 /* Earth radius used in GCTP projection tools for
			       Behrmann Cylindrical Equal Area projection */
#define DEFAULT_BCEA_LTRUESCALE 30.00  /*Latitude of true scale in DMS */
#define BCEA_COS_PHI1 cos(DEFAULT_BCEA_LTRUESCALE *3.141592653589793238 /180.0)
#define PI      3.141592653589793238
#define EASE_GRID_DEFAULT_UPLEFT_LON -180.0
#define EASE_GRID_DEFAULT_UPLEFT_LAT  86.72 
#define EASE_GRID_DEFAULT_LOWRGT_LON  180.0
#define EASE_GRID_DEFAULT_LOWRGT_LAT -86.72

#endif  /* #ifndef EASE_H_ */

/*****************************************************************************
 * scan.h
 *
 * DESCRIPTION
 *    This file contains the code that is used to assist with handling the
 * possible scan values of the grid.
 *
 * HISTORY
 *    9/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef SCAN_H
#define SCAN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef GRIB2BIT_ENUM
#define GRIB2BIT_ENUM
/* See rule (8) bit 1 is most significant, bit 8 least significant. */
enum {GRIB2BIT_1=128, GRIB2BIT_2=64, GRIB2BIT_3=32, GRIB2BIT_4=16,
      GRIB2BIT_5=8, GRIB2BIT_6=4, GRIB2BIT_7=2, GRIB2BIT_8=1};
#endif

#include "type.h"

void XY2ScanIndex (sInt4 *Row, sInt4 x, sInt4 y, uChar scan, sInt4 Nx,
                   sInt4 Ny);

void ScanIndex2XY (sInt4 row, sInt4 *X, sInt4 *Y, uChar scan, sInt4 Nx,
                   sInt4 Ny);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SCAN_H */

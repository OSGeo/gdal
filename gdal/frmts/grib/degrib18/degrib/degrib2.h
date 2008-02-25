/*****************************************************************************
 * degrib2.h
 *
 * DESCRIPTION
 *    This file contains the main driver routines to call the unpack grib2
 * library functions.  It also contains the code needed to figure out the
 * dimensions of the arrays before calling the FORTRAN library.
 *
 * HISTORY
 *    9/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef DEGRIB2_H
#define DEGRIB2_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
/* Include type.h for uChar and sChar */
#include "type.h"
#include "meta.h"

#include "datasource.h"

/* The IS_dataType is used to organize and allocate all the arrays that the
 * unpack library uses. */
typedef struct {
   sInt4 ns[8];         /* Size of each section in bytes. */
   sInt4 *is[8];        /* Section data as sInt4s. */
   sInt4 nd2x3;         /* Nx * Ny */
/*  float *ain;  */    /* Size = Nd2x3. Holds the unpacked array if float. */
   sInt4 *iain;         /* Size = Nd2x3. Holds the unpacked array if int. */
   sInt4 *ib;           /* Size = Nd2x3. Hold and bitmasks.. */
   sInt4 nidat;         /* Size of section 2 data if int. */
   sInt4 *idat;         /* Section 2 data if int */
   sInt4 nrdat;         /* Size of section 2 data if float. */
   float *rdat;         /* Section 2 data if float. */
   sInt4 *ipack;        /* The grib2 message in MSB as a sInt4 (input) */
   sInt4 ipackLen;      /* The length of ipack. */
   sInt4 nd5;           /* Size of current GRIB message rounded up to the
                         * nearest sInt4. nd5 <= ipackLen */
} IS_dataType;

void IS_Init (IS_dataType *is);
void IS_Free (IS_dataType *is);

/*
 * WMO_HEADER_LEN should be 19 + 21 + 21 (+19?) bytes for the first header
 * WMO_SECOND_LEN should be 21 (+19?) bytes for subsequent ones.
 * WMO_ORIG_LEN was 21, so I "grandfathered that in, in ReadSECT0.
 * GRIB_LIMIT how many bytes to search for the GRIB message before giving up.
 */
#define WMO_HEADER_LEN 80
#define WMO_SECOND_LEN 40
#define WMO_HEADER_ORIG_LEN 21
#define GRIB_LIMIT 300

#define SECT0LEN_WORD 4
/* Possible error messages left in errSprintf() */
int ReadSECT0 (DataSource &fp, char **buff, uInt4 *buffLen, sInt4 limit,
               sInt4 sect0[SECT0LEN_WORD], uInt4 *gribLen,
               int *version);

/* Possible error messages left in errSprintf() */
int ReadGrib2Record (DataSource &fp, sChar f_unit, double **Grib_Data,
                     uInt4 *grib_DataLen, grib_MetaData * meta,
                     IS_dataType * IS, int subgNum, double majEarth,
                     double minEarth, int simpVer, sInt4 * f_endMsg,
                     LatLon *lwlf, LatLon *uprt);

/* Possible error messages left in errSprintf() */
int FindGRIBMsg (DataSource &fp, int msg, sInt4 *offset, int *curMsg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* DEGRIB2_H */

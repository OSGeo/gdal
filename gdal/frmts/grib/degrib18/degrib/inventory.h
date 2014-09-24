/*****************************************************************************
 * inventory.h
 *
 * DESCRIPTION
 *    This file contains the code needed to do a quick inventory of the GRIB2
 * file.  The intent is to enable one to figure out which message in a GRIB
 * file one is after without needing to call the FORTRAN library.
 *
 * HISTORY
 *    9/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef INVENTORY_H
#define INVENTORY_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <time.h>
#include "type.h"

#include "datasource.h"


typedef struct {
   sChar GribVersion;        /* 1 if GRIB1, 2 if GRIB2, -1 if it is TDLP */
   sInt4 start;           /* Where this message starts in file. */
   unsigned short int msgNum; /* Which "GRIB2" message we are working on. */
   unsigned short int subgNum; /* 0 for the first grid in the GRIB2 message
                              * NOT file, 1 for the second, etc. */
/*   char *wmo;  */          /* The ASCII descriptor string that is before the
                              * "GRIB" part of the message. */
   double refTime;           /* Reference time in seconds UTC */
   double validTime;         /* The ending time, or valid time, in seconds
                              * UTC.  This is specified in template 4.8, 4.9,
                              * for the others it is refTime + foreSec. */
   char *element;            /* Character look up of variable type. */
   char *comment;            /* A more descriptive look up of variable type. */
   char *unitName;           /* The unit of this element. */ 
   double foreSec;           /* Forecast element in seconds. */
   char *shortFstLevel;      /* Short description of the level of this data
                                (above ground) (500 mb), etc */
   char *longFstLevel;       /* Long description of the level of this data
                                (above ground) (500 mb), etc */
} inventoryType;

void GRIB2InventoryFree (inventoryType *inv);

void GRIB2InventoryPrint (inventoryType *Inv, uInt4 LenInv);

/* Possible error messages left in errSprintf() */
int GRIB2Inventory (DataSource &fp, inventoryType ** Inv, uInt4 *LenInv,
                    int numMsg, int *MsgNum);

int GRIB2RefTime (char *filename, double *refTime);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* INVENTORY_H */

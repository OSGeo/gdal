#ifndef METANAME_H
#define METANAME_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "type.h"
#include "meta.h"

const char *centerLookup(unsigned short int center);

const char *subCenterLookup(unsigned short int center,
                            unsigned short int subcenter);

const char *processLookup(unsigned short int center, unsigned char process);

void ParseElemName (unsigned short int center, unsigned short int subcenter,
                    int prodType, int templat, int cat, int subcat,
                    sInt4 lenTime, uChar timeIncrType, uChar genID,
                    uChar probType, double lowerProb, double upperProb,
                    char **name, char **comment, char **unit, int *convert,
                    sChar percentile);

int ComputeUnit (int convert, char * origName, sChar f_unit, double *unitM,
                 double *unitB, char *name);
/*
int ComputeUnit (int prodType, int templat, int cat, int subcat, sChar f_unit,
                 double *unitM, double *unitB, char *name);
*/
typedef struct {
    const char *name, *comment, *unit;
} GRIB2SurfTable;

GRIB2SurfTable Table45Index (int i, int *f_reserved, uShort2 center,
                             uShort2 subcenter);
/*
GRIB2SurfTable Table45Index (int i, int *f_reserved);
int Table45Index (int i);
*/

int IsData_NDFD (unsigned short int center, unsigned short int subcenter);

int IsData_MOS (unsigned short int center, unsigned short int subcenter);

void ParseLevelName (unsigned short int center, unsigned short int subcenter,
                     uChar surfType, double value, sChar f_sndValue,
                     double sndValue, char **shortLevelName,
                     char **longLevelName);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* METANAME_H */

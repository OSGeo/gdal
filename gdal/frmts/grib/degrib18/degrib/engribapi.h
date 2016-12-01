#ifndef ENGRIBAPI_H
#define ENGRIBAPI_H

#include "type.h"

typedef struct {
   sInt4 sec0[2];  /* info for section 0 */
   sInt4 sec1[13]; /* info for section 1 */
   uChar *sec2;   /* section 2 free form info */
   sInt4 lenSec2;  /* length of section 2 free form info */
   sInt4 gds[5];
   sInt4 *gdsTmpl; /* grid definition template (mapgrid) */
   sInt4 lenGdsTmpl; /* length of grid definition template (mapgrid) */
   sInt4 *idefList;
   sInt4 idefnum;

   sInt4 ipdsnum; /* Product Definition Template Number (Code Table 4.0) */
   sInt4 *pdsTmpl; /* Contains the data values for the specified Product
                    * Definition Template (N=ipdsnum).  Each element of this
                    * integer array contains an entry (in the order
                    * specified) of Product Definition Template 4.N */
   sInt4 lenPdsTmpl; /* length of product definition template*/
   float *coordlist; /* Array containing floating point values intended to
                      * document the vertical discretization associated to
                      * model data on hybrid coordinate vertical levels. */
   sInt4 numcoord; /* number of values in array coordlist. */

   sInt4 idrsnum; /* Data Representation Template Number (Code Table 5.0) */
   sInt4 *drsTmpl; /* Contains the data values for the specified Data
                    * Representation Template (N=idrsnum).  Each element of
                    * this integer array contains an entry (in the order
                    * specified) of Data Representation Template 5.N. Note
                    * that some values in this template (eg. reference
                    * values, number of bits, etc...) may be changed by the
                    * data packing algorithms.  Use this to specify scaling
                    * factors and order of spatial differencing, if desired. */
   sInt4 lenDrsTmpl; /* length of data representation template*/

   float *fld;    /* Array of data points to pack. */
   sInt4 ngrdpts; /* Number of data points in grid. i.e. size of fld and bmap. */
   sInt4 ibmap;   /* Bitmap indicator ( see Code Table 6.0 )
                   * 0 = bitmap applies and is included in Section 6.
                   * 1-253 = Predefined bitmap applies
                   * 254 = Previously defined bitmap applies to this field
                   * 255 = Bit map does not apply to this product. */
   sInt4 *bmap;   /* Integer array containing bitmap to be added. (if ibmap=0) */
} enGribMeta;

typedef struct {
   uChar processID;     /* Statistical process method used. */
   uChar incrType;      /* Type of time increment between intervals */
   uChar timeRangeUnit; /* Time range unit. [Code Table 4.4] */
   sInt4 lenTime;       /* Range or length of time interval. */
   uChar incrUnit;      /* Unit of time increment. [Code Table 4.4] */
   sInt4 timeIncr;      /* Time increment between intervals. */
} sect4IntervalType;

void initEnGribMeta (enGribMeta *en);

void freeEnGribMeta (enGribMeta *en);

void fillSect0 (enGribMeta *en, uChar prodType);

void fillSect1 (enGribMeta *en, uShort2 center, uShort2 subCenter,
                uChar mstrVer, uChar lclVer, uChar refCode, sInt4 refYear,
                int refMonth, int refDay, int refHour, int refMin, int refSec,
                uChar prodStat, uChar typeData);

void fillSect2 (enGribMeta *en, uChar *sec2, sInt4 lenSec2);

int fillSect3 (enGribMeta *en, uShort2 tmplNum, double majEarth,
               double minEarth, sInt4 Nx, sInt4 Ny, double lat1, double lon1,
               double lat2, double lon2, double Dx, double Dy, uChar resFlag,
               uChar scanFlag, uChar centerFlag, sInt4 angle, sInt4 subDivis,
               double meshLat, double orientLon, double scaleLat1,
               double scaleLat2, double southLat, double southLon);

int fillSect4_0 (enGribMeta *en, uShort2 tmplNum, uChar cat, uChar subCat,
                 uChar genProcess, uChar bgGenID, uChar genID,
                 uChar f_valCutOff, sInt4 cutOff, uChar timeCode,
                 double foreSec, uChar surfType1, sChar surfScale1,
                 double dSurfVal1, uChar surfType2, sChar surfScale2,
                 double dSurfVal2);
int fillSect4_1 (enGribMeta *en, uShort2 tmplNum, uChar typeEnsemble,
                 uChar perturbNum, uChar numFcsts);
int fillSect4_2 (enGribMeta *en, uShort2 tmplNum, uChar numFcsts,
                 uChar derivedFcst);
int fillSect4_5 (enGribMeta *en, uShort2 tmplNum, uChar numFcsts,
                 uChar foreProbNum, uChar probType, sChar lowScale,
                 double dlowVal, sChar upScale, double dupVal);
int fillSect4_8 (enGribMeta *en, uShort2 tmplNum, sInt4 endYear, int endMonth,
                 int endDay, int endHour, int endMin, int endSec,
                 uChar numInterval, sInt4 numMissing,
                 sect4IntervalType * interval);
int fillSect4_9 (enGribMeta *en, uShort2 tmplNum, uChar numFcsts,
                 uChar foreProbNum, uChar probType, sChar lowScale,
                 double dlowVal, sChar upScale, double dupVal, sInt4 endYear,
                 int endMonth, int endDay, int endHour, int endMin,
                 int endSec, uChar numInterval, sInt4 numMissing,
                 sect4IntervalType * interval);
int fillSect4_10 (enGribMeta *en, uShort2 tmplNum, int percentile,
                  sInt4 endYear, int endMonth, int endDay, int endHour,
                  int endMin, int endSec, uChar numInterval, sInt4 numMissing,
                  sect4IntervalType * interval);
int fillSect4_12 (enGribMeta *en, uShort2 tmplNum, uChar numFcsts,
                  uChar derivedFcst, sInt4 endYear, int endMonth, int endDay,
                  int endHour, int endMin, int endSec, uChar numInterval,
                  sInt4 numMissing, sect4IntervalType * interval);

int fillSect5 (enGribMeta *en, uShort2 tmplNum, sShort2 BSF, sShort2 DSF,
               uChar fieldType, uChar f_miss, float missPri, float missSec,
               uChar orderOfDiff);

int fillGrid (enGribMeta *en, double *data, sInt4 lenData, sInt4 Nx, sInt4 Ny,
              sInt4 ibmap, sChar f_boustify, uChar f_miss, float missPri,
              float missSec);

#endif

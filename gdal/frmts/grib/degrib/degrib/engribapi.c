/*****************************************************************************
 * engrib.c
 *
 * DESCRIPTION
 *    This file contains simple tools to fill out meta data section prior to
 * calling NCEP GRIB2 encoding routines.
 *
 * HISTORY
 *   4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "grib2api.h"
#include "engribapi.h"
#include "myassert.h"
#include "gridtemplates.h"
#include "pdstemplates.h"
#include "drstemplates.h"

#ifdef MEMWATCH
#include "memwatch.h"
#endif

#define GRIB2MISSING_u1 (uChar) (0xff)
#define GRIB2MISSING_s1 (sChar) -1 * (0x7f)
#define GRIB2MISSING_u2 (uShort2) (0xffff)
#define GRIB2MISSING_s2 (sShort2) -1 * (0x7fff)
#define GRIB2MISSING_u4 (uInt4) (0xffffffff)
/* following is -1 * 2&31 because of the way signed integers are stored in
   GRIB2. */
#define GRIB2MISSING_s4 (sInt4) -2147483647
/*
#define GRIB2MISSING_1 (int) (0xff)
#define GRIB2MISSING_2 (int) (0xffff)
#define GRIB2MISSING_4 (sInt4) (0xffffffff)
*/

/*****************************************************************************
 * NearestInt() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find the nearest integer to the given double.
 *
 * ARGUMENTS
 * a = The given float. (Input)
 *
 * RETURNS: sInt4 (The nearest integer)
 *
 *  4/2006 Arthur Taylor (MDL): Commented (copied from pack.c).
 *
 * NOTES:
 *****************************************************************************
 */
static sInt4 NearestInt(double a)
{
   return (sInt4)floor(a + .5);
}

/*****************************************************************************
 * AdjustLon() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Adjust the longitude so that it is in the range of 0..360.
 *
 * ARGUMENTS
 * lon = The given longitude. (Input)
 *
 * RETURNS: double (a longitude in the correct range)
 *
 *  4/2006 Arthur Taylor (MDL): Created
 *
 * NOTES:
 *****************************************************************************
 */
static double AdjustLon (double lon)
{
   while (lon < 0)
      lon += 360;
   while (lon > 360)
      lon -= 360;
   return lon;
}

/*****************************************************************************
 * initEnGribMeta() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Initialize the dynamic memory in the enGribMeta data structure.
 *
 * ARGUMENTS
 * en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *
 * RETURNS: void
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
void initEnGribMeta(enGribMeta * en)
{
   en->sec2 = NULL;
   en->lenSec2 = 0;
   en->gdsTmpl = NULL;
   en->lenGdsTmpl = 0;
   en->idefList = NULL;
   en->idefnum = 0;
   en->pdsTmpl = NULL;
   en->lenPdsTmpl = 0;
   en->coordlist = NULL;
   en->numcoord = 0;
   en->drsTmpl = NULL;
   en->lenDrsTmpl = 0;
   en->fld = NULL;
   en->ngrdpts = 0;
   en->bmap = NULL;
   en->ibmap = GRIB2MISSING_u1;
}

/*****************************************************************************
 * freeEnGribMeta() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Free the dynamic memory in the enGribMeta data structure.
 *
 * ARGUMENTS
 * en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *
 * RETURNS: void
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
void freeEnGribMeta(enGribMeta * en)
{
   if (en->sec2 != NULL) {
      free(en->sec2);
      en->sec2 = NULL;
   }
   en->lenSec2 = 0;
   if (en->gdsTmpl != NULL) {
      free(en->gdsTmpl);
      en->gdsTmpl = NULL;
   }
   en->lenGdsTmpl = 0;
   if (en->idefList != NULL) {
      free(en->idefList);
      en->idefList = NULL;
   }
   en->idefnum = 0;
   if (en->pdsTmpl != NULL) {
      free(en->pdsTmpl);
      en->pdsTmpl = NULL;
   }
   en->lenPdsTmpl = 0;
   if (en->coordlist != NULL) {
      free(en->coordlist);
      en->coordlist = NULL;
   }
   en->numcoord = 0;
   if (en->drsTmpl != NULL) {
      free(en->drsTmpl);
      en->drsTmpl = NULL;
   }
   en->lenDrsTmpl = 0;
   if (en->fld != NULL) {
      free(en->fld);
      en->fld = NULL;
   }
   en->ngrdpts = 0;
   if (en->bmap != NULL) {
      free(en->bmap);
      en->bmap = NULL;
   }
   en->ibmap = GRIB2MISSING_u1;
}

/*****************************************************************************
 * fillSect0() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 0 data.
 *
 * ARGUMENTS
 *       en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 * prodType = Discipline-GRIB Master Table [Code:0.0] (Input)
 *
 * RETURNS: void
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
void fillSect0(enGribMeta * en, uChar prodType)
{
   en->sec0[0] = prodType;
   en->sec0[1] = 2;
}

/*****************************************************************************
 * fillSect1() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 1 data.
 *
 * ARGUMENTS
 *        en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *    center = center of origin of data. (Input)
 * subCenter = subCenter of origin of data. (Input)
 *   mstrVer = GRIB2 master table Version (currently 3) (Input)
 *    lclVer = GRIB2 local table version (typically 0) (Input)
 *   refCode = Significance of refTime [Code:1.2] (Input)
 *   refYear = The year of the reference time. (Input)
 *  refMonth = The month of the reference time. (Input)
 *    refDay = The day of the reference time. (Input)
 *   refHour = The hour of the reference time. (Input)
 *    refMin = The min of the reference time. (Input)
 *    refSec = The sec of the reference time. (Input)
 *  prodStat = Production Status of data [Code:1.3] (Input)
 *  typeData = Type of Data [Code:1.4] (Input)
 *
 * RETURNS: void
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
void fillSect1(enGribMeta * en, uShort2 center, uShort2 subCenter,
               uChar mstrVer, uChar lclVer, uChar refCode, sInt4 refYear,
               int refMonth, int refDay, int refHour, int refMin, int refSec,
               uChar prodStat, uChar typeData)
{
   en->sec1[0] = center;
   en->sec1[1] = subCenter;
   en->sec1[2] = mstrVer;
   en->sec1[3] = lclVer;
   en->sec1[4] = refCode;
   en->sec1[5] = refYear;
   en->sec1[6] = refMonth;
   en->sec1[7] = refDay;
   en->sec1[8] = refHour;
   en->sec1[9] = refMin;
   en->sec1[10] = refSec;
   en->sec1[11] = prodStat;
   en->sec1[12] = typeData;
}

/*****************************************************************************
 * fillSect2() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 2 data.
 *
 * ARGUMENTS
 *      en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *    sec2 = Character array with info to be added in Sect 2. (Input)
 * lenSec2 = Num of bytes of sec2 to be added to Sect 2. (Input)
 *
 * RETURNS: void
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
void fillSect2(enGribMeta * en, uChar *sec2, sInt4 lenSec2)
{
   if (lenSec2 == 0) {
      if (en->sec2 != NULL) {
         free(en->sec2);
         en->sec2 = NULL;
      }
      en->lenSec2 = 0;
      return;
   }
   if (lenSec2 > en->lenSec2) {
      if (en->sec2 != NULL) {
         free(en->sec2);
      }
      en->sec2 = (uChar *)malloc(lenSec2 * sizeof(char));
   }
   en->lenSec2 = lenSec2;
   memcpy(en->sec2, sec2, lenSec2);
}

/*****************************************************************************
 * getShpEarth() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Given a major Earth axis and a minor Earth axis, determine how to store
 * it in GRIB2.
 *
 * ARGUMENTS
 *   majEarth = major axis of earth in km (Input)
 *   minEarth = minor axis of earth in km (Input)
 * shapeEarth = [Code:3.2] shape of the Earth defined by GRIB2 (Output)
 *    factRad = Scale factor of radius of spherical Earth. (Output)
 *     valRad = Value of radius of spherical Earth (Output)
 *    factMaj = Scale factor of major axis of elliptical Earth. (Output)
 *     valMaj = Value of major axis of elliptical Earth (Output)
 *    factMin = Scale factor of minor axis of elliptical Earth. (Output)
 *     valMin = Value of minor axis of elliptical Earth (Output)
 *
 * RETURNS: void
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
static void getShpEarth(double majEarth, double minEarth, sInt4 *shapeEarth,
                        sInt4 *factRad, sInt4 *valRad, sInt4 *factMaj,
                        sInt4 *valMaj, sInt4 *factMin, sInt4 *valMin)
{
   *factRad = 0;
   *factMaj = 0;
   *factMin = 0;
   *valRad = 0;
   *valMaj = 0;
   *valMin = 0;
   if (majEarth == minEarth) {
      if (majEarth == 6367.47) {
         *shapeEarth = 0;
         *valRad = 6367470;
      } else if (majEarth == 6371.229) {
         *shapeEarth = 6;
         *valRad = 6371229;
      } else {
         *shapeEarth = 1;
         *valRad = NearestInt(majEarth * 1000);
      }
   } else {
      if ((majEarth == 6378.16) && (minEarth == 6356.775)) {
         *shapeEarth = 2;
         *valMaj = 6378160;
         *valMin = 6356775;
      } else if ((majEarth == 6378.137) && (minEarth == 6356.752314)) {
         *shapeEarth = 4;
         *valMaj = 6378137;
         /* Should this be 3 or -3? */
         *factMin = 2;
         /* 6 356 752 314 > Max unsigned Long Int */
         *valMin = 635675231;
      } else {
         *shapeEarth = 7;
         *valMaj = NearestInt(majEarth * 1000);
         *valMin = NearestInt(majEarth * 1000);
      }
   }
}

/*****************************************************************************
 * fillSect3() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 3 data.
 *
 * ARGUMENTS
 *         en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *    tmplNum = [Code Table 3.1] Grid Definition Template Number. (Input)
 *   majEarth = major axis of earth in km (Input)
 *   minEarth = minor axis of earth in km (Input)
 *         Nx = Number of X coordinates (Input)
 *         Ny = Number of Y coordinates (Input)
 *       lat1 = latitude of first grid point (Input)
 *       lon1 = longitude of first grid point (Input)
 * [tmplNum=0,10]
 *       lat2 = latitude of last grid point (Input)
 *       lon2 = longitude of last grid point (Input)
 * [tmplNum=*]
 *         Dx = Grid Length in X direction (Input)
 *         Dy = Grid length in Y direction (Input)
 *    resFlag = [Code Table 3.3] Resolution and components flag (Input)
 *              (bit 3 = flag of i direction given)
 *              (bit 4 = flag of j direction given)
 *              (bit 5 = flag of u/v relative to grid) (typically 0)
 *   scanFlag = [Code Table 3.4] Scanning mode flag (Input)
 *              (bit 1 = flag of flow in -i direction)
 *              (bit 2 = flag of flow in +j direction)
 *              (bit 3 = column oriented (varies by column faster than row)
 *              (bit 4 = flag of boustophotonic) (typically 64)
 * [tmplNum=20,30]
 * centerFlag = [Code Table 3.5] Center flag (Input)
 *              (bit 1 = flag of south pole on plane)
 *              (bit 2 = flag of bi-polar projection) (typically 0)
 * [tmplNum=0]
 *      angle = rule 92.1.6 may not hold, in which case, angle != 0, and
 *              unit = angle/subdivision. Typically 0 (Input)
 *   subDivis = 0 or see angle explanation. (Input)
 * [tmplNum=10,20,30]
 *    meshLat = Latitude where Dx and Dy are specified (Input)
 *  orientLon = Orientation of the grid (Input)
 * [tmplNum=30]
 *  scaleLat1 = The tangent latitude.  If differs from scaleLat2, then they
 *              the two are the latitudes where the scale should be equal.
 *              One can compute a tangent latitude so that the scale at
 *              scaleLat1 is the same as the scale at scaleLat2. (Input)
 *  scaleLat2 = see scaleLat1. (Input)
 *   southLat = latitude of the south pole of the projection (Input)
 *   southLon = longitude of the south pole of the projection (Input)
 *
 * RETURNS: int
 *    > 0 (length of section 3).
 *    -1 if numExOctet != 0 (can't handle idefList yet)
 *    -2 not in list of templates supported by NCEP
 *    -3 can't determine the unit. (see angle / subDivis)
 *    -4 haven't finished mapping this projection to the template.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect3(enGribMeta * en, uShort2 tmplNum, double majEarth,
              double minEarth, sInt4 Nx, sInt4 Ny, double lat1, double lon1,
              double lat2, double lon2, double Dx, double Dy, uChar resFlag,
              uChar scanFlag, uChar centerFlag, sInt4 angle, sInt4 subDivis,
              double meshLat, double orientLon, double scaleLat1,
              double scaleLat2, double southLat, double southLon)
{
   const struct gridtemplate *templatesgrid = get_templatesgrid();
   int i;               /* loop counter over number of GDS templates. */
   double unit;         /* Used to convert from stored value to degrees
                         * lat/lon. See GRIB2 Regulation 92.1.6 */

   if (tmplNum == 65535) {
      /* can't handle lack of a grid definition template */
      return -1;
   }
   /* srcGridDef = [Code:3.0] 0 => Use a grid template * 1 => predetermined
    * grid (may not have grid template) * 255 => means no grid applies (no
    * grid def applies) * for 1,255 templateNum = 65535 means no grid
    * template. */
   en->gds[0] = 0;
   en->gds[1] = Nx * Ny;
   /* numExOctet = Number of octets needed for each additional grid points
    * definition.  Used to define number of points in each row (or column)
    * for non-regular grids. 0, if using regular grid. */
   en->gds[2] = 0;
   /* interpList = [Code Table 3.11] Interpretation of list for optional
    * points definition.  0 if no appended list. */
   en->gds[3] = 0;
   en->gds[4] = tmplNum;

   /* Find NCEP's template match */
   for (i = 0; i < MAXGRIDTEMP; i++) {
      if (templatesgrid[i].template_num == tmplNum) {
         break;
      }
   }
   if (i == MAXGRIDTEMP) {
      /* not in list of templates supported by NCEP */
      return -2;
   }
   if (templatesgrid[i].needext) {
      /* can't handle idefList yet. */
      return -1;
   }

   if (en->lenGdsTmpl < templatesgrid[i].mapgridlen) {
      if (en->gdsTmpl != NULL) {
         free(en->gdsTmpl);
      }
      en->gdsTmpl = (sInt4 *)malloc(templatesgrid[i].mapgridlen *
                                    sizeof(sInt4));
   }
   en->lenGdsTmpl = templatesgrid[i].mapgridlen;

   /* using 1 / 10^-6 to reduce division later */
   unit = 1e6;
   /* lat/lon grid */
   if (tmplNum == 0) {
      getShpEarth(majEarth, minEarth, &(en->gdsTmpl[0]), &(en->gdsTmpl[1]),
                  &(en->gdsTmpl[2]), &(en->gdsTmpl[3]), &(en->gdsTmpl[4]),
                  &(en->gdsTmpl[5]), &(en->gdsTmpl[6]));
      en->gdsTmpl[7] = Nx;
      en->gdsTmpl[8] = Ny;
      en->gdsTmpl[9] = angle;
      en->gdsTmpl[10] = subDivis;
      if (angle != 0) {
         if (subDivis == 0) {
            /* can't determine the unit. */
            return -3;
         }
         /* using 1 / (angle / subdivis) to reduce division later */
         unit = subDivis / (double)angle;
      }
      en->gdsTmpl[11] = NearestInt(lat1 * unit);
      en->gdsTmpl[12] = NearestInt(AdjustLon(lon1) * unit);
      en->gdsTmpl[13] = resFlag;
      en->gdsTmpl[14] = NearestInt(lat2 * unit);
      en->gdsTmpl[15] = NearestInt(AdjustLon(lon2) * unit);
      en->gdsTmpl[16] = NearestInt(Dx * unit);
      en->gdsTmpl[17] = NearestInt(Dy * unit);
      en->gdsTmpl[18] = scanFlag;
      return 72;
      /* mercator grid */
   } else if (tmplNum == 10) {
      getShpEarth(majEarth, minEarth, &(en->gdsTmpl[0]), &(en->gdsTmpl[1]),
                  &(en->gdsTmpl[2]), &(en->gdsTmpl[3]), &(en->gdsTmpl[4]),
                  &(en->gdsTmpl[5]), &(en->gdsTmpl[6]));
      en->gdsTmpl[7] = Nx;
      en->gdsTmpl[8] = Ny;
      en->gdsTmpl[9] = NearestInt(lat1 * unit);
      en->gdsTmpl[10] = NearestInt(AdjustLon(lon1) * unit);
      en->gdsTmpl[11] = resFlag;
      en->gdsTmpl[12] = NearestInt(meshLat * unit);
      en->gdsTmpl[13] = NearestInt(lat2 * unit);
      en->gdsTmpl[14] = NearestInt(AdjustLon(lon2) * unit);
      en->gdsTmpl[15] = scanFlag;
      en->gdsTmpl[16] = NearestInt(AdjustLon(orientLon) * unit);
      en->gdsTmpl[17] = NearestInt(Dx * 1000.);
      en->gdsTmpl[18] = NearestInt(Dy * 1000.);
      return 72;
      /* polar grid */
   } else if (tmplNum == 20) {
      getShpEarth(majEarth, minEarth, &(en->gdsTmpl[0]), &(en->gdsTmpl[1]),
                  &(en->gdsTmpl[2]), &(en->gdsTmpl[3]), &(en->gdsTmpl[4]),
                  &(en->gdsTmpl[5]), &(en->gdsTmpl[6]));
      en->gdsTmpl[7] = Nx;
      en->gdsTmpl[8] = Ny;
      en->gdsTmpl[9] = NearestInt(lat1 * unit);
      en->gdsTmpl[10] = NearestInt(AdjustLon(lon1) * unit);
      en->gdsTmpl[11] = resFlag;
      en->gdsTmpl[12] = NearestInt(meshLat * unit);
      en->gdsTmpl[13] = NearestInt(AdjustLon(orientLon) * unit);
      en->gdsTmpl[14] = NearestInt(Dx * 1000.);
      en->gdsTmpl[15] = NearestInt(Dy * 1000.);
      en->gdsTmpl[16] = centerFlag;
      en->gdsTmpl[17] = scanFlag;
      return 65;
      /* lambert grid */
   } else if (tmplNum == 30) {
      getShpEarth(majEarth, minEarth, &(en->gdsTmpl[0]), &(en->gdsTmpl[1]),
                  &(en->gdsTmpl[2]), &(en->gdsTmpl[3]), &(en->gdsTmpl[4]),
                  &(en->gdsTmpl[5]), &(en->gdsTmpl[6]));
      en->gdsTmpl[7] = Nx;
      en->gdsTmpl[8] = Ny;
      en->gdsTmpl[9] = NearestInt(lat1 * unit);
      en->gdsTmpl[10] = NearestInt(AdjustLon(lon1) * unit);
      en->gdsTmpl[11] = resFlag;
      en->gdsTmpl[12] = NearestInt(meshLat * unit);
      en->gdsTmpl[13] = NearestInt(AdjustLon(orientLon) * unit);
      en->gdsTmpl[14] = NearestInt(Dx * 1000.);
      en->gdsTmpl[15] = NearestInt(Dy * 1000.);
      en->gdsTmpl[16] = centerFlag;
      en->gdsTmpl[17] = scanFlag;
      en->gdsTmpl[18] = NearestInt(scaleLat1 * unit);
      en->gdsTmpl[19] = NearestInt(scaleLat2 * unit);
      en->gdsTmpl[20] = NearestInt(southLat * unit);
      en->gdsTmpl[21] = NearestInt(AdjustLon(southLon) * unit);
      return 81;
   }
   /* Haven't finished mapping this projection to the template. */
   return -4;
}

/*****************************************************************************
 * getCodedTime() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *    Change from seconds to the time units provided in timeCode.
 *
 * ARGUMENTS
 * timeCode = The time units to convert into (see code table 4.4). (Input)
 *     time = The time in seconds to convert. (Input)
 *      ans = The converted answer. (Output)
 *
 * RETURNS: int
 *  0 = OK
 * -1 = could not determine.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int getCodedTime(uChar timeCode, double time, sInt4 *ans)
{
   /* Following is a lookup table for unit conversion (see code table 4.4). */
   static const sInt4 unit2sec[] = {
      60, 3600, 86400L, 0, 0,
      0, 0, 0, 0, 0,
      10800, 21600L, 43200L, 1
   };

   if (timeCode < 14) {
      if (unit2sec[timeCode] != 0) {
         *ans = NearestInt(time / unit2sec[timeCode]);
         return 0;
      }
   }
   *ans = 0;
   return -1;
}

/*****************************************************************************
 * fillSect4_0() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 0) data.
 *
 * ARGUMENTS
 *          en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *     tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 *         cat = [Code Table 4.1] General category of Meteo Product. (Input)
 *      subCat = [Code Table 4.2] Specific subcategory of Meteo Product. (In)
 *  genProcess = [Code Table 4.3] type of generating process (Analysis,
 *               Forecast, Probability Forecast, etc) (Input)
 *     bgGenID = Background generating process id. (Input)
 *       genID = Analysis/Forecast generating process id. (Input)
 * f_valCutOff = Flag if we have a valid cutoff time (Input)
 *      cutOff = Cut off time for forecast (Input)
 *    timeCode = [Code Table 4.4] Unit of time to store in (Input)
 *     foreSec = Forecast time in seconds (Input)
 *   surfType1 = [Code Table 4.5] Type of the first surface (Input)
 *  surfScale1 = scale amount for the first surface (Input)
 *   dSurfVal1 = value of the first surface (before scaling) (Input)
 *   surfType2 = [Code Table 4.5] Type of the second surface (Input)
 *  surfScale2 = scale amount for the second surface (Input)
 *   dSurfVal2 = value of the second surface (before scaling) (Input)
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 This is specifically for template 4.0 (1,2,5,8,8,12)
 *    -2 not in list of templates supported by NCEP
 *    -3 can't handle the timeCode.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_0(enGribMeta * en, uShort2 tmplNum, uChar cat, uChar subCat,
                uChar genProcess, uChar bgGenID, uChar genID,
                uChar f_valCutOff, sInt4 cutOff, uChar timeCode,
                double foreSec, uChar surfType1, sChar surfScale1,
                double dSurfVal1, uChar surfType2, sChar surfScale2,
                double dSurfVal2)
{
   int i;               /* loop counter over number of PDS templates. */
   const struct pdstemplate *templatespds = get_templatespds();

   /* analysis template (0) */
   /* In addition templates (1, 2, 5, 8, 9, 12) begin with 4.0 info. */
   if ((tmplNum != 0) && (tmplNum != 1) && (tmplNum != 2) && (tmplNum != 5) &&
       (tmplNum != 8) && (tmplNum != 9) && (tmplNum != 10) &&
       (tmplNum != 12)) {
      /* This is specifically for template 4.0 (1,2,5,8,9,10,12) */
      return -1;
   }
   en->ipdsnum = tmplNum;

   /* Find NCEP's template match */
   for (i = 0; i < MAXPDSTEMP; i++) {
      if (templatespds[i].template_num == tmplNum) {
         break;
      }
   }
   if (i == MAXPDSTEMP) {
      /* not in list of templates supported by NCEP */
      return -2;
   }
   /* Allocate memory for it. */
   if (en->lenPdsTmpl < templatespds[i].mappdslen) {
      if (en->pdsTmpl != NULL) {
         free(en->pdsTmpl);
      }
      en->pdsTmpl = (sInt4 *)malloc(templatespds[i].mappdslen *
                                    sizeof(sInt4));
   }
   en->lenPdsTmpl = templatespds[i].mappdslen;

   en->pdsTmpl[0] = cat;
   en->pdsTmpl[1] = subCat;
   en->pdsTmpl[2] = genProcess;
   en->pdsTmpl[3] = bgGenID;
   en->pdsTmpl[4] = genID;
   if (f_valCutOff) {
      en->pdsTmpl[5] = cutOff / 3600;
      en->pdsTmpl[6] = (cutOff % 3600) / 60;
   } else {
      en->pdsTmpl[5] = GRIB2MISSING_u2;
      en->pdsTmpl[6] = GRIB2MISSING_u1;
   }
   en->pdsTmpl[7] = timeCode;
   if (getCodedTime(timeCode, foreSec, &(en->pdsTmpl[8])) != 0) {
      /* can't handle this time code yet. */
      return -3;
   }
   en->pdsTmpl[9] = surfType1;
   if (surfType1 == GRIB2MISSING_u1) {
      en->pdsTmpl[10] = GRIB2MISSING_s1;
      en->pdsTmpl[11] = GRIB2MISSING_s4;
   } else {
      en->pdsTmpl[10] = surfScale1;
      en->pdsTmpl[11] = NearestInt (dSurfVal1 * pow (10.0, surfScale1));
   }
   en->pdsTmpl[12] = surfType2;
   if (surfType2 == GRIB2MISSING_u1) {
      en->pdsTmpl[13] = GRIB2MISSING_s1;
      en->pdsTmpl[14] = GRIB2MISSING_s4;
   } else {
      en->pdsTmpl[13] = surfScale2;
      en->pdsTmpl[14] = NearestInt (dSurfVal2 * pow (10.0, surfScale2));
   }
   return 34;
}

/*****************************************************************************
 * fillSect4_1() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 1) data.  Call fillSect4_0 first.
 *
 * ARGUMENTS
 *           en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *      tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 * typeEnsemble = [Code Table 4.6] Type of ensemble (Input)
 *   perturbNum = Perturbation number (Input)
 *     numFcsts = number of forecasts (Input)
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 if not template 4.1, or fillSect4_0 was not already called.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_1(enGribMeta * en, uShort2 tmplNum, uChar typeEnsemble,
                uChar perturbNum, uChar numFcsts)
{
   /* Ensemble template (1) */
   if (tmplNum != 1) {
      /* This is specifically for template 4.1 */
      return -1;
   }
   if (en->ipdsnum != tmplNum) {
      /* Didn't call fillSect4_0 first */
      return -1;
   }
   en->pdsTmpl[15] = typeEnsemble;
   en->pdsTmpl[16] = perturbNum;
   en->pdsTmpl[17] = numFcsts;
   return 37;
}

/*****************************************************************************
 * fillSect4_2() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 2) data.  Call fillSect4_0 first.
 *
 * ARGUMENTS
 *          en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *     tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 *    numFcsts = number of forecasts (Input)
 * derivedFcst = [Code Table 4.7] Derived forecast type (Input)
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 if not template 4.2, or fillSect4_0 was not already called.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_2(enGribMeta * en, uShort2 tmplNum, uChar numFcsts,
                uChar derivedFcst)
{
   /* derived template (2) */
   if (tmplNum != 2) {
      /* This is specifically for template 4.2 */
      return -1;
   }
   if (en->ipdsnum != tmplNum) {
      /* Didn't call fillSect4_0 first */
      return -1;
   }
   en->pdsTmpl[15] = derivedFcst;
   en->pdsTmpl[16] = numFcsts;
   return 36;
}

/*****************************************************************************
 * fillSect4_5() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 5) data.  Call fillSect4_0 first.
 *
 * ARGUMENTS
 *          en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *     tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 *    numFcsts = number of forecasts (Input)
 * foreProbNum = Forecast Probability number (Input)
 *    probType = [Code Table 4.9] (Input)
 *               0 probability of event below lower limit
 *               1 probability of event above upper limit
 *               2 probability of event between lower (inclusive) and upper
 *               3 probability of event above lower limit
 *               4 probability of event below upper limit
 *    lowScale = scale amount for the lower limit (Input)
 *     dlowVal = value of the lower limit (before scaling) (Input)
 *     upScale = scale amount for the upper limit (Input)
 *      dupVal = value of the upper limit (before scaling) (Input)
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 if not template 4.5, or fillSect4_0 was not already called.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_5(enGribMeta * en, uShort2 tmplNum, uChar numFcsts,
                uChar foreProbNum, uChar probType, sChar lowScale,
                double dlowVal, sChar upScale, double dupVal)
{
   /* Point Probability template */
   if (tmplNum != 5) {
      /* This is specifically for template 4.5 */
      return -1;
   }
   if (en->ipdsnum != tmplNum) {
      /* Didn't call fillSect4_0 first */
      return -1;
   }
   en->pdsTmpl[15] = foreProbNum;
   en->pdsTmpl[16] = numFcsts;
   en->pdsTmpl[17] = probType;
   if (lowScale == GRIB2MISSING_s1) {
      en->pdsTmpl[18] = GRIB2MISSING_s1;
      en->pdsTmpl[19] = GRIB2MISSING_s4;
   } else {
      en->pdsTmpl[18] = lowScale;
      en->pdsTmpl[19] = NearestInt (dlowVal * pow (10.0, lowScale));
   }
   if (upScale == GRIB2MISSING_s1) {
      en->pdsTmpl[20] = GRIB2MISSING_s1;
      en->pdsTmpl[21] = GRIB2MISSING_s4;
   } else {
      en->pdsTmpl[20] = upScale;
      en->pdsTmpl[21] = NearestInt (dupVal * pow (10.0, upScale));
   }
   return 47;
}

/*****************************************************************************
 * fillSect4_8() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 8) data.  Call fillSect4_0 first.
 *
 * ARGUMENTS
 *          en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *     tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 *     endYear = The year of the end time (valid Time). (Input)
 *    endMonth = The month of the end time (valid Time). (Input)
 *      endDay = The day of the end time (valid Time). (Input)
 *     endHour = The hour of the end time (valid Time). (Input)
 *      endMin = The min of the end time (valid Time). (Input)
 *      endSec = The sec of the end time (valid Time). (Input)
 * numInterval = num of time range specifications (Has to = 1) (Input)
 *  numMissing = total num of missing values in statistical process (Input)
 *    interval = time range intervals.
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 if not template 4.8, or fillSect4_0 was not already called.
 *    -4 can only handle 1 and only 1 time interval
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_8 (enGribMeta *en, uShort2 tmplNum, sInt4 endYear, int endMonth,
                 int endDay, int endHour, int endMin, int endSec,
                 uChar numInterval, sInt4 numMissing,
                 sect4IntervalType * interval)
{
   int j;               /* loop counter over number of intervals. */

   /* statistic template (8) */
   if (tmplNum != 8) {
      /* This is specifically for template 4.8 */
      return -1;
   }
   if (en->ipdsnum != tmplNum) {
      /* Didn't call fillSect4_0 first */
      return -1;
   }
   en->pdsTmpl[15] = endYear;
   en->pdsTmpl[16] = endMonth;
   en->pdsTmpl[17] = endDay;
   en->pdsTmpl[18] = endHour;
   en->pdsTmpl[19] = endMin;
   en->pdsTmpl[20] = endSec;
   en->pdsTmpl[21] = numInterval;
   if (numInterval != 1) {
      /* can only handle 1 and only 1 time interval */
      return -4;
   }
   en->pdsTmpl[22] = numMissing;
   for (j = 0; j < numInterval; j++) {
      en->pdsTmpl[23] = interval[j].processID;
      en->pdsTmpl[24] = interval[j].incrType;
      en->pdsTmpl[25] = interval[j].timeRangeUnit;
      en->pdsTmpl[26] = interval[j].lenTime;
      en->pdsTmpl[27] = interval[j].incrUnit;
      en->pdsTmpl[28] = interval[j].timeIncr;
   }
   return 58;
}

/*****************************************************************************
 * fillSect4_9() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 9) data.  Call fillSect4_0 first.
 *
 * ARGUMENTS
 *          en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *     tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 *    numFcsts = number of forecasts (Input)
 * foreProbNum = Forecast Probability number (Input)
 *    probType = [Code Table 4.9] (Input)
 *               0 probability of event below lower limit
 *               1 probability of event above upper limit
 *               2 probability of event between lower (inclusive) and upper
 *               3 probability of event above lower limit
 *               4 probability of event below upper limit
 *    lowScale = scale amount for the lower limit (Input)
 *     dlowVal = value of the lower limit (before scaling) (Input)
 *     upScale = scale amount for the upper limit (Input)
 *      dupVal = value of the upper limit (before scaling) (Input)
 *     endYear = The year of the end time (valid Time). (Input)
 *    endMonth = The month of the end time (valid Time). (Input)
 *      endDay = The day of the end time (valid Time). (Input)
 *     endHour = The hour of the end time (valid Time). (Input)
 *      endMin = The min of the end time (valid Time). (Input)
 *      endSec = The sec of the end time (valid Time). (Input)
 * numInterval = num of time range specifications (Has to = 1) (Input)
 *  numMissing = total num of missing values in statistical process (Input)
 *    interval = time range intervals.
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 if not template 4.9, or fillSect4_0 was not already called.
 *    -4 can only handle 1 and only 1 time interval
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_9(enGribMeta * en, uShort2 tmplNum, uChar numFcsts,
                uChar foreProbNum, uChar probType, sChar lowScale,
                double dlowVal, sChar upScale, double dupVal, sInt4 endYear,
                int endMonth, int endDay, int endHour, int endMin,
                int endSec, uChar numInterval, sInt4 numMissing,
                sect4IntervalType * interval)
{
   int j;               /* loop counter over number of intervals. */

   /* probability time template (9) */
   if (tmplNum != 9) {
      /* This is specifically for template 4.9 */
      return -1;
   }
   if (en->ipdsnum != tmplNum) {
      /* Didn't call fillSect4_0 first */
      return -1;
   }
   en->pdsTmpl[15] = foreProbNum;
   en->pdsTmpl[16] = numFcsts;
   en->pdsTmpl[17] = probType;
   if (lowScale == GRIB2MISSING_s1) {
      en->pdsTmpl[18] = GRIB2MISSING_s1;
      en->pdsTmpl[19] = GRIB2MISSING_s4;
   } else {
      en->pdsTmpl[18] = lowScale;
      en->pdsTmpl[19] = NearestInt (dlowVal * pow (10.0, lowScale));
   }
   if (upScale == GRIB2MISSING_s1) {
      en->pdsTmpl[20] = GRIB2MISSING_s1;
      en->pdsTmpl[21] = GRIB2MISSING_s4;
   } else {
      en->pdsTmpl[20] = upScale;
      en->pdsTmpl[21] = NearestInt (dupVal * pow (10.0, upScale));
   }
   en->pdsTmpl[22] = endYear;
   en->pdsTmpl[23] = endMonth;
   en->pdsTmpl[24] = endDay;
   en->pdsTmpl[25] = endHour;
   en->pdsTmpl[26] = endMin;
   en->pdsTmpl[27] = endSec;
   en->pdsTmpl[28] = numInterval;
   if (numInterval != 1) {
      /* can only handle 1 and only 1 time interval */
      return -4;
   }
   en->pdsTmpl[29] = numMissing;
   for (j = 0; j < numInterval; j++) {
      en->pdsTmpl[30] = interval[j].processID;
      en->pdsTmpl[31] = interval[j].incrType;
      en->pdsTmpl[32] = interval[j].timeRangeUnit;
      en->pdsTmpl[33] = interval[j].lenTime;
      en->pdsTmpl[34] = interval[j].incrUnit;
      en->pdsTmpl[35] = interval[j].timeIncr;
   }
   return 71;
}

/*****************************************************************************
 * fillSect4_10() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 10) data.  Call fillSect4_0 first.
 *
 * ARGUMENTS
 *          en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *     tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 *  percentile = Percentile value. (Input)
 *     endYear = The year of the end time (valid Time). (Input)
 *    endMonth = The month of the end time (valid Time). (Input)
 *      endDay = The day of the end time (valid Time). (Input)
 *     endHour = The hour of the end time (valid Time). (Input)
 *      endMin = The min of the end time (valid Time). (Input)
 *      endSec = The sec of the end time (valid Time). (Input)
 * numInterval = num of time range specifications (Has to = 1) (Input)
 *  numMissing = total num of missing values in statistical process (Input)
 *    interval = time range intervals.
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 if not template 4.9, or fillSect4_0 was not already called.
 *    -4 can only handle 1 and only 1 time interval
 *
 *  5/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_10(enGribMeta * en, uShort2 tmplNum, int percentile,
                 sInt4 endYear, int endMonth, int endDay, int endHour,
                 int endMin, int endSec, uChar numInterval, sInt4 numMissing,
                 sect4IntervalType * interval)
{
   int j;               /* loop counter over number of intervals. */

   /* percentile template (10) */
   if (tmplNum != 10) {
      /* This is specifically for template 4.10 */
      return -1;
   }
   if (en->ipdsnum != tmplNum) {
      /* Didn't call fillSect4_0 first */
      return -1;
   }
   en->pdsTmpl[15] = percentile;
   en->pdsTmpl[16] = endYear;
   en->pdsTmpl[17] = endMonth;
   en->pdsTmpl[18] = endDay;
   en->pdsTmpl[19] = endHour;
   en->pdsTmpl[20] = endMin;
   en->pdsTmpl[21] = endSec;
   en->pdsTmpl[22] = numInterval;
   if (numInterval != 1) {
      /* can only handle 1 and only 1 time interval */
      return -4;
   }
   en->pdsTmpl[23] = numMissing;
   for (j = 0; j < numInterval; j++) {
      en->pdsTmpl[24] = interval[j].processID;
      en->pdsTmpl[25] = interval[j].incrType;
      en->pdsTmpl[26] = interval[j].timeRangeUnit;
      en->pdsTmpl[27] = interval[j].lenTime;
      en->pdsTmpl[28] = interval[j].incrUnit;
      en->pdsTmpl[29] = interval[j].timeIncr;
   }
   return 59;
}

/*****************************************************************************
 * fillSect4_12() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 4 (using template 12) data.  Call fillSect4_0 first.
 *
 * ARGUMENTS
 *          en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *     tmplNum = [Code Table 4.0] Product Definition Template Number (Input)
 *    numFcsts = number of forecasts (Input)
 * derivedFcst = [Code Table 4.7] Derived forecast type (Input)
 *     endYear = The year of the end time (valid Time). (Input)
 *    endMonth = The month of the end time (valid Time). (Input)
 *      endDay = The day of the end time (valid Time). (Input)
 *     endHour = The hour of the end time (valid Time). (Input)
 *      endMin = The min of the end time (valid Time). (Input)
 *      endSec = The sec of the end time (valid Time). (Input)
 * numInterval = num of time range specifications (Has to = 1) (Input)
 *  numMissing = total num of missing values in statistical process (Input)
 *    interval = time range intervals.
 *
 * RETURNS: int
 *    > 0 (length of section 4).
 *    -1 if not template 4.12, or fillSect4_0 was not already called.
 *    -4 can only handle 1 and only 1 time interval
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect4_12(enGribMeta * en, uShort2 tmplNum, uChar numFcsts,
                 uChar derivedFcst, sInt4 endYear, int endMonth, int endDay,
                 int endHour, int endMin, int endSec, uChar numInterval,
                 sInt4 numMissing, sect4IntervalType * interval)
{
   int j;               /* loop counter over number of intervals. */

   /* derived interval template (12) */
   if (tmplNum != 12) {
      /* This is specifically for template 4.12 */
      return -1;
   }
   if (en->ipdsnum != tmplNum) {
      /* Didn't call fillSect4_0 first */
      return -1;
   }
   en->pdsTmpl[15] = derivedFcst;
   en->pdsTmpl[16] = numFcsts;
   en->pdsTmpl[17] = endYear;
   en->pdsTmpl[18] = endMonth;
   en->pdsTmpl[19] = endDay;
   en->pdsTmpl[20] = endHour;
   en->pdsTmpl[21] = endMin;
   en->pdsTmpl[22] = endSec;
   en->pdsTmpl[23] = numInterval;
   if (numInterval != 1) {
      /* can only handle 1 and only 1 time interval */
      return -4;
   }
   en->pdsTmpl[24] = numMissing;
   for (j = 0; j < numInterval; j++) {
      en->pdsTmpl[25] = interval[j].processID;
      en->pdsTmpl[26] = interval[j].incrType;
      en->pdsTmpl[27] = interval[j].timeRangeUnit;
      en->pdsTmpl[28] = interval[j].lenTime;
      en->pdsTmpl[29] = interval[j].incrUnit;
      en->pdsTmpl[30] = interval[j].timeIncr;
   }
   return 60;
}

/*****************************************************************************
 * fillSect5() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Complete section 5 data.
 *
 * ARGUMENTS
 *           en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *      tmplNum = [Code Table 5.0] Product Definition Template Number (Input)
 *          BSF = Binary scale factor (Input)
 *          DSF = Decimal scale factor (Input)
 * [tmplNum=0,2,3,40,41]
 *    fieldType = [Code Table 5.1] type of original field values. (Input)
 * [tmplNum=2,3]
 *       f_miss = [Code Table 5.5] missing value management used. (Input)
 *      missPri = Primary missing value (Input)
 *      missSec = Secondary missing value (Input)
 * [tmplNum=3]
 *  orderOfDiff = Order of differencing (1 or 2) (Input)
 *
 * RETURNS: int
 *    > 0 (length of section 5).
 *    -1 can't handle extended lists yet
 *    -2 not in list of templates supported by NCEP
 *    -3 can't handle this order of differencing.
 *    -4 haven't finished mapping this projection to the template.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillSect5(enGribMeta * en, uShort2 tmplNum, sShort2 BSF, sShort2 DSF,
              uChar fieldType, uChar f_miss, float missPri, float missSec,
              uChar orderOfDiff)
{
   int i;               /* loop counter over number of DRS templates. */
   const struct drstemplate *templatesdrs = get_templatesdrs();

   /* Find NCEP's template match */
   for (i = 0; i < MAXDRSTEMP; i++) {
      if (templatesdrs[i].template_num == tmplNum) {
         break;
      }
   }
   if (i == MAXDRSTEMP) {
      /* not in list of templates supported by NCEP */
      return -2;
   }
   if (templatesdrs[i].needext) {
      /* can't handle extended data yet. */
      return -1;
   }

   if (en->lenDrsTmpl < templatesdrs[i].mapdrslen) {
      if (en->drsTmpl != NULL) {
         free(en->drsTmpl);
      }
      en->drsTmpl = (sInt4 *)malloc(templatesdrs[i].mapdrslen *
                                    sizeof(sInt4));
   }
   en->lenDrsTmpl = templatesdrs[i].mapdrslen;

   en->idrsnum = tmplNum;
   /* simple packing */
   if (tmplNum == 0) {
      en->drsTmpl[0] = 9999; /* missing for Ref value (set later) */
      en->drsTmpl[1] = BSF;
      en->drsTmpl[2] = DSF;
      en->drsTmpl[3] = 9999; /* missing for numBits used (set later) */
      en->drsTmpl[4] = fieldType; /* code table 5.1 */
      return 21;
      /* complex packing */
   } else if (tmplNum == 2) {
      en->drsTmpl[0] = 9999; /* missing for Ref value (set later) */
      en->drsTmpl[1] = BSF;
      en->drsTmpl[2] = DSF;
      en->drsTmpl[3] = 9999; /* missing for numBits used (set later) */
      en->drsTmpl[4] = fieldType; /* code table 5.1 */
      en->drsTmpl[5] = 9999; /* missing for group splitting method used */
      en->drsTmpl[6] = f_miss;
      if (fieldType == 1) {
         en->drsTmpl[7] = (sInt4)missPri;
         en->drsTmpl[8] = (sInt4)missSec;
      } else {
         memcpy(&(en->drsTmpl[7]), &missPri, sizeof(float));
         memcpy(&(en->drsTmpl[8]), &missSec, sizeof(float));
      }
      en->drsTmpl[9] = 9999; /* number of groups */
      en->drsTmpl[10] = 9999; /* group widths */
      en->drsTmpl[11] = 9999; /* numBits for group widths */
      en->drsTmpl[12] = 9999; /* ref for group len */
      en->drsTmpl[13] = 9999; /* len increment for group lengths */
      en->drsTmpl[14] = 9999; /* true len of last group */
      en->drsTmpl[15] = 9999; /* numBits used for scaled group lens */
      return 47;
      /* complex spatial packing */
   } else if (tmplNum == 3) {
      en->drsTmpl[0] = 9999; /* missing for Ref value (set later) */
      en->drsTmpl[1] = BSF;
      en->drsTmpl[2] = DSF;
      en->drsTmpl[3] = 9999; /* missing for numBits used (set later) */
      en->drsTmpl[4] = fieldType; /* code table 5.1 */
      en->drsTmpl[5] = 9999; /* missing for group splitting method used */
      en->drsTmpl[6] = f_miss;
      if (fieldType == 1) {
         en->drsTmpl[7] = (sInt4)missPri;
         en->drsTmpl[8] = (sInt4)missSec;
      } else {
         memcpy(&(en->drsTmpl[7]), &missPri, sizeof(float));
         memcpy(&(en->drsTmpl[8]), &missSec, sizeof(float));
      }
      en->drsTmpl[9] = 9999; /* number of groups */
      en->drsTmpl[10] = 9999; /* group widths */
      en->drsTmpl[11] = 9999; /* numBits for group widths */
      en->drsTmpl[12] = 9999; /* ref for group len */
      en->drsTmpl[13] = 9999; /* len increment for group lengths */
      en->drsTmpl[14] = 9999; /* true len of last group */
      en->drsTmpl[15] = 9999; /* numBits used for scaled group lens */
      if (orderOfDiff > 2) {
         /* NCEP can not handle order of differencing > 2 */
         return -3;
      }
      en->drsTmpl[16] = orderOfDiff;
      en->drsTmpl[17] = 9999; /* num extra octets need for spatial differ */
      return 49;
      /* jpeg2000 packing */
   } else if ((tmplNum == 40) || (tmplNum == 40000)) {
      en->drsTmpl[0] = 9999; /* missing for Ref value (set later) */
      en->drsTmpl[1] = BSF;
      en->drsTmpl[2] = DSF;
      en->drsTmpl[3] = 9999; /* depth of grayscale image (set later) */
      en->drsTmpl[4] = fieldType; /* code table 5.1 */
      en->drsTmpl[5] = 9999; /* type of compression used (0 is lossless)
                              * (code table 5.40) */
      en->drsTmpl[6] = 9999; /* compression ratio */
      return 23;
      /* png packing */
   } else if ((tmplNum == 41) || (tmplNum == 40010)) {
      en->drsTmpl[0] = 9999; /* missing for Ref value (set later) */
      en->drsTmpl[1] = BSF;
      en->drsTmpl[2] = DSF;
      en->drsTmpl[3] = 9999; /* depth of grayscale image (set later) */
      en->drsTmpl[4] = fieldType; /* code table 5.1 */
      return 21;
      /* spectral packing */
   } else if (tmplNum == 50) {
      en->drsTmpl[0] = 9999; /* missing for Ref value (set later) */
      en->drsTmpl[1] = BSF;
      en->drsTmpl[2] = DSF;
      en->drsTmpl[3] = 9999; /* num bits used for each packed value */
      en->drsTmpl[4] = 9999; /* real part of (0,0) coefficient */
      return 24;
      /* harmonic packing */
   } else if (tmplNum == 51) {
      en->drsTmpl[0] = 9999; /* missing for Ref value (set later) */
      en->drsTmpl[1] = BSF;
      en->drsTmpl[2] = DSF;
      en->drsTmpl[3] = 9999; /* num bits used for each packed value */
      en->drsTmpl[4] = 9999; /* P - Laplacian scaling factor */
      en->drsTmpl[5] = 9999; /* Js - pentagonal resolution parameter */
      en->drsTmpl[6] = 9999; /* Ks - pentagonal resolution parameter */
      en->drsTmpl[7] = 9999; /* Ms - pentagonal resolution parameter */
      en->drsTmpl[8] = 9999; /* Ts - total num values in subset */
      en->drsTmpl[9] = 9999; /* Precision of unpacked subset */
      return 35;
   }
   /* Haven't finished mapping this drs to a template. */
   return -4;
}

/*****************************************************************************
 * fillGrid() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Completes the data portion.  If f_boustify, then it walks through the
 * data winding back and forth.  Note it does this in a row oriented fashion
 * If you need a column oriented fashion because your grid is defined the
 * other way, then swap your Nx and Ny in your call.
 *
 * ARGUMENTS
 *         en = A pointer to meta data to pass to GRIB2 encoder. (Output)
 *       data = Data array to add. (Input)
 *    lenData = Length of Data array. (Input)
 *         Nx = Number of X coordinates (Input)
 *         Ny = Number of Y coordinates (Input)
 *      ibmap = [Code 6.0] Bitmap indicator (Input)
 *              0 = bitmap applies and is included in Section 6.
 *              1-253 = Predefined bitmap applies
 *              254 = Previously defined bitmap applies to this field
 *              255 = Bit map does not apply to this product.
 * f_boustify = true if we should re-Wrap the grid. (Input)
 *     f_miss = 1 if missPri valid, 2 if missSec valid. (Input)
 *    missPri = Primary missing value (Input)
 *    missSec = Secondary missing value (Input)
 *
 * RETURNS: int
 *    > 0 (max length of sect 6 and sect 7).
 *    -1 Can't handle this kind of bitmap (pre-defined).
 *    -2 No missing value when trying to create the bmap.
 *    -3 Can't handle Nx * Ny != lenData.
 *
 *  4/2006 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
int fillGrid(enGribMeta * en, double *data, sInt4 lenData, sInt4 Nx, sInt4 Ny,
             sInt4 ibmap, sChar f_boustify, uChar f_miss, float missPri,
             float missSec)
{
   uChar f_flip;        /* Used to help keep track of the direction when
                         * "boustifying" the data. */
   sInt4 x;             /* loop counter over Nx. */
   sInt4 y;             /* loop counter over Ny. */
   sInt4 ind1;          /* index to copy to. */
   sInt4 ind2;          /* index to copy from. */

   if ((ibmap != 0) && (ibmap != 255)) {
      /* Can't handle this kind of bitmap (pre-defined). */
      return -1;
   }
   if ((ibmap == 0) && (f_miss != 1) && (f_miss != 2)) {
      /* No missing value when trying to create the bmap. */
      return -2;
   }
   if (Nx * Ny != lenData) {
      /* Can't handle Nx * Ny != lenData. */
      return -3;
   }

   if (en->ngrdpts < lenData) {
      if (en->fld != NULL) {
         free(en->fld);
      }
      en->fld = (float *)malloc(lenData * sizeof(float));
      if (ibmap == 0) {
         if (en->bmap != NULL) {
            free(en->bmap);
         }
         en->bmap = (sInt4 *)malloc(lenData * sizeof(sInt4));
      }
   }
   en->ngrdpts = lenData;
   en->ibmap = ibmap;

   /* Now need to walk over data and boustify it and create bmap. */

   if (ibmap == 0) {
      /* boustify uses row oriented boustification, however for column
       * oriented, swap the Ny and Nx in the call to the procedure. */
      if (f_boustify) {
         f_flip = 0;
         for (y = 0; y < Ny; y++) {
            for (x = 0; x < Nx; x++) {
               ind1 = x + y * Nx;
               if (!f_flip) {
                  ind2 = ind1;
               } else {
                  ind2 = (Nx - x - 1) + y * Nx;
               }
               en->fld[ind1] = (float)data[ind2];
               if ((data[ind2] == missPri) ||
                   ((f_miss == 2) && (data[ind2] == missSec))) {
                  en->bmap[ind1] = 0;
               } else {
                  en->bmap[ind1] = 1;
               }
            }
            f_flip = (!f_flip);
         }
      } else {
         for (ind1 = 0; ind1 < lenData; ind1++) {
            en->fld[ind1] = (float)data[ind1];
            if ((data[ind1] == missPri) ||
                ((f_miss == 2) && (data[ind1] == missSec))) {
               en->bmap[ind1] = 0;
            } else {
               en->bmap[ind1] = 1;
            }
         }
      }
      /* len(sect6) < 6 + (lenData/8 + 1), len(sect7) < 5 + lenData * 4 */
      return (6 + lenData / 8 + 1) + (5 + lenData * 4);
   } else {
      /* boustify uses row oriented boustification, however for column
       * oriented, swap the Ny and Nx in the call to the procedure. */
      if (f_boustify) {
         f_flip = 0;
         for (y = 0; y < Ny; y++) {
            for (x = 0; x < Nx; x++) {
               ind1 = x + y * Nx;
               if (!f_flip) {
                  ind2 = ind1;
               } else {
                  ind2 = (Nx - x - 1) + y * Nx;
               }
               en->fld[ind1] = (float)data[ind2];
            }
            f_flip = (!f_flip);
         }
      } else {
         for (ind1 = 0; ind1 < lenData; ind1++) {
            en->fld[ind1] = (float)data[ind1];
         }
      }
      /* len(sect6) = 6, len(sect7) < 5 + lenData * 4 */
      return 6 + (5 + lenData * 4);
   }
}


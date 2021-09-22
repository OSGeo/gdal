/*****************************************************************************
 * degrib1.c
 *
 * DESCRIPTION
 *    This file contains the main driver routines to unpack GRIB 1 files.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *   GRIB 1 files are assumed to be big endian.
 *****************************************************************************
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <limits>

#include "degrib2.h"
#include "myerror.h"
#include "myassert.h"
#include "tendian.h"
#include "scan.h"
#include "degrib1.h"
#include "metaname.h"
#include "clock.h"
#include "cpl_error.h"
#include "cpl_string.h"

/* default missing data value (see: bitmap GRIB1: sect3 and sect4) */
/* UNDEFINED is default, UNDEFINED_PRIM is desired choice. */
#define UNDEFINED 9.999e20
#define UNDEFINED_PRIM 9999

#define GRIB_UNSIGN_INT3(a,b,c) ((a<<16)+(b<<8)+c)
#define GRIB_UNSIGN_INT2(a,b) ((a<<8)+b)
#define GRIB_SIGN_INT3(a,b,c) ((1-(int) ((unsigned) (a & 0x80) >> 6)) * (int) (((a & 127) << 16)+(b<<8)+c))
#define GRIB_SIGN_INT2(a,b) ((1-(int) ((unsigned) (a & 0x80) >> 6)) * (int) (((a & 0x7f) << 8) + b))

/* various centers */
#define NMC	7
#define US_OTHER 9
#define CPTEC 46
/* Canada Center */
#define CMC	54
#define AFWA 57
#define DWD 78
#define ECMWF 98
#define ATHENS 96
#define NORWAY 88

/* various subcenters */
#define SUBCENTER_MDL 14
#define SUBCENTER_TDL 11

/* The idea of rean or opn is to give a warning about default choice of
   which table to use. */
#define DEF_NCEP_TABLE rean_nowarn
enum Def_NCEP_Table { rean, opn, rean_nowarn, opn_nowarn };

#if 0 // moved by GDAL in degrib1.h

/* For an update to these tables see:
 * http://www.nco.ncep.noaa.gov/pmb/docs/on388/table2.html
 */

extern GRIB1ParmTable parm_table_ncep_opn[256];
extern GRIB1ParmTable parm_table_ncep_reanal[256];
extern GRIB1ParmTable parm_table_ncep_tdl[256];
extern GRIB1ParmTable parm_table_ncep_mdl[256];
extern GRIB1ParmTable parm_table_omb[256];
extern GRIB1ParmTable parm_table_nceptab_129[256];
extern GRIB1ParmTable parm_table_nceptab_130[256];
extern GRIB1ParmTable parm_table_nceptab_131[256];
extern GRIB1ParmTable parm_table_nceptab_133[256];
extern GRIB1ParmTable parm_table_nceptab_140[256];
extern GRIB1ParmTable parm_table_nceptab_141[256];

extern GRIB1ParmTable parm_table_nohrsc[256];

extern GRIB1ParmTable parm_table_cptec_254[256];

extern GRIB1ParmTable parm_table_afwa_000[256];
extern GRIB1ParmTable parm_table_afwa_001[256];
extern GRIB1ParmTable parm_table_afwa_002[256];
extern GRIB1ParmTable parm_table_afwa_003[256];
extern GRIB1ParmTable parm_table_afwa_010[256];
extern GRIB1ParmTable parm_table_afwa_011[256];

extern GRIB1ParmTable parm_table_dwd_002[256];
extern GRIB1ParmTable parm_table_dwd_201[256];
extern GRIB1ParmTable parm_table_dwd_202[256];
extern GRIB1ParmTable parm_table_dwd_203[256];

extern GRIB1ParmTable parm_table_ecmwf_128[256];
extern GRIB1ParmTable parm_table_ecmwf_129[256];
extern GRIB1ParmTable parm_table_ecmwf_130[256];
extern GRIB1ParmTable parm_table_ecmwf_131[256];
extern GRIB1ParmTable parm_table_ecmwf_140[256];
extern GRIB1ParmTable parm_table_ecmwf_150[256];
extern GRIB1ParmTable parm_table_ecmwf_160[256];
extern GRIB1ParmTable parm_table_ecmwf_170[256];
extern GRIB1ParmTable parm_table_ecmwf_180[256];

extern GRIB1ParmTable parm_table_athens[256];

extern GRIB1ParmTable parm_table_norway128[256];

extern GRIB1ParmTable parm_table_cmc[256];

extern GRIB1ParmTable parm_table_undefined[256];

#endif

/*****************************************************************************
 * Choose_ParmTable() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Chooses the correct Parameter table depending on what is in the GRIB1
 * message's "Product Definition Section".
 *
 * ARGUMENTS
 *   pdsMeta = The filled out pdsMeta data structure to base choice on. (Input)
 *    center = The Center that created the data (Input)
 * subcenter = The Sub Center that created the data (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: ParmTable (appropriate parameter table.)
 *
 * HISTORY
 *   <unknown> : wgrib library : cnames.c
 *   4/2003 Arthur Taylor (MDL/RSIS): Modified
 *  10/2005 AAT: Adjusted to take center, subcenter
 *
 * NOTES
 *****************************************************************************
 */
static const GRIB1ParmTable *Choose_ParmTable (pdsG1Type *pdsMeta,
                                         unsigned short int center,
                                         unsigned short int subcenter)
{
   int process;         /* The process ID from the GRIB1 message. */

   switch (center) {
      case NMC:
         if (pdsMeta->mstrVersion <= 3) {
            switch (subcenter) {
               case 1:
                  return &parm_table_ncep_reanal[0];
               case SUBCENTER_TDL:
                  return &parm_table_ncep_tdl[0];
               case SUBCENTER_MDL:
                  return &parm_table_ncep_mdl[0];
            }
         }
         /* figure out if NCEP opn or reanalysis */
         switch (pdsMeta->mstrVersion) {
            case 0:
               return &parm_table_ncep_opn[0];
            case 1:
            case 2:
               process = pdsMeta->genProcess;
               if ((subcenter != 0) || ((process != 80) && (process != 180))) {
                  return &parm_table_ncep_opn[0];
               }
#if 0
               /* At this point could be either the opn or reanalysis table */
               switch (DEF_NCEP_TABLE) {
                  case opn_nowarn:
                     return &parm_table_ncep_opn[0];
                  case rean_nowarn:
                     return &parm_table_ncep_reanal[0];
               }
               break;
#else
               // ERO: this is the non convoluted version of the above code
               return &parm_table_ncep_reanal[0];
#endif
            case 3:
               return &parm_table_ncep_opn[0];
            case 128:
               return &parm_table_omb[0];
            case 129:
               return &parm_table_nceptab_129[0];
            case 130:
               return &parm_table_nceptab_130[0];
            case 131:
               return &parm_table_nceptab_131[0];
            case 133:
               return &parm_table_nceptab_133[0];
            case 140:
               return &parm_table_nceptab_140[0];
            case 141:
               return &parm_table_nceptab_141[0];
         }
         break;
      case AFWA:
         switch (subcenter) {
            case 0:
               return &parm_table_afwa_000[0];
            case 1:
            case 4:
               return &parm_table_afwa_001[0];
            case 2:
               return &parm_table_afwa_002[0];
            case 3:
               return &parm_table_afwa_003[0];
            case 10:
               return &parm_table_afwa_010[0];
            case 11:
               return &parm_table_afwa_011[0];
/*            case 5:*/
               /* Didn't have a table 5. */
         }
         break;
      case ECMWF:
         switch (pdsMeta->mstrVersion) {
            case 128:
               return &parm_table_ecmwf_128[0];
            case 129:
               return &parm_table_ecmwf_129[0];
            case 130:
               return &parm_table_ecmwf_130[0];
            case 131:
               return &parm_table_ecmwf_131[0];
            case 140:
               return &parm_table_ecmwf_140[0];
            case 150:
               return &parm_table_ecmwf_150[0];
            case 160:
               return &parm_table_ecmwf_160[0];
            case 170:
               return &parm_table_ecmwf_170[0];
            case 180:
               return &parm_table_ecmwf_180[0];
            case 228:
               return &parm_table_ecmwf_228[0];
         }
         break;
      case DWD:
         switch (pdsMeta->mstrVersion) {
            case 2:
               return &parm_table_dwd_002[0];
            case 201:
               return &parm_table_dwd_201[0];
            case 202:
               return &parm_table_dwd_202[0];
            case 203:
               return &parm_table_dwd_203[0];
         }
         break;
      case CPTEC:
         switch (pdsMeta->mstrVersion) {
            case 254:
               return &parm_table_cptec_254[0];
         }
         break;
      case US_OTHER:
         switch (subcenter) {
            case 163:
               return &parm_table_nohrsc[0];
            /* Based on 11/7/2006 email with Rob Doornbos, mimic'd what wgrib
             * did which was to use parm_table_ncep_opn. */
            case 161:
               return &parm_table_ncep_opn[0];
         }
         break;
      case ATHENS:
         return &parm_table_athens[0];
         break;
      case NORWAY:
         if (pdsMeta->mstrVersion == 128) {
            return &parm_table_norway128[0];
         }
         break;
      case CMC:
         return &parm_table_cmc[0];
         break;
   }
   if (pdsMeta->mstrVersion > 3) {
      CPLError( CE_Warning, CPLE_AppDefined, "GRIB: Don't understand the parameter table, since center %d-%d used\n"
              "parameter table version %d instead of 3 (international exchange).\n"
              "Using default for now (which might lead to erroneous interpretation), but please email arthur.taylor@noaa.gov\n"
              "about adding this table to his 'degrib1.c' and 'grib1tab.c' files.",
              center, subcenter, pdsMeta->mstrVersion);
   }
   if (pdsMeta->cat > 127) {
      CPLError(CE_Warning, CPLE_AppDefined, "GRIB: Parameter %d is > 127, so it falls in the local use section of\n"
              "the parameter table (and is undefined on the international table.\n"
              "Using default for now(which might lead to erroneous interpretation), but please email arthur.taylor@noaa.gov\n"
              "about adding this table to his 'degrib1.c' and 'grib1tab.c' files.",
              pdsMeta->cat);
   }
   return &parm_table_undefined[0];
}

/*****************************************************************************
 * GRIB1_Table2LookUp() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Returns the variable name (type of data) and comment (longer form of the
 * name) for the data that is in the GRIB1 message.
 *
 * ARGUMENTS
 *      name = A pointer to the resulting short name. (Output)
 *   comment = A pointer to the resulting long name. (Output)
 *   pdsMeta = The filled out pdsMeta data structure to base choice on. (Input)
 *    center = The Center that created the data (Input)
 * subcenter = The Sub Center that created the data (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created
 *  10/2005 AAT: Adjusted to take center, subcenter
 *
 * NOTES
 *****************************************************************************
 */
static void GRIB1_Table2LookUp (pdsG1Type *pdsMeta, const char **name,
                                const char **comment, const char **unit,
                                int *convert,
                                unsigned short int center,
                                unsigned short int subcenter)
{
   const GRIB1ParmTable *table; /* The parameter table chosen by the pdsMeta data */

   table = Choose_ParmTable (pdsMeta, center, subcenter);
   if ((center == NMC) && (pdsMeta->mstrVersion == 129)
       && (pdsMeta->cat == 180)) {
      if (pdsMeta->timeRange == 3) {
         *name = "AVGOZCON";
         *comment = "Average Ozone Concentration";
         *unit = "PPB";
         *convert = UC_NONE;
         return;
      }
   }
   *name = table[pdsMeta->cat].name;
   if( strcmp(*name, CPLSPrintf("var%d", pdsMeta->cat)) == 0 )
   {
       if( center == ECMWF )
           *name = CPLSPrintf("var%d of table %d of center ECMWF", pdsMeta->cat, pdsMeta->mstrVersion);
       else
           *name = CPLSPrintf("var%d of table %d of center %d", pdsMeta->cat, pdsMeta->mstrVersion, center);
   }
   *comment = table[pdsMeta->cat].comment;
   *unit = table[pdsMeta->cat].unit;
   *convert = table[pdsMeta->cat].convert;
/*   printf ("%s %s %s\n", *name, *comment, *unit);*/
}

/* Similar to metaname.c :: ParseLevelName() */
static void GRIB1_Table3LookUp (pdsG1Type *pdsMeta, char **shortLevelName,
                                char **longLevelName)
{
   uChar type = pdsMeta->levelType;
   uChar level1, level2;

   free (*shortLevelName);
   *shortLevelName = nullptr;
   free (*longLevelName);
   *longLevelName = nullptr;
   /* Find out if val is a 2 part value or not */
   if (GRIB1Surface[type].f_twoPart) {
      level1 = (pdsMeta->levelVal >> 8);
      level2 = (pdsMeta->levelVal & 0xff);
      reallocSprintf (shortLevelName, "%d-%d-%s", level1, level2,
                      GRIB1Surface[type].name);
      reallocSprintf (longLevelName, "%d-%d[%s] %s (%s)", level1, level2,
                      GRIB1Surface[type].unit, GRIB1Surface[type].name,
                      GRIB1Surface[type].comment);
   } else {
      reallocSprintf (shortLevelName, "%d-%s", pdsMeta->levelVal,
                      GRIB1Surface[type].name);
      reallocSprintf (longLevelName, "%d[%s] %s (%s)", pdsMeta->levelVal,
                      GRIB1Surface[type].unit, GRIB1Surface[type].name,
                      GRIB1Surface[type].comment);
   }
}

/*****************************************************************************
 * fval_360() --
 *
 * Albion Taylor / ARL
 *
 * PURPOSE
 *   Converts an IBM360 floating point number to an IEEE floating point
 * number.  The IBM floating point spec represents the fraction as the last
 * 3 bytes of the number, with 0xffffff being just shy of 1.0.  The first byte
 * leads with a sign bit, and the last seven bits represent the powers of 16
 * (not 2), with a bias of 0x40 giving 16^0.
 *
 * ARGUMENTS
 * aval = A sInt4 containing the original IBM 360 number. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: double = the value that aval represents.
 *
 * HISTORY
 *   <unknown> Albion Taylor (ARL): Created
 *   4/2003 Arthur Taylor (MDL/RSIS): Cleaned up.
 *   5/2003 AAT: some kind of Bug due to optimizations...
 *          -1055916032 => 0 instead of -1
 *
 * NOTES
 *****************************************************************************
 */
static double fval_360 (uInt4 aval)
{
   short int ptr[4];
#ifdef LITTLE_ENDIAN
   ptr[3] = ((((aval >> 24) & 0x7f) << 2) + (0x3ff - 0x100)) << 4;
   ptr[2] = 0;
   ptr[1] = 0;
   ptr[0] = 0;
#else
   ptr[0] = ((((aval >> 24) & 0x7f) << 2) + (0x3ff - 0x100)) << 4;
   ptr[1] = 0;
   ptr[2] = 0;
   ptr[3] = 0;
#endif
   double pow16;
   memcpy(&pow16, ptr, 8);
   return ((aval & 0x80000000) ? -pow16 : pow16) *
         (aval & 0xffffff) / ((double) 0x1000000);
}

/*****************************************************************************
 * ReadGrib1Sect1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the GRIB1 "Product Definition Section" or section 1, filling out
 * the pdsMeta data structure.
 *
 * ARGUMENTS
 *       pds = The compressed part of the message dealing with "PDS". (Input)
 *    pdsLen = Size of pds in bytes. (Input)
 *   gribLen = The total length of the GRIB1 message. (Input)
 *    curLoc = Current location in the GRIB1 message. (Output)
 *   pdsMeta = The filled out pdsMeta data structure. (Output)
 *     f_gds = boolean if there is a Grid Definition Section. (Output)
 *    gridID = The Grid ID. (Output)
 *     f_bms = boolean if there is a Bitmap Section. (Output)
 *       DSF = Decimal Scale Factor for unpacking the data. (Output)
 *    center = The Center that created the data (Output)
 * subcenter = The Sub Center that created the data (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = gribLen is too small.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created.
 *   5/2004 AAT: Paid attention to table 5 (Time range indicator) and which of
 *          P1 and P2 are the valid times.
 *  10/2005 AAT: Adjusted to take center, subcenter
 *
 * NOTES
 *****************************************************************************
 */
static int ReadGrib1Sect1 (uChar *pds, uInt4 pdsLen, uInt4 gribLen, uInt4 *curLoc,
                           pdsG1Type *pdsMeta, char *f_gds, uChar *gridID,
                           char *f_bms, short int *DSF,
                           unsigned short int *center,
                           unsigned short int *subcenter)
{
   uInt4 sectLen;       /* Length in bytes of the current section. */
   int year;            /* The year of the GRIB1 Message. */
   double P1_DeltaTime; /* Used to parse the time for P1 */
   double P2_DeltaTime; /* Used to parse the time for P2 */
   uInt4 uli_temp;
#ifdef DEBUG
/*
   int i;
*/
#endif
   /* We will read the first required 28 bytes */
   if( pdsLen < 28 )
       return -1;

   sectLen = GRIB_UNSIGN_INT3 (*pds, pds[1], pds[2]);
   if( sectLen > pdsLen )
       return -1;
#ifdef DEBUG
/*
   printf ("Section 1 length = %ld\n", sectLen);
   for (i = 0; i < sectLen; i++) {
      printf ("Sect1: item %d = %d\n", i + 1, pds[i]);
   }
   printf ("Century is item 25\n");
*/
#endif
   *curLoc += sectLen;
   if (*curLoc > gribLen) {
      errSprintf ("Ran out of data in PDS (GRIB 1 Section 1)\n");
      return -1;
   }
   pds += 3;
   pdsMeta->mstrVersion = *(pds++);
   *center = *(pds++);
   pdsMeta->genProcess = *(pds++);
   *gridID = *(pds++);
   *f_gds = GRIB2BIT_1 & *pds;
   *f_bms = GRIB2BIT_2 & *pds;
   pds++;
   pdsMeta->cat = *(pds++);
   pdsMeta->levelType = *(pds++);
   pdsMeta->levelVal = GRIB_UNSIGN_INT2 (*pds, pds[1]);
   pds += 2;
   if (*pds == 0) {
      /* The 12 is because we have increased pds by 12. (but 25 is in
       * reference of 1..25, so we need another -1) */
      year = (pds[25 - 13] * 100);
   } else {
      /* The 12 is because we have increased pds by 12. (but 25 is in
       * reference of 1..25, so we need another -1) */
      year = *pds + ((pds[25 - 13] - 1) * 100);
   }

   if (ParseTime (&(pdsMeta->refTime), year, pds[1], pds[2], pds[3], pds[4],
                  0) != 0) {
      preErrSprintf ("Error In call to ParseTime\n");
      errSprintf ("(Probably a corrupt file)\n");
      return -1;
   }
   pds += 5;
   pdsMeta->timeRange = pds[3];
   if (ParseSect4Time2secV1 (pds[1], *pds, &P1_DeltaTime) == 0) {
      pdsMeta->P1 = pdsMeta->refTime + P1_DeltaTime;
   } else {
      pdsMeta->P1 = pdsMeta->refTime;
      printf ("Warning! : Can't figure out time unit of %u\n", *pds);
   }
   if (ParseSect4Time2secV1 (pds[2], *pds, &P2_DeltaTime) == 0) {
      pdsMeta->P2 = pdsMeta->refTime + P2_DeltaTime;
   } else {
      pdsMeta->P2 = pdsMeta->refTime;
      printf ("Warning! : Can't figure out time unit of %u\n", *pds);
   }
   /* The following is based on Table 5. */
   /* Note: For ensemble forecasts, 119 has meaning. */
   switch (pdsMeta->timeRange) {
      case 0:
      case 1:
      case 113:
      case 114:
      case 115:
      case 116:
      case 117:
      case 118:
      case 123:
      case 124:
         pdsMeta->validTime = pdsMeta->P1;
         break;
      case 2:
         /* Puzzling case. */
         pdsMeta->validTime = pdsMeta->P2;
         break;
      case 3:
      case 4:
      case 5:
      case 51:
         pdsMeta->validTime = pdsMeta->P2;
         break;
      case 10:
         if (ParseSect4Time2secV1 (GRIB_UNSIGN_INT2 (pds[1], pds[2]), *pds,
                                   &P1_DeltaTime) == 0) {
            pdsMeta->P2 = pdsMeta->P1 = pdsMeta->refTime + P1_DeltaTime;
         } else {
            pdsMeta->P2 = pdsMeta->P1 = pdsMeta->refTime;
            printf ("Warning! : Can't figure out time unit of %u\n", *pds);
         }
         pdsMeta->validTime = pdsMeta->P1;
         break;
      default:
         pdsMeta->validTime = pdsMeta->P1;
   }
   pds += 4;
   pdsMeta->Average = GRIB_UNSIGN_INT2 (*pds, pds[1]);
   pds += 2;
   pdsMeta->numberMissing = *(pds++);
   /* Skip over centry of reference time. */
   pds++;
   *subcenter = *(pds++);
   *DSF = GRIB_SIGN_INT2 (*pds, pds[1]);
   pds += 2;
   pdsMeta->f_hasEns = 0;
   pdsMeta->f_hasProb = 0;
   pdsMeta->f_hasCluster = 0;
   if (sectLen < 41) {
      return 0;
   }
   /* Following is based on:
    * http://www.emc.ncep.noaa.gov/gmb/ens/info/ens_grib.html */
   if ((*center == NMC) && (*subcenter == 2)) {
      if (sectLen < 45) {
         printf ("Warning! Problems with Ensemble section\n");
         return 0;
      }
      pdsMeta->f_hasEns = 1;
      pdsMeta->ens.BitFlag = *(pds++);
/*      octet21 = pdsMeta->timeRange; = 119 has meaning now */
      pds += 11;
      pdsMeta->ens.Application = *(pds++);
      pdsMeta->ens.Type = *(pds++);
      pdsMeta->ens.Number = *(pds++);
      pdsMeta->ens.ProdID = *(pds++);
      pdsMeta->ens.Smooth = *(pds++);
      if ((pdsMeta->cat == 191) || (pdsMeta->cat == 192) ||
          (pdsMeta->cat == 193)) {
         if (sectLen < 60) {
            printf ("Warning! Problems with Ensemble Probability section\n");
            return 0;
         }
         pdsMeta->f_hasProb = 1;
         pdsMeta->prob.Cat = pdsMeta->cat;
         pdsMeta->cat = *(pds++);
         pdsMeta->prob.Type = *(pds++);
         MEMCPY_BIG (&uli_temp, pds, sizeof (sInt4));
         pdsMeta->prob.lower = fval_360 (uli_temp);
         pds += 4;
         MEMCPY_BIG (&uli_temp, pds, sizeof (sInt4));
         pdsMeta->prob.upper = fval_360 (uli_temp);
         pds += 4;
         pds += 4;
      }
      if ((pdsMeta->ens.Type == 4) || (pdsMeta->ens.Type == 5)) {
         /* 87 ... 100 was reserved, but may not be encoded */
         if ((sectLen < 100) && (sectLen != 86)) {
            printf ("Warning! Problems with Ensemble Clustering section\n");
            printf ("Section length == %u\n", sectLen);
            return 0;
         }
         if (pdsMeta->f_hasProb == 0) {
            pds += 14;
         }
         pdsMeta->f_hasCluster = 1;
         pdsMeta->cluster.ensSize = *(pds++);
         pdsMeta->cluster.clusterSize = *(pds++);
         pdsMeta->cluster.Num = *(pds++);
         pdsMeta->cluster.Method = *(pds++);
         pdsMeta->cluster.NorLat = GRIB_UNSIGN_INT3 (*pds, pds[1], pds[2]);
         pdsMeta->cluster.NorLat = pdsMeta->cluster.NorLat / 1000.;
         pds += 3;
         pdsMeta->cluster.SouLat = GRIB_UNSIGN_INT3 (*pds, pds[1], pds[2]);
         pdsMeta->cluster.SouLat = pdsMeta->cluster.SouLat / 1000.;
         pds += 3;
         pdsMeta->cluster.EasLon = GRIB_UNSIGN_INT3 (*pds, pds[1], pds[2]);
         pdsMeta->cluster.EasLon = pdsMeta->cluster.EasLon / 1000.;
         pds += 3;
         pdsMeta->cluster.WesLon = GRIB_UNSIGN_INT3 (*pds, pds[1], pds[2]);
         pdsMeta->cluster.WesLon = pdsMeta->cluster.WesLon / 1000.;
         pds += 3;
         memcpy (pdsMeta->cluster.Member, pds, 10);
         pdsMeta->cluster.Member[10] = '\0';
      }
      /* Following based on:
       * http://www.ecmwf.int/publications/manuals/libraries/gribex/
       * localGRIBUsage.html */
   } else if (*center == ECMWF) {
      if (sectLen < 45) {
         printf ("Warning! Problems with ECMWF PDS extension\n");
         return 0;
      }
      /*
      sInt4 i_temp;
      pds += 12;
      i_temp = GRIB_SIGN_INT2 (pds[3], pds[4]);
      printf ("ID %d Class %d Type %d Stream %d", pds[0], pds[1], pds[2],
              i_temp);
      pds += 5;
      printf (" Ver %c%c%c%c, ", pds[0], pds[1], pds[2], pds[3]);
      pds += 4;
      printf ("Octet-50 %d, Octet-51 %d SectLen %d\n", pds[0], pds[1],
              sectLen);
      */
   } else {
      printf ("Un-handled possible ensemble section center %u "
              "subcenter %u\n", *center, *subcenter);
   }
   return 0;
}

/*****************************************************************************
 * Grib1_Inventory() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the GRIB1 "Product Definition Section" for enough information to
 * fill out the inventory data structure so we can do a simple inventory on
 * the file in a similar way to how we did it for GRIB2.
 *
 * ARGUMENTS
 *      fp = An opened GRIB2 file already at the correct message. (Input)
 * gribLen = The total length of the GRIB1 message. (Input)
 *     inv = The inventory data structure that we need to fill. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = gribLen is too small.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
int GRIB1_Inventory (VSILFILE *fp, uInt4 gribLen, inventoryType *inv)
{
   uChar temp[3];        /* Used to determine the section length. */
   uInt4 sectLen;       /* Length in bytes of the current section. */
   uChar *pds;          /* The part of the message dealing with the PDS. */
   pdsG1Type pdsMeta;   /* The pds parsed into a usable data structure. */
   char f_gds;          /* flag if there is a gds section. */
   char f_bms;          /* flag if there is a bms section. */
   short int DSF;       /* Decimal Scale Factor for unpacking the data. */
   uChar gridID;        /* Which GDS specs to use. */
   const char *varName; /* The name of the data stored in the grid. */
   const char *varComment; /* Extra comments about the data stored in grid. */
   const char *varUnit; /* Holds the name of the unit [K] [%] .. etc */
   int convert;         /* Conversion method for this variable's unit. */
   uInt4 curLoc;        /* Where we are in the current GRIB message. */
   unsigned short int center; /* The Center that created the data */
   unsigned short int subcenter; /* The Sub Center that created the data */

   curLoc = 8;
   if (VSIFReadL(temp, sizeof (char), 3, fp) != 3) {
      errSprintf ("Ran out of file.\n");
      return -1;
   }
   sectLen = GRIB_UNSIGN_INT3 (*temp, temp[1], temp[2]);
   if (curLoc + sectLen > gribLen) {
      errSprintf ("Ran out of data in PDS (GRIB1_Inventory)\n");
      return -1;
   }
   if( sectLen < 3 )
   {
       errSprintf ("Invalid sectLen.\n");
       return -1;
   }
   pds = (uChar *) malloc (sectLen * sizeof (uChar));
   if( pds == nullptr )
   {
       errSprintf ("Ran out of memory.\n");
       return -1;
   }
   *pds = *temp;
   pds[1] = temp[1];
   pds[2] = temp[2];
   if (VSIFReadL(pds + 3, sizeof (char), sectLen - 3, fp) + 3 != sectLen) {
      errSprintf ("Ran out of file.\n");
      free (pds);
      return -1;
   }

   if (ReadGrib1Sect1 (pds, sectLen, gribLen, &curLoc, &pdsMeta, &f_gds, &gridID,
                       &f_bms, &DSF, &center, &subcenter) != 0) {
      preErrSprintf ("Inside GRIB1_Inventory\n");
      free (pds);
      return -1;
   }
   free (pds);
   inv->refTime = pdsMeta.refTime;
   inv->validTime = pdsMeta.validTime;
   inv->foreSec = inv->validTime - inv->refTime;
   GRIB1_Table2LookUp (&(pdsMeta), &varName, &varComment, &varUnit,
                       &convert, center, subcenter);
   inv->element = (char *) malloc ((1 + strlen (varName)) * sizeof (char));
   strcpy (inv->element, varName);
   inv->unitName = (char *) malloc ((1 + 2 + strlen (varUnit)) *
                                    sizeof (char));
   snprintf (inv->unitName, (1 + 2 + strlen (varUnit)) *
                                    sizeof (char), "[%s]", varUnit);
   inv->comment = (char *) malloc ((1 + strlen (varComment) +
                                    strlen (varUnit) + 2 + 1) *
                                   sizeof (char));
   snprintf (inv->comment, (1 + strlen (varComment) +
                                    strlen (varUnit) + 2 + 1) *
                                   sizeof (char),
             "%s [%s]", varComment, varUnit);

   GRIB1_Table3LookUp (&(pdsMeta), &(inv->shortFstLevel),
                       &(inv->longFstLevel));

   /* Get to the end of the GRIB1 message. */
   /* (inventory.c : GRIB2Inventory), is responsible for this. */
   /* fseek (fp, gribLen - sectLen, SEEK_CUR); */
   return 0;
}

int GRIB1_RefTime (VSILFILE *fp, uInt4 gribLen, double *refTime)
{
   uChar temp[3];        /* Used to determine the section length. */
   uInt4 sectLen;       /* Length in bytes of the current section. */
   uChar *pds;          /* The part of the message dealing with the PDS. */
   pdsG1Type pdsMeta;   /* The pds parsed into a usable data structure. */
   char f_gds;          /* flag if there is a gds section. */
   char f_bms;          /* flag if there is a bms section. */
   short int DSF;       /* Decimal Scale Factor for unpacking the data. */
   uChar gridID;        /* Which GDS specs to use. */
   uInt4 curLoc;        /* Where we are in the current GRIB message. */
   unsigned short int center; /* The Center that created the data */
   unsigned short int subcenter; /* The Sub Center that created the data */

   curLoc = 8;
   if (VSIFReadL (temp, sizeof (char), 3, fp) != 3) {
      errSprintf ("Ran out of file.\n");
      return -1;
   }
   sectLen = GRIB_UNSIGN_INT3 (*temp, temp[1], temp[2]);
   if (curLoc + sectLen > gribLen) {
      errSprintf ("Ran out of data in PDS (GRIB1_Inventory)\n");
      return -1;
   }
   pds = (uChar *) malloc (sectLen * sizeof (uChar));
   *pds = *temp;
   pds[1] = temp[1];
   pds[2] = temp[2];
   if (VSIFReadL (pds + 3, sizeof (char), sectLen - 3, fp) + 3 != sectLen) {
      errSprintf ("Ran out of file.\n");
      free (pds);
      return -1;
   }

   if (ReadGrib1Sect1 (pds, sectLen, gribLen, &curLoc, &pdsMeta, &f_gds, &gridID,
                       &f_bms, &DSF, &center, &subcenter) != 0) {
      preErrSprintf ("Inside GRIB1_Inventory\n");
      free (pds);
      return -1;
   }
   free (pds);

   *refTime = pdsMeta.refTime;

   /* Get to the end of the GRIB1 message. */
   /* (inventory.c : GRIB2Inventory), is responsible for this. */
   /* fseek (fp, gribLen - sectLen, SEEK_CUR); */
   return 0;
}

/*****************************************************************************
 * ReadGrib1Sect2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the GRIB1 "Grid Definition Section" or section 2, filling out
 * the gdsMeta data structure.
 *
 * ARGUMENTS
 *     gds = The compressed part of the message dealing with "GDS". (Input)
 * gribLen = The total length of the GRIB1 message. (Input)
 *  curLoc = Current location in the GRIB1 message. (Output)
 * gdsMeta = The filled out gdsMeta data structure. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = gribLen is too small.
 * -2 = unexpected values in gds.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created.
 *  12/2003 AAT: adas data encoder seems to have # of vertical data = 1, but
 *        parameters of vertical data = 255, which doesn't make sense.
 *        Changed the error from "fatal" to a warning in debug mode.
 *   6/2004 AAT: Modified to allow "extended" lat/lon grids (i.e. stretched or
 *        stretched and rotated).
 *
 * NOTES
 *****************************************************************************
 */
static int ReadGrib1Sect2 (uChar *gds, uInt4 gribLen, uInt4 *curLoc,
                           gdsType *gdsMeta)
{
   uInt4 sectLen;       /* Length in bytes of the current section. */
   int gridType;        /* Which type of grid. (see enumerated types). */
   double unit = 1e-3; /* Used for converting to the correct unit. */
   uInt4 uli_temp;      /* Used for reading a GRIB1 float. */
   int i;
   int f_allZero;       /* Used to find out if the "lat/lon" extension part
                         * is all 0 hence missing. */
   int f_allOne;        /* Used to find out if the "lat/lon" extension part
                         * is all 1 hence missing. */

   if( gribLen < *curLoc || gribLen - *curLoc < 6 ) {
      errSprintf ("Ran out of data in GDS (GRIB 1 Section 2)\n");
      return -1;
   }
   sectLen = GRIB_UNSIGN_INT3 (*gds, gds[1], gds[2]);
#ifdef DEBUG
/*
   printf ("Section 2 length = %ld\n", sectLen);
*/
#endif
   *curLoc += sectLen;
   if (*curLoc > gribLen) {
      errSprintf ("Ran out of data in GDS (GRIB 1 Section 2)\n");
      return -1;
   }
   gds += 3;
/*
#ifdef DEBUG
   if ((*gds != 0) || (gds[1] != 255)) {
      printf ("GRIB1 GDS: Expect (NV = 0) != %d, (PV = 255) != %d\n",
              *gds, gds[1]);
      errSprintf ("SectLen == %ld\n", sectLen);
      errSprintf ("GridType == %d\n", gds[2]);
   }
#endif
*/
#ifdef DEBUG
   /*if (gds[1] != 255) {
      printf ("\n\tCaution: GRIB1 GDS: FOR ALL NWS products, PV should be "
              "255 rather than %u\n", gds[1]);
   }*/
#endif
   if ((gds[1] != 255) && (gds[1] > 6)) {
      //errSprintf ("GRIB1 GDS: Expect PV = 255 != %d\n", gds[1]);
      //return -2;
   }
   gds += 2;
   gridType = *(gds++);
   switch (gridType) {
      case GB1S2_LATLON: // Latitude/Longitude Grid
      case GB1S2_GAUSSIAN_LATLON: // Gaussian Latitude/Longitude
      case GB1S2_ROTATED: // Rotated Latitude/Longitude
         if( gridType == GB1S2_ROTATED && (sectLen < 42)) {
            errSprintf ("For Rotated LatLon GDS, should have at least 42 bytes "
                        "of data\n");
            return -1;
         }
         if ((sectLen < 32)) {
            errSprintf ("For LatLon GDS, should have at least 32 bytes "
                        "of data\n");
            return -1;
         }
         switch(gridType) {
         case GB1S2_GAUSSIAN_LATLON:
            gdsMeta->projType = GS3_GAUSSIAN_LATLON;
            break;
         case GB1S2_ROTATED:
            gdsMeta->projType = GS3_ROTATED_LATLON;
            break;
         default:
            gdsMeta->projType = GS3_LATLON;
            break;
         }
         gdsMeta->orientLon = 0;
         gdsMeta->meshLat = 0;
         gdsMeta->scaleLat1 = 0;
         gdsMeta->scaleLat2 = 0;
         gdsMeta->southLat = 0;
         gdsMeta->southLon = 0;
         gdsMeta->center = 0;

         gdsMeta->Nx = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         if( gdsMeta->Nx == 65535 )
         {
             /* https://rda.ucar.edu/docs/formats/grib/gribdoc/llgrid.html */
             errSprintf ("Quasi rectangular grid with varying number of grids points per row are not supported\n");
             return -1;
         }
         gds += 2;
         gdsMeta->Ny = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         gds += 2;
         gdsMeta->lat1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->lon1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;

         gdsMeta->resFlag = *(gds++);
         if (gdsMeta->resFlag & 0x40) {
            gdsMeta->f_sphere = 0;
            gdsMeta->majEarth = 6378.160;
            gdsMeta->minEarth = 6356.775;
         } else {
            gdsMeta->f_sphere = 1;
            gdsMeta->majEarth = 6367.47;
            gdsMeta->minEarth = 6367.47;
         }

         gdsMeta->lat2 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->lon2 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->Dx = GRIB_UNSIGN_INT2 (*gds, gds[1]) * unit;
         gds += 2;
         if (gridType == GB1S2_GAUSSIAN_LATLON) {
            int np = GRIB_UNSIGN_INT2 (*gds, gds[1]); /* parallels between a pole and the equator */
            if( np == 0 )
            {
                errSprintf ("Invalid Gaussian LatLon\n" );
                return -1;
            }
            gdsMeta->Dy = 90.0 / np;
         } else
            gdsMeta->Dy = GRIB_UNSIGN_INT2 (*gds, gds[1]) * unit;
         gds += 2;
         gdsMeta->scan = *gds;
         gdsMeta->f_typeLatLon = 0;
#ifdef DEBUG
/*
         printf ("sectLen %ld\n", sectLen);
*/
#endif
         if (gridType == GB1S2_ROTATED && sectLen >= 42) {
            /* Check if all 0's or all 1's, which means f_typeLatLon == 0 */
            f_allZero = 1;
            f_allOne = 1;
            for (i = 0; i < 10; i++) {
               if (gds[i] != 0)
                  f_allZero = 0;
               if (gds[i] != 255)
                  f_allOne = 0;
            }
            if (!f_allZero && !f_allOne) {
               gdsMeta->f_typeLatLon = 3;
               gds += 5;
               gdsMeta->southLat = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                   unit);
               gds += 3;
               gdsMeta->southLon = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                   unit);
               gds += 3;
               MEMCPY_BIG (&uli_temp, gds, sizeof (sInt4));
               gdsMeta->angleRotate = fval_360 (uli_temp);
            }
         }
#if 0
         else if (gridType == GB1S2_STRETCHED && sectLen >= 42) {
            gds += 5;
            /* Check if all 0's or all 1's, which means f_typeLatLon == 0 */
            f_allZero = 1;
            f_allOne = 1;
            for (i = 0; i < 20; i++) {
               if (gds[i] != 0)
                  f_allZero = 0;
               if (gds[i] != 255)
                  f_allOne = 0;
            }
            if (!f_allZero && !f_allOne) {
               gdsMeta->f_typeLatLon = 1;
               gdsMeta->poleLat = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                   unit);
               gds += 3;
               gdsMeta->poleLon = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                   unit);
               gds += 3;
               MEMCPY_BIG (&uli_temp, gds, sizeof (sInt4));
               gdsMeta->stretchFactor = fval_360 (uli_temp);
            }
         else if (gridType == GB1S2_ROTATED_STRETCHED && sectLen >= 52) {
            gds += 5;
            /* Check if all 0's or all 1's, which means f_typeLatLon == 0 */
            f_allZero = 1;
            f_allOne = 1;
            for (i = 0; i < 20; i++) {
               if (gds[i] != 0)
                  f_allZero = 0;
               if (gds[i] != 255)
                  f_allOne = 0;
            }
            if (!f_allZero && !f_allOne) {
               gdsMeta->f_typeLatLon = 2;
               gdsMeta->southLat = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                    unit);
               gds += 3;
               gdsMeta->southLon = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                    unit);
               gds += 3;
               MEMCPY_BIG (&uli_temp, gds, sizeof (sInt4));
               gdsMeta->angleRotate = fval_360 (uli_temp);
               gds += 4;
               gdsMeta->poleLat = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                   unit);
               gds += 3;
               gdsMeta->poleLon = (GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) *
                                   unit);
               gds += 3;
               MEMCPY_BIG (&uli_temp, gds, sizeof (sInt4));
               gdsMeta->stretchFactor = fval_360 (uli_temp);
            }
#endif

         break;

      case GB1S2_POLAR:
         if (sectLen < 32) {
            errSprintf ("For Polar GDS, should have 32 bytes of data\n");
            return -1;
         }
         gdsMeta->projType = GS3_POLAR;
         gdsMeta->lat2 = 0;
         gdsMeta->lon2 = 0;
         gdsMeta->southLat = 0;
         gdsMeta->southLon = 0;

         gdsMeta->Nx = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         gds += 2;
         gdsMeta->Ny = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         gds += 2;
         gdsMeta->lat1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->lon1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;

         gdsMeta->resFlag = *(gds++);
         if (gdsMeta->resFlag & 0x40) {
            gdsMeta->f_sphere = 0;
            gdsMeta->majEarth = 6378.160;
            gdsMeta->minEarth = 6356.775;
         } else {
            gdsMeta->f_sphere = 1;
            gdsMeta->majEarth = 6367.47;
            gdsMeta->minEarth = 6367.47;
         }

         gdsMeta->orientLon = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->Dx = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]);
         gds += 3;
         gdsMeta->Dy = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]);
         gds += 3;
         gdsMeta->meshLat = 60; /* Depends on hemisphere. */
         gdsMeta->center = *(gds++);
         if (gdsMeta->center & GRIB2BIT_1) {
            /* South polar stereographic. */
            gdsMeta->scaleLat1 = gdsMeta->scaleLat2 = -90;
         } else {
            /* North polar stereographic. */
            gdsMeta->scaleLat1 = gdsMeta->scaleLat2 = 90;
         }
         gdsMeta->scan = *gds;
         break;

      case GB1S2_LAMBERT:
         if (sectLen < 42) {
            errSprintf ("For Lambert GDS, should have 42 bytes of data\n");
            return -1;
         }
         gdsMeta->projType = GS3_LAMBERT;
         gdsMeta->lat2 = 0;
         gdsMeta->lon2 = 0;

         gdsMeta->Nx = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         gds += 2;
         gdsMeta->Ny = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         gds += 2;
         gdsMeta->lat1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->lon1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;

         gdsMeta->resFlag = *(gds++);
         if (gdsMeta->resFlag & 0x40) {
            gdsMeta->f_sphere = 0;
            gdsMeta->majEarth = 6378.160;
            gdsMeta->minEarth = 6356.775;
         } else {
            gdsMeta->f_sphere = 1;
            gdsMeta->majEarth = 6367.47;
            gdsMeta->minEarth = 6367.47;
         }

         gdsMeta->orientLon = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->Dx = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]);
         gds += 3;
         gdsMeta->Dy = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]);
         gds += 3;
         gdsMeta->center = *(gds++);
         gdsMeta->scan = *(gds++);
         gdsMeta->scaleLat1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->scaleLat2 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->meshLat = gdsMeta->scaleLat1;
         gdsMeta->southLat = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->southLon = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         break;

      case GB1S2_MERCATOR:
         if (sectLen < 42) {
            errSprintf ("For Mercator GDS, should have 42 bytes of data\n");
            return -1;
         }
         gdsMeta->projType = GS3_MERCATOR;
         gdsMeta->southLat = 0;
         gdsMeta->southLon = 0;
         gdsMeta->orientLon = 0;
         gdsMeta->center = 0;

         gdsMeta->Nx = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         gds += 2;
         gdsMeta->Ny = GRIB_UNSIGN_INT2 (*gds, gds[1]);
         gds += 2;
         gdsMeta->lat1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->lon1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;

         gdsMeta->resFlag = *(gds++);
         if (gdsMeta->resFlag & 0x40) {
            gdsMeta->f_sphere = 0;
            gdsMeta->majEarth = 6378.160;
            gdsMeta->minEarth = 6356.775;
         } else {
            gdsMeta->f_sphere = 1;
            gdsMeta->majEarth = 6367.47;
            gdsMeta->minEarth = 6367.47;
         }

         gdsMeta->lat2 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->lon2 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->scaleLat1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) * unit;
         gds += 3;
         gdsMeta->scaleLat2 = gdsMeta->scaleLat1;
         gdsMeta->meshLat = gdsMeta->scaleLat1;
         /* Reserved set to 0. */
         gds++;
         gdsMeta->scan = *(gds++);
         gdsMeta->Dx = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]);
         gds += 3;
         gdsMeta->Dy = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]);
         break;

      default:
         errSprintf ("Grid projection number is %d\n", gridType);
         errSprintf ("Don't know how to handle this grid projection.\n");
         return -2;
   }
   gdsMeta->numPts = gdsMeta->Nx * gdsMeta->Ny;
#ifdef DEBUG
/*
printf ("NumPts = %ld\n", gdsMeta->numPts);
printf ("Nx = %ld, Ny = %ld\n", gdsMeta->Nx, gdsMeta->Ny);
*/
#endif
   return 0;
}

/*****************************************************************************
 * ReadGrib1Sect3() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the GRIB1 "Bit Map Section" or section 3, filling out the bitmap
 * as needed.
 *
 * ARGUMENTS
 *     bms = The compressed part of the message dealing with "BMS". (Input)
 * gribLen = The total length of the GRIB1 message. (Input)
 *  curLoc = Current location in the GRIB1 message. (Output)
 * pBitmap = Pointer to the extracted bitmap. (Output)
 *    NxNy = The total size of the grid. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = gribLen is too small.
 * -2 = unexpected values in bms.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int ReadGrib1Sect3 (uChar *bms, uInt4 gribLen, uInt4 *curLoc,
                           uChar **pBitmap, uInt4 NxNy)
{
   uInt4 sectLen;       /* Length in bytes of the current section. */
   short int numeric;   /* Determine if this is a predefined bitmap */
   uChar bits;          /* Used to locate which bit we are currently using. */
   uInt4 i;             /* Helps traverse the bitmap. */

   *pBitmap = nullptr;

   uInt4 bmsRemainingSize = gribLen - *curLoc;
   if( bmsRemainingSize < 6 )
   {
      errSprintf ("Ran out of data in BMS (GRIB 1 Section 3)\n");
      return -1;
   }
   sectLen = GRIB_UNSIGN_INT3 (*bms, bms[1], bms[2]);
#ifdef DEBUG
/*
   printf ("Section 3 length = %ld\n", sectLen);
*/
#endif
   *curLoc += sectLen;
   if (*curLoc > gribLen) {
      errSprintf ("Ran out of data in BMS (GRIB 1 Section 3)\n");
      return -1;
   }
   bms += 3;
   /* Assert: *bms currently points to number of unused bits at end of BMS. */
   if (NxNy + *bms + 6 * 8 != sectLen * 8) {
      errSprintf ("NxNy + # of unused bits != # of available bits\n");
      // commented out to avoid unsigned integer overflow
      // (sInt4) (NxNy + *bms), (sInt4) ((sectLen - 6) * 8));
      return -2;
   }
   bms++;
   /* Assert: Non-zero "numeric" means predefined bitmap. */
   numeric = GRIB_UNSIGN_INT2 (*bms, bms[1]);
   bms += 2;
   if (numeric != 0) {
      errSprintf ("Don't handle predefined bitmaps yet.\n");
      return -2;
   }
   bmsRemainingSize -= 6;
   if( bmsRemainingSize < (NxNy+7) / 8 )
   {
      errSprintf ("Ran out of data in BMS (GRIB 1 Section 3)\n");
      return -1;
   }
   *pBitmap = (uChar*) malloc(NxNy);
   auto bitmap = *pBitmap;
   if( bitmap== nullptr )
   {
      errSprintf ("Ran out of memory in allocating bitmap (GRIB 1 Section 3)\n");
      return -1;
   }
   bits = 0x80;
   for (i = 0; i < NxNy; i++) {
      *(bitmap++) = (*bms) & bits;
      bits = bits >> 1;
      if (bits == 0) {
         bms++;
         bits = 0x80;
      }
   }
   return 0;
}

#ifdef USE_UNPACKCMPLX
static int UnpackCmplx (uChar *bds, CPL_UNUSED uInt4 gribLen, CPL_UNUSED uInt4 *curLoc,
                        CPL_UNUSED short int DSF, CPL_UNUSED double *data, CPL_UNUSED grib_MetaData *meta,
                        CPL_UNUSED char f_bms, CPL_UNUSED uChar *bitmap, CPL_UNUSED double unitM,
                        CPL_UNUSED double unitB, CPL_UNUSED short int ESF, CPL_UNUSED double refVal,
                        uChar numBits, uChar f_octet14)
{
   uInt4 secLen;
   int N1;
   int N2;
   int P1;
   int P2;
   uChar octet14;
   uChar f_maxtrixValues;
   uChar f_secBitmap = 0;
   uChar f_secValDiffWid;
   int i;
   uInt4 uli_temp;      /* Used to store sInt4s (temporarily) */
   uChar bufLoc;        /* Keeps track of where to start getting more data
                         * out of the packed data stream. */
   size_t numUsed;      /* How many bytes were used in a given call to
                         * memBitRead. */
   uChar *width;

   secLen = 11;
   N1 = GRIB_UNSIGN_INT2 (bds[0], bds[1]);
   octet14 = bds[2];
   printf ("octet14, %d\n", octet14);
   if (f_octet14) {
      f_maxtrixValues = octet14 & GRIB2BIT_2;
      f_secBitmap = octet14 & GRIB2BIT_3;
      f_secValDiffWid = octet14 & GRIB2BIT_4;
      printf ("f_matrixValues, f_secBitmap, f_secValeDiffWid %d %d %d\n",
              f_maxtrixValues, f_secBitmap, f_secValDiffWid);
   }
   N2 = GRIB_UNSIGN_INT2 (bds[3], bds[4]);
   P1 = GRIB_UNSIGN_INT2 (bds[5], bds[6]);
   P2 = GRIB_UNSIGN_INT2 (bds[7], bds[8]);
   printf ("N1 N2 P1 P2 : %d %d %d %d\n", N1, N2, P1, P2);
   printf ("Reserved %u\n", bds[9]);
   bds += 10;
   secLen += 10;

   width = (uChar *) malloc (P1 * sizeof (uChar));

   for (i = 0; i < P1; i++) {
      width[i] = *bds;
      printf ("(Width %d %u)\n", i, width[i]);
      bds++;
      secLen++;
   }
   if (f_secBitmap) {
      bufLoc = 8;
      for (i = 0; i < P2; i++) {
         memBitRead (&uli_temp, sizeof (sInt4), bds, 1, &bufLoc, &numUsed);
         printf ("(%d %u) ", i, uli_temp);
         if (numUsed != 0) {
            printf ("\n");
            bds += numUsed;
            secLen++;
         }
      }
      if (bufLoc != 8) {
         bds++;
         secLen++;
      }
      printf ("Observed Sec Len %u\n", secLen);
   } else {
      /* Jump over widths and secondary bitmap */
      bds += (N1 - 21);
      secLen += (N1 - 21);
   }

   bufLoc = 8;
   for (i = 0; i < P1; i++) {
      memBitRead (&uli_temp, sizeof (sInt4), bds, numBits, &bufLoc, &numUsed);
      printf ("(%d %u) (numUsed %ld numBits %d)", i, uli_temp,
              (long) numUsed, numBits);
      if (numUsed != 0) {
         printf ("\n");
         bds += numUsed;
         secLen++;
      }
   }
   if (bufLoc != 8) {
      // cppcheck-suppress uselessAssignmentPtrArg
      bds++;
      secLen++;
   }

   printf ("Observed Sec Len %u\n", secLen);
   printf ("N2 = %d\n", N2);

   errSprintf ("Don't know how to handle Complex GRIB1 packing yet.\n");
   free (width);
   return -2;

}
#endif /* USE_UNPACKCMPLX */

/*****************************************************************************
 * ReadGrib1Sect4() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Unpacks the "Binary Data Section" or section 4.
 *
 * ARGUMENTS
 *     bds = The compressed part of the message dealing with "BDS". (Input)
 * gribLen = The total length of the GRIB1 message. (Input)
 *  curLoc = Current location in the GRIB1 message. (Input/Output)
 *     DSF = Decimal Scale Factor for unpacking the data. (Input)
 *    data = The extracted grid. (Output)
 *    meta = The meta data associated with the grid (Input/Output)
 *   f_bms = True if bitmap is to be used. (Input)
 *  bitmap = 0 if missing data, 1 if valid data. (Input)
 *   unitM = The M unit conversion value in equation y = Mx + B. (Input)
 *   unitB = The B unit conversion value in equation y = Mx + B. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = gribLen is too small.
 * -2 = unexpected values in bds.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created
 *   3/2004 AAT: Switched {# Pts * (# Bits in a Group) +
 *          # of unused bits != # of available bits} to a warning from an
 *          error.
 *
 * NOTES
 * 1) See metaparse.c : ParseGrid()
 * 2) Currently, only handles "Simple pack".
 *****************************************************************************
 */
static int ReadGrib1Sect4 (uChar *bds, uInt4 gribLen, uInt4 *curLoc,
                           short int DSF, double *data, grib_MetaData *meta,
                           char f_bms, uChar *bitmap, double unitM,
                           double unitB)
{
   uInt4 sectLen;       /* Length in bytes of the current section. */
   short int ESF;       /* Power of 2 scaling factor. */
   uInt4 uli_temp;      /* Used to store sInt4s (temporarily) */
   double refVal;       /* The reference value for the grid, also the minimum
                         * value. */
   uChar numBits;       /* # of bits for a single element of data. */
   uChar numUnusedBit;  /* # of extra bits at end of record. */
   uChar f_spherHarm;   /* Flag if data contains Spherical Harmonics. */
   uChar f_cmplxPack;   /* Flag if complex packing was used. */
#ifdef USE_UNPACKCMPLX
   uChar f_octet14;     /* Flag if octet 14 was used. */
#endif
   uChar bufLoc;        /* Keeps track of where to start getting more data
                         * out of the packed data stream. */
   uChar f_convert;     /* Determine if scan mode implies that we have to do
                         * manipulation as we read the grid to get desired
                         * internal scan mode. */
   uInt4 i;             /* Used to traverse the grid. */
   size_t numUsed;      /* How many bytes were used in a given call to
                         * memBitRead. */
   double d_temp;       /* Holds the extracted data until we put it in data */
   sInt4 newIndex;      /* Where to put the answer (primarily if f_convert) */
   sInt4 x;             /* Used to help compute newIndex , if f_convert. */
   sInt4 y;             /* Used to help compute newIndex , if f_convert. */
   double resetPrim;    /* If possible, used to reset the primary missing
                         * value from 9.999e20 to a reasonable # (9999) */

   if (meta->gds.Nx * meta->gds.Ny != meta->gds.numPts) {
      errSprintf ("(Nx * Ny != numPts) ?? in BDS (GRIB 1 Section 4)\n");
      return -2;
   }
   if( *curLoc >= gribLen )
       return -1;

   uInt4 bdsRemainingSize = gribLen - *curLoc;
   if( bdsRemainingSize < 3 )
       return -1;

   sectLen = GRIB_UNSIGN_INT3 (*bds, bds[1], bds[2]);
#ifdef DEBUG
/*
   printf ("Section 4 length = %ld\n", sectLen);
*/
#endif
   *curLoc += sectLen;
   if (*curLoc > gribLen) {
      errSprintf ("Ran out of data in BDS (GRIB 1 Section 4)\n");
      return -1;
   }
   bds += 3;
   bdsRemainingSize -= 3;

   /* Assert: bds now points to the main pack flag. */
   if( bdsRemainingSize < 1 )
       return -1;
   f_spherHarm = (*bds) & GRIB2BIT_1;
   f_cmplxPack = (*bds) & GRIB2BIT_2;
   meta->gridAttrib.fieldType = (*bds) & GRIB2BIT_3;
#ifdef USE_UNPACKCMPLX
   f_octet14 = (*bds) & GRIB2BIT_4;
#endif

   numUnusedBit = (*bds) & 0x0f;
#ifdef DEBUG
/*
   printf ("bds byte flag = %d\n", *bds);
   printf ("Number of unused bits = %d\n", numUnusedBit);
*/
#endif
   if (f_spherHarm) {
      errSprintf ("Don't know how to handle Spherical Harmonics yet.\n");
      return -2;
   }
/*
   if (f_octet14) {
      errSprintf ("Don't know how to handle Octet 14 data yet.\n");
      errSprintf ("bds byte flag = %d\n", *bds);
      errSprintf ("bds byte: %d %d %d %d\n", f_spherHarm, f_cmplxPack,
                  meta->gridAttrib.fieldType, f_octet14);
      return -2;
   }
*/
   if (f_cmplxPack) {
      meta->gridAttrib.packType = 2;
   } else {
      meta->gridAttrib.packType = 0;
   }
   bds++;
   bdsRemainingSize --;

   /* Assert: bds now points to E (power of 2 scaling factor). */
   if( bdsRemainingSize < 2 )
       return -1;
   ESF = GRIB_SIGN_INT2 (*bds, bds[1]);
   bds += 2;
   bdsRemainingSize -= 2;

   if( bdsRemainingSize < 4 )
       return -1;
   MEMCPY_BIG (&uli_temp, bds, sizeof (sInt4));
   refVal = fval_360 (uli_temp);
   bds += 4;
   bdsRemainingSize -= 4;

   /* Assert: bds is now the number of bits in a group. */
   if( bdsRemainingSize < 1 )
       return -1;
   numBits = *bds;
/*
#ifdef DEBUG
   printf ("refValue %f numBits %d\n", refVal, numBits);
   printf ("ESF %d DSF %d\n", ESF, DSF);
#endif
*/
   if (f_cmplxPack) {
#ifdef USE_UNPACKCMPLX
      bds++;
      bdsRemainingSize --;
      return UnpackCmplx (bds, gribLen, curLoc, DSF, data, meta, f_bms,
                          bitmap, unitM, unitB, ESF, refVal, numBits,
                          f_octet14);
#else
      errSprintf ("Don't know how to handle Complex GRIB1 packing yet.\n");
      return -2;
#endif
   }

   if (!f_bms && (
       sectLen < 11 ||
       (numBits > 0 && meta->gds.numPts > UINT_MAX / numBits) ||
       (meta->gds.numPts * numBits > UINT_MAX - numUnusedBit))) {
     printf ("numPts * (numBits in a Group) + # of unused bits != "
              "# of available bits\n");
   }
   else if (!f_bms &&
            (meta->gds.numPts * numBits + numUnusedBit) != (sectLen - 11) * 8) {
      printf ("numPts * (numBits in a Group) + # of unused bits %d != "
              "# of available bits %d\n",
              (sInt4) (meta->gds.numPts * numBits + numUnusedBit),
              (sInt4) ((sectLen - 11) * 8));
/*
      errSprintf ("numPts * (numBits in a Group) + # of unused bits %ld != "
                  "# of available bits %ld\n",
                  (sInt4) (meta->gds.numPts * numBits + numUnusedBit),
                  (sInt4) ((sectLen - 11) * 8));
      return -2;
*/
   }
   if (numBits > 32) {
      errSprintf ("The number of bits per number is larger than 32?\n");
      return -2;
   }
   bds++;
   bdsRemainingSize -= 1;

   /* Convert Units. */
   {
   double pow_10_DSF = pow (10.0, DSF);
   if( pow_10_DSF == 0.0 ) {
      errSprintf ("pow_10_DSF == 0.0\n");
      return -2;
   }
   if (unitM == -10) {
      meta->gridAttrib.min = pow (10.0, (refVal * pow (2.0, ESF) /
                                       pow_10_DSF));
   } else {
/*      meta->gridAttrib.min = unitM * (refVal / pow (10.0, DSF)) + unitB; */
      meta->gridAttrib.min = unitM * (refVal * pow (2.0, ESF) /
                                      pow_10_DSF) + unitB;
   }
   }

   meta->gridAttrib.max = meta->gridAttrib.min;
   meta->gridAttrib.f_maxmin = 1;
   meta->gridAttrib.numMiss = 0;
   if (refVal >= std::numeric_limits<float>::max() || CPLIsNan(refVal)) {
      meta->gridAttrib.refVal = std::numeric_limits<float>::max();
   } else if (refVal <= -std::numeric_limits<float>::max()) {
      meta->gridAttrib.refVal = -std::numeric_limits<float>::max();
   } else {
      meta->gridAttrib.refVal = static_cast<float>(refVal);
   }
   meta->gridAttrib.ESF = ESF;
   meta->gridAttrib.DSF = DSF;
   bufLoc = 8;
   /* Internally we use scan = 0100.  Scan is usually 0100 but if need be, we
    * can convert it. */
   f_convert = ((meta->gds.scan & 0xe0) != 0x40);

   if (f_bms) {
      meta->gridAttrib.f_miss = 1;
      meta->gridAttrib.missPri = UNDEFINED;
   }
   else
   {
      meta->gridAttrib.f_miss = 0;
   }

   if (f_bms) {
/*
#ifdef DEBUG
      printf ("There is a bitmap?\n");
#endif
*/
      /* Start unpacking the data, assuming there is a bitmap. */
      for (i = 0; i < meta->gds.numPts; i++) {
         /* Find the destination index. */
         if (f_convert) {
            /* ScanIndex2XY returns value as if scan was 0100 */
            ScanIndex2XY (i, &x, &y, meta->gds.scan, meta->gds.Nx,
                          meta->gds.Ny);
            newIndex = (x - 1) + (y - 1) * meta->gds.Nx;
         } else {
            newIndex = i;
         }
         /* A 0 in bitmap means no data. A 1 in bitmap means data. */
         // cppcheck-suppress nullPointer
         if (!bitmap[i]) {
            meta->gridAttrib.numMiss++;
            if( data ) data[newIndex] = UNDEFINED;
         } else {
            if (numBits != 0) {
                if( ((int)bdsRemainingSize - 1) * 8 + bufLoc < (int)numBits )
                    return -1;
               memBitRead (&uli_temp, sizeof (sInt4), bds, numBits,
                           &bufLoc, &numUsed);
               assert( numUsed <= bdsRemainingSize );
               bds += numUsed;
               bdsRemainingSize -= (uInt4)numUsed;
               d_temp = (refVal + (uli_temp * pow (2.0, ESF))) / pow (10.0, DSF);
               /* Convert Units. */
               if (unitM == -10) {
                  d_temp = pow (10.0, d_temp);
               } else {
                  d_temp = unitM * d_temp + unitB;
               }
               if (meta->gridAttrib.max < d_temp) {
                  meta->gridAttrib.max = d_temp;
               }
               if( data ) data[newIndex] = d_temp;
            } else {
               /* Assert: d_temp = unitM * refVal / pow (10.0,DSF) + unitB. */
               /* Assert: min = unitM * refVal / pow (10.0, DSF) + unitB. */
               if( data ) data[newIndex] = meta->gridAttrib.min;
            }
         }
      }
      /* Reset the missing value to UNDEFINED_PRIM if possible.  If not
       * possible, make sure UNDEFINED is outside the range.  If UNDEFINED
       * is_ in the range, choose max + 1 for missing. */
      resetPrim = 0;
      if ((meta->gridAttrib.max < UNDEFINED_PRIM) ||
          (meta->gridAttrib.min > UNDEFINED_PRIM)) {
         resetPrim = UNDEFINED_PRIM;
      } else if ((meta->gridAttrib.max >= UNDEFINED) &&
                 (meta->gridAttrib.min <= UNDEFINED)) {
         resetPrim = meta->gridAttrib.max + 1;
      }
      if (resetPrim != 0) {
         meta->gridAttrib.missPri = resetPrim;
      }
      if (data != nullptr && resetPrim != 0) {
         for (i = 0; i < meta->gds.numPts; i++) {
            /* Find the destination index. */
            if (f_convert) {
               /* ScanIndex2XY returns value as if scan was 0100 */
               ScanIndex2XY (i, &x, &y, meta->gds.scan, meta->gds.Nx,
                             meta->gds.Ny);
               newIndex = (x - 1) + (y - 1) * meta->gds.Nx;
            } else {
               newIndex = i;
            }
            // cppcheck-suppress nullPointer
            if (!bitmap[i]) {
               data[newIndex] = resetPrim;
            }
         }
      }

   } else {

      if( !data )
        return 0;

#ifdef DEBUG
/*
      printf ("There is no bitmap?\n");
*/
#endif

      /* Start unpacking the data, assuming there is NO bitmap. */
      for (i = 0; i < meta->gds.numPts; i++) {
         if (numBits != 0) {
            /* Find the destination index. */
            if (f_convert) {
               /* ScanIndex2XY returns value as if scan was 0100 */
               ScanIndex2XY (i, &x, &y, meta->gds.scan, meta->gds.Nx,
                             meta->gds.Ny);
               newIndex = (x - 1) + (y - 1) * meta->gds.Nx;
            } else {
               newIndex = i;
            }

            if( ((int)bdsRemainingSize - 1) * 8 + bufLoc < (int)numBits )
                return -1;
            memBitRead (&uli_temp, sizeof (sInt4), bds, numBits, &bufLoc,
                        &numUsed);
            assert( numUsed <= bdsRemainingSize );
            bds += numUsed;
            bdsRemainingSize -= (uInt4)numUsed;
            d_temp = (refVal + (uli_temp * pow (2.0, ESF))) / pow (10.0, DSF);

#ifdef DEBUG
/*
            if (i == 1) {
               printf ("refVal %f, uli_temp %ld, ans %f\n", refVal, uli_temp,
                       d_temp);
               printf ("numBits %d, bufLoc %d, numUsed %d\n", numBits,
                       bufLoc, numUsed);
            }
*/
#endif

            /* Convert Units. */
            if (unitM == -10) {
               d_temp = pow (10.0, d_temp);
            } else {
               d_temp = unitM * d_temp + unitB;
            }
            if (meta->gridAttrib.max < d_temp) {
               meta->gridAttrib.max = d_temp;
            }
            data[newIndex] = d_temp;
         } else {
            /* Assert: whole array = unitM * refVal + unitB. */
            /* Assert: *min = unitM * refVal + unitB. */
            data[i] = meta->gridAttrib.min;
         }
      }
   }
   return 0;
}

/*****************************************************************************
 * ReadGrib1Record() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Reads in a GRIB1 message, and parses the data into various data
 * structures, for use with other code.
 *
 * ARGUMENTS
 *           fp = An opened GRIB2 file already at the correct message. (Input)
 *       f_unit = 0 use GRIB2 units, 1 use English, 2 use metric. (Input)
 *    Grib_Data = The read in GRIB2 grid. (Output)
 * grib_DataLen = Size of Grib_Data. (Output)
 *         meta = A filled in meta structure (Output)
 *           IS = The structure containing all the arrays that the
 *                unpacker uses (Output)
 *        sect0 = Already read in section 0 data. (Input)
 *      gribLen = Length of the GRIB1 message. (Input)
 *     majEarth = Used to override the GRIB major axis of earth. (Input)
 *     minEarth = Used to override the GRIB minor axis of earth. (Input)
 *
 * FILES/DATABASES:
 *   An already opened file pointing to the desired GRIB1 message.
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = Problems reading in the PDS.
 * -2 = Problems reading in the GDS.
 * -3 = Problems reading in the BMS.
 * -4 = Problems reading in the BDS.
 * -5 = Problems reading the closing section.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created
 *   5/2003 AAT: Was not updating offset.  It should be updated by
 *               calling routine anyways, so I got rid of the parameter.
 *   7/2003 AAT: Allowed user to override the radius of earth.
 *   8/2003 AAT: Found a memory Leak (Had been setting unitName to NULL).
 *   2/2004 AAT: Added maj/min earth override.
 *   3/2004 AAT: Added ability to change units.
 *
 * NOTES
 * 1) Could also compare GDS with the one specified by gridID
 * 2) Could add gridID support.
 * 3) Should add unitM / unitB support.
 *****************************************************************************
 */
int ReadGrib1Record (VSILFILE *fp, sChar f_unit, double **Grib_Data,
                     uInt4 *grib_DataLen, grib_MetaData *meta,
                     IS_dataType *IS, sInt4 sect0[SECT0LEN_WORD],
                     uInt4 gribLen, double majEarth, double minEarth)
{
   sInt4 nd5;           /* Size of grib message rounded up to the nearest *
                         * sInt4. */
   uChar *c_ipack;      /* A char ptr to the message stored in IS->ipack */
   uInt4 curLoc;        /* Current location in the GRIB message. */
   char f_gds;          /* flag if there is a gds section. */
   char f_bms;          /* flag if there is a bms section. */
   double *grib_Data = nullptr;   /* A pointer to Grib_Data for ease of manipulation. */
   uChar *bitmap = nullptr; /* A char field (0=noData, 1=data) set up in BMS. */
   short int DSF;       /* Decimal Scale Factor for unpacking the data. */
   double unitM = 1;    /* M in y = Mx + B, for unit conversion. */
   double unitB = 0;    /* B in y = Mx + B, for unit conversion. */
   uChar gridID;        /* Which GDS specs to use. */
   const char *varName; /* The name of the data stored in the grid. */
   const char *varComment; /* Extra comments about the data stored in grid. */
   const char *varUnit; /* Holds the name of the unit [K] [%] .. etc */
   sInt4 li_temp;       /* Used to make sure section 5 is 7777. */
   char unitName[15];   /* Holds the string name of the current unit. */
   int unitLen;         /* String length of string name of current unit. */

   /* Make room for entire message, and read it in. */
   /* nd5 needs to be gribLen in (sInt4) units rounded up. */
   nd5 = (gribLen + 3) / 4;
   if (nd5 > IS->ipackLen) {
      if( gribLen > 100 * 1024 * 1024 )
      {
         vsi_l_offset curPos = VSIFTellL(fp);
         VSIFSeekL(fp, 0, SEEK_END);
         vsi_l_offset fileSize = VSIFTellL(fp);
         VSIFSeekL(fp, curPos, SEEK_SET);
         if( fileSize < gribLen )
         {
            errSprintf("File too short");
            return -1;
         }
      }
      sInt4* newipack = (sInt4 *) realloc ((void *) (IS->ipack),
                                     nd5 * sizeof (sInt4));
      if( newipack == nullptr )
      {
          errSprintf ("Out of memory\n");
          return -1;
      }
      IS->ipackLen = nd5;
      IS->ipack = newipack;
   }
   c_ipack = (uChar *) IS->ipack;
   /* Init last sInt4 to 0, to make sure that the padded bytes are 0. */
   IS->ipack[nd5 - 1] = 0;
   /* Init first 2 sInt4 to sect0. */
   memcpy (c_ipack, sect0, SECT0LEN_WORD * 2);
   /* Read in the rest of the message. */
   if (VSIFReadL (c_ipack + SECT0LEN_WORD * 2, sizeof (char),
              (gribLen - SECT0LEN_WORD * 2), fp) + SECT0LEN_WORD * 2 != gribLen) {
      errSprintf ("Ran out of file\n");
      return -1;
   }

   /* Preceding was in degrib2, next part is specific to GRIB1. */
   curLoc = 8;
   if (ReadGrib1Sect1 (c_ipack + curLoc, gribLen - curLoc, gribLen, &curLoc, &(meta->pds1),
                       &f_gds, &gridID, &f_bms, &DSF, &(meta->center),
                       &(meta->subcenter)) != 0) {
      preErrSprintf ("Inside ReadGrib1Record\n");
      return -1;
   }

   /* Get the Grid Definition Section. */
   if (f_gds) {
      if (ReadGrib1Sect2 (c_ipack + curLoc, gribLen, &curLoc,
                          &(meta->gds)) != 0) {
         preErrSprintf ("Inside ReadGrib1Record\n");
         return -2;
      }
      /* Could also compare GDS with the one specified by gridID? */
   } else {
      errSprintf ("Don't know how to handle a gridID lookup yet.\n");
      return -2;
   }
   meta->pds1.gridID = gridID;
   /* Allow data originating from NCEP to be 6371.2 by default. */
   if (meta->center == NMC) {
      if (meta->gds.majEarth == 6367.47) {
         meta->gds.f_sphere = 1;
         meta->gds.majEarth = 6371.2;
         meta->gds.minEarth = 6371.2;
      }
   }
   if ((majEarth > 6300) && (majEarth < 6400)) {
      if ((minEarth > 6300) && (minEarth < 6400)) {
         meta->gds.f_sphere = 0;
         meta->gds.majEarth = majEarth;
         meta->gds.minEarth = minEarth;
         if (majEarth == minEarth) {
            meta->gds.f_sphere = 1;
         }
      } else {
         meta->gds.f_sphere = 1;
         meta->gds.majEarth = majEarth;
         meta->gds.minEarth = majEarth;
      }
   }

   if( Grib_Data )
   {
     /* Allocate memory for the grid. */
     if (meta->gds.numPts > *grib_DataLen) {
      if( meta->gds.numPts > 100 * 1024 * 1024 )
      {
          vsi_l_offset curPos = VSIFTellL(fp);
          VSIFSeekL(fp, 0, SEEK_END);
          vsi_l_offset fileSize = VSIFTellL(fp);
          VSIFSeekL(fp, curPos, SEEK_SET);
          // allow a compression ratio of 1:1000
          if( meta->gds.numPts / 1000 > (uInt4)fileSize )
          {
            errSprintf ("ERROR: File too short\n");
            *grib_DataLen = 0;
            *Grib_Data = nullptr;
            return -2;
          }
      }

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
      if( meta->gds.numPts > static_cast<size_t>(INT_MAX) /  sizeof (double) )
      {
          errSprintf ("Memory allocation failed due to being bigger than 2 GB in fuzzing mode");
          *grib_DataLen = 0;
          *Grib_Data = nullptr;
          return -2;
      }
#endif

      *grib_DataLen = meta->gds.numPts;
      *Grib_Data = (double *) realloc ((void *) (*Grib_Data),
                                       (*grib_DataLen) * sizeof (double));
      if (!(*Grib_Data))
      {
        *grib_DataLen = 0;
        return -1;
      }
     }
     grib_Data = *Grib_Data;
   }

   /* Get the Bit Map Section. */
   if (f_bms) {
      if (ReadGrib1Sect3 (c_ipack + curLoc, gribLen, &curLoc, &bitmap,
                          meta->gds.numPts) != 0) {
         free (bitmap);
         preErrSprintf ("Inside ReadGrib1Record\n");
         return -3;
      }
   }

   /* Figure out some basic stuff about the grid. */
   /* Following is similar to metaparse.c : ParseElemName */
   GRIB1_Table2LookUp (&(meta->pds1), &varName, &varComment, &varUnit,
                       &(meta->convert), meta->center, meta->subcenter);
   meta->element = (char *) realloc ((void *) (meta->element),
                                     (1 + strlen (varName)) * sizeof (char));
   strcpy (meta->element, varName);
   meta->unitName = (char *) realloc ((void *) (meta->unitName),
                                      (1 + 2 + strlen (varUnit)) *
                                      sizeof (char));
   snprintf (meta->unitName,
            (1 + 2 + strlen (varUnit)) *
                                      sizeof (char),
            "[%s]", varUnit);
   meta->comment = (char *) realloc ((void *) (meta->comment),
                                     (1 + strlen (varComment) +
                                      strlen (varUnit)
                                      + 2 + 1) * sizeof (char));
   snprintf (meta->comment,
            (1 + strlen (varComment) +
                                      strlen (varUnit)
                                      + 2 + 1) * sizeof (char),
            "%s [%s]", varComment, varUnit);

   if (ComputeUnit (meta->convert, meta->unitName, f_unit, &unitM, &unitB,
                    unitName) == 0) {
      unitLen = static_cast<int>(strlen (unitName));
      meta->unitName = (char *) realloc ((void *) (meta->unitName),
                                         1 + unitLen * sizeof (char));
      memcpy (meta->unitName, unitName, unitLen);
      meta->unitName[unitLen] = '\0';
   }

   /* Read the GRID. */
   if (ReadGrib1Sect4 (c_ipack + curLoc, gribLen, &curLoc, DSF, grib_Data,
                       meta, f_bms, bitmap, unitM, unitB) != 0) {
      free (bitmap);
      preErrSprintf ("Inside ReadGrib1Record\n");
      return -4;
   }
   if (f_bms) {
      free (bitmap);
   }

   GRIB1_Table3LookUp (&(meta->pds1), &(meta->shortFstLevel),
                       &(meta->longFstLevel));
/*   printf ("%s .. %s\n", meta->shortFstLevel, meta->longFstLevel);*/

/*
   strftime (meta->refTime, 20, "%Y%m%d%H%M",
             gmtime (&(meta->pds1.refTime)));
*/
   Clock_Print (meta->refTime, 20, meta->pds1.refTime, "%Y%m%d%H%M", 0);

/*
   strftime (meta->validTime, 20, "%Y%m%d%H%M",
             gmtime (&(meta->pds1.validTime)));
*/
   Clock_Print (meta->validTime, 20, meta->pds1.validTime, "%Y%m%d%H%M", 0);

   double deltaTime = meta->pds1.validTime - meta->pds1.refTime;
   if (deltaTime >= std::numeric_limits<sInt4>::max()) {
       printf ("Clamped deltaTime.  Was %lf\n", deltaTime);
       deltaTime = std::numeric_limits<sInt4>::max();
   }
   if (deltaTime <= std::numeric_limits<sInt4>::min()) {
       printf ("Clamped deltaTime.  Was %lf\n", deltaTime);
       deltaTime = std::numeric_limits<sInt4>::min();
   }
   meta->deltTime = static_cast<sInt4>(deltaTime);

   /* Read section 5.  If it is "7777" == 926365495 we are done. */
   if (curLoc == gribLen) {
      printf ("Warning: either gribLen did not account for section 5, or "
              "section 5 is missing\n");
      return 0;
   }
   if (curLoc + 4 != gribLen) {
      errSprintf ("Invalid number of bytes for the end of the message.\n");
      return -5;
   }
   memcpy (&li_temp, c_ipack + curLoc, 4);
   if (li_temp != 926365495L) {
      errSprintf ("Did not find the end of the message.\n");
      return -5;
   }

   return 0;
}

/*****************************************************************************
 * main() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To test the capabilities of this module, and give an example as to how
 * ReadGrib1Record expects to be called.
 *
 * ARGUMENTS
 * argc = The number of arguments on the command line. (Input)
 * argv = The arguments on the command line. (Input)
 *
 * FILES/DATABASES:
 *   A GRIB1 file.
 *
 * RETURNS: int
 *  0 = OK
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created
 *   6/2003 Matthew T. Kallio (matt@wunderground.com):
 *          "wmo" dimension increased to WMO_HEADER_LEN + 1 (for '\0' char)
 *
 * NOTES
 *****************************************************************************
 */
#ifdef DEBUG_DEGRIB1
int main (int argc, char **argv)
{
   VSILFILE * grib_fp;       /* The opened grib2 file for input. */
   char *buff = nullptr;
   uInt4 buffLen = 0;
   sInt4 sect0[SECT0LEN_WORD] = { 0 }; /* Holds the current Section 0. */
   uInt4 gribLen;       /* Length of the current GRIB message. */
   char *msg;
   int version;
   sChar f_unit = 0;
   double *grib_Data;
   uInt4 grib_DataLen;
   grib_MetaData meta;

   double majEarth = 0.0;  // -radEarth if < 6000 ignore, otherwise use this
                           // to override the radEarth in the GRIB1 or GRIB2
                           // message.  Needed because NCEP uses 6371.2 but
                           // GRIB1 could only state 6367.47.
   double minEarth = 0.0;  // -minEarth if < 6000 ignore, otherwise use this
                           // to override the minEarth in the GRIB1 or GRIB2
                           // message.


   IS_dataType is;      /* Un-parsed meta data for this GRIB2 message. As
                         * well as some memory used by the unpacker. */

   if ((grib_fp = VSIFOpenL (argv[1], "rb")) == NULL) {
      printf ("Problems opening %s for read\n", argv[1]);
      return 1;
   }
   IS_Init (&is);
   MetaInit (&meta);

   if (ReadSECT0 (grib_fp, &buff, &buffLen, -1, sect0, &gribLen, &version) < 0) {
      VSIFCloseL(grib_fp);
      free(buff);
      msg = errSprintf (NULL);
      printf ("%s\n", msg);
      return -1;
   }
   free(buff);

   grib_DataLen = 0;
   grib_Data = NULL;
   if (version == 1) {
      meta.GribVersion = version;
      ReadGrib1Record (grib_fp, f_unit, &grib_Data, &grib_DataLen, &meta,
                       &is, sect0, gribLen, majEarth, minEarth);
   }

   MetaFree (&meta);
   IS_Free (&is);
   free (grib_Data);
   VSIFCloseL(grib_fp);
   return 0;
}
#endif

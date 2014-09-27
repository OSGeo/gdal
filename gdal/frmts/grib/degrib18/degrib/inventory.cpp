/*****************************************************************************
 * inventory.c
 *
 * DESCRIPTION
 *    This file contains the code needed to do a quick inventory of the GRIB2
 * file.  The intent is to enable one to figure out which message in a GRIB
 * file one is after without needing to call the FORTRAN library.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL / RSIS): Created.
 *  12/2002 Tim Kempisty, Ana Canizares, Tim Boyer, & Marc Saccucci
 *          (TK,AC,TB,&MS): Code Review 1.
 *
 * NOTES
 *****************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "clock.h"
#include "memendian.h"
#include "fileendian.h"
#include "degrib2.h"
#include "degrib1.h"
#include "tdlpack.h"
#include "myerror.h"
#include "myutil.h"
#include "myassert.h"
#include "inventory.h"
#include "metaname.h"
#include "filedatasource.h"

#define SECT0LEN_BYTE 16

typedef union {
   sInt4 li;
   char buffer[4];
} wordType;

/*****************************************************************************
 * GRIB2InventoryFree() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Free's any memory that was allocated for the inventory of a single grib
 * message
 *
 * ARGUMENTS
 * inv = Pointer to the inventory of a single grib message. (Input/Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   7/2003 AAT: memwatch detected unfreed inv->unitName
 *
 * NOTES
 *****************************************************************************
 */
void GRIB2InventoryFree (inventoryType *inv)
{
   free (inv->element);
   inv->element = NULL;
   free (inv->comment);
   inv->comment = NULL;
   free (inv->unitName);
   inv->unitName = NULL;
   free (inv->shortFstLevel);
   inv->shortFstLevel = NULL;
   free (inv->longFstLevel);
   inv->longFstLevel = NULL;
}

/*****************************************************************************
 * GRIB2InventoryPrint() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Prints to standard out, an inventory of the file, assuming one has an
 * array of invenories of single grib messages.
 *
 * ARGUMENTS
 *    Inv = Pointer to an Array of inventories to print. (Input)
 * LenInv = Length of the Array Inv (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   1/2004 AAT: Added short form of First level to print out.
 *   3/2004 AAT: Switched from "#, Byte, ..." to "MsgNum, Byte, ..."
 *
 * NOTES
 *****************************************************************************
 */
void GRIB2InventoryPrint (inventoryType *Inv, uInt4 LenInv)
{
   uInt4 i;             /* Counter of which inventory we are printing. */
   double delta;        /* Difference between valid and reference time. */
   char refTime[25];    /* Used to store the formatted reference time. */
   char validTime[25];  /* Used to store the formatted valid time. */

   printf ("MsgNum, Byte, GRIB-Version, elem, level, reference(UTC),"
           " valid(UTC), Proj(hr)\n");
   fflush (stdout);
   for (i = 0; i < LenInv; i++) {
/*      strftime (refTime, 25, "%m/%d/%Y %H:%M", gmtime (&(Inv[i].refTime)));*/
      Clock_Print (refTime, 25, Inv[i].refTime, "%m/%d/%Y %H:%M", 0);
/*      strftime (validTime, 25, "%m/%d/%Y %H:%M",
                gmtime (&(Inv[i].validTime)));*/
      Clock_Print (validTime, 25, Inv[i].validTime, "%m/%d/%Y %H:%M", 0);
      delta = (Inv[i].validTime - Inv[i].refTime) / 3600.;
      delta = myRound (delta, 2);
      if (Inv[i].comment == NULL) {
         printf ("%d.%d, %d, %d, %s, %s, %s, %s, %.2f\n",
                 Inv[i].msgNum, Inv[i].subgNum, Inv[i].start,
                 Inv[i].GribVersion, Inv[i].element, Inv[i].shortFstLevel,
                 refTime, validTime, delta);
         fflush (stdout);
      } else {
         printf ("%d.%d, %d, %d, %s=\"%s\", %s, %s, %s, %.2f\n",
                 Inv[i].msgNum, Inv[i].subgNum, Inv[i].start,
                 Inv[i].GribVersion, Inv[i].element, Inv[i].comment,
                 Inv[i].shortFstLevel, refTime, validTime, delta);
         fflush (stdout);
      }
   }
}

/*****************************************************************************
 * InventoryParseTime() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *    To parse the time data from a grib2 char array to a time_t in UTC
 * seconds from the epoch.  This is very similar to metaparse.c:ParseTime
 * except using char * instead of sInt4
 *
 * ARGUMENTS
 *      is = The char array to read the time info from. (Input)
 * AnsTime = The time_t value to fill with the resulting time. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  11/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *
 * NOTES
 * 1) Couldn't use the default time_zone variable (concern over portability
 *    issues), so we print the hours, and compare them to the hours we had
 *    intended.  Then subtract the difference from the AnsTime.
 * 2) Similar to metaparse.c:ParseTime except using char * instead of sInt4
 *****************************************************************************
 */
static int InventoryParseTime (char *is, double *AnsTime)
{
   /* struct tm time; *//* A temporary variable to put the time info into. */
   /* char buffer[10]; *//* Used when printing the AnsTime's Hr. */
   /* int timeZone; *//* The adjustment in Hr needed to get the right UTC * time. */
   short int si_temp;   /* Temporarily stores the year as a short int to fix
                         * possible endian problems. */

/*   memset (&time, 0, sizeof (struct tm));*/
   MEMCPY_BIG (&si_temp, is + 0, sizeof (short int));
   if ((si_temp < 1900) || (si_temp > 2100)) {
      return -1;
   }
   if ((is[2] > 12) || (is[3] == 0) || (is[3] > 31) || (is[4] > 24) ||
       (is[5] > 60) || (is[6] > 61)) {
      return -1;
   }
   Clock_ScanDate (AnsTime, si_temp, is[2], is[3]);
   *AnsTime += is[4] * 3600. + is[5] * 60. + is[6];
/*
   time.tm_year = si_temp - 1900;
   time.tm_mon = is[2] - 1;
   time.tm_mday = is[3];
   time.tm_hour = is[4];
   time.tm_min = is[5];
   time.tm_sec = is[6];
   *AnsTime = mktime (&time) - (Clock_GetTimeZone () * 3600);
*/
   /* Cheap method of getting global time_zone variable. */
/*
   strftime (buffer, 10, "%H", gmtime (AnsTime));
   timeZone = atoi (buffer) - is[4];
   if (timeZone < 0) {
      timeZone += 24;
   }
   *AnsTime = *AnsTime - (timeZone * 3600);
*/
   return 0;
}

/*****************************************************************************
 * GRIB2SectToBuffer() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *    To read in a GRIB2 section into a buffer.  Reallocates space for the
 * section if buffLen < secLen.  Reads in secLen and checks that the section
 * is valid, and the file is large enough to hold the entire section.
 *
 * ARGUMENTS
 *      fp = Opened file pointing to the section in question. (Input/Output)
 * gribLen = The total length of the grib message. (Input)
 *    sect = Which section we think we are reading.
 *           If it is -1, then set it to the section the file says we are
 *           reading (useful for optional sect 2)) (Input/Output).
 *  secLen = The length of this section (Output)
 * buffLen = Allocated length of buff (Input/Output)
 *    buff = Stores the section (Output)
 *
 * FILES/DATABASES:
 *    An already opened GRIB2 file pointer, already at section in question.
 *
 * RETURNS: int (could use errSprintf())
 *  0 = Ok.
 * -1 = Ran out of file.
 * -2 = Section was miss-labeled.
 *
 * HISTORY
 *  11/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   8/2003 AAT: Removed dependence on curTot
 *
 * NOTES
 *   May want to put this in degrib2.c
 *****************************************************************************
 */
static int GRIB2SectToBuffer (DataSource &fp,
                              CPL_UNUSED uInt4 gribLen,
                              sChar *sect,
                              uInt4 *secLen, uInt4 *buffLen, char **buff)
{
   char *buffer = *buff; /* Local ptr to buff to reduce ptr confusion. */

   if (FREAD_BIG (secLen, sizeof (sInt4), 1, fp) != 1) {
      if (*sect != -1) {
         errSprintf ("ERROR: Ran out of file in Section %d\n", *sect);
      } else {
         errSprintf ("ERROR: Ran out of file in GRIB2SectToBuffer\n");
      }
      return -1;
   }
   if (*buffLen < *secLen) {
      *buffLen = *secLen;
      *buff = (char *) realloc ((void *) *buff, *buffLen * sizeof (char));
      buffer = *buff;
   }

   if (fp.DataSourceFread (buffer, sizeof (char), *secLen - sizeof (sInt4)) !=
       *secLen - sizeof (sInt4)) {
      if (*sect != -1) {
         errSprintf ("ERROR: Ran out of file in Section %d\n", *sect);
      } else {
         errSprintf ("ERROR: Ran out of file in GRIB2SectToBuffer\n");
      }
      return -1;
   }
   if (*sect == -1) {
      *sect = buffer[5 - 5];
   } else if (buffer[5 - 5] != *sect) {
      errSprintf ("ERROR: Section %d misslabeled\n", *sect);
      return -2;
   }
   return 0;
}

/*****************************************************************************
 * GRIB2SectJump() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *    To jump past a GRIB2 section.  Reads in secLen and checks that the
 * section is valid.
 *
 * ARGUMENTS
 *      fp = Opened file pointing to the section in question. (Input/Output)
 * gribLen = The total length of the grib message. (Input)
 *    sect = Which section we think we are reading.
 *           If it is -1, then set it to the section the file says we are
 *           reading (useful for optional sect 2)) (Input/Output).
 *  secLen = The length of this section (Output)
 *
 * FILES/DATABASES:
 *    An already opened GRIB2 file pointer, already at section in question.
 *
 * RETURNS: int (could use errSprintf())
 *  0 = Ok.
 * -1 = Ran out of file.
 * -2 = Section was miss-labeled.
 *
 * HISTORY
 *   3/2003 Arthur Taylor (MDL/RSIS): Created.
 *   8/2003 AAT: Removed dependence on curTot, which was used to compute if
 *          the file should be large enough for the fseek, but didn't check
 *          if it actually was.
 *
 * NOTES
 *   May want to put this in degrib2.c
 *****************************************************************************
 */
static int GRIB2SectJump (DataSource &fp,
                          CPL_UNUSED sInt4 gribLen, sChar *sect, uInt4 *secLen)
{
   char sectNum;        /* Validates that we are on the correct section. */
   int c;               /* Check that the fseek is still inside the file. */

   if (FREAD_BIG (secLen, sizeof (sInt4), 1, fp) != 1) {
      if (*sect != -1) {
         errSprintf ("ERROR: Ran out of file in Section %d\n", *sect);
      } else {
         errSprintf ("ERROR: Ran out of file in GRIB2SectSkip\n");
      }
      return -1;
   }
   if (fp.DataSourceFread (&sectNum, sizeof (char), 1) != 1) {
      if (*sect != -1) {
         errSprintf ("ERROR: Ran out of file in Section %d\n", *sect);
      } else {
         errSprintf ("ERROR: Ran out of file in GRIB2SectSkip\n");
      }
      return -1;
   }
   if (*sect == -1) {
      *sect = sectNum;
   } else if (sectNum != *sect) {
      errSprintf ("ERROR: Section %d misslabeled\n", *sect);
      return -2;
   }
   /* Since fseek does not give an error if we jump outside the file, we test 
    * it by using fgetc / ungetc. */
   fp.DataSourceFseek (*secLen - 5, SEEK_CUR);
   if ((c = fp.DataSourceFgetc()) == EOF) {
      errSprintf ("ERROR: Ran out of file in Section %d\n", *sect);
      return -1;
   } else {
		 fp.DataSourceUngetc(c);
   }
   return 0;
}

/*****************************************************************************
 * GRIB2Inventory2to7() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Inventories sections 3 to 7, filling out the inv record with the data in
 * section 4.  (Note: No Call to FORTRAN routines here).
 *
 * ARGUMENTS
 *   sectNum = Which section we are currently reading. (Input)
 *        fp = An opened file pointer to the file to the inventory of (In/Out)
 *   gribLen = The total length of the grib message. (Input)
 *   buffLen = length of buffer. (Input)
 *    buffer = Holds a given section. (Input)
 *       inv = The current inventory record to fill out. (Output)
 *  prodType = The GRIB2 type of product: 0 is meteo product, 1 is hydro,
 *             2 is land, 3 is space, 10 is oceanographic. (Input)
 *    center = Who produced it (Input)
 * subcenter = A sub group of center that actually produced it (Input)
 *
 * FILES/DATABASES:
 *
 * RETURNS: int (could use errSprintf())
 *  0 = "Ok"
 * -5 = Problems Reading in section 2 or 3
 * -6 = Problems Reading in section 3
 * -7 = Problems Reading in section 4
 * -8 = Problems Parsing section 4.
 * -9 = Problems Reading in section 5
 * -10 = Problems Reading in section 6
 * -11 = Problems Reading in section 7
 *
 * HISTORY
 *   3/2003 Arthur Taylor (MDL/RSIS): Created.
 *   4/2003 AAT: Modified to not have prodType, cat, subcat, templat in
 *          inventoryType structure.
 *   8/2003 AAT: curTot no longer serves a purpose.
 *   1/2004 AAT: Added center/subcenter.
 *
 * NOTES
 *****************************************************************************
 */
static int GRIB2Inventory2to7 (sChar sectNum, DataSource &fp, sInt4 gribLen,
                               uInt4 *buffLen, char **buffer,
                               inventoryType *inv, uChar prodType,
                               unsigned short int center,
                               unsigned short int subcenter)
{
   uInt4 secLen;        /* The length of the current section. */
   sInt4 foreTime;      /* forecast time (NDFD treats as "projection") */
   uChar foreTimeUnit;  /* The time unit of the "forecast time". */
   /* char *element; *//* Holds the name of the current variable. */
   /* char *comment; *//* Holds more comments about current variable. */
   /* char *unitName; *//* Holds the name of the unit [K] [%] .. etc */
   int convert;         /* Enum type of unit conversions (metaname.c),
                         * Conversion method for this variable's unit. */
   uChar cat;           /* General category of Meteo Product. */
   unsigned short int templat; /* The section 4 template number. */
   uChar subcat;        /* Specific subcategory of Product. */
   uChar fstSurfType;   /* Type of the first fixed surface. */
   double fstSurfValue; /* Value of first fixed surface. */
   sInt4 value;         /* The scaled value from GRIB2 file. */
   sChar factor;        /* The scaled factor from GRIB2 file */
   sChar scale;         /* Surface scale as opposed to probility factor. */
   uChar sndSurfType;   /* Type of the second fixed surface. */
   double sndSurfValue; /* Value of second fixed surface. */
   sChar f_sndValue;    /* flag if SndValue is valid. */
   uChar timeRangeUnit;
   sInt4 lenTime;       /* Used by parseTime to tell difference betweeen 8hr
                         * average and 1hr average ozone. */
   uChar genID;         /* The Generating process ID (used for GFS MOS) */
   uChar probType;      /* The probability type */
   double lowerProb;    /* The lower limit on probability forecast if
                         * template 4.5 or 4.9 */
   double upperProb;    /* The upper limit on probability forecast if
                         * template 4.5 or 4.9 */
   uChar timeIncrType;
   sChar percentile = 0;

   if ((sectNum == 2) || (sectNum == 3)) {
      /* Jump past section (2 or 3). */
      sectNum = -1;
      if (GRIB2SectJump (fp, gribLen, &sectNum, &secLen) != 0) {
         errSprintf ("ERROR: Problems Jumping past section 2 || 3\n");
         return -6;
      }
      if ((sectNum != 2) && (sectNum != 3)) {
         errSprintf ("ERROR: Section 2 or 3 misslabeled\n");
         return -5;
      } else if (sectNum == 2) {
         /* Jump past section 3. */
         sectNum = 3;
         if (GRIB2SectJump (fp, gribLen, &sectNum, &secLen) != 0) {
            errSprintf ("ERROR: Problems Jumping past section 3\n");
            return -6;
         }
      }
   }
   /* Read section 4 into buffer. */
   sectNum = 4;
   if (GRIB2SectToBuffer (fp, gribLen, &sectNum, &secLen, buffLen,
                          buffer) != 0) {
      errSprintf ("ERROR: Problems with section 4\n");
      return -7;
   }
/*
enum { GS4_ANALYSIS, GS4_ENSEMBLE, GS4_DERIVED, GS4_PROBABIL_PNT = 5,
   GS4_STATISTIC = 8, GS4_PROBABIL_TIME = 9, GS4_PERCENTILE = 10,
   GS4_RADAR = 20, GS4_SATELLITE = 30
};
*/
   /* Parse the interesting data out of sect 4. */
   MEMCPY_BIG (&templat, *buffer + 8 - 5, sizeof (short int));
   if ((templat != GS4_ANALYSIS) && (templat != GS4_ENSEMBLE)
       && (templat != GS4_DERIVED)
       && (templat != GS4_PROBABIL_PNT) && (templat != GS4_STATISTIC)
       && (templat != GS4_PROBABIL_TIME) && (templat != GS4_PERCENTILE)
       && (templat != GS4_ENSEMBLE_STAT)
       && (templat != GS4_RADAR) && (templat != GS4_SATELLITE)
       && (templat != GS4_DERIVED_INTERVAL)) {
      errSprintf ("This was only designed for templates 0, 1, 2, 5, 8, 9, "
                  "10, 11, 12, 20, 30\n");
      return -8;
   }
   cat = (*buffer)[10 - 5];
   subcat = (*buffer)[11 - 5];
   genID = 0;
   probType = 0;
   lowerProb = 0;
   upperProb = 0;
   if ((templat == GS4_RADAR) || (templat == GS4_SATELLITE) ||
       (templat == 254)) {
      inv->foreSec = 0;
      inv->validTime = inv->refTime;
      timeIncrType = 255;
      timeRangeUnit = 255;
      lenTime = 0;
   } else {
      genID = (*buffer)[14 - 5];
      /* Compute forecast time. */
      foreTimeUnit = (*buffer)[18 - 5];
      MEMCPY_BIG (&foreTime, *buffer + 19 - 5, sizeof (sInt4));
      if (ParseSect4Time2sec (foreTime, foreTimeUnit, &(inv->foreSec)) != 0) {
         errSprintf ("unable to convert TimeUnit: %d \n", foreTimeUnit);
         return -8;
      }
      /* Compute valid time. */
      inv->validTime = inv->refTime + inv->foreSec;
      timeIncrType = 255;
      timeRangeUnit = 1;
      lenTime = (sInt4) (inv->foreSec / 3600);
      switch (templat) {
         case GS4_PROBABIL_PNT: /* 4.5 */
            probType = (*buffer)[37 - 5];
            factor = (sChar) (*buffer)[38 - 5];
            MEMCPY_BIG (&value, *buffer + 39 - 5, sizeof (sInt4));
            lowerProb = value * pow (10.0, -1 * factor);
            factor = (sChar) (*buffer)[43 - 5];
            MEMCPY_BIG (&value, *buffer + 44 - 5, sizeof (sInt4));
            upperProb = value * pow (10.0, -1 * factor);
            break;
         case GS4_DERIVED_INTERVAL: /* 4.12 */
            if (InventoryParseTime (*buffer + 37 - 5, &(inv->validTime)) != 0) {
               printf ("Warning: Investigate Template 4.12 bytes 37-43\n");
               inv->validTime = inv->refTime + inv->foreSec;
            }
            timeIncrType = (*buffer)[50 - 5];
            timeRangeUnit = (*buffer)[51 - 5];
            MEMCPY_BIG (&lenTime, *buffer + 52 - 5, sizeof (sInt4));
/* If lenTime == missing (2^32 -1) we might do something, but not with 255.*/
/*
            if (lenTime == 255) {
               lenTime = (inv->validTime -
                          (inv->refTime + inv->foreSec)) / 3600;
            }
*/
            break;
         case GS4_PERCENTILE: /* 4.10 */
            percentile = (*buffer)[35 - 5];
            if (InventoryParseTime (*buffer + 36 - 5, &(inv->validTime)) != 0) {
               printf ("Warning: Investigate Template 4.10 bytes 36-42\n");
               inv->validTime = inv->refTime + inv->foreSec;
            }
            timeIncrType = (*buffer)[49 - 5];
            timeRangeUnit = (*buffer)[50 - 5];
            MEMCPY_BIG (&lenTime, *buffer + 51 - 5, sizeof (sInt4));
/* If lenTime == missing (2^32 -1) we might do something, but not with 255.*/
/*
            if (lenTime == 255) {
               lenTime = (inv->validTime -
                          (inv->refTime + inv->foreSec)) / 3600;
            }
*/
            break;
         case GS4_STATISTIC: /* 4.8 */
            if (InventoryParseTime (*buffer + 35 - 5, &(inv->validTime)) != 0) {
               printf ("Warning: Investigate Template 4.8 bytes 35-41\n");
               inv->validTime = inv->refTime + inv->foreSec;
            }
            timeIncrType = (*buffer)[48 - 5];
            timeRangeUnit = (*buffer)[49 - 5];
            MEMCPY_BIG (&lenTime, *buffer + 50 - 5, sizeof (sInt4));
/* If lenTime == missing (2^32 -1) we might do something, but not with 255.*/
/*
            if (lenTime == 255) {
               lenTime = (inv->validTime -
                          (inv->refTime + inv->foreSec)) / 3600;
            }
*/
            break;
         case GS4_ENSEMBLE_STAT: /* 4.11 */
            if (InventoryParseTime (*buffer + 38 - 5, &(inv->validTime)) != 0) {
               printf ("Warning: Investigate Template 4.11 bytes 38-44\n");
               inv->validTime = inv->refTime + inv->foreSec;
            }
            timeIncrType = (*buffer)[51 - 5];
            timeRangeUnit = (*buffer)[52 - 5];
            MEMCPY_BIG (&lenTime, *buffer + 53 - 5, sizeof (sInt4));
/* If lenTime == missing (2^32 -1) we might do something, but not with 255.*/
/*
            if (lenTime == 255) {
               lenTime = (inv->validTime -
                          (inv->refTime + inv->foreSec)) / 3600;
            }
*/
            break;
         case GS4_PROBABIL_TIME: /* 4.9 */
            probType = (*buffer)[37 - 5];
            if ((uChar) (*buffer)[38 - 5] > 128) {
               factor = 128 - (uChar) (*buffer)[38 - 5];
            } else {
               factor = (*buffer)[38 - 5];
            }
            MEMCPY_BIG (&value, *buffer + 39 - 5, sizeof (sInt4));
            lowerProb = value * pow (10.0, -1 * factor);

            if ((uChar) (*buffer)[43 - 5] > 128) {
               factor = 128 - (uChar) (*buffer)[43 - 5];
            } else {
               factor = (*buffer)[43 - 5];
            }
            MEMCPY_BIG (&value, *buffer + 44 - 5, sizeof (sInt4));
            upperProb = value * pow (10.0, -1 * factor);

            if (InventoryParseTime (*buffer + 48 - 5, &(inv->validTime)) != 0) {
               printf ("Warning: Investigate Template 4.9 bytes 48-54\n");
               inv->validTime = inv->refTime + inv->foreSec;
            }
            timeIncrType = (*buffer)[61 - 5];
            timeRangeUnit = (*buffer)[62 - 5];
            MEMCPY_BIG (&lenTime, *buffer + 63 - 5, sizeof (sInt4));
/* If lenTime == missing (2^32 -1) we might do something, but not with 255.*/
/*
            if (lenTime == 255) {
               lenTime = (inv->validTime -
                          (inv->refTime + inv->foreSec)) / 3600;
            }
*/
            break;
      }
   }

   if (timeRangeUnit == 255) {
      timeRangeUnit = 1;
      lenTime = (sInt4) ((inv->validTime - inv->foreSec - inv->refTime) /
                         3600);
   }
/*   myAssert (timeRangeUnit == 1);*/
   /* Try to convert lenTime to hourly. */
   if (timeRangeUnit == 0) {
      lenTime = (sInt4) (lenTime / 60.);
      timeRangeUnit = 1;
   } else if (timeRangeUnit == 1) {
   } else if (timeRangeUnit == 2) {
      lenTime = lenTime * 24;
      timeRangeUnit = 1;
   } else if (timeRangeUnit == 10) {
      lenTime = lenTime * 3;
      timeRangeUnit = 1;
   } else if (timeRangeUnit == 11) {
      lenTime = lenTime * 6;
      timeRangeUnit = 1;
   } else if (timeRangeUnit == 12) {
      lenTime = lenTime * 12;
      timeRangeUnit = 1;
   } else if (timeRangeUnit == 13) {
      lenTime = (sInt4) (lenTime / 3600.);
      timeRangeUnit = 1;
   } else {
      printf ("Can't handle this timeRangeUnit\n");
      myAssert (timeRangeUnit == 1);
   }
   if (lenTime == GRIB2MISSING_s4) {
      lenTime = 0;
   }
   /* Find out what the name of this variable is. */
   ParseElemName (center, subcenter, prodType, templat, cat, subcat,
                  lenTime, timeIncrType, genID, probType, lowerProb,
                  upperProb, &(inv->element), &(inv->comment),
                  &(inv->unitName), &convert, percentile);
/*
   if (strcmp (element, "") == 0) {
      mallocSprintf (&(inv->element), "unknown");
      mallocSprintf (&(inv->unitName), "[%s]", unitName);
      if (strcmp (comment, "unknown") == 0) {
         mallocSprintf (&(inv->comment), "(prodType %d, cat %d, subcat %d)"
                        " [%s]", prodType, cat, subcat, unitName);
      } else {
         mallocSprintf (&(inv->comment), "%s [%s]", comment, unitName);
      }
   } else {
      if (IsData_MOS (center, subcenter)) {
         * See : http://www.nco.ncep.noaa.gov/pmb/docs/on388/tablea.html *
         if (genID == 96) {
            inv->element = (char *) malloc ((1 + 7 + strlen (element))
                                            * sizeof (char));
            sprintf (inv->element, "MOSGFS-%s", element);
         } else {
            inv->element = (char *) malloc ((1 + 4 + strlen (element))
                                            * sizeof (char));
            sprintf (inv->element, "MOS-%s", element);
         }
      } else {
         inv->element = (char *) malloc ((1 + strlen (element))
                                         * sizeof (char));
         strcpy (inv->element, element);
      }
      mallocSprintf (&(inv->unitName), "[%s]", unitName);
      mallocSprintf (&(inv->comment), "%s [%s]", comment, unitName);
*
      inv->unitName = (char *) malloc ((1 + 2 + strlen (unitName))
                                       * sizeof (char));
      sprintf (inv->unitName, "[%s]", unitName);
      inv->comment = (char *) malloc ((1 + 3 + strlen (unitName) + strlen (comment))
                                      * sizeof (char));
      sprintf (inv->comment, "%s [%s]", comment, unitName);
*
   }
*/

   if ((templat == GS4_RADAR) || (templat == GS4_SATELLITE)
       || (templat == 254) || (templat == 1000) || (templat == 1001)
       || (templat == 1002)) {
      reallocSprintf (&(inv->shortFstLevel), "0 undefined");
      reallocSprintf (&(inv->longFstLevel), "0.000[-] undefined ()");
   } else {
      fstSurfType = (*buffer)[23 - 5];
      scale = (*buffer)[24 - 5];
      MEMCPY_BIG (&value, *buffer + 25 - 5, sizeof (sInt4));
      if ((value == GRIB2MISSING_s4) || (scale == GRIB2MISSING_s1)) {
         fstSurfValue = 0;
      } else {
         fstSurfValue = value * pow (10.0, (int) (-1 * scale));
      }
      sndSurfType = (*buffer)[29 - 5];
      scale = (*buffer)[30 - 5];
      MEMCPY_BIG (&value, *buffer + 31 - 5, sizeof (sInt4));
      if ((value == GRIB2MISSING_s4) || (scale == GRIB2MISSING_s1) ||
          (sndSurfType == GRIB2MISSING_u1)) {
         sndSurfValue = 0;
         f_sndValue = 0;
      } else {
         sndSurfValue = value * pow (10.0, -1 * scale);
         f_sndValue = 1;
      }

      ParseLevelName (center, subcenter, fstSurfType, fstSurfValue,
                      f_sndValue, sndSurfValue, &(inv->shortFstLevel),
                      &(inv->longFstLevel));
   }

   /* Jump past section 5. */
   sectNum = 5;
   if (GRIB2SectJump (fp, gribLen, &sectNum, &secLen) != 0) {
      errSprintf ("ERROR: Problems Jumping past section 5\n");
      return -9;
   }
   /* Jump past section 6. */
   sectNum = 6;
   if (GRIB2SectJump (fp, gribLen, &sectNum, &secLen) != 0) {
      errSprintf ("ERROR: Problems Jumping past section 6\n");
      return -10;
   }
   /* Jump past section 7. */
   sectNum = 7;
   if (GRIB2SectJump (fp, gribLen, &sectNum, &secLen) != 0) {
      errSprintf ("ERROR: Problems Jumping past section 7\n");
      return -11;
   }
   return 0;
}

/*****************************************************************************
 * GRIB2Inventory() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Fills out an inventory structure for each GRIB message in a GRIB file,
 * without calling the FORTRAN routines to unpack the message.  It returns
 * the number of messages it found, or a negative number signifying an error.
 *
 * ARGUMENTS
 * filename = File to do the inventory of. (Input)
 *      Inv = The resultant array of inventories. (Output)
 *   LenInv = Length of the Array Inv (Output)
 *   numMsg = # of messages to inventory (0 = all, 1 = just first) (In)
 *   msgNum = MsgNum to start with, MsgNum of last message (Input/Output)
 *
 * FILES/DATABASES:
 *    Opens a GRIB2 file for reading given its filename.
 *
 * RETURNS: int (could use errSprintf())
 * +# = number of GRIB2 messages in the file.
 * -1 = Problems opening file for read.
 * -2 = Problems in section 0
 * -3 = Ran out of file.
 * -4 = Problems Reading in section 1
 * -5 = Problems Reading in section 2 or 3
 * -6 = Problems Reading in section 3
 * -7 = Problems Reading in section 4
 * -8 = Problems Parsing section 4.
 * -9 = Problems Reading in section 5
 * -10 = Problems Reading in section 6
 * -11 = Problems Reading in section 7
 * -12 = Problems inventory'ing a GRIB1 record
 * -13 = Problems inventory'ing a TDLP record
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT: Revised.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   3/2003 AAT: Corrected some satelite type mistakes.
 *   3/2003 AAT: Implemented multiple grid inventories in the same GRIB2
 *          message.
 *   4/2003 AAT: Started adding GRIB1 support
 *   6/2003 Matthew T. Kallio (matt@wunderground.com):
 *          "wmo" dimension increased to WMO_HEADER_LEN + 1 (for '\0' char)
 *   7/2003 AAT: Added numMsg so we can quickly find the reference time for
 *          a file by inventorying just the first message.
 *   8/2003 AAT: Adjusted use of GRIB_LIMIT to only affect the first message
 *          after we know we have a GRIB file, we don't want "trailing" bytes
 *          to break the program.
 *   8/2003 AAT: switched fileLen to only be computed for an error message.
 *   8/2003 AAT: curTot no longer serves a purpse.
 *   5/2004 AAT: Added a check for section number 2..8 for the repeated
 *          section (otherwise error)
 *  10/2004 AAT: Added ability to inventory TDLP records.
 *
 * NOTES
 *****************************************************************************
 */
int GRIB2Inventory (DataSource &fp, inventoryType **Inv, uInt4 *LenInv,
                    int numMsg, int *MsgNum)
{
   //FileDataSource fp (filename);            /* The opened GRIB2 file. */
   sInt4 offset = 0;    /* Where we are in the file. */
   sInt4 msgNum;        /* Which GRIB2 message we are on. */
   uInt4 gribLen;       /* Length of the current GRIB message. */
   uInt4 secLen;        /* Length of current section. */
   sChar sectNum;       /* Which section we are reading. */
   char *buff;          /* Holds the info between records. */
   uInt4 buffLen;       /* Length of info between records. */
   sInt4 sect0[SECT0LEN_WORD]; /* Holds the current Section 0. */
   char *buffer = NULL; /* Holds a given section. */
   uInt4 bufferLen = 0; /* Size of buffer. */
   inventoryType *inv;  /* Local ptr to Inv to reduce ptr confusion. */
   inventoryType *lastInv; /* Used to point to last inventory record when
                            * there are multiple grids in the same message. */
   wordType word;       /* Used to parse the prodType out of Sect 0. */
   int ans;             /* The return error code of ReadSect0. */
   char *msg;           /* Used to pop messages off the error Stack. */
   int version;         /* Which version of GRIB is in this message. */
   uChar prodType;      /* Which GRIB2 type of product, 0 is meteo, 1 is
                         * hydro, 2 is land, 3 is space, 10 is oceanographic. 
                         */
   int grib_limit;      /* How many bytes to look for before the first "GRIB" 
                         * in the file.  If not found, is not a GRIB file. */
   int c;               /* Determine if end of the file without fileLen. */
   sInt4 fileLen;       /* Length of the GRIB2 file. */
   unsigned short int center, subcenter; /* Who produced it. */
   // char *ptr;           /* used to find the file extension. */

   grib_limit = GRIB_LIMIT;
	 /*
   if (filename != NULL) {
      //if ((fp = fopen (filename, "rb")) == NULL) {
      //   errSprintf ("ERROR: Problems opening %s for read.", filename);
      //   return -1;
      //}
		  //fp = FileDataSource(filename);
      ptr = strrchr (filename, '.');
      if (ptr != NULL) {
         if (strcmp (ptr, ".tar") == 0) {
            grib_limit = 5000;
         }
      }
   } else {
      //fp = stdin; // TODO!!
   }
	 */
   msgNum = *MsgNum;

   buff = NULL;
   buffLen = 0;
   while ((c = fp.DataSourceFgetc()) != EOF) {
		 fp.DataSourceUngetc(c);
      // ungetc (c, fp);
      /* msgNum++ done first so any error messages range from 1..n, instead
       * of 0.. n-1. Note msgNum should end up as n not (n-1) */
      msgNum++;
/* Used when testing inventory of large TDLPack files. */
/*
#ifdef DEBUG
      myAssert (msgNum < 32500L);
      if (msgNum % 10 == 0) {
         printf ("%ld :: %f\n", msgNum, clock () / (double) CLOCKS_PER_SEC);
      }
#endif
*/
      /* Make it so the second, third, etc messages have no limit to finding
       * the "GRIB" keyword. */
      if (msgNum > 1) {
         grib_limit = -1;
      }
      /* Read in the wmo header and sect0. */
      if (ReadSECT0 (fp, &buff, &buffLen, grib_limit, sect0, &gribLen,
                     &version) < 0) {
         if (msgNum == 1) {
            /* Handle case where we couldn't find 'GRIB' in the message. */
            preErrSprintf ("Inside GRIB2Inventory, Message # %d\n", msgNum);
            free (buffer);
            free (buff);
            //fclose (fp);
            return -2;
         } else {
            /* Handle case where there are trailing bytes. */
            msg = errSprintf (NULL);
            printf ("Warning: Inside GRIB2Inventory, Message # %d\n",
                    msgNum);
            printf ("%s", msg);
            free (msg);
            /* find out how big the file is. */
            fp.DataSourceFseek (0L, SEEK_END);
            fileLen = fp.DataSourceFtell();
            /* fseek (fp, 0L, SEEK_SET); */
            printf ("There were %d trailing bytes in the file.\n",
                    fileLen - offset);
            free (buffer);
            free (buff);
            //fclose (fp);
            return msgNum;
         }
      }

      /* Make room for this GRIB message in the inventory list. */
      *LenInv = *LenInv + 1;
      *Inv = (inventoryType *) realloc ((void *) *Inv,
                                        *LenInv * sizeof (inventoryType));
      inv = *Inv + (*LenInv - 1);

      /* Start parsing the message. */
      inv->GribVersion = version;
      inv->msgNum = msgNum;
      inv->subgNum = 0;
      inv->start = offset;
      inv->element = NULL;
      inv->comment = NULL;
      inv->unitName = NULL;
      inv->shortFstLevel = NULL;
      inv->longFstLevel = NULL;

      if (version == 1) {
         if (GRIB1_Inventory (fp, gribLen, inv) != 0) {
            preErrSprintf ("Inside GRIB2Inventory \n");
            free (buffer);
            free (buff);
            //fclose (fp);
            return -12;
         }
      } else if (version == -1) {
         if (TDLP_Inventory (fp, gribLen, inv) != 0) {
            preErrSprintf ("Inside GRIB2Inventory \n");
            free (buffer);
            free (buff);
            //fclose (fp);
            return -13;
         }
      } else {
         word.li = sect0[1];
         prodType = word.buffer[2];

         /* Read section 1 into buffer. */
         sectNum = 1;
         if (GRIB2SectToBuffer (fp, gribLen, &sectNum, &secLen, &bufferLen,
                                &buffer) != 0) {
            errSprintf ("ERROR: Problems with section 1\n");
            free (buffer);
            free (buff);
            //fclose (fp);
            return -4;
         }
         /* Parse the interesting data out of sect 1. */
         InventoryParseTime (buffer + 13 - 5, &(inv->refTime));
         MEMCPY_BIG (&center, buffer + 6 - 5, sizeof (short int));
         MEMCPY_BIG (&subcenter, buffer + 8 - 5, sizeof (short int));

         sectNum = 2;
         do {
            /* Look at sections 2 to 7 */
            if ((ans = GRIB2Inventory2to7 (sectNum, fp, gribLen, &bufferLen,
                                           &buffer, inv, prodType, center,
                                           subcenter)) != 0) {
               //fclose (fp);
               free (buffer);
               free (buff);
               return ans;
            }
            /* Try to read section 8. If it is "7777" = 926365495 regardless
             * of endian'ness then we have a simple message, otherwise it is
             * complex, and we need to read more. */
            if (FREAD_BIG (&secLen, sizeof (sInt4), 1, fp) != 1) {
               errSprintf ("ERROR: Ran out of file looking for Sect 8.\n");
               free (buffer);
               free (buff);
               // fclose (fp);
               return -4;
            }
            if (secLen == 926365495L) {
               sectNum = 8;
            } else {
               if (fp.DataSourceFread (&sectNum, sizeof (char), 1) != 1) {
                  errSprintf ("ERROR: Ran out of file looking for "
                              "subMessage.\n");
                  free (buffer);
                  free (buff);
                  //fclose (fp);
                  return -4;
               }
               if ((sectNum < 2) || (sectNum > 7)) {
                  errSprintf ("ERROR (GRIB2Inventory): Couldn't find the end"
                              " of message\n");
                  errSprintf ("and it doesn't appear to repeat sections.\n");
                  errSprintf ("so it is probably an ASCII / binary bug\n");
                  free (buffer);
                  free (buff);
                  //fclose (fp);
                  return -4;
               }
               fp.DataSourceFseek (-5, SEEK_CUR);
               /* Make room for the next part of this GRIB message in the
                * inventory list.  This is for when we have sub-grids. */
               *LenInv = *LenInv + 1;
               *Inv = (inventoryType *) realloc ((void *) *Inv,
                                                 *LenInv *
                                                 sizeof (inventoryType));
               inv = *Inv + (*LenInv - 1);
               lastInv = *Inv + (*LenInv - 2);

               inv->GribVersion = version;
               inv->msgNum = msgNum;
               inv->subgNum = lastInv->subgNum + 1;
               inv->start = offset;
               inv->element = NULL;
               inv->comment = NULL;
               inv->unitName = NULL;
               inv->shortFstLevel = NULL;
               inv->longFstLevel = NULL;

               word.li = sect0[1];
               prodType = word.buffer[2];
               inv->refTime = lastInv->refTime;
            }
         } while (sectNum != 8);
      }

      /* added to inventory either first msgNum messages, or all messages */
      if (numMsg == msgNum) {
         break;
      }
      /* Continue on to the next GRIB2 message. */
      if (version == -1) {
         /* TDLPack uses 4 bytes for FORTRAN record size, then another 8
          * bytes for the size of the record (so FORTRAN can see it), then
          * the data rounded up to an 8 byte boundary, then a trailing 4
          * bytes for a final FORTRAN record size.  However it only stores
          * in_ the gribLen the non-rounded amount, so we need to take care
          * of the rounding, and the trailing 4 bytes here. */
         offset += buffLen + ((sInt4) ceil (gribLen / 8.0)) * 8 + 4;
      } else {
         offset += buffLen + gribLen;
      }
      fp.DataSourceFseek (offset, SEEK_SET);
   }
   free (buffer);
   free (buff);
   //fclose (fp);
   *MsgNum = msgNum;
   return msgNum;
}

int GRIB2RefTime (char *filename, double *refTime)
{
   FileDataSource fp (filename);            /* The opened GRIB2 file. */
   sInt4 offset = 0;    /* Where we are in the file. */
   sInt4 msgNum;        /* Which GRIB2 message we are on. */
   uInt4 gribLen;       /* Length of the current GRIB message. */
   uInt4 secLen;        /* Length of current section. */
   sChar sectNum;       /* Which section we are reading. */
   char *buff;          /* Holds the info between records. */
   uInt4 buffLen;       /* Length of info between records. */
   sInt4 sect0[SECT0LEN_WORD]; /* Holds the current Section 0. */
   char *buffer = NULL; /* Holds a given section. */
   uInt4 bufferLen = 0; /* Size of buffer. */
   /* wordType word; */       /* Used to parse the prodType out of Sect 0. */
   int ans;             /* The return error code of ReadSect0. */
   char *msg;           /* Used to pop messages off the error Stack. */
   int version;         /* Which version of GRIB is in this message. */
   /* uChar prodType; */      /* Which GRIB2 type of product, 0 is meteo, 1 is
                         * hydro, 2 is land, 3 is space, 10 is oceanographic. 
                         */
   int grib_limit;      /* How many bytes to look for before the first "GRIB" 
                         * in the file.  If not found, is not a GRIB file. */
   int c;               /* Determine if end of the file without fileLen. */
   sInt4 fileLen;       /* Length of the GRIB2 file. */
   char *ptr;           /* used to find the file extension. */
   double refTime1;

   grib_limit = GRIB_LIMIT;
   if (filename != NULL) {
      //if ((fp = fopen (filename, "rb")) == NULL) {
      //   errSprintf ("ERROR: Problems opening %s for read.", filename);
      //   return -1;
      //}
		  //fp = DataSource(filename);
      ptr = strrchr (filename, '.');
      if (ptr != NULL) {
         if (strcmp (ptr, ".tar") == 0) {
            grib_limit = 5000;
         }
      }
   } else {
      // fp = stdin; // TODO!!
   }
   msgNum = 0;

   buff = NULL;
   buffLen = 0;
   while ((c = fp.DataSourceFgetc()) != EOF) {
		 fp.DataSourceUngetc(c);
      /* msgNum++ done first so any error messages range from 1..n, instead
       * of 0.. n-1. Note msgNum should end up as n not (n-1) */
      msgNum++;
      /* Make it so the second, third, etc messages have no limit to finding
       * the "GRIB" keyword. */
      if (msgNum > 1) {
         grib_limit = -1;
      }
      /* Read in the wmo header and sect0. */
      if ((ans = ReadSECT0 (fp, &buff, &buffLen, grib_limit, sect0, &gribLen,
                            &version)) < 0) {
         if (msgNum == 1) {
            /* Handle case where we couldn't find 'GRIB' in the message. */
            preErrSprintf ("Inside GRIB2RefTime, Message # %d\n", msgNum);
            free (buffer);
            free (buff);
            //fclose (fp);
            return -2;
         } else {
            /* Handle case where there are trailing bytes. */
            msg = errSprintf (NULL);
            printf ("Warning: Inside GRIB2RefTime, Message # %d\n", msgNum);
            printf ("%s", msg);
            free (msg);
            /* find out how big the file is. */
            fp.DataSourceFseek (0L, SEEK_END);
            fileLen = fp.DataSourceFtell();
            /* fseek (fp, 0L, SEEK_SET); */
            printf ("There were %d trailing bytes in the file.\n",
                    fileLen - offset);
            free (buffer);
            free (buff);
            //fclose (fp);
            return msgNum;
         }
      }

      if (version == 1) {
         if (GRIB1_RefTime (fp, gribLen, &(refTime1)) != 0) {
            preErrSprintf ("Inside GRIB1_RefTime\n");
            free (buffer);
            free (buff);
            //fclose (fp);
            return -12;
         }
      } else if (version == -1) {
         if (TDLP_RefTime (fp, gribLen, &(refTime1)) != 0) {
            preErrSprintf ("Inside TDLP_RefTime\n");
            free (buffer);
            free (buff);
            //fclose (fp);
            return -13;
         }
      } else {
          /* word.li = sect0[1]; */
         /* prodType = word.buffer[2]; */

         /* Read section 1 into buffer. */
         sectNum = 1;
         if (GRIB2SectToBuffer (fp, gribLen, &sectNum, &secLen, &bufferLen,
                                &buffer) != 0) {
            errSprintf ("ERROR: Problems with section 1\n");
            free (buffer);
            //fclose (fp);
            return -4;
         }
         /* Parse the interesting data out of sect 1. */
         InventoryParseTime (buffer + 13 - 5, &(refTime1));
      }
      if (msgNum == 1) {
         *refTime = refTime1;
      } else {
         if (*refTime > refTime1) {
            *refTime = refTime1;
         }
      }

      /* Continue on to the next GRIB2 message. */
      offset += gribLen + buffLen;
      fp.DataSourceFseek (offset, SEEK_SET);
   }
   free (buffer);
   free (buff);
   //fclose (fp);
   return 0;
}

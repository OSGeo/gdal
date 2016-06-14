#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tdlpack.h"
#include "myerror.h"
#include "meta.h"
#include "memendian.h"
#include "fileendian.h"
#include "myassert.h"
#include "myutil.h"
#include "clock.h"
#ifdef MEMWATCH
#include "memwatch.h"
#endif

#define GRIB_UNSIGN_INT3(a,b,c) ((a<<16)+(b<<8)+c)
#define GRIB_UNSIGN_INT2(a,b) ((a<<8)+b)
#define GRIB_SIGN_INT3(a,b,c) ((1-(int) ((unsigned) (a & 0x80) >> 6)) * (int) (((a & 127) << 16)+(b<<8)+c))
#define GRIB_SIGN_INT2(a,b) ((1-(int) ((unsigned) (a & 0x80) >> 6)) * (int) (((a & 0x7f) << 8) + b))

/* *INDENT-OFF* */
static const TDLP_TableType TDLP_B_Table[5] = {
   /* 0 */ {0, "Continuous field"},
   /* 1 */ {1, "Point Binary - cumulative from above"},
   /* 2 */ {2, "Point Binary - cumulative from below"},
   /* 3 */ {3, "Point Binary - discrete"},
   /* 4 */ {5, "Grid Binary - "},
};

static const TDLP_TableType TDLP_DD_Table[9] = {
   /* 0 */ {0, "Independent of model"},
   /* 1 */ {6, "NGM model"},
   /* 2 */ {7, "Eta model"},
   /* 3 */ {8, "AVN model"},
   /* 4 */ {9, "MRF model"},
   /* 5 */ {79, "NDFD forecast"},
   /* 6 */ {80, "MOS AEV forecast"},
   /* 7 */ {81, "Local AEV Firecasts"},
   /* 8 */ {82, "Obs matching AEV Forecasts"},
};

static const TDLP_TableType TDLP_V_Table[4] = {
   /* 0 */ {0, "No vertical processing"},
   /* 1 */ {1, "Difference Levels (UUUU - LLLL)"},
   /* 2 */ {2, "Sum Levels (UUUU + LLLL)"},
   /* 3 */ {3, "Mean Levels (UUUU + LLLL) / 2."},
};

static const TDLP_TableType TDLP_T_Table[3] = {
   /* 0 */ {0, "No nolinear transform"},  // TODO: "No no"?
   /* 1 */ {1, "Square transform"},
   /* 2 */ {2, "Square root transform"},
};

static const TDLP_TableType TDLP_Oper_Table[9] = {
   /* 0 */ {0, "No time operator"},
   /* 1 */ {1, "Mean (Var 1, Var 2)"},
   /* 2 */ {2, "Difference (Var 1 - Var 2)"},
   /* 3 */ {3, "Maximum (Var 1, Var 2)"},
   /* 4 */ {4, "Minimum (Var 1, Var 2)"},
   /* 5 */ {5, "Mean (Var 1, Var 2)"},
   /* 6 */ {6, "Difference (Var 2 - Var 1)"},
   /* 7 */ {7, "Maximum (Var 1, Var 2)"},
   /* 8 */ {8, "Minimum (Var 1, Var 2)"},
};

static const TDLP_TableType TDLP_I_Table[4] = {
   /* 0 */ {0, "No interpolation"},
   /* 1 */ {1, "Bi-quadratic interpolation"},
   /* 2 */ {2, "Bi-linear interpolation"},
   /* 3 */ {3, "Special interpolation for QPF"},
};

static const TDLP_TableType TDLP_S_Table[6] = {
   /* 0 */ {0, "No smoothing"},
   /* 1 */ {1, "5-point smoothing"},
   /* 2 */ {2, "9-point smoothing"},
   /* 3 */ {3, "25-point smoothing"},
   /* 4 */ {4, "81-point smoothing"},
   /* 5 */ {5, "168-point smoothing"},
};
/* *INDENT-ON*  */

typedef struct {
   sInt4 min;           /* Minimum value in a group. Can cause problems if
                         * this is unsigned. */
   uChar bit;           /* # of bits in a group. 2^31 is the largest # of
                         * bits that can be used to hold # of bits in a
                         * group. However, the # of bits for a number can't
                         * be > 64, and is probably < 32, so bit is < 6 and
                         * probably < 5. */
   uInt4 num;           /* number of values in the group. May need this to be 
                         * signed. */
   sInt4 max;           /* Max value for the group */
   uInt4 start;         /* index in Data where group starts. */
   uChar f_trySplit;    /* Flag, if we have tried to split this group. */
   uChar f_tryShift;    /* Flag, if we have tried to shift this group. */
} TDLGroupType;

/*****************************************************************************
 * ReadTDLPSect1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the TDLP "Product Definition Section" or section 1, filling out
 * the pdsMeta data structure.
 *
 * ARGUMENTS
 *     pds = The compressed part of the message dealing with "PDS". (Input)
 * gribLen = The total length of the TDLP message. (Input)
 *  curLoc = Current location in the TDLP message. (Output)
 * pdsMeta = The filled out pdsMeta data structure. (Output)
 *   f_gds = boolean if there is a Grid Definition Section. (Output)
 *   f_bms = boolean if there is a Bitmap Section. (Output)
 *     DSF = Decimal Scale Factor for unpacking the data. (Output)
 *     BSF = Binary Scale Factor for unpacking the data. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = gribLen is too small.
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int ReadTDLPSect1 (uChar *pds, sInt4 tdlpLen, sInt4 *curLoc,
                          pdsTDLPType * pdsMeta, char *f_gds, char *f_bms,
                          short int *DSF, short int *BSF)
{
   char sectLen;        /* Length of section. */
   sInt4 li_temp;       /* Temporary variable. */
   int W, XXXX, YY;     /* Helps with Threshold calculation. */
   int year, t_year;    /* The reference year, and a consistency test */
   uChar month, t_month; /* The reference month, and a consistency test */
   uChar day, t_day;    /* The reference day, and a consistency test */
   uChar hour, t_hour;  /* The reference hour, and a consistency test */
   uChar min;           /* The reference minute */
   uShort2 project_hr;  /* The projection in hours. */
   sInt4 tau;           /* Used to cross check project_hr */
   int lenPL;           /* Length of the Plain Language descriptor. */

   sectLen = *(pds++);
   *curLoc += sectLen;
   if (*curLoc > tdlpLen) {
      errSprintf ("Ran out of data in PDS (TDLP Section 1)\n");
      return -1;
   }
   if( sectLen > 71 ) {
      errSprintf ("TDLP Section 1 is too big.\n");
      return -1;
   }
   if (sectLen < 39) {
      errSprintf ("TDLP Section 1 is too small.\n");
      return -1;
   }
   *f_bms = (GRIB2BIT_7 & *pds) ? 1 : 0;
   *f_gds = (GRIB2BIT_8 & *pds) ? 1 : 0;
   pds++;
   year = GRIB_UNSIGN_INT2 (*pds, pds[1]);
   pds += 2;
   month = *(pds++);
   day = *(pds++);
   hour = *(pds++);
   min = *(pds++);
   MEMCPY_BIG (&li_temp, pds, sizeof (sInt4));
   pds += 4;
   t_year = li_temp / 1000000;
   li_temp -= t_year * 1000000;
   t_month = li_temp / 10000;
   li_temp -= t_month * 10000;
   t_day = li_temp / 100;
   t_hour = li_temp - t_day * 100;
   if ((t_year != year) || (t_month != month) || (t_day != day) ||
       (t_hour != hour)) {
      errSprintf ("Error Inconsistent Times in ReadTDLPSect1.\n");
      return -1;
   }
   if (ParseTime (&(pdsMeta->refTime), year, month, day, hour, min, 0) != 0) {
      preErrSprintf ("Error In call to ParseTime in ReadTDLPSect1.\n");
      return -1;
   }
   MEMCPY_BIG (&(li_temp), pds, sizeof (sInt4));
   pds += 4;
   pdsMeta->ID1 = li_temp;
   pdsMeta->CCC = li_temp / 1000000;
   li_temp -= pdsMeta->CCC * 1000000;
   pdsMeta->FFF = li_temp / 1000;
   li_temp -= pdsMeta->FFF * 1000;
   pdsMeta->B = li_temp / 100;
   pdsMeta->DD = li_temp - pdsMeta->B * 100;
   MEMCPY_BIG (&(li_temp), pds, sizeof (sInt4));
   pds += 4;
   pdsMeta->ID2 = li_temp;
   pdsMeta->V = li_temp / 100000000;
   li_temp -= pdsMeta->V * 100000000;
   pdsMeta->LLLL = li_temp / 10000;
   pdsMeta->UUUU = li_temp - pdsMeta->LLLL * 10000;
   MEMCPY_BIG (&(li_temp), pds, sizeof (sInt4));
   pds += 4;
   pdsMeta->ID3 = li_temp;
   pdsMeta->T = li_temp / 100000000;
   li_temp -= pdsMeta->T * 100000000;
   pdsMeta->RR = li_temp / 1000000;
   li_temp -= pdsMeta->RR * 1000000;
   pdsMeta->Oper = li_temp / 100000;
   li_temp -= pdsMeta->Oper * 100000;
   pdsMeta->HH = li_temp / 1000;
   pdsMeta->ttt = li_temp - pdsMeta->HH * 1000;
   MEMCPY_BIG (&(li_temp), pds, sizeof (sInt4));
   pds += 4;
   pdsMeta->ID4 = li_temp;
   W = li_temp / 1000000000;
   li_temp -= W * 1000000000;
   XXXX = li_temp / 100000;
   li_temp -= XXXX * 100000;
   if (W) {
      XXXX = -1 * XXXX;
   }
   YY = li_temp / 1000;
   li_temp -= YY * 1000;
   if (YY >= 50) {
      YY = -1 * (YY - 50);
   }
   pdsMeta->thresh = (XXXX / 10000.) * pow (10.0, YY);
   pdsMeta->I = li_temp / 100;
   li_temp -= pdsMeta->I * 100;
   pdsMeta->S = li_temp / 10;
   pdsMeta->G = li_temp - pdsMeta->S * 10;
   project_hr = GRIB_UNSIGN_INT2 (*pds, pds[1]);
   tau = pdsMeta->ID3 - ((pdsMeta->ID3 / 1000) * 1000);
   if (tau != project_hr) {
      printf ("Warning: Inconsistent Projections in hours in "
              "ReadTDLPSect1 (%d vs %d)\n", tau, project_hr);
/*
      errSprintf ("Warning: Inconsistent Projections in hours in "
                  "ReadTDLPSect1 (%ld vs %d)\n", tau, project_hr);
*/
      project_hr = tau;
/*      return -1; */
   }
   pds += 2;
   pdsMeta->project = project_hr * 3600 + (*(pds++)) * 60;
   pdsMeta->procNum = (*(pds++));
   pdsMeta->seqNum = (*(pds++));
   *DSF = (*pds > 128) ? 128 - (*(pds++)) : (*(pds++));
   *BSF = (*pds > 128) ? 128 - (*(pds++)) : (*(pds++));
   if ((*pds != 0) || (pds[1] != 0) || (pds[2] != 0)) {
      errSprintf ("Error Reserved was not set to 0 in ReadTDLPSect1.\n");
      return -1;
   }
   pds += 3;
   lenPL = (*(pds++));
   if (sectLen - lenPL != 39) {
      errSprintf ("Error sectLen(%d) - lenPL(%d) != 39 in ReadTDLPSect1.\n",
                  sectLen, lenPL);
      return -1;
   }
   if (lenPL > 32) {
      lenPL = 32;
   }
   strncpy (pdsMeta->Descriptor, (char *) pds, lenPL);
   pdsMeta->Descriptor[lenPL] = '\0';
   strTrim (pdsMeta->Descriptor);
   return 0;
}

/*****************************************************************************
 * TDLP_TableLookUp() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To look up TDL Ids information in a given table.
 *
 * ARGUMENTS
 *    table = The Table to look up the Id in. (Input)
 * tableLen = The length of the Table. (Input)
 *    index = The index into the Table. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: char *
 *   Returns the meta data that is associated with index in the table.
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static const char *TDLP_TableLookUp(const TDLP_TableType * table, int tableLen,
                                    int index)
{
   int i;               /* Loop counter. */

   for (i = 0; i < tableLen; i++) {
      if (table[i].index == index)
         return (table[i].data);
   }
   return "Unknown";
}

/*****************************************************************************
 * PrintPDS_TDLP() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the message for the Product Definition Sections of the TDLP
 * Message.
 *
 * ARGUMENTS
 * pds = The TDLP Product Definition Section to print. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
void PrintPDS_TDLP (pdsTDLPType * pds)
{
   char buffer[25];     /* Stores format of pds1->refTime. */

/*
   strftime (buffer, 25, "%m/%d/%Y %H:%M:%S UTC", gmtime (&(pds->refTime)));
*/
   Clock_Print (buffer, 25, pds->refTime, "%m/%d/%Y %H:%M:%S UTC", 0);

   Print ("PDS-TDLP", "Reference Time", Prt_S, buffer);
   Print ("PDS-TDLP", "Plain Language", Prt_S, pds->Descriptor);
   snprintf(buffer, sizeof(buffer), "%09d", pds->ID1);
   Print ("PDS-TDLP", "ID 1", Prt_S, buffer);
   snprintf(buffer, sizeof(buffer), "%09d", pds->ID2);
   Print ("PDS-TDLP", "ID 2", Prt_S, buffer);
   snprintf(buffer, sizeof(buffer), "%09d", pds->ID3);
   Print ("PDS-TDLP", "ID 3", Prt_S, buffer);
   Print ("PDS-TDLP", "ID 4", Prt_D, pds->ID4);
   Print ("PDS-TDLP", "Model or Process Number", Prt_D, pds->procNum);
   Print ("PDS-TDLP", "Sequence Number", Prt_D, pds->seqNum);

   snprintf(buffer, sizeof(buffer), "%03d", pds->CCC);
   Print ("PDS-TDLP", "ID1-CCC", Prt_S, buffer);
   snprintf(buffer, sizeof(buffer), "%03d", pds->FFF);
   Print ("PDS-TDLP", "ID1-FFF", Prt_S, buffer);
   Print ("PDS-TDLP", "ID1-B", Prt_DS, pds->B,
          TDLP_TableLookUp (TDLP_B_Table, sizeof (TDLP_B_Table), pds->B));
   snprintf(buffer, sizeof(buffer), "%02d", pds->DD);
   Print ("PDS-TDLP", "ID1-DD", Prt_SS, buffer,
          TDLP_TableLookUp (TDLP_DD_Table, sizeof (TDLP_DD_Table), pds->DD));

   Print ("PDS-TDLP", "ID2-V", Prt_DS, pds->V,
          TDLP_TableLookUp (TDLP_V_Table, sizeof (TDLP_V_Table), pds->V));
   snprintf(buffer, sizeof(buffer), "%04d", pds->LLLL);
   Print ("PDS-TDLP", "ID2-LLLL", Prt_S, buffer);
   snprintf(buffer, sizeof(buffer), "%04d", pds->UUUU);
   Print ("PDS-TDLP", "ID2-UUUU", Prt_S, buffer);

   if (pds->Oper != 0) {
      Print ("PDS-TDLP", "ID3-T", Prt_DS, pds->T,
             TDLP_TableLookUp (TDLP_T_Table, sizeof (TDLP_T_Table), pds->T));
      snprintf(buffer, sizeof(buffer), "%02d", pds->RR);
      Print ("PDS-TDLP", "ID3-RR", Prt_SS, buffer,
             "Run time offset in hours");
      Print ("PDS-TDLP", "ID3-Oper", Prt_DS, pds->Oper,
             TDLP_TableLookUp (TDLP_Oper_Table, sizeof (TDLP_Oper_Table),
                               pds->Oper));
      snprintf(buffer, sizeof(buffer), "%02d", pds->HH);
      Print ("PDS-TDLP", "ID3-HH", Prt_SS, buffer,
             "Number of hours between variables");
   } else {
      Print ("PDS-TDLP", "ID3-Oper", Prt_DS, pds->Oper,
             TDLP_TableLookUp (TDLP_Oper_Table, sizeof (TDLP_Oper_Table),
                               pds->Oper));
   }
   snprintf(buffer, sizeof(buffer), "%03d", pds->ttt);
   Print ("PDS-TDLP", "ID3-ttt", Prt_SS, buffer, "Forecast Projection");

   Print ("PDS-TDLP", "ID4-thresh", Prt_F, pds->thresh);
   Print ("PDS-TDLP", "ID4-I", Prt_DS, pds->I,
          TDLP_TableLookUp (TDLP_I_Table, sizeof (TDLP_I_Table), pds->I));
   Print ("PDS-TDLP", "ID4-S", Prt_DS, pds->S,
          TDLP_TableLookUp (TDLP_S_Table, sizeof (TDLP_S_Table), pds->S));
   Print ("PDS-TDLP", "ID4-G", Prt_D, pds->G);
}

/*****************************************************************************
 * TDLP_ElemSurfUnit() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Deal with element name, unit, and comment for TDLP items.  Also deals
 * with the surface information stored in ID2.
 *
 * ARGUMENTS
 *           pds = The TDLP Product Definition Section to parse. (Input)
 *       element = The resulting element name. (Output)
 *      unitName = The resulting unit name. (Output)
 *       comment = The resulting comment. (Output)
 * shortFstLevel = The resulting short forecast level. (Output)
 *  longFstLevel = The resulting long forecast level. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static void TDLP_ElemSurfUnit (pdsTDLPType * pds, char **element,
                               char **unitName, char **comment,
                               char **shortFstLevel, char **longFstLevel)
{
   char *ptr;           /* Help guess unitName, and clean the elemName. */
   char *ptr2;          /* Help guess unitName, and clean the elemName. */

   myAssert (*element == NULL);
   myAssert (*unitName == NULL);
   myAssert (*comment == NULL);
   myAssert (*shortFstLevel == NULL);
   myAssert (*longFstLevel == NULL);

   *element = (char *) malloc (1 + strlen (pds->Descriptor) * sizeof (char));
   strcpy (*element, pds->Descriptor);
   (*element)[strlen (pds->Descriptor)] = '\0';

   ptr = strchr (*element, '(');
   if (ptr != NULL) {
      ptr2 = strchr (ptr, ')');
      *ptr2 = '\0';
      if (strcmp (ptr + 1, "unofficial id") == 0) {
         *unitName = (char *) malloc ((1 + 3) * sizeof (char));
         strcpy (*unitName, "[-]");
      } else {
         reallocSprintf (unitName, "[%s]", ptr + 1);
      }
      /* Trim any parens from element. */
      *ptr = '\0';
      strTrimRight (*element, ' ');
   } else {
      *unitName = (char *) malloc ((1 + 3) * sizeof (char));
      strcpy (*unitName, "[-]");
   }
   ptr = *element;
   while (*ptr != '\0') {
      if (*ptr == ' ') {
         *ptr = '-';
      }
      ptr++;
   }
   strCompact (*element, '-');

   reallocSprintf (comment, "%09ld-%09ld-%09ld-%ld %s", pds->ID1,
                   pds->ID2, pds->ID3, pds->ID4, *unitName);
   reallocSprintf (shortFstLevel, "%09ld", pds->ID2);
   reallocSprintf (longFstLevel, "%09ld", pds->ID2);
}

/*****************************************************************************
 * TDLP_Inventory() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the TDLP "Product Definition Section" for enough information to
 * fill out the inventory data structure so we can do a simple inventory on
 * the file in a similar way to how we did it for GRIB1 and GRIB2.
 *
 * ARGUMENTS
 *      fp = An opened TDLP file already at the correct message. (Input)
 * tdlpLen = The total length of the TDLP message. (Input)
 *     inv = The inventory data structure that we need to fill. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = tdlpLen is too small.
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *   Speed improvements...
 * 1) pds does not need to be allocated each time.
 * 2) Not all data is needed, do something like TDLP_RefTime
 * 3) TDLP_ElemSurfUnit may be slow?
 *****************************************************************************
 */
int TDLP_Inventory (DataSource &fp, sInt4 tdlpLen, inventoryType *inv)
{
   sInt4 curLoc;        /* Where we are in the current TDLP message. */
   int i_temp;
   uChar sectLen;       /* Length of section. */
   uChar *pds;          /* The part of the message dealing with the PDS. */
   pdsTDLPType pdsMeta; /* The pds parsed into a usable data structure. */
   char f_gds;          /* flag if there is a GDS section. */
   char f_bms;          /* flag if there is a BMS section. */
   short int DSF;       /* Decimal Scale Factor for unpacking the data. */
   short int BSF;       /* Binary Scale Factor for unpacking the data. */

   curLoc = 8;
   if ((i_temp = fp.DataSourceFgetc()) == EOF) {
      errSprintf ("Ran out of file in PDS (TDLP_Inventory).\n");
      return -1;
   }
   sectLen = (uChar) i_temp;
   curLoc += sectLen;
   if (curLoc > tdlpLen) {
      errSprintf ("Ran out of data in PDS (TDLP_Inventory)\n");
      return -1;
   }
   if( sectLen < 1 ) {
       errSprintf ("Wrong sectLen (TDLP_Inventory)\n");
       return -1;
   }
   pds = (uChar *) malloc (sectLen * sizeof (uChar));
   if( pds == NULL ) 
   {
      errSprintf ("Ran out of memory in PDS (TDLP_Inventory)\n");
      return -1;
   }
   *pds = sectLen;
   if (fp.DataSourceFread (pds + 1, sizeof (char), sectLen - 1) + 1 != sectLen) {
      errSprintf ("Ran out of file.\n");
      free (pds);
      return -1;
   }

   if (ReadTDLPSect1 (pds, tdlpLen, &curLoc, &pdsMeta, &f_gds, &f_bms,
                      &DSF, &BSF) != 0) {
      preErrSprintf ("Inside TDLP_Inventory\n");
      free (pds);
      return -1;
   }
   free (pds);

   inv->element = NULL;
   inv->unitName = NULL;
   inv->comment = NULL;
   free (inv->shortFstLevel);
   inv->shortFstLevel = NULL;
   free (inv->longFstLevel);
   inv->longFstLevel = NULL;
   TDLP_ElemSurfUnit (&pdsMeta, &(inv->element), &(inv->unitName),
                      &(inv->comment), &(inv->shortFstLevel),
                      &(inv->longFstLevel));

   inv->refTime = pdsMeta.refTime;
   inv->validTime = pdsMeta.refTime + pdsMeta.project;
   inv->foreSec = pdsMeta.project;
   return 0;
}

/*****************************************************************************
 * TDLP_RefTime() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Return just the reference time of a TDLP message.
 *
 * ARGUMENTS
 *      fp = An opened TDLP file already at the correct message. (Input)
 * tdlpLen = The total length of the TDLP message. (Input)
 * refTime = The reference time of interest. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = tdlpLen is too small.
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
int TDLP_RefTime (DataSource &fp, sInt4 tdlpLen, double *refTime)
{
   int sectLen;         /* Length of section. */
   sInt4 curLoc;        /* Where we are in the current TDLP message. */
   int c_temp;          /* Temporary variable for use with fgetc */
   short int si_temp;   /* Temporary variable. */
   int year, t_year;    /* The reference year, and a consistency test */
   uChar month, t_month; /* The reference month, and a consistency test */
   uChar day, t_day;    /* The reference day, and a consistency test */
   uChar hour, t_hour;  /* The reference hour, and a consistency test */
   uChar min;           /* The reference minute */
   sInt4 li_temp;       /* Temporary variable. */

   if ((sectLen = fp.DataSourceFgetc ()) == EOF)
      goto error;
   curLoc = 8 + sectLen;
   if (curLoc > tdlpLen) {
      errSprintf ("Ran out of data in PDS (TDLP_RefTime)\n");
      return -1;
   }
   if( sectLen > 71 ) {
      errSprintf ("TDLP Section 1 is too big.\n");
      return -1;
   }
   if (sectLen < 39) {
      errSprintf ("TDLP Section 1 is too small.\n");
      return -1;
   }

   if ((c_temp = fp.DataSourceFgetc()) == EOF)
      goto error;
   if (FREAD_BIG (&si_temp, sizeof (short int), 1, fp) != 1)
      goto error;
   year = si_temp;
   if ((c_temp = fp.DataSourceFgetc()) == EOF)
      goto error;
   month = c_temp;
   if ((c_temp = fp.DataSourceFgetc()) == EOF)
      goto error;
   day = c_temp;
   if ((c_temp = fp.DataSourceFgetc()) == EOF)
      goto error;
   hour = c_temp;
   if ((c_temp = fp.DataSourceFgetc()) == EOF)
      goto error;
   min = c_temp;

   if (FREAD_BIG (&li_temp, sizeof (sInt4), 1, fp) != 1)
      goto error;
   t_year = li_temp / 1000000;
   li_temp -= t_year * 1000000;
   t_month = li_temp / 10000;
   li_temp -= t_month * 10000;
   t_day = li_temp / 100;
   t_hour = li_temp - t_day * 100;

   if ((t_year != year) || (t_month != month) || (t_day != day) ||
       (t_hour != hour)) {
      errSprintf ("Error Inconsistent Times in TDLP_RefTime.\n");
      return -1;
   }
   if (ParseTime (refTime, year, month, day, hour, min, 0) != 0) {
      preErrSprintf ("Error In call to ParseTime in TDLP_RefTime.\n");
      return -1;
   }

   /* Get to the end of the TDLP message. */
   /* (inventory.c : GRIB2Inventory), is responsible for this. */
   /* fseek (fp, gribLen - sectLen, SEEK_CUR); */
   return 0;
 error:
   errSprintf ("Ran out of file in PDS (TDLP_RefTime).\n");
   return -1;
}

/*****************************************************************************
 * ReadTDLPSect2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the TDLP "Grid Definition Section" or section 2, filling out
 * the gdsMeta data structure.
 *
 * ARGUMENTS
 *     gds = The compressed part of the message dealing with "GDS". (Input)
 * tdlpLen = The total length of the TDLP message. (Input)
 *  curLoc = Current location in the TDLP message. (Output)
 * gdsMeta = The filled out gdsMeta data structure. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = tdlpLen is too small.
 * -2 = unexpected values in gds.
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int ReadTDLPSect2 (uChar *gds, sInt4 tdlpLen, sInt4 *curLoc,
                          gdsType *gdsMeta)
{
   char sectLen;        /* Length of section. */
   int gridType;        /* Which type of grid. (see enumerated types). */
   sInt4 li_temp;       /* Temporary variable. */

   sectLen = *(gds++);
   *curLoc += sectLen;
   if (*curLoc > tdlpLen) {
      errSprintf ("Ran out of data in GDS (TDLP Section 2)\n");
      return -1;
   }
   if( sectLen != 28 ) {
      errSprintf ("Wrong sectLen (TDLP Section 2)\n");
      return -1;
   }

   gridType = *(gds++);
   gdsMeta->Nx = GRIB_UNSIGN_INT2 (*gds, gds[1]);
   gds += 2;
   gdsMeta->Ny = GRIB_UNSIGN_INT2 (*gds, gds[1]);
   gds += 2;
   gdsMeta->lat1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) / 10000.0;
   gds += 3;
   gdsMeta->lon1 = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) / 10000.0;
   gdsMeta->lon1 = 360 - gdsMeta->lon1;
   if (gdsMeta->lon1 < 0) {
      gdsMeta->lon1 += 360;
   }
   if (gdsMeta->lon1 > 360) {
      gdsMeta->lon1 -= 360;
   }
   gds += 3;
   gdsMeta->orientLon = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) / 10000.0;
   gdsMeta->orientLon = 360 - gdsMeta->orientLon;
   if (gdsMeta->orientLon < 0) {
      gdsMeta->orientLon += 360;
   }
   if (gdsMeta->orientLon > 360) {
      gdsMeta->orientLon -= 360;
   }
   gds += 3;
   MEMCPY_BIG (&li_temp, gds, sizeof (sInt4));
   gdsMeta->Dx = li_temp / 1000.0;
   gds += 4;
   gdsMeta->meshLat = GRIB_SIGN_INT3 (*gds, gds[1], gds[2]) / 10000.0;
   gds += 3;
   if ((*gds != 0) || (gds[1] != 0) || (gds[2] != 0) || (gds[3] != 0) ||
       (gds[4] != 0) || (gds[5] != 0)) {
      errSprintf ("Error Reserved was not set to 0 in ReadTDLPSect2.\n");
      return -1;
   }

   gdsMeta->numPts = gdsMeta->Nx * gdsMeta->Ny;
   gdsMeta->f_sphere = 1;
   gdsMeta->majEarth = 6371.2;
   gdsMeta->minEarth = 6371.2;
   gdsMeta->Dy = gdsMeta->Dx;
   gdsMeta->resFlag = 0;
   gdsMeta->center = 0;
   gdsMeta->scan = 64;
   gdsMeta->lat2 = 0;
   gdsMeta->lon2 = 0;
   gdsMeta->scaleLat1 = gdsMeta->meshLat;
   gdsMeta->scaleLat2 = gdsMeta->meshLat;
   gdsMeta->southLat = 0;
   gdsMeta->southLon = 0;
   switch (gridType) {
      case TDLP_POLAR:
         gdsMeta->projType = GS3_POLAR;
         /* 4/24/2006 Added the following. */
         gdsMeta->scaleLat1 = 90;
         gdsMeta->scaleLat2 = 90;
         break;
      case TDLP_LAMBERT:
         gdsMeta->projType = GS3_LAMBERT;
         break;
      case TDLP_MERCATOR:
         gdsMeta->projType = GS3_MERCATOR;
         /* 4/24/2006 Added the following. */
         gdsMeta->scaleLat1 = 0;
         gdsMeta->scaleLat2 = 0;
         break;
      default:
         errSprintf ("Grid projection number is %d\n", gridType);
         return -2;
   }
   return 0;
}

/*****************************************************************************
 * ReadTDLPSect3() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses the TDLP "Bit Map Section" or section 3, filling out the bitmap
 * as needed.
 *
 * ARGUMENTS
 *     bms = The compressed part of the message dealing with "BMS". (Input)
 * tdlpLen = The total length of the TDLP message. (Input)
 *  curLoc = Current location in the TDLP message. (Output)
 *  bitmap = The extracted bitmap. (Output)
 *    NxNy = The total size of the grid. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = tdlpLen is too small.
 * -2 = unexpected values in bms.
 *
 * HISTORY
 *  10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int ReadTDLPSect3 (CPL_UNUSED uChar *bms,
                          CPL_UNUSED sInt4 tdlpLen,
                          CPL_UNUSED sInt4 *curLoc,
                          CPL_UNUSED uChar *bitmap,
                          CPL_UNUSED sInt4 NxNy)
{
   errSprintf ("Bitmap data is Not Supported\n");
   return -1;
}

/*****************************************************************************
 * ReadTDLPSect4() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Unpacks the "Binary Data Section" or section 4.
 *
 * ARGUMENTS
 *     bds = The compressed part of the message dealing with "BDS". (Input)
 * tdlpLen = The total length of the TDLP message. (Input)
 *  curLoc = Current location in the TDLP message. (Output)
 *     DSF = Decimal Scale Factor for unpacking the data. (Input)
 *     BSF = Binary Scale Factor for unpacking the data. (Input)
 *    data = The extracted grid. (Output)
 *    meta = The meta data associated with the grid (Input/Output)
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
 *   2/2005 AAT: Found bug: memBitRead grp[i].bit was sizeof sInt4 instead
 *          of uChar.
 *   2/2005 AAT: Second order diff, no miss value bug (lastData - 1) should
 *          be lastData.
 *   2/2005 AAT: Added test to see if the number of bits needed matches the
 *          section length.
 *
 * NOTES
 * 1) See metaparse.c : ParseGrid()
 *****************************************************************************
 */
static int ReadTDLPSect4 (uChar *bds, sInt4 tdlpLen, sInt4 *curLoc,
                          short int DSF, short int BSF, double *data,
                          grib_MetaData *meta,
                          CPL_UNUSED double unitM,
                          CPL_UNUSED double unitB)
{
   uInt4 sectLen;       /* Length in bytes of the current section. */
   uChar f_notGridPnt;  /* Not Grid point data? */
   uChar f_complexPack; /* Complex packing? */
   uChar f_sndOrder;    /* Second order differencing? */
   uChar f_primMiss;    /* Primary missing value? */
   uChar f_secMiss;     /* Secondary missing value? */
   uInt4 numPack;       /* Number of points packed. */
   sInt4 li_temp;       /* Temporary variable. */
   uInt4 uli_temp;      /* Temporary variable. */
   uChar bufLoc;        /* Keeps track of where to start getting more data
                         * out of the packed data stream. */
   uChar f_negative;    /* used to help with signs of numbers. */
   size_t numUsed;      /* How many bytes were used in a given call to
                         * memBitRead. */
   sInt4 origVal = 0;   /* Original value. */
   uChar mbit;          /* # of bits for abs (first first order difference) */
   sInt4 fstDiff = 0;   /* First first order difference. */
   sInt4 diff = 0;      /* general first order difference. */
   uChar nbit;          /* # of bits for abs (overall min value) */
   sInt4 minVal;        /* Minimum value. */
   size_t LX;           /* Number of groups. */
   uChar ibit;          /* # of bits for group min values. */
   uChar jbit;          /* # of bits for # of bits for group. */
   uChar kbit;          /* # of bits for # values in a group. */
   TDLGroupType *grp;   /* Holds the info about each group. */
   size_t i, j;         /* Loop counters. */
   uInt4 t_numPack;     /* Used to total number of values in a group to check
                         * the numPack value. */
   uInt4 t_numBits;     /* Used to total number of bits used in the groups. */
   uInt4 t_numBytes;    /* Used to total number of bytes used to compare to
                         * sectLen. */
   sInt4 maxVal;        /* The max value in a group. */
   uInt4 dataCnt;       /* How many values (miss or otherwise) we have read. */
   uInt4 lastData;      /* Index to last actual data. */
   uInt4 numVal;        /* # of actual (non-missing values) we have. */
   double scale;        /* Amount to scale values by. */
   uInt4 dataInd;       /* Index into data for this value (used to switch
                         * from a11..a1n,a2n..a21 to normal grid of
                         * a11..a1n,a21..a2n. */
   uChar f_missing;     /* Used to help with primary missing values, and the
                         * 0 bit possibility. */
#ifdef DEBUG
   sInt4 t_UK1 = 0;     /* Used to test theories about un defined values. */
   sInt4 t_UK2 = 0;     /* Used to test theories about un defined values. */
#endif

   sectLen = GRIB_UNSIGN_INT3 (*bds, bds[1], bds[2]);
   *curLoc += sectLen;
   if (*curLoc > tdlpLen) {
      errSprintf ("Ran out of data in BDS (TDLP Section 4)\n");
      return -1;
   }
   bds += 3;
   t_numBytes = 3;
   f_notGridPnt = (GRIB2BIT_4 & *bds) ? 1 : 0;
   f_complexPack = (GRIB2BIT_5 & *bds) ? 1 : 0;
   f_sndOrder = (GRIB2BIT_6 & *bds) ? 1 : 0;
   f_primMiss = (GRIB2BIT_7 & *bds) ? 1 : 0;
   f_secMiss = (GRIB2BIT_8 & *bds) ? 1 : 0;

   if (f_secMiss && (!f_primMiss)) {
      errSprintf ("Secondary missing value without a primary!\n");
      return -1;
   }
   if (f_complexPack) {
      if (!f_sndOrder) {
         meta->gridAttrib.packType = GS5_CMPLX;
      } else {
         meta->gridAttrib.packType = GS5_CMPLXSEC;
      }
   } else {
      errSprintf ("Simple pack is not supported at this time.\n");
      return -1;
   }
   bds++;
   MEMCPY_BIG (&numPack, bds, sizeof (sInt4));
   bds += 4;
   t_numBytes += 5;
   if (!f_notGridPnt) {
      if (numPack != meta->gds.numPts) {
         errSprintf ("Number packed %d != number of points %d\n", numPack,
                     meta->gds.numPts);
         return -1;
      }
   }
   meta->gridAttrib.DSF = DSF;
   meta->gridAttrib.ESF = BSF;
   meta->gridAttrib.fieldType = 0;
   if (!f_primMiss) {
      meta->gridAttrib.f_miss = 0;
      meta->gridAttrib.missPri = 0;
      meta->gridAttrib.missSec = 0;
   } else {
      MEMCPY_BIG (&li_temp, bds, sizeof (sInt4));
      bds += 4;
      t_numBytes += 4;
      meta->gridAttrib.missPri = li_temp / 10000.0;
      if (!f_secMiss) {
         meta->gridAttrib.f_miss = 1;
         meta->gridAttrib.missSec = 0;
      } else {
         MEMCPY_BIG (&li_temp, bds, sizeof (sInt4));
         bds += 4;
         t_numBytes += 4;
         meta->gridAttrib.missSec = li_temp / 10000.0;
         meta->gridAttrib.f_miss = 2;
      }
   }
   /* Init the buffer location. */
   bufLoc = 8;
   /* The origValue and fstDiff are only present if sndOrder packed. */
   if (f_sndOrder) {
      memBitRead (&f_negative, sizeof (f_negative), bds, 1, &bufLoc,
                  &numUsed);
      memBitRead (&uli_temp, sizeof (sInt4), bds, 31, &bufLoc, &numUsed);
      myAssert (numUsed == 4);
      bds += numUsed;
      t_numBytes += static_cast<uInt4>(numUsed);
      origVal = (f_negative) ? -1 * (sInt4)uli_temp : uli_temp;
      memBitRead (&mbit, sizeof (mbit), bds, 5, &bufLoc, &numUsed);
      memBitRead (&f_negative, sizeof (f_negative), bds, 1, &bufLoc,
                  &numUsed);
      myAssert (numUsed == 0);
      myAssert ((mbit > 0) && (mbit < 32));
      memBitRead (&uli_temp, sizeof (sInt4), bds, mbit, &bufLoc, &numUsed);
      bds += numUsed;
      t_numBytes += static_cast<uInt4>(numUsed);
      fstDiff = (f_negative) ? -1 * (sInt4)uli_temp : uli_temp;
   }
   memBitRead (&nbit, sizeof (nbit), bds, 5, &bufLoc, &numUsed);
   bds += numUsed;
   t_numBytes += static_cast<uInt4>(numUsed);
   memBitRead (&f_negative, sizeof (f_negative), bds, 1, &bufLoc, &numUsed);
   bds += numUsed;
   t_numBytes += static_cast<uInt4>(numUsed);
   myAssert ((nbit > 0) && (nbit < 32));
   memBitRead (&uli_temp, sizeof (sInt4), bds, nbit, &bufLoc, &numUsed);
   bds += numUsed;
   t_numBytes += static_cast<uInt4>(numUsed);
   minVal = (f_negative) ? -1 * (sInt4)uli_temp : uli_temp;
   memBitRead (&LX, sizeof (LX), bds, 16, &bufLoc, &numUsed);
   bds += numUsed;
   t_numBytes += static_cast<uInt4>(numUsed);
   grp = (TDLGroupType *) malloc (LX * sizeof (TDLGroupType));
   memBitRead (&ibit, sizeof (ibit), bds, 5, &bufLoc, &numUsed);
   bds += numUsed;
   t_numBytes += static_cast<uInt4>(numUsed);
   memBitRead (&jbit, sizeof (jbit), bds, 5, &bufLoc, &numUsed);
   /* Following assert is because it is the # of bits of # of bits.  Which
    * means that # of bits of value that has a max of 64. */
   myAssert (jbit < 6);
   bds += numUsed;
   t_numBytes += static_cast<uInt4>(numUsed);
   memBitRead (&kbit, sizeof (kbit), bds, 5, &bufLoc, &numUsed);
   bds += numUsed;
   t_numBytes += static_cast<uInt4>(numUsed);
   myAssert (ibit < 33);
   for (i = 0; i < LX; i++) {
      if (ibit == 0) {
         grp[i].min = 0;
      } else {
         memBitRead (&(grp[i].min), sizeof (sInt4), bds, ibit, &bufLoc,
                     &numUsed);
         bds += numUsed;
         t_numBytes += static_cast<uInt4>(numUsed);
      }
   }
   myAssert (jbit < 8);
   for (i = 0; i < LX; i++) {
      if (jbit == 0) {
         grp[i].bit = 0;
      } else {
         myAssert (jbit <= sizeof (uChar) * 8);
         memBitRead (&(grp[i].bit), sizeof (uChar), bds, jbit, &bufLoc,
                     &numUsed);
         bds += numUsed;
         t_numBytes += static_cast<uInt4>(numUsed);
      }
      myAssert (grp[i].bit < 32);
   }
   myAssert (kbit < 33);
   t_numPack = 0;
   t_numBits = 0;
   for (i = 0; i < LX; i++) {
      if (kbit == 0) {
         grp[i].num = 0;
      } else {
         memBitRead (&(grp[i].num), sizeof (sInt4), bds, kbit, &bufLoc,
                     &numUsed);
         bds += numUsed;
         t_numBytes += static_cast<uInt4>(numUsed);
      }
      t_numPack += grp[i].num;
      t_numBits += grp[i].num * grp[i].bit;
   }
   if (t_numPack != numPack) {
      errSprintf ("Number packed %d != number of values in groups %d\n",
                  numPack, t_numPack);
      free (grp);
      return -1;
   }
   if ((t_numBytes + ceil (t_numBits / 8.)) > sectLen) {
      errSprintf ("# bytes in groups %ld (%ld + %ld / 8) > sectLen %ld\n",
                  (sInt4) (t_numBytes + ceil (t_numBits / 8.)),
                  t_numBytes, t_numBits, sectLen);
      free (grp);
      return -1;
   }
   dataCnt = 0;
   dataInd = 0;

#ifdef DEBUG
   printf ("nbit %d, ibit %d, jbit %d, kbit %d\n", nbit, ibit, jbit, kbit);
   if ((t_numBytes + ceil (t_numBits / 8.)) != sectLen) {
      printf ("Caution: # bytes in groups %d (%d + %d / 8) != "
              "sectLen %d\n", (sInt4) (t_numBytes + ceil (t_numBits / 8.)),
              t_numBytes, t_numBits, sectLen);
   }
#endif

   /* Binary scale factor in TDLP has reverse sign from GRIB definition. */
   scale = pow (10.0, -1 * DSF) * pow (2.0, -1 * BSF);

   meta->gridAttrib.f_maxmin = 0;
   /* Work with Second order complex packed data. */
   if (f_sndOrder) {
      /* *INDENT-OFF* */
      /* The algorithm appears to be:
       * Data:      a1  a2 a3 a4 a5 ...
       * 1st diff:   0  b2 b3 b4 b5 ...
       * 2nd diff: UK1 UK2 c3 c4 c5 ...
       * We already know a1 and b2, and unpack a stream of UK1 UK2 c3 c4
       * The problem is that UK1, UK2 is undefined.  Originally I thought
       * this was 0, or c3, but it appears that if b2 != 0, then
       * UK2 = c3 + 2 b2, and UK1 = c3 + 1 * b2, otherwise it appears that
       * UK1 == UK2, and typically UK1 == c3 (but not always). */
      /* *INDENT-ON* */
      myAssert (numPack >= 2);
      if (f_secMiss) {
         numVal = 0;
         lastData = 0;
         for (i = 0; i < LX; i++) {
            maxVal = (1 << grp[i].bit) - 1;
            for (j = 0; j < grp[i].num; j++) {
               /* signed int. */
               memBitRead (&(li_temp), sizeof (sInt4), bds, grp[i].bit,
                           &bufLoc, &numUsed);
               bds += numUsed;
               if (li_temp == maxVal) {
                  data[dataInd] = meta->gridAttrib.missPri;
               } else if (li_temp == (maxVal - 1)) {
                  data[dataInd] = meta->gridAttrib.missSec;
               } else {
                  if (numVal > 1) {
#ifdef DEBUG
                     if (numVal == 2) {
                        if (fstDiff != 0) {
/*
                           myAssert (t_UK1 == li_temp + fstDiff);
                           myAssert (t_UK2 == li_temp + 2 * fstDiff);
*/
                        } else {
                           myAssert (t_UK1 == t_UK2);
                        }
                     }
#endif
                     diff += (li_temp + grp[i].min + minVal);
                     data[dataInd] = data[lastData] + diff * scale;
                     lastData = dataInd;
                  } else if (numVal == 1) {
                     data[dataInd] = (origVal + fstDiff) * scale;
                     lastData = dataInd;
                     diff = fstDiff;
#ifdef DEBUG
                     t_UK2 = li_temp;
#endif
                  } else {
                     data[dataInd] = origVal * scale;
#ifdef DEBUG
                     t_UK1 = li_temp;
#endif
                  }
                  numVal++;
                  if (!meta->gridAttrib.f_maxmin) {
                     meta->gridAttrib.min = data[dataInd];
                     meta->gridAttrib.max = data[dataInd];
                     meta->gridAttrib.f_maxmin = 1;
                  } else {
                     if (data[dataInd] < meta->gridAttrib.min) {
                        meta->gridAttrib.min = data[dataInd];
                     }
                     if (data[dataInd] > meta->gridAttrib.max) {
                        meta->gridAttrib.max = data[dataInd];
                     }
                  }
               }
               dataCnt++;
               dataInd = ((dataCnt / meta->gds.Nx) % 2) ?
                     (2 * (dataCnt / meta->gds.Nx) + 1) *
                     meta->gds.Nx - dataCnt - 1 : dataCnt;
               myAssert ((dataInd < numPack) || (dataCnt == numPack));
            }
         }
      } else if (f_primMiss) {
         numVal = 0;
         lastData = 0;
         for (i = 0; i < LX; i++) {
            maxVal = (1 << grp[i].bit) - 1;
            for (j = 0; j < grp[i].num; j++) {
               /* signed int. */
               memBitRead (&(li_temp), sizeof (sInt4), bds, grp[i].bit,
                           &bufLoc, &numUsed);
               bds += numUsed;
               f_missing = 0;
               if (li_temp == maxVal) {
                  data[dataInd] = meta->gridAttrib.missPri;
                  /* In the case of grp[i].bit == 0, if grp[i].min == 0, then 
                   * it is the missing value, otherwise regular value. Only
                   * need to be concerned for primary missing values. */
                  f_missing = 1;
                  if ((grp[i].bit == 0) && (grp[i].min != 0)) {
#ifdef DEBUG
                     printf ("This doesn't happen often.\n");
                     printf ("%d %d %d\n", (int) i, grp[i].bit, grp[i].min);
#endif
                     myAssert (1 == 2);
                     f_missing = 0;
                  }
               }
               if (!f_missing) {
                  if (numVal > 1) {
#ifdef DEBUG
                     if (numVal == 2) {
                        if (fstDiff != 0) {
/*
                           myAssert (t_UK1 == li_temp + fstDiff);
                           myAssert (t_UK2 == li_temp + 2 * fstDiff);
*/
                        } else {
                           myAssert (t_UK1 == t_UK2);
                        }
                     }
#endif
                     diff += (li_temp + grp[i].min + minVal);
                     data[dataInd] = data[lastData] + diff * scale;
                     lastData = dataInd;
                  } else if (numVal == 1) {
                     data[dataInd] = (origVal + fstDiff) * scale;
                     lastData = dataInd;
                     diff = fstDiff;
#ifdef DEBUG
                     t_UK2 = li_temp;
#endif
                  } else {
                     data[dataInd] = origVal * scale;
#ifdef DEBUG
                     t_UK1 = li_temp;
#endif
                  }
                  numVal++;
                  if (!meta->gridAttrib.f_maxmin) {
                     meta->gridAttrib.min = data[dataInd];
                     meta->gridAttrib.max = data[dataInd];
                     meta->gridAttrib.f_maxmin = 1;
                  } else {
                     if (data[dataInd] < meta->gridAttrib.min) {
                        meta->gridAttrib.min = data[dataInd];
                     }
                     if (data[dataInd] > meta->gridAttrib.max) {
                        meta->gridAttrib.max = data[dataInd];
                     }
                  }
               }
               dataCnt++;
               dataInd = ((dataCnt / meta->gds.Nx) % 2) ?
                     (2 * (dataCnt / meta->gds.Nx) + 1) *
                     meta->gds.Nx - dataCnt - 1 : dataCnt;
               myAssert ((dataInd < numPack) || (dataCnt == numPack));
            }
         }
      } else {
         lastData = 0;
         for (i = 0; i < LX; i++) {
            for (j = 0; j < grp[i].num; j++) {
               /* signed int. */
               memBitRead (&(li_temp), sizeof (sInt4), bds, grp[i].bit,
                           &bufLoc, &numUsed);
               bds += numUsed;
               if (dataCnt > 1) {
#ifdef DEBUG
                  if (dataCnt == 2) {
                     if (fstDiff != 0) {
/*
                        myAssert (t_UK1 == li_temp + fstDiff);
                        myAssert (t_UK2 == li_temp + 2 * fstDiff);
*/
                     } else {
                        myAssert (t_UK1 == t_UK2);
                     }
                  }
#endif
                  diff += (li_temp + grp[i].min + minVal);
                  data[dataInd] = data[lastData] + diff * scale;
                  lastData = dataInd;
               } else if (dataCnt == 1) {
                  data[dataInd] = (origVal + fstDiff) * scale;
                  lastData = dataInd;
                  diff = fstDiff;
#ifdef DEBUG
                  t_UK2 = li_temp;
#endif
               } else {
                  data[dataInd] = origVal * scale;
#ifdef DEBUG
                  t_UK1 = li_temp;
#endif
               }
#ifdef DEBUG
/*
               if (i >= 4153) {
*/
/*
               if ((data[dataInd] > 100) || (data[dataInd] < -100)) {
*/
/*
               if ((diff > 50) || (diff < -50)) {
                  printf ("li_temp :: %ld, diff = %ld\n", li_temp, diff);
                  printf ("data[dataInd] :: %f\n", data[dataInd]);
                  printf ("Group # %d element %d, grp[i].min %ld, "
                          "grp[i].bit %d, minVal %ld\n", i, j, grp[i].min,
                          grp[i].bit, minVal);
               }
*/
#endif
               if (!meta->gridAttrib.f_maxmin) {
                  meta->gridAttrib.min = data[dataInd];
                  meta->gridAttrib.max = data[dataInd];
                  meta->gridAttrib.f_maxmin = 1;
               } else {
                  if (data[dataInd] < meta->gridAttrib.min) {
                     meta->gridAttrib.min = data[dataInd];
                  }
                  if (data[dataInd] > meta->gridAttrib.max) {
                     meta->gridAttrib.max = data[dataInd];
                  }
               }
               dataCnt++;
               dataInd = ((dataCnt / meta->gds.Nx) % 2) ?
                     (2 * (dataCnt / meta->gds.Nx) + 1) *
                     meta->gds.Nx - dataCnt - 1 : dataCnt;
               myAssert ((dataInd < numPack) || (dataCnt == numPack));
            }
         }
         numVal = dataCnt;
      }

      /* Work with regular complex packed data. */
   } else {
#ifdef DEBUG
/*
   printf ("Work with regular complex packed data\n");
*/
#endif
      if (f_secMiss) {
         numVal = 0;
         for (i = 0; i < LX; i++) {
            maxVal = (1 << grp[i].bit) - 1;
            for (j = 0; j < grp[i].num; j++) {
               /* signed int. */
               memBitRead (&(li_temp), sizeof (sInt4), bds, grp[i].bit,
                           &bufLoc, &numUsed);
               bds += numUsed;
               if (li_temp == maxVal) {
                  data[dataInd] = meta->gridAttrib.missPri;
               } else if (li_temp == (maxVal - 1)) {
                  data[dataInd] = meta->gridAttrib.missSec;
               } else {
                  data[dataInd] = (li_temp + grp[i].min + minVal) * scale;
                  numVal++;
                  if (!meta->gridAttrib.f_maxmin) {
                     meta->gridAttrib.min = data[dataInd];
                     meta->gridAttrib.max = data[dataInd];
                     meta->gridAttrib.f_maxmin = 1;
                  } else {
                     if (data[dataInd] < meta->gridAttrib.min) {
                        meta->gridAttrib.min = data[dataInd];
                     }
                     if (data[dataInd] > meta->gridAttrib.max) {
                        meta->gridAttrib.max = data[dataInd];
                     }
                  }
               }
               dataCnt++;
               dataInd = ((dataCnt / meta->gds.Nx) % 2) ?
                     (2 * (dataCnt / meta->gds.Nx) + 1) *
                     meta->gds.Nx - dataCnt - 1 : dataCnt;
               myAssert ((dataInd < numPack) || (dataCnt == numPack));
            }
         }
      } else if (f_primMiss) {
#ifdef DEBUG
/*
   printf ("Work with primary missing data\n");
*/
#endif
         numVal = 0;
         for (i = 0; i < LX; i++) {
            maxVal = (1 << grp[i].bit) - 1;
            for (j = 0; j < grp[i].num; j++) {
               memBitRead (&(li_temp), sizeof (sInt4), bds, grp[i].bit,
                           &bufLoc, &numUsed);
               bds += numUsed;
               f_missing = 0;
               if (li_temp == maxVal) {
                  data[dataInd] = meta->gridAttrib.missPri;
                  /* In the case of grp[i].bit == 0, if grp[i].min == 0, then 
                   * it is the missing value, otherwise regular value. Only
                   * need to be concerned for primary missing values. */
                  f_missing = 1;
                  if ((grp[i].bit == 0) && (grp[i].min != 0)) {
#ifdef DEBUG
                     printf ("This doesn't happen often.\n");
                     printf ("%d %d %d\n", (int) i, grp[i].bit, grp[i].min);
                     myAssert (1 == 2);
#endif
                     f_missing = 0;
                  }
               }
               if (!f_missing) {
                  data[dataInd] = (li_temp + grp[i].min + minVal) * scale;
                  numVal++;
                  if (!meta->gridAttrib.f_maxmin) {
                     meta->gridAttrib.min = data[dataInd];
                     meta->gridAttrib.max = data[dataInd];
                     meta->gridAttrib.f_maxmin = 1;
                  } else {
                     if (data[dataInd] < meta->gridAttrib.min) {
                        meta->gridAttrib.min = data[dataInd];
                     }
                     if (data[dataInd] > meta->gridAttrib.max) {
                        meta->gridAttrib.max = data[dataInd];
                     }
                  }
               }
               dataCnt++;
               dataInd = ((dataCnt / meta->gds.Nx) % 2) ?
                     (2 * (dataCnt / meta->gds.Nx) + 1) *
                     meta->gds.Nx - dataCnt - 1 : dataCnt;
               myAssert ((dataInd < numPack) || (dataCnt == numPack));
            }
         }
      } else {
         for (i = 0; i < LX; i++) {
            for (j = 0; j < grp[i].num; j++) {
               memBitRead (&(li_temp), sizeof (sInt4), bds, grp[i].bit,
                           &bufLoc, &numUsed);
               bds += numUsed;
               data[dataInd] = (li_temp + grp[i].min + minVal) * scale;
               if (!meta->gridAttrib.f_maxmin) {
                  meta->gridAttrib.min = data[dataInd];
                  meta->gridAttrib.max = data[dataInd];
                  meta->gridAttrib.f_maxmin = 1;
               } else {
                  if (data[dataInd] < meta->gridAttrib.min) {
                     meta->gridAttrib.min = data[dataInd];
                  }
                  if (data[dataInd] > meta->gridAttrib.max) {
                     meta->gridAttrib.max = data[dataInd];
                  }
               }
               dataCnt++;
               dataInd = ((dataCnt / meta->gds.Nx) % 2) ?
                     (2 * (dataCnt / meta->gds.Nx) + 1) *
                     meta->gds.Nx - dataCnt - 1 : dataCnt;
               myAssert ((dataInd < numPack) || (dataCnt == numPack));
            }
         }
         numVal = dataCnt;
      }
   }
   meta->gridAttrib.numMiss = dataCnt - numVal;
   meta->gridAttrib.refVal = minVal * scale;

   free (grp);
   return 0;
}

/*****************************************************************************
 * ReadTDLPRecord() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Reads in a TDLP message, and parses the data into various data
 * structures, for use with other code.
 *
 * ARGUMENTS
 *           fp = An opened TDLP file already at the correct message. (Input)
 *    TDLP_Data = The read in TDLP data. (Output)
 * tdlp_DataLen = Size of TDLP_Data. (Output)
 *         meta = A filled in meta structure (Output)
 *           IS = The structure containing all the arrays that the
 *                unpacker uses (Output)
 *        sect0 = Already read in section 0 data. (Input)
 *      tdlpLen = Length of the TDLP message. (Input)
 *     majEarth = Used to override the TDLP major axis of earth. (Input)
 *     minEarth = Used to override the TDLP minor axis of earth. (Input)
 *
 * FILES/DATABASES:
 *   An already opened file pointing to the desired TDLP message.
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
 *  10/2004 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
int ReadTDLPRecord (DataSource &fp, double **TDLP_Data, uInt4 *tdlp_DataLen,
                    grib_MetaData *meta, IS_dataType *IS,
                    sInt4 sect0[SECT0LEN_WORD], uInt4 tdlpLen,
                    double majEarth, double minEarth)
{
   sInt4 nd5;           /* Size of TDLP message rounded up to the nearest *
                         * sInt4. */
   uChar *c_ipack;      /* A char ptr to the message stored in IS->ipack */
   sInt4 curLoc;        /* Current location in the GRIB message. */
   char f_gds;          /* flag if there is a GDS section. */
   char f_bms;          /* flag if there is a BMS section. */
   short int DSF;       /* Decimal Scale Factor for unpacking the data. */
   short int BSF;       /* Binary Scale Factor for unpacking the data. */
   double *tdlp_Data;   /* A pointer to TDLP_Data for ease of manipulation. */
   double unitM = 1;    /* M in y = Mx + B, for unit conversion. */
   double unitB = 0;    /* B in y = Mx + B, for unit conversion. */
   sInt4 li_temp;       /* Used to make sure section 5 is 7777. */
   size_t pad;          /* Number of bytes to pad the message to get to the
                         * correct byte boundary. */
   char buffer[24];     /* Read the trailing bytes in the TDLPack record. */
   uChar *bitmap;       /* Would contain bitmap data if it was supported. */

   /* Make room for entire message, and read it in. */
   /* nd5 needs to be tdlpLen in (sInt4) units rounded up. */
   nd5 = (tdlpLen + 3) / 4;
   if (nd5 > IS->ipackLen) {
      IS->ipackLen = nd5;
      IS->ipack = (sInt4 *) realloc ((void *) (IS->ipack),
                                     (IS->ipackLen) * sizeof (sInt4));
   }
   c_ipack = (uChar *) IS->ipack;
   /* Init last sInt4 to 0, to make sure that the padded bytes are 0. */
   IS->ipack[nd5 - 1] = 0;
   /* Init first 2 sInt4 to sect0. */
   memcpy (c_ipack, sect0, SECT0LEN_WORD * 2);
   /* Read in the rest of the message. */
   if (fp.DataSourceFread(c_ipack + SECT0LEN_WORD * 2, sizeof (char),
              (tdlpLen - SECT0LEN_WORD * 2)) + SECT0LEN_WORD * 2 != tdlpLen) {
      errSprintf ("Ran out of file\n");
      return -1;
   }

   /* Preceding was in degrib2, next part is specific to TDLP. */
   curLoc = 8;
   if (ReadTDLPSect1 (c_ipack + curLoc, tdlpLen, &curLoc, &(meta->pdsTdlp),
                      &f_gds, &f_bms, &DSF, &BSF) != 0) {
      preErrSprintf ("Inside ReadGrib1Record\n");
      return -1;
   }

   /* Figure out some basic stuff about the grid. */
   free (meta->element);
   meta->element = NULL;
   free (meta->unitName);
   meta->unitName = NULL;
   free (meta->comment);
   meta->comment = NULL;
   free (meta->shortFstLevel);
   meta->shortFstLevel = NULL;
   free (meta->longFstLevel);
   meta->longFstLevel = NULL;
   TDLP_ElemSurfUnit (&(meta->pdsTdlp), &(meta->element), &(meta->unitName),
                      &(meta->comment), &(meta->shortFstLevel),
                      &(meta->longFstLevel));
   meta->center = 7;    /* US NWS, NCEP */
   meta->subcenter = 14; /* NWS Meteorological Development Laboratory */

/*   strftime (meta->refTime, 20, "%Y%m%d%H%M",
             gmtime (&(meta->pdsTdlp.refTime)));
*/
   Clock_Print (meta->refTime, 20, meta->pdsTdlp.refTime, "%Y%m%d%H%M", 0);

/*
   validTime = meta->pdsTdlp.refTime + meta->pdsTdlp.project;
   strftime (meta->validTime, 20, "%Y%m%d%H%M", gmtime (&(validTime)));
*/
   Clock_Print (meta->validTime, 20, meta->pdsTdlp.refTime +
                meta->pdsTdlp.project, "%Y%m%d%H%M", 0);

   meta->deltTime = meta->pdsTdlp.project;

   /* Get the Grid Definition Section. */
   if (f_gds) {
      if (ReadTDLPSect2 (c_ipack + curLoc, tdlpLen, &curLoc,
                         &(meta->gds)) != 0) {
         preErrSprintf ("Inside ReadGrib1Record\n");
         return -2;
      }
   } else {
      errSprintf ("Don't know how to handle vector data yet.\n");
      return -2;
   }

   /* Allow over ride of the earth radii. */
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

   /* Allocate memory for the grid. */
   if (meta->gds.numPts > *tdlp_DataLen) {
      *tdlp_DataLen = meta->gds.numPts;
      *TDLP_Data = (double *) realloc ((void *) (*TDLP_Data),
                                       (*tdlp_DataLen) * sizeof (double));
   }
   tdlp_Data = *TDLP_Data;

   /* Get the Bit Map Section. */
   if (f_bms) {
/*      errSprintf ("Bitmap data is Not Supported\n");*/
      /* Need to allocate bitmap when this is implemented. */
      bitmap = NULL;
      ReadTDLPSect3 (c_ipack + curLoc, tdlpLen, &curLoc, bitmap,
                     meta->gds.numPts);
      return -1;
   }

   /* Read the GRID. */
   if (ReadTDLPSect4 (c_ipack + curLoc, tdlpLen, &curLoc, DSF, BSF,
                      tdlp_Data, meta, unitM, unitB) != 0) {
      preErrSprintf ("Inside ReadTDLPRecord\n");
      return -4;
   }

   /* Read section 5.  If it is "7777" == 926365495 we are done. */
   memcpy (&li_temp, c_ipack + curLoc, 4);
   if (li_temp != 926365495L) {
      errSprintf ("Did not find the end of the message.\n");
      return -5;
   }
   curLoc += 4;
   /* Read the trailing part of the message. */
   /* TDLPack uses 4 bytes for FORTRAN record size, then another 8 bytes for
    * the size of the record (so FORTRAN can see it), then the data rounded
    * up to an 8 byte boundary, then a trailing 4 bytes for a final FORTRAN
    * record size.  However it only stores in the gribLen the non-rounded
    * amount, so we need to take care of the rounding, and the trailing 4
    * bytes here. */
   pad = ((sInt4) (ceil (tdlpLen / 8.0))) * 8 - tdlpLen + 4;
   if (fp.DataSourceFread(buffer, sizeof (char), pad) != pad) {
      errSprintf ("Ran out of file\n");
      return -1;
   }
   return 0;
}

/*****************************************************************************
 * TDL_ScaleData() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Deal with scaling while excluding the primary and secondary missing
 * values.  After this, dst should contain scaled data + primary or secondary
 * missing values
 *   "tdlpack library"::pack2d.f line 257 or search for:
 "the above statement"
 *
 * ARGUMENTS
 *        Src = The original data. (Input)
 *        Dst = The scaled data. (Output)
 *    numData = The number of elements in data. (Input)
 *        DSF = Decimal Scale Factor for scaling the data. (Input)
 *        BSF = Binary Scale Factor for scaling the data. (Input)
 * f_primMiss = Flag saying if we have a primary missing value (In/Out)
 *   primMiss = primary missing value. (In/Out)
 *  f_secMiss = Flag saying if we have a secondary missing value (In/Out)
 *    secMiss = secondary missing value. (In/Out)
 *      f_min = Flag saying if we have the minimum value. (Output)
 *        min = minimum scaled value in the grid. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
#define SCALE_MISSING 10000
static void TDL_ScaleData (double *Src, sInt4 *Dst, sInt4 numData,
                           int DSF, int BSF, char *f_primMiss,
                           double *primMiss, char *f_secMiss,
                           double *secMiss, char *f_min, sInt4 *min)
{
   sInt4 cnt;
   double *src = Src;
   sInt4 *dst = Dst;
   double scale = pow (10.0, -1 * DSF) * pow (2.0, -1 * BSF);
   char f_actualPrim = 0;
   char f_actualSec = 0;
   sInt4 li_primMiss = (sInt4) (*primMiss * SCALE_MISSING + .5);
   sInt4 li_secMiss = (sInt4) (*secMiss * SCALE_MISSING + .5);

   *f_min = 0;
   for (cnt = 0; cnt < numData; cnt++) {
      if (((*f_primMiss) || (*f_secMiss)) && (*src == *primMiss)) {
         *(dst++) = li_primMiss;
         src++;
         f_actualPrim = 1;
      } else if ((*f_secMiss) && (*src == *secMiss)) {
         *(dst++) = li_secMiss;
         src++;
         f_actualSec = 1;
      } else {
         *(dst) = (sInt4) (floor ((*(src++) / scale) + .5));
         /* Check if scaled value == primary missing value. */
         if (((*f_primMiss) || (*f_secMiss)) && (*dst == li_primMiss)) {
            *dst = *dst - 1;
         }
         /* Check if scaled value == secondary missing value. */
         if ((*f_secMiss) && (*dst == li_secMiss)) {
            *dst = *dst - 1;
            /* Check if adjustment caused scaled value == primary missing. */
            if (*dst == li_primMiss) {
               *dst = *dst - 1;
            }
         }
         if (!(*f_min)) {
            *min = *dst;
            *f_min = 1;
         } else if (*min > *dst) {
            *min = *dst;
         }
         dst++;
      }
   }
   if ((*f_secMiss) && (!f_actualSec)) {
      *f_secMiss = 0;
   }
   if (((*f_secMiss) || (*f_primMiss)) && (!f_actualPrim)) {
      *f_primMiss = 0;
      /* Check consistency. */
      if (*f_secMiss) {
         *f_secMiss = 0;
         *f_primMiss = 1;
         *primMiss = *secMiss;
      }
   }
}

/*****************************************************************************
 * TDL_ReorderGrid() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Loop through the data, so that
 * data is:    "a1,1 ... a1,n  a2,n ... a2,1 ..."
 * instead of: "a1,1 ... a1,n  a2,1 ... a2,n ..."
 *
 * ARGUMENTS
 * Src = The data. (Input/Output)
 *  NX = The number of X values. (Input)
 *  NY = The number of Y values. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static void TDL_ReorderGrid (sInt4 *Src, short int NX, short int NY)
{
   int i, j;
   sInt4 *src1, *src2;
   sInt4 li_temp;

   for (j = 1; j < NY; j += 2) {
      src1 = Src + j * NX;
      src2 = Src + (j + 1) * NX - 1;
      for (i = 0; i < (NX / 2); i++) {
         li_temp = *src1;
         *(src1++) = *src2;
         *(src2--) = li_temp;
      }
   }
}

/*****************************************************************************
 * TDL_GetSecDiff() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Get the second order difference where we have special values for missing,
 * and for actual data we have the following scheme.
 *       Data:      a1  a2 a3 a4 a5 ...
 *       1st diff:   0  b2 b3 b4 b5 ...
 *       2nd diff: UK1 UK2 c3 c4 c5 ...
 * where UK1 = c3 + b2, and UK2 = c3 + 2 * b2.  Note: The choice of UK1, and
 * UK2 doesn't matter because of the following FORTRAN unpacking code:
 *       IWORK(1)=IFIRST
 *       IWORK(2)=IWORK(1)+IFOD
 *       ISUM=IFOD
 *       DO 385 K=3,IS4(3)
 *         ISUM=IWORK(K)+ISUM
 *         IWORK(K)=IWORK(K-1)+ISUM
 * 385   CONTINUE
 * So ISUM is a function of IWORK(3), not IWORK(1).
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *     SecDiff = The secondary differences of the data. (Output)
 *  f_primMiss = Flag saying if we have a primary missing value (Input)
 * li_primMiss = Scaled primary missing value. (Input)
 *          a1 = First non-missing value in the field. (Output)
 *          b2 = First non-missing value in the 1st order delta field (Out)
 *         min = The minimum value (Input).
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Success
 *  1 = Couldn't find second differences (don't use).
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static int TDL_GetSecDiff (sInt4 *Data, int numData, sInt4 *SecDiff,
                           char f_primMiss, sInt4 li_primMiss,
                           sInt4 *a1, sInt4 *b2, sInt4 *min)
{
   int i;
   char f_min = 0;
   sInt4 last = 0, before_last = 0;
   int a1Index = -1;
   int a2Index = -1;

   if (numData < 3) {
      return 1;
   }
   if (f_primMiss) {
      for (i = 0; i < numData; i++) {
         if (Data[i] == li_primMiss) {
            SecDiff[i] = li_primMiss;
         } else if (a1Index == -1) {
            a1Index = i;
            *a1 = Data[a1Index];
         } else if (a2Index == -1) {
            a2Index = i;
            *b2 = Data[a2Index] - Data[a1Index];
            before_last = Data[a1Index];
            last = Data[a2Index];
         } else {
            SecDiff[i] = Data[i] - 2 * last + before_last;
            before_last = last;
            last = Data[i];
            if (!f_min) {
               /* Set the UK1, UK2 values. */
               *min = SecDiff[i];
               f_min = 1;
               SecDiff[a1Index] = SecDiff[i] + *b2;
               SecDiff[a2Index] = SecDiff[i] + 2 * (*b2);
            } else if (*min > SecDiff[i]) {
               *min = SecDiff[i];
            }
         }
      }
      if (!f_min) {
         return 1;
      }
   } else {
      *a1 = Data[0];
      *b2 = Data[1] - Data[0];
      for (i = 3; i < numData; i++) {
         SecDiff[i] = Data[i] - 2 * Data[i - 1] - Data[i - 2];
         if (i == 3) {
            *min = SecDiff[i];
            /* Set the UK1, UK2 values. */
            SecDiff[0] = SecDiff[i] + *b2;
            SecDiff[1] = SecDiff[i] + 2 * (*b2);
         } else if (*min > SecDiff[i]) {
            *min = SecDiff[i];
         }
      }
   }
   return 0;
}

/*****************************************************************************
 * TDL_UseSecDiff_Prim() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Checks if the average range of 2nd order differences < average range of
 *   0 order differences, to determine if we should use second order
 *   differences. This deals with the case when we have primary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *     SecDiff = The secondary differences of the data. (Input)
 * li_primMiss = Scaled primary missing value. (Input)
 *    minGroup = The minimum group size. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Don't use 2nd order differences.
 *  1 = Use 2nd order differences.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static int TDL_UseSecDiff_Prim (sInt4 *Data, sInt4 numData,
                                sInt4 *SecDiff, sInt4 li_primMiss,
                                int minGroup)
{
   int i, locCnt;
   int range0, range2;
   int tot0, tot2;
   char f_min;
   sInt4 min = 0, max = 0;

   locCnt = 0;
   range0 = 0;
   tot0 = 0;
   f_min = 0;
   /* Compute scores for no differences */
   for (i = 0; i < numData; i++) {
      if (Data[i] != li_primMiss) {
         if (!f_min) {
            min = Data[i];
            max = Data[i];
            f_min = 1;
         } else {
            if (min > Data[i])
               min = Data[i];
            if (max < Data[i])
               max = Data[i];
         }
      }
      locCnt++;
      /* Fake a "group" by using the minimum group size. */
      if (locCnt == minGroup) {
         if (f_min) {
            range0 += (max - min);
            tot0++;
            f_min = 0;
         }
         locCnt = 0;
      }
   }
   if (locCnt != 0) {
      range0 += (max - min);
      tot0++;
   }

   locCnt = 0;
   range2 = 0;
   tot2 = 0;
   f_min = 0;
   /* Compute scores for second order differences */
   for (i = 0; i < numData; i++) {
      if (SecDiff[i] != li_primMiss) {
         if (!f_min) {
            min = SecDiff[i];
            max = SecDiff[i];
            f_min = 1;
         } else {
            if (min > SecDiff[i])
               min = SecDiff[i];
            if (max < SecDiff[i])
               max = SecDiff[i];
         }
      }
      locCnt++;
      /* Fake a "group" by using the minimum group size. */
      if (locCnt == minGroup) {
         if (f_min) {
            range2 += (max - min);
            tot2++;
            f_min = 0;
         }
         locCnt = 0;
      }
   }
   if (locCnt != 0) {
      range2 += (max - min);
      tot2++;
   }

   /* Compare average group size of no differencing to second order. */
   if ((range0 / (tot0 + 0.0)) <= (range2 / (tot2 + 0.0))) {
      return 0;
   } else {
      return 1;
   }
}

/*****************************************************************************
 * TDL_UseSecDiff() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Checks if the average range of 2nd order differences < average range of
 *   0 order differences, to determine if we should use second order
 *   differences.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *     SecDiff = The secondary differences of the data. (Input)
 *    minGroup = The minimum group size. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Don't use 2nd order differences.
 *  1 = Use 2nd order differences.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static int TDL_UseSecDiff (sInt4 *Data, sInt4 numData,
                           sInt4 *SecDiff, int minGroup)
{
   int i, locCnt;
   int range0, range2;
   int tot0, tot2;
   sInt4 min = 0, max = 0;

   locCnt = 0;
   range0 = 0;
   tot0 = 0;
   /* Compute scores for no differences */
   for (i = 0; i < numData; i++) {
      if (locCnt == 0) {
         min = Data[i];
         max = Data[i];
      } else {
         if (min > Data[i])
            min = Data[i];
         if (max < Data[i])
            max = Data[i];
      }
      locCnt++;
      /* Fake a "group" by using the minimum group size. */
      if (locCnt == minGroup) {
         range0 += (max - min);
         tot0++;
         locCnt = 0;
      }
   }
   if (locCnt != 0) {
      range0 += (max - min);
      tot0++;
   }

   locCnt = 0;
   range2 = 0;
   tot2 = 0;
   /* Compute scores for second order differences */
   for (i = 0; i < numData; i++) {
      if (locCnt == 0) {
         min = SecDiff[i];
         max = SecDiff[i];
      } else {
         if (min > SecDiff[i])
            min = SecDiff[i];
         if (max < SecDiff[i])
            max = SecDiff[i];
      }
      locCnt++;
      /* Fake a "group" by using the minimum group size. */
      if (locCnt == minGroup) {
         range2 += (max - min);
         tot2++;
         locCnt = 0;
      }
   }
   if (locCnt != 0) {
      range2 += (max - min);
      tot2++;
   }

   /* Compare average group size of no differencing to second order. */
   if ((range0 / (tot0 + 0.0)) <= (range2 / (tot2 + 0.0))) {
      return 0;
   } else {
      return 1;
   }
}

/*****************************************************************************
 * power() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Calculate the number of bits required to store a given positive number.
 *
 * ARGUMENTS
 *   val = The number to store (Input)
 * extra = number of slots to allocate for prim/sec missing values (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *   The number of bits needed to store this number.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static int power (uInt4 val, int extra)
{
   int i;

   val += extra;
   if (val == 0) {
      return 1;
   }
   for (i = 0; val != 0; i++) {
      val = val >> 1;
   }
   return i;
}

/*****************************************************************************
 * findMaxMin2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find the min/max value between start/stop index values in the data
 * Assuming primary and secondary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 * li_primMiss = scaled primary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *         min = The min value found (Output)
 *         max = The max value found (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: char
 *   Flag if min/max are valid.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static char findMaxMin2 (sInt4 *Data, int start, int stop,
                         sInt4 li_primMiss, sInt4 li_secMiss,
                         sInt4 *min, sInt4 *max)
{
   char f_min = 0;      /* Flag if we found the max/min values */
   int i;               /* Loop counter. */

   *max = *min = Data[start];
   for (i = start; i < stop; i++) {
      if ((Data[i] != li_secMiss) && (Data[i] != li_primMiss)) {
         if (!f_min) {
            *max = Data[i];
            *min = Data[i];
            f_min = 1;
         } else {
            if (*max < Data[i]) {
               *max = Data[i];
            } else if (*min > Data[i]) {
               *min = Data[i];
            }
         }
      }
   }
   return f_min;
}

/*****************************************************************************
 * findMaxMin1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find the min/max value between start/stop index values in the data
 * Assuming primary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 * li_primMiss = scaled primary missing value (Input)
 *         min = The min value found (Output)
 *         max = The max value found (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: char
 *   Flag if min/max are valid.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static char findMaxMin1 (sInt4 *Data, int start, int stop,
                         sInt4 li_primMiss, sInt4 *min, sInt4 *max)
{
   char f_min = 0;      /* Flag if we found the max/min values */
   int i;               /* Loop counter. */

   *max = *min = Data[start];
   for (i = start; i < stop; i++) {
      if (Data[i] != li_primMiss) {
         if (!f_min) {
            *max = Data[i];
            *min = Data[i];
            f_min = 1;
         } else {
            if (*max < Data[i]) {
               *max = Data[i];
            } else if (*min > Data[i]) {
               *min = Data[i];
            }
         }
      }
   }
   return f_min;
}

/*****************************************************************************
 * findMaxMin0() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find the min/max value between start/stop index values in the data
 * Assuming no missing values.
 *
 * ARGUMENTS
 *  Data = The data. (Input)
 * start = The starting index in data (Input)
 *  stop = The stopping index in data (Input)
 *   min = The min value found (Output)
 *   max = The max value found (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *
 * NOTES
 *****************************************************************************
 */
static void findMaxMin0 (sInt4 *Data, int start, int stop, sInt4 *min,
                         sInt4 *max)
{
   int i;               /* Loop counter. */

   *max = *min = Data[start];
   for (i = start + 1; i < stop; i++) {
      if (*max < Data[i]) {
         *max = Data[i];
      } else if (*min > Data[i]) {
         *min = Data[i];
      }
   }
}

/*****************************************************************************
 * findGroup2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between start and split are within
 * "range" of each other... stops if it reaches "stop".
 *   Assumes primary and secondary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 * li_primMiss = scaled primary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *       range = The range to use (Input)
 *       split = The first index that is out of the range (Output)
 *         min = The min value for the group. (Output)
 *         max = The max value for the group. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void findGroup2 (sInt4 *Data, int start, int stop,
                        sInt4 li_primMiss, sInt4 li_secMiss,
                        sInt4 range, int *split, sInt4 *min, sInt4 *max)
{
   char f_min = 0;      /* Flag if we found the max/min values */
   int i;               /* Loop counter. */

   *min = *max = 0;
   for (i = start; i < stop; i++) {
      if ((Data[i] != li_secMiss) && (Data[i] != li_primMiss)) {
         if (!f_min) {
            *max = *min = Data[i];
            f_min = 1;
         } else {
            if (*max < Data[i]) {
               if ((Data[i] - *min) > range) {
                  *split = i;
                  return;
               }
               *max = Data[i];
            } else if (*min > Data[i]) {
               if ((*max - Data[i]) > range) {
                  *split = i;
                  return;
               }
               *min = Data[i];
            }
         }
      }
   }
   *split = stop;
}

/*****************************************************************************
 * findGroup1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between start and split are within
 * "range" of each other... stops if it reaches "stop".
 *   Assumes primary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 * li_primMiss = scaled primary missing value (Input)
 *       range = The range to use (Input)
 *       split = The first index that is out of the range (Output)
 *         min = The min value for the group. (Output)
 *         max = The max value for the group. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void findGroup1 (sInt4 *Data, int start, int stop,
                        sInt4 li_primMiss, sInt4 range, int *split,
                        sInt4 *min, sInt4 *max)
{
   char f_min = 0;      /* Flag if we found the max/min values */
   int i;               /* Loop counter. */

   *min = *max = 0;
   for (i = start; i < stop; i++) {
      if (Data[i] != li_primMiss) {
         if (!f_min) {
            *max = *min = Data[i];
            f_min = 1;
         } else {
            if (*max < Data[i]) {
               if ((Data[i] - *min) > range) {
                  *split = i;
                  return;
               }
               *max = Data[i];
            } else if (*min > Data[i]) {
               if ((*max - Data[i]) > range) {
                  *split = i;
                  return;
               }
               *min = Data[i];
            }
         }
      }
   }
   *split = stop;
}

/*****************************************************************************
 * findGroup0() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between start and split are within
 * "range" of each other... stops if it reaches "stop".
 *   Assumes no missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 *       range = The range to use (Input)
 *       split = The first index that is out of the range (Output)
 *         min = The min value for the group. (Output)
 *         max = The max value for the group. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void findGroup0 (sInt4 *Data, int start, int stop,
                        sInt4 range, int *split, sInt4 *min, sInt4 *max)
{
   int i;               /* Loop counter. */

   *max = *min = Data[0];
   for (i = start + 1; i < stop; i++) {
      if (*max < Data[i]) {
         if ((Data[i] - *min) > range) {
            *split = i;
            return;
         }
         *max = Data[i];
      } else if (*min > Data[i]) {
         if ((*max - Data[i]) > range) {
            *split = i;
            return;
         }
         *min = Data[i];
      }
   }
   *split = stop;
}

/*****************************************************************************
 * findGroupRev2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between split and stop are within
 * "range" of each other... stops if it reaches "start".
 *   Assumes primary and secondary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 * li_primMiss = scaled primary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *       range = The range to use (Input)
 *       split = The first index that is still in the range (Output)
 *         min = The min value for the group. (Output)
 *         max = The max value for the group. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void findGroupRev2 (sInt4 *Data, int start, int stop,
                           sInt4 li_primMiss, sInt4 li_secMiss,
                           sInt4 range, int *split, sInt4 *min, sInt4 *max)
{
   char f_min = 0;      /* Flag if we found the max/min values */
   int i;               /* Loop counter. */

   *min = *max = 0;
   for (i = stop - 1; i >= start; i--) {
      if ((Data[i] != li_secMiss) && (Data[i] != li_primMiss)) {
         if (!f_min) {
            *max = *min = Data[i];
            f_min = 1;
         } else {
            if (*max < Data[i]) {
               if ((Data[i] - *min) > range) {
                  *split = i + 1;
                  return;
               }
               *max = Data[i];
            } else if (*min > Data[i]) {
               if ((*max - Data[i]) > range) {
                  *split = i + 1;
                  return;
               }
               *min = Data[i];
            }
         }
      }
   }
   *split = start;
}

/*****************************************************************************
 * findGroupRev1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between split and stop are within
 * "range" of each other... stops if it reaches "start".
 *   Assumes primary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 * li_primMiss = scaled primary missing value (Input)
 *       range = The range to use (Input)
 *       split = The first index that is still in the range (Output)
 *         min = The min value for the group. (Output)
 *         max = The max value for the group. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void findGroupRev1 (sInt4 *Data, int start, int stop,
                           sInt4 li_primMiss, sInt4 range, int *split,
                           sInt4 *min, sInt4 *max)
{
   char f_min = 0;      /* Flag if we found the max/min values */
   int i;               /* Loop counter. */

   *min = *max = 0;
   for (i = stop - 1; i >= start; i--) {
      if (Data[i] != li_primMiss) {
         if (!f_min) {
            *max = *min = Data[i];
            f_min = 1;
         } else {
            if (*max < Data[i]) {
               if ((Data[i] - *min) > range) {
                  *split = i + 1;
                  return;
               }
               *max = Data[i];
            } else if (*min > Data[i]) {
               if ((*max - Data[i]) > range) {
                  *split = i + 1;
                  return;
               }
               *min = Data[i];
            }
         }
      }
   }
   *split = start;
}

/*****************************************************************************
 * findGroupRev0() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between split and stop are within
 * "range" of each other... stops if it reaches "start".
 *   Assumes no missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *       start = The starting index in data (Input)
 *        stop = The stopping index in data (Input)
 * li_primMiss = scaled primary missing value (Input)
 *       range = The range to use (Input)
 *       split = The first index that is still in the range (Output)
 *         min = The min value for the group. (Output)
 *         max = The max value for the group. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void findGroupRev0 (sInt4 *Data, int start, int stop,
                           sInt4 range, int *split, sInt4 *min, sInt4 *max)
{
   int i;               /* Loop counter. */

   *max = *min = Data[stop - 1];
   for (i = stop - 2; i >= start; i--) {
      if (*max < Data[i]) {
         if ((Data[i] - *min) > range) {
            *split = i + 1;
            return;
         }
         *max = Data[i];
      } else if (*min > Data[i]) {
         if ((*max - Data[i]) > range) {
            *split = i + 1;
            return;
         }
         *min = Data[i];
      }
   }
   *split = start;
}

/*****************************************************************************
 * shiftGroup2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between split and start1 are all inside
 * the range defined by max, min and bit.  It allows max and min to change,
 * as long as it doesn't exceed the range defined by bit.
 *   This is very similar to findGroupRev?() but here we already know
 * information about the min/max values, and are just expanding the group a
 * little, while the other one knew nothing about the group, and just wanted
 * a group of a given range.
 *   Assumes primary and secondary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *      start1 = The starting index in data (Input)
 *      start2 = The starting index of the earlier group (i.e. don't go to any
 *               earlier indices than this. (Input)
 * li_primMiss = scaled primary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *         bit = The range we are allowed to store this in. (Input)
 *         min = The min value for the group. (Input/Output)
 *         max = The max value for the group. (Input/Output)
 *       split = The first index that is still in the range (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void shiftGroup2 (sInt4 *Data, int start1, int start2,
                         sInt4 li_primMiss, sInt4 li_secMiss, int bit,
                         sInt4 *min, sInt4 *max, size_t *split)
{
   int i;               /* Loop counter. */
   int range;           /* The range defined by bit. */

   range = (int) (pow (2.0, bit) - 1) - 1;
   myAssert (start2 <= start1);
   for (i = start1; i >= start2; i--) {
      if ((Data[i] != li_primMiss) && (Data[i] != li_secMiss)) {
         if (Data[i] > *max) {
            if ((Data[i] - *min) <= range) {
               *max = Data[i];
            } else {
               *split = i + 1;
               return;
            }
         } else if (Data[i] < *min) {
            if ((*max - Data[i]) <= range) {
               *min = Data[i];
            } else {
               *split = i + 1;
               return;
            }
         }
      }
   }
   *split = start2;
}

/*****************************************************************************
 * shiftGroup1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between split and start1 are all inside
 * the range defined by max, min and bit.  It allows max and min to change,
 * as long as it doesn't exceed the range defined by bit.
 *   This is very similar to findGroupRev?() but here we already know
 * information about the min/max values, and are just expanding the group a
 * little, while the other one knew nothing about the group, and just wanted
 * a group of a given range.
 *   Assumes primary missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *      start1 = The starting index in data (Input)
 *      start2 = The starting index of the earlier group (i.e. don't go to any
 *               earlier indices than this. (Input)
 * li_primMiss = scaled primary missing value (Input)
 *         bit = The range we are allowed to store this in. (Input)
 *         min = The min value for the group. (Input/Output)
 *         max = The max value for the group. (Input/Output)
 *       split = The first index that is still in the range (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void shiftGroup1 (sInt4 *Data, int start1, int start2,
                         sInt4 li_primMiss, int bit,
                         sInt4 *min, sInt4 *max, size_t *split)
{
   int i;               /* Loop counter. */
   int range;           /* The range defined by bit. */

   range = (int) (pow (2.0, bit) - 1) - 1;
   myAssert (start2 <= start1);
   for (i = start1; i >= start2; i--) {
      if (Data[i] != li_primMiss) {
         if (Data[i] > *max) {
            if ((Data[i] - *min) <= range) {
               *max = Data[i];
            } else {
               *split = i + 1;
               return;
            }
         } else if (Data[i] < *min) {
            if ((*max - Data[i]) <= range) {
               *min = Data[i];
            } else {
               *split = i + 1;
               return;
            }
         }
      }
   }
   *split = start2;
}

/*****************************************************************************
 * shiftGroup0() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Find "split" so that the numbers between split and start1 are all inside
 * the range defined by max, min and bit.  It allows max and min to change,
 * as long as it doesn't exceed the range defined by bit.
 *   This is very similar to findGroupRev?() but here we already know
 * information about the min/max values, and are just expanding the group a
 * little, while the other one knew nothing about the group, and just wanted
 * a group of a given range.
 *   Assumes no missing values.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *      start1 = The starting index in data (Input)
 *      start2 = The starting index of the earlier group (i.e. don't go to any
 *               earlier indices than this. (Input)
 *         bit = The range we are allowed to store this in. (Input)
 *         min = The min value for the group. (Input/Output)
 *         max = The max value for the group. (Input/Output)
 *       split = The first index that is still in the range (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2005 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
static void shiftGroup0 (sInt4 *Data, int start1, int start2, int bit,
                         sInt4 *min, sInt4 *max, size_t *split)
{
   int i;               /* Loop counter. */
   int range;           /* The range defined by bit. */

   range = (int) (pow (2.0, bit) - 1) - 0;
   myAssert (start2 <= start1);
   for (i = start1; i >= start2; i--) {
      if (Data[i] > *max) {
         if ((Data[i] - *min) <= range) {
            *max = Data[i];
         } else {
            *split = i + 1;
            return;
         }
      } else if (Data[i] < *min) {
         if ((*max - Data[i]) <= range) {
            *min = Data[i];
         } else {
            *split = i + 1;
            return;
         }
      }
   }
   *split = start2;
}

/*****************************************************************************
 * doSplit() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Reduce the "bit range", and create groups that grab as much as they can
 * to the right.  Then reduce those groups if they improve the score.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *           G = The group to split. (Input)
 *    lclGroup = The resulting groups (Output)
 * numLclGroup = The number of resulting groups. (Output)
 *  f_primMiss = Flag if we have a primary missing value (Input)
 * li_primMiss = scaled primary missing value (Input)
 *   f_secMiss = Flag if we have a secondary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *     xFactor = Estimate of cost (in bits) of a group. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  1/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static void doSplit (sInt4 *Data,
                     CPL_UNUSED int numData,
                     TDLGroupType * G,
                     TDLGroupType ** lclGroup, int *numLclGroup,
                     char f_primMiss, sInt4 li_primMiss,
                     char f_secMiss, sInt4 li_secMiss, int xFactor)
{
   int start;           /* Where to start the current group. */
   int range;           /* The range to make the groups. */
   int final;           /* One more than the last index in the group G. */
   int split;           /* Where to split the group. */
   TDLGroupType G1;     /* The current group to add. */
   TDLGroupType G2;     /* The group if we evaporated the previous group. */
   int evaporate;       /* How many groups we have "evaporated". */
   int i;               /* Loop counter to help "evaporate" groups. */
   sInt4 scoreA;        /* The original score for 2 groups */
   sInt4 scoreB;        /* The new score (having evaporated a group) */
   int GroupLen;        /* Actual alloc'ed group len. */

   /* *INDENT-OFF* */
   /* The (pow (2, ..) -1) is because 2^n - 1 is max range of a group.
    * Example n = 1, 2^1 -1 = 1 range of (1,0) is 1.
    * Example n = 2, 2^2 -1 = 3 range of (3,2,1,0) is 3.
    * The G.bit - 1 is because we are trying to reduce the range. */
   /* *INDENT-ON* */
   range = (int) (pow (2.0, G->bit - 1) - 1) - (f_secMiss + f_primMiss);
   split = G->start;
   start = G->start;
   final = G->start + G->num;
   myAssert (final <= numData);
   *numLclGroup = 0;
   GroupLen = 1;
   *lclGroup = (TDLGroupType *) malloc (GroupLen * sizeof (TDLGroupType));
   while (split < final) {
      if (f_secMiss) {
         findGroup2 (Data, start, final, li_primMiss, li_secMiss, range,
                     &split, &(G1.min), &(G1.max));
      } else if (f_primMiss) {
         findGroup1 (Data, start, final, li_primMiss, range, &split,
                     &(G1.min), &(G1.max));
      } else {
         findGroup0 (Data, start, final, range, &split, &(G1.min), &(G1.max));
      }
      G1.bit = (char) power ((uInt4) (G1.max - G1.min),
                             f_secMiss + f_primMiss);
      G1.num = split - start;
      G1.start = start;
      G1.f_trySplit = 1;
      G1.f_tryShift = 1;
      /* Test if we should add to previous group, or create a new group. */
      if (*numLclGroup == 0) {
         *numLclGroup = 1;
         (*lclGroup)[0] = G1;
      } else {
         G2.start = (*lclGroup)[*numLclGroup - 1].start;
         G2.num = (*lclGroup)[*numLclGroup - 1].num + G1.num;
         G2.min = ((*lclGroup)[*numLclGroup - 1].min < G1.min) ?
               (*lclGroup)[*numLclGroup - 1].min : G1.min;
         G2.max = ((*lclGroup)[*numLclGroup - 1].max > G1.max) ?
               (*lclGroup)[*numLclGroup - 1].max : G1.max;
         G2.bit = (char) power ((uInt4) (G2.max - G2.min),
                                f_secMiss + f_primMiss);
         G2.f_trySplit = 1;
         G2.f_tryShift = 1;
         scoreA = ((*lclGroup)[*numLclGroup - 1].bit *
                   (*lclGroup)[*numLclGroup - 1].num) + xFactor;
         scoreA += G1.bit * G1.num + xFactor;
         scoreB = G2.bit * G2.num + xFactor;
         if (scoreB < scoreA) {
            (*lclGroup)[*numLclGroup - 1] = G2;
            /* See if we can evaporate any of the old groups */
            evaporate = 0;
            for (i = *numLclGroup - 1; i > 0; i--) {
               G1.start = (*lclGroup)[i - 1].start;
               G1.num = (*lclGroup)[i].num + (*lclGroup)[i - 1].num;
               G1.min = ((*lclGroup)[i].min < (*lclGroup)[i - 1].min) ?
                     (*lclGroup)[i].min : (*lclGroup)[i - 1].min;
               G1.max = ((*lclGroup)[i].max > (*lclGroup)[i - 1].max) ?
                     (*lclGroup)[i].max : (*lclGroup)[i - 1].max;
               G1.bit = (char) power ((uInt4) (G1.max - G1.min),
                                      f_secMiss + f_primMiss);
               G1.f_trySplit = 1;
               G1.f_tryShift = 1;
               scoreA = (*lclGroup)[i].bit * (*lclGroup)[i].num + xFactor;
               scoreA += ((*lclGroup)[i - 1].bit * (*lclGroup)[i - 1].num +
                          xFactor);
               scoreB = G1.bit * G1.num + xFactor;
               if (scoreB < scoreA) {
                  evaporate++;
                  (*lclGroup)[i - 1] = G1;
               } else {
                  break;
               }
            }
            if (evaporate != 0) {
               *numLclGroup = *numLclGroup - evaporate;
            }
         } else {
            *numLclGroup = *numLclGroup + 1;
            if (*numLclGroup > GroupLen) {
               GroupLen = *numLclGroup;
               *lclGroup = (TDLGroupType *) realloc ((void *) *lclGroup,
                                                     GroupLen *
                                                     sizeof (TDLGroupType));
            }
            (*lclGroup)[*numLclGroup - 1] = G1;
         }
      }
      start = split;
   }
}

/*****************************************************************************
 * doSplitRight() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Break into two groups right has range n - 1, left has range n.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *           G = The group to split. (Input)
 *          G1 = The right most group (Output)
 *          G2 = The remainder. (Output)
 *  f_primMiss = Flag if we have a primary missing value (Input)
 * li_primMiss = scaled primary missing value (Input)
 *   f_secMiss = Flag if we have a secondary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  1/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static void doSplitRight (sInt4 *Data,
                          CPL_UNUSED int numData,
                          TDLGroupType * G,
                          TDLGroupType * G1, TDLGroupType * G2,
                          char f_primMiss, sInt4 li_primMiss,
                          char f_secMiss, sInt4 li_secMiss)
{
   int range;           /* The range to make the right most group. */
   int final;           /* One more than the last index in the group. */
   int split;           /* Where to split the group. */

   /* *INDENT-OFF* */
   /* The (pow (2, ..) -1) is because 2^n - 1 is max range of a group.
    * Example n = 1, 2^1 -1 = 1 range of (1,0) is 1.
    * Example n = 2, 2^2 -1 = 3 range of (3,2,1,0) is 3.
    * The G.bit - 1 is because we are trying to reduce the range. */
   /* *INDENT-ON* */
   range = (int) (pow (2.0, G->bit - 1) - 1) - (f_secMiss + f_primMiss);
   final = G->start + G->num;
   split = final;
   myAssert (final <= numData);

   if (f_secMiss) {
      findGroupRev2 (Data, G->start, final, li_primMiss, li_secMiss, range,
                     &split, &(G1->min), &(G1->max));
      findMaxMin2 (Data, G->start, split, li_primMiss, li_secMiss,
                   &(G2->min), &(G2->max));
   } else if (f_primMiss) {
      findGroupRev1 (Data, G->start, final, li_primMiss, range, &split,
                     &(G1->min), &(G1->max));
      findMaxMin1 (Data, G->start, split, li_primMiss, &(G2->min),
                   &(G2->max));
   } else {
      findGroupRev0 (Data, G->start, final, range, &split, &(G1->min),
                     &(G1->max));
      findMaxMin0 (Data, G->start, split, &(G2->min), &(G2->max));
   }

   G1->bit = (char) power ((uInt4) (G1->max - G1->min),
                           f_secMiss + f_primMiss);
   G2->bit = (char) power ((uInt4) (G2->max - G2->min),
                           f_secMiss + f_primMiss);
   G1->start = split;
   G2->start = G->start;
   G1->num = final - split;
   G2->num = split - G->start;
   G1->f_trySplit = 1;
   G1->f_tryShift = 1;
   G2->f_trySplit = 1;
   G2->f_tryShift = 1;
}

/*****************************************************************************
 * ComputeGroupSize() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Compute the number of bits needed for the various elements of the groups
 * as well as the total number of bits needed.
 *
 * ARGUMENTS
 *    group = Groups (Input)
 * numGroup = Number of groups (Input)
 *     ibit = Number of bits needed for the minimum values of each group.
 *            Find max absolute value of group mins.  Note: all group mins
 *            are positive (Output)
 *     jbit = Number of bits needed for the number of bits for each group.
 *            Find max absolute value of number of bits. (Output)
 *     kbit = Number of bits needed for the number of values for each group.
 *            Find max absolute value of number of values. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: sInt4
 *   number of bits needed by the groups
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "tdlpack.c" in "C" tdlpack code
 *
 * NOTES
 *****************************************************************************
 */
static sInt4 ComputeGroupSize (TDLGroupType * group, int numGroup,
                               size_t *ibit, size_t *jbit, size_t *kbit)
{
   int i;               /* loop counter. */
   sInt4 ans = 0;       /* The number of bits needed. */
   sInt4 maxMin = 0;    /* The largest min value in the groups */
   uChar maxBit = 0;    /* The largest needed bits in the groups */
   uInt4 maxNum = 0;    /* The largest number of values in the groups. */

   for (i = 0; i < numGroup; i++) {
      ans += group[i].bit * group[i].num;
      if (group[i].min > maxMin) {
         maxMin = group[i].min;
      }
      if (group[i].bit > maxBit) {
         maxBit = group[i].bit;
      }
      if (group[i].num > maxNum) {
         maxNum = group[i].num;
      }
   }
   /* This only works for pos numbers... */
   for (i = 0; (maxMin != 0); i++) {
      maxMin = maxMin >> 1;
   }
   /* Allow 0 bits for min.  Assumes that decoder allows 0 bits */
   *ibit = i;
   /* This only works for pos numbers... */
   for (i = 0; (maxBit != 0); i++) {
      maxBit = maxBit >> 1;
   }
   /* Allow 0 bits for min.  Assumes that decoder allows 0 bits */
   *jbit = i;
   /* This only works for pos numbers... */
   for (i = 0; (maxNum != 0); i++) {
      maxNum = maxNum >> 1;
   }
   /* Allow 0 bits for min.  Assumes that decoder allows 0 bits */
   *kbit = i;
   ans += (sInt4) (((*ibit) + (*jbit) + (*kbit)) * numGroup);
   return ans;
}

/*****************************************************************************
 * splitGroup() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Tries to reduce (split) each group by 1 bit.  It does this by:
 *   A) reduce the "bit range", and create groups that grab as much as they
 * can to the right.  Then reduce those groups if they improve the score.
 *   B) reduce the bit range and grab the left most group only, leaving the
 * rest unchanged.
 *   C) reduce the bit range and grab the right most group only, leaving the
 * rest unchanged.
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *       group = The groups using the "best minGroup. (Input)
 *    numGroup = Number of groups (Input)
 *    lclGroup = The local copy of the groups (Output)
 * numLclGroup = Number of local groups (Output)
 *  f_primMiss = Flag if we have a primary missing value (Input)
 * li_primMiss = scaled primary missing value (Input)
 *   f_secMiss = Flag if we have a secondary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *     xFactor = Estimate of cost (in bits) of a group. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  1/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int splitGroup (sInt4 *Data, int numData, TDLGroupType * group,
                       int numGroup, TDLGroupType ** lclGroup,
                       int *numLclGroup, char f_primMiss,
                       sInt4 li_primMiss, char f_secMiss,
                       sInt4 li_secMiss, size_t xFactor)
{
   uInt4 minBit;        /* The fewest # of bits, with no subdivision. */
   TDLGroupType *subGroup; /* The subgroups that we tried splitting the
                            * primary group into. */
   int numSubGroup;     /* The number of groups in subGroup. */
   sInt4 A_max;         /* Max value of a given group. */
   sInt4 A_min;         /* Min value of a given group. */
   sInt4 scoreA;        /* The original score for a given group */
   sInt4 scoreB;        /* The new score */
   int f_adjust = 0;    /* Flag if group has changed. */
   int f_keep;          /* Flag to keep the subgroup instead of original */
   int i;               /* Loop counters */
   int sub;             /* loop counter over the sub group. */
   int lclIndex;        /* Used to help copy data from subGroup to answer. */
   int GroupLen;        /* Actual alloc'ed group len. */
   int extra;           /* Used to reduce number of allocs. */

   /* Figure out how few bits a group can have without being able to further
    * divide it. */
   if (f_secMiss) {
      /* 11 = primMiss 10 = secMiss 01, 00 = data. */
      minBit = 2;
   } else if (f_primMiss) {
      /* 1 = primMiss 0 = data. */
      /* might try minBit = 1 here. */
      minBit = 1;
   } else {
      /* 1, 0 = data. */
      minBit = 1;
   }

   *numLclGroup = 0;
   *lclGroup = (TDLGroupType *) malloc (numGroup * sizeof (TDLGroupType));
   GroupLen = numGroup;
   extra = 0;
   for (i = 0; i < numGroup; i++) {
      /* Check if we have already tried to split this group, or it has too
       * few members, or it doesn't have enough bits to split.  If so, skip
       * this group. */
      if ((group[i].f_trySplit) && (group[i].num > xFactor) &&
          (group[i].bit > minBit)) {
         f_keep = 0;
         doSplit (Data, numData, &(group[i]), &subGroup, &numSubGroup,
                  f_primMiss, li_primMiss, f_secMiss, li_secMiss, static_cast<int>(xFactor));
         if (numSubGroup != 1) {
            scoreA = static_cast<sInt4>(group[i].bit * group[i].num + xFactor);
            scoreB = 0;
            for (sub = 0; sub < numSubGroup; sub++) {
               scoreB += (sInt4) (subGroup[sub].bit * subGroup[sub].num + xFactor);
            }
            if (scoreB < scoreA) {
               f_keep = 1;
            } else if (numSubGroup > 2) {
               /* We can do "doSplitLeft" (which is breaking it into 2 groups 
                * the first having range n - 1, the second having range n,
                * using what we know from doSplit. */
               subGroup[1].num = group[i].num - subGroup[0].num;
               if (f_secMiss) {
                  findMaxMin2 (Data, subGroup[1].start,
                               subGroup[1].start + subGroup[1].num,
                               li_primMiss, li_secMiss, &A_min, &A_max);
               } else if (f_primMiss) {
                  findMaxMin1 (Data, subGroup[1].start,
                               subGroup[1].start + subGroup[1].num,
                               li_primMiss, &A_min, &A_max);
               } else {
                  findMaxMin0 (Data, subGroup[1].start,
                               subGroup[1].start + subGroup[1].num,
                               &A_min, &A_max);
               }
               subGroup[1].min = A_min;
               subGroup[1].max = A_max;
               subGroup[1].bit = power ((uInt4) (A_max - A_min),
                                        f_secMiss + f_primMiss);
               subGroup[1].f_trySplit = 1;
               subGroup[1].f_tryShift = 1;
               numSubGroup = 2;
               scoreB = static_cast<sInt4>(subGroup[0].bit * subGroup[0].num + xFactor);
               scoreB += static_cast<sInt4>(subGroup[1].bit * subGroup[1].num + xFactor);
               if (scoreB < scoreA) {
                  f_keep = 1;
               }
            }
         }
         if (!f_keep) {
            if (numSubGroup == 1) {
               subGroup = (TDLGroupType *) realloc (subGroup,
                                                    2 *
                                                    sizeof (TDLGroupType));
            }
            numSubGroup = 2;
            doSplitRight (Data, numData, &(group[i]), &(subGroup[1]),
                          &(subGroup[0]), f_primMiss, li_primMiss, f_secMiss,
                          li_secMiss);
            scoreA = static_cast<sInt4>(group[i].bit * group[i].num + xFactor);
            scoreB = static_cast<sInt4>(subGroup[0].bit * subGroup[0].num + xFactor);
            scoreB += static_cast<sInt4>(subGroup[1].bit * subGroup[1].num + xFactor);
            if (scoreB < scoreA) {
               f_keep = 1;
            }
         }
         if (f_keep) {
            lclIndex = *numLclGroup;
            *numLclGroup = *numLclGroup + numSubGroup;
            if (*numLclGroup > GroupLen) {
               GroupLen += extra;
               extra = 0;
               if (*numLclGroup > GroupLen) {
                  GroupLen = *numLclGroup;
               }
               *lclGroup = (TDLGroupType *) realloc ((void *) *lclGroup,
                                                     GroupLen *
                                                     sizeof (TDLGroupType));
            } else {
               extra += numSubGroup - 1;
            }
            memcpy ((*lclGroup) + lclIndex, subGroup,
                    numSubGroup * sizeof (TDLGroupType));
            f_adjust = 1;
         } else {
            *numLclGroup = *numLclGroup + 1;
            if (*numLclGroup > GroupLen) {
               GroupLen += extra;
               extra = 0;
               if (*numLclGroup > GroupLen) {
                  GroupLen = *numLclGroup;
               }
               *lclGroup = (TDLGroupType *) realloc ((void *) *lclGroup,
                                                     GroupLen *
                                                     sizeof (TDLGroupType));
            }
            (*lclGroup)[*numLclGroup - 1] = group[i];
            (*lclGroup)[*numLclGroup - 1].f_trySplit = 0;
         }
         free (subGroup);
         subGroup = NULL;
      } else {
         *numLclGroup = *numLclGroup + 1;
         if (*numLclGroup > GroupLen) {
            GroupLen += extra;
            extra = 0;
            if (*numLclGroup > GroupLen) {
               GroupLen = *numLclGroup;
            }
            *lclGroup = (TDLGroupType *) realloc ((void *) *lclGroup,
                                                  GroupLen *
                                                  sizeof (TDLGroupType));
         }
         (*lclGroup)[*numLclGroup - 1] = group[i];
         (*lclGroup)[*numLclGroup - 1].f_trySplit = 0;
      }
   }
   myAssert (GroupLen == *numLclGroup);
   return f_adjust;
}

/*****************************************************************************
 * shiftGroup() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Tries to shift / join the groups together.  It does this by first
 * calculating if a group should still exist.  If it should, then it has
 * each group grab as much as it can to the left without increasing its "bit
 * range".
 *
 * ARGUMENTS
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *       group = The resulting groups. (Output)
 *    numGroup = Number of groups (Output)
 *  f_primMiss = Flag if we have a primary missing value (Input)
 * li_primMiss = scaled primary missing value (Input)
 *   f_secMiss = Flag if we have a secondary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *     xFactor = Estimate of cost (in bits) of a group. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  1/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
static void shiftGroup (sInt4 *Data,
                        CPL_UNUSED int numData,
                        TDLGroupType ** Group,
                        size_t *NumGroup, char f_primMiss, sInt4 li_primMiss,
                        char f_secMiss, sInt4 li_secMiss, int xFactor)
{
   TDLGroupType *group = (*Group); /* Local pointer to Group. */
   int numGroup = static_cast<int>(*NumGroup); /* # elements in group. */
   int i, j;            /* loop counters. */
   sInt4 A_max;         /* Max value of a given group. */
   sInt4 A_min;         /* Min value of a given group. */
   size_t begin;        /* New start to the group. */
   sInt4 scoreA;        /* The original score for group i and i - 1 */
   sInt4 scoreB;        /* The new score for group i and i - 1 */
   TDLGroupType G1;     /* The "new" group[i - 1]. */
   TDLGroupType G2;     /* The "new" group[i]. */
   int evaporate = 0;   /* number of groups that "evaporated" */
   int index;           /* index while getting rid of "evaporated groups". */

   for (i = numGroup - 1; i > 0; i--) {
      myAssert (group[i].num > 0);
      /* See if we can evaporate the group n - 1 */
      G1.start = group[i - 1].start;
      G1.num = group[i].num + group[i - 1].num;
      G1.min = (group[i].min < group[i - 1].min) ?
            group[i].min : group[i - 1].min;
      G1.max = (group[i].max > group[i - 1].max) ?
            group[i].max : group[i - 1].max;
      G1.bit = (char) power ((uInt4) (G1.max - G1.min),
                             f_secMiss + f_primMiss);
      G1.f_trySplit = 1;
      G1.f_tryShift = 1;
      scoreA = group[i].bit * group[i].num + xFactor;
      scoreA += group[i - 1].bit * group[i - 1].num + xFactor;
      scoreB = G1.bit * G1.num + xFactor;
      if (scoreB < scoreA) {
         /* One of the groups evaporated. */
         evaporate++;
         group[i - 1] = G1;
         group[i].num = 0;
         /* See if that affects any of the previous groups. */
         for (j = i + 1; j < numGroup; j++) {
            if (group[j].num != 0) {
               /*G1.start = G1. start;*/ /* self-assignment... */
               G1.num = group[i - 1].num + group[j].num;
               G1.min = (group[i - 1].min < group[j].min) ?
                     group[i - 1].min : group[j].min;
               G1.max = (group[i - 1].max > group[j].max) ?
                     group[i - 1].max : group[j].max;
               G1.bit = (char) power ((uInt4) (G1.max - G1.min),
                                      f_secMiss + f_primMiss);
               G1.f_trySplit = 1;
               G1.f_tryShift = 1;
               scoreA = group[i - 1].bit * group[i - 1].num + xFactor;
               scoreA += group[j].bit * group[j].num + xFactor;
               scoreB = G1.bit * G1.num + xFactor;
               if (scoreB < scoreA) {
                  evaporate++;
                  group[i - 1] = G1;
                  group[j].num = 0;
               } else {
                  break;
               }
            }
         }
      } else if (group[i].f_tryShift) {
         /* Group did not evaporate, so do the "grabby" algorithm. */
         if ((group[i].bit != 0) && (group[i - 1].bit >= group[i].bit)) {
            if (f_secMiss) {
               A_max = group[i].max;
               A_min = group[i].min;
               shiftGroup2 (Data, group[i].start - 1, group[i - 1].start,
                            li_primMiss, li_secMiss, group[i].bit, &A_min,
                            &A_max, &begin);
            } else if (f_primMiss) {
               A_max = group[i].max;
               A_min = group[i].min;
               shiftGroup1 (Data, group[i].start - 1, group[i - 1].start,
                            li_primMiss, group[i].bit, &A_min, &A_max,
                            &begin);
            } else {
               A_max = group[i].max;
               A_min = group[i].min;
               shiftGroup0 (Data, group[i].start - 1, group[i - 1].start,
                            group[i].bit, &A_min, &A_max, &begin);
            }
            if (begin != group[i].start) {
               /* Re-Calculate min/max of group[i - 1], since it could have
                * moved to group[i] */
               G1 = group[i - 1];
               G2 = group[i];
               G2.min = A_min;
               G2.max = A_max;
               G1.num -= static_cast<uInt4>(group[i].start - begin);
               if (f_secMiss) {
                  findMaxMin2 (Data, G1.start, G1.start + G1.num,
                               li_primMiss, li_secMiss, &A_min, &A_max);
               } else if (f_primMiss) {
                  findMaxMin1 (Data, G1.start, G1.start + G1.num,
                               li_primMiss, &A_min, &A_max);
               } else {
                  findMaxMin0 (Data, G1.start, G1.start + G1.num,
                               &A_min, &A_max);
               }
               if ((A_min != G1.min) || (A_max != G1.max)) {
                  G1.min = A_min;
                  G1.max = A_max;
                  G1.bit = (char) power ((uInt4) (A_max - A_min),
                                         f_secMiss + f_primMiss);
                  G1.f_trySplit = 1;
                  G1.f_tryShift = 1;
               }
               G2.num += static_cast<uInt4>(group[i].start - begin);
               G2.start = static_cast<uInt4>(begin);
               G2.f_trySplit = 1;
               G2.f_tryShift = 1;
               scoreA = group[i].bit * group[i].num + xFactor;
               scoreA += group[i - 1].bit * group[i - 1].num + xFactor;
               scoreB = G2.bit * G2.num + xFactor;
               if (G1.num != 0) {
                  scoreB += G1.bit * G1.num + xFactor;
               }
#ifdef DEBUG
               if (scoreB > scoreA) {
                  printf ("Made score worse!\n");
               }
#endif
               if (begin == group[i - 1].start) {
                  /* Grabby algorithm evaporated a group. */
                  evaporate++;
                  myAssert (G1.num == 0);
                  /* Switch the evaporating group to other side so we have
                   * potential to continue evaporating the next group down. */
                  group[i - 1] = G2;
                  group[i] = G1;
               } else {
                  group[i - 1] = G1;
                  group[i] = G2;
               }
            } else {
               group[i].f_tryShift = 0;
            }
         }
      }
   }
   /* Loop through the grid removing the evaporated groups. */
   if (evaporate != 0) {
      index = 0;
      for (i = 0; i < numGroup; i++) {
         if (group[i].num != 0) {
            group[index] = group[i];
            index++;
         }
      }
      *NumGroup = numGroup - evaporate;
      *Group = (TDLGroupType *) realloc ((void *) (*Group),
                                         *NumGroup * sizeof (TDLGroupType));
   }
}

/*****************************************************************************
 * GroupIt() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Attempts to find groups for packing the data.  It starts by preparing
 * the data, by removing the overall min value.
 *
 *   Next it Creates any 0 bit groups (primary missing: missing are all 0
 * bit groups, const values are 0 bit groups.  No missing: const values are
 * 0 bit groups.)
 *
 *   Next it tries to reduce (split) each group by 1 bit.  It does this by:
 *   A) reduce the "bit range", and create groups that grab as much as they
 * can to the right.  Then reduce those groups if they improve the score.
 *   B) reduce the bit range and grab the left most group only, leaving the
 * rest unchanged.
 *   C) reduce the bit range and grab the right most group only, leaving the
 * rest unchanged.
 *
 *   Next it tries to shift / join those groups together.  It does this by
 * first calculating if a group should still exist.  If it should, then it
 * has each group grab as much as it can to the left without increasing its
 * "bit range".
 *
 * ARGUMENTS
 *  OverallMin = The overall min value in the data. (Input)
 *        Data = The data. (Input)
 *     numData = The number of elements in data. (Input)
 *       group = The resulting groups. (Output)
 *    numGroup = Number of groups (Output)
 *  f_primMiss = Flag if we have a primary missing value (Input)
 * li_primMiss = scaled primary missing value (Input)
 *   f_secMiss = Flag if we have a secondary missing value (Input)
 *  li_secMiss = scaled secondary missing value (Input)
 *   groupSize = How many bytes the groups and data will take. (Output)
 *        ibit = # of bits for largest minimum value in groups (Output)
 *        jbit = # of bits for largest # bits in groups (Output)
 *        kbit = # of bits for largest # values in groups (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  1/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *  1) Have not implemented const 0 bit groups for prim miss or no miss.
 *  2) MAX_GROUP_LEN (found experimentally) is useful to keep cost of groups
 *     down.
 *****************************************************************************
 */
#define MAX_GROUP_LEN 255
static void GroupIt (sInt4 OverallMin, sInt4 *Data, size_t numData,
                     TDLGroupType ** group, size_t *numGroup, char f_primMiss,
                     sInt4 li_primMiss, char f_secMiss,
                     sInt4 li_secMiss, sInt4 *groupSize, size_t *ibit,
                     size_t *jbit, size_t *kbit)
{
   sInt4 A_max;         /* Max value of a given group. */
   sInt4 A_min;         /* Min value of a given group. */
   TDLGroupType G;      /* Used to init the groups. */
   int f_adjust;        /* Flag if we have changed the groups. */
   TDLGroupType *lclGroup; /* A temporary copy of the groups. */
   int numLclGroup;     /* # of groups in lclGroup. */
   size_t xFactor;      /* Estimate of cost (in bits) of a group. */
   size_t i;            /* loop counter. */

   /* Subtract the Overall Min Value. */
   if (OverallMin != 0) {
      if (f_secMiss) {
         for (i = 0; i < numData; i++) {
            if ((Data[i] != li_secMiss) && (Data[i] != li_primMiss)) {
               Data[i] -= OverallMin;
               // Check if we accidentally adjusted to prim or sec, if so add 1.
               if ((Data[i] == li_secMiss) || (Data[i] == li_primMiss)) {
                  myAssert (1 == 2);
                  Data[i]++;
                  if ((Data[i] == li_secMiss) || (Data[i] == li_primMiss)) {
                     myAssert (1 == 2);
                     Data[i]++;
                  }
               }
            }
         }
      } else if (f_primMiss) {
         for (i = 0; i < numData; i++) {
            if (Data[i] != li_primMiss) {
               Data[i] -= OverallMin;
               // Check if we accidentally adjusted to prim or sec, if so add 1.
               if (Data[i] == li_primMiss) {
                  myAssert (1 == 2);
                  Data[i]++;
               }
            }
         }
      } else {
         for (i = 0; i < numData; i++) {
            Data[i] -= OverallMin;
         }
      }
   }

   myAssert ((f_secMiss == 0) || (f_secMiss == 1));
   myAssert ((f_primMiss == 0) || (f_primMiss == 1));

   /* Create zero groups. */
   *numGroup = 0;
   *group = NULL;
   if (f_primMiss) {
      G.min = Data[0];
      G.max = Data[0];
      G.num = 1;
      G.start = 0;
      for (i = 1; i < numData; i++) {
         if (G.min == li_primMiss) {
            if (Data[i] == li_primMiss) {
               G.num++;
               if (G.num == (MAX_GROUP_LEN + 1)) {
                  /* Close a missing group */
                  G.f_trySplit = 0;
                  G.f_tryShift = 1;
                  G.bit = 0;
                  G.min = 0;
                  G.max = 0;
                  G.num = MAX_GROUP_LEN;
                  (*numGroup)++;
                  *group = (TDLGroupType *) realloc ((void *) *group,
                                                     *numGroup *
                                                     sizeof (TDLGroupType));
                  (*group)[(*numGroup) - 1] = G;
                  /* Init a missing group. */
                  G.min = Data[i];
                  G.max = Data[i];
                  G.num = 1;
                  G.start = static_cast<uInt4>(i);
               }
            } else {
               /* Close a missing group */
               G.f_trySplit = 0;
               G.f_tryShift = 1;
               G.bit = 0;
               G.min = 0;
               G.max = 0;
               (*numGroup)++;
               *group = (TDLGroupType *) realloc ((void *) *group,
                                                  *numGroup *
                                                  sizeof (TDLGroupType));
               (*group)[(*numGroup) - 1] = G;
               /* Init a non-missing group. */
               G.min = Data[i];
               G.max = Data[i];
               G.num = 1;
               G.start = static_cast<uInt4>(i);
            }
         } else {
            if (Data[i] == li_primMiss) {
               /* Close a non-missing group */
               G.f_trySplit = 1;
               G.f_tryShift = 1;
               G.bit = (char) power ((uInt4) (G.max - G.min),
                                     f_secMiss + f_primMiss);
               myAssert (G.bit != 0);
               if ((G.min == 0) && (G.bit == 0) && (f_primMiss == 1)) {
                  printf ("Warning: potential confusion between const value "
                          "and prim-missing.\n");
                  G.bit = 1;
               }
               (*numGroup)++;
               *group = (TDLGroupType *) realloc ((void *) *group,
                                                  *numGroup *
                                                  sizeof (TDLGroupType));
               (*group)[(*numGroup) - 1] = G;
               /* Init a missing group. */
               G.min = Data[i];
               G.max = Data[i];
               G.num = 1;
               G.start = static_cast<uInt4>(i);
            } else {
               if (G.min > Data[i]) {
                  G.min = Data[i];
               } else if (G.max < Data[i]) {
                  G.max = Data[i];
               }
               G.num++;
            }
         }
      }
      if (G.min == li_primMiss) {
         /* Close a missing group */
         G.f_trySplit = 0;
         G.f_tryShift = 1;
         G.bit = 0;
         G.min = 0;
         G.max = 0;
         (*numGroup)++;
         *group = (TDLGroupType *) realloc ((void *) *group,
                                            *numGroup *
                                            sizeof (TDLGroupType));
         (*group)[(*numGroup) - 1] = G;
      } else {
         /* Close a non-missing group */
         G.f_trySplit = 1;
         G.f_tryShift = 1;
         G.bit = (char) power ((uInt4) (G.max - G.min),
                               f_secMiss + f_primMiss);
         myAssert (G.bit != 0);
         if ((G.min == 0) && (G.bit == 0) && (f_primMiss == 1)) {
            printf ("Warning: potential confusion between const value and "
                    "prim-missing.\n");
            G.bit = 1;
         }
         (*numGroup)++;
         *group = (TDLGroupType *) realloc ((void *) *group,
                                            *numGroup *
                                            sizeof (TDLGroupType));
         (*group)[(*numGroup) - 1] = G;
      }
   } else {
      /* Already handled the f_primMiss case */
      if (f_secMiss) {
         findMaxMin2 (Data, 0, static_cast<int>(numData), li_primMiss, li_secMiss, &A_min,
                      &A_max);
      } else {
         findMaxMin0 (Data, 0, static_cast<int>(numData), &A_min, &A_max);
      }
      G.start = 0;
      G.num = static_cast<uInt4>(numData);
      G.min = A_min;
      G.max = A_max;
      G.bit = (char) power ((uInt4) (A_max - A_min), f_secMiss + f_primMiss);
      G.f_trySplit = 1;
      G.f_tryShift = 1;
      *numGroup = 1;
      *group = (TDLGroupType *) malloc (sizeof (TDLGroupType));
      (*group)[0] = G;
   }

   lclGroup = NULL;
   numLclGroup = 0;
   *groupSize = ComputeGroupSize (*group, static_cast<int>(*numGroup), ibit, jbit, kbit);
   xFactor = *ibit + *jbit + *kbit;
#ifdef DEBUG
/*
   printf ("NumGroup = %d: Bytes = %ld: XFactor %d\n", *numGroup,
           (*groupSize / 8) + 1, xFactor);
*/
#endif

   f_adjust = 1;
   while (f_adjust) {
      f_adjust = splitGroup (Data, static_cast<int>(numData), *group, static_cast<int>(*numGroup), &lclGroup,
                             &numLclGroup, f_primMiss, li_primMiss,
                             f_secMiss, li_secMiss, xFactor);
      free (*group);
      *group = lclGroup;
      *numGroup = numLclGroup;

      if (f_adjust) {
         shiftGroup (Data, static_cast<int>(numData), group, numGroup, f_primMiss,
                     li_primMiss, f_secMiss, li_secMiss, static_cast<int>(xFactor));
         *groupSize = ComputeGroupSize (*group, static_cast<int>(*numGroup), ibit, jbit, kbit);
         if (xFactor != *ibit + *jbit + *kbit) {
            for (i = 0; i < *numGroup; i++) {
               if (((*group)[i].num > *ibit + *jbit + *kbit) &&
                   ((*group)[i].num <= xFactor)) {
                  (*group)[i].f_trySplit = 1;
               }
            }
         }
         xFactor = *ibit + *jbit + *kbit;
#ifdef DEBUG
/*
         printf ("NumGroup = %d: Bytes = %ld: XFactor %d\n", *numGroup,
                 (*groupSize / 8) + 1, xFactor);
         fflush (stdout);
*/
#endif
      }
   }
}

/*****************************************************************************
 * GroupPack() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To compute groups for packing the data using complex or second order
 * complex packing.
 *
 * ARGUMENTS
 *        Src = The original data. (Input)
 *        Dst = The scaled data. (Output)
 *    numData = The number of elements in data. (Input)
 *        DSF = Decimal Scale Factor for scaling the data. (Input)
 *        BSF = Binary Scale Factor for scaling the data. (Input)
 * f_primMiss = Flag saying if we have a primary missing value (In/Out)
 *   primMiss = primary missing value. (In/Out)
 *  f_secMiss = Flag saying if we have a secondary missing value (In/Out)
 *    secMiss = secondary missing value. (In/Out)
 *     f_grid = Flag if this is grid data (or vector) (Input)
 *         NX = The number of X values. (Input)
 *         NY = The number of Y values. (Input)
 * f_sndOrder = Flag if we should do second order differencing (Output)
 *      group = Resulting groups. (Output)
 *   numGroup = Number of groups. (Output)
 *        Min = Overall minimum. (Output)
 *         a1 = if f_sndOrder, the first first order difference (Output)
 *         b2 = if f_sndOrder, the first second order difference (Output)
 *  groupSize = How many bytes the groups and data will take. (Output)
 *       ibit = # of bits for largest minimum value in groups (Output)
 *       jbit = # of bits for largest # bits in groups (Output)
 *       kbit = # of bits for largest # values in groups (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *   0 = OK
 *  -1 = Primary or Secondary missing value == 0.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Updated from "group.c" in "C" tdlpack code.
 *   1/2005 AAT: Cleaned up.
 *
 * NOTES
 *****************************************************************************
 */
static int GroupPack (double *Src, sInt4 **Dst, sInt4 numData,
                      int DSF, int BSF, char *f_primMiss, double *primMiss,
                      char *f_secMiss, double *secMiss, char f_grid,
                      short int NX, short int NY, char *f_sndOrder,
                      TDLGroupType ** group, size_t *numGroup,
                      sInt4 *Min, sInt4 *a1, sInt4 *b2,
                      sInt4 *groupSize, size_t *ibit, size_t *jbit,
                      size_t *kbit)
{
   sInt4 *SecDiff = NULL; /* Consists of the 2nd order differences if *
                           * requested. */
   sInt4 *Data;         /* The scaled data. */
   char f_min;          /* Flag saying overallMin is valid. */
   sInt4 overallMin = 0;    /* The overall min of the scaled data. */
   sInt4 secMin;        /* The overall min of the 2nd order differences */
   sInt4 li_primMiss = 0; /* The scaled primary missing value */
   sInt4 li_secMiss = 0; /* The scaled secondary missing value */
   int minGroup = 20;   /* The minimum group size. Equivalent to xFactor?
                         * Chose 20 because that was a good estimate of
                         * XFactor. */

   /* Check consistency of f_primMiss and f_secMiss. */
   if (*primMiss == *secMiss) {
      *f_secMiss = 0;
   }
   if ((*f_secMiss) && (!(*f_primMiss))) {
      *f_primMiss = *f_secMiss;
      *primMiss = *secMiss;
      *f_secMiss = 0;
   }
   if (*f_secMiss && (*secMiss == 0)) {
      errSprintf ("Error: Secondary missing value not allowed to = 0.\n");
      return -1;
   }
   if (*f_primMiss && (*primMiss == 0)) {
      errSprintf ("Error: Primary missing value not allowed to = 0.\n");
      return -1;
   }

   /* Check minGroup size. */
   if (minGroup > numData) {
      minGroup = numData;
   }

   /* Scale the data and check if we can change f_prim or f_sec. */
   /* Note: if we use sec_diff, we have a different overall min. */
   f_min = 0;
   Data = (sInt4 *) malloc (numData * sizeof (sInt4));
   TDL_ScaleData (Src, Data, numData, DSF, BSF, f_primMiss, primMiss,
                  f_secMiss, secMiss, &f_min, &overallMin);
   /* Note: ScaleData also scales missing values. */
   if (*f_primMiss) {
      li_primMiss = (sInt4) (*primMiss * SCALE_MISSING + .5);
   }
   if (*f_secMiss) {
      li_secMiss = (sInt4) (*secMiss * SCALE_MISSING + .5);
   }

   /* Reason this is after TDL_ScaleData is we don't want to reorder the
    * caller's copy of the data. */
   if (f_grid) {
      TDL_ReorderGrid (Data, NX, NY);
   } else {
      /* TDLPack requires the following (see pack2d.f and pack1d.f) */
      *f_sndOrder = 0;
   }
   /* TDLPack requires the following (see pack2d.f) */
   if (*f_secMiss) {
      *f_sndOrder = 0;
   }
   /* If overallMin is "invalid" then they are all prim or sec values. */
   /* See pack2d.f line 336 */
   /* IF ALL VALUES ARE MISSING, JUST PACK; DON'T CONSIDER 2ND ORDER
    * DIFFERENCES. */
   if (!f_min) {
      *f_sndOrder = 0;
   }

   /* This has to be after TDL_ReorderGrid */
   if (*f_sndOrder) {
      SecDiff = (sInt4 *) malloc (numData * sizeof (sInt4));
      if (TDL_GetSecDiff (Data, numData, SecDiff, *f_primMiss, li_primMiss,
                          a1, b2, &secMin)) {
         /* Problem finding SecDiff, so we don't bother with it. */
         *f_sndOrder = 0;
      } else {
         /* Check if it is worth doing second order differences. */
         if (*f_primMiss) {
            *f_sndOrder = TDL_UseSecDiff_Prim (Data, numData, SecDiff,
                                               li_primMiss, minGroup);
         } else {
            *f_sndOrder = TDL_UseSecDiff (Data, numData, SecDiff, minGroup);
         }
      }
   }

   /* Side affect of GroupIt2: it subtracts OverallMin from Data. */
   if (!(*f_sndOrder)) {
      GroupIt (overallMin, Data, numData, group, numGroup, *f_primMiss,
               li_primMiss, *f_secMiss, li_secMiss, groupSize, ibit, jbit,
               kbit);
      *Min = overallMin;
      *a1 = 0;
      *b2 = 0;
      *Dst = Data;
      free (SecDiff);
   } else {
      GroupIt (secMin, SecDiff, numData, group, numGroup, *f_primMiss,
               li_primMiss, *f_secMiss, li_secMiss, groupSize, ibit, jbit,
               kbit);
      *Min = secMin;
      *Dst = SecDiff;
      free (Data);
   }
   return 0;
}

/*****************************************************************************
 * WriteTDLPRecord() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Writes a TDLP message to file.
 *
 * ARGUMENTS
 *         fp = An opened TDLP file already at the correct location. (Input)
 *       Data = The data to write. (Input)
 *    DataLen = Length of Data. (Input)
 *        DSF = Decimal scale factor to apply to the data (Input)
 *        BSF = Binary scale factor to apply to the data (Input)
 * f_primMiss = Flag saying if we have a primary missing value (Input)
 *   primMiss = primary missing value. (Input)
 *  f_secMiss = Flag saying if we have a secondary missing value (Input)
 *    secMiss = secondary missing value. (Input)
 *        gds = The grid definition section (Input)
 *    comment = Describes the kind of data (max 32 bytes). (Input)
 *    refTime = The reference (creation) time of this message. (Input)
 *        ID1 = TDLPack ID1 (Input)
 *        ID2 = TDLPack ID2 (Input)
 *        ID3 = TDLPack ID3 (Input)
 *        ID4 = TDLPack ID4 (Input)
 *    projSec = The projection in seconds (Input)
 * processNum = The process number that created it (Input)
 *     seqNum = The sequence number that created it (Input)
 *
 * FILES/DATABASES:
 *   An already opened file pointing to the desired TDLP message.
 *
 * RETURNS: int (could use errSprintf())
 *   0 = OK
 *  -1 = comment is too long.
 *  -2 = projHr is inconsistent with ID3.
 *  -3 = Type of map projection that TDLP can't handle.
 *  -4 = Primary or Secondary missing value == 0.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Created
 *   1/2005 AAT: Cleaned up.
 *
 * NOTES
 *****************************************************************************
 */
int WriteTDLPRecord (FILE * fp, double *Data, sInt4 DataLen, int DSF,
                     int BSF, char f_primMiss, double primMiss,
                     char f_secMiss, double secMiss, gdsType *gds,
                     char *comment, double refTime, sInt4 ID1,
                     sInt4 ID2, sInt4 ID3, sInt4 ID4,
                     sInt4 projSec, sInt4 processNum, sInt4 seqNum)
{
   sInt4 *Scaled;       /* The scaled data. */
   TDLGroupType *group; /* The groups used to pack the data. */
   size_t numGroup;     /* Number of groups. */
   char f_grid = 1;     /* Flag if this is gridded data. In theory can handle
                         * vector data, but haven't tested it. */
   char f_sndOrder;     /* Flag if we should try second order packing. */

   /* TODO: Trace overallMin to figure out how it could be used uninitialized */
   sInt4 overallMin;    /* Overall min value of the scaled data. */

   sInt4 a1;            /* 2nd order difference: 1st value. */
   sInt4 b2;            /* 2nd order difference: 1st 1st order difference */
   sInt4 li_primMiss;   /* Scaled primary missing value. */
   sInt4 li_secMiss;    /* Scaled secondary missing value. */
   int mbit;            /* # of bits for b2. */
   int nbit;            /* # of bits for overallMin. */
   size_t ibit;         /* # of bits for largest minimum value in groups */
   size_t jbit;         /* # of bits for largest # bits in groups */
   size_t kbit;         /* # of bits for largest # values in groups */
   int sec1Len;         /* Length of section 1. */
   size_t pad;          /* Number of bytes to pad the message to get to the
                         * correct byte boundary. */
   sInt4 groupSize;     /* How many bytes the groups and data will take. */
   sInt4 sec4Len;       /* Length of section 4. */
   sInt4 tdlRecSize;    /* Size of the TDLP message. */
   sInt4 recSize;       /* Actual record size (including FORTRAN bytes). */
   int commentLen;      /* Length of comment */
   short int projHr;    /* The hours part of the forecast projection. */
   char projMin;        /* The minutes part of the forecast projection. */
   sInt4 year;          /* The reference year. */
   int month, day;      /* The reference month day. */
   int hour, min;       /* The reference hour minute. */
   double sec;          /* The reference second. */
   char f_bitmap = 0;   /* Bitmap flag: not implemented in specs. */
   char f_simple = 0;   /* Simple Pack flag: not implemented in specs. */
   int gridType;        /* Which type of grid. (Polar, Mercator, Lambert). */
   int dataCnt;         /* Keeps track of which element we are writing. */
   sInt4 max0;          /* The max value in a group.  Represents primary or * 
                         * secondary missing value depending on scheme. */
   sInt4 max1;          /* The next to max value in a group.  Represents *
                         * secondary missing value. */
   size_t i, j;         /* loop counters */
   sInt4 li_temp;       /* Temporary variable (sInt4). */
   short int si_temp;   /* Temporary variable (short int). */
   double d_temp;       /* Temporary variable (double). */
   char buffer[6];      /* Used to write reserved values */
   uChar pbuf;          /* A buffer of bits that were not written to disk */
   sChar pbufLoc;       /* Where in pbuf to add more bits. */

   commentLen = static_cast<int>(strlen (comment));
   if (commentLen > 32) {
      errSprintf ("Error: '%s' is > 32 bytes long\n", comment);
      return -1;
   }
   projHr = projSec / 3600;
   projMin = (projSec % 3600) / 60;
   if (projHr != (ID3 - ((ID3 / 1000) * 1000))) {
      errSprintf ("Error: projHr = %d is inconsistent with ID3 = %ld\n",
                  projHr, ID3);
      return -2;
   }
   if (f_grid) {
      switch (gds->projType) {
         case GS3_POLAR:
            gridType = TDLP_POLAR;
            break;
         case GS3_LAMBERT:
            gridType = TDLP_LAMBERT;
            break;
         case GS3_MERCATOR:
            gridType = TDLP_MERCATOR;
            break;
         default:
            errSprintf ("TDLPack can't handle GRIB projection type %d\n",
                        gds->projType);
            return -3;
      }
   }

   if (GroupPack (Data, &Scaled, DataLen, DSF, BSF, &f_primMiss, &primMiss,
                  &f_secMiss, &secMiss, f_grid, gds->Nx, gds->Ny,
                  &f_sndOrder, &group, &numGroup, &overallMin, &a1, &b2,
                  &groupSize, &ibit, &jbit, &kbit) != 0) {
      return -4;
   }

   /* Make sure missing data is properly scaled. */
   if (f_primMiss) {
      li_primMiss = (sInt4) (primMiss * SCALE_MISSING + .5);
   }
   if (f_secMiss) {
      li_secMiss = (sInt4) (secMiss * SCALE_MISSING + .5);
   }

   /* Compute TDL record size. */
/* *INDENT-OFF* */
   /* TDL Record size
    * 8 (section 0),
    * 39 + strlen(comment) (section 1),
    * 0 or 28 (depending on if you have a section 2),
    * 0 (section 3),
    * 16 + 5 + 1 + nbit(min val) + 16 + 5 + 5 + 5 + GroupSize() (section 4)
    * 4 (section 5)
    * pad (to even 8 bytes...)
    * Group size uses:
    *    ibit * num_groups + jbit * num_groups +
    *    kbit * num_groups + group.nbit * group.nvalues */
/* *INDENT-ON*  */
   sec1Len = 39 + commentLen;
   if (overallMin < 0) {
      nbit = power (-1 * overallMin, 0);
   } else {
      nbit = power (overallMin, 0);
   }
   if (!f_sndOrder) {
      if (f_secMiss) {
         sec4Len = 16 + (sInt4) (ceil ((5 + 1 + nbit + 16 + 5 + 5 + 5
                                        + groupSize) / 8.));
      } else if (f_primMiss) {
         sec4Len = 12 + (sInt4) (ceil ((5 + 1 + nbit + 16 + 5 + 5 + 5
                                        + groupSize) / 8.));
      } else {
         sec4Len = 8 + (sInt4) (ceil ((5 + 1 + nbit + 16 + 5 + 5 + 5
                                       + groupSize) / 8.));
      }
   } else {
      if (b2 < 0) {
         mbit = power (-1 * b2, 0);
      } else {
         mbit = power (b2, 0);
      }
      if (f_secMiss) {
         sec4Len =
               16 +
               (sInt4) (ceil
                        ((32 + 5 + 1 + mbit + 5 + 1 + nbit + 16 + 5 + 5 + 5 +
                          groupSize) / 8.));
      } else if (f_primMiss) {
         sec4Len =
               12 +
               (sInt4) (ceil
                        ((32 + 5 + 1 + mbit + 5 + 1 + nbit + 16 + 5 + 5 + 5 +
                          groupSize) / 8.));
      } else {
         sec4Len =
               8 +
               (sInt4) (ceil
                        ((32 + 5 + 1 + mbit + 5 + 1 + nbit + 16 + 5 + 5 + 5 +
                          groupSize) / 8.));
      }
   }
   if (f_grid) {
      tdlRecSize = 8 + sec1Len + 28 + 0 + sec4Len + 4;
   } else {
      tdlRecSize = 8 + sec1Len + 0 + 0 + sec4Len + 4;
   }
   /* Actual recSize is 8 + round(tdlRecSize) to nearest 8 byte boundary. */
   recSize = 8 + (sInt4) (ceil (tdlRecSize / 8.0)) * 8;
   pad = (int) ((recSize - 8) - tdlRecSize);

   /* --- Start Writing record. --- */
   /* First write FORTRAN record information */
   const size_t read_size = fread(&(recSize), sizeof (sInt4), 1, fp);
   if (read_size != 1) {
     fprintf(stderr, "WARNING: tdlpack.cpp read of recSize failed.\n");
   }
   li_temp = 0;
   FWRITE_BIG (&(li_temp), sizeof (sInt4), 1, fp);
   li_temp = recSize - 8; /* FORTRAN rec length. */
   FWRITE_BIG (&(li_temp), sizeof (sInt4), 1, fp);

   /* Now write section 0. */
   fwrite ("TDLP", sizeof (char), 4, fp);
   FWRITE_ODDINT_BIG (&(tdlRecSize), 3, fp);
   /* version number */
   fputc (0, fp);

   /* Now write section 1. */
   fputc (sec1Len, fp);
   /* output type specification... */
   i = 0;
   if (f_grid) {
      i |= 1;
   }
   if (f_bitmap) {
      i |= 2;
   }
   fputc (static_cast<int>(i), fp);
/*   tempTime = gmtime (&(refTime));*/
   Clock_PrintDate (refTime, &year, &month, &day, &hour, &min, &sec);
/* year = tempTime->tm_year + 1900;
   month = tempTime->tm_mon + 1;
   day = tempTime->tm_mday;
   hour = tempTime->tm_hour;
   min = tempTime->tm_min;
*/
   si_temp = year;
   FWRITE_BIG (&si_temp, sizeof (short int), 1, fp);
   fputc (month, fp);
   fputc (day, fp);
   fputc (hour, fp);
   fputc (min, fp);
   li_temp = (year * 1000000 + month * 10000 + day * 100 + hour);
   FWRITE_BIG (&li_temp, sizeof (sInt4), 1, fp);
   FWRITE_BIG (&ID1, sizeof (sInt4), 1, fp);
   FWRITE_BIG (&ID2, sizeof (sInt4), 1, fp);
   FWRITE_BIG (&ID3, sizeof (sInt4), 1, fp);
   FWRITE_BIG (&ID4, sizeof (sInt4), 1, fp);
   FWRITE_BIG (&projHr, sizeof (short int), 1, fp);
   fputc (projMin, fp);
   fputc (processNum, fp);
   fputc (seqNum, fp);
   i = (DSF < 0) ? 128 - DSF : DSF;
   fputc (static_cast<int>(i), fp);
   i = (BSF < 0) ? 128 - BSF : BSF;
   fputc (static_cast<int>(i), fp);
   /* Reserved: 3 bytes of 0. */
   li_temp = 0;
   fwrite (&li_temp, sizeof (char), 3, fp);
   fputc (commentLen, fp);
   fwrite (comment, sizeof (char), commentLen, fp);

   /* Now write section 2. */
   if (f_grid) {
      fputc (28, fp);
      fputc (gridType, fp);
      si_temp = gds->Nx;
      FWRITE_BIG (&si_temp, sizeof (short int), 1, fp);
      si_temp = gds->Ny;
      FWRITE_BIG (&si_temp, sizeof (short int), 1, fp);
      li_temp = (sInt4) (gds->lat1 * 10000. + .5);
      if (li_temp < 0) {
         pbuf = 128;
         li_temp = -1 * li_temp;
      } else {
         pbuf = 0;
      }
      pbufLoc = 7;
      fileBitWrite (&(li_temp), sizeof (li_temp), 23, fp, &pbuf, &pbufLoc);
      myAssert (pbufLoc == 8);
      myAssert (pbuf == 0);
      d_temp = 360 - gds->lon1;
      if (d_temp < 0)
         d_temp += 360;
      if (d_temp > 360)
         d_temp -= 360;
      li_temp = (sInt4) (d_temp * 10000. + .5);
      if (li_temp < 0) {
         pbuf = 128;
         li_temp = -1 * li_temp;
      }
      pbufLoc = 7;
      fileBitWrite (&(li_temp), sizeof (li_temp), 23, fp, &pbuf, &pbufLoc);
      myAssert (pbufLoc == 8);
      myAssert (pbuf == 0);
      d_temp = 360 - gds->orientLon;
      if (d_temp < 0)
         d_temp += 360;
      if (d_temp > 360)
         d_temp -= 360;
      li_temp = (sInt4) (d_temp * 10000. + .5);
      if (li_temp < 0) {
         pbuf = 128;
         li_temp = -1 * li_temp;
      }
      pbufLoc = 7;
      fileBitWrite (&(li_temp), sizeof (li_temp), 23, fp, &pbuf, &pbufLoc);
      myAssert (pbufLoc == 8);
      myAssert (pbuf == 0);
      li_temp = (sInt4) (gds->Dx * 1000. + .5);
      FWRITE_BIG (&li_temp, sizeof (sInt4), 1, fp);
      li_temp = (sInt4) (gds->meshLat * 10000. + .5);
      if (li_temp < 0) {
         pbuf = 128;
         li_temp = -1 * li_temp;
      }
      pbufLoc = 7;
      fileBitWrite (&(li_temp), sizeof (li_temp), 23, fp, &pbuf, &pbufLoc);
      myAssert (pbufLoc == 8);
      myAssert (pbuf == 0);
      memset (buffer, 0, 6);
      fwrite (buffer, sizeof (char), 6, fp);
   }

   /* Now write section 3. */
   /* Bitmap is not supported, skipping. */
   myAssert (!f_bitmap);

   /* Now write section 4. */
   FWRITE_ODDINT_BIG (&(sec4Len), 3, fp);
   i = 0;
   if (f_secMiss)
      i |= 1;
   if (f_primMiss)
      i |= 2;
   if (f_sndOrder)
      i |= 4;
   if (!f_simple)
      i |= 8;
   if (!f_grid)
      i |= 16;
   fputc (static_cast<int>(i), fp);
   li_temp = DataLen;
   FWRITE_BIG (&li_temp, sizeof (sInt4), 1, fp);
   if (f_primMiss) {
      FWRITE_BIG (&(li_primMiss), sizeof (sInt4), 1, fp);
      if (f_secMiss) {
         FWRITE_BIG (&(li_secMiss), sizeof (sInt4), 1, fp);
      }
   }
   if (f_sndOrder) {
      if (a1 < 0) {
         pbuf = 128;
         li_temp = -1 * a1;
      } else {
         pbuf = 0;
         li_temp = a1;
      }
      pbufLoc = 7;
      fileBitWrite (&(li_temp), sizeof (li_temp), 31, fp, &pbuf, &pbufLoc);
      myAssert (pbufLoc == 8);
      myAssert (pbuf == 0);
      fileBitWrite (&mbit, sizeof (mbit), 5, fp, &pbuf, &pbufLoc);
      if (b2 < 0) {
         i = 1;
         li_temp = -1 * b2;
      } else {
         i = 0;
         li_temp = b2;
      }
      fileBitWrite (&i, sizeof (i), 1, fp, &pbuf, &pbufLoc);
      myAssert (pbufLoc == 2);
      fileBitWrite (&li_temp, sizeof (li_temp), (unsigned short int) mbit,
                    fp, &pbuf, &pbufLoc);
   }
   fileBitWrite (&nbit, sizeof (nbit), 5, fp, &pbuf, &pbufLoc);
   if (overallMin < 0) {
      i = 1;
      li_temp = -1 * overallMin;
   } else {
      i = 0;
      li_temp = overallMin;
   }
   fileBitWrite (&i, sizeof (i), 1, fp, &pbuf, &pbufLoc);
   fileBitWrite (&li_temp, sizeof (li_temp), (unsigned short int) nbit, fp,
                 &pbuf, &pbufLoc);
   fileBitWrite (&numGroup, sizeof (numGroup), 16, fp, &pbuf, &pbufLoc);
   fileBitWrite (&ibit, sizeof (ibit), 5, fp, &pbuf, &pbufLoc);
   fileBitWrite (&jbit, sizeof (jbit), 5, fp, &pbuf, &pbufLoc);
   fileBitWrite (&kbit, sizeof (kbit), 5, fp, &pbuf, &pbufLoc);
   for (i = 0; i < numGroup; i++) {
      fileBitWrite (&(group[i].min), sizeof (sInt4),
                    (unsigned short int) ibit, fp, &pbuf, &pbufLoc);
   }
   for (i = 0; i < numGroup; i++) {
      fileBitWrite (&(group[i].bit), sizeof (char),
                    (unsigned short int) jbit, fp, &pbuf, &pbufLoc);
   }
#ifdef DEBUG
   li_temp = 0;
#endif
   for (i = 0; i < numGroup; i++) {
      fileBitWrite (&(group[i].num), sizeof (sInt4),
                    (unsigned short int) kbit, fp, &pbuf, &pbufLoc);
#ifdef DEBUG
      li_temp += group[i].num;
#endif
   }
#ifdef DEBUG
   /* Sanity check ! */
   if (li_temp != DataLen) {
      printf ("Total packed in groups %d != DataLen %d\n", li_temp,
              DataLen);
   }
   myAssert (li_temp == DataLen);
#endif

   dataCnt = 0;
   /* Start going through the data. Grid data has already been reordered.
    * Already taken care of 2nd order differences. Data at this point should
    * contain a stream of either 2nd order differences or 0 order
    * differences. We have also removed overallMin from all non-missing
    * elements in the stream */
   if (f_secMiss) {
      for (i = 0; i < numGroup; i++) {
         max0 = (1 << group[i].bit) - 1;
         max1 = (1 << group[i].bit) - 2;
         for (j = 0; j < group[i].num; j++) {
            if (Scaled[dataCnt] == li_primMiss) {
               li_temp = max0;
            } else if (Scaled[dataCnt] == li_secMiss) {
               li_temp = max1;
            } else {
               li_temp = Scaled[dataCnt] - group[i].min;
            }
            fileBitWrite (&(li_temp), sizeof (sInt4), group[i].bit, fp,
                          &pbuf, &pbufLoc);
            dataCnt++;
         }
      }
   } else if (f_primMiss) {
      for (i = 0; i < numGroup; i++) {
         /* see what happens when bit == 0. */
         max0 = (1 << group[i].bit) - 1;
         for (j = 0; j < group[i].num; j++) {
            if (group[i].bit != 0) {
               if (Scaled[dataCnt] == li_primMiss) {
                  li_temp = max0;
               } else {
                  li_temp = Scaled[dataCnt] - group[i].min;
               }
               fileBitWrite (&(li_temp), sizeof (sInt4), group[i].bit, fp,
                             &pbuf, &pbufLoc);
            }
            dataCnt++;
         }
      }
   } else {
      for (i = 0; i < numGroup; i++) {
         for (j = 0; j < group[i].num; j++) {
            li_temp = Scaled[dataCnt] - group[i].min;
            if (group[i].bit != 0) {
               fileBitWrite (&(li_temp), sizeof (sInt4), group[i].bit, fp,
                             &pbuf, &pbufLoc);
            }
            dataCnt++;
         }
      }
   }
   myAssert (dataCnt == DataLen);
   /* flush the PutBit buffer... */
   if (pbufLoc != 8) {
      fputc ((int) pbuf, fp);
   }

   /* Now write section 5. */
   fwrite ("7777", sizeof (char), 4, fp);

   /* Deal with padding at end of record... */
   for (i = 0; i < pad; i++) {
      fputc (0, fp);
   }
   /* Now write FORTRAN record information */
   FWRITE_BIG (&(recSize), sizeof (sInt4), 1, fp);

   free (Scaled);
   free (group);
   return 0;
}

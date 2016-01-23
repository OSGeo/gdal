/*****************************************************************************
 * metaprint.c
 *
 * DESCRIPTION
 *    This file contains the code necessary to write out the meta data
 * structure.
 *
 * HISTORY
 *    9/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 * 1) Need to add support for GS3_ORTHOGRAPHIC = 90,
 *    GS3_EQUATOR_EQUIDIST = 110, GS3_AZIMUTH_RANGE = 120
 * 2) Need to add support for GS4_RADAR = 20, GS4_SATELLITE = 30
 *****************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#ifndef DONT_DEPRECATE_SPRINTF
#define DONT_DEPRECATE_SPRINTF
#endif
#include "cpl_port.h"
#include "meta.h"
#include "metaname.h"
#include "myerror.h"
#include "myutil.h"
#include "tdlpack.h"
#include "myassert.h"
#include "clock.h"

/*****************************************************************************
 * Lookup() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To lookup the string value in a table, given the table, the index, and
 * some default values.
 *
 * ARGUMENTS
 * table = The table to look in. (Input)
 *     n = Size of table. (Input)
 * index = Index to look up. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: char *
 *   The desired index value, or the appropriate default message.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * Can not do a sizeof(table) here because table is now of arbitrary length.
 * Instead do sizeof(table) in calling procedure.
 *****************************************************************************
 */
static const char *Lookup(const char * const *table, size_t n, size_t index)
{
   static const char * const def[] =
    { "Reserved", "Reserved for local use", "Missing" };
   if (index < (n / sizeof (char *))) {
      return table[index];
   } else if (index < 192) {
      return def[0];
   } else if (index < 255) {
      return def[1];
   } else {
      return def[2];
   }
}

/*****************************************************************************
 * Print() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To print the message to a local static array in a way similar to
 * myerror.c::errSprintf.  This allows us to pass the results back to Tcl/Tk,
 * as well as to save it to disk.  It also serves as a central place to
 * change if we want a different style of output.
 *
 *   The caller gives a series of calls with fmt != NULL, followed by
 * a fmt == NULL.  This last call will return the constructed message to the
 * caller, and reset the message to NULL.  It is caller's responsibility to
 * free the message, and to make sure that last call to Print has fmt = NULL,
 * so that the routine doesn't accidentally keep memory.
 *
 * ARGUMENTS
 *   label = A label for this set of data. (Input)
 * varName = A char string describing the variable.. (Input)
 *     fmt = Prt_NULL, Prt_D, Prt_DS, Prt_DSS, Prt_S, Prt_F, Prt_FS, Prt_E,
 *           Prt_ES. (Input)
 *           determines what to expect in the rest of the arguments.
 *           d = sInt4, s = char *, f = double,
 *           NULL = return the constructed answer, and reset answer to NULL.
 *
 * FILES/DATABASES: None
 *
 * RETURNS: char *
 *   NULL if (fmt != NULL) (i.e. we added to message)
 *   message if (fmt == NULL) (i.e. return the message).
 *       It is caller's responsibility to free the message, and to make sure
 *       that last call to Print has fmt = NULL.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   4/2003 AAT: Changed so it could print different types of labels
 *               "GDS" instead of "S3"
 *  10/2004 AAT: Added Prt_SS
 *
 * NOTES
 * Using enumerated type instead of "ds" "dss" etc.  For speed considerations.
 *****************************************************************************
 */
char *Print(const char *label, const char *varName, Prt_TYPE fmt, ...)
{
   static char *buffer = NULL; /* Copy of message generated so far. */
   va_list ap;          /* pointer to variable argument list. */
   sInt4 lival;         /* Store a sInt4 val from argument list. */
   char *sval;          /* Store a string val from argument. */
   char *unit;          /* Second string val is usually a unit string. */
   double dval;         /* Store a double val from argument list. */
   char *ans;           /* Final message to return if fmt = Prt_NULL. */

   if (fmt == Prt_NULL) {
      ans = buffer;
      buffer = NULL;
      return ans;
   }
   va_start (ap, fmt);  /* make ap point to 1st unnamed arg. */
   switch (fmt) {
      case Prt_D:
         lival = va_arg (ap, sInt4);
         reallocSprintf (&buffer, "%s | %s | %ld\n", label, varName, lival);
         break;
      case Prt_DS:
         lival = va_arg (ap, sInt4);
         sval = va_arg (ap, char *);
         reallocSprintf (&buffer, "%s | %s | %ld (%s)\n", label, varName,
                         lival, sval);
         break;
      case Prt_DSS:
         lival = va_arg (ap, sInt4);
         sval = va_arg (ap, char *);
         unit = va_arg (ap, char *);
         reallocSprintf (&buffer, "%s | %s | %ld (%s [%s])\n", label,
                         varName, lival, sval, unit);
         break;
      case Prt_S:
         sval = va_arg (ap, char *);
         reallocSprintf (&buffer, "%s | %s | %s\n", label, varName, sval);
         break;
      case Prt_SS:
         sval = va_arg (ap, char *);
         unit = va_arg (ap, char *);
         reallocSprintf (&buffer, "%s | %s | %s (%s)\n", label, varName,
                         sval, unit);
         break;
      case Prt_F:
         dval = va_arg (ap, double);
         reallocSprintf (&buffer, "%s | %s | %f\n", label, varName, dval);
         break;
      case Prt_E:
         dval = va_arg (ap, double);
         reallocSprintf (&buffer, "%s | %s | %e\n", label, varName, dval);
         break;
      case Prt_G:
         dval = va_arg (ap, double);
         reallocSprintf (&buffer, "%s | %s | %g\n", label, varName, dval);
         break;
      case Prt_FS:
         dval = va_arg (ap, double);
         unit = va_arg (ap, char *);
         reallocSprintf (&buffer, "%s | %s | %f (%s)\n", label, varName,
                         dval, unit);
         break;
      case Prt_ES:
         dval = va_arg (ap, double);
         unit = va_arg (ap, char *);
         reallocSprintf (&buffer, "%s | %s | %e (%s)\n", label, varName,
                         dval, unit);
         break;
      case Prt_GS:
         dval = va_arg (ap, double);
         unit = va_arg (ap, char *);
         reallocSprintf (&buffer, "%s | %s | %g (%s)\n", label, varName,
                         dval, unit);
         break;
      default:
         reallocSprintf (&buffer, "ERROR: Invalid Print option '%d'\n", fmt);
   }
   va_end (ap);         /* clean up when done. */
   return NULL;
}

/*****************************************************************************
 * PrintSect1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the message for GRIB2 section 1.
 *
 * ARGUMENTS
 *      pds2 = The GRIB2 Product Definition Section to print. (Input)
 *    center = The Center that created the data (Input)
 * subcenter = The Sub Center that created the data (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   4/2003 AAT: Changed to accept pointer to pdsG2Type pds2
 *  10/2005 AAT: Adjusted to take center, subcenter as we moved that out of
 *               the pdsG2 type.
 *
 * NOTES
 *****************************************************************************
 */
static void PrintSect1 (pdsG2Type * pds2, unsigned short int center,
                        unsigned short int subcenter)
{
   /* Based on Grib2 Code Table 1.2 */
   static const char * const table12[] = { "Analysis", "Start of Forecast",
      "Verifying time of forecast", "Observation time"
   };

   /* Based on Grib2 Code Table 1.3 */
   static const char * const table13[] = { "Operational products",
      "Operational test products", "Research products",
      "Re-analysis products"
   };

   /* Based on Grib2 Code Table 1.4 */
   static const char * const table14[] = { "Analysis products",
      "Forecast products", "Analysis and forecast products",
      "Control forecast products", "Perturbed forecast products",
      "Control and perturbed forecast products",
      "Processed satellite observations", "Processed radar observations"
   };

   char buffer[25];     /* Stores format of pds2->refTime. */
   const char *ptr;

   ptr = centerLookup (center);
   if (ptr != NULL) {
      Print ("PDS-S1", "Originating center", Prt_DS, center, ptr);
   } else {
      Print ("PDS-S1", "Originating center", Prt_D, center);
   }
   if (subcenter != GRIB2MISSING_u2) {
      ptr = subCenterLookup (center, subcenter);
      if (ptr != NULL) {
         Print ("PDS-S1", "Originating sub-center", Prt_DS, subcenter, ptr);
      } else {
         Print ("PDS-S1", "Originating sub-center", Prt_D, subcenter);
      }
   }
   Print ("PDS-S1", "GRIB Master Tables Version", Prt_D, pds2->mstrVersion);
   Print ("PDS-S1", "GRIB Local Tables Version", Prt_D, pds2->lclVersion);
   Print ("PDS-S1", "Significance of reference time", Prt_DS, pds2->sigTime,
          Lookup (table12, sizeof (table12), pds2->sigTime));

/*   strftime (buffer, 25, "%m/%d/%Y %H:%M:%S UTC", gmtime (&(pds2->refTime)));*/
   Clock_Print (buffer, 25, pds2->refTime, "%m/%d/%Y %H:%M:%S UTC", 0);

   Print ("PDS-S1", "Reference Time", Prt_S, buffer);
   Print ("PDS-S1", "Operational Status", Prt_DS, pds2->operStatus,
          Lookup (table13, sizeof (table13), pds2->operStatus));
   Print ("PDS-S1", "Type of Data", Prt_DS, pds2->dataType,
          Lookup (table14, sizeof (table14), pds2->dataType));
}

/*****************************************************************************
 * PrintSect2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate a message for the section 2 data.  This may be more
 * appropriate in its own file (particularly the weather table.)
 *
 * ARGUMENTS
 * sect2 = The sect2 structure to print (initialized by ParseSect2). (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   2/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static void PrintSect2 (sect2_type * sect2)
{
   size_t i;            /* loop counter over number of sect2 data. */
   char buffer[25];     /* Assists with labeling. */

   switch (sect2->ptrType) {
      case GS2_WXTYPE:
         Print ("PDS-S2", "Number of Elements in Section 2", Prt_D,
                sect2->wx.dataLen);
         for (i = 0; i < sect2->wx.dataLen; i++) {
            if (sect2->wx.ugly[i].validIndex != -1) {
                sprintf (buffer, "Elem %3d  Is Used", (int) i);
            } else {
                sprintf (buffer, "Elem %3d NOT Used", (int) i);
            }
            Print ("PDS-S2", buffer, Prt_S, sect2->wx.data[i]);
         }
         break;
      case GS2_UNKNOWN:
         Print ("PDS-S2", "Number of Elements in Section 2", Prt_D,
                sect2->unknown.dataLen);
         for (i = 0; i < sect2->unknown.dataLen; i++) {
             sprintf (buffer, "Element %d", (int) i);
            Print ("PDS-S2", buffer, Prt_F, sect2->unknown.data[i]);
         }
         break;
      default:
         return;
   }
}

/*****************************************************************************
 * PrintSect4_Category() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the category message for section 4.
 *
 * ARGUMENTS
 * meta = The meta file structure to generate the message for. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   6/2003 Arthur Taylor (MDL/RSIS): Extracted this part from PrintSect4().
 *   1/2004 AAT: Combined PrintSect4_Meteo and PrintSect4_Ocean into this.
 *   5/2004 AAT: Found out that I skipped "Momentum Probabilities" in tbl41_0.
 *
 * NOTES
 *****************************************************************************
 */
static void PrintSect4_Category (grib_MetaData *meta)
{
   sect4_type *sect4 = &(meta->pds2.sect4);

   /* Based on Grib2 Code Table 4.1 discipline 0 */
   static const char * const tbl41_0[] = {
      "Temperature", "Moisture", "Momentum", "Mass", "Short-wave Radiation",
      "Long-wave Radiation", "Cloud", "Thermodynamic Stability indices",
      "Kinematic Stability indices", "Temperature Probabilities",
      "Moisture Probabilities", "Momentum Probabilities",
      "Mass Probabilities", "Aerosols", "Trace gases (e.g. ozone, C02)",
      "Radar", "Forecast Radar Imagery", "Electro-dynamics",
      "Nuclear/radiology", "Physical atmospheric properties"
   };
   /* Based on Grib2 Code Table 4.1 discipline 1 */
   static const char * const tbl41_1[] = {
      "Hydrology basic products", "Hydrology probabilities"
   };
   /* Based on Grib2 Code Table 4.1 discipline 2 */
   static const char * const tbl41_2[] = {
      "Vegetation/Biomass", "Agri-/aquacultural Special Products",
      "Transportation-related Products", "Soil Products"
   };
   /* Based on Grib2 Code Table 4.1 discipline 3 */
   static const char * const tbl41_3[] = {
      "Image format products", "Quantitative products"
   };
   /* Based on Grib2 Code Table 4.1 discipline 10 */
   static const char * const tbl41_10[] = {
      "Waves", "Currents", "Ice", "Surface Properties",
      "Sub-surface Properties"
   };

   switch (meta->pds2.prodType) {
      case 0:          /* Meteo category. */
         switch (sect4->cat) {
            case 190:
               Print ("PDS-S4", "Category Description", Prt_DS, sect4->cat,
                      "CCITT IA5 string");
               break;
            case 191:
               Print ("PDS-S4", "Category Description", Prt_DS, sect4->cat,
                      "Miscellaneous");
               break;
            default:
               Print ("PDS-S4", "Category Description", Prt_DS, sect4->cat,
                      Lookup (tbl41_0, sizeof (tbl41_0), sect4->cat));
         }
         break;
      case 1:          /* Hydrological */
         Print ("PDS-S4", "Category Description", Prt_DS, sect4->cat,
                Lookup (tbl41_1, sizeof (tbl41_1), sect4->cat));
         break;
      case 2:          /* Land surface */
         Print ("PDS-S4", "Category Description", Prt_DS, sect4->cat,
                Lookup (tbl41_2, sizeof (tbl41_2), sect4->cat));
         break;
      case 3:          /* Space */
         Print ("PDS-S4", "Category Description", Prt_DS, sect4->cat,
                Lookup (tbl41_3, sizeof (tbl41_3), sect4->cat));
         break;
      case 10:         /* Oceanographic */
         Print ("PDS-S4", "Category Description", Prt_DS, sect4->cat,
                Lookup (tbl41_10, sizeof (tbl41_10), sect4->cat));
         break;
      default:
         Print ("PDS-S4", "PrintSect4() does not handle this prodType",
                Prt_D, meta->pds2.prodType);
   }
}

/*****************************************************************************
 * PrintSect4() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the message for section 4.
 *
 * ARGUMENTS
 *   meta = The meta file structure to generate the message for. (Input)
 * f_unit = 0 (GRIB unit), 1 (english), 2 (metric) (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 if no error.
 * -2 if asked to print data for a template that we don't support.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   2/2003 AAT: Adjusted the interpretation of the scale vector and value.
 *          to be consistent with what Matt found from email conversations
 *          with WMO GRIB2 experts.
 *   2/2003 AAT: Switched from: value / pow (10, factor)
 *                          to: value * pow (10, -1 * factor)
 *   6/2003 AAT: Extracted the prodType = 0 only info and put in _Meteo
 *   6/2003 AAT: Created a PrintSect4_Ocean for ocean stuff.
 *   1/2004 AAT: Updated table 4.7
 *   1/2004 AAT: Modified to use "comment" from metaname.c instead of
 *          PrintSect4_Meteo, and PrintSect4_Ocean (that way local tables are
 *          enabled.) (Fixed meta file for PoP12).
 *   3/2004 AAT: Added emphasis on "computation" of unit conversion.
 *   3/2005 AAT: Added support for GS4_PROBABIL_PNT
 *
 * NOTES
 * Need to add support for GS4_RADAR = 20
 *****************************************************************************
 */
static int PrintSect4 (grib_MetaData *meta, sChar f_unit)
{
   sect4_type *sect4 = &(meta->pds2.sect4);
   /* Based on Grib2 Code Table 4.0 */
   static const char * const tbl40[] = {
      "Analysis at a horizontal layer at a point in time",
      "Individual ensemble forecast at a horizontal layer at a point in time",
      "Derived forecast based on ensemble members at a horizontal layer at a"
            " point in time",
      "Probability forecast at a horizontal layer or level at a point in "
            "time",
      "Statistically processed data at a horizontal layer or level in a time"
            " interval",
      "Probability forecast at a horizontal layer or level in a time "
            "interval",
      "Percentile forecasts at a horizontal layer or level in a time "
            "interval",
      "Individual ensemble forecast at a horizontal layer or level in a time"
            " interval",
      "Derived forecasts based in all ensemble members at a horizontal level "
            "or layer in a time interval",
      "Radar product", "Satellite product"
   };

   /* Based on Grib2 Code Table 4.3 */
   static const char * const tbl43[] = {
      "Analysis", "Initialization", "Forecast", "Bias corrected forecast",
      "Ensemble forecast", "Probability forecast", "Forecast error",
      "Analysis error", "Observation"
   };

   /* Based on Grib2 Code Table 4.4 */
   static const char * const tbl44[] = {
      "Minute", "Hour", "Day", "Month", "Year", "Decade",
      "Normal (30 years)", "Century", "Reserved", "Reserved",
      "3 hours", "6 hours", "12 hours", "Second"
   };

   /* Based on Grib2 Code Table 4.5 */
   /* See "metaname.c :: Surface[]" */

   /* Based on Grib2 Code Table 4.6 */
   static const char * const tbl46[] = {
      "Unperturbed high-resolution control forecast",
      "Unperturbed low-resolution control forecast",
      "Negatively perturbed forecast", "Positively perturbed forecast"
   };

   /* Based on Grib2 Code Table 4.7 */
   static const char * const tbl47[] = {
      "Unweighted mean of all members", "Weighted mean of all members",
      "Standard deviation with respect to cluster mean",
      "Standard deviation with respect to cluster mean, normalized",
      "Spread of all members",
      "Large anomaly index of all members",
      "Unweighted mean of the cluster members"
   };

   /* Based on Grib2 Code Table 4.9 */
   static const char * const tbl49[] = {
      "Probability of event below lower limit",
      "Probability of event above upper limit",
      "Probability of event between limits (include lower, exclude upper)",
      "Probability of event above lower limit",
      "Probability of event below upper limit"
   };

   /* Based on Grib2 Code Table 4.10 */
   static const char * const tbl410[] = {
      "Average", "Accumulation", "Maximum", "Minimum",
      "Difference (Value at end of time minus beginning)",
      "Root mean square", "Standard deviation",
      "Covariance (Temporal variance)",
      "Difference (Value at beginning of time minus end)", "Ratio"
   };

   /* Based on Grib2 Code Table 4.11 */
   static const char * const tbl411[] = {
      "Reserved",
      "Successive times; same forecast time, start time incremented",
      "Successive times; same start time, forecast time incremented",
      "Successive times; start time incremented, forecast time decremented, "
            "valid time constant",
      "Successive times; start time decremented, forecast time incremented, "
            "valid time constant",
      "Floating subinterval of time between forecast time, and end"
   };

   char buffer[50];     /* Temp storage for various uses including time
                         * format. */
   int i;               /* counter for templat 4.8/4.9 for num time range
                         * specs. */
   int f_reserved;      /* Whether Table4.5 is a reserved entry or not. */
   GRIB2SurfTable surf; /* Surface look up in Table4.5. */
   const char *ptr;

   switch (sect4->templat) {
      case GS4_ANALYSIS:
      case GS4_ENSEMBLE:
      case GS4_DERIVED:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat,
                tbl40[sect4->templat]);
         break;
      case GS4_PROBABIL_PNT:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[3]);
         break;
      case GS4_STATISTIC:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[4]);
         break;
      case GS4_PROBABIL_TIME:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[5]);
         break;
      case GS4_PERCENTILE:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[6]);
         break;
      case GS4_ENSEMBLE_STAT:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[7]);
         break;
      case GS4_DERIVED_INTERVAL:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[8]);
         break;
/*
 * The following lines were removed until such time that the rest of this
 * procedure can properly handle this template type.
 *
      case GS4_RADAR:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[9]);
         break;
*/
      case GS4_SATELLITE:
         Print ("PDS-S4", "Product type", Prt_DS, sect4->templat, tbl40[10]);
         break;
      default:
         Print ("PDS-S4", "Product type", Prt_D, sect4->templat);
         errSprintf ("Un-supported Sect4 template %ld\n", sect4->templat);
         return -2;
   }

   PrintSect4_Category (meta);
   Print ("PDS-S4", "Category Sub-Description", Prt_DS, sect4->subcat,
          meta->comment);

   if (f_unit == 1) {
      Print ("PDS-S4", "Output grid, (COMPUTED) english unit is", Prt_S,
             meta->unitName);
   } else if (f_unit == 2) {
      Print ("PDS-S4", "Output grid, (COMPUTED) metric unit is", Prt_S,
             meta->unitName);
   }
   Print ("PDS-S4", "Generation process", Prt_DS, sect4->genProcess,
          Lookup (tbl43, sizeof (tbl43), sect4->genProcess));
   if (sect4->templat == GS4_SATELLITE) {
      Print ("PDS-S4", "Observation generating process", Prt_D, sect4->genID);
      Print ("PDS-S4", "Number of contributing spectral bands", Prt_D,
             sect4->numBands);
      for (i = 0; i < sect4->numBands; i++) {
         Print ("PDS-S4", "Satellite series", Prt_D, sect4->bands[i].series);
         Print ("PDS-S4", "Satellite numbers", Prt_D,
                sect4->bands[i].numbers);
         Print ("PDS-S4", "Instrument type", Prt_D, sect4->bands[i].instType);
         Print ("PDS-S4", "Scale Factor of central wave number", Prt_D,
                sect4->bands[i].centWaveNum.factor);
         Print ("PDS-S4", "Scale Value of central wave number", Prt_D,
                sect4->bands[i].centWaveNum.value);
      }
      return 0;
   }
   if (sect4->bgGenID != GRIB2MISSING_u1) {
      ptr = processLookup (meta->center, sect4->bgGenID);
      if (ptr != NULL) {
         Print ("PDS-S4", "Background generating process ID", Prt_DS,
                sect4->bgGenID, ptr);
      } else {
         Print ("PDS-S4", "Background generating process ID", Prt_D,
                sect4->bgGenID);
      }
   }
   if (sect4->genID != GRIB2MISSING_u1) {
      ptr = processLookup (meta->center, sect4->genID);
      if (ptr != NULL) {
         Print ("PDS-S4", "Forecast generating process ID", Prt_DS,
                sect4->genID, ptr);
      } else {
         Print ("PDS-S4", "Forecast generating process ID", Prt_D,
                sect4->genID);
      }
   }
   if (sect4->f_validCutOff) {
      Print ("PDS-S4", "Data cut off after reference time in seconds", Prt_D,
             sect4->cutOff);
   }
   Print ("PDS-S4", "Forecast time in hours", Prt_F,
          (double) (sect4->foreSec / 3600.));
   surf = Table45Index (sect4->fstSurfType, &f_reserved, meta->center,
                        meta->subcenter);
   Print ("PDS-S4", "Type of first fixed surface", Prt_DSS,
          sect4->fstSurfType, surf.comment, surf.unit);
   Print ("PDS-S4", "Value of first fixed surface", Prt_F,
          sect4->fstSurfValue);
   if (sect4->sndSurfType != GRIB2MISSING_u1) {
      surf = Table45Index (sect4->sndSurfType, &f_reserved, meta->center,
                           meta->subcenter);
      Print ("PDS-S4", "Type of second fixed surface", Prt_DSS,
             sect4->sndSurfType, surf.comment, surf.unit);
      Print ("PDS-S4", "Value of second fixed surface", Prt_F,
             sect4->sndSurfValue);
   }
   switch (sect4->templat) {
      case GS4_ANALYSIS:
         break;
      case GS4_ENSEMBLE:
         Print ("PDS-S4", "Type of Ensemble forecast", Prt_DS,
                sect4->typeEnsemble,
                Lookup (tbl46, sizeof (tbl46), sect4->typeEnsemble));
         Print ("PDS-S4", "Perturbation number", Prt_D, sect4->perturbNum);
         Print ("PDS-S4", "Number of forecasts in ensemble", Prt_D,
                sect4->numberFcsts);
         break;
      case GS4_ENSEMBLE_STAT:
         Print ("PDS-S4", "Type of Ensemble forecast", Prt_DS,
                sect4->typeEnsemble,
                Lookup (tbl46, sizeof (tbl46), sect4->typeEnsemble));
         Print ("PDS-S4", "Perturbation number", Prt_D, sect4->perturbNum);
         Print ("PDS-S4", "Number of forecasts in ensemble", Prt_D,
                sect4->numberFcsts);
         Clock_Print (buffer, 100, sect4->validTime, "%m/%d/%Y %H:%M:%S UTC",
                      0);
         Print ("PDS-S4", "End of overall time interval", Prt_S, buffer);
         Print ("PDS-S4", "Total number of missing values", Prt_D,
                sect4->numMissing);
         Print ("PDS-S4", "Number of time range specifications", Prt_D,
                sect4->numInterval);
         for (i = 0; i < sect4->numInterval; i++) {
            Print ("PDS-S4", "Interval number", Prt_D, i + 1);
            Print ("PDS-S4", "Statistical process", Prt_DS,
                   sect4->Interval[i].processID,
                   Lookup (tbl410, sizeof (tbl410),
                           sect4->Interval[i].processID));
            Print ("PDS-S4", "Type of time increment", Prt_DS,
                   sect4->Interval[i].incrType,
                   Lookup (tbl411, sizeof (tbl411),
                           sect4->Interval[i].incrType));
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].lenTime,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].timeRangeUnit));
            Print ("PDS-S4", "Time range for processing", Prt_S, buffer);
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].timeIncr,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].incrUnit));
            Print ("PDS-S4", "Time increment", Prt_S, buffer);
         }
         break;
      case GS4_DERIVED:
         Print ("PDS-S4", "Derived forecast", Prt_DS, sect4->derivedFcst,
                Lookup (tbl47, sizeof (tbl47), sect4->derivedFcst));
         Print ("PDS-S4", "Number of forecasts in ensemble", Prt_D,
                sect4->numberFcsts);
         break;
      case GS4_DERIVED_INTERVAL:
         Print ("PDS-S4", "Derived forecast", Prt_DS, sect4->derivedFcst,
                Lookup (tbl47, sizeof (tbl47), sect4->derivedFcst));
         Print ("PDS-S4", "Number of forecasts in ensemble", Prt_D,
                sect4->numberFcsts);
         Clock_Print (buffer, 100, sect4->validTime, "%m/%d/%Y %H:%M:%S UTC",
                      0);

         Print ("PDS-S4", "End of overall time interval", Prt_S, buffer);
         Print ("PDS-S4", "Total number of missing values", Prt_D,
                sect4->numMissing);
         Print ("PDS-S4", "Number of time range specifications", Prt_D,
                sect4->numInterval);
         for (i = 0; i < sect4->numInterval; i++) {
            Print ("PDS-S4", "Interval number", Prt_D, i + 1);
            Print ("PDS-S4", "Statistical process", Prt_DS,
                   sect4->Interval[i].processID,
                   Lookup (tbl410, sizeof (tbl410),
                           sect4->Interval[i].processID));
            Print ("PDS-S4", "Type of time increment", Prt_DS,
                   sect4->Interval[i].incrType,
                   Lookup (tbl411, sizeof (tbl411),
                           sect4->Interval[i].incrType));
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].lenTime,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].timeRangeUnit));
            Print ("PDS-S4", "Time range for processing", Prt_S, buffer);
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].timeIncr,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].incrUnit));
            Print ("PDS-S4", "Time increment", Prt_S, buffer);
         }
         break;
      case GS4_PROBABIL_PNT:
         Print ("PDS-S4", "Forecast Probability Number", Prt_D,
                sect4->foreProbNum);
         Print ("PDS-S4", "Total Number of Forecast Probabilities", Prt_D,
                sect4->numForeProbs);
         Print ("PDS-S4", "Probability type", Prt_DS, sect4->probType,
                Lookup (tbl49, sizeof (tbl49), sect4->probType));
         sprintf (buffer, "%d, %d", sect4->lowerLimit.value,
                  sect4->lowerLimit.factor);
         Print ("PDS-S4", "Lower limit (scale value, scale factor)", Prt_GS,
                sect4->lowerLimit.value *
                pow (10.0, -1 * sect4->lowerLimit.factor), buffer);
         sprintf (buffer, "%d, %d", sect4->upperLimit.value,
                  sect4->upperLimit.factor);
         Print ("PDS-S4", "Upper limit (scale value, scale factor)", Prt_GS,
                sect4->upperLimit.value *
                pow (10.0, -1 * sect4->upperLimit.factor), buffer);
/*         printf ("Hello world 1\n");*/
         break;
      case GS4_PERCENTILE:
         Print ("PDS-S4", "Percentile", Prt_DS, sect4->percentile, "[%]");
/*         strftime (buffer, 100, "%m/%d/%Y %H:%M:%S UTC",
                   gmtime (&(sect4->validTime)));*/
         Clock_Print (buffer, 100, sect4->validTime, "%m/%d/%Y %H:%M:%S UTC",
                      0);

         Print ("PDS-S4", "End of overall time interval", Prt_S, buffer);
         Print ("PDS-S4", "Total number of missing values", Prt_D,
                sect4->numMissing);
         Print ("PDS-S4", "Number of time range specifications", Prt_D,
                sect4->numInterval);
         for (i = 0; i < sect4->numInterval; i++) {
            Print ("PDS-S4", "Interval number", Prt_D, i + 1);
            Print ("PDS-S4", "Statistical process", Prt_DS,
                   sect4->Interval[i].processID,
                   Lookup (tbl410, sizeof (tbl410),
                           sect4->Interval[i].processID));
            Print ("PDS-S4", "Type of time increment", Prt_DS,
                   sect4->Interval[i].incrType,
                   Lookup (tbl411, sizeof (tbl411),
                           sect4->Interval[i].incrType));
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].lenTime,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].timeRangeUnit));
            Print ("PDS-S4", "Time range for processing", Prt_S, buffer);
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].timeIncr,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].incrUnit));
            Print ("PDS-S4", "Time increment", Prt_S, buffer);
         }
         break;
      case GS4_PROBABIL_TIME:
         Print ("PDS-S4", "Forecast Probability Number", Prt_D,
                sect4->foreProbNum);
         Print ("PDS-S4", "Total Number of Forecast Probabilities", Prt_D,
                sect4->numForeProbs);
         Print ("PDS-S4", "Probability type", Prt_DS, sect4->probType,
                Lookup (tbl49, sizeof (tbl49), sect4->probType));
         sprintf (buffer, "%d, %d", sect4->lowerLimit.value,
                  sect4->lowerLimit.factor);
         Print ("PDS-S4", "Lower limit (scale value, scale factor)", Prt_GS,
                sect4->lowerLimit.value *
                pow (10.0, -1 * sect4->lowerLimit.factor), buffer);
         sprintf (buffer, "%d, %d", sect4->upperLimit.value,
                  sect4->upperLimit.factor);
         Print ("PDS-S4", "Upper limit (scale value, scale factor)", Prt_GS,
                sect4->upperLimit.value *
                pow (10.0, -1 * sect4->upperLimit.factor), buffer);
         /* Intentionally fall through. */
         // CPL_FALLTHROUGH
      case GS4_STATISTIC:
/*         strftime (buffer, 100, "%m/%d/%Y %H:%M:%S UTC",
                   gmtime (&(sect4->validTime)));*/
         Clock_Print (buffer, 100, sect4->validTime, "%m/%d/%Y %H:%M:%S UTC",
                      0);

         Print ("PDS-S4", "End of overall time interval", Prt_S, buffer);
         Print ("PDS-S4", "Total number of missing values", Prt_D,
                sect4->numMissing);
         Print ("PDS-S4", "Number of time range specifications", Prt_D,
                sect4->numInterval);
         for (i = 0; i < sect4->numInterval; i++) {
            Print ("PDS-S4", "Interval number", Prt_D, i + 1);
            Print ("PDS-S4", "Statistical process", Prt_DS,
                   sect4->Interval[i].processID,
                   Lookup (tbl410, sizeof (tbl410),
                           sect4->Interval[i].processID));
            Print ("PDS-S4", "Type of time increment", Prt_DS,
                   sect4->Interval[i].incrType,
                   Lookup (tbl411, sizeof (tbl411),
                           sect4->Interval[i].incrType));
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].lenTime,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].timeRangeUnit));
            Print ("PDS-S4", "Time range for processing", Prt_S, buffer);
            /* Following is so we get "# str" not "# (str)" */
            sprintf (buffer, "%d %s", sect4->Interval[i].timeIncr,
                     Lookup (tbl44, sizeof (tbl44),
                             sect4->Interval[i].incrUnit));
            Print ("PDS-S4", "Time increment", Prt_S, buffer);
         }
         break;
      default:
         /* This case should have been handled in first switch statement of
          * this procedure, but just in case... */
         errSprintf ("Un-supported Sect4 template %d\n", sect4->templat);
         return -2;
   }
   return 0;
}

/*****************************************************************************
 * PrintPDS2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the message for the Product Definition Sections of the GRIB2
 * Message.
 *
 * ARGUMENTS
 *   meta = The meta file structure to generate the message for. (Input)
 * f_unit = 0 (GRIB unit), 1 (english), 2 (metric) (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int PrintPDS2 (grib_MetaData *meta, sChar f_unit)
{
   pdsG2Type *pds2 = &(meta->pds2);
   /* Based on Grib2 Code Table 0.0 */
   static const char * const table0[] = {
      "Meteorological products", "Hydrological products",
      "Land surface products", "Space products", "Oceanographic products"
   };
   int ierr;            /* The error code of a called routine */

   /* Print the data from Section 0 */
   switch (pds2->prodType) {
      case 10:         /* Oceanographic Product. */
         Print ("PDS-S0", "DataType", Prt_DS, pds2->prodType, table0[4]);
         break;
      case 5:          /* Reserved. */
         Print ("PDS-S0", "DataType", Prt_DS, pds2->prodType,
                Lookup (table0, sizeof (table0), 191));
         break;
      default:
         Print ("PDS-S0", "DataType", Prt_DS, pds2->prodType,
                Lookup (table0, sizeof (table0), pds2->prodType));
   }
   PrintSect1 (pds2, meta->center, meta->subcenter);
   PrintSect2 (&(pds2->sect2));
   if ((ierr = PrintSect4 (meta, f_unit)) != 0) {
      return ierr;
   }
   return 0;
}

/*****************************************************************************
 * PrintPDS1() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the message for the Product Definition Sections of the GRIB1
 * Message.
 *
 * ARGUMENTS
 *      pds1 = The GRIB1 Product Definition Section to print. (Input)
 *   comment = A description about this element. See GRIB1_Table2LookUp (Input)
 *    center = The Center that created the data (Input)
 * subcenter = The Sub Center that created the data (Input)
 *    f_unit = The unit conversion method used on the output data (Input)
 *  unitName = The name of the output unit type. (Input)
 *   convert = Conversion method used. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created.
 *  10/2005 AAT: Adjusted to take center, subcenter as we moved that out of
 *               the pdsG1 type.
 *  11/2005 AAT: Added f_unit variable.
 *
 * NOTES
 *****************************************************************************
 */
static void PrintPDS1 (pdsG1Type *pds1, char *comment,
                       unsigned short int center,
                       unsigned short int subcenter, sChar f_unit,
                       char *unitName, int convert)
{
   char buffer[25];     /* Stores format of pds1->refTime. */
   const char *ptr;

   Print ("PDS-S1", "Parameter Tables Version", Prt_D, pds1->mstrVersion);
   ptr = centerLookup (center);
   if (ptr != NULL) {
      Print ("PDS-S1", "Originating center", Prt_DS, center, ptr);
   } else {
      Print ("PDS-S1", "Originating center", Prt_D, center);
   }
   ptr = subCenterLookup (center, subcenter);
   if (ptr != NULL) {
      Print ("PDS-S1", "Originating sub-center", Prt_DS, subcenter, ptr);
   } else {
      Print ("PDS-S1", "Originating sub-center", Prt_D, subcenter);
   }
   ptr = processLookup (center, pds1->genProcess);
   if (ptr != NULL) {
      Print ("PDS-S1", "Generation process", Prt_DS, pds1->genProcess, ptr);
   } else {
      Print ("PDS-S1", "Generation process", Prt_D, pds1->genProcess);
   }
   Print ("PDS-S1", "Grid Identification Number", Prt_D, pds1->gridID);
   Print ("PDS-S1", "Indicator of parameter and units", Prt_DS, pds1->cat,
          comment);
   if (convert != UC_NONE) {
      if (f_unit == 1) {
         Print ("PDS-S1", "Output grid, (COMPUTED) english unit is", Prt_S,
                unitName);
      } else if (f_unit == 2) {
         Print ("PDS-S1", "Output grid, (COMPUTED) metric unit is", Prt_S,
                unitName);
      }
   }
   Print ("PDS-S1", "Type of fixed surface", Prt_D, pds1->levelType);
   Print ("PDS-S1", "Value of fixed surface", Prt_D, pds1->levelVal);

/* strftime (buffer, 25, "%m/%d/%Y %H:%M:%S UTC", gmtime (&(pds1->refTime))); */
   Clock_Print (buffer, 25, pds1->refTime, "%m/%d/%Y %H:%M:%S UTC", 0);

   Print ("PDS-S1", "Reference Time", Prt_S, buffer);

/* strftime (buffer, 25, "%m/%d/%Y %H:%M:%S UTC",
             gmtime (&(pds1->validTime))); */
   Clock_Print (buffer, 25, pds1->validTime, "%m/%d/%Y %H:%M:%S UTC", 0);

   Print ("PDS-S1", "Valid Time", Prt_S, buffer);

/* strftime (buffer, 25, "%m/%d/%Y %H:%M:%S UTC", gmtime (&(pds1->P1))); */
   Clock_Print (buffer, 25, pds1->P1, "%m/%d/%Y %H:%M:%S UTC", 0);

   Print ("PDS-S1", "P1 Time", Prt_S, buffer);

/* strftime (buffer, 25, "%m/%d/%Y %H:%M:%S UTC", gmtime (&(pds1->P2))); */
   Clock_Print (buffer, 25, pds1->P2, "%m/%d/%Y %H:%M:%S UTC", 0);

   Print ("PDS-S1", "P2 Time", Prt_S, buffer);
   Print ("PDS-S1", "Time range indicator", Prt_D, pds1->timeRange);
   Print ("PDS-S1", "Number included in average", Prt_D, pds1->Average);
   Print ("PDS-S1", "Number missing from average or accumulation", Prt_D,
          pds1->numberMissing);

   if (pds1->f_hasEns) {
      Print ("PDS-S1", "Ensemble BitFlag (octet 29)", Prt_D,
             pds1->ens.BitFlag);
      Print ("PDS-S1", "Ensemble Application", Prt_D, pds1->ens.Application);
      Print ("PDS-S1", "Ensemble Type", Prt_D, pds1->ens.Type);
      Print ("PDS-S1", "Ensemble Number", Prt_D, pds1->ens.Number);
      Print ("PDS-S1", "Ensemble ProdID", Prt_D, pds1->ens.ProdID);
      Print ("PDS-S1", "Ensemble Smoothing", Prt_D, pds1->ens.Smooth);
   }
   if (pds1->f_hasProb) {
      Print ("PDS-S1", "Prob Category", Prt_D, pds1->prob.Cat);
      Print ("PDS-S1", "Prob Type", Prt_D, pds1->prob.Type);
      Print ("PDS-S1", "Prob lower", Prt_F, pds1->prob.lower);
      Print ("PDS-S1", "Prob upper", Prt_F, pds1->prob.upper);
   }
   if (pds1->f_hasCluster) {
      Print ("PDS-S1", "Cluster Ens Size", Prt_D, pds1->cluster.ensSize);
      Print ("PDS-S1", "Cluster Size", Prt_D, pds1->cluster.clusterSize);
      Print ("PDS-S1", "Cluster Number", Prt_D, pds1->cluster.Num);
      Print ("PDS-S1", "Cluster Method", Prt_D, pds1->cluster.Method);
      Print ("PDS-S1", "Cluster North Latitude", Prt_F, pds1->cluster.NorLat);
      Print ("PDS-S1", "Cluster South Latitude", Prt_F, pds1->cluster.SouLat);
      Print ("PDS-S1", "Cluster East Longitude", Prt_F, pds1->cluster.EasLon);
      Print ("PDS-S1", "Cluster West Longitude", Prt_F, pds1->cluster.WesLon);
      sprintf (buffer, "'%10s'", pds1->cluster.Member);
      Print ("PDS-S1", "Cluster Membership", Prt_S, buffer);
   }
}

/*****************************************************************************
 * PrintGDS() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the message for the Grid Definition Section.
 *
 * ARGUMENTS
 *     gds = The gds structure to print. (Input)
 * version = The GRIB version number (so we know what type of projection) (In)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 if no error.
 * -1 if asked to print a map projection that we don't support.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   4/2003 AAT: Switched from sect3 to gds
 *   5/2003 AAT: Since the number for ProjectionType changed from GRIB1 to
 *          GRIB2, we use the GRIB2 internally, but for meta data we want to
 *          print the appropriate one.
 *   5/2003 AAT: Decided to have 1,1 be lower left corner in .shp files.
 *  10/2004 AAT: Added TDLP support.
 *
 * NOTES
 * Need to add support for GS3_ORTHOGRAPHIC = 90,
 * GS3_EQUATOR_EQUIDIST = 110, GS3_AZIMUTH_RANGE = 120
 *****************************************************************************
 */
static int PrintGDS (gdsType *gds, int version)
{
   /* Based on Grib2 Code Table 3.1 */
   static const char * const table31[] = { "Latitude/Longitude", "Mercator",
      "Polar Stereographic", "Lambert Conformal",
      "Space view perspective orthographic",
      "Equatorial azimuthal equidistant projection",
      "Azimuth-range projection"
   };
   char buffer[50];     /* Temporary storage for info about scan flag. */

   Print ("GDS", "Number of Points", Prt_D, gds->numPts);
   switch (gds->projType) {
      case GS3_LATLON: /* 0 */
         if (version == 1) {
            Print ("GDS", "Projection Type", Prt_DS, GB1S2_LATLON,
                   table31[0]);
         } else {
            Print ("GDS", "Projection Type", Prt_DS, gds->projType,
                   table31[0]);
         }
         break;
      case GS3_MERCATOR: /* 10 */
         if (version == 1) {
            Print ("GDS", "Projection Type", Prt_DS, GB1S2_MERCATOR,
                   table31[1]);
         } else if (version == -1) {
            Print ("GDS", "Projection Type", Prt_DS, TDLP_MERCATOR,
                   table31[1]);
         } else {
            Print ("GDS", "Projection Type", Prt_DS, gds->projType,
                   table31[1]);
         }
         break;
      case GS3_POLAR:  /* 20 */
         if (version == 1) {
            Print ("GDS", "Projection Type", Prt_DS, GB1S2_POLAR, table31[2]);
         } else if (version == -1) {
            Print ("GDS", "Projection Type", Prt_DS, TDLP_POLAR, table31[2]);
         } else {
            Print ("GDS", "Projection Type", Prt_DS, gds->projType,
                   table31[2]);
         }
         break;
      case GS3_LAMBERT: /* 30 */
         if (version == 1) {
            Print ("GDS", "Projection Type", Prt_DS, GB1S2_LAMBERT,
                   table31[3]);
         } else if (version == -1) {
            Print ("GDS", "Projection Type", Prt_DS, TDLP_LAMBERT,
                   table31[3]);
         } else {
            Print ("GDS", "Projection Type", Prt_DS, gds->projType,
                   table31[3]);
         }
         break;
/*
 * The following lines were removed until such time that the rest of this
 * procedure can properly handle these three projection types.
 *
      case GS3_ORTHOGRAPHIC:  * 90 *
         Print ("GDS", "Projection Type", Prt_DS, gds->projType, table31[4]);
         break;
      case GS3_EQUATOR_EQUIDIST:  * 110 *
         Print ("GDS", "Projection Type", Prt_DS, gds->projType, table31[5]);
         break;
      case GS3_AZIMUTH_RANGE:  * 120 *
         Print ("GDS", "Projection Type", Prt_DS, gds->projType, table31[6]);
         break;
*/
      default:
         Print ("GDS", "Projection Type", Prt_D, gds->projType);
         errSprintf ("Un-supported Map Projection %d\n", gds->projType);
         return -1;
   }
   if (gds->f_sphere) {
      Print ("GDS", "Shape of Earth", Prt_S, "sphere");
      Print ("GDS", "Radius", Prt_FS, gds->majEarth, "km");
   } else {
      Print ("GDS", "Shape of Earth", Prt_S, "oblate spheroid");
      Print ("GDS", "semi Major axis", Prt_FS, gds->majEarth, "km");
      Print ("GDS", "semi Minor axis", Prt_FS, gds->minEarth, "km");
   }
   Print ("GDS", "Nx (Number of points on parallel)", Prt_D, gds->Nx);
   Print ("GDS", "Ny (Number of points on meridian)", Prt_D, gds->Ny);
   Print ("GDS", "Lat1", Prt_F, gds->lat1);
   Print ("GDS", "Lon1", Prt_F, gds->lon1);
   if (gds->resFlag & GRIB2BIT_5) {
      Print ("GDS", "u/v vectors relative to", Prt_S, "grid");
   } else {
      Print ("GDS", "u/v vectors relative to", Prt_S, "easterly/northerly");
   }
   if (gds->projType == GS3_LATLON) {
      Print ("GDS", "Lat2", Prt_F, gds->lat2);
      Print ("GDS", "Lon2", Prt_F, gds->lon2);
      Print ("GDS", "Dx", Prt_FS, gds->Dx, "degrees");
      Print ("GDS", "Dy", Prt_FS, gds->Dy, "degrees");
   } else if (gds->projType == GS3_MERCATOR) {
      Print ("GDS", "Lat2", Prt_F, gds->lat2);
      Print ("GDS", "Lon2", Prt_F, gds->lon2);
      Print ("GDS", "Dx", Prt_FS, gds->Dx, "m");
      Print ("GDS", "Dy", Prt_FS, gds->Dy, "m");
   } else if ((gds->projType == GS3_POLAR)
              || (gds->projType == GS3_LAMBERT)) {
      Print ("GDS", "Dx", Prt_FS, gds->Dx, "m");
      Print ("GDS", "Dy", Prt_FS, gds->Dy, "m");
   }
   /* For scan mode... The user of this data doesn't necessarily care how it
    * was stored in the Grib2 grid (i.e. gds->scan), they just care about how
    * the data they are accessing is scanned (i.e. scan=0000) */
   sprintf (buffer, "%d%d%d%d", ((gds->scan & GRIB2BIT_1) / GRIB2BIT_1),
            ((gds->scan & GRIB2BIT_2) / GRIB2BIT_2),
            ((gds->scan & GRIB2BIT_3) / GRIB2BIT_3),
            ((gds->scan & GRIB2BIT_4) / GRIB2BIT_4));
   Print ("GDS", "Input GRIB2 grid, scan mode", Prt_DS, gds->scan, buffer);
/*
   Print ("GDS", "Output grid, scan mode", Prt_DS, 0, "0000");
   Print ("GDS", "Output grid, scan i/x direction", Prt_S, "positive");
   Print ("GDS", "Output grid, scan j/y direction", Prt_S, "negative");
*/
   Print ("GDS", "Output grid, scan mode", Prt_DS, 64, "0100");
   Print ("GDS", "(.flt file grid), scan mode", Prt_DS, 0, "0000");
   Print ("GDS", "Output grid, scan i/x direction", Prt_S, "positive");
   Print ("GDS", "Output grid, scan j/y direction", Prt_S, "positive");
   Print ("GDS", "(.flt file grid), scan j/y direction", Prt_S, "negative");
   Print ("GDS", "Output grid, consecutive points in", Prt_S,
          "i/x direction");
   Print ("GDS", "Output grid, adjacent rows scan in", Prt_S,
          "same direction");

   /* Meshlat/orient lon/scale lat have no meaning for lat/lon grids. */
   if (gds->projType != GS3_LATLON) {
      Print ("GDS", "MeshLat", Prt_F, gds->meshLat);
      Print ("GDS", "OrientLon", Prt_F, gds->orientLon);
      if ((gds->projType == GS3_POLAR) || (gds->projType == GS3_LAMBERT)) {
         if (gds->center & GRIB2BIT_1) {
            Print ("GDS", "Which pole is on the plane", Prt_S, "South");
         } else {
            Print ("GDS", "Which pole is on the plane", Prt_S, "North");
         }
         if (gds->center & GRIB2BIT_2) {
            Print ("GDS", "bi-polar projection", Prt_S, "Yes");
         } else {
            Print ("GDS", "bi-polar projection", Prt_S, "No");
         }
      }
      Print ("GDS", "Tangent Lat1", Prt_F, gds->scaleLat1);
      Print ("GDS", "Tangent Lat2", Prt_F, gds->scaleLat2);
      Print ("GDS", "Southern Lat", Prt_F, gds->southLat);
      Print ("GDS", "Southern Lon", Prt_F, gds->southLon);
   }
   return 0;
}

/*****************************************************************************
 * PrintGridAttrib() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the message for the various attributes of the grid.
 *
 * ARGUMENTS
 *  attrib = The Grid Attribute structure to print. (Input)
 * decimal = How many decimals to round to. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   5/2003 AAT: Added rounding to decimal.
 *
 * NOTES
 *****************************************************************************
 */
static void PrintGridAttrib (gridAttribType *attrib, sChar decimal)
{
   /* Based on Grib2 Code Table 5.0 */
   static const char * const table50[] = {
      "Grid point data - simple packing", "Matrix value - simple packing",
      "Grid point data - complex packing",
      "Grid point data - complex packing and spatial differencing"
   };

   /* Based on Grib2 Code Table 5.1 */
   static const char * const table51[] = { "Floating point", "Integer" };

   /* Based on Grib2 Code Table 5.5 */
   static const char * const table55[] = {
      "No explicit missing value included with data",
      "Primary missing value included with data",
      "Primary and Secondary missing values included with data"
   };

   if ((attrib->packType == GS5_JPEG2000) ||
       (attrib->packType == GS5_JPEG2000_ORG)) {
      Print ("Info", "Packing that was used", Prt_DS, attrib->packType,
             "JPEG 2000");
   } else if ((attrib->packType == GS5_PNG) ||
              (attrib->packType == GS5_PNG_ORG)) {
      Print ("Info", "Packing that was used", Prt_DS, attrib->packType,
             "Portable Network Graphics (PNG)");
   } else {
      Print ("Info", "Packing that was used", Prt_DS, attrib->packType,
             Lookup (table50, sizeof (table50), attrib->packType));
   }
   /* Added next two 1/27/2006 because of questions from Val. */
   Print ("Info", "Decimal Scale Factor", Prt_D, attrib->DSF);
   Print ("Info", "Binary Scale Factor", Prt_D, attrib->ESF);
   Print ("Info", "Original field type", Prt_DS, attrib->fieldType,
          Lookup (table51, sizeof (table51), attrib->fieldType));
   Print ("Info", "Missing value management", Prt_DS, attrib->f_miss,
          Lookup (table55, sizeof (table55), attrib->f_miss));
   if (attrib->f_miss == 1) {
      Print ("Info", "Primary missing value", Prt_F,
             myRound (attrib->missPri, decimal));
   } else if (attrib->f_miss == 2) {
      Print ("Info", "Primary missing value", Prt_F,
             myRound (attrib->missPri, decimal));
      Print ("Info", "Secondary missing value", Prt_F,
             myRound (attrib->missSec, decimal));
   }
   Print ("Info", "Detected number of Missing", Prt_D, attrib->numMiss);
   if (attrib->f_maxmin) {
      Print ("Info", "Field minimum value", Prt_F,
             myRound (attrib->min, decimal));
      Print ("Info", "Field maximum value", Prt_F,
             myRound (attrib->max, decimal));
   }
}

/*****************************************************************************
 * MetaPrintGDS() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate a message specific for the GDS.  Basically a wrapper for
 * PrintGDS and Print.
 *
 * ARGUMENTS
 * gds = The Grid Definition Section to generate the message for. (Input)
 * version = The GRIB version number (so we know what type of projection) (In)
 * ans = The resulting message. Up to caller to free. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 if no error.
 * -1 if asked to print a map projection that we don't support.
 * -2 if asked to print data for a template that we don't support.
 *
 * HISTORY
 *   4/2003 Arthur Taylor (MDL/RSIS): Created.
 *   5/2003 AAT: Commented out.  Purpose was mainly for debugging degrib1.c,
 *               which is now working.
 *
 * NOTES
 *****************************************************************************
 */
int MetaPrintGDS (gdsType *gds, int version, char **ans)
{
   int ierr;            /* The error code of a called routine */

   if ((ierr = PrintGDS (gds, version)) != 0) {
      *ans = Print (NULL, NULL, Prt_NULL);
      preErrSprintf ("Print error Section 3\n");
      return ierr;
   }
   *ans = Print (NULL, NULL, Prt_NULL);
   return 0;
}

/*****************************************************************************
 * MetaPrint() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To generate the meta file message.
 *
 * ARGUMENTS
 *    meta = The meta file structure to generate the message for. (Input)
 *     ans = The resulting message. Up to caller to free. (Output)
 * decimal = How many decimals to round to. (Input)
 *  f_unit = 0 (GRIB unit), 1 (english), 2 (metric) (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 if no error.
 * -1 if asked to print a map projection that we don't support.
 * -2 if asked to print data for a template that we don't support.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   5/2003 AAT: Added rounding to decimal.
 *
 * NOTES
 *****************************************************************************
 */
int MetaPrint (grib_MetaData *meta, char **ans, sChar decimal, sChar f_unit)
{
   int ierr;            /* The error code of a called routine */

   if (meta->GribVersion == 1) {
      PrintPDS1 (&(meta->pds1), meta->comment, meta->center,
                 meta->subcenter, f_unit, meta->unitName, meta->convert);
   } else if (meta->GribVersion == -1) {
      PrintPDS_TDLP (&(meta->pdsTdlp));
   } else {
      if ((ierr = PrintPDS2 (meta, f_unit)) != 0) {
         *ans = Print (NULL, NULL, Prt_NULL);
         preErrSprintf ("Print error in PDS for GRIB2\n");
         return ierr;
      }
   }
   if ((ierr = PrintGDS (&(meta->gds), meta->GribVersion)) != 0) {
      *ans = Print (NULL, NULL, Prt_NULL);
      preErrSprintf ("Print error Section 3\n");
      return ierr;
   }
   PrintGridAttrib (&(meta->gridAttrib), decimal);
   *ans = Print (NULL, NULL, Prt_NULL);
   return 0;
}

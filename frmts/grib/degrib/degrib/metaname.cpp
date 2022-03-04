/*****************************************************************************
 * metaname.c
 *
 * DESCRIPTION
 *    This file contains the code necessary to parse the GRIB2 product
 * definition information into human readable text.  In addition to the
 * tables in the GRIB2 specs, it also attempts to handle local table
 * definitions that NCEP and NDFD have developed.
 *
 * HISTORY
 *    1/2004 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#include <string.h>
#include <stdlib.h>
#include <limits>
#include "meta.h"
#include "metaname.h"
#include "myerror.h"
#include "myassert.h"
#include "myutil.h"

#include "cpl_port.h"
#include "cpl_csv.h"

static const char* GetGRIB2_CSVFilename(const char* pszFilename)
{
    const char* pszGribTableDirectory = CPLGetConfigOption("GRIB_RESOURCE_DIR", nullptr);
    if( pszGribTableDirectory )
    {
        const char* pszFullFilename = CPLFormFilename(pszGribTableDirectory, pszFilename, nullptr);
        VSIStatBufL sStat;
        if( VSIStatL(pszFullFilename, &sStat) == 0 )
            return pszFullFilename;
        return nullptr;
    }
    const char* pszRet = CSVFilename(pszFilename);
    // CSVFilename() returns the same content as pszFilename if it does not
    // find the file.
    if( pszRet && strcmp(pszRet, pszFilename) == 0 )
        return nullptr;
    return pszRet;
}

const char *centerLookup (unsigned short int center)
{
    const char* pszFilename = GetGRIB2_CSVFilename("grib2_center.csv");
    if( pszFilename == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find grib2_center.csv");
        return nullptr;
    }
    const char* pszName = CSVGetField( pszFilename, "code", CPLSPrintf("%d", center),
                                       CC_Integer, "name" );
    if( pszName && pszName[0] == 0 )
        pszName = nullptr;
    return pszName;
}

const char *subCenterLookup(unsigned short int center,
                            unsigned short int subcenter)
{
    const char* pszFilename = GetGRIB2_CSVFilename("grib2_subcenter.csv");
    if( pszFilename == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find grib2_subcenter.csv");
        return nullptr;
    }
    int iCenter = CSVGetFileFieldId(pszFilename,"center_code");
    int iSubCenter = CSVGetFileFieldId(pszFilename,"subcenter_code");
    int iName = CSVGetFileFieldId(pszFilename,"name");
    if( iCenter < 0 || iSubCenter < 0 || iName < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad structure for %s", pszFilename);
        return nullptr;
    }
    CSVRewind(pszFilename);
    while( char** papszFields = CSVGetNextLine(pszFilename) )
    {
        if( atoi(papszFields[iCenter]) == static_cast<int>(center) &&
            atoi(papszFields[iSubCenter]) == static_cast<int>(subcenter) )
        {
            return papszFields[iName];
        }
    }
    return nullptr;
}

#ifdef unused_by_GDAL
const char *processLookup (unsigned short int center, unsigned char process)
{
    const char* pszFilename = GetGRIB2_CSVFilename("grib2_process.csv");
    if( pszFilename == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find grib2_process.csv");
        return nullptr;
    }
    int iCenter = CSVGetFileFieldId(pszFilename,"center_code");
    int iProcess = CSVGetFileFieldId(pszFilename,"process_code");
    int iName = CSVGetFileFieldId(pszFilename,"name");
    if( iCenter < 0 || iProcess < 0 || iName < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad structure for %s", pszFilename);
        return nullptr;
    }
    CSVRewind(pszFilename);
    while( char** papszFields = CSVGetNextLine(pszFilename) )
    {
        if( atoi(papszFields[iCenter]) == static_cast<int>(center) &&
            atoi(papszFields[iProcess]) == static_cast<int>(process) )
        {
            return papszFields[iName];
        }
    }
    return nullptr;
}
#endif

typedef struct {
    const char *GRIB2name, *NDFDname;
} NDFD_AbrevOverrideTable;


static unit_convert GetUnitConvertFromString(const char* pszUnitConv)
{
    unit_convert convert;
    if( strcmp(pszUnitConv, "UC_NONE") == 0 )
        convert = UC_NONE;
    else if( strcmp(pszUnitConv, "UC_K2F") == 0 )
        convert = UC_K2F;
    else if( strcmp(pszUnitConv, "UC_InchWater") == 0 )
        convert = UC_InchWater;
    else if( strcmp(pszUnitConv, "UC_M2Feet") == 0 )
        convert = UC_M2Feet;
    else if( strcmp(pszUnitConv, "UC_M2Inch") == 0 )
        convert = UC_M2Inch;
    else if( strcmp(pszUnitConv, "UC_MS2Knots") == 0 )
        convert = UC_MS2Knots;
    else if( strcmp(pszUnitConv, "UC_LOG10") == 0 )
        convert = UC_LOG10;
    else if( strcmp(pszUnitConv, "UC_UVIndex") == 0 )
        convert = UC_UVIndex;
    else if( strcmp(pszUnitConv, "UC_M2StatuteMile") == 0 )
        convert = UC_M2StatuteMile;
    else
    {
        convert = UC_NONE;
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unhandled unit conversion: %s", pszUnitConv);
    }
    return convert;
}

/*****************************************************************************
 * GetGrib2Table4_2_Record() --
 *
 * PURPOSE
 *   Chooses the correct Parameter table depending on what is in the GRIB2
 * message's "Product Definition Section".
 *
 * ARGUMENTS
 * prodType = The product type (meteo, hydro, land, space, ocean, etc) (In)
 *      cat = The category inside the product (Input)
 *   subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 * shortName= Pointer to short name of the parameter, or nullptr(Output)
 *     name = Pointer to longer name of the parameter, or nullptr (Output)
 *     unit = Pointer to unit name, or nullptr (Output)
 *  convert = Pointer to unit converter, or nullptr (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: TRUE in case of success
 *****************************************************************************
 */
static int GetGrib2Table4_2_Record (int prodType, int cat, int subcat,
                                    const char** shortName,
                                    const char** name,
                                    const char** unit,
                                    unit_convert* convert)
{
    const char* pszBaseFilename = CPLSPrintf("grib2_table_4_2_%d_%d.csv",
                                             prodType, cat);
    const char* pszFilename = GetGRIB2_CSVFilename(pszBaseFilename);
    if( pszFilename == nullptr )
    {
        return FALSE;
    }
    int iSubcat = CSVGetFileFieldId(pszFilename,"subcat");
    int iShortName = CSVGetFileFieldId(pszFilename,"short_name");
    int iName = CSVGetFileFieldId(pszFilename,"name");
    int iUnit = CSVGetFileFieldId(pszFilename,"unit");
    int iUnitConv = CSVGetFileFieldId(pszFilename,"unit_conv");
    if( iSubcat < 0 || iShortName < 0 || iName < 0 || iUnit < 0 || iUnitConv < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad structure for %s", pszFilename);
        return FALSE;
    }
    CSVRewind(pszFilename);
    while( char** papszFields = CSVGetNextLine(pszFilename) )
    {
        if( atoi(papszFields[iSubcat]) == subcat )
        {
            if( shortName )
            {
                // Short name is unavailable from WMO-only entries, so
                // use longer name
                if( papszFields[iShortName][0] == 0 )
                    *shortName = papszFields[iName];
                else
                    *shortName = papszFields[iShortName];
            }
            if( name )
                *name = papszFields[iName];
            if( unit )
                *unit = papszFields[iUnit];
            if( convert )
                *convert = GetUnitConvertFromString(papszFields[iUnitConv]);
            return TRUE;
        }
    }
    return FALSE;
}

/* *INDENT-OFF* */
static const NDFD_AbrevOverrideTable NDFD_Override[] = {
   /*  0 */ {"TMP", "T"},
   /*  1 */ {"TMAX", "MaxT"},
   /*  2 */ {"TMIN", "MinT"},
   /*  3 */ {"DPT", "Td"},
   /*  4 */ {"APCP", "QPF"},
   /* Don't need SNOD for now. */
   /*  5 */ /* {"SNOD", "SnowDepth"}, */
   /*  6 */ {"WDIR", "WindDir"},
   /*  7 */ {"WIND", "WindSpd"},
   /*  8 */ {"TCDC", "Sky"},
   /*  9 */ {"WVHGT", "WaveHeight"},
   /* 10 */ {"ASNOW", "SnowAmt"},
   /* 11 */ {"GUST", "WindGust"},
   /* 12 */ {"MAXRH", "MaxRH"},                /* MPA added 201202 */
   /* 13 */ {"HTSGW", "WaveHeight"},           /* MPA added 201709 */
};
/* *INDENT-ON* */

int IsData_NDFD (unsigned short int center, unsigned short int subcenter)
{
   return ((center == 8) &&
           ((subcenter == GRIB2MISSING_u2) || (subcenter == 0)));
}

int IsData_MOS (unsigned short int center, unsigned short int subcenter)
{
   return ((center == 7) && (subcenter == 14));
}

static const char* GetGrib2LocalTable4_2FileName(int center,
                                                 int subcenter)
{
    const char* pszFilename = GetGRIB2_CSVFilename("grib2_table_4_2_local_index.csv");
    if( pszFilename == nullptr )
    {
        return nullptr;
    }
    int iCenter = CSVGetFileFieldId(pszFilename,"center_code");
    int iSubCenter = CSVGetFileFieldId(pszFilename,"subcenter_code");
    int iFilename = CSVGetFileFieldId(pszFilename,"filename");
    if( iCenter < 0 || iSubCenter < 0 || iFilename < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad structure for %s", pszFilename);
        return nullptr;
    }
    CSVRewind(pszFilename);
    while( char** papszFields = CSVGetNextLine(pszFilename) )
    {
        if( atoi(papszFields[iCenter]) == center )
        {
            if( papszFields[iSubCenter][0] == '\0' ||
                atoi(papszFields[iSubCenter]) == subcenter )
            {
                return GetGRIB2_CSVFilename(papszFields[iFilename]);
            }
        }
    }
    return nullptr;
}

/*****************************************************************************
 * GetGrib2LocalTable4_2_Record() --
 *
 * PURPOSE
 *   Return the parameter definition depending on what is in the GRIB2
 * message's "Product Definition Section" from a local parameter table
 * for a given center/subcenter.
 * Typically this is called after the default Choose_ParmTable was tried,
 * since it consists of all the local specs, and one has to linearly walk
 * through the table.
 *
 * ARGUMENTS
 *    center = The center that created the data. (Input)
 * subcenter = The subcenter that created the data. (Input)
 * prodType = The product type (meteo, hydro, land, space, ocean, etc) (In)
 *      cat = The category inside the product (Input)
 *   subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 * shortName= Pointer to short name of the parameter, or nullptr(Output)
 *     name = Pointer to longer name of the parameter, or nullptr (Output)
 *     unit = Pointer to unit name, or nullptr (Output)
 *  convert = Pointer to unit converter, or nullptr (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: TRUE in case of success
 *****************************************************************************
 */

static int GetGrib2LocalTable4_2_Record (int center,
                                         int subcenter,
                                         int prodType, int cat, int subcat,
                                         const char** shortName,
                                         const char** name,
                                         const char** unit,
                                         unit_convert* convert)
{
    const char* pszFilename = GetGrib2LocalTable4_2FileName(center, subcenter);
    if( pszFilename == nullptr )
    {
        return FALSE;
    }
    int iProd = CSVGetFileFieldId(pszFilename,"prod");
    int iCat = CSVGetFileFieldId(pszFilename,"cat");
    int iSubcat = CSVGetFileFieldId(pszFilename,"subcat");
    int iShortName = CSVGetFileFieldId(pszFilename,"short_name");
    int iName = CSVGetFileFieldId(pszFilename,"name");
    int iUnit = CSVGetFileFieldId(pszFilename,"unit");
    int iUnitConv = CSVGetFileFieldId(pszFilename,"unit_conv");
    if( iProd < 0 || iCat < 0 || iSubcat < 0 || iShortName < 0 ||
        iName < 0 || iUnit < 0 || iUnitConv < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad structure for %s", pszFilename);
        return FALSE;
    }
    CSVRewind(pszFilename);
    while( char** papszFields = CSVGetNextLine(pszFilename) )
    {
        if( atoi(papszFields[iProd]) == prodType &&
            atoi(papszFields[iCat]) == cat &&
            atoi(papszFields[iSubcat]) == subcat )
        {
            if( shortName )
                *shortName = papszFields[iShortName];
            if( name )
                *name = papszFields[iName];
            if( unit )
                *unit = papszFields[iUnit];
            if( convert )
                *convert = GetUnitConvertFromString(papszFields[iUnitConv]);
            return TRUE;
        }
    }
    return FALSE;
}


/*****************************************************************************
 * ParseElemName() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Converts a prodType, template, category and subcategory quadruple to the
 * ASCII string abbreviation of that variable.
 *   For example: 0, 0, 0, 0, = "T" for temperature.
 *
 * ARGUMENTS
 *    center = The center that created the data. (Input)
 * subcenter = The subcenter that created the data. (Input)
 *  prodType = The GRIB2, section 0 product type. (Input)
 *   templat = The GRIB2 section 4 template number. (Input)
 *       cat = The GRIB2 section 4 "General category of Product." (Input)
 *    subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 *   lenTime = The length of time over which statistics are done
 *             (see template 4.8). (Input)
 *     genID = The Generating process ID (used for GFS MOS) (Input)
 *  probType = For Probability templates (Input)
 * lowerProb = Lower Limit for probability templates. (Input)
 * upperProb = Upper Limit for probability templates. (Input)
 *      name = Short name for the data set (T, MaxT, etc) (Output)
 *   comment = Long form of the name (Temperature, etc) (Output)
 *      unit = What unit this variable is originally in (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Re-Created.
 *   6/2004 AAT: Added deltTime (because of Ozone issues).
 *   8/2004 AAT: Adjusted so template 9 gets units of % and no convert.
 *   3/2005 AAT: ReWrote to handle template 5, 9 and MOS.
 *   9/2005 AAT: Added code to handle MOS PoP06 vs MOS PoP12.
 *
 * NOTES
 *****************************************************************************
 */
/* Deal with probability templates 2/16/2006 */
static void ElemNameProb (uChar mstrVersion, uShort2 center, uShort2 subcenter, int prodType,
                          CPL_UNUSED int templat,
                          uChar cat, uChar subcat, sInt4 lenTime,
                          uChar timeRangeUnit,
                          uChar timeIncrType,
                          CPL_UNUSED uChar genID,
                          uChar probType,
                          double lowerProb, double upperProb, char **name,
                          char **comment, char **unit, int *convert)
{
   char f_isNdfd = IsData_NDFD (center, subcenter);
   char f_isMos = IsData_MOS (center, subcenter);

   *unit = (char *) malloc (strlen ("[%]") + 1);
   strcpy (*unit, "[%]");

   {
      // 25.4 mm = 1 inch
      const double tmp = upperProb * 25.4;

      // TODO(schwehr): Make a function and reuse it for other limit checks.
      if (upperProb > tmp ||
          tmp > std::numeric_limits<int>::max() ||
          tmp < std::numeric_limits<int>::min() ||
          CPLIsNan(tmp) ) {
         // TODO(schwehr): What is the correct response?
         errSprintf ("ERROR: upperProb out of range.  Setting to 0.\n");
         upperProb = 0.0;
      }
   }

   if (f_isNdfd || f_isMos) {
      /* Deal with NDFD/MOS handling of Prob Precip_Tot -> PoP12 */
      if ((prodType == 0) && (cat == 1) && (subcat == 8)) {
         if (probType == 0) {
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "ProbPrcpBlw%02dm", lenTime);
                  mallocSprintf (comment, "%02d mon Prob of Precip below average", lenTime);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "ProbPrcpBlw%02dy", lenTime);
                  mallocSprintf (comment, "%02d yr Prob of Precip below average", lenTime);
               } else {
                  mallocSprintf (name, "ProbPrcpBlw%02d", lenTime);
                  mallocSprintf (comment, "%02d hr Prob of Precip below average", lenTime);
               }
            } else {
               mallocSprintf (name, "ProbPrcpBlw");
               mallocSprintf (comment, "Prob of precip below average");
            }
         } else if (probType == 3) {
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "ProbPrcpAbv%02dm", lenTime);
                  mallocSprintf (comment, "%02d mon Prob of Precip above average", lenTime);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "ProbPrcpAbv%02dy", lenTime);
                  mallocSprintf (comment, "%02d yr Prob of Precip above average", lenTime);
               } else {
                  mallocSprintf (name, "ProbPrcpAbv%02d", lenTime);
                  mallocSprintf (comment, "%02d hr Prob of Precip above average", lenTime);
               }
            } else {
               mallocSprintf (name, "ProbPrcpAbv");
               mallocSprintf (comment, "Prob of precip above average");
            }
         } else {
            myAssert (probType == 1);
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  if (upperProb != (double) .254) {
                     mallocSprintf (name, "PoP%02dm-%03d", lenTime, (int) (upperProb / .254 + .5));
                  } else {
                     mallocSprintf (name, "PoP%02dm", lenTime);
                  }
                  mallocSprintf (comment, "%02d mon Prob of Precip > %g In.", lenTime, upperProb / 25.4);
               } else if (timeRangeUnit == 4) {
                  if (upperProb != (double) .254) {
                     mallocSprintf (name, "PoP%02dy-%03d", lenTime, (int) (upperProb / .254 + .5));
                  } else {
                     mallocSprintf (name, "PoP%02dy", lenTime);
                  }
                  mallocSprintf (comment, "%02d yr Prob of Precip > %g In.", lenTime, upperProb / 25.4);
               } else {
                  /* The 300 is to deal with an old NDFD encoding bug from 2002:
                   * PDS-S4 | Upper limit (scale value, scale factor) | 300 (3, -2)
                   * 25.4 mm = 1 inch.  Rain typically .01 inches = .254 mm
                   */
                  if ((upperProb != (double) .254) && (upperProb != (double) 300)) {
                     mallocSprintf (name, "PoP%02d-%03d", lenTime, (int) (upperProb / .254 + .5));
                  } else {
                     mallocSprintf (name, "PoP%02d", lenTime);
                  }
                  if (upperProb != (double) 300) {
                     mallocSprintf (comment, "%02d hr Prob of Precip > %g In.", lenTime, upperProb / 25.4);
                  } else {
                     mallocSprintf (comment, "%02d hr Prob of Precip > 0.01 In.", lenTime);
                  }
               }
            } else {
               if (upperProb != (double) .254) {
                  mallocSprintf (name, "PoP-p%03d", (int) (upperProb / .254 + .5));
               } else {
                  mallocSprintf (name, "PoP");
               }
               mallocSprintf (comment, "Prob of Precip > %g In.", upperProb / 25.4);
            }
         }
         *convert = UC_NONE;
         return;
      }
      /*
       * Deal with NDFD handling of Prob. Wind speeds.
       * There are different solutions for naming the Prob. Wind fields
       * AAT(Mine): ProbSurge5c
       */
      if ((prodType == 10) && (cat == 3) && (subcat == 192)) {
         myAssert (probType == 1);
         myAssert (lenTime > 0);
         if (timeIncrType == 2) {
            /* Incremental */
            mallocSprintf (name, "ProbSurge%02di",
                           (int) ((upperProb / 0.3048) + .5));
         } else {
            /* Cumulative */
            myAssert (timeIncrType == 192);
            mallocSprintf (name, "ProbSurge%02dc",
                           (int) ((upperProb / 0.3048) + .5));
         }
         if (timeRangeUnit == 3) {
            mallocSprintf (comment, "%02d mon Prob of Hurricane Storm Surge > %g "
                           "m", lenTime, upperProb);
         } else if (timeRangeUnit == 4) {
            mallocSprintf (comment, "%02d yr Prob of Hurricane Storm Surge > %g "
                           "m", lenTime, upperProb);
         } else {
            mallocSprintf (comment, "%02d hr Prob of Hurricane Storm Surge > %g "
                           "m", lenTime, upperProb);
         }
         *convert = UC_NONE;
         return;
      }
   }
   if (f_isNdfd) {
      /*
       * Deal with NDFD handling of Prob. Wind speeds.
       * There are different solutions for naming the Prob. Wind fields
       * Tim Boyer: TCWindSpdIncr34 TCWindSpdIncr50 TCWindSpdIncr64
       *            TCWindSpdCumu34 TCWindSpdCumu50 TCWindSpdCumu64
       * Dave Ruth: tcwspdabv34i tcwspdabv50i tcwspdabv64i
       *            tcwspdabv34c tcwspdabv50c tcwspdabv64c
       * AAT(Mine): ProbWindSpd34c ProbWindSpd50c ProbWindSpd64c
       *            ProbWindSpd34i ProbWindSpd50i ProbWindSpd64i
       */
      if ((prodType == 0) && (cat == 2) && (subcat == 1)) {
         myAssert (probType == 1);
         myAssert (lenTime > 0);
         if (timeIncrType == 2) {
            /* Incremental */
            mallocSprintf (name, "ProbWindSpd%02di",
                           (int) ((upperProb * 3600. / 1852.) + .5));
         } else {
            /* Cumulative */
            myAssert (timeIncrType == 192);
            mallocSprintf (name, "ProbWindSpd%02dc",
                           (int) ((upperProb * 3600. / 1852.) + .5));
         }
         if (timeRangeUnit == 3) {
            mallocSprintf (comment, "%02d mon Prob of Wind speed > %g m/s",
                           lenTime, upperProb);
         } else if (timeRangeUnit == 4) {
            mallocSprintf (comment, "%02d yr Prob of Wind speed > %g m/s",
                           lenTime, upperProb);
         } else {
            mallocSprintf (comment, "%02d hr Prob of Wind speed > %g m/s",
                           lenTime, upperProb);
         }
         *convert = UC_NONE;
         return;
      }
   }

   /* Only look at Generic tables if mstrVersion is not 255. */
   int gotRecordGeneric = FALSE;
   const char* pszShortName = nullptr;
   const char* pszName = nullptr;
   const char* pszUnit = nullptr;
   if (mstrVersion != 255) {
       gotRecordGeneric = GetGrib2Table4_2_Record (prodType, cat, subcat,
                                            &pszShortName, &pszName, &pszUnit,
                                            nullptr);
   }

   if (gotRecordGeneric && strcmp(pszName, "Reserved for local use") == 0) {
       gotRecordGeneric = false;
   }

   if (gotRecordGeneric) {
         /* Check for NDFD over-rides. */
         /* The NDFD over-rides for probability templates have already been
          * handled. */
         if (lenTime > 0) {
            if (timeRangeUnit == 3) {
               mallocSprintf (name, "Prob%s%02dm", pszShortName, lenTime);
               mallocSprintf (comment, "%02d mon Prob of %s ", lenTime,
                              pszName);
            } else if (timeRangeUnit == 4) {
               mallocSprintf (name, "Prob%s%02dy", pszShortName, lenTime);
               mallocSprintf (comment, "%02d yr Prob of %s ", lenTime,
                              pszName);
            } else {
               mallocSprintf (name, "Prob%s%02d", pszShortName, lenTime);
               mallocSprintf (comment, "%02d hr Prob of %s ", lenTime,
                              pszName);
            }
         } else {
            mallocSprintf (name, "Prob%s", pszShortName);
            mallocSprintf (comment, "Prob of %s ", pszName);
         }
         if (probType == 0) {
            if ((f_isNdfd || f_isMos) && (strcmp (pszShortName, "TMP") == 0)) {
               reallocSprintf (comment, "below average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sBlw%02dm", pszShortName, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sBlw%02dy", pszShortName, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sBlw%02d", pszShortName, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sBlw", pszShortName);
               }
            } else {
               reallocSprintf (comment, "< %g %s", lowerProb, pszUnit);
            }
         } else if (probType == 1) {
            if ((f_isNdfd || f_isMos) && (strcmp (pszShortName, "TMP") == 0)) {
               reallocSprintf (comment, "above average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sAbv%02dm", pszShortName, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sAbv%02dy", pszShortName, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sAbv%02d", pszShortName, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sAbv", pszShortName);
               }
            } else {
               reallocSprintf (comment, "> %g %s", upperProb, pszUnit);
            }
         } else if (probType == 2) {
            reallocSprintf (comment, ">= %g, < %g %s", lowerProb, upperProb,
                            pszUnit);
         } else if (probType == 3) {
            if ((f_isNdfd || f_isMos) && (strcmp (pszShortName, "TMP") == 0)) {
               reallocSprintf (comment, "above average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sAbv%02dm", pszShortName, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sAbv%02dy", pszShortName, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sAbv%02d", pszShortName, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sAbv", pszShortName);
               }
            } else {
               reallocSprintf (comment, "> %g %s", lowerProb, pszUnit);
            }
         } else if (probType == 4) {
            if ((f_isNdfd || f_isMos) && (strcmp (pszShortName, "TMP") == 0)) {
               reallocSprintf (comment, "below average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sBlw%02dm", pszShortName, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sBlw%02dy", pszShortName, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sBlw%02d", pszShortName, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sBlw", pszShortName);
               }
            } else {
               reallocSprintf (comment, "< %g %s", upperProb, pszUnit);
            }
         } else {
            reallocSprintf (comment, "%s", pszUnit);
         }
         *convert = UC_NONE;
         return;
   }

   /* Local use tables. */
   int gotRecordLocal = GetGrib2LocalTable4_2_Record (center, subcenter,
                                                 prodType, cat, subcat,
                                                 &pszShortName, &pszName, &pszUnit,
                                                 nullptr);
   if (gotRecordLocal) {
            /* Ignore adding Prob prefix and "Probability of" to NDFD SPC prob
             * products. */
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "Prob%s%02dm", pszShortName, lenTime);
                  mallocSprintf (comment, "%02d mon Prob of %s ", lenTime,
                                 pszName);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "Prob%s%02dy", pszShortName, lenTime);
                  mallocSprintf (comment, "%02d yr Prob of %s ", lenTime,
                                 pszName);
               } else {
                  mallocSprintf (name, "Prob%s%02d", pszShortName, lenTime);
                  mallocSprintf (comment, "%02d hr Prob of %s ", lenTime,
                                 pszName);
               }
            } else {
               mallocSprintf (name, "Prob%s", pszShortName);
               mallocSprintf (comment, "Prob of %s ", pszName);
            }
            if (probType == 0) {
               reallocSprintf (comment, "< %g %s", lowerProb,
                               pszUnit);
            } else if (probType == 1) {
               reallocSprintf (comment, "> %g %s", upperProb,
                               pszUnit);
            } else if (probType == 2) {
               reallocSprintf (comment, ">= %g, < %g %s", lowerProb,
                               upperProb, pszUnit);
            } else if (probType == 3) {
               reallocSprintf (comment, "> %g %s", lowerProb,
                               pszUnit);
            } else if (probType == 4) {
               reallocSprintf (comment, "< %g %s", upperProb,
                               pszUnit);
            } else {
               reallocSprintf (comment, "%s", pszUnit);
            }
            *convert = UC_NONE;
            return;
   }

   *name = (char *) malloc (strlen ("ProbUnknown") + 1);
   strcpy (*name, "ProbUnknown");
   mallocSprintf (comment, "Prob of (prodType %d, cat %d, subcat %d)",
                  prodType, cat, subcat);
   *convert = UC_NONE;
   return;
}

/* Deal with percentile templates 5/1/2006 */
static void ElemNamePerc (uChar mstrVersion, uShort2 center, uShort2 subcenter, int prodType,
                          CPL_UNUSED int templat,
                          uChar cat, uChar subcat, sInt4 lenTime,
                          uChar timeRangeUnit,
                          sChar percentile, char **name, char **comment,
                          char **unit, int *convert)
{
   /* Only look at Generic tables if mstrVersion is not 255. */
   int gotRecordGeneric = FALSE;
   const char* pszShortName = nullptr;
   const char* pszName = nullptr;
   const char* pszUnit = nullptr;
   unit_convert unitConvert = UC_NONE;
   if (mstrVersion != 255) {
       gotRecordGeneric = GetGrib2Table4_2_Record (prodType, cat, subcat,
                                            &pszShortName, &pszName, &pszUnit,
                                            &unitConvert);
   }

   if (gotRecordGeneric && strcmp(pszName, "Reserved for local use") == 0) {
       gotRecordGeneric = false;
   }

   if (gotRecordGeneric) {
         /* Check for NDFD over-rides. */
         if (IsData_NDFD (center, subcenter) ||
             IsData_MOS (center, subcenter)) {
            for (size_t i = 0; i < (sizeof (NDFD_Override) /
                             sizeof (NDFD_AbrevOverrideTable)); i++) {
               if (strcmp (pszShortName, "ASNOW") == 0) {
                 if (timeRangeUnit == 3) {
                    mallocSprintf (name, "%s%02dm%s%02dm", "Snow", lenTime, "e", percentile);
                    mallocSprintf (comment, "%02d mon %s Percentile(%d)", lenTime,
                                 pszName, percentile);
                 } else if (timeRangeUnit == 4) {
                    mallocSprintf (name, "%s%02dy%s%02dy", "Snow", lenTime, "e", percentile);
                    mallocSprintf (comment, "%02d yr %s Percentile(%d)", lenTime,
                                   pszName, percentile);
                 } else {
                    mallocSprintf (name, "%s%02d%s%02d", "Snow", lenTime, "e", percentile);
                    mallocSprintf (comment, "%02d hr %s Percentile(%d)", lenTime,
                                   pszName, percentile);
                 }
                 mallocSprintf (unit, "[%s]", pszUnit);
                 *convert = unitConvert;
                 return;
               }
               if (strcmp (NDFD_Override[i].GRIB2name, pszShortName) ==
                   0) {
                  mallocSprintf (name, "%s%02d", NDFD_Override[i].NDFDname,
                                 percentile);
                  if (lenTime > 0) {
                     if (timeRangeUnit == 3) {
                        mallocSprintf (comment, "%02d mon %s Percentile(%d)",
                                       lenTime, pszName,
                                       percentile);
                     } else if (timeRangeUnit == 4) {
                        mallocSprintf (comment, "%02d yr %s Percentile(%d)",
                                       lenTime, pszName,
                                       percentile);
                     } else {
                        mallocSprintf (comment, "%02d hr %s Percentile(%d)",
                                       lenTime, pszName,
                                       percentile);
                     }
                  } else {
                     mallocSprintf (comment, "%s Percentile(%d)",
                                    pszName, percentile);
                  }
                  mallocSprintf (unit, "[%s]", pszUnit);
                  *convert = unitConvert;
                  return;
               }
            }
         }
         mallocSprintf (name, "%s%02d", pszShortName, percentile);
         if (lenTime > 0) {
            if (timeRangeUnit == 3) {
               mallocSprintf (comment, "%02d mon %s Percentile(%d)",
                              lenTime, pszName, percentile);
            } else if (timeRangeUnit == 4) {
               mallocSprintf (comment, "%02d yr %s Percentile(%d)",
                              lenTime, pszName, percentile);
            } else {
               mallocSprintf (comment, "%02d hr %s Percentile(%d)",
                              lenTime, pszName, percentile);
            }
         } else {
            mallocSprintf (comment, "%s Percentile(%d)",
                           pszName, percentile);
         }
         mallocSprintf (unit, "[%s]", pszUnit);
         *convert = unitConvert;
         return;
   }

   /* Local use tables. */
   int gotRecordLocal = GetGrib2LocalTable4_2_Record (center, subcenter,
                                             prodType, cat, subcat,
                                             &pszShortName, &pszName, &pszUnit,
                                             &unitConvert);
   if (gotRecordLocal) {
/* If last two characters in name are numbers, then the name contains
 * the percentile (or exceedance value) so don't tack on percentile here.*/
            size_t len = strlen(pszShortName);
            if (len >= 2 &&
                isdigit(pszShortName[len -1]) &&
                isdigit(pszShortName[len -2])) {
               mallocSprintf (name, "%s", pszShortName);
            } else if ((strcmp (pszShortName, "Surge") == 0) ||
                       (strcmp (pszShortName, "SURGE") == 0)) {
/* Provide a special exception for storm surge exceedance.
 * Want exceedance value rather than percentile value.
 */
               mallocSprintf (name, "%s%02d", pszShortName, 100 - percentile);
            } else {
               mallocSprintf (name, "%s%02d", pszShortName, percentile);
            }

            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (comment, "%02d mon %s Percentile(%d)",
                                 lenTime, pszName, percentile);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (comment, "%02d yr %s Percentile(%d)",
                                 lenTime, pszName, percentile);
               } else {
                  mallocSprintf (comment, "%02d hr %s Percentile(%d)",
                                 lenTime, pszName, percentile);
               }
            } else {
               mallocSprintf (comment, "%s Percentile(%d)",
                              pszName, percentile);
            }
            mallocSprintf (unit, "[%s]", pszUnit);
            *convert = unitConvert;
            return;
   }

   *name = (char *) malloc (strlen ("unknown") + 1);
   strcpy (*name, "unknown");
   mallocSprintf (comment, "(prodType %d, cat %d, subcat %d)", prodType,
                  cat, subcat);
   *unit = (char *) malloc (strlen ("[-]") + 1);
   strcpy (*unit, "[-]");
   *convert = UC_NONE;
   return;
}

/* Deal with non-prob templates 2/16/2006 */
static void ElemNameNorm (uChar mstrVersion, uShort2 center, uShort2 subcenter, int prodType,
                          int templat, uChar cat, uChar subcat, sInt4 lenTime,
                          uChar timeRangeUnit, uChar statProcessID,
                          CPL_UNUSED uChar timeIncrType,
                          CPL_UNUSED uChar genID,
                          CPL_UNUSED uChar probType,
                          CPL_UNUSED double lowerProb,
                          CPL_UNUSED double upperProb,
                          char **name,
                          char **comment, char **unit, int *convert,
                          sChar f_fstValue, double fstSurfValue,
                          sChar f_sndValue, double sndSurfValue)
{
   sChar f_accum;
   /* float delt; */

   /* Check for over-ride case for ozone.  Originally just for NDFD, but I
    * think it is useful for ozone data that originated elsewhere. */
   if ((prodType == 0) && (templat == 8) && (cat == 14) && (subcat == 193)) {
      if (lenTime > 0) {
         if (timeRangeUnit == 3) {
            mallocSprintf (name, "Ozone%02dm", lenTime);
            mallocSprintf (comment, "%d mon Average Ozone Concentration", lenTime);
         } else if (timeRangeUnit == 4) {
            mallocSprintf (name, "Ozone%02dy", lenTime);
            mallocSprintf (comment, "%d yr Average Ozone Concentration", lenTime);
         } else {
            mallocSprintf (name, "Ozone%02d", lenTime);
            mallocSprintf (comment, "%d hr Average Ozone Concentration", lenTime);
         }
      } else {
         *name = (char *) malloc (strlen ("AVGOZCON") + 1);
         strcpy (*name, "AVGOZCON");
         *comment = (char *) malloc (strlen ("Average Ozone Concentration") +
                                     1);
         strcpy (*comment, "Average Ozone Concentration");
      }
      *unit = (char *) malloc (strlen ("[PPB]") + 1);
      strcpy (*unit, "[PPB]");
      *convert = UC_NONE;
      return;
   }
   /* Check for over-ride case for smokec / smokes. */
   if (center == 7) {
      if ((prodType == 0) && (cat == 13) && (subcat == 195)) {
         /* If NCEP/ARL (genID=6) then it is dust */
         if (genID == 6) {
            if (f_fstValue && f_sndValue) {
               const double delt = fstSurfValue - sndSurfValue;
               if ((delt <= 100) && (delt >= -100)) {
                  *name = (char *) malloc (strlen ("dusts") + 1);
                  strcpy (*name, "dusts");
                  *comment = (char *) malloc (strlen ("Surface level dust") + 1);
                  strcpy (*comment, "Surface level dust");
                  *unit = (char *) malloc (strlen ("[log10(10^-6g/m^3)]") + 1);
                  strcpy (*unit, "[log10(10^-6g/m^3)]");
                  *convert = UC_LOG10;
                  return;
               } else if ((delt <= 5000) && (delt >= -5000)) {
                  *name = (char *) malloc (strlen ("dustc") + 1);
                  strcpy (*name, "dustc");
                  *comment = (char *) malloc (strlen ("Average vertical column dust") + 1);
                  strcpy (*comment, "Average vertical column dust");
                  *unit = (char *) malloc (strlen ("[log10(10^-6g/m^3)]") + 1);
                  strcpy (*unit, "[log10(10^-6g/m^3)]");
                  *convert = UC_LOG10;
                  return;
               }
            }
         } else {
            if (f_fstValue && f_sndValue) {
               const double delt = fstSurfValue - sndSurfValue;
               if ((delt <= 100) && (delt >= -100)) {
                  *name = (char *) malloc (strlen ("smokes") + 1);
                  strcpy (*name, "smokes");
                  *comment = (char *) malloc (strlen ("Surface level smoke from fires") + 1);
                  strcpy (*comment, "Surface level smoke from fires");
                  *unit = (char *) malloc (strlen ("[log10(10^-6g/m^3)]") + 1);
                  strcpy (*unit, "[log10(10^-6g/m^3)]");
                  *convert = UC_LOG10;
                  return;
               } else if ((delt <= 5000) && (delt >= -5000)) {
                  *name = (char *) malloc (strlen ("smokec") + 1);
                  strcpy (*name, "smokec");
                  *comment = (char *) malloc (strlen ("Average vertical column smoke from fires") + 1);
                  strcpy (*comment, "Average vertical column smoke from fires");
                  *unit = (char *) malloc (strlen ("[log10(10^-6g/m^3)]") + 1);
                  strcpy (*unit, "[log10(10^-6g/m^3)]");
                  *convert = UC_LOG10;
                  return;
               }
            }
         }
      }
   }

   /* Only look at Generic tables if mstrVersion is not 255. */
   int gotRecordGeneric = FALSE;
   const char* pszShortName = nullptr;
   const char* pszName = nullptr;
   const char* pszUnit = nullptr;
   unit_convert unitConvert = UC_NONE;
   if (mstrVersion != 255) {
       gotRecordGeneric = GetGrib2Table4_2_Record (prodType, cat, subcat,
                                            &pszShortName, &pszName, &pszUnit,
                                            &unitConvert);
   }

   if (gotRecordGeneric && strcmp(pszName, "Reserved for local use") == 0) {
       gotRecordGeneric = false;
   }

   if (gotRecordGeneric) {
         /* Check for NDFD over-rides. */
         if (IsData_MOS (center, subcenter)) {
            if (strcmp (pszShortName, "APCP") == 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "%s%02dm", "QPF", lenTime);
                  mallocSprintf (comment, "%02d mon %s", lenTime,
                                 pszName);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "%s%02dy", "QPF", lenTime);
                  mallocSprintf (comment, "%02d yr %s", lenTime,
                                 pszName);
               } else {
                  mallocSprintf (name, "%s%02d", "QPF", lenTime);
                  mallocSprintf (comment, "%02d hr %s", lenTime,
                                 pszName);
               }
               mallocSprintf (unit, "[%s]", pszUnit);
               *convert = unitConvert;
               return;
            }
            if (strcmp (pszShortName, "ASNOW") == 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "%s%02dm", "SnowAmt", lenTime);
                  mallocSprintf (comment, "%02d mon %s", lenTime,
                                 pszName);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "%s%02dy", "SnowAmt", lenTime);
                  mallocSprintf (comment, "%02d yr %s", lenTime,
                                 pszName);
               } else {
                  mallocSprintf (name, "%s%02d", "SnowAmt", lenTime);
                  mallocSprintf (comment, "%02d hr %s", lenTime,
                                 pszName);
               }
               mallocSprintf (unit, "[%s]", pszUnit);
               *convert = unitConvert;
               return;
            }
         }
         if (IsData_NDFD (center, subcenter) || IsData_MOS (center, subcenter)) {
            if (strcmp (pszShortName, "EVP") == 0) {
               if (statProcessID == 10) {
                  mallocSprintf (name, "%s%02d", "EvpDep", lenTime);
                  mallocSprintf (comment, "%02d hr Evapo-Transpiration departure from normal",
                                 lenTime);
               } else {
                  mallocSprintf (name, "%s%02d", "Evp", lenTime);
                  mallocSprintf (comment, "%02d hr Evapo-Transpiration", lenTime);
               }
               mallocSprintf (unit, "[%s]", pszUnit);
               *convert = unitConvert;
               return;
            }
            for (size_t i = 0; i < (sizeof (NDFD_Override) /
                             sizeof (NDFD_AbrevOverrideTable)); i++) {
               if (strcmp (NDFD_Override[i].GRIB2name, pszShortName) ==
                   0) {
                  *name = (char *) malloc (strlen (NDFD_Override[i].NDFDname) + 1);
                  strcpy (*name, NDFD_Override[i].NDFDname);
                  *comment = (char *) malloc (strlen (pszName) + 1);
                  strcpy (*comment, pszName);
                  mallocSprintf (unit, "[%s]", pszUnit);
                  *convert = unitConvert;
                  return;
               }
            }
         }
         /* Allow hydrologic PoP, thunderstorm probability (TSTM), or APCP to
          * have lenTime labels. */
         f_accum = (((prodType == 1) && (cat == 1) && (subcat == 2)) ||
                    ((prodType == 0) && (cat == 19) && (subcat == 2)) ||
                    ((prodType == 0) && (cat == 1) && (subcat == 8)) ||
                    ((prodType == 0) && (cat == 19) && (subcat == 203)));
         if (f_accum && (lenTime > 0)) {
            if (timeRangeUnit == 3) {
               mallocSprintf (name, "%s%02dm", pszShortName, lenTime);
               mallocSprintf (comment, "%02d mon %s", lenTime,
                              pszName);
            } else if (timeRangeUnit == 4) {
               mallocSprintf (name, "%s%02dy", pszShortName, lenTime);
               mallocSprintf (comment, "%02d yr %s", lenTime,
                              pszName);
            } else {
               mallocSprintf (name, "%s%02d", pszShortName, lenTime);
               mallocSprintf (comment, "%02d hr %s", lenTime,
                              pszName);
            }
         } else {
            *name = (char *) malloc (strlen (pszShortName) + 1);
            strcpy (*name, pszShortName);
            *comment = (char *) malloc (strlen (pszName) + 1);
            strcpy (*comment, pszName);
         }
         mallocSprintf (unit, "[%s]", pszUnit);
         *convert = unitConvert;
         return;
   }

   /* Local use tables. */
   int gotRecordLocal = GetGrib2LocalTable4_2_Record (center, subcenter,
                                             prodType, cat, subcat,
                                             &pszShortName, &pszName, &pszUnit,
                                             &unitConvert);
   if (gotRecordLocal) {
            /* Allow specific products with non-zero lenTime to reflect that.
             */
#ifdef deadcode
            f_accum = 0;
            if (f_accum && (lenTime > 0)) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "%s%02dm", pszShortName, lenTime);
                  mallocSprintf (comment, "%02d mon %s", lenTime,
                                 pszName);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "%s%02dy", pszShortName, lenTime);
                  mallocSprintf (comment, "%02d yr %s", lenTime,
                                 pszName);
               } else {
                  mallocSprintf (name, "%s%02d", pszShortName, lenTime);
                  mallocSprintf (comment, "%02d hr %s", lenTime,
                                 pszName);
               }
            } else
#endif
            {
               *name = (char *) malloc (strlen (pszShortName) + 1);
               strcpy (*name, pszShortName);
               *comment = (char *) malloc (strlen (pszName) + 1);
               strcpy (*comment, pszName);
            }
            mallocSprintf (unit, "[%s]", pszUnit);
            *convert = unitConvert;
            return;
   }

   *name = (char *) malloc (strlen ("unknown") + 1);
   strcpy (*name, "unknown");
   mallocSprintf (comment, "(prodType %d, cat %d, subcat %d)", prodType,
                  cat, subcat);
   *unit = (char *) malloc (strlen ("[-]") + 1);
   strcpy (*unit, "[-]");
   *convert = UC_NONE;
   return;
}

void ParseElemName (CPL_UNUSED uChar mstrVersion, uShort2 center, uShort2 subcenter, int prodType,
                    int templat, int cat, int subcat, sInt4 lenTime,
                    uChar timeRangeUnit, CPL_UNUSED uChar statProcessID,
                    uChar timeIncrType, uChar genID, uChar probType,
                    double lowerProb, double upperProb,
                    uChar derivedFcst,
                    char **name,
                    char **comment, char **unit, int *convert,
                    sChar percentile, uChar genProcess,
                    sChar f_fstValue, double fstSurfValue,
                    sChar f_sndValue, double sndSurfValue)
{
   char f_isNdfd = IsData_NDFD (center, subcenter);
   myAssert (*name == nullptr);
   myAssert (*comment == nullptr);
   myAssert (*unit == nullptr);

   /* Check if this is Probability data */
   if ((templat == GS4_PROBABIL_TIME) || (templat == GS4_PROBABIL_PNT)) {
      if (f_isNdfd && (prodType == 0) && (cat == 19)) {
         /* don't use ElemNameProb. */
         ElemNameNorm (mstrVersion, center, subcenter, prodType, templat, cat, subcat,
                       lenTime, timeRangeUnit, statProcessID, timeIncrType, genID, probType, lowerProb,
                       upperProb, name, comment, unit, convert, f_fstValue, fstSurfValue,
                       f_sndValue, sndSurfValue);

      } else {
         ElemNameProb (mstrVersion, center, subcenter, prodType, templat, cat, subcat,
                       lenTime, timeRangeUnit, timeIncrType, genID, probType, lowerProb,
                       upperProb, name, comment, unit, convert);
      }
   } else if ((templat == GS4_PERCENT_TIME) || (templat == GS4_PERCENT_PNT)) {
      ElemNamePerc (mstrVersion, center, subcenter, prodType, templat, cat, subcat,
                    lenTime, timeRangeUnit, percentile, name, comment, unit, convert);
   } else {
      ElemNameNorm (mstrVersion, center, subcenter, prodType, templat, cat, subcat,
                    lenTime, timeRangeUnit, statProcessID, timeIncrType, genID, probType, lowerProb,
                    upperProb, name, comment, unit, convert, f_fstValue, fstSurfValue,
                       f_sndValue, sndSurfValue);
   }

   // https://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table4-7.shtml
   if( derivedFcst == 2 // Standard Deviation with respect to Cluster Mean
       || derivedFcst == 3 // Standard Deviation with respect to Cluster Mean, Normalized
       || derivedFcst == 4 // Spread of All Members
       || derivedFcst == 5 // Large Anomaly Index of All Members
       || derivedFcst == 7 // Interquartile Range (Range between the 25th and 75th quantile)
   )
   {
        const char* overrideUnit = nullptr;
        switch( derivedFcst )
        {
            case 2: overrideUnit = "[stddev]"; break;
            case 3: overrideUnit = "[stddev normalized]"; break;
            case 4: overrideUnit = "[spread]"; break;
            case 5: overrideUnit = "[large anomaly index]"; break;
            case 7: overrideUnit = "[interquantile range]"; break;
            default: break;
        }
        if( overrideUnit )
        {
            free(*unit);
            *unit = (char *) malloc (strlen (overrideUnit) + 1);
            strcpy (*unit, overrideUnit);
        }
        *convert = UC_NONE;
   }

   if ((genProcess == 6) || (genProcess == 7)) {
      *convert = UC_NONE;
      reallocSprintf (name, "ERR");
      reallocSprintf (comment, " error %s", *unit);
   } else {
      reallocSprintf (comment, " %s", *unit);
   }
}

/*****************************************************************************
 * ParseElemName2() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Converts a prodType, template, category and subcategory quadruple to the
 * ASCII string abbreviation of that variable.
 *   For example: 0, 0, 0, 0, = "T" for temperature.
 *
 * ARGUMENTS
 * prodType = The GRIB2, section 0 product type. (Input)
 *  templat = The GRIB2 section 4 template number. (Input)
 *      cat = The GRIB2 section 4 "General category of Product." (Input)
 *   subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 *     name = Where to store the result (assumed already allocated to at
 *            least 15 bytes) (Output)
 *  comment = Extra info about variable (assumed already allocated to at
 *            least 100 bytes) (Output)
 *     unit = What unit this variable is in. (assumed already allocated to at
 *            least 20 bytes) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: char *
 *   Same as 'strcpy', i.e. it returns name.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT: Added MOIST_TOT_SNOW (and switched MOIST_SNOWAMT to
 *               SnowDepth)
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   2/2003 AAT: moved from degrib.c to metaparse.c
 *              (Reason: primarily for Sect2 Parsing)
 *              (renamed from ElementName to ParseElemName)
 *   4/2003 AAT: Added the comment as a return element.(see GRIB2 discipline)
 *   6/2003 AAT: Added the unit as a return element.
 *   6/2003 AAT: Added Wave Height.
 *
 * NOTES
 *   Similar to GRIB1_Table2LookUp... May want to take this and the unit
 * stuff and combine them into a module.
 *****************************************************************************
 */
/*
static void ParseElemName2 (int prodType, int templat, int cat, int subcat,
                            char *name, char *comment, char *unit)
{
   if (prodType == 0) {
      if (cat == CAT_TEMP) { * 0 *
         switch (subcat) {
            case TEMP_TEMP: * 0 *
               strcpy (comment, "Temperature [K]");
               strcpy (name, "T");
               strcpy (unit, "[K]");
               return;
            case TEMP_MAXT: * 4 *
               strcpy (comment, "Maximum temperature [K]");
               strcpy (name, "MaxT");
               strcpy (unit, "[K]");
               return;
            case TEMP_MINT: * 5 *
               strcpy (comment, "Minimum temperature [K]");
               strcpy (name, "MinT");
               strcpy (unit, "[K]");
               return;
            case TEMP_DEW_TEMP: * 6 *
               strcpy (comment, "Dew point temperature [K]");
               strcpy (name, "Td");
               strcpy (unit, "[K]");
               return;
            case TEMP_WINDCHILL: * 13 *
               strcpy (comment, "Wind chill factor [K]");
               strcpy (name, "WCI");
               strcpy (unit, "[K]");
               return;
            case TEMP_HEAT: * 12 *
               strcpy (comment, "Heat index [K]");
               strcpy (name, "HeatIndex");
               strcpy (unit, "[K]");
               return;
         }
      } else if (cat == CAT_MOIST) { * 1 *
         switch (subcat) {
            case MOIST_REL_HUMID: * 1 *
               strcpy (comment, "Relative Humidity [%]");
               strcpy (name, "RH");
               strcpy (unit, "[%]");
               return;
            case MOIST_PRECIP_TOT: * 8 *
               if (templat == GS4_PROBABIL_TIME) { * template number 9 implies prob. *
                  strcpy (comment, "Prob of 0.01 In. of Precip [%]");
                  strcpy (name, "PoP12");
                  strcpy (unit, "[%]");
                  return;
               } else {
                  strcpy (comment, "Total precipitation [kg/(m^2)]");
                  strcpy (name, "QPF");
                  strcpy (unit, "[kg/(m^2)]");
                  return;
               }
            case MOIST_SNOWAMT: * 11 *
               strcpy (comment, "Snow Depth [m]");
               strcpy (name, "SnowDepth");
               strcpy (unit, "[m]");
               return;
            case MOIST_TOT_SNOW: * 29 *
               strcpy (comment, "Total snowfall [m]");
               strcpy (name, "SnowAmt");
               strcpy (unit, "[m]");
               return;
            case 192:  * local use moisture. *
               strcpy (comment, "Weather (local use moisture) [-]");
               strcpy (name, "Wx");
               strcpy (unit, "[-]");
               return;
         }
      } else if (cat == CAT_MOMENT) { * 2 *
         switch (subcat) {
            case MOMENT_WINDDIR: * 0 *
               strcpy (comment, "Wind direction (from which blowing) "
                       "[deg true]");
               strcpy (name, "WindDir");
               strcpy (unit, "[deg true]");
               return;
            case MOMENT_WINDSPD: * 1 *
               strcpy (comment, "Wind speed [m/s]");
               strcpy (name, "WindSpd");
               strcpy (unit, "[m/s]");
               return;
         }
      } else if (cat == CAT_CLOUD) { * 6 *
         switch (subcat) {
            case CLOUD_COVER: * 1 *
               strcpy (comment, "Total cloud cover [%]");
               strcpy (name, "Sky");
               strcpy (unit, "[%]");
               return;
         }
      } else if (cat == CAT_MOISTURE_PROB) { * 10 *
         if (subcat == 8) { * grandfather'ed in. *
            strcpy (comment, "Prob of 0.01 In. of Precip [%]");
            strcpy (name, "PoP12");
            strcpy (unit, "[%]");
            return;
         }
      }
   } else if (prodType == 10) {
      if (cat == OCEAN_CAT_WAVES) { * 0 *
         if (subcat == OCEAN_WAVE_SIG_HT_WV) { * 5 *
            strcpy (comment, "Significant height of wind waves [m]");
            strcpy (name, "WaveHeight");
            strcpy (unit, "[m]");
            return;
         }
      }
   }
   strcpy (name, "");
   strcpy (comment, "unknown");
   strcpy (unit, "[-]");
   return;
}
*/

/*****************************************************************************
 * ComputeUnit() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Sets m, and b for equation y = mx + b, where x is in the unit
 * specified by GRIB2, and y is the one specified by f_unit.  The default
 * is m = 1, b = 0.
 *
 * Currently:
 *   For f_unit = 1 (English) we return Fahrenheit, knots, and inches for
 * temperature, wind speed, and amount of snow or rain.  The original units
 * are Kelvin, m/s, kg/m**2.
 *   For f_unit = 2 (metric) we return Celsius instead of Kelvin.
 *
 * ARGUMENTS
 *  convert = The enumerated type describing the type of conversion. (Input)
 * origName = Original unit name (needed for log10 option) (Input)
 *   f_unit = What type of unit to return (see above) (Input).
 *    unitM = M in equation y = m x + b (Output)
 *    unitB = B in equation y = m x + b (Output)
 *     name = Where to store the result (assumed already allocated to at
 *           least 15 bytes) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *   0 if we set M and B, 1 if we used defaults.
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Re-Created.
 *
 * NOTES
 *****************************************************************************
 */
int ComputeUnit (int convert, char *origName, sChar f_unit, double *unitM,
                 double *unitB, char *name)
{
   switch (convert) {
      case UC_NONE:
         break;
      case UC_K2F:     /* Convert from Kelvin to F or C. */
         if (f_unit == 1) {
            strcpy (name, "[F]");
            *unitM = 9. / 5.;
            /* 32 - (9/5 * 273.15) = 32 - 491.67 = -459.67. */
            *unitB = -459.67;
            return 0;
         } else if (f_unit == 2) {
            strcpy (name, "[C]");
            *unitM = 1;
            *unitB = -273.15;
            return 0;
         }
         break;
      case UC_InchWater: /* Convert from kg/(m^2) to inches water. */
         if (f_unit == 1) {
            strcpy (name, "[inch]");
            /*
             * kg/m**2 / density of water (1000 kg/m**3)
             * 1/1000 m * 1/2.54 in/cm * 100 cm/m = 1/25.4 inches
             */
            *unitM = 1. / 25.4;
            *unitB = 0;
            return 0;
         }
         break;
      case UC_M2Feet:  /* Convert from meters to feet. */
         if (f_unit == 1) {
            /* 1 (m) * (100cm/m) * (inch/2.54cm) * (ft/12inch) = X (ft) */
            strcpy (name, "[feet]");
            *unitM = 100. / 30.48;
            *unitB = 0;
            return 0;
         }
         break;
      case UC_M2Inch:  /* Convert from meters to inches. */
         if (f_unit == 1) {
            strcpy (name, "[inch]");
            *unitM = 100. / 2.54; /* inch / m */
            *unitB = 0;
            return 0;
         }
         break;
      case UC_M2StatuteMile: /* Convert from meters to statute miles. */
         if (f_unit == 1) {
            strcpy (name, "[statute mile]");
            *unitM = 1. / 1609.344; /* mile / m */
            *unitB = 0;
            return 0;
         }
         break;
         /* NCEP goes with a convention of 1 nm = 1853.248 m.
          * http://www.sizes.com/units/mile_USnautical.htm Shows that on
          * 7/1/1954 US Department of Commerce switched to 1 nm = 1852 m
          * (International standard.) */
      case UC_MS2Knots: /* Convert from m/s to knots. */
         if (f_unit == 1) {
            strcpy (name, "[knots]");
            *unitM = 3600. / 1852.; /* knot / m s**-1 */
            *unitB = 0;
            return 0;
         }
         break;
      case UC_UVIndex: /* multiply by Watts/ m**2 by 40 for the UV index. */
         if (f_unit == 1) {
            strcpy (name, "[UVI]");
            *unitM = 40;
            *unitB = 0;
            return 0;
         }
         break;
      case UC_LOG10:   /* convert from log10 (x) to x */
         if ((f_unit == 1) || (f_unit == 2)) {
            origName[strlen (origName) - 2] = '\0';
            if (strlen (origName) > 21)
               origName[21] = '\0';
            snprintf (name, 15, "[%s]", origName + 7);
            *unitM = -10; /* M = -10 => take 10^(x) */
            *unitB = 0;
            return 0;
         }
         break;
   }
   /* Default case is for the unit in the GRIB2 document. */
   strcpy (name, "[GRIB2 unit]");
   *unitM = 1;
   *unitB = 0;
   return 1;
}

/*****************************************************************************
 * ComputeUnit2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Sets m, and b for equation y = mx + b, where x is in the unit
 * specified by GRIB2, and y is the one specified by f_unit.  The default
 * is m = 1, b = 0.
 *
 * Currently:
 *   For f_unit = 1 (English) we return Fahrenheit, knots, and inches for
 * temperature, wind speed, and amount of snow or rain.  The original units
 * are Kelvin, m/s, kg/m**2.
 *   For f_unit = 2 (metric) we return Celsius instead of Kelvin.
 *
 * ARGUMENTS
 * prodType = The GRIB2, section 0 product type. (Input)
 *  templat = The GRIB2 section 4 template number. (Input)
 *      cat = The GRIB2 section 4 "General category of Product." (Input)
 *   subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 *   f_unit = What type of unit to return (see above) (Input).
 *    unitM = M in equation y = m x + b (Output)
 *    unitB = B in equation y = m x + b (Output)
 *     name = Where to store the result (assumed already allocated to at
 *            least 15 bytes) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *   0 if we set M and B, 1 if we used defaults.
 *
 * HISTORY
 *  11/2002 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
/*
static int ComputeUnit2 (int prodType, int templat, int cat, int subcat,
                         sChar f_unit, double *unitM, double *unitB,
                         char *name)
{
   if (prodType == 0) {
      switch (cat) {
         case CAT_TEMP:
            * subcat 8 is K/m, 10, 11 is W/m**2 *
            if ((subcat < 16) && (subcat != 8) &&
                (subcat != 10) && (subcat != 11)) {
               if (f_unit == 1) {
                  strcpy (name, "[F]");
                  *unitM = 9. / 5.;
                  * 32 - (9/5 * 273.15) = 32 - 491.67 = -459.67. *
                  *unitB = -459.67;
                  return 0;
               } else if (f_unit == 2) {
                  strcpy (name, "[C]");
                  *unitM = 1;
                  *unitB = -273.15;
                  return 0;
               }
            }
            break;
         case CAT_MOIST:
            if (subcat == MOIST_PRECIP_TOT) {
               if (templat != 9) { * template number != 9 implies QPF. *
                  if (f_unit == 1) {
                     strcpy (name, "[inch]");
                     *
                      * kg/m**2 / density of water (1000 kg/m**3)
                      * 1/1000 m * 1/2.54 in/cm * 100 cm/m = 1/25.4 inches
                      *
                     *unitM = 1. / 25.4;
                     *unitB = 0;
                     return 0;
                  }
               }
            }
            if ((subcat == MOIST_SNOWAMT) || (subcat == MOIST_TOT_SNOW)) {
               if (f_unit == 1) {
                  strcpy (name, "[inch]");
                  *unitM = 100. / 2.54; * inch / m *
                  *unitB = 0;
                  return 0;
               }
            }
            break;
         case CAT_MOMENT:
            if (subcat == MOMENT_WINDSPD) {
               if (f_unit == 1) {
                  strcpy (name, "[knots]");
                  *unitM = 3600. / 1852.; * knot / m s**-1 *
                  *unitB = 0;
                  return 0;
               }
            }
            break;
      }
   } else if (prodType == 10) {
      if (cat == OCEAN_CAT_WAVES) { * 0 *
         if (subcat == OCEAN_WAVE_SIG_HT_WV) { * 5 *
            if (f_unit == 1) {
               * 1 (m) * (100cm/m) * (inch/2.54cm) * (ft/12inch) = X (ft) *
               strcpy (name, "[feet]");
               *unitM = 100. / 30.48;
               *unitB = 0;
               return 0;
            }
         }
      }
   }
   * Default case is for the unit in the GRIB2 document. *
   strcpy (name, "[GRIB2 unit]");
   *unitM = 1;
   *unitB = 0;
   return 1;
}
*/

/* GRIB2 Code Table 4.5 */
/* *INDENT-OFF* */


/*****************************************************************************
 * Table45Lookup() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To figure out the entry in the "Surface" table (used for Code Table 4.5)
 *
 * ARGUMENTS
 *       code = The original index to look up. (Input)
 *     center = Center code (Input)
 *  subcenter = Subcenter code (Input)
 * f_reserved = If the index is a "reserved" index (Output)
 *  shortName = Pointer to short name of the parameter, or nullptr(Output)
 *       name = Pointer to longer name of the parameter, or nullptr (Output)
 *       unit = Pointer to unit name, or nullptr (Output)
 * FILES/DATABASES: None
 *
 * RETURNS: TRUE in case of success

 * NOTES
 *****************************************************************************
 */
int  Table45Lookup (int code,
                    uShort2 center,
                    uShort2 /* subcenter */,
                    int *f_reserved,
                    const char** shortName,
                    const char** name,
                    const char** unit
                    )
{
   *f_reserved = 1;
    if( shortName )
        *shortName = "RESERVED";
    if( name )
        *name = "Reserved";
    if( unit )
        *unit = "-";

   if ((code > 255) || (code < 0)) {
#ifdef DEBUG
      printf ("Surface index is out of 0..255 range?\n");
#endif
      return FALSE;
   }

   // Substantially changed by GDAL
   *f_reserved = 0;
   if( code > 191 && code < 255 && center != 7) {
       // Codes in range [192,254] are reserved for local use.
       // grib2_table_4_5.csv contains the codes valid for NCEP only
       // so for other centers, do not use the .csv file
        *f_reserved = 1;
        if( shortName )
            *shortName = "RESERVED";
        if( name )
            *name = "Reserved Local use";
        if( unit )
            *unit = "-";
        return TRUE;
   }

    const char* pszFilename = GetGRIB2_CSVFilename("grib2_table_4_5.csv");
    if( pszFilename == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find grib2_table_4_5.csv");
        return FALSE;
    }
    int iCode = CSVGetFileFieldId(pszFilename,"code");
    int iShortName = CSVGetFileFieldId(pszFilename,"short_name");
    int iName = CSVGetFileFieldId(pszFilename,"name");
    int iUnit = CSVGetFileFieldId(pszFilename,"unit");
    if( iCode < 0 || iShortName < 0 || iName < 0 || iUnit < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad structure for %s", pszFilename);
        return FALSE;
    }
    CSVRewind(pszFilename);
    while( char** papszFields = CSVGetNextLine(pszFilename) )
    {
        if( atoi(papszFields[iCode]) == code )
        {
            const char* pszShortName = papszFields[iShortName];
            if( code > 191 && code < 255 &&
                strcmp(papszFields[iName], "Reserved for local use") == 0 )
            {
                pszShortName = "RESERVED";
                *f_reserved = 1;
            }

            if( shortName )
                *shortName = pszShortName;
            if( name )
                *name = papszFields[iName];
            if( unit )
                *unit = papszFields[iUnit];
            return TRUE;
        }
    }

    return FALSE;
}

void ParseLevelName (unsigned short int center, unsigned short int subcenter,
                     uChar surfType, double value, sChar f_sndValue,
                     double sndValue, char **shortLevelName,
                     char **longLevelName)
{
   int f_reserved;
   char valBuff[512];
   char sndBuff[512];
   const char* surfshortname = nullptr;
   const char* surfname = nullptr;
   const char* surfunit = nullptr;
   Table45Lookup (surfType, center, subcenter,
                   &f_reserved, &surfshortname, &surfname, &surfunit);

   /* Check if index is defined... 191 is undefined. */
   free (*shortLevelName);
   *shortLevelName = nullptr;
   free (*longLevelName);
   *longLevelName = nullptr;
   snprintf (valBuff, sizeof(valBuff), "%f", value);
   strTrimRight (valBuff, '0');
   if (valBuff[strlen (valBuff) - 1] == '.') {
      valBuff[strlen (valBuff) - 1] = '\0';
   }
   if (f_sndValue) {
      snprintf (sndBuff, sizeof(sndBuff), "%f", sndValue);
      strTrimRight (sndBuff, '0');
      if (sndBuff[strlen (sndBuff) - 1] == '.') {
         sndBuff[strlen (sndBuff) - 1] = '\0';
      }
      if (f_reserved) {
         reallocSprintf (shortLevelName, "%s-%s-%s(%d)", valBuff, sndBuff,
                         surfshortname, surfType);
         reallocSprintf (longLevelName, "%s-%s[%s] %s(%d) (%s)", valBuff,
                         sndBuff, surfunit, surfshortname, surfType,
                         surfname);
      } else {
         reallocSprintf (shortLevelName, "%s-%s-%s", valBuff, sndBuff,
                         surfshortname);
         reallocSprintf (longLevelName, "%s-%s[%s] %s=\"%s\"", valBuff,
                         sndBuff, surfunit, surfshortname, surfname);
      }
   } else {
      if (f_reserved) {
         reallocSprintf (shortLevelName, "%s-%s(%d)", valBuff, surfshortname,
                         surfType);
         reallocSprintf (longLevelName, "%s[%s] %s(%d) (%s)", valBuff,
                         surfunit, surfshortname, surfType, surfname);
      } else {
         reallocSprintf (shortLevelName, "%s-%s", valBuff, surfshortname);
         reallocSprintf (longLevelName, "%s[%s] %s=\"%s\"", valBuff,
                         surfunit, surfshortname, surfname);
      }
   }
}

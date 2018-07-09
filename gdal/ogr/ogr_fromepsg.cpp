/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Generate an OGRSpatialReference object based on an EPSG
 *           PROJCS, or GEOGCS code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_srs_api.h"

#include <cctype>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <limits>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

extern void OGRsnPrintDouble( char * pszStrBuf, size_t size, double dfValue );

int EPSGGetWGS84Transform( int nGeogCS, std::vector<CPLString>& asTransform );
void OGREPSGDatumNameMassage( char ** ppszDatum );

void CleanupFindMatchesCacheAndMutex();

static const char * const apszDatumEquiv[] =
{
    "Militar_Geographische_Institut",
    "Militar_Geographische_Institute",
    "World_Geodetic_System_1984",
    "WGS_1984",
    "WGS_72_Transit_Broadcast_Ephemeris",
    "WGS_1972_Transit_Broadcast_Ephemeris",
    "World_Geodetic_System_1972",
    "WGS_1972",
    "European_Terrestrial_Reference_System_89",
    "European_Reference_System_1989",
    nullptr
};

static CPLMutex* hFindMatchesMutex = nullptr;
static std::vector<OGRSpatialReference*>* papoSRSCache_PROJCS = nullptr;
static std::vector<OGRSpatialReference*>* papoSRSCache_GEOGCS = nullptr;
static std::map<CPLString, int>* poMapESRIPROJCSNameToEPSGCode = nullptr;
static std::map<CPLString, int>* poMapESRIGEOGCSNameToEPSGCode = nullptr;

/************************************************************************/
/*                      OGREPSGDatumNameMassage()                       */
/*                                                                      */
/*      Massage an EPSG datum name into WMT format.  Also transform     */
/*      specific exception cases into WKT versions.                     */
/************************************************************************/

void OGREPSGDatumNameMassage( char ** ppszDatum )

{
    char *pszDatum = *ppszDatum;

    if( pszDatum[0] == '\0' )
        return;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( int i = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[i] != '+'
            && !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
            && !(pszDatum[i] >= 'a' && pszDatum[i] <= 'z')
            && !(pszDatum[i] >= '0' && pszDatum[i] <= '9') )
        {
            pszDatum[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    int j = 0;  // Used after for loop.
    for( int i = 1; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[j] == '_' && pszDatum[i] == '_' )
            continue;

        pszDatum[++j] = pszDatum[i];
    }
    if( pszDatum[j] == '_' )
        pszDatum[j] = '\0';
    else
        pszDatum[j+1] = '\0';

/* -------------------------------------------------------------------- */
/*      Search for datum equivelences.  Specific massaged names get     */
/*      mapped to OpenGIS specified names.                              */
/* -------------------------------------------------------------------- */
    for( int i = 0; apszDatumEquiv[i] != nullptr; i += 2 )
    {
        if( EQUAL(*ppszDatum, apszDatumEquiv[i]) )
        {
            CPLFree( *ppszDatum );
            *ppszDatum = CPLStrdup( apszDatumEquiv[i+1] );
            break;
        }
    }
}

/************************************************************************/
/*                        EPSGAngleStringToDD()                         */
/*                                                                      */
/*      Convert an angle in the specified units to decimal degrees.     */
/************************************************************************/

static double
EPSGAngleStringToDD( const char * pszAngle, int nUOMAngle )

{
    double dfAngle = 0.0;

    if( nUOMAngle == 9110 )  // DDD.MMSSsss
    {
        dfAngle = std::abs(atoi(pszAngle));
        const char *pszDecimal = strchr(pszAngle, '.');
        if( pszDecimal != nullptr && strlen(pszDecimal) > 1 )
        {
            char szMinutes[3] = { '\0', '\0', '\0' };

            szMinutes[0] = pszDecimal[1];
            if( pszDecimal[2] >= '0' && pszDecimal[2] <= '9' )
                szMinutes[1] = pszDecimal[2];
            else
                szMinutes[1] = '0';

            dfAngle += atoi(szMinutes) / 60.0;

            if( strlen(pszDecimal) > 3 )
            {
                char szSeconds[64] = { '\0' };
                szSeconds[0] = pszDecimal[3];
                if( pszDecimal[4] >= '0' && pszDecimal[4] <= '9' )
                {
                    szSeconds[1] = pszDecimal[4];
                    szSeconds[2] = '.';
                    strncpy( szSeconds+3, pszDecimal + 5, sizeof(szSeconds)-3-1 );
                    szSeconds[sizeof(szSeconds)-1] = 0;
                }
                else
                {
                    szSeconds[1] = '0';
                    szSeconds[2] = '\0';
                }
                dfAngle += CPLAtof(szSeconds) / 3600.0;
            }
        }

        if( pszAngle[0] == '-' )
            dfAngle *= -1;
    }
    else if( nUOMAngle == 9105 || nUOMAngle == 9106 )   // Grad.
    {
        dfAngle = 180.0 * (CPLAtof(pszAngle ) / 200.0);
    }
    else if( nUOMAngle == 9101 )                        // Radians.
    {
        dfAngle = 180.0 * (CPLAtof(pszAngle ) / M_PI);
    }
    else if( nUOMAngle == 9103 )                        // Arc-minute.
    {
        dfAngle = CPLAtof(pszAngle) / 60.0;
    }
    else if( nUOMAngle == 9104 )                        // Arc-second.
    {
        dfAngle = CPLAtof(pszAngle) / 3600.0;
    }
    else  // Decimal degrees.  Some cases missing, but seemingly never used.
    {
        CPLAssert( nUOMAngle == 9102 || nUOMAngle == 0 );

        dfAngle = CPLAtof(pszAngle );
    }

    return dfAngle;
}

/************************************************************************/
/*                        EPSGGetUOMAngleInfo()                         */
/************************************************************************/

static bool EPSGGetUOMAngleInfo( int nUOMAngleCode,
                                 char **ppszUOMName,
                                 double * pdfInDegrees )

{
    // We do a special override of some of the DMS formats name
    // This will also solve accuracy problems when computing
    // the dfInDegree value from the CSV values (#3643).
    if( nUOMAngleCode == 9102 || nUOMAngleCode == 9107
        || nUOMAngleCode == 9108 || nUOMAngleCode == 9110
        || nUOMAngleCode == 9122 )
    {
        if( ppszUOMName != nullptr )
            *ppszUOMName = CPLStrdup("degree");
        if( pdfInDegrees != nullptr )
            *pdfInDegrees = 1.0;
        return true;
    }

    const char *pszFilename = CSVFilename( "unit_of_measure.csv" );

    char szSearchKey[24] = { '\0' };
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nUOMAngleCode );

    const char *pszUOMName = CSVGetField( pszFilename,
                                          "UOM_CODE", szSearchKey, CC_Integer,
                                          "UNIT_OF_MEAS_NAME" );

/* -------------------------------------------------------------------- */
/*      If the file is found, read from there.  Note that FactorC is    */
/*      an empty field for any of the DMS style formats, and in this    */
/*      case we really want to return the default InDegrees value       */
/*      (1.0) from above.                                               */
/* -------------------------------------------------------------------- */
    double dfInDegrees = 1.0;

    if( !EQUAL( pszUOMName, "" ) )
    {
        const double dfFactorB =
            CPLAtof(CSVGetField( pszFilename,
                                 "UOM_CODE", szSearchKey, CC_Integer,
                                 "FACTOR_B" ));

        const double dfFactorC =
            CPLAtof(CSVGetField( pszFilename,
                                 "UOM_CODE", szSearchKey, CC_Integer,
                                 "FACTOR_C" ));

        if( dfFactorC != 0.0 )
            dfInDegrees = (dfFactorB / dfFactorC) * (180.0 / M_PI);

        // For some reason, (FactorB) is not very precise in EPSG, use
        // a more exact form for grads.
        if( nUOMAngleCode == 9105 )
            dfInDegrees = 180.0 / 200.0;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise handle a few well known units directly.               */
/* -------------------------------------------------------------------- */
    else
    {
        switch( nUOMAngleCode )
        {
          case 9101:
            pszUOMName = "radian";
            dfInDegrees = 180.0 / M_PI;
            break;

          case 9103:
            pszUOMName = "arc-minute";
            dfInDegrees = 1 / 60.0;
            break;

          case 9104:
            pszUOMName = "arc-second";
            dfInDegrees = 1 / 3600.0;
            break;

          case 9105:
            pszUOMName = "grad";
            dfInDegrees = 180.0 / 200.0;
            break;

          case 9106:
            pszUOMName = "gon";
            dfInDegrees = 180.0 / 200.0;
            break;

          case 9109:
            pszUOMName = "microradian";
            dfInDegrees = 180.0 / (M_PI * 1000000.0);
            break;

          default:
            return false;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return to caller.                                               */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != nullptr )
        *ppszUOMName = CPLStrdup( pszUOMName );

    if( pdfInDegrees != nullptr )
        *pdfInDegrees = dfInDegrees;

    return true;
}

/************************************************************************/
/*                        EPSGGetUOMLengthInfo()                        */
/*                                                                      */
/*      Note: This function should eventually also know how to          */
/*      lookup length aliases in the UOM_LE_ALIAS table.                */
/************************************************************************/

static bool
EPSGGetUOMLengthInfo( int nUOMLengthCode,
                      char **ppszUOMName,
                      double * pdfInMeters )

{
/* -------------------------------------------------------------------- */
/*      We short cut meter to save work in the most common case.        */
/* -------------------------------------------------------------------- */
    if( nUOMLengthCode == 9001 )
    {
        if( ppszUOMName != nullptr )
            *ppszUOMName = CPLStrdup( "metre" );
        if( pdfInMeters != nullptr )
            *pdfInMeters = 1.0;

        return true;
    }

/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    const char *uom_filename = CSVFilename( "unit_of_measure.csv" );

    char szSearchKey[24] = { '\0' };
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nUOMLengthCode );
    char **papszUnitsRecord =
        CSVScanFileByName( uom_filename, "UOM_CODE", szSearchKey, CC_Integer );

    if( papszUnitsRecord == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != nullptr )
    {
        const int iNameField =
            CSVGetFileFieldId( uom_filename, "UNIT_OF_MEAS_NAME" );
        *ppszUOMName = CPLStrdup( CSLGetField(papszUnitsRecord, iNameField) );
    }

/* -------------------------------------------------------------------- */
/*      Get the A and B factor fields, and create the multiplicative    */
/*      factor.                                                         */
/* -------------------------------------------------------------------- */
    if( pdfInMeters != nullptr )
    {
        const int iBFactorField = CSVGetFileFieldId( uom_filename, "FACTOR_B" );
        const int iCFactorField = CSVGetFileFieldId( uom_filename, "FACTOR_C" );

        if( CPLAtof(CSLGetField(papszUnitsRecord, iCFactorField)) > 0.0 )
            *pdfInMeters = CPLAtof(CSLGetField(papszUnitsRecord, iBFactorField))
                / CPLAtof(CSLGetField(papszUnitsRecord, iCFactorField));
        else
            *pdfInMeters = 0.0;
    }

    return true;
}

/************************************************************************/
/*                         EPSGNegateString()                           */
/************************************************************************/

static void EPSGNegateString(CPLString& osValue)
{
    if( osValue.compare("0") == 0 )
        return;
    if( osValue[0] == '-' )
    {
        osValue = osValue.substr(1);
        return;
    }
    if( osValue[0] == '+' )
    {
        osValue[0] = '-';
        return;
    }
    osValue = "-" + osValue;
}

/************************************************************************/
/*                       EPSGGetWGS84Transform()                        */
/*                                                                      */
/*      The following code attempts to find a bursa-wolf                */
/*      transformation from this GeogCS to WGS84 (4326).                */
/*                                                                      */
/*      Faults:                                                         */
/*       o I think there are codes other than 9603 and 9607 that        */
/*         return compatible, or easily transformed parameters.         */
/*       o Only the first path from the given GeogCS is checked due     */
/*         to limitations in the CSV API.                               */
/************************************************************************/

int EPSGGetWGS84Transform( int nGeogCS, std::vector<CPLString>& asTransform )

{
/* -------------------------------------------------------------------- */
/*      Fetch the line from the GCS table.                              */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CSVFilename("gcs.override.csv");
    char szCode[32] = { '\0' };
    snprintf( szCode, sizeof(szCode), "%d", nGeogCS );
    char **papszLine = CSVScanFileByName(
        pszFilename, "COORD_REF_SYS_CODE", szCode, CC_Integer );
    if( papszLine == nullptr )
    {
        pszFilename = CSVFilename("gcs.csv");
        snprintf( szCode, sizeof(szCode), "%d", nGeogCS );
        papszLine = CSVScanFileByName( pszFilename,
                                       "COORD_REF_SYS_CODE",
                                       szCode, CC_Integer );
    }

    if( papszLine == nullptr )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Verify that the method code is one of our accepted ones.        */
/* -------------------------------------------------------------------- */
    const int nMethodCode =
        atoi(CSLGetField( papszLine,
                          CSVGetFileFieldId(pszFilename,
                                            "COORD_OP_METHOD_CODE")));
    if( nMethodCode != 9603 && nMethodCode != 9607 && nMethodCode != 9606 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch the transformation parameters.                            */
/* -------------------------------------------------------------------- */
    const int iDXField = CSVGetFileFieldId(pszFilename, "DX");
    if( iDXField < 0 || CSLCount(papszLine) < iDXField + 7 )
        return FALSE;

    asTransform.clear();
    asTransform.reserve(7);
    for( int iField = 0; iField < 7; iField++ )
    {
        const char* pszValue = papszLine[iDXField+iField];
        if( pszValue[0] )
            asTransform.emplace_back(pszValue);
        else
            asTransform.emplace_back("0");
    }

/* -------------------------------------------------------------------- */
/*      9607 - coordinate frame rotation has reverse signs on the       */
/*      rotational coefficients.  Fix up now since we internal          */
/*      operate according to method 9606 (position vector 7-parameter). */
/* -------------------------------------------------------------------- */
    if( nMethodCode == 9607 )
    {
        EPSGNegateString(asTransform[3]);
        EPSGNegateString(asTransform[4]);
        EPSGNegateString(asTransform[5]);
    }

    return TRUE;
}

/************************************************************************/
/*                           EPSGGetPMInfo()                            */
/*                                                                      */
/*      Get the offset between a given prime meridian and Greenwich     */
/*      in degrees.                                                     */
/************************************************************************/

static bool
EPSGGetPMInfo( int nPMCode, char ** ppszName, double *pdfOffset )

{
/* -------------------------------------------------------------------- */
/*      Use a special short cut for Greenwich, since it is so common.   */
/* -------------------------------------------------------------------- */
    // FIXME? Where does 7022 come from ? Let's keep it just in case
    // 8901 is the official current code for Greenwich.
    if( nPMCode == 7022 /* PM_Greenwich */ || nPMCode == 8901 )
    {
        if( pdfOffset != nullptr )
            *pdfOffset = 0.0;
        if( ppszName != nullptr )
            *ppszName = CPLStrdup( "Greenwich" );
        return true;
    }

/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    char szSearchKey[24] = { '\0' };
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nPMCode );

    const char *PM_FILENAME = CSVFilename("prime_meridian.csv");
    const int nUOMAngle =
        atoi(CSVGetField( PM_FILENAME,
                          "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                          "UOM_CODE" ) );
    if( nUOMAngle < 1 )
        return false;

/* -------------------------------------------------------------------- */
/*      Get the PM offset.                                              */
/* -------------------------------------------------------------------- */
    if( pdfOffset != nullptr )
    {
        *pdfOffset =
            EPSGAngleStringToDD(
                CSVGetField( PM_FILENAME,
                             "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                             "GREENWICH_LONGITUDE" ),
                nUOMAngle );
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != nullptr )
        *ppszName =
            CPLStrdup(
                CSVGetField( PM_FILENAME,
                             "PRIME_MERIDIAN_CODE", szSearchKey, CC_Integer,
                             "PRIME_MERIDIAN_NAME" ));

    return true;
}

/************************************************************************/
/*                           EPSGGetGCSInfo()                           */
/*                                                                      */
/*      Fetch the datum, and prime meridian related to a particular     */
/*      GCS.                                                            */
/************************************************************************/

static bool
EPSGGetGCSInfo( int nGCSCode, char ** ppszName,
                int * pnDatum, char **ppszDatumName,
                int * pnPM, int *pnEllipsoid, int *pnUOMAngle,
                int * pnCoordSysCode )

{
/* -------------------------------------------------------------------- */
/*      Search the database for the corresponding datum code.           */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CSVFilename("gcs.override.csv");
    char szSearchKey[24] = { '\0' };
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nGCSCode );

    char **papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                            szSearchKey, CC_Integer );

    if( papszRecord == nullptr )
    {
        pszFilename = CSVFilename( "gcs.csv" );
        snprintf( szSearchKey, sizeof(szSearchKey), "%d", nGCSCode );
        papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                         szSearchKey, CC_Integer );
    }

    if( papszRecord == nullptr )
        return false;

    int nDatum = atoi(CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "DATUM_CODE")));
    if( nDatum < 1 )
        return false;

    if( pnDatum != nullptr )
        *pnDatum = nDatum;

/* -------------------------------------------------------------------- */
/*      Get the PM.                                                     */
/* -------------------------------------------------------------------- */
    const int nPM = atoi(CSLGetField( papszRecord,
                            CSVGetFileFieldId(pszFilename,
                                           "PRIME_MERIDIAN_CODE" ) ));

    if( nPM < 1 )
        return false;

    if( pnPM != nullptr )
        *pnPM = nPM;

/* -------------------------------------------------------------------- */
/*      Get the Ellipsoid.                                              */
/* -------------------------------------------------------------------- */
    const int nEllipsoid = atoi(CSLGetField( papszRecord,
                                    CSVGetFileFieldId(pszFilename,
                                           "ELLIPSOID_CODE" ) ));

    if( nEllipsoid < 1 )
        return false;

    if( pnEllipsoid != nullptr )
        *pnEllipsoid = nEllipsoid;

/* -------------------------------------------------------------------- */
/*      Get the angular units.                                          */
/* -------------------------------------------------------------------- */
    const int nUOMAngle = atoi(CSLGetField( papszRecord,
                                    CSVGetFileFieldId(pszFilename,
                                            "UOM_CODE" ) ));

    if( nUOMAngle < 1 )
        return false;

    if( pnUOMAngle != nullptr )
        *pnUOMAngle = nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != nullptr )
    {
        CPLString osGCSName =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "COORD_REF_SYS_NAME" ));

        const char *pszDeprecated =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "DEPRECATED") );

        if( pszDeprecated != nullptr && *pszDeprecated == '1' )
            osGCSName += " (deprecated)";

        *ppszName = CPLStrdup(osGCSName);
    }

/* -------------------------------------------------------------------- */
/*      Get the datum name, if requested.                               */
/* -------------------------------------------------------------------- */
    if( ppszDatumName != nullptr )
        *ppszDatumName =
            CPLStrdup(CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "DATUM_NAME" )));

/* -------------------------------------------------------------------- */
/*      Get the CoordSysCode                                            */
/* -------------------------------------------------------------------- */
    const int nCSC = atoi(CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "COORD_SYS_CODE" ) ));

    if( pnCoordSysCode != nullptr )
        *pnCoordSysCode = nCSC;

    return TRUE;
}

/************************************************************************/
/*                         OSRGetEllipsoidInfo()                        */
/************************************************************************/

/**
 * Fetch info about an ellipsoid.
 *
 * This helper function will return ellipsoid parameters corresponding to EPSG
 * code provided. Axes are always returned in meters.  Semi major computed
 * based on inverse flattening where that is provided.
 *
 * @param nCode EPSG code of the requested ellipsoid
 *
 * @param ppszName pointer to string where ellipsoid name will be returned. It
 * is caller responsibility to free this string after using with CPLFree().
 *
 * @param pdfSemiMajor pointer to variable where semi major axis will be
 * returned.
 *
 * @param pdfInvFlattening pointer to variable where inverse flattening will
 * be returned.
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 **/

OGRErr
OSRGetEllipsoidInfo( int nCode, char ** ppszName,
                     double * pdfSemiMajor, double * pdfInvFlattening )

{
/* -------------------------------------------------------------------- */
/*      Get the semi major axis.                                        */
/* -------------------------------------------------------------------- */
    char szSearchKey[24] = { '\0' };
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nCode );
    szSearchKey[sizeof(szSearchKey) - 1] = '\n';

    double dfSemiMajor =
        CPLAtof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                             "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                             "SEMI_MAJOR_AXIS" ) );
    if( dfSemiMajor == 0.0 )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Get the translation factor into meters.                         */
/* -------------------------------------------------------------------- */
    const int nUOMLength = atoi(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                             "ELLIPSOID_CODE", szSearchKey,
                                             CC_Integer,
                                             "UOM_CODE" ));
    double dfToMeters = 1.0;
    if( !EPSGGetUOMLengthInfo( nUOMLength, nullptr, &dfToMeters ) )
    {
        dfToMeters = 1.0;
    }

    dfSemiMajor *= dfToMeters;

    if( pdfSemiMajor != nullptr )
        *pdfSemiMajor = dfSemiMajor;

/* -------------------------------------------------------------------- */
/*      Get the semi-minor if requested.  If the Semi-minor axis        */
/*      isn't available, compute it based on the inverse flattening.    */
/* -------------------------------------------------------------------- */
    if( pdfInvFlattening != nullptr )
    {
        *pdfInvFlattening =
            CPLAtof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                 "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                 "INV_FLATTENING" ));

        if( *pdfInvFlattening == 0.0 )
        {
            const double dfSemiMinor =
                CPLAtof(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                  "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                  "SEMI_MINOR_AXIS" )) * dfToMeters;

            if( dfSemiMajor == 0.0 )
                *pdfInvFlattening = 0.0;
            else
                *pdfInvFlattening =
                    OSRCalcInvFlattening(dfSemiMajor, dfSemiMinor);
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != nullptr )
        *ppszName =
            CPLStrdup(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "ELLIPSOID_NAME" ));

    return OGRERR_NONE;
}

constexpr int CoLatConeAxis =        1036;  // See #4223.
constexpr int NatOriginLat =         8801;
constexpr int NatOriginLong =        8802;
constexpr int NatOriginScaleFactor = 8805;
constexpr int FalseEasting =         8806;
constexpr int FalseNorthing =        8807;
constexpr int ProjCenterLat =        8811;
constexpr int ProjCenterLong =       8812;
constexpr int Azimuth =              8813;
constexpr int AngleRectifiedToSkewedGrid = 8814;
constexpr int InitialLineScaleFactor = 8815;
constexpr int ProjCenterEasting =    8816;
constexpr int ProjCenterNorthing =   8817;
constexpr int PseudoStdParallelLat = 8818;
constexpr int PseudoStdParallelScaleFactor = 8819;
constexpr int FalseOriginLat =       8821;
constexpr int FalseOriginLong =      8822;
constexpr int StdParallel1Lat =      8823;
constexpr int StdParallel2Lat =      8824;
constexpr int FalseOriginEasting =   8826;
constexpr int FalseOriginNorthing =  8827;
constexpr int SphericalOriginLat =   8828;
constexpr int SphericalOriginLong =  8829;
#if 0
constexpr int InitialLongitude =     8830;
constexpr int ZoneWidth =            8831;
#endif
constexpr int PolarLatStdParallel =  8832;
constexpr int PolarLongOrigin =      8833;

/************************************************************************/
/*                         EPSGGetProjTRFInfo()                         */
/*                                                                      */
/*      Transform a PROJECTION_TRF_CODE into a projection method,       */
/*      and a set of parameters.  The parameters identify will          */
/*      depend on the returned method, but they will all have been      */
/*      normalized into degrees and meters.                             */
/************************************************************************/

static bool
EPSGGetProjTRFInfo( int nPCS, int * pnProjMethod,
                    int *panParmIds, double * padfProjParms )

{
/* -------------------------------------------------------------------- */
/*      Get the proj method.  If this fails to return a meaningful      */
/*      number, then the whole function fails.                          */
/* -------------------------------------------------------------------- */
    CPLString osFilename = CSVFilename( "pcs.override.csv" );
    char szTRFCode[16] = { '\0' };
    snprintf( szTRFCode, sizeof(szTRFCode), "%d", nPCS );

    int nProjMethod =
        atoi( CSVGetField( osFilename,
                           "COORD_REF_SYS_CODE", szTRFCode, CC_Integer,
                           "COORD_OP_METHOD_CODE" ) );
    if( nProjMethod == 0 )
    {
        osFilename = CSVFilename( "pcs.csv" );
        snprintf( szTRFCode, sizeof(szTRFCode), "%d", nPCS );
        nProjMethod =
            atoi( CSVGetField( osFilename,
                               "COORD_REF_SYS_CODE", szTRFCode, CC_Integer,
                               "COORD_OP_METHOD_CODE" ) );
        if( nProjMethod == 0 )
            return false;
    }

/* -------------------------------------------------------------------- */
/*      Get the parameters for this projection.                         */
/* -------------------------------------------------------------------- */
    double adfProjParms[7] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

    for( int i = 0; i < 7; i++ )
    {
        if( panParmIds == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "panParmIds cannot be NULL." );
            return false;
        }

        char szParamUOMID[32] = { '\0' };
        char szParamValueID[32] = { '\0' };
        char szParamCodeID[32] = { '\0' };

        snprintf( szParamCodeID, sizeof(szParamCodeID),
                  "PARAMETER_CODE_%d", i+1 );
        snprintf( szParamUOMID, sizeof(szParamUOMID), "PARAMETER_UOM_%d", i+1 );
        snprintf( szParamValueID, sizeof(szParamValueID),
                  "PARAMETER_VALUE_%d", i+1 );

        panParmIds[i] =
            atoi(CSVGetField( osFilename, "COORD_REF_SYS_CODE", szTRFCode,
                              CC_Integer, szParamCodeID ));

        int nUOM = atoi(CSVGetField( osFilename, "COORD_REF_SYS_CODE",
                                     szTRFCode,
                                     CC_Integer, szParamUOMID ));
        char *pszValue = CPLStrdup(
            CSVGetField( osFilename, "COORD_REF_SYS_CODE", szTRFCode,
                         CC_Integer, szParamValueID ));

        // there is a bug in the EPSG 6.2.2 database for PCS 2935 and 2936
        // such that they have foot units for the scale factor.  Avoid this.
        if( (panParmIds[i] == NatOriginScaleFactor
             || panParmIds[i] == InitialLineScaleFactor
             || panParmIds[i] == PseudoStdParallelScaleFactor)
            && nUOM < 9200 )
            nUOM = 9201;

        if( nUOM >= 9100 && nUOM < 9200 )
        {
            adfProjParms[i] = EPSGAngleStringToDD( pszValue, nUOM );
        }
        else if( nUOM > 9000 && nUOM < 9100 )
        {
            double dfInMeters = 0.0;

            if( !EPSGGetUOMLengthInfo( nUOM, nullptr, &dfInMeters ) )
                dfInMeters = 1.0;
            adfProjParms[i] = CPLAtof(pszValue) * dfInMeters;
        }
        else if( EQUAL(pszValue, "") )  // Null field.
        {
            adfProjParms[i] = 0.0;
        }
        else // Really, should consider looking up other scaling factors.
        {
            if( nUOM != 9201 )
                CPLDebug( "OGR",
                          "Non-unity scale factor units! (UOM=%d, PCS=%d)",
                          nUOM, nPCS );
            adfProjParms[i] = CPLAtof(pszValue);
        }

        CPLFree( pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Transfer requested data into passed variables.                  */
/* -------------------------------------------------------------------- */
    if( pnProjMethod != nullptr )
        *pnProjMethod = nProjMethod;

    if( padfProjParms != nullptr )
    {
        for( int i = 0; i < 7; i++ )
            padfProjParms[i] = adfProjParms[i];
    }

    return true;
}

/************************************************************************/
/*                           EPSGGetPCSInfo()                           */
/************************************************************************/

static bool
EPSGGetPCSInfo( int nPCSCode, char **ppszEPSGName,
                int *pnUOMLengthCode, int *pnUOMAngleCode,
                int *pnGeogCS, int *pnTRFCode, int *pnCoordSysCode,
                double* adfTOWGS84 )

{

/* -------------------------------------------------------------------- */
/*      Search the units database for this unit.  If we don't find      */
/*      it return failure.                                              */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CSVFilename( "pcs.override.csv" );
    char szSearchKey[24] = { '\0' };
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nPCSCode );
    char **papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                            szSearchKey, CC_Integer );

    if( papszRecord == nullptr )
    {
        pszFilename = CSVFilename( "pcs.csv" );
        snprintf( szSearchKey, sizeof(szSearchKey), "%d", nPCSCode );
        papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                         szSearchKey, CC_Integer );
    }

    if( papszRecord == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszEPSGName != nullptr )
    {
        CPLString osPCSName =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "COORD_REF_SYS_NAME"));

        const char *pszDeprecated =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "DEPRECATED") );

        if( pszDeprecated != nullptr && *pszDeprecated == '1' )
            osPCSName += " (deprecated)";

        *ppszEPSGName = CPLStrdup(osPCSName);
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Length code, if requested.                          */
/* -------------------------------------------------------------------- */
    if( pnUOMLengthCode != nullptr )
    {
        const char *pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename, "UOM_CODE"));
        if( atoi(pszValue) > 0 )
            *pnUOMLengthCode = atoi(pszValue);
        else
            *pnUOMLengthCode = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Angle code, if requested.                           */
/* -------------------------------------------------------------------- */
    if( pnUOMAngleCode != nullptr )
    {
        const char *pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename, "UOM_ANGLE_CODE") );

        if( atoi(pszValue) > 0 )
            *pnUOMAngleCode = atoi(pszValue);
        else
            *pnUOMAngleCode = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the GeogCS (Datum with PM) code, if requested.              */
/* -------------------------------------------------------------------- */
    if( pnGeogCS != nullptr )
    {
        const char *pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename, "SOURCE_GEOGCRS_CODE"));
        if( atoi(pszValue) > 0 )
            *pnGeogCS = atoi(pszValue);
        else
            *pnGeogCS = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the GeogCS (Datum with PM) code, if requested.              */
/* -------------------------------------------------------------------- */
    if( pnTRFCode != nullptr )
    {
        const char *pszValue =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename, "COORD_OP_CODE"));

        if( atoi(pszValue) > 0 )
            *pnTRFCode = atoi(pszValue);
        else
            *pnTRFCode = 0;
    }

/* -------------------------------------------------------------------- */
/*      Get the CoordSysCode                                            */
/* -------------------------------------------------------------------- */
    const int nCSC = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                       szSearchKey, CC_Integer,
                                       "COORD_SYS_CODE" ) );

    if( pnCoordSysCode != nullptr )
        *pnCoordSysCode = nCSC;

/* -------------------------------------------------------------------- */
/*      Get the TOWGS84 (override) parameters                           */
/* -------------------------------------------------------------------- */

    const char *pszDX =
        CSLGetField( papszRecord, CSVGetFileFieldId(pszFilename, "DX"));
    if( pszDX[0] != '\0' )
    {
        adfTOWGS84[0] = CPLAtof(pszDX);
        adfTOWGS84[1] = CPLAtof(CSLGetField( papszRecord, CSVGetFileFieldId(pszFilename, "DY")));
        adfTOWGS84[2] = CPLAtof(CSLGetField( papszRecord, CSVGetFileFieldId(pszFilename, "DZ")));
        adfTOWGS84[3] = CPLAtof(CSLGetField( papszRecord, CSVGetFileFieldId(pszFilename, "RX")));
        adfTOWGS84[4] = CPLAtof(CSLGetField( papszRecord, CSVGetFileFieldId(pszFilename, "RY")));
        adfTOWGS84[5] = CPLAtof(CSLGetField( papszRecord, CSVGetFileFieldId(pszFilename, "RZ")));
        adfTOWGS84[6] = CPLAtof(CSLGetField( papszRecord, CSVGetFileFieldId(pszFilename, "DS")));
    }

    return true;
}

/************************************************************************/
/*                          SetEPSGAxisInfo()                           */
/************************************************************************/

static OGRErr SetEPSGAxisInfo( OGRSpatialReference *poSRS,
                               const char *pszTargetKey,
                               int nCoordSysCode )

{
/* -------------------------------------------------------------------- */
/*      Special cases for well known and common values.  We short       */
/*      circuit these to save time doing file lookups.                  */
/* -------------------------------------------------------------------- */
    // Conventional and common Easting/Northing values.
    if( nCoordSysCode >= 4400 && nCoordSysCode <= 4410 )
    {
        return
            poSRS->SetAxes( pszTargetKey,
                            "Easting", OAO_East,
                            "Northing", OAO_North );
    }

    // Conventional and common Easting/Northing values.
    if( nCoordSysCode >= 6400 && nCoordSysCode <= 6423 )
    {
        return
            poSRS->SetAxes( pszTargetKey,
                            "Latitude", OAO_North,
                            "Longitude", OAO_East );
    }

/* -------------------------------------------------------------------- */
/*      Get the definition from the coordinate_axis.csv file.           */
/* -------------------------------------------------------------------- */
    char szSearchKey[24] = { '\0' };
    const char *pszFilename = CSVFilename( "coordinate_axis.csv" );
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nCoordSysCode );
    char **papszRecord = CSVScanFileByName( pszFilename, "COORD_SYS_CODE",
                                            szSearchKey, CC_Integer );

    char **papszAxis1 = nullptr;
    char **papszAxis2 = nullptr;
    if( papszRecord != nullptr )
    {
        papszAxis1 = CSLDuplicate( papszRecord );
        papszRecord = CSVGetNextLine( pszFilename );
        if( CSLCount(papszRecord) > 0
            && EQUAL(papszRecord[0], papszAxis1[0]) )
            papszAxis2 = CSLDuplicate( papszRecord );
    }

    if( papszAxis2 == nullptr )
    {
        CSLDestroy( papszAxis1 );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find entries for COORD_SYS_CODE %d "
                  "in coordinate_axis.csv",
                  nCoordSysCode );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the records are complete, and work out which columns    */
/*      are which.                                                      */
/* -------------------------------------------------------------------- */
    const int iAxisOrientationField =
        CSVGetFileFieldId( pszFilename, "coord_axis_orientation" );
    const int iAxisAbbrevField =
        CSVGetFileFieldId( pszFilename, "coord_axis_abbreviation" );
    const int iAxisOrderField =
        CSVGetFileFieldId( pszFilename, "coord_axis_order" );
    const int iAxisNameCodeField =
        CSVGetFileFieldId( pszFilename, "coord_axis_name_code" );

    // Check that all fields are available and that the axis_order field
    // is the one with highest index.
    if( !(iAxisOrientationField >= 0 &&
          iAxisOrientationField < iAxisOrderField &&
          iAxisAbbrevField >= 0 &&
          iAxisAbbrevField < iAxisOrderField &&
          iAxisOrderField >= 0 &&
          iAxisNameCodeField >= 0 &&
          iAxisNameCodeField < iAxisOrderField) )
    {
        CSLDestroy( papszAxis1 );
        CSLDestroy( papszAxis2 );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "coordinate_axis.csv corrupted" );
        return OGRERR_FAILURE;
    }

    if( CSLCount(papszAxis1) < iAxisOrderField+1
        || CSLCount(papszAxis2) < iAxisOrderField+1 )
    {
        CSLDestroy( papszAxis1 );
        CSLDestroy( papszAxis2 );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Axis records appear incomplete for COORD_SYS_CODE %d "
                  "in coordinate_axis.csv",
                  nCoordSysCode );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to switch the axes around?                           */
/* -------------------------------------------------------------------- */
    if( atoi(papszAxis2[iAxisOrderField]) < atoi(papszAxis1[iAxisOrderField]) )
    {
        papszRecord = papszAxis1;
        papszAxis1 = papszAxis2;
        papszAxis2 = papszRecord;
    }

/* -------------------------------------------------------------------- */
/*      Work out axis enumeration values.                               */
/* -------------------------------------------------------------------- */
    OGRAxisOrientation eOAxis1 = OAO_Other;
    OGRAxisOrientation eOAxis2 = OAO_Other;
    constexpr int anCodes[7] = { -1, 9907, 9909, 9906, 9908, -1, -1 };

    for( int iAO = 0; iAO < 7; iAO++ )
    {
        const OGRAxisOrientation eAO = static_cast<OGRAxisOrientation>(iAO);
        if( EQUAL(papszAxis1[iAxisOrientationField],
                  OSRAxisEnumToName(eAO)) )
            eOAxis1 = eAO;
        if( EQUAL(papszAxis2[iAxisOrientationField],
                  OSRAxisEnumToName(eAO)) )
            eOAxis2 = eAO;

        if( eOAxis1 == OAO_Other
            && anCodes[iAO] == atoi(papszAxis1[iAxisNameCodeField]) )
            eOAxis1 = eAO;
        if( eOAxis2 == OAO_Other
            && anCodes[iAO] == atoi(papszAxis2[iAxisNameCodeField]) )
            eOAxis2 = eAO;
    }

/* -------------------------------------------------------------------- */
/*      Work out the axis name.  We try to expand the abbreviation      */
/*      to a longer name.                                               */
/* -------------------------------------------------------------------- */
    const char *apszAxisName[2] = {
        papszAxis1[iAxisAbbrevField],
        papszAxis2[iAxisAbbrevField] };

    for( int iAO : {0, 1} )
    {
        if( EQUAL(apszAxisName[iAO], "N") )
            apszAxisName[iAO] = "Northing";
        else if( EQUAL(apszAxisName[iAO], "E") )
            apszAxisName[iAO] = "Easting";
        else if( EQUAL(apszAxisName[iAO], "S") )
            apszAxisName[iAO] = "Southing";
        else if( EQUAL(apszAxisName[iAO], "W") )
            apszAxisName[iAO] = "Westing";
    }

/* -------------------------------------------------------------------- */
/*      Set the axes.                                                   */
/* -------------------------------------------------------------------- */
    const OGRErr eResult = poSRS->SetAxes( pszTargetKey,
                                           apszAxisName[0], eOAxis1,
                                           apszAxisName[1], eOAxis2 );

    CSLDestroy( papszAxis1 );
    CSLDestroy( papszAxis2 );

    return eResult;
}

/************************************************************************/
/*                           SetEPSGGeogCS()                            */
/*                                                                      */
/*      FLAWS:                                                          */
/*       o Units are all hardcoded.                                     */
/************************************************************************/

static OGRErr SetEPSGGeogCS( OGRSpatialReference * poSRS, int nGeogCS )

{
    int nDatumCode = 0;
    int nPMCode = 0;
    int nUOMAngle = 0;
    int nEllipsoidCode = 0;
    int nCSC = 0;
    char *pszGeogCSName = nullptr;
    char *pszDatumName = nullptr;
    char *pszAngleName = nullptr;

    if( !EPSGGetGCSInfo( nGeogCS, &pszGeogCSName,
                         &nDatumCode, &pszDatumName,
                         &nPMCode, &nEllipsoidCode, &nUOMAngle, &nCSC ) )
        return OGRERR_UNSUPPORTED_SRS;

    char *pszPMName = nullptr;
    double dfPMOffset = 0.0;
    if( !EPSGGetPMInfo( nPMCode, &pszPMName, &dfPMOffset ) )
    {
        CPLFree( pszDatumName );
        CPLFree( pszGeogCSName );
        return OGRERR_UNSUPPORTED_SRS;
    }

    OGREPSGDatumNameMassage( &pszDatumName );

    char *pszEllipsoidName = nullptr;
    double dfSemiMajor = 0.0;
    double dfInvFlattening = 0.0;
    if( OSRGetEllipsoidInfo( nEllipsoidCode, &pszEllipsoidName,
                             &dfSemiMajor, &dfInvFlattening ) != OGRERR_NONE )
    {
        CPLFree( pszDatumName );
        CPLFree( pszGeogCSName );
        CPLFree( pszPMName );
        return OGRERR_UNSUPPORTED_SRS;
    }

    double dfAngleInDegrees = 0.0;
    double dfAngleInRadians = 0.0;
    if( !EPSGGetUOMAngleInfo( nUOMAngle, &pszAngleName, &dfAngleInDegrees ) )
    {
        pszAngleName = CPLStrdup("degree");
        dfAngleInDegrees = 1.0;
        nUOMAngle = -1;
    }

    if( dfAngleInDegrees == 1.0 )
        dfAngleInRadians = CPLAtof(SRS_UA_DEGREE_CONV);
    else
        dfAngleInRadians = CPLAtof(SRS_UA_DEGREE_CONV) * dfAngleInDegrees;

    poSRS->SetGeogCS( pszGeogCSName, pszDatumName,
                      pszEllipsoidName, dfSemiMajor, dfInvFlattening,
                      pszPMName, dfPMOffset,
                      pszAngleName, dfAngleInRadians );

    std::vector<CPLString> asBursaTransform;
    if( EPSGGetWGS84Transform( nGeogCS, asBursaTransform ) )
    {
        OGR_SRSNode *poWGS84 = new OGR_SRSNode( "TOWGS84" );

        for( int iCoeff = 0; iCoeff < 7; iCoeff++ )
        {
            poWGS84->AddChild( new OGR_SRSNode(
                asBursaTransform[iCoeff].c_str() ) );
        }

        poSRS->GetAttrNode( "DATUM" )->AddChild( poWGS84 );
    }

    poSRS->SetAuthority( "GEOGCS", "EPSG", nGeogCS );
    poSRS->SetAuthority( "DATUM", "EPSG", nDatumCode );
    poSRS->SetAuthority( "SPHEROID", "EPSG", nEllipsoidCode );
    poSRS->SetAuthority( "PRIMEM", "EPSG", nPMCode );

    if( nUOMAngle > 0 )
        poSRS->SetAuthority( "GEOGCS|UNIT", "EPSG", nUOMAngle );

    CPLFree( pszAngleName );
    CPLFree( pszDatumName );
    CPLFree( pszEllipsoidName );
    CPLFree( pszGeogCSName );
    CPLFree( pszPMName );

/* -------------------------------------------------------------------- */
/*      Set axes                                                        */
/* -------------------------------------------------------------------- */
    if( nCSC > 0 )
    {
        SetEPSGAxisInfo( poSRS, "GEOGCS", nCSC );
        CPLErrorReset();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OGR_FetchParm()                            */
/*                                                                      */
/*      Fetch a parameter from the parm list, based on its EPSG         */
/*      parameter code.                                                 */
/************************************************************************/

static double OGR_FetchParm( double *padfProjParms,
                             int *panParmIds,
                             int nTargetId,
                             double /* dfFromGreenwich */)
{
/* -------------------------------------------------------------------- */
/*      Set default in meters/degrees.                                  */
/* -------------------------------------------------------------------- */
    double dfResult = 0.0;
    switch( nTargetId )
    {
      case NatOriginScaleFactor:
      case InitialLineScaleFactor:
      case PseudoStdParallelScaleFactor:
        dfResult = 1.0;
        break;

      case AngleRectifiedToSkewedGrid:
        dfResult = 90.0;
        break;

      default:
        dfResult = 0.0;
    }

/* -------------------------------------------------------------------- */
/*      Try to find actual value in parameter list.                     */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < 7; i++ )
    {
        if( panParmIds[i] == nTargetId )
        {
            dfResult = padfProjParms[i];
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      EPSG longitudes are relative to greenwich.  The follow code     */
/*      could be used to make them relative to the prime meridian of    */
/*      the associated GCS if that was appropriate.  However, the       */
/*      SetNormProjParm() method expects longitudes relative to         */
/*      greenwich, so there is nothing for us to do.                    */
/* -------------------------------------------------------------------- */
#if 0
    switch( nTargetId )
    {
      case NatOriginLong:
      case ProjCenterLong:
      case FalseOriginLong:
      case SphericalOriginLong:
      case InitialLongitude:
        // Note that the EPSG values are already relative to greenwich.
        // This shift is really making it relative to the provided prime
        // meridian, so that when SetTM() and company the correction back
        // ends up back relative to greenwich.
        dfResult = dfResult + dfFromGreenwich;
        break;

      default:
        ;
    }
#endif

    return dfResult;
}

#define OGR_FP(x) OGR_FetchParm(adfProjParms, anParmIds, (x), dfFromGreenwich)

/************************************************************************/
/*                           SetEPSGProjCS()                            */
/************************************************************************/

static OGRErr SetEPSGProjCS( OGRSpatialReference * poSRS, int nPCSCode )

{
    int nGCSCode = 0;
    int nUOMAngleCode = 0;
    int nUOMLength = 0;
    int nTRFCode = 0;
    int nCSC = 0;
    char *pszPCSName = nullptr;
    double adfTOWGS84[7] = { std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity() };

    if( !EPSGGetPCSInfo( nPCSCode, &pszPCSName,
                         &nUOMLength, &nUOMAngleCode,
                         &nGCSCode, &nTRFCode, &nCSC, adfTOWGS84 ) )
    {
        CPLFree(pszPCSName);
        return OGRERR_UNSUPPORTED_SRS;
    }

    poSRS->SetNode( "PROJCS", pszPCSName );

/* -------------------------------------------------------------------- */
/*      Set GEOGCS.                                                     */
/* -------------------------------------------------------------------- */
    const OGRErr nErr = SetEPSGGeogCS( poSRS, nGCSCode );
    if( nErr != OGRERR_NONE )
    {
        CPLFree(pszPCSName);
        return nErr;
    }

/* -------------------------------------------------------------------- */
/*      Set overridden TOWGS84 parameters                               */
/* -------------------------------------------------------------------- */
    if( adfTOWGS84[0] != std::numeric_limits<double>::infinity() )
    {
        poSRS->SetTOWGS84( adfTOWGS84[0],
                           adfTOWGS84[1],
                           adfTOWGS84[2],
                           adfTOWGS84[3],
                           adfTOWGS84[4],
                           adfTOWGS84[5],
                           adfTOWGS84[6] );
    }

    // Used by OGR_FP macro
    const double dfFromGreenwich = poSRS->GetPrimeMeridian();

/* -------------------------------------------------------------------- */
/*      Set linear units.                                               */
/* -------------------------------------------------------------------- */
    char *pszUOMLengthName = nullptr;
    double dfInMeters = 0.0;

    if( !EPSGGetUOMLengthInfo( nUOMLength, &pszUOMLengthName, &dfInMeters ) )
    {
        CPLFree(pszPCSName);
        return OGRERR_UNSUPPORTED_SRS;
    }

    poSRS->SetLinearUnits( pszUOMLengthName, dfInMeters );
    poSRS->SetAuthority( "PROJCS|UNIT", "EPSG", nUOMLength );

    CPLFree( pszUOMLengthName );
    CPLFree( pszPCSName );

/* -------------------------------------------------------------------- */
/*      Set projection and parameters.                                  */
/* -------------------------------------------------------------------- */
    int nProjMethod = 0;
    int anParmIds[7] = {};
    double adfProjParms[7] = {};

    if( !EPSGGetProjTRFInfo( nPCSCode, &nProjMethod, anParmIds, adfProjParms ))
        return OGRERR_UNSUPPORTED_SRS;

    switch( nProjMethod )
    {
      case 9801:
      case 9817:  // Really LCC near conformal.
        poSRS->SetLCC1SP( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                          OGR_FP( NatOriginScaleFactor ),
                          OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9802:
        poSRS->SetLCC( OGR_FP( StdParallel1Lat ), OGR_FP( StdParallel2Lat ),
                       OGR_FP( FalseOriginLat ), OGR_FP( FalseOriginLong ),
                       OGR_FP( FalseOriginEasting ),
                       OGR_FP( FalseOriginNorthing ));
        break;

      case 9803:
        poSRS->SetLCCB( OGR_FP( StdParallel1Lat ), OGR_FP( StdParallel2Lat ),
                        OGR_FP( FalseOriginLat ), OGR_FP( FalseOriginLong ),
                        OGR_FP( FalseOriginEasting ),
                        OGR_FP( FalseOriginNorthing ));
        break;

      case 9805:
        poSRS->SetMercator2SP( OGR_FP( StdParallel1Lat ),
                               OGR_FP( NatOriginLat ), OGR_FP(NatOriginLong),
                               OGR_FP( FalseEasting ), OGR_FP(FalseNorthing) );

        break;

      case 9804:
      case 9841:  // Mercator 1SP (Spherical).
      case 1024:  // Google Mercator.
        poSRS->SetMercator( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                            OGR_FP( NatOriginScaleFactor ),
                            OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );

        // override hack for google mercator.
        if( nProjMethod == 1024 || nProjMethod == 9841 )
        {
            poSRS->SetExtension(
                "PROJCS", "PROJ4",
                "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 "
                "+x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null "
                "+wktext +no_defs" );
        }
        break;

      case 9806:
        poSRS->SetCS( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                            OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9807:
        poSRS->SetTM( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                      OGR_FP( NatOriginScaleFactor ),
                      OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9808:
        poSRS->SetTMSO( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                        OGR_FP( NatOriginScaleFactor ),
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9809:
        poSRS->SetOS( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                      OGR_FP( NatOriginScaleFactor ),
                      OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9810:
        poSRS->SetPS( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                      OGR_FP( NatOriginScaleFactor ),
                      OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9811:
        poSRS->SetNZMG( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9812:
      case 9813:
      {
        poSRS->SetHOM( OGR_FP( ProjCenterLat ), OGR_FP( ProjCenterLong ),
                       OGR_FP( Azimuth ),
                       OGR_FP( AngleRectifiedToSkewedGrid ),
                       OGR_FP( InitialLineScaleFactor ),
                       OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );

        OGR_SRSNode *poNode = poSRS->GetAttrNode( "PROJECTION" )->GetChild( 0 );
        if( nProjMethod == 9813 )
            poNode->SetValue( SRS_PT_LABORDE_OBLIQUE_MERCATOR );
        break;
      }
      case 9814:
        // NOTE: This is no longer used.  Swiss Oblique Mercator gets
        // implemented using 9815 instead.
        poSRS->SetSOC( OGR_FP( ProjCenterLat ), OGR_FP( ProjCenterLong ),
                       OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9815:
        poSRS->SetHOMAC( OGR_FP( ProjCenterLat ), OGR_FP( ProjCenterLong ),
                         OGR_FP( Azimuth ),
                         OGR_FP( AngleRectifiedToSkewedGrid ),
                         OGR_FP( InitialLineScaleFactor ),
                         OGR_FP( ProjCenterEasting ),
                         OGR_FP( ProjCenterNorthing ) );
        break;

      case 9816:
        poSRS->SetTMG( OGR_FP( FalseOriginLat ), OGR_FP( FalseOriginLong ),
                       OGR_FP( FalseOriginEasting ),
                       OGR_FP( FalseOriginNorthing ) );
        break;

      case 9818:
        poSRS->SetPolyconic( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                             OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 1041:  // Used by EPSG:5514.
      case 9819:
      {
          double dfCenterLong = OGR_FP( ProjCenterLong );

          if( dfCenterLong == 0.0 )  // See ticket #2559.
              dfCenterLong = OGR_FP( PolarLongOrigin );

          double dfAzimuth = OGR_FP( CoLatConeAxis );  // See ticket #4223.
          if( dfAzimuth == 0.0 )
              dfAzimuth = OGR_FP( Azimuth );

          poSRS->SetKrovak( OGR_FP( ProjCenterLat ), dfCenterLong,
                            dfAzimuth,
                            OGR_FP( PseudoStdParallelLat ),
                            OGR_FP( PseudoStdParallelScaleFactor ),
                            OGR_FP( ProjCenterEasting ),
                            OGR_FP( ProjCenterNorthing ) );
      }
      break;

      case 9820:
      case 1027:  // Used by EPSG:2163, 3408, 3409, 3973 and 3974.
        poSRS->SetLAEA( OGR_FP( NatOriginLat ), OGR_FP( NatOriginLong ),
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9821: // DEPRECATED : this is the spherical form, and really needs
                 // different equations which give different results but PROJ.4
                 // doesn't seem to support the spherical form.
        poSRS->SetLAEA( OGR_FP( SphericalOriginLat ),
                        OGR_FP( SphericalOriginLong ),
                        OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9822:  // Albers (Conic) Equal Area.
        poSRS->SetACEA( OGR_FP( StdParallel1Lat ),
                        OGR_FP( StdParallel2Lat ),
                        OGR_FP( FalseOriginLat ),
                        OGR_FP( FalseOriginLong ),
                        OGR_FP( FalseOriginEasting ),
                        OGR_FP( FalseOriginNorthing ) );
        break;

      case 9823:  // Equidistant Cylindrical / Plate Carre / Equirectangular.
      case 9842:
      case 1028:
      case 1029:
        poSRS->SetEquirectangular( OGR_FP( NatOriginLat ),
                                   OGR_FP( NatOriginLong ),
                                   0.0, 0.0 );
        break;

      case 9829:  // Polar Stereographic (Variant B).
        poSRS->SetPS( OGR_FP( PolarLatStdParallel ), OGR_FP(PolarLongOrigin),
                      1.0,
                      OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      case 9834: // Lambert Cylindrical Equal Area (Spherical) bug #2659.
      case 9835: // Lambert Cylindrical Equal Area (Ellipsoidal).
        poSRS->SetCEA( OGR_FP( StdParallel1Lat ), OGR_FP( NatOriginLong ),
                       OGR_FP( FalseEasting ), OGR_FP( FalseNorthing ) );
        break;

      default:
        CPLDebug( "EPSG", "No WKT support for projection method %d.",
                  nProjMethod );
        return OGRERR_UNSUPPORTED_SRS;
    }

/* -------------------------------------------------------------------- */
/*      Set overall PCS authority code.                                 */
/* -------------------------------------------------------------------- */
    poSRS->SetAuthority( "PROJCS", "EPSG", nPCSCode );

/* -------------------------------------------------------------------- */
/*      Set axes                                                        */
/* -------------------------------------------------------------------- */
    if( nCSC > 0 )
    {
        SetEPSGAxisInfo( poSRS, "PROJCS", nCSC );
        CPLErrorReset();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetEPSGVertCS()                            */
/************************************************************************/

static OGRErr SetEPSGVertCS( OGRSpatialReference * poSRS, int nVertCSCode )

{
/* -------------------------------------------------------------------- */
/*      Fetch record from the vertcs.csv or override file.              */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CSVFilename( "vertcs.override.csv" );
    char szSearchKey[24] = {};
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nVertCSCode );
    char **papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                     szSearchKey, CC_Integer );

    if( papszRecord == nullptr )
    {
        pszFilename = CSVFilename( "vertcs.csv" );
        papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                         szSearchKey, CC_Integer );
    }

    if( papszRecord == nullptr )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Setup the basic VERT_CS.                                        */
/* -------------------------------------------------------------------- */
    poSRS->SetVertCS(
        CSLGetField( papszRecord,
                     CSVGetFileFieldId(pszFilename,
                                       "COORD_REF_SYS_NAME")),
        CSLGetField( papszRecord,
                     CSVGetFileFieldId(pszFilename,
                                       "DATUM_NAME")) );

/* -------------------------------------------------------------------- */
/*      Should we add a geoidgrids extension node?                      */
/* -------------------------------------------------------------------- */
    const char *pszMethod =
        CSLGetField( papszRecord,
                     CSVGetFileFieldId(pszFilename, "COORD_OP_METHOD_CODE_1"));
    if( pszMethod && EQUAL(pszMethod, "9665") )
    {
        const char *pszParm11 =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename, "PARM_1_1"));

        poSRS->SetExtension( "VERT_CS|VERT_DATUM", "PROJ4_GRIDS", pszParm11 );
    }

/* -------------------------------------------------------------------- */
/*      Setup the VERT_DATUM node.                                      */
/* -------------------------------------------------------------------- */
    poSRS->SetAuthority( "VERT_CS|VERT_DATUM", "EPSG",
                         atoi(CSLGetField( papszRecord,
                                           CSVGetFileFieldId(pszFilename,
                                                             "DATUM_CODE"))) );

/* -------------------------------------------------------------------- */
/*      Set linear units.                                               */
/* -------------------------------------------------------------------- */
    const int nUOM_CODE =
        atoi(CSLGetField( papszRecord,
                          CSVGetFileFieldId(pszFilename, "UOM_CODE")));

    char *pszUOMLengthName = nullptr;
    double dfInMeters = 0.0;

    if( !EPSGGetUOMLengthInfo( nUOM_CODE, &pszUOMLengthName, &dfInMeters ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to lookup UOM CODE %d", nUOM_CODE );
    }
    else
    {
        poSRS->SetTargetLinearUnits( "VERT_CS", pszUOMLengthName, dfInMeters );
        poSRS->SetAuthority( "VERT_CS|UNIT", "EPSG", nUOM_CODE );

        CPLFree( pszUOMLengthName );
    }

/* -------------------------------------------------------------------- */
/*      Set overall authority code.                                     */
/* -------------------------------------------------------------------- */
    poSRS->SetAuthority( "VERT_CS", "EPSG", nVertCSCode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetEPSGCompdCS()                           */
/************************************************************************/

static OGRErr SetEPSGCompdCS( OGRSpatialReference * poSRS, int nCCSCode )

{
/* -------------------------------------------------------------------- */
/*      Fetch record from the compdcs.csv or override file.             */
/* -------------------------------------------------------------------- */
    char szSearchKey[24] = {};
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nCCSCode );

// So far no override file needed.
//    pszFilename = CSVFilename( "compdcs.override.csv" );
//    papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
//                                     szSearchKey, CC_Integer );

    //if( papszRecord == NULL )
    const char *pszFilename = CSVFilename( "compdcs.csv" );
    char **papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                            szSearchKey, CC_Integer );

    if( papszRecord == nullptr )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Fetch subinformation now before anything messes with the        */
/*      last loaded record.                                             */
/* -------------------------------------------------------------------- */
    const int nPCSCode =
        atoi(CSLGetField( papszRecord,
                          CSVGetFileFieldId(pszFilename,
                                            "CMPD_HORIZCRS_CODE")));
    const int nVertCSCode =
        atoi(CSLGetField( papszRecord,
                          CSVGetFileFieldId(pszFilename,
                                            "CMPD_VERTCRS_CODE")));

/* -------------------------------------------------------------------- */
/*      Set the COMPD_CS node with a name.                              */
/* -------------------------------------------------------------------- */
    poSRS->SetNode( "COMPD_CS",
                    CSLGetField( papszRecord,
                                 CSVGetFileFieldId(pszFilename,
                                                   "COORD_REF_SYS_NAME")) );

/* -------------------------------------------------------------------- */
/*      Lookup the projected coordinate system.  Can the            */
/*      horizontal CRS be a GCS?                                        */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oPCS;
    OGRErr eErr = SetEPSGProjCS( &oPCS, nPCSCode );
    if( eErr != OGRERR_NONE )
    {
        // perhaps it is a GCS?
        eErr = SetEPSGGeogCS( &oPCS, nPCSCode );
    }

    if( eErr != OGRERR_NONE )
    {
        return eErr;
    }

    poSRS->GetRoot()->AddChild(
        oPCS.GetRoot()->Clone() );

/* -------------------------------------------------------------------- */
/*      Lookup the VertCS.                                              */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oVertCS;
    eErr = SetEPSGVertCS( &oVertCS, nVertCSCode );
    if( eErr != OGRERR_NONE )
        return eErr;

    poSRS->GetRoot()->AddChild(
        oVertCS.GetRoot()->Clone() );

/* -------------------------------------------------------------------- */
/*      Set overall authority code.                                     */
/* -------------------------------------------------------------------- */
    poSRS->SetAuthority( "COMPD_CS", "EPSG", nCCSCode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetEPSGGeocCS()                            */
/************************************************************************/

static OGRErr SetEPSGGeocCS( OGRSpatialReference * poSRS, int nGCSCode )

{
/* -------------------------------------------------------------------- */
/*      Fetch record from the geoccs.csv or override file.              */
/* -------------------------------------------------------------------- */
    char szSearchKey[24] = {};
    snprintf( szSearchKey, sizeof(szSearchKey), "%d", nGCSCode );

// So far no override file needed.
//    pszFilename = CSVFilename( "compdcs.override.csv" );
//    papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
//                                     szSearchKey, CC_Integer );

    // if( papszRecord == NULL )
    const char *pszFilename = CSVFilename( "geoccs.csv" );
    char **papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                            szSearchKey, CC_Integer );

    if( papszRecord == nullptr )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Set the GEOCCS node with a name.                                */
/* -------------------------------------------------------------------- */
    poSRS->Clear();
    CPLString osGCCSName =
        CSLGetField( papszRecord,
                     CSVGetFileFieldId( pszFilename,
                                        "COORD_REF_SYS_NAME" ));

    const char *pszDeprecated =
        CSLGetField( papszRecord,
                     CSVGetFileFieldId( pszFilename,
                                        "DEPRECATED") );

    if ( pszDeprecated != nullptr && *pszDeprecated == '1' )
         osGCCSName += " (deprecated)";

    poSRS->SetGeocCS( osGCCSName );

/* -------------------------------------------------------------------- */
/*      Get datum related information.                                  */
/* -------------------------------------------------------------------- */
    const int nDatumCode = atoi(CSLGetField( papszRecord,
                                             CSVGetFileFieldId(pszFilename,
                                                               "DATUM_CODE")));

    char *pszDatumName =
        CPLStrdup( CSLGetField(papszRecord,
                               CSVGetFileFieldId(pszFilename, "DATUM_NAME")) );
    OGREPSGDatumNameMassage( &pszDatumName );

    const int nEllipsoidCode = atoi(CSLGetField(
        papszRecord, CSVGetFileFieldId(pszFilename, "ELLIPSOID_CODE")));

    const int nPMCode = atoi(CSLGetField(
        papszRecord, CSVGetFileFieldId(pszFilename,
                                       "PRIME_MERIDIAN_CODE")));

/* -------------------------------------------------------------------- */
/*      Get prime meridian information.                                 */
/* -------------------------------------------------------------------- */
    char *pszPMName = nullptr;
    double dfPMOffset = 0.0;

    if( !EPSGGetPMInfo( nPMCode, &pszPMName, &dfPMOffset ) )
    {
        CPLFree( pszDatumName );
        return OGRERR_UNSUPPORTED_SRS;
    }

/* -------------------------------------------------------------------- */
/*      Get the ellipsoid information.                                  */
/* -------------------------------------------------------------------- */
    char *pszEllipsoidName = nullptr;
    double dfSemiMajor = 0.0;
    double dfInvFlattening = 0.0;

    if( OSRGetEllipsoidInfo( nEllipsoidCode, &pszEllipsoidName,
                             &dfSemiMajor, &dfInvFlattening ) != OGRERR_NONE )
    {
        CPLFree( pszDatumName );
        CPLFree( pszPMName );
        return OGRERR_UNSUPPORTED_SRS;
    }

/* -------------------------------------------------------------------- */
/*      Setup the spheroid.                                             */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poSpheroid = new OGR_SRSNode( "SPHEROID" );
    poSpheroid->AddChild( new OGR_SRSNode( pszEllipsoidName ) );

    char szValue[128] = {};
    OGRsnPrintDouble( szValue, sizeof(szValue), dfSemiMajor );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfInvFlattening );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

    CPLFree( pszEllipsoidName );

/* -------------------------------------------------------------------- */
/*      Setup the Datum.                                                */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDatum = new OGR_SRSNode( "DATUM" );
    poDatum->AddChild( new OGR_SRSNode(pszDatumName) );
    poDatum->AddChild( poSpheroid );

    poSRS->GetRoot()->AddChild( poDatum );

    CPLFree( pszDatumName );

/* -------------------------------------------------------------------- */
/*      Setup the prime meridian.                                       */
/* -------------------------------------------------------------------- */
    if( dfPMOffset == 0.0 )
        strcpy( szValue, "0" );
    else
        OGRsnPrintDouble( szValue, sizeof(szValue), dfPMOffset );

    OGR_SRSNode *poPM = new OGR_SRSNode( "PRIMEM" );
    poPM->AddChild( new OGR_SRSNode( pszPMName ) );
    poPM->AddChild( new OGR_SRSNode( szValue ) );

    poSRS->GetRoot()->AddChild( poPM );

    CPLFree( pszPMName );

/* -------------------------------------------------------------------- */
/*      Should we try to lookup a datum transform?                      */
/* -------------------------------------------------------------------- */
#if 0
    if( EPSGGetWGS84Transform( nGeogCS, adfBursaTransform ) )
    {
        char szValue[100] = {};

        OGR_SRSNode *poWGS84 = new OGR_SRSNode( "TOWGS84" );

        for( int iCoeff = 0; iCoeff < 7; iCoeff++ )
        {
            CPLsnprintf( szValue, sizeof(szValue),
                         "%g", adfBursaTransform[iCoeff] );
            poWGS84->AddChild( new OGR_SRSNode( szValue ) );
        }

        poSRS->GetAttrNode( "DATUM" )->AddChild( poWGS84 );
    }
#endif

/* -------------------------------------------------------------------- */
/*      Set linear units.                                               */
/* -------------------------------------------------------------------- */
    int nUOMLength = atoi(CSLGetField( papszRecord,
                                       CSVGetFileFieldId(pszFilename,
                                                         "UOM_CODE")));

    double dfInMeters = 1.0;
    char *pszUOMLengthName = nullptr;
    if( !EPSGGetUOMLengthInfo( nUOMLength, &pszUOMLengthName, &dfInMeters ) )
    {
        return OGRERR_UNSUPPORTED_SRS;
    }

    poSRS->SetLinearUnits( pszUOMLengthName, dfInMeters );
    poSRS->SetAuthority( "GEOCCS|UNIT", "EPSG", nUOMLength );

    CPLFree( pszUOMLengthName );

/* -------------------------------------------------------------------- */
/*      Set axes                                                        */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( "Geocentric X" ) );
    poAxis->AddChild( new OGR_SRSNode( OSRAxisEnumToName(OAO_Other) ) );

    poSRS->GetRoot()->AddChild( poAxis );

    poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( "Geocentric Y" ) );
    poAxis->AddChild( new OGR_SRSNode( OSRAxisEnumToName(OAO_Other) ) );

    poSRS->GetRoot()->AddChild( poAxis );

    poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( "Geocentric Z" ) );
    poAxis->AddChild( new OGR_SRSNode( OSRAxisEnumToName(OAO_North) ) );

    poSRS->GetRoot()->AddChild( poAxis );

/* -------------------------------------------------------------------- */
/*      Set the authority codes.                                        */
/* -------------------------------------------------------------------- */
    poSRS->SetAuthority( "DATUM", "EPSG", nDatumCode );
    poSRS->SetAuthority( "SPHEROID", "EPSG", nEllipsoidCode );
    poSRS->SetAuthority( "PRIMEM", "EPSG", nPMCode );
    poSRS->SetAuthority( "GEOCCS", "EPSG", nGCSCode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromEPSG()                           */
/************************************************************************/

/**
 * \brief Initialize SRS based on EPSG GCS or PCS code.
 *
 * This method will initialize the spatial reference based on the
 * passed in EPSG GCS or PCS code.  The coordinate system definitions
 * are normally read from the EPSG derived support files such as
 * pcs.csv, gcs.csv, pcs.override.csv, gcs.override.csv and falling
 * back to search for a PROJ.4 epsg init file or a definition in epsg.wkt.
 *
 * These support files are normally searched for in /usr/local/share/gdal
 * or in the directory identified by the GDAL_DATA configuration option.
 * See CPLFindFile() for details.
 *
 * This method is relatively expensive, and generally involves quite a bit
 * of text file scanning.  Reasonable efforts should be made to avoid calling
 * it many times for the same coordinate system.
 *
 * This method is similar to importFromEPSGA() except that EPSG preferred
 * axis ordering will *not* be applied for geographic coordinate systems.
 * EPSG normally defines geographic coordinate systems to use lat/long
 * contrary to typical GIS use). Since OGR 1.10.0, EPSG preferred
 * axis ordering will also *not* be applied for projected coordinate systems
 * that use northing/easting order.
 *
 * This method is the same as the C function OSRImportFromEPSG().
 *
 * @param nCode a GCS or PCS code from the horizontal coordinate system table.
 *
 * @return OGRERR_NONE on success, or an error code on failure.
 */

OGRErr OGRSpatialReference::importFromEPSG( int nCode )

{
    OGRErr eErr = importFromEPSGA( nCode );

    // Strip any GCS axis settings found.
    if( eErr == OGRERR_NONE )
    {
        OGR_SRSNode *poGEOGCS = GetAttrNode( "GEOGCS" );

        if( poGEOGCS != nullptr )
            poGEOGCS->StripNodes( "AXIS" );

        OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
        if( poPROJCS != nullptr && EPSGTreatsAsNorthingEasting() )
            poPROJCS->StripNodes( "AXIS" );
    }

    return eErr;
}

/************************************************************************/
/*                         OSRImportFromEPSG()                          */
/************************************************************************/

/**
 * \brief  Initialize SRS based on EPSG GCS or PCS code.
 *
 * This function is the same as OGRSpatialReference::importFromEPSG().
 */

OGRErr CPL_STDCALL OSRImportFromEPSG( OGRSpatialReferenceH hSRS, int nCode )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromEPSG", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        importFromEPSG( nCode );
}

/************************************************************************/
/*                          importFromEPSGA()                           */
/************************************************************************/

/**
 * \brief Initialize SRS based on EPSG GCS or PCS code.
 *
 * This method will initialize the spatial reference based on the
 * passed in EPSG GCS or PCS code.
 *
 * This method is similar to importFromEPSG() except that EPSG preferred axis
 * ordering *will* be applied for geographic and projected coordinate systems.
 * EPSG normally defines geographic coordinate systems to use lat/long, and also
 * there are also a few projected coordinate systems that use northing/easting
 * order contrary to typical GIS use).  See
 * OGRSpatialReference::importFromEPSG() for more details on operation of this
 * method.
 *
 * This method is the same as the C function OSRImportFromEPSGA().
 *
 * @param nCode a GCS or PCS code from the horizontal coordinate system table.
 *
 * @return OGRERR_NONE on success, or an error code on failure.
 */

OGRErr OGRSpatialReference::importFromEPSGA( int nCode )

{
    return importFromEPSGAInternal(nCode, nullptr);
}

/************************************************************************/
/*                       importFromEPSGAInternal()                      */
/************************************************************************/

OGRErr OGRSpatialReference::importFromEPSGAInternal( int nCode,
                                                     const char* pszSRSType )
{
    const int nCodeIn = nCode;
    // HACK to support 3D WGS84
    if( nCode == 4979 )
        nCode = 4326;
    bNormInfoSet = FALSE;

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    if( GetRoot() != nullptr )
    {
        delete poRoot;
        poRoot = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Verify that we can find the required filename(s).               */
/* -------------------------------------------------------------------- */
    if( CSVScanFileByName( CSVFilename( "gcs.csv" ),
                           "COORD_REF_SYS_CODE",
                           "4269", CC_Integer ) == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open EPSG support file %s.  "
                  "Try setting the GDAL_DATA environment variable to point to "
                  "the directory containing EPSG csv files.",
                  CSVFilename( "gcs.csv" ) );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Try this as various sorts of objects till one works.            */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    if( pszSRSType && EQUAL(pszSRSType, "GEOGCS") )
    {
        eErr = SetEPSGGeogCS( this, nCode );
        if( eErr != OGRERR_NONE )
            return eErr;
    }
    else if( pszSRSType && EQUAL(pszSRSType, "PROJCS") )
    {
        eErr = SetEPSGProjCS( this, nCode );
        if( eErr != OGRERR_NONE )
            return eErr;
    }
    else
    {
        eErr = SetEPSGGeogCS( this, nCode );
    }
    if( eErr == OGRERR_UNSUPPORTED_SRS )
        eErr = SetEPSGProjCS( this, nCode );
    if( eErr == OGRERR_UNSUPPORTED_SRS )
        eErr = SetEPSGVertCS( this, nCode );
    if( eErr == OGRERR_UNSUPPORTED_SRS )
        eErr = SetEPSGCompdCS( this, nCode );
    if( eErr == OGRERR_UNSUPPORTED_SRS )
        eErr = SetEPSGGeocCS( this, nCode );

/* -------------------------------------------------------------------- */
/*      If we get it as an unsupported code, try looking it up in       */
/*      the epsg.wkt coordinate system dictionary.                      */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_UNSUPPORTED_SRS )
    {
        char szCode[32] = { 0 };
        snprintf( szCode, sizeof(szCode), "%d", nCode );
        eErr = importFromDict( "epsg.wkt", szCode );
    }

/* -------------------------------------------------------------------- */
/*      If we get it as an unsupported code, try looking it up in       */
/*      the PROJ.4 support file(s).                                     */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_UNSUPPORTED_SRS )
    {
        char szWrkDefn[100] = {};
        snprintf( szWrkDefn, sizeof(szWrkDefn), "+init=epsg:%d", nCode );

        char *pszNormalized = OCTProj4Normalize( szWrkDefn );

        if( strstr(pszNormalized, "proj=") != nullptr )
            eErr = importFromProj4( pszNormalized );

        CPLFree( pszNormalized );
    }

/* -------------------------------------------------------------------- */
/*      Push in authority information if we were successful, and it     */
/*      is not already present.                                         */
/* -------------------------------------------------------------------- */
    const char *pszAuthName = nullptr;

    if( IsProjected() )
        pszAuthName = GetAuthorityName( "PROJCS" );
    else
        pszAuthName = GetAuthorityName( "GEOGCS" );

    if( eErr == OGRERR_NONE && (pszAuthName == nullptr || nCode != nCodeIn) )
    {
        if( IsProjected() )
            SetAuthority( "PROJCS", "EPSG", nCodeIn );
        else if( IsGeographic() )
            SetAuthority( "GEOGCS", "EPSG", nCodeIn );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise officially issue an error message.                    */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_UNSUPPORTED_SRS )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "EPSG PCS/GCS code %d not found in EPSG support files.  "
                  "Is this a valid EPSG coordinate system?",
                  nCode );
    }

/* -------------------------------------------------------------------- */
/*      To the extent possible, we want to return the results in as     */
/*      close to standard OGC format as possible, so we fixup the       */
/*      ordering.                                                       */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        eErr = FixupOrdering();
    }

    return eErr;
}

/************************************************************************/
/*                         OSRImportFromEPSGA()                         */
/************************************************************************/

/**
 * \brief  Initialize SRS based on EPSG GCS or PCS code.
 *
 * This function is the same as OGRSpatialReference::importFromEPSGA().
 */

OGRErr CPL_STDCALL OSRImportFromEPSGA( OGRSpatialReferenceH hSRS, int nCode )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromEPSGA", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        importFromEPSGA( nCode );
}

/************************************************************************/
/*                           SetStatePlane()                            */
/************************************************************************/

/**
 * \brief Set State Plane projection definition.
 *
 * This will attempt to generate a complete definition of a state plane
 * zone based on generating the entire SRS from the EPSG tables.  If the
 * EPSG tables are unavailable, it will produce a stubbed LOCAL_CS definition
 * and return OGRERR_FAILURE.
 *
 * This method is the same as the C function OSRSetStatePlaneWithUnits().
 *
 * @param nZone State plane zone number, in the USGS numbering scheme (as
 * distinct from the Arc/Info and Erdas numbering scheme.
 *
 * @param bNAD83 TRUE if the NAD83 zone definition should be used or FALSE
 * if the NAD27 zone definition should be used.
 *
 * @param pszOverrideUnitName Linear unit name to apply overriding the
 * legal definition for this zone.
 *
 * @param dfOverrideUnit Linear unit conversion factor to apply overriding
 * the legal definition for this zone.
 *
 * @return OGRERR_NONE on success, or OGRERR_FAILURE on failure, mostly likely
 * due to the EPSG tables not being accessible.
 */

OGRErr OGRSpatialReference::SetStatePlane( int nZone, int bNAD83,
                                           const char *pszOverrideUnitName,
                                           double dfOverrideUnit )

{

/* -------------------------------------------------------------------- */
/*      Get the index id from stateplane.csv.                           */
/* -------------------------------------------------------------------- */

    if( !bNAD83 && nZone > INT_MAX - 10000 )
        return OGRERR_FAILURE;

    const int nAdjustedId = bNAD83 ? nZone : nZone + 10000;

/* -------------------------------------------------------------------- */
/*      Turn this into a PCS code.  We assume there will only be one    */
/*      PCS corresponding to each Proj_ code since the proj code        */
/*      already effectively indicates NAD27 or NAD83.                   */
/* -------------------------------------------------------------------- */
    char szID[32] = {};
    snprintf( szID, sizeof(szID), "%d", nAdjustedId );
    const int nPCSCode =
        atoi( CSVGetField( CSVFilename( "stateplane.csv" ),
                           "ID", szID, CC_Integer,
                           "EPSG_PCS_CODE" ) );
    if( nPCSCode < 1 )
    {
        static bool bFailureReported = false;

        if( !bFailureReported )
        {
            bFailureReported = true;
            CPLError( CE_Warning, CPLE_OpenFailed,
                      "Unable to find state plane zone in stateplane.csv, "
                      "likely because the GDAL data files cannot be found.  "
                      "Using incomplete definition of state plane zone." );
        }

        Clear();
        if( bNAD83 )
        {
            char szName[128] = {};
            snprintf( szName, sizeof(szName),
                      "State Plane Zone %d / NAD83", nZone );
            SetLocalCS( szName );
            SetLinearUnits( SRS_UL_METER, 1.0 );
        }
        else
        {
            char szName[128] = {};
            snprintf( szName, sizeof(szName),
                      "State Plane Zone %d / NAD27", nZone );
            SetLocalCS( szName );
            SetLinearUnits( SRS_UL_US_FOOT, CPLAtof(SRS_UL_US_FOOT_CONV) );
        }

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Define based on a full EPSG definition of the zone.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr = importFromEPSG( nPCSCode );

    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Apply units override if required.                               */
/*                                                                      */
/*      We will need to adjust the linear projection parameter to       */
/*      match the provided units, and clear the authority code.         */
/* -------------------------------------------------------------------- */
    if( dfOverrideUnit != 0.0
        && fabs(dfOverrideUnit - GetLinearUnits()) > 0.0000000001 )
    {
        const double dfFalseEasting = GetNormProjParm( SRS_PP_FALSE_EASTING );
        const double dfFalseNorthing = GetNormProjParm( SRS_PP_FALSE_NORTHING);

        SetLinearUnits( pszOverrideUnitName, dfOverrideUnit );

        SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
        SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

        OGR_SRSNode * const poPROJCS = GetAttrNode( "PROJCS" );
        if( poPROJCS != nullptr && poPROJCS->FindChild( "AUTHORITY" ) != -1 )
        {
            poPROJCS->DestroyChild( poPROJCS->FindChild( "AUTHORITY" ) );
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetStatePlane()                          */
/************************************************************************/

/**
 * \brief Set State Plane projection definition.
 *
 * This function is the same as OGRSpatialReference::SetStatePlane().
 */

OGRErr OSRSetStatePlane( OGRSpatialReferenceH hSRS, int nZone, int bNAD83 )

{
    VALIDATE_POINTER1( hSRS, "OSRSetStatePlane", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetStatePlane( nZone, bNAD83 );
}

/************************************************************************/
/*                     OSRSetStatePlaneWithUnits()                      */
/************************************************************************/

/**
 * \brief Set State Plane projection definition.
 *
 * This function is the same as OGRSpatialReference::SetStatePlane().
 */

OGRErr OSRSetStatePlaneWithUnits( OGRSpatialReferenceH hSRS,
                                  int nZone, int bNAD83,
                                  const char *pszOverrideUnitName,
                                  double dfOverrideUnit )

{
    VALIDATE_POINTER1( hSRS, "OSRSetStatePlaneWithUnits", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetStatePlane( nZone, bNAD83,
                       pszOverrideUnitName,
                       dfOverrideUnit );
}

/************************************************************************/
/*                           GetEPSGGeogCS()                            */
/************************************************************************/

/** Try to establish what the EPSG code for this coordinate systems
 * GEOGCS might be.  Returns -1 if no reasonable guess can be made.
 *
 * @return EPSG code
 */

// TODO: We really need to do some name lookups.

int OGRSpatialReference::GetEPSGGeogCS() const

{
    const char *pszAuthName = GetAuthorityName( "GEOGCS" );

/* -------------------------------------------------------------------- */
/*      Do we already have it?                                          */
/* -------------------------------------------------------------------- */
    if( pszAuthName != nullptr && EQUAL(pszAuthName, "epsg") )
        return atoi(GetAuthorityCode( "GEOGCS" ));

/* -------------------------------------------------------------------- */
/*      Get the datum and geogcs names.                                 */
/* -------------------------------------------------------------------- */
    const char *pszGEOGCS = GetAttrValue( "GEOGCS" );
    const char *pszDatum = GetAttrValue( "DATUM" );

    // We can only operate on coordinate systems with a geogcs.
    if( pszGEOGCS == nullptr || pszDatum == nullptr )
        return -1;

/* -------------------------------------------------------------------- */
/*      Is this a "well known" geographic coordinate system?            */
/* -------------------------------------------------------------------- */
    const bool bWGS = strstr(pszGEOGCS, "WGS") != nullptr
        || strstr(pszDatum, "WGS")
        || strstr(pszGEOGCS, "World Geodetic System")
        || strstr(pszGEOGCS, "World_Geodetic_System")
        || strstr(pszDatum, "World Geodetic System")
        || strstr(pszDatum, "World_Geodetic_System");

    const bool bNAD = strstr(pszGEOGCS, "NAD") != nullptr
        || strstr(pszDatum, "NAD")
        || strstr(pszGEOGCS, "North American")
        || strstr(pszGEOGCS, "North_American")
        || strstr(pszDatum, "North American")
        || strstr(pszDatum, "North_American");

    if( bWGS && (strstr(pszGEOGCS, "84") || strstr(pszDatum, "84")) )
        return 4326;

    if( bWGS && (strstr(pszGEOGCS, "72") || strstr(pszDatum, "72")) )
        return 4322;

    if( bNAD && (strstr(pszGEOGCS, "83") || strstr(pszDatum, "83")) )
        return 4269;

    if( bNAD && (strstr(pszGEOGCS, "27") || strstr(pszDatum, "27")) )
        return 4267;

/* -------------------------------------------------------------------- */
/*      If we know the datum, associate the most likely GCS with        */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    pszAuthName = GetAuthorityName( "GEOGCS|DATUM" );

    if( pszAuthName != nullptr
        && EQUAL(pszAuthName, "epsg")
        && GetPrimeMeridian() == 0.0 )
    {
        const int nDatum = atoi(GetAuthorityCode("GEOGCS|DATUM"));

        if( nDatum >= 6000 && nDatum <= 6999 )
            return nDatum - 2000;
    }

    return -1;
}

/************************************************************************/
/*                          AutoIdentifyEPSG()                          */
/************************************************************************/

/**
 * \brief Set EPSG authority info if possible.
 *
 * This method inspects a WKT definition, and adds EPSG authority nodes
 * where an aspect of the coordinate system can be easily and safely
 * corresponded with an EPSG identifier.  In practice, this method will
 * evolve over time.  In theory it can add authority nodes for any object
 * (i.e. spheroid, datum, GEOGCS, units, and PROJCS) that could have an
 * authority node.  Mostly this is useful to inserting appropriate
 * PROJCS codes for common formulations (like UTM n WGS84).
 *
 * If it success the OGRSpatialReference is updated in place, and the
 * method return OGRERR_NONE.  If the method fails to identify the
 * general coordinate system OGRERR_UNSUPPORTED_SRS is returned but no
 * error message is posted via CPLError().
 *
 * This method is the same as the C function OSRAutoIdentifyEPSG().
 *
 * Since GDAL 2.3, the FindMatches() method can also be used for improved
 * matching by researching the EPSG catalog.
 *
 * @return OGRERR_NONE or OGRERR_UNSUPPORTED_SRS.
 */

OGRErr OGRSpatialReference::AutoIdentifyEPSG()

{
/* -------------------------------------------------------------------- */
/*      Do we have a GEOGCS node, but no authority?  If so, try         */
/*      guessing it.                                                    */
/* -------------------------------------------------------------------- */
    if( (IsProjected() || IsGeographic())
        && GetAuthorityCode( "GEOGCS" ) == nullptr )
    {
        const int nGCS = GetEPSGGeogCS();
        if( nGCS != -1 )
            SetAuthority( "GEOGCS", "EPSG", nGCS );
    }

    if( IsProjected() && GetAuthorityCode( "PROJCS") == nullptr )
    {
        const char *pszProjection = GetAttrValue( "PROJECTION" );

/* -------------------------------------------------------------------- */
/*      Is this a UTM coordinate system with a common GEOGCS?           */
/* -------------------------------------------------------------------- */
        int nZone = 0;
        int bNorth = FALSE;
        if( (nZone = GetUTMZone( &bNorth )) != 0 )
        {
            const char *pszAuthName = GetAuthorityName( "PROJCS|GEOGCS" );
            const char *pszAuthCode = GetAuthorityCode( "PROJCS|GEOGCS" );

            if( pszAuthName == nullptr || pszAuthCode == nullptr )
            {
                // Don't exactly recognise datum.
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4326 )
            {
                // WGS84
                if( bNorth )
                    SetAuthority( "PROJCS", "EPSG", 32600 + nZone );
                else
                    SetAuthority( "PROJCS", "EPSG", 32700 + nZone );
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4267
                    && nZone >= 3 && nZone <= 22 && bNorth )
            {
                SetAuthority( "PROJCS", "EPSG", 26700 + nZone ); // NAD27
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4269
                    && nZone >= 3 && nZone <= 23 && bNorth )
            {
                SetAuthority( "PROJCS", "EPSG", 26900 + nZone ); // NAD83
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4322 )
            { // WGS72
                if( bNorth )
                    SetAuthority( "PROJCS", "EPSG", 32200 + nZone );
                else
                    SetAuthority( "PROJCS", "EPSG", 32300 + nZone );
            }
        }

/* -------------------------------------------------------------------- */
/*      Is this a Polar Stereographic system on WGS 84 ?                */
/* -------------------------------------------------------------------- */
        else if ( pszProjection != nullptr &&
                  EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
        {
            const char *pszAuthName = GetAuthorityName( "PROJCS|GEOGCS" );
            const char *pszAuthCode = GetAuthorityCode( "PROJCS|GEOGCS" );
            const double dfLatOrigin = GetNormProjParm(
                                            SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );

            if( pszAuthName != nullptr && EQUAL(pszAuthName, "EPSG") &&
                pszAuthCode != nullptr && atoi(pszAuthCode) == 4326 &&
                fabs( fabs(dfLatOrigin ) - 71.0 ) < 1e-15 &&
                fabs(GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 )) < 1e-15 &&
                fabs(GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) - 1.0) < 1e-15 &&
                fabs(GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 )) < 1e-15 &&
                fabs(GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 )) < 1e-15 &&
                fabs(GetLinearUnits() - 1.0) < 1e-15 )
            {
                if( dfLatOrigin > 0 )
                    // Arctic Polar Stereographic
                    SetAuthority( "PROJCS", "EPSG", 3995 );
                else
                    // Antarctic Polar Stereographic
                    SetAuthority( "PROJCS", "EPSG", 3031 );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Return.                                                         */
/* -------------------------------------------------------------------- */
    if( IsProjected() && GetAuthorityCode("PROJCS") != nullptr )
        return OGRERR_NONE;

    if( IsGeographic() && GetAuthorityCode("GEOGCS") != nullptr )
        return OGRERR_NONE;

    return OGRERR_UNSUPPORTED_SRS;
}

/************************************************************************/
/*                        OSRAutoIdentifyEPSG()                         */
/************************************************************************/

/**
 * \brief Set EPSG authority info if possible.
 *
 * This function is the same as OGRSpatialReference::AutoIdentifyEPSG().
 *
 * Since GDAL 2.3, the OSRFindMatches() function can also be used for improved
 * matching by researching the EPSG catalog.
 *
 */

OGRErr OSRAutoIdentifyEPSG( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRAutoIdentifyEPSG", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->AutoIdentifyEPSG();
}

/************************************************************************/
/*                        EPSGTreatsAsLatLong()                         */
/************************************************************************/

/**
 * \brief This method returns TRUE if EPSG feels this geographic coordinate
 * system should be treated as having lat/long coordinate ordering.
 *
 * Currently this returns TRUE for all geographic coordinate systems
 * with an EPSG code set, and AXIS values set defining it as lat, long.
 * Note that coordinate systems with an EPSG code and no axis settings
 * will be assumed to not be lat/long.
 *
 * FALSE will be returned for all coordinate systems that are not geographic,
 * or that do not have an EPSG code set.
 *
 * This method is the same as the C function OSREPSGTreatsAsLatLong().
 *
 * @return TRUE or FALSE.
 */

int OGRSpatialReference::EPSGTreatsAsLatLong() const

{
    if( !IsGeographic() )
        return FALSE;

    const char *pszAuth = GetAuthorityName( "GEOGCS" );

    if( pszAuth == nullptr || !EQUAL(pszAuth, "EPSG") )
        return FALSE;

    const OGR_SRSNode * const poFirstAxis = GetAttrNode( "GEOGCS|AXIS" );

    if( poFirstAxis == nullptr )
        return FALSE;

    if( poFirstAxis->GetChildCount() >= 2
        && EQUAL(poFirstAxis->GetChild(1)->GetValue(), "NORTH") )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                       OSREPSGTreatsAsLatLong()                       */
/************************************************************************/

/**
 * \brief This function returns TRUE if EPSG feels this geographic coordinate
 * system should be treated as having lat/long coordinate ordering.
 *
 * This function is the same as OGRSpatialReference::OSREPSGTreatsAsLatLong().
 */

int OSREPSGTreatsAsLatLong( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSREPSGTreatsAsLatLong", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->EPSGTreatsAsLatLong();
}

/************************************************************************/
/*                     EPSGTreatsAsNorthingEasting()                    */
/************************************************************************/

/**
 * \brief This method returns TRUE if EPSG feels this projected coordinate
 * system should be treated as having northing/easting coordinate ordering.
 *
 * Currently this returns TRUE for all projected coordinate systems
 * with an EPSG code set, and AXIS values set defining it as northing, easting.
 *
 * FALSE will be returned for all coordinate systems that are not projected,
 * or that do not have an EPSG code set.
 *
 * This method is the same as the C function EPSGTreatsAsNorthingEasting().
 *
 * @return TRUE or FALSE.
 *
 * @since OGR 1.10.0
 */

int OGRSpatialReference::EPSGTreatsAsNorthingEasting() const

{
    if( !IsProjected() )
        return FALSE;

    const char *pszAuth = GetAuthorityName( "PROJCS" );

    if( pszAuth == nullptr || !EQUAL(pszAuth, "EPSG") )
        return FALSE;

    const OGR_SRSNode * const poFirstAxis = GetAttrNode( "PROJCS|AXIS" );

    if( poFirstAxis == nullptr )
        return FALSE;

    if( poFirstAxis->GetChildCount() >= 2
        && EQUAL(poFirstAxis->GetChild(1)->GetValue(), "NORTH") )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                     OSREPSGTreatsAsNorthingEasting()                 */
/************************************************************************/

/**
 * \brief This function returns TRUE if EPSG feels this geographic coordinate
 * system should be treated as having northing/easting coordinate ordering.
 *
 * This function is the same as
 * OGRSpatialReference::EPSGTreatsAsNorthingEasting().
 *
 * @since OGR 1.10.0
 */

int OSREPSGTreatsAsNorthingEasting( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSREPSGTreatsAsNorthingEasting", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        EPSGTreatsAsNorthingEasting();
}

/************************************************************************/
/*                   CleanupFindMatchesCacheAndMutex()                  */
/************************************************************************/

void CleanupFindMatchesCacheAndMutex()
{
    if( hFindMatchesMutex != nullptr )
    {
        CPLDestroyMutex(hFindMatchesMutex);
        hFindMatchesMutex = nullptr;
    }
    if( papoSRSCache_GEOGCS )
    {
        for( auto& poSRS: *papoSRSCache_GEOGCS )
            delete poSRS;
        delete papoSRSCache_GEOGCS;
        papoSRSCache_GEOGCS = nullptr;
    }
    if( papoSRSCache_PROJCS )
    {
        for( auto& poSRS: *papoSRSCache_PROJCS )
            delete poSRS;
        delete papoSRSCache_PROJCS;
        papoSRSCache_PROJCS = nullptr;
    }
    delete poMapESRIPROJCSNameToEPSGCode;
    poMapESRIPROJCSNameToEPSGCode = nullptr;
    delete poMapESRIGEOGCSNameToEPSGCode;
    poMapESRIGEOGCSNameToEPSGCode = nullptr;

}

/************************************************************************/
/*                           MassageSRSName()                           */
/************************************************************************/

/* Transform a SRS name typically coming from EPSG or ESRI into a simplified
 * form that can be compared. */
static CPLString MassageSRSName(const char* pszInput, bool bExtraMassaging)
{
    CPLString osRet;
    bool bLastWasSep = false;
    for( int i = 0; pszInput[i] != '\0'; ++ i)
    {
        if( isdigit( pszInput[i] ) )
        {
            if( (i > 0 && isalpha( pszInput[i-1] )) || bLastWasSep )
                osRet += "_";
            bLastWasSep = false;

            /* Abbreviate 19xx as xx */
            if( pszInput[i] == '1' && pszInput[i+1] == '9' &&
                pszInput[i+2] != '\0' && isdigit(pszInput[i+2]) &&
                (i == 0 || !isdigit(pszInput[i-1])) )
            {
                i ++;
                continue;
            }
            osRet += pszInput[i];
        }
        else if( isalpha( pszInput[i] ) )
        {
            if( bLastWasSep )
                osRet += "_";
            osRet += pszInput[i];
            bLastWasSep = false;
        }
        else
        {
            bLastWasSep = true;
        }
    }

    osRet.tolower();
    osRet.replaceAll("gauss_kruger", "gk"); // EPSG -> ESRI
    osRet.replaceAll("rt_90_25", "rt_90_2_5"); // ESRI -> EPSG
    osRet.replaceAll("rt_38_25", "rt_38_2_5"); // ESRI -> EPSG
    osRet.replaceAll("_zone_", "_"); // EPSG -> ESRI
    osRet.replaceAll("_stateplane_", "_"); // ESRI -> EPSG
    osRet.replaceAll("_nsidc_", "_"); // EPSG -> ESRI
    osRet.replaceAll("_I_", "_1_"); // ESRI -> EPSG
    osRet.replaceAll("_II_", "_2_"); // ESRI -> EPSG
    osRet.replaceAll("_III_", "_3_"); // ESRI -> EPSG
    osRet.replaceAll("_IV_", "_4_"); // ESRI -> EPSG
    osRet.replaceAll("_V_", "_5_"); // ESRI -> EPSG
    osRet.replaceAll("pulkovo_42_adj_83_", "pulkovo_42_83_"); // ESRI -> EPSG
    osRet.replaceAll("_old_fips", "_deprecated_fips");
    if( bExtraMassaging )
        osRet.replaceAll("_deprecated", ""); // EPSG -> ESRI

    // _FIPS_XXXX_Feet  --> _ftUS       ESRI -> EPSG
    // _FIPS_XXXX_Ft_US --> _ftUS       ESRI -> EPSG
    // _FIPS_XXXX       --> ""          ESRI -> EPSG
    size_t nPos = osRet.find("_fips_");
    if( nPos != std::string::npos )
    {
        size_t nPos2 = osRet.find("_feet", nPos + strlen("_fips_"));
        if( nPos2 != std::string::npos &&
            nPos2 + strlen("_feet") == osRet.size() )
        {
            osRet.resize(nPos);
            osRet += "_ftus";
        }
        else
        {
            nPos2 =  osRet.find("_ft_us", nPos + strlen("_fips_"));
            if( nPos2 != std::string::npos &&
                nPos2 + strlen("_ft_us") == osRet.size() )
            {
                osRet.resize(nPos);
                osRet += "_ftus";
            }
            else if( osRet.find('_', nPos + strlen("_fips_")) ==
                                                            std::string::npos )
            {
                osRet.resize(nPos);
            }
        }
    }

    return osRet;
}

/************************************************************************/
/*                            IngestDict()                              */
/************************************************************************/

static void IngestDict(const char* pszDictFile,
                       const char* pszSRSType,
                       std::vector<OGRSpatialReference*>* papoSRSCache,
                       VSILFILE* fpOut)
{
/* -------------------------------------------------------------------- */
/*      Find and open file.                                             */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CPLFindFile( "gdal", pszDictFile );
    if( pszFilename == nullptr )
        return;

    VSILFILE *fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == nullptr )
        return;

    if( fpOut )
    {
        VSIFPrintfL(fpOut, "# From %s\n", pszDictFile);
    }

/* -------------------------------------------------------------------- */
/*      Process lines.                                                  */
/* -------------------------------------------------------------------- */
    const char *pszLine = nullptr;
    while( (pszLine = CPLReadLineL(fp)) != nullptr )
    {
        if( pszLine[0] == '#' )
            continue;

        const char* pszComma = strchr(pszLine, ',');
        if( pszComma )
        {
            const char* pszWKT = pszComma + 1;
            if( STARTS_WITH(pszWKT, pszSRSType) )
            {
                OGRSpatialReference* poSRS = new OGRSpatialReference();
                if( poSRS->SetFromUserInput(pszWKT) == OGRERR_NONE )
                {
                    const char *pszProjection = poSRS->GetAttrValue( "PROJECTION" );
                    if( pszProjection &&
                        EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
                    {
                        // Remove duplicate Standard_Parallel_1
                        double dfLatOrigin =
                            poSRS->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN);
                        double dfStdParallel1 =
                            poSRS->GetProjParm(SRS_PP_STANDARD_PARALLEL_1);
                        if( dfLatOrigin == dfStdParallel1 )
                        {
                            OGR_SRSNode *poPROJCS = poSRS->GetAttrNode( "PROJCS" );
                            if( poPROJCS )
                            {
                                const int iChild = poSRS->FindProjParm(
                                    SRS_PP_STANDARD_PARALLEL_1, poPROJCS );
                                if( iChild != -1 )
                                    poPROJCS->DestroyChild( iChild);
                            }
                        }
                    }
                    poSRS->morphFromESRI();

                    papoSRSCache->push_back(poSRS);

                    if( fpOut )
                    {
                        VSIFPrintfL(fpOut, "%s\n", pszWKT);
                    }
                }
                else
                {
                    delete poSRS;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );
}

/************************************************************************/
/*                        BuildESRICSNameCache()                        */
/************************************************************************/

static void BuildESRICSNameCache(const char* pszSRSType,
                                 std::map<CPLString, int>* poMapCSNameToCode,
                                 VSILFILE* fpOut)
{
    const char *pszFilename = CPLFindFile( "gdal", "esri_epsg.wkt" );
    if( pszFilename == nullptr )
        return;

    VSILFILE *fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Process lines.                                                  */
/* -------------------------------------------------------------------- */
    const char *pszLine = nullptr;
    while( (pszLine = CPLReadLineL(fp)) != nullptr )
    {
        if( pszLine[0] == '#' )
            continue;

        const char* pszComma = strchr(pszLine, ',');
        if( pszComma )
        {
            const char* pszWKT = pszComma + 1;
            if( STARTS_WITH(pszWKT, pszSRSType) )
            {
                OGRSpatialReference oSRS;
                if( oSRS.SetFromUserInput(pszWKT) == OGRERR_NONE )
                {
                    const char* pszSRSName = oSRS.GetAttrValue(pszSRSType);
                    const char* pszAuthCode = oSRS.GetAuthorityCode(nullptr);
                    if( pszSRSName && pszAuthCode )
                    {
                        (*poMapCSNameToCode)[pszSRSName] = atoi(pszAuthCode);
                        VSIFPrintfL(fpOut, "%s,%s\n", pszSRSName, pszAuthCode);
                    }
                }
            }
        }
    }

    VSIFCloseL( fp );
}

/************************************************************************/
/*                            GetSRSCache()                             */
/************************************************************************/

const std::vector<OGRSpatialReference*>* OGRSpatialReference::GetSRSCache(
                                const char* pszSRSType,
                                const std::map<CPLString, int>*& poMapCSNameToCodeOut)
{
    if( pszSRSType == nullptr )
        return nullptr;

    std::vector<OGRSpatialReference*>* papoSRSCache = nullptr;
    std::map<CPLString, int>* poMapCSNameToCode = nullptr;

    CPLMutexHolderD(&hFindMatchesMutex);
    CPLString osFilename;
    const char* pszFilename = "";
    if( EQUAL(pszSRSType, "PROJCS") )
    {
        pszFilename = "pcs.csv";
        if( papoSRSCache_PROJCS != nullptr )
            return papoSRSCache_PROJCS;
        papoSRSCache_PROJCS = new std::vector<OGRSpatialReference*>();
        papoSRSCache = papoSRSCache_PROJCS;
        poMapESRIPROJCSNameToEPSGCode = new std::map<CPLString, int>();
        poMapCSNameToCode = poMapESRIPROJCSNameToEPSGCode;
    }
    else if( EQUAL(pszSRSType, "GEOGCS") )
    {
        pszFilename = "gcs.csv" ;
        if( papoSRSCache_GEOGCS != nullptr )
            return papoSRSCache_GEOGCS;
        papoSRSCache_GEOGCS = new std::vector<OGRSpatialReference*>();
        papoSRSCache = papoSRSCache_GEOGCS;
        poMapESRIGEOGCSNameToEPSGCode = new std::map<CPLString, int>();
        poMapCSNameToCode = poMapESRIGEOGCSNameToEPSGCode;
    }
    else
    {
        return nullptr;
    }
    poMapCSNameToCodeOut = poMapCSNameToCode;

    // First try to look an already built SRS cache in ~/.gdal/X.Y/srs_cache
    const char* pszHome = CPLGetHomeDir();
    const char* pszCSVFilename = CSVFilename(pszFilename);
    CPLString osCacheFilename;
    CPLString osCacheDirectory =
            CPLGetConfigOption("OSR_SRS_CACHE_DIRECTORY", "");
    if( (pszHome != nullptr || !osCacheDirectory.empty()) &&
        CPLTestBool(CPLGetConfigOption("OSR_SRS_CACHE", "YES")) )
    {
        if( osCacheDirectory.empty() )
        {
            osCacheDirectory = CPLFormFilename(pszHome, ".gdal", nullptr);
            // Version this, because might be sensitive to GDAL / 
            // EPSG versions
            osCacheDirectory = CPLFormFilename(osCacheDirectory,
                CPLSPrintf("%d.%d",
                            GDAL_VERSION_MAJOR,
                            GDAL_VERSION_MINOR), nullptr);
            osCacheDirectory = CPLFormFilename(osCacheDirectory,
                                                "srs_cache", nullptr);
        }
        osCacheFilename = CPLFormFilename(osCacheDirectory,
                        CPLResetExtension(pszFilename, "wkt"), nullptr);
        VSIStatBufL sStatCache;
        VSIStatBufL sStatCSV;
        VSILFILE* fp = nullptr;
        VSILFILE* fpESRINames = nullptr;
        if( VSIStatL((osCacheFilename + ".gz").c_str(), &sStatCache) == 0 &&
            VSIStatL(pszCSVFilename, &sStatCSV) == 0 &&
            sStatCache.st_mtime >= sStatCSV.st_mtime &&
            (VSIStatL(CPLResetExtension(
                pszCSVFilename, "override.csv"), &sStatCSV) != 0 ||
                sStatCache.st_mtime >= sStatCSV.st_mtime) )
        {
            fp = VSIFOpenL(("/vsigzip/" + osCacheFilename + ".gz").c_str(), "rb");
            if( fp )
            {
                fpESRINames = VSIFOpenL(
                    (CPLString("/vsigzip/") +
                     CPLResetExtension(osCacheFilename, "esri.gz")).c_str(), "rb");
            }
        }
        if( fp )
        {
            CPLDebug("OSR", "Using %s cache",
                        osCacheFilename.c_str());
            const char* pszLine;
            while( (pszLine = CPLReadLineL(fp)) != nullptr )
            {
                OGRSpatialReference* poSRS =
                                new OGRSpatialReference();
                poSRS->SetFromUserInput(pszLine);
                papoSRSCache->push_back(poSRS);
                const char* pszSRSName = poSRS->GetAttrValue(pszSRSType);
                const char* pszAuthCode = poSRS->GetAuthorityCode(nullptr);
                if( pszSRSName && pszAuthCode )
                {
                    (*poMapCSNameToCode)[pszSRSName] = atoi(pszAuthCode);
                }
            }
            VSIFCloseL(fp);

            if( fpESRINames )
            {
                while( (pszLine = CPLReadLineL(fpESRINames)) != nullptr )
                {
                    const char* pszComma = strchr(pszLine, ',');
                    if( pszComma )
                    {
                        CPLString osName(pszLine);
                        osName.resize(pszComma - pszLine);
                        (*poMapCSNameToCode)[osName] = atoi(pszComma + 1);
                    }
                }
                VSIFCloseL(fpESRINames);
            }

            return papoSRSCache;
        }
    }

    // If no already built cache, ingest the EPSG database and write the
    // cache
    CPLDebug("OSR", "Building %s cache", pszSRSType);
    VSILFILE* fp = VSIFOpenL(pszCSVFilename, "rb");
    if( fp == nullptr )
    {
        return nullptr;
    }
    VSILFILE* fpOut = nullptr;
    if( !osCacheFilename.empty() )
    {
        CPLString osDirname(CPLGetDirname(osCacheFilename));
        CPLString osDirnameParent(CPLGetDirname(osDirname));
        CPLString osDirnameGrantParent(
                                CPLGetDirname(osDirnameParent));
        VSIMkdir( osDirnameGrantParent, 0755 );
        VSIMkdir( osDirnameParent, 0755 );
        VSIMkdir( osDirname, 0755 );
        fpOut = VSIFOpenL(
            ("/vsigzip/" + osCacheFilename + ".gz").c_str(), "wb");
        if( fpOut != nullptr )
        {
            VSIFPrintfL(fpOut, "# From %s\n", pszFilename);
        }
    }
    const char* pszLine;
    OGRSpatialReference* poSRS = nullptr;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    while( (pszLine = CPLReadLineL(fp)) != nullptr )
    {
        if( poSRS == nullptr )
            poSRS = new OGRSpatialReference();
        int nCode = atoi(pszLine);
        if( nCode > 0 &&
            poSRS->importFromEPSGAInternal(nCode, pszSRSType)
                                                == OGRERR_NONE )
        {
            // Strip AXIS like in importFromEPSG()
            OGR_SRSNode *poGEOGCS = poSRS->GetAttrNode( "GEOGCS" );

            if( poGEOGCS != nullptr )
                poGEOGCS->StripNodes( "AXIS" );

            OGR_SRSNode *poPROJCS = poSRS->GetAttrNode( "PROJCS" );
            if( poPROJCS != nullptr &&
                poSRS->EPSGTreatsAsNorthingEasting() )
                poPROJCS->StripNodes( "AXIS" );

            if( fpOut )
            {
                char* pszWKT = nullptr;
                poSRS->exportToWkt(&pszWKT);
                if( pszWKT )
                {
                    VSIFPrintfL(fpOut, "%s\n", pszWKT);
                    CPLFree(pszWKT);
                }
            }

            papoSRSCache->push_back(poSRS);
            const char* pszSRSName = poSRS->GetAttrValue(pszSRSType);
            if( pszSRSName )
            {
                (*poMapCSNameToCode)[pszSRSName] = nCode;
            }
            poSRS = nullptr;
        }
    }
    CPLPopErrorHandler();
    delete poSRS;
    VSIFCloseL(fp);

    IngestDict("esri_extra.wkt", pszSRSType, papoSRSCache, fpOut);

    if( fpOut )
        VSIFCloseL(fpOut);

    fpOut = VSIFOpenL(
        (CPLString("/vsigzip/") +
            CPLResetExtension(osCacheFilename, "esri.gz")).c_str(), "wb");
    if( fpOut != nullptr )
    {
        BuildESRICSNameCache(pszSRSType, poMapCSNameToCode, fpOut);
        VSIFCloseL(fpOut);
    }

    return papoSRSCache;
}

/************************************************************************/
/*                            FindMatches()                             */
/************************************************************************/

/**
 * \brief Try to identify a match between the passed SRS and a related SRS
 * in a catalog (currently EPSG only)
 *
 * Matching may be partial, or may fail.
 * Returned entries will be sorted by decreasing match confidence (first
 * entry has the highest match confidence).
 *
 * The exact way matching is done may change in future versions.
 * 
 *  The current algorithm is:
 * - try first AutoIdentifyEPSG(). If it succeeds, return the corresponding SRS
 * - otherwise iterate over all SRS from the EPSG catalog (as found in GDAL
 *   pcs.csv and gcs.csv files+esri_extra.wkt), and find those that match the
 *   input SRS using the IsSame() function (ignoring TOWGS84 clauses)
 * - if there is a single match using IsSame() or one of the matches has the
 *   same SRS name, return it with 100% confidence
 * - if a SRS has the same SRS name, but does not pass the IsSame() criteria,
 *   return it with 50% confidence.
 * - otherwise return all candidate SRS that pass the IsSame() criteria with a
 *   90% confidence.
 * 
 * A pre-built SRS cache in ~/.gdal/X.Y/srs_cache will be used if existing,
 * otherwise it will be built at the first run of this function.
 *
 * This method is the same as OSRFindMatches().
 *
 * @param papszOptions NULL terminated list of options or NULL
 * @param pnEntries Output parameter. Number of values in the returned array.
 * @param ppanMatchConfidence Output parameter (or NULL). *ppanMatchConfidence
 * will be allocated to an array of *pnEntries whose values between 0 and 100
 * indicate the confidence in the match. 100 is the highest confidence level.
 * The array must be freed with CPLFree().
 * 
 * @return an array of SRS that match the passed SRS, or NULL. Must be freed with
 * OSRFreeSRSArray()
 *
 * @since GDAL 2.3
 */
OGRSpatialReferenceH* OGRSpatialReference::FindMatches(
                                          char** papszOptions,
                                          int* pnEntries,
                                          int** ppanMatchConfidence ) const
{
    CPL_IGNORE_RET_VAL(papszOptions);

    if( pnEntries )
        *pnEntries = 0;
    if( ppanMatchConfidence )
        *ppanMatchConfidence = nullptr;

    OGRSpatialReference oSRSClone(*this);
    if( oSRSClone.AutoIdentifyEPSG() == OGRERR_NONE )
    {
        const char* pszCode = oSRSClone.GetAuthorityCode(nullptr);
        if( pszCode )
            oSRSClone.importFromEPSG(atoi(pszCode));
        OGRSpatialReferenceH* pahRet =
            static_cast<OGRSpatialReferenceH*>(
                    CPLCalloc(sizeof(OGRSpatialReferenceH), 2));
        pahRet[0] = reinterpret_cast<OGRSpatialReferenceH>(oSRSClone.Clone());
        if( pnEntries )
            *pnEntries = 1;
        if( ppanMatchConfidence )
        {
            *ppanMatchConfidence = static_cast<int*>(CPLMalloc(sizeof(int)));
            (*ppanMatchConfidence)[0] = 100;
        }
        return pahRet;
    }

    const char* pszSRSType = "";
    if( IsProjected() )
    {
        pszSRSType = "PROJCS";
    }
    else if( IsGeographic() )
    {
        pszSRSType = "GEOGCS";
    }
    else
    {
        return nullptr;
    }
    const char*pszSRSName = GetAttrValue(pszSRSType);
    if( pszSRSName == nullptr )
        return nullptr;

    const std::map<CPLString, int>* poMapCSNameToCode = nullptr;
    const std::vector<OGRSpatialReference*>* papoSRSCache =
        GetSRSCache(pszSRSType, poMapCSNameToCode);
    if( papoSRSCache == nullptr )
        return nullptr;

    // If we have an exact match with a coordinate system name coming from
    // EPSG entries (either ours or ESRI), and the SRS are equivalent, then
    // use that exact match
    const char* apszOptions[] = { "TOWGS84=ONLY_IF_IN_BOTH", nullptr };
    if( poMapCSNameToCode )
    {
        auto oIter = poMapCSNameToCode->find(pszSRSName);
        if( oIter != poMapCSNameToCode->end() )
        {
            OGRSpatialReference oSRS;
            if( oSRS.importFromEPSG(oIter->second) == OGRERR_NONE )
            {
                if( IsSame(&oSRS, apszOptions) )
                {
                    OGRSpatialReferenceH* pahRet =
                        static_cast<OGRSpatialReferenceH*>(
                                CPLCalloc(sizeof(OGRSpatialReferenceH), 2));
                    pahRet[0] = reinterpret_cast<OGRSpatialReferenceH>(
                        oSRS.Clone());
                    if( pnEntries )
                        *pnEntries = 1;
                    if( ppanMatchConfidence )
                    {
                        *ppanMatchConfidence = static_cast<int*>(
                            CPLMalloc(sizeof(int)));
                        (*ppanMatchConfidence)[0] = 100;
                    }
                    return pahRet;
                }
            }
        }
    }

    std::vector< OGRSpatialReference* > apoSameSRS;
    CPLString osSRSName(MassageSRSName(pszSRSName, false));
    CPLString osSRSNameExtra(MassageSRSName(osSRSName, true));
    std::vector<size_t> anMatchingSRSNameIndices;
    for(size_t i = 0; i < papoSRSCache->size(); i++ )
    {
        const OGRSpatialReference* poOtherSRS = (*papoSRSCache)[i];
#ifdef notdef
        if( EQUAL(poOtherSRS->GetAuthorityCode(nullptr), "2765") )
        {
            printf("brkpt\n"); /* ok */
        }
#endif
        const char* pszOtherSRSName = poOtherSRS->GetAttrValue(pszSRSType);
        if( pszOtherSRSName == nullptr )
            continue;
        CPLString osOtherSRSName(
            MassageSRSName(pszOtherSRSName, false));
        if( EQUAL( osSRSName, osOtherSRSName ) )
        {
            anMatchingSRSNameIndices.push_back(i);
        }
        if( IsSame(poOtherSRS, apszOptions) )
        {
            apoSameSRS.push_back( poOtherSRS->Clone() );
        }
    }

    const size_t nSameCount = apoSameSRS.size();

    if( nSameCount == 1 )
    {
        OGRSpatialReferenceH* pahRet =
            static_cast<OGRSpatialReferenceH*>(
                    CPLCalloc(sizeof(OGRSpatialReferenceH), 2));
        pahRet[0] = reinterpret_cast<OGRSpatialReferenceH>(apoSameSRS[0]);
        if( pnEntries )
            *pnEntries = 1;
        if( ppanMatchConfidence )
        {
            *ppanMatchConfidence = static_cast<int*>(CPLMalloc(sizeof(int)));
            (*ppanMatchConfidence)[0] = 100;
        }
        return pahRet;
    }

    int nCountExtraMatches = 0;
    size_t iExtraMatch = 0;
    for(size_t i=0; i<nSameCount; i++)
    {
        CPLString osOtherSRSName(
            MassageSRSName(apoSameSRS[i]->GetAttrValue(pszSRSType), false));
        CPLString osOtherSRSNameExtra(MassageSRSName(osOtherSRSName, true));
        if( EQUAL(osSRSName, osOtherSRSName) )
        {
            OGRSpatialReferenceH* pahRet =
                static_cast<OGRSpatialReferenceH*>(
                        CPLCalloc(sizeof(OGRSpatialReferenceH), 2));
            pahRet[0] = reinterpret_cast<OGRSpatialReferenceH>(apoSameSRS[i]);
            if( pnEntries )
                *pnEntries = 1;
            if( ppanMatchConfidence )
            {
                *ppanMatchConfidence = static_cast<int*>(CPLMalloc(sizeof(int)));
                (*ppanMatchConfidence)[0] = 100;
            }
            for(size_t j=0; j<nSameCount; j++)
            {
                if( i != j )
                    delete apoSameSRS[j];
            }
            return pahRet;
        }
        else if ( EQUAL(osSRSNameExtra, osOtherSRSNameExtra) )
        {
            nCountExtraMatches ++;
            iExtraMatch = i;
        }
    }

    if( nCountExtraMatches == 1 )
    {
        OGRSpatialReferenceH* pahRet =
            static_cast<OGRSpatialReferenceH*>(
                    CPLCalloc(sizeof(OGRSpatialReferenceH), 2));
        pahRet[0] = reinterpret_cast<OGRSpatialReferenceH>(apoSameSRS[iExtraMatch]);
        if( pnEntries )
            *pnEntries = 1;
        if( ppanMatchConfidence )
        {
            *ppanMatchConfidence = static_cast<int*>(CPLMalloc(sizeof(int)));
            (*ppanMatchConfidence)[0] = 100;
        }
        for(size_t j=0; j<nSameCount; j++)
        {
            if( iExtraMatch != j )
                delete apoSameSRS[j];
        }
        return pahRet;
    }

    if( nSameCount == 0 && anMatchingSRSNameIndices.size() == 1 )
    {
        const OGRSpatialReference* poOtherSRS =
                            (*papoSRSCache)[anMatchingSRSNameIndices[0]];
        OGRSpatialReferenceH* pahRet =
            static_cast<OGRSpatialReferenceH*>(
                    CPLCalloc(sizeof(OGRSpatialReferenceH), 2));
        pahRet[0] = reinterpret_cast<OGRSpatialReferenceH>(poOtherSRS->Clone());
        if( pnEntries )
            *pnEntries = 1;
        if( ppanMatchConfidence )
        {
            *ppanMatchConfidence = static_cast<int*>(CPLMalloc(sizeof(int)));
            (*ppanMatchConfidence)[0] = 50;
        }
        return pahRet;
    }

    if( nSameCount == 0 )
        return nullptr;

    if( pnEntries )
        *pnEntries = static_cast<int>(nSameCount);
    OGRSpatialReferenceH* pahRet =
                static_cast<OGRSpatialReferenceH*>(
                        CPLCalloc(sizeof(OGRSpatialReferenceH),
                                  nSameCount + 1));
    if( ppanMatchConfidence )
    {
        *ppanMatchConfidence = static_cast<int*>(
                            CPLMalloc(sizeof(int) * (nSameCount + 1)));
    }
    for(size_t i=0; i<nSameCount; i++)
    {
        pahRet[i] = reinterpret_cast<OGRSpatialReferenceH>(apoSameSRS[i]);
        if( ppanMatchConfidence )
            (*ppanMatchConfidence)[i] = 90; // Arbitrary...
    }
    pahRet[ nSameCount ] = nullptr;

    return pahRet;
}

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

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <limits>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"


CPL_CVSID("$Id$")

extern void OGRsnPrintDouble( char * pszStrBuf, size_t size, double dfValue );

int EPSGGetWGS84Transform( int nGeogCS, std::vector<CPLString>& asTransform );
void OGREPSGDatumNameMassage( char ** ppszDatum );

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
    NULL
};

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
    for( int i = 0; apszDatumEquiv[i] != NULL; i += 2 )
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
        if( pszDecimal != NULL && strlen(pszDecimal) > 1 )
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
                    strncpy( szSeconds+3, pszDecimal + 5, sizeof(szSeconds)-3 );
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
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup("degree");
        if( pdfInDegrees != NULL )
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
    if( ppszUOMName != NULL )
        *ppszUOMName = CPLStrdup( pszUOMName );

    if( pdfInDegrees != NULL )
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
        if( ppszUOMName != NULL )
            *ppszUOMName = CPLStrdup( "metre" );
        if( pdfInMeters != NULL )
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

    if( papszUnitsRecord == NULL )
        return false;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszUOMName != NULL )
    {
        const int iNameField =
            CSVGetFileFieldId( uom_filename, "UNIT_OF_MEAS_NAME" );
        *ppszUOMName = CPLStrdup( CSLGetField(papszUnitsRecord, iNameField) );
    }

/* -------------------------------------------------------------------- */
/*      Get the A and B factor fields, and create the multiplicative    */
/*      factor.                                                         */
/* -------------------------------------------------------------------- */
    if( pdfInMeters != NULL )
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
    if( papszLine == NULL )
    {
        pszFilename = CSVFilename("gcs.csv");
        snprintf( szCode, sizeof(szCode), "%d", nGeogCS );
        papszLine = CSVScanFileByName( pszFilename,
                                       "COORD_REF_SYS_CODE",
                                       szCode, CC_Integer );
    }

    if( papszLine == NULL )
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

    asTransform.resize(0);
    for( int iField = 0; iField < 7; iField++ )
    {
        const char* pszValue = papszLine[iDXField+iField];
        if( pszValue[0] )
            asTransform.push_back(pszValue);
        else
            asTransform.push_back("0");
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
        if( pdfOffset != NULL )
            *pdfOffset = 0.0;
        if( ppszName != NULL )
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
    if( pdfOffset != NULL )
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
    if( ppszName != NULL )
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

    int nDatum = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                         szSearchKey, CC_Integer,
                                         "DATUM_CODE" ) );

    if( nDatum < 1 )
    {
        pszFilename = CSVFilename("gcs.csv");
        snprintf( szSearchKey, sizeof(szSearchKey), "%d", nGCSCode );

        nDatum = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                   szSearchKey, CC_Integer,
                                   "DATUM_CODE" ) );
    }

    if( nDatum < 1 )
        return false;

    if( pnDatum != NULL )
        *pnDatum = nDatum;

/* -------------------------------------------------------------------- */
/*      Get the PM.                                                     */
/* -------------------------------------------------------------------- */
    const int nPM = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                      szSearchKey, CC_Integer,
                                      "PRIME_MERIDIAN_CODE" ) );

    if( nPM < 1 )
        return false;

    if( pnPM != NULL )
        *pnPM = nPM;

/* -------------------------------------------------------------------- */
/*      Get the Ellipsoid.                                              */
/* -------------------------------------------------------------------- */
    const int nEllipsoid = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                             szSearchKey, CC_Integer,
                                             "ELLIPSOID_CODE" ) );

    if( nEllipsoid < 1 )
        return false;

    if( pnEllipsoid != NULL )
        *pnEllipsoid = nEllipsoid;

/* -------------------------------------------------------------------- */
/*      Get the angular units.                                          */
/* -------------------------------------------------------------------- */
    const int nUOMAngle = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                            szSearchKey, CC_Integer,
                                            "UOM_CODE" ) );

    if( nUOMAngle < 1 )
        return false;

    if( pnUOMAngle != NULL )
        *pnUOMAngle = nUOMAngle;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                   szSearchKey, CC_Integer,
                                   "COORD_REF_SYS_NAME" ));

/* -------------------------------------------------------------------- */
/*      Get the datum name, if requested.                               */
/* -------------------------------------------------------------------- */
    if( ppszDatumName != NULL )
        *ppszDatumName =
            CPLStrdup(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                   szSearchKey, CC_Integer,
                                   "DATUM_NAME" ));

/* -------------------------------------------------------------------- */
/*      Get the CoordSysCode                                            */
/* -------------------------------------------------------------------- */
    const int nCSC = atoi(CSVGetField( pszFilename, "COORD_REF_SYS_CODE",
                                       szSearchKey, CC_Integer,
                                       "COORD_SYS_CODE" ) );

    if( pnCoordSysCode != NULL )
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
    if( !EPSGGetUOMLengthInfo( nUOMLength, NULL, &dfToMeters ) )
    {
        dfToMeters = 1.0;
    }

    dfSemiMajor *= dfToMeters;

    if( pdfSemiMajor != NULL )
        *pdfSemiMajor = dfSemiMajor;

/* -------------------------------------------------------------------- */
/*      Get the semi-minor if requested.  If the Semi-minor axis        */
/*      isn't available, compute it based on the inverse flattening.    */
/* -------------------------------------------------------------------- */
    if( pdfInvFlattening != NULL )
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
    if( ppszName != NULL )
        *ppszName =
            CPLStrdup(CSVGetField( CSVFilename("ellipsoid.csv" ),
                                   "ELLIPSOID_CODE", szSearchKey, CC_Integer,
                                   "ELLIPSOID_NAME" ));

    return OGRERR_NONE;
}

static const int CoLatConeAxis =        1036;  // See #4223.
static const int NatOriginLat =         8801;
static const int NatOriginLong =        8802;
static const int NatOriginScaleFactor = 8805;
static const int FalseEasting =         8806;
static const int FalseNorthing =        8807;
static const int ProjCenterLat =        8811;
static const int ProjCenterLong =       8812;
static const int Azimuth =              8813;
static const int AngleRectifiedToSkewedGrid = 8814;
static const int InitialLineScaleFactor = 8815;
static const int ProjCenterEasting =    8816;
static const int ProjCenterNorthing =   8817;
static const int PseudoStdParallelLat = 8818;
static const int PseudoStdParallelScaleFactor = 8819;
static const int FalseOriginLat =       8821;
static const int FalseOriginLong =      8822;
static const int StdParallel1Lat =      8823;
static const int StdParallel2Lat =      8824;
static const int FalseOriginEasting =   8826;
static const int FalseOriginNorthing =  8827;
static const int SphericalOriginLat =   8828;
static const int SphericalOriginLong =  8829;
#if 0
static const int InitialLongitude =     8830;
static const int ZoneWidth =            8831;
#endif
static const int PolarLatStdParallel =  8832;
static const int PolarLongOrigin =      8833;

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
        if( panParmIds == NULL )
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

            if( !EPSGGetUOMLengthInfo( nUOM, NULL, &dfInMeters ) )
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
    if( pnProjMethod != NULL )
        *pnProjMethod = nProjMethod;

    if( padfProjParms != NULL )
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

    if( papszRecord == NULL )
    {
        pszFilename = CSVFilename( "pcs.csv" );
        snprintf( szSearchKey, sizeof(szSearchKey), "%d", nPCSCode );
        papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                         szSearchKey, CC_Integer );
    }

    if( papszRecord == NULL )
        return false;

/* -------------------------------------------------------------------- */
/*      Get the name, if requested.                                     */
/* -------------------------------------------------------------------- */
    if( ppszEPSGName != NULL )
    {
        CPLString osPCSName =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "COORD_REF_SYS_NAME"));

        const char *pszDeprecated =
            CSLGetField( papszRecord,
                         CSVGetFileFieldId(pszFilename,
                                           "DEPRECATED") );

        if( pszDeprecated != NULL && *pszDeprecated == '1' )
            osPCSName += " (deprecated)";

        *ppszEPSGName = CPLStrdup(osPCSName);
    }

/* -------------------------------------------------------------------- */
/*      Get the UOM Length code, if requested.                          */
/* -------------------------------------------------------------------- */
    if( pnUOMLengthCode != NULL )
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
    if( pnUOMAngleCode != NULL )
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
    if( pnGeogCS != NULL )
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
    if( pnTRFCode != NULL )
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

    if( pnCoordSysCode != NULL )
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

    char **papszAxis1 = NULL;
    char **papszAxis2 = NULL;
    if( papszRecord != NULL )
    {
        papszAxis1 = CSLDuplicate( papszRecord );
        papszRecord = CSVGetNextLine( pszFilename );
        if( CSLCount(papszRecord) > 0
            && EQUAL(papszRecord[0], papszAxis1[0]) )
            papszAxis2 = CSLDuplicate( papszRecord );
    }

    if( papszAxis2 == NULL )
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
    static const int anCodes[7] = { -1, 9907, 9909, 9906, 9908, -1, -1 };

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

    for( int iAO = 0; iAO < 2; iAO++ )
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
    char *pszGeogCSName = NULL;
    char *pszDatumName = NULL;
    char *pszAngleName = NULL;

    if( !EPSGGetGCSInfo( nGeogCS, &pszGeogCSName,
                         &nDatumCode, &pszDatumName,
                         &nPMCode, &nEllipsoidCode, &nUOMAngle, &nCSC ) )
        return OGRERR_UNSUPPORTED_SRS;

    char *pszPMName = NULL;
    double dfPMOffset = 0.0;
    if( !EPSGGetPMInfo( nPMCode, &pszPMName, &dfPMOffset ) )
    {
        CPLFree( pszDatumName );
        CPLFree( pszGeogCSName );
        return OGRERR_UNSUPPORTED_SRS;
    }

    OGREPSGDatumNameMassage( &pszDatumName );

    char *pszEllipsoidName = NULL;
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
/*      Fetch a parameter from the parm list, based on it's EPSG        */
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
    char *pszPCSName = NULL;
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
    char *pszUOMLengthName = NULL;
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

    if( papszRecord == NULL )
    {
        pszFilename = CSVFilename( "vertcs.csv" );
        papszRecord = CSVScanFileByName( pszFilename, "COORD_REF_SYS_CODE",
                                         szSearchKey, CC_Integer );
    }

    if( papszRecord == NULL )
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

    char *pszUOMLengthName = NULL;
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

    if( papszRecord == NULL )
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

    if( papszRecord == NULL )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Set the GEOCCS node with a name.                                */
/* -------------------------------------------------------------------- */
    poSRS->Clear();
    poSRS->SetGeocCS( CSLGetField( papszRecord,
                                   CSVGetFileFieldId(pszFilename,
                                                     "COORD_REF_SYS_NAME")) );

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
    char *pszPMName = NULL;
    double dfPMOffset = 0.0;

    if( !EPSGGetPMInfo( nPMCode, &pszPMName, &dfPMOffset ) )
    {
        CPLFree( pszDatumName );
        return OGRERR_UNSUPPORTED_SRS;
    }

/* -------------------------------------------------------------------- */
/*      Get the ellipsoid information.                                  */
/* -------------------------------------------------------------------- */
    char *pszEllipsoidName = NULL;
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
    char *pszUOMLengthName = NULL;
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

        if( poGEOGCS != NULL )
            poGEOGCS->StripNodes( "AXIS" );

        OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
        if( poPROJCS != NULL && EPSGTreatsAsNorthingEasting() )
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
    const int nCodeIn = nCode;
    // HACK to support 3D WGS84
    if( nCode == 4979 )
        nCode = 4326;
    bNormInfoSet = FALSE;

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    if( GetRoot() != NULL )
    {
        delete poRoot;
        poRoot = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Verify that we can find the required filename(s).               */
/* -------------------------------------------------------------------- */
    if( CSVScanFileByName( CSVFilename( "gcs.csv" ),
                           "COORD_REF_SYS_CODE",
                           "4269", CC_Integer ) == NULL )
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
    OGRErr eErr = SetEPSGGeogCS( this, nCode );
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

        if( strstr(pszNormalized, "proj=") != NULL )
            eErr = importFromProj4( pszNormalized );

        CPLFree( pszNormalized );
    }

/* -------------------------------------------------------------------- */
/*      Push in authority information if we were successful, and it     */
/*      is not already present.                                         */
/* -------------------------------------------------------------------- */
    const char *pszAuthName = NULL;

    if( IsProjected() )
        pszAuthName = GetAuthorityName( "PROJCS" );
    else
        pszAuthName = GetAuthorityName( "GEOGCS" );

    if( eErr == OGRERR_NONE && (pszAuthName == NULL || nCode != nCodeIn) )
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
        if( poPROJCS != NULL && poPROJCS->FindChild( "AUTHORITY" ) != -1 )
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

int OGRSpatialReference::GetEPSGGeogCS()

{
    const char *pszAuthName = GetAuthorityName( "GEOGCS" );

/* -------------------------------------------------------------------- */
/*      Do we already have it?                                          */
/* -------------------------------------------------------------------- */
    if( pszAuthName != NULL && EQUAL(pszAuthName, "epsg") )
        return atoi(GetAuthorityCode( "GEOGCS" ));

/* -------------------------------------------------------------------- */
/*      Get the datum and geogcs names.                                 */
/* -------------------------------------------------------------------- */
    const char *pszGEOGCS = GetAttrValue( "GEOGCS" );
    const char *pszDatum = GetAttrValue( "DATUM" );

    // We can only operate on coordinate systems with a geogcs.
    if( pszGEOGCS == NULL || pszDatum == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Is this a "well known" geographic coordinate system?            */
/* -------------------------------------------------------------------- */
    const bool bWGS = strstr(pszGEOGCS, "WGS") != NULL
        || strstr(pszDatum, "WGS")
        || strstr(pszGEOGCS, "World Geodetic System")
        || strstr(pszGEOGCS, "World_Geodetic_System")
        || strstr(pszDatum, "World Geodetic System")
        || strstr(pszDatum, "World_Geodetic_System");

    const bool bNAD = strstr(pszGEOGCS, "NAD") != NULL
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

    if( pszAuthName != NULL
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
 * @return OGRERR_NONE or OGRERR_UNSUPPORTED_SRS.
 */

OGRErr OGRSpatialReference::AutoIdentifyEPSG()

{
/* -------------------------------------------------------------------- */
/*      Do we have a GEOGCS node, but no authority?  If so, try         */
/*      guessing it.                                                    */
/* -------------------------------------------------------------------- */
    if( (IsProjected() || IsGeographic())
        && GetAuthorityCode( "GEOGCS" ) == NULL )
    {
        const int nGCS = GetEPSGGeogCS();
        if( nGCS != -1 )
            SetAuthority( "GEOGCS", "EPSG", nGCS );
    }

    if( IsProjected() && GetAuthorityCode( "PROJCS") == NULL )
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

            if( pszAuthName == NULL || pszAuthCode == NULL )
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
        else if ( pszProjection != NULL &&
                  EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
        {
            const char *pszAuthName = GetAuthorityName( "PROJCS|GEOGCS" );
            const char *pszAuthCode = GetAuthorityCode( "PROJCS|GEOGCS" );
            const double dfLatOrigin = GetNormProjParm(
                                            SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );

            if( pszAuthName != NULL && EQUAL(pszAuthName, "EPSG") &&
                pszAuthCode != NULL && atoi(pszAuthCode) == 4326 &&
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
    if( IsProjected() && GetAuthorityCode("PROJCS") != NULL )
        return OGRERR_NONE;

    if( IsGeographic() && GetAuthorityCode("GEOGCS") != NULL )
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

int OGRSpatialReference::EPSGTreatsAsLatLong()

{
    if( !IsGeographic() )
        return FALSE;

    const char *pszAuth = GetAuthorityName( "GEOGCS" );

    if( pszAuth == NULL || !EQUAL(pszAuth, "EPSG") )
        return FALSE;

    OGR_SRSNode * const poFirstAxis = GetAttrNode( "GEOGCS|AXIS" );

    if( poFirstAxis == NULL )
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

int OGRSpatialReference::EPSGTreatsAsNorthingEasting()

{
    if( !IsProjected() )
        return FALSE;

    const char *pszAuth = GetAuthorityName( "PROJCS" );

    if( pszAuth == NULL || !EQUAL(pszAuth, "EPSG") )
        return FALSE;

    OGR_SRSNode * const poFirstAxis = GetAttrNode( "PROJCS|AXIS" );

    if( poFirstAxis == NULL )
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

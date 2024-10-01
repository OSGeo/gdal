/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Some private helper functions and stuff for OGR implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_P_H_INCLUDED
#define OGR_P_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      Include the common portability library ... lets us do lots      */
/*      of stuff easily.                                                */
/* -------------------------------------------------------------------- */

#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"

#include "ogr_core.h"

#include <limits>

class OGRGeometry;
class OGRFieldDefn;

/* A default name for the default geometry column, instead of '' */
#define OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME "_ogr_geometry_"

#ifdef CPL_MSB
#define OGR_SWAP(x) (x == wkbNDR)
#else
#define OGR_SWAP(x) (x == wkbXDR)
#endif

/* PostGIS 1.X has non standard codes for the following geometry types */
#define POSTGIS15_CURVEPOLYGON 13 /* instead of 10 */
#define POSTGIS15_MULTICURVE 14   /* instead of 11 */
#define POSTGIS15_MULTISURFACE 15 /* instead of 12 */

/* Has been deprecated. Can only be used in very specific circumstances */
#ifdef GDAL_COMPILATION
#define wkb25DBitInternalUse 0x80000000
#endif

/* -------------------------------------------------------------------- */
/*      helper function for parsing well known text format vector objects.*/
/* -------------------------------------------------------------------- */

#ifdef OGR_GEOMETRY_H_INCLUDED
#define OGR_WKT_TOKEN_MAX 64

const char CPL_DLL *OGRWktReadToken(const char *pszInput, char *pszToken);

const char CPL_DLL *OGRWktReadPoints(const char *pszInput,
                                     OGRRawPoint **ppaoPoints, double **ppadfZ,
                                     int *pnMaxPoints, int *pnReadPoints);

const char CPL_DLL *
OGRWktReadPointsM(const char *pszInput, OGRRawPoint **ppaoPoints,
                  double **ppadfZ, double **ppadfM,
                  int *flags, /* geometry flags, are we expecting Z, M, or both;
                                 may change due to input */
                  int *pnMaxPoints, int *pnReadPoints);

void CPL_DLL OGRMakeWktCoordinate(char *, double, double, double, int);
std::string CPL_DLL OGRMakeWktCoordinate(double, double, double, int,
                                         const OGRWktOptions &opts);
void CPL_DLL OGRMakeWktCoordinateM(char *, double, double, double, double,
                                   OGRBoolean, OGRBoolean);
std::string CPL_DLL OGRMakeWktCoordinateM(double, double, double, double,
                                          OGRBoolean, OGRBoolean,
                                          const OGRWktOptions &opts);

#endif

void CPL_DLL OGRFormatDouble(char *pszBuffer, int nBufferLen, double dfVal,
                             char chDecimalSep, int nPrecision = 15,
                             char chConversionSpecifier = 'f');

#ifdef OGR_GEOMETRY_H_INCLUDED
std::string CPL_DLL OGRFormatDouble(double val, const OGRWktOptions &opts,
                                    int nDimIdx);
#endif

int OGRFormatFloat(char *pszBuffer, int nBufferLen, float fVal, int nPrecision,
                   char chConversionSpecifier);

/* -------------------------------------------------------------------- */
/*      Date-time parsing and processing functions                      */
/* -------------------------------------------------------------------- */

/* Internal use by OGR drivers only, CPL_DLL is just there in case */
/* they are compiled as plugins  */

int CPL_DLL OGRTimezoneToTZFlag(const char *pszTZ,
                                bool bEmitErrorIfUnhandledFormat);
std::string CPL_DLL OGRTZFlagToTimezone(int nTZFlag,
                                        const char *pszUTCRepresentation);

int CPL_DLL OGRGetDayOfWeek(int day, int month, int year);
int CPL_DLL OGRParseXMLDateTime(const char *pszXMLDateTime, OGRField *psField);
int CPL_DLL OGRParseRFC822DateTime(const char *pszRFC822DateTime,
                                   OGRField *psField);
char CPL_DLL *OGRGetRFC822DateTime(const OGRField *psField);
char CPL_DLL *OGRGetXMLDateTime(const OGRField *psField);
char CPL_DLL *OGRGetXMLDateTime(const OGRField *psField,
                                bool bAlwaysMillisecond);
// 30 = strlen("YYYY-MM-DDThh:mm:ss.sss+hh:mm") + 1
#define OGR_SIZEOF_ISO8601_DATETIME_BUFFER 30
int CPL_DLL
OGRGetISO8601DateTime(const OGRField *psField, bool bAlwaysMillisecond,
                      char szBuffer[OGR_SIZEOF_ISO8601_DATETIME_BUFFER]);

/** Precision of formatting */
enum class OGRISO8601Precision
{
    /** Automated mode: millisecond included if non zero, otherwise truncated at second */
    AUTO,
    /** Always include millisecond */
    MILLISECOND,
    /** Always include second, but no millisecond */
    SECOND,
    /** Always include minute, but no second */
    MINUTE
};

/** Configuration of the ISO8601 formatting output */
struct OGRISO8601Format
{
    /** Precision of formatting */
    OGRISO8601Precision ePrecision;
};

int CPL_DLL
OGRGetISO8601DateTime(const OGRField *psField, const OGRISO8601Format &sFormat,
                      char szBuffer[OGR_SIZEOF_ISO8601_DATETIME_BUFFER]);
char CPL_DLL *OGRGetXML_UTF8_EscapedString(const char *pszString);
bool CPL_DLL OGRParseDateTimeYYYYMMDDTHHMMZ(const char *pszInput, size_t nLen,
                                            OGRField *psField);
bool CPL_DLL OGRParseDateTimeYYYYMMDDTHHMMSSZ(const char *pszInput, size_t nLen,
                                              OGRField *psField);
bool CPL_DLL OGRParseDateTimeYYYYMMDDTHHMMSSsssZ(const char *pszInput,
                                                 size_t nLen,
                                                 OGRField *psField);

int OGRCompareDate(const OGRField *psFirstTuple,
                   const OGRField *psSecondTuple); /* used by ogr_gensql.cpp and
                                                      ogrfeaturequery.cpp */

/* General utility option processing. */
int CPL_DLL OGRGeneralCmdLineProcessor(int nArgc, char ***ppapszArgv,
                                       int nOptions);

/************************************************************************/
/*     Support for special attributes (feature query and selection)     */
/************************************************************************/
#define SPF_FID 0
#define SPF_OGR_GEOMETRY 1
#define SPF_OGR_STYLE 2
#define SPF_OGR_GEOM_WKT 3
#define SPF_OGR_GEOM_AREA 4
#define SPECIAL_FIELD_COUNT 5

extern const char *const SpecialFieldNames[SPECIAL_FIELD_COUNT];

/************************************************************************/
/*     Some SRS related stuff, search in SRS data files.                */
/************************************************************************/

OGRErr CPL_DLL OSRGetEllipsoidInfo(int, char **, double *, double *);

/* Fast atof function */
double OGRFastAtof(const char *pszStr);

OGRErr CPL_DLL OGRCheckPermutation(const int *panPermutation, int nSize);

/* GML related */

OGRGeometry CPL_DLL *GML2OGRGeometry_XMLNode(
    const CPLXMLNode *psNode, int nPseudoBoolGetSecondaryGeometryOption,
    int nRecLevel = 0, int nSRSDimension = 0, bool bIgnoreGSG = false,
    bool bOrientation = true, bool bFaceHoleNegative = false);

/************************************************************************/
/*                        PostGIS EWKB encoding                         */
/************************************************************************/

OGRGeometry CPL_DLL *OGRGeometryFromEWKB(GByte *pabyWKB, int nLength,
                                         int *pnSRID, int bIsPostGIS1_EWKB);
OGRGeometry CPL_DLL *OGRGeometryFromHexEWKB(const char *pszBytea, int *pnSRID,
                                            int bIsPostGIS1_EWKB);
char CPL_DLL *OGRGeometryToHexEWKB(OGRGeometry *poGeometry, int nSRSId,
                                   int nPostGISMajor, int nPostGISMinor);

/************************************************************************/
/*                        WKB Type Handling encoding                    */
/************************************************************************/

OGRErr CPL_DLL OGRReadWKBGeometryType(const unsigned char *pabyData,
                                      OGRwkbVariant wkbVariant,
                                      OGRwkbGeometryType *eGeometryType);

/************************************************************************/
/*                        WKT Type Handling encoding                    */
/************************************************************************/

OGRErr CPL_DLL OGRReadWKTGeometryType(const char *pszWKT,
                                      OGRwkbGeometryType *peGeometryType);

/************************************************************************/
/*                            Other                                     */
/************************************************************************/

void CPL_DLL OGRUpdateFieldType(OGRFieldDefn *poFDefn, OGRFieldType eNewType,
                                OGRFieldSubType eNewSubType);

/************************************************************************/
/*                         OGRRoundValueIEEE754()                       */
/************************************************************************/

/** Set to zero least significants bits of a double precision floating-point
 * number (passed as an integer), taking into account a desired bit precision.
 *
 * @param nVal Integer representation of a IEEE754 double-precision number.
 * @param nBitsPrecision Desired precision (number of bits after integral part)
 * @return quantized nVal.
 * @since GDAL 3.9
 */
inline uint64_t OGRRoundValueIEEE754(uint64_t nVal,
                                     int nBitsPrecision) CPL_WARN_UNUSED_RESULT;

inline uint64_t OGRRoundValueIEEE754(uint64_t nVal, int nBitsPrecision)
{
    constexpr int MANTISSA_SIZE = std::numeric_limits<double>::digits - 1;
    constexpr int MAX_EXPONENT = std::numeric_limits<double>::max_exponent;
#if __cplusplus >= 201703L
    static_assert(MANTISSA_SIZE == 52);
    static_assert(MAX_EXPONENT == 1024);
#endif
    // Extract the binary exponent from the IEEE754 representation
    const int nExponent =
        ((nVal >> MANTISSA_SIZE) & (2 * MAX_EXPONENT - 1)) - (MAX_EXPONENT - 1);
    // Add 1 to round-up and the desired precision
    const int nBitsRequired = 1 + nExponent + nBitsPrecision;
    // Compute number of nullified bits
    int nNullifiedBits = MANTISSA_SIZE - nBitsRequired;
    // this will also capture NaN and Inf since nExponent = 1023,
    // and thus nNullifiedBits < 0
    if (nNullifiedBits <= 0)
        return nVal;
    if (nNullifiedBits >= MANTISSA_SIZE)
        nNullifiedBits = MANTISSA_SIZE;
    nVal >>= nNullifiedBits;
    nVal <<= nNullifiedBits;
    return nVal;
}

/************************************************************************/
/*                   OGRRoundCoordinatesIEEE754XYValues()               */
/************************************************************************/

/** Quantize XY values.
 *
 * @since GDAL 3.9
 */
template <int SPACING>
inline void OGRRoundCoordinatesIEEE754XYValues(int nBitsPrecision,
                                               GByte *pabyBase, size_t nPoints)
{
    // Note: we use SPACING as template for improved code generation.

    if (nBitsPrecision != INT_MIN)
    {
        for (size_t i = 0; i < nPoints; i++)
        {
            uint64_t nVal;

            memcpy(&nVal, pabyBase + SPACING * i, sizeof(uint64_t));
            nVal = OGRRoundValueIEEE754(nVal, nBitsPrecision);
            memcpy(pabyBase + SPACING * i, &nVal, sizeof(uint64_t));

            memcpy(&nVal, pabyBase + sizeof(uint64_t) + SPACING * i,
                   sizeof(uint64_t));
            nVal = OGRRoundValueIEEE754(nVal, nBitsPrecision);
            memcpy(pabyBase + sizeof(uint64_t) + SPACING * i, &nVal,
                   sizeof(uint64_t));
        }
    }
}

/************************************************************************/
/*                     OGRRoundCoordinatesIEEE754()                     */
/************************************************************************/

/** Quantize Z or M values.
 *
 * @since GDAL 3.9
 */
template <int SPACING>
inline void OGRRoundCoordinatesIEEE754(int nBitsPrecision, GByte *pabyBase,
                                       size_t nPoints)
{
    if (nBitsPrecision != INT_MIN)
    {
        for (size_t i = 0; i < nPoints; i++)
        {
            uint64_t nVal;

            memcpy(&nVal, pabyBase + SPACING * i, sizeof(uint64_t));
            nVal = OGRRoundValueIEEE754(nVal, nBitsPrecision);
            memcpy(pabyBase + SPACING * i, &nVal, sizeof(uint64_t));
        }
    }
}

#endif /* ndef OGR_P_H_INCLUDED */

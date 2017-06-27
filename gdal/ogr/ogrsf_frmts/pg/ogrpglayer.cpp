/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGLayer class  which implements shared handling
 *           of feature geometry and so forth needed by OGRPGResultLayer and
 *           OGRPGTableLayer.
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

/* Some functions have been extracted from PostgreSQL code base  */
/* The applicable copyright & licence notice is the following one : */
/*
PostgreSQL Database Management System
(formerly known as Postgres, then as Postgres95)

Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group

Portions Copyright (c) 1994, The Regents of the University of California

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement
is hereby granted, provided that the above copyright notice and this
paragraph and the following two paragraphs appear in all copies.

IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS TO
PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/

#include "ogr_pg.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#define PQexec this_is_an_error

CPL_CVSID("$Id$")

// These originally are defined in libpq-fs.h.

#ifndef INV_WRITE
#define INV_WRITE               0x00020000
#define INV_READ                0x00040000
#endif

/************************************************************************/
/*                           OGRPGLayer()                               */
/************************************************************************/

OGRPGLayer::OGRPGLayer() :
    poFeatureDefn(NULL),
    nCursorPage(atoi(CPLGetConfigOption("OGR_PG_CURSOR_PAGE", "500"))),
    iNextShapeId(0),
    poDS(NULL),
    pszQueryStatement(NULL),
    pszCursorName(NULL),
    hCursorResult(NULL),
    bInvalidated(FALSE),
    nResultOffset(0),
    bWkbAsOid(FALSE),
    pszFIDColumn(NULL),
    bCanUseBinaryCursor(TRUE),
    m_panMapFieldNameToIndex(NULL),
    m_panMapFieldNameToGeomIndex(NULL)
{
    pszCursorName = CPLStrdup(CPLSPrintf("OGRPGLayerReader%p", this));
}

/************************************************************************/
/*                            ~OGRPGLayer()                             */
/************************************************************************/

OGRPGLayer::~OGRPGLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "PG", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
                  poFeatureDefn->GetName() );
    }

    ResetReading();

    CPLFree( pszFIDColumn );
    CPLFree( pszQueryStatement );
    CPLFree( m_panMapFieldNameToIndex );
    CPLFree( m_panMapFieldNameToGeomIndex );
    CPLFree( pszCursorName );

    if( poFeatureDefn )
    {
        poFeatureDefn->UnsetLayer();
        poFeatureDefn->Release();
    }

    CloseCursor();
}

/************************************************************************/
/*                            CloseCursor()                             */
/************************************************************************/

void OGRPGLayer::CloseCursor()
{
    PGconn      *hPGConn = poDS->GetPGConn();

    if( hCursorResult != NULL )
    {
        OGRPGClearResult( hCursorResult );

        CPLString    osCommand;
        osCommand.Printf("CLOSE %s", pszCursorName );

        /* In case of interleaving read in different layers we might have */
        /* close the transaction, and thus implicitly the cursor, so be */
        /* quiet about errors. This is potentially an issue by the way */
        hCursorResult = OGRPG_PQexec(hPGConn, osCommand.c_str(), FALSE, TRUE);
        OGRPGClearResult( hCursorResult );

        poDS->SoftCommitTransaction();

        hCursorResult = NULL;
    }
}

/************************************************************************/
/*                       InvalidateCursor()                             */
/************************************************************************/

void OGRPGLayer::InvalidateCursor()
{
    CloseCursor();
    bInvalidated = TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGLayer::ResetReading()

{
    GetLayerDefn();

    iNextShapeId = 0;

    CloseCursor();
    bInvalidated = FALSE;
}

#if defined(BINARY_CURSOR_ENABLED)
/************************************************************************/
/*                    OGRPGGetStrFromBinaryNumeric()                    */
/************************************************************************/

/* Adaptation of get_str_from_var() from pgsql/src/backend/utils/adt/numeric.c */

typedef short NumericDigit;

typedef struct NumericVar
{
        int ndigits;           /* # of digits in digits[] - can be 0! */
        int weight;            /* weight of first digit */
        int sign;              /* NUMERIC_POS, NUMERIC_NEG, or NUMERIC_NAN */
        int dscale;            /* display scale */
        NumericDigit *digits;  /* base-NBASE digits */
} NumericVar;

#define NUMERIC_POS 0x0000
#define NUMERIC_NEG 0x4000
#define NUMERIC_NAN 0xC000

#define DEC_DIGITS 4
/*
* get_str_from_var() -
*
*       Convert a var to text representation (guts of numeric_out).
*       CAUTION: var's contents may be modified by rounding!
*       Returns a malloc'd string.
*/
static char *
OGRPGGetStrFromBinaryNumeric(NumericVar *var)
{
        char   *str;
        char   *cp;
        char   *endcp;
        int     i;
        int     d;
        NumericDigit dig;
        NumericDigit d1;

        int dscale = var->dscale;

        /*
        * Allocate space for the result.
        *
        * i is set to to # of decimal digits before decimal point. dscale is the
        * # of decimal digits we will print after decimal point. We may generate
        * as many as DEC_DIGITS-1 excess digits at the end, and in addition we
        * need room for sign, decimal point, null terminator.
        */
        i = (var->weight + 1) * DEC_DIGITS;
        if (i <= 0)
                i = 1;

        str = (char*)CPLMalloc(i + dscale + DEC_DIGITS + 2);
        cp = str;

        /*
        * Output a dash for negative values
        */
        if (var->sign == NUMERIC_NEG)
                *cp++ = '-';

        /*
        * Output all digits before the decimal point
        */
        if (var->weight < 0)
        {
                d = var->weight + 1;
                *cp++ = '0';
        }
        else
        {
                for (d = 0; d <= var->weight; d++)
                {
                        dig = (d < var->ndigits) ? var->digits[d] : 0;
                        CPL_MSBPTR16(&dig);
                        /* In the first digit, suppress extra leading
                           decimal zeroes */
                        {
                                bool putit = (d > 0);

                                d1 = dig / 1000;
                                dig -= d1 * 1000;
                                putit |= (d1 > 0);
                                if (putit)
                                        *cp++ = (char)(d1 + '0');
                                d1 = dig / 100;
                                dig -= d1 * 100;
                                putit |= (d1 > 0);
                                if (putit)
                                        *cp++ = (char)(d1 + '0');
                                d1 = dig / 10;
                                dig -= d1 * 10;
                                putit |= (d1 > 0);
                                if (putit)
                                        *cp++ = (char)(d1 + '0');
                                *cp++ = (char)(dig + '0');
                        }
                }
        }

        /*
        * If requested, output a decimal point and all the digits that follow it.
        * We initially put out a multiple of DEC_DIGITS digits, then truncate if
        * needed.
        */
        if (dscale > 0)
        {
                *cp++ = '.';
                endcp = cp + dscale;
                for (i = 0; i < dscale; d++, i += DEC_DIGITS)
                {
                        dig = (d >= 0 && d < var->ndigits) ? var->digits[d] : 0;
                        CPL_MSBPTR16(&dig);
                        d1 = dig / 1000;
                        dig -= d1 * 1000;
                        *cp++ = (char)(d1 + '0');
                        d1 = dig / 100;
                        dig -= d1 * 100;
                        *cp++ = (char)(d1 + '0');
                        d1 = dig / 10;
                        dig -= d1 * 10;
                        *cp++ = (char)(d1 + '0');
                        *cp++ = (char)(dig + '0');
                }
                cp = endcp;
        }

        /*
        * terminate the string and return it
        */
        *cp = '\0';
        return str;
}

/************************************************************************/
/*                         OGRPGj2date()                            */
/************************************************************************/

/* Coming from j2date() in pgsql/src/backend/utils/adt/datetime.c */

#define POSTGRES_EPOCH_JDATE 2451545 /* == date2j(2000, 1, 1) */

static
void OGRPGj2date(int jd, int *year, int *month, int *day)
{
    unsigned int julian;
    unsigned int quad;
    unsigned int extra;
    int y;

    julian = jd;
    julian += 32044;
    quad = julian / 146097;
    extra = (julian - quad * 146097) * 4 + 3;
    julian += 60 + quad * 3 + extra / 146097;
    quad = julian / 1461;
    julian -= quad * 1461;
    y = julian * 4 / 1461;
    julian = ((y != 0) ? ((julian + 305) % 365) : ((julian + 306) % 366))
        + 123;
    y += quad * 4;
    *year = y - 4800;
    quad = julian * 2141 / 65536;
    *day = julian - 7834 * quad / 256;
    *month = (quad + 10) % 12 + 1;

    return;
}  /* j2date() */

/************************************************************************/
/*                              OGRPGdt2time()                          */
/************************************************************************/

#define USECS_PER_SEC 1000000
#define USECS_PER_MIN   ((GIntBig) 60 * USECS_PER_SEC)
#define USECS_PER_HOUR  ((GIntBig) 3600 * USECS_PER_SEC)
#define USECS_PER_DAY   ((GIntBig) 3600 * 24 * USECS_PER_SEC)

/* Coming from dt2time() in pgsql/src/backend/utils/adt/timestamp.c */

static
void
OGRPGdt2timeInt8(GIntBig jd, int *hour, int *min, int *sec, double *fsec)
{
    GIntBig time;

    time = jd;

    *hour = (int) (time / USECS_PER_HOUR);
    time -= (GIntBig) (*hour) * USECS_PER_HOUR;
    *min = (int) (time / USECS_PER_MIN);
    time -=  (GIntBig) (*min) * USECS_PER_MIN;
    *sec = (int)time / USECS_PER_SEC;
    *fsec = (double)(time - *sec * USECS_PER_SEC);
}  /* dt2time() */

static
void
OGRPGdt2timeFloat8(double jd, int *hour, int *min, int *sec, double *fsec)
{
    double time;

    time = jd;

    *hour = (int) (time / 3600.);
    time -= (*hour) * 3600.;
    *min = (int) (time / 60.);
    time -=  (*min) * 60.;
    *sec = (int)time;
    *fsec = time - *sec;
}

/************************************************************************/
/*                        OGRPGTimeStamp2DMYHMS()                       */
/************************************************************************/

#define TMODULO(t,q,u) \
do { \
        (q) = ((t) / (u)); \
        if ((q) != 0) (t) -= ((q) * (u)); \
} while( false )

/* Coming from timestamp2tm() in pgsql/src/backend/utils/adt/timestamp.c */

static
int OGRPGTimeStamp2DMYHMS(GIntBig dt, int *year, int *month, int *day,
                                      int* hour, int* min, double* pdfSec)
{
    GIntBig date;
    GIntBig time;
    int nSec;
    double dfSec;

    time = dt;
    TMODULO(time, date, USECS_PER_DAY);

    if (time < 0)
    {
        time += USECS_PER_DAY;
        date -= 1;
    }

    /* add offset to go from J2000 back to standard Julian date */
    date += POSTGRES_EPOCH_JDATE;

    /* Julian day routine does not work for negative Julian days */
    if (date < 0 || date > (double) INT_MAX)
        return -1;

    OGRPGj2date((int) date, year, month, day);
    OGRPGdt2timeInt8(time, hour, min, &nSec, &dfSec);
    *pdfSec += nSec + dfSec;

    return 0;
}

#endif // defined(BINARY_CURSOR_ENABLED)

/************************************************************************/
/*                   TokenizeStringListFromText()                       */
/*                                                                      */
/* Tokenize a varchar[] returned as a text                              */
/************************************************************************/

static void OGRPGTokenizeStringListUnescapeToken(char* pszToken)
{
    if (EQUAL(pszToken, "NULL"))
    {
        pszToken[0] = '\0';
        return;
    }

    int iSrc = 0, iDst = 0;
    for(iSrc = 0; pszToken[iSrc] != '\0'; iSrc++)
    {
        pszToken[iDst] = pszToken[iSrc];
        if (pszToken[iSrc] != '\\')
            iDst ++;
    }
    pszToken[iDst] = '\0';
}

/* {"a\",b",d,NULL,e}  should be tokenized into 3 pieces :  a",b     d    empty_string    e */
static char ** OGRPGTokenizeStringListFromText(const char* pszText)
{
    char** papszTokens = NULL;
    const char* pszCur = strchr(pszText, '{');
    if (pszCur == NULL)
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Incorrect string list : %s", pszText);
        return papszTokens;
    }

    const char* pszNewTokenStart = NULL;
    int bInDoubleQuotes = FALSE;
    pszCur ++;
    while(*pszCur)
    {
        if (*pszCur == '\\')
        {
            pszCur ++;
            if (*pszCur == 0)
                break;
            pszCur ++;
            continue;
        }

        if (*pszCur == '"')
        {
            bInDoubleQuotes = !bInDoubleQuotes;
            if (bInDoubleQuotes)
                pszNewTokenStart = pszCur + 1;
            else
            {
                if (pszCur[1] == ',' || pszCur[1] == '}')
                {
                    if (pszNewTokenStart != NULL && pszCur > pszNewTokenStart)
                    {
                        char* pszNewToken = (char*) CPLMalloc(pszCur - pszNewTokenStart + 1);
                        memcpy(pszNewToken, pszNewTokenStart, pszCur - pszNewTokenStart);
                        pszNewToken[pszCur - pszNewTokenStart] = 0;
                        OGRPGTokenizeStringListUnescapeToken(pszNewToken);
                        papszTokens = CSLAddString(papszTokens, pszNewToken);
                        CPLFree(pszNewToken);
                    }
                    pszNewTokenStart = NULL;
                    if (pszCur[1] == ',')
                        pszCur ++;
                    else
                        return papszTokens;
                }
                else
                {
                    /* error */
                    break;
                }
            }
        }
        if (!bInDoubleQuotes)
        {
            if (*pszCur == '{')
            {
                /* error */
                break;
            }
            else if (*pszCur == '}')
            {
                if (pszNewTokenStart != NULL && pszCur > pszNewTokenStart)
                {
                    char* pszNewToken = (char*) CPLMalloc(pszCur - pszNewTokenStart + 1);
                    memcpy(pszNewToken, pszNewTokenStart, pszCur - pszNewTokenStart);
                    pszNewToken[pszCur - pszNewTokenStart] = 0;
                    OGRPGTokenizeStringListUnescapeToken(pszNewToken);
                    papszTokens = CSLAddString(papszTokens, pszNewToken);
                    CPLFree(pszNewToken);
                }
                return papszTokens;
            }
            else if (*pszCur == ',')
            {
                if (pszNewTokenStart != NULL && pszCur > pszNewTokenStart)
                {
                    char* pszNewToken = (char*) CPLMalloc(pszCur - pszNewTokenStart + 1);
                    memcpy(pszNewToken, pszNewTokenStart, pszCur - pszNewTokenStart);
                    pszNewToken[pszCur - pszNewTokenStart] = 0;
                    OGRPGTokenizeStringListUnescapeToken(pszNewToken);
                    papszTokens = CSLAddString(papszTokens, pszNewToken);
                    CPLFree(pszNewToken);
                }
                pszNewTokenStart = pszCur + 1;
            }
            else if (pszNewTokenStart == NULL)
                pszNewTokenStart = pszCur;
        }
        pszCur++;
    }

    CPLError(CE_Warning, CPLE_AppDefined, "Incorrect string list : %s", pszText);
    return papszTokens;
}

/************************************************************************/
/*                          RecordToFeature()                           */
/*                                                                      */
/*      Convert the indicated record of the current result set into     */
/*      a feature.                                                      */
/************************************************************************/

OGRFeature *OGRPGLayer::RecordToFeature( PGresult* hResult,
                                         const int* panMapFieldNameToIndex,
                                         const int* panMapFieldNameToGeomIndex,
                                         int iRecord )

{
/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetFID( iNextShapeId );
    m_nFeaturesRead++;

/* ==================================================================== */
/*      Transfer all result fields we can.                              */
/* ==================================================================== */
    for( iField = 0;
         iField < PQnfields(hResult);
         iField++ )
    {
        int     iOGRField;

#if defined(BINARY_CURSOR_ENABLED)
        int nTypeOID = PQftype(hResult, iField);
#endif
        const char* pszFieldName = PQfname(hResult,iField);

/* -------------------------------------------------------------------- */
/*      Handle FID.                                                     */
/* -------------------------------------------------------------------- */
        if( pszFIDColumn != NULL && EQUAL(pszFieldName,pszFIDColumn) )
        {
#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1 ) // Binary data representation
            {
                if ( nTypeOID == INT4OID)
                {
                    int nVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(int));
                    memcpy( &nVal, PQgetvalue( hResult, iRecord, iField ), sizeof(int) );
                    CPL_MSBPTR32(&nVal);
                    poFeature->SetFID( nVal );
                }
                else if ( nTypeOID == INT8OID)
                {
                    GIntBig nVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(GIntBig));
                    memcpy( &nVal, PQgetvalue( hResult, iRecord, iField ), sizeof(GIntBig) );
                    CPL_MSBPTR64(&nVal);
                    poFeature->SetFID( nVal );
                }
                else
                {
                    CPLDebug("PG", "FID. Unhandled OID %d.", nTypeOID );
                    continue;
                }
            }
            else
#endif /* defined(BINARY_CURSOR_ENABLED) */
            {
                char* pabyData = PQgetvalue(hResult,iRecord,iField);
                /* ogr_pg_20 may crash if PostGIS is unavailable and we don't test pabyData */
                if (pabyData)
                    poFeature->SetFID( CPLAtoGIntBig(pabyData) );
                else
                    continue;
            }
        }

/* -------------------------------------------------------------------- */
/*      Handle PostGIS geometry                                         */
/* -------------------------------------------------------------------- */
        int iOGRGeomField = panMapFieldNameToGeomIndex[iField];
        OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
        if( iOGRGeomField >= 0 )
            poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(iOGRGeomField);
        if( iOGRGeomField >= 0 && (
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY) )
        {
            if ( STARTS_WITH_CI(pszFieldName, "ST_AsBinary") ||
                      STARTS_WITH_CI(pszFieldName, "AsBinary") )
            {
                GByte* pabyVal = (GByte*) PQgetvalue( hResult,
                                             iRecord, iField);
                const char* pszVal = (const char*) pabyVal;

                int nLength = PQgetlength(hResult, iRecord, iField);

                /* No geometry */
                if (nLength == 0)
                    continue;

                OGRGeometry * poGeom = NULL;
                if( !poDS->bUseBinaryCursor && nLength >= 4 &&
                    /* escaped byea data */
                    (STARTS_WITH(pszVal, "\\000") || STARTS_WITH(pszVal, "\\001") ||
                    /* hex bytea data (PostgreSQL >= 9.0) */
                     STARTS_WITH(pszVal, "\\x00") || STARTS_WITH(pszVal, "\\x01")) )
                {
                    poGeom = BYTEAToGeometry(pszVal, (poDS->sPostGISVersion.nMajor < 2));
                }
                else
                    OGRGeometryFactory::createFromWkb( pabyVal, NULL, &poGeom, nLength,
                                                       (poDS->sPostGISVersion.nMajor < 2) ? wkbVariantPostGIS1 : wkbVariantOldOgc );

                if( poGeom != NULL )
                {
                    poGeom->assignSpatialReference( poGeomFieldDefn->GetSpatialRef() );
                    poFeature->SetGeomFieldDirectly(iOGRGeomField,  poGeom );
                }

                continue;
            }
            else if ( !poDS->bUseBinaryCursor &&
                      STARTS_WITH_CI(pszFieldName, "EWKBBase64") )
            {
                GByte* pabyData = (GByte*)PQgetvalue( hResult,
                                                        iRecord, iField);

                int nLength = PQgetlength(hResult, iRecord, iField);

                /* No geometry */
                if (nLength == 0)
                    continue;

                nLength = CPLBase64DecodeInPlace(pabyData);
                OGRGeometry * poGeom = OGRGeometryFromEWKB(pabyData, nLength, NULL,
                                                           poDS->sPostGISVersion.nMajor < 2);

                if( poGeom != NULL )
                {
                    poGeom->assignSpatialReference( poGeomFieldDefn->GetSpatialRef() );
                    poFeature->SetGeomFieldDirectly(iOGRGeomField,  poGeom );
                }

                continue;
            }
            else if ( poDS->bUseBinaryCursor ||
                      EQUAL(pszFieldName,"ST_AsEWKB") ||
                      EQUAL(pszFieldName,"AsEWKB") )
            {
                /* Handle HEX result or EWKB binary cursor result */
                char * pabyData = PQgetvalue( hResult,
                                                        iRecord, iField);

                int nLength = PQgetlength(hResult, iRecord, iField);

                /* No geometry */
                if (nLength == 0)
                    continue;

                OGRGeometry * poGeom = NULL;

                if( !poDS->bUseBinaryCursor &&
                    (STARTS_WITH(pabyData, "\\x00") || STARTS_WITH(pabyData, "\\x01") ||
                     STARTS_WITH(pabyData, "\\000") || STARTS_WITH(pabyData, "\\001")) )
                {
                    GByte* pabyEWKB = BYTEAToGByteArray(pabyData, &nLength);
                    poGeom = OGRGeometryFromEWKB(pabyEWKB, nLength, NULL,
                                                 poDS->sPostGISVersion.nMajor < 2);
                    CPLFree(pabyEWKB);
                }
                else if( nLength >= 2 && (STARTS_WITH_CI(pabyData, "00") || STARTS_WITH_CI(pabyData, "01")) )
                {
                    poGeom = OGRGeometryFromHexEWKB(pabyData, NULL,
                                                    poDS->sPostGISVersion.nMajor < 2);
                }
                else
                {
                    poGeom = OGRGeometryFromEWKB((GByte*)pabyData, nLength, NULL,
                                                 poDS->sPostGISVersion.nMajor < 2);
                }

                if( poGeom != NULL )
                {
                    poGeom->assignSpatialReference( poGeomFieldDefn->GetSpatialRef() );
                    poFeature->SetGeomFieldDirectly(iOGRGeomField,  poGeom );
                }

                continue;
            }
            else /*if (EQUAL(pszFieldName,"asEWKT") ||
                     EQUAL(pszFieldName,"asText") ||
                     EQUAL(pszFieldName,"ST_AsEWKT") ||
                     EQUAL(pszFieldName,"ST_AsText") )*/
            {
                /* Handle WKT */
                char *pszWKT = PQgetvalue( hResult, iRecord, iField );
                char *pszPostSRID = pszWKT;

                // optionally strip off PostGIS SRID identifier.  This
                // happens if we got a raw geometry field.
                if( STARTS_WITH_CI(pszPostSRID, "SRID=") )
                {
                    while( *pszPostSRID != '\0' && *pszPostSRID != ';' )
                        pszPostSRID++;
                    if( *pszPostSRID == ';' )
                        pszPostSRID++;
                }

                OGRGeometry *poGeometry = NULL;
                if( STARTS_WITH_CI(pszPostSRID, "00") || STARTS_WITH_CI(pszPostSRID, "01") )
                {
                    poGeometry = OGRGeometryFromHexEWKB( pszWKT, NULL,
                                                         poDS->sPostGISVersion.nMajor < 2 );
                }
                else
                    OGRGeometryFactory::createFromWkt( &pszPostSRID, NULL,
                                                    &poGeometry );
                if( poGeometry != NULL )
                {
                    poGeometry->assignSpatialReference( poGeomFieldDefn->GetSpatialRef() );
                    poFeature->SetGeomFieldDirectly(iOGRGeomField,  poGeometry );
                }

                continue;
            }
        }
/* -------------------------------------------------------------------- */
/*      Handle raw binary geometry ... this hasn't been tested in a     */
/*      while.                                                          */
/* -------------------------------------------------------------------- */
        else if( iOGRGeomField >= 0 &&
                 poGeomFieldDefn->ePostgisType == GEOM_TYPE_WKB )
        {
            OGRGeometry *poGeometry = NULL;
            GByte* pabyData = (GByte*) PQgetvalue( hResult, iRecord, iField);

            if( bWkbAsOid )
            {
                poGeometry =
                    OIDToGeometry( (Oid) atoi((const char*)pabyData) );
            }
            else
            {
#if defined(BINARY_CURSOR_ENABLED)
                if (poDS->bUseBinaryCursor
                    && PQfformat( hResult, iField ) == 1
                   )
                {
                    int nLength = PQgetlength(hResult, iRecord, iField);
                    poGeometry = OGRGeometryFromEWKB(pabyData, nLength, NULL,
                                                     poDS->sPostGISVersion.nMajor < 2 );
                }
#endif
                if (poGeometry == NULL)
                {
                    poGeometry = BYTEAToGeometry( (const char*)pabyData,
                                                  (poDS->sPostGISVersion.nMajor < 2) );
                }
            }

            if( poGeometry != NULL )
            {
                poGeometry->assignSpatialReference( poGeomFieldDefn->GetSpatialRef() );
                poFeature->SetGeomFieldDirectly(iOGRGeomField,  poGeometry );
            }

            continue;
        }

/* -------------------------------------------------------------------- */
/*      Transfer regular data fields.                                   */
/* -------------------------------------------------------------------- */
        iOGRField = panMapFieldNameToIndex[iField];

        if( iOGRField < 0 )
            continue;

        if( PQgetisnull( hResult, iRecord, iField ) )
        {
            poFeature->SetFieldNull( iOGRField );
            continue;
        }

        OGRFieldType eOGRType =
            poFeatureDefn->GetFieldDefn(iOGRField)->GetType();

        if( eOGRType == OFTIntegerList)
        {
            int *panList, nCount, i;

#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1 ) // Binary data representation
            {
                if (nTypeOID == INT2ARRAYOID || nTypeOID == INT4ARRAYOID)
                {
                    char * pData = PQgetvalue( hResult, iRecord, iField );

                    // goto number of array elements
                    pData += 3 * sizeof(int);
                    memcpy( &nCount, pData, sizeof(int) );
                    CPL_MSBPTR32( &nCount );

                    panList = (int *) CPLCalloc(sizeof(int),nCount);

                    // goto first array element
                    pData += 2 * sizeof(int);

                    for( i = 0; i < nCount; i++ )
                    {
                        // get element size
                        int nSize = *(int *)(pData);
                        CPL_MSBPTR32( &nSize );
                        pData += sizeof(int);

                        if (nTypeOID == INT4ARRAYOID  )
                        {
                            CPLAssert( nSize == sizeof(int) );
                            memcpy( &panList[i], pData, nSize );
                            CPL_MSBPTR32(&panList[i]);
                        }
                        else
                        {
                            CPLAssert( nSize == sizeof(GInt16) );
                            GInt16 nVal;
                            memcpy( &nVal, pData, nSize );
                            CPL_MSBPTR16(&nVal);
                            panList[i] = nVal;
                        }

                        pData += nSize;
                    }
                }
                else
                {
                    CPLDebug("PG", "Field %d: Incompatible OID (%d) with OFTIntegerList.", iOGRField, nTypeOID );
                    continue;
                }
            }
            else
#endif
            {
                char **papszTokens = CSLTokenizeStringComplex(
                    PQgetvalue( hResult, iRecord, iField ),
                    "{,}", FALSE, FALSE );

                nCount = CSLCount(papszTokens);
                panList = (int *) CPLCalloc(sizeof(int),nCount);

                if( poFeatureDefn->GetFieldDefn(iOGRField)->GetSubType() == OFSTBoolean )
                {
                    for( i = 0; i < nCount; i++ )
                        panList[i] = EQUAL(papszTokens[i], "t");
                }
                else
                {
                    for( i = 0; i < nCount; i++ )
                        panList[i] = atoi(papszTokens[i]);
                }
                CSLDestroy( papszTokens );
            }
            poFeature->SetField( iOGRField, nCount, panList );
            CPLFree( panList );
        }

        else if( eOGRType == OFTInteger64List)
        {
            int nCount = 0;
            GIntBig *panList = NULL;

#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1 ) // Binary data representation
            {
                if (nTypeOID == INT8ARRAYOID)
                {
                    char * pData = PQgetvalue( hResult, iRecord, iField );

                    // goto number of array elements
                    pData += 3 * sizeof(int);
                    memcpy( &nCount, pData, sizeof(int) );
                    CPL_MSBPTR32( &nCount );

                    panList = (GIntBig *) CPLCalloc(sizeof(GIntBig),nCount);

                    // goto first array element
                    pData += 2 * sizeof(int);

                    for( int i = 0; i < nCount; i++ )
                    {
                        // get element size
                        int nSize = *(int *)(pData);
                        CPL_MSBPTR32( &nSize );

                        CPLAssert( nSize == sizeof(GIntBig) );

                        pData += sizeof(int);

                        memcpy( &panList[i], pData, nSize );
                        CPL_MSBPTR64(&panList[i]);

                        pData += nSize;
                    }
                }
                else
                {
                    CPLDebug("PG", "Field %d: Incompatible OID (%d) with OFTInteger64List.", iOGRField, nTypeOID );
                    continue;
                }
            }
            else
#endif
            {
                char **papszTokens = CSLTokenizeStringComplex(
                    PQgetvalue( hResult, iRecord, iField ),
                    "{,}", FALSE, FALSE );

                nCount = CSLCount(papszTokens);
                panList = (GIntBig *) CPLCalloc(sizeof(GIntBig),nCount);

                if( poFeatureDefn->GetFieldDefn(iOGRField)->GetSubType() == OFSTBoolean )
                {
                    for( int i = 0; i < nCount; i++ )
                        panList[i] = EQUAL(papszTokens[i], "t");
                }
                else
                {
                    for( int i = 0; i < nCount; i++ )
                        panList[i] = CPLAtoGIntBig(papszTokens[i]);
                }
                CSLDestroy( papszTokens );
            }
            poFeature->SetField( iOGRField, nCount, panList );
            CPLFree( panList );
        }

        else if( eOGRType == OFTRealList )
        {
            int nCount, i;
            double *padfList = NULL;

#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1 ) // Binary data representation
            {
                if (nTypeOID == FLOAT8ARRAYOID || nTypeOID == FLOAT4ARRAYOID)
                {
                    char * pData = PQgetvalue( hResult, iRecord, iField );

                    // goto number of array elements
                    pData += 3 * sizeof(int);
                    memcpy( &nCount, pData, sizeof(int) );
                    CPL_MSBPTR32( &nCount );

                    padfList = (double *) CPLCalloc(sizeof(double),nCount);

                    // goto first array element
                    pData += 2 * sizeof(int);

                    for( i = 0; i < nCount; i++ )
                    {
                        // get element size
                        int nSize = *(int *)(pData);
                        CPL_MSBPTR32( &nSize );

                        pData += sizeof(int);

                        if (nTypeOID == FLOAT8ARRAYOID)
                        {
                            CPLAssert( nSize == sizeof(double) );

                            memcpy( &padfList[i], pData, nSize );
                            CPL_MSBPTR64(&padfList[i]);
                        }
                        else
                        {
                            float fVal;
                            CPLAssert( nSize == sizeof(float) );

                            memcpy( &fVal, pData, nSize );
                            CPL_MSBPTR32(&fVal);

                            padfList[i] = fVal;
                        }

                        pData += nSize;
                    }
                }
                else
                {
                    CPLDebug("PG", "Field %d: Incompatible OID (%d) with OFTRealList.", iOGRField, nTypeOID );
                    continue;
                }
            }
            else
#endif
            {
                char **papszTokens = CSLTokenizeStringComplex(
                    PQgetvalue( hResult, iRecord, iField ),
                    "{,}", FALSE, FALSE );

                nCount = CSLCount(papszTokens);
                padfList = (double *) CPLCalloc(sizeof(double),nCount);

                for( i = 0; i < nCount; i++ )
                    padfList[i] = CPLAtof(papszTokens[i]);
                CSLDestroy( papszTokens );
            }

            poFeature->SetField( iOGRField, nCount, padfList );
            CPLFree( padfList );
        }

        else if( eOGRType == OFTStringList )
        {
            char **papszTokens = NULL;

#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1 ) // Binary data representation
            {
                char * pData = PQgetvalue( hResult, iRecord, iField );
                int nCount, i;

                // goto number of array elements
                pData += 3 * sizeof(int);
                memcpy( &nCount, pData, sizeof(int) );
                CPL_MSBPTR32( &nCount );

                // goto first array element
                pData += 2 * sizeof(int);

                for( i = 0; i < nCount; i++ )
                {
                    // get element size
                    int nSize = *(int *)(pData);
                    CPL_MSBPTR32( &nSize );

                    pData += sizeof(int);

                    if (nSize <= 0)
                        papszTokens = CSLAddString(papszTokens, "");
                    else
                    {
                        if (pData[nSize] == '\0')
                            papszTokens = CSLAddString(papszTokens, pData);
                        else
                        {
                            char* pszToken = (char*) CPLMalloc(nSize + 1);
                            memcpy(pszToken, pData, nSize);
                            pszToken[nSize] = '\0';
                            papszTokens = CSLAddString(papszTokens, pszToken);
                            CPLFree(pszToken);
                        }

                        pData += nSize;
                    }
                }
            }
            else
#endif
            {
                papszTokens =
                        OGRPGTokenizeStringListFromText(PQgetvalue(hResult, iRecord, iField ));
            }

            if ( papszTokens )
            {
                poFeature->SetField( iOGRField, papszTokens );
                CSLDestroy( papszTokens );
            }
        }

        else if( eOGRType == OFTDate
                 || eOGRType == OFTTime
                 || eOGRType == OFTDateTime )
        {
#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1 ) // Binary data
            {
                if ( nTypeOID == DATEOID )
                {
                    int nVal, nYear, nMonth, nDay;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(int));
                    memcpy( &nVal, PQgetvalue( hResult, iRecord, iField ), sizeof(int) );
                    CPL_MSBPTR32(&nVal);
                    OGRPGj2date(nVal + POSTGRES_EPOCH_JDATE, &nYear, &nMonth, &nDay);
                    poFeature->SetField( iOGRField, nYear, nMonth, nDay);
                }
                else if ( nTypeOID == TIMEOID )
                {
                    int nHour, nMinute, nSecond;
                    char szTime[32];
                    double dfsec;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == 8);
                    if (poDS->bBinaryTimeFormatIsInt8)
                    {
                        unsigned int nVal[2];
                        GIntBig llVal;
                        memcpy( nVal, PQgetvalue( hResult, iRecord, iField ), 8 );
                        CPL_MSBPTR32(&nVal[0]);
                        CPL_MSBPTR32(&nVal[1]);
                        llVal = (GIntBig) ((((GUIntBig)nVal[0]) << 32) | nVal[1]);
                        OGRPGdt2timeInt8(llVal, &nHour, &nMinute, &nSecond, &dfsec);
                    }
                    else
                    {
                        double dfVal;
                        memcpy( &dfVal, PQgetvalue( hResult, iRecord, iField ), 8 );
                        CPL_MSBPTR64(&dfVal);
                        OGRPGdt2timeFloat8(dfVal, &nHour, &nMinute, &nSecond, &dfsec);
                    }
                    snprintf(szTime, sizeof(szTime), "%02d:%02d:%02d", nHour, nMinute, nSecond);
                    poFeature->SetField( iOGRField, szTime);
                }
                else if ( nTypeOID == TIMESTAMPOID || nTypeOID == TIMESTAMPTZOID )
                {
                    unsigned int nVal[2];
                    GIntBig llVal;
                    int nYear, nMonth, nDay, nHour, nMinute;
                    double dfSecond = 0.0;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == 8);
                    memcpy( nVal, PQgetvalue( hResult, iRecord, iField ), 8 );
                    CPL_MSBPTR32(&nVal[0]);
                    CPL_MSBPTR32(&nVal[1]);
                    llVal = (GIntBig) ((((GUIntBig)nVal[0]) << 32) | nVal[1]);
                    if (OGRPGTimeStamp2DMYHMS(llVal, &nYear, &nMonth, &nDay, &nHour, &nMinute, &dfSecond) == 0)
                        poFeature->SetField( iOGRField, nYear, nMonth, nDay, nHour, nMinute, (float)dfSecond, 100);
                }
                else if ( nTypeOID == TEXTOID )
                {
                    OGRField  sFieldValue;

                    if( OGRParseDate( PQgetvalue( hResult, iRecord, iField ),
                                    &sFieldValue, 0 ) )
                    {
                        poFeature->SetField( iOGRField, &sFieldValue );
                    }
                }
                else
                {
                    CPLDebug( "PG", "Binary DATE format not yet implemented. OID = %d", nTypeOID );
                }
            }
            else
#endif
            {
                OGRField  sFieldValue;

                if( OGRParseDate( PQgetvalue( hResult, iRecord, iField ),
                                  &sFieldValue, 0 ) )
                {
                    poFeature->SetField( iOGRField, &sFieldValue );
                }
            }
        }
        else if( eOGRType == OFTBinary )
        {
#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1)
            {
                int nLength = PQgetlength(hResult, iRecord, iField);
                GByte* pabyData = (GByte*) PQgetvalue( hResult, iRecord, iField );
                poFeature->SetField( iOGRField, nLength, pabyData );
            }
            else
#endif  /* defined(BINARY_CURSOR_ENABLED) */
            {
                int nLength = PQgetlength(hResult, iRecord, iField);
                const char* pszBytea = (const char*) PQgetvalue( hResult, iRecord, iField );
                GByte* pabyData = BYTEAToGByteArray( pszBytea, &nLength );
                poFeature->SetField( iOGRField, nLength, pabyData );
                CPLFree(pabyData);
            }
        }
        else
        {
#if defined(BINARY_CURSOR_ENABLED)
            if ( PQfformat( hResult, iField ) == 1 &&
                 eOGRType != OFTString ) // Binary data
            {
                if ( nTypeOID == BOOLOID )
                {
                    char cVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(char));
                    cVal = *PQgetvalue( hResult, iRecord, iField );
                    poFeature->SetField( iOGRField, cVal );
                }
                else if ( nTypeOID == NUMERICOID )
                {
                    unsigned short sLen, sSign, sDscale;
                    short sWeight;
                    char* pabyData = PQgetvalue( hResult, iRecord, iField );
                    memcpy( &sLen, pabyData, sizeof(short));
                    pabyData += sizeof(short);
                    CPL_MSBPTR16(&sLen);
                    memcpy( &sWeight, pabyData, sizeof(short));
                    pabyData += sizeof(short);
                    CPL_MSBPTR16(&sWeight);
                    memcpy( &sSign, pabyData, sizeof(short));
                    pabyData += sizeof(short);
                    CPL_MSBPTR16(&sSign);
                    memcpy( &sDscale, pabyData, sizeof(short));
                    pabyData += sizeof(short);
                    CPL_MSBPTR16(&sDscale);
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == (int)((4 + sLen) * sizeof(short)));

                    NumericVar var;
                    var.ndigits = sLen;
                    var.weight = sWeight;
                    var.sign = sSign;
                    var.dscale = sDscale;
                    var.digits = (NumericDigit*)pabyData;
                    char* str = OGRPGGetStrFromBinaryNumeric(&var);
                    poFeature->SetField( iOGRField, CPLAtof(str));
                    CPLFree(str);
                }
                else if ( nTypeOID == INT2OID )
                {
                    short sVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(short));
                    memcpy( &sVal, PQgetvalue( hResult, iRecord, iField ), sizeof(short) );
                    CPL_MSBPTR16(&sVal);
                    poFeature->SetField( iOGRField, sVal );
                }
                else if ( nTypeOID == INT4OID )
                {
                    int nVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(int));
                    memcpy( &nVal, PQgetvalue( hResult, iRecord, iField ), sizeof(int) );
                    CPL_MSBPTR32(&nVal);
                    poFeature->SetField( iOGRField, nVal );
                }
                else if ( nTypeOID == INT8OID )
                {
                    unsigned int nVal[2];
                    GIntBig llVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == 8);
                    memcpy( nVal, PQgetvalue( hResult, iRecord, iField ), 8 );
                    CPL_MSBPTR32(&nVal[0]);
                    CPL_MSBPTR32(&nVal[1]);
                    llVal = (GIntBig) ((((GUIntBig)nVal[0]) << 32) | nVal[1]);
                    poFeature->SetField( iOGRField, llVal );
                }
                else if ( nTypeOID == FLOAT4OID )
                {
                    float fVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(float));
                    memcpy( &fVal, PQgetvalue( hResult, iRecord, iField ), sizeof(float) );
                    CPL_MSBPTR32(&fVal);
                    poFeature->SetField( iOGRField, fVal );
                }
                else if ( nTypeOID == FLOAT8OID )
                {
                    double dfVal;
                    CPLAssert(PQgetlength(hResult, iRecord, iField) == sizeof(double));
                    memcpy( &dfVal, PQgetvalue( hResult, iRecord, iField ), sizeof(double) );
                    CPL_MSBPTR64(&dfVal);
                    poFeature->SetField( iOGRField, dfVal );
                }
                else
                {
                    CPLDebug("PG", "Field %d(%s): Incompatible OID (%d) with %s.",
                             iOGRField, poFeatureDefn->GetFieldDefn(iOGRField)->GetNameRef(),
                             nTypeOID,
                             OGRFieldDefn::GetFieldTypeName( eOGRType ));
                    continue;
                }
            }
            else
#endif /* defined(BINARY_CURSOR_ENABLED) */
            {
                if ( eOGRType == OFTInteger &&
                     poFeatureDefn->GetFieldDefn(iOGRField)->GetWidth() == 1)
                {
                    char* pabyData = PQgetvalue( hResult, iRecord, iField );
                    if (STARTS_WITH_CI(pabyData, "T"))
                        poFeature->SetField( iOGRField, 1);
                    else if (STARTS_WITH_CI(pabyData, "F"))
                        poFeature->SetField( iOGRField, 0);
                    else
                        poFeature->SetField( iOGRField, pabyData);
                }
                else if ( eOGRType == OFTReal )
                {
                    poFeature->SetField( iOGRField,
                                CPLAtof(PQgetvalue( hResult, iRecord, iField )) );
                }
                else
                {
                    poFeature->SetField( iOGRField,
                                        PQgetvalue( hResult, iRecord, iField ) );
                }
            }
        }
    }

    return poFeature;
}

/************************************************************************/
/*                    OGRPGIsKnownGeomFuncPrefix()                      */
/************************************************************************/

static const char* const apszKnownGeomFuncPrefixes[] = {
    "ST_AsBinary", "ST_AsEWKT", "ST_AsEWKB", "EWKBBase64",
    "ST_AsText", "AsBinary", "asEWKT", "asEWKB", "asText" };
static int OGRPGIsKnownGeomFuncPrefix(const char* pszFieldName)
{
    for(size_t i=0; i<sizeof(apszKnownGeomFuncPrefixes) / sizeof(char*); i++)
    {
        if( EQUALN(pszFieldName, apszKnownGeomFuncPrefixes[i],
                   static_cast<int>(strlen(apszKnownGeomFuncPrefixes[i]))) )
            return static_cast<int>(i);
    }
    return -1;
}

/************************************************************************/
/*                CreateMapFromFieldNameToIndex()                       */
/************************************************************************/

/* Evaluating GetFieldIndex() on each field of each feature can be very */
/* expensive if the layer has many fields (total complexity of O(n^2) where */
/* n is the number of fields), so it is valuable to compute the map from */
/* the fetched fields to the OGR field index */
void OGRPGLayer::CreateMapFromFieldNameToIndex(PGresult* hResult,
                                               OGRFeatureDefn* poFeatureDefn,
                                               int*& panMapFieldNameToIndex,
                                               int*& panMapFieldNameToGeomIndex)
{
    CPLFree(panMapFieldNameToIndex);
    panMapFieldNameToIndex = NULL;
    CPLFree(panMapFieldNameToGeomIndex);
    panMapFieldNameToGeomIndex = NULL;
    if ( PQresultStatus(hResult)  == PGRES_TUPLES_OK )
    {
        panMapFieldNameToIndex =
                (int*)CPLMalloc(sizeof(int) * PQnfields(hResult));
        panMapFieldNameToGeomIndex =
                (int*)CPLMalloc(sizeof(int) * PQnfields(hResult));
        for( int iField = 0;
            iField < PQnfields(hResult);
            iField++ )
        {
            const char* pszName = PQfname(hResult,iField);
            panMapFieldNameToIndex[iField] =
                    poFeatureDefn->GetFieldIndex(pszName);
            if( panMapFieldNameToIndex[iField] < 0 )
            {
                panMapFieldNameToGeomIndex[iField] =
                        poFeatureDefn->GetGeomFieldIndex(pszName);
                if( panMapFieldNameToGeomIndex[iField] < 0 )
                {
                    int iKnownPrefix = OGRPGIsKnownGeomFuncPrefix(pszName);
                    if( iKnownPrefix >= 0 &&
                        pszName[ strlen(apszKnownGeomFuncPrefixes[iKnownPrefix]) ] == '_' )
                    {
                        panMapFieldNameToGeomIndex[iField] =
                            poFeatureDefn->GetGeomFieldIndex(pszName +
                            strlen(apszKnownGeomFuncPrefixes[iKnownPrefix]) + 1);
                    }
                }
            }
            else
                panMapFieldNameToGeomIndex[iField] = -1;
        }
    }
}

/************************************************************************/
/*                     SetInitialQueryCursor()                          */
/************************************************************************/

void OGRPGLayer::SetInitialQueryCursor()
{
    PGconn      *hPGConn = poDS->GetPGConn();
    CPLString   osCommand;

    CPLAssert( pszQueryStatement != NULL );

    poDS->SoftStartTransaction();

#if defined(BINARY_CURSOR_ENABLED)
    if ( poDS->bUseBinaryCursor && bCanUseBinaryCursor )
        osCommand.Printf( "DECLARE %s BINARY CURSOR for %s",
                            pszCursorName, pszQueryStatement );
    else
#endif
        osCommand.Printf( "DECLARE %s CURSOR for %s",
                            pszCursorName, pszQueryStatement );

    hCursorResult = OGRPG_PQexec(hPGConn, osCommand );
    if ( !hCursorResult || PQresultStatus(hCursorResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage( hPGConn ) );
        poDS->SoftRollbackTransaction();
    }
    OGRPGClearResult( hCursorResult );

    osCommand.Printf( "FETCH %d in %s", nCursorPage, pszCursorName );
    hCursorResult = OGRPG_PQexec(hPGConn, osCommand );

    CreateMapFromFieldNameToIndex(hCursorResult,
                                  poFeatureDefn,
                                  m_panMapFieldNameToIndex,
                                  m_panMapFieldNameToGeomIndex);

    nResultOffset = 0;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRPGLayer::GetNextRawFeature()

{
    PGconn      *hPGConn = poDS->GetPGConn();
    CPLString   osCommand;

    if( bInvalidated )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cursor used to read layer has been closed due to a COMMIT. "
                 "ResetReading() must be explicitly called to restart reading");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to establish an initial query?                       */
/* -------------------------------------------------------------------- */
    if( iNextShapeId == 0 && hCursorResult == NULL )
    {
        SetInitialQueryCursor();
    }

/* -------------------------------------------------------------------- */
/*      Are we in some sort of error condition?                         */
/* -------------------------------------------------------------------- */
    if( hCursorResult == NULL
        || PQresultStatus(hCursorResult) != PGRES_TUPLES_OK )
    {
        CPLDebug( "PG", "PQclear() on an error condition");

        OGRPGClearResult( hCursorResult );

        iNextShapeId = MAX(1,iNextShapeId);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to fetch more records?                               */
/* -------------------------------------------------------------------- */

    /* We test for PQntuples(hCursorResult) == 1 in the case the previous */
    /* request was a SetNextByIndex() */
    if( (PQntuples(hCursorResult) == 1 || PQntuples(hCursorResult) == nCursorPage) &&
        nResultOffset == PQntuples(hCursorResult) )
    {
        OGRPGClearResult( hCursorResult );

        osCommand.Printf( "FETCH %d in %s", nCursorPage, pszCursorName );
        hCursorResult = OGRPG_PQexec(hPGConn, osCommand );

        nResultOffset = 0;
    }

/* -------------------------------------------------------------------- */
/*      Are we out of results?  If so complete the transaction, and     */
/*      cleanup, but don't reset the next shapeid.                      */
/* -------------------------------------------------------------------- */
    if( nResultOffset == PQntuples(hCursorResult) )
    {
        CloseCursor();

        iNextShapeId = MAX(1,iNextShapeId);

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = RecordToFeature( hCursorResult,
                                             m_panMapFieldNameToIndex,
                                             m_panMapFieldNameToGeomIndex,
                                             nResultOffset );

    nResultOffset++;
    iNextShapeId++;

    return poFeature;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRPGLayer::SetNextByIndex( GIntBig nIndex )

{
    GetLayerDefn();

    if( !TestCapability(OLCFastSetNextByIndex) )
        return OGRLayer::SetNextByIndex(nIndex);

    if( nIndex == iNextShapeId)
    {
        return OGRERR_NONE;
    }

    if( nIndex < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid index");
        return OGRERR_FAILURE;
    }

    if( nIndex == 0 )
    {
        ResetReading();
        return OGRERR_NONE;
    }

    PGconn      *hPGConn = poDS->GetPGConn();
    CPLString   osCommand;

    if (hCursorResult == NULL )
    {
        SetInitialQueryCursor();
    }

    OGRPGClearResult( hCursorResult );

    osCommand.Printf( "FETCH ABSOLUTE " CPL_FRMT_GIB " in %s", nIndex+1, pszCursorName );
    hCursorResult = OGRPG_PQexec(hPGConn, osCommand );

    if (PQresultStatus(hCursorResult) != PGRES_TUPLES_OK ||
        PQntuples(hCursorResult) != 1)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read feature at invalid index (" CPL_FRMT_GIB ").", nIndex );

        CloseCursor();

        iNextShapeId = 0;

        return OGRERR_FAILURE;
    }

    nResultOffset = 0;
    iNextShapeId = nIndex;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        BYTEAToGByteArray()                           */
/************************************************************************/

GByte* OGRPGLayer::BYTEAToGByteArray( const char *pszBytea, int* pnLength )
{
    if( pszBytea == NULL )
    {
        if (pnLength) *pnLength = 0;
        return NULL;
    }

    /* hex bytea data (PostgreSQL >= 9.0) */
    if (pszBytea[0] == '\\' && pszBytea[1] == 'x')
        return CPLHexToBinary(pszBytea + 2, pnLength);

    /* +1 just to please Coverity that thinks we allocate for a null-terminate string */
    GByte* pabyData = (GByte *) CPLMalloc(strlen(pszBytea)+1);

    int iSrc = 0;
    int iDst = 0;
    while( pszBytea[iSrc] != '\0' )
    {
        if( pszBytea[iSrc] == '\\' )
        {
            if( pszBytea[iSrc+1] >= '0' && pszBytea[iSrc+1] <= '9' )
            {
                if (pszBytea[iSrc+2] == '\0' ||
                    pszBytea[iSrc+3] == '\0')
                    break;

                pabyData[iDst++] =
                    (pszBytea[iSrc+1] - 48) * 64
                    + (pszBytea[iSrc+2] - 48) * 8
                    + (pszBytea[iSrc+3] - 48) * 1;
                iSrc += 4;
            }
            else
            {
                if (pszBytea[iSrc+1] == '\0')
                    break;

                pabyData[iDst++] = pszBytea[iSrc+1];
                iSrc += 2;
            }
        }
        else
        {
            pabyData[iDst++] = pszBytea[iSrc++];
        }
    }
    if (pnLength) *pnLength = iDst;

    return pabyData;
}

/************************************************************************/
/*                          BYTEAToGeometry()                           */
/************************************************************************/

OGRGeometry *OGRPGLayer::BYTEAToGeometry( const char *pszBytea, int bIsPostGIS1 )

{
    if( pszBytea == NULL )
        return NULL;

    int nLen = 0;
    GByte *pabyWKB = BYTEAToGByteArray(pszBytea, &nLen);

    OGRGeometry *poGeometry = NULL;
    OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeometry, nLen,
                                       (bIsPostGIS1) ? wkbVariantPostGIS1 : wkbVariantOldOgc );

    CPLFree( pabyWKB );
    return poGeometry;
}

/************************************************************************/
/*                        GByteArrayToBYTEA()                           */
/************************************************************************/

char* OGRPGLayer::GByteArrayToBYTEA( const GByte* pabyData, int nLen)
{
    const size_t nTextBufLen = nLen*5+1;
    char* pszTextBuf = (char *) CPLMalloc(nTextBufLen);

    int iDst = 0;

    for( int iSrc = 0; iSrc < nLen; iSrc++ )
    {
        if( pabyData[iSrc] < 40 || pabyData[iSrc] > 126
            || pabyData[iSrc] == '\\' )
        {
            snprintf( pszTextBuf+iDst, nTextBufLen-iDst, "\\\\%03o", pabyData[iSrc] );
            iDst += 5;
        }
        else
            pszTextBuf[iDst++] = pabyData[iSrc];
    }
    pszTextBuf[iDst] = '\0';

    return pszTextBuf;
}

/************************************************************************/
/*                          GeometryToBYTEA()                           */
/************************************************************************/

char *OGRPGLayer::GeometryToBYTEA( OGRGeometry * poGeometry, int nPostGISMajor, int nPostGISMinor )

{
    const int nWkbSize = poGeometry->WkbSize();

    GByte *pabyWKB = (GByte *) CPLMalloc(nWkbSize);
    if( (nPostGISMajor > 2 || (nPostGISMajor == 2 && nPostGISMinor >= 2)) &&
        wkbFlatten(poGeometry->getGeometryType()) == wkbPoint &&
        poGeometry->IsEmpty() )
    {
        if( poGeometry->exportToWkb( wkbNDR, pabyWKB, wkbVariantIso ) != OGRERR_NONE )
        {
            CPLFree( pabyWKB );
            return CPLStrdup("");
        }
    }
    else if( poGeometry->exportToWkb( wkbNDR, pabyWKB,
                                 (nPostGISMajor < 2) ? wkbVariantPostGIS1 : wkbVariantOldOgc ) != OGRERR_NONE )
    {
        CPLFree(pabyWKB);
        return CPLStrdup("");
    }

    char *pszTextBuf = GByteArrayToBYTEA( pabyWKB, nWkbSize );
    CPLFree(pabyWKB);

    return pszTextBuf;
}

/************************************************************************/
/*                          OIDToGeometry()                             */
/************************************************************************/

OGRGeometry *OGRPGLayer::OIDToGeometry( Oid oid )

{
    if( oid == 0 )
        return NULL;

    PGconn *hPGConn = poDS->GetPGConn();
    const int fd = lo_open( hPGConn, oid, INV_READ );
    if( fd < 0 )
        return NULL;

    static const int MAX_WKB = 500000;
    GByte *pabyWKB = (GByte *) CPLMalloc(MAX_WKB);
    const int nBytes = lo_read( hPGConn, fd, (char *) pabyWKB, MAX_WKB );
    lo_close( hPGConn, fd );

    OGRGeometry *poGeometry = NULL;
    OGRGeometryFactory::createFromWkb(
        pabyWKB, NULL, &poGeometry, nBytes,
        poDS->sPostGISVersion.nMajor < 2
        ? wkbVariantPostGIS1
        : wkbVariantOldOgc );

    CPLFree( pabyWKB );

    return poGeometry;
}

/************************************************************************/
/*                           GeometryToOID()                            */
/************************************************************************/

Oid OGRPGLayer::GeometryToOID( OGRGeometry * poGeometry )

{
    PGconn *hPGConn = poDS->GetPGConn();
    const int nWkbSize = poGeometry->WkbSize();

    GByte *pabyWKB = (GByte *) CPLMalloc(nWkbSize);
    if( poGeometry->exportToWkb( wkbNDR, pabyWKB,
                                 (poDS->sPostGISVersion.nMajor < 2) ? wkbVariantPostGIS1 : wkbVariantOldOgc ) != OGRERR_NONE )
        return 0;

    Oid oid = lo_creat( hPGConn, INV_READ|INV_WRITE );

    const int fd = lo_open( hPGConn, oid, INV_WRITE );
    const int nBytesWritten = lo_write( hPGConn, fd, (char *) pabyWKB, nWkbSize );
    lo_close( hPGConn, fd );

    if( nBytesWritten != nWkbSize )
    {
        CPLDebug( "PG",
                  "Only wrote %d bytes of %d intended for (fd=%d,oid=%d).\n",
                  nBytesWritten, nWkbSize, fd, oid );
    }

    CPLFree( pabyWKB );

    return oid;
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRPGLayer::StartTransaction()

{
    return poDS->StartTransaction();
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRPGLayer::CommitTransaction()

{
    return poDS->CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRPGLayer::RollbackTransaction()

{
    return poDS->RollbackTransaction();
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRPGLayer::GetFIDColumn()

{
    GetLayerDefn();

    if( pszFIDColumn != NULL )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      For PostGIS use internal Extend(geometry) function              */
/*      in other cases we use standard OGRLayer::GetExtent()            */
/************************************************************************/

OGRErr OGRPGLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    CPLString   osCommand;

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    OGRPGGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->myGetGeomFieldDefn(iGeomField);

    const char* pszExtentFct =
        poDS->sPostGISVersion.nMajor >= 2 ? "ST_Extent" : "Extent";

    if ( TestCapability(OLCFastGetExtent) )
    {
        /* Do not take the spatial filter into account */
        osCommand.Printf( "SELECT %s(%s) FROM %s AS ogrpgextent",
                          pszExtentFct,
                          OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef()).c_str(),
                          GetFromClauseForGetExtent().c_str() );
    }
    else if ( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
    {
        /* Probably not very efficient, but more efficient than client-side implementation */
        osCommand.Printf( "SELECT %s(ST_GeomFromWKB(ST_AsBinary(%s))) FROM %s AS ogrpgextent",
                          pszExtentFct,
                          OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef()).c_str(),
                          GetFromClauseForGetExtent().c_str() );
    }

    if( !osCommand.empty() )
    {
        if( RunGetExtentRequest(psExtent, bForce, osCommand, FALSE) == OGRERR_NONE )
            return OGRERR_NONE;
    }
    if( iGeomField == 0 )
        return OGRLayer::GetExtent( psExtent, bForce );
    else
        return OGRLayer::GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRPGLayer::RunGetExtentRequest( OGREnvelope *psExtent,
                                        CPL_UNUSED int bForce,
                                        CPLString osCommand,
                                        int bErrorAsDebug )
{
    if ( psExtent == NULL )
        return OGRERR_FAILURE;

    PGconn      *hPGConn = poDS->GetPGConn();
    PGresult    *hResult = NULL;

    hResult = OGRPG_PQexec( hPGConn, osCommand, FALSE, bErrorAsDebug );
    if( ! hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK || PQgetisnull(hResult,0,0) )
    {
        OGRPGClearResult( hResult );
        CPLDebug("PG","Unable to get extent by PostGIS.");
        return OGRERR_FAILURE;
    }

    char * pszBox = PQgetvalue(hResult,0,0);
    char * ptr, *ptrEndParenthesis;
    char szVals[64*6+6];

    ptr = strchr(pszBox, '(');
    if (ptr)
        ptr ++;
    if (ptr == NULL ||
        (ptrEndParenthesis = strchr(ptr, ')')) == NULL ||
        ptrEndParenthesis - ptr > (int)(sizeof(szVals) - 1))
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                    "Bad extent representation: '%s'", pszBox);

        OGRPGClearResult( hResult );
        return OGRERR_FAILURE;
    }

    strncpy(szVals,ptr,ptrEndParenthesis - ptr);
    szVals[ptrEndParenthesis - ptr] = '\0';

    char ** papszTokens = CSLTokenizeString2(szVals," ,",CSLT_HONOURSTRINGS);
    int nTokenCnt = poDS->sPostGISVersion.nMajor >= 1 ? 4 : 6;

    if ( CSLCount(papszTokens) != nTokenCnt )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                    "Bad extent representation: '%s'", pszBox);
        CSLDestroy(papszTokens);

        OGRPGClearResult( hResult );
        return OGRERR_FAILURE;
    }

    // Take X,Y coords
    // For PostGIS ver >= 1.0.0 -> Tokens: X1 Y1 X2 Y2 (nTokenCnt = 4)
    // For PostGIS ver < 1.0.0 -> Tokens: X1 Y1 Z1 X2 Y2 Z2 (nTokenCnt = 6)
    // =>   X2 index calculated as nTokenCnt/2
    //      Y2 index calculated as nTokenCnt/2+1

    psExtent->MinX = CPLAtof( papszTokens[0] );
    psExtent->MinY = CPLAtof( papszTokens[1] );
    psExtent->MaxX = CPLAtof( papszTokens[nTokenCnt/2] );
    psExtent->MaxY = CPLAtof( papszTokens[nTokenCnt/2+1] );

    CSLDestroy(papszTokens);
    OGRPGClearResult( hResult );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetLayerDefn()                              */
/************************************************************************/

OGRFeatureDefn * OGRPGLayer::GetLayerDefn()
{
    return poFeatureDefn;
}

/************************************************************************/
/*                        ReadResultDefinition()                        */
/*                                                                      */
/*      Build a schema from the current resultset.                      */
/************************************************************************/

int OGRPGLayer::ReadResultDefinition(PGresult *hInitialResultIn)

{
    PGresult            *hResult = hInitialResultIn;

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    poFeatureDefn = new OGRPGFeatureDefn( "sql_statement" );
    SetDescription( poFeatureDefn->GetName() );
    int            iRawField;

    poFeatureDefn->Reference();

    for( iRawField = 0; iRawField < PQnfields(hResult); iRawField++ )
    {
        OGRFieldDefn    oField( PQfname(hResult,iRawField), OFTString);
        Oid             nTypeOID;

        nTypeOID = PQftype(hResult,iRawField);

        int iGeomFuncPrefix;
        if( EQUAL(oField.GetNameRef(),"ogc_fid") )
        {
            if (pszFIDColumn)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "More than one ogc_fid column was found in the result of the SQL request. Only last one will be used");
            }
            CPLFree(pszFIDColumn);
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
        else if( (iGeomFuncPrefix =
                    OGRPGIsKnownGeomFuncPrefix(oField.GetNameRef())) >= 0 ||
                 nTypeOID == poDS->GetGeometryOID()  ||
                 nTypeOID == poDS->GetGeographyOID()  )
        {
            OGRPGGeomFieldDefn* poGeomFieldDefn =
                new OGRPGGeomFieldDefn(this, oField.GetNameRef());
            if( iGeomFuncPrefix >= 0 &&
                oField.GetNameRef()[strlen(
                    apszKnownGeomFuncPrefixes[iGeomFuncPrefix])] == '_' )
            {
                poGeomFieldDefn->SetName( oField.GetNameRef() +
                    strlen(apszKnownGeomFuncPrefixes[iGeomFuncPrefix]) + 1 );
            }
            if (nTypeOID == poDS->GetGeographyOID())
            {
                poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOGRAPHY;
                poGeomFieldDefn->nSRSId = 4326;
            }
            else
                poGeomFieldDefn->ePostgisType = GEOM_TYPE_GEOMETRY;
            poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
            continue;
        }
        else if( EQUAL(oField.GetNameRef(),"WKB_GEOMETRY") )
        {
            if( nTypeOID == OIDOID )
                bWkbAsOid = TRUE;
            OGRPGGeomFieldDefn* poGeomFieldDefn =
                new OGRPGGeomFieldDefn(this, oField.GetNameRef());
            poGeomFieldDefn->ePostgisType = GEOM_TYPE_WKB;
            poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
            continue;
        }

        //CPLDebug("PG", "Field %s, oid %d", oField.GetNameRef(), nTypeOID);

        if( nTypeOID == BYTEAOID )
        {
            oField.SetType( OFTBinary );
        }
        else if( nTypeOID == CHAROID ||
                 nTypeOID == TEXTOID ||
                 nTypeOID == BPCHAROID ||
                 nTypeOID == VARCHAROID )
        {
            oField.SetType( OFTString );

            /* See http://www.mail-archive.com/pgsql-hackers@postgresql.org/msg57726.html */
            /* nTypmod = width + 4 */
            int nTypmod = PQfmod(hResult, iRawField);
            if (nTypmod >= 4 && (nTypeOID == BPCHAROID ||
                               nTypeOID == VARCHAROID ) )
            {
                oField.SetWidth( nTypmod - 4);
            }
        }
        else if( nTypeOID == BOOLOID )
        {
            oField.SetType( OFTInteger );
            oField.SetSubType( OFSTBoolean );
            oField.SetWidth( 1 );
        }
        else if (nTypeOID == INT2OID )
        {
            oField.SetType( OFTInteger );
            oField.SetSubType( OFSTInt16 );
            oField.SetWidth( 5 );
        }
        else if (nTypeOID == INT4OID )
        {
            oField.SetType( OFTInteger );
        }
        else if ( nTypeOID == INT8OID )
        {
            oField.SetType( OFTInteger64 );
        }
        else if( nTypeOID == FLOAT4OID )
        {
            oField.SetType( OFTReal );
            oField.SetSubType( OFSTFloat32 );
        }
        else if( nTypeOID == FLOAT8OID )
        {
            oField.SetType( OFTReal );
        }
        else if( nTypeOID == NUMERICOID || nTypeOID == NUMERICARRAYOID )
        {
            /* See http://www.mail-archive.com/pgsql-hackers@postgresql.org/msg57726.html */
            /* typmod = (width << 16) + precision + 4 */
            int nTypmod = PQfmod(hResult, iRawField);
            if (nTypmod >= 4)
            {
                int nWidth = (nTypmod - 4) >> 16;
                int nPrecision = (nTypmod - 4) & 0xFFFF;
                if (nWidth <= 10 && nPrecision == 0)
                {
                    oField.SetType( (nTypeOID == NUMERICOID) ? OFTInteger : OFTIntegerList );
                    oField.SetWidth( nWidth );
                }
                else
                {
                    oField.SetType( (nTypeOID == NUMERICOID) ? OFTReal : OFTRealList );
                    oField.SetWidth( nWidth );
                    oField.SetPrecision( nPrecision );
                }
            }
            else
                oField.SetType( (nTypeOID == NUMERICOID) ? OFTReal : OFTRealList );
        }
        else if ( nTypeOID == BOOLARRAYOID )
        {
            oField.SetType ( OFTIntegerList );
            oField.SetSubType( OFSTBoolean );
            oField.SetWidth( 1 );
        }
        else if ( nTypeOID == INT2ARRAYOID )
        {
            oField.SetType ( OFTIntegerList );
            oField.SetSubType( OFSTInt16 );
        }
        else if ( nTypeOID == INT4ARRAYOID )
        {
            oField.SetType ( OFTIntegerList );
        }
        else if ( nTypeOID == INT8ARRAYOID )
        {
            oField.SetType ( OFTInteger64List );
        }
        else if ( nTypeOID == FLOAT4ARRAYOID )
        {
          oField.SetType ( OFTRealList );
          oField.SetSubType( OFSTFloat32 );
        }
        else if ( nTypeOID == FLOAT8ARRAYOID )
        {
            oField.SetType ( OFTRealList );
        }
        else if ( nTypeOID == TEXTARRAYOID ||
                  nTypeOID == BPCHARARRAYOID ||
                  nTypeOID == VARCHARARRAYOID )
        {
            oField.SetType ( OFTStringList );
        }
        else if ( nTypeOID == DATEOID )
        {
            oField.SetType( OFTDate );
        }
        else if ( nTypeOID == TIMEOID )
        {
            oField.SetType( OFTTime );
        }
        else if ( nTypeOID == TIMESTAMPOID ||
                  nTypeOID == TIMESTAMPTZOID )
        {
#if defined(BINARY_CURSOR_ENABLED)
            /* We can't deserialize properly timestamp with time zone */
            /* with binary cursors */
            if (nTypeOID == TIMESTAMPTZOID)
                bCanUseBinaryCursor = FALSE;
#endif

            oField.SetType( OFTDateTime );
        }
        else /* unknown type */
        {
            CPLDebug("PG", "Unhandled OID (%d) for column %s. Defaulting to String.",
                     nTypeOID, oField.GetNameRef());
            oField.SetType( OFTString );
        }

        poFeatureDefn->AddFieldDefn( &oField );
    }

    return TRUE;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

OGRSpatialReference* OGRPGGeomFieldDefn::GetSpatialRef()
{
    if( poLayer == NULL )
        return NULL;
    if (nSRSId == UNDETERMINED_SRID)
        poLayer->ResolveSRID(this);

    if( poSRS == NULL && nSRSId > 0 )
    {
        poSRS = poLayer->GetDS()->FetchSRS( nSRSId );
        if( poSRS != NULL )
            poSRS->Reference();
    }
    return poSRS;
}

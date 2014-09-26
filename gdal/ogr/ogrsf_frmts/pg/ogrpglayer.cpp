/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

#define CURSOR_PAGE     500

// These originally are defined in libpq-fs.h.

#ifndef INV_WRITE
#define INV_WRITE               0x00020000
#define INV_READ                0x00040000
#endif

/************************************************************************/
/*                           OGRPGLayer()                               */
/************************************************************************/

OGRPGLayer::OGRPGLayer()

{
    poDS = NULL;

    bWkbAsOid = FALSE;
    pszQueryStatement = NULL;

    bHasFid = FALSE;
    pszFIDColumn = NULL;

    iNextShapeId = 0;
    nResultOffset = 0;

    pszCursorName = CPLStrdup(CPLSPrintf("OGRPGLayerReader%p", this));

    hCursorResult = NULL;

    bCanUseBinaryCursor = TRUE;

    poFeatureDefn = NULL;
    panMapFieldNameToIndex = NULL;
    panMapFieldNameToGeomIndex = NULL;
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
    CPLFree( panMapFieldNameToIndex );
    CPLFree( panMapFieldNameToGeomIndex );
    CPLFree( pszCursorName );

    if( poFeatureDefn )
    {
        poFeatureDefn->UnsetLayer();
        poFeatureDefn->Release();
    }
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

        hCursorResult = OGRPG_PQexec(hPGConn, osCommand.c_str());
        OGRPGClearResult( hCursorResult );

        poDS->FlushSoftTransaction();

        hCursorResult = NULL;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGLayer::ResetReading()

{
    GetLayerDefn();

    iNextShapeId = 0;

    CloseCursor();
}

/************************************************************************/
/*                    OGRPGGetStrFromBinaryNumeric()                    */
/************************************************************************/

/* Adaptation of get_str_from_var() from pgsql/src/backend/utils/adt/numeric.c */

typedef short NumericDigit;

typedef struct NumericVar
{
        int			ndigits;		/* # of digits in digits[] - can be 0! */
        int			weight;			/* weight of first digit */
        int			sign;			/* NUMERIC_POS, NUMERIC_NEG, or NUMERIC_NAN */
        int			dscale;			/* display scale */
        NumericDigit *digits;		/* base-NBASE digits */
} NumericVar;

#define NUMERIC_POS			0x0000
#define NUMERIC_NEG			0x4000
#define NUMERIC_NAN			0xC000

#define DEC_DIGITS	4
/*
* get_str_from_var() -
*
*	Convert a var to text representation (guts of numeric_out).
*	CAUTION: var's contents may be modified by rounding!
*	Returns a palloc'd string.
*/
static char *
OGRPGGetStrFromBinaryNumeric(NumericVar *var)
{
        char	   *str;
        char	   *cp;
        char	   *endcp;
        int			i;
        int			d;
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
                        /* In the first digit, suppress extra leading decimal zeroes */
                        {
                                bool		putit = (d > 0);

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

#define POSTGRES_EPOCH_JDATE	2451545 /* == date2j(2000, 1, 1) */

static
void OGRPGj2date(int jd, int *year, int *month, int *day)
{
	unsigned int julian;
	unsigned int quad;
	unsigned int extra;
	int			y;

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
}	/* j2date() */


/************************************************************************/
/*                              OGRPGdt2time()                          */
/************************************************************************/

#define USECS_PER_SEC	1000000
#define USECS_PER_MIN   ((GIntBig) 60 * USECS_PER_SEC)
#define USECS_PER_HOUR  ((GIntBig) 3600 * USECS_PER_SEC)
#define USECS_PER_DAY   ((GIntBig) 3600 * 24 * USECS_PER_SEC)

/* Coming from dt2time() in pgsql/src/backend/utils/adt/timestamp.c */

static
void
OGRPGdt2timeInt8(GIntBig jd, int *hour, int *min, int *sec, double *fsec)
{
	GIntBig		time;

	time = jd;

	*hour = (int) (time / USECS_PER_HOUR);
	time -= (GIntBig) (*hour) * USECS_PER_HOUR;
	*min = (int) (time / USECS_PER_MIN);
	time -=  (GIntBig) (*min) * USECS_PER_MIN;
	*sec = (int)time / USECS_PER_SEC;
	*fsec = (double)(time - *sec * USECS_PER_SEC);
}	/* dt2time() */

static
void
OGRPGdt2timeFloat8(double jd, int *hour, int *min, int *sec, double *fsec)
{
	double	time;

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
} while(0)

/* Coming from timestamp2tm() in pgsql/src/backend/utils/adt/timestamp.c */

static
int OGRPGTimeStamp2DMYHMS(GIntBig dt, int *year, int *month, int *day,
                                      int* hour, int* min, int* sec)
{
        GIntBig date;
	GIntBig time;
        double fsec;

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
	OGRPGdt2timeInt8(time, hour, min, sec, &fsec);

        return 0;
}


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

OGRFeature *OGRPGLayer::RecordToFeature( int iRecord )

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
         iField < PQnfields(hCursorResult);
         iField++ )
    {
        int     iOGRField;

#if !defined(PG_PRE74)
        int nTypeOID = PQftype(hCursorResult, iField);
#endif
        const char* pszFieldName = PQfname(hCursorResult,iField);

/* -------------------------------------------------------------------- */
/*      Handle FID.                                                     */
/* -------------------------------------------------------------------- */
        if( bHasFid && EQUAL(pszFieldName,pszFIDColumn) )
        {
#if !defined(PG_PRE74)
            if ( PQfformat( hCursorResult, iField ) == 1 ) // Binary data representation
            {
                if ( nTypeOID == INT4OID)
                {
                    int nVal;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == sizeof(int));
                    memcpy( &nVal, PQgetvalue( hCursorResult, iRecord, iField ), sizeof(int) );
                    CPL_MSBPTR32(&nVal);
                    poFeature->SetFID( nVal );
                }
                else
                {
                    CPLDebug("PG", "FID. Unhandled OID %d.", nTypeOID );
                    continue;
                }
            }
            else
#endif /* notdef PG_PRE74 */
            {
                char* pabyData = PQgetvalue(hCursorResult,iRecord,iField);
                /* ogr_pg_20 may crash if PostGIS is unavailable and we don't test pabyData */
                if (pabyData)
                    poFeature->SetFID( atoi(pabyData) );
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
            if ( !poDS->bUseBinaryCursor &&
                 EQUALN(pszFieldName,"BinaryBase64", strlen("BinaryBase64")) )
            {
                GByte* pabyData = (GByte*)PQgetvalue( hCursorResult,
                                                        iRecord, iField);

                int nLength = PQgetlength(hCursorResult, iRecord, iField);

                /* No geometry */
                if (nLength == 0)
                    continue;

                nLength = CPLBase64DecodeInPlace(pabyData);
                OGRGeometry * poGeom = NULL;
                OGRGeometryFactory::createFromWkb( pabyData, NULL, &poGeom, nLength );

                if( poGeom != NULL )
                {
                    poGeom->assignSpatialReference( poGeomFieldDefn->GetSpatialRef() );
                    poFeature->SetGeomFieldDirectly(iOGRGeomField,  poGeom );
                }

                continue;
            }
            else if ( EQUALN(pszFieldName,"ST_AsBinary", strlen("ST_AsBinary")) ||
                      EQUALN(pszFieldName,"AsBinary", strlen("AsBinary")) )
            {
                GByte* pabyVal = (GByte*) PQgetvalue( hCursorResult,
                                             iRecord, iField);
                const char* pszVal = (const char*) pabyVal;

                int nLength = PQgetlength(hCursorResult, iRecord, iField);

                /* No geometry */
                if (nLength == 0)
                    continue;
                    
                OGRGeometry * poGeom = NULL;
                if( !poDS->bUseBinaryCursor && nLength >= 4 &&
                    /* escaped byea data */
                    (strncmp(pszVal, "\\000",4) == 0 || strncmp(pszVal, "\\001",4) == 0 ||
                    /* hex bytea data (PostgreSQL >= 9.0) */
                     strncmp(pszVal, "\\x00",4) == 0 || strncmp(pszVal, "\\x01",4) == 0) )
                {
                    poGeom = BYTEAToGeometry(pszVal);
                }
                else
                    OGRGeometryFactory::createFromWkb( pabyVal, NULL, &poGeom, nLength );
                
                if( poGeom != NULL )
                {
                    poGeom->assignSpatialReference( poGeomFieldDefn->GetSpatialRef() );
                    poFeature->SetGeomFieldDirectly(iOGRGeomField,  poGeom );
                }

                continue;
            }
            else if ( !poDS->bUseBinaryCursor &&
                      EQUALN(pszFieldName,"EWKBBase64",strlen("EWKBBase64")) )
            {
                GByte* pabyData = (GByte*)PQgetvalue( hCursorResult,
                                                        iRecord, iField);

                int nLength = PQgetlength(hCursorResult, iRecord, iField);

                /* No geometry */
                if (nLength == 0)
                    continue;

                nLength = CPLBase64DecodeInPlace(pabyData);
                OGRGeometry * poGeom = OGRGeometryFromEWKB(pabyData, nLength, NULL);

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
                char * pabyData = PQgetvalue( hCursorResult,
                                                        iRecord, iField);

                int nLength = PQgetlength(hCursorResult, iRecord, iField);

                /* No geometry */
                if (nLength == 0)
                    continue;

                OGRGeometry * poGeom;

                if( !poDS->bUseBinaryCursor &&
                    (strncmp(pabyData, "\\x00",4) == 0 || strncmp(pabyData, "\\x01",4) == 0 ||
                     strncmp(pabyData, "\\000",4) == 0 || strncmp(pabyData, "\\001",4) == 0) )
                {
                    GByte* pabyEWKB = BYTEAToGByteArray(pabyData, &nLength);
                    poGeom = OGRGeometryFromEWKB(pabyEWKB, nLength, NULL);
                    CPLFree(pabyEWKB);
                }
                else if( nLength >= 2 && (EQUALN(pabyData,"00",2) || EQUALN(pabyData,"01",2)) )
                {
                    poGeom = OGRGeometryFromHexEWKB(pabyData, NULL);
                }
                else
                {
                    poGeom = OGRGeometryFromEWKB((GByte*)pabyData, nLength, NULL);
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
                char        *pszWKT;
                char        *pszPostSRID;
                OGRGeometry *poGeometry = NULL;

                pszWKT = PQgetvalue( hCursorResult, iRecord, iField );
                pszPostSRID = pszWKT;

                // optionally strip off PostGIS SRID identifier.  This
                // happens if we got a raw geometry field.
                if( EQUALN(pszPostSRID,"SRID=",5) )
                {
                    while( *pszPostSRID != '\0' && *pszPostSRID != ';' )
                        pszPostSRID++;
                    if( *pszPostSRID == ';' )
                        pszPostSRID++;
                }

                if( EQUALN(pszPostSRID,"00",2) || EQUALN(pszPostSRID,"01",2) )
                {
                    poGeometry = OGRGeometryFromHexEWKB( pszWKT, NULL );
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
            GByte* pabyData = (GByte*) PQgetvalue( hCursorResult, iRecord, iField);

            if( bWkbAsOid )
            {
                poGeometry =
                    OIDToGeometry( (Oid) atoi((const char*)pabyData) );
            }
            else
            {
                if (poDS->bUseBinaryCursor
#if !defined(PG_PRE74)
                    && PQfformat( hCursorResult, iField ) == 1 
#endif
                   )
                {
                    int nLength = PQgetlength(hCursorResult, iRecord, iField);
                    poGeometry = OGRGeometryFromEWKB(pabyData, nLength, NULL);
                }
                if (poGeometry == NULL)
                {
                    poGeometry = BYTEAToGeometry( (const char*)pabyData );
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

        if( PQgetisnull( hCursorResult, iRecord, iField ) )
            continue;

        OGRFieldType eOGRType = 
            poFeatureDefn->GetFieldDefn(iOGRField)->GetType();

        if( eOGRType == OFTIntegerList)
        {
            int *panList, nCount, i;

#if !defined(PG_PRE74)
            if ( PQfformat( hCursorResult, iField ) == 1 ) // Binary data representation
            {
                if (nTypeOID == INT4ARRAYOID)
                {
                    char * pData = PQgetvalue( hCursorResult, iRecord, iField );

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

                        CPLAssert( nSize == sizeof(int) );

                        pData += sizeof(int);

                        memcpy( &panList[i], pData, nSize );
                        CPL_MSBPTR32(&panList[i]);

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
#endif /* notdef PG_PRE74 */
            {
                char **papszTokens;
                papszTokens = CSLTokenizeStringComplex(
                    PQgetvalue( hCursorResult, iRecord, iField ),
                    "{,}", FALSE, FALSE );

                nCount = CSLCount(papszTokens);
                panList = (int *) CPLCalloc(sizeof(int),nCount);

                for( i = 0; i < nCount; i++ )
                    panList[i] = atoi(papszTokens[i]);
                CSLDestroy( papszTokens );
            }
            poFeature->SetField( iOGRField, nCount, panList );
            CPLFree( panList );
        }

        else if( eOGRType == OFTRealList )
        {
            int nCount, i;
            double *padfList;

#if !defined(PG_PRE74)
            if ( PQfformat( hCursorResult, iField ) == 1 ) // Binary data representation
            {
                if (nTypeOID == FLOAT8ARRAYOID || nTypeOID == FLOAT4ARRAYOID)
                {
                    char * pData = PQgetvalue( hCursorResult, iRecord, iField );

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
#endif /* notdef PG_PRE74 */
            {
                char **papszTokens;
                papszTokens = CSLTokenizeStringComplex(
                    PQgetvalue( hCursorResult, iRecord, iField ),
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
            char **papszTokens = 0;

#if !defined(PG_PRE74)
            if ( PQfformat( hCursorResult, iField ) == 1 ) // Binary data representation
            {
                char * pData = PQgetvalue( hCursorResult, iRecord, iField );
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
#endif /* notdef PG_PRE74 */
            {
                papszTokens =
                        OGRPGTokenizeStringListFromText(PQgetvalue(hCursorResult, iRecord, iField ));
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
#if !defined(PG_PRE74)
            if ( PQfformat( hCursorResult, iField ) == 1 ) // Binary data
            {
                if ( nTypeOID == DATEOID )
                {
                    int nVal, nYear, nMonth, nDay;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == sizeof(int));
                    memcpy( &nVal, PQgetvalue( hCursorResult, iRecord, iField ), sizeof(int) );
                    CPL_MSBPTR32(&nVal);
                    OGRPGj2date(nVal + POSTGRES_EPOCH_JDATE, &nYear, &nMonth, &nDay);
                    poFeature->SetField( iOGRField, nYear, nMonth, nDay);
                }
                else if ( nTypeOID == TIMEOID )
                {
                    int nHour, nMinute, nSecond;
                    char szTime[32];
                    double dfsec;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == 8);
                    if (poDS->bBinaryTimeFormatIsInt8)
                    {
                        unsigned int nVal[2];
                        GIntBig llVal;
                        memcpy( nVal, PQgetvalue( hCursorResult, iRecord, iField ), 8 );
                        CPL_MSBPTR32(&nVal[0]);
                        CPL_MSBPTR32(&nVal[1]);
                        llVal = (GIntBig) ((((GUIntBig)nVal[0]) << 32) | nVal[1]);
                        OGRPGdt2timeInt8(llVal, &nHour, &nMinute, &nSecond, &dfsec);
                    }
                    else
                    {
                        double dfVal;
                        memcpy( &dfVal, PQgetvalue( hCursorResult, iRecord, iField ), 8 );
                        CPL_MSBPTR64(&dfVal);
                        OGRPGdt2timeFloat8(dfVal, &nHour, &nMinute, &nSecond, &dfsec);
                    }
                    sprintf(szTime, "%02d:%02d:%02d", nHour, nMinute, nSecond);
                    poFeature->SetField( iOGRField, szTime);
                }
                else if ( nTypeOID == TIMESTAMPOID || nTypeOID == TIMESTAMPTZOID )
                {
                    unsigned int nVal[2];
                    GIntBig llVal;
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == 8);
                    memcpy( nVal, PQgetvalue( hCursorResult, iRecord, iField ), 8 );
                    CPL_MSBPTR32(&nVal[0]);
                    CPL_MSBPTR32(&nVal[1]);
                    llVal = (GIntBig) ((((GUIntBig)nVal[0]) << 32) | nVal[1]);
                    if (OGRPGTimeStamp2DMYHMS(llVal, &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) == 0)
                        poFeature->SetField( iOGRField, nYear, nMonth, nDay, nHour, nMinute, nSecond);
                }
                else if ( nTypeOID == TEXTOID )
                {
                    OGRField  sFieldValue;

                    if( OGRParseDate( PQgetvalue( hCursorResult, iRecord, iField ),
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
#endif /* notdef PG_PRE74 */
            {
                OGRField  sFieldValue;

                if( OGRParseDate( PQgetvalue( hCursorResult, iRecord, iField ),
                                  &sFieldValue, 0 ) )
                {
                    poFeature->SetField( iOGRField, &sFieldValue );
                }
            }
        }
        else if( eOGRType == OFTBinary )
        {
#if !defined(PG_PRE74)
            if ( PQfformat( hCursorResult, iField ) == 1)
            {
                int nLength = PQgetlength(hCursorResult, iRecord, iField);
                GByte* pabyData = (GByte*) PQgetvalue( hCursorResult, iRecord, iField );
                poFeature->SetField( iOGRField, nLength, pabyData );
            }
            else
#endif  /* notdef PG_PRE74 */
            {
                int nLength = PQgetlength(hCursorResult, iRecord, iField);
                const char* pszBytea = (const char*) PQgetvalue( hCursorResult, iRecord, iField );
                GByte* pabyData = BYTEAToGByteArray( pszBytea, &nLength );
                poFeature->SetField( iOGRField, nLength, pabyData );
                CPLFree(pabyData);
            }
        }
        else
        {
#if !defined(PG_PRE74)
            if ( PQfformat( hCursorResult, iField ) == 1 &&
                 eOGRType != OFTString ) // Binary data
            {
                if ( nTypeOID == BOOLOID )
                {
                    char cVal;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == sizeof(char));
                    cVal = *PQgetvalue( hCursorResult, iRecord, iField );
                    poFeature->SetField( iOGRField, cVal );
                }
                else if ( nTypeOID == NUMERICOID )
                {
                    unsigned short sLen, sSign, sDscale;
                    short sWeight;
                    char* pabyData = PQgetvalue( hCursorResult, iRecord, iField );
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
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == (int)((4 + sLen) * sizeof(short)));

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
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == sizeof(short));
                    memcpy( &sVal, PQgetvalue( hCursorResult, iRecord, iField ), sizeof(short) );
                    CPL_MSBPTR16(&sVal);
                    poFeature->SetField( iOGRField, sVal );
                }
                else if ( nTypeOID == INT4OID )
                {
                    int nVal;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == sizeof(int));
                    memcpy( &nVal, PQgetvalue( hCursorResult, iRecord, iField ), sizeof(int) );
                    CPL_MSBPTR32(&nVal);
                    poFeature->SetField( iOGRField, nVal );
                }
                else if ( nTypeOID == INT8OID )
                {
                    unsigned int nVal[2];
                    GIntBig llVal;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == 8);
                    memcpy( nVal, PQgetvalue( hCursorResult, iRecord, iField ), 8 );
                    CPL_MSBPTR32(&nVal[0]);
                    CPL_MSBPTR32(&nVal[1]);
                    llVal = (GIntBig) ((((GUIntBig)nVal[0]) << 32) | nVal[1]);
                    /* FIXME : 64bit -> 32bit conversion */
                    poFeature->SetField( iOGRField, (int)llVal );
                }
                else if ( nTypeOID == FLOAT4OID )
                {
                    float fVal;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == sizeof(float));
                    memcpy( &fVal, PQgetvalue( hCursorResult, iRecord, iField ), sizeof(float) );
                    CPL_MSBPTR32(&fVal);
                    poFeature->SetField( iOGRField, fVal );
                }
                else if ( nTypeOID == FLOAT8OID )
                {
                    double dfVal;
                    CPLAssert(PQgetlength(hCursorResult, iRecord, iField) == sizeof(double));
                    memcpy( &dfVal, PQgetvalue( hCursorResult, iRecord, iField ), sizeof(double) );
                    CPL_MSBPTR64(&dfVal);
                    poFeature->SetField( iOGRField, dfVal );
                }
                else
                {
                    CPLDebug("PG", "Field %d: Incompatible OID (%d) with %s.", iOGRField, nTypeOID,
                             OGRFieldDefn::GetFieldTypeName( eOGRType ));
                    continue;
                }
            }
            else
#endif /* notdef PG_PRE74 */
            {
                if ( eOGRType == OFTInteger &&
                     poFeatureDefn->GetFieldDefn(iOGRField)->GetWidth() == 1)
                {
                    char* pabyData = PQgetvalue( hCursorResult, iRecord, iField );
                    if (EQUALN(pabyData, "T", 1))
                        poFeature->SetField( iOGRField, 1);
                    else if (EQUALN(pabyData, "F", 1))
                        poFeature->SetField( iOGRField, 0);
                    else
                        poFeature->SetField( iOGRField, pabyData);
                }
                else if ( eOGRType == OFTReal )
                {
                    poFeature->SetField( iOGRField,
                                CPLAtof(PQgetvalue( hCursorResult, iRecord, iField )) );
                }
                else
                {
                    poFeature->SetField( iOGRField,
                                        PQgetvalue( hCursorResult, iRecord, iField ) );
                }
            }
        }
    }

    return poFeature;
}

/************************************************************************/
/*                    OGRPGIsKnownGeomFuncPrefix()                      */
/************************************************************************/

static const char* papszKnownGeomFuncPrefixes[] = {
    "ST_AsBinary", "BinaryBase64", "ST_AsEWKT", "ST_AsEWKB", "EWKBBase64",
    "ST_AsText", "AsBinary", "asEWKT", "asEWKB", "asText" };
static int OGRPGIsKnownGeomFuncPrefix(const char* pszFieldName)
{
    for(size_t i=0; i<sizeof(papszKnownGeomFuncPrefixes) / sizeof(char*); i++)
    {
        if( EQUALN(pszFieldName, papszKnownGeomFuncPrefixes[i],
                   strlen(papszKnownGeomFuncPrefixes[i])) )
            return i;
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
void OGRPGLayer::CreateMapFromFieldNameToIndex()
{
    CPLFree(panMapFieldNameToIndex);
    panMapFieldNameToIndex = NULL;
    CPLFree(panMapFieldNameToGeomIndex);
    panMapFieldNameToGeomIndex = NULL;
    if ( PQresultStatus(hCursorResult)  == PGRES_TUPLES_OK )
    {
        panMapFieldNameToIndex =
                (int*)CPLMalloc(sizeof(int) * PQnfields(hCursorResult));
        panMapFieldNameToGeomIndex =
                (int*)CPLMalloc(sizeof(int) * PQnfields(hCursorResult));
        for( int iField = 0;
            iField < PQnfields(hCursorResult);
            iField++ )
        {
            const char* pszName = PQfname(hCursorResult,iField);
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
                        pszName[ strlen(papszKnownGeomFuncPrefixes[iKnownPrefix]) ] == '_' )
                    {
                        panMapFieldNameToGeomIndex[iField] =
                            poFeatureDefn->GetGeomFieldIndex(pszName +
                            strlen(papszKnownGeomFuncPrefixes[iKnownPrefix]) + 1);
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

    poDS->FlushSoftTransaction();
    poDS->SoftStartTransaction();

    if ( poDS->bUseBinaryCursor && bCanUseBinaryCursor )
        osCommand.Printf( "DECLARE %s BINARY CURSOR for %s",
                            pszCursorName, pszQueryStatement );
    else
        osCommand.Printf( "DECLARE %s CURSOR for %s",
                            pszCursorName, pszQueryStatement );

    hCursorResult = OGRPG_PQexec(hPGConn, osCommand );
    if ( !hCursorResult || PQresultStatus(hCursorResult) != PGRES_COMMAND_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", PQerrorMessage( hPGConn ) );
    }
    OGRPGClearResult( hCursorResult );

    osCommand.Printf( "FETCH %d in %s", CURSOR_PAGE, pszCursorName );
    hCursorResult = OGRPG_PQexec(hPGConn, osCommand );

    CreateMapFromFieldNameToIndex();

    nResultOffset = 0;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRPGLayer::GetNextRawFeature()

{
    PGconn      *hPGConn = poDS->GetPGConn();
    CPLString   osCommand;

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
    if( (PQntuples(hCursorResult) == 1 || PQntuples(hCursorResult) == CURSOR_PAGE) &&
        nResultOffset == PQntuples(hCursorResult) )
    {
        OGRPGClearResult( hCursorResult );
        
        osCommand.Printf( "FETCH %d in %s", CURSOR_PAGE, pszCursorName );
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
    OGRFeature *poFeature = RecordToFeature( nResultOffset );

    nResultOffset++;
    iNextShapeId++;

    return poFeature;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRPGLayer::SetNextByIndex( long nIndex )

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
    
    osCommand.Printf( "FETCH ABSOLUTE %ld in %s", nIndex+1, pszCursorName );
    hCursorResult = OGRPG_PQexec(hPGConn, osCommand );

    if (PQresultStatus(hCursorResult) != PGRES_TUPLES_OK ||
        PQntuples(hCursorResult) != 1)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read feature at invalid index (%ld).", nIndex );

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
    GByte* pabyData;
    int iSrc=0, iDst=0;

    if( pszBytea == NULL )
    {
        if (pnLength) *pnLength = 0;
        return NULL;
    }

    /* hex bytea data (PostgreSQL >= 9.0) */
    if (pszBytea[0] == '\\' && pszBytea[1] == 'x')
        return CPLHexToBinary(pszBytea + 2, pnLength);

    pabyData = (GByte *) CPLMalloc(strlen(pszBytea));

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

OGRGeometry *OGRPGLayer::BYTEAToGeometry( const char *pszBytea )

{
    GByte       *pabyWKB;
    int nLen=0;
    OGRGeometry *poGeometry;

    if( pszBytea == NULL )
        return NULL;

    pabyWKB = BYTEAToGByteArray(pszBytea, &nLen);

    poGeometry = NULL;
    OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeometry, nLen );

    CPLFree( pabyWKB );
    return poGeometry;
}


/************************************************************************/
/*                        GByteArrayToBYTEA()                           */
/************************************************************************/

char* OGRPGLayer::GByteArrayToBYTEA( const GByte* pabyData, int nLen)
{
    char* pszTextBuf;

    pszTextBuf = (char *) CPLMalloc(nLen*5+1);

    int  iSrc, iDst=0;

    for( iSrc = 0; iSrc < nLen; iSrc++ )
    {
        if( pabyData[iSrc] < 40 || pabyData[iSrc] > 126
            || pabyData[iSrc] == '\\' )
        {
            sprintf( pszTextBuf+iDst, "\\\\%03o", pabyData[iSrc] );
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

char *OGRPGLayer::GeometryToBYTEA( OGRGeometry * poGeometry )

{
    int         nWkbSize = poGeometry->WkbSize();
    GByte       *pabyWKB;
    char        *pszTextBuf;

    pabyWKB = (GByte *) CPLMalloc(nWkbSize);
    if( poGeometry->exportToWkb( wkbNDR, pabyWKB ) != OGRERR_NONE )
    {
        CPLFree(pabyWKB);
        return CPLStrdup("");
    }

    pszTextBuf = GByteArrayToBYTEA( pabyWKB, nWkbSize );
    CPLFree(pabyWKB);

    return pszTextBuf;
}

/************************************************************************/
/*                          OIDToGeometry()                             */
/************************************************************************/

OGRGeometry *OGRPGLayer::OIDToGeometry( Oid oid )

{
    PGconn      *hPGConn = poDS->GetPGConn();
    GByte       *pabyWKB;
    int         fd, nBytes;
    OGRGeometry *poGeometry;

#define MAX_WKB 500000

    if( oid == 0 )
        return NULL;

    fd = lo_open( hPGConn, oid, INV_READ );
    if( fd < 0 )
        return NULL;

    pabyWKB = (GByte *) CPLMalloc(MAX_WKB);
    nBytes = lo_read( hPGConn, fd, (char *) pabyWKB, MAX_WKB );
    lo_close( hPGConn, fd );

    poGeometry = NULL;
    OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeometry, nBytes );

    CPLFree( pabyWKB );

    return poGeometry;
}

/************************************************************************/
/*                           GeometryToOID()                            */
/************************************************************************/

Oid OGRPGLayer::GeometryToOID( OGRGeometry * poGeometry )

{
    PGconn      *hPGConn = poDS->GetPGConn();
    int         nWkbSize = poGeometry->WkbSize();
    GByte       *pabyWKB;
    Oid         oid;
    int         fd, nBytesWritten;

    pabyWKB = (GByte *) CPLMalloc(nWkbSize);
    if( poGeometry->exportToWkb( wkbNDR, pabyWKB ) != OGRERR_NONE )
        return 0;

    oid = lo_creat( hPGConn, INV_READ|INV_WRITE );

    fd = lo_open( hPGConn, oid, INV_WRITE );
    nBytesWritten = lo_write( hPGConn, fd, (char *) pabyWKB, nWkbSize );
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
    return poDS->SoftStartTransaction();
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRPGLayer::CommitTransaction()

{
    return poDS->SoftCommit();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRPGLayer::RollbackTransaction()

{
    return poDS->SoftRollback();
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

    const char* pszExtentFct;
    if (poDS->sPostGISVersion.nMajor >= 2)
        pszExtentFct = "ST_Extent";
    else
        pszExtentFct = "Extent";

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

    if( osCommand.size() != 0 )
    {
        if( RunGetExtentRequest(psExtent, bForce, osCommand) == OGRERR_NONE )
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
                                        CPLString osCommand)
{
    if ( psExtent == NULL )
        return OGRERR_FAILURE;

    PGconn      *hPGConn = poDS->GetPGConn();
    PGresult    *hResult = NULL;

    hResult = OGRPG_PQexec( hPGConn, osCommand );
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
    // For PostGis ver >= 1.0.0 -> Tokens: X1 Y1 X2 Y2 (nTokenCnt = 4)
    // For PostGIS ver < 1.0.0 -> Tokens: X1 Y1 Z1 X2 Y2 Z2 (nTokenCnt = 6)
    // =>   X2 index calculated as nTokenCnt/2
    //      Y2 index caluclated as nTokenCnt/2+1
    
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
            if (bHasFid)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "More than one ogc_fid column was found in the result of the SQL request. Only last one will be used");
            }
            bHasFid = TRUE;
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
                    papszKnownGeomFuncPrefixes[iGeomFuncPrefix])] == '_' )
            {
                poGeomFieldDefn->SetName( oField.GetNameRef() +
                    strlen(papszKnownGeomFuncPrefixes[iGeomFuncPrefix]) + 1 );
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
            oField.SetWidth( 1 );
        }
        else if (nTypeOID == INT2OID )
        {
            oField.SetType( OFTInteger );
            oField.SetWidth( 5 );
        }
        else if (nTypeOID == INT4OID )
        {
            oField.SetType( OFTInteger );
        }
        else if ( nTypeOID == INT8OID )
        {
            /* FIXME: OFTInteger can not handle 64bit integers */
            oField.SetType( OFTInteger );
        }
        else if( nTypeOID == FLOAT4OID ||
                 nTypeOID == FLOAT8OID )
        {
            oField.SetType( OFTReal );
        }
        else if( nTypeOID == NUMERICOID )
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
                    oField.SetType( OFTInteger );
                    oField.SetWidth( nWidth );
                }
                else
                {
                    oField.SetType( OFTReal );
                    oField.SetWidth( nWidth );
                    oField.SetPrecision( nPrecision );
                }
            }
            else
                oField.SetType( OFTReal );
        }
        else if ( nTypeOID == INT4ARRAYOID )
        {
            oField.SetType ( OFTIntegerList );
        }
        else if ( nTypeOID == FLOAT4ARRAYOID ||
                  nTypeOID == FLOAT8ARRAYOID )
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
            /* We can't deserialize properly timestamp with time zone */
            /* with binary cursors */
            if (nTypeOID == TIMESTAMPTZOID)
                bCanUseBinaryCursor = FALSE;

            oField.SetType( OFTDateTime );
        }
        else /* unknown type */
        {
            CPLDebug("PG", "Unhandled OID (%d) for column %d. Defaulting to String.", nTypeOID, iRawField);
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

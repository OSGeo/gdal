/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  Implementation of DTEDCreate() portion of DTED API.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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

#include "dted_api.h"
#include <assert.h>

CPL_CVSID("$Id$")

#define DTED_ABS_VERT_ACC "NA  "
#define DTED_SECURITY     "U"
#define DTED_EDITION      1

/************************************************************************/
/*                           DTEDFormatDMS()                            */
/************************************************************************/

static void DTEDFormatDMS( unsigned char *achField,
                           size_t nTargetLenSize,
                           size_t nOffset,
                           double dfAngle,
                           const char *pszLatLong, const char *pszFormat )

{
    char        chHemisphere;
    int         nDegrees, nMinutes, nSeconds;
    double      dfRemainder;

    if( pszFormat == NULL )
        pszFormat = "%03d%02d%02d%c";

    assert( EQUAL(pszLatLong,"LAT") || EQUAL(pszLatLong,"LONG") );

    if( EQUAL(pszLatLong,"LAT") )
    {
        if( dfAngle < 0.0 )
            chHemisphere = 'S';
        else
            chHemisphere = 'N';
    }
    else
    {
        if( dfAngle < 0.0 )
            chHemisphere = 'W';
        else
            chHemisphere = 'E';
    }

    dfAngle = ABS(dfAngle);

    nDegrees = (int) floor(dfAngle + 0.5/3600.0);
    dfRemainder = dfAngle - nDegrees;
    nMinutes = (int) floor(dfRemainder*60.0 + 0.5/60.0);
    dfRemainder = dfRemainder - nMinutes / 60.0;
    nSeconds = (int) floor(dfRemainder * 3600.0 + 0.5);

    snprintf( (char*)achField + nOffset, nTargetLenSize - nOffset,
              pszFormat,
              nDegrees, nMinutes, nSeconds, chHemisphere );
}

/************************************************************************/
/*                             DTEDFormat()                             */
/************************************************************************/

static void DTEDFormat( unsigned char *pszTarget,
                        size_t nTargetLenSize,
                        size_t nOffset,
                        const char *pszFormat, ... )
                                                CPL_PRINT_FUNC_FORMAT (4, 5);

static void DTEDFormat( unsigned char *pszTarget,
                        size_t nTargetLenSize,
                        size_t nOffset,
                        const char *pszFormat, ... )

{
    va_list args;

    va_start(args, pszFormat);
    CPLvsnprintf( (char*)pszTarget + nOffset, nTargetLenSize - nOffset,
                  pszFormat, args );
    va_end(args);
}

/************************************************************************/
/*                             DTEDCreate()                             */
/************************************************************************/

const char *DTEDCreate( const char *pszFilename, int nLevel,
                        int nLLOriginLat, int nLLOriginLong )

{
    VSILFILE     *fp;
    unsigned char achRecord[3601*2 + 12];
    int         nXSize, nYSize, nReferenceLat, iProfile;

/* -------------------------------------------------------------------- */
/*      Establish resolution.                                           */
/* -------------------------------------------------------------------- */
    if( nLevel == 0 )
    {
        nXSize = 121;
        nYSize = 121;
    }
    else if( nLevel == 1 )
    {
        nXSize = 1201;
        nYSize = 1201;
    }
    else if( nLevel == 2 )
    {
        nXSize = 3601;
        nYSize = 3601;
    }
    else
    {
        return CPLSPrintf( "Illegal DTED Level value %d, only 0-2 allowed.",
                 nLevel );
    }

    nReferenceLat = nLLOriginLat < 0 ? - (nLLOriginLat + 1) : nLLOriginLat;

    if( nReferenceLat >= 80 )
        nXSize = (nXSize - 1) / 6 + 1;
    else if( nReferenceLat >= 75 )
        nXSize = (nXSize - 1) / 4 + 1;
    else if( nReferenceLat >= 70 )
        nXSize = (nXSize - 1) / 3 + 1;
    else if( nReferenceLat >= 50 )
        nXSize = (nXSize - 1) / 2 + 1;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        return CPLSPrintf( "Unable to create file `%s'.", pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Format and write the UHL record.                                */
/* -------------------------------------------------------------------- */
    memset( achRecord, ' ', DTED_UHL_SIZE );

    DTEDFormat( achRecord, sizeof(achRecord), 0, "UHL1" );

    DTEDFormatDMS( achRecord, sizeof(achRecord), 4, nLLOriginLong, "LONG", NULL );
    DTEDFormatDMS( achRecord, sizeof(achRecord), 12, nLLOriginLat, "LAT", NULL );

    DTEDFormat( achRecord, sizeof(achRecord), 20,
                "%04d", (3600 / (nXSize-1)) * 10 );
    DTEDFormat( achRecord, sizeof(achRecord), 24,
                "%04d", (3600 / (nYSize-1)) * 10 );

    DTEDFormat( achRecord, sizeof(achRecord), 28, "%4s", DTED_ABS_VERT_ACC );
    DTEDFormat( achRecord, sizeof(achRecord), 32, "%-3s", DTED_SECURITY );
    DTEDFormat( achRecord, sizeof(achRecord), 47, "%04d", nXSize );
    DTEDFormat( achRecord, sizeof(achRecord), 51, "%04d", nYSize );
    DTEDFormat( achRecord, sizeof(achRecord), 55, "%c", '0' );

    if( VSIFWriteL( achRecord, DTED_UHL_SIZE, 1, fp ) != 1 )
        return "UHL record write failed.";

/* -------------------------------------------------------------------- */
/*      Format and write the DSI record.                                */
/* -------------------------------------------------------------------- */
    memset( achRecord, ' ', DTED_DSI_SIZE );

    DTEDFormat( achRecord, sizeof(achRecord), 0, "DSI" );
    DTEDFormat( achRecord, sizeof(achRecord), 3, "%1s", DTED_SECURITY );

    DTEDFormat( achRecord, sizeof(achRecord), 59, "DTED%d", nLevel );
    DTEDFormat( achRecord, sizeof(achRecord), 64, "%015d", 0 );
    DTEDFormat( achRecord, sizeof(achRecord), 87, "%02d", DTED_EDITION );
    DTEDFormat( achRecord, sizeof(achRecord), 89, "%c", 'A' );
    DTEDFormat( achRecord, sizeof(achRecord), 90, "%04d", 0 );
    DTEDFormat( achRecord, sizeof(achRecord), 94, "%04d", 0 );
    DTEDFormat( achRecord, sizeof(achRecord), 98, "%04d", 0 );
    DTEDFormat( achRecord, sizeof(achRecord), 126, "PRF89020B");
    DTEDFormat( achRecord, sizeof(achRecord), 135, "00");
    DTEDFormat( achRecord, sizeof(achRecord), 137, "0005");
    DTEDFormat( achRecord, sizeof(achRecord), 141, "MSL" );
    DTEDFormat( achRecord, sizeof(achRecord), 144, "WGS84" );

    /* origin */
    DTEDFormatDMS( achRecord, sizeof(achRecord), 185, nLLOriginLat, "LAT",
                   "%02d%02d%02d.0%c" );
    DTEDFormatDMS( achRecord, sizeof(achRecord), 194, nLLOriginLong, "LONG",
                   "%03d%02d%02d.0%c" );

    /* SW */
    DTEDFormatDMS( achRecord, sizeof(achRecord), 204,
                   nLLOriginLat, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord, sizeof(achRecord), 211,
                   nLLOriginLong, "LONG", NULL );

    /* NW */
    DTEDFormatDMS( achRecord, sizeof(achRecord), 219,
                   nLLOriginLat+1, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord, sizeof(achRecord), 226,
                   nLLOriginLong, "LONG", NULL );

    /* NE */
    DTEDFormatDMS( achRecord, sizeof(achRecord), 234,
                   nLLOriginLat+1, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord, sizeof(achRecord), 241,
                   nLLOriginLong+1, "LONG", NULL );

    /* SE */
    DTEDFormatDMS( achRecord, sizeof(achRecord), 249,
                   nLLOriginLat, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord, sizeof(achRecord), 256,
                   nLLOriginLong+1, "LONG", NULL );

    DTEDFormat( achRecord, sizeof(achRecord), 264, "0000000.0" );
    DTEDFormat( achRecord, sizeof(achRecord), 264, "0000000.0" );

    DTEDFormat( achRecord, sizeof(achRecord), 273,
                "%04d", (3600 / (nYSize-1)) * 10 );
    DTEDFormat( achRecord, sizeof(achRecord), 277,
                "%04d", (3600 / (nXSize-1)) * 10 );

    DTEDFormat( achRecord, sizeof(achRecord), 281, "%04d", nYSize );
    DTEDFormat( achRecord, sizeof(achRecord), 285, "%04d", nXSize );
    DTEDFormat( achRecord, sizeof(achRecord), 289, "%02d", 0 );

    if( VSIFWriteL( achRecord, DTED_DSI_SIZE, 1, fp ) != 1 )
        return "DSI record write failed.";

/* -------------------------------------------------------------------- */
/*      Create and write ACC record.                                    */
/* -------------------------------------------------------------------- */
    memset( achRecord, ' ', DTED_ACC_SIZE );

    DTEDFormat( achRecord, sizeof(achRecord), 0, "ACC" );

    DTEDFormat( achRecord, sizeof(achRecord), 3, "NA" );
    DTEDFormat( achRecord, sizeof(achRecord), 7, "NA" );
    DTEDFormat( achRecord, sizeof(achRecord), 11, "NA" );
    DTEDFormat( achRecord, sizeof(achRecord), 15, "NA" );

    DTEDFormat( achRecord, sizeof(achRecord), 55, "00" );

    if( VSIFWriteL( achRecord, DTED_ACC_SIZE, 1, fp ) != 1 )
        return "ACC record write failed.";

/* -------------------------------------------------------------------- */
/*      Write blank template profile data records.                      */
/* -------------------------------------------------------------------- */
    memset( achRecord, 0, nYSize*2 + 12 );
    memset( achRecord + 8, 0xff, nYSize*2 );

    achRecord[0] = 0252;

    for( iProfile = 0; iProfile < nXSize; iProfile++ )
    {
        achRecord[1] = 0;
        achRecord[2] = (GByte) (iProfile / 256);
        achRecord[3] = (GByte) (iProfile % 256);

        achRecord[4] = (GByte) (iProfile / 256);
        achRecord[5] = (GByte) (iProfile % 256);

        if( VSIFWriteL( achRecord, nYSize*2 + 12, 1, fp ) != 1 )
            return "Data record write failed.";
    }

    if( VSIFCloseL( fp ) != 0 )
        return "I/O error";

    return NULL;
}

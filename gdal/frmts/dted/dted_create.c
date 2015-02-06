/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  Implementation of DTEDCreate() portion of DTED API.
 * Author:   Frank Warmerdam, warmerdamm@pobox.com
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

CPL_CVSID("$Id$");

#define DTED_ABS_VERT_ACC "NA  "
#define DTED_SECURITY     "U"
#define DTED_EDITION      1

/************************************************************************/
/*                           DTEDFormatDMS()                            */
/************************************************************************/

static void DTEDFormatDMS( unsigned char *achField, double dfAngle, 
                           const char *pszLatLong, const char *pszFormat )

{
    char        chHemisphere;
    char        szWork[128];
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

    sprintf( szWork, pszFormat,
             nDegrees, nMinutes, nSeconds, chHemisphere );

    strncpy( (char *) achField, szWork, strlen(szWork) );
}

/************************************************************************/
/*                             DTEDFormat()                             */
/************************************************************************/

static void DTEDFormat( unsigned char *pszTarget, const char *pszFormat, ... ) CPL_PRINT_FUNC_FORMAT (2, 3);

static void DTEDFormat( unsigned char *pszTarget, const char *pszFormat, ... )

{
    va_list args;
    char    szWork[512];

    va_start(args, pszFormat);
    vsprintf( szWork, pszFormat, args );
    va_end(args);

    strncpy( (char *) pszTarget, szWork, strlen(szWork) );
}

/************************************************************************/
/*                             DTEDCreate()                             */
/************************************************************************/

const char *DTEDCreate( const char *pszFilename, int nLevel, 
                        int nLLOriginLat, int nLLOriginLong )

{
    VSILFILE     *fp;
    unsigned char achRecord[3601*2 + 12];
    int         nXSize, nYSize, iProfile;
    static char szError[512];

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
        sprintf( szError, "Illegal DTED Level value %d, only 0-2 allowed.",
                 nLevel );
        return szError;
    }

    if( ABS(nLLOriginLat) >= 80 )
        nXSize = (nXSize - 1) / 6 + 1;
    else if( ABS(nLLOriginLat) >= 75 )
        nXSize = (nXSize - 1) / 4 + 1;
    else if( ABS(nLLOriginLat) >= 70 )
        nXSize = (nXSize - 1) / 3 + 1;
    else if( ABS(nLLOriginLat) >= 50 )
        nXSize = (nXSize - 1) / 2 + 1;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        sprintf( szError, "Unable to create file `%s'.", pszFilename );
        return szError;
    }

/* -------------------------------------------------------------------- */
/*      Format and write the UHL record.                                */
/* -------------------------------------------------------------------- */
    memset( achRecord, ' ', DTED_UHL_SIZE );

    DTEDFormat( achRecord + 0, "UHL1" );
    
    DTEDFormatDMS( achRecord + 4, nLLOriginLong, "LONG", NULL );
    DTEDFormatDMS( achRecord + 12, nLLOriginLat, "LAT", NULL );

    DTEDFormat( achRecord + 20, "%04d", (3600 / (nXSize-1)) * 10 );
    DTEDFormat( achRecord + 24, "%04d", (3600 / (nYSize-1)) * 10 );

    DTEDFormat( achRecord + 28, "%4s", DTED_ABS_VERT_ACC );
    DTEDFormat( achRecord + 32, "%-3s", DTED_SECURITY );
    DTEDFormat( achRecord + 47, "%04d", nXSize );
    DTEDFormat( achRecord + 51, "%04d", nYSize );
    DTEDFormat( achRecord + 55, "%c", '0' );

    if( VSIFWriteL( achRecord, DTED_UHL_SIZE, 1, fp ) != 1 )
        return "UHL record write failed.";

/* -------------------------------------------------------------------- */
/*      Format and write the DSI record.                                */
/* -------------------------------------------------------------------- */
    memset( achRecord, ' ', DTED_DSI_SIZE );

    DTEDFormat( achRecord + 0, "DSI" );
    DTEDFormat( achRecord + 3, "%1s", DTED_SECURITY );

    DTEDFormat( achRecord + 59, "DTED%d", nLevel );
    DTEDFormat( achRecord + 64, "%015d", 0 );
    DTEDFormat( achRecord + 87, "%02d", DTED_EDITION );
    DTEDFormat( achRecord + 89, "%c", 'A' );
    DTEDFormat( achRecord + 90, "%04d", 0 );
    DTEDFormat( achRecord + 94, "%04d", 0 );
    DTEDFormat( achRecord + 98, "%04d", 0 );
    DTEDFormat( achRecord + 126, "PRF89020B");
    DTEDFormat( achRecord + 135, "00");
    DTEDFormat( achRecord + 137, "0005");
    DTEDFormat( achRecord + 141, "MSL" );
    DTEDFormat( achRecord + 144, "WGS84" );

    /* origin */
    DTEDFormatDMS( achRecord + 185, nLLOriginLat, "LAT", 
                   "%02d%02d%02d.0%c" );
    DTEDFormatDMS( achRecord + 194, nLLOriginLong, "LONG", 
                   "%03d%02d%02d.0%c" );

    /* SW */
    DTEDFormatDMS( achRecord + 204, nLLOriginLat, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord + 211, nLLOriginLong, "LONG", NULL );

    /* NW */
    DTEDFormatDMS( achRecord + 219, nLLOriginLat+1, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord + 226, nLLOriginLong, "LONG", NULL );

    /* NE */
    DTEDFormatDMS( achRecord + 234, nLLOriginLat+1, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord + 241, nLLOriginLong+1, "LONG", NULL );

    /* SE */
    DTEDFormatDMS( achRecord + 249, nLLOriginLat, "LAT", "%02d%02d%02d%c" );
    DTEDFormatDMS( achRecord + 256, nLLOriginLong+1, "LONG", NULL );

    DTEDFormat( achRecord + 264, "0000000.0" );
    DTEDFormat( achRecord + 264, "0000000.0" );

    DTEDFormat( achRecord + 273, "%04d", (3600 / (nYSize-1)) * 10 );
    DTEDFormat( achRecord + 277, "%04d", (3600 / (nXSize-1)) * 10 );

    DTEDFormat( achRecord + 281, "%04d", nYSize );
    DTEDFormat( achRecord + 285, "%04d", nXSize );
    DTEDFormat( achRecord + 289, "%02d", 0 );

    if( VSIFWriteL( achRecord, DTED_DSI_SIZE, 1, fp ) != 1 )
        return "DSI record write failed.";

/* -------------------------------------------------------------------- */
/*      Create and write ACC record.                                    */
/* -------------------------------------------------------------------- */
    memset( achRecord, ' ', DTED_ACC_SIZE );

    DTEDFormat( achRecord + 0, "ACC" );

    DTEDFormat( achRecord + 3, "NA" );
    DTEDFormat( achRecord + 7, "NA" );
    DTEDFormat( achRecord + 11, "NA" );
    DTEDFormat( achRecord + 15, "NA" );

    DTEDFormat( achRecord + 55, "00" );
    
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

    VSIFCloseL( fp );

    return NULL;
}

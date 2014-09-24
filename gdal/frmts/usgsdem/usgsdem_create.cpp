/******************************************************************************
 * $Id$
 *
 * Project:  USGS DEM Driver
 * Purpose:  CreateCopy() implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * This writing code based on the format specification: 
 *   Canadian Digital Elevation Data Product Specification - Edition 2.0
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "gdalwarper.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

typedef struct 
{
    GDALDataset *poSrcDS;
    char        *pszFilename;
    int         nXSize, nYSize;

    char       *pszDstSRS;

    double      dfLLX, dfLLY;  // These are adjusted in to center of 
    double      dfULX, dfULY;  // corner pixels, and in decimal degrees.
    double      dfURX, dfURY;
    double      dfLRX, dfLRY;

    int         utmzone;
    char        horizdatum[2];

    double      dfHorizStepSize;
    double      dfVertStepSize;
    double      dfElevStepSize;

    char 	**papszOptions;
    int         bStrict;

    VSILFILE  *fp;

    GInt16     *panData;
    
} USGSDEMWriteInfo;

#define DEM_NODATA -32767

/************************************************************************/
/*                        USGSDEMWriteCleanup()                         */
/************************************************************************/

static void USGSDEMWriteCleanup( USGSDEMWriteInfo *psWInfo )

{
    CSLDestroy( psWInfo->papszOptions );
    CPLFree( psWInfo->pszDstSRS );
    CPLFree( psWInfo->pszFilename );
    if( psWInfo->fp != NULL )
        VSIFCloseL( psWInfo->fp );
    if( psWInfo->panData != NULL )
        VSIFree( psWInfo->panData );
}

/************************************************************************/
/*                       USGSDEMDectoPackedDMS()                        */
/************************************************************************/
const char *USGSDEMDecToPackedDMS( double dfDec )
{
    double  dfSeconds;
    int nDegrees, nMinutes, nSign;
    static char szPackBuf[100];

    nSign = ( dfDec < 0.0 )? -1 : 1;

    dfDec = ABS( dfDec );
    /* If the difference between the value and the nearest degree
       is less than 1e-5 second, then we force to round to the
       nearest degree, to avoid result strings like '40 59 60.0000' instead of '41'.
       This is of general interest, but was mainly done to workaround a strange
       Valgrind bug when running usgsdem_6 where the value of psDInfo->dfULCornerY
       computed in DTEDOpen() differ between Valgrind and non-Valgrind executions.
    */
    if (fabs(dfDec - (int) floor( dfDec + .5)) < 1e-5 / 3600)
        dfDec = nDegrees = (int) floor( dfDec + .5);
    else
        nDegrees = (int) floor( dfDec );
    nMinutes = (int) floor( ( dfDec - nDegrees ) * 60.0 );
    dfSeconds = (dfDec - nDegrees) * 3600.0 - nMinutes * 60.0;

    sprintf( szPackBuf, "%4d%2d%7.4f", 
             nSign * nDegrees, nMinutes, dfSeconds );
    return szPackBuf;
}

/************************************************************************/
/*                              TextFill()                              */
/************************************************************************/

static void TextFill( char *pszTarget, unsigned int nMaxChars, 
                      const char *pszSrc )

{
    if( strlen(pszSrc) < nMaxChars )
    {
        memcpy( pszTarget, pszSrc, strlen(pszSrc) );
        memset( pszTarget + strlen(pszSrc), ' ', nMaxChars - strlen(pszSrc));
    }
    else
    {
        memcpy( pszTarget, pszSrc, nMaxChars );
    }
}

/************************************************************************/
/*                             TextFillR()                              */
/*                                                                      */
/*      Right justified.                                                */
/************************************************************************/

static void TextFillR( char *pszTarget, unsigned int nMaxChars, 
                       const char *pszSrc )

{
    if( strlen(pszSrc) < nMaxChars )
    {
        memset( pszTarget, ' ', nMaxChars - strlen(pszSrc) );
        memcpy( pszTarget + nMaxChars - strlen(pszSrc), pszSrc, 
                strlen(pszSrc) );
    }
    else
        memcpy( pszTarget, pszSrc, nMaxChars );
}

/************************************************************************/
/*                         USGSDEMPrintDouble()                         */
/*                                                                      */
/*      The MSVC C runtime library uses 3 digits                        */
/*      for the exponent.  This causes various problems, so we try      */
/*      to correct it here.                                             */
/************************************************************************/

#if defined(_MSC_VER) || defined(__MSVCRT__)
#  define MSVC_HACK
#endif

static void USGSDEMPrintDouble( char *pszBuffer, double dfValue )

{
#define DOUBLE_BUFFER_SIZE 64

    char    szTemp[DOUBLE_BUFFER_SIZE];
    int     i;
#ifdef MSVC_HACK
    const char *pszFormat = "%25.15e";
#else
    const char *pszFormat = "%24.15e";
#endif

    if ( !pszBuffer )
        return;

#if defined(HAVE_SNPRINTF)
    snprintf( szTemp, DOUBLE_BUFFER_SIZE, pszFormat, dfValue );
#else
    sprintf( szTemp, pszFormat, dfValue );
#endif
    szTemp[DOUBLE_BUFFER_SIZE - 1] = '\0';

    for( i = 0; szTemp[i] != '\0'; i++ )
    {
        if( szTemp[i] == 'E' || szTemp[i] == 'e' )
            szTemp[i] = 'D';
#ifdef MSVC_HACK
        if( (szTemp[i] == '+' || szTemp[i] == '-')
            && szTemp[i+1] == '0' && isdigit(szTemp[i+2]) 
            && isdigit(szTemp[i+3]) && szTemp[i+4] == '\0' )
        {
            memmove( szTemp+i+1, szTemp+i+2, 2 );
            szTemp[i+3] = '\0';
            break;
        }
#endif
    }

    TextFillR( pszBuffer, 24, szTemp );
}

/************************************************************************/
/*                         USGSDEMPrintSingle()                         */
/*                                                                      */
/*      The MSVC C runtime library uses 3 digits                        */
/*      for the exponent.  This causes various problems, so we try      */
/*      to correct it here.                                             */
/************************************************************************/

static void USGSDEMPrintSingle( char *pszBuffer, double dfValue )

{
#define DOUBLE_BUFFER_SIZE 64

    char    szTemp[DOUBLE_BUFFER_SIZE];
    int     i;
#ifdef MSVC_HACK
    const char *pszFormat = "%13.6e";
#else
    const char *pszFormat = "%12.6e";
#endif

    if ( !pszBuffer )
        return;

#if defined(HAVE_SNPRINTF)
    snprintf( szTemp, DOUBLE_BUFFER_SIZE, pszFormat, dfValue );
#else
    sprintf( szTemp, pszFormat, dfValue );
#endif
    szTemp[DOUBLE_BUFFER_SIZE - 1] = '\0';

    for( i = 0; szTemp[i] != '\0'; i++ )
    {
        if( szTemp[i] == 'E' || szTemp[i] == 'e' )
            szTemp[i] = 'D';
#ifdef MSVC_HACK
        if( (szTemp[i] == '+' || szTemp[i] == '-')
            && szTemp[i+1] == '0' && isdigit(szTemp[i+2]) 
            && isdigit(szTemp[i+3]) && szTemp[i+4] == '\0' )
        {
            memmove( szTemp+i+1, szTemp+i+2, 2 );
            szTemp[i+3] = '\0';
            break;
        }
#endif
    }

    TextFillR( pszBuffer, 12, szTemp );
}

/************************************************************************/
/*                        USGSDEMWriteARecord()                         */
/************************************************************************/

static int USGSDEMWriteARecord( USGSDEMWriteInfo *psWInfo )

{
    char achARec[1024];
    int  i;
    const char *pszOption;

/* -------------------------------------------------------------------- */
/*      Init to blanks.                                                 */
/* -------------------------------------------------------------------- */
    memset( achARec, ' ', sizeof(achARec) );

/* -------------------------------------------------------------------- */
/*      Load template file, if one is indicated.                        */
/* -------------------------------------------------------------------- */
    const char *pszTemplate = 
        CSLFetchNameValue( psWInfo->papszOptions, "TEMPLATE" );
    if( pszTemplate != NULL )
    {
        VSILFILE *fpTemplate;

        fpTemplate = VSIFOpenL( pszTemplate, "rb" );
        if( fpTemplate == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open template file '%s'.\n%s", 
                      pszTemplate, VSIStrerror( errno ) );
            return FALSE;
        }

        if( VSIFReadL( achARec, 1, 1024, fpTemplate ) != 1024 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read 1024 byte A Record from template file '%s'.\n%s",
                      pszTemplate, VSIStrerror( errno ) );
            return FALSE;
        }
        VSIFCloseL( fpTemplate );
    }
    
/* -------------------------------------------------------------------- */
/*      Filename (right justify)                                        */
/* -------------------------------------------------------------------- */
    TextFillR( achARec +   0, 40, CPLGetFilename( psWInfo->pszFilename ) );

/* -------------------------------------------------------------------- */
/*      Producer                                                        */
/* -------------------------------------------------------------------- */
    pszOption = CSLFetchNameValue( psWInfo->papszOptions, "PRODUCER" );

    if( pszOption != NULL )
        TextFillR( achARec +  40, 60, pszOption );

    else if( pszTemplate == NULL )
        TextFill( achARec +  40, 60, "" );

/* -------------------------------------------------------------------- */
/*      Filler                                                          */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 100, 9, "" );

/* -------------------------------------------------------------------- */
/*      SW Geographic Corner - SDDDMMSS.SSSS - longitude then latitude  */
/* -------------------------------------------------------------------- */
    if ( ! psWInfo->utmzone )
    {
        TextFill( achARec + 109, 13, 
            USGSDEMDecToPackedDMS( psWInfo->dfLLX ) ); // longitude
        TextFill( achARec + 122, 13, 
            USGSDEMDecToPackedDMS( psWInfo->dfLLY ) ); // latitude
    }
    /* this may not be best according to the spec.  But for now,
     * we won't try to convert the UTM coordinates to lat/lon
     */

/* -------------------------------------------------------------------- */
/*      Process code.                                                   */
/* -------------------------------------------------------------------- */
    pszOption = CSLFetchNameValue( psWInfo->papszOptions, "ProcessCode" );

    if( pszOption != NULL )
        TextFill( achARec + 135, 1, pszOption );

    else if( pszTemplate == NULL )
        TextFill( achARec + 135, 1, " " ); 

/* -------------------------------------------------------------------- */
/*      Filler                                                          */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 136, 1, "" );

/* -------------------------------------------------------------------- */
/*      Sectional indicator                                             */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec + 137, 3, "" );
    
/* -------------------------------------------------------------------- */
/*      Origin code                                                     */
/* -------------------------------------------------------------------- */
    pszOption = CSLFetchNameValue( psWInfo->papszOptions, "OriginCode" );

    if( pszOption != NULL )
        TextFill( achARec + 140, 4, pszOption );  // Should be YT for Yukon.

    else if( pszTemplate == NULL )
        TextFill( achARec + 140, 4, "" );

/* -------------------------------------------------------------------- */
/*      DEM level code (right justify)                                  */
/* -------------------------------------------------------------------- */
    pszOption = CSLFetchNameValue( psWInfo->papszOptions, "DEMLevelCode" );

    if( pszOption != NULL )
        TextFillR( achARec + 144, 6, pszOption );  // 1, 2 or 3.
        
    else if( pszTemplate == NULL )
        TextFillR( achARec + 144, 6, "1" );  // 1, 2 or 3.
        /* some DEM readers require a value, 1 seems to be a
         * default
         */
    
/* -------------------------------------------------------------------- */
/*      Elevation Pattern                                               */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 150, 6, "1" );  // "1" for regular (random is 2)
    
/* -------------------------------------------------------------------- */
/*      Horizontal Reference System.                                    */
/*                                                                      */
/*      0 = Geographic                                                  */
/*      1 = UTM                                                         */
/*      2 = Stateplane                                                  */
/* -------------------------------------------------------------------- */
    if ( ! psWInfo->utmzone )
    {
        TextFillR( achARec + 156, 6, "0" );
    }
    else
    {
        TextFillR( achARec + 156, 6, "1" );
    }

/* -------------------------------------------------------------------- */
/*      UTM / State Plane zone.                                         */
/* -------------------------------------------------------------------- */
    if ( ! psWInfo->utmzone )
    {
        TextFillR( achARec + 162, 6, "0");
    }
    else
    {
        TextFillR( achARec + 162, 6,
            CPLSPrintf( "%02d", psWInfo->utmzone) );
    } 
    
/* -------------------------------------------------------------------- */
/*      Map Projection Parameters (all 0.0).                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < 15; i++ )
        TextFillR( achARec + 168 + i*24, 24, "0.0" );

/* -------------------------------------------------------------------- */
/*      Horizontal Unit of Measure                                      */
/*      0 = radians                                                     */
/*      1 = feet                                                        */
/*      2 = meters                                                      */
/*      3 = arc seconds                                                 */
/* -------------------------------------------------------------------- */
    if ( ! psWInfo->utmzone )
    {
        TextFillR( achARec + 528, 6, "3" );
    }
    else
    {
        TextFillR( achARec + 528, 6, "2" );
    }

/* -------------------------------------------------------------------- */
/*      Vertical unit of measure.                                       */
/*      1 = feet                                                        */
/*      2 = meters                                                      */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 534, 6, "2" );

/* -------------------------------------------------------------------- */
/*      Number of sides in coverage polygon (always 4)                  */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 540, 6, "4" );

/* -------------------------------------------------------------------- */
/*      4 corner coordinates: SW, NW, NE, SE                            */
/*      Corners are in 24.15 format in arc seconds.                     */
/* -------------------------------------------------------------------- */
    if ( ! psWInfo->utmzone )
    {
        // SW - longitude
        USGSDEMPrintDouble( achARec + 546, psWInfo->dfLLX * 3600.0 );
        // SW - latitude
        USGSDEMPrintDouble( achARec + 570, psWInfo->dfLLY * 3600.0 );

        // NW - longitude
        USGSDEMPrintDouble( achARec + 594, psWInfo->dfULX * 3600.0 );
        // NW - latitude
        USGSDEMPrintDouble( achARec + 618, psWInfo->dfULY * 3600.0 );

        // NE - longitude
        USGSDEMPrintDouble( achARec + 642, psWInfo->dfURX * 3600.0 );
        // NE - latitude
        USGSDEMPrintDouble( achARec + 666, psWInfo->dfURY * 3600.0 );

        // SE - longitude
        USGSDEMPrintDouble( achARec + 690, psWInfo->dfLRX * 3600.0 );
        // SE - latitude
        USGSDEMPrintDouble( achARec + 714, psWInfo->dfLRY * 3600.0 );
    }
    else
    {
        // SW - easting
        USGSDEMPrintDouble( achARec + 546, psWInfo->dfLLX );
        // SW - northing
        USGSDEMPrintDouble( achARec + 570, psWInfo->dfLLY );

        // NW - easting
        USGSDEMPrintDouble( achARec + 594, psWInfo->dfULX );
        // NW - northing
        USGSDEMPrintDouble( achARec + 618, psWInfo->dfULY );

        // NE - easting
        USGSDEMPrintDouble( achARec + 642, psWInfo->dfURX );
        // NE - northing
        USGSDEMPrintDouble( achARec + 666, psWInfo->dfURY );

        // SE - easting
        USGSDEMPrintDouble( achARec + 690, psWInfo->dfLRX );
        // SE - northing
        USGSDEMPrintDouble( achARec + 714, psWInfo->dfLRY );
    }

/* -------------------------------------------------------------------- */
/*      Minimum and Maximum elevations for this cell.                   */
/*      24.15 format.                                                   */
/* -------------------------------------------------------------------- */
    GInt16  nMin = DEM_NODATA, nMax = DEM_NODATA;
    int     nVoid = 0;

    for( i = psWInfo->nXSize*psWInfo->nYSize-1; i >= 0; i-- )
    {
        if( psWInfo->panData[i] != DEM_NODATA )
        {
            if( nMin == DEM_NODATA )
            {
                nMin = nMax = psWInfo->panData[i];
            }
            else
            {
                nMin = MIN(nMin,psWInfo->panData[i]);
                nMax = MAX(nMax,psWInfo->panData[i]);
            }
        }
        else
            nVoid++;
    }

    /* take into account z resolutions that are not 1.0 */
    nMin = (GInt16) floor(nMin * psWInfo->dfElevStepSize);
    nMax = (GInt16) ceil(nMax * psWInfo->dfElevStepSize);
    
    USGSDEMPrintDouble( achARec + 738, (double) nMin );
    USGSDEMPrintDouble( achARec + 762, (double) nMax );

/* -------------------------------------------------------------------- */
/*      Counter Clockwise angle (in radians).  Normally 0               */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 786, 24, "0.0" );

/* -------------------------------------------------------------------- */
/*      Accurancy code for elevations. 0 means there will be no C       */
/*      record.                                                         */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 810, 6, "0" );

/* -------------------------------------------------------------------- */
/*      Spatial Resolution (x, y and z).   12.6 format.                 */
/* -------------------------------------------------------------------- */
    if ( ! psWInfo->utmzone )
    {
        USGSDEMPrintSingle( achARec + 816,
            psWInfo->dfHorizStepSize*3600.0 );
        USGSDEMPrintSingle( achARec + 828,
            psWInfo->dfVertStepSize*3600.0 );
    }
    else
    {
        USGSDEMPrintSingle( achARec + 816,
            psWInfo->dfHorizStepSize );
        USGSDEMPrintSingle( achARec + 828,
            psWInfo->dfVertStepSize );
    }

    USGSDEMPrintSingle( achARec + 840, psWInfo->dfElevStepSize);

/* -------------------------------------------------------------------- */
/*      Rows and Columns of profiles.                                   */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 852, 6, CPLSPrintf( "%d", 1 ) );
    TextFillR( achARec + 858, 6, CPLSPrintf( "%d", psWInfo->nXSize ) );

/* -------------------------------------------------------------------- */
/*      Largest primary contour interval (blank).                       */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 864, 5, "" );

/* -------------------------------------------------------------------- */
/*      Largest source contour internal unit (blank).                   */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 869, 1, "" );

/* -------------------------------------------------------------------- */
/*      Smallest primary contour interval.                              */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 870, 5, "" );

/* -------------------------------------------------------------------- */
/*      Smallest source contour interval unit.                          */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 875, 1, "" );

/* -------------------------------------------------------------------- */
/*      Data source data - YYMM                                         */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec + 876, 4, "" );

/* -------------------------------------------------------------------- */
/*      Data inspection/revision data (YYMM).                           */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec + 880, 4, "" );

/* -------------------------------------------------------------------- */
/*      Inspection revision flag (I or R) (blank)                       */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec + 884, 1, "" );

/* -------------------------------------------------------------------- */
/*      Data validation flag.                                           */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec + 885, 1, "" );

/* -------------------------------------------------------------------- */
/*      Suspect and void area flag.                                     */
/*        0 = none                                                      */
/*        1 = suspect areas                                             */
/*        2 = void areas                                                */
/*        3 = suspect and void areas                                    */
/* -------------------------------------------------------------------- */
    if( nVoid > 0 )
        TextFillR( achARec + 886, 2, "2" );
    else
        TextFillR( achARec + 886, 2, "0" );

/* -------------------------------------------------------------------- */
/*      Vertical datum                                                  */
/*      1 = MSL                                                         */
/*      2 = NGVD29                                                      */
/*      3 = NAVD88                                                      */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFillR( achARec + 888, 2, "1" );

/* -------------------------------------------------------------------- */
/*      Horizonal Datum                                                 */
/*      1 = NAD27                                                       */
/*      2 = WGS72                                                       */
/*      3 = WGS84                                                       */
/*      4 = NAD83                                                       */
/* -------------------------------------------------------------------- */
    if( strlen( psWInfo->horizdatum ) == 0) {
        if( pszTemplate == NULL )
            TextFillR( achARec + 890, 2, "4" );
    }
    else
    {
        if( pszTemplate == NULL )
            TextFillR( achARec + 890, 2, psWInfo->horizdatum );
    }

/* -------------------------------------------------------------------- */
/*      Data edition/version, specification edition/version.            */
/* -------------------------------------------------------------------- */
    pszOption = CSLFetchNameValue( psWInfo->papszOptions, "DataSpecVersion" );

    if( pszOption != NULL )
        TextFill( achARec + 892, 4, pszOption );
        
    else if( pszTemplate == NULL )
        TextFill( achARec + 892, 4, "" );

/* -------------------------------------------------------------------- */
/*      Percent void.                                                   */
/*                                                                      */
/*      Round to nearest integer percentage.                            */
/* -------------------------------------------------------------------- */
    int nPercent;

    nPercent = (int) 
        (((nVoid * 100.0) / (psWInfo->nXSize * psWInfo->nYSize)) + 0.5);
        
    TextFillR( achARec + 896, 4, CPLSPrintf( "%4d", nPercent ) );

/* -------------------------------------------------------------------- */
/*      Edge matching flags.                                            */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec + 900, 8, "" );

/* -------------------------------------------------------------------- */
/*      Vertical datum shift (F7.2).                                    */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 908, 7, "" );

/* -------------------------------------------------------------------- */
/*      Write to file.                                                  */
/* -------------------------------------------------------------------- */
    if( VSIFWriteL( achARec, 1, 1024, psWInfo->fp ) != 1024 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Error writing DEM/CDED A record.\n%s", 
                  VSIStrerror( errno ) );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                        USGSDEMWriteProfile()                         */
/*                                                                      */
/*      Write B logical record.   Split into 1024 byte chunks.          */
/************************************************************************/

static int USGSDEMWriteProfile( USGSDEMWriteInfo *psWInfo, int iProfile )

{
    char achBuffer[1024];

    memset( achBuffer, ' ', sizeof(achBuffer) );

/* -------------------------------------------------------------------- */
/*      Row #.                                                          */
/* -------------------------------------------------------------------- */
    TextFillR( achBuffer +   0, 6, "1" );

/* -------------------------------------------------------------------- */
/*      Column #.                                                       */
/* -------------------------------------------------------------------- */
    TextFillR( achBuffer +   6, 6, CPLSPrintf( "%d", iProfile + 1 ) );

/* -------------------------------------------------------------------- */
/*      Number of data items.                                           */
/* -------------------------------------------------------------------- */
    TextFillR( achBuffer +  12, 6, CPLSPrintf( "%d", psWInfo->nYSize ) );
    TextFillR( achBuffer +  18, 6, "1" );

/* -------------------------------------------------------------------- */
/*      Location of center of bottom most sample in profile.            */
/*      Format D24.15.  In arc-seconds if geographic, meters            */
/*      if UTM.                                                         */
/* -------------------------------------------------------------------- */
    if( ! psWInfo->utmzone )
    {
        // longitude
        USGSDEMPrintDouble( achBuffer +  24, 
                        3600 * (psWInfo->dfLLX 
                                + iProfile * psWInfo->dfHorizStepSize) );

        // latitude
        USGSDEMPrintDouble( achBuffer +  48, psWInfo->dfLLY * 3600.0 );
    }
    else
    {
        // easting
        USGSDEMPrintDouble( achBuffer +  24, 
                        (psWInfo->dfLLX 
                            + iProfile * psWInfo->dfHorizStepSize) );

        // northing
        USGSDEMPrintDouble( achBuffer +  48, psWInfo->dfLLY );
    }


/* -------------------------------------------------------------------- */
/*      Local vertical datum offset.                                    */
/* -------------------------------------------------------------------- */
    TextFillR( achBuffer + 72, 24, "0.000000D+00" );

/* -------------------------------------------------------------------- */
/*      Min/Max elevation values for this profile.                      */
/* -------------------------------------------------------------------- */
    int iY; 
    GInt16  nMin = DEM_NODATA, nMax = DEM_NODATA;

    for( iY = 0; iY < psWInfo->nYSize; iY++ )
    {
        int iData = (psWInfo->nYSize-iY-1) * psWInfo->nXSize + iProfile; 

        if( psWInfo->panData[iData] != DEM_NODATA )
        {
            if( nMin == DEM_NODATA )
            {
                nMin = nMax = psWInfo->panData[iData];
            }
            else
            {
                nMin = MIN(nMin,psWInfo->panData[iData]);
                nMax = MAX(nMax,psWInfo->panData[iData]);
            }
        }
    }
    
    /* take into account z resolutions that are not 1.0 */
    nMin = (GInt16) floor(nMin * psWInfo->dfElevStepSize);
    nMax = (GInt16) ceil(nMax * psWInfo->dfElevStepSize);

    USGSDEMPrintDouble( achBuffer +  96, (double) nMin );
    USGSDEMPrintDouble( achBuffer +  120, (double) nMax );

/* -------------------------------------------------------------------- */
/*      Output all the actually elevation values, flushing blocks       */
/*      when they fill up.                                              */
/* -------------------------------------------------------------------- */
    int iOffset = 144;

    for( iY = 0; iY < psWInfo->nYSize; iY++ )
    {
        int iData = (psWInfo->nYSize-iY-1) * psWInfo->nXSize + iProfile; 
        char szWord[10];

        if( iOffset + 6 > 1024 )
        {
            if( VSIFWriteL( achBuffer, 1, 1024, psWInfo->fp ) != 1024 )
            {
                CPLError( CE_Failure, CPLE_FileIO, 
                          "Failure writing profile to disk.\n%s", 
                          VSIStrerror( errno ) );
                return FALSE;
            }
            iOffset = 0;
            memset( achBuffer, ' ', 1024 );
        }

        sprintf( szWord, "%d", psWInfo->panData[iData] );
        TextFillR( achBuffer + iOffset, 6, szWord );
        
        iOffset += 6;
    }

/* -------------------------------------------------------------------- */
/*      Flush final partial block.                                      */
/* -------------------------------------------------------------------- */
    if( VSIFWriteL( achBuffer, 1, 1024, psWInfo->fp ) != 1024 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failure writing profile to disk.\n%s", 
                  VSIStrerror( errno ) );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                      USGSDEM_LookupNTSByLoc()                        */
/************************************************************************/

static int 
USGSDEM_LookupNTSByLoc( double dfULLong, double dfULLat,
                        char *pszTile, char *pszName )

{
/* -------------------------------------------------------------------- */
/*      Access NTS 1:50k sheet CSV file.                                */
/* -------------------------------------------------------------------- */
    const char *pszNTSFilename = CSVFilename( "NTS-50kindex.csv" );
    FILE *fpNTS;

    fpNTS = VSIFOpen( pszNTSFilename, "rb" );
    if( fpNTS == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to find NTS mapsheet lookup file: %s", 
                  pszNTSFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Skip column titles line.                                        */
/* -------------------------------------------------------------------- */
    CSLDestroy( CSVReadParseLine( fpNTS ) );

/* -------------------------------------------------------------------- */
/*      Find desired sheet.                                             */
/* -------------------------------------------------------------------- */
    int  bGotHit = FALSE;
    char **papszTokens;

    while( !bGotHit 
           && (papszTokens = CSVReadParseLine( fpNTS )) != NULL )
    {
        if( CSLCount( papszTokens ) != 4 )
            continue;

        if( ABS(dfULLong - atof(papszTokens[2])) < 0.01 
            && ABS(dfULLat - atof(papszTokens[3])) < 0.01 )
        {
            bGotHit = TRUE;
            strncpy( pszTile, papszTokens[0], 7 );
            if( pszName != NULL )
                strncpy( pszName, papszTokens[1], 100 );
        }

        CSLDestroy( papszTokens );
    }

    VSIFClose( fpNTS );

    return bGotHit;
}

/************************************************************************/
/*                      USGSDEM_LookupNTSByTile()                       */
/************************************************************************/

static int 
USGSDEM_LookupNTSByTile( const char *pszTile, char *pszName,
                         double *pdfULLong, double *pdfULLat )

{
/* -------------------------------------------------------------------- */
/*      Access NTS 1:50k sheet CSV file.                                */
/* -------------------------------------------------------------------- */
    const char *pszNTSFilename = CSVFilename( "NTS-50kindex.csv" );
    FILE *fpNTS;

    fpNTS = VSIFOpen( pszNTSFilename, "rb" );
    if( fpNTS == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to find NTS mapsheet lookup file: %s", 
                  pszNTSFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Skip column titles line.                                        */
/* -------------------------------------------------------------------- */
    CSLDestroy( CSVReadParseLine( fpNTS ) );

/* -------------------------------------------------------------------- */
/*      Find desired sheet.                                             */
/* -------------------------------------------------------------------- */
    int  bGotHit = FALSE;
    char **papszTokens;

    while( !bGotHit 
           && (papszTokens = CSVReadParseLine( fpNTS )) != NULL )
    {
        if( CSLCount( papszTokens ) != 4 )
            continue;

        if( EQUAL(pszTile,papszTokens[0]) )
        {
            bGotHit = TRUE;
            if( pszName != NULL )
                strncpy( pszName, papszTokens[1], 100 );
            *pdfULLong = atof(papszTokens[2]);
            *pdfULLat = atof(papszTokens[3]);
        }

        CSLDestroy( papszTokens );
    }

    VSIFClose( fpNTS );

    return bGotHit;
}

/************************************************************************/
/*                    USGSDEMProductSetup_CDED50K()                     */
/************************************************************************/

static int USGSDEMProductSetup_CDED50K( USGSDEMWriteInfo *psWInfo )

{
/* -------------------------------------------------------------------- */
/*      Fetch TOPLEFT location so we know what cell we are dealing      */
/*      with.                                                           */
/* -------------------------------------------------------------------- */
    const char *pszNTS = 
        CSLFetchNameValue( psWInfo->papszOptions, "NTS" );
    const char *pszTOPLEFT = CSLFetchNameValue( psWInfo->papszOptions, 
                                                "TOPLEFT" );
    double dfULX = (psWInfo->dfULX+psWInfo->dfURX)*0.5;
    double dfULY = (psWInfo->dfULY+psWInfo->dfURY)*0.5;

    // Have we been given an explicit NTS mapsheet name? 
    if( pszNTS != NULL )
    {
        char szTrimmedTile[7];

        strncpy( szTrimmedTile, pszNTS, 6 );
        szTrimmedTile[6] = '\0';

        if( !USGSDEM_LookupNTSByTile( szTrimmedTile, NULL, &dfULX, &dfULY ) )
            return FALSE;

        if( EQUALN(pszNTS+6,"e",1) )
            dfULX += (( dfULY < 68.1 ) ? 0.25 : ( dfULY < 80.1 ) ? 0.5 : 1);
    }

    // Try looking up TOPLEFT as a NTS mapsheet name.
    else if( pszTOPLEFT != NULL && strstr(pszTOPLEFT,",") == NULL
        && (strlen(pszTOPLEFT) == 6 || strlen(pszTOPLEFT) == 7) )
    {
        char szTrimmedTile[7];

        strncpy( szTrimmedTile, pszTOPLEFT, 6 );
        szTrimmedTile[6] = '\0';

        if( !USGSDEM_LookupNTSByTile( szTrimmedTile, NULL, &dfULX, &dfULY ) )
            return FALSE;

        if( EQUAL(pszTOPLEFT+6,"e") )
            dfULX += (( dfULY < 68.1 ) ? 0.25 : ( dfULY < 80.1 ) ? 0.5 : 1);
    }

    // Assume TOPLEFT is a long/lat corner.
    else if( pszTOPLEFT != NULL )
    {
        char **papszTokens = CSLTokenizeString2( pszTOPLEFT, ",", 0 );

        if( CSLCount( papszTokens ) != 2 )
        {
            CSLDestroy( papszTokens );
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to parse TOPLEFT, should have form like '138d15W,59d0N'." );
            return FALSE;
        }

        dfULX = CPLDMSToDec( papszTokens[0] );
        dfULY = CPLDMSToDec( papszTokens[1] );
        CSLDestroy( papszTokens );

        if( ABS(dfULX*4-floor(dfULX*4+0.00005)) > 0.0001 
            || ABS(dfULY*4-floor(dfULY*4+0.00005)) > 0.0001 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "TOPLEFT must be on a 15\" boundary for CDED50K, but is not." );
            return FALSE;
        }
    }
    else if( strlen(psWInfo->pszFilename) == 12 
             && psWInfo->pszFilename[6] == '_'
             && EQUAL(psWInfo->pszFilename+8,".dem") )
    {
        char szTrimmedTile[7];

        strncpy( szTrimmedTile, psWInfo->pszFilename, 6 );
        szTrimmedTile[6] = '\0';

        if( !USGSDEM_LookupNTSByTile( szTrimmedTile, NULL, &dfULX, &dfULY ) )
            return FALSE;

        if( EQUALN(psWInfo->pszFilename+7,"e",1) )
            dfULX += (( dfULY < 68.1 ) ? 0.25 : ( dfULY < 80.1 ) ? 0.5 : 1);
    }
             
    else if( strlen(psWInfo->pszFilename) == 14 
             && EQUALN(psWInfo->pszFilename+6,"DEM",3)
             && EQUAL(psWInfo->pszFilename+10,".dem") )
    {
        char szTrimmedTile[7];

        strncpy( szTrimmedTile, psWInfo->pszFilename, 6 );
        szTrimmedTile[6] = '\0';

        if( !USGSDEM_LookupNTSByTile( szTrimmedTile, NULL, &dfULX, &dfULY ) )
            return FALSE;

        if( EQUALN(psWInfo->pszFilename+9,"e",1) )
            dfULX += (( dfULY < 68.1 ) ? 0.25 : ( dfULY < 80.1 ) ? 0.5 : 1);
    }

/* -------------------------------------------------------------------- */
/*      Set resolution and size information.                            */
/* -------------------------------------------------------------------- */

    dfULX = floor( dfULX * 4 + 0.00005 ) / 4.0;
    dfULY = floor( dfULY * 4 + 0.00005 ) / 4.0;

    psWInfo->nXSize = 1201;
    psWInfo->nYSize = 1201;
    psWInfo->dfVertStepSize = 0.75 / 3600.0;

    /* Region A */
    if( dfULY < 68.1 )
    {
        psWInfo->dfHorizStepSize = 0.75 / 3600.0;
    }

    /* Region B */
    else if( dfULY < 80.1 )
    {
        psWInfo->dfHorizStepSize = 1.5 / 3600.0;
        dfULX = floor( dfULX * 2 + 0.001 ) / 2.0;
    }

    /* Region C */
    else
    {
        psWInfo->dfHorizStepSize = 3.0 / 3600.0;
        dfULX = floor( dfULX + 0.001 );
    }

/* -------------------------------------------------------------------- */
/*      Set bounds based on this top left anchor.                       */
/* -------------------------------------------------------------------- */

    psWInfo->dfULX = dfULX;
    psWInfo->dfULY = dfULY;
    psWInfo->dfLLX = dfULX;
    psWInfo->dfLLY = dfULY - 0.25;
    psWInfo->dfURX = dfULX + psWInfo->dfHorizStepSize * 1200.0;
    psWInfo->dfURY = dfULY;
    psWInfo->dfLRX = dfULX + psWInfo->dfHorizStepSize * 1200.0;
    psWInfo->dfLRY = dfULY - 0.25;

/* -------------------------------------------------------------------- */
/*      Can we find the NTS 50k tile name that corresponds with         */
/*      this?                                                           */
/* -------------------------------------------------------------------- */
    const char *pszINTERNAL = 
        CSLFetchNameValue( psWInfo->papszOptions, "INTERNALNAME" );
    char szTile[10];
    char chEWFlag = ' ';

    if( USGSDEM_LookupNTSByLoc( dfULX, dfULY, szTile, NULL ) )
    {
        chEWFlag = 'w';
    }
    else if( USGSDEM_LookupNTSByLoc( dfULX-0.25, dfULY, szTile, NULL ) )
    {
        chEWFlag = 'e';
    }

    if( pszINTERNAL != NULL )
    {
        CPLFree( psWInfo->pszFilename );
        psWInfo->pszFilename = CPLStrdup( pszINTERNAL );
    }
    else if( chEWFlag != ' ' )
    {
        CPLFree( psWInfo->pszFilename );
        psWInfo->pszFilename = 
            CPLStrdup( CPLSPrintf("%sDEM%c", szTile, chEWFlag ) );
    }
    else
    {
        const char *pszBasename = CPLGetFilename( psWInfo->pszFilename);
        if( !EQUALN(pszBasename+6,"DEM",3) 
            || strlen(pszBasename) != 10 )
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Internal filename required to be of 'nnnannDEMz', the output\n"
                      "filename is not of the required format, and the tile could not be\n"
                      "identified in the NTS mapsheet list (or the NTS mapsheet could not\n"
                      "be found).  Correct output filename for correct CDED production." );
    }

/* -------------------------------------------------------------------- */
/*      Set some specific options for CDED 50K.                         */
/* -------------------------------------------------------------------- */
    psWInfo->papszOptions = 
        CSLSetNameValue( psWInfo->papszOptions, "DEMLevelCode", "1" );

    if( CSLFetchNameValue( psWInfo->papszOptions, "DataSpecVersion" ) == NULL )
        psWInfo->papszOptions = 
            CSLSetNameValue( psWInfo->papszOptions, "DataSpecVersion", 
                             "1020" );

/* -------------------------------------------------------------------- */
/*      Set the destination coordinate system.                          */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    oSRS.SetWellKnownGeogCS( "NAD83" );
    strncpy( psWInfo->horizdatum, "4", 2 );  //USGS DEM code for NAD83

    oSRS.exportToWkt( &(psWInfo->pszDstSRS) );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CPLReadLine( NULL );

    return TRUE;
}

/************************************************************************/
/*                    USGSDEMProductSetup_DEFAULT()                     */
/*                                                                      */
/*      Sets up the new DEM dataset parameters, using the source        */
/*      dataset's parameters.  If the source dataset uses UTM or        */
/*      geographic coordinates, the coordinate system is carried over   */
/*      to the new DEM file's parameters.  If the source dataset has a  */
/*      DEM compatible horizontal datum, the datum is carried over.     */
/*      Otherwise, the DEM dataset is configured to use geographic      */
/*      coordinates and a default datum.                                */
/*      (Hunter Blanks, 8/31/04, hblanks@artifex.org)                   */
/************************************************************************/

static int USGSDEMProductSetup_DEFAULT( USGSDEMWriteInfo *psWInfo )

{

/* -------------------------------------------------------------------- */
/*      Set the destination coordinate system.                          */
/* -------------------------------------------------------------------- */
    OGRSpatialReference DstoSRS;
    OGRSpatialReference SrcoSRS;
    char                *sourceWkt;
    int                 bNorth = TRUE;
        /* XXX here we are assume (!) northern hemisphere UTM datasets  */
    char                **readSourceWkt;
    int                 i;
    int                 numdatums = 4;
    const char          DatumCodes[4][2] = { "1", "2", "3", "4" };
    char                Datums[4][6] = { "NAD27", "WGS72", "WGS84",
                                            "NAD83" };

    /* get the source dataset's projection */
    sourceWkt = (char *) psWInfo->poSrcDS->GetProjectionRef();
    readSourceWkt = &sourceWkt;
    if (SrcoSRS.importFromWkt(readSourceWkt) != OGRERR_NONE)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "DEM Default Setup: Importing source dataset projection failed" );
        return FALSE;
    }
    
    /* Set the destination dataset's projection.  If the source datum
     * used is DEM compatible, just use it.  Otherwise, default to the
     * last datum in the Datums array.
     */
    for( i=0; i < numdatums; i++ )
    {
        if (DstoSRS.SetWellKnownGeogCS(Datums[i]) != OGRERR_NONE)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                "DEM Default Setup: Failed to set datum of destination" );
            return FALSE;
        }
        /* XXX Hopefully it's ok, to just keep changing the projection
         * of our destination.  If not, we'll want to reinitialize the
         * OGRSpatialReference each time.
         */
        if ( DstoSRS.IsSameGeogCS( &SrcoSRS ) )
        {
            break;
        }
    }
    strncpy( psWInfo->horizdatum, DatumCodes[i], 2 );
    
    /* get the UTM zone, if any */
    psWInfo->utmzone = SrcoSRS.GetUTMZone(&bNorth);
    if (psWInfo->utmzone)
    {
        if (DstoSRS.SetUTM(psWInfo->utmzone) != OGRERR_NONE)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
              "DEM Default Setup: Failed to set utm zone of destination" );
            /* SetUTM isn't documented to return OGRERR_NONE
             * on success, but it does, so, we'll check for it.
             */
            return FALSE;
        }
    }
    
    /* export the projection to sWInfo */
    if (DstoSRS.exportToWkt( &(psWInfo->pszDstSRS) ) != OGRERR_NONE)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "UTMDEM: Failed to export destination Wkt to psWInfo" );
    }
    return TRUE;
}

/************************************************************************/
/*                         USGSDEMLoadRaster()                          */
/*                                                                      */
/*      Loads the raster from the source dataset (not normally USGS     */
/*      DEM) into memory.  If nodata is marked, a special effort is     */
/*      made to translate it properly into the USGS nodata value.       */
/************************************************************************/

static int USGSDEMLoadRaster( USGSDEMWriteInfo *psWInfo,
                              CPL_UNUSED GDALRasterBand *poSrcBand )
{
    CPLErr eErr;
    int i;

/* -------------------------------------------------------------------- */
/*      Allocate output array, and pre-initialize to NODATA value.      */
/* -------------------------------------------------------------------- */
    psWInfo->panData = 
        (GInt16 *) VSIMalloc3( 2, psWInfo->nXSize, psWInfo->nYSize );
    if( psWInfo->panData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory allocating %d byte internal copy of DEM.", 
                  2 * psWInfo->nXSize * psWInfo->nYSize );
        return FALSE;
    }

    for( i = 0; i < psWInfo->nXSize * psWInfo->nYSize; i++ )
        psWInfo->panData[i] = DEM_NODATA;

/* -------------------------------------------------------------------- */
/*      Make a "memory dataset" wrapper for this data array.            */
/* -------------------------------------------------------------------- */
    GDALDriver  *poMemDriver = (GDALDriver *) GDALGetDriverByName( "MEM" );
    GDALDataset *poMemDS;

    if( poMemDriver == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find MEM driver." );
        return FALSE;
    }
   
    poMemDS = 
        poMemDriver->Create( "USGSDEM_temp", psWInfo->nXSize, psWInfo->nYSize, 
                         0, GDT_Int16, NULL );
    if( poMemDS == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Now add the array itself as a band.                             */
/* -------------------------------------------------------------------- */
    char szDataPointer[100];
    char *apszOptions[] = { szDataPointer, NULL };

    memset( szDataPointer, 0, sizeof(szDataPointer) );
    sprintf( szDataPointer, "DATAPOINTER=" );
    CPLPrintPointer( szDataPointer+strlen(szDataPointer), 
                     psWInfo->panData, 
                     sizeof(szDataPointer) - strlen(szDataPointer) );

    if( poMemDS->AddBand( GDT_Int16, apszOptions ) != CE_None )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Assign geotransform and nodata indicators.                      */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];

    adfGeoTransform[0] = psWInfo->dfULX - psWInfo->dfHorizStepSize * 0.5;
    adfGeoTransform[1] = psWInfo->dfHorizStepSize;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = psWInfo->dfULY + psWInfo->dfVertStepSize * 0.5;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -psWInfo->dfVertStepSize;

    poMemDS->SetGeoTransform( adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Set coordinate system if we have a special one to set.          */
/* -------------------------------------------------------------------- */
    if( psWInfo->pszDstSRS )
        poMemDS->SetProjection( psWInfo->pszDstSRS );

/* -------------------------------------------------------------------- */
/*      Establish the resampling kernel to use.                         */
/* -------------------------------------------------------------------- */
    GDALResampleAlg eResampleAlg = GRA_Bilinear;
    const char *pszResample = CSLFetchNameValue( psWInfo->papszOptions, 
                                                 "RESAMPLE" );

    if( pszResample == NULL )
        /* bilinear */;
    else if( EQUAL(pszResample,"Nearest") )
        eResampleAlg = GRA_NearestNeighbour;
    else if( EQUAL(pszResample,"Bilinear") )
        eResampleAlg = GRA_Bilinear;
    else if( EQUAL(pszResample,"Cubic") )
        eResampleAlg = GRA_Cubic;
    else if( EQUAL(pszResample,"CubicSpline") )
        eResampleAlg = GRA_CubicSpline;
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "RESAMPLE=%s, not a supported resampling kernel.", 
                  pszResample );
        return FALSE;
    }
        
/* -------------------------------------------------------------------- */
/*      Perform a warp from source dataset to destination buffer        */
/*      (memory dataset).                                               */
/* -------------------------------------------------------------------- */
    eErr = GDALReprojectImage( (GDALDatasetH) psWInfo->poSrcDS, 
                               psWInfo->poSrcDS->GetProjectionRef(),
                               (GDALDatasetH) poMemDS, 
                               psWInfo->pszDstSRS,
                               eResampleAlg, 0.0, 0.0, NULL, NULL, 
                               NULL );

/* -------------------------------------------------------------------- */
/*      Deallocate memory wrapper for the buffer.                       */
/* -------------------------------------------------------------------- */
    delete poMemDS;

    return eErr == CE_None;
}


/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
USGSDEMCreateCopy( const char *pszFilename, GDALDataset *poSrcDS, 
                   int bStrict, char **papszOptions,
                   CPL_UNUSED GDALProgressFunc pfnProgress, CPL_UNUSED void * pProgressData )
{
    USGSDEMWriteInfo sWInfo;

    if( poSrcDS->GetRasterCount() != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create multi-band USGS DEM / CDED files." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Capture some preliminary information.                           */
/* -------------------------------------------------------------------- */
    memset( &sWInfo, 0, sizeof(sWInfo) );

    sWInfo.poSrcDS = poSrcDS;
    sWInfo.pszFilename = CPLStrdup(pszFilename);
    sWInfo.nXSize = poSrcDS->GetRasterXSize();
    sWInfo.nYSize = poSrcDS->GetRasterYSize();
    sWInfo.papszOptions = CSLDuplicate( papszOptions );
    sWInfo.bStrict = bStrict;
    sWInfo.utmzone = 0;
    strncpy( sWInfo.horizdatum, "", 1 );

    if ( sWInfo.nXSize <= 1 || sWInfo.nYSize <= 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Source dataset dimensions must be at least 2x2." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Work out corner coordinates.                                    */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];

    poSrcDS->GetGeoTransform( adfGeoTransform );
    
    sWInfo.dfLLX = adfGeoTransform[0] + adfGeoTransform[1] * 0.5;
    sWInfo.dfLLY = adfGeoTransform[3] 
        + adfGeoTransform[5] * (sWInfo.nYSize - 0.5);

    sWInfo.dfULX = adfGeoTransform[0] + adfGeoTransform[1] * 0.5;
    sWInfo.dfULY = adfGeoTransform[3] + adfGeoTransform[5] * 0.5;
    
    sWInfo.dfURX = adfGeoTransform[0]
        + adfGeoTransform[1] * (sWInfo.nXSize - 0.5);
    sWInfo.dfURY = adfGeoTransform[3] + adfGeoTransform[5] * 0.5;
    
    sWInfo.dfLRX = adfGeoTransform[0] 
        + adfGeoTransform[1] * (sWInfo.nXSize - 0.5);
    sWInfo.dfLRY = adfGeoTransform[3] 
        + adfGeoTransform[5] * (sWInfo.nYSize - 0.5);

    sWInfo.dfHorizStepSize = (sWInfo.dfURX - sWInfo.dfULX) / (sWInfo.nXSize-1);
    sWInfo.dfVertStepSize = (sWInfo.dfURY - sWInfo.dfLRY) / (sWInfo.nYSize-1);

/* -------------------------------------------------------------------- */
/*      Allow override of z resolution, but default to 1.0.             */
/* -------------------------------------------------------------------- */
     const char *zResolution = CSLFetchNameValue(
             sWInfo.papszOptions, "ZRESOLUTION" );

     if( zResolution == NULL || EQUAL(zResolution,"DEFAULT") )
     {
         sWInfo.dfElevStepSize = 1.0;
     }
     else 
     {
         // XXX: We are using atof() here instead of CPLAtof() because
         // zResolution value comes from user's input and supposed to be
         // written according to user's current locale. atof() honors locale
         // setting, CPLAtof() is not.
         sWInfo.dfElevStepSize = atof( zResolution );
         if ( sWInfo.dfElevStepSize <= 0 )
         {
             /* don't allow negative values */
             sWInfo.dfElevStepSize = 1.0;
         }
     }
 
/* -------------------------------------------------------------------- */
/*      Initialize for special product configurations.                  */
/* -------------------------------------------------------------------- */
    const char *pszProduct = CSLFetchNameValue( sWInfo.papszOptions, 
                                                "PRODUCT" );

    if( pszProduct == NULL || EQUAL(pszProduct,"DEFAULT") )
    {
        if ( !USGSDEMProductSetup_DEFAULT( &sWInfo ) )
        {
            USGSDEMWriteCleanup( &sWInfo );
            return NULL;
        }
    }
    else if( EQUAL(pszProduct,"CDED50K") )
    {
        if( !USGSDEMProductSetup_CDED50K( &sWInfo ) )
        {
            USGSDEMWriteCleanup( &sWInfo );
            return NULL;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "DEM PRODUCT='%s' not recognised.", 
                  pszProduct );
        USGSDEMWriteCleanup( &sWInfo );
        return NULL;
    }
    

/* -------------------------------------------------------------------- */
/*      Read the whole area of interest into memory.                    */
/* -------------------------------------------------------------------- */
    if( !USGSDEMLoadRaster( &sWInfo, poSrcDS->GetRasterBand( 1 ) ) )
    {
        USGSDEMWriteCleanup( &sWInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    sWInfo.fp = VSIFOpenL( pszFilename, "wb" );
    if( sWInfo.fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s", VSIStrerror( errno ) );
        USGSDEMWriteCleanup( &sWInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write the A record.                                             */
/* -------------------------------------------------------------------- */
    if( !USGSDEMWriteARecord( &sWInfo ) ) 
    {
        USGSDEMWriteCleanup( &sWInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write profiles.                                                 */
/* -------------------------------------------------------------------- */
    int iProfile;

    for( iProfile = 0; iProfile < sWInfo.nXSize; iProfile++ )
    {
        if( !USGSDEMWriteProfile( &sWInfo, iProfile ) )
        {
            USGSDEMWriteCleanup( &sWInfo );
            return NULL;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    USGSDEMWriteCleanup( &sWInfo );

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

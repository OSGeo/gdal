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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.1  2004/03/27 17:02:00  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

typedef struct 
{
    const char *pszFilename;
    int         nXSize, nYSize;

    double      dfLLX, dfLLY;  // These are adjusted in to center of 
    double      dfULX, dfULY;  // corner pixels, and in decimal degrees.
    double      dfURX, dfURY;
    double      dfLRX, dfLRY;

    double      dfHorizStepSize;
    double      dfVertStepSize;

    char 	**papszOptions;
    int         bStrict;

    FILE       *fp;

    GInt16     *panData;
    
} USGSDEMWriteInfo;

/************************************************************************/
/*                        USGSDEMWriteCleanup()                         */
/************************************************************************/

static void USGSDEMWriteCleanup( USGSDEMWriteInfo *psWInfo )

{
    CSLDestroy( psWInfo->papszOptions );
    if( psWInfo->fp != NULL )
        VSIFClose( psWInfo->fp );
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
/*                        USGSDEMWriteARecord()                         */
/************************************************************************/

static int USGSDEMWriteARecord( USGSDEMWriteInfo *psWInfo )

{
    char achARec[1024];
    int  i;

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
        FILE *fpTemplate;

        fpTemplate = VSIFOpen( pszTemplate, "rb" );
        if( fpTemplate == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open template file '%s'.\n%s", 
                      pszTemplate, VSIStrerror( errno ) );
            return FALSE;
        }

        if( VSIFRead( achARec, 1, 1024, fpTemplate ) != 1024 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read 1024 byte A Record from template file '%s'.\n%s",
                      pszTemplate, VSIStrerror( errno ) );
            return FALSE;
        }
        VSIFClose( fpTemplate );
    }
    
/* -------------------------------------------------------------------- */
/*      Filename (right justify)                                        */
/* -------------------------------------------------------------------- */
    TextFillR( achARec +   0, 40, CPLGetFilename( psWInfo->pszFilename ) );

/* -------------------------------------------------------------------- */
/*      Producer                                                        */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec +  40, 60, "" );

/* -------------------------------------------------------------------- */
/*      Filler                                                          */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 100, 9, "" );

/* -------------------------------------------------------------------- */
/*      SW Geographic Corner - SDDDMMSS.SSSS - longitude then latitude  */
/* -------------------------------------------------------------------- */
    TextFill( achARec + 109, 13, 
              USGSDEMDecToPackedDMS( psWInfo->dfLLX ) ); // longitude
    TextFill( achARec + 122, 13, 
              USGSDEMDecToPackedDMS( psWInfo->dfLLY ) ); // latitude

/* -------------------------------------------------------------------- */
/*      Process code.                                                   */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFill( achARec + 135, 1, " " ); // Do we need a code for GDAL? 

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
    if( pszTemplate == NULL )
        TextFill( achARec + 140, 4, "" );  // Should be YT for Yukon.

/* -------------------------------------------------------------------- */
/*      DEM level code (right justify)                                  */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
        TextFillR( achARec + 144, 6, "" );  // 1, 2 or 3.
    
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
    TextFillR( achARec + 156, 6, "0" );

/* -------------------------------------------------------------------- */
/*      UTM / State Plane zone.                                         */
/* -------------------------------------------------------------------- */
    TextFillR( achARec + 162, 6, "0" );

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
    TextFillR( achARec + 528, 6, "3" );

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
    // SW - longitude
    TextFillR( achARec + 546, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfLLX * 3600.0 ) );
    // SW - latitude
    TextFillR( achARec + 570, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfLLY * 3600.0 ) );  

    // NW - longitude
    TextFillR( achARec + 594, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfULX * 3600.0 ) );  
    // NW - latitude
    TextFillR( achARec + 618, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfULY * 3600.0 ) );

    // NE - longitude
    TextFillR( achARec + 642, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfURX * 3600.0 ) );  
    // NE - latitude
    TextFillR( achARec + 666, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfURY * 3600.0 ) );

    // SE - longitude
    TextFillR( achARec + 690, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfLRX * 3600.0 ) );
    // SE - latitude
    TextFillR( achARec + 714, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfLRY * 3600.0 ) );

/* -------------------------------------------------------------------- */
/*      Minimum and Maximum elevations for this cell.                   */
/*      24.15 format.                                                   */
/* -------------------------------------------------------------------- */
    GInt16  nMin = -32767, nMax = -32767;
    int     nVoid = 0;

    for( i = psWInfo->nXSize*psWInfo->nYSize-1; i >= 0; i-- )
    {
        if( psWInfo->panData[i] != -32767 )
        {
            if( nMin == -32767 )
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

    TextFillR( achARec + 738, 24, CPLSPrintf( "%24.15e", (double) nMin) );
    TextFillR( achARec + 762, 24, CPLSPrintf( "%24.15e", (double) nMax) );

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
    TextFillR( achARec + 816, 12, 
               CPLSPrintf( "%12.6e", psWInfo->dfHorizStepSize*3600.0 ) );
    TextFillR( achARec + 828, 12, 
               CPLSPrintf( "%12.6e", psWInfo->dfVertStepSize*3600.0 ) );
    TextFillR( achARec + 840, 12, 
               CPLSPrintf( "%12.6e", 1.0 ) );

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
    if( pszTemplate == NULL )
        TextFillR( achARec + 890, 2, "4" );

/* -------------------------------------------------------------------- */
/*      Data edition/version, specification edition/version.            */
/* -------------------------------------------------------------------- */
    if( pszTemplate == NULL )
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
    if( VSIFWrite( achARec, 1, 1024, psWInfo->fp ) != 1024 )
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
/*      Format D24.15.  In arc-seconds.                                 */
/* -------------------------------------------------------------------- */
    // longitude
    TextFillR( achBuffer +  24, 24, 
               CPLSPrintf( "%24.15e", 
                           3600 * (psWInfo->dfLLX 
                           + iProfile * psWInfo->dfHorizStepSize) ) );

    // latitude
    TextFillR( achBuffer +  48, 24, 
               CPLSPrintf( "%24.15e", psWInfo->dfLLY * 3600.0 ) );

/* -------------------------------------------------------------------- */
/*      Local vertical datum offset.                                    */
/* -------------------------------------------------------------------- */
    TextFillR( achBuffer + 72, 24, "0.000000e+00" );

/* -------------------------------------------------------------------- */
/*      Min/Max elevation values for this profile.                      */
/* -------------------------------------------------------------------- */
    int iY; 
    GInt16  nMin = -32767, nMax = -32767;

    for( iY = 0; iY < psWInfo->nYSize; iY++ )
    {
        int iData = (psWInfo->nYSize-iY-1) * psWInfo->nXSize + iProfile; 

        if( psWInfo->panData[iData] != -32767 )
        {
            if( nMin == -32767 )
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

    TextFillR( achBuffer +  96, 24, 
               CPLSPrintf( "%24.15e", (double) nMin ) );
    TextFillR( achBuffer +  120, 24, 
               CPLSPrintf( "%24.15e", (double) nMax ) );

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
            if( VSIFWrite( achBuffer, 1, 1024, psWInfo->fp ) != 1024 )
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
    if( VSIFWrite( achBuffer, 1, 1024, psWInfo->fp ) != 1024 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failure writing profile to disk.\n%s", 
                  VSIStrerror( errno ) );
        return FALSE;
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
                              GDALRasterBand *poSrcBand )

{
    CPLErr eErr;

    psWInfo->panData = 
        (GInt16 *) VSIMalloc( 2 * psWInfo->nXSize * psWInfo->nYSize );
    if( psWInfo->panData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory allocating %d byte internal copy of DEM.", 
                  2 * psWInfo->nXSize * psWInfo->nYSize );
        return FALSE;
    }


    eErr = 
        poSrcBand->RasterIO( GF_Read, 0, 0, psWInfo->nXSize, psWInfo->nYSize, 
                             psWInfo->panData, psWInfo->nXSize,psWInfo->nYSize,
                             GDT_Int16, 0, 0 );
    return eErr == CE_None;
}


/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
USGSDEMCreateCopy( const char *pszFilename, GDALDataset *poSrcDS, 
                   int bStrict, char **papszOptions,
                   GDALProgressFunc pfnProgress, void * pProgressData )

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

    sWInfo.pszFilename = pszFilename;
    sWInfo.nXSize = poSrcDS->GetRasterXSize();
    sWInfo.nYSize = poSrcDS->GetRasterYSize();
    sWInfo.papszOptions = CSLDuplicate( papszOptions );
    sWInfo.bStrict = bStrict;

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
    sWInfo.fp = VSIFOpen( pszFilename, "wb" );
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

    return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
}

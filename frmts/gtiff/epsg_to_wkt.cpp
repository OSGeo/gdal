/******************************************************************************
 * $Id$
 *
 * Project:  EPSG To WKT Translator
 * Purpose:  Mainline for EPSG to WKT Translator
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.1  1999/09/09 13:55:43  warmerda
 * New
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tiffio.h"
#include "geotiffio.h"
#include "xtiffio.h"
#include "geo_normalize.h"
#include "cpl_csv.h"


static char *PCSToOGISDefn( int, int );
static void ProcessAllPCSCodes();
static void EmitWKTString( const char *, FILE * );

CPL_C_START
char *  GTIFGetOGISDefn( GTIFDefn * );
CPL_C_END

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
#ifdef notdef
    for( int iPCS = 4000; iPCS < 5000; iPCS++ )
    {
        char	*pszWKT;

        pszWKT = PCSToOGISDefn( iPCS );

        if( pszWKT != NULL )
        {
            printf( "\nEPSG = %d\n%s\n", iPCS, pszWKT );
            CPLFree( pszWKT );
        }
    }
#endif
    
    ProcessAllPCSCodes();
    
    exit( 0 );
}

/************************************************************************/
/*                         ProcessAllPCSCodes()                         */
/************************************************************************/

static void ProcessAllPCSCodes()

{
    FILE	*fp;
    const char  *pszFilename = CSVFilename( "horiz_cs.csv" );

/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "rt" );
    if( fp == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Scan the file from the beginning, replacing the ``current       */
/*      record'' in our structure with the one that is found.           */
/* -------------------------------------------------------------------- */
    VSIRewind( fp );
    CPLReadLine( fp );		/* throw away the header line */
    
    while( TRUE )
    {
        int		nPCS;
        char		**papszFields;
        
        papszFields = CSVReadParseLine( fp );
        if( papszFields == NULL || papszFields[0] == NULL )
            break;

        nPCS = atoi(papszFields[0]);
        CSLDestroy( papszFields );

        char	*pszWKT;

        pszWKT = PCSToOGISDefn( nPCS, nPCS > 3999 && nPCS < 5000 );

        if( pszWKT != NULL )
        {
            printf( "\nEPSG = %d\n", nPCS );
            EmitWKTString( pszWKT, stdout );
            CPLFree( pszWKT );
        }
    }

    VSIFClose( fp );
}

/************************************************************************/
/*                           EmitWKTString()                            */
/************************************************************************/

static void EmitWKTString( const char * pszWKT, FILE * fpOut )

{
    int		i;

    for( i = 0; pszWKT[i] != '\0'; i++ )
    {
        fputc( pszWKT[i], fpOut );
        if( pszWKT[i] == ',' && pszWKT[i+1] >= 'A' && pszWKT[i+1] <= 'Z' )
            fputc('\n', fpOut );
    }
    fputc('\n', fpOut );
}

/************************************************************************/
/*                           PCSToOGISDefn()                            */
/*                                                                      */
/*      In order to reuse all the logic in GTIFGetDefn(), we            */
/*      actually write out a tiny TIFF file with the PCS set.           */
/************************************************************************/

static char *PCSToOGISDefn( int nCode, int bIsGCS )

{
    TIFF	*hTIFF;
    GTIF	*hGTIF;

/* -------------------------------------------------------------------- */
/*      Write a tiny GeoTIFF file with the PCS code.                    */
/* -------------------------------------------------------------------- */
    hTIFF = XTIFFOpen( "temp.tif", "w+" );

    
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, 2 );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, 2 );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, 8 );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, 2 );

    hGTIF = GTIFNew( hTIFF );

    if( bIsGCS )
    {
        GTIFKeySet( hGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                    ModelTypeGeographic );
        GTIFKeySet( hGTIF, GeographicTypeGeoKey, TYPE_SHORT, 1, nCode );
    }
    else
    {
        GTIFKeySet( hGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                    ModelTypeProjected );
        GTIFKeySet( hGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nCode );
    }
    
    GTIFWriteKeys( hGTIF );
    GTIFFree( hGTIF );

    TIFFWriteEncodedStrip( hTIFF, 0, "    ", 4 );
    
    XTIFFClose( hTIFF );

/* -------------------------------------------------------------------- */
/*      Read a GeoTIFF definition from this file.                       */
/* -------------------------------------------------------------------- */
    GTIFDefn	sGeoTIFFProj;
    int		bSuccess;
    
    hTIFF = XTIFFOpen( "temp.tif", "r" );

    hGTIF = GTIFNew( hTIFF );
    bSuccess = GTIFGetDefn( hGTIF, &sGeoTIFFProj );
    GTIFFree( hGTIF );

    XTIFFClose( hTIFF );

/* -------------------------------------------------------------------- */
/*      Cleanup the file.                                               */
/* -------------------------------------------------------------------- */
    unlink( "temp.tif" );

    if( bSuccess && sGeoTIFFProj.GCS != KvUserDefined )
        return( GTIFGetOGISDefn( &sGeoTIFFProj ) );
    else
        return NULL;
}

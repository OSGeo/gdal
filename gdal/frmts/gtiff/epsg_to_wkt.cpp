/******************************************************************************
 * $Id$
 *
 * Project:  EPSG To WKT Translator
 * Purpose:  Mainline for EPSG to WKT Translator
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tiffio.h"
#include "geotiffio.h"
#include "xtiffio.h"
#include "geo_normalize.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

static char *PCSToOGISDefn( int, int );
static void ProcessAllPCSCodes();
static void EmitWKTString( const char *, FILE * );
static void WKTComparitor( const char *, const char * );

CPL_C_START
char *  GTIFGetOGISDefn( GTIFDefn * );
CPL_C_END

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    if( argc == 3 )
    {
        WKTComparitor( argv[1], argv[2] );
    }
    else
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
            printf( "EPSG=%d\n", nPCS );
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
//        if( pszWKT[i] == ',' && pszWKT[i+1] >= 'A' && pszWKT[i+1] <= 'Z' )
//            fputc('\n', fpOut );
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

/************************************************************************/
/*                              ReadWKT()                               */
/*                                                                      */
/*      Read EPSG code, and WKT from input file.                        */
/************************************************************************/

static OGRSpatialReference *ReadWKT( FILE * fp, int *pnEPSG )

{
    const char	*pszLine;

    pszLine = CPLReadLine( fp );
    if( pszLine == NULL || !EQUALN(pszLine,"EPSG=",5) )
        return NULL;

    *pnEPSG = atoi(pszLine+5);

    pszLine = CPLReadLine( fp );
    if( pszLine == NULL )
        return NULL;
    else
        return new OGRSpatialReference( pszLine );
}

/************************************************************************/
/*                           WKTCompareItem()                           */
/************************************************************************/

static int WKTCompareItem( const char * pszFile1,
                           OGRSpatialReference * poSRS1,
                           const char * pszFile2,
                           OGRSpatialReference * poSRS2,
                           const char * pszTargetItem, int nEPSG )

{
    if( poSRS1->GetAttrValue(pszTargetItem) == NULL
        && poSRS1->GetAttrValue(pszTargetItem) == NULL )
        return TRUE;
    
    if( poSRS1->GetAttrValue(pszTargetItem) == NULL )
    {
        printf( "File %s missing %s for EPSG %d\n",
                pszFile1, pszTargetItem, nEPSG );
        return FALSE;
    }

    if( poSRS2->GetAttrValue(pszTargetItem) == NULL )
    {
        printf( "File %s missing %s for EPSG %d\n",
                pszFile1, pszTargetItem, nEPSG );
        return FALSE;
    }

    if( !EQUAL(poSRS1->GetAttrValue(pszTargetItem),
               poSRS2->GetAttrValue(pszTargetItem)) )
    {
        printf( "Datum mismatch for EPSG %d:\n"
                "%s: %s\n"
                "%s: %s\n",
                nEPSG,
                pszFile1, poSRS1->GetAttrValue(pszTargetItem),
                pszFile2, poSRS2->GetAttrValue(pszTargetItem) );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           WKTComparitor()                            */
/************************************************************************/

static void WKTComparitor( const char * pszFile1, const char * pszFile2 )

{
    FILE	*fp1, *fp2;

/* -------------------------------------------------------------------- */
/*      Open files                                                      */
/* -------------------------------------------------------------------- */
    fp1 = VSIFOpen( pszFile1, "rt" );
    if( fp1 == NULL )
    {
        fprintf( stderr, "Could not open %s.\n", pszFile1 );
        return;
    }

    fp2 = VSIFOpen( pszFile2, "rt" );
    if( fp2 == NULL )
    {
        fprintf( stderr, "Could not open %s.\n", pszFile2 );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Read a code from each file.                                     */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS1, *poSRS2;
    int			nEPSG1, nEPSG2;
    
    while( (poSRS1 = ReadWKT(fp1,&nEPSG1)) != NULL )
    {
        poSRS2 = ReadWKT(fp2,&nEPSG2);

        if( nEPSG1 != nEPSG2 )
        {
            printf( "EPSG codes (%d, %d) don't match, bailing out.\n",
                    nEPSG1, nEPSG2 );
            break;
        }

        WKTCompareItem( pszFile1, poSRS1, pszFile2, poSRS2,
                        "DATUM", nEPSG1 );
        
        WKTCompareItem( pszFile1, poSRS1, pszFile2, poSRS2,
                        "PROJECTION", nEPSG1 );
        
        delete poSRS1;
        delete poSRS2;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFClose( fp2 );
    VSIFClose( fp1 );
}



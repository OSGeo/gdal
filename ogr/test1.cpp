/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Working test harnass.
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
 * Revision 1.6  1999/05/31 20:40:17  warmerda
 * modified createFromWkt() to indicate how much text consumed
 *
 * Revision 1.5  1999/05/23 05:34:41  warmerda
 * added support for clone(), multipolygons and geometry collections
 *
 * Revision 1.4  1999/05/20 14:36:19  warmerda
 * added well known text support
 *
 * Revision 1.3  1999/03/30 21:21:43  warmerda
 * added linearring/polygon support
 *
 * Revision 1.2  1999/03/29 21:36:14  warmerda
 * New
 *
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ReportBin( const char * );
static void CreateBin( OGRGeometry *, const char * );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    if( nArgc < 3 )
    {
        printf( "Usage: test1 -reportbin bin_file\n" );
        printf( "    or test1 -reporttxt txt_file\n" );
        printf( "    or test1 -createbin bin_file {point,line,polygon,multipolygon}\n" );
        exit( 1 );
    }

    if( strcmp( papszArgv[1], "-reportbin" ) == 0 )
    {
        ReportBin( papszArgv[2] );
    }
    else if( strcmp( papszArgv[1], "-reporttxt" ) == 0 )
    {
        ReportBin( papszArgv[2] );
    }
    else if( strcmp( papszArgv[1], "-createbin" ) == 0 )
    {
        if( strcmp( papszArgv[3], "point") == 0 )
        {
            OGRPoint	oPoint( 100, 200 );
            
            CreateBin( &oPoint, papszArgv[2] );
        }
        else if( strcmp( papszArgv[3], "line") == 0 )
        {
            OGRLineString	oLine;

            oLine.addPoint( 200, 300 );
            oLine.addPoint( 300, 400 );
            oLine.addPoint( 0, 0 );
            
            CreateBin( &oLine, papszArgv[2] );
        }
        else if( strcmp( papszArgv[3], "polygon") == 0 )
        {
            OGRPolygon	oPolygon;
            OGRLinearRing oRing;

            oRing.addPoint( 0, 0 );
            oRing.addPoint( 200, 300 );
            oRing.addPoint( 300, 400 );
            oRing.addPoint( 0, 0 );

            oPolygon.addRing( &oRing );

            oRing.setNumPoints( 0 );
            oRing.addPoint( 10, 10 );
            oRing.addPoint( 20, 30 );
            oRing.addPoint( 30, 40 );
            oRing.addPoint( 10, 10 );

            oPolygon.addRing( &oRing );

            CreateBin( &oPolygon, papszArgv[2] );
        }
        else if( strcmp( papszArgv[3], "multipolygon") == 0 )
        {
            OGRPolygon	oPolygon;
            OGRLinearRing oRing;
            OGRMultiPolygon oMPoly;

            oRing.addPoint( 0, 0 );
            oRing.addPoint( 200, 300 );
            oRing.addPoint( 300, 400 );
            oRing.addPoint( 0, 0 );

            oPolygon.addRing( &oRing );

            oRing.setNumPoints( 0 );
            oRing.addPoint( 10, 10 );
            oRing.addPoint( 20, 30 );
            oRing.addPoint( 30, 40 );
            oRing.addPoint( 10, 10 );

            oPolygon.addRing( &oRing );

            oMPoly.addGeometry( &oPolygon );
            oMPoly.addGeometry( &oPolygon );

            CreateBin( &oMPoly, papszArgv[2] );
        }
    }

    return 0;
}

/************************************************************************/
/*                             ReportBin()                              */
/*                                                                      */
/*      Read a WKB file into a geometry, and dump in human readable     */
/*      format.                                                         */
/************************************************************************/

void ReportBin( const char * pszFilename )

{
    FILE	*fp;
    long	length;
    unsigned char * pabyData;
    OGRGeometry *poGeom;
    OGRErr	eErr;
    
/* -------------------------------------------------------------------- */
/*      Open source file.                                               */
/* -------------------------------------------------------------------- */
    fp = fopen( pszFilename, "rb" );
    if( fp == NULL )
    {
        perror( "fopen" );

        fprintf( stderr, "Failed to open `%s'\n", pszFilename );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Get length.                                                     */
/* -------------------------------------------------------------------- */
    fseek( fp, 0, SEEK_END );
    length = ftell( fp );
    fseek( fp, 0, SEEK_SET );
    
/* -------------------------------------------------------------------- */
/*      Read into a block of memory.                                    */
/* -------------------------------------------------------------------- */
    pabyData = (unsigned char *) OGRCalloc( 1, length+1 );
    if( pabyData == NULL )
    {
        fclose( fp );
        fprintf( stderr, "Failed to allocate %ld bytes\n", length );
        return;
    }

    fread( pabyData, length, 1, fp );

/* -------------------------------------------------------------------- */
/*      Instantiate a geometry from this data.  If the first byte is    */
/*      over ``31'' we will assume it is text format, otherwise binary. */
/* -------------------------------------------------------------------- */
    poGeom = NULL;
    if( pabyData[0] > 31 )
    {
        char	*pszInput = (char *) pabyData;
        
        eErr = OGRGeometryFactory::createFromWkt( &pszInput, NULL, &poGeom );
    }
    else
    {
        eErr = OGRGeometryFactory::createFromWkb( pabyData, NULL,
                                                  &poGeom, length );
    }

    if( eErr == OGRERR_NONE )
    {
        poGeom->dumpReadable( stdout );
    }
    else
    {
        fprintf( stderr,
                 "Encountered error %d trying to create the geometry in "
                 "OGRGeometryFactory.\n",
                 eErr );
    }

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    OGRFree( pabyData );
    
    delete poGeom;
    fclose( fp );
}

/************************************************************************/
/*                             CreateBin()                              */
/*                                                                      */
/*      Create a binary file representation from a given geometry       */
/*      object.                                                         */
/************************************************************************/

void CreateBin( OGRGeometry * poGeom, const char * pszFilename )

{
    FILE	*fp;
    unsigned char *pabyData;
    long	nWkbSize;

/* -------------------------------------------------------------------- */
/*      Translate geometry into a binary representation.                */
/* -------------------------------------------------------------------- */
    nWkbSize = poGeom->WkbSize();
    
    pabyData = (unsigned char *) OGRMalloc(nWkbSize);
    if( pabyData == NULL )
    {
        fprintf( stderr, "Unable to allocate %ld bytes.\n",
                 nWkbSize );
        return;
    }

    poGeom->exportToWkb( wkbNDR, pabyData );

/* -------------------------------------------------------------------- */
/*      Open output file.                                               */
/* -------------------------------------------------------------------- */
    fp = fopen( pszFilename, "wb" );
    if( fp == NULL )
    {
        OGRFree( pabyData );
        perror( "fopen" );
        fprintf( stderr, "Can't create file `%s'\n", pszFilename );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Write data and cleanup                                          */
/* -------------------------------------------------------------------- */
    fwrite( pabyData, nWkbSize, 1, fp );

    fclose( fp );

    OGRFree( pabyData );
}

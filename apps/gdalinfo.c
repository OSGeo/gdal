/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * gdalinfo.c
 *
 * Simple application for dumping all the data about a dataset.  Mainly
 * serves as an early test harnass.
 *
 * $Log$
 * Revision 1.3  1999/03/02 21:12:01  warmerda
 * add DMS reporting of lat/long
 *
 * Revision 1.2  1999/01/11 15:27:59  warmerda
 * Add projection support
 *
 * Revision 1.1  1998/12/03 18:35:06  warmerda
 * New
 *
 */

#include "gdal.h"

int main( int argc, char ** argv )

{
    GDALDatasetH	hDataset;
    GDALRasterBandH	hBand;
    int			i;
    double		adfGeoTransform[6];
    GDALProjDefH	hProjDef;

    if( argc < 2 )
    {
        printf( "Usage: gdalinfo datasetname\n" );
        exit( 10 );
    }

    GDALAllRegister();

    hDataset = GDALOpen( argv[1], GA_ReadOnly );
    
    if( hDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        exit( 1 );
    }

    printf( "Size is %d, %d\n",
            GDALGetRasterXSize( hDataset ), 
            GDALGetRasterYSize( hDataset ) );

    printf( "Projection is `%s'\n",
            GDALGetProjectionRef( hDataset ) );

    GDALGetGeoTransform( hDataset, adfGeoTransform );
    printf( "Origin = (%g,%g)\n",
            adfGeoTransform[0], adfGeoTransform[3] );
    
    GDALGetGeoTransform( hDataset, adfGeoTransform );
    printf( "Pixel Size = (%g,%g)\n",
            adfGeoTransform[1], adfGeoTransform[5] );

    hProjDef = GDALCreateProjDef( GDALGetProjectionRef( hDataset ) );
    if( hProjDef != NULL )
    {
        if( GDALReprojectToLongLat( hProjDef,
                                    adfGeoTransform + 0,
                                    adfGeoTransform + 3 ) == CE_None )
        {
            printf( "Origin (long/lat) = (%g,%g)",
                    adfGeoTransform[0], adfGeoTransform[3] );

            printf( " (%s,",  GDALDecToDMS( adfGeoTransform[0], "Long", 2 ) );
            printf( " %s)\n",  GDALDecToDMS( adfGeoTransform[3], "Lat", 2 ) );
        }
        else
        {
            printf( "GDALReprojectToLongLat(): %s\n",
                    CPLGetLastErrorMsg() );
        }

        GDALDestroyProjDef( hProjDef );
    }

    for( i = 0; i < GDALGetRasterCount( hDataset ); i++ )
    {
        hBand = GDALGetRasterBand( hDataset, i+1 );
        printf( "Band %d Type = %d\n", i+1, GDALGetRasterDataType( hBand ) );
    }

    GDALClose( hDataset );
    
    exit( 0 );
}

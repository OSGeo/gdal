/* ****************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 * ****************************************************************************
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
 * ****************************************************************************
 *
 * $Log$
 * Revision 1.6  1999/10/01 14:45:14  warmerda
 * prettied up
 *
 * Revision 1.5  1999/08/12 18:23:47  warmerda
 * Attempt to create the output file with the data type of the first
 * source band.
 *
 * Revision 1.4  1999/05/17 01:38:05  warmerda
 * added transfer of geotransform and proj
 *
 * Revision 1.3  1999/05/13 15:33:12  warmerda
 * added support for selecting a single band
 *
 * Revision 1.2  1999/04/21 04:14:05  warmerda
 * Fixed GDALOpen() call
 *
 * Revision 1.1  1999/03/02 21:12:25  warmerda
 * New
 */

#include "gdal.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"

/*  ******************************************************************* */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage()

{
    printf( "Usage: gdal_translate [-of format] [-b band]\n"
            "                      src_dataset dst_dataset\n" );
}

/* ******************************************************************** */
/*                                main()                                */
/* ******************************************************************** */

int main( int argc, char ** argv )

{
    GDALDatasetH	hDataset, hOutDS;
    GDALRasterBandH	hBand;
    int			i, nSrcBand = -1;
    int			nRasterXSize, nRasterYSize;
    const char		*pszSource=NULL, *pszDest=NULL, *pszFormat = "GTiff";
    GDALDriverH		hDriver;
    int			*panBandList, nBandCount;
    double		adfGeoTransform[6];

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-of") )
            pszFormat = argv[++i];

        else if( EQUAL(argv[i],"-b") )
            nSrcBand = atoi(argv[++i]);
            
        else if( pszSource == NULL )
            pszSource = argv[i];

        else if( pszDest == NULL )
            pszDest = argv[i];

        else
        {
            printf( "Too many command options.\n\n" );
            Usage();
            exit( 2 );
        }
    }

    if( pszDest == NULL )
    {
        Usage();
        exit( 10 );
    }

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and identify output driver.     */
/* -------------------------------------------------------------------- */
    GDALAllRegister();

/* -------------------------------------------------------------------- */
/*      Attempt to open source file.                                    */
/* -------------------------------------------------------------------- */

    hDataset = GDALOpen( pszSource, GA_ReadOnly );
    
    if( hDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        exit( 1 );
    }

    nRasterXSize = GDALGetRasterXSize( hDataset );
    nRasterYSize = GDALGetRasterYSize( hDataset );
    
    printf( "Size is %d, %d\n", nRasterXSize, nRasterYSize );

/* -------------------------------------------------------------------- */
/*	Build band list to translate					*/
/* -------------------------------------------------------------------- */
    if( nSrcBand != -1 )
    {
        nBandCount = 1;
        panBandList = (int *) CPLMalloc(sizeof(int)*nBandCount);
        panBandList[0] = nSrcBand;
    }
    else
    {
        nBandCount = GDALGetRasterCount( hDataset );
        panBandList = (int *) CPLMalloc(sizeof(int)*nBandCount);
        for( i = 0; i < nBandCount; i++ )
            panBandList[i] = i+1;
    }

/* -------------------------------------------------------------------- */
/*      Create the output database.                                     */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == NULL )
    {
        printf( "Output driver `%s' not recognised.\n", pszFormat );
        Usage();
        exit( 1 );
    }
    
    hBand = GDALGetRasterBand( hDataset, panBandList[0] );
    hOutDS = GDALCreate( hDriver, pszDest, nRasterXSize, nRasterYSize,
                         nBandCount,
                         GDALGetRasterDataType(hBand), NULL );
    if( hOutDS == NULL )
    {
        printf( "GDALCreate() failed.\n" );
        exit( 10 );
    }
    
/* -------------------------------------------------------------------- */
/*	Copy over projection, and geotransform information.		*/
/* -------------------------------------------------------------------- */
    GDALSetProjection( hOutDS, GDALGetProjectionRef( hDataset ) );

    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
        GDALSetGeoTransform( hOutDS, adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Loop copying bands.                                             */
/* -------------------------------------------------------------------- */
    
    for( i = 0; i < nBandCount; i++ )
    {
        GByte	*pabyBlock;
        int     iBlockY;
        int	nPixelSize;
        GDALRasterBandH hDstBand;

        hBand = GDALGetRasterBand( hDataset, panBandList[i] );
        hDstBand = GDALGetRasterBand( hOutDS, i+1 );
        
        printf( "Band %d Type = %d\n",
                panBandList[i], GDALGetRasterDataType( hBand ) );

/* -------------------------------------------------------------------- */
/*      Write out the raw raster data.                                  */
/* -------------------------------------------------------------------- */
        nPixelSize = GDALGetDataTypeSize( GDALGetRasterDataType(hBand) ) / 8;

        pabyBlock = (GByte *) CPLMalloc(nPixelSize*nRasterXSize);

        for( iBlockY = 0; iBlockY < nRasterYSize; iBlockY++ )
        {
            GDALRasterIO( hBand, GF_Read,
                          0, iBlockY, nRasterXSize, 1,
                          pabyBlock, nRasterXSize, 1,
                          GDALGetRasterDataType(hBand),
                          0, 0 );
            GDALRasterIO( hDstBand, GF_Write,
                          0, iBlockY, nRasterXSize, 1,
                          pabyBlock, nRasterXSize, 1,
                          GDALGetRasterDataType(hBand),
                          0, 0 );
        }

        CPLFree( pabyBlock );
    }

    GDALClose( hOutDS );
    GDALClose( hDataset );
    
    exit( 0 );
}

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
 * Revision 1.13  2000/04/30 23:21:47  warmerda
 * uses new CreateCopy when possible
 *
 * Revision 1.12  2000/03/31 13:43:42  warmerda
 * added source window
 *
 * Revision 1.11  2000/03/23 20:32:37  gdalanon
 * fix usage
 *
 * Revision 1.10  2000/03/23 18:48:19  warmerda
 * Added support for creation options.
 *
 * Revision 1.9  2000/03/06 02:18:42  warmerda
 * added -outsize option
 *
 * Revision 1.8  2000/02/18 03:52:16  warmerda
 * Added support for setting the output band data type, but still does
 * no scaling.
 *
 * Revision 1.7  1999/10/21 13:21:49  warmerda
 * Added report of available formats.
 *
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
#include "cpl_string.h"

/*  ******************************************************************* */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage()

{
    printf( "Usage: gdal_translate [-ot {Byte/UInt16/UInt32/Int32/Float32/Float64}]\n"
            "                      [-of format] [-b band]\n"
            "			   [-outsize xsize ysize]\n"
            "                      [-srcwin xoff yoff xsize ysize]\n"
            "                      [-co \"NAME=VALUE.\"]*\n"
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
    GDALDataType	eOutputType = GDT_Unknown;
    int			nOXSize = 0, nOYSize = 0;
    char                **papszCreateOptions = NULL;
    int                 anSrcWin[4];
    const char          *pszProjection;

    anSrcWin[0] = 0;
    anSrcWin[1] = 0;
    anSrcWin[2] = 0;
    anSrcWin[3] = 0;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-of") && i < argc-1 )
            pszFormat = argv[++i];

        else if( EQUAL(argv[i],"-ot") && i < argc-1 )
        {
            int	iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             argv[i+1]) )
                {
                    eOutputType = (GDALDataType) iType;
                }
            }

            if( eOutputType == GDT_Unknown )
            {
                printf( "Unknown output pixel type: %s\n", argv[i+1] );
                Usage();
                exit( 2 );
            }
            i++;
        }
        else if( EQUAL(argv[i],"-b") && i < argc-1 )
            nSrcBand = atoi(argv[++i]);
            
        else if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }   

        else if( EQUAL(argv[i],"-outsize") && i < argc-2 )
        {
            nOXSize = atoi(argv[++i]);
            nOYSize = atoi(argv[++i]);
        }   

        else if( EQUAL(argv[i],"-srcwin") && i < argc-4 )
        {
            anSrcWin[0] = atoi(argv[++i]);
            anSrcWin[1] = atoi(argv[++i]);
            anSrcWin[2] = atoi(argv[++i]);
            anSrcWin[3] = atoi(argv[++i]);
        }   

        else if( argv[i][0] == '-' )
        {
            printf( "Option %s incomplete, or not recognised.\n\n", 
                    argv[i] );
            Usage();
            exit( 2 );
        }
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

    if( anSrcWin[2] == 0 && anSrcWin[3] == 0 )
    {
        anSrcWin[2] = nRasterXSize;
        anSrcWin[3] = nRasterYSize;
    }

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
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == NULL )
    {
        int	iDr;
        
        printf( "Output driver `%s' not recognised.\n", pszFormat );
        printf( "The following format drivers are configured:\n" );
        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            printf( "  %s: %s\n",
                    GDALGetDriverShortName( GDALGetDriver(iDr) ),
                    GDALGetDriverLongName( GDALGetDriver(iDr) ) );
        }
        printf( "\n" );
        Usage();
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      The short form is to CreateCopy().  We use this if the input    */
/*      matches the whole dataset.  Eventually we should rewrite        */
/*      this entire program to use virtual datasets to construct a      */
/*      virtual input source to copy from.                              */
/* -------------------------------------------------------------------- */
    if( eOutputType == GDT_Unknown 
        && nBandCount == GDALGetRasterCount(hDataset)
        && anSrcWin[0] == 0 && anSrcWin[1] == 0 
        && anSrcWin[2] == GDALGetRasterXSize(hDataset)
        && anSrcWin[3] == GDALGetRasterYSize(hDataset) 
        && nOXSize == 0 && nOYSize == 0 )
    {
        
        hOutDS = GDALCreateCopy( hDriver, pszDest, hDataset, 
                                 TRUE, papszCreateOptions, 
                                 GDALDummyProgress, NULL );
        if( hOutDS != NULL )
            GDALClose( hOutDS );
        
        GDALClose( hDataset );

        exit( hOutDS == NULL );
    }

/* -------------------------------------------------------------------- */
/*      Establish some parameters.                                      */
/* -------------------------------------------------------------------- */
    if( eOutputType == GDT_Unknown )
    {
        hBand = GDALGetRasterBand( hDataset, panBandList[0] );
        eOutputType = GDALGetRasterDataType(hBand);
    }

    if( nOXSize == 0 )
    {
        nOXSize = anSrcWin[2];
        nOYSize = anSrcWin[3];
    }
    
/* -------------------------------------------------------------------- */
/*      Create the output database.                                     */
/* -------------------------------------------------------------------- */
    hOutDS = GDALCreate( hDriver, pszDest, nOXSize, nOYSize, 
                         nBandCount, eOutputType, papszCreateOptions );
    if( hOutDS == NULL )
    {
        printf( "GDALCreate() failed.\n" );
        exit( 10 );
    }
    
/* -------------------------------------------------------------------- */
/*	Copy over projection, and geotransform information.		*/
/* -------------------------------------------------------------------- */
    pszProjection = GDALGetProjectionRef( hDataset );
    if( pszProjection != NULL && strlen(pszProjection) > 0 )
        GDALSetProjection( hOutDS, pszProjection );

    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        adfGeoTransform[0] += anSrcWin[0] * adfGeoTransform[1]
                            + anSrcWin[1] * adfGeoTransform[4];
        adfGeoTransform[1] += anSrcWin[0] * adfGeoTransform[2]
                            + anSrcWin[1] * adfGeoTransform[5];

        adfGeoTransform[1] *= anSrcWin[2] / (double) nOXSize;
        adfGeoTransform[2] *= anSrcWin[3] / (double) nOYSize;
        adfGeoTransform[4] *= anSrcWin[2] / (double) nOXSize;
        adfGeoTransform[5] *= anSrcWin[3] / (double) nOYSize;

        GDALSetGeoTransform( hOutDS, adfGeoTransform );
    }

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

        pabyBlock = (GByte *) CPLMalloc(nPixelSize*nOXSize);

        for( iBlockY = 0; iBlockY < nOYSize; iBlockY++ )
        {
            int		iSrcYOff;

            if( nOYSize == anSrcWin[3] )
                iSrcYOff = iBlockY + anSrcWin[1];
            else
            {
                iSrcYOff = (iBlockY / (double) nOYSize) * anSrcWin[3]
                    + anSrcWin[1];
                iSrcYOff = MAX(0,MIN(anSrcWin[3]-1,iSrcYOff));
            }

            GDALRasterIO( hBand, GF_Read,
                          anSrcWin[0], iSrcYOff, anSrcWin[2], 1,
                          pabyBlock, nOXSize, 1,
                          GDALGetRasterDataType(hBand),
                          0, 0 );

            GDALRasterIO( hDstBand, GF_Write,
                          0, iBlockY, nOXSize, 1,
                          pabyBlock, nOXSize, 1,
                          GDALGetRasterDataType(hBand),
                          0, 0 );
        }

        CPLFree( pabyBlock );
    }

    GDALClose( hOutDS );
    GDALClose( hDataset );
    
    exit( 0 );
}

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
 * Revision 1.2  2001/07/18 05:05:12  warmerda
 * added CPL_CSVID
 *
 * Revision 1.1  1999/06/03 14:22:15  warmerda
 * New
 *
 * Revision 1.1  1998/12/03 18:35:06  warmerda
 * New
 *
 */

#include "gdal.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

int main( int argc, char ** argv )

{
    GDALDatasetH	hDataset;
    GDALRasterBandH	hBand;
    int			i;
    int			nRasterXSize, nRasterYSize;

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

    nRasterXSize = GDALGetRasterXSize( hDataset );
    nRasterYSize = GDALGetRasterYSize( hDataset );
    
    printf( "Size is %d, %d\n", nRasterXSize, nRasterYSize );

    for( i = 0; i < GDALGetRasterCount( hDataset ); i++ )
    {
        char	szFilename[128];
        GByte	*pabyBlock;
        int     nBlockXSize, nBlockYSize, iBlockX, iBlockY;
        int	nPixelSize;
        FILE	*fp;
        const char * pszDataTypeName;

        hBand = GDALGetRasterBand( hDataset, i+1 );
        printf( "Band %d Type = %d\n", i+1, GDALGetRasterDataType( hBand ) );

/* -------------------------------------------------------------------- */
/*      Write out the raw raster data.                                  */
/* -------------------------------------------------------------------- */
        sprintf( szFilename, "Band_%d.raw", i+1 );
        fp = VSIFOpen( szFilename, "wb" );
        CPLAssert( fp != NULL );

        nPixelSize = GDALGetDataTypeSize( GDALGetRasterDataType(hBand) ) / 8;

        pabyBlock = (GByte *) CPLMalloc(nPixelSize*nRasterXSize);

        for( iBlockY = 0; iBlockY < nRasterYSize; iBlockY++ )
        {
            GDALRasterIO( hBand, GF_Read,
                          0, iBlockY, nRasterXSize, 1,
                          pabyBlock, nRasterXSize, 1,
                          GDALGetRasterDataType(hBand),
                          0, 0 );
            
            VSIFWrite( pabyBlock, nPixelSize, nRasterXSize, fp);
        }

        CPLFree( pabyBlock );

        VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Write out a corresponding .aux file.                            */
/* -------------------------------------------------------------------- */
        sprintf( szFilename, "Band_%d.aux", i+1 );
        fp = VSIFOpen( szFilename, "wt" );
        CPLAssert( fp != NULL );

        switch( GDALGetRasterDataType(hBand) )
        {
          case GDT_Byte:
            pszDataTypeName = "8U";
            break;
            
          case GDT_Float32:
            pszDataTypeName = "32R";
            break;
            
          case GDT_UInt16:
            pszDataTypeName = "16U";
            break;
            
          case GDT_Int16:
            pszDataTypeName = "16S";
            break;

          default:
            CPLAssert( FALSE );
        }

        fprintf( fp, "AuxilaryTarget: Band_%d.raw\n", i+1 );
        fprintf( fp, "RawDefinition: %d %d 1\n", nRasterXSize, nRasterYSize );
        fprintf( fp, "ChanDefinition-1: %s 0 %d %d Swapped\n",
                 pszDataTypeName, nPixelSize, nPixelSize * nRasterXSize );

        VSIFClose( fp );
    }

    GDALClose( hDataset );
    
    exit( 0 );
}

/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Mainline for Imagine to TIFF translation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 * Revision 1.1  1999/01/22 17:40:43  warmerda
 * New
 *
 */

#include "hfa.h"

#include "tiffiop.h"
#include "xtiffio.h"

static void ImagineBandToGeoTIFF( HFAHandle, int, const char * );
CPLErr ImagineToGeoTIFFProjection( HFAHandle hHFA, TIFF * hTIFF );

CPL_C_START
CPLErr CopyPyramidsToTiff( HFAHandle, int, TIFF * );
CPL_C_END

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage()

{
    printf(
      "Usage: img2tif ...\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    int		i, nBandCount, nBand;
    const char	*pszSrcFilename = NULL;
    const char	*pszDstBasename = NULL;
    HFAHandle   hHFA;

/* -------------------------------------------------------------------- */
/*      Parse commandline options.                                      */
/* -------------------------------------------------------------------- */
    for( i = 1; i < nArgc; i++ )
    {
        if( EQUAL(papszArgv[i],"-i") && i+1 < nArgc )
        {
            pszSrcFilename = papszArgv[i+1];
            i++;
        }
        else if( EQUAL(papszArgv[i],"-o") && i+1 < nArgc )
        {
            pszDstBasename = papszArgv[i+1];
            i++;
        }
        else
        {
            printf( "Unexpected argument: %s\n\n", papszArgv[i] );
            Usage();
        }
    }

    if( pszSrcFilename == NULL )
    {
        printf( "No source file provided.\n\n" );
        Usage();
    }
    
    if( pszDstBasename == NULL )
    {
        printf( "No destination file provided.\n\n" );
        Usage();
    }
    
/* -------------------------------------------------------------------- */
/*      Open the imagine file.                                          */
/* -------------------------------------------------------------------- */
    hHFA = HFAOpen( pszSrcFilename, "r" );

    if( hHFA == NULL )
        exit( 100 );

/* -------------------------------------------------------------------- */
/*      Loop over all bands, generating each TIFF file.                 */
/* -------------------------------------------------------------------- */
    HFAGetRasterInfo( hHFA, NULL, NULL, &nBandCount );

    for( nBand = 1; nBand <= nBandCount; nBand++ )
    {
        ImagineBandToGeoTIFF( hHFA, nBand, pszDstBasename );
    }

    HFAClose( hHFA );

    return 0;
}

/************************************************************************/
/*                      ImagineToGeoTIFFPalette()                       */
/************************************************************************/

void ImagineToGeoTIFFPalette( HFAHandle hHFA, int nBand, TIFF * hTIFF )

{
    unsigned short	anTRed[256], anTGreen[256], anTBlue[256];
    double	*padfRed, *padfGreen, *padfBlue;
    int		nColors, i;
    
    HFAGetPCT( hHFA, nBand, &nColors, &padfRed, &padfGreen, &padfBlue );
    CPLAssert( nColors > 0 );

    for( i = 0; i < 256; i++ )
    {
        if( i < nColors )
        {
            anTRed[i] = (unsigned short) (65535 * padfRed[i]);
            anTGreen[i] = (unsigned short) (65535 * padfGreen[i]);
            anTBlue[i] = (unsigned short) (65535 * padfBlue[i]);
        }
        else
        {
            anTRed[i] = 0;
            anTGreen[i] = 0;
            anTBlue[i] = 0;
        }
    }

    TIFFSetField( hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
}

/************************************************************************/
/*                     ImagineToGeoTIFFDataRange()                      */
/************************************************************************/

static void ImagineToGeoTIFFDataRange( HFAHandle hHFA, int nBand, TIFF *hTIFF)

{
    double	dfMin, dfMax;
    unsigned short	nTMin, nTMax;

    if( HFAGetDataRange( hHFA, nBand, &dfMin, &dfMax ) != CE_None )
        return;

    if( dfMin < 0 || dfMin > 65536 || dfMax < 0 || dfMax > 65535
        || dfMin >= dfMax )
        return;


    nTMin = (unsigned short) dfMin;
    nTMax = (unsigned short) dfMax;
    
    TIFFSetField( hTIFF, TIFFTAG_MINSAMPLEVALUE, nTMin );
    TIFFSetField( hTIFF, TIFFTAG_MAXSAMPLEVALUE, nTMax );
}

/************************************************************************/
/*                        ImagineBandToGeoTIFF()                        */
/************************************************************************/

static void ImagineBandToGeoTIFF( HFAHandle hHFA, int nBand,
                                  const char * pszDstBasename )

{
    TIFF	*hTIFF;
    char	szDstFilename[1024];
    int		nXSize, nYSize, nBlockXSize, nBlockYSize, nDataType;
    int		iBlockX, iBlockY, nBlocksPerRow, nBlocksPerColumn, nTileSize;
    void	*pData;
    double	*padfRed, *padfGreen, *padfBlue;
    int		nColors;

    HFAGetRasterInfo( hHFA, &nXSize, &nYSize, NULL );
    HFAGetBandInfo( hHFA, nBand, &nDataType, &nBlockXSize, &nBlockYSize );
    HFAGetPCT( hHFA, nBand, &nColors, &padfRed, &padfGreen, &padfBlue );

    nBlocksPerRow = (nXSize + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (nYSize + nBlockYSize - 1) / nBlockYSize;
    
/* -------------------------------------------------------------------- */
/*      Create the new file.                                            */
/* -------------------------------------------------------------------- */
    sprintf( szDstFilename, "%s%d.tif", pszDstBasename, nBand );

    hTIFF = XTIFFOpen( szDstFilename, "w+" );

/* -------------------------------------------------------------------- */
/*      Write standard header fields.                                   */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  HFAGetDataTypeBits(nDataType) );

    /* notdef: should error on illegal types */

    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );

    TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
    TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );

    if( nColors > 0 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    
/* -------------------------------------------------------------------- */
/*	Do we have min/max value information?				*/
/* -------------------------------------------------------------------- */
    ImagineToGeoTIFFDataRange( hHFA, nBand, hTIFF );

/* -------------------------------------------------------------------- */
/*	Allocate a block buffer.					*/
/* -------------------------------------------------------------------- */
    nTileSize = TIFFTileSize( hTIFF );
    pData = VSIMalloc(nTileSize);
    if( pData == NULL )
    {
        printf( "Out of memory allocating working tile of %d bytes.\n",
                nTileSize );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Write each of the tiles.                                        */
/* -------------------------------------------------------------------- */
    for( iBlockY = 0; iBlockY < nBlocksPerColumn; iBlockY++ )
    {
        for( iBlockX = 0; iBlockX < nBlocksPerRow; iBlockX++ )
        {
            int	iBlock = iBlockX + iBlockY * nBlocksPerRow;

            if( HFAGetRasterBlock( hHFA, nBand, iBlockX, iBlockY, pData )
                != CE_None )
                return;

            if( HFAGetDataTypeBits(nDataType) == 16 )
            {
                int		ii;

                for( ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
                {
                    unsigned char *pabyData = (unsigned char *) pData;
                    int		nTemp;

                    nTemp = pabyData[ii*2];
                    pabyData[ii*2] = pabyData[ii*2+1];
                    pabyData[ii*2+1] = nTemp;
                }
            }

            if( TIFFWriteEncodedTile( hTIFF, iBlock, pData, nTileSize ) < 1 )
                return;
        }
    }

    VSIFree( pData );

/* -------------------------------------------------------------------- */
/*      Write Geotiff information.                                      */
/* -------------------------------------------------------------------- */
    ImagineToGeoTIFFProjection( hHFA, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write Palette                                                   */
/* -------------------------------------------------------------------- */
    if( nColors > 0 )
        ImagineToGeoTIFFPalette( hHFA, nBand, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write overviews                                                 */
/* -------------------------------------------------------------------- */
    CopyPyramidsToTiff( hHFA, nBand, hTIFF );

    XTIFFClose( hTIFF );
}

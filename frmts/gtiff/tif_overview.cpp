/******************************************************************************
 * $Id$
 *
 * Project:  TIFF Overview Builder
 * Purpose:  Library function for building overviews in a TIFF file.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 * Notes:
 *  o This module uses the RawBlockedImage class to hold the overviews as
 *    they are being built since we can't easily be reading from one directory
 *    in a TIFF file, and writing to a bunch of others.
 *
 *  o RawBlockedImage will create temporary files in the current directory
 *    to cache the overviews so it doesn't have to hold them all in memory.
 *    If the application crashes these will not be deleted (*.rbi).
 *
 *  o Currently only images with samples_per_pixel=1, and bits_per_sample of
 *    a multiple of eight will work.
 *
 *  o The downsampler currently just takes the top left pixel from the
 *    source rectangle.  Eventually sampling options of averaging, mode, and
 *    ``center pixel'' should be offered.
 *
 *  o The code will attempt to use the same kind of compression,
 *    photometric interpretation, and organization as the source image, but
 *    it doesn't copy geotiff tags to the reduced resolution images. 
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
 * Revision 1.2  1999/02/11 18:37:43  warmerda
 * Removed debugging malloc stuff.
 *
 * Revision 1.1  1999/02/11 18:12:30  warmerda
 * New
 *
 */

#include "tiffio.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "rawblockedimage.h"

extern "C" {
    void TIFF_BuildOverviews( const char *, int, int * );
}

/************************************************************************/
/*                         TIFF_WriteOverview()                         */
/************************************************************************/

static
void TIFF_WriteOverview( TIFF *hTIFF, RawBlockedImage *poRBI, int bTiled,
                         int nCompressFlag, int nPhotometric,
                         GUInt16 *panRed, GUInt16 *panGreen, GUInt16 *panBlue )

{
    int		iTileX, iTileY;
                                   
/* -------------------------------------------------------------------- */
/*      Setup TIFF fields.                                              */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, poRBI->GetXSize() );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, poRBI->GetYSize() );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG,
                  PLANARCONFIG_SEPARATE );

    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, poRBI->GetBitsPerPixel() );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompressFlag );
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, nPhotometric );

    if( bTiled )
    {
        TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, poRBI->GetBlockXSize() );
        TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, poRBI->GetBlockYSize() );
    }
    else
        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, poRBI->GetBlockYSize() );

    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE );
    
/* -------------------------------------------------------------------- */
/*	Write color table if one is present.				*/
/* -------------------------------------------------------------------- */
    if( panRed != NULL )
    {
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP, panRed, panGreen, panBlue );
    }

/* -------------------------------------------------------------------- */
/*      Write blocks to TIFF file.                                      */
/* -------------------------------------------------------------------- */
    for( iTileY = 0;
         iTileY*poRBI->GetBlockYSize() < poRBI->GetYSize();
         iTileY++ )
    {
        for( iTileX = 0;
             iTileX*poRBI->GetBlockXSize() < poRBI->GetXSize();
             iTileX++ )
        {
            GByte	*pabyData = poRBI->GetTile( iTileX, iTileY );

            if( bTiled )
            {
                TIFFWriteEncodedTile( hTIFF,
                          TIFFComputeTile(hTIFF,
                                          iTileX * poRBI->GetBlockXSize(),
                                          iTileY * poRBI->GetBlockYSize(),
                                          0, 0 ),
                                      pabyData, TIFFTileSize(hTIFF) );
            }
            else
            {
                TIFFWriteEncodedStrip( hTIFF, iTileY, pabyData,
                                       TIFFStripSize( hTIFF ) );
            }
        }
    }

    TIFFWriteDirectory( hTIFF );
}

/************************************************************************/
/*                          TIFF_DownSample()                           */
/*                                                                      */
/*      Down sample a tile of full res data into a window of a tile     */
/*      of downsampled data.                                            */
/************************************************************************/

static
void TIFF_DownSample( GByte * pabySrcTile, int nBlockXSize, int nBlockYSize,
                      int nBitsPerPixel,
                      GByte * pabyOTile, int nOBlockXSize, int nOBlockYSize,
                      int nTXOff, int nTYOff, int nOMult )

{
    int		i, j, k, nBytes = nBitsPerPixel / 8;
    GByte	*pabySrc, *pabyDst;

    CPLAssert( nBitsPerPixel >= 8 );

/* -------------------------------------------------------------------- */
/*      Handle case of one or more whole bytes per sample.              */
/* -------------------------------------------------------------------- */
    for( j = 0; j*nOMult < nBlockYSize; j++ )
    {
        if( j + nTYOff >= nOBlockYSize )
            break;
            
        pabySrc = pabySrcTile + j*nOMult*nBlockXSize * nBytes;
        pabyDst = pabyOTile + ((j+nTYOff)*nOBlockXSize + nTXOff) * nBytes;

        for( i = 0; i*nOMult < nBlockXSize; i++ )
        {
            if( i + nTXOff >= nOBlockXSize )
                break;
            
            /*
             * For now use simple subsampling, from the top left corner
             * of the source block of pixels.
             */

            for( k = 0; k < nBytes; k++ )
            {
                *(pabyDst++) = pabySrc[k];
            }
            
            pabySrc += nOMult * nBytes;
        }
    }
}

/************************************************************************/
/*                        TIFF_BuildOverviews()                         */
/*                                                                      */
/*      Build the requested list of overviews.  Overviews are           */
/*      maintained in a bunch of temporary files and then these are     */
/*      written back to the TIFF file.  Only one pass through the       */
/*      source TIFF file is made for any number of output               */
/*      overviews.                                                      */
/************************************************************************/

void TIFF_BuildOverviews( const char * pszTIFFFilename,
                          int nOverviews, int * panOvList )

{
    RawBlockedImage	**papoRawBIs;
    uint32		nXSize, nYSize, nBlockXSize, nBlockYSize;
    uint16		nBitsPerPixel, nPhotometric, nCompressFlag;
    int			bTiled, nSXOff, nSYOff, i;
    GByte		*pabySrcTile;
    TIFF		*hTIFF;
    GUInt16		*panRedMap, *panGreenMap, *panBlueMap;

/* -------------------------------------------------------------------- */
/*      Get the base raster size.                                       */
/* -------------------------------------------------------------------- */
    hTIFF = TIFFOpen( pszTIFFFilename, "r" );
    if( hTIFF == NULL )
    {
        fprintf( stderr, "TIFFOpen(%s) failed.\n", pszTIFFFilename );
        exit( 1 );
    }

    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );

    TIFFGetField( hTIFF, TIFFTAG_BITSPERSAMPLE, &nBitsPerPixel );

    TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &nPhotometric );
    TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &nCompressFlag );

    if( nBitsPerPixel < 8 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File `%s' has samples of %d bits per sample.  Sample\n"
                  "sizes of less than 8 bits per sample are not supported.\n",
                  pszTIFFFilename, nBitsPerPixel );
        return;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the base raster block size.                                 */
/* -------------------------------------------------------------------- */
    if( TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP, &(nBlockYSize) ) )
    {
        nBlockXSize = nXSize;
        bTiled = FALSE;
    }
    else
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &nBlockXSize );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &nBlockYSize );
        bTiled = TRUE;
    }

/* -------------------------------------------------------------------- */
/*	Capture the pallette if there is one.				*/
/* -------------------------------------------------------------------- */
    if( TIFFGetField( hTIFF, TIFFTAG_COLORMAP,
                      &panRedMap, &panGreenMap, &panBlueMap ) )
    {
        GUInt16		*panRed2, *panGreen2, *panBlue2;

        panRed2 = (GUInt16 *) CPLCalloc(2,256);
        panGreen2 = (GUInt16 *) CPLCalloc(2,256);
        panBlue2 = (GUInt16 *) CPLCalloc(2,256);

        memcpy( panRed2, panRedMap, 512 );
        memcpy( panGreen2, panGreenMap, 512 );
        memcpy( panBlue2, panBlueMap, 512 );

        panRedMap = panRed2;
        panGreenMap = panGreen2;
        panBlueMap = panBlue2;
    }
    else
    {
        panRedMap = panGreenMap = panBlueMap = NULL;
    }
        
/* -------------------------------------------------------------------- */
/*      Initialize the overview raw layers                              */
/* -------------------------------------------------------------------- */
    papoRawBIs = (RawBlockedImage **) CPLCalloc(nOverviews,sizeof(void*));
    for( i = 0; i < nOverviews; i++ )
    {
        int	nOXSize, nOYSize, nOBlockXSize, nOBlockYSize;

        nOXSize = (nXSize + panOvList[i] - 1) / panOvList[i];
        nOYSize = (nYSize + panOvList[i] - 1) / panOvList[i];

        nOBlockXSize = MIN((int)nBlockXSize,nOXSize);
        nOBlockYSize = MIN((int)nBlockYSize,nOYSize);

        papoRawBIs[i] = new RawBlockedImage( nOXSize, nOYSize,
                                             nOBlockXSize, nOBlockYSize,
                                             nBitsPerPixel );
    }

/* -------------------------------------------------------------------- */
/*      Allocate a buffer to hold a source block.                       */
/* -------------------------------------------------------------------- */
    if( bTiled )
        pabySrcTile = (GByte *) CPLMalloc(TIFFTileSize(hTIFF));
    else
        pabySrcTile = (GByte *) CPLMalloc(TIFFStripSize(hTIFF));
    
/* -------------------------------------------------------------------- */
/*      Loop over the source raster, applying data to the               */
/*      destination raster.                                             */
/* -------------------------------------------------------------------- */
    for( nSYOff = 0; nSYOff < (int) nYSize; nSYOff += nBlockYSize )
    {
        for( nSXOff = 0; nSXOff < (int) nXSize; nSXOff += nBlockXSize )
        {
            int		iOverview;

            /*
             * Read the source tile/strip
             */
            if( bTiled )
            {
                TIFFReadEncodedTile( hTIFF,
                                     TIFFComputeTile(hTIFF, nSXOff, nSYOff,
                                                     0, 0 ),
                                     pabySrcTile,
                                     TIFFTileSize(hTIFF));
            }
            else
            {
                TIFFReadEncodedStrip( hTIFF, nSYOff / nBlockYSize,
                                      pabySrcTile,
                                      TIFFTileSize(hTIFF) );
            }

            /*
             * Loop over destination overview layers
             */
            for( iOverview = 0; iOverview < nOverviews; iOverview++ )
            {
                RawBlockedImage *poRBI = papoRawBIs[iOverview];
                GByte	*pabyOTile;
                int	nTXOff, nTYOff, nOXOff, nOYOff, nOMult;
                int	nOBlockXSize = poRBI->GetBlockXSize();
                int	nOBlockYSize = poRBI->GetBlockYSize();

                /*
                 * Fetch the destination overview tile
                 */
                nOMult = panOvList[iOverview];
                nOXOff = (nSXOff/nOMult) / nOBlockXSize;
                nOYOff = (nSYOff/nOMult) / nOBlockYSize;
                pabyOTile = poRBI->GetTileForUpdate( nOXOff, nOYOff );
                
                /*
                 * Establish the offset into this tile at which we should
                 * start placing data.
                 */
                nTXOff = (nSXOff - nOXOff*nOMult*nOBlockXSize) / nOMult;
                nTYOff = (nSYOff - nOYOff*nOMult*nOBlockYSize) / nOMult;

                /*
                 * Perform the downsampling.
                 */
                TIFF_DownSample( pabySrcTile, nBlockXSize, nBlockYSize,
                                 nBitsPerPixel,
                                 pabyOTile,
                                 poRBI->GetBlockXSize(),
                                 poRBI->GetBlockYSize(),
                                 nTXOff, nTYOff,
                                 nOMult );
            }
        }
    }

    CPLFree( pabySrcTile );

    TIFFClose( hTIFF );

/* ==================================================================== */
/*      We now have the overview rasters built, and held as             */
/*      RawBlockedImage's.  Now we need to write them to new TIFF       */
/*      layers.                                                         */
/* ==================================================================== */
    hTIFF = TIFFOpen( pszTIFFFilename, "a" );
    if( hTIFF == NULL )
    {
        fprintf( stderr,
                 "TIFFOpen(%s,\"a\") failed.  No overviews written.\n"
                 "Do you have write permissions on that file?\n",
                 pszTIFFFilename );
    }
    else
    {
        for( i = 0; i < nOverviews; i++ )
        {
            TIFF_WriteOverview( hTIFF, papoRawBIs[i], bTiled,
                                nCompressFlag, nPhotometric,
                                panRedMap, panGreenMap, panBlueMap );
        }
        
        TIFFClose( hTIFF );
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup the rawblockedimage files.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nOverviews; i++ )
    {
        delete papoRawBIs[i];
    }

    CPLFree( papoRawBIs );
    CPLFree( panRedMap );
    CPLFree( panGreenMap );
    CPLFree( panBlueMap );
}

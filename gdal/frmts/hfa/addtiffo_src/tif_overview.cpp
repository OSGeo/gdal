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
 *  o Currently only images with bits_per_sample of a multiple of eight
 *    will work.
 *
 *  o The downsampler currently just takes the top left pixel from the
 *    source rectangle.  Eventually sampling options of averaging, mode, and
 *    ``center pixel'' should be offered.
 *
 *  o The code will attempt to use the same kind of compression,
 *    photometric interpretation, and organization as the source image, but
 *    it doesn't copy geotiff tags to the reduced resolution images.
 *
 *  o Reduced resolution overviews for multi-sample files will currently
 *    always be generated as PLANARCONFIG_SEPARATE.  This could be fixed
 *    reasonable easily if needed to improve compatibility with other
 *    packages.  Many don't properly support PLANARCONFIG_SEPARATE. 
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
 * Revision 1.2  2006/05/15 19:18:14  fwarmerdam
 * include string.h
 *
 * Revision 1.1  1999/11/29 21:33:22  warmerda
 * New
 *
 * Revision 1.1  1999/08/17 01:47:59  warmerda
 * New
 *
 * Revision 1.7  1999/03/12 17:47:26  warmerda
 * made independent of CPL
 *
 * Revision 1.6  1999/02/24 16:24:00  warmerda
 * Don't include cpl_string.h
 *
 * Revision 1.5  1999/02/11 22:27:12  warmerda
 * Added multi-sample support
 *
 * Revision 1.4  1999/02/11 19:23:39  warmerda
 * Only fix on multiples of 16 in block size if it is a tiled file.
 *
 * Revision 1.3  1999/02/11 19:21:14  warmerda
 * Limit tile sizes to multiples of 16
 *
 * Revision 1.2  1999/02/11 18:37:43  warmerda
 * Removed debugging malloc stuff.
 *
 * Revision 1.1  1999/02/11 18:12:30  warmerda
 * New
 *
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "tiffio.h"
#include "rawblockedimage.h"

#ifndef FALSE
#  define FALSE 0
#  define TRUE 1
#endif

#ifndef MAX
#  define MIN(a,b)      ((a<b) ? a : b)
#  define MAX(a,b)      ((a>b) ? a : b)
#endif

extern "C" {
    void TIFFBuildOverviews( const char *, int, int *, int );
}

/************************************************************************/
/*                         TIFF_WriteOverview()                         */
/************************************************************************/

static
void TIFF_WriteOverview( TIFF *hTIFF, int nSamples, RawBlockedImage **papoRBI,
                         int bTiled, int nCompressFlag, int nPhotometric,
                         unsigned short *panRed,
                         unsigned short *panGreen,
                         unsigned short *panBlue,
                         int bUseSubIFDs )

{
    int		iSample;
    RawBlockedImage	*poRBI = papoRBI[0];
                                   
/* -------------------------------------------------------------------- */
/*      Setup TIFF fields.                                              */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, poRBI->GetXSize() );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, poRBI->GetYSize() );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG,
                  PLANARCONFIG_SEPARATE );

    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, poRBI->GetBitsPerPixel() );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, nSamples );
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
    for( iSample = 0; iSample < nSamples; iSample++ )
    {
        int		iTileX, iTileY;
        
        poRBI = papoRBI[iSample];
        
        for( iTileY = 0;
             iTileY*poRBI->GetBlockYSize() < poRBI->GetYSize();
             iTileY++ )
        {
            for( iTileX = 0;
                 iTileX*poRBI->GetBlockXSize() < poRBI->GetXSize();
                 iTileX++ )
            {
                unsigned char	*pabyData = poRBI->GetTile( iTileX, iTileY );
                int	nTileID;

                if( bTiled )
                {
                    nTileID =
                        TIFFComputeTile(hTIFF,
                                        iTileX * poRBI->GetBlockXSize(),
                                        iTileY * poRBI->GetBlockYSize(),
                                        0, iSample );
                    TIFFWriteEncodedTile( hTIFF, nTileID, 
                                          pabyData, TIFFTileSize(hTIFF) );
                }
                else
                {
                    nTileID =
                        TIFFComputeStrip(hTIFF, iTileY*poRBI->GetBlockYSize(),
                                         iSample);

                    TIFFWriteEncodedStrip( hTIFF, nTileID,
                                           pabyData, TIFFStripSize( hTIFF ) );
                }
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
void TIFF_DownSample( unsigned char *pabySrcTile,
                      int nBlockXSize, int nBlockYSize,
                      int nPixelSkewBits, int nBitsPerPixel,
                      unsigned char * pabyOTile,
                      int nOBlockXSize, int nOBlockYSize,
                      int nTXOff, int nTYOff, int nOMult )

{
    int		i, j, k, nPixelBytes = (nBitsPerPixel) / 8;
    int		nPixelGroupBytes = (nBitsPerPixel+nPixelSkewBits)/8;
    unsigned char *pabySrc, *pabyDst;

    assert( nBitsPerPixel >= 8 );

/* -------------------------------------------------------------------- */
/*      Handle case of one or more whole bytes per sample.              */
/* -------------------------------------------------------------------- */
    for( j = 0; j*nOMult < nBlockYSize; j++ )
    {
        if( j + nTYOff >= nOBlockYSize )
            break;
            
        pabySrc = pabySrcTile + j*nOMult*nBlockXSize * nPixelGroupBytes;
        pabyDst = pabyOTile
            + ((j+nTYOff)*nOBlockXSize + nTXOff) * nPixelBytes;

        for( i = 0; i*nOMult < nBlockXSize; i++ )
        {
            if( i + nTXOff >= nOBlockXSize )
                break;
            
            /*
             * For now use simple subsampling, from the top left corner
             * of the source block of pixels.
             */

            for( k = 0; k < nPixelBytes; k++ )
            {
                *(pabyDst++) = pabySrc[k];
            }
            
            pabySrc += nOMult * nPixelGroupBytes;
        }
    }
}

/************************************************************************/
/*                      TIFF_ProcessFullResBlock()                      */
/*                                                                      */
/*      Process one block of full res data, downsampling into each      */
/*      of the overviews.                                               */
/************************************************************************/

void TIFF_ProcessFullResBlock( TIFF *hTIFF, int nPlanarConfig,
                               int nOverviews, int * panOvList,
                               int nBitsPerPixel, 
                               int nSamples, RawBlockedImage ** papoRawBIs,
                               int nSXOff, int nSYOff,
                               unsigned char *pabySrcTile,
                               int nBlockXSize, int nBlockYSize )

{
    int		iOverview, iSample;

    for( iSample = 0; iSample < nSamples; iSample++ )
    {
        /*
         * We have to read a tile/strip for each sample for
         * PLANARCONFIG_SEPARATE.  Otherwise, we just read all the samples
         * at once when handling the first sample.
         */
        if( nPlanarConfig == PLANARCONFIG_SEPARATE || iSample == 0 )
        {
            if( TIFFIsTiled(hTIFF) )
            {
                TIFFReadEncodedTile( hTIFF,
                                     TIFFComputeTile(hTIFF, nSXOff, nSYOff,
                                                     0, iSample ),
                                     pabySrcTile,
                                     TIFFTileSize(hTIFF));
            }
            else
            {
                TIFFReadEncodedStrip( hTIFF,
                                      TIFFComputeStrip(hTIFF, nSYOff, iSample),
                                      pabySrcTile,
                                      TIFFStripSize(hTIFF) );
            }
        }

        /*        
         * Loop over destination overview layers
         */
        for( iOverview = 0; iOverview < nOverviews; iOverview++ )
        {
            RawBlockedImage *poRBI = papoRawBIs[iOverview*nSamples + iSample];
            unsigned char *pabyOTile;
            int	nTXOff, nTYOff, nOXOff, nOYOff, nOMult;
            int	nOBlockXSize = poRBI->GetBlockXSize();
            int	nOBlockYSize = poRBI->GetBlockYSize();
            int	nSkewBits, nSampleByteOffset; 

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
             * Figure out the skew (extra space between ``our samples'') and
             * the byte offset to the first sample.
             */
            assert( (nBitsPerPixel % 8) == 0 );
            if( nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                nSkewBits = 0;
                nSampleByteOffset = 0;
            }
            else
            {
                nSkewBits = nBitsPerPixel * (nSamples-1);
                nSampleByteOffset = (nBitsPerPixel/8) * iSample;
            }
            
            /*
             * Perform the downsampling.
             */
#ifdef DBMALLOC
            malloc_chain_check( 1 );
#endif
            TIFF_DownSample( pabySrcTile + nSampleByteOffset,
                             nBlockXSize, nBlockYSize,
                             nSkewBits, nBitsPerPixel, pabyOTile,
                             poRBI->GetBlockXSize(),
                             poRBI->GetBlockYSize(),
                             nTXOff, nTYOff,
                             nOMult );
#ifdef DBMALLOC
            malloc_chain_check( 1 );
#endif            
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

void TIFFBuildOverviews( const char * pszTIFFFilename,
                         int nOverviews, int * panOvList,
                         int bUseSubIFDs )

{
    RawBlockedImage	**papoRawBIs;
    uint32		nXSize, nYSize, nBlockXSize, nBlockYSize;
    uint16		nBitsPerPixel, nPhotometric, nCompressFlag, nSamples,
                        nPlanarConfig;
    int			bTiled, nSXOff, nSYOff, i, iSample;
    unsigned char	*pabySrcTile;
    TIFF		*hTIFF;
    uint16		*panRedMap, *panGreenMap, *panBlueMap;

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
    TIFFGetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamples );
    TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &nPlanarConfig );

    TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &nPhotometric );
    TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &nCompressFlag );

    if( nBitsPerPixel < 8 )
    {
        TIFFError( "TIFFBuildOverviews",
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
        uint16		*panRed2, *panGreen2, *panBlue2;

        panRed2 = (uint16 *) calloc(2,256);
        panGreen2 = (uint16 *) calloc(2,256);
        panBlue2 = (uint16 *) calloc(2,256);

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
    papoRawBIs = (RawBlockedImage **)
        calloc(nOverviews*nSamples,sizeof(void*));

    for( i = 0; i < nOverviews; i++ )
    {
        int	nOXSize, nOYSize, nOBlockXSize, nOBlockYSize;

        nOXSize = (nXSize + panOvList[i] - 1) / panOvList[i];
        nOYSize = (nYSize + panOvList[i] - 1) / panOvList[i];

        nOBlockXSize = MIN((int)nBlockXSize,nOXSize);
        nOBlockYSize = MIN((int)nBlockYSize,nOYSize);

        if( bTiled )
        {
            if( (nOBlockXSize % 16) != 0 )
                nOBlockXSize = nOBlockXSize + 16 - (nOBlockXSize % 16);
            
            if( (nOBlockYSize % 16) != 0 )
                nOBlockYSize = nOBlockYSize + 16 - (nOBlockYSize % 16);
        }

        for( iSample = 0; iSample < nSamples; iSample++ )
        {
            papoRawBIs[i*nSamples + iSample] =
                new RawBlockedImage( nOXSize, nOYSize,
                                     nOBlockXSize, nOBlockYSize,
                                     nBitsPerPixel );
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate a buffer to hold a source block.                       */
/* -------------------------------------------------------------------- */
    if( bTiled )
        pabySrcTile = (unsigned char *) malloc(TIFFTileSize(hTIFF));
    else
        pabySrcTile = (unsigned char *) malloc(TIFFStripSize(hTIFF));
    
/* -------------------------------------------------------------------- */
/*      Loop over the source raster, applying data to the               */
/*      destination raster.                                             */
/* -------------------------------------------------------------------- */
    for( nSYOff = 0; nSYOff < (int) nYSize; nSYOff += nBlockYSize )
    {
        for( nSXOff = 0; nSXOff < (int) nXSize; nSXOff += nBlockXSize )
        {
            /*
             * Read and resample into the various overview images.
             */
            
            TIFF_ProcessFullResBlock( hTIFF, nPlanarConfig,
                                      nOverviews, panOvList,
                                      nBitsPerPixel, nSamples, papoRawBIs,
                                      nSXOff, nSYOff, pabySrcTile,
                                      nBlockXSize, nBlockYSize );
        }
    }

    free( pabySrcTile );

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
            TIFF_WriteOverview( hTIFF, nSamples, papoRawBIs + i*nSamples,
                                bTiled, nCompressFlag, nPhotometric,
                                panRedMap, panGreenMap, panBlueMap,
                                bUseSubIFDs );
        }
        
        TIFFClose( hTIFF );
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup the rawblockedimage files.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nOverviews*nSamples; i++ )
    {
        delete papoRawBIs[i];
    }

    if( papoRawBIs != NULL )
        free( papoRawBIs );

    if( panRedMap != NULL )
    {
        free( panRedMap );
        free( panGreenMap );
        free( panBlueMap );
    }
}

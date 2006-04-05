/******************************************************************************
 * tif_ovrcache.c,v 1.1 2000/11/24 18:13:43 warmerda Exp
 *
 * Project:  TIFF Overview Builder
 * Purpose:  Library functions to maintain two rows of tiles or two strips
 *           of data for output overviews as an output cache. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * tif_ovrcache.c,v
 * Revision 1.1  2000/11/24 18:13:43  warmerda
 * New
 *
 * Revision 1.2  2000/01/28 15:42:25  warmerda
 * Avoid warnings on Windows.
 *
 * Revision 1.1  2000/01/28 15:03:44  warmerda
 * New
 *
 */

#include "cpl_port.h"
#include "tiffio.h"
#include "tif_ovrcache.h"
#include <assert.h>

CPL_CVSID("tif_ovrcache.c,v 1.2 2001/07/18 04:51:56 warmerda Exp");

/************************************************************************/
/*                         TIFFCreateOvrCache()                         */
/*                                                                      */
/*      Create an overview cache to hold two rows of blocks from an     */
/*      existing TIFF directory.                                        */
/************************************************************************/

TIFFOvrCache *TIFFCreateOvrCache( TIFF *hTIFF, int nDirOffset )

{
    TIFFOvrCache	*psCache;
    int			nBytesPerRow;
    uint32		nBaseDirOffset;

    psCache = (TIFFOvrCache *) _TIFFmalloc(sizeof(TIFFOvrCache));
    psCache->nDirOffset = nDirOffset;
    psCache->hTIFF = hTIFF;
    
/* -------------------------------------------------------------------- */
/*      Get definition of this raster from the TIFF file itself.        */
/* -------------------------------------------------------------------- */
    nBaseDirOffset = TIFFCurrentDirOffset( psCache->hTIFF );
    TIFFSetSubDirectory( hTIFF, nDirOffset );
    
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &(psCache->nXSize) );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &(psCache->nYSize) );

    TIFFGetField( hTIFF, TIFFTAG_BITSPERSAMPLE, &(psCache->nBitsPerPixel) );
    TIFFGetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, &(psCache->nSamples) );

    if( !TIFFIsTiled( hTIFF ) )
    {
        TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP, &(psCache->nBlockYSize) );
        psCache->nBlockXSize = psCache->nXSize;
        psCache->bTiled = FALSE;
    }
    else
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(psCache->nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(psCache->nBlockYSize) );
        psCache->bTiled = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Compute some values from this.                                  */
/* -------------------------------------------------------------------- */

    psCache->nBlocksPerRow = (psCache->nXSize + psCache->nBlockXSize - 1)
        		/ psCache->nBlockXSize;
    psCache->nBlocksPerColumn = (psCache->nYSize + psCache->nBlockYSize - 1)
        		/ psCache->nBlockYSize;
    
    psCache->nBytesPerBlock =
        (psCache->nBlockXSize*psCache->nBlockYSize*psCache->nBitsPerPixel + 7) / 8;

/* -------------------------------------------------------------------- */
/*      Allocate and initialize the data buffers.                       */
/* -------------------------------------------------------------------- */
    nBytesPerRow = psCache->nBytesPerBlock * psCache->nBlocksPerRow
                    * psCache->nSamples;

    psCache->pabyRow1Blocks = (unsigned char *) _TIFFmalloc(nBytesPerRow);
    psCache->pabyRow2Blocks = (unsigned char *) _TIFFmalloc(nBytesPerRow);

    if( psCache->pabyRow1Blocks == NULL
        || psCache->pabyRow2Blocks == NULL )
    {
        TIFFError( "TIFFCreateOvrCache",
                   "Can't allocate memory for overview cache." );
        return NULL;
    }

    _TIFFmemset( psCache->pabyRow1Blocks, 0, nBytesPerRow );
    _TIFFmemset( psCache->pabyRow2Blocks, 0, nBytesPerRow );

    psCache->nBlockOffset = 0;

    TIFFSetSubDirectory( psCache->hTIFF, nBaseDirOffset );
    
    return psCache;
}

/************************************************************************/
/*                          TIFFWriteOvrRow()                           */
/*                                                                      */
/*      Write one entire row of blocks (row 1) to the tiff file, and    */
/*      then rotate the block buffers, essentially moving things        */
/*      down by one block.                                              */
/************************************************************************/

static void TIFFWriteOvrRow( TIFFOvrCache * psCache )

{
    int		nRet, iSample, iTileX, iTileY = psCache->nBlockOffset;
    unsigned char *pabyData;
    uint32	nBaseDirOffset;
    
/* -------------------------------------------------------------------- */
/*	If the output cache is multi-byte per sample, and the file	*/
/*	being written to is of a different byte order than the current	*/
/*	platform, we will need to byte swap the data. 			*/
/* -------------------------------------------------------------------- */
    if( TIFFIsByteSwapped(psCache->hTIFF) )
    {
        if( psCache->nBitsPerPixel == 16 )
            TIFFSwabArrayOfShort( (uint16 *) psCache->pabyRow1Blocks,
                      (psCache->nBytesPerBlock * psCache->nSamples) / 2 );

        else if( psCache->nBitsPerPixel == 32 )
            TIFFSwabArrayOfLong( (uint32 *) psCache->pabyRow1Blocks,
                         (psCache->nBytesPerBlock * psCache->nSamples) / 4 );

        else if( psCache->nBitsPerPixel == 64 )
            TIFFSwabArrayOfDouble( (double *) psCache->pabyRow1Blocks,
                         (psCache->nBytesPerBlock * psCache->nSamples) / 8 );
    }

/* -------------------------------------------------------------------- */
/*      Record original directory position, so we can restore it at     */
/*      end.                                                            */
/* -------------------------------------------------------------------- */
    nBaseDirOffset = TIFFCurrentDirOffset( psCache->hTIFF );
    nRet = TIFFSetSubDirectory( psCache->hTIFF, psCache->nDirOffset );
    assert( nRet == 1 );

/* -------------------------------------------------------------------- */
/*      Write blocks to TIFF file.                                      */
/* -------------------------------------------------------------------- */
    for( iTileX = 0; iTileX < psCache->nBlocksPerRow; iTileX++ )
    {
        for( iSample = 0; iSample < psCache->nSamples; iSample++ )
        {
            int		  nTileID;

            pabyData = TIFFGetOvrBlock( psCache, iTileX, iTileY, iSample );

            if( psCache->bTiled )
            {
                nTileID =
                    TIFFComputeTile(psCache->hTIFF,
                                    iTileX * psCache->nBlockXSize,
                                    iTileY * psCache->nBlockYSize,
                                    0, (tsample_t) iSample );
                TIFFWriteEncodedTile( psCache->hTIFF, nTileID, 
                                      pabyData,
                                      TIFFTileSize(psCache->hTIFF) );
            }
            else
            {
                nTileID =
                    TIFFComputeStrip(psCache->hTIFF,
                                     iTileY * psCache->nBlockYSize,
                                     (tsample_t) iSample);

                TIFFWriteEncodedStrip( psCache->hTIFF, nTileID,
                                       pabyData,
                                       TIFFStripSize(psCache->hTIFF) );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Rotate buffers.                                                 */
/* -------------------------------------------------------------------- */
    pabyData = psCache->pabyRow1Blocks;
    psCache->pabyRow1Blocks = psCache->pabyRow2Blocks;
    psCache->pabyRow2Blocks = pabyData;

    _TIFFmemset( pabyData, 0,
                 psCache->nBytesPerBlock * psCache->nSamples
                 * psCache->nBlocksPerRow );

    psCache->nBlockOffset++;

/* -------------------------------------------------------------------- */
/*      Restore access to original directory.                           */
/* -------------------------------------------------------------------- */
    TIFFFlush( psCache->hTIFF );
    
    TIFFSetSubDirectory( psCache->hTIFF, nBaseDirOffset );
}

/************************************************************************/
/*                          TIFFGetOvrBlock()                           */
/************************************************************************/

unsigned char *TIFFGetOvrBlock( TIFFOvrCache *psCache, int iTileX, int iTileY,
                                int iSample )

{
    int		nRowOffset;
    
    if( iTileY > psCache->nBlockOffset + 1 )
        TIFFWriteOvrRow( psCache );

    assert( iTileX >= 0 && iTileX < psCache->nBlocksPerRow );
    assert( iTileY >= 0 && iTileY < psCache->nBlocksPerColumn );
    assert( iTileY >= psCache->nBlockOffset
            && iTileY < psCache->nBlockOffset+2 );
    assert( iSample >= 0 && iSample < psCache->nSamples );

    nRowOffset = ((iTileX * psCache->nSamples) + iSample)
        		* psCache->nBytesPerBlock;

    if( iTileY == psCache->nBlockOffset )
        return psCache->pabyRow1Blocks + nRowOffset;
    else
        return psCache->pabyRow2Blocks + nRowOffset;
}

/************************************************************************/
/*                        TIFFDestroyOvrCache()                         */
/************************************************************************/

void TIFFDestroyOvrCache( TIFFOvrCache * psCache )

{
    while( psCache->nBlockOffset < psCache->nBlocksPerColumn )
        TIFFWriteOvrRow( psCache );

    _TIFFfree( psCache->pabyRow1Blocks );
    _TIFFfree( psCache->pabyRow2Blocks );
    _TIFFfree( psCache );
}

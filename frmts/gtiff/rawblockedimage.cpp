/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Overview Builder
 * Purpose:  Implement the RawBlockedImage class, for holding ``under
 *           construction'' overviews in a temporary file. 
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
 * $Log: 
 */

#include "rawblockedimage.h"
#include "cpl_vsi.h"


/************************************************************************/
/*                          RawBlockedImage()                           */
/************************************************************************/

RawBlockedImage::RawBlockedImage( int nXSizeIn, int nYSizeIn,
                                  int nBlockXSizeIn, int nBlockYSizeIn,
                                  int nBitsPerPixelIn )

{
    static int		nTempCounter = 0;
    char		szFilename[128];
    
/* -------------------------------------------------------------------- */
/*      Initialize stuff.                                               */
/* -------------------------------------------------------------------- */
    nXSize = nXSizeIn;
    nYSize = nYSizeIn;
    nBlockXSize = nBlockXSizeIn;
    nBlockYSize = nBlockYSizeIn;
    nBitsPerPixel = nBitsPerPixelIn;

/* -------------------------------------------------------------------- */
/*      Create the raw temporary file, trying to verify first that      */
/*      it doesn't already exist.                                       */
/* -------------------------------------------------------------------- */
    fp = NULL;
    while( fp == NULL )
    {
        VSIStatBuf	sStat;
        
        sprintf( szFilename, "temp_%d.rbi", nTempCounter++ );
        if( VSIStat( szFilename, &sStat ) != 0 )
            fp = VSIFOpen( szFilename, "w+b" );
    }

    pszFilename = CPLStrdup( szFilename );
    nCurFileSize = 0;

/* -------------------------------------------------------------------- */
/*      Initialize other stuff.                                         */
/* -------------------------------------------------------------------- */
    nBlocksPerRow = (nXSize + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (nYSize + nBlockYSize - 1) / nBlockYSize;
    nBytesPerBlock = (nBlockXSize*nBlockYSize*nBitsPerPixel + 7) / 8;

    nBlocks = nBlocksPerRow * nBlocksPerColumn;
    nBlocksInCache = 0;
    nMaxBlocksInCache = MIN(nBlocks, 2*nBlocksPerRow);

    papoBlocks = (RawBlock **) CPLCalloc(sizeof(RawBlock*),nBlocks);

    poLRUHead = NULL;
    poLRUTail = NULL;
}

/************************************************************************/
/*                          ~RawBlockedImage()                          */
/************************************************************************/

RawBlockedImage::~RawBlockedImage()

{
    int		i;

    for( i = 0; i < nBlocks; i++ )
    {
        if( papoBlocks[i] != NULL )
        {
            CPLFree( papoBlocks[i]->pabyData );
            delete papoBlocks[i];
        }
    }

    CPLFree( papoBlocks );

    VSIFClose( fp );

    unlink( pszFilename );	/* wrap this? */
    
    CPLFree( pszFilename );
}

/************************************************************************/
/*                          InsertInLRUList()                           */
/*                                                                      */
/*      Insert this link at the beginning of the LRU list.  First       */
/*      removed from it's current position if it is in the list.        */
/************************************************************************/

void RawBlockedImage::InsertInLRUList( RawBlock * poBlock )

{
/* -------------------------------------------------------------------- */
/*      Remove from list, if it is currently in it.                     */
/* -------------------------------------------------------------------- */
    if( poBlock->poPrevLRU != NULL || poLRUHead == poBlock )
        RemoveFromLRUList( poBlock );

/* -------------------------------------------------------------------- */
/*      Add at the head.                                                */
/* -------------------------------------------------------------------- */
    if( poLRUHead != NULL )
    {
        poLRUHead->poPrevLRU = poBlock;
    }

    poBlock->poNextLRU = poLRUHead;
    poLRUHead = poBlock;

    if( poLRUTail == NULL )
        poLRUTail = poBlock;
}

/************************************************************************/
/*                         RemoveFromLRUList()                          */
/*                                                                      */
/*      Remove this block from the LRU list, if present.                */
/************************************************************************/

void RawBlockedImage::RemoveFromLRUList( RawBlock * poBlock )

{
/* -------------------------------------------------------------------- */
/*      Is it even in the list?                                         */
/* -------------------------------------------------------------------- */
    if( poBlock->poPrevLRU == NULL && poLRUHead != poBlock )
        return;

/* -------------------------------------------------------------------- */
/*      Fix the link before this in the list (or head pointer).         */
/* -------------------------------------------------------------------- */
    if( poBlock->poPrevLRU == NULL )
    {
        poLRUHead = poBlock->poNextLRU;
    }
    else
    {
        poBlock->poPrevLRU->poNextLRU = poBlock->poNextLRU;
    }
    
/* -------------------------------------------------------------------- */
/*      Fix the link after this one, or the tail pointer.               */
/* -------------------------------------------------------------------- */
    if( poBlock->poNextLRU == NULL )
    {
        poLRUTail = poBlock->poPrevLRU;
    }
    else
    {
        poBlock->poNextLRU->poPrevLRU = poBlock->poPrevLRU;
    }

/* -------------------------------------------------------------------- */
/*      Update this link to indicate it isn't in the list now.          */
/* -------------------------------------------------------------------- */
    poBlock->poPrevLRU = poBlock->poNextLRU = NULL;
}


/************************************************************************/
/*                             FlushBlock()                             */
/************************************************************************/

void RawBlockedImage::FlushBlock( RawBlock * poBlock )

{
/* -------------------------------------------------------------------- */
/*      If we aren't given a particular block to flush, then select     */
/*      the lest recently used one from the LRU list.                   */
/* -------------------------------------------------------------------- */
    if( poBlock == NULL )
    {
        if( poLRUTail == NULL )
            return;
        
        poBlock = poLRUTail;
    }

/* -------------------------------------------------------------------- */
/*      Remove from the LRU list.                                       */
/* -------------------------------------------------------------------- */
    RemoveFromLRUList( poBlock );

/* -------------------------------------------------------------------- */
/*      If the block has no data, then it doesn't really need to be     */
/*      flushed.                                                        */
/* -------------------------------------------------------------------- */
    if( poBlock->pabyData == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Is this block dirty?  If so we will have to try and save it.    */
/* -------------------------------------------------------------------- */
    if( poBlock->nDirty )
    {
        if( poBlock->nPositionInFile == -1 )
            poBlock->nPositionInFile = nCurFileSize;

        nCurFileSize += nBytesPerBlock;
        if( VSIFSeek( fp, poBlock->nPositionInFile, SEEK_SET ) != 0 )
        {
            CPLError( CE_Fatal, CPLE_FileIO,
                      "Seek to %d in overview spill file %s failed.\n",
                      poBlock->nPositionInFile, pszFilename );
            CPLAssert( FALSE );
        }

        if( VSIFWrite( poBlock->pabyData, 1, nBytesPerBlock, fp )
            != (size_t) nBytesPerBlock )
        {
            CPLError( CE_Fatal, CPLE_FileIO,
                      "Write of %d bytes at %d in overview spill file %s.\n"
                      "Is the disk full?\n",
                      nBytesPerBlock, poBlock->nPositionInFile, pszFilename );
            CPLAssert( FALSE );
        }
        
        poBlock->nDirty = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Free the data block, and decrement used count.                  */
/* -------------------------------------------------------------------- */
    nBlocksInCache--;
    CPLFree( poBlock->pabyData );
    poBlock->pabyData = NULL;
}


/************************************************************************/
/*                            GetRawBlock()                             */
/************************************************************************/

RawBlock *RawBlockedImage::GetRawBlock( int nXOff, int nYOff )

{
    int		nBlock = nXOff + nYOff * nBlocksPerRow;
    RawBlock	*poBlock;

    CPLAssert( nBlock >= 0 && nBlock < nBlocks );

/* -------------------------------------------------------------------- */
/*      Is this the first request?  If so, create the block object,     */
/*      initialize the data memory, and return it.                      */
/* -------------------------------------------------------------------- */
    poBlock = papoBlocks[nBlock];
    if( poBlock == NULL )
    {
        poBlock = papoBlocks[nBlock] = new RawBlock;
        poBlock->nDirty = FALSE;
        poBlock->poPrevLRU = poBlock->poNextLRU = NULL;
        poBlock->nPositionInFile = -1;
        poBlock->pabyData = (GByte *) CPLCalloc(1,nBytesPerBlock);
        nBlocksInCache++;
    }

/* -------------------------------------------------------------------- */
/*      Does this block need to be read off disk?                       */
/* -------------------------------------------------------------------- */
    else if( poBlock->nPositionInFile >= 0 && poBlock->pabyData == NULL )
    {
        nBlocksInCache++;
        poBlock->pabyData = (GByte *) CPLCalloc(1,nBytesPerBlock);
        VSIFSeek( fp, poBlock->nPositionInFile, SEEK_SET );
        VSIFRead( poBlock->pabyData, nBytesPerBlock, 1, fp );
    }

/* -------------------------------------------------------------------- */
/*      Does the data need to be allocated?                             */
/* -------------------------------------------------------------------- */
    else if( poBlock->pabyData == NULL )
    {
        poBlock->pabyData = (GByte *) CPLCalloc(1,nBytesPerBlock);
    }

/* -------------------------------------------------------------------- */
/*      Push on the LRU stack, or pop it back to the top.               */
/* -------------------------------------------------------------------- */
    InsertInLRUList( poBlock );

/* -------------------------------------------------------------------- */
/*      If we have exceeded our self imposed caching limit, flush       */
/*      one block.                                                      */
/* -------------------------------------------------------------------- */
    if( nBlocksInCache > nMaxBlocksInCache )
        FlushBlock( NULL );

    return( poBlock );
}

/************************************************************************/
/*                              GetTile()                               */
/************************************************************************/

GByte *RawBlockedImage::GetTile( int nXOff, int nYOff )

{
    RawBlock	*poBlock;

    poBlock = GetRawBlock(nXOff,nYOff);
    if( poBlock != NULL )
        return poBlock->pabyData;
    else
        return NULL;
}

/************************************************************************/
/*                          GetTileForUpdate()                          */
/************************************************************************/

GByte *RawBlockedImage::GetTileForUpdate( int nXOff, int nYOff )

{
    RawBlock	*poBlock;

    poBlock = GetRawBlock(nXOff,nYOff);
    if( poBlock != NULL )
    {
        poBlock->nDirty = TRUE;
        
        return poBlock->pabyData;
    }
    else
        return NULL;
}

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
 * $Log$
 * Revision 1.1  1999/11/29 21:33:22  warmerda
 * New
 *
 * Revision 1.1  1999/08/17 01:47:59  warmerda
 * New
 *
 * Revision 1.2  1999/03/12 17:29:34  warmerda
 * Use _WIN32 rather than WIN32.
 *
 */

#include <assert.h>
#include <string.h>

#include <stdlib.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

#include "rawblockedimage.h"

#ifndef FALSE
#  define FALSE 0
#  define TRUE 1
#endif

#ifndef MAX
#  define MIN(a,b)      ((a<b) ? a : b)
#  define MAX(a,b)      ((a>b) ? a : b)
#endif

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
        sprintf( szFilename, "temp_%d.rbi", nTempCounter++ );
        fp = fopen( szFilename, "r" );
        if( fp != NULL )
            fclose( fp );
        else
            fp = fopen( szFilename, "w+b" );
    }

    pszFilename = strdup( szFilename );
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

    papoBlocks = (RawBlock **) calloc(sizeof(RawBlock*),nBlocks);

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
            if( papoBlocks[i]->pabyData != NULL )
                free( papoBlocks[i]->pabyData );

            delete papoBlocks[i];
        }
    }

    if( papoBlocks != NULL)
        free( papoBlocks );

    fclose( fp );

    unlink( pszFilename );	/* wrap this? */

    free( pszFilename );
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
        if( fseek( fp, poBlock->nPositionInFile, SEEK_SET ) != 0 )
        {
            fprintf( stderr,
                     "Seek to %d in overview spill file %s failed.\n",
                     poBlock->nPositionInFile, pszFilename );
            exit( 1 );
        }

        if( fwrite( poBlock->pabyData, 1, nBytesPerBlock, fp )
            != (size_t) nBytesPerBlock )
        {
            fprintf( stderr, 
                     "Write of %d bytes at %d in overview spill file %s.\n"
                     "Is the disk full?\n",
                     nBytesPerBlock, poBlock->nPositionInFile, pszFilename );
            exit( 1 );
        }
        
        poBlock->nDirty = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Free the data block, and decrement used count.                  */
/* -------------------------------------------------------------------- */
    nBlocksInCache--;
    if( poBlock->pabyData != NULL )
        free( poBlock->pabyData );
    poBlock->pabyData = NULL;
}


/************************************************************************/
/*                            GetRawBlock()                             */
/************************************************************************/

RawBlock *RawBlockedImage::GetRawBlock( int nXOff, int nYOff )

{
    int		nBlock = nXOff + nYOff * nBlocksPerRow;
    RawBlock	*poBlock;

    assert( nBlock >= 0 && nBlock < nBlocks );

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
        poBlock->pabyData = (unsigned char *) calloc(1,nBytesPerBlock);
        nBlocksInCache++;

        if( poBlock->pabyData == NULL )
        {
            fprintf( stderr,
                     "RawBlockedImage::GetRawBlock() - out of memory\n" );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Does this block need to be read off disk?                       */
/* -------------------------------------------------------------------- */
    else if( poBlock->nPositionInFile >= 0 && poBlock->pabyData == NULL )
    {
        nBlocksInCache++;
        poBlock->pabyData = (unsigned char *) calloc(1,nBytesPerBlock);
        fseek( fp, poBlock->nPositionInFile, SEEK_SET );
        fread( poBlock->pabyData, nBytesPerBlock, 1, fp );
    }

/* -------------------------------------------------------------------- */
/*      Does the data need to be allocated?                             */
/* -------------------------------------------------------------------- */
    else if( poBlock->pabyData == NULL )
    {
        poBlock->pabyData = (unsigned char *) calloc(1,nBytesPerBlock);
        if( poBlock->pabyData == NULL )
        {
            fprintf( stderr,
                     "RawBlockedImage::GetRawBlock() - out of memory\n" );
            exit( 1 );
        }
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

unsigned char *RawBlockedImage::GetTile( int nXOff, int nYOff )

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

unsigned char *RawBlockedImage::GetTileForUpdate( int nXOff, int nYOff )

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

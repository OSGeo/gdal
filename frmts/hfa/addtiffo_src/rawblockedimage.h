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

#ifndef RAWBLOCKEDIMAGE_H_INCLUDED
#define RAWBLOCKEDIMAGE_H_INCLUDED

#include <stdio.h>

/************************************************************************/
/* ==================================================================== */
/*			RawBlockedImage					*/
/*									*/
/*	The RawBlockedImage class is used to maintain a single band	*/
/*	raster tiled image on disk. 					*/
/* ==================================================================== */
/************************************************************************/

class RawBlock
{
public:
    RawBlock	*poNextLRU;
    RawBlock	*poPrevLRU;
    
    int		nDirty;
    int		nPositionInFile;

    unsigned char *pabyData;
};

class RawBlockedImage
{
    int		nXSize;
    int		nYSize;

    int		nBlockXSize;
    int		nBlockYSize;
    int		nBitsPerPixel;
    int		nBytesPerBlock;

    int		nBlocksPerRow;
    int		nBlocksPerColumn;

    int		nBlocks;
    RawBlock	**papoBlocks;

    int		nBlocksInCache;
    int		nMaxBlocksInCache;

    FILE	*fp;
    int		nCurFileSize;
    char	*pszFilename;

    RawBlock	*GetRawBlock( int, int );
    void	FlushBlock( RawBlock * );
    void	InsertInLRUList( RawBlock * );
    void	RemoveFromLRUList( RawBlock * );

    RawBlock	*poLRUHead;
    RawBlock	*poLRUTail;
    
public:
    		RawBlockedImage( int nXSize, int nYSize,
                                 int nBlockXSize, int nBlockYSize,
                                 int nBitsPerPixel );

    		~RawBlockedImage();

    unsigned char*GetTile( int, int );
    unsigned char*GetTileForUpdate( int, int );

    int		GetBlockXSize() { return nBlockXSize; }
    int		GetBlockYSize() { return nBlockYSize; }
    int		GetXSize() { return nXSize; }
    int		GetYSize() { return nYSize; }
    int		GetBitsPerPixel() { return nBitsPerPixel; }
};

#endif /* ndef RAWBLOCKEDIMAGE_H_INCLUDED */

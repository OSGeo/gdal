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
 * gdalrasterblock.cpp
 *
 * The GDALRasterBlock class.
 *
 * 
 * $Log$
 * Revision 1.3  2000/03/31 13:42:49  warmerda
 * added debugging code
 *
 * Revision 1.2  2000/03/24 00:09:05  warmerda
 * rewrote cache management
 *
 * Revision 1.1  1998/12/31 18:52:58  warmerda
 * New
 *
 */

#include "gdal_priv.h"

static int nTileAgeTicker = 0; 
static int nCacheMax = 5 * 1024*1024;
static int nCacheUsed = 0;

static GDALRasterBlock   *poOldest = NULL;    /* tail */
static GDALRasterBlock   *poNewest = NULL;    /* head */


/************************************************************************/
/*                          GDALSetCacheMax()                           */
/************************************************************************/

void GDALSetCacheMax( int nNewSize )

{
    nCacheMax = nNewSize;
    if( nCacheUsed > nCacheMax )
        GDALFlushCacheBlock();
}

/************************************************************************/
/*                          GDALGetCacheMax()                           */
/************************************************************************/

int GDALGetCacheMax()
{
    return nCacheMax;
}

/************************************************************************/
/*                          GDALGetCacheUsed()                          */
/************************************************************************/

int GDALGetCacheUsed()
{
    return nCacheUsed;
}

/************************************************************************/
/*                        GDALFlushCacheBlock()                         */
/*                                                                      */
/*      The workhorse of cache management!                              */
/************************************************************************/

int GDALFlushCacheBlock()

{
    if( poOldest == NULL )
        return FALSE;
    poOldest->GetBand()->FlushBlock( poOldest->GetXOff(), 
                                     poOldest->GetYOff() );

    return TRUE;
}

/************************************************************************/
/*                           GDALRasterBand()                           */
/************************************************************************/

GDALRasterBlock::GDALRasterBlock( GDALRasterBand *poBandIn, 
                                  int nXOffIn, int nYOffIn )

{
    poBand = poBandIn;

    poBand->GetBlockSize( &nXSize, &nYSize );
    eType = poBand->GetRasterDataType();
    pData = NULL;
    bDirty = FALSE;

    poNext = poPrevious = NULL;

    nXOff = nXOffIn;
    nYOff = nYOffIn;
}

/************************************************************************/
/*                          ~GDALRasterBlock()                          */
/************************************************************************/

GDALRasterBlock::~GDALRasterBlock()

{
    if( pData != NULL )
    {
        int nSizeInBytes;

        VSIFree( pData );

        nSizeInBytes = (nXSize * nYSize * GDALGetDataTypeSize(eType)+7)/8;
        nCacheUsed -= nSizeInBytes;
    }

    if( poOldest == this )
        poOldest = poPrevious;

    if( poNewest == this )
    {
        poNewest = poNext;
    }

    if( poPrevious != NULL )
        poPrevious->poNext = poNext;

    if( poNext != NULL )
        poNext->poPrevious = poPrevious;

#ifdef ENABLE_DEBUG
    Verify();
#endif

    nAge = -1;
}

/************************************************************************/
/*                               Verify()                               */
/************************************************************************/

void GDALRasterBlock::Verify()

{
    CPLAssert( (poNewest == NULL && poOldest == NULL)
               || (poNewest != NULL && poOldest != NULL) );

    if( poNewest != NULL )
    {
        CPLAssert( poNewest->poPrevious == NULL );
        CPLAssert( poOldest->poNext == NULL );
        

        for( GDALRasterBlock *poBlock = poNewest; 
             poBlock != NULL;
             poBlock = poBlock->poNext )
        {
            if( poBlock->poPrevious )
            {
                CPLAssert( poBlock->poPrevious->poNext == poBlock );
            }

            if( poBlock->poNext )
            {
                CPLAssert( poBlock->poNext->poPrevious == poBlock );
            }
        }
    }
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

CPLErr GDALRasterBlock::Write()

{
    if( !GetDirty() )
        return CE_None;

    if( poBand == NULL )
        return CE_Failure;

    MarkClean();

    return poBand->IWriteBlock( nXOff, nYOff, pData );
}

/************************************************************************/
/*                               Touch()                                */
/************************************************************************/

void GDALRasterBlock::Touch()

{
    nAge = nTileAgeTicker++;

    if( poNewest == this )
        return;

    if( poOldest == this )
        poOldest = this->poPrevious;
    
    if( poPrevious != NULL )
        poPrevious->poNext = poNext;

    if( poNext != NULL )
        poNext->poPrevious = poPrevious;

    poPrevious = NULL;
    poNext = poNewest;

    if( poNewest != NULL )
    {
        CPLAssert( poNewest->poPrevious == NULL );
        poNewest->poPrevious = this;
    }
    poNewest = this;
    
    if( poOldest == NULL )
    {
        CPLAssert( poPrevious == NULL && poNext == NULL );
        poOldest = this;
    }
#ifdef ENABLE_DEBUG
    Verify();
#endif
}

/************************************************************************/
/*                            Internalize()                             */
/************************************************************************/

CPLErr GDALRasterBlock::Internalize()

{
    void	*pNewData;
    int		nSizeInBytes;

    nSizeInBytes = (nXSize * nYSize * GDALGetDataTypeSize( eType ) + 7) / 8;

    pNewData = VSIMalloc( nSizeInBytes );
    if( pNewData == NULL )
        return( CE_Failure );

    if( pData != NULL )
        memcpy( pNewData, pData, nSizeInBytes );
    
    pData = pNewData;

/* -------------------------------------------------------------------- */
/*      Flush old blocks if we are nearing our memory limit.            */
/* -------------------------------------------------------------------- */
    nCacheUsed += nSizeInBytes;
    while( nCacheUsed > nCacheMax )
    {
        int nOldCacheUsed = nCacheUsed;

        GDALFlushCacheBlock();

        if( nCacheUsed == nOldCacheUsed )
        {
            static int bReported = FALSE;

            if( !bReported )
            {
                bReported = TRUE;
            }
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Add this block to the list.                                     */
/* -------------------------------------------------------------------- */
    Touch();
    return( CE_None );
}

/************************************************************************/
/*                             MarkDirty()                              */
/************************************************************************/

void GDALRasterBlock::MarkDirty()

{
    bDirty = TRUE;
}


/************************************************************************/
/*                             MarkClean()                              */
/************************************************************************/

void GDALRasterBlock::MarkClean()

{
    bDirty = FALSE;
}



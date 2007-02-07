/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALRasterBlock class and related global 
 *           raster block cache management.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 1998, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#include "gdal_priv.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

static int bCacheMaxInitialized = FALSE;
static int nCacheMax = 40 * 1024*1024;
static volatile int nCacheUsed = 0;

static volatile GDALRasterBlock *poOldest = NULL;    /* tail */
static volatile GDALRasterBlock *poNewest = NULL;    /* head */

static void *hRBMutex = NULL;


/************************************************************************/
/*                          GDALSetCacheMax()                           */
/************************************************************************/

/**
 * Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that GDAL is permitted
 * to use for GDALRasterBlock caching.
 *
 * @param nNewSize the maximum number of bytes for caching.  Maximum is 2GB.
 */

void CPL_STDCALL GDALSetCacheMax( int nNewSize )

{
    nCacheMax = nNewSize;

/* -------------------------------------------------------------------- */
/*      Flush blocks till we are under the new limit or till we         */
/*      can't seem to flush anymore.                                    */
/* -------------------------------------------------------------------- */
    while( nCacheUsed > nCacheMax )
    {
        int nOldCacheUsed = nCacheUsed;

        GDALFlushCacheBlock();

        if( nCacheUsed == nOldCacheUsed )
            break;
    }
}

/************************************************************************/
/*                          GDALGetCacheMax()                           */
/************************************************************************/

/**
 * Get maximum cache memory.
 *
 * Gets the maximum amount of memory available to the GDALRasterBlock
 * caching system for caching GDAL read/write imagery. 
 *
 * @return maximum in bytes. 
 */

int CPL_STDCALL GDALGetCacheMax()
{
    if( !bCacheMaxInitialized )
    {
        if( CPLGetConfigOption("GDAL_CACHEMAX",NULL) != NULL )
        {
            nCacheMax = atoi(CPLGetConfigOption("GDAL_CACHEMAX","10"));
            if( nCacheMax < 10000 )
                nCacheMax *= 1024 * 1024;
        }
        bCacheMaxInitialized = TRUE;
    }
    
    return nCacheMax;
}

/************************************************************************/
/*                          GDALGetCacheUsed()                          */
/************************************************************************/

/**
 * Get cache memory used.
 *
 * @return the number of bytes of memory currently in use by the 
 * GDALRasterBlock memory caching.
 */

int CPL_STDCALL GDALGetCacheUsed()
{
    return nCacheUsed;
}

/************************************************************************/
/*                        GDALFlushCacheBlock()                         */
/*                                                                      */
/*      The workhorse of cache management!                              */
/************************************************************************/

int CPL_STDCALL GDALFlushCacheBlock()

{
    return GDALRasterBlock::FlushCacheBlock();
}

/************************************************************************/
/*                          FlushCacheBlock()                           */
/*                                                                      */
/*      Note, if we have alot of blocks locked for a long time, this    */
/*      method is going to get slow because it will have to traverse    */
/*      the linked list a long ways looking for a flushing              */
/*      candidate.   It might help to re-touch locked blocks to push    */
/*      them to the top of the list.                                    */
/************************************************************************/

int GDALRasterBlock::FlushCacheBlock()

{
    int nXOff, nYOff;
    GDALRasterBand *poBand;

    {
        CPLMutexHolderD( &hRBMutex );
        GDALRasterBlock *poTarget = (GDALRasterBlock *) poOldest;

        while( poTarget != NULL && poTarget->GetLockCount() > 0 ) 
            poTarget = poTarget->poPrevious;
        
        if( poTarget == NULL )
            return FALSE;

        poTarget->Detach();

        nXOff = poTarget->GetXOff();
        nYOff = poTarget->GetYOff();
        poBand = poTarget->GetBand();
    }

    poBand->FlushBlock( nXOff, nYOff );

    return TRUE;
}

/************************************************************************/
/*                          GDALRasterBlock()                           */
/************************************************************************/

GDALRasterBlock::GDALRasterBlock( GDALRasterBand *poBandIn, 
                                  int nXOffIn, int nYOffIn )

{
    poBand = poBandIn;

    poBand->GetBlockSize( &nXSize, &nYSize );
    eType = poBand->GetRasterDataType();
    pData = NULL;
    bDirty = FALSE;
    nLockCount = 0;

    poNext = poPrevious = NULL;

    nXOff = nXOffIn;
    nYOff = nYOffIn;
}

/************************************************************************/
/*                          ~GDALRasterBlock()                          */
/************************************************************************/

GDALRasterBlock::~GDALRasterBlock()

{
    Detach();

    if( pData != NULL )
    {
        int nSizeInBytes;

        VSIFree( pData );

        nSizeInBytes = (nXSize * nYSize * GDALGetDataTypeSize(eType)+7)/8;

        {
            CPLMutexHolderD( &hRBMutex );
            nCacheUsed -= nSizeInBytes;
        }
    }

    CPLAssert( nLockCount == 0 );

#ifdef ENABLE_DEBUG
    Verify();
#endif
}

/************************************************************************/
/*                               Detach()                               */
/*                                                                      */
/*      Remove from block lists.                                        */
/************************************************************************/

void GDALRasterBlock::Detach()

{
    CPLMutexHolderD( &hRBMutex );

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

    poPrevious = NULL;
    poNext = NULL;
}

/************************************************************************/
/*                               Verify()                               */
/************************************************************************/

void GDALRasterBlock::Verify()

{
    CPLMutexHolderD( &hRBMutex );

    CPLAssert( (poNewest == NULL && poOldest == NULL)
               || (poNewest != NULL && poOldest != NULL) );

    if( poNewest != NULL )
    {
        CPLAssert( poNewest->poPrevious == NULL );
        CPLAssert( poOldest->poNext == NULL );
        
        for( GDALRasterBlock *poBlock = (GDALRasterBlock *) poNewest; 
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
    CPLMutexHolderD( &hRBMutex );

    if( poNewest == this )
        return;

    if( poOldest == this )
        poOldest = this->poPrevious;
    
    if( poPrevious != NULL )
        poPrevious->poNext = poNext;

    if( poNext != NULL )
        poNext->poPrevious = poPrevious;

    poPrevious = NULL;
    poNext = (GDALRasterBlock *) poNewest;

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
    CPLMutexHolderD( &hRBMutex );
    void        *pNewData;
    int         nSizeInBytes;
    int         nCurCacheMax = GDALGetCacheMax();

    nSizeInBytes = nXSize * nYSize * (GDALGetDataTypeSize(eType) / 8);

    pNewData = VSIMalloc( nSizeInBytes );
    if( pNewData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Internalize failed" );
        return( CE_Failure );
    }

    if( pData != NULL )
        memcpy( pNewData, pData, nSizeInBytes );
    
    pData = pNewData;

/* -------------------------------------------------------------------- */
/*      Flush old blocks if we are nearing our memory limit.            */
/* -------------------------------------------------------------------- */
    AddLock(); /* don't flush this block! */

    nCacheUsed += nSizeInBytes;
    while( nCacheUsed > nCurCacheMax )
    {
        int nOldCacheUsed = nCacheUsed;

        GDALFlushCacheBlock();

        if( nCacheUsed == nOldCacheUsed )
            break;
    }

/* -------------------------------------------------------------------- */
/*      Add this block to the list.                                     */
/* -------------------------------------------------------------------- */
    Touch();
    DropLock();

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

/************************************************************************/
/*                           SafeLockBlock()                            */
/************************************************************************/

/**
 * Safely lock block.
 *
 * This method locks a GDALRasterBlock (and touches it) in a thread-safe
 * manner.  The global block cache mutex is held while locking the block,
 * in order to avoid race conditions with other threads that might be
 * trying to expire the block at the same time.  The block pointer may be
 * safely NULL, in which case this method does nothing. 
 *
 * @param ppBlock Pointer to the block pointer to try and lock/touch.
 */
 
int GDALRasterBlock::SafeLockBlock( GDALRasterBlock ** ppBlock )

{
    CPLMutexHolderD( &hRBMutex );

    if( *ppBlock != NULL )
    {
        (*ppBlock)->AddLock();
        (*ppBlock)->Touch();
        
        return TRUE;
    }
    else
        return FALSE;
}

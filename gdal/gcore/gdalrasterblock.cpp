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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
static GIntBig nCacheMax = 40 * 1024*1024;
static volatile GIntBig nCacheUsed = 0;

static GDALRasterBlock *poOldest = NULL;    /* tail */
static GDALRasterBlock *poNewest = NULL;    /* head */

#if 0
static CPLMutex *hRBLock = NULL;
#define INITIALIZE_LOCK         CPLMutexHolderD( &hRBLock )
#define TAKE_LOCK               CPLMutexHolderOptionalLockD( hRBLock )
#define DESTROY_LOCK            CPLDestroyMutex( hRBLock )
#else

static CPLLock* hRBLock = NULL;
static int bDebugContention = FALSE;
static CPLLockType GetLockType()
{
    static int nLockType = -1;
    if( nLockType < 0 )
    {
        const char* pszLockType = CPLGetConfigOption("GDAL_RB_LOCK_TYPE", "ADAPTIVE");
        if( EQUAL(pszLockType, "ADAPTIVE") )
            nLockType = LOCK_ADAPTIVE_MUTEX;
        else if( EQUAL(pszLockType, "RECURSIVE") )
            nLockType = LOCK_RECURSIVE_MUTEX;
        else if( EQUAL(pszLockType, "SPIN") )
            nLockType = LOCK_SPIN;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "GDAL_RB_LOCK_TYPE=%s not supported. Falling back to ADAPTIVE",
                     pszLockType);
            nLockType = LOCK_ADAPTIVE_MUTEX;
        }
        bDebugContention = CSLTestBoolean(CPLGetConfigOption("GDAL_RB_LOCK_DEBUG_CONTENTION", "NO"));
    }
    return (CPLLockType) nLockType;
}

#define INITIALIZE_LOCK         CPLLockHolderD( &hRBLock, GetLockType() ); \
                                CPLLockSetDebugPerf(hRBLock, bDebugContention)
#define TAKE_LOCK               CPLLockHolderOptionalLockD( hRBLock )
#define DESTROY_LOCK            CPLDestroyLock( hRBLock )

#endif

//#define ENABLE_DEBUG

/************************************************************************/
/*                          GDALSetCacheMax()                           */
/************************************************************************/

/**
 * \brief Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that GDAL is permitted
 * to use for GDALRasterBlock caching. The unit of the value is bytes.
 *
 * The maximum value is 2GB, due to the use of a signed 32 bit integer.
 * Use GDALSetCacheMax64() to be able to set a higher value.
 *
 * @param nNewSizeInBytes the maximum number of bytes for caching.
 */

void CPL_STDCALL GDALSetCacheMax( int nNewSizeInBytes )

{
    GDALSetCacheMax64(nNewSizeInBytes);
}


/************************************************************************/
/*                        GDALSetCacheMax64()                           */
/************************************************************************/

/**
 * \brief Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that GDAL is permitted
 * to use for GDALRasterBlock caching. The unit of the value is bytes.
 *
 * Note: On 32 bit platforms, the maximum amount of memory that can be addressed
 * by a process might be 2 GB or 3 GB, depending on the operating system
 * capabilities. This function will not make any attempt to check the
 * consistency of the passed value with the effective capabilities of the OS.
 *
 * @param nNewSizeInBytes the maximum number of bytes for caching.
 *
 * @since GDAL 1.8.0
 */

void CPL_STDCALL GDALSetCacheMax64( GIntBig nNewSizeInBytes )

{
    bCacheMaxInitialized = TRUE;
    nCacheMax = nNewSizeInBytes;

/* -------------------------------------------------------------------- */
/*      Flush blocks till we are under the new limit or till we         */
/*      can't seem to flush anymore.                                    */
/* -------------------------------------------------------------------- */
    while( nCacheUsed > nCacheMax )
    {
        GIntBig nOldCacheUsed = nCacheUsed;

        GDALFlushCacheBlock();

        if( nCacheUsed == nOldCacheUsed )
            break;
    }
}

/************************************************************************/
/*                          GDALGetCacheMax()                           */
/************************************************************************/

/**
 * \brief Get maximum cache memory.
 *
 * Gets the maximum amount of memory available to the GDALRasterBlock
 * caching system for caching GDAL read/write imagery.
 *
 * The first type this function is called, it will read the GDAL_CACHEMAX
 * configuation option to initialize the maximum cache memory.
 *
 * This function cannot return a value higher than 2 GB. Use
 * GDALGetCacheMax64() to get a non-truncated value.
 *
 * @return maximum in bytes. 
 */

int CPL_STDCALL GDALGetCacheMax()
{
    GIntBig nRes = GDALGetCacheMax64();
    if (nRes > INT_MAX)
    {
        static int bHasWarned = FALSE;
        if (!bHasWarned)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cache max value doesn't fit on a 32 bit integer. "
                     "Call GDALGetCacheMax64() instead");
            bHasWarned = TRUE;
        }
        nRes = INT_MAX;
    }
    return (int)nRes;
}

/************************************************************************/
/*                         GDALGetCacheMax64()                          */
/************************************************************************/

/**
 * \brief Get maximum cache memory.
 *
 * Gets the maximum amount of memory available to the GDALRasterBlock
 * caching system for caching GDAL read/write imagery.
 *
 * The first type this function is called, it will read the GDAL_CACHEMAX
 * configuation option to initialize the maximum cache memory.
 *
 * @return maximum in bytes.
 *
 * @since GDAL 1.8.0
 */

GIntBig CPL_STDCALL GDALGetCacheMax64()
{
    if( !bCacheMaxInitialized )
    {
        {
            INITIALIZE_LOCK;
        }
        const char* pszCacheMax = CPLGetConfigOption("GDAL_CACHEMAX",NULL);
        bCacheMaxInitialized = TRUE;
        if( pszCacheMax != NULL )
        {
            GIntBig nNewCacheMax = (GIntBig)CPLScanUIntBig(pszCacheMax, strlen(pszCacheMax));
            if( nNewCacheMax < 100000 )
            {
                if (nNewCacheMax < 0)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid value for GDAL_CACHEMAX. Using default value.");
                    return nCacheMax;
                }
                nNewCacheMax *= 1024 * 1024;
            }
            nCacheMax = nNewCacheMax;
        }
    }

    return nCacheMax;
}

/************************************************************************/
/*                          GDALGetCacheUsed()                          */
/************************************************************************/

/**
 * \brief Get cache memory used.
 *
 * @return the number of bytes of memory currently in use by the 
 * GDALRasterBlock memory caching.
 */

int CPL_STDCALL GDALGetCacheUsed()
{
    if (nCacheUsed > INT_MAX)
    {
        static int bHasWarned = FALSE;
        if (!bHasWarned)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cache used value doesn't fit on a 32 bit integer. "
                     "Call GDALGetCacheUsed64() instead");
            bHasWarned = TRUE;
        }
        return INT_MAX;
    }
    return (int)nCacheUsed;
}

/************************************************************************/
/*                        GDALGetCacheUsed64()                          */
/************************************************************************/

/**
 * \brief Get cache memory used.
 *
 * @return the number of bytes of memory currently in use by the
 * GDALRasterBlock memory caching.
 *
 * @since GDAL 1.8.0
 */

GIntBig CPL_STDCALL GDALGetCacheUsed64()
{
    return nCacheUsed;
}

/************************************************************************/
/*                        GDALFlushCacheBlock()                         */
/*                                                                      */
/*      The workhorse of cache management!                              */
/************************************************************************/

/**
 * \brief Try to flush one cached raster block
 *
 * This function will search the first unlocked raster block and will
 * flush it to release the associated memory.
 *
 * @return TRUE if one block was flushed, FALSE if there are no cached blocks
 *         or if they are currently locked.
 */
int CPL_STDCALL GDALFlushCacheBlock()

{
    return GDALRasterBlock::FlushCacheBlock();
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALRasterBlock                            */
/* ==================================================================== */
/************************************************************************/

/**
 * \class GDALRasterBlock "gdal_priv.h"
 *
 * GDALRasterBlock objects hold one block of raster data for one band
 * that is currently stored in the GDAL raster cache.  The cache holds
 * some blocks of raster data for zero or more GDALRasterBand objects
 * across zero or more GDALDataset objects in a global raster cache with
 * a least recently used (LRU) list and an upper cache limit (see
 * GDALSetCacheMax()) under which the cache size is normally kept. 
 *
 * Some blocks in the cache may be modified relative to the state on disk
 * (they are marked "Dirty") and must be flushed to disk before they can
 * be discarded.  Other (Clean) blocks may just be discarded if their memory
 * needs to be recovered. 
 *
 * In normal situations applications do not interact directly with the
 * GDALRasterBlock - instead it it utilized by the RasterIO() interfaces
 * to implement caching. 
 *
 * Some driver classes are implemented in a fashion that completely avoids
 * use of the GDAL raster cache (and GDALRasterBlock) though this is not very
 * common.
 */

/************************************************************************/
/*                          FlushCacheBlock()                           */
/*                                                                      */
/*      Note, if we have alot of blocks locked for a long time, this    */
/*      method is going to get slow because it will have to traverse    */
/*      the linked list a long ways looking for a flushing              */
/*      candidate.   It might help to re-touch locked blocks to push    */
/*      them to the top of the list.                                    */
/************************************************************************/

/**
 * \brief Attempt to flush at least one block from the cache.
 *
 * This static method is normally used to recover memory when a request
 * for a new cache block would put cache memory use over the established
 * limit.   
 *
 * C++ analog to the C function GDALFlushCacheBlock().
 * 
 * @param bDirtyBlocksOnly Only flushes dirty blocks.
 * @return TRUE if successful or FALSE if no flushable block is found.
 */

int GDALRasterBlock::FlushCacheBlock(int bDirtyBlocksOnly)

{
    GDALRasterBlock *poTarget;

    {
        INITIALIZE_LOCK;
        poTarget = poOldest;

        while( poTarget != NULL && (poTarget->GetLockCount() > 0 ||
               (bDirtyBlocksOnly && !poTarget->GetDirty())) )
            poTarget = poTarget->poPrevious;
        
        if( poTarget == NULL )
            return FALSE;

        poTarget->Detach_unlocked();
        poTarget->GetBand()->UnreferenceBlock(poTarget->GetXOff(),poTarget->GetYOff());
    }

    if( poTarget->GetDirty() )
    {
        CPLErr eErr = poTarget->Write();
        if( eErr != CE_None )
        {
             /* Save the error for later reporting */
            poTarget->GetBand()->SetFlushBlockErr(eErr);
        }
    }
    delete poTarget;

    return TRUE;
}

/************************************************************************/
/*                          FlushDirtyBlocks()                          */
/************************************************************************/

/**
 * \brief Flush all dirty blocks from cache.
 *
 * This static method is normally used to recover memory and is especially
 * usefull when doing multi-threaded code that can trigger the block cache.
 *
 * Due to the current design of the block cache, dirty blocks belonging to a same
 * dataset could be pushed simultanously to the IWriteBlock() method of that
 * dataset from different threads, causing races.
 *
 * Calling this method before that code can help workarounding that issue,
 * in a multiple readers, one writer scenario.
 *
 * @since GDAL 2.0
 */

void GDALRasterBlock::FlushDirtyBlocks()

{
    while( FlushCacheBlock(TRUE) )
    {
        /* go on */
    }
}

/************************************************************************/
/*                          GDALRasterBlock()                           */
/************************************************************************/

/**
 * @brief GDALRasterBlock Constructor 
 *
 * Normally only called from GDALRasterBand::GetLockedBlockRef().
 *
 * @param poBandIn the raster band used as source of raster block
 * being constructed.
 *
 * @param nXOffIn the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYOffIn the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 */

GDALRasterBlock::GDALRasterBlock( GDALRasterBand *poBandIn, 
                                  int nXOffIn, int nYOffIn )

{
    CPLAssert( NULL != poBandIn );

    poBand = poBandIn;

    poBand->GetBlockSize( &nXSize, &nYSize );
    eType = poBand->GetRasterDataType();
    pData = NULL;
    bDirty = FALSE;
    nLockCount = 0;

    poNext = poPrevious = NULL;

    nXOff = nXOffIn;
    nYOff = nYOffIn;
    bMustDetach = TRUE;
}

/************************************************************************/
/*                          ~GDALRasterBlock()                          */
/************************************************************************/

/**
 * Block destructor. 
 *
 * Normally called from GDALRasterBand::FlushBlock().
 */

GDALRasterBlock::~GDALRasterBlock()

{
    Detach();

    if( pData != NULL )
    {
        VSIFree( pData );
    }

    CPLAssert( nLockCount == 0 );

#ifdef ENABLE_DEBUG
    Verify();
#endif
}

/************************************************************************/
/*                               Detach()                               */
/************************************************************************/

/**
 * Remove block from cache.
 *
 * This method removes the current block from the linked list used to keep
 * track of all cached blocks in order of age.  It does not affect whether
 * the block is referenced by a GDALRasterBand nor does it destroy or flush
 * the block.
 */

void GDALRasterBlock::Detach()

{
    if( bMustDetach )
    {
        TAKE_LOCK;
        Detach_unlocked();
    }
}

void GDALRasterBlock::Detach_unlocked()
{
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
    bMustDetach = FALSE;

    if( pData )
        nCacheUsed -= GetBlockSize();

#ifdef ENABLE_DEBUG
    Verify();
#endif
}

/************************************************************************/
/*                               Verify()                               */
/************************************************************************/

/**
 * Confirms (via assertions) that the block cache linked list is in a
 * consistent state. 
 */

void GDALRasterBlock::Verify()

{
    TAKE_LOCK;

    CPLAssert( (poNewest == NULL && poOldest == NULL)
               || (poNewest != NULL && poOldest != NULL) );

    if( poNewest != NULL )
    {
        CPLAssert( poNewest->poPrevious == NULL );
        CPLAssert( poOldest->poNext == NULL );
        
        GDALRasterBlock* poLast = NULL;
        for( GDALRasterBlock *poBlock = poNewest; 
             poBlock != NULL;
             poBlock = poBlock->poNext )
        {
            CPLAssert( poBlock->poPrevious == poLast );

            poLast = poBlock;
        }

        CPLAssert( poOldest == poLast );
    }
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/**
 * Force writing of the current block, if dirty.
 *
 * The block is written using GDALRasterBand::IWriteBlock() on it's 
 * corresponding band object.  Even if the write fails the block will 
 * be marked clean. 
 *
 * @return CE_None otherwise the error returned by IWriteBlock().
 */

CPLErr GDALRasterBlock::Write()

{
    if( !GetDirty() )
        return CE_None;

    if( poBand == NULL )
        return CE_Failure;

    MarkClean();

    if (poBand->eFlushBlockErr == CE_None)
    {
        int bCallLeaveReadWrite = poBand->EnterReadWrite(GF_Write);
        CPLErr eErr = poBand->IWriteBlock( nXOff, nYOff, pData );
        if( bCallLeaveReadWrite ) poBand->LeaveReadWrite();
        return eErr;
    }
    else
        return poBand->eFlushBlockErr;
}

/************************************************************************/
/*                               Touch()                                */
/************************************************************************/

/**
 * Push block to top of LRU (least-recently used) list.
 *
 * This method is normally called when a block is used to keep track 
 * that it has been recently used. 
 */

void GDALRasterBlock::Touch()

{
    TAKE_LOCK;
    Touch_unlocked();
}


void GDALRasterBlock::Touch_unlocked()

{
    if( poNewest == this )
        return;

    // In theory, we shouldn't try to touch a block that has been detached
    CPLAssert(bMustDetach);
    if( !bMustDetach )
    {
        if( pData )
            nCacheUsed += GetBlockSize();

        bMustDetach = TRUE;
    }

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

/**
 * Allocate memory for block.
 *
 * This method allocates memory for the block, and attempts to flush other
 * blocks, if necessary, to bring the total cache size back within the limits.
 * The newly allocated block is touched and will be considered most recently
 * used in the LRU list. 
 * 
 * @return CE_None on success or CE_Failure if memory allocation fails. 
 */

CPLErr GDALRasterBlock::Internalize()

{
    void        *pNewData;
    int         nSizeInBytes;

    CPLAssert( pData == NULL );

    // This call will initialize the hRBLock mutex. Other call places can
    // only be called if we have go through there.
    GIntBig     nCurCacheMax = GDALGetCacheMax64();

    /* No risk of overflow as it is checked in GDALRasterBand::InitBlockInfo() */
    nSizeInBytes = GetBlockSize();

/* -------------------------------------------------------------------- */
/*      Flush old blocks if we are nearing our memory limit.            */
/* -------------------------------------------------------------------- */
    GDALRasterBlock* apoBlocksToFree[64];
    int nBlocksToFree = 0;
    {
        TAKE_LOCK;

        nCacheUsed += nSizeInBytes;
        GDALRasterBlock *poTarget = poOldest;
        while( nCacheUsed > nCurCacheMax )
        {
            while( poTarget != NULL && poTarget->GetLockCount() > 0 ) 
                poTarget = poTarget->poPrevious;

            if( poTarget != NULL )
            {
                GDALRasterBlock* _poPrevious = poTarget->poPrevious;

                poTarget->Detach_unlocked();
                poTarget->GetBand()->UnreferenceBlock(poTarget->GetXOff(),poTarget->GetYOff());

                apoBlocksToFree[nBlocksToFree++] = poTarget;
                if( nBlocksToFree == 64 )
                {
                    CPLDebug("GDAL", "More than 64 blocks are flagged to be flushed. Not trying more");
                    break;
                }

                poTarget = _poPrevious;
            }
            else
                break;
        }

    /* -------------------------------------------------------------------- */
    /*      Add this block to the list.                                     */
    /* -------------------------------------------------------------------- */
        Touch_unlocked();
    }

    /* Now free blocks we have detached and removed from their band */
    pNewData = NULL;
    for(int i=0;i<nBlocksToFree;i++)
    {
        GDALRasterBlock *poBlock = apoBlocksToFree[i];

        if( poBlock->GetDirty() )
        {
            CPLErr eErr = poBlock->Write();
            if( eErr != CE_None )
            {
                 /* Save the error for later reporting */
                poBlock->GetBand()->SetFlushBlockErr(eErr);
            }
        }

        /* Try to recycle the data of an existing block */
        void* pDataBlock = poBlock->pData;
        if( pNewData == NULL && pDataBlock != NULL &&
            poBlock->GetBlockSize() >= nSizeInBytes )
        {
            pNewData = pDataBlock;
            poBlock->pData = NULL;
        }

        delete poBlock;
    }

    if( pNewData == NULL )
    {
        pNewData = VSIMalloc( nSizeInBytes );
        if( pNewData == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                    "GDALRasterBlock::Internalize : Out of memory allocating %d bytes.",
                    nSizeInBytes);
            return( CE_Failure );
        }
    }

    pData = pNewData;

    return( CE_None );
}

/************************************************************************/
/*                             MarkDirty()                              */
/************************************************************************/

/**
 * Mark the block as modified.
 *
 * A dirty block is one that has been modified and will need to be written
 * to disk before it can be flushed.
 */

void GDALRasterBlock::MarkDirty()

{
    bDirty = TRUE;
}


/************************************************************************/
/*                             MarkClean()                              */
/************************************************************************/

/**
 * Mark the block as unmodified.
 *
 * A dirty block is one that has been modified and will need to be written
 * to disk before it can be flushed.
 */

void GDALRasterBlock::MarkClean()

{
    bDirty = FALSE;
}

/************************************************************************/
/*                           SafeLockBlock()                            */
/************************************************************************/

/**
 * \brief Safely lock block.
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
    CPLAssert( NULL != ppBlock );

    TAKE_LOCK;

    if( *ppBlock != NULL )
    {
        (*ppBlock)->AddLock();
        (*ppBlock)->Touch_unlocked();
        
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                          DestroyRBMutex()                           */
/************************************************************************/

void GDALRasterBlock::DestroyRBMutex()
{
    if( hRBLock != NULL )
        DESTROY_LOCK;
    hRBLock = NULL;
}

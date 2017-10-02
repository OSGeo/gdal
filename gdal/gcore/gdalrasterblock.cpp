/******************************************************************************
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

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"

#include <climits>
#include <cstring>

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

static bool bCacheMaxInitialized = false;
// Will later be overridden by the default 5% if GDAL_CACHEMAX not defined.
static GIntBig nCacheMax = 40 * 1024 * 1024;
static volatile GIntBig nCacheUsed = 0;

static GDALRasterBlock *poOldest = NULL;  // Tail.
static GDALRasterBlock *poNewest = NULL;  // Head.

static int nDisableDirtyBlockFlushCounter = 0;

#if 0
static CPLMutex *hRBLock = NULL;
#define INITIALIZE_LOCK CPLMutexHolderD( &hRBLock )
#define TAKE_LOCK       CPLMutexHolderOptionalLockD( hRBLock )
#define DESTROY_LOCK    CPLDestroyMutex( hRBLock )
#else

static CPLLock* hRBLock = NULL;
static bool bDebugContention = false;
static bool bSleepsForBockCacheDebug = false;
static CPLLockType GetLockType()
{
    static int nLockType = -1;
    if( nLockType < 0 )
    {
        const char* pszLockType =
            CPLGetConfigOption("GDAL_RB_LOCK_TYPE", "ADAPTIVE");
        if( EQUAL(pszLockType, "ADAPTIVE") )
            nLockType = LOCK_ADAPTIVE_MUTEX;
        else if( EQUAL(pszLockType, "RECURSIVE") )
            nLockType = LOCK_RECURSIVE_MUTEX;
        else if( EQUAL(pszLockType, "SPIN") )
            nLockType = LOCK_SPIN;
        else
        {
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "GDAL_RB_LOCK_TYPE=%s not supported. Falling back to ADAPTIVE",
                pszLockType);
            nLockType = LOCK_ADAPTIVE_MUTEX;
        }
        bDebugContention = CPLTestBool(
            CPLGetConfigOption("GDAL_RB_LOCK_DEBUG_CONTENTION", "NO"));
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
#if 0
    if( nNewSizeInBytes == 12346789 )
    {
        GDALRasterBlock::DumpAll();
        return;
    }
#endif

    {
        INITIALIZE_LOCK;
    }
    bCacheMaxInitialized = true;
    nCacheMax = nNewSizeInBytes;

/* -------------------------------------------------------------------- */
/*      Flush blocks till we are under the new limit or till we         */
/*      can't seem to flush anymore.                                    */
/* -------------------------------------------------------------------- */
    while( nCacheUsed > nCacheMax )
    {
        const GIntBig nOldCacheUsed = nCacheUsed;

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
 * configuration option to initialize the maximum cache memory.
 * Starting with GDAL 2.1, the value can be expressed as x% of the usable
 * physical RAM (which may potentially be used by other processes). Otherwise
 * it is expected to be a value in MB.
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
        static bool bHasWarned = false;
        if (!bHasWarned)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Cache max value doesn't fit on a 32 bit integer. "
                      "Call GDALGetCacheMax64() instead" );
            bHasWarned = true;
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
 * configuration option to initialize the maximum cache memory.
 * Starting with GDAL 2.1, the value can be expressed as x% of the usable
 * physical RAM (which may potentially be used by other processes). Otherwise
 * it is expected to be a value in MB.
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
        bSleepsForBockCacheDebug = CPLTestBool(
            CPLGetConfigOption("GDAL_DEBUG_BLOCK_CACHE", "NO"));

        const char* pszCacheMax = CPLGetConfigOption("GDAL_CACHEMAX","5%");

        GIntBig nNewCacheMax;
        if( strchr(pszCacheMax, '%') != NULL )
        {
            GIntBig nUsablePhysicalRAM = CPLGetUsablePhysicalRAM();
            if( nUsablePhysicalRAM > 0 )
            {
                // For some reason, coverity pretends that this will overflow.
                // "Multiply operation overflows on operands static_cast<double>(
                // nUsablePhysicalRAM ) and CPLAtof(pszCacheMax). Example values for
                // operands: CPLAtof( pszCacheMax ) = 2251799813685248,
                // static_cast<double>(nUsablePhysicalRAM) = -9223372036854775808."
                // coverity[overflow]
                double dfCacheMax =
                    static_cast<double>(nUsablePhysicalRAM) *
                    CPLAtof(pszCacheMax) / 100.0;
                if( dfCacheMax >= 0 && dfCacheMax < 1e15 )
                    nNewCacheMax = static_cast<GIntBig>(dfCacheMax);
                else
                    nNewCacheMax = nCacheMax;
            }
            else
            {
                CPLDebug("GDAL",
                         "Cannot determine usable physical RAM.");
                nNewCacheMax = nCacheMax;
            }
        }
        else
        {
            nNewCacheMax = CPLAtoGIntBig(pszCacheMax);
            if( nNewCacheMax < 100000 )
            {
                if (nNewCacheMax < 0)
                {
                    CPLError(
                        CE_Failure, CPLE_NotSupported,
                        "Invalid value for GDAL_CACHEMAX. "
                        "Using default value.");
                    GIntBig nUsablePhysicalRAM = CPLGetUsablePhysicalRAM();
                    if( nUsablePhysicalRAM )
                        nNewCacheMax = nUsablePhysicalRAM / 20;
                    else
                    {
                        CPLDebug("GDAL",
                                 "Cannot determine usable physical RAM.");
                        nNewCacheMax = nCacheMax;
                    }
                }
                else
                {
                    nNewCacheMax *= 1024 * 1024;
                }
            }
        }
        nCacheMax = nNewCacheMax;
        CPLDebug( "GDAL", "GDAL_CACHEMAX = " CPL_FRMT_GIB " MB",
                  nCacheMax / (1024 * 1024));
        bCacheMaxInitialized = true;
    }
    // coverity[overflow_sink]
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
        static bool bHasWarned = false;
        if (!bHasWarned)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Cache used value doesn't fit on a 32 bit integer. "
                      "Call GDALGetCacheUsed64() instead" );
            bHasWarned = true;
        }
        return INT_MAX;
    }
    return static_cast<int>(nCacheUsed);
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

GIntBig CPL_STDCALL GDALGetCacheUsed64() { return nCacheUsed; }

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
/*      Note, if we have a lot of blocks locked for a long time, this    */
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

int GDALRasterBlock::FlushCacheBlock( int bDirtyBlocksOnly )

{
    GDALRasterBlock *poTarget;

    {
        INITIALIZE_LOCK;
        poTarget = poOldest;

        while( poTarget != NULL )
        {
            if( !bDirtyBlocksOnly ||
                (poTarget->GetDirty() && nDisableDirtyBlockFlushCounter == 0) )
            {
                if( CPLAtomicCompareAndExchange(
                        &(poTarget->nLockCount), 0, -1) )
                    break;
            }
            poTarget = poTarget->poPrevious;
        }

        if( poTarget == NULL )
            return FALSE;
        if( bSleepsForBockCacheDebug )
            CPLSleep(CPLAtof(
                CPLGetConfigOption(
                    "GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_DROP_LOCK", "0")));

        poTarget->Detach_unlocked();
        poTarget->GetBand()->UnreferenceBlock(poTarget);
    }

    if( bSleepsForBockCacheDebug )
        CPLSleep(CPLAtof(
            CPLGetConfigOption("GDAL_RB_FLUSHBLOCK_SLEEP_AFTER_RB_LOCK", "0")));

    if( poTarget->GetDirty() )
    {
        const CPLErr eErr = poTarget->Write();
        if( eErr != CE_None )
        {
            // Save the error for later reporting.
            poTarget->GetBand()->SetFlushBlockErr(eErr);
        }
    }

    VSIFreeAligned(poTarget->pData);
    poTarget->pData = NULL;
    poTarget->GetBand()->AddBlockToFreeList(poTarget);

    return TRUE;
}

/************************************************************************/
/*                          FlushDirtyBlocks()                          */
/************************************************************************/

/**
 * \brief Flush all dirty blocks from cache.
 *
 * This static method is normally used to recover memory and is especially
 * useful when doing multi-threaded code that can trigger the block cache.
 *
 * Due to the current design of the block cache, dirty blocks belonging to a
 * same dataset could be pushed simultaneously to the IWriteBlock() method of
 * that dataset from different threads, causing races.
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
/*                      EnterDisableDirtyBlockFlush()                   */
/************************************************************************/

/**
 * \brief Starts preventing dirty blocks from being flushed
 *
 * This static method is used to prevent dirty blocks from being flushed.
 * This might be useful when in a IWriteBlock() method, whose implementation
 * can directly/indirectly cause the block cache to evict new blocks, to
 * be recursively called on the same dataset.
 *
 * This method implements a reference counter and is thread-safe.
 * 
 * This call must be paired with a corresponding LeaveDisableDirtyBlockFlush().
 *
 * @since GDAL 2.2.2
 */

void GDALRasterBlock::EnterDisableDirtyBlockFlush()
{
    CPLAtomicInc(&nDisableDirtyBlockFlushCounter);
}

/************************************************************************/
/*                      LeaveDisableDirtyBlockFlush()                   */
/************************************************************************/

/**
 * \brief Ends preventing dirty blocks from being flushed.
 *
 * Undoes the effect of EnterDisableDirtyBlockFlush().
 *
 * @since GDAL 2.2.2
 */

void GDALRasterBlock::LeaveDisableDirtyBlockFlush()
{
    CPLAtomicDec(&nDisableDirtyBlockFlushCounter);
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
                                  int nXOffIn, int nYOffIn ) :
    eType(poBandIn->GetRasterDataType()),
    bDirty(false),
    nLockCount(0),
    nXOff(nXOffIn),
    nYOff(nYOffIn),
    nXSize(0),
    nYSize(0),
    pData(NULL),
    poBand(poBandIn),
    poNext(NULL),
    poPrevious(NULL),
    bMustDetach(true)
{
    CPLAssert( poBandIn != NULL );
    poBand->GetBlockSize( &nXSize, &nYSize );
}

/************************************************************************/
/*                          GDALRasterBlock()                           */
/************************************************************************/

/**
 * @brief GDALRasterBlock Constructor (for GDALHashSetBandBlockAccess purpose)
 *
 * Normally only called from GDALHashSetBandBlockAccess class. Such a block
 * is completely non functional and only meant as being used to do a look-up
 * in the hash set of GDALHashSetBandBlockAccess
 *
 * @param nXOffIn the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param nYOffIn the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 */

GDALRasterBlock::GDALRasterBlock( int nXOffIn, int nYOffIn ) :
    eType(GDT_Unknown),
    bDirty(false),
    nLockCount(0),
    nXOff(nXOffIn),
    nYOff(nYOffIn),
    nXSize(0),
    nYSize(0),
    pData(NULL),
    poBand(NULL),
    poNext(NULL),
    poPrevious(NULL),
    bMustDetach(false)
{}

/************************************************************************/
/*                                  RecycleFor()                        */
/************************************************************************/

/**
 * Recycle an existing block (of the same band)
 *
 * Normally called from GDALAbstractBandBlockCache::CreateBlock().
 */

void GDALRasterBlock::RecycleFor( int nXOffIn, int nYOffIn )
{
    CPLAssert(pData == NULL);
    pData = NULL;
    bDirty = false;
    nLockCount = 0;

    poNext = NULL;
    poPrevious = NULL;

    nXOff = nXOffIn;
    nYOff = nYOffIn;
    bMustDetach = true;
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
        VSIFreeAligned( pData );
    }

    CPLAssert( nLockCount <= 0 );

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
    bMustDetach = false;

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

#ifdef ENABLE_DEBUG
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

#else
void GDALRasterBlock::Verify() {}
#endif

#ifdef notdef
void GDALRasterBlock::CheckNonOrphanedBlocks( GDALRasterBand* poBand )
{
    TAKE_LOCK;
    for( GDALRasterBlock *poBlock = poNewest;
                          poBlock != NULL;
                          poBlock = poBlock->poNext )
    {
        if ( poBlock->GetBand() == poBand )
        {
            printf("Cache has still blocks of band %p\n", poBand);/*ok*/
            printf("Band : %d\n", poBand->GetBand());/*ok*/
            printf("nRasterXSize = %d\n", poBand->GetXSize());/*ok*/
            printf("nRasterYSize = %d\n", poBand->GetYSize());/*ok*/
            int nBlockXSize, nBlockYSize;
            poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
            printf("nBlockXSize = %d\n", nBlockXSize);/*ok*/
            printf("nBlockYSize = %d\n", nBlockYSize);/*ok*/
            printf("Dataset : %p\n", poBand->GetDataset());/*ok*/
            if( poBand->GetDataset() )
                printf("Dataset : %s\n",/*ok*/
                       poBand->GetDataset()->GetDescription());
        }
    }
}
#endif

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/**
 * Force writing of the current block, if dirty.
 *
 * The block is written using GDALRasterBand::IWriteBlock() on its
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
    // Can be safely tested outside the lock
    if( poNewest == this )
        return;

    TAKE_LOCK;
    Touch_unlocked();
}

void GDALRasterBlock::Touch_unlocked()

{
    // Could happen even if tested in Touch() before taking the lock
    // Scenario would be :
    // 0. this is the second block (the one pointed by poNewest->poNext)
    // 1. Thread 1 calls Touch() and poNewest != this at that point
    // 2. Thread 2 detaches poNewest
    // 3. Thread 1 arrives here
    if( poNewest == this )
        return;

    // In theory, we should not try to touch a block that has been detached.
    CPLAssert(bMustDetach);
    if( !bMustDetach )
    {
        if( pData )
            nCacheUsed += GetBlockSize();

        bMustDetach = true;
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
    CPLAssert( pData == NULL );

    void        *pNewData = NULL;

    // This call will initialize the hRBLock mutex. Other call places can
    // only be called if we have go through there.
    const GIntBig nCurCacheMax = GDALGetCacheMax64();

    // No risk of overflow as it is checked in GDALRasterBand::InitBlockInfo().
    const int nSizeInBytes = GetBlockSize();

/* -------------------------------------------------------------------- */
/*      Flush old blocks if we are nearing our memory limit.            */
/* -------------------------------------------------------------------- */
    bool bFirstIter = true;
    bool bLoopAgain = false;
    do
    {
        bLoopAgain = false;
        GDALRasterBlock* apoBlocksToFree[64] = { NULL };
        int nBlocksToFree = 0;
        {
            TAKE_LOCK;

            if( bFirstIter )
                nCacheUsed += nSizeInBytes;
            GDALRasterBlock *poTarget = poOldest;
            while( nCacheUsed > nCurCacheMax )
            {
                while( poTarget != NULL )
                {
                    if( !poTarget->GetDirty() ||
                        nDisableDirtyBlockFlushCounter == 0 )
                    {
                        if( CPLAtomicCompareAndExchange(
                                &(poTarget->nLockCount), 0, -1) )
                            break;
                    }
                    poTarget = poTarget->poPrevious;
                }

                if( poTarget != NULL )
                {
                    if( bSleepsForBockCacheDebug )
                        CPLSleep(CPLAtof(
                            CPLGetConfigOption(
                                "GDAL_RB_INTERNALIZE_SLEEP_AFTER_DROP_LOCK",
                                "0")));

                    GDALRasterBlock* _poPrevious = poTarget->poPrevious;

                    poTarget->Detach_unlocked();
                    poTarget->GetBand()->UnreferenceBlock(poTarget);

                    apoBlocksToFree[nBlocksToFree++] = poTarget;
                    if( poTarget->GetDirty() )
                    {
                        // Only free one dirty block at a time so that
                        // other dirty blocks of other bands with the same
                        // coordinates can be found with TryGetLockedBlock()
                        bLoopAgain = nCacheUsed > nCurCacheMax;
                        break;
                    }
                    if( nBlocksToFree == 64 )
                    {
                        bLoopAgain = ( nCacheUsed > nCurCacheMax );
                        break;
                    }

                    poTarget = _poPrevious;
                }
                else
                {
                    break;
                }
            }

        /* ------------------------------------------------------------------ */
        /*      Add this block to the list.                                   */
        /* ------------------------------------------------------------------ */
            if( !bLoopAgain )
                Touch_unlocked();
        }

        bFirstIter = false;

        // Now free blocks we have detached and removed from their band.
        for( int i = 0; i < nBlocksToFree; ++i)
        {
            GDALRasterBlock * const poBlock = apoBlocksToFree[i];

            if( poBlock->GetDirty() )
            {
                CPLErr eErr = poBlock->Write();
                if( eErr != CE_None )
                {
                    // Save the error for later reporting.
                    poBlock->GetBand()->SetFlushBlockErr(eErr);
                }
            }

            // Try to recycle the data of an existing block.
            void* pDataBlock = poBlock->pData;
            if( pNewData == NULL && pDataBlock != NULL &&
                poBlock->GetBlockSize() == nSizeInBytes )
            {
                pNewData = pDataBlock;
            }
            else
            {
                VSIFreeAligned(poBlock->pData);
            }
            poBlock->pData = NULL;

            poBlock->GetBand()->AddBlockToFreeList(poBlock);
        }
    }
    while(bLoopAgain);

    if( pNewData == NULL )
    {
        pNewData = VSI_MALLOC_ALIGNED_AUTO_VERBOSE( nSizeInBytes );
        if( pNewData == NULL )
        {
            return( CE_Failure );
        }
    }

    pData = pNewData;

    return CE_None;
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
    bDirty = true;
    if( poBand )
        poBand->InitRWLock();
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

void GDALRasterBlock::MarkClean() { bDirty = false; }

/************************************************************************/
/*                          DestroyRBMutex()                           */
/************************************************************************/

/*! @cond Doxygen_Suppress */
void GDALRasterBlock::DestroyRBMutex()
{
    if( hRBLock != NULL )
        DESTROY_LOCK;
    hRBLock = NULL;
}
/*! @endcond */

/************************************************************************/
/*                              TakeLock()                              */
/************************************************************************/

/**
 * Take a lock
 *
 * Should only be used by GDALArrayBandBlockCache::TryGetLockedBlockRef()
 * and GDALHashSetBandBlockCache::TryGetLockedBlockRef()
 *
 * @return TRUE if the lock has been successfully acquired. If FALSE, this
 *         should be reattempted after fetching again from the raster band
 *         block storage.
 */

int GDALRasterBlock::TakeLock()
{
    const int nLockVal = AddLock();
    CPLAssert(nLockVal >= 0);
    if( bSleepsForBockCacheDebug )
        CPLSleep(CPLAtof(
            CPLGetConfigOption("GDAL_RB_TRYGET_SLEEP_AFTER_TAKE_LOCK", "0")));
    if( nLockVal == 0 )
    {
#ifdef DEBUG
        CPLDebug(
            "GDAL",
            "TakeLock(%p): Block(%d,%d,%p) is being evicted while trying to "
            "reacquire it.",
            reinterpret_cast<void *>(CPLGetPID()), nXOff, nYOff, poBand );
#endif
        // The block is being evicted by GDALRasterBlock::Internalize()
        // or FlushCacheBlock(), so wait for this to be done before trying
        // again.
        DropLock();

        // wait for the block having been unreferenced
        TAKE_LOCK;

        return FALSE;
    }
    Touch();
    return TRUE;
}

/************************************************************************/
/*                      DropLockForRemovalFromStorage()                 */
/************************************************************************/

/**
 * Drop a lock before removing the block from the band storage.
 *
 * Should only be used by GDALArrayBandBlockCache::FlushBlock()
 * and GDALHashSetBandBlockCache::FlushBlock()
 *
 * @return TRUE if the lock has been successfully dropped.
 */

int GDALRasterBlock::DropLockForRemovalFromStorage()
{
    // Detect potential conflict with GDALRasterBlock::Internalize()
    // or FlushCacheBlock()
    if( CPLAtomicCompareAndExchange(&nLockCount, 0, -1) )
        return TRUE;
#ifdef DEBUG
    CPLDebug(
        "GDAL",
        "DropLockForRemovalFromStorage(%p): Block(%d,%d,%p) was attempted "
        "to be flushed from band but it is flushed by global cache.",
        reinterpret_cast<void *>(CPLGetPID()), nXOff, nYOff, poBand );
#endif

    // Wait for the block for having been unreferenced.
    TAKE_LOCK;

    return FALSE;
}

#if 0
void GDALRasterBlock::DumpAll()
{
    int iBlock = 0;
    for( GDALRasterBlock *poBlock = poNewest;
         poBlock != NULL;
         poBlock = poBlock->poNext )
    {
        printf("Block %d\n", iBlock);/*ok*/
        poBlock->DumpBlock();
        printf("\n");/*ok*/
        iBlock++;
    }
}

void GDALRasterBlock::DumpBlock()
{
    printf("  Lock count = %d\n", nLockCount);/*ok*/
    printf("  bDirty = %d\n", static_cast<int>(bDirty));/*ok*/
    printf("  nXOff = %d\n", nXOff);/*ok*/
    printf("  nYOff = %d\n", nYOff);/*ok*/
    printf("  nXSize = %d\n", nXSize);/*ok*/
    printf("  nYSize = %d\n", nYSize);/*ok*/
    printf("  eType = %d\n", eType);/*ok*/
    printf("  Band %p\n", GetBand());/*ok*/
    printf("  Band %d\n", GetBand()->GetBand());/*ok*/
    if( GetBand()->GetDataset() )
        printf("  Dataset = %s\n",/*ok*/
               GetBand()->GetDataset()->GetDescription());
}
#endif  // if 0

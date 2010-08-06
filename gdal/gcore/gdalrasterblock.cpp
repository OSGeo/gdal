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
static GIntBig nCacheMax = 40 * 1024*1024;
static volatile GIntBig nCacheUsed = 0;

static volatile GDALRasterBlock *poOldest = NULL;    /* tail */
static volatile GDALRasterBlock *poNewest = NULL;    /* head */

static void *hRBMutex = NULL;


/************************************************************************/
/*                          GDALSetCacheMax()                           */
/************************************************************************/

/**
 * \brief Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that GDAL is permitted
 * to use for GDALRasterBlock caching.
 *
 * The maximum value is 2GB, due to the use of a signed 32 bit integer.
 * Use GDALSetCacheMax64() to be able to set a higher value.
 *
 * @param nNewSize the maximum number of bytes for caching.
 */

void CPL_STDCALL GDALSetCacheMax( int nNewSize )

{
    GDALSetCacheMax64(nNewSize);
}


/************************************************************************/
/*                        GDALSetCacheMax64()                           */
/************************************************************************/

/**
 * \brief Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that GDAL is permitted
 * to use for GDALRasterBlock caching.
 *
 * Note: On 32 bit platforms, the maximum amount of memory that can be addressed
 * by a process might be 2 GB or 3 GB, depending on the operating system
 * capabilities. This function will not make any attempt to check the
 * consistency of the passed value with the effective capabilities of the OS.
 *
 * @param nNewSize the maximum number of bytes for caching.
 *
 * @since GDAL 1.8.0
 */

void CPL_STDCALL GDALSetCacheMax64( GIntBig nNewSize )

{
    nCacheMax = nNewSize;

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
 * @return TRUE if successful or FALSE if no flushable block is found.
 */

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

    CPLErr eErr = poBand->FlushBlock( nXOff, nYOff );
    if (eErr != CE_None)
    {
        /* Save the error for later reporting */
        poBand->SetFlushBlockErr(eErr);
    }

    return TRUE;
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

/**
 * Confirms (via assertions) that the block cache linked list is in a
 * consistent state. 
 */

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

    return poBand->IWriteBlock( nXOff, nYOff, pData );
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
    CPLMutexHolderD( &hRBMutex );
    void        *pNewData;
    int         nSizeInBytes;
    GIntBig     nCurCacheMax = GDALGetCacheMax64();

    /* No risk of overflow as it is checked in GDALRasterBand::InitBlockInfo() */
    nSizeInBytes = nXSize * nYSize * (GDALGetDataTypeSize(eType) / 8);

    pNewData = VSIMalloc( nSizeInBytes );
    if( pNewData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "GDALRasterBlock::Internalize : Out of memory allocating %d bytes.",
                  nSizeInBytes);
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
        GIntBig nOldCacheUsed = nCacheUsed;

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

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
    GetGDALRasterBlockManager()->SetCacheMax(nNewSizeInBytes);
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
    GetGDALRasterBlockManager()->SetCacheMax(nNewSizeInBytes);
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
    GIntBig nRes = GetGDALRasterBlockManager()->GetCacheMax();
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
    return GetGDALRasterBlockManager()->GetCacheMax();
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
    GIntBig nCacheUsed = GetGDALRasterBlockManager()->GetCacheUsed();
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
    return GetGDALRasterBlockManager()->GetCacheUsed();
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
    return GetGDALRasterBlockManager()->FlushCacheBlock();
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
                                  int nXOffIn, int nYOffIn,
                                  GDALRasterBlockManager *poManagerIn )

{
    CPLAssert( NULL != poBandIn );

    poBand = poBandIn;
    poManager = poManagerIn;

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
            CPLMutexHolderD( &(poManager->hRBMMutex) );
            poManager->nCacheUsed -= nSizeInBytes;
        }
    }

    CPLAssert( nLockCount == 0 );

#ifdef ENABLE_DEBUG
    poManager->Verify();
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
    CPLMutexHolderD( &(poManager->hRBMMutex) );

    if( poManager->poOldest == this )
        poManager->poOldest = poPrevious;

    if( poManager->poNewest == this )
    {
        poManager->poNewest = poNext;
    }

    if( poPrevious != NULL )
        poPrevious->poNext = poNext;

    if( poNext != NULL )
        poNext->poPrevious = poPrevious;

    poPrevious = NULL;
    poNext = NULL;
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
        return poBand->IWriteBlock( nXOff, nYOff, pData );
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
    CPLMutexHolderD( &(poManager->hRBMMutex) );

    if( poManager->poNewest == this )
        return;

    if( poManager->poOldest == this )
        poManager->poOldest = this->poPrevious;
    
    if( poPrevious != NULL )
        poPrevious->poNext = poNext;

    if( poNext != NULL )
        poNext->poPrevious = poPrevious;

    poPrevious = NULL;
    poNext = (GDALRasterBlock *) poManager->poNewest;

    if( poManager->poNewest != NULL )
    {
        CPLAssert( poManager->poNewest->poPrevious == NULL );
        poManager->poNewest->poPrevious = this;
    }
    poManager->poNewest = this;
    
    if( poManager->poOldest == NULL )
    {
        CPLAssert( poPrevious == NULL && poNext == NULL );
        poManager->poOldest = this;
    }
#ifdef ENABLE_DEBUG
    poManager->Verify();
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
    CPLMutexHolderD( &(poManager->hRBMMutex) );
    void        *pNewData;
    int         nSizeInBytes;
    GIntBig     nCurCacheMax = poManager->GetCacheMax();

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

    poManager->nCacheUsed += nSizeInBytes;
    while( poManager->nCacheUsed > nCurCacheMax )
    {
        GIntBig nOldCacheUsed = poManager->nCacheUsed;

        poManager->FlushCacheBlock();

        if( poManager->nCacheUsed == nOldCacheUsed )
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


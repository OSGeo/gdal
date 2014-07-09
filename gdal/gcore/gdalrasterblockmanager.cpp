/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALRasterBlockManager class.
 * Author:   Blake Thompson, flippmoke@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "ogr_srs_api.h"
#include "cpl_multiproc.h"
#include "gdal_pam.h"
#include "gdal_alg_priv.h"

#ifdef _MSC_VER
#  ifdef MSVC_USE_VLD
#    include <wchar.h>
#    include <vld.h>
#  endif
#endif

CPL_CVSID("$Id$");


/************************************************************************/
/* ==================================================================== */
/*                      GDALRasterBlockManager                          */
/* ==================================================================== */
/************************************************************************/

static volatile GDALRasterBlockManager        *poRBM = NULL;
static void *hRBMGlobalMutex = NULL;

void** GDALGetphRBMMutex() { return &hRBMGlobalMutex; }

/************************************************************************/
/*                   GetGDALRasterBlockManager()                        */
/*                                                                      */
/*      A freestanding function to get the global instance of the       */
/*      GDALRasterBlockManager.                                         */
/************************************************************************/

/**
 * \brief Fetch the global GDAL raster block manager.
 *
 * This function fetches the pointer to the singleton global raster block manager.
 * If the driver manager doesn't exist it is automatically created.
 *
 * @return pointer to the global raster block manager.  This should not be able
 * to fail.
 */

GDALRasterBlockManager * GetGDALRasterBlockManager()

{
    if( poRBM == NULL )
    {
        CPLMutexHolderD( &hRBMGlobalMutex );

        if( poRBM == NULL )
            poRBM = new GDALRasterBlockManager();
    }

    CPLAssert( NULL != poRBM );

    return const_cast<GDALRasterBlockManager *>( poRBM );
}

/************************************************************************/
/*                      GDALRasterBlockManager()                        */
/************************************************************************/

GDALRasterBlockManager::GDALRasterBlockManager()

{

    bCacheMaxInitialized = FALSE;
    nCacheMax = 40 * 1024*1024;
    nCacheUsed = 0;

    poOldest = NULL;    /* tail */
    poNewest = NULL;    /* head */

    hRBMMutex = NULL;
   
}

/************************************************************************/
/*                         ~GDALRasterBlockManager()                         */
/************************************************************************/

GDALRasterBlockManager::~GDALRasterBlockManager()

{
    DestroyRBMMutex();
    poOldest = NULL;   
    poNewest = NULL; 
}

/************************************************************************/
/*                        SetCacheMax()                                 */
/************************************************************************/

/**
 * \brief Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that Manager is permitted
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

void GDALRasterBlockManager::SetCacheMax( GIntBig nNewSizeInBytes )

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

        FlushCacheBlock();

        if( nCacheUsed == nOldCacheUsed )
            break;
    }
}

/************************************************************************/
/*                         GDALGetCacheMax()                            */
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

GIntBig GDALRasterBlockManager::GetCacheMax()
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
/*                         GDALGetCacheUsed()                           */
/************************************************************************/

/**
 * \brief Get cache memory used.
 *
 * @return the number of bytes of memory currently in use by the
 * GDALRasterBlock memory caching.
 *
 * @since GDAL 1.8.0
 */

GIntBig GDALRasterBlockManager::GetCacheUsed()
{
    return nCacheUsed;
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

int GDALRasterBlockManager::FlushCacheBlock()
{
    int nXOff, nYOff;
    GDALRasterBand *poBand;

    {
        CPLMutexHolderD( &hRBMMutex );
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
/*                               Verify()                               */
/************************************************************************/

/**
 * Confirms (via assertions) that the block cache linked list is in a
 * consistent state. 
 */

void GDALRasterBlockManager::Verify()

{
    CPLMutexHolderD( &hRBMMutex );

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
/*                           SafeLockBlock()                            */
/************************************************************************/

/**
 * \brief Safely lock block.
 *
 * This method locks a GDALRasterBlock (and touches it) in a thread-safe
 * manner.  The block cache mutex is held while locking the block,
 * in order to avoid race conditions with other threads that might be
 * trying to expire the block at the same time.  The block pointer may be
 * safely NULL, in which case this method does nothing. 
 *
 * @param ppBlock Pointer to the block pointer to try and lock/touch.
 */
 
int GDALRasterBlockManager::SafeLockBlock( GDALRasterBlock ** ppBlock )

{
    CPLAssert( NULL != ppBlock );

    CPLMutexHolderD( &hRBMMutex );

    if( *ppBlock != NULL )
    {
        (*ppBlock)->AddLock();
        (*ppBlock)->Touch();
        
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                          DestroyRBMMutex()                           */
/************************************************************************/

void GDALRasterBlockManager::DestroyRBMMutex()
{
    if( hRBMMutex != NULL )
        CPLDestroyMutex(hRBMMutex);
    hRBMMutex = NULL;
}

/************************************************************************/
/*                      GDALDestroyRasterBlockManager()                      */
/************************************************************************/

/**
 * \brief Destroy the global raster block manager.
 *
 * NOTE: This function is not thread safe.  It should not be called while
 * other threads are actively using GDAL. 
 */

void CPL_STDCALL GDALDestroyRasterBlockManager( void )

{
    // THREADSAFETY: We would like to lock the mutex here, but it 
    // needs to be reacquired within the destructor during driver
    // deregistration.
    if( poRBM != NULL )
    {
        CPLMutexHolderD( &hRBMGlobalMutex );
        if (poRBM != NULL ) {
            delete poRBM;
        }
    }

    CPLDestroyMutex(hRBMGlobalMutex);
    hRBMGlobalMutex = NULL;
}



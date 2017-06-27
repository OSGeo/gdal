/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Store cached blocks
 * Author:   Even Rouault, <even dot rouault at spatialys dot org>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot org>
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
#include "gdal_priv.h"

#include <cstddef>
#include <new>

#include "cpl_atomic_ops.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"

//! @cond Doxygen_Suppress

CPL_CVSID("$Id$")

#ifdef DEBUG_VERBOSE_ABBC
static int nAllBandsKeptAlivedBlocks = 0;
#endif

/************************************************************************/
/*                       GDALArrayBandBlockCache()                      */
/************************************************************************/

GDALAbstractBandBlockCache::GDALAbstractBandBlockCache(
    GDALRasterBand* poBandIn ) :
    hSpinLock(CPLCreateLock(LOCK_SPIN)),
    psListBlocksToFree(NULL),
    hCond(CPLCreateCond()),
    hCondMutex(CPLCreateMutex()),
    nKeepAliveCounter(0)
{
    poBand = poBandIn;
    if( hCondMutex )
        CPLReleaseMutex(hCondMutex);
}

/************************************************************************/
/*                      ~GDALAbstractBandBlockCache()                   */
/************************************************************************/

GDALAbstractBandBlockCache::~GDALAbstractBandBlockCache()
{
    CPLAssert(nKeepAliveCounter == 0);
    FreeDanglingBlocks();
    if( hSpinLock )
        CPLDestroyLock(hSpinLock);
    if( hCondMutex )
        CPLDestroyMutex(hCondMutex);
    if( hCond )
        CPLDestroyCond(hCond);
}

/************************************************************************/
/*                            UnreferenceBlockBase()                    */
/*                                                                      */
/*      This is called by GDALRasterBlock::Internalize() and            */
/*      FlushCacheBlock() when they remove a block from the linked list */
/*      but haven't yet flushed it to disk or recovered its pData member*/
/*      We must be aware that they are blocks in that state, since the  */
/*      band must be kept alive while AddBlockToFreeList() hasn't been  */
/*      called (in case a block is being flushed while the final        */
/*      FlushCache() of the main thread of the dataset is running).     */
/************************************************************************/

void GDALAbstractBandBlockCache::UnreferenceBlockBase()
{
    CPLAtomicInc(&nKeepAliveCounter);
}

/************************************************************************/
/*                          AddBlockToFreeList()                        */
/*                                                                      */
/*      This is called by GDALRasterBlock::Internalize() and            */
/*      FlushCacheBlock() after they have been finished with a block.   */
/************************************************************************/

void GDALAbstractBandBlockCache::AddBlockToFreeList( GDALRasterBlock *poBlock )
{
    CPLAssert(poBlock->poPrevious == NULL);
    CPLAssert(poBlock->poNext == NULL);
    {
#ifdef DEBUG_VERBOSE_ABBC
        CPLAtomicInc(&nAllBandsKeptAlivedBlocks);
        fprintf(stderr, "AddBlockToFreeList(): nAllBandsKeptAlivedBlocks=%d\n", nAllBandsKeptAlivedBlocks);/*ok*/
#endif
        CPLLockHolderOptionalLockD(hSpinLock);
        poBlock->poNext = psListBlocksToFree;
        psListBlocksToFree = poBlock;
    }

    // If no more blocks in transient state, then warn WaitKeepAliveCounter()
    CPLAcquireMutex(hCondMutex, 1000);
    if( CPLAtomicDec(&nKeepAliveCounter) == 0 )
    {
        CPLCondSignal(hCond);
    }
    CPLReleaseMutex(hCondMutex);
}

/************************************************************************/
/*                         WaitKeepAliveCounter()                       */
/************************************************************************/

void GDALAbstractBandBlockCache::WaitKeepAliveCounter()
{
#ifdef DEBUG_VERBOSE
    CPLDebug("GDAL", "WaitKeepAliveCounter()");
#endif

    CPLAcquireMutex(hCondMutex, 1000);
    while( nKeepAliveCounter != 0 )
    {
        CPLDebug( "GDAL", "Waiting for other thread to finish working with our "
                  "blocks" );
        CPLCondWait(hCond, hCondMutex);
    }
    CPLReleaseMutex(hCondMutex);
}

/************************************************************************/
/*                           FreeDanglingBlocks()                       */
/************************************************************************/

void GDALAbstractBandBlockCache::FreeDanglingBlocks()
{
    GDALRasterBlock* poList;
    {
        CPLLockHolderOptionalLockD(hSpinLock);
        poList = psListBlocksToFree;
        psListBlocksToFree = NULL;
    }
    while( poList )
    {
#ifdef DEBUG_VERBOSE_ABBC
        CPLAtomicDec(&nAllBandsKeptAlivedBlocks);
        fprintf(stderr, "FreeDanglingBlocks(): nAllBandsKeptAlivedBlocks=%d\n", nAllBandsKeptAlivedBlocks);/*ok*/
#endif
        GDALRasterBlock* poNext = poList->poNext;
        poList->poNext = NULL;
        delete poList;
        poList = poNext;
    }
}

/************************************************************************/
/*                            CreateBlock()                             */
/************************************************************************/

GDALRasterBlock* GDALAbstractBandBlockCache::CreateBlock(int nXBlockOff,
                                                         int nYBlockOff)
{
    GDALRasterBlock* poBlock;
    {
        CPLLockHolderOptionalLockD(hSpinLock);
        poBlock = psListBlocksToFree;
        if( poBlock )
        {
#ifdef DEBUG_VERBOSE_ABBC
            CPLAtomicDec(&nAllBandsKeptAlivedBlocks);
            fprintf(stderr, "CreateBlock(): nAllBandsKeptAlivedBlocks=%d\n", nAllBandsKeptAlivedBlocks);/*ok*/
#endif
            psListBlocksToFree = poBlock->poNext;
        }
    }
    if( poBlock )
        poBlock->RecycleFor(nXBlockOff, nYBlockOff);
    else
        poBlock = new (std::nothrow) GDALRasterBlock(
            poBand, nXBlockOff, nYBlockOff );
    return poBlock;
}
//! @endcond

/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Store cached blocks in a hash set
 * Author:   Even Rouault, <even dot rouault at spatialys dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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

#include "cpl_hash_set.h"
#include "cpl_multiproc.h"
#include "gdal_priv.h"
#include <algorithm>

CPL_CVSID("$Id$");

/* ******************************************************************** */
/*                        GDALHashSetBandBlockCache                     */
/* ******************************************************************** */

class GDALHashSetBandBlockCache CPL_FINAL : public GDALAbstractBandBlockCache
{
    CPLHashSet     *hSet;
    CPLLock        *hLock;

    public:
            GDALHashSetBandBlockCache(GDALRasterBand* poBand);
           ~GDALHashSetBandBlockCache();

           virtual bool             Init();
           virtual bool             IsInitOK();
           virtual CPLErr           FlushCache();
           virtual CPLErr           AdoptBlock( GDALRasterBlock * );
           virtual GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff,
                                                          int nYBlockYOff );
           virtual CPLErr           UnreferenceBlock( GDALRasterBlock* poBlock );
           virtual CPLErr           FlushBlock( int nXBlockOff, int nYBlockOff,
                                                int bWriteDirtyBlock );
};

/************************************************************************/
/*                     GDALHashSetBandBlockCacheCreate()                */
/************************************************************************/

GDALAbstractBandBlockCache* GDALHashSetBandBlockCacheCreate(GDALRasterBand* poBand)
{
    return new GDALHashSetBandBlockCache(poBand);
}

/************************************************************************/
/*                      GDALRasterBlockHashFunc()                       */
/************************************************************************/

/* calculate hash value */
static unsigned long GDALRasterBlockHashFunc( const void * const elt )
{
    const GDALRasterBlock * const poBlock =
        static_cast<const GDALRasterBlock * const>(elt);
#if SIZEOF_UNSIGNED_LONG == 8
    return static_cast<unsigned long>(
        poBlock->GetXOff() | (((unsigned long)poBlock->GetYOff()) << 32));
#else
    return static_cast<unsigned long>(
        ((poBlock->GetXOff() & 0xFFFF) ^ (poBlock->GetYOff() >> 16)) |
        (((poBlock->GetYOff() & 0xFFFF) ^ (poBlock->GetXOff() >> 16)) << 16));
#endif
}

/************************************************************************/
/*                      GDALRasterBlockEqualFunc()                      */
/************************************************************************/

/* test equality */
static int GDALRasterBlockEqualFunc( const void * const elt1,
                                     const void * const elt2 )
{
    const GDALRasterBlock * const poBlock1 =
        static_cast<const GDALRasterBlock * const>(elt1);
    const GDALRasterBlock * const poBlock2 =
        static_cast<const GDALRasterBlock * const >(elt2);
    return poBlock1->GetXOff() == poBlock2->GetXOff() &&
           poBlock1->GetYOff() == poBlock2->GetYOff();
}

/************************************************************************/
/*                       GDALHashSetBandBlockCache()                    */
/************************************************************************/

GDALHashSetBandBlockCache::GDALHashSetBandBlockCache(GDALRasterBand* poBandIn) :
    GDALAbstractBandBlockCache(poBandIn),
    hSet(CPLHashSetNew(GDALRasterBlockHashFunc, GDALRasterBlockEqualFunc, NULL)),
    hLock(CPLCreateLock(LOCK_ADAPTIVE_MUTEX))
{}

/************************************************************************/
/*                      ~GDALHashSetBandBlockCache()                    */
/************************************************************************/

GDALHashSetBandBlockCache::~GDALHashSetBandBlockCache()
{
    FlushCache();
    CPLHashSetDestroy(hSet);
    CPLDestroyLock(hLock);
}

/************************************************************************/
/*                                  Init()                              */
/************************************************************************/

bool GDALHashSetBandBlockCache::Init()
{
    return true;
}

/************************************************************************/
/*                             IsInitOK()                               */
/************************************************************************/

bool GDALHashSetBandBlockCache::IsInitOK()
{
    return true;
}

/************************************************************************/
/*                            AdoptBlock()                              */
/************************************************************************/

CPLErr GDALHashSetBandBlockCache::AdoptBlock( GDALRasterBlock * poBlock )

{
    FreeDanglingBlocks();

    CPLLockHolderOptionalLockD( hLock );
    CPLHashSetInsert(hSet, poBlock);

    return CE_None;
}

/************************************************************************/
/*              GDALHashSetBandBlockCacheFlushCacheIterFunc()           */
/************************************************************************/

static int GDALHashSetBandBlockCacheFlushCacheIterFunc(void* elt, void* user_data)
{
    std::vector<GDALRasterBlock*>* papoBlocks = (std::vector<GDALRasterBlock*>*)user_data;
    GDALRasterBlock* poBlock = (GDALRasterBlock*) elt;
    papoBlocks->push_back(poBlock);
    return TRUE;
}

/************************************************************************/
/*                  GDALHashSetBandBlockCacheSortBlocks()               */
/************************************************************************/

// TODO: Both args should be const.
static bool GDALHashSetBandBlockCacheSortBlocks(GDALRasterBlock* poBlock1,
                                                GDALRasterBlock* poBlock2)
{
    return poBlock1->GetYOff() < poBlock2->GetYOff() ||
           ( poBlock1->GetYOff() == poBlock2->GetYOff() &&
             poBlock1->GetXOff() < poBlock2->GetXOff() );
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

CPLErr GDALHashSetBandBlockCache::FlushCache()
{
    FreeDanglingBlocks();

    CPLErr eGlobalErr = poBand->eFlushBlockErr;

    std::vector<GDALRasterBlock*> apoBlocks;
    {
        CPLLockHolderOptionalLockD( hLock );
        CPLHashSetForeach(hSet,
                          GDALHashSetBandBlockCacheFlushCacheIterFunc,
                          &apoBlocks);

        CPLHashSetClear(hSet);
    }

    // Sort blocks by increasing y and then x in order to please some tests
    // like tiff_write_133
    std::sort(apoBlocks.begin(), apoBlocks.end(), GDALHashSetBandBlockCacheSortBlocks);

    for(size_t i=0;i<apoBlocks.size();i++)
    {
        GDALRasterBlock* poBlock = apoBlocks[i];

        if( poBlock->DropLockForRemovalFromStorage() )
        {
            CPLErr    eErr = CE_None;

            if( eGlobalErr == CE_None && poBlock->GetDirty() )
                eErr = poBlock->Write();

            delete poBlock;

            if( eErr != CE_None )
                eGlobalErr = eErr;
        }
    }

    WaitKeepAliveCounter();

    return( eGlobalErr );
}

/************************************************************************/
/*                        UnreferenceBlock()                            */
/************************************************************************/

CPLErr GDALHashSetBandBlockCache::UnreferenceBlock( GDALRasterBlock* poBlock )
{
    UnreferenceBlockBase();

    CPLLockHolderOptionalLockD( hLock );
    CPLHashSetRemoveDeferRehash(hSet, poBlock);
    return CE_None;
}

/************************************************************************/
/*                            FlushBlock()                              */
/************************************************************************/

CPLErr GDALHashSetBandBlockCache::FlushBlock( int nXBlockOff, int nYBlockOff,
                                              int bWriteDirtyBlock )

{
    GDALRasterBlock oBlockForLookup(nXBlockOff, nYBlockOff);
    GDALRasterBlock* poBlock = NULL;
    {
        CPLLockHolderOptionalLockD( hLock );
        poBlock = reinterpret_cast<GDALRasterBlock*>(
            CPLHashSetLookup(hSet, &oBlockForLookup) );
        if( poBlock == NULL )
            return CE_None;
        CPLHashSetRemove(hSet, poBlock);
    }

    if( !poBlock->DropLockForRemovalFromStorage() )
        return CE_None;

    CPLErr eErr = CE_None;

    if( bWriteDirtyBlock && poBlock->GetDirty() )
        eErr = poBlock->Write();

    delete poBlock;

    return eErr;
}

/************************************************************************/
/*                        TryGetLockedBlockRef()                        */
/************************************************************************/

GDALRasterBlock *GDALHashSetBandBlockCache::TryGetLockedBlockRef( int nXBlockOff,
                                                                  int nYBlockOff )

{
    GDALRasterBlock oBlockForLookup(nXBlockOff, nYBlockOff);
    GDALRasterBlock* poBlock = NULL;
    while( true )
    {
        {
            CPLLockHolderOptionalLockD( hLock );
            poBlock = reinterpret_cast<GDALRasterBlock*>(
                CPLHashSetLookup(hSet, &oBlockForLookup) );
        }
        if( poBlock == NULL )
            return NULL;
        if( poBlock->TakeLock()  )
            break;
    }

    poBlock->Touch();
    return poBlock;
}

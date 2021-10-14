/******************************************************************************
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

#include "cpl_port.h"
#include "gdal_priv.h"

#include <cstddef>
#include <algorithm>
#include <set>
#include <vector>

#include "cpl_config.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

/* ******************************************************************** */
/*                        GDALHashSetBandBlockCache                     */
/* ******************************************************************** */

class GDALHashSetBandBlockCache final : public GDALAbstractBandBlockCache
{
    struct BlockComparator
    {
        // Do not change this comparator, because this order is assumed by
        // tests like tiff_write_133 for flushing from top to bottom, left
        // to right.
        bool operator() (const GDALRasterBlock* const& lhs,
                         const GDALRasterBlock* const& rhs) const
        {
            if( lhs->GetYOff() < rhs->GetYOff() )
                return true;
            if( lhs->GetYOff() > rhs->GetYOff() )
                return false;
            return lhs->GetXOff() < rhs->GetXOff();
        }
    };

    std::set<GDALRasterBlock*, BlockComparator> m_oSet{};
    CPLLock        *hLock = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALHashSetBandBlockCache)

  public:
    explicit GDALHashSetBandBlockCache( GDALRasterBand* poBand );
    ~GDALHashSetBandBlockCache() override;

    bool Init() override;
    bool IsInitOK() override;
    CPLErr FlushCache() override;
    CPLErr AdoptBlock( GDALRasterBlock * ) override;
    GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff,
                                           int nYBlockYOff ) override;
    CPLErr UnreferenceBlock( GDALRasterBlock* poBlock ) override;
    CPLErr FlushBlock( int nXBlockOff, int nYBlockOff,
                       int bWriteDirtyBlock ) override;
};

/************************************************************************/
/*                     GDALHashSetBandBlockCacheCreate()                */
/************************************************************************/

GDALAbstractBandBlockCache* GDALHashSetBandBlockCacheCreate(
    GDALRasterBand* poBand )
{
    return new GDALHashSetBandBlockCache(poBand);
}

/************************************************************************/
/*                       GDALHashSetBandBlockCache()                    */
/************************************************************************/

GDALHashSetBandBlockCache::GDALHashSetBandBlockCache(
    GDALRasterBand* poBandIn ) :
    GDALAbstractBandBlockCache(poBandIn),

    hLock(CPLCreateLock(LOCK_ADAPTIVE_MUTEX))
{}

/************************************************************************/
/*                      ~GDALHashSetBandBlockCache()                    */
/************************************************************************/

GDALHashSetBandBlockCache::~GDALHashSetBandBlockCache()
{
    GDALHashSetBandBlockCache::FlushCache();
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
    m_oSet.insert(poBlock);

    return CE_None;
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

CPLErr GDALHashSetBandBlockCache::FlushCache()
{
    FreeDanglingBlocks();

    CPLErr eGlobalErr = poBand->eFlushBlockErr;

    std::set<GDALRasterBlock*, BlockComparator> oOldSet;
    {
        CPLLockHolderOptionalLockD( hLock );
        oOldSet = std::move(m_oSet);
    }

    StartDirtyBlockFlushingLog();
    for( auto& poBlock: oOldSet )
    {
        if( poBlock->DropLockForRemovalFromStorage() )
        {
            CPLErr eErr = CE_None;

            if( m_bWriteDirtyBlocks && eGlobalErr == CE_None && poBlock->GetDirty() )
            {
                UpdateDirtyBlockFlushingLog();
                eErr = poBlock->Write();
            }

            delete poBlock;

            if( eErr != CE_None )
                eGlobalErr = eErr;
        }
    }
    EndDirtyBlockFlushingLog();

    WaitCompletionPendingTasks();

    return( eGlobalErr );
}

/************************************************************************/
/*                        UnreferenceBlock()                            */
/************************************************************************/

CPLErr GDALHashSetBandBlockCache::UnreferenceBlock( GDALRasterBlock* poBlock )
{
    UnreferenceBlockBase();

    CPLLockHolderOptionalLockD( hLock );
    m_oSet.erase(poBlock);
    return CE_None;
}

/************************************************************************/
/*                            FlushBlock()                              */
/************************************************************************/

CPLErr GDALHashSetBandBlockCache::FlushBlock( int nXBlockOff, int nYBlockOff,
                                              int bWriteDirtyBlock )

{
    GDALRasterBlock oBlockForLookup(nXBlockOff, nYBlockOff);
    GDALRasterBlock* poBlock = nullptr;
    {
        CPLLockHolderOptionalLockD( hLock );
        auto oIter = m_oSet.find(&oBlockForLookup);
        if( oIter == m_oSet.end() )
            return CE_None;
        poBlock = *oIter;
        m_oSet.erase(oIter);
    }

    if( !poBlock->DropLockForRemovalFromStorage() )
        return CE_None;

    CPLErr eErr = CE_None;

    if( m_bWriteDirtyBlocks && bWriteDirtyBlock && poBlock->GetDirty() )
        eErr = poBlock->Write();

    delete poBlock;

    return eErr;
}

/************************************************************************/
/*                        TryGetLockedBlockRef()                        */
/************************************************************************/

GDALRasterBlock *GDALHashSetBandBlockCache::TryGetLockedBlockRef(
    int nXBlockOff, int nYBlockOff )

{
    GDALRasterBlock oBlockForLookup(nXBlockOff, nYBlockOff);
    GDALRasterBlock* poBlock;
    {
        CPLLockHolderOptionalLockD( hLock );
        auto oIter = m_oSet.find(&oBlockForLookup);
        if( oIter == m_oSet.end() )
            return nullptr;
        poBlock = *oIter;
    }
    if( !poBlock->TakeLock()  )
        return nullptr;
    return poBlock;
}

//! @endcond

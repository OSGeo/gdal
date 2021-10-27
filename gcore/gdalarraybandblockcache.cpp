/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Store cached blocks in a array or a two-level array
 * Author:   Even Rouault, <even dot rouault at spatialys dot org>
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot org>
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

#include <cassert>
#include <climits>
#include <cstddef>
#include <new>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_vsi.h"

//! @cond Doxygen_Suppress

constexpr int SUBBLOCK_SIZE = 64;
#define TO_SUBBLOCK(x) ((x) >> 6)
#define WITHIN_SUBBLOCK(x) ((x) & 0x3f)

CPL_CVSID("$Id$")

/* ******************************************************************** */
/*                        GDALArrayBandBlockCache                       */
/* ******************************************************************** */

class GDALArrayBandBlockCache final : public GDALAbstractBandBlockCache
{
    bool              bSubBlockingActive = false;
    int               nSubBlocksPerRow = 0;
    int               nSubBlocksPerColumn = 0;
    union u
    {
        GDALRasterBlock **papoBlocks;
        GDALRasterBlock ***papapoBlocks;

        u(): papoBlocks(nullptr) {}
    } u{};

    CPL_DISALLOW_COPY_ASSIGN(GDALArrayBandBlockCache)

  public:
    explicit GDALArrayBandBlockCache( GDALRasterBand* poBand );
    ~GDALArrayBandBlockCache() override;

    bool Init() override;
    bool IsInitOK() override;
    CPLErr FlushCache() override;
    CPLErr AdoptBlock( GDALRasterBlock * )  override;
    GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff,
                                           int nYBlockYOff ) override;
    CPLErr UnreferenceBlock( GDALRasterBlock* poBlock ) override;
    CPLErr FlushBlock( int nXBlockOff, int nYBlockOff,
                       int bWriteDirtyBlock ) override;
};

/************************************************************************/
/*                     GDALArrayBandBlockCacheCreate()                 */
/************************************************************************/

GDALAbstractBandBlockCache* GDALArrayBandBlockCacheCreate(GDALRasterBand* poBand)
{
    return new (std::nothrow) GDALArrayBandBlockCache(poBand);
}

/************************************************************************/
/*                       GDALArrayBandBlockCache()                      */
/************************************************************************/

GDALArrayBandBlockCache::GDALArrayBandBlockCache(GDALRasterBand* poBandIn) :
    GDALAbstractBandBlockCache(poBandIn)
{
}

/************************************************************************/
/*                      ~GDALArrayBandBlockCache()                     */
/************************************************************************/

GDALArrayBandBlockCache::~GDALArrayBandBlockCache()
{
    GDALArrayBandBlockCache::FlushCache();

    if( !bSubBlockingActive )
        CPLFree( u.papoBlocks );
    else
        CPLFree( u.papapoBlocks );
}

/************************************************************************/
/*                                  Init()                              */
/************************************************************************/

bool GDALArrayBandBlockCache::Init()
{
    if( poBand->nBlocksPerRow < SUBBLOCK_SIZE/2 )
    {
        bSubBlockingActive = false;

        if (poBand->nBlocksPerRow < INT_MAX / poBand->nBlocksPerColumn)
        {
            u.papoBlocks = static_cast<GDALRasterBlock **>(
                VSICalloc( sizeof(void*),
                           poBand->nBlocksPerRow * poBand->nBlocksPerColumn ) );
            if( u.papoBlocks == nullptr )
            {
                poBand->ReportError( CE_Failure, CPLE_OutOfMemory,
                                    "Out of memory in InitBlockInfo()." );
                return false;
            }
        }
        else
        {
            poBand->ReportError( CE_Failure, CPLE_NotSupported,
                                 "Too many blocks : %d x %d",
                                 poBand->nBlocksPerRow, poBand->nBlocksPerColumn );
            return false;
        }
    }
    else
    {
        bSubBlockingActive = true;

        nSubBlocksPerRow = DIV_ROUND_UP(poBand->nBlocksPerRow, SUBBLOCK_SIZE);
        nSubBlocksPerColumn = DIV_ROUND_UP(poBand->nBlocksPerColumn, SUBBLOCK_SIZE);

        if (nSubBlocksPerRow < INT_MAX / nSubBlocksPerColumn)
        {
            u.papapoBlocks = static_cast<GDALRasterBlock ***>(
                VSICalloc( sizeof(void*),
                           nSubBlocksPerRow * nSubBlocksPerColumn ) );
            if( u.papapoBlocks == nullptr )
            {
                poBand->ReportError( CE_Failure, CPLE_OutOfMemory,
                                    "Out of memory in InitBlockInfo()." );
                return false;
            }
        }
        else
        {
            poBand->ReportError( CE_Failure, CPLE_NotSupported,
                                 "Too many subblocks : %d x %d",
                                 nSubBlocksPerRow, nSubBlocksPerColumn );
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                             IsInitOK()                               */
/************************************************************************/

bool GDALArrayBandBlockCache::IsInitOK()
{
    return (!bSubBlockingActive) ?
        u.papoBlocks != nullptr : u.papapoBlocks != nullptr;
}

/************************************************************************/
/*                            AdoptBlock()                              */
/************************************************************************/

CPLErr GDALArrayBandBlockCache::AdoptBlock( GDALRasterBlock * poBlock )

{
    const int nXBlockOff = poBlock->GetXOff();
    const int nYBlockOff = poBlock->GetYOff();

    FreeDanglingBlocks();

/* -------------------------------------------------------------------- */
/*      Simple case without subblocking.                                */
/* -------------------------------------------------------------------- */

    if( !bSubBlockingActive )
    {
        const int nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        CPLAssert( u.papoBlocks[nBlockIndex] == nullptr );
        u.papoBlocks[nBlockIndex] = poBlock;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Identify the subblock in which our target occurs, and create    */
/*      it if necessary.                                                */
/* -------------------------------------------------------------------- */
        const int nSubBlock = TO_SUBBLOCK(nXBlockOff)
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

        if( u.papapoBlocks[nSubBlock] == nullptr )
        {
            const int nSubGridSize =
                sizeof(GDALRasterBlock*) * SUBBLOCK_SIZE * SUBBLOCK_SIZE;

            u.papapoBlocks[nSubBlock] = static_cast<GDALRasterBlock **>(
                VSICalloc(1, nSubGridSize) );
            if( u.papapoBlocks[nSubBlock] == nullptr )
            {
                poBand->ReportError( CE_Failure, CPLE_OutOfMemory,
                        "Out of memory in AdoptBlock()." );
                return CE_Failure;
            }
        }

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = u.papapoBlocks[nSubBlock];

        const int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        CPLAssert( papoSubBlockGrid[nBlockInSubBlock] == nullptr );
        papoSubBlockGrid[nBlockInSubBlock] = poBlock;
    }

    return CE_None;
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

CPLErr GDALArrayBandBlockCache::FlushCache()
{
    FreeDanglingBlocks();

    CPLErr eGlobalErr = poBand->eFlushBlockErr;

    StartDirtyBlockFlushingLog();

/* -------------------------------------------------------------------- */
/*      Flush all blocks in memory ... this case is without subblocking.*/
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive && u.papoBlocks != nullptr )
    {
        const int nBlocksPerColumn = poBand->nBlocksPerColumn;
        const int nBlocksPerRow = poBand->nBlocksPerRow;
        for( int iY = 0; iY < nBlocksPerColumn; iY++ )
        {
            for( int iX = 0; iX < nBlocksPerRow; iX++ )
            {
                if( u.papoBlocks[iX + iY*nBlocksPerRow] != nullptr )
                {
                    CPLErr eErr = FlushBlock( iX, iY, eGlobalErr == CE_None );

                    if( eErr != CE_None )
                        eGlobalErr = eErr;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      With subblocking.  We can short circuit missing subblocks.      */
/* -------------------------------------------------------------------- */
    else if( u.papapoBlocks != nullptr )
    {
        for( int iSBY = 0; iSBY < nSubBlocksPerColumn; iSBY++ )
        {
            for( int iSBX = 0; iSBX < nSubBlocksPerRow; iSBX++ )
            {
                const int nSubBlock = iSBX + iSBY * nSubBlocksPerRow;

                GDALRasterBlock **papoSubBlockGrid =  u.papapoBlocks[nSubBlock];

                if( papoSubBlockGrid == nullptr )
                    continue;

                for( int iY = 0; iY < SUBBLOCK_SIZE; iY++ )
                {
                    for( int iX = 0; iX < SUBBLOCK_SIZE; iX++ )
                    {
                        if( papoSubBlockGrid[iX + iY * SUBBLOCK_SIZE] != nullptr )
                        {
                            CPLErr eErr = FlushBlock( iX + iSBX * SUBBLOCK_SIZE,
                                                      iY + iSBY * SUBBLOCK_SIZE,
                                                      eGlobalErr == CE_None );
                            if( eErr != CE_None )
                                eGlobalErr = eErr;
                        }
                    }
                }

                // We might as well get rid of this grid chunk since we know
                // it is now empty.
                u.papapoBlocks[nSubBlock] = nullptr;
                CPLFree( papoSubBlockGrid );
            }
        }
    }

    EndDirtyBlockFlushingLog();

    WaitCompletionPendingTasks();

    return( eGlobalErr );
}

/************************************************************************/
/*                        UnreferenceBlock()                            */
/************************************************************************/

CPLErr GDALArrayBandBlockCache::UnreferenceBlock( GDALRasterBlock* poBlock )
{
    const int nXBlockOff = poBlock->GetXOff();
    const int nYBlockOff = poBlock->GetYOff();

    UnreferenceBlockBase();

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        const int nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        u.papoBlocks[nBlockIndex] = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        const int nSubBlock = TO_SUBBLOCK(nXBlockOff)
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = u.papapoBlocks[nSubBlock];
        if( papoSubBlockGrid == nullptr )
            return CE_None;

        const int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        papoSubBlockGrid[nBlockInSubBlock] = nullptr;
    }

    return CE_None;
}

/************************************************************************/
/*                            FlushBlock()                              */
/************************************************************************/

CPLErr GDALArrayBandBlockCache::FlushBlock( int nXBlockOff, int nYBlockOff,
                                             int bWriteDirtyBlock )

{
    GDALRasterBlock *poBlock = nullptr;

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        const int nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        assert(u.papoBlocks);
        poBlock = u.papoBlocks[nBlockIndex];
        u.papoBlocks[nBlockIndex] = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        const int nSubBlock = TO_SUBBLOCK(nXBlockOff)
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = u.papapoBlocks[nSubBlock];
        if( papoSubBlockGrid == nullptr )
            return CE_None;

        const int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        poBlock = papoSubBlockGrid[nBlockInSubBlock];
        papoSubBlockGrid[nBlockInSubBlock] = nullptr;
    }

    if( poBlock == nullptr )
        return CE_None;

    if( !poBlock->DropLockForRemovalFromStorage() )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Is the target block dirty?  If so we need to write it.          */
/* -------------------------------------------------------------------- */
    poBlock->Detach();

    CPLErr eErr = CE_None;

    if( m_bWriteDirtyBlocks && bWriteDirtyBlock && poBlock->GetDirty() )
    {
        UpdateDirtyBlockFlushingLog();

        eErr = poBlock->Write();
    }

/* -------------------------------------------------------------------- */
/*      Deallocate the block;                                           */
/* -------------------------------------------------------------------- */
    delete poBlock;

    return eErr;
}

/************************************************************************/
/*                        TryGetLockedBlockRef()                        */
/************************************************************************/

GDALRasterBlock *GDALArrayBandBlockCache::TryGetLockedBlockRef( int nXBlockOff,
                                                                 int nYBlockOff )

{
/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        const int nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        GDALRasterBlock* poBlock = u.papoBlocks[nBlockIndex];
        if( poBlock == nullptr || !poBlock->TakeLock() )
            return nullptr;
        return poBlock;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
        const int nSubBlock = TO_SUBBLOCK(nXBlockOff)
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = u.papapoBlocks[nSubBlock];
        if( papoSubBlockGrid == nullptr )
            return nullptr;

        const int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        GDALRasterBlock* poBlock = papoSubBlockGrid[nBlockInSubBlock];
        if( poBlock == nullptr || !poBlock->TakeLock() )
            return nullptr;
        return poBlock;
    }
}

//! @endcond

/******************************************************************************
 * $Id$
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

#include "gdal_priv.h"
#include "cpl_multiproc.h"

#define SUBBLOCK_SIZE 64
#define TO_SUBBLOCK(x) ((x) >> 6)
#define WITHIN_SUBBLOCK(x) ((x) & 0x3f)

CPL_CVSID("$Id$");

/* ******************************************************************** */
/*                        GDALArrayBandBlockCache                       */
/* ******************************************************************** */

class GDALArrayBandBlockCache: public GDALAbstractBandBlockCache
{
    int               bSubBlockingActive;
    int               nSubBlocksPerRow;
    int               nSubBlocksPerColumn;
    union
    {
        GDALRasterBlock **papoBlocks;
        GDALRasterBlock ***papapoBlocks;
    } u;

    public:
            GDALArrayBandBlockCache(GDALRasterBand* poBand);
           ~GDALArrayBandBlockCache();
           
           virtual int              Init();
           virtual int              IsInitOK();
           virtual CPLErr           FlushCache();
           virtual CPLErr           AdoptBlock( GDALRasterBlock * );
           virtual GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff, int nYBlockYOff );
           virtual CPLErr           UnreferenceBlock( GDALRasterBlock* poBlock );
           virtual CPLErr           FlushBlock( int nXBlockOff, int nYBlockOff, int bWriteDirtyBlock );
};

/************************************************************************/
/*                     GDALArrayBandBlockCacheCreate()                 */
/************************************************************************/

GDALAbstractBandBlockCache* GDALArrayBandBlockCacheCreate(GDALRasterBand* poBand)
{
    return new GDALArrayBandBlockCache(poBand);
}

/************************************************************************/
/*                       GDALArrayBandBlockCache()                      */
/************************************************************************/

GDALArrayBandBlockCache::GDALArrayBandBlockCache(GDALRasterBand* poBand) :
                                            GDALAbstractBandBlockCache(poBand)
{
    nSubBlocksPerRow = 0;
    nSubBlocksPerColumn = 0;
    bSubBlockingActive = FALSE;
    u.papoBlocks = NULL;
}

/************************************************************************/
/*                      ~GDALArrayBandBlockCache()                     */
/************************************************************************/

GDALArrayBandBlockCache::~GDALArrayBandBlockCache()
{
    FlushCache();

    if( !bSubBlockingActive )
        CPLFree( u.papoBlocks );
    else
        CPLFree( u.papapoBlocks );
}

/************************************************************************/
/*                                  Init()                              */
/************************************************************************/

int GDALArrayBandBlockCache::Init()
{
    if( poBand->nBlocksPerRow < SUBBLOCK_SIZE/2 )
    {
        bSubBlockingActive = FALSE;

        if (poBand->nBlocksPerRow < INT_MAX / poBand->nBlocksPerColumn)
        {
            u.papoBlocks = (GDALRasterBlock **)
                VSICalloc( sizeof(void*), poBand->nBlocksPerRow * poBand->nBlocksPerColumn );
            if( u.papoBlocks == NULL )
            {
                poBand->ReportError( CE_Failure, CPLE_OutOfMemory,
                                    "Out of memory in InitBlockInfo()." );
                return FALSE;
            }
        }
        else
        {
            poBand->ReportError( CE_Failure, CPLE_NotSupported,
                                 "Too many blocks : %d x %d",
                                 poBand->nBlocksPerRow, poBand->nBlocksPerColumn );
            return FALSE;
        }
    }
    else
    {
        bSubBlockingActive = TRUE;

        nSubBlocksPerRow = DIV_ROUND_UP(poBand->nBlocksPerRow, SUBBLOCK_SIZE);
        nSubBlocksPerColumn = DIV_ROUND_UP(poBand->nBlocksPerColumn, SUBBLOCK_SIZE);

        if (nSubBlocksPerRow < INT_MAX / nSubBlocksPerColumn)
        {
            u.papapoBlocks = (GDALRasterBlock ***)
                VSICalloc( sizeof(void*), nSubBlocksPerRow * nSubBlocksPerColumn );
            if( u.papapoBlocks == NULL )
            {
                poBand->ReportError( CE_Failure, CPLE_OutOfMemory,
                                    "Out of memory in InitBlockInfo()." );
                return FALSE;
            }

        }
        else
        {
            poBand->ReportError( CE_Failure, CPLE_NotSupported,
                                 "Too many subblocks : %d x %d",
                                 nSubBlocksPerRow, nSubBlocksPerColumn );
            return FALSE;
        }
    }

    
    return TRUE;
}

/************************************************************************/
/*                             IsInitOK()                               */
/************************************************************************/

int GDALArrayBandBlockCache::IsInitOK()
{
    return (!bSubBlockingActive) ? u.papoBlocks != NULL : u.papapoBlocks != NULL;
}

/************************************************************************/
/*                            AdoptBlock()                              */
/************************************************************************/

CPLErr GDALArrayBandBlockCache::AdoptBlock( GDALRasterBlock * poBlock )

{
    int         nBlockIndex;
    int         nXBlockOff = poBlock->GetXOff();
    int         nYBlockOff = poBlock->GetYOff();

    FreeDanglingBlocks();

/* -------------------------------------------------------------------- */
/*      Simple case without subblocking.                                */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        CPLAssert( u.papoBlocks[nBlockIndex] == NULL );
        u.papoBlocks[nBlockIndex] = poBlock;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Identify the subblock in which our target occurs, and create    */
/*      it if necessary.                                                */
/* -------------------------------------------------------------------- */
        int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

        if( u.papapoBlocks[nSubBlock] == NULL )
        {
            const int nSubGridSize = 
                sizeof(GDALRasterBlock*) * SUBBLOCK_SIZE * SUBBLOCK_SIZE;

            u.papapoBlocks[nSubBlock] = (GDALRasterBlock **) VSICalloc(1, nSubGridSize);
            if( u.papapoBlocks[nSubBlock] == NULL )
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

        int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        CPLAssert( papoSubBlockGrid[nBlockInSubBlock] == NULL );
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

/* -------------------------------------------------------------------- */
/*      Flush all blocks in memory ... this case is without subblocking.*/
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        for( int iY = 0; iY < poBand->nBlocksPerColumn; iY++ )
        {
            for( int iX = 0; iX < poBand->nBlocksPerRow; iX++ )
            {
                if( u.papoBlocks[iX + iY*poBand->nBlocksPerRow] != NULL )
                {
                    CPLErr    eErr;

                    eErr = FlushBlock( iX, iY, eGlobalErr == CE_None );

                    if( eErr != CE_None )
                        eGlobalErr = eErr;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      With subblocking.  We can short circuit missing subblocks.      */
/* -------------------------------------------------------------------- */
    else
    {
        int iSBX, iSBY;

        for( iSBY = 0; iSBY < nSubBlocksPerColumn; iSBY++ )
        {
            for( iSBX = 0; iSBX < nSubBlocksPerRow; iSBX++ )
            {
                int nSubBlock = iSBX + iSBY * nSubBlocksPerRow;
            
                GDALRasterBlock **papoSubBlockGrid =  u.papapoBlocks[nSubBlock];

                if( papoSubBlockGrid == NULL )
                    continue;

                for( int iY = 0; iY < SUBBLOCK_SIZE; iY++ )
                {
                    for( int iX = 0; iX < SUBBLOCK_SIZE; iX++ )
                    {
                        if( papoSubBlockGrid[iX + iY * SUBBLOCK_SIZE] != NULL )
                        {
                            CPLErr eErr;

                            eErr = FlushBlock( iX + iSBX * SUBBLOCK_SIZE, 
                                            iY + iSBY * SUBBLOCK_SIZE,
                                            eGlobalErr == CE_None );
                            if( eErr != CE_None )
                                eGlobalErr = eErr;
                        }
                    }
                }

                // We might as well get rid of this grid chunk since we know 
                // it is now empty.
                u.papapoBlocks[nSubBlock] = NULL;
                CPLFree( papoSubBlockGrid );
            }
        }
    }

    WaitKeepAliveCounter();

    return( eGlobalErr );
}

/************************************************************************/
/*                        UnreferenceBlock()                            */
/************************************************************************/

CPLErr GDALArrayBandBlockCache::UnreferenceBlock( GDALRasterBlock* poBlock )
{
    int nXBlockOff = poBlock->GetXOff();
    int nYBlockOff = poBlock->GetYOff();

    UnreferenceBlockBase();

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        int nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        u.papoBlocks[nBlockIndex] = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = u.papapoBlocks[nSubBlock];
        if( papoSubBlockGrid == NULL )
            return CE_None;

        int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        papoSubBlockGrid[nBlockInSubBlock] = NULL;
    }

    return CE_None;
}

/************************************************************************/
/*                            FlushBlock()                              */
/************************************************************************/

CPLErr GDALArrayBandBlockCache::FlushBlock( int nXBlockOff, int nYBlockOff,
                                             int bWriteDirtyBlock )

{
    int             nBlockIndex;
    GDALRasterBlock *poBlock = NULL;

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        poBlock = u.papoBlocks[nBlockIndex];
        u.papoBlocks[nBlockIndex] = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;
        
/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid =  u.papapoBlocks[nSubBlock];
        if( papoSubBlockGrid == NULL )
            return CE_None;

        int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        poBlock = papoSubBlockGrid[nBlockInSubBlock];
        papoSubBlockGrid[nBlockInSubBlock] = NULL;
    }

    if( poBlock == NULL )
        return CE_None;

    if( !poBlock->DropLockForRemovalFromStorage() )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Is the target block dirty?  If so we need to write it.          */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    poBlock->Detach();

    if( bWriteDirtyBlock && poBlock->GetDirty() )
        eErr = poBlock->Write();

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
    GDALRasterBlock *poBlock;
    int             nBlockIndex = 0;

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * poBand->nBlocksPerRow;

        while( TRUE )
        {
            poBlock = u.papoBlocks[nBlockIndex];
            if( poBlock == NULL )
                return NULL;
            if( poBlock->TakeLock() )
                break;
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
        int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = u.papapoBlocks[nSubBlock];
        if( papoSubBlockGrid == NULL )
            return NULL;

        int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

        while( TRUE )
        {
            poBlock = papoSubBlockGrid[nBlockInSubBlock];
            if( poBlock == NULL )
                return NULL;
            if( poBlock->TakeLock() )
                break;
        }
    }

    return poBlock;
}

/******************************************************************************
 *
 * Name:     gdal_abstractbandblockcache.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALAbstractBandBlockCache base class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALABSTRACTBANDBLOCKCACHE_H_INCLUDED
#define GDALABSTRACTBANDBLOCKCACHE_H_INCLUDED

#include "cpl_port.h"
#include "cpl_error.h"

typedef struct _CPLCond CPLCond;
typedef struct _CPLLock CPLLock;
typedef struct _CPLMutex CPLMutex;

class GDALRasterBand;
class GDALRasterBlock;

/* ******************************************************************** */
/*                       GDALAbstractBandBlockCache                     */
/* ******************************************************************** */

//! @cond Doxygen_Suppress

//! This manages how a raster band store its cached block.
// only used by GDALRasterBand implementation.

class GDALAbstractBandBlockCache /* non final */
{
    // List of blocks that can be freed or recycled, and its lock
    CPLLock *hSpinLock = nullptr;
    GDALRasterBlock *psListBlocksToFree = nullptr;

    // Band keep alive counter, and its lock & condition
    CPLCond *hCond = nullptr;
    CPLMutex *hCondMutex = nullptr;
    volatile int nKeepAliveCounter = 0;

    volatile int m_nDirtyBlocks = 0;

    CPL_DISALLOW_COPY_ASSIGN(GDALAbstractBandBlockCache)

  protected:
    explicit GDALAbstractBandBlockCache(GDALRasterBand *poBand);

    GDALRasterBand *poBand;

    int m_nInitialDirtyBlocksInFlushCache = 0;
    int m_nLastTick = -1;
    size_t m_nWriteDirtyBlocksDisabled = 0;

    void FreeDanglingBlocks();
    void UnreferenceBlockBase();

    void StartDirtyBlockFlushingLog();
    void UpdateDirtyBlockFlushingLog();
    void EndDirtyBlockFlushingLog();

  public:
    virtual ~GDALAbstractBandBlockCache();

    GDALRasterBlock *CreateBlock(int nXBlockOff, int nYBlockOff);
    void AddBlockToFreeList(GDALRasterBlock *poBlock);
    void IncDirtyBlocks(int nInc);
    void WaitCompletionPendingTasks();

    void EnableDirtyBlockWriting()
    {
        --m_nWriteDirtyBlocksDisabled;
    }

    void DisableDirtyBlockWriting()
    {
        ++m_nWriteDirtyBlocksDisabled;
    }

    bool HasDirtyBlocks() const
    {
        return m_nDirtyBlocks > 0;
    }

    virtual bool Init() = 0;
    virtual bool IsInitOK() = 0;
    virtual CPLErr FlushCache() = 0;
    virtual CPLErr AdoptBlock(GDALRasterBlock *poBlock) = 0;
    virtual GDALRasterBlock *TryGetLockedBlockRef(int nXBlockOff,
                                                  int nYBlockYOff) = 0;
    virtual CPLErr UnreferenceBlock(GDALRasterBlock *poBlock) = 0;
    virtual CPLErr FlushBlock(int nXBlockOff, int nYBlockOff,
                              int bWriteDirtyBlock) = 0;
};

GDALAbstractBandBlockCache *
GDALArrayBandBlockCacheCreate(GDALRasterBand *poBand);
GDALAbstractBandBlockCache *
GDALHashSetBandBlockCacheCreate(GDALRasterBand *poBand);

//! @endcond

#endif

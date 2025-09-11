/******************************************************************************
 *
 * Name:     gdal_rasterblock.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALRasterBlock class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALRASTERBLOCK_H_INCLUDED
#define GDALRASTERBLOCK_H_INCLUDED

#include "cpl_port.h"
#include "cpl_atomic_ops.h"
#include "gdal.h"

/* ******************************************************************** */
/*                           GDALRasterBlock                            */
/* ******************************************************************** */

class GDALRasterBand;

/** A single raster block in the block cache.
 *
 * And the global block manager that manages a least-recently-used list of
 * blocks from various datasets/bands */
class CPL_DLL GDALRasterBlock final
{
    friend class GDALAbstractBandBlockCache;

    GDALDataType eType = GDT_Unknown;

    bool bDirty = false;
    volatile int nLockCount = 0;

    int nXOff = 0;
    int nYOff = 0;

    int nXSize = 0;
    int nYSize = 0;

    void *pData = nullptr;

    GDALRasterBand *poBand = nullptr;

    GDALRasterBlock *poNext = nullptr;
    GDALRasterBlock *poPrevious = nullptr;

    bool bMustDetach = false;

    CPL_INTERNAL void Detach_unlocked(void);
    CPL_INTERNAL void Touch_unlocked(void);

    CPL_INTERNAL void RecycleFor(int nXOffIn, int nYOffIn);

  public:
    GDALRasterBlock(GDALRasterBand *, int, int);
    GDALRasterBlock(int nXOffIn, int nYOffIn); /* only for lookup purpose */
    ~GDALRasterBlock();

    CPLErr Internalize(void);
    void Touch(void);
    void MarkDirty(void);
    void MarkClean(void);

    /** Increment the lock count */
    int AddLock(void)
    {
        return CPLAtomicInc(&nLockCount);
    }

    /** Decrement the lock count */
    int DropLock(void)
    {
        return CPLAtomicDec(&nLockCount);
    }

    void Detach();

    CPLErr Write();

    /** Return the data type
     * @return data type
     */
    GDALDataType GetDataType() const
    {
        return eType;
    }

    /** Return the x offset of the top-left corner of the block
     * @return x offset
     */
    int GetXOff() const
    {
        return nXOff;
    }

    /** Return the y offset of the top-left corner of the block
     * @return y offset
     */
    int GetYOff() const
    {
        return nYOff;
    }

    /** Return the width of the block
     * @return width
     */
    int GetXSize() const
    {
        return nXSize;
    }

    /** Return the height of the block
     * @return height
     */
    int GetYSize() const
    {
        return nYSize;
    }

    /** Return the dirty flag
     * @return dirty flag
     */
    int GetDirty() const
    {
        return bDirty;
    }

    /** Return the data buffer
     * @return data buffer
     */
    void *GetDataRef(void)
    {
        return pData;
    }

    /** Return the block size in bytes
     * @return block size.
     */
    GPtrDiff_t GetBlockSize() const
    {
        return static_cast<GPtrDiff_t>(nXSize) * nYSize *
               GDALGetDataTypeSizeBytes(eType);
    }

    int TakeLock();
    int DropLockForRemovalFromStorage();

    /// @brief Accessor to source GDALRasterBand object.
    /// @return source raster band of the raster block.
    GDALRasterBand *GetBand()
    {
        return poBand;
    }

    static void FlushDirtyBlocks();
    static int FlushCacheBlock(int bDirtyBlocksOnly = FALSE);
    static void Verify();

    static void EnterDisableDirtyBlockFlush();
    static void LeaveDisableDirtyBlockFlush();

#ifdef notdef
    static void CheckNonOrphanedBlocks(GDALRasterBand *poBand);
    void DumpBlock();
    static void DumpAll();
#endif

    /* Should only be called by GDALDestroyDriverManager() */
    //! @cond Doxygen_Suppress
    CPL_INTERNAL static void DestroyRBMutex();
    //! @endcond

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALRasterBlock)
};

#endif

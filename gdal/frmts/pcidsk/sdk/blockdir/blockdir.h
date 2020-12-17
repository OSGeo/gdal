/******************************************************************************
 *
 * Purpose:  Block directory API.
 *
 ******************************************************************************
 * Copyright (c) 2011
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#ifndef PCIDSK_BLOCK_DIR_H
#define PCIDSK_BLOCK_DIR_H

#include "pcidsk_config.h"
#include <vector>
#include <string>

namespace PCIDSK
{

class BlockFile;
class BlockLayer;

#define INVALID_SEGMENT ((uint16) -1)
#define INVALID_LAYER   ((uint32) -1)
#define INVALID_BLOCK   ((uint32) -1)
#define INVALID_OFFSET  ((uint64) -1)

#pragma pack(push, 1)

/// The block info structure.
struct BlockInfo
{
    uint16 nSegment;
    uint32 nStartBlock;
};

#pragma pack(pop)

/// The block layer list type.
typedef std::vector<BlockLayer *> BlockLayerList;

/// The block info list type.
typedef std::vector<BlockInfo> BlockInfoList;

/************************************************************************/
/*                              class BlockDir                          */
/************************************************************************/

/**
 * Class used as the base class for all block directories.
 */
class PCIDSK_DLL BlockDir
{
protected:
    /// The associated file.
    BlockFile *         mpoFile;

    /// The block directory segment.
    uint16              mnSegment;

    /// The block directory version.
    uint16              mnVersion;

    // The endianness of the block directory on disk.
    char                mchEndianness;

    // If the block directory on disk needs swapping.
    bool                mbNeedsSwap;

    /// The block directory validity info.
    uint16              mnValidInfo;

    /// If the block directory is modified.
    bool                mbModified;

    /// If the block directory is on disk.
    bool                mbOnDisk;

    /// The block layer list.
    BlockLayerList      moLayerList;

    /// The free block layer.
    BlockLayer *        mpoFreeBlockLayer;

    virtual void        ReadLayerBlocks(uint32 iLayer) = 0;
    virtual void        ReadFreeBlockLayer(void) = 0;
    virtual void        WriteDir(void) = 0;

    virtual BlockLayer *_CreateLayer(uint16 nLayerType, uint32 iLayer) = 0;
    virtual void        _DeleteLayer(uint32 iLayer) = 0;

    virtual uint32      GetNewBlockCount(void) const = 0;

    virtual std::string GetDataSegmentName(void) const = 0;
    virtual std::string GetDataSegmentDesc(void) const = 0;

    virtual void        ValidateNewBlocks(CPL_UNUSED uint32 & nBlockCount,
                                          CPL_UNUSED bool bFreeBlocks) { }

    void                SwapValue(uint16 * pnValue) const;

    // We need the block layer interface to be friend so that it can request
    // to read block information off of the disk.
    friend class BlockLayer;

public:
    BlockDir(BlockFile * poFile, uint16 nSegment);
    BlockDir(BlockFile * poFile, uint16 nSegment, uint16 nVersion);

    virtual             ~BlockDir(void);

/**
 * Gets the block size of the block directory.
 *
 * @return The block size of the block directory.
 */
    virtual uint32      GetBlockSize(void) const = 0;

    void                Sync(void);

    BlockFile *         GetFile(void) const;

    uint16              GetSegmentIndex(void) const;

    uint16              GetVersion(void) const;

    bool                NeedsSwap(void) const;

    bool                IsValid(void) const;

    bool                IsModified(void) const;

    uint32              GetLayerCount(void) const;
    uint16              GetLayerType(uint32 iLayer) const;
    uint64              GetLayerSize(uint32 iLayer) const;
    bool                IsLayerValid(uint32 iLayer) const;

    BlockLayer *        GetLayer(uint32 iLayer);

    uint32              CreateLayer(int16 nLayerType);

    void                DeleteLayer(uint32 iLayer);

    BlockInfoList       CreateNewBlocks(uint32 nBlockCount);

    void                CreateFreeBlocks(uint32 nBlockCount);

    void                AddFreeBlocks(const BlockInfoList & oBlockList);

    BlockInfo           GetFreeBlock(void);
};

} // namespace PCIDSK

#endif

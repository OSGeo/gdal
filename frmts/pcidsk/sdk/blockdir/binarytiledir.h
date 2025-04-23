/******************************************************************************
 *
 * Purpose:  Block directory API.
 *
 ******************************************************************************
 * Copyright (c) 2011
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef PCIDSK_BINARY_TILE_DIR_H
#define PCIDSK_BINARY_TILE_DIR_H

#include "blockdir/blocktiledir.h"

namespace PCIDSK
{

class BinaryTileLayer;

/************************************************************************/
/*                           class BinaryTileDir                        */
/************************************************************************/

/**
 * Class used to manage a binary block tile directory.
 *
 * @see BlockTileDir
 */
class PCIDSK_DLL BinaryTileDir : public BlockTileDir
{
public:
#pragma pack(push, 1)

    /// The block directory info.
    struct BlockDirInfo
    {
        uint32 nLayerCount;
        uint32 nBlockSize;
    };

#pragma pack(pop)

protected:
    // The block directory info.
    BlockDirInfo        msBlockDir;

    size_t              GetDirSize(void) const;

    void                InitBlockList(BinaryTileLayer * poLayer);

    virtual void        ReadLayerBlocks(uint32 iLayer) override;
    virtual void        ReadFreeBlockLayer(void) override;
    virtual void        WriteDir(void) override;

    virtual BlockLayer *_CreateLayer(uint16 nLayerType, uint32 iLayer) override;
    virtual void        _DeleteLayer(uint32 iLayer) override;

    virtual std::string GetDataSegmentName(void) const override;
    virtual std::string GetDataSegmentDesc(void) const override;

    void                SwapBlockDir(BlockDirInfo * psBlockDir);

public:
    static uint32       GetOptimizedBlockSize(BlockFile * poFile);
    static size_t       GetOptimizedDirSize(BlockFile * poFile);

    BinaryTileDir(BlockFile * poFile, uint16 nSegment);
    BinaryTileDir(BlockFile * poFile, uint16 nSegment, uint32 nBlockSize);

    BinaryTileLayer *   GetTileLayer(uint32 iLayer);

    virtual uint32      GetBlockSize(void) const override;
};

} // namespace PCIDSK

#endif

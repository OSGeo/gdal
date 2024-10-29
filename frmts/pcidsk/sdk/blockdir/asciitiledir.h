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

#ifndef PCIDSK_ASCII_TILE_DIR_H
#define PCIDSK_ASCII_TILE_DIR_H

#include "blockdir/blocktiledir.h"

namespace PCIDSK
{

class AsciiTileLayer;

/************************************************************************/
/*                            class AsciiTileDir                        */
/************************************************************************/

/**
 * Class used to manage a ascii block tile directory.
 *
 * @see BlockTileDir
 */
class PCIDSK_DLL AsciiTileDir : public BlockTileDir
{
public:
    /// The block directory info.
    struct BlockDirInfo
    {
        uint32 nLayerCount;
        uint32 nBlockCount;
        uint32 nFirstFreeBlock;
    };

protected:
    /// The block directory info.
    BlockDirInfo        msBlockDir;

    void                ReadFullDir(void);
    void                ReadPartialDir(void);

    size_t              GetDirSize(void) const;

    uint32              GetLayerBlockCount(void) const;
    uint32              GetFreeBlockCount(void) const;

    void                UpdateBlockDirInfo(void);

    void                InitBlockList(AsciiTileLayer * poLayer);

    virtual void        ReadLayerBlocks(uint32 iLayer) override;
    virtual void        ReadFreeBlockLayer(void) override;
    virtual void        WriteDir(void) override;

    virtual BlockLayer *_CreateLayer(uint16 nLayerType, uint32 iLayer) override;
    virtual void        _DeleteLayer(uint32 iLayer) override;

    virtual std::string GetDataSegmentName(void) const override;
    virtual std::string GetDataSegmentDesc(void) const override;

    virtual void        ValidateNewBlocks(uint32 & nNewBlockCount,
                                          bool bFreeBlocks) override;

public:
    static size_t       GetOptimizedDirSize(BlockFile * poFile);

    AsciiTileDir(BlockFile * poFile, uint16 nSegment);
    AsciiTileDir(BlockFile * poFile, uint16 nSegment, uint32 nBlockSize);

    AsciiTileLayer *    GetTileLayer(uint32 iLayer);

    virtual uint32      GetBlockSize(void) const override;
};

} // namespace PCIDSK

#endif

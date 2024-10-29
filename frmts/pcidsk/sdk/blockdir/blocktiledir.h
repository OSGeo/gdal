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

#ifndef PCIDSK_BLOCK_TILE_DIR_H
#define PCIDSK_BLOCK_TILE_DIR_H

#include "blockdir/blockdir.h"

namespace PCIDSK
{

class BlockTileLayer;

/************************************************************************/
/*                            class BlockTileDir                        */
/************************************************************************/

/**
 * Class used to manage a block tile directory.
 *
 * @see BlockDir
 */
class PCIDSK_DLL BlockTileDir : public BlockDir
{
public:
#pragma pack(push, 1)

    /// The block layer info structure.
    struct BlockLayerInfo
    {
        uint16 nLayerType = 0;
        uint32 nStartBlock = 0;
        uint32 nBlockCount = 0;
        uint64 nLayerSize = 0;
    };

    /// The tile layer info structure.
    struct TileLayerInfo
    {
        uint32 nXSize;
        uint32 nYSize;
        uint32 nTileXSize;
        uint32 nTileYSize;
        char   szDataType[4];
        char   szCompress[8];
        uint16 bNoDataValid;
        double dfNoDataValue;
    };

#pragma pack(pop)

    /// The block layer info list type.
    typedef std::vector<BlockLayerInfo *> BlockLayerInfoList;

    /// The tile layer info list type.
    typedef std::vector<TileLayerInfo *> TileLayerInfoList;

protected:
    /// The block layer info list.
    BlockLayerInfoList  moLayerInfoList;

    /// The tile layer info list.
    TileLayerInfoList   moTileLayerInfoList;

    /// The free block layer info.
    BlockLayerInfo      msFreeBlockLayer{};

    virtual uint32      GetNewBlockCount(void) const override;

    void                SwapBlockLayer(BlockLayerInfo * psBlockLayer);
    void                SwapTileLayer(TileLayerInfo * psTileLayer);
    void                SwapBlock(BlockInfo * psBlock, size_t nCount);

public:
    BlockTileDir(BlockFile * poFile, uint16 nSegment);
    BlockTileDir(BlockFile * poFile, uint16 nSegment, uint16 nVersion);

    virtual             ~BlockTileDir(void);

    BlockTileLayer *    GetTileLayer(uint32 iLayer);
};

} // namespace PCIDSK

#endif

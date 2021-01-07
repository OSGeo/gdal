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

#ifndef PCIDSK_BLOCK_TILE_LAYER_H
#define PCIDSK_BLOCK_TILE_LAYER_H

#include "blockdir/blocklayer.h"
#include "blockdir/blocktiledir.h"
#include "pcidsk_mutex.h"
#include <string>

namespace PCIDSK
{

/************************************************************************/
/*                           class BlockTileLayer                       */
/************************************************************************/

/**
 * Class used as the base class for all block tile layers.
 *
 * @see BlockLayer
 */
class PCIDSK_DLL BlockTileLayer : public BlockLayer
{
protected:
#pragma pack(push, 1)

    /// The block tile info structure.
    struct BlockTileInfo
    {
        uint64 nOffset;
        uint32 nSize;
    };

#pragma pack(pop)

    /// The block info list type.
    typedef std::vector<BlockTileInfo> BlockTileInfoList;

    /// The block layer info type.
    typedef BlockTileDir::BlockLayerInfo BlockLayerInfo;

    /// The tile layer info type.
    typedef BlockTileDir::TileLayerInfo TileLayerInfo;

    /// The block layer info.
    BlockLayerInfo *    mpsBlockLayer;

    /// The tile layer info.
    TileLayerInfo *     mpsTileLayer;

    /// The block tile info list.
    BlockTileInfoList   moTileList;

    /// The tile list mutex.
    Mutex *             mpoTileListMutex;

    /// If the block tile layer is modified.
    bool                mbModified;

    mutable char        mszDataType[5];
    mutable char        mszCompress[9];

/**
 * Sets the type of the layer.
 *
 * @param nLayerType The type of the layer.
 */
    virtual void        _SetLayerType(uint16 nLayerType) override
    {
        mpsBlockLayer->nLayerType = nLayerType;
    }

/**
 * Sets the number of blocks in the block layer.
 *
 * @param nBlockCount The number of blocks in the block layer.
 */
    virtual void        _SetBlockCount(uint32 nBlockCount) override
    {
        mpsBlockLayer->nBlockCount = nBlockCount;
    }

/**
 * Sets the size in bytes of the layer.
 *
 * @param nLayerSize The size in bytes of the layer.
 */
    virtual void        _SetLayerSize(uint64 nLayerSize) override
    {
        mpsBlockLayer->nLayerSize = nLayerSize;
    }

/**
 * Writes the tile list to disk.
 */
    virtual void        WriteTileList(void) = 0;

/**
 * Reads the tile list from disk.
 */
    virtual void        ReadTileList(void) = 0;

    BlockTileInfo *     GetTileInfo(uint32 nCol, uint32 nRow);

public:
    BlockTileLayer(BlockDir * poBlockDir, uint32 nLayer,
                   BlockLayerInfo * psBlockLayer,
                   TileLayerInfo * psTileLayer);

    virtual             ~BlockTileLayer(void);

    void                Sync(void);

    bool                IsCorrupted(void) const;

    uint32              GetTileCount(void) const;
    uint32              GetTilePerRow(void) const;
    uint32              GetTilePerCol(void) const;

    uint32              GetTileSize(void) const;

    uint32              GetDataTypeSize(void) const;

    bool                IsTileValid(uint32 nCol, uint32 nRow);

    uint32              GetTileDataSize(uint32 nCol, uint32 nRow);

    bool                WriteSparseTile(const void * pData,
                                        uint32 nCol, uint32 nRow);

    void                WriteTile(const void * pData,
                                  uint32 nCol, uint32 nRow, uint32 nSize = 0);

    bool                ReadSparseTile(void * pData,
                                       uint32 nCol, uint32 nRow);

    uint32              ReadTile(void * pData, uint32 nCol, uint32 nRow, uint32 nSize);

    bool                ReadPartialSparseTile(void * pData,
                                              uint32 nCol, uint32 nRow,
                                              uint32 nOffset, uint32 nSize);

    bool                ReadPartialTile(void * pData,
                                        uint32 nCol, uint32 nRow,
                                        uint32 nOffset, uint32 nSize);

    void                SetTileLayerInfo(uint32 nXSize, uint32 nYSize,
                                         uint32 nTileXSize, uint32 nTileYSize,
                                         const std::string & oDataType,
                                         const std::string & oCompress,
                                         bool bNoDataValid = false,
                                         double dfNoDataValue = 0.0);

/**
 * Gets the type of the layer.
 *
 * @return The type of the layer.
 */
    virtual uint16      GetLayerType(void) const override
    {
        return mpsBlockLayer->nLayerType;
    }

/**
 * Gets the number of blocks in the block layer.
 *
 * @return The number of blocks in the block layer.
 */
    virtual uint32      GetBlockCount(void) const override
    {
        return mpsBlockLayer->nBlockCount;
    }

/**
 * Gets the size in bytes of the layer.
 *
 * @return The size in bytes of the layer.
 */
    virtual uint64      GetLayerSize(void) const override
    {
        return mpsBlockLayer->nLayerSize;
    }

/**
 * Gets the width of the tile layer.
 *
 * @return The width of the tile layer.
 */
    uint32              GetXSize(void) const
    {
        return mpsTileLayer->nXSize;
    }

/**
 * Gets the height of the tile layer.
 *
 * @return The height of the tile layer.
 */
    uint32              GetYSize(void) const
    {
        return mpsTileLayer->nYSize;
    }

/**
 * Gets the width of a tile.
 *
 * @return The width of a tile.
 */
    uint32              GetTileXSize(void) const
    {
        return mpsTileLayer->nTileXSize;
    }

/**
 * Gets the height of a tile.
 *
 * @return The height of a tile.
 */
    uint32              GetTileYSize(void) const
    {
        return mpsTileLayer->nTileYSize;
    }

    const char *        GetDataType(void) const;

    const char *        GetCompressType(void) const;

/**
 * Checks if the NoData value is valid.
 *
 * @return If the NoData value is valid.
 */
    bool                IsNoDataValid(void) const
    {
        return mpsTileLayer->bNoDataValid != 0;
    }

/**
 * Gets the NoData value of the tile layer.
 *
 * @return The NoData value of the tile layer.
 */
    double              GetNoDataValue(void) const
    {
        return mpsTileLayer->dfNoDataValue;
    }
};

} // namespace PCIDSK

#endif

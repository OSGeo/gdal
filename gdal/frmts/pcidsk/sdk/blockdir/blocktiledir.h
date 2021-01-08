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
        uint16 nLayerType;
        uint32 nStartBlock;
        uint32 nBlockCount;
        uint64 nLayerSize;
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
    BlockLayerInfo      msFreeBlockLayer;

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

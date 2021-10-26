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

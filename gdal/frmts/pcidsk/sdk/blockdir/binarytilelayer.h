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

#ifndef PCIDSK_BINARY_TILE_LAYER_H
#define PCIDSK_BINARY_TILE_LAYER_H

#include "blockdir/blocktilelayer.h"
#include "blockdir/binarytiledir.h"

namespace PCIDSK
{

/************************************************************************/
/*                          class BinaryTileLayer                       */
/************************************************************************/

/**
 * Class used to manage a binary block tile layer.
 *
 * @see BlockTileLayer
 */
class PCIDSK_DLL BinaryTileLayer : public BlockTileLayer
{
protected:
    virtual void        WriteTileList(void) override;
    virtual void        ReadTileList(void) override;

    void                SwapBlockTile(BlockTileInfo * psTile, size_t nCount);

    // We need the tile block directory implementation class to be friend
    // since it is responsible to fill in the block list.
    friend class BinaryTileDir;

public:
    BinaryTileLayer(BlockDir * poBlockDir, uint32 nLayer,
                    BlockLayerInfo * psBlockLayer,
                    TileLayerInfo * psTileLayer);
};

} // namespace PCIDSK

#endif

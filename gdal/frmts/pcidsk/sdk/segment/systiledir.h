/******************************************************************************
 *
 * Purpose:  Declaration of the SysTileDir class.
 *
 * This class is used to manage access to the system block tile directory.
 * This segment is used to keep track of one or more tile layers stored in
 * system block data segments. These tile layers are used to hold tiled images
 * for primary bands or overviews.
 *
 * This class is closely partnered with the CTiledChannel class.
 *
 ******************************************************************************
 * Copyright (c) 2009
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

#ifndef PCIDSK_SEGMENT_SYSTEM_TILE_DIRECTORY_H
#define PCIDSK_SEGMENT_SYSTEM_TILE_DIRECTORY_H

#include "pcidsk_config.h"
#include "pcidsk_segment.h"
#include "segment/cpcidsksegment.h"

namespace PCIDSK
{

class BlockTileDir;
class BlockTileLayer;

class SysTileDir : virtual public CPCIDSKSegment
{
public:
    SysTileDir(PCIDSKFile * poFile, int nSegment, const char *pbySegmentData);

    virtual             ~SysTileDir(void);

    virtual void        Initialize(void) override final;
    virtual void        Synchronize(void) override final;

    void                LoadTileDir(void);
    void                CreateTileDir(void);

    BlockTileLayer *    GetTileLayer(uint32 nLayer);

    uint32              CreateTileLayer(uint32 nWidth, uint32 nHeight,
                                        uint32 nTileWidth, uint32 nTileHeight,
                                        eChanType nDataType,
                                        std::string oCompress);

private:
    BlockTileDir *      mpoTileDir;
};

} // namespace PCIDSK

#endif

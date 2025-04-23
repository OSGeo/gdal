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
 * SPDX-License-Identifier: MIT
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

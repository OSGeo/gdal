/******************************************************************************
 *
 * Purpose:  Declaration of the CTiledChannel raster access strategy
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_CHANNEL_CTILEDCHANNEL_H
#define INCLUDE_CHANNEL_CTILEDCHANNEL_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "channel/cpcidskchannel.h"

namespace PCIDSK
{
    class BlockTileLayer;
/************************************************************************/
/*                            CTiledChannel                             */
/*                                                                      */
/*      Internal tiled data stored in special tiled imagery             */
/*      segments.  Imagery may be compressed.                           */
/************************************************************************/

    class CTiledChannel final: public CPCIDSKChannel
    {

    public:
        CTiledChannel( PCIDSKBuffer &image_header,
                       uint64 ih_offset,
                       PCIDSKBuffer &file_header,
                       int channelnum,
                       CPCIDSKFile *file,
                       eChanType pixel_type );
        ~CTiledChannel() override;

        int GetBlockWidth() const override;
        int GetBlockHeight() const override;
        int GetWidth() const override;
        int GetHeight() const override;
        eChanType GetType() const override;

        virtual int ReadBlock( int block_index, void *buffer,
                               int xoff=-1, int yoff=-1,
                               int xsize=-1, int ysize=-1 ) override;
        int WriteBlock( int block_index, void *buffer ) override;

        void Synchronize() override;



    private:
        int                      image;
        mutable BlockTileLayer * mpoTileLayer;

        void                     EstablishAccess() const;

        void                     ReadTile(void * pData, uint32 nCol, uint32 nRow);

        void                     RLEDecompressBlock( PCIDSKBuffer &oCompressed,
                                                     PCIDSKBuffer &oDecompressed );
        void                     RLECompressBlock( PCIDSKBuffer &oUncompressed,
                                                   PCIDSKBuffer &oCompressed );
        void                     JPEGDecompressBlock( PCIDSKBuffer &oCompressed,
                                                      PCIDSKBuffer &oDecompressed );
        void                     JPEGCompressBlock( PCIDSKBuffer &oDecompressed,
                                                    PCIDSKBuffer &oCompressed );
    };
} // end namespace PCIDSK

#endif // INCLUDE_CHANNEL_CTILEDCHANNEL_H

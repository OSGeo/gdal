/******************************************************************************
 *
 * Purpose:  Declaration of the CTiledChannel raster access strategy
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
        virtual ~CTiledChannel();

        virtual int GetBlockWidth() const override;
        virtual int GetBlockHeight() const override;
        virtual int GetWidth() const override;
        virtual int GetHeight() const override;
        virtual eChanType GetType() const override;

        virtual int ReadBlock( int block_index, void *buffer,
                               int xoff=-1, int yoff=-1,
                               int xsize=-1, int ysize=-1 ) override;
        virtual int WriteBlock( int block_index, void *buffer ) override;

        virtual void Synchronize() override;



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

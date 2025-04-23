/******************************************************************************
 *
 * Purpose:  Declaration of the CPixelInterleavedChannel class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_CHANNEL_CPIXELINTERLEAVEDCHANNEL_H
#define INCLUDE_CHANNEL_CPIXELINTERLEAVEDCHANNEL_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "channel/cpcidskchannel.h"

namespace PCIDSK
{
    class CPCIDSKFile;

/************************************************************************/
/*                       CPixelInterleavedChannel                       */
/************************************************************************/
    class CPixelInterleavedChannel : public CPCIDSKChannel
    {


    public:
        CPixelInterleavedChannel( PCIDSKBuffer &image_header,
            uint64 ih_offset,
            PCIDSKBuffer &file_header,
            int channelnum,
            CPCIDSKFile *file,
            int image_offset,
            eChanType pixel_type );
        virtual ~CPixelInterleavedChannel();

        virtual int ReadBlock( int block_index, void *buffer,
            int xoff=-1, int yoff=-1,
            int xsize=-1, int ysize=-1 ) override;

        virtual int WriteBlock( int block_index, void *buffer ) override;
    private:
        int      image_offset;
    };
} // end namespace PCIDSK

#endif // INCLUDE_CHANNEL_CPIXELINTERLEAVEDCHANNEL_H

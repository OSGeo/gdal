/******************************************************************************
 *
 * Purpose:  Declaration of the CPixelInterleavedChannel class.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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
#ifndef __INCLUDE_CHANNEL_CPIXELINTERLEAVEDCHANNEL_H
#define __INCLUDE_CHANNEL_CPIXELINTERLEAVEDCHANNEL_H

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
            int xsize=-1, int ysize=-1 );

        virtual int WriteBlock( int block_index, void *buffer );
    private:
        int      image_offset;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_CHANNEL_CPIXELINTERLEAVEDCHANNEL_H

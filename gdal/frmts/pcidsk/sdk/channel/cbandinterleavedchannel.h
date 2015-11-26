/******************************************************************************
 *
 * Purpose:  Declaration of the CBandInterleavedChannel class.
 *
 * This class is used to implement band interleaved channels within a 
 * PCIDSK file (which are always packed, and FILE interleaved data from
 * external raw files which may not be packed. 
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
#ifndef INCLUDE_CHANNEL_CBANDINTERLEAVEDCHANNEL_H
#define INCLUDE_CHANNEL_CBANDINTERLEAVEDCHANNEL_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "channel/cpcidskchannel.h"
#include <string>

namespace PCIDSK
{
    class CPCIDSKFile;
    class Mutex;
/************************************************************************/
/*                       CBandInterleavedChannel                        */
/*                                                                      */
/*      Also used for FILE interleaved raw files.                       */
/************************************************************************/

    class CBandInterleavedChannel : public CPCIDSKChannel
    {
    public:
        CBandInterleavedChannel( PCIDSKBuffer &image_header, 
            uint64 ih_offset,
            PCIDSKBuffer &file_header, 
            int channelnum,
            CPCIDSKFile *file,
            uint64 image_offset,
            eChanType pixel_type );
        virtual ~CBandInterleavedChannel();

        virtual int ReadBlock( int block_index, void *buffer,
            int xoff=-1, int yoff=-1,
            int xsize=-1, int ysize=-1 );
        virtual int WriteBlock( int block_index, void *buffer );

        virtual void GetChanInfo( std::string &filename, uint64 &image_offset, 
                                  uint64 &pixel_offset, uint64 &line_offset, 
                                  bool &little_endian ) const;
        virtual void SetChanInfo( std::string filename, uint64 image_offset, 
                                  uint64 pixel_offset, uint64 line_offset, 
                                  bool little_endian );

    private:
    // raw file layout - internal or external
        uint64    start_byte;
        uint64    pixel_offset;
        uint64    line_offset;

        std::string filename;

        void     **io_handle_p;
        Mutex    **io_mutex_p;

        std::string MassageLink( std::string ) const;
    };
} // end namespace PCIDSK

#endif // INCLUDE_CHANNEL_CBANDINTERLEAVEDCHANNEL_H

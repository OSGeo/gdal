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
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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
            int xsize=-1, int ysize=-1 ) override;
        virtual int WriteBlock( int block_index, void *buffer ) override;

        virtual void GetChanInfo( std::string &filename, uint64 &image_offset,
                                  uint64 &pixel_offset, uint64 &line_offset,
                                  bool &little_endian ) const override;
        virtual void SetChanInfo( std::string filename, uint64 image_offset,
                                  uint64 pixel_offset, uint64 line_offset,
                                  bool little_endian ) override;

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

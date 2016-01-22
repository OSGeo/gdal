/******************************************************************************
 *
 * Purpose:  Declaration of the CTiledChannel raster access strategy
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
#ifndef __INCLUDE_CHANNEL_CTILEDCHANNEL_H
#define __INCLUDE_CHANNEL_CTILEDCHANNEL_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "channel/cpcidskchannel.h"

#include <vector>
#include <string>

namespace PCIDSK
{
    class SysVirtualFile;
/************************************************************************/
/*                            CTiledChannel                             */
/*                                                                      */
/*      Internal tiled data stored in special tiled imagery             */
/*      segments.  Imagery may be compressed.                           */
/************************************************************************/

    class CTiledChannel : public CPCIDSKChannel
    {

    public:
        CTiledChannel( PCIDSKBuffer &image_header, 
                       uint64 ih_offset,
                       PCIDSKBuffer &file_header, 
                       int channelnum,
                       CPCIDSKFile *file,
                       eChanType pixel_type );
        virtual ~CTiledChannel();

        virtual int GetBlockWidth() const;
        virtual int GetBlockHeight() const;
        virtual int GetWidth() const;
        virtual int GetHeight() const;
        virtual eChanType GetType() const;

        virtual int ReadBlock( int block_index, void *buffer,
                               int xoff=-1, int yoff=-1,
                               int xsize=-1, int ysize=-1 );
        virtual int WriteBlock( int block_index, void *buffer );

        virtual void Synchronize();
        
        
        
    private:
        int                      image;
        mutable int              tile_count;
        mutable int              tiles_per_row;
        mutable int              tiles_per_col;
        mutable SysVirtualFile  *vfile;
        mutable std::string      compression;
        
        void                     EstablishAccess() const;
        void                     RLEDecompressBlock( PCIDSKBuffer &oCompressed,
                                                     PCIDSKBuffer &oDecompressed );
        void                     RLECompressBlock( PCIDSKBuffer &oUncompressed,
                                                   PCIDSKBuffer &oCompressed );
        void                     JPEGDecompressBlock( PCIDSKBuffer &oCompressed,
                                                      PCIDSKBuffer &oDecompressed );
        void                     JPEGCompressBlock( PCIDSKBuffer &oDecompressed,
                                                    PCIDSKBuffer &oCompressed );

        bool                     IsTileEmpty(void* buffer) const;

        // managed paged list of tile offsets and sizes.
        void                     GetTileInfo( int tile_index,
                                              uint64 &offset, int &size );
        void                     SetTileInfo( int tile_index,
                                              uint64 offset, int size );
        void                     LoadTileInfoBlock( int tile_info_block );
        void                     SaveTileInfoBlock( int tile_info_block );

        static const int                           tile_block_size = 4096;
        mutable std::vector< std::vector<uint64> > tile_offsets;
        mutable std::vector< std::vector<int> >    tile_sizes;
        mutable std::vector<bool>                  tile_info_dirty;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_CHANNEL_CTILEDCHANNEL_H

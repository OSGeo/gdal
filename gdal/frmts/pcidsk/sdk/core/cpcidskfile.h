/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKFile class.
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
#ifndef INCLUDE_PRIV_CPCIDSKFILE_H
#define INCLUDE_PRIV_CPCIDSKFILE_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "pcidsk_file.h"
#include "pcidsk_mutex.h"
#include "pcidsk_interfaces.h"
#include "core/metadataset.h"
#include "core/protectedfile.h"

#include <string>
#include <vector>

namespace PCIDSK
{
    class PCIDSKChannel;
    class PCIDSKSegment;
    class PCIDSKInterfaces;
/************************************************************************/
/*                             CPCIDSKFile                              */
/************************************************************************/
    class CPCIDSKFile : public PCIDSKFile
    {
        friend PCIDSKFile PCIDSK_DLL *Open( std::string filename, 
            std::string access, const PCIDSKInterfaces *interfaces );
    public:

        CPCIDSKFile( std::string filename );
        virtual ~CPCIDSKFile();

        virtual PCIDSKInterfaces *GetInterfaces() { return &interfaces; }

        PCIDSKChannel  *GetChannel( int band );
        PCIDSKSegment  *GetSegment( int segment );
        std::vector<PCIDSKSegment *> GetSegments();

        PCIDSKSegment  *GetSegment( int type, std::string name,
            int previous = 0 );
        int  CreateSegment( std::string name, std::string description,
            eSegType seg_type, int data_blocks );
        void DeleteSegment( int segment );
        void CreateOverviews( int chan_count, int *chan_list, 
            int factor, std::string resampling );

        int       GetWidth() const { return width; }
        int       GetHeight() const { return height; }
        int       GetChannels() const { return channel_count; }
        std::string GetInterleaving() const { return interleaving; }
        bool      GetUpdatable() const { return updatable; } 
        uint64    GetFileSize() const { return file_size; }

        // the following are only for pixel interleaved IO
        int       GetPixelGroupSize() const { return pixel_group_size; }
        void     *ReadAndLockBlock( int block_index, int xoff=-1, int xsize=-1 );
        void      UnlockBlock( bool mark_dirty = false );
        void      WriteBlock( int block_index, void *buffer );
        void      FlushBlock();

        void      WriteToFile( const void *buffer, uint64 offset, uint64 size );
        void      ReadFromFile( void *buffer, uint64 offset, uint64 size );

        std::string GetFilename() const { return base_filename; }

        void      GetIODetails( void ***io_handle_pp, Mutex ***io_mutex_pp,
                                std::string filename="", bool writable=false );

        bool      GetEDBFileDetails( EDBFile** file_p, Mutex **io_mutex_p,
                                     std::string filename );

        std::string GetMetadataValue( const std::string& key ) 
            { return metadata.GetMetadataValue(key); }
        void        SetMetadataValue( const std::string& key, const std::string& value ) 
            { metadata.SetMetadataValue(key,value); }
        std::vector<std::string> GetMetadataKeys() 
            { return metadata.GetMetadataKeys(); }

        void      Synchronize();

    // not exposed to applications.
        void      ExtendFile( uint64 blocks_requested, bool prezero = false );
        void      ExtendSegment( int segment, uint64 blocks_to_add,
            bool prezero = false );
        void      MoveSegmentToEOF( int segment );

    private:
        PCIDSKInterfaces interfaces;

        void         InitializeFromHeader();

        std::string  base_filename;
        
        int          width;
        int          height;
        int          channel_count;
        std::string  interleaving;

        std::vector<PCIDSKChannel*> channels;

        int          segment_count;
        uint64       segment_pointers_offset;
        PCIDSKBuffer segment_pointers;

        std::vector<PCIDSKSegment*> segments;

    // pixel interleaved info.
        uint64       block_size; // pixel interleaved scanline size.
        int          pixel_group_size; // pixel interleaved pixel_offset value.
        uint64       first_line_offset;

        int          last_block_index;
        bool         last_block_dirty;
        int          last_block_xoff;
        int          last_block_xsize;
        void        *last_block_data;
        Mutex       *last_block_mutex;

        void        *io_handle;
        Mutex       *io_mutex;
        bool         updatable;

        uint64       file_size; // in blocks.

    // register of open external raw files.
        std::vector<ProtectedFile>  file_list;

    // register of open external databasefiles
        std::vector<ProtectedEDBFile> edb_file_list;

        MetadataSet  metadata;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PRIV_CPCIDSKFILE_H

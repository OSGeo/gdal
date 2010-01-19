/******************************************************************************
 *
 * Purpose:  Implementation of the SysVirtualFile class.
 *
 * This class is used to manage access to a single virtual file stored in
 * SysBData segments based on a block map stored in the SysBMDir segment 
 * (and managed by SysBlockMap class). 
 *
 * The virtual files are allocated in 8K chunks (block_size) in segments.
 * To minimize IO requests, other overhead, we keep one such 8K block in
 * our working cache for the virtual file stream.  
 *
 * This class is primarily used by the CTiledChannel class for access to 
 * tiled images.
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
#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "pcidsk_exception.h"
#include "core/sysvirtualfile.h"
#include "core/cpcidskfile.h"
#include "segment/sysblockmap.h"
#include <cassert>
#include <cstring>

using namespace PCIDSK;


const int SysVirtualFile::block_size = SYSVIRTUALFILE_BLOCKSIZE;

/************************************************************************/
/*                           SysVirtualFile()                           */
/************************************************************************/

SysVirtualFile::SysVirtualFile( CPCIDSKFile *file, int start_block, 
                                uint64 image_length, 
                                PCIDSKBuffer &block_map_data,
                                SysBlockMap *sysblockmap,
                                int image_index )

{
    file_length = image_length;
    this->file = file;
    this->sysblockmap = sysblockmap;
    this->image_index = image_index;

    loaded_block = -1;
    loaded_block_dirty = false;
    
    last_bm_index = -1;

    int next_block = start_block;

    // perhaps we should defer all this work till the first request is made?
    while( next_block != -1 )
    {
        int offset = 512 + next_block * 28;

        block_segment.push_back( block_map_data.GetInt( offset+0, 4 ) );
        block_index.push_back( block_map_data.GetInt( offset+4, 8 ) );

        last_bm_index = next_block;
        next_block = block_map_data.GetInt( offset + 20, 8 );
    }

    assert( block_index.size() * block_size >= file_length );
}

/************************************************************************/
/*                          ~SysVirtualFile()                           */
/************************************************************************/

SysVirtualFile::~SysVirtualFile()

{
    Synchronize();
}

/************************************************************************/
/*                            Synchronize()                             */
/************************************************************************/

void SysVirtualFile::Synchronize()

{
    if( loaded_block_dirty )
    {
        PCIDSKSegment *data_seg_obj = 
            file->GetSegment( block_segment[loaded_block] );

        data_seg_obj->WriteToFile( block_data, 
                                   block_size * (uint64) block_index[loaded_block],
                                   block_size );
        loaded_block_dirty = false;
    }
}

/************************************************************************/
/*                            WriteToFile()                             */
/************************************************************************/

void 
SysVirtualFile::WriteToFile( const void *buffer, uint64 offset, uint64 size )

{
    uint64 buffer_offset = 0;

    while( buffer_offset < size )
    {
        int request_block = (int) ((offset + buffer_offset) / block_size);
        int offset_in_block = (int) ((offset + buffer_offset) % block_size);
        int amount_to_copy;

        LoadBlock( request_block );

        amount_to_copy = block_size - offset_in_block;
        if( amount_to_copy > (int) (size - buffer_offset) )
            amount_to_copy = (int) (size - buffer_offset);

        memcpy( block_data + offset_in_block, 
                ((uint8 *) buffer) + buffer_offset, 
                amount_to_copy );

        loaded_block_dirty = true;

        buffer_offset += amount_to_copy;
    }

    if( offset+size > file_length )
    {
        file_length = offset+size;
        sysblockmap->SetVirtualFileSize( image_index, file_length );
    }
}

/************************************************************************/
/*                            ReadFromFile()                            */
/************************************************************************/

void SysVirtualFile::ReadFromFile( void *buffer, uint64 offset, uint64 size )

{
    uint64 buffer_offset = 0;

    while( buffer_offset < size )
    {
        int request_block = (int) ((offset + buffer_offset) / block_size);
        int offset_in_block = (int) ((offset + buffer_offset) % block_size);
        int amount_to_copy;

        LoadBlock( request_block );

        amount_to_copy = block_size - offset_in_block;
        if( amount_to_copy > (int) (size - buffer_offset) )
            amount_to_copy = (int) (size - buffer_offset);

        memcpy( ((uint8 *) buffer) + buffer_offset, 
                block_data + offset_in_block, amount_to_copy );

        buffer_offset += amount_to_copy;
    }
}

/************************************************************************/
/*                             LoadBlock()                              */
/************************************************************************/

void SysVirtualFile::LoadBlock( int requested_block )

{
/* -------------------------------------------------------------------- */
/*      Do we already have this block?                                  */
/* -------------------------------------------------------------------- */
    if( requested_block == loaded_block )
        return;

/* -------------------------------------------------------------------- */
/*      Do we need to grow the virtual file by one block?               */
/* -------------------------------------------------------------------- */
    if( requested_block == (int) block_index.size() )
    {
        int new_seg;

        block_index.push_back( 
            sysblockmap->GrowVirtualFile( image_index, 
                                          last_bm_index, new_seg ) );
        block_segment.push_back( new_seg );
    }

/* -------------------------------------------------------------------- */
/*      Does this block exist in the virtual file?                      */
/* -------------------------------------------------------------------- */
    if( requested_block < 0 || requested_block >= (int) block_index.size() )
        ThrowPCIDSKException( "SysVirtualFile::LoadBlock(%d) - block out of range.",
                              requested_block );

/* -------------------------------------------------------------------- */
/*      Do we have a dirty block loaded that needs to be saved?         */
/* -------------------------------------------------------------------- */
    if( loaded_block_dirty )
    {
        PCIDSKSegment *data_seg_obj = 
            file->GetSegment( block_segment[loaded_block] );
        
        data_seg_obj->WriteToFile( block_data, 
                                   block_size * (uint64) block_index[loaded_block],
                                   block_size );
        loaded_block_dirty = false;
    }

/* -------------------------------------------------------------------- */
/*      Load the requested block.                                       */
/* -------------------------------------------------------------------- */
    PCIDSKSegment *data_seg_obj = 
        file->GetSegment( block_segment[requested_block] );

    data_seg_obj->ReadFromFile( block_data, 
                                block_size * (uint64) block_index[requested_block],
                                block_size );

    loaded_block = requested_block;
    loaded_block_dirty = false;
}


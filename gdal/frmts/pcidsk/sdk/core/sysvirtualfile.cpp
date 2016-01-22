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
#include "pcidsk_utils.h"
#include "core/sysvirtualfile.h"
#include "core/cpcidskfile.h"
#include "core/mutexholder.h"
#include "segment/sysblockmap.h"
#include <cassert>
#include <cstring>
#if 0
#include <cstdio>
#endif

using namespace PCIDSK;


const int SysVirtualFile::block_size = SYSVIRTUALFILE_BLOCKSIZE;

/************************************************************************/
/*                           SysVirtualFile()                           */
/************************************************************************/

SysVirtualFile::SysVirtualFile( CPCIDSKFile *file, int start_block, 
                                uint64 image_length, 
                                SysBlockMap *sysblockmap,
                                int image_index )

{
    io_handle = NULL;
    io_mutex = NULL;

    file_length = image_length;
    this->file = file;
    this->sysblockmap = sysblockmap;
    this->image_index = image_index;

    loaded_block = -1;
    loaded_block_dirty = false;
    
    last_bm_index = -1;

    next_bm_entry_to_load = start_block;

    regular_blocks = false;
    blocks_loaded = 0;
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
    FlushDirtyBlock();
}

/************************************************************************/
/*                          GetBlockSegment()                           */
/************************************************************************/

uint16 SysVirtualFile::GetBlockSegment( int requested_block )

{
    if( requested_block < 0 )
        ThrowPCIDSKException( "SysVirtualFile::GetBlockSegment(%d) - illegal request.",
                              requested_block );

    if( requested_block >= blocks_loaded )
        LoadBMEntrysTo( requested_block );

    if( regular_blocks )
        // regular blocks are all in one segment.
        return xblock_segment[0];
    else
        return xblock_segment[requested_block];
}

/************************************************************************/
/*                       GetBlockIndexInSegment()                       */
/************************************************************************/

int SysVirtualFile::GetBlockIndexInSegment( int requested_block )

{
    if( requested_block < 0 )
        ThrowPCIDSKException( "SysVirtualFile::GetBlockIndexInSegment(%d) - illegal request.",
                              requested_block );

    if( requested_block >= blocks_loaded )
        LoadBMEntrysTo( requested_block );

    if( regular_blocks )
        // regular blocks all follow the first block in order.
        return xblock_index[0] + requested_block;
    else
        return xblock_index[requested_block];
}


/************************************************************************/
/*                            SetBlockInfo()                            */
/************************************************************************/

void SysVirtualFile::SetBlockInfo( int requested_block,
                                   uint16 new_block_segment, 
                                   int new_block_index )

{
    if( requested_block < 0 )
        ThrowPCIDSKException( "SysVirtualFile::SetBlockSegment(%d) - illegal request.",
                              requested_block );

    // this should always be the case.
    assert( requested_block == blocks_loaded );

    // Initialization case.
    if( requested_block == 0 && blocks_loaded == 0 )
    {
        xblock_segment.push_back( new_block_segment );
        xblock_index.push_back( new_block_index );
        blocks_loaded = 1;
        return;
    }

    if( !regular_blocks )
    {
        xblock_segment.push_back( new_block_segment );
        xblock_index.push_back( new_block_index );
        blocks_loaded++;
        return;
    }

    // Are things still regular?
    if( new_block_segment == xblock_segment[0]
        && new_block_index == xblock_index[0] + requested_block )
    {
        blocks_loaded++;
        return;
    }

    // Ah, we see they are now irregular.  We need to build up the
    // segment/index arrays and proceed to populate them. 
    Debug( file->GetInterfaces()->Debug, 
           "SysVirtualFile - Discovered stream is irregulr.  %d/%d follows %d/%d at block %d.\n",
           new_block_segment, new_block_index,
           xblock_segment[0], xblock_index[0], 
           requested_block );
           
    regular_blocks = false;
    while( (int) xblock_segment.size() < blocks_loaded )
    {
        xblock_segment.push_back( xblock_segment[0] );
        xblock_index.push_back( xblock_index[xblock_index.size()-1]+1 );
    }

    xblock_segment.push_back( new_block_segment );
    xblock_index.push_back( new_block_index );
    blocks_loaded++;
}

/************************************************************************/
/*                            WriteToFile()                             */
/************************************************************************/

void 
SysVirtualFile::WriteToFile( const void *buffer, uint64 offset, uint64 size )

{
    uint64 buffer_offset = 0;

    if(io_handle == NULL || io_mutex == NULL)
        file->GetIODetails( &io_handle, &io_mutex );

    MutexHolder oMutex(*io_mutex);

    while( buffer_offset < size )
    {
        int request_block = (int) ((offset + buffer_offset) / block_size);
        int offset_in_block = (int) ((offset + buffer_offset) % block_size);
        int amount_to_copy = block_size - offset_in_block;

        if (offset_in_block != 0 || (size - buffer_offset) < (uint64)block_size) {
            // we need to read in the block for update
            LoadBlock( request_block );
            if( amount_to_copy > (int) (size - buffer_offset) )
                amount_to_copy = (int) (size - buffer_offset);

            // fill in the block
            memcpy( block_data + offset_in_block,
                    ((uint8 *) buffer) + buffer_offset,
                    amount_to_copy );

            loaded_block_dirty = true;
        } else {
            int num_full_blocks = (int) ((size - buffer_offset) / block_size);
            
            WriteBlocks(request_block, num_full_blocks, (uint8*)buffer + buffer_offset);
            
            amount_to_copy = num_full_blocks * block_size;
        }

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
    if(io_handle == NULL || io_mutex == NULL)
        file->GetIODetails( &io_handle, &io_mutex );

    MutexHolder oMutex(*io_mutex);

    uint64 buffer_offset = 0;
#if 0
    printf("Requesting region at %llu of size %llu\n", offset, size);
#endif
    while( buffer_offset < size )
    {
        int request_block = (int) ((offset + buffer_offset) / block_size);
        int offset_in_block = (int) ((offset + buffer_offset) % block_size);
        int amount_to_copy = block_size - offset_in_block;
        


        if (offset_in_block != 0 || (size - buffer_offset) < (uint64)block_size) {
            // Deal with the case where we need to load a partial block. Hopefully
            // this doesn't happen often
            LoadBlock( request_block );
            if( amount_to_copy > (int) (size - buffer_offset) )
                amount_to_copy = (int) (size - buffer_offset);
            memcpy( ((uint8 *) buffer) + buffer_offset,
                    block_data + offset_in_block, amount_to_copy );
        } else {
            // Use the bulk loading of blocks. First, compute the range
            // of full blocks we need to load
            int num_full_blocks = (int) ((size - buffer_offset)/block_size);
            
            LoadBlocks(request_block, num_full_blocks, ((uint8*)buffer) + buffer_offset);
            amount_to_copy = num_full_blocks * block_size;
        }


        buffer_offset += amount_to_copy;
    }
}

/************************************************************************/
/*                             LoadBlock()                              */
/************************************************************************/
/**
 * Loads the requested_block block from the system virtual file. Extends
 * the file if necessary
 */
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
    GrowVirtualFile(requested_block);

/* -------------------------------------------------------------------- */
/*      Does this block exist in the virtual file?                      */
/* -------------------------------------------------------------------- */
    if( requested_block < 0 || requested_block >= blocks_loaded )
        ThrowPCIDSKException( "SysVirtualFile::LoadBlock(%d) - block out of range.",
                              requested_block );

/* -------------------------------------------------------------------- */
/*      Do we have a dirty block loaded that needs to be saved?         */
/* -------------------------------------------------------------------- */
    FlushDirtyBlock();

/* -------------------------------------------------------------------- */
/*      Load the requested block.                                       */
/* -------------------------------------------------------------------- */
    LoadBMEntrysTo( requested_block );
    PCIDSKSegment *data_seg_obj =
        file->GetSegment( GetBlockSegment( requested_block ) );

    data_seg_obj->ReadFromFile( block_data,
                                block_size * (uint64) GetBlockIndexInSegment( requested_block ),
                                block_size );

    loaded_block = requested_block;
    loaded_block_dirty = false;
}

/************************************************************************/
/*                         FlushDirtyBlock()                            */
/************************************************************************/
/**
 * If the block currently loaded is dirty, flush it to the file
 */
void SysVirtualFile::FlushDirtyBlock(void)
{
    if (loaded_block_dirty) {
        if(io_handle == NULL || io_mutex == NULL)
            file->GetIODetails( &io_handle, &io_mutex );

        MutexHolder oMutex(*io_mutex);

        PCIDSKSegment *data_seg_obj =
            file->GetSegment( GetBlockSegment( loaded_block ) );
        
        data_seg_obj->WriteToFile( block_data,
                                   block_size * (uint64) GetBlockIndexInSegment( loaded_block ),
                                   block_size );
        loaded_block_dirty = false;
    }
}

/************************************************************************/
/*                          GrowVirtualFile()                           */
/************************************************************************/
void SysVirtualFile::GrowVirtualFile(std::ptrdiff_t requested_block)
{
    LoadBMEntrysTo( requested_block );

    if( requested_block == blocks_loaded )
    {
        if(io_handle == NULL || io_mutex == NULL)
            file->GetIODetails( &io_handle, &io_mutex );

        MutexHolder oMutex(*io_mutex);

        int new_seg;
        int offset;

        offset = 
            sysblockmap->GrowVirtualFile( image_index, last_bm_index, new_seg);
        SetBlockInfo( requested_block, (uint16) new_seg, offset );
    }
}

/************************************************************************/
/*                            WriteBlocks()                             */
/************************************************************************/
/**
 * \brief Writes a group of blocks
 * Attempts to create a group of blocks (if the SysVirtualFile
 * is not already large enough to hold them) and then write them
 * out contiguously
 */
void SysVirtualFile::WriteBlocks(int first_block,
                                 int block_count,
                                 void* const buffer)
{
    if(io_handle == NULL || io_mutex == NULL)
        file->GetIODetails( &io_handle, &io_mutex );

    MutexHolder oMutex(*io_mutex);

    FlushDirtyBlock();
    // Iterate through all the blocks to be written, first, then
    // grow the virtual file
    for (unsigned int i = 0; i <= (unsigned int) block_count; i++) {
        GrowVirtualFile(first_block + i);
    }
    
    // Using a similar algorithm to the LoadBlocks() function,
    // attempt to coalesce writes
    std::size_t buffer_off = 0;
    std::size_t blocks_written = 0;
    std::size_t current_first_block = first_block;
    while (blocks_written < (std::size_t) block_count) {
        LoadBMEntrysTo( current_first_block+1 );

        unsigned int cur_segment = GetBlockSegment( current_first_block );
        unsigned int cur_block = current_first_block;
        while (cur_block < (unsigned int)block_count + first_block &&
               (unsigned int) GetBlockSegment(cur_block + 1) == cur_segment)
        {
            cur_block++;
            LoadBMEntrysTo( current_first_block+1 );
        }
        
        // Find largest span of contiguous blocks we can write
        uint64 write_start = GetBlockIndexInSegment(current_first_block);
        uint64 write_cur = write_start * block_size;
        unsigned int count_to_write = 1;
        while (write_cur + block_size ==
               (uint64)GetBlockIndexInSegment(count_to_write + current_first_block - 1) * block_size &&
            count_to_write < (cur_block - current_first_block))
        {
            write_cur += block_size;
            count_to_write++;
        }
        
        PCIDSKSegment *data_seg_obj =
            file->GetSegment( cur_segment );
        
        std::size_t bytes_to_write = count_to_write * block_size;
        
        data_seg_obj->WriteToFile((uint8*)buffer + buffer_off,
                                  block_size * write_start,
                                  bytes_to_write);
    
        buffer_off += bytes_to_write;
        blocks_written += count_to_write;
        current_first_block += count_to_write;
    }
}

/************************************************************************/
/*                             LoadBlocks()                             */
/************************************************************************/
/**
 * \brief Load a group of blocks
 * Attempts to coalesce reading of groups of blocks into a single
 * filesystem I/O operation. Does not cache the loaded block, nor
 * does it modify the state of the SysVirtualFile, other than to
 * flush the loaded block if it is dirty.
 */
void SysVirtualFile::LoadBlocks(int requested_block_start,
                                int requested_block_count,
                                void* const buffer)
{
    if(io_handle == NULL || io_mutex == NULL)
        file->GetIODetails( &io_handle, &io_mutex );

    MutexHolder oMutex(*io_mutex);

    FlushDirtyBlock();

    unsigned int blocks_read = 0;
    unsigned int current_start = requested_block_start;
    
    std::size_t buffer_off = 0;
    
    while (blocks_read < (unsigned int)requested_block_count) {
        // Coalesce blocks that are in the same segment
        LoadBMEntrysTo( current_start+1 );
        unsigned int cur_segment = GetBlockSegment(current_start); // segment of current
                // first block
        unsigned int cur_block = current_start; // starting block ID
        while (cur_block < (unsigned int)requested_block_count + requested_block_start &&
               GetBlockSegment(cur_block + 1) == cur_segment) {
            // this block is in the same segment as the previous one we
            // wanted to read.
            cur_block++;
            LoadBMEntrysTo( cur_block+1 );
        }
        
        // now attempt to determine if the region of blocks (from current_start
        // to cur_block are contiguous
        uint64 read_start = GetBlockIndexInSegment(current_start);
        uint64 read_cur = read_start * block_size;
        unsigned int count_to_read = 1; // we'll read at least one block
        while (read_cur + block_size ==
               (uint64)GetBlockIndexInSegment(count_to_read + current_start) * block_size && // compare count of blocks * offset with stored offset
            count_to_read < (cur_block - current_start) ) // make sure we don't try to read more blocks than we determined fell in
                                                            // this segment
        {
            read_cur += block_size;
            count_to_read++;
        }

#if 0
        // Check if we need to grow the virtual file for each of these blocks
        for (unsigned int i = 0 ; i < count_to_read; i++) {
            GrowVirtualFile(i + current_start);
        }
        
        printf("Coalescing the read of %d blocks\n", count_to_read);
#endif

        // Perform the actual read
        PCIDSKSegment *data_seg_obj =
            file->GetSegment( cur_segment );
        
        std::size_t data_size = block_size * count_to_read;

#if 0
        printf("Reading %d bytes at offset %d in buffer\n", data_size, buffer_off);
#endif

        data_seg_obj->ReadFromFile( ((uint8*)buffer) + buffer_off,
                                    block_size * read_start,
                                    data_size );
                                    
        buffer_off += data_size; // increase buffer offset
        
        // Increment the current start by the number of blocks we jsut read
        current_start += count_to_read;
        blocks_read += count_to_read;
    }
}

/************************************************************************/
/*                           LoadBMEntryTo()                            */
/*                                                                      */
/*      We load the blockmap "as needed".  This method fills in         */
/*      blockmap entries up to the target block, if not already         */
/*      loaded.  Passing in a target block_index of -1 loads the        */
/*      whole blockmap.  It is harmless to request more blocks than     */
/*      are available.                                                  */
/************************************************************************/

void SysVirtualFile::LoadBMEntrysTo( int target_index )

{
    if( target_index > 0 )
    {
        target_index += 200 - (target_index%200);
    }
    
    while( (target_index == -1 || blocks_loaded <= target_index ) 
           && next_bm_entry_to_load != -1 )
    {
        uint16 segment;
        int    block;
       
        last_bm_index = next_bm_entry_to_load;
        next_bm_entry_to_load = 
            sysblockmap->GetNextBlockMapEntry( 
                next_bm_entry_to_load, segment, block );

        SetBlockInfo( blocks_loaded, segment, block );
    }
}


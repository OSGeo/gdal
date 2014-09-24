/******************************************************************************
 *
 * Purpose:  Implementation of the SysBlockMap class.
 *
 * This class is used to manage access to the SYS virtual block map segment
 * (named SysBMDir).  This segment is used to keep track of one or more 
 * virtual files stored in SysBData segments.  These virtual files are normally
 * used to hold tiled images for primary bands or overviews.  
 *
 * This class is closely partnered with the SysVirtualFile class, and the
 * primary client is the CTiledChannel class. 
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

#include "pcidsk_exception.h"
#include "pcidsk_file.h"
#include "core/sysvirtualfile.h"
#include "segment/sysblockmap.h"
#include "core/cpcidskfile.h"

#include <cassert>
#include <vector>
#include <cstring>
#include <cstdlib>

using namespace PCIDSK;

/************************************************************************/
/*                            SysBlockMap()                             */
/************************************************************************/

SysBlockMap::SysBlockMap( PCIDSKFile *file, int segment,
                              const char *segment_pointer )
        : CPCIDSKSegment( file, segment, segment_pointer )

{
    partial_loaded = false;
    full_loaded = false;
    dirty = false;
    growing_segment = 0;
}

/************************************************************************/
/*                            ~SysBlockMap()                            */
/************************************************************************/

SysBlockMap::~SysBlockMap()

{
    size_t i;
    
    for( i = 0; i < virtual_files.size(); i++ )
    {
        delete virtual_files[i];
        virtual_files[i] = NULL;
    }

    Synchronize();
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      This method is used after creation of the SysBMDir segment      */
/*      to fill in valid contents.  Prepares bare minimum contents.     */
/************************************************************************/

void SysBlockMap::Initialize()

{
    PCIDSKBuffer init_data(512);

    init_data.Put( "VERSION  1", 0, 10 );
    init_data.Put( 0, 10, 8 );
    init_data.Put( 0, 18, 8 );
    init_data.Put( -1, 26, 8 );
    init_data.Put( "", 34, 512-34 );

    WriteToFile( init_data.buffer, 0, init_data.buffer_size );
#ifdef notdef
    // arbitrarily grow the segment a bit to avoid having to move it too soon.
    WriteToFile( "\0", 8191, 1 );				       
#endif
}

/************************************************************************/
/*                            PartialLoad()                             */
/*                                                                      */
/*      Load the header and some per-layer information.                 */
/************************************************************************/

void SysBlockMap::PartialLoad()

{
    if( partial_loaded )
        return;

//    printf( "<PartialLoad>" );
//    fflush( stdout );

/* -------------------------------------------------------------------- */
/*      Load the 512 byte count section of the blockmap.                */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer count_data;

    count_data.SetSize( 512 );
    ReadFromFile( count_data.buffer, 0, 512 );

    if( strncmp(count_data.buffer,"VERSION",7) != 0 )
        ThrowPCIDSKException( "SysBlockMap::PartialLoad() - block map corrupt." );

    if( count_data.GetInt( 7, 3 ) != 1 )
        ThrowPCIDSKException( "SysBlockMap::PartialLoad() - unsupported version." );

/* -------------------------------------------------------------------- */
/*      Establish our SysVirtualFile array based on the number of       */
/*      images listed in the image list.                                */
/* -------------------------------------------------------------------- */
    int layer_count = count_data.GetInt( 10, 8 );

    virtual_files.resize( layer_count );

    block_count = count_data.GetInt( 18, 8 );
    first_free_block = count_data.GetInt( 26, 8 );

/* -------------------------------------------------------------------- */
/*      Load the layer list definitions.  These are fairly small.       */
/* -------------------------------------------------------------------- */
    layer_data.SetSize( 24 * layer_count );
    ReadFromFile( layer_data.buffer, 
                  512 + 28 * block_count, 
                  layer_data.buffer_size);

    partial_loaded = true;

//    FullLoad();
}

/************************************************************************/
/*                              FullLoad()                              */
/*                                                                      */
/*      Load the blockmap data (can be large) into blockmap_data.       */
/************************************************************************/

void SysBlockMap::FullLoad()

{
    PartialLoad();

    if( full_loaded )
        return;

//    printf( "<FullLoad>" );
//    fflush( stdout );

    // TODO: this should likely be protected by a mutex. 

/* -------------------------------------------------------------------- */
/*      Load the segment contents into a buffer.                        */
/* -------------------------------------------------------------------- */
    blockmap_data.SetSize( block_count * 28 );
    ReadFromFile( blockmap_data.buffer, 512, blockmap_data.buffer_size );

    full_loaded = true;
}

/************************************************************************/
/*                            Synchronize()                             */
/************************************************************************/

void SysBlockMap::Synchronize()

{
    if( !full_loaded || !dirty )
        return;

    PCIDSKBuffer init_data(512);

    init_data.Put( "VERSION  1", 0, 10 );
    init_data.Put( (int) virtual_files.size(), 10, 8 );
    init_data.Put( block_count, 18, 8 );
    init_data.Put( first_free_block, 26, 8 );
    init_data.Put( "", 34, 512-34 );

    WriteToFile( init_data.buffer, 0, init_data.buffer_size );

    WriteToFile( blockmap_data.buffer, 512, blockmap_data.buffer_size );
    WriteToFile( layer_data.buffer, 512 + blockmap_data.buffer_size, 
                 layer_data.buffer_size );

    dirty = false;
}

/************************************************************************/
/*                           AllocateBlocks()                           */
/*                                                                      */
/*      Allocate a bunch of new blocks and attach to the free list.     */
/************************************************************************/

void SysBlockMap::AllocateBlocks()

{
    FullLoad();

/* -------------------------------------------------------------------- */
/*      Find a segment we can extend.  We consider any SYS segments     */
/*      with a name of SysBData.                                        */
/* -------------------------------------------------------------------- */
    PCIDSKSegment *seg;

    if( growing_segment > 0 )
    {
        seg = file->GetSegment( growing_segment );
        if( !seg->IsAtEOF() )
            growing_segment = 0;
    }

    if( growing_segment == 0 )
    {
        PCIDSKSegment *seg;
        int  previous = 0;

        while( (seg=file->GetSegment( SEG_SYS, "SysBData", previous )) != NULL )
        {
            previous = seg->GetSegmentNumber();
            
            if( seg->IsAtEOF() )
            {
                growing_segment = previous;
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we didn't find one, then create a new segment.               */
/* -------------------------------------------------------------------- */
    if( growing_segment == 0 )
    {
        growing_segment = 
            file->CreateSegment( "SysBData", 
                                 "System Block Data for Tiles and Overviews "
                                 "- Do not modify",
                                 SEG_SYS, 0L );
    }

/* -------------------------------------------------------------------- */
/*      Allocate another set of space.                                  */
/* -------------------------------------------------------------------- */
    uint64 new_big_blocks = 16;
    uint64 new_bytes = new_big_blocks * SysVirtualFile::block_size;
    seg = file->GetSegment( growing_segment );
    int block_index_in_segment = (int) 
        (seg->GetContentSize() / SysVirtualFile::block_size);

    seg->WriteToFile( "\0", seg->GetContentSize() + new_bytes - 1, 1 );
    
/* -------------------------------------------------------------------- */
/*      Resize the memory image of the blockmap.                        */
/* -------------------------------------------------------------------- */
    if( 28 * (block_count + new_big_blocks) 
        > (unsigned int) blockmap_data.buffer_size )
        blockmap_data.SetSize( (int) (28 * (block_count + new_big_blocks)) );

/* -------------------------------------------------------------------- */
/*      Fill in info on the new blocks.                                 */
/* -------------------------------------------------------------------- */
    uint64 block_index;

    for( block_index = block_count; 
         block_index < block_count + new_big_blocks;
         block_index++ )
    {
        int bi_offset = (int) (block_index * 28);

        blockmap_data.Put( growing_segment, bi_offset, 4 );
        blockmap_data.Put( block_index_in_segment++, bi_offset+4, 8 );
        blockmap_data.Put( -1, bi_offset+12, 8 );

        if( block_index == block_count + new_big_blocks - 1 )
            blockmap_data.Put( -1, bi_offset+20, 8 );
        else
            blockmap_data.Put( block_index+1, bi_offset+20, 8 );
    }

    first_free_block = block_count;

    block_count += (int) new_big_blocks;

    dirty = true;
}

/************************************************************************/
/*                          GrowVirtualFile()                           */
/*                                                                      */
/*      Get one more block for this virtual file.                       */
/************************************************************************/

int SysBlockMap::GrowVirtualFile( int image, int &last_block, 
                                  int &block_segment_ret )

{
    FullLoad();

/* -------------------------------------------------------------------- */
/*      Do we need to create new free blocks?                           */
/* -------------------------------------------------------------------- */
    if( first_free_block == -1 )
        AllocateBlocks();

/* -------------------------------------------------------------------- */
/*      Return the first free block, and update the next pointer of     */
/*      the previous block.                                             */
/* -------------------------------------------------------------------- */
    int alloc_block = first_free_block;

    // update first free block to point to the next free block.
    first_free_block = blockmap_data.GetInt( alloc_block*28+20, 8);

    // mark block as owned by this layer/image. 
    blockmap_data.Put( image, alloc_block*28 + 12, 8 );

    // clear next free block on allocated block - it is the last in the chain
    blockmap_data.Put( -1, alloc_block*28 + 20, 8 );

    // point the previous "last block" for this virtual file to this new block
    if( last_block != -1 )
        blockmap_data.Put( alloc_block, last_block*28 + 20, 8 );
    else
        layer_data.Put( alloc_block, image*24 + 4, 8 );

    dirty = true;

    block_segment_ret = blockmap_data.GetInt( alloc_block*28, 4 );
    last_block = alloc_block;

    return blockmap_data.GetInt( alloc_block*28+4, 8 );
}

/************************************************************************/
/*                         SetVirtualFileSize()                         */
/************************************************************************/

void SysBlockMap::SetVirtualFileSize( int image_index, uint64 file_length )

{
    FullLoad();

    layer_data.Put( file_length, 24*image_index + 12, 12 );
    dirty = true;
}

/************************************************************************/
/*                           GetVirtualFile()                           */
/************************************************************************/

SysVirtualFile *SysBlockMap::GetVirtualFile( int image )

{
    PartialLoad();

    if( image < 0 || image >= (int) virtual_files.size() )
        ThrowPCIDSKException( "GetImageSysFile(%d): invalid image index",
                              image );

    if( virtual_files[image] != NULL )
        return virtual_files[image];

    uint64  vfile_length = layer_data.GetUInt64( 24*image + 12, 12 );
    int  start_block = layer_data.GetInt( 24*image + 4, 8 );

    virtual_files[image] = 
        new SysVirtualFile( dynamic_cast<CPCIDSKFile *>(file), 
                            start_block, vfile_length,
                            this, image );

    return virtual_files[image];
}

/************************************************************************/
/*                         CreateVirtualFile()                          */
/************************************************************************/

int SysBlockMap::CreateVirtualFile()

{
    FullLoad();

/* -------------------------------------------------------------------- */
/*      Is there an existing dead layer we can reuse?                   */
/* -------------------------------------------------------------------- */
    unsigned int layer_index;

    for( layer_index = 0; layer_index < virtual_files.size(); layer_index++ )
    {
        if( layer_data.GetInt( 24*layer_index + 0, 4 ) == 1 /* dead */ )
        {
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      If not, extend the layer table.                                 */
/* -------------------------------------------------------------------- */
    if( layer_index == virtual_files.size() )
    {
        layer_index = virtual_files.size();
        layer_data.SetSize( (layer_index+1) * 24 );
        virtual_files.push_back( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Set all the entries for this layer.                             */
/* -------------------------------------------------------------------- */
    dirty = true;

    layer_data.Put( 2, 24*layer_index + 0, 4 );
    layer_data.Put( -1, 24*layer_index + 4, 8 );
    layer_data.Put( 0, 24*layer_index + 12, 12 );

    return layer_index;
}

/************************************************************************/
/*                       CreateVirtualImageFile()                       */
/************************************************************************/

int SysBlockMap::CreateVirtualImageFile( int width, int height, 
                                         int block_width, int block_height,
                                         eChanType chan_type,
                                         std::string compression )

{
    if( compression == "" )
        compression = "NONE";

/* -------------------------------------------------------------------- */
/*      Create the underlying virtual file.                             */
/* -------------------------------------------------------------------- */
    int img_index = CreateVirtualFile();
    SysVirtualFile *vfile = GetVirtualFile( img_index );

/* -------------------------------------------------------------------- */
/*      Set up the image header.                                        */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer theader(128);

    theader.Put( "", 0, 128 );

    theader.Put( width, 0, 8 );
    theader.Put( height, 8, 8 );
    theader.Put( block_width, 16, 8 );
    theader.Put( block_height, 24, 8 );
    theader.Put( DataTypeName(chan_type).c_str(), 32, 4 );
    theader.Put( compression.c_str(), 54, 8 );

    vfile->WriteToFile( theader.buffer, 0, 128 );

/* -------------------------------------------------------------------- */
/*      Setup the tile map - initially with no tiles referenced.        */
/* -------------------------------------------------------------------- */
    int tiles_per_row = (width + block_width - 1) / block_width;
    int tiles_per_col = (height + block_height - 1) / block_height;
    int tile_count = tiles_per_row * tiles_per_col;
    int i;

    PCIDSKBuffer tmap( tile_count * 20 );

    for( i = 0; i < tile_count; i++ )
    {
        tmap.Put( -1, i*12, 12 );
        tmap.Put( 0, tile_count*12 + i*8, 8 );
    }

    vfile->WriteToFile( tmap.buffer, 128, tile_count*20 );

    return img_index;
}

/************************************************************************/
/*                        GetNextBlockMapEntry()                        */
/*                                                                      */
/*      SysVirtualFile's call this method to find the next block in     */
/*      the blockmap which belongs to them.  This allows them to        */
/*      fill their blockmap "as needed" without necessarily forcing     */
/*      a full load of the blockmap.                                    */
/************************************************************************/

int SysBlockMap::GetNextBlockMapEntry( int bm_index,
                                       uint16 &segment,
                                       int &block_in_segment )

{
    if( !partial_loaded )
        PartialLoad();

/* -------------------------------------------------------------------- */
/*      If the full blockmap is already loaded, just fetch it from      */
/*      there to avoid extra IO or confusion between what is disk       */
/*      and what is in memory.                                          */
/*                                                                      */
/*      Otherwise we read from disk and hope the io level buffering     */
/*      is pretty good.                                                 */
/* -------------------------------------------------------------------- */
    char bm_entry[29];

    if( full_loaded )
    {
        memcpy( bm_entry, blockmap_data.buffer + bm_index * 28, 28 );
    }
    else
    {
        ReadFromFile( bm_entry, bm_index * 28 + 512, 28 );
    }
    
/* -------------------------------------------------------------------- */
/*      Parse the values as efficiently as we can.                      */
/* -------------------------------------------------------------------- */
    bm_entry[28] = '\0';

    int next_block = atoi( bm_entry+20 );

    bm_entry[12] = '\0';
    block_in_segment = atoi(bm_entry+4);

    bm_entry[4] = '\0';
    segment = atoi(bm_entry);
    
    return next_block;
}

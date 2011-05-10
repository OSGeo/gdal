/******************************************************************************
 *
 * Purpose:  Implementation of the CTiledChannel class.
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

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_exception.h"
#include "channel/ctiledchannel.h"
#include "segment/sysblockmap.h"
#include "core/sysvirtualfile.h"
#include "core/cpcidskfile.h"
#include "core/pcidsk_utils.h"
#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace PCIDSK;

/************************************************************************/
/*                           CTiledChannel()                            */
/************************************************************************/

CTiledChannel::CTiledChannel( PCIDSKBuffer &image_header, 
                              uint64 ih_offset,
                              PCIDSKBuffer &file_header,
                              int channelnum,
                              CPCIDSKFile *file,
                              eChanType pixel_type )
        : CPCIDSKChannel( image_header, ih_offset, file, pixel_type, channelnum)

{
/* -------------------------------------------------------------------- */
/*      Establish the virtual file we will be accessing.                */
/* -------------------------------------------------------------------- */
    std::string filename;

    image_header.Get(64,64,filename);

    assert( strstr(filename.c_str(),"SIS=") != NULL );

    image = atoi(strstr(filename.c_str(),"SIS=") + 4);

    vfile = NULL;

/* -------------------------------------------------------------------- */
/*      If this is an unassociated channel (ie. an overview), we        */
/*      will set the size and blocksize values to something             */
/*      unreasonable and set them properly in EstablishAccess()         */
/* -------------------------------------------------------------------- */
    if( channelnum == -1 )
    {
        width = -1;
        height = -1;
        block_width = -1;
        block_height = -1;
    }
}

/************************************************************************/
/*                           ~CTiledChannel()                           */
/************************************************************************/

CTiledChannel::~CTiledChannel()

{
    Synchronize();
}

/************************************************************************/
/*                          EstablishAccess()                           */
/************************************************************************/

void CTiledChannel::EstablishAccess() const

{
    if( vfile != NULL )
        return;
    
/* -------------------------------------------------------------------- */
/*      Establish the virtual file to access this image.                */
/* -------------------------------------------------------------------- */
    SysBlockMap *bmap = dynamic_cast<SysBlockMap*>(
        file->GetSegment( SEG_SYS, "SysBMDir" ));

    if( bmap == NULL )
        ThrowPCIDSKException( "Unable to find SysBMDir segment." );

    vfile = bmap->GetVirtualFile( image );

/* -------------------------------------------------------------------- */
/*      Parse the header.                                               */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer theader(128);
    std::string data_type;

    vfile->ReadFromFile( theader.buffer, 0, 128 );

    width = theader.GetInt(0,8);
    height = theader.GetInt(8,8);
    block_width = theader.GetInt(16,8);
    block_height = theader.GetInt(24,8);

    theader.Get(32,4,data_type);
    theader.Get(54, 8, compression);
    
    pixel_type = GetDataTypeFromName(data_type);
    if (pixel_type == CHN_UNKNOWN)
    {
        ThrowPCIDSKException( "Unknown channel type: %s", 
                              data_type.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Compute information on the tiles.                               */
/* -------------------------------------------------------------------- */
    tiles_per_row = (width + block_width - 1) / block_width;
    tiles_per_col = (height + block_height - 1) / block_height;
    tile_count = tiles_per_row * tiles_per_col;

/* -------------------------------------------------------------------- */
/*      Resize our tile info cache.                                     */
/* -------------------------------------------------------------------- */
    int tile_block_info_count = 
        (tile_count + tile_block_size - 1) / tile_block_size;

    tile_offsets.resize( tile_block_info_count );
    tile_sizes.resize( tile_block_info_count );
    tile_info_dirty.resize( tile_block_info_count, false );

/* -------------------------------------------------------------------- */
/*      Establish byte swapping.  Tiled data files are always big       */
/*      endian, regardless of what the headers might imply.             */
/* -------------------------------------------------------------------- */
    unsigned short test_value = 1;
    
    if( ((uint8 *) &test_value)[0] == 1 )
        needs_swap = pixel_type != CHN_8U;
    else
        needs_swap = false;
}

/************************************************************************/
/*                         LoadTileInfoBlock()                          */
/************************************************************************/

void CTiledChannel::LoadTileInfoBlock( int block )

{
    assert( tile_offsets[block].size() == 0 );

/* -------------------------------------------------------------------- */
/*      How many tiles in this block?                                   */
/* -------------------------------------------------------------------- */
    int tiles_in_block = tile_block_size;

    if( block * tile_block_size + tiles_in_block > tile_count )
        tiles_in_block = tile_count - block * tile_block_size;

/* -------------------------------------------------------------------- */
/*      Resize the vectors for this block.                              */
/* -------------------------------------------------------------------- */
    tile_offsets[block].resize( tiles_in_block );
    tile_sizes[block].resize( tiles_in_block );

/* -------------------------------------------------------------------- */
/*      Read the offset and size data from disk.                        */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer offset_map( tiles_in_block * 12 + 1 );
    PCIDSKBuffer size_map( tiles_in_block * 8 + 1 );

    vfile->ReadFromFile( offset_map.buffer, 
                         128 + block * tile_block_size * 12, 
                         tiles_in_block * 12 );
    vfile->ReadFromFile( size_map.buffer, 
                         128 + tile_count * 12 + block * tile_block_size * 8,
                         tiles_in_block * 8 );
    
    for( int i = 0; i < tiles_in_block; i++ )
    {
        char chSaved;
        char *target = offset_map.buffer + i*12;

        chSaved = target[12];
        target[12] = '\0';
        tile_offsets[block][i] = atouint64(target);
        target[12] = chSaved;

        target = size_map.buffer + i*8;
        chSaved = target[8];
        target[8] = '\0';
        tile_sizes[block][i] = atoi(target);
        target[8] = chSaved;
    }
}

/************************************************************************/
/*                         SaveTileInfoBlock()                          */
/************************************************************************/

void CTiledChannel::SaveTileInfoBlock( int block )

{
    assert( tile_offsets[block].size() != 0 );
    int tiles_in_block = tile_offsets[block].size();

/* -------------------------------------------------------------------- */
/*      Write the offset and size data to disk.                         */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer offset_map( tiles_in_block * 12 + 1 );
    PCIDSKBuffer size_map( tiles_in_block * 8 + 1 );

    for( int i = 0; i < tiles_in_block; i++ )
    {
        if( tile_offsets[block][i] == (uint64) -1 
            || tile_offsets[block][i] == 0 )
            offset_map.Put( -1, i*12, 12 );
        else
            offset_map.Put( tile_offsets[block][i], i*12, 12 );

        size_map.Put( tile_sizes[block][i], i*8, 8 );
    }

    vfile->WriteToFile( offset_map.buffer, 
                        128 + block * tile_block_size * 12, 
                        tiles_in_block * 12 );
    vfile->WriteToFile( size_map.buffer, 
                        128 + tile_count * 12 + block * tile_block_size * 8,
                        tiles_in_block * 8 );

    tile_info_dirty[block] = false;
}

/************************************************************************/
/*                            GetTileInfo()                             */
/*                                                                      */
/*      Fetch the tile offset and size for the indicated tile.          */
/************************************************************************/

void CTiledChannel::GetTileInfo( int tile_index, uint64 &offset, int &size )

{
    int block = tile_index / tile_block_size;
    int index_within_block = tile_index - block * tile_block_size;

    if( tile_offsets[block].size() == 0 )
        LoadTileInfoBlock( block );

    offset = tile_offsets[block][index_within_block];
    size = tile_sizes[block][index_within_block];
}

/************************************************************************/
/*                            SetTileInfo()                             */
/************************************************************************/

void CTiledChannel::SetTileInfo( int tile_index, uint64 offset, int size )

{
    int block = tile_index / tile_block_size;
    int index_within_block = tile_index - block * tile_block_size;

    if( tile_offsets[block].size() == 0 )
        LoadTileInfoBlock( block );

    if( offset != tile_offsets[block][index_within_block]
        || size != tile_sizes[block][index_within_block] )
    {
        tile_offsets[block][index_within_block] = offset;
        tile_sizes[block][index_within_block] = size;
        
        tile_info_dirty[block] = true;
    }
}

/************************************************************************/
/*                            Synchronize()                             */
/*                                                                      */
/*      Flush updated blockmap to disk if it is dirty.                  */
/************************************************************************/

void CTiledChannel::Synchronize()

{
    if( tile_info_dirty.size() == 0 )
        return;

    int i;

    for( i = 0; i < (int) tile_info_dirty.size(); i++ )
    {
        if( tile_info_dirty[i] )
            SaveTileInfoBlock( i );
    }

    vfile->Synchronize();
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

int CTiledChannel::ReadBlock( int block_index, void *buffer,
                              int xoff, int yoff, 
                              int xsize, int ysize )

{
    int pixel_size = DataTypeSize(GetType());

/* -------------------------------------------------------------------- */
/*      Default window if needed.                                       */
/* -------------------------------------------------------------------- */
    if( xoff == -1 && yoff == -1 && xsize == -1 && ysize == -1 )
    {
        xoff = 0;
        yoff = 0;
        xsize = GetBlockWidth();
        ysize = GetBlockHeight();
    }

/* -------------------------------------------------------------------- */
/*      Validate Window                                                 */
/* -------------------------------------------------------------------- */
    if( xoff < 0 || xoff + xsize > GetBlockWidth()
        || yoff < 0 || yoff + ysize > GetBlockHeight() )
    {
        ThrowPCIDSKException( 
            "Invalid window in ReadBloc(): xoff=%d,yoff=%d,xsize=%d,ysize=%d",
            xoff, yoff, xsize, ysize );
    }

    if( block_index < 0 || block_index >= tile_count )
    {
        ThrowPCIDSKException( "Requested non-existant block (%d)", 
                              block_index );
    }

/* -------------------------------------------------------------------- */
/*      Does this tile exist?  If not return a zeroed buffer.           */
/* -------------------------------------------------------------------- */
    uint64 tile_offset;
    int    tile_size;

    GetTileInfo( block_index, tile_offset, tile_size );

    if( tile_size == 0 )
    {
        memset( buffer, 0, GetBlockWidth() * GetBlockHeight() * pixel_size );
        return 1;
    }

/* -------------------------------------------------------------------- */
/*      The simpliest case it an uncompressed direct and complete       */
/*      tile read into the destination buffer.                          */
/* -------------------------------------------------------------------- */
    if( xoff == 0 && xsize == GetBlockWidth() 
        && yoff == 0 && ysize == GetBlockHeight() 
        && tile_size == xsize * ysize * pixel_size 
        && compression == "NONE" )
    {
        vfile->ReadFromFile( buffer, tile_offset, tile_size );

        // Do byte swapping if needed.
        if( needs_swap )
            SwapPixels( buffer, pixel_type, xsize * ysize );

        return 1;
    }

/* -------------------------------------------------------------------- */
/*      Load uncompressed data, one scanline at a time, into the        */
/*      target buffer.                                                  */
/* -------------------------------------------------------------------- */
    if( compression == "NONE" )
    {
        int iy;

        for( iy = 0; iy < ysize; iy++ )
        {
            vfile->ReadFromFile( ((uint8 *) buffer) 
                                 + iy * xsize * pixel_size,
                                 tile_offset 
                                 + ((iy+yoff)*block_width + xoff) * pixel_size,
                                 xsize * pixel_size );
        }
        
        // Do byte swapping if needed.
        if( needs_swap )
            SwapPixels( buffer, pixel_type, xsize * ysize );
        
        return 1;
    }

/* -------------------------------------------------------------------- */
/*      Load the whole compressed data into a working buffer.           */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer oCompressedData( tile_size );
    PCIDSKBuffer oUncompressedData( pixel_size * block_width * block_height );

    vfile->ReadFromFile( oCompressedData.buffer, tile_offset, tile_size );
    
/* -------------------------------------------------------------------- */
/*      Handle decompression.                                           */
/* -------------------------------------------------------------------- */
    if( compression == "RLE" )
    {
        RLEDecompressBlock( oCompressedData, oUncompressedData );
    }
    else if( strncmp(compression.c_str(),"JPEG",4) == 0 )
    {
        JPEGDecompressBlock( oCompressedData, oUncompressedData );
    }
    else
    {
        ThrowPCIDSKException( 
            "Unable to read tile of unsupported compression type: %s",
            compression.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Swap if necessary.  TODO: there is some reason to doubt that    */
/*      the old implementation properly byte swapped compressed         */
/*      data.  Perhaps this should be conditional?                      */
/* -------------------------------------------------------------------- */
    if( needs_swap )
        SwapPixels( oUncompressedData.buffer, pixel_type, 
                  GetBlockWidth() * GetBlockHeight() );

/* -------------------------------------------------------------------- */
/*      Copy out the desired subwindow.                                 */
/* -------------------------------------------------------------------- */
    int iy;
    
    for( iy = 0; iy < ysize; iy++ )
    {
        memcpy( ((uint8 *) buffer) + iy * xsize * pixel_size,
                oUncompressedData.buffer 
                + ((iy+yoff)*block_width + xoff) * pixel_size,
                xsize * pixel_size );
    }

    return 1;
}

/************************************************************************/
/*                            IsTileEmpty()                             */
/************************************************************************/
bool CTiledChannel::IsTileEmpty(void *buffer) const
{
    assert(sizeof(int32) == 4); // just to be on the safe side...

    unsigned int num_dword = 
        (block_width * block_height * DataTypeSize(pixel_type)) / 4;
    unsigned int rem = 
        (block_width * block_height * DataTypeSize(pixel_type)) % 4;

    int32* int_buf = reinterpret_cast<int32*>(buffer);

    if (num_dword > 0) {
        for (unsigned int n = 0; n < num_dword; n++) {
            if (int_buf[n]) return false;
        }
    }

    char* char_buf = reinterpret_cast<char*>(int_buf + num_dword);
    if (rem > 0) {
        for (unsigned int n = 0; n < rem; n++) {
            if (char_buf[n]) return false;
        }
    }

    return true;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int CTiledChannel::WriteBlock( int block_index, void *buffer )

{
    if( !file->GetUpdatable() )
        throw PCIDSKException( "File not open for update in WriteBlock()" );

    InvalidateOverviews();

    int pixel_size = DataTypeSize(GetType());
    int pixel_count = GetBlockWidth() * GetBlockHeight();

    if( block_index < 0 || block_index >= tile_count )
    {
        ThrowPCIDSKException( "Requested non-existant block (%d)", 
                              block_index );
    }

/* -------------------------------------------------------------------- */
/*      Fetch existing tile offset and size.                            */
/* -------------------------------------------------------------------- */
    uint64 tile_offset;
    int    tile_size;

    GetTileInfo( block_index, tile_offset, tile_size );

/* -------------------------------------------------------------------- */
/*      The simpliest case it an uncompressed direct and complete       */
/*      tile read into the destination buffer.                          */
/* -------------------------------------------------------------------- */
    if( compression == "NONE" 
        && tile_size == pixel_count * pixel_size )
    {
        // Do byte swapping if needed.
        if( needs_swap )
            SwapPixels( buffer, pixel_type, pixel_count );

        vfile->WriteToFile( buffer, tile_offset, tile_size );

        if( needs_swap )
            SwapPixels( buffer, pixel_type, pixel_count );

        return 1;
    }

    if ((int64)tile_offset == -1)
    {
        // Check if the tile is empty. If it is, we can skip writing it,
        // unless the tile is already dirty.
        bool is_empty = IsTileEmpty(buffer);

        if (is_empty) return 1; // we don't need to do anything else
    }

/* -------------------------------------------------------------------- */
/*      Copy the uncompressed data into a PCIDSKBuffer, and byte        */
/*      swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer oUncompressedData( pixel_size * block_width * block_height );

    memcpy( oUncompressedData.buffer, buffer, 
            oUncompressedData.buffer_size );

    if( needs_swap )
        SwapPixels( oUncompressedData.buffer, pixel_type, pixel_count );

/* -------------------------------------------------------------------- */
/*      Compress the imagery.                                           */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer oCompressedData;

    if( compression == "NONE" )
    {
        oCompressedData = oUncompressedData;
    }
    else if( compression == "RLE" )
    {
        RLECompressBlock( oUncompressedData, oCompressedData );
    }
    else if( strncmp(compression.c_str(),"JPEG",4) == 0 )
    {
        JPEGCompressBlock( oUncompressedData, oCompressedData );
    }
    else
    {
        ThrowPCIDSKException( 
            "Unable to write tile of unsupported compression type: %s",
            compression.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      If this fits in the existing space, just write it directly.     */
/* -------------------------------------------------------------------- */
    if( oCompressedData.buffer_size <= tile_size )
    {
        vfile->WriteToFile( oCompressedData.buffer, tile_offset, tile_size );

        tile_size = oCompressedData.buffer_size;
        SetTileInfo( block_index, tile_offset, tile_size );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we try and write it at the end of the virtual file.   */
/* -------------------------------------------------------------------- */
    else
    {
        uint64 new_offset = vfile->GetLength();
        
        vfile->WriteToFile( oCompressedData.buffer, 
                            new_offset, oCompressedData.buffer_size );

        SetTileInfo( block_index, new_offset, oCompressedData.buffer_size );
    }

    return 1;
}

/************************************************************************/
/*                           GetBlockWidth()                            */
/************************************************************************/

int CTiledChannel::GetBlockWidth() const

{
    EstablishAccess();
    return CPCIDSKChannel::GetBlockWidth();
}

/************************************************************************/
/*                           GetBlockHeight()                           */
/************************************************************************/

int CTiledChannel::GetBlockHeight() const

{
    EstablishAccess();
    return CPCIDSKChannel::GetBlockHeight();
}

/************************************************************************/
/*                              GetWidth()                              */
/************************************************************************/

int CTiledChannel::GetWidth() const

{
    if( width == -1 )
        EstablishAccess();

    return CPCIDSKChannel::GetWidth();
}

/************************************************************************/
/*                             GetHeight()                              */
/************************************************************************/

int CTiledChannel::GetHeight() const

{
    if( height == -1 )
        EstablishAccess();

    return CPCIDSKChannel::GetHeight();
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

eChanType CTiledChannel::GetType() const

{
    if( pixel_type == CHN_UNKNOWN )
        EstablishAccess();

    return CPCIDSKChannel::GetType();
}

/************************************************************************/
/*                         RLEDecompressBlock()                         */
/************************************************************************/

void CTiledChannel::RLEDecompressBlock( PCIDSKBuffer &oCompressedData,
                                        PCIDSKBuffer &oDecompressedData )

                               
{
    int    src_offset=0, dst_offset=0;
    uint8  *src = (uint8 *) oCompressedData.buffer;
    uint8  *dst = (uint8 *) oDecompressedData.buffer;
    int    pixel_size = DataTypeSize(GetType());

/* -------------------------------------------------------------------- */
/*      Process till we are out of source data, or our destination      */
/*      buffer is full.  These conditions should be satisified at       */
/*      the same time!                                                  */
/* -------------------------------------------------------------------- */
    while( src_offset + 1 + pixel_size <= oCompressedData.buffer_size
           && dst_offset < oDecompressedData.buffer_size )
    {
/* -------------------------------------------------------------------- */
/*      Extract a repeat run                                            */
/* -------------------------------------------------------------------- */
        if( src[src_offset] > 127 )
        {
            int count = src[src_offset++] - 128;
            int i;

            if( dst_offset + count * pixel_size > oDecompressedData.buffer_size)
            {
                ThrowPCIDSKException( "RLE compressed tile corrupt, overrun avoided." );
            }

            while( count-- > 0 )
            {
                for( i = 0; i < pixel_size; i++ )
                    dst[dst_offset++] = src[src_offset+i];
            }
            src_offset += pixel_size;
        }

/* -------------------------------------------------------------------- */
/*      Extract a literal run.                                          */
/* -------------------------------------------------------------------- */
        else 
        {
            int count = src[src_offset++];

            if( dst_offset + count*pixel_size > oDecompressedData.buffer_size
                || src_offset + count*pixel_size > oCompressedData.buffer_size)
            {
                ThrowPCIDSKException( "RLE compressed tile corrupt, overrun avoided." );
            }

            memcpy( dst + dst_offset, src + src_offset, 
                    pixel_size * count );
            src_offset += pixel_size * count;
            dst_offset += pixel_size * count;
        }

    }

/* -------------------------------------------------------------------- */
/*      Final validation.                                               */
/* -------------------------------------------------------------------- */
    if( src_offset != oCompressedData.buffer_size 
        || dst_offset != oDecompressedData.buffer_size ) 
    {
        ThrowPCIDSKException( "RLE compressed tile corrupt, result incomplete." );
    }
}

/************************************************************************/
/*                         RLECompressBlock()                           */
/*                                                                      */
/*      TODO: There does not seem to be any byte order logic in here!   */
/************************************************************************/

void CTiledChannel::RLECompressBlock( PCIDSKBuffer &oUncompressedData,
                                      PCIDSKBuffer &oCompressedData )

                               
{
    int    src_bytes = oUncompressedData.buffer_size;
    int    pixel_size = DataTypeSize(GetType());
    int    src_offset = 0, dst_offset = 0;
    int    i;
    uint8  *src = (uint8 *) oUncompressedData.buffer;

/* -------------------------------------------------------------------- */
/*      Loop till input exausted.                                       */
/* -------------------------------------------------------------------- */
    while( src_offset < src_bytes )
    {
        bool	bGotARun = false;
        
/* -------------------------------------------------------------------- */
/*	Establish the run length, and emit if greater than 3. 		*/
/* -------------------------------------------------------------------- */
        if( src_offset + 3*pixel_size < src_bytes )
        {
            int		count = 1;

            while( count < 127
                   && src_offset + count*pixel_size < src_bytes )
            {
                bool	bWordMatch = true;

                for( i = 0; i < pixel_size; i++ )
                {
                    if( src[src_offset+i] 
                        != src[src_offset+i+count*pixel_size] )
                        bWordMatch = false;
                }

                if( !bWordMatch )
                    break;

                count++;
            }

            if( count >= 3 )
            {
                if( oCompressedData.buffer_size < dst_offset + pixel_size+1 )
                    oCompressedData.SetSize( oCompressedData.buffer_size*2+100);

                oCompressedData.buffer[dst_offset++] = (char) (count+128);

                for( i = 0; i < pixel_size; i++ )
                    oCompressedData.buffer[dst_offset++] = src[src_offset+i];

                src_offset += count * pixel_size;

                bGotARun = true;
            }
            else
                bGotARun = false;
        }
        
/* -------------------------------------------------------------------- */
/*      Otherwise emit a literal till we encounter at least a three	*/
/*	word series.							*/
/* -------------------------------------------------------------------- */
        if( !bGotARun )
        {
            int		count = 1;
            int		match_count = 0;

            while( count < 127
                   && src_offset + count*pixel_size < src_bytes )
            {
                bool	bWordMatch = true;

                for( i = 0; i < pixel_size; i++ )
                {
                    if( src[src_offset+i]
                        != src[src_offset+i+count*pixel_size] )
                        bWordMatch = false;
                }

                if( bWordMatch )
                    match_count++;
                else
                    match_count = 0;

                if( match_count > 2 )
                    break;

                count++;
            }
            
            assert( src_offset + count*pixel_size <= src_bytes );

            while( oCompressedData.buffer_size 
                   < dst_offset + count*pixel_size+1 )
                oCompressedData.SetSize( oCompressedData.buffer_size*2+100 );

            oCompressedData.buffer[dst_offset++] = (char) count;
            memcpy( oCompressedData.buffer + dst_offset, 
                    src + src_offset, 
                    count * pixel_size );
            src_offset += count * pixel_size;
            dst_offset += count * pixel_size;
        }
    }

    oCompressedData.buffer_size = dst_offset;
}

/************************************************************************/
/*                        JPEGDecompressBlock()                         */
/************************************************************************/

void CTiledChannel::JPEGDecompressBlock( PCIDSKBuffer &oCompressedData,
                                         PCIDSKBuffer &oDecompressedData )

                               
{
    if( file->GetInterfaces()->JPEGDecompressBlock == NULL )
        ThrowPCIDSKException( "JPEG decompression not enabled in the PCIDSKInterfaces of this build." );

    file->GetInterfaces()->JPEGDecompressBlock( 
        (uint8 *) oCompressedData.buffer, oCompressedData.buffer_size,
        (uint8 *) oDecompressedData.buffer, oDecompressedData.buffer_size,
        GetBlockWidth(), GetBlockHeight(), GetType() );
}

/************************************************************************/
/*                         JPEGCompressBlock()                          */
/************************************************************************/

void CTiledChannel::JPEGCompressBlock( PCIDSKBuffer &oDecompressedData,
                                       PCIDSKBuffer &oCompressedData )
                                       

                               
{
    if( file->GetInterfaces()->JPEGCompressBlock == NULL )
        ThrowPCIDSKException( "JPEG compression not enabled in the PCIDSKInterfaces of this build." );

/* -------------------------------------------------------------------- */
/*      What quality should we be using?                                */
/* -------------------------------------------------------------------- */
    int quality = 75;

    if( compression.c_str()[4] >= '1' 
        && compression.c_str()[4] <= '0' )
        quality = atoi(compression.c_str() + 4);

/* -------------------------------------------------------------------- */
/*      Make the output buffer plent big to hold any conceivable        */
/*      result.                                                         */
/* -------------------------------------------------------------------- */
    oCompressedData.SetSize( oDecompressedData.buffer_size * 2 + 1000 );

/* -------------------------------------------------------------------- */
/*      invoke.                                                         */
/* -------------------------------------------------------------------- */
    file->GetInterfaces()->JPEGCompressBlock( 
        (uint8 *) oDecompressedData.buffer, oDecompressedData.buffer_size,
        (uint8 *) oCompressedData.buffer, oCompressedData.buffer_size,
        GetBlockWidth(), GetBlockHeight(), GetType(), 75 );
}


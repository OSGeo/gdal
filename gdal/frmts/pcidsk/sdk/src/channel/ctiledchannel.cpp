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
                              PCIDSKBuffer &file_header,
                              int channelnum,
                              CPCIDSKFile *file,
                              eChanType pixel_type )
        : CPCIDSKChannel( image_header, file, pixel_type, channelnum )

{
    tile_info_dirty = false;

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
/*      will set the size and blocksize values to someone               */
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

void CTiledChannel::EstablishAccess()

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
    
    if( data_type == "8U" )
        pixel_type = CHN_8U;
    else if( data_type == "16S" )
        pixel_type = CHN_16S;
    else if( data_type == "16U" )
        pixel_type = CHN_16U;
    else if( data_type == "32R" )
        pixel_type = CHN_32R;
    else
    {
        ThrowPCIDSKException( "Unknown channel type: %s", 
                              data_type.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Extract the tile map                                            */
/* -------------------------------------------------------------------- */
    int tiles_per_row = (width + block_width - 1) / block_width;
    int tiles_per_col = (height + block_height - 1) / block_height;
    int tile_count = tiles_per_row * tiles_per_col;
    int i;

    tile_offsets.resize( tile_count );
    tile_sizes.resize( tile_count );

    PCIDSKBuffer tmap( tile_count * 20 );

    vfile->ReadFromFile( tmap.buffer, 128, tile_count*20 );
    
    for( i = 0; i < tile_count; i++ )
    {
        tile_offsets[i] = tmap.GetUInt64( i*12 + 0, 12 );
        tile_sizes[i] = tmap.GetInt( tile_count*12 + i*8, 8 );
    }

    tile_info_dirty = false;

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
/*                            Synchronize()                             */
/*                                                                      */
/*      Flush updated blockmap to disk if it is dirty.                  */
/************************************************************************/

void CTiledChannel::Synchronize()

{
    if( !tile_info_dirty )
        return;

    int tiles_per_row = (width + block_width - 1) / block_width;
    int tiles_per_col = (height + block_height - 1) / block_height;
    int tile_count = tiles_per_row * tiles_per_col;
    int i;

    PCIDSKBuffer tmap( tile_count * 20 );

    for( i = 0; i < tile_count; i++ )
    {
        if( tile_offsets[i] == (uint64) -1 || tile_offsets[i] == 0 )
            tmap.Put( -1, i*12 + 0, 12 );
        else
            tmap.Put( tile_offsets[i], i*12 + 0, 12 );
        tmap.Put( tile_sizes[i], tile_count*12 + i*8, 8 );
    }

    vfile->WriteToFile( tmap.buffer, 128, tile_count*20 );
    vfile->Synchronize();
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

int CTiledChannel::ReadBlock( int block_index, void *buffer,
                              int xoff, int yoff, 
                              int xsize, int ysize )

{
    if( !vfile )
        EstablishAccess();

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

    if( block_index < 0 || block_index >= (int) tile_offsets.size() )
    {
        ThrowPCIDSKException( "Requested non-existant block (%d)", 
                              block_index );
    }

/* -------------------------------------------------------------------- */
/*      Does this tile exist?  If not return a zeroed buffer.           */
/* -------------------------------------------------------------------- */
    if( tile_sizes[block_index] == 0 )
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
        && tile_sizes[block_index] == xsize * ysize * pixel_size 
        && compression == "NONE" )
    {
        vfile->ReadFromFile( buffer, 
                             tile_offsets[block_index], 
                             tile_sizes[block_index] );
        // Do byte swapping if needed.
        if( needs_swap )
            SwapData( buffer, pixel_size, xsize * ysize );

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
                                 tile_offsets[block_index] 
                                 + ((iy+yoff)*block_width + xoff) * pixel_size,
                                 xsize * pixel_size );
        }
        
        // Do byte swapping if needed.
        if( needs_swap )
            SwapData( buffer, pixel_size, xsize * ysize );
        
        return 1;
    }

/* -------------------------------------------------------------------- */
/*      Load the whole compressed data into a working buffer.           */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer oCompressedData( tile_sizes[block_index] );
    PCIDSKBuffer oUncompressedData( pixel_size * block_width * block_height );

    vfile->ReadFromFile( oCompressedData.buffer, 
                         tile_offsets[block_index], 
                         tile_sizes[block_index] );
    
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
        SwapData( oUncompressedData.buffer, pixel_size, 
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
/*                             WriteBlock()                             */
/************************************************************************/

int CTiledChannel::WriteBlock( int block_index, void *buffer )

{
    if( !vfile )
        EstablishAccess();

    int pixel_size = DataTypeSize(GetType());
    int pixel_count = GetBlockWidth() * GetBlockHeight();

    if( block_index < 0 || block_index >= (int) tile_offsets.size() )
    {
        ThrowPCIDSKException( "Requested non-existant block (%d)", 
                              block_index );
    }

/* -------------------------------------------------------------------- */
/*      The simpliest case it an uncompressed direct and complete       */
/*      tile read into the destination buffer.                          */
/* -------------------------------------------------------------------- */
    if( compression == "NONE" 
        && tile_sizes[block_index] == pixel_count * pixel_size )
    {
        // Do byte swapping if needed.
        if( needs_swap )
            SwapData( buffer, pixel_size, pixel_count );

        vfile->WriteToFile( buffer, 
                            tile_offsets[block_index], 
                            tile_sizes[block_index] );

        if( needs_swap )
            SwapData( buffer, pixel_size, pixel_count );

        return 1;
    }

/* -------------------------------------------------------------------- */
/*      Copy the uncompressed data into a PCIDSKBuffer, and byte        */
/*      swap if needed.                                                 */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer oUncompressedData( pixel_size * block_width * block_height );

    memcpy( oUncompressedData.buffer, buffer, 
            oUncompressedData.buffer_size );

    if( needs_swap )
        SwapData( oUncompressedData.buffer, pixel_size, pixel_count );

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
    if( oCompressedData.buffer_size <= tile_sizes[block_index] )
    {
        vfile->WriteToFile( oCompressedData.buffer,
                            tile_offsets[block_index], 
                            oCompressedData.buffer_size );
        tile_sizes[block_index] = oCompressedData.buffer_size;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we try and write it at the end of the virtual file.   */
/* -------------------------------------------------------------------- */
    else
    {
        uint64 new_offset = vfile->GetLength();
        
        vfile->WriteToFile( oCompressedData.buffer, 
                            new_offset, oCompressedData.buffer_size );

        tile_offsets[block_index] = new_offset;
        tile_sizes[block_index] = oCompressedData.buffer_size;

    }

    tile_info_dirty = true;

    return 1;
}

/************************************************************************/
/*                           GetBlockWidth()                            */
/************************************************************************/

int CTiledChannel::GetBlockWidth()

{
    EstablishAccess();
    return CPCIDSKChannel::GetBlockWidth();
}

/************************************************************************/
/*                           GetBlockHeight()                           */
/************************************************************************/

int CTiledChannel::GetBlockHeight()

{
    EstablishAccess();
    return CPCIDSKChannel::GetBlockHeight();
}

/************************************************************************/
/*                              GetWidth()                              */
/************************************************************************/

int CTiledChannel::GetWidth()

{
    if( width == -1 )
        EstablishAccess();

    return CPCIDSKChannel::GetWidth();
}

/************************************************************************/
/*                             GetHeight()                              */
/************************************************************************/

int CTiledChannel::GetHeight()

{
    if( height == -1 )
        EstablishAccess();

    return CPCIDSKChannel::GetHeight();
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

eChanType CTiledChannel::GetType()

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

                oCompressedData.buffer[dst_offset++] = count+128;

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

            oCompressedData.buffer[dst_offset++] = count;
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


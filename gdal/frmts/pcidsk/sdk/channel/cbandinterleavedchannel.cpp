/******************************************************************************
 *
 * Purpose:  Implementation of the CBandInterleavedChannel class.
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
#include "pcidsk_channel.h"
#include "pcidsk_buffer.h"
#include "pcidsk_exception.h"
#include "pcidsk_file.h"
#include "core/pcidsk_utils.h"
#include "core/cpcidskfile.h"
#include "core/clinksegment.h"
#include "channel/cbandinterleavedchannel.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "cpl_port.h"

using namespace PCIDSK;

/************************************************************************/
/*                      CBandInterleavedChannel()                       */
/************************************************************************/

CBandInterleavedChannel::CBandInterleavedChannel( PCIDSKBuffer &image_header, 
                                                  uint64 ih_offset, 
                                                  CPL_UNUSED PCIDSKBuffer &file_header,
                                                  int channelnum,
                                                  CPCIDSKFile *file,
                                                  uint64 image_offset,
                                                  eChanType pixel_type )
        : CPCIDSKChannel( image_header, ih_offset, file, pixel_type, channelnum)

{
    io_handle_p = NULL;
    io_mutex_p = NULL;

/* -------------------------------------------------------------------- */
/*      Establish the data layout.                                      */
/* -------------------------------------------------------------------- */
    if( strcmp(file->GetInterleaving().c_str(),"FILE") == 0 )
    {
        start_byte = atouint64(image_header.Get( 168, 16 ));
        pixel_offset = atouint64(image_header.Get( 184, 8 ));
        line_offset = atouint64(image_header.Get( 192, 8 ));
    }
    else
    {
        start_byte = image_offset;
        pixel_offset = DataTypeSize(pixel_type);
        line_offset = pixel_offset * width;
    }

/* -------------------------------------------------------------------- */
/*      Establish the file we will be accessing.                        */
/* -------------------------------------------------------------------- */
    image_header.Get(64,64,filename);

    filename = MassageLink( filename );

    if( filename.length() == 0 )
        file->GetIODetails( &io_handle_p, &io_mutex_p );

    else
        filename = MergeRelativePath( file->GetInterfaces()->io,
                                      file->GetFilename(), 
                                      filename );
}

/************************************************************************/
/*                      ~CBandInterleavedChannel()                      */
/************************************************************************/

CBandInterleavedChannel::~CBandInterleavedChannel()

{
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

int CBandInterleavedChannel::ReadBlock( int block_index, void *buffer,
                                        int xoff, int yoff, 
                                        int xsize, int ysize )

{
    PCIDSKInterfaces *interfaces = file->GetInterfaces();

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

/* -------------------------------------------------------------------- */
/*      Establish region to read.                                       */
/* -------------------------------------------------------------------- */
    int    pixel_size = DataTypeSize( pixel_type );
    uint64 offset = start_byte + line_offset * block_index
        + pixel_offset * xoff;
    int    window_size = (int) (pixel_offset*(xsize-1) + pixel_size);

/* -------------------------------------------------------------------- */
/*      Get file access handles if we don't already have them.          */
/* -------------------------------------------------------------------- */
    if( io_handle_p == NULL )
        file->GetIODetails( &io_handle_p, &io_mutex_p, filename.c_str(),
                            file->GetUpdatable() );

/* -------------------------------------------------------------------- */
/*      If the imagery is packed, we can read directly into the         */
/*      target buffer.                                                  */
/* -------------------------------------------------------------------- */
    if( pixel_size == (int) pixel_offset )
    {
        MutexHolder holder( *io_mutex_p );

        interfaces->io->Seek( *io_handle_p, offset, SEEK_SET );
        interfaces->io->Read( buffer, 1, window_size, *io_handle_p );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we allocate a working buffer that holds the whole     */
/*      line, read into that, and pick out our data of interest.        */
/* -------------------------------------------------------------------- */
    else
    {
        int  i;
        PCIDSKBuffer line_from_disk( window_size );
        char *this_pixel;

        MutexHolder holder( *io_mutex_p );
        
        interfaces->io->Seek( *io_handle_p, offset, SEEK_SET );
        interfaces->io->Read( line_from_disk.buffer, 
                              1, line_from_disk.buffer_size, 
                              *io_handle_p );

        for( i = 0, this_pixel = line_from_disk.buffer; i < xsize; i++ )
        {
            memcpy( ((char *) buffer) + pixel_size * i, 
                    this_pixel, pixel_size );
            this_pixel += pixel_size;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do byte swapping if needed.                                     */
/* -------------------------------------------------------------------- */
    if( needs_swap )
        SwapPixels( buffer, pixel_type, xsize );

    return 1;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int CBandInterleavedChannel::WriteBlock( int block_index, void *buffer )

{
    PCIDSKInterfaces *interfaces = file->GetInterfaces();

    if( !file->GetUpdatable() )
        throw PCIDSKException( "File not open for update in WriteBlock()" );

    InvalidateOverviews();

/* -------------------------------------------------------------------- */
/*      Establish region to read.                                       */
/* -------------------------------------------------------------------- */
    int    pixel_size = DataTypeSize( pixel_type );
    uint64 offset = start_byte + line_offset * block_index;
    int    window_size = (int) (pixel_offset*(width-1) + pixel_size);

/* -------------------------------------------------------------------- */
/*      Get file access handles if we don't already have them.          */
/* -------------------------------------------------------------------- */
    if( io_handle_p == NULL )
        file->GetIODetails( &io_handle_p, &io_mutex_p, filename.c_str(),
                            file->GetUpdatable() );

/* -------------------------------------------------------------------- */
/*      If the imagery is packed, we can read directly into the         */
/*      target buffer.                                                  */
/* -------------------------------------------------------------------- */
    if( pixel_size == (int) pixel_offset )
    {
        MutexHolder holder( *io_mutex_p );

        if( needs_swap ) // swap before write.
            SwapPixels( buffer, pixel_type, width );

        interfaces->io->Seek( *io_handle_p, offset, SEEK_SET );
        interfaces->io->Write( buffer, 1, window_size, *io_handle_p );

        if( needs_swap ) // restore to original order.
            SwapPixels( buffer, pixel_type, width );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we allocate a working buffer that holds the whole     */
/*      line, read into that, and pick out our data of interest.        */
/* -------------------------------------------------------------------- */
    else
    {
        int  i;
        PCIDSKBuffer line_from_disk( window_size );
        char *this_pixel;

        MutexHolder holder( *io_mutex_p );
        
        interfaces->io->Seek( *io_handle_p, offset, SEEK_SET );
        interfaces->io->Read( buffer, 1, line_from_disk.buffer_size, 
                              *io_handle_p );

        for( i = 0, this_pixel = line_from_disk.buffer; i < width; i++ )
        {
            memcpy( this_pixel, ((char *) buffer) + pixel_size * i, 
                    pixel_size );

            if( needs_swap ) // swap before write.
                SwapPixels( this_pixel, pixel_type, 1 );

            this_pixel += pixel_size;
        }

        interfaces->io->Seek( *io_handle_p, offset, SEEK_SET );
        interfaces->io->Write( buffer, 1, line_from_disk.buffer_size, 
                               *io_handle_p );
    }

/* -------------------------------------------------------------------- */
/*      Do byte swapping if needed.                                     */
/* -------------------------------------------------------------------- */

    return 1;
}

/************************************************************************/
/*                            GetChanInfo()                             */
/************************************************************************/
void CBandInterleavedChannel
::GetChanInfo( std::string &filename_ret, uint64 &image_offset, 
               uint64 &pixel_offset, uint64 &line_offset, 
               bool &little_endian ) const

{
    image_offset = start_byte;
    pixel_offset = this->pixel_offset;
    line_offset = this->line_offset;
    little_endian = (byte_order == 'S');

/* -------------------------------------------------------------------- */
/*      We fetch the filename from the header since it will be the      */
/*      "clean" version without any paths.                              */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer ih(64);
    file->ReadFromFile( ih.buffer, ih_offset+64, 64 );

    ih.Get(0,64,filename_ret);
    filename_ret = MassageLink( filename_ret );
}

/************************************************************************/
/*                            SetChanInfo()                             */
/************************************************************************/

void CBandInterleavedChannel
::SetChanInfo( std::string filename, uint64 image_offset, 
               uint64 pixel_offset, uint64 line_offset, 
               bool little_endian )

{
    if( ih_offset == 0 )
        ThrowPCIDSKException( "No Image Header available for this channel." );

/* -------------------------------------------------------------------- */
/*      Fetch the existing image header.                                */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer ih(1024);

    file->ReadFromFile( ih.buffer, ih_offset, 1024 );

/* -------------------------------------------------------------------- */
/*      If the linked filename is too long to fit in the 64             */
/*      character IHi.2 field, then we need to use a link segment to    */
/*      store the filename.                                             */
/* -------------------------------------------------------------------- */
    std::string IHi2_filename;
    
    if( filename.size() > 64 )
    {
        int link_segment;
        
        ih.Get( 64, 64, IHi2_filename );
                
        if( IHi2_filename.substr(0,3) == "LNK" )
        {
            link_segment = std::atoi( IHi2_filename.c_str() + 4 );
        }
        else
        {
            char link_filename[64];
           
            link_segment = 
                file->CreateSegment( "Link    ", 
                                     "Long external channel filename link.", 
                                     SEG_SYS, 1 );

            sprintf( link_filename, "LNK %4d", link_segment );
            IHi2_filename = link_filename;
        }

        CLinkSegment *link = 
            dynamic_cast<CLinkSegment*>( file->GetSegment( link_segment ) );
        
        if( link != NULL )
        {
            link->SetPath( filename );
            link->Synchronize();
        }
    }
    
/* -------------------------------------------------------------------- */
/*      If we used to have a link segment but no longer need it, we     */
/*      need to delete the link segment.                                */
/* -------------------------------------------------------------------- */
    else
    {
        ih.Get( 64, 64, IHi2_filename );
                
        if( IHi2_filename.substr(0,3) == "LNK" )
        {
            int link_segment = std::atoi( IHi2_filename.c_str() + 4 );

            file->DeleteSegment( link_segment );
        }
        
        IHi2_filename = filename;
    }
        
/* -------------------------------------------------------------------- */
/*      Update the image header.                                        */
/* -------------------------------------------------------------------- */
    // IHi.2
    ih.Put( IHi2_filename.c_str(), 64, 64 );

    // IHi.6.1
    ih.Put( image_offset, 168, 16 );

    // IHi.6.2
    ih.Put( pixel_offset, 184, 8 );

    // IHi.6.3
    ih.Put( line_offset, 192, 8 );

    // IHi.6.5
    if( little_endian )
        ih.Put( "S", 201, 1 );
    else
        ih.Put( "N", 201, 1 );

    file->WriteToFile( ih.buffer, ih_offset, 1024 );

/* -------------------------------------------------------------------- */
/*      Update local configuration.                                     */
/* -------------------------------------------------------------------- */
    this->filename = MergeRelativePath( file->GetInterfaces()->io,
                                        file->GetFilename(), 
                                        filename );

    start_byte = image_offset;
    this->pixel_offset = pixel_offset;
    this->line_offset = line_offset;
    
    if( little_endian )
        byte_order = 'S';
    else
        byte_order = 'N';

/* -------------------------------------------------------------------- */
/*      Determine if we need byte swapping.                             */
/* -------------------------------------------------------------------- */
    unsigned short test_value = 1;

    if( ((uint8 *) &test_value)[0] == 1 )
        needs_swap = (byte_order != 'S');
    else
        needs_swap = (byte_order == 'S');
    
    if( pixel_type == CHN_8U )
        needs_swap = 0;
}

/************************************************************************/
/*                            MassageLink()                             */
/*                                                                      */
/*      Return the filename after applying translation of long          */
/*      linked filenames using a link segment.                          */
/************************************************************************/

std::string CBandInterleavedChannel::MassageLink( std::string filename_in ) const

{
    if (filename_in.find("LNK") == 0)
    {
        std::string seg_str(filename_in, 4, 4);
        unsigned int seg_num = std::atoi(seg_str.c_str());
        
        if (seg_num == 0)
        {
            throw PCIDSKException("Unable to find link segment. Link name: %s",
                                  filename_in.c_str());
        }
        
        CLinkSegment* link_seg = 
            dynamic_cast<CLinkSegment*>(file->GetSegment(seg_num));
        if (link_seg == NULL)
        {
            throw PCIDSKException("Failed to get Link Information Segment.");
        }
        
        filename_in = link_seg->GetPath();
    }

    return filename_in;
}


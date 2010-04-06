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
#include "channel/cbandinterleavedchannel.h"
#include <cassert>
#include <cstring>
#include <cstdio>

using namespace PCIDSK;

/************************************************************************/
/*                      CBandInterleavedChannel()                       */
/************************************************************************/

CBandInterleavedChannel::CBandInterleavedChannel( PCIDSKBuffer &image_header, 
                                                  uint64 ih_offset, 
                                                  PCIDSKBuffer &file_header,
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

    if( filename.length() == 0 )
        file->GetIODetails( &io_handle_p, &io_mutex_p );
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
        file->GetIODetails( &io_handle_p, &io_mutex_p, filename.c_str() );

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
        interfaces->io->Read( buffer, 1, line_from_disk.buffer_size, 
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
        file->GetIODetails( &io_handle_p, &io_mutex_p, filename.c_str() );

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


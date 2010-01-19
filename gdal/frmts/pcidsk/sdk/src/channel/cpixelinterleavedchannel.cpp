/******************************************************************************
 *
 * Purpose:  Implementation of the CPixelInterleavedChannel class.
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
#include "core/pcidsk_utils.h"
#include "core/cpcidskfile.h"
#include "channel/cpixelinterleavedchannel.h"
#include <cassert>
#include <cstring>

using namespace PCIDSK;

/************************************************************************/
/*                      CPixelInterleavedChannel()                      */
/************************************************************************/

CPixelInterleavedChannel::CPixelInterleavedChannel( PCIDSKBuffer &image_header, 
                                                    PCIDSKBuffer &file_header,
                                                    int channelnum,
                                                    CPCIDSKFile *file,
                                                    int image_offset,
                                                    eChanType pixel_type )
        : CPCIDSKChannel( image_header, file, pixel_type, channelnum )

{
    this->image_offset = image_offset;
}

/************************************************************************/
/*                     ~CPixelInterleavedChannel()                      */
/************************************************************************/

CPixelInterleavedChannel::~CPixelInterleavedChannel()

{
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

int CPixelInterleavedChannel::ReadBlock( int block_index, void *buffer,
                                         int win_xoff, int win_yoff, 
                                         int win_xsize, int win_ysize )

{
/* -------------------------------------------------------------------- */
/*      Default window if needed.                                       */
/* -------------------------------------------------------------------- */
    if( win_xoff == -1 && win_yoff == -1 && win_xsize == -1 && win_ysize == -1 )
    {
        win_xoff = 0;
        win_yoff = 0;
        win_xsize = GetBlockWidth();
        win_ysize = GetBlockHeight();
    }

/* -------------------------------------------------------------------- */
/*      Validate Window                                                 */
/* -------------------------------------------------------------------- */
    if( win_xoff < 0 || win_xoff + win_xsize > GetBlockWidth()
        || win_yoff < 0 || win_yoff + win_ysize > GetBlockHeight() )
    {
        ThrowPCIDSKException( 
            "Invalid window in ReadBloc(): win_xoff=%d,win_yoff=%d,xsize=%d,ysize=%d",
            win_xoff, win_yoff, win_xsize, win_ysize );
    }

/* -------------------------------------------------------------------- */
/*      Work out sizes and offsets.                                     */
/* -------------------------------------------------------------------- */
    int pixel_group = file->GetPixelGroupSize();
    int pixel_size = DataTypeSize(GetType());

/* -------------------------------------------------------------------- */
/*      Read and lock the scanline.                                     */
/* -------------------------------------------------------------------- */
    uint8 *pixel_buffer = (uint8 *) 
        file->ReadAndLockBlock( block_index, win_xoff, win_xsize);

/* -------------------------------------------------------------------- */
/*      Copy the data into our callers buffer.  Try to do this          */
/*      reasonably efficiently.  We might consider adding faster        */
/*      cases for 16/32bit data that is word aligned.                   */
/* -------------------------------------------------------------------- */
    if( pixel_size == pixel_group )
        memcpy( buffer, pixel_buffer, pixel_size * win_xsize );
    else
    {
        int i;
        uint8  *src = ((uint8 *)pixel_buffer) + image_offset;
        uint8  *dst = (uint8 *) buffer;

        if( pixel_size == 1 )
        {
            for( i = win_xsize; i != 0; i-- )
            {
                *dst = *src;
                dst++;
                src += pixel_group;
            }
        }
        else if( pixel_size == 2 )
        {
            for( i = win_xsize; i != 0; i-- )
            {
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                src += pixel_group-2;
            }
        }
        else if( pixel_size == 4 )
        {
            for( i = win_xsize; i != 0; i-- )
            {
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                src += pixel_group-4;
            }
        }
        else
            ThrowPCIDSKException( "Unsupported pixel type..." );
    }
    
    file->UnlockBlock( 0 );

/* -------------------------------------------------------------------- */
/*      Do byte swapping if needed.                                     */
/* -------------------------------------------------------------------- */
    if( needs_swap )
        SwapData( buffer, pixel_size, win_xsize );

    return 1;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int CPixelInterleavedChannel::WriteBlock( int block_index, void *buffer )

{
    if( !file->GetUpdatable() )
        throw PCIDSKException( "File not open for update in WriteBlock()" );

/* -------------------------------------------------------------------- */
/*      Work out sizes and offsets.                                     */
/* -------------------------------------------------------------------- */
    int pixel_group = file->GetPixelGroupSize();
    int pixel_size = DataTypeSize(GetType());

/* -------------------------------------------------------------------- */
/*      Read and lock the scanline.                                     */
/* -------------------------------------------------------------------- */
    uint8 *pixel_buffer = (uint8 *) file->ReadAndLockBlock( block_index );

/* -------------------------------------------------------------------- */
/*      Copy the data into our callers buffer.  Try to do this          */
/*      reasonably efficiently.  We might consider adding faster        */
/*      cases for 16/32bit data that is word aligned.                   */
/* -------------------------------------------------------------------- */
    if( pixel_size == pixel_group )
        memcpy( pixel_buffer, buffer, pixel_size * width );
    else
    {
        int i;
        uint8  *dst = ((uint8 *)pixel_buffer) + image_offset;
        uint8  *src = (uint8 *) buffer;

        if( pixel_size == 1 )
        {
            for( i = width; i != 0; i-- )
            {
                *dst = *src;
                src++;
                dst += pixel_group;
            }
        }
        else if( pixel_size == 2 )
        {
            for( i = width; i != 0; i-- )
            {
                *(dst++) = *(src++);
                *(dst++) = *(src++);

                if( needs_swap )
                    SwapData( dst-2, 2, 1 );

                dst += pixel_group-2;
            }
        }
        else if( pixel_size == 4 )
        {
            for( i = width; i != 0; i-- )
            {
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);

                if( needs_swap )
                    SwapData( dst-4, 4, 1);

                dst += pixel_group-4;

            }
        }
        else
            ThrowPCIDSKException( "Unsupported pixel type..." );
    }
    
    file->UnlockBlock( 1 );

/* -------------------------------------------------------------------- */
/*      Do byte swapping if needed.                                     */
/* -------------------------------------------------------------------- */

    return 1;
}


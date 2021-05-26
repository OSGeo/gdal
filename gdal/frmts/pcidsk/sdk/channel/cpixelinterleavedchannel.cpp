/******************************************************************************
 *
 * Purpose:  Implementation of the CPixelInterleavedChannel class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

CPixelInterleavedChannel::CPixelInterleavedChannel( PCIDSKBuffer &image_headerIn,
                                                    uint64 ih_offsetIn,
                                                    CPL_UNUSED PCIDSKBuffer &file_headerIn,
                                                    int channelnumIn,
                                                    CPCIDSKFile *fileIn,
                                                    int image_offsetIn,
                                                    eChanType pixel_typeIn )
        : CPCIDSKChannel( image_headerIn, ih_offsetIn, fileIn, pixel_typeIn, channelnumIn)

{
    this->image_offset = image_offsetIn;
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
        return ThrowPCIDSKException(0,
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
        else if( pixel_size == 8 )
        {
            for( i = win_xsize; i != 0; i-- )
            {
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                src += pixel_group-8;
            }
        }
        else
            return ThrowPCIDSKException(0, "Unsupported pixel type..." );
    }

    file->UnlockBlock( false );

/* -------------------------------------------------------------------- */
/*      Do byte swapping if needed.                                     */
/* -------------------------------------------------------------------- */
    if( needs_swap )
        SwapPixels( buffer, pixel_type, win_xsize );

    return 1;
}

/************************************************************************/
/*                             CopyPixels()                             */
/************************************************************************/

template <typename T>
void CopyPixels(const T* const src, T* const dst,
                std::size_t offset, std::size_t count)
{
    for (std::size_t i = 0; i < count; i++)
    {
        dst[i] = src[(i + 1) * offset];
    }
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int CPixelInterleavedChannel::WriteBlock( int block_index, void *buffer )

{
    if( !file->GetUpdatable() )
        return ThrowPCIDSKException(0, "File not open for update in WriteBlock()" );

    InvalidateOverviews();

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
    {
        memcpy( pixel_buffer, buffer, pixel_size * width );

        if( needs_swap )
        {
            bool complex = IsDataTypeComplex( GetType() );

            if( complex )
                SwapData( pixel_buffer, pixel_size/2, width*2 );
            else
                SwapData( pixel_buffer, pixel_size, width );
        }
    }
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
            bool complex = IsDataTypeComplex( GetType() );

            for( i = width; i != 0; i-- )
            {
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);

                if( needs_swap )
                {
                    if( complex )
                        SwapData( dst-4, 2, 2);
                    else
                        SwapData( dst-4, 4, 1);
                }

                dst += pixel_group-4;
            }
        }
        else if( pixel_size == 8 )
        {
            bool complex = IsDataTypeComplex( GetType() );

            for( i = width; i != 0; i-- )
            {
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);
                *(dst++) = *(src++);

                if( needs_swap )
                {
                    if( complex )
                        SwapData( dst-8, 4, 2);
                    else
                        SwapData( dst-8, 8, 1);
                }

                dst += pixel_group-8;
            }
        }
        else
            return ThrowPCIDSKException(0, "Unsupported pixel type..." );
    }

    file->UnlockBlock( true );

    return 1;
}

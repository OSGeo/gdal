/******************************************************************************
 *
 * Purpose:  Implementation of the CExternalChannel class.
 *
 * This class is used to implement band interleaved channels that are
 * references to an external image database that is not just a raw file.
 * It uses the application supplied EDB interface to access non-PCIDSK files.
 * 
 ******************************************************************************
 * Copyright (c) 2010
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
#include "channel/cexternalchannel.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace PCIDSK;

/************************************************************************/
/*                          CExternalChannel()                          */
/************************************************************************/

CExternalChannel::CExternalChannel( PCIDSKBuffer &image_header, 
                                    uint64 ih_offset, 
                                    PCIDSKBuffer &file_header,
                                    int channelnum,
                                    CPCIDSKFile *file,
                                    eChanType pixel_type )
        : CPCIDSKChannel( image_header, ih_offset, file, pixel_type, channelnum)

{
    db = NULL;
    mutex = NULL;

/* -------------------------------------------------------------------- */
/*      Establish the data window.                                      */
/* -------------------------------------------------------------------- */
    exoff  = atoi(image_header.Get( 250, 8 ));
    eyoff  = atoi(image_header.Get( 258, 8 ));
    exsize = atoi(image_header.Get( 266, 8 ));
    eysize = atoi(image_header.Get( 274, 8 ));

    echannel = atoi(image_header.Get( 282, 8 ));

/* -------------------------------------------------------------------- */
/*      Establish the file we will be accessing.                        */
/* -------------------------------------------------------------------- */
    image_header.Get(64,64,filename);
}

/************************************************************************/
/*                         ~CExternalChannel()                          */
/************************************************************************/

CExternalChannel::~CExternalChannel()

{
    // no need to deaccess the EDBFile - the file is responsible for that.
}

/************************************************************************/
/*                              AccessDB()                              */
/************************************************************************/

void CExternalChannel::AccessDB() const

{
    if( db != NULL )
        return;

/* -------------------------------------------------------------------- */
/*      open, or fetch an already open file handle.                     */
/* -------------------------------------------------------------------- */
    writable = file->GetEDBFileDetails( &db, &mutex, filename );

/* -------------------------------------------------------------------- */
/*      Capture the block size.                                         */
/* -------------------------------------------------------------------- */
    block_width = db->GetBlockWidth( echannel );
    if( block_width > width )
        block_width = width;
    block_height = db->GetBlockHeight( echannel );
    if( block_height > height )
        block_height = height;

    blocks_per_row = (GetWidth() + block_width - 1) / block_width;
}

/************************************************************************/
/*                           GetBlockWidth()                            */
/************************************************************************/

int CExternalChannel::GetBlockWidth() const

{
    AccessDB();

    return block_width;
}

/************************************************************************/
/*                           GetBlockHeight()                           */
/************************************************************************/

int CExternalChannel::GetBlockHeight() const

{
    AccessDB();

    return block_height;
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

int CExternalChannel::ReadBlock( int block_index, void *buffer,
                                 int xoff, int yoff, 
                                 int xsize, int ysize )

{
    AccessDB();

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
            "Invalid window in ReadBlock(): xoff=%d,yoff=%d,xsize=%d,ysize=%d",
            xoff, yoff, xsize, ysize );
    }

/* -------------------------------------------------------------------- */
/*      Do a direct call for the simpliest case of 1:1 block mapping.   */
/* -------------------------------------------------------------------- */
    if( exoff == 0 && eyoff == 0 
        && exsize == db->GetWidth()
        && eysize == db->GetHeight() )
    {
        MutexHolder oHolder( mutex );
        return db->ReadBlock( echannel, block_index, buffer, 
                              xoff, yoff, xsize, ysize );
    }

/* ==================================================================== */
/*      Otherwise we need to break this down into potentially up to     */
/*      four requests against the source file.                          */
/* ==================================================================== */
    int src_block_width  = db->GetBlockWidth( echannel );
    int src_block_height = db->GetBlockHeight( echannel );
    int src_blocks_per_row = (db->GetWidth() + src_block_width - 1) 
        / src_block_width;
    int pixel_size = DataTypeSize(GetType());
    uint8 *temp_buffer = (uint8 *) calloc(src_block_width*src_block_height,
                                          pixel_size);
    int txoff, tyoff, txsize, tysize;
    int dst_blockx, dst_blocky;

    if( temp_buffer == NULL )
        ThrowPCIDSKException( "Failed to allocate temporary block buffer." );

    dst_blockx = block_index % blocks_per_row;
    dst_blocky = block_index / blocks_per_row;

    // what is the region of our desired data on the destination file?

    txoff = dst_blockx * block_width + exoff + xoff;
    tyoff = dst_blocky * block_height + eyoff + yoff;
    txsize = xsize;
    tysize = ysize;
    
/* -------------------------------------------------------------------- */
/*      read external block for top left corner of target block.        */
/* -------------------------------------------------------------------- */
    int ablock_x, ablock_y, i_line;
    int axoff, ayoff, axsize, aysize;
    int block1_xsize=0, block1_ysize=0;
    int ttxoff, ttyoff, ttxsize, ttysize;
    
    ttxoff = txoff;
    ttyoff = tyoff;
    ttxsize = txsize;
    ttysize = tysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 )
        block1_xsize = axsize;
    else
        block1_xsize = 0;

    if( aysize > 0 )
        block1_ysize = aysize;
    else
        block1_ysize = 0;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer, axoff, ayoff, axsize, aysize );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( ((uint8*) buffer) + i_line * xsize * pixel_size, 
                    temp_buffer + i_line * axsize * pixel_size,
                    axsize * pixel_size );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      read external block for top right corner of target block.       */
/* -------------------------------------------------------------------- */
    ttxoff = txoff + block1_xsize;
    ttyoff = tyoff;
    ttxsize = txsize - block1_xsize;
    ttysize = tysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer, axoff, ayoff, axsize, aysize );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( ((uint8*) buffer) 
                    + (block1_xsize + i_line * xsize) * pixel_size, 
                    temp_buffer + i_line * axsize * pixel_size,
                    axsize * pixel_size );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      read external block for bottom left corner of target block.     */
/* -------------------------------------------------------------------- */
    ttxoff = txoff;
    ttyoff = tyoff + block1_ysize;
    ttxsize = txsize;
    ttysize = tysize - block1_ysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer, axoff, ayoff, axsize, aysize );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( ((uint8*) buffer) 
                    + (i_line + block1_ysize) * xsize * pixel_size, 
                    temp_buffer + i_line * axsize * pixel_size,
                    axsize * pixel_size );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      read external block for bottom left corner of target block.     */
/* -------------------------------------------------------------------- */
    ttxoff = txoff + block1_xsize;
    ttyoff = tyoff + block1_ysize;
    ttxsize = txsize - block1_xsize;
    ttysize = tysize - block1_ysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer, axoff, ayoff, axsize, aysize );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( ((uint8*) buffer) 
                    + (block1_xsize + (i_line + block1_ysize) * xsize) * pixel_size, 
                    temp_buffer + i_line * axsize * pixel_size,
                    axsize * pixel_size );
        }
    }
    
    free( temp_buffer );

    return 1;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int CExternalChannel::WriteBlock( int block_index, void *buffer )

{
    AccessDB();

    if( !file->GetUpdatable() || !writable )
        throw PCIDSKException( "File not open for update in WriteBlock()" );

/* -------------------------------------------------------------------- */
/*      Pass the request on directly in the simple case.                */
/* -------------------------------------------------------------------- */
    if( exoff == 0 && eyoff == 0 
        && exsize == db->GetWidth()
        && eysize == db->GetHeight() )
    {
        MutexHolder oHolder( mutex );
        return db->WriteBlock( echannel, block_index, buffer ); 
    }

/* ==================================================================== */
/*      Otherwise we need to break this down into potentially up to     */
/*      four requests against the source file.                          */
/* ==================================================================== */
    int src_block_width  = db->GetBlockWidth( echannel );
    int src_block_height = db->GetBlockHeight( echannel );
    int src_blocks_per_row = (db->GetWidth() + src_block_width - 1) 
        / src_block_width;
    int pixel_size = DataTypeSize(GetType());
    uint8 *temp_buffer = (uint8 *) calloc(src_block_width*src_block_height,
                                          pixel_size);
    int txoff, tyoff, txsize, tysize;
    int dst_blockx, dst_blocky;

    if( temp_buffer == NULL )
        ThrowPCIDSKException( "Failed to allocate temporary block buffer." );

    dst_blockx = block_index % blocks_per_row;
    dst_blocky = block_index / blocks_per_row;

    // what is the region of our desired data on the destination file?

    txoff = dst_blockx * block_width + exoff;
    tyoff = dst_blocky * block_height + eyoff;
    txsize = block_width;
    tysize = block_height;
    
/* -------------------------------------------------------------------- */
/*      process external block for top left corner of target block.     */
/* -------------------------------------------------------------------- */
    int ablock_x, ablock_y, i_line;
    int axoff, ayoff, axsize, aysize;
    int block1_xsize=0, block1_ysize=0;
    int ttxoff, ttyoff, ttxsize, ttysize;
    
    ttxoff = txoff;
    ttyoff = tyoff;
    ttxsize = txsize;
    ttysize = tysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 )
        block1_xsize = axsize;
    else
        block1_xsize = 0;

    if( aysize > 0 )
        block1_ysize = aysize;
    else
        block1_ysize = 0;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( temp_buffer 
                    + (i_line+ayoff) * src_block_width * pixel_size
                    + axoff * pixel_size,
                    ((uint8*) buffer) + i_line * block_width * pixel_size, 
                    axsize * pixel_size );
        }

        db->WriteBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                        temp_buffer );
    }

/* -------------------------------------------------------------------- */
/*      read external block for top right corner of target block.       */
/* -------------------------------------------------------------------- */
    ttxoff = txoff + block1_xsize;
    ttyoff = tyoff;
    ttxsize = txsize - block1_xsize;
    ttysize = tysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( temp_buffer 
                    + (i_line+ayoff) * src_block_width * pixel_size
                    + axoff * pixel_size,
                    ((uint8*) buffer) + i_line * block_width * pixel_size
                    + block1_xsize * pixel_size,
                    axsize * pixel_size );
        }

        db->WriteBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                        temp_buffer );
    }
    
/* -------------------------------------------------------------------- */
/*      read external block for bottom left corner of target block.     */
/* -------------------------------------------------------------------- */
    ttxoff = txoff;
    ttyoff = tyoff + block1_ysize;
    ttxsize = txsize;
    ttysize = tysize - block1_ysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( temp_buffer 
                    + (i_line+ayoff) * src_block_width * pixel_size
                    + axoff * pixel_size,
                    ((uint8*) buffer) 
                    + (i_line+block1_ysize) * block_width * pixel_size,
                    axsize * pixel_size );
        }

        db->WriteBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                        temp_buffer );
    }
    
/* -------------------------------------------------------------------- */
/*      read external block for bottom left corner of target block.     */
/* -------------------------------------------------------------------- */
    ttxoff = txoff + block1_xsize;
    ttyoff = tyoff + block1_ysize;
    ttxsize = txsize - block1_xsize;
    ttysize = tysize - block1_ysize;
    
    ablock_x = ttxoff / src_block_width;
    ablock_y = ttyoff / src_block_height;

    axoff = ttxoff - ablock_x * src_block_width;
    ayoff = ttyoff - ablock_y * src_block_height;

    if( axoff + ttxsize > src_block_width )
        axsize = src_block_width - axoff;
    else
        axsize = ttxsize;

    if( ayoff + ttysize > src_block_height )
        aysize = src_block_height - ayoff;
    else
        aysize = ttysize;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        db->ReadBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                       temp_buffer );
                       
        for( i_line = 0; i_line < aysize; i_line++ )
        {
            memcpy( temp_buffer 
                    + (i_line+ayoff) * src_block_width * pixel_size
                    + axoff * pixel_size,
                    ((uint8*) buffer) 
                    + (i_line+block1_ysize) * block_width * pixel_size
                    + block1_xsize * pixel_size,
                    axsize * pixel_size );
        }

        db->WriteBlock( echannel, ablock_x + ablock_y * src_blocks_per_row, 
                        temp_buffer );
    }

    free( temp_buffer );

    return 1;
}


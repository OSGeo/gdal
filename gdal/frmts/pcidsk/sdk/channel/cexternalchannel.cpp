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

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_channel.h"
#include "pcidsk_buffer.h"
#include "pcidsk_exception.h"
#include "pcidsk_file.h"
#include "core/pcidsk_utils.h"
#include "core/cpcidskfile.h"
#include "channel/cexternalchannel.h"
#include "core/clinksegment.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>

using namespace PCIDSK;

/************************************************************************/
/*                          CExternalChannel()                          */
/************************************************************************/

CExternalChannel::CExternalChannel( PCIDSKBuffer &image_headerIn,
                                    uint64 ih_offsetIn,
                                    CPL_UNUSED PCIDSKBuffer &file_headerIn,
                                    std::string filenameIn,
                                    int channelnumIn,
                                    CPCIDSKFile *fileIn,
                                    eChanType pixel_typeIn )
        : CPCIDSKChannel( image_headerIn, ih_offsetIn, fileIn, pixel_typeIn, channelnumIn)

{
    db = nullptr;
    mutex = nullptr;
    writable = false;
    blocks_per_row = 0;

/* -------------------------------------------------------------------- */
/*      Establish the data window.                                      */
/* -------------------------------------------------------------------- */
    exoff  = atoi(image_headerIn.Get( 250, 8 ));
    eyoff  = atoi(image_headerIn.Get( 258, 8 ));
    exsize = atoi(image_headerIn.Get( 266, 8 ));
    eysize = atoi(image_headerIn.Get( 274, 8 ));

    echannel = atoi(image_headerIn.Get( 282, 8 ));

    if (echannel == 0) {
        echannel = channelnumIn;
    }

    if( exoff < 0 || eyoff < 0 || exsize < 0 || eysize < 0 )
    {
        ThrowPCIDSKException("Invalid data window parameters for CExternalChannel");
    }

/* -------------------------------------------------------------------- */
/*      Establish the file we will be accessing.                        */
/* -------------------------------------------------------------------- */
    if( filenameIn != "" )
        this->filename = filenameIn;
    else
        image_headerIn.Get(64,64,this->filename);
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
    if( db != nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      open, or fetch an already open file handle.                     */
/* -------------------------------------------------------------------- */
    writable = file->GetEDBFileDetails( &db, &mutex, filename );

    if( echannel < 0 || echannel > db->GetChannels() )
    {
        ThrowPCIDSKException( 0,
            "Invalid channel number: %d", echannel );
    }

    pixel_type = db->GetType(echannel);

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
/*                              GetType()                               */
/************************************************************************/

eChanType CExternalChannel::GetType() const
{
    AccessDB();

    return pixel_type;
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
        return ThrowPCIDSKException( 0,
            "Invalid window in ReadBlock(): xoff=%d,yoff=%d,xsize=%d,ysize=%d",
            xoff, yoff, xsize, ysize );
    }

/* -------------------------------------------------------------------- */
/*      Do a direct call for the simplest case of 1:1 block mapping.   */
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

    if( temp_buffer == nullptr )
        return ThrowPCIDSKException(0, "Failed to allocate temporary block buffer." );

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

    PCIDSKBuffer oTempBufferAutoPtr;
    oTempBufferAutoPtr.buffer = (char *) temp_buffer;

    if( axsize > 0 && aysize > 0 )
    {
        MutexHolder oHolder( mutex );
        if( src_blocks_per_row > 0 &&
            (ablock_y > INT_MAX / src_blocks_per_row ||
             ablock_y * src_blocks_per_row > INT_MAX - ablock_x) )
        {
            ThrowPCIDSKException(0, "Integer overflow." );
        }
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

    return 1;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int CExternalChannel::WriteBlock( int block_index, void *buffer )

{
    AccessDB();

    if( !file->GetUpdatable() || !writable )
        return ThrowPCIDSKException(0, "File not open for update in WriteBlock()" );

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

    if( temp_buffer == nullptr )
        return ThrowPCIDSKException(0, "Failed to allocate temporary block buffer." );

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

/************************************************************************/
/*                            GetEChanInfo()                            */
/************************************************************************/
void CExternalChannel::GetEChanInfo( std::string &filenameOut, int &echannelOut,
                                     int &exoffOut, int &eyoffOut,
                                     int &exsizeOut, int &eysizeOut ) const

{
    echannelOut = this->echannel;
    exoffOut = this->exoff;
    eyoffOut = this->eyoff;
    exsizeOut = this->exsize;
    eysizeOut = this->eysize;
    filenameOut = this->filename;
}

/************************************************************************/
/*                            SetEChanInfo()                            */
/************************************************************************/

void CExternalChannel::SetEChanInfo( std::string filenameIn, int echannelIn,
                                     int exoffIn, int eyoffIn,
                                     int exsizeIn, int eysizeIn )

{
    if( ih_offset == 0 )
        return ThrowPCIDSKException( "No Image Header available for this channel." );

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

    if( filenameIn.size() > 64 )
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

            snprintf( link_filename, sizeof(link_filename), "LNK %4d", link_segment );
            IHi2_filename = link_filename;
        }

        CLinkSegment *link =
            dynamic_cast<CLinkSegment*>( file->GetSegment( link_segment ) );

        if( link != nullptr )
        {
            link->SetPath( filenameIn );
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

        IHi2_filename = filenameIn;
    }

/* -------------------------------------------------------------------- */
/*      Update the image header.                                        */
/* -------------------------------------------------------------------- */
    // IHi.2
    ih.Put( IHi2_filename.c_str(), 64, 64 );

    // IHi.6.1
    ih.Put( "", 168, 16 );

    // IHi.6.2
    ih.Put( "", 184, 8 );

    // IHi.6.3
    ih.Put( "", 192, 8 );

    // IHi.6.5
    ih.Put( "", 201, 1 );

    // IHi.6.7
    ih.Put( exoffIn, 250, 8 );

    // IHi.6.8
    ih.Put( eyoffIn, 258, 8 );

    // IHi.6.9
    ih.Put( exsizeIn, 266, 8 );

    // IHi.6.10
    ih.Put( eysizeIn, 274, 8 );

    // IHi.6.11
    ih.Put( echannelIn, 282, 8 );

    file->WriteToFile( ih.buffer, ih_offset, 1024 );

/* -------------------------------------------------------------------- */
/*      Update local configuration.                                     */
/* -------------------------------------------------------------------- */
    this->filename = file->GetInterfaces()->MergeRelativePath( file->GetInterfaces()->io,
                                        file->GetFilename(),
                                        filenameIn );

    this->exoff = exoffIn;
    this->eyoff = eyoffIn;
    this->exsize = exsizeIn;
    this->eysize = eysizeIn;
    this->echannel = echannelIn;
}

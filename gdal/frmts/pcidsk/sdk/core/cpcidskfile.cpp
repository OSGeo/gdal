/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKFile class.
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
#include "pcidsk_file.h"
#include "pcidsk_exception.h"
#include "pcidsk_channel.h"
#include "pcidsk_segment.h"
#include "core/mutexholder.h"
#include "core/pcidsk_utils.h"
#include "core/cpcidskfile.h"

// Channel types
#include "channel/cbandinterleavedchannel.h"
#include "channel/cpixelinterleavedchannel.h"
#include "channel/ctiledchannel.h"

// Segment types
#include "segment/cpcidskgeoref.h"
#include "segment/cpcidskpct.h"
#include "segment/cpcidskvectorsegment.h"
#include "segment/metadatasegment.h"
#include "segment/sysblockmap.h"
#include "segment/cpcidskrpcmodel.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

#include <iostream>

using namespace PCIDSK;

/************************************************************************/
/*                             CPCIDSKFile()                             */
/************************************************************************/

CPCIDSKFile::CPCIDSKFile()

{
    io_handle = NULL;
    io_mutex = NULL;
    updatable = false;

/* -------------------------------------------------------------------- */
/*      Initialize the metadata object, but do not try to load till     */
/*      needed.                                                         */
/* -------------------------------------------------------------------- */
    metadata.Initialize( this, "FIL", 0 );
}

/************************************************************************/
/*                            ~CPCIDSKFile()                             */
/************************************************************************/

CPCIDSKFile::~CPCIDSKFile()

{
    Synchronize();

/* -------------------------------------------------------------------- */
/*      Cleanup last block buffer.                                      */
/* -------------------------------------------------------------------- */
    if( last_block_data != NULL )
    {
        last_block_index = -1;
        free( last_block_data );
        last_block_data = NULL;
        delete last_block_mutex;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup channels and segments.                                  */
/* -------------------------------------------------------------------- */
    size_t i;
    for( i = 0; i < channels.size(); i++ )
    {
        delete channels[i];
        channels[i] = NULL;
    }
    
    for( i = 0; i < segments.size(); i++ )
    {
        delete segments[i];
        segments[i] = NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Close and cleanup IO stuff.                                     */
/* -------------------------------------------------------------------- */
    {
        MutexHolder oHolder( io_mutex );

        if( io_handle )
        {
            interfaces.io->Close( io_handle );
            io_handle = NULL;
        }
    }

    size_t i_file;

    for( i_file=0; i_file < file_list.size(); i_file++ )
    {
        delete file_list[i_file].io_mutex;
        file_list[i_file].io_mutex = NULL;

        interfaces.io->Close( file_list[i_file].io_handle );
        file_list[i_file].io_handle = NULL;
    }

    delete io_mutex;
}

/************************************************************************/
/*                            Synchronize()                             */
/************************************************************************/

void CPCIDSKFile::Synchronize()

{
    if( !GetUpdatable() )
        return;

/* -------------------------------------------------------------------- */
/*      Flush out last line caching stuff for pixel interleaved data.   */
/* -------------------------------------------------------------------- */
    FlushBlock();

/* -------------------------------------------------------------------- */
/*      Synchronize all channels.                                       */
/* -------------------------------------------------------------------- */
    size_t i;
    for( i = 0; i < channels.size(); i++ )
        channels[i]->Synchronize();
    
/* -------------------------------------------------------------------- */
/*      Synchronize all segments we have instantiated.                  */
/* -------------------------------------------------------------------- */
    for( i = 0; i < segments.size(); i++ )
    {
        if( segments[i] != NULL )
            segments[i]->Synchronize();
    }

/* -------------------------------------------------------------------- */
/*      Ensure the file is synhronized to disk.                         */
/* -------------------------------------------------------------------- */
    MutexHolder oHolder( io_mutex );

    interfaces.io->Flush( io_handle );
}

/************************************************************************/
/*                             GetChannel()                             */
/************************************************************************/

PCIDSKChannel *CPCIDSKFile::GetChannel( int band )

{
    if( band < 1 || band > channel_count )
        ThrowPCIDSKException( "Out of range band (%d) requested.", 
                              band );

    return channels[band-1];
}

/************************************************************************/
/*                             GetSegment()                             */
/************************************************************************/

PCIDSK::PCIDSKSegment *CPCIDSKFile::GetSegment( int segment )

{
/* -------------------------------------------------------------------- */
/*      Is this a valid segment?                                        */
/* -------------------------------------------------------------------- */
    if( segment < 1 || segment > segment_count )
        return NULL;

    const char *segment_pointer = segment_pointers.buffer + (segment-1) * 32;

    if( segment_pointer[0] != 'A' && segment_pointer[0] != 'L' )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we already have a corresponding object?                      */
/* -------------------------------------------------------------------- */
    if( segments[segment] != NULL )
        return segments[segment];

/* -------------------------------------------------------------------- */
/*      Instantiate per the type.                                       */
/* -------------------------------------------------------------------- */
    int segment_type = segment_pointers.GetInt((segment-1)*32+1,3);
    PCIDSKSegment *segobj = NULL;

    switch( segment_type )
    {
      case SEG_GEO:
        segobj = new CPCIDSKGeoref( this, segment, segment_pointer );
        break;

      case SEG_PCT:
        segobj = new CPCIDSK_PCT( this, segment, segment_pointer );
        break;

      case SEG_VEC:
        segobj = new CPCIDSKVectorSegment( this, segment, segment_pointer );
        break;

      case SEG_SYS:
        if( strncmp(segment_pointer + 4, "SysBMDir",8) == 0 )
            segobj = new SysBlockMap( this, segment, segment_pointer );
        else if( strncmp(segment_pointer + 4, "METADATA",8) == 0 )
            segobj = new MetadataSegment( this, segment, segment_pointer );
        else
            segobj = new CPCIDSKSegment( this, segment, segment_pointer );

        break;
    
      case SEG_BIN:
        if (!strncmp(segment_pointer + 4, "RFMODEL ", 8))
            segobj = new CPCIDSKRPCModelSegment( this, segment, segment_pointer );
        break;
        
    }
    
    if (segobj == NULL)
        segobj = new CPCIDSKSegment( this, segment, segment_pointer );
        
    segments[segment] = segobj;

    return segobj;
}

/************************************************************************/
/*                             GetSegment()                             */
/*                                                                      */
/*      Find segment by type/name.                                      */
/************************************************************************/

PCIDSK::PCIDSKSegment *CPCIDSKFile::GetSegment( int type, std::string name, 
                                                int previous )

{
    int  i;
    char type_str[4];

    name += "        "; // white space pad name.

    sprintf( type_str, "%03d", type );

    for( i = previous; i < segment_count; i++ )
    {
        if( type != SEG_UNKNOWN 
            && strncmp(segment_pointers.buffer+i*32+1,type_str,3) != 0 )
            continue;

        if( name != "        " 
            && strncmp(segment_pointers.buffer+i*32+4,name.c_str(),8) != 0 )
            continue;

        return GetSegment(i+1);
    }

    return NULL;
}


/************************************************************************/
/*                            GetSegments()                             */
/************************************************************************/

std::vector<PCIDSK::PCIDSKSegment *> CPCIDSKFile::GetSegments()

{
    PCIDSK::ThrowPCIDSKException( "Objects list access not implemented yet." );

    std::vector<PCIDSK::PCIDSKSegment *> list;
    return list;
}

/************************************************************************/
/*                        InitializeFromHeader()                        */
/************************************************************************/

void CPCIDSKFile::InitializeFromHeader()

{
/* -------------------------------------------------------------------- */
/*      Process the file header.                                        */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer fh(512);

    ReadFromFile( fh.buffer, 0, 512 );

    width = atoi(fh.Get(384,8));
    height = atoi(fh.Get(392,8));
    channel_count = atoi(fh.Get(376,8));
    file_size = fh.GetUInt64(16,16);

    uint64 ih_start_block = atouint64(fh.Get(336,16));
    uint64 image_start_block = atouint64(fh.Get(304,16));
    fh.Get(360,8,interleaving);

    uint64 image_offset = (image_start_block-1) * 512;

    block_size = 0;
    last_block_index = -1;
    last_block_dirty = 0;
    last_block_data = NULL;
    last_block_mutex = NULL;

/* -------------------------------------------------------------------- */
/*      Load the segment pointers into a PCIDSKBuffer.  For now we      */
/*      try to avoid doing too much other processing on them.           */
/* -------------------------------------------------------------------- */
    int segment_block_count = atoi(fh.Get(456,8));
    
    segment_count = (segment_block_count * 512) / 32;
    segment_pointers.SetSize( segment_block_count * 512 );
    segment_pointers_offset = atouint64(fh.Get(440,16)) * 512 - 512;
    ReadFromFile( segment_pointers.buffer, segment_pointers_offset,
                  segment_block_count * 512 );

    segments.resize( segment_count + 1 );

/* -------------------------------------------------------------------- */
/*      Get the number of each channel type - only used for some        */
/*      interleaving cases.                                             */
/* -------------------------------------------------------------------- */
    int count_8u = atoi(fh.Get(464,4));
    int count_16s = atoi(fh.Get(468,4));
    int count_16u = atoi(fh.Get(472,4));
    int count_32r = atoi(fh.Get(476,4));

/* -------------------------------------------------------------------- */
/*      for pixel interleaved files we need to compute the length of    */
/*      a scanline padded out to a 512 byte boundary.                   */
/* -------------------------------------------------------------------- */
    if( interleaving == "PIXEL" )
    {
        first_line_offset = image_offset;
        pixel_group_size = count_8u + count_16s*2 + count_16u*2 + count_32r*4;
        
        block_size = pixel_group_size * width;
        if( block_size % 512 != 0 )
            block_size += 512 - (block_size % 512);

        last_block_data = malloc((size_t) block_size);
        if( last_block_data == NULL )
            ThrowPCIDSKException( "Allocating %d bytes for scanline buffer failed.", 
                                       (int) block_size );

        last_block_mutex = interfaces.CreateMutex();
        image_offset = 0;
    }

/* -------------------------------------------------------------------- */
/*      Initialize the list of channels.                                */
/* -------------------------------------------------------------------- */
    int channelnum;

    for( channelnum = 1; channelnum <= channel_count; channelnum++ )
    {
        PCIDSKBuffer ih(1024);
        PCIDSKChannel *channel = NULL;
        
        ReadFromFile( ih.buffer, 
                      (ih_start_block-1)*512 + (channelnum-1)*1024, 
                      1024);

        // fetch the filename, if there is one.
        std::string filename;
        ih.Get(64,64,filename);

        // work out channel type from header
        eChanType pixel_type;
        const char *pixel_type_string = ih.Get( 160, 8 );
    
        if( strcmp(pixel_type_string,"8U      ") == 0 )
            pixel_type = CHN_8U;
        else if( strcmp(pixel_type_string,"16S     ") == 0 )
            pixel_type = CHN_16S;
        else if( strcmp(pixel_type_string,"16U     ") == 0 )
            pixel_type = CHN_16U;
        else if( strcmp(pixel_type_string,"32R     ") == 0 )
            pixel_type = CHN_32R;
        else
            pixel_type = CHN_UNKNOWN; // should we throw an exception?  

        // if we didn't get channel type in header, work out from counts (old)

        if( channelnum <= count_8u )
            pixel_type = CHN_8U;
        else if( channelnum <= count_8u + count_16s )
            pixel_type = CHN_16S;
        else if( channelnum <= count_8u + count_16s + count_16u )
            pixel_type = CHN_16U;
        else 
            pixel_type = CHN_32R;
            
        if( interleaving == "BAND" )
        {
            channel = new CBandInterleavedChannel( ih, fh, channelnum, this,
                                                   image_offset, pixel_type );

            
            image_offset += DataTypeSize(channel->GetType())
                * width * height;
        }

        else if( interleaving == "PIXEL" )
        {
            channel = new CPixelInterleavedChannel( ih, fh, channelnum, this,
                                                    (int) image_offset, 
                                                    pixel_type );
            image_offset += DataTypeSize(pixel_type);
        }

        else if( interleaving == "FILE" 
                 && strncmp(filename.c_str(),"/SIS=",5) == 0 )
        {
            channel = new CTiledChannel( ih, fh, channelnum, this, pixel_type );
        }

        else if( interleaving == "FILE" )
        {
            channel = new CBandInterleavedChannel( ih, fh, channelnum, this,
                                                   0, pixel_type );
        }

        else
            ThrowPCIDSKException( "Unsupported interleaving:%s", 
                                       interleaving.c_str() );

        channels.push_back( channel );
    }
}

/************************************************************************/
/*                            ReadFromFile()                            */
/************************************************************************/

void CPCIDSKFile::ReadFromFile( void *buffer, uint64 offset, uint64 size )

{
    MutexHolder oHolder( io_mutex );

    interfaces.io->Seek( io_handle, offset, SEEK_SET );
    if( interfaces.io->Read( buffer, 1, size, io_handle ) != size )
        ThrowPCIDSKException( "PCIDSKFile:Failed to read %d bytes at %d.", 
                                   (int) size, (int) offset );
}

/************************************************************************/
/*                            WriteToFile()                             */
/************************************************************************/

void CPCIDSKFile::WriteToFile( const void *buffer, uint64 offset, uint64 size )

{
    if( !GetUpdatable() )
        throw PCIDSKException( "File not open for update in WriteToFile()" );

    MutexHolder oHolder( io_mutex );

    interfaces.io->Seek( io_handle, offset, SEEK_SET );
    if( interfaces.io->Write( buffer, 1, size, io_handle ) != size )
        ThrowPCIDSKException( "PCIDSKFile:Failed to write %d bytes at %d.",
                                   (int) size, (int) offset );
}

/************************************************************************/
/*                          ReadAndLockBlock()                          */
/************************************************************************/

void *CPCIDSKFile::ReadAndLockBlock( int block_index, 
                                     int win_xoff, int win_xsize )

{
    if( last_block_data == NULL )
        ThrowPCIDSKException( "ReadAndLockBlock() called on a file that is not pixel interleaved." );

/* -------------------------------------------------------------------- */
/*      Default, and validate windowing.                                */
/* -------------------------------------------------------------------- */
    if( win_xoff == -1 && win_xsize == -1 )
    {
        win_xoff = 0;
        win_xsize = GetWidth();
    }

    if( win_xoff < 0 || win_xoff+win_xsize > GetWidth() )
    {
        ThrowPCIDSKException( "CPCIDSKFile::ReadAndLockBlock(): Illegal window - xoff=%d, xsize=%d", 
                                   win_xoff, win_xsize );
    }

    if( block_index == last_block_index 
        && win_xoff == last_block_xoff
        && win_xsize == last_block_xsize )
    {
        last_block_mutex->Acquire();
        return last_block_data;
    }

/* -------------------------------------------------------------------- */
/*      Flush any dirty writable data.                                  */
/* -------------------------------------------------------------------- */
    FlushBlock();

/* -------------------------------------------------------------------- */
/*      Read the requested window.                                      */
/* -------------------------------------------------------------------- */
    last_block_mutex->Acquire();

    ReadFromFile( last_block_data, 
                  first_line_offset + block_index*block_size
                  + win_xoff * pixel_group_size,
                  pixel_group_size * win_xsize );
    last_block_index = block_index;
    last_block_xoff = win_xoff;
    last_block_xsize = win_xsize;
    
    return last_block_data;
}

/************************************************************************/
/*                            UnlockBlock()                             */
/************************************************************************/

void CPCIDSKFile::UnlockBlock( bool mark_dirty )

{
    if( last_block_mutex == NULL )
        return;

    last_block_dirty |= mark_dirty;
    last_block_mutex->Release();
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

void CPCIDSKFile::WriteBlock( int block_index, void *buffer )

{
    if( !GetUpdatable() )
        throw PCIDSKException( "File not open for update in WriteBlock()" );

    if( last_block_data == NULL )
        ThrowPCIDSKException( "WriteBlock() called on a file that is not pixel interleaved." );

    WriteToFile( buffer,
                 first_line_offset + block_index*block_size,
                 block_size );
}

/************************************************************************/
/*                             FlushBlock()                             */
/************************************************************************/

void CPCIDSKFile::FlushBlock()

{
    if( last_block_dirty ) 
    {
        last_block_mutex->Acquire();
        if( last_block_dirty ) // is it still dirty?
        {
            WriteBlock( last_block_index, last_block_data );
            last_block_dirty = 0;
        }
        last_block_mutex->Release();
    }
}

/************************************************************************/
/*                            GetIODetails()                            */
/************************************************************************/

void CPCIDSKFile::GetIODetails( void ***io_handle_pp, 
                                Mutex ***io_mutex_pp, 
                                std::string filename )

{
    *io_handle_pp = NULL;
    *io_mutex_pp = NULL;

/* -------------------------------------------------------------------- */
/*      Does this reference the PCIDSK file itself?                     */
/* -------------------------------------------------------------------- */
    if( filename.size() == 0 )
    {
        *io_handle_pp = &io_handle;
        *io_mutex_pp = &io_mutex;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Does the file exist already in our file list?                   */
/* -------------------------------------------------------------------- */
    unsigned int i;

    for( i = 0; i < file_list.size(); i++ )
    {
        if( file_list[i].filename == filename )
        {
            *io_handle_pp = &(file_list[i].io_handle);
            *io_mutex_pp = &(file_list[i].io_mutex);
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      If not, we need to try and open the file.  Eventually we        */
/*      will need better rules about read or update access.             */
/* -------------------------------------------------------------------- */
    ProtectedFile new_file;
    
    new_file.io_handle = interfaces.io->Open( filename, "r" );
    if( new_file.io_handle == NULL )
        ThrowPCIDSKException( "Unable to open file '%s'.", 
                              filename.c_str() );

/* -------------------------------------------------------------------- */
/*      Push the new file into the list of files managed for this       */
/*      PCIDSK file.                                                    */
/* -------------------------------------------------------------------- */
    new_file.io_mutex = interfaces.CreateMutex();
    new_file.filename = filename;

    file_list.push_back( new_file );

    *io_handle_pp = &(file_list[file_list.size()-1].io_handle);
    *io_mutex_pp  = &(file_list[file_list.size()-1].io_mutex);
}

/************************************************************************/
/*                           DeleteSegment()                            */
/************************************************************************/

void CPCIDSKFile::DeleteSegment( int segment )

{
/* -------------------------------------------------------------------- */
/*      Is this an existing segment?                                    */
/* -------------------------------------------------------------------- */
    PCIDSKSegment *poSeg = GetSegment( segment );

    if( poSeg == NULL )
        ThrowPCIDSKException( "DeleteSegment(%d) failed, segment does not exist.", segment );

/* -------------------------------------------------------------------- */
/*      Wipe associated metadata.                                       */
/* -------------------------------------------------------------------- */
    std::vector<std::string> md_keys = poSeg->GetMetadataKeys();
    unsigned int i;

    for( i = 0; i < md_keys.size(); i++ )
        poSeg->SetMetadataValue( md_keys[i], "" );

/* -------------------------------------------------------------------- */
/*      Remove the segment object from the segment object cache.  I     */
/*      hope the application is not retaining any references to this    */
/*      segment!                                                        */
/* -------------------------------------------------------------------- */
    segments[segment] = NULL;
    delete poSeg;

/* -------------------------------------------------------------------- */
/*      Mark the segment pointer as deleted.                            */
/* -------------------------------------------------------------------- */
    segment_pointers.buffer[(segment-1)*32] = 'D';

    // write the updated segment pointer back to the file. 
    WriteToFile( segment_pointers.buffer + (segment-1)*32, 
                 segment_pointers_offset + (segment-1)*32, 
                 32 );
}

/************************************************************************/
/*                           CreateSegment()                            */
/************************************************************************/

int CPCIDSKFile::CreateSegment( std::string name, std::string description,
                                eSegType seg_type, int data_blocks )

{
/* -------------------------------------------------------------------- */
/*	Set the size of fixed length segments.				*/
/* -------------------------------------------------------------------- */
    int expected_data_blocks = 0;

    switch( seg_type )
    {
      case SEG_LUT:
	expected_data_blocks = 2;
	break;

      case SEG_PCT:
	expected_data_blocks = 6;
	break;

      case SEG_SIG:
	expected_data_blocks = 12;
	break;

      case SEG_GCP2:
	// expected_data_blocks = 67;
	// Change seg type to new GCP segment type
	expected_data_blocks = 129;
	break;
	
      case SEG_GEO:
	expected_data_blocks = 6;
	break;

/* -------------------------------------------------------------------- */
/*      We do some complicated stuff here to avoid exceeding the        */
/*      largest number representable in a int32 (2GB).                  */
/* -------------------------------------------------------------------- */
#ifdef notdef
      case SEG_BIT:
        {
            int	  nBlocksPerScanline, nExtraPixels;
            int   nBlocksPerPixel, nExtraScanlines;
            
            nExtraScanlines = IDB->lines % 4096;
            nBlocksPerPixel = IDB->lines / 4096;
            
            nExtraPixels = IDB->pixels % 4096;
            nBlocksPerScanline = IDB->pixels / 4096;
            
            nBlocks = (nExtraPixels * nExtraScanlines + 4095) / 4096
                + nBlocksPerScanline * IDB->lines
                + nBlocksPerPixel * nExtraPixels
                + 2;

	    if ((double)IDB->pixels * (double)IDB->lines/8.0 > (double)512*(double)2147483647)
		IMPError( 68, ERRTYP_UFATAL,
                      MkName(NLSLookup("@CantCreatePCIDSKWithBMPlarger1024GcurrentBMP_NUM_NUM_NUM_:Cannot "
		      "create PCIDSK with a bitmap larger than 1024GB in size.\n"
                      "The bitmap is %dp x %dl~= %6.1fMB.\n"),
                      IDB->pixels, IDB->lines , 
                      IDB->pixels * (double) IDB->lines
                      / 1000000.0 ));
        }
        break

      case SEG_TEX:
        if( nBlocks < 66 )
            nBlocks = 66;
        break;
#endif

      default:
        break;
    }

    if( data_blocks == 0 && expected_data_blocks != 0 )
        data_blocks = expected_data_blocks;

/* -------------------------------------------------------------------- */
/*      Find an empty Segment Pointer.  For System segments we start    */
/*      at the end, instead of the beginning to avoid using up          */
/*      segment numbers that the user would notice.                     */
/* -------------------------------------------------------------------- */
    int segment = 1;
    int64 seg_start = -1;
    PCIDSKBuffer segptr( 32 );

    if( seg_type == SEG_SYS )
    {
        for( segment=segment_count; segment >= 1; segment-- )
        {
            memcpy( segptr.buffer, segment_pointers.buffer+(segment-1)*32, 32);

            uint64 this_seg_size = segptr.GetUInt64(23,9);
            char flag = (char) segptr.buffer[0];

            if( flag == 'D' 
                && (uint64) data_blocks+2 == this_seg_size 
                && this_seg_size > 0 )
                seg_start = segptr.GetUInt64(12,11) - 1;
            else if( flag == ' ' )
                seg_start = 0;
            else if( flag && this_seg_size == 0 )
                seg_start = 0;

            if( seg_start != -1 )
                break;
        }
    }
    else
    {
        for( segment=1; segment <= segment_count; segment++ )
        {
            memcpy( segptr.buffer, segment_pointers.buffer+(segment-1)*32, 32);

            uint64 this_seg_size = segptr.GetUInt64(23,9);
            char flag = (char) segptr.buffer[0];

            if( flag == 'D' 
                && (uint64) data_blocks+2 == this_seg_size 
                && this_seg_size > 0 )
                seg_start = segptr.GetUInt64(12,11) - 1;
            else if( flag == ' ' )
                seg_start = 0;
            else if( flag && this_seg_size == 0 )
                seg_start = 0;

            if( seg_start != -1 )
                break;
        }
    }
    
    if( segment > segment_count )
        ThrowPCIDSKException( "All %d segment pointers in use.", segment_count);

/* -------------------------------------------------------------------- */
/*      If the segment does not have a data area already, identify      */
/*      it's location at the end of the file, and extend the file to    */
/*      the desired length.                                             */
/* -------------------------------------------------------------------- */
    if( seg_start == 0 )
    {
        seg_start = GetFileSize();
        ExtendFile( data_blocks + 2 );
    }

/* -------------------------------------------------------------------- */
/*      Update the segment pointer information.                         */
/* -------------------------------------------------------------------- */
    // SP1.1 - Flag
    segptr.Put( "A", 0, 1 );

    // SP1.2 - Type
    segptr.Put( (int) seg_type, 1, 3 );

    // SP1.3 - Name
    segptr.Put( name.c_str(), 4, 8 );
    
    // SP1.4 - start block
    segptr.Put( (uint64) (seg_start + 1), 12, 11 );
    
    // SP1.5 - data blocks.
    segptr.Put( data_blocks+2, 23, 9 );

    // Update in memory copy of segment pointers.
    memcpy( segment_pointers.buffer+(segment-1)*32, segptr.buffer, 32);

    // Update on disk. 
    WriteToFile( segptr.buffer, 
                 segment_pointers_offset + (segment-1)*32, 32 );
    
/* -------------------------------------------------------------------- */
/*      Prepare segment header.                                         */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer sh(1024);

    char current_time[17];
    GetCurrentDateTime( current_time );

    sh.Put( " ", 0, 1024 );

    // SH1 - segment content description
    sh.Put( description.c_str(), 0, 64 );

    // SH3 - Creation time/date
    sh.Put( current_time, 128, 16 );

    // SH4 - Last Update time/date
    sh.Put( current_time, 144, 16 );

/* -------------------------------------------------------------------- */
/*      Write segment header.                                           */
/* -------------------------------------------------------------------- */
    WriteToFile( sh.buffer, seg_start * 512, 1024 );

    return segment;
}

/************************************************************************/
/*                             ExtendFile()                             */
/************************************************************************/

void CPCIDSKFile::ExtendFile( uint64 blocks_requested, bool prezero )

{
    if( prezero )
    {
        std::vector<uint8> zeros;
        uint64 blocks_to_zero = blocks_requested;
        
        zeros.resize( 512 * 32 );
        
        while( blocks_to_zero > 0 )
        {
            uint64 this_time = blocks_to_zero;
            if( this_time > 32 )
                this_time = 32;

            WriteToFile( &(zeros[0]), file_size * 512, this_time*512 );
            blocks_to_zero -= this_time;
            file_size += this_time;
        }
    }
    else
    {
        WriteToFile( "\0", (file_size + blocks_requested) * 512 - 1, 1 );
        file_size += blocks_requested;
    }

    PCIDSKBuffer fh3( 16 );
    fh3.Put( file_size, 0, 16 );
    WriteToFile( fh3.buffer, 16, 16 );
}

/************************************************************************/
/*                           ExtendSegment()                            */
/************************************************************************/

void CPCIDSKFile::ExtendSegment( int segment, uint64 blocks_requested,
                                 bool prezero )

{
    // for now we take it for granted that the segment is valid and at th
    // end of the file - later we should support moving it. 

    ExtendFile( blocks_requested, prezero );

    // Update the block count. 
    segment_pointers.Put( 
        segment_pointers.GetUInt64((segment-1)*32+23,9) + blocks_requested,
        (segment-1)*32+23, 9 );

    // write the updated segment pointer back to the file. 
    WriteToFile( segment_pointers.buffer + (segment-1)*32, 
                 segment_pointers_offset + (segment-1)*32, 
                 32 );
}

/************************************************************************/
/*                          MoveSegmentToEOF()                          */
/************************************************************************/

void CPCIDSKFile::MoveSegmentToEOF( int segment )

{
    int segptr_off = (segment - 1) * 32;
    uint64 seg_start, seg_size;
    uint64 new_seg_start;

    seg_start = segment_pointers.GetUInt64( segptr_off + 12, 11 );
    seg_size = segment_pointers.GetUInt64( segptr_off + 23, 9 );

    // Are we already at the end of the file?
    if( (seg_start + seg_size - 1) == file_size )
        return;

    new_seg_start = file_size + 1;

    // Grow the file to hold the segment at the end.
    ExtendFile( seg_size, false );

    // Move the segment data to the new location.
    uint8 copy_buf[16384];
    uint64 srcoff, dstoff, bytes_to_go;

    bytes_to_go = seg_size * 512;
    srcoff = (seg_start - 1) * 512;
    dstoff = (new_seg_start - 1) * 512;

    while( bytes_to_go > 0 )
    {
        uint64 bytes_this_chunk = sizeof(copy_buf);
        if( bytes_to_go < bytes_this_chunk )
            bytes_this_chunk = bytes_to_go;

        ReadFromFile( copy_buf, srcoff, bytes_this_chunk );
        WriteToFile( copy_buf, dstoff, bytes_this_chunk );

        srcoff += bytes_this_chunk;
        dstoff += bytes_this_chunk;
        bytes_to_go -= bytes_this_chunk;
    }

    // Update segment pointer in memory and on disk. 
    segment_pointers.Put( new_seg_start, segptr_off + 12, 11 );

    WriteToFile( segment_pointers.buffer + segptr_off, 
                 segment_pointers_offset + segptr_off, 
                 32 );
    
    // Update the segments own information.
    if( segments[segment] != NULL )
    {
        CPCIDSKSegment *seg = 
            dynamic_cast<CPCIDSKSegment *>( segments[segment] );

        seg->LoadSegmentPointer( segment_pointers.buffer + segptr_off );
    }
}

/************************************************************************/
/*                          CreateOverviews()                           */
/************************************************************************/
/*
 const char *pszResampling;
 	     Either "NEAREST" for Nearest Neighbour resampling (the fastest),
             "AVERAGE" for block averaging or "MODE" for block mode.  This
             establishing the type of resampling to be applied when preparing
             the decimated overviews.  
*/

void CPCIDSKFile::CreateOverviews( int chan_count, int *chan_list, 
                                   int factor, std::string resampling )

{
    std::vector<int> default_chan_list;

/* -------------------------------------------------------------------- */
/*      Validate resampling method.                                     */
/* -------------------------------------------------------------------- */
    UCaseStr( resampling );

    if( resampling != "NEAREST" 
        && resampling != "AVERAGE"
        && resampling != "MODE" )
    {
        ThrowPCIDSKException( "Requested overview resampling '%s' not supported.\nUse one of NEAREST, AVERAGE or MODE.",
                              resampling.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Default to processing all bands.                                */
/* -------------------------------------------------------------------- */
    if( chan_count == 0 )
    {
        chan_count = channel_count;
        default_chan_list.resize( chan_count );

        for( int i = 0; i < chan_count; i++ )
            default_chan_list[i] = i+1;

        chan_list = &(default_chan_list[0]);
    }

/* -------------------------------------------------------------------- */
/*      Work out the creation options that should apply for the         */
/*      overview.                                                       */
/* -------------------------------------------------------------------- */
    std::string layout = GetMetadataValue( "_DBLayout" );
    int         blocksize = 127;
    std::string compression = "NONE";

    if( strncmp( layout.c_str(), "TILED", 5 ) == 0 )
    {
        ParseTileFormat( layout, blocksize, compression );
    }

/* -------------------------------------------------------------------- */
/*      Make sure we have a blockmap segment for managing the tiled     */
/*      layers.                                                         */
/* -------------------------------------------------------------------- */
    PCIDSKSegment *bm_seg = GetSegment( SEG_SYS, "SysBMDir" );
    SysBlockMap *bm;

    if( bm_seg == NULL )
    {
        CreateSegment( "SysBMDir", 
                       "System Block Map Directory - Do not modify.",
                       SEG_SYS, 0 );
        bm_seg = GetSegment( SEG_SYS, "SysBMDir" );
        bm = dynamic_cast<SysBlockMap *>(bm_seg);
        bm->Initialize();
    }
    else
        bm = dynamic_cast<SysBlockMap *>(bm_seg);
        
/* ==================================================================== */
/*      Loop over the channels.                                         */
/* ==================================================================== */
    for( int chan_index = 0; chan_index < chan_count; chan_index++ )
    {
        int channel_number = chan_list[chan_index];
        PCIDSKChannel *channel = GetChannel( channel_number );
        
/* -------------------------------------------------------------------- */
/*      Do we have a preexisting overview that corresponds to this      */
/*      factor?  If so, throw an exception.  Would it be better to      */
/*      just return quietly?                                            */
/* -------------------------------------------------------------------- */
        for( int i = channel->GetOverviewCount()-1; i >= 0; i-- )
        {
            PCIDSKChannel *overview = channel->GetOverview( i );
 
            if( overview->GetWidth() == channel->GetWidth() / factor
                && overview->GetHeight() == channel->GetHeight() / factor )
            {
                ThrowPCIDSKException( "Channel %d already has a factor %d overview.",
                                      channel_number, factor );
            }
        }

/* -------------------------------------------------------------------- */
/*      Create the overview as a tiled image layer.                     */
/* -------------------------------------------------------------------- */
        int virtual_image = 
            bm->CreateVirtualImageFile( channel->GetWidth() / factor, 
                                        channel->GetHeight() / factor,
                                        blocksize, blocksize, 
                                        channel->GetType(), compression );

/* -------------------------------------------------------------------- */
/*      Attach reference to this overview as metadata.                  */
/* -------------------------------------------------------------------- */
        char overview_md_value[128];
        char overview_md_key[128];

        sprintf( overview_md_key, "_Overview_%d", factor );
        sprintf( overview_md_value, "%d 0 %s",virtual_image,resampling.c_str());
                 
        channel->SetMetadataValue( overview_md_key, overview_md_value );

/* -------------------------------------------------------------------- */
/*      Force channel to invalidate it's loaded overview list.          */
/* -------------------------------------------------------------------- */
        dynamic_cast<CPCIDSKChannel *>(channel)->InvalidateOverviewInfo();
    }
}

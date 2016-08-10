/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKSegment class.
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

#include "segment/cpcidsksegment.h"
#include "core/metadataset.h"
#include "core/cpcidskfile.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_buffer.h"
#include "pcidsk_exception.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

using namespace PCIDSK;

/************************************************************************/
/*                           PCIDSKSegment()                            */
/************************************************************************/

CPCIDSKSegment::CPCIDSKSegment( PCIDSKFile *fileIn, int segmentIn,
                              const char *segment_pointer )

{
    this->file = fileIn;
    this->segment = segmentIn;

    LoadSegmentPointer( segment_pointer );
    LoadSegmentHeader(); // eventually we might want to defer this.

/* -------------------------------------------------------------------- */
/*      Initialize the metadata object, but do not try to load till     */
/*      needed.                                                         */
/* -------------------------------------------------------------------- */
    metadata = new MetadataSet;
    metadata->Initialize( file, SegmentTypeName(segment_type), segment );
}

/************************************************************************/
/*                           ~PCIDSKSegment()                           */
/************************************************************************/

CPCIDSKSegment::~CPCIDSKSegment()

{
    delete metadata;
}

/************************************************************************/
/*                          GetMetadataValue()                          */
/************************************************************************/
std::string CPCIDSKSegment::GetMetadataValue( const std::string &key ) const
{
    return metadata->GetMetadataValue(key);
}

/************************************************************************/
/*                          SetMetadataValue()                          */
/************************************************************************/
void CPCIDSKSegment::SetMetadataValue( const std::string &key, const std::string &value ) 
{
    metadata->SetMetadataValue(key,value);
}

/************************************************************************/
/*                           GetMetdataKeys()                           */
/************************************************************************/
std::vector<std::string> CPCIDSKSegment::GetMetadataKeys() const
{
    return metadata->GetMetadataKeys();
}

/************************************************************************/
/*                         LoadSegmentPointer()                         */
/************************************************************************/

void CPCIDSKSegment::LoadSegmentPointer( const char *segment_pointer )

{
    PCIDSKBuffer segptr( segment_pointer, 32 );

    segment_flag = segptr.buffer[0];
    segment_type = (eSegType) (atoi(segptr.Get(1,3)));
    data_offset = (atouint64(segptr.Get(12,11))-1) * 512;
    data_size = atouint64(segptr.Get(23,9)) * 512;

    segptr.Get(4,8,segment_name);
}

/************************************************************************/
/*                         LoadSegmentHeader()                          */
/************************************************************************/
#include <iostream>
void CPCIDSKSegment::LoadSegmentHeader()

{
    header.SetSize(1024);

    file->ReadFromFile( header.buffer, data_offset, 1024 );
    
    // Read the history from the segment header. PCIDSK supports
    // 8 history entries per segment.
    std::string hist_msg;
    history_.clear();
    for (unsigned int i = 0; i < 8; i++)
    {
        header.Get(384 + i * 80, 80, hist_msg);

        // Some programs seem to push history records with a trailing '\0'
        // so do some extra processing to cleanup.  FUN records on segment
        // 3 of eltoro.pix are an example of this.
        size_t size = hist_msg.size();
        while( size > 0 
               && (hist_msg[size-1] == ' ' || hist_msg[size-1] == '\0') )
            size--;

        hist_msg.resize(size);
        
        history_.push_back(hist_msg);
    }
}

/************************************************************************/
/*                            FlushHeader()                             */
/*                                                                      */
/*      This is used primarily after this class or subclasses have      */
/*      modified the header buffer and need it pushed back to disk.     */
/************************************************************************/

void CPCIDSKSegment::FlushHeader()

{
    file->WriteToFile( header.buffer, data_offset, 1024 );
}

/************************************************************************/
/*                            ReadFromFile()                            */
/************************************************************************/

void CPCIDSKSegment::ReadFromFile( void *buffer, uint64 offset, uint64 size )

{
    if( offset+size+1024 > data_size )
        return ThrowPCIDSKException( 
            "Attempt to read past end of segment %d (%u bytes at offset %u)",
            segment, (unsigned int) offset, (unsigned int) size );
    file->ReadFromFile( buffer, offset + data_offset + 1024, size );
}

/************************************************************************/
/*                            WriteToFile()                             */
/************************************************************************/

void CPCIDSKSegment::WriteToFile( const void *buffer, uint64 offset, uint64 size )
{
    if( offset+size > data_size-1024 )
    {
        CPCIDSKFile *poFile = dynamic_cast<CPCIDSKFile *>(file);
        
        if (poFile == NULL) {
            return ThrowPCIDSKException("Attempt to dynamic_cast the file interface "
                "to a CPCIDSKFile failed. This is a programmer error, and should "
                "be reported to your software provider.");
        }
        
        if( !IsAtEOF() )
            poFile->MoveSegmentToEOF( segment );

        uint64 blocks_to_add = 
            ((offset+size+511) - (data_size - 1024)) / 512;

        // prezero if we aren't directly writing all the new blocks.
        poFile->ExtendSegment( segment, blocks_to_add, 
                             !(offset == data_size - 1024
                               && size == blocks_to_add * 512) );
        data_size += blocks_to_add * 512;
    }

    file->WriteToFile( buffer, offset + data_offset + 1024, size );
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

std::string CPCIDSKSegment::GetDescription()
{
    std::string target;

    header.Get( 0, 64, target );

    return target;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void CPCIDSKSegment::SetDescription( const std::string &description )
{
    header.Put( description.c_str(), 0, 64);

    file->WriteToFile( header.buffer, data_offset, 1024 );
}

/************************************************************************/
/*                              IsAtEOF()                               */
/************************************************************************/

bool CPCIDSKSegment::IsAtEOF()
{
    if( 512 * file->GetFileSize() == data_offset + data_size )
        return true;
    else
        return false;
}

/************************************************************************/
/*                         GetHistoryEntries()                          */
/************************************************************************/

std::vector<std::string> CPCIDSKSegment::GetHistoryEntries() const
{
    return history_;
}

/************************************************************************/
/*                         SetHistoryEntries()                          */
/************************************************************************/

void CPCIDSKSegment::SetHistoryEntries(const std::vector<std::string> &entries)

{
    for( unsigned int i = 0; i < 8; i++ )
    {
        const char *msg = "";
        if( entries.size() > i )
            msg = entries[i].c_str();

        header.Put( msg, 384 + i * 80, 80 );
    }

    FlushHeader();

    // Force reloading of history_
    LoadSegmentHeader();
}

/************************************************************************/
/*                            PushHistory()                             */
/************************************************************************/

void CPCIDSKSegment::PushHistory( const std::string &app,
                                  const std::string &message )

{
#define MY_MIN(a,b)      ((a<b) ? a : b)

    char current_time[17];
    char history[81];

    GetCurrentDateTime( current_time );

    memset( history, ' ', 80 );
    history[80] = '\0';

    memcpy( history + 0, app.c_str(), MY_MIN(app.size(),7) );
    history[7] = ':';
    
    memcpy( history + 8, message.c_str(), MY_MIN(message.size(),56) );
    memcpy( history + 64, current_time, 16 );

    std::vector<std::string> history_entries = GetHistoryEntries();

    history_entries.insert( history_entries.begin(), history );
    history_entries.resize(8);

    SetHistoryEntries( history_entries );
}


/************************************************************************/
/*                              MoveData()                              */
/*                                                                      */
/*      Move a chunk of data within a segment. Overlapping source       */
/*      and destination are permitted.                                  */
/************************************************************************/

void CPCIDSKSegment::MoveData( uint64 src_offset, uint64 dst_offset, 
                               uint64 size_in_bytes )

{
    bool copy_backwards = false;

    // We move things backwards if the areas overlap and the destination
    // is further on in the segment. 
    if( dst_offset > src_offset
        && src_offset + size_in_bytes > dst_offset )
        copy_backwards = true;

    
    // Move the segment data to the new location.
    uint8 copy_buf[16384];
    uint64 bytes_to_go;

    bytes_to_go = size_in_bytes;

    while( bytes_to_go > 0 )
    {
        uint64 bytes_this_chunk = sizeof(copy_buf);
        if( bytes_to_go < bytes_this_chunk )
            bytes_this_chunk = bytes_to_go;

        if( copy_backwards )
        {
            ReadFromFile( copy_buf, 
                          src_offset + bytes_to_go - bytes_this_chunk, 
                          bytes_this_chunk );
            WriteToFile( copy_buf, 
                         dst_offset + bytes_to_go - bytes_this_chunk, 
                         bytes_this_chunk );
        }
        else
        {
            ReadFromFile( copy_buf, src_offset, bytes_this_chunk );
            WriteToFile( copy_buf, dst_offset, bytes_this_chunk );

            src_offset += bytes_this_chunk;
            dst_offset += bytes_this_chunk;
        }

        bytes_to_go -= bytes_this_chunk;
    }
}

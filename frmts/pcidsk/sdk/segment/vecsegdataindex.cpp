/******************************************************************************
 *
 * Purpose:  Implementation of the VecSegIndex class.  
 *
 * This class is used to manage a vector segment data block index.  There
 * will be two instances created, one for the record data (sec_record) and
 * one for the vertices (sec_vert).  This class is exclusively a private
 * helper class for VecSegHeader.
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

#include "pcidsk.h"
#include "core/pcidsk_utils.h"
#include "segment/cpcidskvectorsegment.h"
#include <cassert>
#include <cstring>
#include <cstdio>

using namespace PCIDSK;

/* -------------------------------------------------------------------- */
/*      Size of a block in the record/vertice block tables.  This is    */
/*      determined by the PCIDSK format and may not be changed.         */
/* -------------------------------------------------------------------- */
static const int block_page_size = 8192;  

/************************************************************************/
/*                          VecSegDataIndex()                           */
/************************************************************************/

VecSegDataIndex::VecSegDataIndex()

{
    block_initialized = false;
    vs = NULL;
    dirty = false;
}

/************************************************************************/
/*                          ~VecSegDataIndex()                          */
/************************************************************************/

VecSegDataIndex::~VecSegDataIndex()

{
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void VecSegDataIndex::Initialize( CPCIDSKVectorSegment *vs, int section )

{
    this->section = section;
    this->vs = vs;

    if( section == sec_vert )
        offset_on_disk_within_section = 0;
    else 
        offset_on_disk_within_section = vs->di[sec_vert].SerializedSize();

    uint32 offset = offset_on_disk_within_section 
        + vs->vh.section_offsets[hsec_shape];

    memcpy( &block_count, vs->GetData(sec_raw,offset,NULL,4), 4);
    memcpy( &bytes, vs->GetData(sec_raw,offset+4,NULL,4), 4);

    bool needs_swap = !BigEndianSystem();

    if( needs_swap )
    {
        SwapData( &block_count, 4, 1 );
        SwapData( &bytes, 4, 1 );
    }

    size_on_disk = block_count * 4 + 8;
}

/************************************************************************/
/*                           SerializedSize()                           */
/************************************************************************/

uint32 VecSegDataIndex::SerializedSize()

{
    return 8 + 4 * block_count;
}

/************************************************************************/
/*                           GetBlockIndex()                            */
/************************************************************************/

const std::vector<uint32> *VecSegDataIndex::GetIndex()

{
/* -------------------------------------------------------------------- */
/*      Load block map if needed.                                       */
/* -------------------------------------------------------------------- */
    if( !block_initialized )
    {
        block_index.resize( block_count );
        vs->ReadFromFile( &(block_index[0]), 
                          offset_on_disk_within_section
                          + vs->vh.section_offsets[hsec_shape] + 8, 
                          4 * block_count );

        bool needs_swap = !BigEndianSystem();

        if( needs_swap )
            SwapData( &(block_index[0]), 4, block_count );

        block_initialized = true;
    }

    return &block_index;
}
                             
/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

void VecSegDataIndex::Flush()

{
    if( !dirty )
        return;

    GetIndex(); // force loading if not already loaded!

    PCIDSKBuffer wbuf( SerializedSize() );

    memcpy( wbuf.buffer + 0, &block_count, 4 );
    memcpy( wbuf.buffer + 4, &bytes, 4 );
    memcpy( wbuf.buffer + 8, &(block_index[0]), 4*block_count );

    bool needs_swap = !BigEndianSystem();

    if( needs_swap )
        SwapData( wbuf.buffer, 4, block_count+2 );

    // Make sure this section of the header is large enough.
    int32 shift = (int32) wbuf.buffer_size - (int32) size_on_disk;
    
    if( shift != 0 )
    {
        uint32 old_section_size = vs->vh.section_sizes[hsec_shape];

//        fprintf( stderr, "Shifting section %d by %d bytes.\n",
//                 section, shift );

        vs->vh.GrowSection( hsec_shape, old_section_size + shift );

        if( section == sec_vert )
        {
            // move record block index and shape index.
            vs->MoveData( vs->vh.section_offsets[hsec_shape]
                          + vs->di[sec_vert].size_on_disk,
                          vs->vh.section_offsets[hsec_shape]
                          + vs->di[sec_vert].size_on_disk + shift,
                          old_section_size - size_on_disk );
        }
        else
        {
            // only move shape index.
            vs->MoveData( vs->vh.section_offsets[hsec_shape]
                          + vs->di[sec_vert].size_on_disk
                          + vs->di[sec_record].size_on_disk,
                          vs->vh.section_offsets[hsec_shape]
                          + vs->di[sec_vert].size_on_disk
                          + vs->di[sec_record].size_on_disk 
                          + shift,
                          old_section_size 
                          - vs->di[sec_vert].size_on_disk
                          - vs->di[sec_record].size_on_disk );
        }

        if( section == sec_vert )
            vs->di[sec_record].offset_on_disk_within_section += shift;
    }

    // Actually write to disk.
    vs->WriteToFile( wbuf.buffer, 
                     offset_on_disk_within_section 
                     + vs->vh.section_offsets[hsec_shape], 
                     wbuf.buffer_size );

    size_on_disk = wbuf.buffer_size;
    dirty = false;
}

/************************************************************************/
/*                           GetSectionEnd()                            */
/************************************************************************/

uint32 VecSegDataIndex::GetSectionEnd()

{
    return bytes;
}

/************************************************************************/
/*                           SetSectionEnd()                            */
/************************************************************************/

void VecSegDataIndex::SetSectionEnd( uint32 new_end )

{
    // should we keep track of the need to write this back to disk?
    bytes = new_end;
}

/************************************************************************/
/*                          AddBlockToIndex()                           */
/************************************************************************/

void VecSegDataIndex::AddBlockToIndex( uint32 block )

{
    GetIndex(); // force loading.
        
    block_index.push_back( block );
    block_count++;
    dirty = true;
}

/************************************************************************/
/*                              SetDirty()                              */
/*                                                                      */
/*      This method is primarily used to mark the need to write the     */
/*      index when the location changes.                                */
/************************************************************************/

void VecSegDataIndex::SetDirty()

{
    dirty = true;
}

/************************************************************************/
/*                          VacateBlockRange()                          */
/*                                                                      */
/*      Move any blocks in the indicated block range to the end of      */
/*      the segment to make space for a growing header.                 */
/************************************************************************/

void VecSegDataIndex::VacateBlockRange( uint32 start, uint32 count )

{
    GetIndex(); // make sure loaded.

    unsigned int i;
    uint32  next_block = vs->GetContentSize() / block_page_size;

    for( i = 0; i < block_count; i++ )
    {
        if( block_index[i] >= start && block_index[i] < start+count )
        {
            vs->MoveData( block_index[i] * block_page_size,
                          next_block * block_page_size, 
                          block_page_size );
            block_index[i] = next_block;
            dirty = true;
            next_block++;
        }
    }
}

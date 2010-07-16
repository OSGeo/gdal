/******************************************************************************
 *
 * Purpose:  Implementation of the VecSegHeader class.
 * 
 * This class is used to manage reading and writing of the vector segment 
 * header section, growing them as needed.  It is exclusively a private
 * helper class for the CPCIDSKVectorSegment.
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
#include "segment/vecsegheader.h"
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
/*                            VecSegHeader()                            */
/************************************************************************/

VecSegHeader::VecSegHeader()

{
    vs = NULL;
    initialized = false;
    needs_swap = !BigEndianSystem();
}

/************************************************************************/
/*                           ~VecSegHeader()                            */
/************************************************************************/

VecSegHeader::~VecSegHeader()

{
}

/************************************************************************/
/*                           InitializeNew()                            */
/*                                                                      */
/*      Initialize the header of a new vector segment in a              */
/*      consistent state for an empty segment.                          */
/************************************************************************/

void VecSegHeader::InitializeNew()

{
    PCIDSKBuffer header( 8 * 1024 );
    uint32   ivalue, hoffset;

    memset( header.buffer, 0, header.buffer_size );

    // magic cookie
    ivalue = 0xffffffff;
    memcpy( header.buffer + 0, &ivalue, 4 );
    memcpy( header.buffer + 4, &ivalue, 4 );

    ivalue = 21;
    memcpy( header.buffer + 8, &ivalue, 4 );
    ivalue = 4;
    memcpy( header.buffer + 12, &ivalue, 4 );
    ivalue = 19;
    memcpy( header.buffer + 16, &ivalue, 4 );
    ivalue = 69;
    memcpy( header.buffer + 20, &ivalue, 4 );
    ivalue = 1;
    memcpy( header.buffer + 24, &ivalue, 4 );

    // blocks in header.
    ivalue = 1;
    memcpy( header.buffer + 68, &ivalue, 4 );

    // offset to Projection
    hoffset = 88;
    memcpy( header.buffer + 72, &hoffset, 4 );

    // Project segment
    double dvalue;
    dvalue = 0.0;
    memcpy( header.buffer + hoffset, &dvalue, 8 );
    memcpy( header.buffer + hoffset+8, &dvalue, 8 );
    dvalue = 1.0;
    memcpy( header.buffer + hoffset+16, &dvalue, 8 );
    memcpy( header.buffer + hoffset+24, &dvalue, 8 );
    if( needs_swap )
        SwapData( header.buffer + hoffset, 8, 4 );
    hoffset += 33;

    // offset to RST
    memcpy( header.buffer + 76, &hoffset, 4 );

    // RST - two zeros means no rst + empty string.
    hoffset += 9;

    // offset to Records
    memcpy( header.buffer + 80, &hoffset, 4 );

    // Records - zeros means no fields.
    hoffset += 4;

    // offset to Shapes
    memcpy( header.buffer + 84, &hoffset, 4 );

    // Shapes - zero means no shapes.
    hoffset += 4;

    if( needs_swap )
        SwapData( header.buffer, 4, 22 );

    vs->WriteToFile( header.buffer, 0, header.buffer_size );
}

/************************************************************************/
/*                         InitializeExisting()                         */
/*                                                                      */
/*      Establish the location and sizes of the various header          */
/*      sections.                                                       */
/************************************************************************/

void VecSegHeader::InitializeExisting()

{
    if( initialized )
        return;

    initialized = true;

/* -------------------------------------------------------------------- */
/*      Check fixed portion of the header to ensure this is a V6        */
/*      style vector segment.                                           */
/* -------------------------------------------------------------------- */
    static const unsigned char magic[24] = 
        { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
          0, 0, 0, 21, 0, 0, 0, 4, 0, 0, 0, 19, 0, 0, 0, 69 };

    if( memcmp( vs->GetData( sec_raw, 0, NULL, 24 ), magic, 24 ) != 0 )
    {
        ThrowPCIDSKException( "Unexpected vector header values, possibly it is not a V6 vector segment?" );
    }
    
/* -------------------------------------------------------------------- */
/*      Establish how big the header is currently.                      */
/* -------------------------------------------------------------------- */
    memcpy( &header_blocks, vs->GetData( sec_raw, 68, NULL, 4 ), 4 );
    if( needs_swap )
        SwapData( &header_blocks, 4, 1 );

/* -------------------------------------------------------------------- */
/*      Load section offsets.                                           */
/* -------------------------------------------------------------------- */
    memcpy( section_offsets, vs->GetData( sec_raw, 72, NULL, 16 ), 16 );
    if( needs_swap )
        SwapData( section_offsets, 4, 4 );

/* -------------------------------------------------------------------- */
/*      Determine the size of the projection section.                   */
/* -------------------------------------------------------------------- */
    ShapeField work_value;
    uint32 next_off = section_offsets[hsec_proj];
    
    next_off += 32; // xoff/yoff/xsize/ysize values.

    next_off = vs->ReadField( next_off, work_value, FieldTypeString, sec_raw );
    section_sizes[hsec_proj] = next_off - section_offsets[hsec_proj];

/* -------------------------------------------------------------------- */
/*      Determine the size of the RST.                                  */
/* -------------------------------------------------------------------- */
    // yikes, not too sure!  for now assume it is empty.
    section_sizes[hsec_rst] = 8;

/* -------------------------------------------------------------------- */
/*      Load the field definitions.                                     */
/* -------------------------------------------------------------------- */
    int  field_count, i;

    next_off = section_offsets[hsec_record];
    
    next_off = vs->ReadField( next_off, work_value, FieldTypeInteger, sec_raw );
    field_count = work_value.GetValueInteger();

    for( i = 0; i < field_count; i++ )
    {
        next_off = vs->ReadField( next_off, work_value, FieldTypeString, sec_raw );
        field_names.push_back( work_value.GetValueString() );
        
        next_off = vs->ReadField( next_off, work_value, FieldTypeString, sec_raw );
        field_descriptions.push_back( work_value.GetValueString() );
        
        next_off = vs->ReadField( next_off, work_value, FieldTypeInteger, sec_raw );
        field_types.push_back( (ShapeFieldType) work_value.GetValueInteger() );
        
        next_off = vs->ReadField( next_off, work_value, FieldTypeString, sec_raw );
        field_formats.push_back( work_value.GetValueString() );
        
        next_off = vs->ReadField( next_off, work_value, field_types[i], sec_raw );
        field_defaults.push_back( work_value );
    }

    section_sizes[hsec_record] = next_off - section_offsets[hsec_record];

/* -------------------------------------------------------------------- */
/*      Fetch the vertex block basics.                                  */
/* -------------------------------------------------------------------- */
    next_off = section_offsets[hsec_shape];

    vs->di[sec_vert].Initialize( vs, sec_vert );
    next_off += vs->di[sec_vert].SerializedSize();

/* -------------------------------------------------------------------- */
/*      Fetch the record block basics.                                  */
/* -------------------------------------------------------------------- */
    vs->di[sec_record].Initialize( vs, sec_record );
    next_off += vs->di[sec_record].SerializedSize();

/* -------------------------------------------------------------------- */
/*      Fetch the shapeid basics.                                       */
/* -------------------------------------------------------------------- */
    memcpy( &(vs->shape_count), vs->GetData(sec_raw,next_off,NULL,4), 4);
    if( needs_swap )
        SwapData( &(vs->shape_count), 4, 1 );

    next_off += 4;
    vs->shape_index_start = 0;

    section_sizes[hsec_shape] = next_off - section_offsets[hsec_shape] 
        + vs->shape_count * 12;
}

/************************************************************************/
/*                       WriteFieldDefinitions()                        */
/************************************************************************/

void VecSegHeader::WriteFieldDefinitions()

{
    PCIDSKBuffer hbuf( 1000 );
    uint32  offset = 0, i;
    ShapeField wrkfield;
    
    wrkfield.SetValue( (int32) field_names.size() );
    offset = vs->WriteField( offset, wrkfield, hbuf );
    
    for( i = 0; i < field_names.size(); i++ )
    {
        wrkfield.SetValue( field_names[i] );
        offset = vs->WriteField( offset, wrkfield, hbuf );

        wrkfield.SetValue( field_descriptions[i] );
        offset = vs->WriteField( offset, wrkfield, hbuf );

        wrkfield.SetValue( (int32) field_types[i] );
        offset = vs->WriteField( offset, wrkfield, hbuf );

        wrkfield.SetValue( field_formats[i] );
        offset = vs->WriteField( offset, wrkfield, hbuf );

        offset = vs->WriteField( offset, field_defaults[i], hbuf );
    }

    hbuf.SetSize( offset );

    GrowSection( hsec_record, hbuf.buffer_size );
    vs->WriteToFile( hbuf.buffer, section_offsets[hsec_record], 
                     hbuf.buffer_size );

    // invalidate the raw buffer.
    vs->raw_loaded_data.buffer_size = 0;
}

/************************************************************************/
/*                            GrowSection()                             */
/*                                                                      */
/*      If necessary grow/move the header section specified to have     */
/*      the desired amount of room.  Returns true if the header         */
/*      section has moved.                                              */
/************************************************************************/

bool VecSegHeader::GrowSection( int hsec, uint32 new_size )

{
/* -------------------------------------------------------------------- */
/*      Trivial case.                                                   */
/* -------------------------------------------------------------------- */
    if( section_sizes[hsec] >= new_size )
    {
        section_sizes[hsec] = new_size;
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Can we grow the section in it's currently location without      */
/*      overlapping anything else?                                      */
/* -------------------------------------------------------------------- */
    int ihsec;
    bool grow_ok = true;
    uint32 last_used = 0;

    for( ihsec = 0; ihsec < 4; ihsec++ )
    {
        if( ihsec == hsec )
            continue;
        
        if( section_offsets[ihsec] + section_sizes[ihsec] > last_used )
            last_used = section_offsets[ihsec] + section_sizes[ihsec];

        if( section_offsets[hsec] >= 
            section_offsets[ihsec] + section_sizes[ihsec] )
            continue;

        if( section_offsets[ihsec] >= section_offsets[hsec] + new_size )
            continue;

        // apparent overlap
        grow_ok = false;
    }

/* -------------------------------------------------------------------- */
/*      If we can grow in place and have space there is nothing to do.  */
/* -------------------------------------------------------------------- */
    if( grow_ok 
        && section_offsets[hsec] + new_size
        < header_blocks * block_page_size )
    {
        section_sizes[hsec] = new_size;
        return false;
    }
    
/* -------------------------------------------------------------------- */
/*      Where will the section be positioned after grow?  It might      */
/*      be nice to search for a big enough hole in the existing area    */
/*      to fit the section.                                             */
/* -------------------------------------------------------------------- */
    uint32 new_base;

    if( grow_ok )
        new_base = section_offsets[hsec];
    else
        new_base = last_used;

/* -------------------------------------------------------------------- */
/*      Does the header need to grow?                                   */
/* -------------------------------------------------------------------- */
    if( new_base + new_size > header_blocks * block_page_size )
    {
        GrowHeader( (new_base+new_size+block_page_size-1) / block_page_size 
                    - header_blocks );
    }

/* -------------------------------------------------------------------- */
/*      Move the old section to the new location.                       */
/* -------------------------------------------------------------------- */
    vs->MoveData( section_offsets[hsec], new_base, section_sizes[hsec] );
    section_sizes[hsec] = new_size;
    section_offsets[hsec] = new_base;

/* -------------------------------------------------------------------- */
/*      Update the section offsets list.                                */
/* -------------------------------------------------------------------- */
    uint32 new_offset = section_offsets[hsec];
    if( needs_swap )
        SwapData( &new_offset, 4, 1 );
    vs->WriteToFile( &new_offset, 72 + hsec * 4, 4 );

    return true;
}

/************************************************************************/
/*                           GrowBlockIndex()                           */
/*                                                                      */
/*      Allocate the requested number of additional blocks to the       */
/*      data block index.                                               */
/************************************************************************/

void VecSegHeader::GrowBlockIndex( int section, int new_blocks )

{
    if( new_blocks == 0 )
        return;

    uint32  next_block = vs->GetContentSize() / block_page_size;

    while( new_blocks > 0 )
    {
        vs->di[section].AddBlockToIndex( next_block++ );
        new_blocks--;
    }

    if( GrowSection( hsec_shape, section_sizes[hsec_shape] + 4*new_blocks ) )
    {
        vs->di[sec_vert].SetDirty();
        vs->di[sec_record].SetDirty();
        vs->shape_index_page_dirty = true; // we need to rewrite at new location
    }
}

/************************************************************************/
/*                         ShapeIndexPrepare()                          */
/*                                                                      */
/*      When CPCIDSKVectorSegment::FlushLoadedShapeIndex() needs to     */
/*      write out all the shapeid's and offsets, it calls this          */
/*      method to find the offset from the start of the segment at      */
/*      which it should do the writing.                                 */
/*                                                                      */
/*      We use this opportunity to flush out the vertex, and record     */
/*      block offsets if necessary, and to grow the header if needed    */
/*      to hold the proposed shapeindex size.   The passed in size      */
/*      is the size in bytes from "Number of Shapes" on in the          */
/*      "Shape section" of the header.                                  */
/************************************************************************/

uint32 VecSegHeader::ShapeIndexPrepare( uint32 size )

{
    GrowSection( hsec_shape, 
                 size 
                 + vs->di[sec_vert].size_on_disk 
                 + vs->di[sec_record].size_on_disk );

    return section_offsets[hsec_shape]
        + vs->di[sec_vert].size_on_disk 
        + vs->di[sec_record].size_on_disk;
}

/************************************************************************/
/*                             GrowHeader()                             */
/*                                                                      */
/*      Grow the header by the requested number of blocks.  This        */
/*      will often involve migrating existing vector or record          */
/*      section blocks on to make space since the header must be        */
/*      contiguous.                                                     */
/************************************************************************/

void VecSegHeader::GrowHeader( uint32 new_blocks )

{
//    fprintf( stderr, "GrowHeader(%d) to %d\n", 
//             new_blocks, header_blocks + new_blocks );

/* -------------------------------------------------------------------- */
/*      Process the two existing block maps, moving stuff on if         */
/*      needed.                                                         */
/* -------------------------------------------------------------------- */
    vs->di[sec_vert].VacateBlockRange( header_blocks, new_blocks );
    vs->di[sec_record].VacateBlockRange( header_blocks, new_blocks );

/* -------------------------------------------------------------------- */
/*      Write to ensure the segment is the new size.                    */
/* -------------------------------------------------------------------- */
    vs->WriteToFile( "\0", (header_blocks+new_blocks) * block_page_size - 1, 1);

/* -------------------------------------------------------------------- */
/*      Update to new header size.                                      */
/* -------------------------------------------------------------------- */
    header_blocks += new_blocks;

    uint32 header_block_buf = header_blocks;
    
    if( needs_swap )
        SwapData( &header_block_buf, 4, 1 );
    
    vs->WriteToFile( &header_block_buf, 68, 4 );
}


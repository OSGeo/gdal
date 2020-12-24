/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKVectorSegment class.
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

#include "pcidsk_file.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"
#include "segment/cpcidskvectorsegment.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <algorithm>

using namespace PCIDSK;

/* -------------------------------------------------------------------- */
/*      Size of a block in the record/vertex block tables.  This is    */
/*      determined by the PCIDSK format and may not be changed.         */
/* -------------------------------------------------------------------- */
static const int block_page_size = 8192;


/* -------------------------------------------------------------------- */
/*      Size of one page of loaded shapeids.  This is not related to    */
/*      the file format, and may be changed to alter the number of      */
/*      shapeid pointers kept in RAM at one time from the shape         */
/*      index.                                                          */
/* -------------------------------------------------------------------- */
static const int shapeid_page_size = 1024;

/************************************************************************/
/*                        CPCIDSKVectorSegment()                        */
/************************************************************************/

CPCIDSKVectorSegment::CPCIDSKVectorSegment( PCIDSKFile *fileIn, int segmentIn,
                                            const char *segment_pointer )
        : CPCIDSKSegment( fileIn, segmentIn, segment_pointer )

{
    base_initialized = false;
    needs_swap = false;

    total_shape_count = 0;
    valid_shape_count = 0;

    last_shapes_id = NullShapeId;
    last_shapes_index = -1;

    raw_loaded_data_offset = 0;
    vert_loaded_data_offset = 0;
    record_loaded_data_offset = 0;
    raw_loaded_data_dirty = false;
    vert_loaded_data_dirty = false;
    record_loaded_data_dirty = false;

    shape_index_start = 0;
    shape_index_page_dirty = false;

    shapeid_map_active = false;
    shapeid_pages_certainly_mapped = -1;

    vh.vs = this;

    highest_shapeid_used = NullShapeId;
}

/************************************************************************/
/*                       ~CPCIDSKVectorSegment()                        */
/************************************************************************/

CPCIDSKVectorSegment::~CPCIDSKVectorSegment()

{
    try
    {
        Synchronize();
    }
    catch( const PCIDSKException& e )
    {
        fprintf(stderr, "Exception in ~CPCIDSKVectorSegment(): %s", e.what()); // ok
    }
}

/************************************************************************/
/*                            Synchronize()                             */
/************************************************************************/

void CPCIDSKVectorSegment::Synchronize()
{
    if( base_initialized )
    {
        FlushSegHeaderIfNeeded();

        FlushDataBuffer( sec_vert );
        FlushDataBuffer( sec_record );

        di[sec_vert].Flush();
        di[sec_record].Flush();

        FlushLoadedShapeIndex();

        if( GetHeader().GetInt( 192, 16 ) != total_shape_count
            && file->GetUpdatable() )
        {
            GetHeader().Put( total_shape_count, 192, 16 );
            FlushHeader();
        }
    }
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Initialize the header of a new vector segment in a              */
/*      consistent state for an empty segment.                          */
/************************************************************************/

void CPCIDSKVectorSegment::Initialize()

{
    needs_swap = !BigEndianSystem();

/* -------------------------------------------------------------------- */
/*      Initialize the header that occurs within the regular segment    */
/*      data.                                                           */
/* -------------------------------------------------------------------- */
    vh.InitializeNew();

/* -------------------------------------------------------------------- */
/*      Initialize the values in the generic segment header.            */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer &head = GetHeader();

    head.Put( "METRE", 160, 16 );
    head.Put( 1.0, 176, 16 );
    head.Put( 0, 192, 16 );
    head.Put( 0, 208, 16 );
    head.Put( 0, 224, 16 );
    head.Put( "", 240, 16 );
    head.Put( 0, 256, 16 );
    head.Put( 0, 272, 16 );

#ifdef PCIMAJORVERSION
    PCIDSK::ShapeField oFieldsDefault;
    oFieldsDefault.SetValue(std::vector<int32>{});

    // Add the RingStart field, because it can't be added after
    // shapes have been added. This is a bug that should be properly fixed
    AddField(ATT_RINGSTART,
             PCIDSK::FieldTypeCountedInt,
             "", "",
             &oFieldsDefault);
#endif

    FlushHeader();
}

/************************************************************************/
/*                             LoadHeader()                             */
/*                                                                      */
/*      Initialize minimum information from the vector segment          */
/*      header.  We defer this till an actual vector related action     */
/*      is taken.                                                       */
/************************************************************************/

void CPCIDSKVectorSegment::LoadHeader()

{
    if( base_initialized )
        return;

    base_initialized = true;

    needs_swap = !BigEndianSystem();

    vh.InitializeExisting();

    // When the IDB code deletes a shape, it simply writes a -1
    // into the index. We need to know how many actual valid shapes
    // there are in the segment, so count them
    valid_shape_count = 0;
    ShapeId iShape = FindFirst();
    while (iShape != NullShapeId)
    {
        ++valid_shape_count;
        iShape = FindNext(iShape);
    }
}

/************************************************************************/
/*                             ReadField()                              */
/*                                                                      */
/*      Read a value from the indicated offset in a section of the      */
/*      vector segment, and place the value into a ShapeField           */
/*      structure based on the passed in field type.                    */
/************************************************************************/

uint32 CPCIDSKVectorSegment::ReadField( uint32 offset, ShapeField& field,
                                        ShapeFieldType field_type,
                                        int section )

{
    switch( field_type )
    {
      case FieldTypeInteger:
      {
          int32 value;
          memcpy( &value, GetData( section, offset, nullptr, 4), 4 );
          if( needs_swap )
              SwapData( &value, 4, 1 );
          field.SetValue( value );
          return offset + 4;
      }

      case FieldTypeFloat:
      {
          float value;
          memcpy( &value, GetData( section, offset, nullptr, 4), 4 );
          if( needs_swap )
              SwapData( &value, 4, 1 );
          field.SetValue( value );
          return offset + 4;
      }

      case FieldTypeDouble:
      {
          double value;
          memcpy( &value, GetData( section, offset, nullptr, 8), 8 );
          if( needs_swap )
              SwapData( &value, 8, 1 );
          field.SetValue( value );
          return offset + 8;
      }

      case FieldTypeString:
      {
          int available;
          char *srcdata = GetData( section, offset, &available, 1 );

          // Simple case -- all initially available.
          int string_len = 0;

          while( srcdata[string_len] != '\0' && available - string_len > 0 )
              string_len++;

          if( string_len < available && srcdata[string_len] == '\0' )
          {
              std::string value( srcdata, string_len );
              field.SetValue( value );
              return offset + string_len + 1;
          }

          std::string value;

          while( *srcdata != '\0' )
          {
              value += *(srcdata++);
              offset++;
              available--;
              if( available == 0 )
                  srcdata = GetData( section, offset, &available, 1 );
          }

          field.SetValue( value );
          return offset+1;
      }

      case FieldTypeCountedInt:
      {
          std::vector<int32> value;
          int32 count;
          char *srcdata = GetData( section, offset, nullptr, 4 );
          memcpy( &count, srcdata, 4 );
          if( needs_swap )
              SwapData( &count, 4, 1 );

          value.resize( count );
          if( count > 0 )
          {
              if( offset > std::numeric_limits<uint32>::max() - 8 )
                  return ThrowPCIDSKException(0, "Invalid offset = %u", offset);
              memcpy( &(value[0]), GetData(section,offset+4,nullptr,4*count), 4*count );
              if( needs_swap )
                  SwapData( &(value[0]), 4, count );
          }

          field.SetValue( value );
          return offset + 4 + 4*count;
      }

      default:
        return ThrowPCIDSKException(0, "Unhandled field type %d", field_type);
    }
}

/************************************************************************/
/*                             WriteField()                             */
/*                                                                      */
/*      Write a field value into a buffer, growing the buffer if        */
/*      needed to hold the value.                                       */
/************************************************************************/

uint32 CPCIDSKVectorSegment::WriteField( uint32 offset,
                                         const ShapeField& field,
                                         PCIDSKBuffer& buffer )

{
/* -------------------------------------------------------------------- */
/*      How much space do we need for this value?                       */
/* -------------------------------------------------------------------- */
    uint32 item_size = 0;

    switch( field.GetType() )
    {
      case FieldTypeInteger:
        item_size = 4;
        break;

      case FieldTypeFloat:
        item_size = 4;
        break;

      case FieldTypeDouble:
        item_size = 8;
        break;

      case FieldTypeString:
        item_size = static_cast<uint32>(field.GetValueString().size()) + 1;
        break;

      case FieldTypeCountedInt:
        item_size = static_cast<uint32>(field.GetValueCountedInt().size()) * 4 + 4;
        break;

      default:
        assert( 0 );
        item_size = 0;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to grow the buffer to hold this?  Try to make it     */
/*      plenty larger.                                                  */
/* -------------------------------------------------------------------- */
    if( item_size + offset > (uint32) buffer.buffer_size )
        buffer.SetSize( buffer.buffer_size*2 + item_size );

/* -------------------------------------------------------------------- */
/*      Write to the buffer, and byte swap if needed.                   */
/* -------------------------------------------------------------------- */
    switch( field.GetType() )
    {
      case FieldTypeInteger:
      {
          int32 value = field.GetValueInteger();
          if( needs_swap )
              SwapData( &value, 4, 1 );
          memcpy( buffer.buffer+offset, &value, 4 );
          break;
      }

      case FieldTypeFloat:
      {
          float value = field.GetValueFloat();
          if( needs_swap )
              SwapData( &value, 4, 1 );
          memcpy( buffer.buffer+offset, &value, 4 );
          break;
      }

      case FieldTypeDouble:
      {
          double value = field.GetValueDouble();
          if( needs_swap )
              SwapData( &value, 8, 1 );
          memcpy( buffer.buffer+offset, &value, 8 );
          break;
      }

      case FieldTypeString:
      {
          std::string value = field.GetValueString();
          memcpy( buffer.buffer+offset, value.c_str(), item_size );
          break;
      }

      case FieldTypeCountedInt:
      {
          std::vector<int32> value = field.GetValueCountedInt();
          uint32 count = static_cast<uint32>(value.size());
          memcpy( buffer.buffer+offset, &count, 4 );
          if( count > 0 )
          {
              memcpy( buffer.buffer+offset+4, &(value[0]), count * 4 );
              if( needs_swap )
                  SwapData( buffer.buffer+offset, 4, count+1 );
          }
          break;
      }

      default:
        assert( 0 );
        break;
    }

    return offset + item_size;
}

/************************************************************************/
/*                              GetData()                               */
/************************************************************************/

char *CPCIDSKVectorSegment::GetData( int section, uint32 offset,
                                     int *bytes_available, int min_bytes,
                                     bool update )

{
    if( min_bytes == 0 )
        min_bytes = 1;

/* -------------------------------------------------------------------- */
/*      Select the section to act on.                                   */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer *pbuf = nullptr;
    uint32       *pbuf_offset = nullptr;
    bool         *pbuf_dirty = nullptr;

    if( section == sec_raw )
    {
        pbuf = &raw_loaded_data;
        pbuf_offset = &raw_loaded_data_offset;
        pbuf_dirty = &raw_loaded_data_dirty;
    }
    else if( section == sec_vert )
    {
        pbuf = &vert_loaded_data;
        pbuf_offset = &vert_loaded_data_offset;
        pbuf_dirty = &vert_loaded_data_dirty;
    }
    else if( section == sec_record )
    {
        pbuf = &record_loaded_data;
        pbuf_offset = &record_loaded_data_offset;
        pbuf_dirty = &record_loaded_data_dirty;
    }
    else
    {
        return (char*)ThrowPCIDSKExceptionPtr("Unexpected case");
    }

    if( offset > std::numeric_limits<uint32>::max() - static_cast<uint32>(min_bytes) )
        return (char*)ThrowPCIDSKExceptionPtr("Invalid offset : %u", offset);

/* -------------------------------------------------------------------- */
/*      If the desired data is not within our loaded section, reload    */
/*      one or more blocks around the request.                          */
/* -------------------------------------------------------------------- */
    if( offset < *pbuf_offset
        || offset+static_cast<uint32>(min_bytes) > *pbuf_offset + static_cast<uint32>(pbuf->buffer_size) )
    {
        if( *pbuf_dirty )
            FlushDataBuffer( section );

        // we want whole 8K blocks around the target region.
        uint32 load_offset = offset - (offset % block_page_size);
        int size = (offset + static_cast<uint32>(min_bytes) - load_offset + block_page_size - 1);

        size -= (size % block_page_size);

        // If the request goes beyond the end of the file, and we are
        // in update mode, grow the segment by writing at the end of
        // the requested section.  This will throw an exception if we
        // are unable to grow the file.
        if( section != sec_raw
            && load_offset + size > di[section].GetIndex()->size() * block_page_size
            && update )
        {
            PCIDSKBuffer zerobuf(block_page_size);

            memset( zerobuf.buffer, 0, block_page_size );
            WriteSecToFile( section, zerobuf.buffer,
                            (load_offset + size) / block_page_size - 1, 1 );
        }

        *pbuf_offset = load_offset;
        pbuf->SetSize( size );

        ReadSecFromFile( section, pbuf->buffer,
                         load_offset / block_page_size, size / block_page_size );
    }

/* -------------------------------------------------------------------- */
/*      If an update request goes beyond the end of the last data       */
/*      byte in a data section, then update the bytes used.  Now        */
/*      read into our buffer.                                           */
/* -------------------------------------------------------------------- */
    if( section != sec_raw
        && offset + min_bytes > di[section].GetSectionEnd() )
        di[section].SetSectionEnd( offset + min_bytes );

/* -------------------------------------------------------------------- */
/*      Return desired info.                                            */
/* -------------------------------------------------------------------- */
    if( bytes_available != nullptr )
        *bytes_available = *pbuf_offset + pbuf->buffer_size - offset;

    if( update )
        *pbuf_dirty = true;

    return pbuf->buffer + offset - *pbuf_offset;
}

/************************************************************************/
/*                          ReadSecFromFile()                           */
/*                                                                      */
/*      Read one or more blocks from the desired "section" of the       */
/*      segment data, going through the block pointer map for           */
/*      vect/record sections.                                           */
/************************************************************************/

void CPCIDSKVectorSegment::ReadSecFromFile( int section, char *buffer,
                                            int block_offset,
                                            int block_count )

{
/* -------------------------------------------------------------------- */
/*      Raw is a simple case, directly gulp.                            */
/* -------------------------------------------------------------------- */
    if( section == sec_raw )
    {
        ReadFromFile( buffer, static_cast<uint64>(block_offset)*static_cast<uint32>(block_page_size),
                      block_count*block_page_size );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Process one 8K block at a time in case they are discontiguous   */
/*      which they often are.                                           */
/* -------------------------------------------------------------------- */
    int i;
    const std::vector<uint32> *block_map = di[section].GetIndex();

    if(  block_count + block_offset > (int) block_map->size() )
    {
        return ThrowPCIDSKException("Assertion failed: block_count(=%d) + block_offset(=%d) <= block_map->size()(=%d)",
                                    block_count, block_offset, (int) block_map->size() );
    }

    for( i = 0; i < block_count; i++ )
    {
        ReadFromFile( buffer + i * block_page_size,
                      block_page_size * static_cast<uint64>((*block_map)[block_offset+i]),
                      block_page_size );
    }
}

/************************************************************************/
/*                          FlushDataBuffer()                           */
/*                                                                      */
/*      Flush the indicated data buffer to disk if it is marked         */
/*      dirty.                                                          */
/************************************************************************/

void CPCIDSKVectorSegment::FlushDataBuffer( int section )

{
/* -------------------------------------------------------------------- */
/*      Select the section to act on.                                   */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer *pbuf = nullptr;
    uint32       *pbuf_offset = nullptr;
    bool         *pbuf_dirty = nullptr;

    if( section == sec_raw )
    {
        pbuf = &raw_loaded_data;
        pbuf_offset = &raw_loaded_data_offset;
        pbuf_dirty = &raw_loaded_data_dirty;
    }
    else if( section == sec_vert )
    {
        pbuf = &vert_loaded_data;
        pbuf_offset = &vert_loaded_data_offset;
        pbuf_dirty = &vert_loaded_data_dirty;
    }
    else if( section == sec_record )
    {
        pbuf = &record_loaded_data;
        pbuf_offset = &record_loaded_data_offset;
        pbuf_dirty = &record_loaded_data_dirty;
    }
    else
    {
        return ThrowPCIDSKException("Unexpected case");
    }

    if( ! *pbuf_dirty || pbuf->buffer_size == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      We need to write something.                                     */
/* -------------------------------------------------------------------- */
    assert( (pbuf->buffer_size % block_page_size) == 0 );
    assert( (*pbuf_offset % block_page_size) == 0 );

    WriteSecToFile( section, pbuf->buffer,
                    *pbuf_offset / block_page_size,
                    pbuf->buffer_size / block_page_size );

    *pbuf_dirty = false;
}

/************************************************************************/
/*                           WriteSecToFile()                           */
/*                                                                      */
/*      Read one or more blocks from the desired "section" of the       */
/*      segment data, going through the block pointer map for           */
/*      vect/record sections.                                           */
/************************************************************************/

void CPCIDSKVectorSegment::WriteSecToFile( int section, char *buffer,
                                           int block_offset,
                                           int block_count )

{
/* -------------------------------------------------------------------- */
/*      Raw is a simple case, directly gulp.                            */
/* -------------------------------------------------------------------- */
    if( section == sec_raw )
    {
        WriteToFile( buffer, block_offset*block_page_size,
                     block_count*block_page_size );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to grow this data section to be able to do the       */
/*      write?                                                          */
/* -------------------------------------------------------------------- */
    const std::vector<uint32> *block_map = di[section].GetIndex();

    if( block_count + block_offset > (int) block_map->size() )
    {
        vh.GrowBlockIndex( section,
                           block_count + block_offset - static_cast<int>(block_map->size()) );
    }

/* -------------------------------------------------------------------- */
/*      Process one 8K block at a time in case they are discontiguous   */
/*      which they often are.                                           */
/* -------------------------------------------------------------------- */
    int i;
    for( i = 0; i < block_count; i++ )
    {
        WriteToFile( buffer + i * block_page_size,
                     block_page_size * (*block_map)[block_offset+i],
                     block_page_size );
    }
}

/************************************************************************/
/*                           GetProjection()                            */
/************************************************************************/

std::vector<double> CPCIDSKVectorSegment::GetProjection( std::string &geosys )

{
    LoadHeader();

/* -------------------------------------------------------------------- */
/*      Fetch the projparms string from the proj section of the         */
/*      vector segment header.                                          */
/* -------------------------------------------------------------------- */
    ShapeField projparms;

    ReadField( vh.section_offsets[hsec_proj]+32, projparms,
               FieldTypeString, sec_raw );

/* -------------------------------------------------------------------- */
/*      Read the geosys (units) string from SDH5.VEC1 in the segment    */
/*      header.                                                         */
/* -------------------------------------------------------------------- */
    GetHeader().Get( 160, 16, geosys, 0 ); // do not unpad!

    return ProjParmsFromText( geosys, projparms.GetValueString() );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

void CPCIDSKVectorSegment::SetProjection( std::string geosys,
                                          std::vector<double> parms )

{
    LoadHeader();

/* -------------------------------------------------------------------- */
/*      Apply parameters in the vector segment "proj" header section.   */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer proj(32);
    uint32       proj_size;
    ShapeField   value;

    value.SetValue( ProjParmsToText( parms ) );

    ReadFromFile( proj.buffer, vh.section_offsets[hsec_proj], 32 );
    proj_size = WriteField( 32, value, proj );

    vh.GrowSection( hsec_proj, proj_size );
    WriteToFile( proj.buffer, vh.section_offsets[hsec_proj], proj_size );

/* -------------------------------------------------------------------- */
/*      Write the geosys string to the generic segment header.          */
/* -------------------------------------------------------------------- */
    GetHeader().Put( geosys.c_str(), 160, 16 );
    FlushHeader();
}

/************************************************************************/
/*                          IndexFromShapeId()                          */
/*                                                                      */
/*      Translate a shapeid into a shape index.  Several mechanisms     */
/*      are used to accelerate this when possible.                      */
/************************************************************************/

int CPCIDSKVectorSegment::IndexFromShapeId( ShapeId id )

{
    if( id == NullShapeId )
        return -1;

    LoadHeader();

/* -------------------------------------------------------------------- */
/*      Does this match our last lookup?                                */
/* -------------------------------------------------------------------- */
    if( id == last_shapes_id )
        return last_shapes_index;

/* -------------------------------------------------------------------- */
/*      Is this the next shapeid in sequence, and is it in our          */
/*      loaded index cache?                                             */
/* -------------------------------------------------------------------- */
    if( id == last_shapes_id + 1
        && last_shapes_index + 1 >= shape_index_start
        && last_shapes_index + 1 < shape_index_start + (int) shape_index_ids.size() )
    {
        last_shapes_index++;
        last_shapes_id++;
        return last_shapes_index;
    }

/* -------------------------------------------------------------------- */
/*      Activate the shapeid map, if it is not already active.          */
/* -------------------------------------------------------------------- */
    if( !shapeid_map_active )
    {
        PopulateShapeIdMap();
    }

/* -------------------------------------------------------------------- */
/*      Is this already in our shapeid map?                             */
/* -------------------------------------------------------------------- */
    if( shapeid_map.count( id ) == 1 )
        return shapeid_map[id];

    return -1;
}

/************************************************************************/
/*                          LoadShapeIdPage()                           */
/************************************************************************/

void CPCIDSKVectorSegment::LoadShapeIdPage( int page )

{
/* -------------------------------------------------------------------- */
/*      Load a chunk of shape index information into a                  */
/*      PCIDSKBuffer.                                                   */
/* -------------------------------------------------------------------- */
    uint32 shape_index_byte_offset =
        vh.section_offsets[hsec_shape]
        + di[sec_record].offset_on_disk_within_section
        + di[sec_record].size_on_disk + 4;

    int entries_to_load = shapeid_page_size;

    shape_index_start = page * shapeid_page_size;
    if( shape_index_start + entries_to_load > total_shape_count )
        entries_to_load = total_shape_count - shape_index_start;

    PCIDSKBuffer wrk_index;
    if( entries_to_load < 0 || entries_to_load > std::numeric_limits<int>::max() / 12 )
        return ThrowPCIDSKException("Invalid entries_to_load = %d", entries_to_load);
    wrk_index.SetSize( entries_to_load * 12 );

    ReadFromFile( wrk_index.buffer,
                  shape_index_byte_offset + static_cast<uint64>(shape_index_start)*12,
                  wrk_index.buffer_size );

/* -------------------------------------------------------------------- */
/*      Parse into the vectors for easier use.                          */
/* -------------------------------------------------------------------- */
    int i;

    shape_index_ids.resize( entries_to_load );
    shape_index_vertex_off.resize( entries_to_load );
    shape_index_record_off.resize( entries_to_load );

    for( i = 0; i < entries_to_load; i++ )
    {
        memcpy( &(shape_index_ids[i]), wrk_index.buffer + i*12, 4 );
        memcpy( &(shape_index_vertex_off[i]), wrk_index.buffer + i*12+4, 4 );
        memcpy( &(shape_index_record_off[i]), wrk_index.buffer + i*12+8, 4 );
    }

    if( needs_swap && entries_to_load > 0 )
    {
        SwapData( &(shape_index_ids[0]), 4, entries_to_load );
        SwapData( &(shape_index_vertex_off[0]), 4, entries_to_load );
        SwapData( &(shape_index_record_off[0]), 4, entries_to_load );
    }

    PushLoadedIndexIntoMap();
}

/************************************************************************/
/*                         AccessShapeByIndex()                         */
/*                                                                      */
/*      This method is responsible for loading the set of               */
/*      information for shape "shape_index" into the shape_index data   */
/*      structures if it is not already there.                          */
/************************************************************************/

void CPCIDSKVectorSegment::AccessShapeByIndex( int shape_index )

{
    LoadHeader();

/* -------------------------------------------------------------------- */
/*      Is the requested index already loaded?                          */
/* -------------------------------------------------------------------- */
    if( shape_index >= shape_index_start
        && shape_index < shape_index_start + (int) shape_index_ids.size() )
        return;

    // this is for requesting the next shapeindex after shapecount on
    // a partial page.
    if( shape_index == total_shape_count
        && (int) shape_index_ids.size() < shapeid_page_size
        && total_shape_count == (int) shape_index_ids.size() + shape_index_start )
        return;

/* -------------------------------------------------------------------- */
/*      If the currently loaded shapeindex is dirty, we should write    */
/*      it now.                                                         */
/* -------------------------------------------------------------------- */
    FlushLoadedShapeIndex();

/* -------------------------------------------------------------------- */
/*      Load the page of shapeid information for this shape index.      */
/* -------------------------------------------------------------------- */
    LoadShapeIdPage( shape_index / shapeid_page_size );
}

/************************************************************************/
/*                       PushLoadedIndexIntoMap()                       */
/************************************************************************/

void CPCIDSKVectorSegment::PushLoadedIndexIntoMap()

{
/* -------------------------------------------------------------------- */
/*      If the shapeid map is active, apply the current pages           */
/*      shapeids if it does not already appear to have been             */
/*      applied.                                                        */
/* -------------------------------------------------------------------- */
    int loaded_page = shape_index_start / shapeid_page_size;

    if( shapeid_map_active && !shape_index_ids.empty() )
    {
        unsigned int i;

        for( i = 0; i < shape_index_ids.size(); i++ )
        {
            if( shape_index_ids[i] != NullShapeId )
                shapeid_map[shape_index_ids[i]] = i+shape_index_start;
        }

        if( loaded_page == shapeid_pages_certainly_mapped+1 )
            shapeid_pages_certainly_mapped++;
    }
}

/************************************************************************/
/*                         PopulateShapeIdMap()                         */
/*                                                                      */
/*      Completely populate the shapeid->index map.                     */
/************************************************************************/

void CPCIDSKVectorSegment::PopulateShapeIdMap()

{
/* -------------------------------------------------------------------- */
/*      Enable shapeid_map mode, and load the current page.             */
/* -------------------------------------------------------------------- */
    if( !shapeid_map_active )
    {
        shapeid_map_active = true;
        PushLoadedIndexIntoMap();
    }

/* -------------------------------------------------------------------- */
/*      Load all outstanding pages.                                     */
/* -------------------------------------------------------------------- */
    int shapeid_pages = (total_shape_count+shapeid_page_size-1) / shapeid_page_size;

    while( shapeid_pages_certainly_mapped+1 < shapeid_pages )
    {
        LoadShapeIdPage( shapeid_pages_certainly_mapped+1 );
    }
}

/************************************************************************/
/*                     FindNextValidByIndex()                           */
/************************************************************************/
/**
  * Find the next shape and the given shape index in the segment
  * (including deleted shapes), if the shape at nIndex is NullShapeId then
  * return the nexrt valid shape ID
  *
  * @param nIndex the index into
  */
ShapeId CPCIDSKVectorSegment::FindNextValidByIndex(int nIndex)
{
    LoadHeader();

    if (total_shape_count == 0 || nIndex >= total_shape_count)
        return NullShapeId;


    for (int nShapeIndex = nIndex; nShapeIndex < total_shape_count; ++nShapeIndex)
    {
        // set up shape_index_ids array
        AccessShapeByIndex(nShapeIndex);

        int32 nNextShapeId = shape_index_ids[nShapeIndex - shape_index_start];
        if (nNextShapeId != NullShapeId)
        {
            last_shapes_id = nNextShapeId;
            last_shapes_index = nShapeIndex;
            return last_shapes_id;
        }
    }

    return NullShapeId;
}

/************************************************************************/
/*                             FindFirst()                              */
/************************************************************************/

ShapeId CPCIDSKVectorSegment::FindFirst()
{
    return FindNextValidByIndex(0);
}

/************************************************************************/
/*                              FindNext()                              */
/************************************************************************/

ShapeId CPCIDSKVectorSegment::FindNext( ShapeId previous_id )
{
    if( previous_id == NullShapeId )
        return FindFirst();

    int previous_index = IndexFromShapeId( previous_id );

    return FindNextValidByIndex(previous_index+1);
}

/************************************************************************/
/*                           GetShapeCount()                            */
/************************************************************************/

int CPCIDSKVectorSegment::GetShapeCount()

{
    LoadHeader();

    return valid_shape_count;
}

/************************************************************************/
/*                            GetVertices()                             */
/************************************************************************/

void CPCIDSKVectorSegment::GetVertices( ShapeId shape_id,
                                        std::vector<ShapeVertex> &vertices )

{
    int shape_index = IndexFromShapeId( shape_id );

    if( shape_index == -1 )
        return ThrowPCIDSKException( "Attempt to call GetVertices() on non-existing shape id '%d'.",
                              (int) shape_id );

    AccessShapeByIndex( shape_index );

    uint32 vert_off = shape_index_vertex_off[shape_index - shape_index_start];
    uint32 vertex_count;

    if( vert_off == 0xffffffff )
    {
        vertices.resize(0);
        return;
    }

    if( vert_off > std::numeric_limits<uint32>::max() - 4 )
        return ThrowPCIDSKException( "Invalid vert_off = %u", vert_off);
    memcpy( &vertex_count, GetData( sec_vert, vert_off+4, nullptr, 4 ), 4 );
    if( needs_swap )
        SwapData( &vertex_count, 4, 1 );

    try
    {
        vertices.resize( vertex_count );
    }
    catch( const std::exception& ex )
    {
        return ThrowPCIDSKException("Out of memory allocating vertices(%u): %s",
                                    vertex_count, ex.what());
    }

    // We ought to change this to process the available data and
    // then request more.
    if( vertex_count > 0 )
    {
        if( vert_off > std::numeric_limits<uint32>::max() - 8 )
            return ThrowPCIDSKException( "Invalid vert_off = %u", vert_off);
        memcpy( &(vertices[0]),
                GetData( sec_vert, vert_off+8, nullptr, vertex_count*24),
                vertex_count * 24 );
        if( needs_swap )
            SwapData( &(vertices[0]), 8, vertex_count*3 );
    }
}

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

int CPCIDSKVectorSegment::GetFieldCount()

{
    LoadHeader();

    return static_cast<int>(vh.field_names.size());
}

/************************************************************************/
/*                            GetFieldName()                            */
/************************************************************************/

std::string CPCIDSKVectorSegment::GetFieldName( int field_index )

{
    LoadHeader();

    return vh.field_names[field_index];
}

/************************************************************************/
/*                        GetFieldDescription()                         */
/************************************************************************/

std::string CPCIDSKVectorSegment::GetFieldDescription( int field_index )

{
    LoadHeader();

    return vh.field_descriptions[field_index];
}

/************************************************************************/
/*                            GetFieldType()                            */
/************************************************************************/

ShapeFieldType CPCIDSKVectorSegment::GetFieldType( int field_index )

{
    LoadHeader();

    return vh.field_types[field_index];
}

/************************************************************************/
/*                           GetFieldFormat()                           */
/************************************************************************/

std::string CPCIDSKVectorSegment::GetFieldFormat( int field_index )

{
    LoadHeader();

    return vh.field_formats[field_index];
}

/************************************************************************/
/*                          GetFieldDefault()                           */
/************************************************************************/

ShapeField CPCIDSKVectorSegment::GetFieldDefault( int field_index )

{
    LoadHeader();

    return vh.field_defaults[field_index];
}

/************************************************************************/
/*                             GetFields()                              */
/************************************************************************/

void CPCIDSKVectorSegment::GetFields( ShapeId id,
                                      std::vector<ShapeField>& list )

{
    unsigned int i;
    int shape_index = IndexFromShapeId( id );

    if( shape_index == -1 )
        return ThrowPCIDSKException( "Attempt to call GetFields() on non-existing shape id '%d'.",
                              (int) id );

    AccessShapeByIndex( shape_index );

    uint32 offset = shape_index_record_off[shape_index - shape_index_start];

    list.resize(vh.field_names.size());

    if( offset == 0xffffffff )
    {
        for( i = 0; i < vh.field_names.size(); i++ )
            list[i] = vh.field_defaults[i];
    }
    else
    {
        offset += 4; // skip size

        for( i = 0; i < vh.field_names.size(); i++ )
            offset = ReadField( offset, list[i], vh.field_types[i], sec_record );
    }
}

/************************************************************************/
/*                              AddField()                              */
/************************************************************************/

void CPCIDSKVectorSegment::AddField( std::string name, ShapeFieldType type,
                                     std::string description,
                                     std::string format,
                                     ShapeField *default_value )

{
    ShapeField fallback_default;

    LoadHeader();

/* -------------------------------------------------------------------- */
/*      If we have existing features, we should go through adding       */
/*      this new field.                                                 */
/* -------------------------------------------------------------------- */
    if( total_shape_count > 0 )
    {
        return ThrowPCIDSKException( "Support for adding fields in populated layers "
                              "has not yet been implemented." );
    }

/* -------------------------------------------------------------------- */
/*      If no default is provided, use the obvious value.               */
/* -------------------------------------------------------------------- */
    if( default_value == nullptr )
    {
        switch( type )
        {
          case FieldTypeFloat:
            fallback_default.SetValue( (float) 0.0 );
            break;
          case FieldTypeDouble:
            fallback_default.SetValue( (double) 0.0 );
            break;
          case FieldTypeInteger:
            fallback_default.SetValue( (int32) 0 );
            break;
          case FieldTypeCountedInt:
          {
            std::vector<int32> empty_list;
            fallback_default.SetValue( empty_list );
            break;
          }
          case FieldTypeString:
            fallback_default.SetValue( "" );
            break;

          case FieldTypeNone:
            break;
        }

        default_value = &fallback_default;
    }

/* -------------------------------------------------------------------- */
/*      Make sure the default field is of the correct type.             */
/* -------------------------------------------------------------------- */
    if( default_value->GetType() != type )
    {
        return ThrowPCIDSKException( "Attempt to add field with a default value of "
                              "a different type than the field." );
    }

    if( type == FieldTypeNone )
    {
        return ThrowPCIDSKException( "Creating fields of type None not supported." );
    }

/* -------------------------------------------------------------------- */
/*      Add the field to the definition list.                           */
/* -------------------------------------------------------------------- */
    vh.field_names.push_back( name );
    vh.field_types.push_back( type );
    vh.field_descriptions.push_back( description );
    vh.field_formats.push_back( format );
    vh.field_defaults.push_back( *default_value );

    vh_dirty = true;
}

/************************************************************************/
/*                        FlushSegHeaderIfNeeded()                      */
/************************************************************************/

void CPCIDSKVectorSegment::FlushSegHeaderIfNeeded()
{
    if( vh_dirty )
    {
        vh.WriteFieldDefinitions();
        vh_dirty = false;
    }
}

/************************************************************************/
/*                            CreateShape()                             */
/************************************************************************/

ShapeId CPCIDSKVectorSegment::CreateShape( ShapeId id )

{
    LoadHeader();
    FlushSegHeaderIfNeeded();

/* -------------------------------------------------------------------- */
/*      Make sure we have the last shapeid index page loaded.           */
/* -------------------------------------------------------------------- */
    AccessShapeByIndex( total_shape_count );

    // if highest_shapeid_used is unset, then look at all Ids
    if (highest_shapeid_used == NullShapeId &&!shape_index_ids.empty())
    {
        auto it = std::max_element(shape_index_ids.begin(), shape_index_ids.end());
        highest_shapeid_used = *it;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to assign a shapeid?                                 */
/* -------------------------------------------------------------------- */
    if( id == NullShapeId )
    {
        if( highest_shapeid_used == NullShapeId )
            id = 0;
        else
            id = highest_shapeid_used + 1;
    }
    if( id > highest_shapeid_used )
        highest_shapeid_used = id;
    else
    {
        PopulateShapeIdMap();
        if( shapeid_map.count(id) > 0 )
        {
            return ThrowPCIDSKException( 0, "Attempt to create a shape with id '%d', but that already exists.", id );
        }
    }

/* -------------------------------------------------------------------- */
/*      Push this new shape on to our list of shapeids in the           */
/*      current page, and mark the page as dirty.                       */
/* -------------------------------------------------------------------- */
    shape_index_ids.push_back( id );
    shape_index_record_off.push_back( 0xffffffff );
    shape_index_vertex_off.push_back( 0xffffffff );
    shape_index_page_dirty = true;

    if( shapeid_map_active )
        shapeid_map[id] = total_shape_count;

    total_shape_count++;
    valid_shape_count++;

    return id;
}

/************************************************************************/
/*                            DeleteShape()                             */
/*                                                                      */
/*      Delete a shape by shapeid.                                      */
/************************************************************************/

void CPCIDSKVectorSegment::DeleteShape( ShapeId id )

{
    FlushSegHeaderIfNeeded();
    int shape_index = IndexFromShapeId( id );

    if( shape_index == -1 )
        return ThrowPCIDSKException( "Attempt to call DeleteShape() on non-existing shape '%d'.",
                              (int) id );

/* ==================================================================== */
/*      Our strategy is to move the last shape in our index down to     */
/*      replace the shape that we are deleting.  Unfortunately this     */
/*      will result in an out of sequence shapeid, but it is hard to    */
/*      avoid that without potentially rewriting much of the shape      */
/*      index.                                                          */
/*                                                                      */
/*      Note that the following sequence *does* work for special        */
/*      cases like deleting the last shape in the list, or deleting     */
/*      a shape on the same page as the last shape.   At worst a wee    */
/*      bit of extra work is done.                                      */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Load the page of shapeids containing the last shape in our      */
/*      index, capture the last shape's details, and remove it.         */
/* -------------------------------------------------------------------- */

    uint32 vert_off, rec_off;
    ShapeId  last_id;

    AccessShapeByIndex( total_shape_count-1 );

    last_id = shape_index_ids[total_shape_count-1-shape_index_start];
    vert_off = shape_index_vertex_off[total_shape_count-1-shape_index_start];
    rec_off = shape_index_record_off[total_shape_count-1-shape_index_start];

    // We don't actually have to modify this area of the index on disk.
    // Some of the stuff at the end just becomes unreferenced when we
    // decrement total_shape_count.

/* -------------------------------------------------------------------- */
/*      Load the page with the shape we are deleting, and put last      */
/*      the shapes information over it.                                 */
/* -------------------------------------------------------------------- */
    AccessShapeByIndex( shape_index );

    shape_index_ids[shape_index-shape_index_start] = last_id;
    shape_index_vertex_off[shape_index-shape_index_start] = vert_off;
    shape_index_record_off[shape_index-shape_index_start] = rec_off;

    shape_index_page_dirty = true;

    if( shapeid_map_active )
        shapeid_map.erase( id );

    // if the highest shape_id is the one that was deleted,
    // then reset highest_shapeid_used
    if (id == highest_shapeid_used)
        highest_shapeid_used = NullShapeId;
    total_shape_count--;
    valid_shape_count--;
}

/************************************************************************/
/*                            SetVertices()                             */
/************************************************************************/

void CPCIDSKVectorSegment::SetVertices( ShapeId id,
                                        const std::vector<ShapeVertex>& list )

{
    FlushSegHeaderIfNeeded();
    int shape_index = IndexFromShapeId( id );

    if( shape_index == -1 )
        return ThrowPCIDSKException( "Attempt to call SetVertices() on non-existing shape '%d'.",
                              (int) id );

    PCIDSKBuffer vbuf( static_cast<int>(list.size()) * 24 + 8 );

    AccessShapeByIndex( shape_index );

/* -------------------------------------------------------------------- */
/*      Is the current space big enough to hold the new vertex set?     */
/* -------------------------------------------------------------------- */
    uint32 vert_off = shape_index_vertex_off[shape_index - shape_index_start];
    uint32 chunk_size = 0;

    if( vert_off != 0xffffffff )
    {
        memcpy( &chunk_size, GetData( sec_vert, vert_off, nullptr, 4 ), 4 );
        if( needs_swap )
            SwapData( &chunk_size, 4, 1 );

        if( chunk_size < (uint32) vbuf.buffer_size )
        {
            vert_off = 0xffffffff;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we need to put this at the end of the section?               */
/* -------------------------------------------------------------------- */
    if( vert_off == 0xffffffff )
    {
        vert_off = di[sec_vert].GetSectionEnd();
        chunk_size = vbuf.buffer_size;
    }

/* -------------------------------------------------------------------- */
/*      Format the vertices in a buffer.                                */
/* -------------------------------------------------------------------- */
    uint32 vert_count = static_cast<uint32>(list.size());
    unsigned int i;

    memcpy( vbuf.buffer, &chunk_size, 4 );
    memcpy( vbuf.buffer+4, &vert_count, 4 );
    if( needs_swap )
        SwapData( vbuf.buffer, 4, 2 );

    for( i = 0; i < vert_count; i++ )
    {
        memcpy( vbuf.buffer + 8 + i*24 +  0, &(list[i].x), 8 );
        memcpy( vbuf.buffer + 8 + i*24 +  8, &(list[i].y), 8 );
        memcpy( vbuf.buffer + 8 + i*24 + 16, &(list[i].z), 8 );
    }

    if( needs_swap )
        SwapData( vbuf.buffer + 8, 8, 3*vert_count );

/* -------------------------------------------------------------------- */
/*      Write the data into the working buffer.                         */
/* -------------------------------------------------------------------- */
    memcpy( GetData( sec_vert, vert_off, nullptr, vbuf.buffer_size, true ),
            vbuf.buffer, vbuf.buffer_size );

/* -------------------------------------------------------------------- */
/*      Record the offset                                               */
/* -------------------------------------------------------------------- */
    if( shape_index_vertex_off[shape_index - shape_index_start] != vert_off )
    {
        shape_index_vertex_off[shape_index - shape_index_start] = vert_off;
        shape_index_page_dirty = true;
    }
}

/************************************************************************/
/*                             SetFields()                              */
/************************************************************************/

void CPCIDSKVectorSegment::SetFields( ShapeId id,
                                      const std::vector<ShapeField>& list_in )

{
    FlushSegHeaderIfNeeded();
    uint32 i;
    int shape_index = IndexFromShapeId( id );
    std::vector<ShapeField> full_list;
    const std::vector<ShapeField> *listp = nullptr;

    if( shape_index == -1 )
        return ThrowPCIDSKException( "Attempt to call SetFields() on non-existing shape id '%d'.",
                              (int) id );

    if( list_in.size() > vh.field_names.size() )
    {
        return ThrowPCIDSKException(
            "Attempt to write %d fields to a layer with only %d fields.",
            static_cast<int>(list_in.size()), static_cast<int>(vh.field_names.size()) );
    }

    if( list_in.size() < vh.field_names.size() )
    {
        full_list = list_in;

        // fill out missing fields in list with defaults.
        for( i = static_cast<uint32>(list_in.size()); i < static_cast<uint32>(vh.field_names.size()); i++ )
            full_list[i] = vh.field_defaults[i];

        listp = &full_list;
    }
    else
        listp = &list_in;

    AccessShapeByIndex( shape_index );

/* -------------------------------------------------------------------- */
/*      Format the fields in the buffer.                                */
/* -------------------------------------------------------------------- */
    PCIDSKBuffer fbuf(4);
    uint32 offset = 4;

    for( i = 0; i < listp->size(); i++ )
        offset = WriteField( offset, (*listp)[i], fbuf );

    fbuf.SetSize( offset );

/* -------------------------------------------------------------------- */
/*      Is the current space big enough to hold the new field set?      */
/* -------------------------------------------------------------------- */
    uint32 rec_off = shape_index_record_off[shape_index - shape_index_start];
    uint32 chunk_size = offset;

    if( rec_off != 0xffffffff )
    {
        memcpy( &chunk_size, GetData( sec_record, rec_off, nullptr, 4 ), 4 );
        if( needs_swap )
            SwapData( &chunk_size, 4, 1 );

        if( chunk_size < (uint32) fbuf.buffer_size )
        {
            rec_off = 0xffffffff;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we need to put this at the end of the section?               */
/* -------------------------------------------------------------------- */
    if( rec_off == 0xffffffff )
    {
        rec_off = di[sec_record].GetSectionEnd();
        chunk_size = fbuf.buffer_size;
    }

/* -------------------------------------------------------------------- */
/*      Set the chunk size, and number of fields.                       */
/* -------------------------------------------------------------------- */
    memcpy( fbuf.buffer + 0, &chunk_size, 4 );

    if( needs_swap )
        SwapData( fbuf.buffer, 4, 1 );

/* -------------------------------------------------------------------- */
/*      Write the data into the working buffer.                         */
/* -------------------------------------------------------------------- */
    memcpy( GetData( sec_record, rec_off, nullptr, fbuf.buffer_size, true ),
            fbuf.buffer, fbuf.buffer_size );

/* -------------------------------------------------------------------- */
/*      Record the offset                                               */
/* -------------------------------------------------------------------- */
    if( shape_index_record_off[shape_index - shape_index_start] != rec_off )
    {
        shape_index_record_off[shape_index - shape_index_start] = rec_off;
        shape_index_page_dirty = true;
    }
}

/************************************************************************/
/*                       FlushLoadedShapeIndex()                        */
/************************************************************************/

void CPCIDSKVectorSegment::FlushLoadedShapeIndex()

{
    if( !shape_index_page_dirty )
        return;

    uint32 offset = vh.ShapeIndexPrepare( total_shape_count * 12 + 4 );

    PCIDSKBuffer write_buffer( shapeid_page_size * 12 );

    // Update the count field.
    memcpy( write_buffer.buffer, &total_shape_count, 4 );
    if( needs_swap )
        SwapData( write_buffer.buffer, 4, 1 );
    WriteToFile( write_buffer.buffer, offset, 4 );

    // Write out the page of shapeid information.
    unsigned int i;
    for( i = 0; i < shape_index_ids.size(); i++ )
    {
        memcpy( write_buffer.buffer + 12*i,
                &(shape_index_ids[i]), 4 );
        memcpy( write_buffer.buffer + 12*i + 4,
                &(shape_index_vertex_off[i]), 4 );
        memcpy( write_buffer.buffer + 12*i + 8,
                &(shape_index_record_off[i]), 4 );
    }

    if( needs_swap )
        SwapData( write_buffer.buffer, 4, static_cast<int>(shape_index_ids.size()) * 3 );

    WriteToFile( write_buffer.buffer,
                 offset + 4 + shape_index_start * 12,
                 12 * shape_index_ids.size() );

    // invalidate the raw buffer.
    raw_loaded_data.buffer_size = 0;


    shape_index_page_dirty = false;
}


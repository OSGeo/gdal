/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKVectorSegment class.
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
#ifndef __INCLUDE_SEGMENT_PCIDSKVECTORSEGMENT_H
#define __INCLUDE_SEGMENT_PCIDSKVECTORSEGMENT_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_vectorsegment.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"

#include <string>
#include <map>

namespace PCIDSK
{
    class PCIDSKFile;
    
    /************************************************************************/
    /*                        CPCIDSKVectorSegment                          */
    /************************************************************************/

    class CPCIDSKVectorSegment : public CPCIDSKSegment, 
                                 public PCIDSKVectorSegment
    {
    public:
        CPCIDSKVectorSegment( PCIDSKFile *file, int segment,
                              const char *segment_pointer );

        virtual        ~CPCIDSKVectorSegment();

        std::string     GetRst() { return ""; }

        int             GetFieldCount();
        std::string     GetFieldName(int);
        std::string     GetFieldDescription(int);
        ShapeFieldType  GetFieldType(int);
        std::string     GetFieldFormat(int);
        ShapeField      GetFieldDefault(int);

        ShapeIterator   begin() { return ShapeIterator(this); }
        ShapeIterator   end() { return ShapeIterator(this,NullShapeId); }

        ShapeId         FindFirst();
        ShapeId         FindNext(ShapeId);
        
        void            GetVertices( ShapeId, std::vector<ShapeVertex>& );
        void            GetFields( ShapeId, std::vector<ShapeField>& );

     private:
        bool            base_initialized;
        bool            needs_swap;

        uint32          section_offsets[4];

        // Field Definitions
        std::vector<std::string> field_names;
        std::vector<std::string> field_descriptions;
        std::vector<ShapeFieldType>   field_types;
        std::vector<std::string> field_formats;
        std::vector<ShapeField>  field_defaults;
        
        // Information from the Shape Section of the header.
        bool                 vertex_block_initialized;
        uint32               vertex_block_count;
        uint32               vertex_bytes;
        std::vector<uint32>  vertex_block_index;
        
        bool                 record_block_initialized;
        uint32               record_block_count;
        uint32               record_bytes;
        std::vector<uint32>  record_block_index;

        int32                shape_count;
        //ShapeId              first_shape_id;
        //ShapeId              last_shape_id;
        
        uint32               shape_index_byte_offset; // within segment
        int32                shape_index_start;       // index of first shape
        std::vector<int32>   shape_index_ids;         // loaded shape ids. 
        std::vector<uint32>  shape_index_vertex_off;  // loaded vertex offsets
        std::vector<uint32>  shape_index_record_off;  // loaded record offsets.
        
        ShapeId              last_shapes_id;
        int                  last_shapes_index;

        bool                 shapeid_map_active;
        std::map<ShapeId,int> shapeid_map;
        int                  shapeid_pages_certainly_mapped;

        void                 AccessShapeByIndex( int iIndex );
        int                  IndexFromShapeId( ShapeId id );
        
        // Cached buffers for GetData();
        PCIDSKBuffer         raw_loaded_data;
        uint32               raw_loaded_data_offset;

        PCIDSKBuffer         vert_loaded_data;
        uint32               vert_loaded_data_offset;

        PCIDSKBuffer         record_loaded_data;
        uint32               record_loaded_data_offset;

        static const int     sec_raw ;
        static const int     sec_vert;
        static const int     sec_record;

        char                *GetData( int section, uint32 offset, 
                                      int *bytes_available = NULL, 
                                      int min_bytes = 0 );
        void                 ReadSecFromFile( int section, char *buffer,
                                              int block_offset, 
                                              int block_count );
        void                 Initialize();

        uint32               ReadField( uint32 offset, 
                                        ShapeField& field, 
                                        ShapeFieldType field_type,
                                        int section = sec_record );
    };
} // end namespace PCIDSK

#endif // __INCLUDE_SEGMENT_VECTORSEGMENT_H

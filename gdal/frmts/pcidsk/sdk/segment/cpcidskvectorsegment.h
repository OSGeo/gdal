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
#include "segment/vecsegheader.h"
#include "segment/vecsegdataindex.h"

#include <string>
#include <map>

namespace PCIDSK
{
    class PCIDSKFile;
    
    const int     sec_vert = 0;
    const int     sec_record = 1;
    const int     sec_raw = 2;

    /************************************************************************/
    /*                        CPCIDSKVectorSegment                          */
    /************************************************************************/

    class CPCIDSKVectorSegment : virtual public CPCIDSKSegment, 
                                 public PCIDSKVectorSegment
    {
        friend class VecSegHeader;
        friend class VecSegDataIndex;

    public:
        CPCIDSKVectorSegment( PCIDSKFile *file, int segment,
                              const char *segment_pointer );

        virtual        ~CPCIDSKVectorSegment();

        void            Initialize();
        void            Synchronize();

        std::string     GetRst() { return ""; }
        std::vector<double> GetProjection( std::string &geosys );
        void            SetProjection(std::string geosys, 
                                      std::vector<double> parms);

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

        int             GetShapeCount();
        
        void            GetVertices( ShapeId, std::vector<ShapeVertex>& );
        void            GetFields( ShapeId, std::vector<ShapeField>& );

        void            AddField( std::string name, ShapeFieldType type,
                                  std::string description,
                                  std::string format,
                                  ShapeField *default_value );
        
        ShapeId         CreateShape( ShapeId id );
        void            DeleteShape( ShapeId id );
        void            SetVertices( ShapeId id, 
                                     const std::vector<ShapeVertex>& list );
        void            SetFields( ShapeId id, 
                                   const std::vector<ShapeField>& list );

        std::string     ConsistencyCheck();

        // Essentially internal stuff.
        char                *GetData( int section, uint32 offset, 
                                      int *bytes_available = NULL, 
                                      int min_bytes = 0,
                                      bool update = false );
        uint32               ReadField( uint32 offset, 
                                        ShapeField& field, 
                                        ShapeFieldType field_type,
                                        int section = sec_record );

        uint32               WriteField( uint32 offset,
                                         const ShapeField& field, 
                                         PCIDSKBuffer &buffer );
        void                 ReadSecFromFile( int section, char *buffer,
                                              int block_offset, 
                                              int block_count );
        void                 WriteSecToFile( int section, char *buffer,
                                             int block_offset, 
                                             int block_count );

     private:
        bool            base_initialized;
        bool            needs_swap;

        VecSegHeader    vh;
        VecSegDataIndex di[2];
      
        int32                shape_count;
        ShapeId              highest_shapeid_used;
        //ShapeId              first_shape_id;
        //ShapeId              last_shape_id;
        
        int32                shape_index_start;       // index of first shape
        std::vector<int32>   shape_index_ids;         // loaded shape ids. 
        std::vector<uint32>  shape_index_vertex_off;  // loaded vertex offsets
        std::vector<uint32>  shape_index_record_off;  // loaded record offsets.
        bool                 shape_index_page_dirty;
        
        ShapeId              last_shapes_id;
        int                  last_shapes_index;

        bool                 shapeid_map_active;
        std::map<ShapeId,int> shapeid_map;
        int                  shapeid_pages_certainly_mapped;

        void                 AccessShapeByIndex( int iIndex );
        int                  IndexFromShapeId( ShapeId id );
        void                 LoadShapeIdPage( int page );
        void                 FlushLoadedShapeIndex();
        void                 PushLoadedIndexIntoMap();
        void                 PopulateShapeIdMap();
        
        // Cached buffers for GetData();
        PCIDSKBuffer         raw_loaded_data;
        uint32               raw_loaded_data_offset;
        bool                 raw_loaded_data_dirty;

        PCIDSKBuffer         vert_loaded_data;
        uint32               vert_loaded_data_offset;
        bool                 vert_loaded_data_dirty;

        PCIDSKBuffer         record_loaded_data;
        uint32               record_loaded_data_offset;
        bool                 record_loaded_data_dirty;

        void                 FlushDataBuffer( int section );
        void                 LoadHeader();

        std::string          ConsistencyCheck_Header();
        std::string          ConsistencyCheck_DataIndices();
        std::string          ConsistencyCheck_ShapeIndices();
    };
} // end namespace PCIDSK

#endif // __INCLUDE_SEGMENT_VECTORSEGMENT_H

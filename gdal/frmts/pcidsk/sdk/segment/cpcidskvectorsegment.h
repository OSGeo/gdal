/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKVectorSegment class.
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
#ifndef INCLUDE_SEGMENT_PCIDSKVECTORSEGMENT_H
#define INCLUDE_SEGMENT_PCIDSKVECTORSEGMENT_H

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

    class CPCIDSKVectorSegment final: virtual public CPCIDSKSegment,
                                      public PCIDSKVectorSegment
    {
        friend class VecSegHeader;
        friend class VecSegDataIndex;

    public:
        CPCIDSKVectorSegment( PCIDSKFile *file, int segment,
                              const char *segment_pointer );

        virtual        ~CPCIDSKVectorSegment();

        void            Initialize() override;
        void            Synchronize() override;

        std::string     GetRst() override { return ""; }
        std::vector<double> GetProjection( std::string &geosys ) override;
        void            SetProjection(std::string geosys,
                                      std::vector<double> parms) override;

        int             GetFieldCount() override;
        std::string     GetFieldName(int) override;
        std::string     GetFieldDescription(int) override;
        ShapeFieldType  GetFieldType(int) override;
        std::string     GetFieldFormat(int) override;
        ShapeField      GetFieldDefault(int) override;

        ShapeIterator   begin() override { return ShapeIterator(this); }
        ShapeIterator   end() override { return ShapeIterator(this,NullShapeId); }

        ShapeId         FindFirst() override;
        ShapeId         FindNext(ShapeId) override;

        int             GetShapeCount() override;

        void            GetVertices( ShapeId, std::vector<ShapeVertex>& ) override;
        void            GetFields( ShapeId, std::vector<ShapeField>& ) override;

        void            AddField( std::string name, ShapeFieldType type,
                                  std::string description,
                                  std::string format,
                                  ShapeField *default_value ) override;

        ShapeId         CreateShape( ShapeId id ) override;
        void            DeleteShape( ShapeId id ) override;
        void            SetVertices( ShapeId id,
                                     const std::vector<ShapeVertex>& list ) override;
        void            SetFields( ShapeId id,
                                   const std::vector<ShapeField>& list ) override;

        std::string     ConsistencyCheck() override;

        // Essentially internal stuff.
        char                *GetData( int section, uint32 offset,
                                      int *bytes_available = nullptr,
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

        int32                total_shape_count;
        int32                valid_shape_count;
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

        bool                 vh_dirty = false;

        void                 FlushDataBuffer( int section );
        void                 LoadHeader();
        void                 FlushSegHeaderIfNeeded();

        std::string          ConsistencyCheck_Header();
        std::string          ConsistencyCheck_DataIndices();
        std::string          ConsistencyCheck_ShapeIndices();

        ShapeId         FindNextValidByIndex(int nIndex);
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_VECTORSEGMENT_H

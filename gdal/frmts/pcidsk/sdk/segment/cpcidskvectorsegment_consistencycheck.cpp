/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKVectorSegment class's 
 *           ConsistencyCheck() method.
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

#include "pcidsk_file.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"
#include "segment/cpcidskvectorsegment.h"
#include <cstring>
#include <cstdio>

using namespace PCIDSK;

/* -------------------------------------------------------------------- */
/*      Size of a block in the record/vertice block tables.  This is    */
/*      determined by the PCIDSK format and may not be changed.         */
/* -------------------------------------------------------------------- */
static const int block_page_size = 8192;  

/************************************************************************/
/* ==================================================================== */
/*                           SpaceMap                                   */
/*                                                                      */
/*    Helper class to track space allocations.                          */
/* ==================================================================== */
/************************************************************************/

class SpaceMap
{
public:
    std::vector<uint32>  offsets;
    std::vector<uint32>  sizes;

    // binary search for the offset closes to our target or earlier.
    uint32  FindPreceding( uint32 offset )
        {
            if( offsets.size() == 0 )
                return 0;

            uint32 start=0, end=offsets.size()-1;

            while( end > start )
            {
                uint32 middle = (start+end+1) / 2;
                if( offsets[middle] > offset )
                    end = middle-1;
                else if( offsets[middle] < offset )
                    start = middle;
                else
                    return middle;
            }
            
            return start;
        }

    bool    AddChunk( uint32 offset, uint32 size ) 
        {
            uint32 preceding = FindPreceding( offset );

            // special case for empty
            if( offsets.size() == 0 )
            {
                offsets.push_back( offset );
                sizes.push_back( size );
                return false;
            }
                    
            // special case for before first.
            if( offsets.size() > 0 && offset < offsets[0] )
            {
                if( offset+size > offsets[0] )
                    return true;

                if( offset+size == offsets[0] )
                {
                    offsets[0] = offset;
                    sizes[0] += size;
                }
                else
                {
                    offsets.insert( offsets.begin(), offset );
                    sizes.insert( sizes.begin(), size );
                }
                return false;
            }

            if( offsets[preceding] + sizes[preceding] > offset )
            {
                // conflict!
                return true;
            }

            if( preceding+1 < offsets.size()
                && offsets[preceding+1] < offset+size )
            {
                // conflict!
                return true;
            }

            // can we merge into preceding entry?
            if( offsets[preceding] + sizes[preceding] == offset )
            {
                sizes[preceding] += size;
                return false;
            }

            // can we merge into following entry?
            if( preceding+1 < offsets.size() 
                && offsets[preceding+1] == offset+size )
            {
                offsets[preceding+1] = offset;
                sizes[preceding+1] += size;
                return false;
            }

            // Insert after preceding.
            offsets.insert( offsets.begin() + (preceding + 1), offset );
            sizes.insert( sizes.begin() + (preceding + 1), size );

            return false;
        }
};



/************************************************************************/
/*                          ConsistencyCheck()                          */
/************************************************************************/

std::string CPCIDSKVectorSegment::ConsistencyCheck()

{
    Synchronize();
    
    std::string report = CPCIDSKSegment::ConsistencyCheck();

    report += ConsistencyCheck_Header();
    report += ConsistencyCheck_DataIndices();
    report += ConsistencyCheck_ShapeIndices();

    if( report != "" )
        fprintf( stderr, "ConsistencyCheck() Report:\n%s", report.c_str() );

    return report;
}

/************************************************************************/
/*                      ConsistencyCheck_Header()                       */
/*                                                                      */
/*      Check that the header sections are non-overlapping and fit      */
/*      in the blocks indicated.                                        */
/*                                                                      */
/*      Verify some "fixed" values.                                     */
/************************************************************************/

std::string CPCIDSKVectorSegment::ConsistencyCheck_Header()

{
    std::string report;

    LoadHeader();

    if( vh.header_blocks < 1 )
        report += "less than one header_blocks\n";

    if( vh.header_blocks * block_page_size > GetContentSize() )
        report += "header blocks larger than segment size!";


    SpaceMap smap;
    int i;

    for( i = 0; i < 4; i++ )
    {
        if( smap.AddChunk( vh.section_offsets[i], vh.section_sizes[i] ) )
            report += "A header section overlaps another header section!\n";

        if( vh.section_offsets[i] + vh.section_sizes[i] 
            > vh.header_blocks * block_page_size )
            report += "A header section goes past end of header.\n";
    }

    return report;
}

/************************************************************************/
/*                    ConsistencyCheck_DataIndices()                    */
/************************************************************************/

std::string CPCIDSKVectorSegment::ConsistencyCheck_DataIndices()

{
    std::string report;
    unsigned int section;

    SpaceMap smap;

    smap.AddChunk( 0, vh.header_blocks );

    for( section = 0; section < 2; section++ )
    {
        const std::vector<uint32> *map = di[section].GetIndex();
        unsigned int i;

        for( i = 0; i < map->size(); i++ )
        {
            if( smap.AddChunk( (*map)[i], 1 ) )
            {
                char msg[100];

                sprintf( msg, "Conflict for block %d, held by at least data index '%d'.\n",
                         (*map)[i], section );

                report += msg;
            }
        }

        if( di[section].bytes > di[section].block_count * block_page_size )
        {
            report += "bytes for data index to large for block count.\n";
        }
    }

    return report;
}

/************************************************************************/
/*                   ConsistencyCheck_ShapeIndices()                    */
/************************************************************************/

std::string CPCIDSKVectorSegment::ConsistencyCheck_ShapeIndices()

{
    std::string report;
    SpaceMap  vmap, rmap;
    std::map<ShapeId,uint32> id_map;
    int iShape;

    for( iShape = 0; iShape < shape_count; iShape++ )
    {
        AccessShapeByIndex( iShape );

        unsigned int toff = iShape - shape_index_start;

        if( id_map.count(shape_index_ids[toff]) > 0 )
        {
            char msg[100];

            sprintf( msg, "ShapeID %d is used for shape %d and %d!\n", 
                     shape_index_ids[toff], 
                     toff, id_map[shape_index_ids[toff]]);
            report += msg;
        }

        id_map[shape_index_ids[toff]] = toff;


        if( shape_index_vertex_off[toff] != 0xffffffff )
        {
            uint32 vertex_count;
            uint32 vertex_size;
            uint32 vert_off = shape_index_vertex_off[toff];

            memcpy( &vertex_size, GetData( sec_vert, vert_off, NULL, 4 ), 4 );
            memcpy( &vertex_count, GetData( sec_vert, vert_off+4, NULL, 4 ), 4 );
            if( needs_swap )
            {
                SwapData( &vertex_count, 4, 1 );
                SwapData( &vertex_size, 4, 1 );
            }
            
            if( vertex_size < vertex_count * 24 + 8 )
            {
                report += "vertices for shape index seem larger than space allocated.\n";
            }

            if( vert_off + vertex_size > di[sec_vert].GetSectionEnd() )
            {
                report += "record overruns data index bytes.\n";
            }

            if( vmap.AddChunk( vert_off, vertex_size ) )
            {
                report += "vertex overlap detected!\n";
            }
        }

        if( shape_index_record_off[toff] != 0xffffffff )
        {
            uint32 rec_off = shape_index_record_off[toff];
            uint32 offset = rec_off;
            uint32 record_size, i;
            ShapeField wfld;

            memcpy( &record_size, GetData( sec_record, rec_off, NULL, 4 ), 4 );
            if( needs_swap )
                SwapData( &record_size, 4, 1 );

            offset += 4;
            for( i = 0; i < vh.field_names.size(); i++ )
                offset = ReadField( offset, wfld, vh.field_types[i], 
                                    sec_record );

            if( offset - rec_off > record_size )
                report += "record actually larger than declared record size.\n";
            
            if( rec_off + record_size > di[sec_record].GetSectionEnd() )
            {
                report += "record overruns data index bytes.\n";
            }
            
            if( rmap.AddChunk( rec_off, record_size ) )
            {
                report += "record overlap detected!\n";
            }
        }
    }

    return report;
}

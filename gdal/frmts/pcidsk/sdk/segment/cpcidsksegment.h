/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKSegment class.
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

#ifndef INCLUDE_SEGMENT_PCIDSKSEGMENT_H
#define INCLUDE_SEGMENT_PCIDSKSEGMENT_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "pcidsk_segment.h"

#include <string>
#include <vector>

namespace PCIDSK
{
    class PCIDSKFile;
    class MetadataSet;
    
/************************************************************************/
/*                            CPCIDSKSegment                            */
/*                                                                      */
/*      Base class for accessing all segments.  Provides core           */
/*      PCIDSKObject implementation for segments with raw segment io    */
/*      options.                                                        */
/************************************************************************/

    class CPCIDSKSegment : virtual public PCIDSKSegment
    {
    public:
        CPCIDSKSegment( PCIDSKFile *file, int segment,
            const char *segment_pointer );
        virtual	~CPCIDSKSegment();

        void        LoadSegmentPointer( const char *segment_pointer );
        void        LoadSegmentHeader();

        PCIDSKBuffer &GetHeader() { return header; }
        void        FlushHeader();

        void      WriteToFile( const void *buffer, uint64 offset, uint64 size );
        void      ReadFromFile( void *buffer, uint64 offset, uint64 size );

        eSegType    GetSegmentType() { return segment_type; }
        std::string GetName() { return segment_name; }
        std::string GetDescription();
        int         GetSegmentNumber() { return segment; }
        uint64      GetContentSize() { return data_size - 1024; }
        bool        IsAtEOF();

        void        SetDescription( const std::string &description);
        
        std::string GetMetadataValue( const std::string &key ) const;
        void        SetMetadataValue( const std::string &key, const std::string &value );
        std::vector<std::string> GetMetadataKeys() const;
            
        virtual void Synchronize() {}
        
        std::vector<std::string> GetHistoryEntries() const;
        void SetHistoryEntries( const std::vector<std::string> &entries );
        void PushHistory(const std::string &app,
                         const std::string &message);

        virtual std::string ConsistencyCheck() { return ""; }

    protected:
        PCIDSKFile *file;

        int         segment;

        eSegType    segment_type;
        char        segment_flag;
        std::string segment_name;

        uint64	    data_offset;     // includes 1024 byte segment header.
        uint64      data_size;

        PCIDSKBuffer header;

        mutable MetadataSet  *metadata;
        
        std::vector<std::string> history_;

        void        MoveData( uint64 src_offset, uint64 dst_offset, 
                              uint64 size_in_bytes );
    };
    
} // end namespace PCIDSK
#endif // INCLUDE_SEGMENT_PCIDSKSEGMENT_H

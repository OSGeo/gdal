/******************************************************************************
 *
 * Purpose:  Declaration of the VecSegHeader class.  
 *
 * This class is used to manage reading and writing of the vector segment 
 * header section, growing them as needed.  It is exclusively a private
 * helper class for the CPCIDSKVectorSegment.
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
#ifndef INCLUDE_SEGMENT_VECSEGHEADER_H
#define INCLUDE_SEGMENT_VECSEGHEADER_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "pcidsk_vectorsegment.h"

#include <string>
#include <map>

namespace PCIDSK
{
    class CPCIDSKVectorSegment;

    /************************************************************************/
    /*                        VecSegHeader                                  */
    /************************************************************************/

    const int hsec_proj = 0;
    const int hsec_rst = 1;
    const int hsec_record = 2;
    const int hsec_shape = 3;

    class VecSegHeader 
    {
    public:
        VecSegHeader();

        ~VecSegHeader();

        void            InitializeNew();
        void            InitializeExisting();

        void            GrowHeader( uint32 new_blocks );
        bool            GrowSection( int hsec, uint32 new_size );
        void            WriteHeaderSection( int hsec, PCIDSKBuffer &buffer );

        void            GrowBlockIndex( int section, int new_blocks );

        uint32          section_offsets[4];
        uint32          section_sizes[4];

        // Field Definitions
        std::vector<std::string> field_names;
        std::vector<std::string> field_descriptions;
        std::vector<ShapeFieldType>   field_types;
        std::vector<std::string> field_formats;
        std::vector<ShapeField>  field_defaults;

        void      WriteFieldDefinitions();
        uint32    ShapeIndexPrepare( uint32 byte_size );
        
        CPCIDSKVectorSegment *vs;

        uint32          header_blocks;

     private:

        bool            initialized;
        bool            needs_swap;

    };
} // end namespace PCIDSK

#endif // __INCLUDE_SEGMENT_VECTORSEGMENT_H

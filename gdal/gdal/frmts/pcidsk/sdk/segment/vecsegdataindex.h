/******************************************************************************
 *
 * Purpose:  Declaration of the VecSegIndex class.  
 *
 * This class is used to manage a vector segment data block index.  There
 * will be two instances created, one for the record data (sec_record) and
 * one for the vertices (sec_vert).  This class is exclusively a private
 * helper class for VecSegHeader.
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

#ifndef INCLUDE_SEGMENT_VECSEGDATAINDEX_H
#define INCLUDE_SEGMENT_VECSEGDATAINDEX_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_vectorsegment.h"

#include <map>
#include <vector>

namespace PCIDSK
{
    class VecSegDataIndex 
    {
        friend class CPCIDSKVectorSegment;
        friend class VecSegHeader;

    public:
        VecSegDataIndex();
        ~VecSegDataIndex();

        void                 Initialize( CPCIDSKVectorSegment *seg,
                                         int section );

        uint32               SerializedSize();

        void                 SetDirty();
        void                 Flush();

        const std::vector<uint32> *GetIndex();
        void            AddBlockToIndex( uint32 block );
        void            VacateBlockRange( uint32 start, uint32 count );

        uint32          GetSectionEnd();
        void            SetSectionEnd( uint32 new_size );

    private:
        CPCIDSKVectorSegment *vs;

        int                  section;

        uint32               offset_on_disk_within_section;
        uint32               size_on_disk;

        bool                 block_initialized;
        uint32               block_count;
        uint32               bytes;
        std::vector<uint32>  block_index;
        bool                 dirty;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_VECSEGDATAINDEX_H

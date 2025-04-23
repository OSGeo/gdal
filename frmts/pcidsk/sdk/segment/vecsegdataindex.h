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
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

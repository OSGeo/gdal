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
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

/* -------------------------------------------------------------------- */
/*      Size of a block in the record/vertex block tables.  This is    */
/*      determined by the PCIDSK format and may not be changed.         */
/* -------------------------------------------------------------------- */
constexpr int block_page_size = 8192;

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

#endif // INCLUDE_SEGMENT_VECTORSEGMENT_H

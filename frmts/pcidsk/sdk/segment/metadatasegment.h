/******************************************************************************
 *
 * Purpose:  Declaration of the MetadataSegment class.
 *
 * This class is used to manage access to the SYS METADATA segment.  This
 * segment holds all the metadata for objects in the PCIDSK file.
 *
 * This class is closely partnered with the MetadataSet class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef INCLUDE_SEGMENT_METADATASEGMENT_H
#define INCLUDE_SEGMENT_METADATASEGMENT_H

#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"

#include <map>
#include <string>

namespace PCIDSK
{
    /************************************************************************/
    /*                           MetadataSegment                            */
    /************************************************************************/

    class MetadataSegment final: virtual public CPCIDSKSegment
    {

    public:
        MetadataSegment( PCIDSKFile *file, int segment,
                         const char *segment_pointer );
        virtual     ~MetadataSegment();

        void         FetchGroupMetadata( const char *group, int id,
                                    std::map<std::string,std::string> &md_set );
        void         SetGroupMetadataValue( const char *group, int id,
                                       const std::string& key, const std::string& value );

        void         Synchronize() override;

    private:
       bool         loaded;

       void         Load();
       void         Save();

       PCIDSKBuffer seg_data;

       std::map<std::string,std::string> update_list;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_METADATASEGMENT_H

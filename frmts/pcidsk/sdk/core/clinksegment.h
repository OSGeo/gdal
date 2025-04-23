/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK link info Segments
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_SEGMENT_CLINKSEGMENT_H
#define INCLUDE_PCIDSK_SEGMENT_CLINKSEGMENT_H

#include "segment/cpcidsksegment.h"
#include "pcidsk_buffer.h"

namespace PCIDSK {
    class PCIDSKFile;

    class CLinkSegment : public CPCIDSKSegment
    {
    public:
        CLinkSegment(PCIDSKFile *file, int segment,const char *segment_pointer);
        ~CLinkSegment();

        // Get path
        std::string GetPath(void) const;
        // Set path
        void SetPath(const std::string& oPath);

        //synchronize the segment on disk.
        void Synchronize() override;
    private:
        // Helper housekeeping functions
        void Load();
        void Write();

        bool loaded_;
        bool modified_;
        PCIDSKBuffer seg_data;
        std::string path;
    };
}

#endif // INCLUDE_PCIDSK_SEGMENT_CLINKSEGMENT_H

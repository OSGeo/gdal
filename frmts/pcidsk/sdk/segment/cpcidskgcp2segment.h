/******************************************************************************
 *
 * Purpose: Declaration of access to a PCIDSK GCP2 Segment
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_CPCIDSKGCP2SEGMENT_H
#define INCLUDE_SEGMENT_CPCIDSKGCP2SEGMENT_H

#include "pcidsk_gcp.h"
#include "pcidsk_gcpsegment.h"
#include "segment/cpcidsksegment.h"

namespace PCIDSK {
    class CPCIDSKGCP2Segment : virtual public PCIDSKGCPSegment,
                               public CPCIDSKSegment
    {
    public:
        CPCIDSKGCP2Segment(PCIDSKFile *file, int segment,const char *segment_pointer);
        ~CPCIDSKGCP2Segment();

        // Return all GCPs in the segment
        std::vector<PCIDSK::GCP> const& GetGCPs(void) const override;

        // Write the given GCPs to the segment. If the segment already
        // exists, it will be replaced with this one.
        void SetGCPs(std::vector<PCIDSK::GCP> const& gcps) override;

        // Return the count of GCPs in the segment
        unsigned int GetGCPCount(void) const override;

        // Clear a GCP Segment
        void ClearGCPs(void) override;

        void Synchronize() override;
    private:
        void Load();
        void RebuildSegmentData(void);
        bool loaded_;

        struct PCIDSKGCP2SegInfo
        {
            std::vector<PCIDSK::GCP> gcps;
            unsigned int num_gcps;
            PCIDSKBuffer seg_data;

            std::string map_units;   ///< PCI mapunits string
            std::string proj_parms;  ///< Additional projection parameters
            unsigned int num_proj;
            bool changed;
        };

        PCIDSKGCP2SegInfo* pimpl_;
    };
}

#endif // INCLUDE_SEGMENT_CPCIDSKGCP2SEGMENT_H


/******************************************************************************
 *
 * Purpose: Declaration of access to a PCIDSK GCP2 Segment
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
        struct PCIDSKGCP2SegInfo;
        PCIDSKGCP2SegInfo* pimpl_;
    };
}

#endif // INCLUDE_SEGMENT_CPCIDSKGCP2SEGMENT_H


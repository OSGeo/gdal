/******************************************************************************
 *
 * Purpose: Interface through which a PCIDSK GCP Segment would be accessed
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_PCIDSK_GCPSEGMENT_H
#define INCLUDE_PCIDSK_PCIDSK_GCPSEGMENT_H

#include "pcidsk_gcp.h"

#include <vector>

namespace PCIDSK {

//! Interface to PCIDSK GCP segment.
    class PCIDSKGCPSegment
    {
    public:
        //! Return all GCPs in the segment
        virtual std::vector<PCIDSK::GCP> const& GetGCPs(void) const = 0;

        //! Write the given GCPs to the segment. If the segment already exists, it will be replaced with this one.
        virtual void SetGCPs(std::vector<PCIDSK::GCP> const& gcps) = 0;

        //! Return the count of GCPs in the segment
        virtual unsigned int GetGCPCount(void) const = 0;

        //! Clear a GCP Segment
        virtual void ClearGCPs(void) = 0;

        //! Virtual Destructor
        virtual ~PCIDSKGCPSegment(void) {}
    };
} // end namespace PCIDSK


#endif // INCLUDE_PCIDSK_PCIDSK_GCPSEGMENT_H


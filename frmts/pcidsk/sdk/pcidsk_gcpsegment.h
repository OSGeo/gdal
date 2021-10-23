/******************************************************************************
 *
 * Purpose: Interface through which a PCIDSK GCP Segment would be accessed
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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


/******************************************************************************
 *
 * Purpose: Interface representing access to a PCIDSK Toutin Segment
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_PCIDSK_TOUTIN_H
#define INCLUDE_PCIDSK_PCIDSK_TOUTIN_H

#include <vector>
#include <string>
#include "segment/toutinstructures.h"

namespace PCIDSK {
//! Interface to PCIDSK RPC segment.
    class PCIDSKToutinSegment
    {
    public:

        // Virtual destructor
        virtual ~PCIDSKToutinSegment() {}

        virtual SRITInfo_t GetInfo() const =0;
        virtual void SetInfo(const SRITInfo_t& poInfo) =0;
    };
}

#endif // INCLUDE_PCIDSK_PCIDSK_TOUTIN_H

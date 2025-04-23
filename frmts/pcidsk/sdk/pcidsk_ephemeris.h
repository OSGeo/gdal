/******************************************************************************
 *
 * Purpose: Interface representing access to a PCIDSK Ephemeris segment
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_PCIDSK_EPHEMERIS_H
#define INCLUDE_PCIDSK_PCIDSK_EPHEMERIS_H

#include <vector>
#include <string>
#include "segment/orbitstructures.h"

namespace PCIDSK {
//! Interface to PCIDSK RPC segment.
    class PCIDSKEphemerisSegment
    {
    public:

        // Virtual destructor
        virtual ~PCIDSKEphemerisSegment() {}

        virtual const EphemerisSeg_t& GetEphemeris() const=0;
        virtual void SetEphemeris(const EphemerisSeg_t& oEph) =0;
    };
}

#endif // INCLUDE_PCIDSK_PCIDSK_EPHEMERIS_H

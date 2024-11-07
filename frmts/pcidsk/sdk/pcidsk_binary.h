/******************************************************************************
 *
 * Purpose: Interface representing access to a PCIDSK Binary Segment
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_PCIDSK_BINARY_H
#define INCLUDE_PCIDSK_PCIDSK_BINARY_H

namespace PCIDSK {
//! Interface to PCIDSK Binary segment.
    class PCIDSKBinarySegment
    {
    public:
        virtual const char* GetBuffer(void) const = 0;
        virtual unsigned int GetBufferSize(void) const = 0;
        virtual void SetBuffer(const char* pabyBuf,
            unsigned int nBufSize) = 0;

        // Virtual destructor
        virtual ~PCIDSKBinarySegment() {}
    };
}

#endif // INCLUDE_PCIDSK_PCIDSK_BINARY_H

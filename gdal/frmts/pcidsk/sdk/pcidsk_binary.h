/******************************************************************************
 *
 * Purpose: Interface representing access to a PCIDSK Binary Segment
 * 
 ******************************************************************************
 * Copyright (c) 2010
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
#ifndef __INCLUDE_PCIDSK_PCIDSK_BINARY_H
#define __INCLUDE_PCIDSK_PCIDSK_BINARY_H

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

#endif // __INCLUDE_PCIDSK_PCIDSK_BINARY_H

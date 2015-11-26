/******************************************************************************
 *
 * Purpose: Support for reading and manipulating general PCIDSK Binary Segments
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
#ifndef INCLUDE_PCIDSK_SEGMENT_PCIDSKBINARY_SEG_H
#define INCLUDE_PCIDSK_SEGMENT_PCIDSKBINARY_SEG_H
 
#include "pcidsk_binary.h"
#include "segment/cpcidsksegment.h"

namespace PCIDSK {
    class PCIDSKFile;

    class CPCIDSKBinarySegment : public PCIDSKBinarySegment,
        public CPCIDSKSegment
    {
    public:
        CPCIDSKBinarySegment(PCIDSKFile *file, int segment,
            const char *segment_pointer, bool bLoad=true);
        ~CPCIDSKBinarySegment();

        const char* GetBuffer(void) const
        {
            return seg_data.buffer;
        }

        unsigned int GetBufferSize(void) const
        {
            return seg_data.buffer_size;
        }
        void SetBuffer(const char* pabyBuf, 
            unsigned int nBufSize);

        //synchronize the segment on disk.
        void Synchronize();
    private:
        
        // Helper housekeeping functions
        void Load();
        void Write();

    //functions to read/write binary information
    protected:
        // The raw segment data
        PCIDSKBuffer seg_data;
        bool loaded_;
        bool mbModified;
    };
}

#endif // INCLUDE_PCIDSK_SEGMENT_PCIDSKBINARY_SEG_H

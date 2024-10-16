/******************************************************************************
 *
 * Purpose: Support for reading and manipulating general PCIDSK Binary Segments
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

        const char* GetBuffer(void) const override
        {
            return seg_data.buffer;
        }

        unsigned int GetBufferSize(void) const override
        {
            return seg_data.buffer_size;
        }
        void SetBuffer(const char* pabyBuf,
            unsigned int nBufSize) override;

        //synchronize the segment on disk.
        void Synchronize() override;
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

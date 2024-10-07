/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKBinarySegment class.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "segment/cpcidskbinarysegment.h"
#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"

#include <limits>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>

using namespace PCIDSK;

/**
 * Binary Segment constructor
 * @param[in,out] fileIn the PCIDSK file
 * @param[in] segmentIn the segment index
 * @param[in] segment_pointer the segment pointer
 * @param[in] bLoad true to load the segment, else false (default true)
 */
CPCIDSKBinarySegment::CPCIDSKBinarySegment(PCIDSKFile *fileIn,
                                           int segmentIn,
                                           const char *segment_pointer,
                                           bool bLoad) :
    CPCIDSKSegment(fileIn, segmentIn, segment_pointer),
    loaded_(false),mbModified(false)
{
    if (true == bLoad)
    {
        Load();
    }
    return;
}// Initializer constructor


CPCIDSKBinarySegment::~CPCIDSKBinarySegment()
{
}

/**
 * Load the contents of the segment
 */
void CPCIDSKBinarySegment::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }

    if( data_size < 1024 )
    {
        return ThrowPCIDSKException("Wrong data_size in CPCIDSKBinarySegment");
    }

    if( data_size - 1024 > static_cast<uint64_t>(std::numeric_limits<int>::max()) )
    {
        return ThrowPCIDSKException("too large data_size");
    }

    seg_data.SetSize((int)(data_size - 1024));

    ReadFromFile(seg_data.buffer, 0, data_size - 1024);

    // Mark it as being loaded properly.
    loaded_ = true;
}

/**
 * Write the segment on disk
 */
void CPCIDSKBinarySegment::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!loaded_) {
        return;
    }

    WriteToFile(seg_data.buffer, 0, seg_data.buffer_size);

    mbModified = false;
}

/**
 * Synchronize the segment, if it was modified then
 * write it into disk.
 */
void CPCIDSKBinarySegment::Synchronize()
{
    if(mbModified)
    {
        this->Write();
    }
}

void
CPCIDSKBinarySegment::SetBuffer(const char* pabyBuf,
                                unsigned int nBufSize)
{
    // Round the buffer size up to the next multiple of 512.
    int nNumBlocks = nBufSize / 512 + ((0 == nBufSize % 512) ? 0 : 1);
    unsigned int nAllocBufSize = 512 * nNumBlocks;

    seg_data.SetSize((int)nAllocBufSize);
    data_size = nAllocBufSize + 1024; // Incl. header

    memcpy(seg_data.buffer, pabyBuf, nBufSize);

    // Fill unused data at end with zeroes.
    if (nBufSize < nAllocBufSize)
    {
        memset(seg_data.buffer + nBufSize, 0,
            nAllocBufSize - nBufSize);
    }
    mbModified = true;

    return;
}// SetBuffer

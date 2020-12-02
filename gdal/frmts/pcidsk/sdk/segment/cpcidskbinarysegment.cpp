/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKBinarySegment class.
 *
 ******************************************************************************
 * Copyright (c) 2010
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

#include "segment/cpcidskbinarysegment.h"
#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"

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

    seg_data.SetSize((int)data_size - 1024);

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

/******************************************************************************
 *
 * Purpose:  Block directory API.
 *
 ******************************************************************************
 * Copyright (c) 2011
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

#ifndef PCIDSK_BLOCK_FILE_H
#define PCIDSK_BLOCK_FILE_H

#include "pcidsk_config.h"
#include <string>

namespace PCIDSK
{

/************************************************************************/
/*                             class BlockFile                          */
/************************************************************************/

/**
 * Class used as the interface for accessing file information.
 */
class PCIDSK_DLL BlockFile
{
public:
    virtual             ~BlockFile(void);

    virtual std::string GetFilename(void) const = 0;

    virtual bool        GetUpdatable(void) const = 0;

    virtual uint32      GetWidth(void) const = 0;
    virtual uint32      GetHeight(void) const = 0;

    virtual uint32      GetChannels(void) const = 0;

    virtual std::string GetFileOptions(void) const = 0;

    virtual uint64      GetImageFileSize(void) const = 0;

    virtual bool        IsValidFileOffset(uint64 nOffset) const = 0;

    virtual bool        IsCorruptedSegment(uint16 nSegment, uint64 nOffset, uint64 nSize) const = 0;

    virtual uint16      ExtendSegment(const std::string & oName,
                                      const std::string & oDesc,
                                      uint64 nExtendSize) = 0;

    virtual uint64      GetSegmentSize(uint16 nSegment) = 0;

    virtual void        WriteToSegment(uint16 nSegment, const void * pData,
                                       uint64 nOffset, uint64 nSize) = 0;

    virtual void        ReadFromSegment(uint16 nSegment, void * pData,
                                        uint64 nOffset, uint64 nSize) = 0;
};

} // namespace PCIDSK

#endif

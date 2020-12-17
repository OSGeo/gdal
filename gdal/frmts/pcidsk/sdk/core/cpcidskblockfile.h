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

#ifndef CPCIDSK_BLOCK_FILE_H
#define CPCIDSK_BLOCK_FILE_H

#include "blockdir/blockfile.h"

namespace PCIDSK
{

class CPCIDSKFile;
class PCIDSKFile;
class SysTileDir;

/************************************************************************/
/*                          class CPCIDSKBlockFile                      */
/************************************************************************/

/**
 * Class used by the block directory for accessing file information.
 */
class PCIDSK_DLL CPCIDSKBlockFile : public BlockFile
{
protected:
    CPCIDSKFile *       mpoFile;

    uint16              mnGrowingSegment;

public:
    CPCIDSKBlockFile(PCIDSKFile * poFile);

    SysTileDir *        GetTileDir(void);

    SysTileDir *        CreateTileDir(void);

    virtual std::string GetFilename(void) const;

    virtual bool        GetUpdatable(void) const;

    virtual uint32      GetWidth(void) const;
    virtual uint32      GetHeight(void) const;

    virtual uint32      GetChannels(void) const;

    virtual std::string GetFileOptions(void) const;

    virtual uint64      GetImageFileSize(void) const;

    virtual uint16      ExtendSegment(const std::string & oName,
                                      const std::string & oDesc,
                                      uint64 nExtendSize);

    virtual uint64      GetSegmentSize(uint16 nSegment);

    virtual void        WriteToSegment(uint16 nSegment, const void * pData,
                                       uint64 nOffset, uint64 nSize);

    virtual void        ReadFromSegment(uint16 nSegment, void * pData,
                                        uint64 nOffset, uint64 nSize);
};

} // namespace PCIDSK

#endif

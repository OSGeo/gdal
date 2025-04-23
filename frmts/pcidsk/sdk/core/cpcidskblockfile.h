/******************************************************************************
 *
 * Purpose:  Block directory API.
 *
 ******************************************************************************
 * Copyright (c) 2011
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

    virtual std::string GetFilename(void) const override;

    virtual bool        GetUpdatable(void) const override;

    virtual uint32      GetWidth(void) const override;
    virtual uint32      GetHeight(void) const override;

    virtual uint32      GetChannels(void) const override;

    virtual std::string GetFileOptions(void) const override;

    virtual uint64      GetImageFileSize(void) const override;

    virtual bool        IsValidFileOffset(uint64 nOffset) const override;

    virtual bool        IsCorruptedSegment(uint16 nSegment,
                                           uint64 nOffset, uint64 nSize) const override;

    virtual uint16      ExtendSegment(const std::string & oName,
                                      const std::string & oDesc,
                                      uint64 nExtendSize) override;

    virtual uint64      GetSegmentSize(uint16 nSegment) override;

    virtual void        WriteToSegment(uint16 nSegment, const void * pData,
                                       uint64 nOffset, uint64 nSize) override;

    virtual void        ReadFromSegment(uint16 nSegment, void * pData,
                                        uint64 nOffset, uint64 nSize) override;
};

} // namespace PCIDSK

#endif

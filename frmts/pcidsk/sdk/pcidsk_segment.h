/******************************************************************************
 *
 * Purpose:  Primary public include file for PCIDSK SDK.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef INCLUDE_PCIDSKSEGMENT_H
#define INCLUDE_PCIDSKSEGMENT_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                            PCIDSKSegment                             */
/************************************************************************/

//! Public interface for the PCIDSK Segment Type

    class PCIDSKSegment
    {
    public:
        virtual ~PCIDSKSegment() {}

        virtual void Initialize() {}

        virtual void LoadSegmentPointer( const char *segment_pointer ) = 0;

        virtual void WriteToFile( const void *buffer, uint64 offset, uint64 size)=0;
        virtual void ReadFromFile( void *buffer, uint64 offset, uint64 size ) = 0;

        virtual eSegType    GetSegmentType() = 0;
        virtual std::string GetName() = 0;
        virtual std::string GetDescription() = 0;
        virtual int         GetSegmentNumber() = 0;
        virtual bool        IsContentSizeValid() const = 0;
        virtual uint64      GetContentSize() = 0;
        virtual uint64      GetContentOffset() = 0;
        virtual bool        IsAtEOF() = 0;
        virtual bool        CanExtend(uint64 size) const = 0;

        virtual void        SetDescription( const std::string &description) = 0;

        virtual std::string GetMetadataValue( const std::string &key ) const = 0;
        virtual void SetMetadataValue( const std::string &key, const std::string &value ) = 0;
        virtual std::vector<std::string> GetMetadataKeys() const = 0;

        virtual std::vector<std::string> GetHistoryEntries() const = 0;
        virtual void SetHistoryEntries( const std::vector<std::string> &entries ) = 0;
        virtual void PushHistory(const std::string &app,
                                 const std::string &message) = 0;

        virtual void Synchronize() = 0;

        virtual std::string ConsistencyCheck() = 0;
    };

} // end namespace PCIDSK

#endif // INCLUDE_PCIDSKSEGMENT_H

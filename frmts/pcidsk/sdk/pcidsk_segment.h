/******************************************************************************
 *
 * Purpose:  Primary public include file for PCIDSK SDK.
 * 
 ******************************************************************************
 * Copyright (c) 2009
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
 
#ifndef __INCLUDE_SEGMENT_PCIDSKSEGMENT_H
#define __INCLUDE_SEGMENT_PCIDSKSEGMENT_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                            PCIDSKSegment                             */
/************************************************************************/

//! Public tnterface for the PCIDSK Segment Type

    class PCIDSKSegment 
    {
    public:
        virtual	~PCIDSKSegment() {}

        virtual void Initialize() {}

        virtual void WriteToFile( const void *buffer, uint64 offset, uint64 size)=0;
        virtual void ReadFromFile( void *buffer, uint64 offset, uint64 size ) = 0;

        virtual eSegType    GetSegmentType() = 0;
        virtual std::string GetName() = 0;
        virtual std::string GetDescription() = 0;
        virtual int         GetSegmentNumber() = 0;
        virtual uint64      GetContentSize() = 0;
        virtual bool        IsAtEOF() = 0;

        virtual void        SetDescription( const std::string &description) = 0;

        virtual std::string GetMetadataValue( const std::string &key ) = 0;
        virtual void SetMetadataValue( const std::string &key, const std::string &value ) = 0;
        virtual std::vector<std::string> GetMetadataKeys() = 0;
        
        virtual std::vector<std::string> GetHistoryEntries() const = 0;
        virtual void SetHistoryEntries( const std::vector<std::string> &entries ) = 0;
        virtual void PushHistory(const std::string &app,
                                 const std::string &message) = 0;

        virtual void Synchronize() = 0;
    };

} // end namespace PCIDSK

#endif // __INCLUDE_SEGMENT_PCIDSKSEGMENT_H

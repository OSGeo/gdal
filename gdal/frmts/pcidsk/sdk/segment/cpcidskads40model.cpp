/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKADS40ModelSegment class.
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

#include "pcidsk_ads40.h"
#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include "segment/cpcidskads40model.h"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>

using namespace PCIDSK;

// Struct to store details of the RPC model
struct CPCIDSKADS40ModelSegment::PCIDSKADS40Info
{
    std::string path;
    
    // The raw segment data
    PCIDSKBuffer seg_data;
};

CPCIDSKADS40ModelSegment::CPCIDSKADS40ModelSegment(PCIDSKFile *fileIn, 
                                                   int segmentIn,
                                                   const char *segment_pointer) :
    CPCIDSKSegment(fileIn, segmentIn, segment_pointer), 
    pimpl_(new CPCIDSKADS40ModelSegment::PCIDSKADS40Info), 
    loaded_(false),mbModified(false)
{
    try
    {
        Load();
    }
    catch( const PCIDSKException& )
    {
        delete pimpl_;
        pimpl_ = NULL;
        throw;
    }
}


CPCIDSKADS40ModelSegment::~CPCIDSKADS40ModelSegment()
{
    delete pimpl_;
}

// Load the contents of the segment
void CPCIDSKADS40ModelSegment::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }
    
    if( data_size - 1024 != 1 * 512 )
    {
        return ThrowPCIDSKException("Wrong data_size in CPCIDSKADS40ModelSegment");
    }
    
    pimpl_->seg_data.SetSize(static_cast<int>(data_size) - 1024); // should be 1 * 512
    
    ReadFromFile(pimpl_->seg_data.buffer, 0, data_size - 1024);
    
    // The ADS40 Model Segment is defined as follows:
    // ADs40 Segment: 1 512-byte blocks
    
    // Block 1:
    // Bytes   0-7: 'ADS40  '
    // Byte    8-512: the path
    
    if (!STARTS_WITH(pimpl_->seg_data.buffer, "ADS40   ")) 
    {
        pimpl_->seg_data.Put("ADS40   ",0,8);
        return;
        // Something has gone terribly wrong!
        /*throw PCIDSKException("A segment that was previously identified as an RFMODEL "
            "segment does not contain the appropriate data. Found: [%s]", 
            std::string(pimpl_->seg_data.buffer, 8).c_str());*/
    }
    
    pimpl_->path = std::string(&pimpl_->seg_data.buffer[8]);
    
    // We've now loaded the structure up with data. Mark it as being loaded 
    // properly.
    loaded_ = true;
    
}

void CPCIDSKADS40ModelSegment::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!loaded_) {
        return;
    }
      
    pimpl_->seg_data.Put("ADS40   ",0,8);
    pimpl_->seg_data.Put(pimpl_->path.c_str(),8,static_cast<int>(pimpl_->path.size()));

    WriteToFile(pimpl_->seg_data.buffer,0,data_size-1024);
    mbModified = false;
}

// Get sensor name
std::string CPCIDSKADS40ModelSegment::GetPath(void) const
{
    return pimpl_->path;
}

// Set sensor name
void CPCIDSKADS40ModelSegment::SetPath(const std::string& oPath)
{
    if(oPath.size() < 504)
    {
        pimpl_->path = oPath;
        mbModified = true;
    }
    else
    {
        return ThrowPCIDSKException("The size of the path cannot be"
                              " bigger than 504 characters.");
    }
}

void CPCIDSKADS40ModelSegment::Synchronize()
{
    if(mbModified)
    {
        this->Write();
    }
}


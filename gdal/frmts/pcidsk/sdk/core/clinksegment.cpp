/******************************************************************************
 *
 * Purpose:  Implementation of the CLinkSegment class.
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

#include "core/clinksegment.h"
#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <functional>

using namespace PCIDSK;

CLinkSegment::CLinkSegment(PCIDSKFile *file, 
                           int segment,
                           const char *segment_pointer) :
    CPCIDSKSegment(file, segment, segment_pointer), 
    loaded_(false), modified_(false)
{
    Load();
}


CLinkSegment::~CLinkSegment()
{
}

// Load the contents of the segment
void CLinkSegment::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }
    
    assert(data_size - 1024 == 1 * 512);
    
    seg_data.SetSize(data_size - 1024); // should be 1 * 512
    
    ReadFromFile(seg_data.buffer, 0, data_size - 1024);
    
    if (std::strncmp(seg_data.buffer, "SysLinkF", 8)) 
    {
        seg_data.Put("SysLinkF",0,8);
        return;
    }
    
    path = std::string(&seg_data.buffer[8]);
    std::string::reverse_iterator first_non_space = 
        std::find_if(path.rbegin(), path.rend(), 
                     std::bind2nd(std::not_equal_to<char>(), ' '));

    *(--first_non_space) = '\0';

    
    // We've now loaded the structure up with data. Mark it as being loaded 
    // properly.
    loaded_ = true;
    
}

void CLinkSegment::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!modified_) {
        return;
    }
      
    seg_data.Put("SysLinkF",0,8);
    seg_data.Put(path.c_str(), 8, path.size(), true);

    WriteToFile(seg_data.buffer, 0, data_size-1024);
    modified_ = false;
}

std::string CLinkSegment::GetPath(void) const
{
    return path;
}

void CLinkSegment::SetPath(const std::string& oPath)
{
    if(oPath.size() < 504)
    {
        path = oPath;
        modified_ = true;
    }
    else
    {
        throw PCIDSKException("The size of the path cannot be"
                              " bigger than 504 characters.");
    }
}

void CLinkSegment::Synchronize()
{
    if(modified_)
    {
        this->Write();
    }
}


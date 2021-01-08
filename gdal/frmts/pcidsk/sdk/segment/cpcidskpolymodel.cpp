/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKPolyModelSegment class.
 *
 ******************************************************************************
 * Copyright (c) 2009
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

#include "pcidsk_poly.h"
#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include "segment/cpcidskpolymodel.h"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>

using namespace PCIDSK;

// Struct to store details of the RPC model
struct CPCIDSKPolyModelSegment::PCIDSKPolyInfo
{
    // number of coefficients
    unsigned int nNumCoeffs;

    // pixels in the image
    unsigned int nPixels;
    // lines in the image
    unsigned int nLines;

    // Forward Coefficients (Geo2Img)
    std::vector<double> vdfX1;
    // Forward Coefficients (Geo2Img)
    std::vector<double> vdfY1;
    // Backward Coefficients Img2Geo
    std::vector<double> vdfX2;
    // Backward Coefficients Img2Geo
    std::vector<double> vdfY2;

    //map units of required projection
    std::string oMapUnit;
    //proj parm info of required projection
    std::vector<double> oProjectionInfo;

    // The raw segment data
    PCIDSKBuffer seg_data;
};

CPCIDSKPolyModelSegment::CPCIDSKPolyModelSegment(PCIDSKFile *fileIn,
                                                 int segmentIn,
                                                 const char *segment_pointer) :
    CPCIDSKSegment(fileIn, segmentIn, segment_pointer),
    pimpl_(new CPCIDSKPolyModelSegment::PCIDSKPolyInfo),
    loaded_(false),mbModified(false)
{
    Load();
}


CPCIDSKPolyModelSegment::~CPCIDSKPolyModelSegment()
{
    delete pimpl_;
}

// Load the contents of the segment
void CPCIDSKPolyModelSegment::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }

    if (data_size - 1024 != 7 * 512)
        return ThrowPCIDSKException("Corrupted poly model?");

    pimpl_->seg_data.SetSize((int)(data_size - 1024)); // should be 7 * 512

    ReadFromFile(pimpl_->seg_data.buffer, 0, data_size - 1024);

    // The Polynomial Model Segment is defined as follows:
    // Polynomial Segment: 7 512-byte blocks

    // Block 1:
    // Bytes   0-7: 'POLYMDL '
    if (std::strncmp(pimpl_->seg_data.buffer, "POLYMDL ", 8))
    {
        pimpl_->seg_data.Put("POLYMDL ",0,8);
        return;
        // Something has gone terribly wrong!
        /*throw PCIDSKException("A segment that was previously identified as an RFMODEL "
            "segment does not contain the appropriate data. Found: [%s]",
            std::string(pimpl_->seg_data.buffer, 8).c_str());*/
    }

    // Block 2: number of coefficient and size
    // Bytes   0-21: number of coefficients
    // Bytes  22-41: number of pixels
    // Bytes  42-61: number of lines
    pimpl_->nNumCoeffs = pimpl_->seg_data.GetInt(512, 22);
    pimpl_->nPixels = pimpl_->seg_data.GetInt(512 + 22, 22);
    pimpl_->nLines = pimpl_->seg_data.GetInt(512 + 44, 22);

    int i=0;
    // Block 3: Forward Coefficients (Geo2Img)
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->vdfX1.push_back(pimpl_->seg_data.GetDouble(2*512 + (i*22), 22));
    }

    // Block 4: Forward Coefficients (Geo2Img)
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->vdfY1.push_back(pimpl_->seg_data.GetDouble(3*512 + (i*22), 22));
    }

    // Block 5: Backward Coefficients Img2Geo
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->vdfX2.push_back(pimpl_->seg_data.GetDouble(4*512 + (i*22), 22));
    }

    // Block 6: Backward Coefficients Img2Geo
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->vdfY2.push_back(pimpl_->seg_data.GetDouble(5*512 + (i*22), 22));
    }

    // Block 7: Required projection
    // Bytes 0-16: The map units
    // Bytes 17-511: the proj parm info.
    pimpl_->oMapUnit = pimpl_->seg_data.Get(6*512,17);
    for(i=0 ; i < 19 ; i++)
    {
        pimpl_->oProjectionInfo.push_back(pimpl_->seg_data.GetDouble(6*512+17+i*26,26));
    }

    // We've now loaded the structure up with data. Mark it as being loaded
    // properly.
    loaded_ = true;
}

void CPCIDSKPolyModelSegment::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!loaded_) {
        return;
    }

    // Block 1:
    // Bytes   0-7: 'POLYMDL  '
    pimpl_->seg_data.Put("POLYMDL ",0,8);

    // Block 2: number of coefficient and size
    // Bytes   0-21: number of coefficients
    // Bytes  22-41: number of pixels
    // Bytes  42-61: number of lines
    pimpl_->seg_data.Put(pimpl_->nNumCoeffs,512, 22);
    pimpl_->seg_data.Put(pimpl_->nPixels,512 + 22, 22);
    pimpl_->seg_data.Put(pimpl_->nLines,512 + 44, 22);

    int i=0;
    assert(pimpl_->vdfX1.size() == pimpl_->nNumCoeffs);
    assert(pimpl_->vdfX2.size() == pimpl_->nNumCoeffs);
    assert(pimpl_->vdfY1.size() == pimpl_->nNumCoeffs);
    assert(pimpl_->vdfY2.size() == pimpl_->nNumCoeffs);
    // Block 3: Forward Coefficients (Geo2Img)
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->seg_data.Put(pimpl_->vdfX1[i],2*512 + (i*22), 22,"%20.14f");
    }

    // Block 4: Forward Coefficients (Geo2Img)
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->seg_data.Put(pimpl_->vdfY1[i],3*512 + (i*22), 22,"%20.14f");
    }

    // Block 5: Forward Coefficients (Geo2Img)
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->seg_data.Put(pimpl_->vdfX2[i],4*512 + (i*22), 22,"%20.14f");
    }

    // Block 6: Forward Coefficients (Geo2Img)
    // Each coefficient take 22 Bytes
    for(i=0 ; i < (int)pimpl_->nNumCoeffs ; i++)
    {
        pimpl_->seg_data.Put(pimpl_->vdfY2[i],5*512 + (i*22), 22,"%20.14f");
    }

    assert(pimpl_->oMapUnit.size() <= 17);
    assert(pimpl_->oProjectionInfo.size() <= 512-17-1);
    // Block 7: Required projection
    // Bytes 0-16: The map units
    // Bytes 17-511: the proj parm info.
    pimpl_->seg_data.Put("                 \0",6*512,17);
    pimpl_->seg_data.Put(pimpl_->oMapUnit.c_str(),6*512,(int)pimpl_->oMapUnit.size());
    //19 because (511-17)/26 = 19 (26 is the size of one value)
    for(i=0 ; i < 19 ; i++)
    {
        pimpl_->seg_data.Put(pimpl_->oProjectionInfo[i],6*512+17+(i*26),26,"%20.14f");
    }

    WriteToFile(pimpl_->seg_data.buffer,0,data_size-1024);
    mbModified = false;
}

std::vector<double> CPCIDSKPolyModelSegment::GetXForwardCoefficients() const
{
    return pimpl_->vdfX1;
}

std::vector<double> CPCIDSKPolyModelSegment::GetYForwardCoefficients() const
{
    return pimpl_->vdfY1;
}

std::vector<double> CPCIDSKPolyModelSegment::GetXBackwardCoefficients() const
{
    return pimpl_->vdfX2;
}

std::vector<double> CPCIDSKPolyModelSegment::GetYBackwardCoefficients() const
{
    return pimpl_->vdfY2;
}

void CPCIDSKPolyModelSegment::SetCoefficients(const std::vector<double>& oXForward,
                                              const std::vector<double>& oYForward,
                                              const std::vector<double>& oXBackward,
                                              const std::vector<double>& oYBackward)
{
    assert(oXForward.size() == oYForward.size());
    assert(oYForward.size() == oXBackward.size());
    assert(oXBackward.size() == oYBackward.size());
    pimpl_->vdfX1 = oXForward;
    pimpl_->vdfY1 = oYForward;
    pimpl_->vdfX2 = oXBackward;
    pimpl_->vdfY2 = oYBackward;
    pimpl_->nNumCoeffs = (unsigned int)oXForward.size();
}

unsigned int CPCIDSKPolyModelSegment::GetPixels() const
{
    return pimpl_->nPixels;
}

unsigned int CPCIDSKPolyModelSegment::GetLines() const
{
    return pimpl_->nLines;
}

void CPCIDSKPolyModelSegment::SetRasterSize(unsigned int nLines,unsigned int nPixels)
{
    pimpl_->nPixels = nPixels;
    pimpl_->nLines = nLines;
}

std::string CPCIDSKPolyModelSegment::GetGeosysString() const
{
    return pimpl_->oMapUnit;
}

void CPCIDSKPolyModelSegment::SetGeosysString(const std::string& oGeosys)
{
    pimpl_->oMapUnit = oGeosys;
}

std::vector<double> CPCIDSKPolyModelSegment::GetProjParmInfo() const
{
    return pimpl_->oProjectionInfo;
}

void CPCIDSKPolyModelSegment::SetProjParmInfo(const std::vector<double>& oInfo)
{
    pimpl_->oProjectionInfo = oInfo;
    while(pimpl_->oProjectionInfo.size() < 19)
    {
        pimpl_->oProjectionInfo.push_back(0.0);
    }
}

void CPCIDSKPolyModelSegment::Synchronize()
{
    if(mbModified)
    {
        this->Write();
    }
}


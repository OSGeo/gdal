/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSK_TEX class.
 * 
 ******************************************************************************
 * Copyright (c) 2010
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

#include "pcidsk_exception.h"
#include "segment/cpcidsk_array.h"
#include "core/cpcidskfile.h"
#include <cstring>
#include <sstream>
#include <cassert>
#include "core/pcidsk_utils.h"

using namespace PCIDSK;

/************************************************************************/
/*                            CPCIDSK_ARRAY()                           */
/************************************************************************/

CPCIDSK_ARRAY::CPCIDSK_ARRAY( PCIDSKFile *file, int segment,
                              const char *segment_pointer )
        : CPCIDSKSegment( file, segment, segment_pointer ),
        loaded_(false),mbModified(false)
{
    MAX_DIMENSIONS = 8;
    Load();
}

/************************************************************************/
/*                            ~CPCIDSK_ARRAY                            */
/************************************************************************/

CPCIDSK_ARRAY::~CPCIDSK_ARRAY()

{
}

/**
 * Load the contents of the segment
 */
void CPCIDSK_ARRAY::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }

    PCIDSKBuffer& seg_header = this->GetHeader();
    seg_data.SetSize(GetContentSize());
    ReadFromFile(seg_data.buffer, 0, seg_data.buffer_size);

    if(std::strncmp(seg_header.buffer+160, "64R     ", 8))
    {
        seg_header.Put("64R     ",160,8);
        loaded_ = true;
        return ;
    }

    int nDimension = seg_header.GetInt(160+8,8);
    if(nDimension < 1 || nDimension > MAX_DIMENSIONS)
    {
        std::stringstream oStream;
        oStream << "Invalid array dimension " << nDimension;
        oStream << " stored in the segment.";
        std::string oMsg = oStream.str();
        throw PCIDSKException(oMsg.c_str());
    }
    mnDimension = static_cast<unsigned char>(nDimension);

    moSizes.clear();
    for( int i = 0; i < mnDimension; i++ )
    {
        int nSize = seg_header.GetInt(160+24 + i*8,8);
        if(nSize < 1)
        {
            std::stringstream oStream;
            oStream << "Invalid size " << nSize << " for dimension " << i+1;
            std::string oMsg = oStream.str();
            throw PCIDSKException(oMsg.c_str());
        }
		moSizes.push_back( nSize );
    }

    //calculate the total number of elements in the array.
    unsigned int nElements = 1;
    for(unsigned int i=0 ; i < moSizes.size() ; i++)
    {
        nElements *= moSizes[i];
    }

    for( unsigned int i = 0; i < nElements; i++ )
    {
        const double* pdValue = (const double*)seg_data.Get(i*8,8);
        char uValue[8];
        std::memcpy(uValue,pdValue,8);
        SwapData(uValue,8,1);
        double dValue;
        memcpy(&dValue, uValue, 8);
        moArray.push_back(dValue);
    }

    //PCIDSK doesn't have support for headers.

    // We've now loaded the structure up with data. Mark it as being loaded
    // properly.
    loaded_ = true;

}

/**
 * Write the segment on disk
 */
void CPCIDSK_ARRAY::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!loaded_) {
        return;
    }

    PCIDSKBuffer& seg_header = this->GetHeader();
    int nBlocks = (moArray.size()*8 + 511)/512 ;
    unsigned int nSizeBuffer = (nBlocks)*512 ;
    //64 values can be put into 512 bytes.
    unsigned int nRest = nBlocks*64 - moArray.size();

    seg_data.SetSize(nSizeBuffer);

    seg_header.Put("64R     ",160,8);
    seg_header.Put((int)mnDimension,160+8,8);

    for( int i = 0; i < mnDimension; i++ )
    {
        int nSize = static_cast<int>(moSizes[i]);
        seg_header.Put(nSize,160+24 + i*8,8);
    }

    for( unsigned int i = 0; i < moArray.size(); i++ )
    {
        double dValue = moArray[i];
        SwapData(&dValue,8,1);
        seg_data.PutBin(dValue,i*8);
    }

    //set the end of the buffer to 0.
    for( unsigned int i=0 ; i < nRest ; i++)
    {
        seg_data.Put(0.0,(moArray.size()+i)*8,8,"%22.14f");
    }

    WriteToFile(seg_data.buffer,0,seg_data.buffer_size);

    mbModified = false;
}

/**
 * Synchronize the segement, if it was modified then
 * write it into disk.
 */
void CPCIDSK_ARRAY::Synchronize()
{
    if(mbModified)
    {
        this->Write();
        //write the modified header
        file->WriteToFile( header.buffer, data_offset, 1024 );
    }
}

/**
 * This function returns the number of dimension in the array.
 * an array segment can have minimum 1 dimension and maximum
 * 8 dimension.
 *
 * @return the dimension of the array in [1,8]
 */
unsigned char CPCIDSK_ARRAY::GetDimensionCount() const 
{
    return mnDimension;
}

/**
 * This function set the dimension of the array. the dimension
 * must be in [1,8] or a pci::Exception is thrown.
 *
 * @param nDim number of dimension, should be in [1,8]
 */
void CPCIDSK_ARRAY::SetDimensionCount(unsigned char nDim) 
{
    if(nDim < 1 || nDim > 8)
    {
        throw PCIDSKException("An array cannot have a "
            "dimension bigger than 8 or smaller than 1.");
    }
    mnDimension = nDim;
    mbModified = true;
}

/**
 * Get the number of element that can be put in each of the dimension
 * of the array. the size of the return vector is GetDimensionCount().
 *
 * @return the size of each dimension.
 */
const std::vector<unsigned int>& CPCIDSK_ARRAY::GetSizes() const 
{
    return moSizes;
}

/**
 * Set the size of each dimension. If the size of the array is bigger
 * or smaller than GetDimensionCount(), then a pci::Exception is thrown
 * if one of the sizes is 0, then a pci::Exception is thrown.
 *
 * @param oSizes the size of each dimension 
 */
void CPCIDSK_ARRAY::SetSizes(const std::vector<unsigned int>& oSizes) 
{
    if(oSizes.size() != GetDimensionCount())
    {
        throw PCIDSKException("You need to specify the sizes"
            " for each dimension of the array");
    }

    for( unsigned int i=0 ; i < oSizes.size() ; i++)
    {
        if(oSizes[i] == 0)
        {
            throw PCIDSKException("You cannot define the size of a dimension to 0.");
        }
    }
    moSizes = oSizes;
    mbModified = true;
}

/**
 * Get the array in a vector. the size of this vector is
 * GetSize()[0]*GetSize()[2]*...*GetSize()[GetDimensionCount()-1].
 * value are stored in the following order inside this vector:
 * ViDj = Value i of Dimension j
 * n = size of dimension 1
 * p = size of dimension 2
 * h = size of dimension k
 *
 * V1D1 ... VnD1 V1D2 ... VpD2 ... V1Dk ... VhDk 
 *
 * @return the array.
 */
const std::vector<double>& CPCIDSK_ARRAY::GetArray() const 
{
    return moArray;
}

/**
 * Set the array in the segment. the size of this vector is
 * GetSize()[0]*GetSize()[2]*...*GetSize()[GetDimensionCount()-1].
 * value are stored in the following order inside this vector:
 * ViDj = Value i of Dimension j
 * n = size of dimension 1
 * p = size of dimension 2
 * h = size of dimension k
 *
 * V1D1 ... VnD1 V1D2 ... VpD2 ... V1Dk ... VhDk 
 *
 * If the size of oArray doesn't match the sizes and dimensions
 * then a pci::Exception is thrown.
 *
 * @param oArray the array.
 */
void CPCIDSK_ARRAY::SetArray(const std::vector<double>& oArray) 
{
    unsigned int nLength = 1;
    for( unsigned int i=0 ; i < moSizes.size() ; i++)
    {
        nLength *= moSizes[i];
    }

    if(nLength != oArray.size())
    {
        throw PCIDSKException("the size of this array doesn't match "
            "the size specified in GetSizes(). See documentation for"
            " more information.");
    }
    moArray = oArray;
    mbModified = true;
}

/**
 * Get the headers of this array. If no headers has be specified, then
 * this function return an empty vector.
 * the size of this vector should be equal to the size of the first dimension
 * returned by GetSize()[0]
 *
 * @return the headers.
 */
const std::vector<std::string>&  CPCIDSK_ARRAY::GetHeaders() const 
{
    return moHeaders;
}

/**
 * Set the headers of this array. An empty vector can be specified to clear
 * the headers in the segment.
 * the size of this vector should be equal to the size of the first dimension
 * returned by GetSize()[0]. If it is not the case, a pci::Exception is thrown.
 *
 * @param oHeaders the headers.
 */
void CPCIDSK_ARRAY::SetHeaders(const std::vector<std::string>& oHeaders) 
{
    moHeaders = oHeaders;
    mbModified = true;
}

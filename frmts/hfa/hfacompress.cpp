/******************************************************************************
 *
 * Name:     hfadataset.cpp
 * Project:  Erdas Imagine Driver
 * Purpose:  Imagine Compression code.
 * Author:   Sam Gillingham <sam.gillingham at nrm.qld.gov>
 *
 ******************************************************************************
 * Copyright (c) 2005, Sam Gillingham
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

#include "cpl_port.h"
#include "hfa_p.h"

#include <cstddef>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "hfa.h"

CPL_CVSID("$Id$")

HFACompress::HFACompress( void *pData, GUInt32 nBlockSize, EPTType eDataType ) :
    m_pData(pData),
    m_nBlockSize(nBlockSize),
    m_nBlockCount((nBlockSize * 8) / HFAGetDataTypeBits(eDataType)),
    m_eDataType(eDataType),
    m_nDataTypeNumBits(HFAGetDataTypeBits(eDataType)),
    m_pCounts(nullptr),
    m_pCurrCount(nullptr),
    m_nSizeCounts(0),
    m_pValues(nullptr),
    m_pCurrValues(nullptr),
    m_nSizeValues(0),
    m_nMin(0),
    m_nNumRuns(0),
    m_nNumBits(0)
{
    // Allocate some memory for the count and values - probably too big.
    // About right for worst case scenario.
    m_pCounts = static_cast<GByte *>(
        VSI_MALLOC_VERBOSE(m_nBlockCount * sizeof(GUInt32) + sizeof(GUInt32)));

    m_pValues = static_cast<GByte *>(
        VSI_MALLOC_VERBOSE(m_nBlockCount * sizeof(GUInt32) + sizeof(GUInt32)));
}

HFACompress::~HFACompress()
{
    // Free the compressed data.
    CPLFree(m_pCounts);
    CPLFree(m_pValues);
}

// Returns the number of bits needed to encode a count.
static GByte _FindNumBits( GUInt32 range )
{
    if( range < 0xff )
    {
        return 8;
    }

    if( range < 0xffff )
    {
        return 16;
    }

    return 32;
}

// Gets the value from the uncompressed block as a GUInt32 no matter
// the data type.
GUInt32 HFACompress::valueAsUInt32( GUInt32 iPixel )
{
    GUInt32 val = 0;

    if( m_nDataTypeNumBits == 8 )
    {
        val = ((GByte *)m_pData)[iPixel];
    }
    else if( m_nDataTypeNumBits == 16 )
    {
        val = ((GUInt16 *)m_pData)[iPixel];
    }
    else if( m_nDataTypeNumBits == 32 )
    {
        val = ((GUInt32 *)m_pData)[iPixel];
    }
    else if( m_nDataTypeNumBits == 4 )
    {
        if( iPixel % 2 == 0 )
            val = ((GByte *)m_pData)[iPixel / 2] & 0x0f;
        else
            val = (((GByte *)m_pData)[iPixel / 2] & 0xf0) >> 4;
    }
    else if( m_nDataTypeNumBits == 2 )
    {
        if( iPixel % 4 == 0 )
            val = ((GByte *)m_pData)[iPixel / 4] & 0x03;
        else if( iPixel % 4 == 1 )
            val = (((GByte *)m_pData)[iPixel / 4] & 0x0c) >> 2;
        else if( iPixel % 4 == 2 )
            val = (((GByte *)m_pData)[iPixel / 4] & 0x30) >> 4;
        else
            val = (((GByte *)m_pData)[iPixel / 4] & 0xc0) >> 6;
    }
    else if( m_nDataTypeNumBits == 1 )
    {
        if( ((GByte *)m_pData)[iPixel >> 3] & (0x1 << (iPixel & 0x07)) )
            val = 1;
        else
            val = 0;
    }
    else
    {
        // Should not get to here.  Check in compressBlock() should return false
        // if we can't compress this block because we don't know about the type.
        CPLError(CE_Failure, CPLE_FileIO,
                 "Imagine Datatype 0x%x (0x%x bits) not supported",
                 m_eDataType,
                 m_nDataTypeNumBits);
        CPLAssert(false);
    }

    return val;
}

// Finds the minimum value in a type specific fashion. This value is
// subtracted from each value in the compressed dataset. The maximum
// value is also found and the number of bits that the range can be stored
// is also returned.
//
// TODO: Minimum value returned as pNumBits is now 8 - Imagine
// can handle 1, 2, and 4 bits as well.
GUInt32 HFACompress::findMin( GByte *pNumBits )
{
    GUInt32 u32Min = valueAsUInt32(0);
    GUInt32 u32Max = u32Min;

    for( GUInt32 count = 1; count < m_nBlockCount; count++ )
    {
        GUInt32 u32Val = valueAsUInt32( count );
        if( u32Val < u32Min )
            u32Min = u32Val;
        else if( u32Val > u32Max )
            u32Max = u32Val;
    }

    *pNumBits = _FindNumBits(u32Max - u32Min);

    return u32Min;
}

// Codes the count in the way expected by Imagine - i.e. the lower 2 bits
// specify how many bytes the count takes up.
void HFACompress::makeCount( GUInt32 count, GByte *pCounter,
                             GUInt32 *pnSizeCount )
{
    // Because Imagine stores the number of bits used in the lower 2 bits of the
    // data it restricts what we can use.
    if( count < 0x40 )
    {
        pCounter[0] = static_cast<GByte>(count);
        *pnSizeCount = 1;
    }
    else if( count < 0x4000 )
    {
        pCounter[1] = count & 0xff;
        count /= 256;
        pCounter[0] = static_cast<GByte>(count | 0x40);
        *pnSizeCount = 2;
    }
    else if( count < 0x400000 )
    {
        pCounter[2] = count & 0xff;
        count /= 256;
        pCounter[1] = count & 0xff;
        count /= 256;
        pCounter[0] = static_cast<GByte>(count | 0x80);
        *pnSizeCount = 3;
    }
    else
    {
        pCounter[3] = count & 0xff;
        count /= 256;
        pCounter[2] = count & 0xff;
        count /= 256;
        pCounter[1] = count & 0xff;
        count /= 256;
        pCounter[0] = static_cast<GByte>(count | 0xc0);
        *pnSizeCount = 4;
    }
}

// Encodes the value depending on the number of bits we are using.
void HFACompress::encodeValue( GUInt32 val, GUInt32 repeat )
{
    GUInt32 nSizeCount = 0;

    makeCount(repeat, m_pCurrCount, &nSizeCount);
    m_pCurrCount += nSizeCount;
    if( m_nNumBits == 8 )
    {
        // Only storing 8 bits per value as the range is small.
        *(GByte*)m_pCurrValues = GByte(val - m_nMin);
        m_pCurrValues += sizeof(GByte);
    }
    else if( m_nNumBits == 16 )
    {
        // Only storing 16 bits per value as the range is small.
        *(GUInt16 *)m_pCurrValues = GUInt16(val - m_nMin);
#ifndef CPL_MSB
        CPL_SWAP16PTR(m_pCurrValues);
#endif  // ndef CPL_MSB
        m_pCurrValues += sizeof(GUInt16);
    }
    else
    {
        *(GUInt32 *)m_pCurrValues = GUInt32(val - m_nMin);
#ifndef CPL_MSB
        CPL_SWAP32PTR(m_pCurrValues);
#endif  // ndef CPL_MSB
        m_pCurrValues += sizeof(GUInt32);
    }
}

// This is the guts of the file - call this to compress the block returns false
// if the compression fails - i.e. compressed block bigger than input.
bool HFACompress::compressBlock()
{
    GUInt32 nLastUnique = 0;

    // Check we know about the datatype to be compressed.
    // If we can't compress it we should return false so that
    // the block cannot be compressed (we can handle just about
    // any type uncompressed).
    if( !QueryDataTypeSupported(m_eDataType) )
    {
        CPLDebug("HFA", "Cannot compress HFA datatype 0x%x (0x%x bits). "
                 "Writing uncompressed instead.",
                 m_eDataType,
                 m_nDataTypeNumBits);
        return false;
    }

    // Reset our pointers.
    m_pCurrCount = m_pCounts;
    m_pCurrValues = m_pValues;

    // Get the minimum value.  this can be subtracted from each value in
    // the image.
    m_nMin = findMin(&m_nNumBits);

    // Go through the block.
    GUInt32 u32Last = valueAsUInt32(0);
    for( GUInt32 count = 1; count < m_nBlockCount; count++ )
    {
        const GUInt32 u32Val = valueAsUInt32(count);
        if( u32Val != u32Last )
        {
            // The values have changed - i.e. a run has come to and end.
            encodeValue(u32Last, count - nLastUnique);

            if( (m_pCurrValues - m_pValues) > static_cast<int>(m_nBlockSize) )
            {
                return false;
            }

            m_nNumRuns++;
            u32Last = u32Val;
            nLastUnique = count;
        }
    }

    // We have done the block but have not got the last run because we
    // were only looking for a change in values.
    encodeValue(u32Last, m_nBlockCount - nLastUnique);
    m_nNumRuns++;

    // Set the size variables.
    m_nSizeCounts = static_cast<GUInt32>(m_pCurrCount - m_pCounts);
    m_nSizeValues = static_cast<GUInt32>(m_pCurrValues - m_pValues);

    // The 13 is for the header size - maybe this should live with some
    // constants somewhere?
    return (m_nSizeCounts + m_nSizeValues + 13) < m_nBlockSize;
}

bool HFACompress::QueryDataTypeSupported( EPTType eHFADataType )
{
    const int nBits = HFAGetDataTypeBits(eHFADataType);

    return
        nBits == 1 ||
        nBits == 2 ||
        nBits == 4 ||
        nBits == 8 ||
        nBits == 16 ||
        nBits == 32;
}

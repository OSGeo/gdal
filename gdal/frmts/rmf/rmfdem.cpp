/******************************************************************************
 *
 * Project:  Raster Matrix Format
 * Purpose:  Implementation of the ad-hoc compression algorithm used in
 *           GIS "Panorama"/"Integratsia".
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2009, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "cpl_conv.h"

#include "rmfdataset.h"

CPL_CVSID("$Id$");

/*
 * The encoded data stream is a series of records.
 *
 * Encoded record consist from the 1-byte record header followed by the
 * encoded data block. Header specifies the number of elements in the data
 * block and encoding type. Header format
 *
 * +---+---+---+---+---+---+---+---+
 * |   type    |       count       |
 * +---+---+---+---+---+---+---+---+
 *   7   6   5   4   3   2   1   0
 *
 * If count is zero then it means that there are more than 31 elements in this
 * record. Read the next byte in the stream and increase its value with 32 to
 * get the count. In this case maximum number of elements is 287.
 *
 * The "type" field specifies encoding type. It can be either difference
 * between the previous and the next data value (for the first element the
 * previous value is zero) or out-of-range codes.
 *
 * In case of "out of range" or "zero difference" values there are no more
 * elements in record after the header. Otherwise read as much encoded
 * elements as count specifies.
 */

// Encoding types
enum  RmfTypes {
    TYPE_OUT = 0x00,    // Value is out of range
    TYPE_ZERO = 0x20,   // Zero difference
    TYPE_INT4 = 0x40,   // Difference is 4-bit wide
    TYPE_INT8 = 0x60,   // Difference is 8-bit wide
    TYPE_INT12 = 0x80,  // Difference is 12-bit wide
    TYPE_INT16 = 0xA0,  // Difference is 16-bit wide
    TYPE_INT24 = 0xC0,  // Difference is 24-bit wide
    TYPE_INT32 = 0xE0   // Difference is 32-bit wide
};

// Encoding ranges
GInt32 RANGE_INT4 = 0x00000007L;    // 4-bit
GInt32 RANGE_INT12 = 0x000007FFL;    // 12-bit
GInt32 RANGE_INT24 = 0x007FFFFFL;    // 24-bit

// Out of range codes
GInt32 OUT_INT4 = 0xFFFFFFF8;
GInt32 OUT_INT8 = 0xFFFFFF80;
GInt32 OUT_INT12 = 0xFFFFF800;
GInt32 OUT_INT16 = 0xFFFF8000;
GInt32 OUT_INT24 = 0xFF800000;
GInt32 OUT_INT32 = 0x80000000;

// Inversion masks
GInt32 INV_INT4 = 0xFFFFFFF0L;
GInt32 INV_INT12 = 0xFFFFF000L;
GInt32 INV_INT24 = 0xFF000000L;

// Not sure which behaviour we wish for int32 overflow, so just do the
// addition as uint32 to workaround -ftrapv
static GInt32 AddInt32( GInt32& nTarget, GInt32 nVal )
{
    GUInt32 nTargetU = 0;
    memcpy(&nTargetU, &nTarget, 4);
    GUInt32 nValU = 0;
    memcpy(&nValU, &nVal, 4);
    nTargetU += nValU;
    memcpy(&nTarget, &nTargetU, 4);
    return nTarget;
}

/************************************************************************/
/*                           DEMDecompress()                            */
/************************************************************************/

int RMFDataset::DEMDecompress( const GByte* pabyIn, GUInt32 nSizeIn,
                               GByte* pabyOut, GUInt32 nSizeOut )
{
    if( pabyIn == NULL ||
        pabyOut == NULL ||
        nSizeOut < nSizeIn ||
        nSizeIn < 2 )
        return 0;

    GInt32 iPrev = 0;  // The last data value decoded.

    const char* pabyTempIn  = reinterpret_cast<const char *>(pabyIn);
    GInt32* paiOut = reinterpret_cast<GInt32 *>(pabyOut);
    nSizeOut /= sizeof(GInt32);

    while( nSizeIn > 0 )
    {
        // Read number of codes in the record and encoding type.
        GUInt32 nCount = *pabyTempIn & 0x1F;
        const GUInt32 nType = *pabyTempIn++ & 0xE0;  // The encoding type.
        nSizeIn--;
        if( nCount == 0 )
        {
            if( nSizeIn == 0 )
                break;
            nCount = 32 + *((unsigned char*)pabyTempIn++);
            nSizeIn--;
        }

        switch( nType )
        {
            case TYPE_ZERO:
                if( nSizeOut < nCount )
                    break;
                nSizeOut -= nCount;
                while( nCount-- > 0 )
                    *paiOut++ = iPrev;
                break;

            case TYPE_OUT:
                if( nSizeOut < nCount )
                    break;
                nSizeOut -= nCount;
                while( nCount-- > 0 )
                    *paiOut++ = OUT_INT32;
                break;

            case TYPE_INT4:
                if( nSizeIn < (nCount + 1) / 2 )
                    break;
                if( nSizeOut < nCount )
                    break;
                nSizeIn -= nCount / 2;
                nSizeOut -= nCount;
                while( nCount-- > 0 )
                {
    GInt32 nCode;
                    nCode = (*pabyTempIn) & 0x0F;
                    if( nCode > RANGE_INT4 )
                        nCode |= INV_INT4;
                    *paiOut++ = ( nCode == OUT_INT4 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);

                    if( nCount-- == 0 )
                    {
                        if( nSizeIn )
                        {
                            pabyTempIn++;
                            nSizeIn--;
                        }
                        break;
                    }

                    nCode = ((*pabyTempIn++)>>4) & 0x0F;
                    if( nCode > RANGE_INT4 )
                        nCode |= INV_INT4;
                    *paiOut++ = ( nCode == OUT_INT4 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);
                }
                break;

            case TYPE_INT8:
                if( nSizeIn < nCount )
                    break;
                if( nSizeOut < nCount )
                    break;
                nSizeIn -= nCount;
                nSizeOut -= nCount;
                while( nCount-- > 0 )
                {
    GInt32 nCode;
                    *paiOut++ = ( (nCode = *pabyTempIn++) == OUT_INT8 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);
                }
                break;

            case TYPE_INT12:
                if( nSizeIn < (3 * nCount + 1) / 2 )
                    break;
                if( nSizeOut < nCount )
                    break;
                nSizeIn -= 3 * nCount / 2;
                nSizeOut -= nCount;

                while( nCount-- > 0 )
                {
                    GInt32 nCode = *((GInt16*)pabyTempIn++) & 0x0FFF;
                    if( nCode > RANGE_INT12 )
                        nCode |= INV_INT12;
                    *paiOut++ = ( nCode == OUT_INT12 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);

                    if( nCount-- == 0 )
                    {
                        if( nSizeIn )
                        {
                            pabyTempIn++;
                            nSizeIn--;
                        }
                        break;
                    }

                    nCode = ( (*(GInt16*)pabyTempIn) >> 4 ) & 0x0FFF;
                    pabyTempIn += 2;
                    if( nCode > RANGE_INT12 )
                        nCode |= INV_INT12;
                    *paiOut++ = ( nCode == OUT_INT12 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);
                }
                break;

            case TYPE_INT16:
                if( nSizeIn < 2 * nCount )
                    break;
                if( nSizeOut < nCount )
                    break;
                nSizeIn -= 2 * nCount;
                nSizeOut -= nCount;

                while( nCount-- > 0 )
                {
                    const GInt32 nCode = *((GInt16*)pabyTempIn);
                    pabyTempIn += 2;
                    *paiOut++ = ( nCode == OUT_INT16 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);
                }
                break;

            case TYPE_INT24:
                if( nSizeIn < 3 * nCount )
                    break;
                if( nSizeOut < nCount )
                    break;
                nSizeIn -= 3 * nCount;
                nSizeOut -= nCount;

                while( nCount-- > 0 )
                {
                    GInt32 nCode = *((GInt32 *)pabyTempIn) & 0x00FFFFFF;
                    pabyTempIn += 3;
                    if( nCode > RANGE_INT24 )
                        nCode |= INV_INT24;
                    *paiOut++ = ( nCode == OUT_INT24 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);
                }
                break;

            case TYPE_INT32:
                if( nSizeIn < 4 * nCount )
                    break;
                if( nSizeOut < nCount )
                    break;
                nSizeIn -= 4 * nCount;
                nSizeOut -= nCount;

                while( nCount-- > 0 )
                {
                    GInt32 nCode = *(GInt32 *)pabyTempIn;
                    pabyTempIn += 4;
                    *paiOut++ = ( nCode == OUT_INT32 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);
                }
                break;
    }
  }

  return static_cast<int>((GByte*)paiOut - pabyOut);
}

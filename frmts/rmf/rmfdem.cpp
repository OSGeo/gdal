/******************************************************************************
 * $Id: rmflzw.cpp 11865 2007-08-09 11:53:57Z warmerdam $
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
#define TYPE_OUT    0x00    // Value is out of range
#define TYPE_ZERO   0x20    // Zero difference
#define TYPE_INT4   0x40    // Difference is 4-bit wide
#define TYPE_INT8   0x60    // Difference is 8-bit wide
#define TYPE_INT12  0x80    // Difference is 12-bit wide
#define TYPE_INT16  0xA0    // Difference is 16-bit wide
#define TYPE_INT24  0xC0    // Difference is 24-bit wide
#define TYPE_INT32  0xE0    // Difference is 32-bit wide

// Encoding ranges
#define RANGE_INT4  0x00000007L    // 4-bit
#define RANGE_INT12 0x000007FFL    // 12-bit
#define RANGE_INT24 0x007FFFFFL    // 24-bit

// Out of range codes
#define OUT_INT4    ((GInt32)0xFFFFFFF8)
#define OUT_INT8    ((GInt32)0xFFFFFF80)
#define OUT_INT12   ((GInt32)0xFFFFF800)
#define OUT_INT16   ((GInt32)0xFFFF8000)
#define OUT_INT24   ((GInt32)0xFF800000)
#define OUT_INT32   ((GInt32)0x80000000)

// Inversion masks
#define INV_INT4    0xFFFFFFF0L
#define INV_INT12   0xFFFFF000L
#define INV_INT24   0xFF000000L


/************************************************************************/
/*                           DEMDecompress()                            */
/************************************************************************/

int RMFDataset::DEMDecompress( const GByte* pabyIn, GUInt32 nSizeIn,
                               GByte* pabyOut, GUInt32 nSizeOut )
{
    GUInt32 nCount;             // Number of encoded data elements to read
    char* pabyTempIn;
    GInt32* paiOut;
    GInt32 nType;               // The encoding type
    GInt32 iPrev = 0;           // The last data value decoded
    GInt32 nCode;

    if ( pabyIn == 0 ||
         pabyOut == 0 ||
         nSizeOut < nSizeIn ||
         nSizeIn < 2 )
        return 0;

    pabyTempIn  = (char*)pabyIn;
    paiOut = (GInt32*)pabyOut;
    nSizeOut /= sizeof(GInt32);

    while ( nSizeIn > 0 )
    {
        // Read number of codes in the record and encoding type
        nCount = *pabyTempIn & 0x1F;
        nType = *pabyTempIn++ & 0xE0;
        nSizeIn--;
        if ( nCount == 0 )
        {
            if ( nSizeIn == 0 )
                break;
            nCount = 32 + *((unsigned char*)pabyTempIn++);
            nSizeIn--;
        }

        switch (nType)
        {
            case TYPE_ZERO:
                if ( nSizeOut < nCount )
                    break;
                nSizeOut -= nCount;
                while ( nCount-- > 0 )
                    *paiOut++ = iPrev;
                break;

            case TYPE_OUT:
                if ( nSizeOut < nCount )
                    break;
                nSizeOut -= nCount;
                while ( nCount-- > 0 )
                    *paiOut++ = OUT_INT32;
                break;

            case TYPE_INT4:
                if ( nSizeIn < nCount / 2 )
                    break;
                if ( nSizeOut < nCount )
                    break;
                nSizeIn -= nCount / 2;
                nSizeOut -= nCount;
                while ( nCount-- > 0 )
                {
                    nCode = (*pabyTempIn) & 0x0F;
                    if ( nCode > RANGE_INT4 )
                        nCode |= INV_INT4;
                    *paiOut++ = ( nCode == OUT_INT4 ) ?
                        OUT_INT32 : iPrev += nCode;

                    if ( nCount-- == 0 )
                    {
                        pabyTempIn++;
                        nSizeIn--;
                        break;
                    }

                    nCode = ((*pabyTempIn++)>>4) & 0x0F;
                    if ( nCode > RANGE_INT4 )
                        nCode |= INV_INT4;
                    *paiOut++ = ( nCode == OUT_INT4 ) ?
                        OUT_INT32 : iPrev += nCode;
                }
                break;

            case TYPE_INT8:
                if ( nSizeIn < nCount )
                    break;
                if ( nSizeOut < nCount )
                    break;
                nSizeIn -= nCount;
                nSizeOut -= nCount;
                while ( nCount-- > 0 )
                {
                    *paiOut++ = ( (nCode = *pabyTempIn++) == OUT_INT8 ) ?
                        OUT_INT32 : iPrev += nCode;
                }
                break;

            case TYPE_INT12:
                if ( nSizeIn < 3 * nCount / 2 )
                    break;
                if ( nSizeOut < nCount )
                    break;
                nSizeIn -= 3 * nCount / 2;
                nSizeOut -= nCount;

                while ( nCount-- > 0 )
                {
                    nCode = *((GInt16*)pabyTempIn++) & 0x0FFF;
                    if ( nCode > RANGE_INT12 )
                        nCode |= INV_INT12;
                    *paiOut++ = ( nCode == OUT_INT12 ) ?
                        OUT_INT32 : iPrev += nCode;

                    if ( nCount-- == 0 )
                    {
                        pabyTempIn++;
                        nSizeIn--;
                        break;
                    }

                    nCode = ( (*(GInt16*)pabyTempIn) >> 4 ) & 0x0FFF;
                    pabyTempIn += 2;
                    if ( nCode > RANGE_INT12 )
                        nCode |= INV_INT12;
                    *paiOut++ = ( nCode == OUT_INT12 ) ?
                        OUT_INT32 : iPrev += nCode;
                }
                break;

            case TYPE_INT16:
                if ( nSizeIn < 2 * nCount )
                    break;
                if ( nSizeOut < nCount )
                    break;
                nSizeIn -= 2 * nCount;
                nSizeOut -= nCount;

                while ( nCount-- > 0 )
                {
                    nCode = *((GInt16*)pabyTempIn);
                    pabyTempIn += 2;
                    *paiOut++ = ( nCode == OUT_INT16 ) ?
                        OUT_INT32 : iPrev += nCode;
                }
                break;

            case TYPE_INT24:
                if ( nSizeIn < 3 * nCount )
                    break;
                if ( nSizeOut < nCount )
                    break;
                nSizeIn -= 3 * nCount;
                nSizeOut -= nCount;

                while ( nCount-- > 0 )
                {
                    nCode =*((GInt32 *)pabyTempIn) & 0x0FFF;
                    pabyTempIn += 3;
                    if ( nCode > RANGE_INT24 )
                        nCode |= INV_INT24;
                    *paiOut++ = ( nCode == OUT_INT24 ) ?
                        OUT_INT32 : iPrev += nCode;
                }
                break;

            case TYPE_INT32:
                if ( nSizeIn < 4 * nCount )
                    break;
                if ( nSizeOut < nCount )
                    break;
                nSizeIn -= 4 * nCount;
                nSizeOut -= nCount;

                while ( nCount-- > 0 )
                {
                    nCode = *(GInt32 *)pabyTempIn;
                    pabyTempIn += 4;
                    *paiOut++ = ( nCode == OUT_INT32 ) ?
                        OUT_INT32 : iPrev += nCode;
                }
                break;
    }
  }

  return ((GByte*)paiOut - pabyOut);
}


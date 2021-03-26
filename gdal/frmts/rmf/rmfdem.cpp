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

CPL_CVSID("$Id$")

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

typedef GInt32  DEMWorkT;
typedef GInt64  DEMDiffT;

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
GInt32 RANGE_INT4 =  0x00000007L;    // 4-bit
GInt32 RANGE_INT8 =  0x0000007FL;    // 8-bit
GInt32 RANGE_INT12 = 0x000007FFL;    // 12-bit
GInt32 RANGE_INT16 = 0x00007FFFL;    // 16-bit
GInt32 RANGE_INT24 = 0x007FFFFFL;    // 24-bit

// Out of range codes
GInt32 OUT_INT4  = 0xFFFFFFF8;
GInt32 OUT_INT8  = 0xFFFFFF80;
GInt32 OUT_INT12 = 0xFFFFF800;
GInt32 OUT_INT16 = 0xFFFF8000;
GInt32 OUT_INT24 = 0xFF800000;
GInt32 OUT_INT32 = 0x80000000;

constexpr DEMDiffT DIFF_OUI_OF_RANGE = std::numeric_limits<DEMDiffT>::max();

// Inversion masks
GInt32 INV_INT4  = 0xFFFFFFF0L;
GInt32 INV_INT12 = 0xFFFFF000L;
GInt32 INV_INT24 = 0xFF000000L;

// Not sure which behavior we wish for int32 overflow, so just do the
// addition as uint32 to workaround -ftrapv
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static GInt32 AddInt32( GInt32& nTarget, GInt32 nVal )
{
    GUInt32 nTargetU = static_cast<GUInt32>(nTarget);
    GUInt32 nValU = static_cast<GUInt32>(nVal);
    nTargetU += nValU;
    memcpy(&nTarget, &nTargetU, 4);
    return nTarget;
}

/************************************************************************/
/*                           DEMDecompress()                            */
/************************************************************************/

size_t RMFDataset::DEMDecompress(const GByte* pabyIn, GUInt32 nSizeIn,
                                 GByte* pabyOut, GUInt32 nSizeOut ,
                                 GUInt32, GUInt32)
{
    if( pabyIn == nullptr ||
        pabyOut == nullptr ||
        nSizeOut < nSizeIn ||
        nSizeIn < 2 )
        return 0;

    GInt32 iPrev = 0;  // The last data value decoded.

    const signed char* pabyTempIn  = reinterpret_cast<const signed char *>(pabyIn);
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
            nCount = 32 + *((GByte*)pabyTempIn++);
            nSizeIn--;
        }

        switch( nType )
        {
            case TYPE_ZERO:
                if( nSizeOut < nCount )
                    break;
                nSizeOut -= nCount;
                while( nCount > 0 )
                {
                    nCount --;
                    *paiOut++ = iPrev;
                }
                break;

            case TYPE_OUT:
                if( nSizeOut < nCount )
                    break;
                nSizeOut -= nCount;
                while( nCount > 0 )
                {
                    nCount --;
                    *paiOut++ = OUT_INT32;
                }
                break;

            case TYPE_INT4:
                if( nSizeIn < (nCount + 1) / 2 )
                    break;
                if( nSizeOut < nCount )
                    break;
                nSizeIn -= nCount / 2;
                nSizeOut -= nCount;
                while( nCount > 0 )
                {
                    nCount --;
                    GInt32 nCode;
                    nCode = (*pabyTempIn) & 0x0F;
                    if( nCode > RANGE_INT4 )
                        nCode |= INV_INT4;
                    *paiOut++ = ( nCode == OUT_INT4 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);

                    if( nCount == 0 )
                    {
                        if( nSizeIn )
                        {
                            pabyTempIn++;
                            nSizeIn--;
                        }
                        break;
                    }
                    nCount --;

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
                while( nCount > 0 )
                {
                    nCount --;
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

                while( nCount > 0 )
                {
                    nCount --;
                    GInt32 nCode = CPL_LSBSINT16PTR(pabyTempIn) & 0x0FFF;
                    pabyTempIn += 1;
                    if( nCode > RANGE_INT12 )
                        nCode |= INV_INT12;
                    *paiOut++ = ( nCode == OUT_INT12 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);

                    if( nCount == 0 )
                    {
                        if( nSizeIn )
                        {
                            pabyTempIn++;
                            nSizeIn--;
                        }
                        break;
                    }
                    nCount --;

                    nCode = ( CPL_LSBSINT16PTR(pabyTempIn) >> 4 ) & 0x0FFF;
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

                while( nCount > 0 )
                {
                    nCount --;
                    const GInt32 nCode = CPL_LSBSINT16PTR(pabyTempIn);
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

                while( nCount > 0 )
                {
                    nCount --;
                    GInt32 nCode = (*(GByte*)pabyTempIn) |
                                   ((*(GByte*)(pabyTempIn+1)) << 8) |
                                   ((*(GByte*)(pabyTempIn+2)) << 16);
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

                while( nCount > 0 )
                {
                    nCount --;
                    GInt32 nCode = CPL_LSBSINT32PTR(pabyTempIn);
                    pabyTempIn += 4;
                    *paiOut++ = ( nCode == OUT_INT32 ) ?
                        OUT_INT32 : AddInt32(iPrev, nCode);
                }
                break;
    }
  }

  return (GByte*)paiOut - pabyOut;
}

/************************************************************************/
/*                            DEMWriteCode()                            */
/************************************************************************/

static CPLErr DEMWriteRecord(const DEMDiffT* paiRecord, RmfTypes eRecordType,
                             GUInt32 nRecordSize,GInt32 nSizeOut, GByte*& pabyCurrent)
{
    const GUInt32 nMaxCountInHeader = 31;
    GInt32        iCode;
    GInt32        iPrevCode;

    if(nRecordSize <= nMaxCountInHeader)
    {
        nSizeOut -= 1;
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        *pabyCurrent++ = static_cast<GByte>(eRecordType | nRecordSize);
    }
    else
    {
        nSizeOut -= 2;
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        *pabyCurrent++ = static_cast<GByte>(eRecordType);
        *pabyCurrent++ = static_cast<GByte>(nRecordSize - 32);
    }

    switch(eRecordType)
    {
    case TYPE_INT4:
        nSizeOut -= ((nRecordSize + 1) / 2);
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        for(GUInt32 n = 0; n != nRecordSize; ++n)
        {
            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                iCode = OUT_INT4;
            }
            else
            {
                iCode = static_cast<GInt32>(paiRecord[n]);
            }
            *pabyCurrent = static_cast<GByte>(iCode & 0x0F);

            ++n;
            if(n == nRecordSize)
            {
                pabyCurrent++;
                break;
            }

            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                iCode = OUT_INT4;
            }
            else
            {
                iCode = static_cast<GInt32>(paiRecord[n]);
            }
            *pabyCurrent++ |= static_cast<GByte>((iCode & 0x0F) << 4);
        }
        break;

    case TYPE_INT8:
        nSizeOut -= nRecordSize;
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        for(GUInt32 n = 0; n != nRecordSize; ++n)
        {
            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                *pabyCurrent++ = static_cast<GByte>(OUT_INT8);
            }
            else
            {
                *pabyCurrent++ = static_cast<GByte>(paiRecord[n]);
            }
        }
        break;

    case TYPE_INT12:
        nSizeOut -= ((nRecordSize * 3 + 1) / 2);
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        for(GUInt32 n = 0; n != nRecordSize; ++n)
        {
            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                iCode = OUT_INT12;
            }
            else
            {
                iCode = static_cast<GInt32>(paiRecord[n]);
            }

            iPrevCode = iCode;
            *pabyCurrent++ = static_cast<GByte>(iCode & 0x00FF);

            ++n;
            if(n == nRecordSize)
            {
                *pabyCurrent++ = static_cast<GByte>((iPrevCode & 0x0F00) >> 8);
                break;
            }

            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                iCode = OUT_INT12;
            }
            else
            {
                iCode = static_cast<GInt32>(paiRecord[n]);
            }
            iCode = (((iPrevCode & 0x0F00) >> 8) |
                     ((iCode & 0x0FFF) << 4));

            CPL_LSBPTR32(&iCode);
            memcpy(pabyCurrent, &iCode, 2);
            pabyCurrent += 2;
        }
        break;

    case TYPE_INT16:
        nSizeOut -= (nRecordSize * 2);
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        for(GUInt32 n = 0; n != nRecordSize; ++n)
        {
            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                iCode = OUT_INT16;
            }
            else
            {
                iCode = static_cast<GInt32>(paiRecord[n]);
            }
            CPL_LSBPTR32(&iCode);
            memcpy(pabyCurrent, &iCode, 2);
            pabyCurrent += 2;
        }
        break;

    case TYPE_INT24:
        nSizeOut -= (nRecordSize * 3);
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        for(GUInt32 n = 0; n != nRecordSize; ++n)
        {
            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                iCode = OUT_INT24;
            }
            else
            {
                iCode = static_cast<GInt32>(paiRecord[n]);
            }
            CPL_LSBPTR32(&iCode);
            memcpy(pabyCurrent, &iCode, 3);
            pabyCurrent += 3;
        }
        break;

    case TYPE_INT32:
        nSizeOut -= (nRecordSize * 4);
        if(nSizeOut <= 0)
        {
            return CE_Failure;
        }

        for(GUInt32 n = 0; n != nRecordSize; ++n)
        {
            if(paiRecord[n] == DIFF_OUI_OF_RANGE)
            {
                iCode = OUT_INT32;
            }
            else
            {
                iCode = static_cast<GInt32>(paiRecord[n]);
            }
            CPL_LSBPTR32(&iCode);
            memcpy(pabyCurrent, &iCode, 4);
            pabyCurrent += 4;
        }
        break;

    case TYPE_ZERO:
    case TYPE_OUT:
        break;

    default:
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             DEMDeltaType()                           */
/************************************************************************/

static RmfTypes DEMDeltaType(DEMDiffT delta)
{
    if(delta <= RANGE_INT12)
    {
        if(delta <= RANGE_INT4)
        {
            if(delta == 0)
            {
                return TYPE_ZERO;
            }
            else
            {
                return TYPE_INT4;
            }
        }
        else
        {
            if(delta <= RANGE_INT8)
            {
                return TYPE_INT8;
            }
            else
            {
                return TYPE_INT12;
            }
        }
    }
    else
    {
        if(delta <= RANGE_INT24)
        {
            if(delta <= RANGE_INT16)
            {
                return TYPE_INT16;
            }
            else
            {
                return TYPE_INT24;
            }
        }
        else
        {
            return TYPE_INT32;
        }
    }
}

/************************************************************************/
/*                             DEMCompress()                            */
/************************************************************************/

size_t RMFDataset::DEMCompress(const GByte* pabyIn, GUInt32 nSizeIn,
                               GByte* pabyOut, GUInt32 nSizeOut ,
                               GUInt32, GUInt32, const RMFDataset* poDS)
{
    if( pabyIn == nullptr ||
        pabyOut == nullptr ||
        nSizeIn < sizeof(DEMWorkT) )
        return 0;

    const GUInt32 anDeltaTypeSize[8] = {0,0,4,8,12,16,24,32};
    const GUInt32 nMaxRecordSize = 255 + 32;

    DEMWorkT iMin((poDS == nullptr) ? std::numeric_limits<DEMWorkT>::min() :
                  static_cast<DEMWorkT>(poDS->sHeader.adfElevMinMax[0]));

    GUInt32     nLessCount = 0;
    GUInt32     nRecordSize = 0;
    RmfTypes    eRecordType = TYPE_OUT;
    DEMDiffT    aiRecord[nMaxRecordSize] = {0};
    DEMWorkT    aiPrev[nMaxRecordSize] = {0};

    GByte*      pabyCurrent = pabyOut;
    DEMWorkT    iPrev = 0;

    nSizeIn = nSizeIn/sizeof(DEMWorkT);

    const DEMWorkT* paiIn = reinterpret_cast<const DEMWorkT*>(pabyIn);
    const DEMWorkT* paiInEnd = paiIn + nSizeIn;

    while(true)
    {
        GUInt32     nRecordElementSize = 0;

        if(paiIn >= paiInEnd)
        {
            if(nRecordSize == 0)
            {
                return pabyCurrent - pabyOut;
            }

            if( CE_None != DEMWriteRecord(aiRecord, eRecordType, nRecordSize,
                                          nSizeOut, pabyCurrent))
            {
                return 0;
            }
            nRecordSize = 0;
            continue;
        }

        DEMWorkT iCurr = *(paiIn++);
        RmfTypes eCurrType;

        if(iCurr < iMin)
        {
            eCurrType = TYPE_OUT;
            aiRecord[nRecordSize] = DIFF_OUI_OF_RANGE;
            aiPrev[nRecordSize] = iPrev;
        }
        else
        {
            DEMDiffT  delta = static_cast<DEMDiffT>(iCurr) -
                              static_cast<DEMDiffT>(iPrev);

            aiRecord[nRecordSize] = delta;
            aiPrev[nRecordSize] = iCurr;

            if(delta < 0)
                delta = -delta;

            eCurrType = DEMDeltaType(delta);
            iPrev = iCurr;
        }
        nRecordSize++;

        if(nRecordSize == 1)
        {
            eRecordType = eCurrType;
            //nRecordElementSize = anDeltaTypeSize[eCurrType >> 5];
            continue;
        }

        if(nRecordSize == nMaxRecordSize)
        {
            nLessCount = 0;
            if( CE_None != DEMWriteRecord(aiRecord, eRecordType, nRecordSize,
                                          nSizeOut, pabyCurrent))
            {
                return 0;
            }
            iPrev = aiPrev[nRecordSize - 1];
            nRecordSize = 0;
            continue;
        }

        if(eCurrType == eRecordType)
        {
            nLessCount = 0;
            continue;
        }

        if((eCurrType > eRecordType) || (eCurrType | eRecordType) == TYPE_ZERO)
        {
            --paiIn;
            if( CE_None != DEMWriteRecord(aiRecord, eRecordType,
                                          nRecordSize - 1,
                                          nSizeOut, pabyCurrent))
            {
                return 0;
            }
            iPrev = aiPrev[nRecordSize - 2];
            nRecordSize = 0;
            nLessCount = 0;
            continue;
        }

        nLessCount++;

        GUInt32 nDeltaSize(anDeltaTypeSize[eCurrType >> 5]);
        if(nRecordElementSize < nDeltaSize ||
           (nRecordElementSize - nDeltaSize) * nLessCount < 16)
        {
            continue;
        }

        paiIn -= nLessCount;
        if( CE_None != DEMWriteRecord(aiRecord, eRecordType,
                                      nRecordSize - nLessCount,
                                      nSizeOut, pabyCurrent))
        {
            return 0;
        }
        iPrev = aiPrev[nRecordSize - nLessCount - 1];
        nRecordSize = 0;
        nLessCount = 0;
    }

    return 0;
}

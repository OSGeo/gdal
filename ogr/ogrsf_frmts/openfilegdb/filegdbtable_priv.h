/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB tables
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef FILEGDBTABLE_PRIV_H_INCLUDED
#define FILEGDBTABLE_PRIV_H_INCLUDED

#include "filegdbtable.h"
#include "cpl_error.h"
#include "cpl_time.h"

#include <algorithm>
#include <cwchar>
#include <vector>

#define DIV_ROUND_UP(a, b) ( ((a) % (b)) == 0 ? ((a) / (b)) : (((a) / (b)) + 1) )

#define TEST_BIT(ar, bit)                       (ar[(bit) / 8] & (1 << ((bit) % 8)))
#define BIT_ARRAY_SIZE_IN_BYTES(bitsize)        (((bitsize)+7)/8)

namespace OpenFileGDB
{

/************************************************************************/
/*                              GetInt16()                              */
/************************************************************************/

inline GInt16 GetInt16(const GByte* pBaseAddr, int iOffset)
{
    GInt16 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR16(&nVal);
    return nVal;
}

/************************************************************************/
/*                              GetUInt16()                             */
/************************************************************************/

inline GUInt16 GetUInt16(const GByte* pBaseAddr, int iOffset)
{
    GUInt16 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR16(&nVal);
    return nVal;
}

/************************************************************************/
/*                              GetInt32()                              */
/************************************************************************/

inline GInt32 GetInt32(const GByte* pBaseAddr, int iOffset)
{
    GInt32 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR32(&nVal);
    return nVal;
}

/************************************************************************/
/*                              GetUInt32()                             */
/************************************************************************/

inline GUInt32 GetUInt32(const GByte* pBaseAddr, int iOffset)
{
    GUInt32 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR32(&nVal);
    return nVal;
}

/************************************************************************/
/*                              GetUInt64()                             */
/************************************************************************/

inline uint64_t GetUInt64(const GByte* pBaseAddr, int iOffset)
{
    uint64_t nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR64(&nVal);
    return nVal;
}

/************************************************************************/
/*                             GetFloat32()                             */
/************************************************************************/

inline float GetFloat32(const GByte* pBaseAddr, int iOffset)
{
    float fVal;
    memcpy(&fVal, pBaseAddr + sizeof(fVal) * iOffset, sizeof(fVal));
    CPL_LSBPTR32(&fVal);
    return fVal;
}

/************************************************************************/
/*                             GetFloat64()                             */
/************************************************************************/

inline double GetFloat64(const GByte* pBaseAddr, int iOffset)
{
    double dfVal;
    memcpy(&dfVal, pBaseAddr + sizeof(dfVal) * iOffset, sizeof(dfVal));
    CPL_LSBPTR64(&dfVal);
    return dfVal;
}

/************************************************************************/
/*                          ReadUInt32()                                */
/************************************************************************/

inline bool ReadUInt32(VSILFILE* fp, uint32_t& nVal)
{
    const bool bRet = VSIFReadL(&nVal, 1, sizeof(nVal), fp) == sizeof(nVal);
    CPL_LSBPTR32(&nVal);
    return bRet;
}

/************************************************************************/
/*                          WriteUInt32()                               */
/************************************************************************/

inline bool WriteUInt32(VSILFILE* fp, uint32_t nVal)
{
    CPL_LSBPTR32(&nVal);
    return VSIFWriteL(&nVal, 1, sizeof(nVal), fp) == sizeof(nVal);
}

/************************************************************************/
/*                          WriteUInt64()                               */
/************************************************************************/

inline bool WriteUInt64(VSILFILE* fp, uint64_t nVal)
{
    CPL_LSBPTR64(&nVal);
    return VSIFWriteL(&nVal, 1, sizeof(nVal), fp) == sizeof(nVal);
}

/************************************************************************/
/*                          WriteFloat64()                               */
/************************************************************************/

inline bool WriteFloat64(VSILFILE* fp, double dfVal)
{
    CPL_LSBPTR64(&dfVal);
    return VSIFWriteL(&dfVal, 1, sizeof(dfVal), fp) == sizeof(dfVal);
}

/************************************************************************/
/*                          WriteUInt32()                               */
/************************************************************************/

inline void WriteUInt32(std::vector<GByte>& abyBuffer, uint32_t nVal, size_t nPos = static_cast<size_t>(-1))
{
    CPL_LSBPTR32(&nVal);
    const GByte* pabyInput = reinterpret_cast<const GByte*>(&nVal);
    if( nPos == static_cast<size_t>(-1) )
        abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + sizeof(nVal));
    else
        memcpy(&abyBuffer[nPos], pabyInput, sizeof(nVal));
}

/************************************************************************/
/*                          WriteFloat32()                               */
/************************************************************************/

inline void WriteFloat32(std::vector<GByte>& abyBuffer, float fVal)
{
    CPL_LSBPTR32(&fVal);
    const GByte* pabyInput = reinterpret_cast<const GByte*>(&fVal);
    abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + sizeof(fVal));
}

/************************************************************************/
/*                          WriteFloat64()                               */
/************************************************************************/

inline void WriteFloat64(std::vector<GByte>& abyBuffer, double dfVal)
{
    CPL_LSBPTR64(&dfVal);
    const GByte* pabyInput = reinterpret_cast<const GByte*>(&dfVal);
    abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + sizeof(dfVal));
}

/************************************************************************/
/*                          WriteInt32()                                */
/************************************************************************/

inline void WriteInt32(std::vector<GByte>& abyBuffer, int32_t nVal)
{
    CPL_LSBPTR32(&nVal);
    const GByte* pabyInput = reinterpret_cast<const GByte*>(&nVal);
    abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + sizeof(nVal));
}

/************************************************************************/
/*                          WriteUInt16()                               */
/************************************************************************/

inline void WriteUInt16(std::vector<GByte>& abyBuffer, uint16_t nVal)
{
    CPL_LSBPTR16(&nVal);
    const GByte* pabyInput = reinterpret_cast<const GByte*>(&nVal);
    abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + sizeof(nVal));
}

/************************************************************************/
/*                          WriteInt16()                                */
/************************************************************************/

inline void WriteInt16(std::vector<GByte>& abyBuffer, int16_t nVal)
{
    CPL_LSBPTR16(&nVal);
    const GByte* pabyInput = reinterpret_cast<const GByte*>(&nVal);
    abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + sizeof(nVal));
}

/************************************************************************/
/*                          WriteUInt8()                                */
/************************************************************************/

inline void WriteUInt8(std::vector<GByte>& abyBuffer, uint8_t nVal)
{
    abyBuffer.push_back(nVal);
}

/************************************************************************/
/*                          WriteUInt64()                               */
/************************************************************************/

inline void WriteUInt64(std::vector<GByte>& abyBuffer, uint64_t nVal)
{
    CPL_LSBPTR64(&nVal);
    const GByte* pabyInput = reinterpret_cast<const GByte*>(&nVal);
    abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + sizeof(nVal));
}

/************************************************************************/
/*                             WriteVarUInt()                           */
/************************************************************************/

inline void WriteVarUInt(std::vector<GByte>& abyBuffer, uint64_t nVal)
{
    while( true )
    {
        if( nVal >= 0x80 )
        {
            WriteUInt8(abyBuffer, static_cast<uint8_t>(0x80 | (nVal & 0x7F)));
            nVal >>= 7;
        }
        else
        {
            WriteUInt8(abyBuffer, static_cast<uint8_t>(nVal));
            break;
        }
    }
}

/************************************************************************/
/*                             WriteVarInt()                            */
/************************************************************************/

inline void WriteVarInt(std::vector<GByte>& abyBuffer, int64_t nVal)
{
    uint64_t nUVal;
    if( nVal < 0 )
    {
        if( nVal == std::numeric_limits<int64_t>::min() )
            nUVal = static_cast<uint64_t>(1) << 63;
        else
            nUVal = -nVal;
        if( nUVal >= 0x40 )
        {
            WriteUInt8(abyBuffer, static_cast<uint8_t>(0x80 | 0x40 | (nUVal & 0x3F)));
            nUVal >>= 6;
        }
        else
        {
            WriteUInt8(abyBuffer, static_cast<uint8_t>(0x40 | (nUVal & 0x3F)));
            return;
        }
    }
    else
    {
        nUVal = nVal;
        if( nUVal >= 0x40 )
        {
            WriteUInt8(abyBuffer, static_cast<uint8_t>(0x80 | (nUVal & 0x3F)));
            nUVal >>= 6;
        }
        else
        {
            WriteUInt8(abyBuffer, static_cast<uint8_t>((nUVal & 0x3F)));
            return;
        }
    }

    WriteVarUInt(abyBuffer, nUVal);
}

/************************************************************************/
/*                           WriteUTF16String()                         */
/************************************************************************/

enum UTF16StringFormat
{
    NUMBER_OF_BYTES_ON_UINT16,
    NUMBER_OF_BYTES_ON_VARUINT,
    NUMBER_OF_CHARS_ON_UINT8,
    NUMBER_OF_CHARS_ON_UINT32,
};

inline void WriteUTF16String(std::vector<GByte>& abyBuffer,
                             const char* pszStr,
                             UTF16StringFormat eFormat)
{
    wchar_t* pszWStr = CPLRecodeToWChar(pszStr, CPL_ENC_UTF8, CPL_ENC_UCS2);
    size_t nWLen = wcslen(pszWStr);
    switch( eFormat )
    {
        case NUMBER_OF_BYTES_ON_UINT16:
        {
            // Write length as bytes
            const auto nLenToWrite = std::min(static_cast<size_t>(65534),
                                              sizeof(uint16_t) * nWLen);
            if( nLenToWrite < sizeof(uint16_t) * nWLen )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "String %s truncated to %u bytes",
                         pszStr, static_cast<uint32_t>(nLenToWrite));
                nWLen = nLenToWrite / sizeof(uint16_t);
            }
            WriteUInt16(abyBuffer, static_cast<uint16_t>(nLenToWrite));
            break;
        }

        case NUMBER_OF_BYTES_ON_VARUINT:
        {
            // Write length as bytes
            WriteVarUInt(abyBuffer, sizeof(uint16_t) * nWLen);
            break;
        }

        case NUMBER_OF_CHARS_ON_UINT8:
        {
            // Write length as number of UTF16 characters
            const auto nLenToWrite = std::min(static_cast<size_t>(255), nWLen);
            if( nLenToWrite < nWLen )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "String %s truncated to %u UTF16 characters",
                         pszStr, static_cast<uint32_t>(nLenToWrite));
                nWLen = nLenToWrite;
            }
            WriteUInt8(abyBuffer, static_cast<uint8_t>(nLenToWrite));
            break;
        }

        case NUMBER_OF_CHARS_ON_UINT32:
        {
            // Write length as number of UTF16 characters
            WriteUInt32(abyBuffer, static_cast<uint32_t>(nWLen));
            break;
        }
    }

    if( nWLen )
    {
        std::vector<uint16_t> anChars(nWLen);
        for( size_t i = 0; i < nWLen; ++i )
        {
            anChars[i] = static_cast<uint16_t>(pszWStr[i]);
            CPL_LSBPTR16(&anChars[i]);
        }
        const GByte* pabyInput = reinterpret_cast<const GByte*>(anChars.data());
        abyBuffer.insert(abyBuffer.end(), pabyInput, pabyInput + nWLen * sizeof(uint16_t));
    }
    CPLFree(pszWStr);
}

/************************************************************************/
/*                      FileGDBOGRDateToDoubleDate()                    */
/************************************************************************/

inline double FileGDBOGRDateToDoubleDate(const OGRField* psField)
{
    struct tm brokendowntime;
    brokendowntime.tm_year = psField->Date.Year - 1900;
    brokendowntime.tm_mon = psField->Date.Month - 1;
    brokendowntime.tm_mday = psField->Date.Day;
    brokendowntime.tm_hour = psField->Date.Hour;
    brokendowntime.tm_min = psField->Date.Minute;
    brokendowntime.tm_sec = static_cast<int>(psField->Date.Second + 0.5);
    GIntBig nUnixTime = CPLYMDHMSToUnixTime(&brokendowntime);
    if( psField->Date.TZFlag > 1 && psField->Date.TZFlag != 100 )
    {
        // Convert to GMT
        const int TZOffset = std::abs(psField->Date.TZFlag - 100) * 15;
        const int TZHour = TZOffset / 60;
        const int TZMinute = TZOffset - TZHour * 60;
        const int nOffset = TZHour * 3600 + TZMinute * 60;
        if( psField->Date.TZFlag >= 100 )
            nUnixTime -= nOffset;
        else
            nUnixTime += nOffset;
    }
    // 25569: Number of days between 1899/12/30 00:00:00 and 1970/01/01 00:00:00
    return static_cast<double>(nUnixTime) / 3600.0 / 24.0 + 25569.0;
}

void FileGDBTablePrintError(const char* pszFile, int nLineNumber);

#define PrintError()        FileGDBTablePrintError(__FILE__, __LINE__)

/************************************************************************/
/*                          returnError()                               */
/************************************************************************/

#define returnError() \
    do { PrintError(); return (errorRetValue); } while(0)

/************************************************************************/
/*                         returnErrorIf()                              */
/************************************************************************/

#define returnErrorIf(expr) \
    do { if( (expr) ) returnError(); } while(0)

/************************************************************************/
/*                       returnErrorAndCleanupIf()                      */
/************************************************************************/

#define returnErrorAndCleanupIf(expr, cleanup) \
    do { if( (expr) ) { cleanup; returnError(); } } while(0)

} /* namespace OpenFileGDB */

#endif /* FILEGDBTABLE_PRIV_H_INCLUDED */

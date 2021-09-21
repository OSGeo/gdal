/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#include "io.h"

#include <iostream>
#include <cstdint>
#include <cstring>

const size_t DWGConstants::SentinelLength = 16;
const char * DWGConstants::HeaderVariablesStart
            = "\xCF\x7B\x1F\x23\xFD\xDE\x38\xA9\x5F\x7C\x68\xB8\x4E\x6D\x33\x5F";
const char * DWGConstants::HeaderVariablesEnd
            = "\x30\x84\xE0\xDC\x02\x21\xC7\x56\xA0\x83\x97\x47\xB1\x92\xCC\xA0";

const char * DWGConstants::DSClassesStart
                        = "\x8D\xA1\xC4\xB8\xC4\xA9\xF8\xC5\xC0\xDC\xF4\x5F\xE7\xCF\xB6\x8A";
const char * DWGConstants::DSClassesEnd
                        = "\x72\x5E\x3B\x47\x3B\x56\x07\x3A\x3F\x23\x0B\xA0\x18\x30\x49\x75";

const int DWGCRC8Table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241, 0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1,
    0xC481, 0x0440, 0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40, 0x0A00, 0xCAC1, 0xCB81, 0x0B40,
    0xC901, 0x09C0, 0x0880, 0xC841, 0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40, 0x1E00, 0xDEC1,
    0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41, 0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040, 0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1,
    0xF281, 0x3240, 0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441, 0x3C00, 0xFCC1, 0xFD81, 0x3D40,
    0xFF01, 0x3FC0, 0x3E80, 0xFE41, 0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840, 0x2800, 0xE8C1,
    0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41, 0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640, 0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0,
    0x2080, 0xE041, 0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240, 0x6600, 0xA6C1, 0xA781, 0x6740,
    0xA501, 0x65C0, 0x6480, 0xA441, 0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41, 0xAA01, 0x6AC0,
    0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840, 0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40, 0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1,
    0xB681, 0x7640, 0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041, 0x5000, 0x90C1, 0x9181, 0x5140,
    0x9301, 0x53C0, 0x5280, 0x9241, 0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440, 0x9C01, 0x5CC0,
    0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40, 0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40, 0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0,
    0x4C80, 0x8C41, 0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641, 0x8201, 0x42C0, 0x4380, 0x8341,
    0x4100, 0x81C1, 0x8081, 0x4040
};

unsigned short CalculateCRC8( unsigned short initialVal, const char *ptr, int num )
{
    unsigned char al;
    while( num-- > 0 )
    {
        al = static_cast<unsigned char>( ( *ptr ) ^
                                         ( static_cast<char>( initialVal & 0xFF ) ) );
        initialVal = ( initialVal >> 8 ) & 0xFF;
        initialVal = static_cast<unsigned short>( initialVal ^ DWGCRC8Table[al & 0xFF] );
        ptr++;
    }

    return initialVal;
}

//------------------------------------------------------------------------------
// CADBuffer
//------------------------------------------------------------------------------

CADBuffer::CADBuffer(size_t size) : m_nBitOffsetFromStart(0)
{
    m_pBuffer = new char[size];
    // zero memory
    memset(m_pBuffer, 0, size);
    m_nSize = size;
}

CADBuffer::~CADBuffer()
{
    delete [] m_pBuffer;
}

void CADBuffer::WriteRAW(const void *data, size_t size)
{
    memcpy(m_pBuffer, data, size);
    m_nBitOffsetFromStart += size * 8;
}

unsigned char CADBuffer::Read2B()
{
    unsigned char result           = 0;
    size_t        nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 2 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }

    unsigned char a2BBytes[2];
    memcpy( a2BBytes, m_pBuffer + nByteOffset, 2 );

    switch( nBitOffsetInByte )
    {
        case 7:
            result  = ( a2BBytes[0] & binary( 00000001 ) ) << 1;
            result |= ( a2BBytes[1] & binary( 10000000 ) ) >> 7;
            break;
        default:
            result = ( a2BBytes[0] >> ( 6 - nBitOffsetInByte ) );
            break;
    }

    result &= binary( 00000011 );
    m_nBitOffsetFromStart += 2;

    return result;
}

unsigned char CADBuffer::Read3B()
{
    unsigned char result           = 0;
    size_t        nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 2 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }

    unsigned char a3BBytes[2];
    memcpy( a3BBytes, m_pBuffer + nByteOffset, 2 );

    switch( nBitOffsetInByte )
    {
        case 6:
            result  = ( a3BBytes[0] & binary( 00000011 ) ) << 1;
            result |= ( a3BBytes[1] & binary( 10000000 ) ) >> 7;
            break;

        case 7:
            result  = ( a3BBytes[0] & binary( 00000001 ) ) << 2;
            result |= ( a3BBytes[1] & binary( 11000000 ) ) >> 6;
            break;

        default:
            result = ( a3BBytes[0] >> ( 5 - nBitOffsetInByte ) );
            break;
    }

    result &= binary( 00000111 );
    m_nBitOffsetFromStart += 3;

    return result;
}

unsigned char CADBuffer::Read4B()
{
    unsigned char result           = 0;
    size_t        nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 2 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }

    unsigned char a4BBytes[2];
    memcpy( a4BBytes, m_pBuffer + nByteOffset, 2 );

    switch( nBitOffsetInByte )
    {
        case 5:
            result  = ( a4BBytes[0] & binary( 00000111 ) ) << 1;
            result |= ( a4BBytes[1] & binary( 10000000 ) ) >> 7;
            break;
        case 6:
            result  = ( a4BBytes[0] & binary( 00000011 ) ) << 2;
            result |= ( a4BBytes[1] & binary( 11000000 ) ) >> 6;
            break;

        case 7:
            result  = ( a4BBytes[0] & binary( 00000001 ) ) << 3;
            result |= ( a4BBytes[1] & binary( 11100000 ) ) >> 5;
            break;

        default:
            result = ( a4BBytes[0] >> ( 4 - nBitOffsetInByte ) );
            break;
    }

    result &= binary( 00001111 );
    m_nBitOffsetFromStart += 4;

    return result;
}


double CADBuffer::ReadBITDOUBLE()
{
    unsigned char BITCODE = Read2B();

    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 9 > m_nSize)
    {
        m_bEOB = true;
        return 0.0;
    }

    unsigned char aDoubleBytes[9]; // maximum bytes a single double can take.
    memcpy( aDoubleBytes, m_pBuffer + nByteOffset, 9 );

    switch( BITCODE )
    {
        case BITDOUBLE_NORMAL:
        {
            aDoubleBytes[0] <<= nBitOffsetInByte;
            aDoubleBytes[0] |= ( aDoubleBytes[1] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[1] <<= nBitOffsetInByte;
            aDoubleBytes[1] |= ( aDoubleBytes[2] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[2] <<= nBitOffsetInByte;
            aDoubleBytes[2] |= ( aDoubleBytes[3] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[3] <<= nBitOffsetInByte;
            aDoubleBytes[3] |= ( aDoubleBytes[4] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[4] <<= nBitOffsetInByte;
            aDoubleBytes[4] |= ( aDoubleBytes[5] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[5] <<= nBitOffsetInByte;
            aDoubleBytes[5] |= ( aDoubleBytes[6] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[6] <<= nBitOffsetInByte;
            aDoubleBytes[6] |= ( aDoubleBytes[7] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[7] <<= nBitOffsetInByte;
            aDoubleBytes[7] |= ( aDoubleBytes[8] >> ( 8 - nBitOffsetInByte ) );

            m_nBitOffsetFromStart += 64;

            double result;
            memcpy(&result, aDoubleBytes, sizeof(result));
            FromLSB(result);
            return result;
        }

        case BITDOUBLE_ONE_VALUE:
        {
            m_nBitOffsetFromStart += 0;

            return 1.0f;
        }

        case BITDOUBLE_ZERO_VALUE:
        {
            m_nBitOffsetFromStart += 0;

            return 0.0f;
        }

        case BITDOUBLE_NOT_USED:
        {
            m_nBitOffsetFromStart += 0;

            return 0.0f;
        }
    }

    return 0.0f;
}

void CADBuffer::SkipBITDOUBLE()
{
    unsigned char BITCODE = Read2B();
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    if(nByteOffset + 9 > m_nSize)
    {
        m_bEOB = true;
        return;
    }

    switch( BITCODE )
    {
        case BITDOUBLE_NORMAL:
            m_nBitOffsetFromStart += 64;
            break;
        case BITDOUBLE_ONE_VALUE:
            m_nBitOffsetFromStart += 0;
            break;
        case BITDOUBLE_ZERO_VALUE:
        case BITDOUBLE_NOT_USED:
            break;
    }
}

short CADBuffer::ReadRAWSHORT()
{
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 3 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }
    unsigned char aShortBytes[3];
    memcpy( aShortBytes, m_pBuffer + nByteOffset, 3 );

    switch( nBitOffsetInByte )
    {
        case 0:
            break;

        default:
            aShortBytes[0] <<= nBitOffsetInByte;
            aShortBytes[0] |= ( aShortBytes[1] >> ( 8 - nBitOffsetInByte ) );
            aShortBytes[1] <<= nBitOffsetInByte;
            aShortBytes[1] |= ( aShortBytes[2] >> ( 8 - nBitOffsetInByte ) );
            break;
    }

    int16_t result;
    memcpy(&result, aShortBytes, sizeof(result));
    FromLSB(result);

    m_nBitOffsetFromStart += 16;

    return result;
}

double CADBuffer::ReadRAWDOUBLE()
{
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 9 > m_nSize)
    {
        m_bEOB = true;
        return 0.0;
    }

    unsigned char aDoubleBytes[9];
    memcpy( aDoubleBytes, m_pBuffer + nByteOffset, 9 );

    switch( nBitOffsetInByte )
    {
        case 0:
            break;

        default:
            aDoubleBytes[0] <<= nBitOffsetInByte;
            aDoubleBytes[0] |= ( aDoubleBytes[1] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[1] <<= nBitOffsetInByte;
            aDoubleBytes[1] |= ( aDoubleBytes[2] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[2] <<= nBitOffsetInByte;
            aDoubleBytes[2] |= ( aDoubleBytes[3] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[3] <<= nBitOffsetInByte;
            aDoubleBytes[3] |= ( aDoubleBytes[4] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[4] <<= nBitOffsetInByte;
            aDoubleBytes[4] |= ( aDoubleBytes[5] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[5] <<= nBitOffsetInByte;
            aDoubleBytes[5] |= ( aDoubleBytes[6] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[6] <<= nBitOffsetInByte;
            aDoubleBytes[6] |= ( aDoubleBytes[7] >> ( 8 - nBitOffsetInByte ) );
            aDoubleBytes[7] <<= nBitOffsetInByte;
            aDoubleBytes[7] |= ( aDoubleBytes[8] >> ( 8 - nBitOffsetInByte ) );
            break;
    }

    double result;
    memcpy(&result, aDoubleBytes, sizeof(result));
    FromLSB(result);

    m_nBitOffsetFromStart += 64;

    return result;
}

int CADBuffer::ReadRAWLONG()
{
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 5 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }

    unsigned char aLongBytes[5];
    memcpy( aLongBytes, m_pBuffer + nByteOffset, 5 );

    switch( nBitOffsetInByte )
    {
        case 0:
            break;

        default:
            aLongBytes[0] <<= nBitOffsetInByte;
            aLongBytes[0] |= ( aLongBytes[1] >> ( 8 - nBitOffsetInByte ) );
            aLongBytes[1] <<= nBitOffsetInByte;
            aLongBytes[1] |= ( aLongBytes[2] >> ( 8 - nBitOffsetInByte ) );
            aLongBytes[2] <<= nBitOffsetInByte;
            aLongBytes[2] |= ( aLongBytes[3] >> ( 8 - nBitOffsetInByte ) );
            aLongBytes[3] <<= nBitOffsetInByte;
            aLongBytes[3] |= ( aLongBytes[4] >> ( 8 - nBitOffsetInByte ) );
            break;
    }

    int32_t result;
    memcpy(&result, aLongBytes, sizeof(result));
    FromLSB(result);

    m_nBitOffsetFromStart += 32;

    return result;
}

bool CADBuffer::ReadBIT()
{
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset >= m_nSize)
    {
        m_bEOB = true;
        return false;
    }

    unsigned char resultVal = ( m_pBuffer[nByteOffset] >> ( 7 - nBitOffsetInByte ) ) & binary( 00000001 );
    ++m_nBitOffsetFromStart;

    return resultVal == 0 ? false : true;
}

short CADBuffer::ReadBITSHORT()
{
    unsigned char BITCODE = Read2B();

    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 4 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }

    unsigned char aShortBytes[4]; // maximum bytes a single short can take.
    memcpy( aShortBytes, m_pBuffer + nByteOffset, 4 );

    switch( BITCODE )
    {
        case BITSHORT_NORMAL:
        {
            aShortBytes[0] = ( aShortBytes[0] << nBitOffsetInByte );
            aShortBytes[0] |= ( aShortBytes[1] >> ( 8 - nBitOffsetInByte ) );
            aShortBytes[1] = ( aShortBytes[1] << nBitOffsetInByte );
            aShortBytes[1] |= ( aShortBytes[2] >> ( 8 - nBitOffsetInByte ) );

            m_nBitOffsetFromStart += 16;

            int16_t result;
            memcpy(&result, aShortBytes, sizeof(result));
            FromLSB(result);

            return result;
        }

        case BITSHORT_UNSIGNED_CHAR:
        {
            aShortBytes[0] = ( aShortBytes[0] << nBitOffsetInByte );
            aShortBytes[0] |= ( aShortBytes[1] >> ( 8 - nBitOffsetInByte ) );

            m_nBitOffsetFromStart += 8;

            return static_cast<unsigned char>(aShortBytes[0]);
        }

        case BITSHORT_ZERO_VALUE:
        {
            m_nBitOffsetFromStart += 0;
            return 0;
        }

        case BITSHORT_256:
        {
            m_nBitOffsetFromStart += 0;
            return 256;
        }
    }

    return -1;
}

unsigned char CADBuffer::ReadCHAR()
{
    unsigned char result           = 0;
    size_t        nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 2 > m_nSize)
    {
        m_bEOB = true;
        return result;
    }

    unsigned char aCharBytes[2]; // maximum bytes a single char can take.
    memcpy( aCharBytes, m_pBuffer + nByteOffset, 2 );

    result = ( aCharBytes[0] << nBitOffsetInByte );
    result |= ( aCharBytes[1] >> ( 8 - nBitOffsetInByte ) );

    m_nBitOffsetFromStart += 8;

    return result;
}

std::string CADBuffer::ReadTV()
{
    short stringLength = ReadBITSHORT();

    std::string result;

    for( short i = 0; i < stringLength; ++i )
    {
        result += static_cast<char>(ReadCHAR());
    }

    return result;
}

long CADBuffer::ReadUMCHAR()
{
    long result = 0;
    size_t nByteOffset = m_nBitOffsetFromStart / 8;
    // TODO: bit offset is calculated, but function has nothing to do with it.
    /*size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;*/

    if(nByteOffset + 8 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }
    unsigned char aMCharBytes[8]; // 8 bytes is maximum.
    //memcpy( aMCharBytes, m_pBuffer + nByteOffset, 8 );

    unsigned char nMCharBytesCount = 0;
    for( unsigned char i = 0; i < 8; ++i )
    {
        aMCharBytes[i] = ReadCHAR();
        ++nMCharBytesCount;
        if ( !( aMCharBytes[i] & binary( 10000000 ) ) )
        {
            break;
        }
        aMCharBytes[i] &= binary( 01111111 );
    }

    int nOffset = 0;
    for(unsigned char i = 0; i < nMCharBytesCount; ++i)
    {
        unsigned long nVal = aMCharBytes[i];
        result += nVal << nOffset;
        nOffset += 7;
    }
    return result;
}

long CADBuffer::ReadMCHAR()
{
    long   result      = 0;
    bool   negative    = false;
    size_t nByteOffset = m_nBitOffsetFromStart / 8;

    // TODO: bit offset is calculated, but function has nothing to do with it.
    /*size_t nBitOffsetInByte = nBitOffsetFromStart % 8;*/

    if(nByteOffset + 8 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }
    unsigned char aMCharBytes[8]; // 8 bytes is maximum.

    unsigned char nMCharBytesCount = 0;
    for( unsigned char i = 0; i < 8; ++i )
    {
        aMCharBytes[i] = ReadCHAR();
        ++nMCharBytesCount;
        if ( !( aMCharBytes[i] & binary( 10000000 ) ) )
        {
            break;
        }
        aMCharBytes[i] &= binary( 01111111 );
    }

    if ( ( aMCharBytes[nMCharBytesCount - 1] & binary( 01000000 ) ) == binary( 01000000 ) )
    {
        aMCharBytes[nMCharBytesCount - 1] &= binary( 10111111 );
        negative = true;
    }

    int nOffset = 0;
    for(unsigned char i = 0; i < nMCharBytesCount; ++i)
    {
        unsigned long nVal = aMCharBytes[i];
        result += nVal << nOffset;
        nOffset += 7;
    }

    if( negative )
    {
        result *= -1;
    }

    return result;
}

unsigned int CADBuffer::ReadMSHORT()
{
    unsigned char aMShortBytes[8]; // 8 bytes is maximum.

    // TODO: this function does not support MSHORTS longer than 4 bytes. ODA says
    //       it's impossible, but not sure.
    size_t MShortBytesCount = 2;
    aMShortBytes[0] = ReadCHAR();
    aMShortBytes[1] = ReadCHAR();
    if ( aMShortBytes[1] & binary( 10000000 ) )
    {
        aMShortBytes[2] = ReadCHAR();
        aMShortBytes[3] = ReadCHAR();
        MShortBytesCount = 4;
    }

    SwapEndianness( aMShortBytes, MShortBytesCount );

    if( MShortBytesCount == 2 )
    {
        aMShortBytes[0] &= binary( 01111111 ); // drop high order flag bit.
    }
    else
    {
        aMShortBytes[0] &= binary( 01111111 );
        aMShortBytes[2] &= binary( 01111111 );

        aMShortBytes[2] |= ( aMShortBytes[1] << 7 );
        aMShortBytes[1] = ( aMShortBytes[1] >> 1 );
        aMShortBytes[1] |= ( aMShortBytes[0] << 7 );
        aMShortBytes[0] = ( aMShortBytes[0] >> 1 );
    }

    unsigned int result;
    if( MShortBytesCount == 2 )
    {
        result = (aMShortBytes[0] << 8) | aMShortBytes[1];
    }
    else
    {
        result = (static_cast<unsigned>(aMShortBytes[0]) << 24) |
                 (aMShortBytes[1] << 16) | (aMShortBytes[2] << 8) | aMShortBytes[3];
    }

    return result;
}



double CADBuffer::ReadBITDOUBLEWD(double defaultvalue )
{
    unsigned char aDefaultValueBytes[8];
    memcpy( aDefaultValueBytes, & defaultvalue, 8 );

    unsigned char BITCODE = Read2B();

    switch( BITCODE )
    {
        case BITDOUBLEWD_DEFAULT_VALUE:
        {
            return defaultvalue;
        }

        case BITDOUBLEWD_4BYTES_PATCHED:
        {
            aDefaultValueBytes[0] = ReadCHAR();
            aDefaultValueBytes[1] = ReadCHAR();
            aDefaultValueBytes[2] = ReadCHAR();
            aDefaultValueBytes[3] = ReadCHAR();

            double result;
            memcpy(&result, aDefaultValueBytes, sizeof(result));
            FromLSB(result);
            return result;
        }

        case BITDOUBLEWD_6BYTES_PATCHED:
        {
            aDefaultValueBytes[4] = ReadCHAR();
            aDefaultValueBytes[5] = ReadCHAR();
            aDefaultValueBytes[0] = ReadCHAR();
            aDefaultValueBytes[1] = ReadCHAR();
            aDefaultValueBytes[2] = ReadCHAR();
            aDefaultValueBytes[3] = ReadCHAR();

            double result;
            memcpy(&result, aDefaultValueBytes, sizeof(result));
            FromLSB(result);
            return result;
        }

        case BITDOUBLEWD_FULL_RD:
        {
            aDefaultValueBytes[0] = ReadCHAR();
            aDefaultValueBytes[1] = ReadCHAR();
            aDefaultValueBytes[2] = ReadCHAR();
            aDefaultValueBytes[3] = ReadCHAR();
            aDefaultValueBytes[4] = ReadCHAR();
            aDefaultValueBytes[5] = ReadCHAR();
            aDefaultValueBytes[6] = ReadCHAR();
            aDefaultValueBytes[7] = ReadCHAR();

            double result;
            memcpy(&result, aDefaultValueBytes, sizeof(result));
            FromLSB(result);
            return result;
        }
    }

    return 0.0f;
}

CADHandle CADBuffer::ReadHANDLE()
{
    CADHandle result( Read4B() );
    unsigned char counter = Read4B();
    for( unsigned char i = 0; i < counter; ++i )
    {
        result.addOffset( ReadCHAR() );
    }

    return result;
}

void CADBuffer::SkipHANDLE()
{
    Read4B();
    unsigned char counter = Read4B();
    m_nBitOffsetFromStart += counter * 8;
}

CADHandle CADBuffer::ReadHANDLE8BLENGTH()
{
    CADHandle result;

    unsigned char counter = ReadCHAR();

    for( unsigned char i = 0; i < counter; ++i )
    {
        result.addOffset( ReadCHAR() );
    }

    return result;
}

int CADBuffer::ReadBITLONG()
{
    unsigned char BITCODE = Read2B();

    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = m_nBitOffsetFromStart % 8;

    if(nByteOffset + 5 > m_nSize)
    {
        m_bEOB = true;
        return 0;
    }
    unsigned char aLongBytes[5]; // maximum bytes a single short can take.
    memcpy( aLongBytes, m_pBuffer + nByteOffset, 5 );

    switch( BITCODE )
    {
        case BITLONG_NORMAL:
        {
            aLongBytes[0] <<= nBitOffsetInByte;
            aLongBytes[0] |= ( aLongBytes[1] >> ( 8 - nBitOffsetInByte ) );
            aLongBytes[1] <<= nBitOffsetInByte;
            aLongBytes[1] |= ( aLongBytes[2] >> ( 8 - nBitOffsetInByte ) );
            aLongBytes[2] <<= nBitOffsetInByte;
            aLongBytes[2] |= ( aLongBytes[3] >> ( 8 - nBitOffsetInByte ) );
            aLongBytes[3] <<= nBitOffsetInByte;
            aLongBytes[3] |= ( aLongBytes[4] >> ( 8 - nBitOffsetInByte ) );

            m_nBitOffsetFromStart += 32;

            int32_t result;
            memcpy(&result, aLongBytes, sizeof(result));
            FromLSB(result);
            return result;
        }

        case BITLONG_UNSIGNED_CHAR:
        {
            aLongBytes[0] <<= nBitOffsetInByte;
            aLongBytes[0] |= ( aLongBytes[1] >> ( 8 - nBitOffsetInByte ) );

            m_nBitOffsetFromStart += 8;

            return aLongBytes[0];
        }

        case BITLONG_ZERO_VALUE:
        {
            m_nBitOffsetFromStart += 0;
            return 0;
        }

        case BITLONG_NOT_USED:
        {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
            std::cerr <<
            "THAT SHOULD NEVER HAPPENED! BUG. (in file, or reader, or both.) ReadBITLONG(), case BITLONG_NOT_USED\n";
#endif
            m_nBitOffsetFromStart += 0;
            return 0;
        }
    }

    return -1;
}

void CADBuffer::SkipTV()
{
    short stringLength = ReadBITSHORT();
    if( stringLength < 0 )
    {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        std::cerr << "Negative string length" << std::endl;
#endif
        return;
    }
    m_nBitOffsetFromStart += size_t( stringLength * 8 );
}

void CADBuffer::SkipBITLONG()
{
    unsigned char BITCODE = Read2B();
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    if(nByteOffset + 5 > m_nSize)
    {
        m_bEOB = true;
        return;
    }
    switch( BITCODE )
    {
        case BITLONG_NORMAL:
            m_nBitOffsetFromStart += 32;
            break;

        case BITLONG_UNSIGNED_CHAR:
            m_nBitOffsetFromStart += 8;
            break;

        case BITLONG_ZERO_VALUE:
        case BITLONG_NOT_USED:
            break;
    }
}

void CADBuffer::SkipBITSHORT()
{
    unsigned char BITCODE = Read2B();
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    if(nByteOffset + 4 > m_nSize)
    {
        m_bEOB = true;
        return;
    }
    switch( BITCODE )
    {
        case BITSHORT_NORMAL:
            m_nBitOffsetFromStart += 16;
            break;

        case BITSHORT_UNSIGNED_CHAR:
            m_nBitOffsetFromStart += 8;
            break;

        case BITSHORT_ZERO_VALUE:
        case BITSHORT_256:
            break;
    }
}

void CADBuffer::SkipBIT()
{
    size_t nByteOffset      = m_nBitOffsetFromStart / 8;
    if(nByteOffset >= m_nSize)
    {
        m_bEOB = true;
        return;
    }
    ++m_nBitOffsetFromStart;
}

CADVector CADBuffer::ReadVector()
{
    double x, y, z;
    x = ReadBITDOUBLE();
    y = ReadBITDOUBLE();
    z = ReadBITDOUBLE();

    return CADVector( x, y, z );
}

CADVector CADBuffer::ReadRAWVector()
{
    double x, y;
    x = ReadRAWDOUBLE();
    y = ReadRAWDOUBLE();

    return CADVector( x, y );
}

void CADBuffer::Seek(size_t offset, CADBuffer::SeekPosition position)
{
    switch (position) {
    case BEG:
        m_nBitOffsetFromStart = offset;
        break;
    case CURRENT:
        m_nBitOffsetFromStart += offset;
        break;
    case END:
        m_nBitOffsetFromStart = m_nSize - offset;
        break;
    default:
        break;
    }
}

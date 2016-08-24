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
#include <cstring>

unsigned short CalculateCRC8( unsigned short initialVal, const char * ptr, int num )
{
    unsigned char al;
    while( num-- > 0 )
    {
        al         = static_cast<unsigned char>( ( * ptr ) ^ ( ( char ) ( initialVal & 0xFF ) ) );
        initialVal = ( initialVal >> 8 ) & 0xFF;
        initialVal = initialVal ^ DWGCRC8Table[al & 0xFF];
        ptr++;
    }

    return static_cast<unsigned short>( initialVal );
}

unsigned char Read2B( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char result           = 0;
    size_t        nByteOffset      = nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * p2BByte = pabyInput + nByteOffset;
    unsigned char a2BBytes[2];
    memcpy( a2BBytes, p2BByte, 2 );

    switch( nBitOffsetInByte )
    {
        case 7:
            result = ( a2BBytes[0] & binary(00000001) ) << 1;
            result |= ( a2BBytes[1] & binary(10000000) ) >> 7;
            break;
        default:
            result = ( a2BBytes[0] >> ( 6 - nBitOffsetInByte ) );
            break;
    }

    result &= binary(00000011);
    nBitOffsetFromStart += 2;

    return result;
}

unsigned char Read3B( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char result           = 0;
    size_t        nByteOffset      = nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * p3BByte = pabyInput + nByteOffset;
    unsigned char a3BBytes[2];
    memcpy( a3BBytes, p3BByte, 2 );

    switch( nBitOffsetInByte )
    {
        case 6:
            result = ( a3BBytes[0] & binary(00000011) ) << 1;
            result |= ( a3BBytes[1] & binary(10000000) ) >> 7;
            break;

        case 7:
            result = ( a3BBytes[0] & binary(00000001) ) << 2;
            result |= ( a3BBytes[1] & binary(11000000) ) >> 6;
            break;

        default:
            result = ( a3BBytes[0] >> ( 5 - nBitOffsetInByte ) );
            break;
    }

    result &= binary(00000111);
    nBitOffsetFromStart += 3;

    return result;
}

unsigned char Read4B( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char result           = 0;
    size_t        nByteOffset      = nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * p4BByte = pabyInput + nByteOffset;
    unsigned char a4BBytes[2];
    memcpy( a4BBytes, p4BByte, 2 );

    switch( nBitOffsetInByte )
    {
        case 5:
            result = ( a4BBytes[0] & binary(00000111) ) << 1;
            result |= ( a4BBytes[1] & binary(10000000) ) >> 7;
            break;
        case 6:
            result = ( a4BBytes[0] & binary(00000011) ) << 2;
            result |= ( a4BBytes[1] & binary(11000000) ) >> 6;
            break;

        case 7:
            result = ( a4BBytes[0] & binary(00000001) ) << 3;
            result |= ( a4BBytes[1] & binary(11100000) ) >> 5;
            break;

        default:
            result = ( a4BBytes[0] >> ( 4 - nBitOffsetInByte ) );
            break;
    }

    result &= binary(00001111);
    nBitOffsetFromStart += 4;

    return result;
}

short ReadRAWSHORT( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pShortFirstByte = pabyInput + nByteOffset;
    unsigned char aShortBytes[3];
    memcpy( aShortBytes, pShortFirstByte, 3 );

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

    void  * ptr    = aShortBytes;
    short * result = static_cast<short *>(ptr);

    nBitOffsetFromStart += 16;

    return * result;
}

double ReadRAWDOUBLE( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pDoubleFirstByte = pabyInput + nByteOffset;

    unsigned char aDoubleBytes[9];
    memcpy( aDoubleBytes, pDoubleFirstByte, 9 );

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

    void   * ptr    = aDoubleBytes;
    double * result = static_cast<double *>(ptr);

    nBitOffsetFromStart += 64;

    return * result;
}

int ReadRAWLONG( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pLongFirstByte = pabyInput + nByteOffset;

    unsigned char aLongBytes[5];
    memcpy( aLongBytes, pLongFirstByte, 5 );

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

    void * ptr    = aLongBytes;
    int  * result = static_cast<int *>(ptr);

    nBitOffsetFromStart += 32;

    return * result;
}

bool ReadBIT( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pBoolByte = pabyInput + nByteOffset;

    unsigned char resultVal = ( pBoolByte[0] >> ( 7 - nBitOffsetInByte ) ) & binary(00000001);

    ++nBitOffsetFromStart;

    return resultVal == 0 ? false : true;
}

short ReadBITSHORT( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char BITCODE = Read2B( pabyInput, nBitOffsetFromStart );

    size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pShortFirstByte = pabyInput + nByteOffset;
    unsigned char aShortBytes[4]; // maximum bytes a single short can take.
    memcpy( aShortBytes, pShortFirstByte, 4 );

    switch( BITCODE )
    {
        case BITSHORT_NORMAL:
        {
            aShortBytes[0] = ( aShortBytes[0] << nBitOffsetInByte );
            aShortBytes[0] |= ( aShortBytes[1] >> ( 8 - nBitOffsetInByte ) );
            aShortBytes[1] = ( aShortBytes[1] << nBitOffsetInByte );
            aShortBytes[1] |= ( aShortBytes[2] >> ( 8 - nBitOffsetInByte ) );

            nBitOffsetFromStart += 16;

            void  * ptr    = aShortBytes;
            short * result = static_cast < short * > ( ptr );

            return * result;
        }

        case BITSHORT_UNSIGNED_CHAR:
        {
            aShortBytes[0] = ( aShortBytes[0] << nBitOffsetInByte );
            aShortBytes[0] |= ( aShortBytes[1] >> ( 8 - nBitOffsetInByte ) );

            nBitOffsetFromStart += 8;

            return static_cast<unsigned char>(aShortBytes[0]);
        }

        case BITSHORT_ZERO_VALUE:
        {
            nBitOffsetFromStart += 0;
            return 0;
        }

        case BITSHORT_256:
        {
            nBitOffsetFromStart += 0;
            return 256;
        }
    }

    return -1;
}

unsigned char ReadCHAR( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char result           = 0;
    size_t        nByteOffset      = nBitOffsetFromStart / 8;
    size_t        nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pCharFirstByte = pabyInput + nByteOffset;
    unsigned char aCharBytes[2]; // maximum bytes a single char can take.
    memcpy( aCharBytes, pCharFirstByte, 2 );

    result = ( aCharBytes[0] << nBitOffsetInByte );
    result |= ( aCharBytes[1] >> ( 8 - nBitOffsetInByte ) );

    nBitOffsetFromStart += 8;

    return result;
}

std::string ReadTV( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    // TODO: due to CLion issues with copying text from output window, all
    //       string readed are now not zero-terminated. Will fix soon.
    short stringLength = ReadBITSHORT( pabyInput, nBitOffsetFromStart );

    std::string result;

    for( short i = 0; i < stringLength; ++i )
    {
        result += static_cast<char>(ReadCHAR( pabyInput, nBitOffsetFromStart ));
    }

    return result;
}

long ReadUMCHAR( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    // TODO: bit offset is calculated, but function has nothing to do with it.
    long long result      = 0;
    /*bool   negative = false;*/
    size_t    nByteOffset = nBitOffsetFromStart / 8;
    /*size_t nBitOffsetInByte = nBitOffsetFromStart % 8;*/

    const char * pMCharFirstByte = pabyInput + nByteOffset;
    unsigned char aMCharBytes[8]; // 8 bytes is maximum.
    memcpy( aMCharBytes, pMCharFirstByte, 8 );

    size_t      MCharBytesCount = 0;
    for( size_t i               = 0; i < 8; ++i )
    {
        aMCharBytes[i] = ReadCHAR( pabyInput, nBitOffsetFromStart );
        ++MCharBytesCount;
        if( !( aMCharBytes[i] & binary(10000000) ) )
        {
            break;
        }
    }

    SwapEndianness( aMCharBytes, MCharBytesCount ); // LSB to MSB

    for( size_t i = 0; i < MCharBytesCount; ++i )
    {
        aMCharBytes[i] &= binary(01111111);
    }

    // TODO: this code doesnt cover case when char.bytescount > 3, but its
    //       possible on large files.
    // I just cant write an algorithm that does this.
    switch( MCharBytesCount )
    {
        case 1:
            break;
        case 2:
        {
            char tmp = aMCharBytes[0] & binary(00000001);
            aMCharBytes[0] = aMCharBytes[0] >> 1;
            aMCharBytes[1] |= ( tmp << 7 );
            break;
        }
        case 3:
        {
            unsigned char tmp1 = aMCharBytes[0] & binary(00000011);
            unsigned char tmp2 = aMCharBytes[1] & binary(00000001);
            aMCharBytes[0]     = aMCharBytes[0] >> 2;
            aMCharBytes[1]     = aMCharBytes[1] >> 1;
            aMCharBytes[1] |= ( tmp1 << 6 );
            aMCharBytes[2] |= ( tmp2 << 7 );
            break;
        }
        case 4:
        {
            unsigned char tmp1 = aMCharBytes[0] & binary(00000111);
            unsigned char tmp2 = aMCharBytes[1] & binary(00000011);
            unsigned char tmp3 = aMCharBytes[2] & binary(00000001);
            aMCharBytes[0]     = aMCharBytes[0] >> 3;
            aMCharBytes[1]     = aMCharBytes[1] >> 2;
            aMCharBytes[2]     = aMCharBytes[2] >> 1;
            aMCharBytes[1] |= ( tmp1 << 5 );
            aMCharBytes[2] |= ( tmp2 << 6 );
            aMCharBytes[3] |= ( tmp3 << 7 );
            break;
        }
    }

    SwapEndianness( aMCharBytes, MCharBytesCount ); // MSB to LSB

    memcpy( & result, aMCharBytes, MCharBytesCount );

    return result;
}

long ReadMCHAR( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    long   result      = 0;
    bool   negative    = false;
    size_t nByteOffset = nBitOffsetFromStart / 8;

    // TODO: bit offset is calculated, but function has nothing to do with it.
    /*size_t nBitOffsetInByte = nBitOffsetFromStart % 8;*/

    const char * pMCharFirstByte = pabyInput + nByteOffset;
    unsigned char aMCharBytes[8]; // 8 bytes is maximum.
    memcpy( aMCharBytes, pMCharFirstByte, 8 );

    size_t      MCharBytesCount = 0;
    for( size_t i               = 0; i < 8; ++i )
    {
        aMCharBytes[i] = ReadCHAR( pabyInput, nBitOffsetFromStart );
        ++MCharBytesCount;
        if( !( aMCharBytes[i] & binary(10000000) ) )
        {
            break;
        }
    }

    SwapEndianness( aMCharBytes, MCharBytesCount ); // LSB to MSB

    if( ( aMCharBytes[0] & binary(01000000) ) == binary(01000000) )
    {
        aMCharBytes[0] &= binary(10111111);
        negative = true;
    }

    for( size_t i = 0; i < MCharBytesCount; ++i )
    {
        aMCharBytes[i] &= binary(01111111);
    }

    // TODO: this code doesnt cover case when char.bytescount > 3, but its
    //       possible on large files.
    // I just cant write an algorithm that does this.
    switch( MCharBytesCount )
    {
        case 1:
            break;
        case 2:
        {
            char tmp = aMCharBytes[0] & binary(00000001);
            aMCharBytes[0] = aMCharBytes[0] >> 1;
            aMCharBytes[1] |= ( tmp << 7 );
            break;
        }
        case 3:
        {
            unsigned char tmp1 = aMCharBytes[0] & binary(00000011);
            unsigned char tmp2 = aMCharBytes[1] & binary(00000001);
            aMCharBytes[0]     = aMCharBytes[0] >> 2;
            aMCharBytes[1]     = aMCharBytes[1] >> 1;
            aMCharBytes[1] |= ( tmp1 << 6 );
            aMCharBytes[2] |= ( tmp2 << 7 );
            break;
        }
        case 4:
        {
            unsigned char tmp1 = aMCharBytes[0] & binary(00000111);
            unsigned char tmp2 = aMCharBytes[1] & binary(00000011);
            unsigned char tmp3 = aMCharBytes[2] & binary(00000001);
            aMCharBytes[0]     = aMCharBytes[0] >> 3;
            aMCharBytes[1]     = aMCharBytes[1] >> 2;
            aMCharBytes[2]     = aMCharBytes[2] >> 1;
            aMCharBytes[1] |= ( tmp1 << 5 );
            aMCharBytes[2] |= ( tmp2 << 6 );
            aMCharBytes[3] |= ( tmp3 << 7 );
            break;
        }
    }

    SwapEndianness( aMCharBytes, MCharBytesCount ); // MSB to LSB

    memcpy( & result, aMCharBytes, MCharBytesCount );

    if( negative ) result *= -1;

    return result;
}

unsigned int ReadMSHORT( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned int  result = 0;
    /*size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pMShortFirstByte = pabyInput + nByteOffset;*/
    unsigned char aMShortBytes[8]; // 8 bytes is maximum.

    // TODO: this function doesnot support MSHORTS longer than 4 bytes. ODA says
    //       its impossible, but not sure.
    size_t MShortBytesCount = 2;
    aMShortBytes[0] = ReadCHAR( pabyInput, nBitOffsetFromStart );
    aMShortBytes[1] = ReadCHAR( pabyInput, nBitOffsetFromStart );
    if( aMShortBytes[1] & binary(10000000) )
    {
        aMShortBytes[2] = ReadCHAR( pabyInput, nBitOffsetFromStart );
        aMShortBytes[3] = ReadCHAR( pabyInput, nBitOffsetFromStart );
        MShortBytesCount = 4;
    }

    SwapEndianness( aMShortBytes, MShortBytesCount );

    if( MShortBytesCount == 2 )
    {
        aMShortBytes[0] &= binary(01111111); // drop high order flag bit.
    } else if( MShortBytesCount == 4 )
    {
        aMShortBytes[0] &= binary(01111111);
        aMShortBytes[2] &= binary(01111111);

        aMShortBytes[2] |= ( aMShortBytes[1] << 7 );
        aMShortBytes[1] = ( aMShortBytes[1] >> 1 );
        aMShortBytes[1] |= ( aMShortBytes[0] << 7 );
        aMShortBytes[0] = ( aMShortBytes[0] >> 1 );
    }
    SwapEndianness( aMShortBytes, MShortBytesCount ); // MSB to LSB
    memcpy( & result, aMShortBytes, MShortBytesCount );
    return result;
}

double ReadBITDOUBLE( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char BITCODE = Read2B( pabyInput, nBitOffsetFromStart );

    size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pDoubleFirstByte = pabyInput + nByteOffset;
    unsigned char aDoubleBytes[9]; // maximum bytes a single double can take.
    memcpy( aDoubleBytes, pDoubleFirstByte, 9 );

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

            nBitOffsetFromStart += 64;

            void   * ptr    = aDoubleBytes;
            double * result = static_cast< double *> ( ptr );

            return * result;
        }

        case BITDOUBLE_ONE_VALUE:
        {
            nBitOffsetFromStart += 0;

            return 1.0f;
        }

        case BITDOUBLE_ZERO_VALUE:
        {
            nBitOffsetFromStart += 0;

            return 0.0f;
        }

        case BITDOUBLE_NOT_USED:
        {
            nBitOffsetFromStart += 0;

            return 0.0f;
        }
    }

    return 0.0f;
}

double ReadBITDOUBLEWD( const char * pabyInput, size_t& nBitOffsetFromStart, double defaultvalue )
{
    unsigned char aDefaultValueBytes[8];
    memcpy( aDefaultValueBytes, & defaultvalue, 8 );

    unsigned char BITCODE = Read2B( pabyInput, nBitOffsetFromStart );

    switch( BITCODE )
    {
        case BITDOUBLEWD_DEFAULT_VALUE:
        {
            return defaultvalue;
        }

        case BITDOUBLEWD_4BYTES_PATCHED:
        {
            aDefaultValueBytes[0] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[1] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[2] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[3] = ReadCHAR( pabyInput, nBitOffsetFromStart );

            void   * ptr    = aDefaultValueBytes;
            double * result = static_cast< double *> ( ptr );

            return * result;
        }

        case BITDOUBLEWD_6BYTES_PATCHED:
        {
            aDefaultValueBytes[4] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[5] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[0] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[1] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[2] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[3] = ReadCHAR( pabyInput, nBitOffsetFromStart );

            void   * ptr    = aDefaultValueBytes;
            double * result = static_cast< double *> ( ptr );

            return * result;
        }

        case BITDOUBLEWD_FULL_RD:
        {
            aDefaultValueBytes[0] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[1] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[2] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[3] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[4] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[5] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[6] = ReadCHAR( pabyInput, nBitOffsetFromStart );
            aDefaultValueBytes[7] = ReadCHAR( pabyInput, nBitOffsetFromStart );

            void   * ptr    = aDefaultValueBytes;
            double * result = static_cast< double *> ( ptr );

            return * result;
        }
    }

    return 0.0f;
}

CADHandle ReadHANDLE( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    CADHandle          result( Read4B( pabyInput, nBitOffsetFromStart ) );
    unsigned char      counter = Read4B( pabyInput, nBitOffsetFromStart );
    for( unsigned char i       = 0; i < counter; ++i )
    {
        result.addOffset( ReadCHAR( pabyInput, nBitOffsetFromStart ) );
    }

    return result;
}

void SkipHANDLE( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    Read4B( pabyInput, nBitOffsetFromStart );
    unsigned char counter = static_cast<unsigned char>(Read4B( pabyInput, nBitOffsetFromStart ));
    nBitOffsetFromStart += counter * 8;
}

CADHandle ReadHANDLE8BLENGTH( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    CADHandle result;

    unsigned char counter = ReadCHAR( pabyInput, nBitOffsetFromStart );

    for( unsigned char i = 0; i < counter; ++i )
    {
        result.addOffset( ReadCHAR( pabyInput, nBitOffsetFromStart ) );
    }

    return result;
}

int ReadBITLONG( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char BITCODE = Read2B( pabyInput, nBitOffsetFromStart );

    size_t nByteOffset      = nBitOffsetFromStart / 8;
    size_t nBitOffsetInByte = nBitOffsetFromStart % 8;

    const char * pLongFirstByte = pabyInput + nByteOffset;
    unsigned char aLongBytes[5]; // maximum bytes a single short can take.
    memcpy( aLongBytes, pLongFirstByte, 5 );

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

            nBitOffsetFromStart += 32;

            void * ptr    = aLongBytes;
            int  * result = static_cast < int * > ( ptr );

            return * result;
        }

        case BITLONG_UNSIGNED_CHAR:
        {
            aLongBytes[0] <<= nBitOffsetInByte;
            aLongBytes[0] |= ( aLongBytes[1] >> ( 8 - nBitOffsetInByte ) );

            nBitOffsetFromStart += 8;

            return aLongBytes[0];
        }

        case BITLONG_ZERO_VALUE:
        {
            nBitOffsetFromStart += 0;
            return 0;
        }

        case BITLONG_NOT_USED:
        {
            std::cerr <<
            "THAT SHOULD NEVER HAPPENED! BUG. (in file, or reader, or both.) ReadBITLONG(), case BITLONG_NOT_USED" <<
            std::endl;
            nBitOffsetFromStart += 0;
            return 0;
        }
    }

    return -1;
}

void SkipBITDOUBLE( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char BITCODE = Read2B( pabyInput, nBitOffsetFromStart );

    switch( BITCODE )
    {
        case BITDOUBLE_NORMAL:
            nBitOffsetFromStart += 64;
            break;
        case BITDOUBLE_ONE_VALUE:
            nBitOffsetFromStart += 0;
            break;
        case BITDOUBLE_ZERO_VALUE:
        case BITDOUBLE_NOT_USED:
            break;
    }
}

void SkipTV( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    short stringLength = ReadBITSHORT( pabyInput, nBitOffsetFromStart );
    nBitOffsetFromStart += size_t( stringLength * 8 );
}

void SkipBITLONG( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char BITCODE = Read2B( pabyInput, nBitOffsetFromStart );
    switch( BITCODE )
    {
        case BITLONG_NORMAL:
            nBitOffsetFromStart += 32;
            break;

        case BITLONG_UNSIGNED_CHAR:
            nBitOffsetFromStart += 8;
            break;

        case BITLONG_ZERO_VALUE:
        case BITLONG_NOT_USED:
            break;
    }
}

void SkipBITSHORT( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    unsigned char BITCODE = Read2B( pabyInput, nBitOffsetFromStart );
    switch( BITCODE )
    {
        case BITSHORT_NORMAL:
            nBitOffsetFromStart += 16;
            break;

        case BITSHORT_UNSIGNED_CHAR:
            nBitOffsetFromStart += 8;
            break;

        case BITSHORT_ZERO_VALUE:
        case BITSHORT_256:
            break;
    }
}

void SkipBIT( const char * /*pabyInput*/, size_t& nBitOffsetFromStart )
{
    ++nBitOffsetFromStart;
}

CADVector ReadVector( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    double x, y, z;
    x = ReadBITDOUBLE( pabyInput, nBitOffsetFromStart );
    y = ReadBITDOUBLE( pabyInput, nBitOffsetFromStart );
    z = ReadBITDOUBLE( pabyInput, nBitOffsetFromStart );

    return CADVector( x, y, z );
}

CADVector ReadRAWVector( const char * pabyInput, size_t& nBitOffsetFromStart )
{
    double x, y;
    x = ReadRAWDOUBLE( pabyInput, nBitOffsetFromStart );
    y = ReadRAWDOUBLE( pabyInput, nBitOffsetFromStart );

    return CADVector( x, y );
}

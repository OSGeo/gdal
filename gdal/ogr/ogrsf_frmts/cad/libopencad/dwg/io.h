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
#ifndef DWG_IO_H
#define DWG_IO_H

#include "cadheader.h"
#include "cadobjects.h"

#include <string>
#include <algorithm>

/* DATA TYPES CONSTANTS */

#define BITSHORT_NORMAL         0x0
#define BITSHORT_UNSIGNED_CHAR  0x1
#define BITSHORT_ZERO_VALUE     0x2
#define BITSHORT_256            0x3

#define BITLONG_NORMAL          0x0
#define BITLONG_UNSIGNED_CHAR   0x1
#define BITLONG_ZERO_VALUE      0x2
#define BITLONG_NOT_USED        0x3

#define BITDOUBLE_NORMAL        0x0
#define BITDOUBLE_ONE_VALUE     0x1
#define BITDOUBLE_ZERO_VALUE    0x2
#define BITDOUBLE_NOT_USED      0x3

#define BITDOUBLEWD_DEFAULT_VALUE  0x0
#define BITDOUBLEWD_4BYTES_PATCHED 0x1
#define BITDOUBLEWD_6BYTES_PATCHED 0x2
#define BITDOUBLEWD_FULL_RD        0x3


static const size_t DWGSentinelLength = 16;

static const char * DWGHeaderVariablesStart = "\xCF\x7B\x1F\x23\xFD\xDE\x38\xA9\x5F\x7C\x68\xB8\x4E\x6D\x33\x5F";
static const char * DWGHeaderVariablesEnd   = "\x30\x84\xE0\xDC\x02\x21\xC7\x56\xA0\x83\x97\x47\xB1\x92\xCC\xA0";

static const char * DWGDSPreviewStart = "\x1F\x25\x6D\x07\xD4\x36\x28\x28\x9D\x57\xCA\x3F\x9D\x44\x10\x2B";
static const char * DWGDSPreviewEnd   = "\xE0\xDA\x92\xF8\x2B\xc9\xD7\xD7\x62\xA8\x35\xC0\x62\xBB\xEF\xD4";

static const char * DWGDSClassesStart = "\x8D\xA1\xC4\xB8\xC4\xA9\xF8\xC5\xC0\xDC\xF4\x5F\xE7\xCF\xB6\x8A";
static const char * DWGDSClassesEnd   = "\x72\x5E\x3B\x47\x3B\x56\x07\x3A\x3F\x23\x0B\xA0\x18\x30\x49\x75";

static const char * DWGSecondFileHeaderStart = "\xD4\x7B\x21\xCE\x28\x93\x9F\xBF\x53\x24\x40\x09\x12\x3C\xAA\x01";
static const char * DWGSecondFileHeaderEnd   = "\x2B\x84\xDE\x31\xD7\x6C\x60\x40\xAC\xDB\xBF\xF6\xED\xC3\x55\xFE";

// TODO: probably it would be better to have no dependencies on <algorithm>.
template<typename T, typename S>
inline void SwapEndianness( T&& object, S&& size )
{
    std::reverse( ( char * ) &object, ( char * ) &object + size );
}

/*
 * Method taken from here: http://stackoverflow.com/a/2611850
 * Purpose: no C++14 dependencies in library
 */
template< unsigned long N >
struct bin
{
    enum
    {
        value = ( N % 8 ) + ( bin< N / 8 >::value << 1 )
    };
};

template<>
struct bin< 0 >
{
    enum
    {
        value = 0
    };
};
#define binary( n ) bin<0##n>::value

static const int DWGCRC8Table[256] = {
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

unsigned short CalculateCRC8( unsigned short initialVal, const char * ptr, int num );

long          ReadRAWLONGLONG( const char * pabyInput, size_t& nBitOffsetFromStart );
int           ReadRAWLONG( const char * pabyInput, size_t& nBitOffsetFromStart );
short         ReadRAWSHORT( const char * pabyInput, size_t& nBitOffsetFromStart );
double        ReadRAWDOUBLE( const char * pabyInput, size_t& nBitOffsetFromStart );
unsigned char Read2B( const char * pabyInput, size_t& nBitOffsetFromStart );
unsigned char Read3B( const char * pabyInput, size_t& nBitOffsetFromStart );
unsigned char Read4B( const char * pabyInput, size_t& nBitOffsetFromStart );
CADHandle     ReadHANDLE( const char * pabyInput, size_t& nBitOffsetFromStart );
CADHandle     ReadHANDLE8BLENGTH( const char * pabyInput, size_t& nBitOffsetFromStart );
void          SkipHANDLE( const char * pabyInput, size_t& nBitOffsetFromStart );
bool          ReadBIT( const char * pabyInput, size_t& nBitOffsetFromStart );
void          SkipBIT( const char * pabyInput, size_t& nBitOffsetFromStart );
unsigned char ReadCHAR( const char * pabyInput, size_t& nBitOffsetFromStart );
short         ReadBITSHORT( const char * pabyInput, size_t& nBitOffsetFromStart );
int           ReadBITLONG( const char * pabyInput, size_t& nBitOffsetFromStart );
double        ReadBITDOUBLE( const char * pabyInput, size_t& nBitOffsetFromStart );
void          SkipBITDOUBLE( const char * pabyInput, size_t& nBitOffsetFromStart );
double        ReadBITDOUBLEWD( const char * pabyInput, size_t& nBitOffsetFromStart, double defaultvalue );
long          ReadMCHAR( const char * pabyInput, size_t& nBitOffsetFromStart );
long          ReadUMCHAR( const char * pabyInput, size_t& nBitOffsetFromStart );
unsigned int  ReadMSHORT( const char * pabyInput, size_t& nBitOffsetFromStart );
std::string   ReadTV( const char * pabyInput, size_t& nBitOffsetFromStart );
void          SkipTV( const char * pabyInput, size_t& nBitOffsetFromStart );
void          SkipBITLONG( const char * pabyInput, size_t& nBitOffsetFromStart );
void          SkipBITSHORT( const char * pabyInput, size_t& nBitOffsetFromStart );

CADVector ReadVector( const char * pabyInput, size_t& nBitOffsetFromStart );
CADVector ReadRAWVector( const char * pabyInput, size_t& nBitOffsetFromStart );

#endif // DWG_IO_H

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

namespace DWGConstants {
extern const size_t SentinelLength;
extern const char * HeaderVariablesStart;
extern const char * HeaderVariablesEnd;
extern const char * DSClassesStart;
extern const char * DSClassesEnd;

/* UNUSED
static const char * DWGDSPreviewStart
            = "\x1F\x25\x6D\x07\xD4\x36\x28\x28\x9D\x57\xCA\x3F\x9D\x44\x10\x2B";
static const char * DWGDSPreviewEnd
            = "\xE0\xDA\x92\xF8\x2B\xc9\xD7\xD7\x62\xA8\x35\xC0\x62\xBB\xEF\xD4";

static const char * DWGSecondFileHeaderStart
            = "\xD4\x7B\x21\xCE\x28\x93\x9F\xBF\x53\x24\x40\x09\x12\x3C\xAA\x01";
static const char * DWGSecondFileHeaderEnd
            = "\x2B\x84\xDE\x31\xD7\x6C\x60\x40\xAC\xDB\xBF\xF6\xED\xC3\x55\xFE";
*/
}

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

extern const int DWGCRC8Table[256];

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

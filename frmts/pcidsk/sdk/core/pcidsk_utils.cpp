/******************************************************************************
 *
 * Purpose:  Various private (undocumented) utility functions.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_buffer.h"
#include "pcidsk_exception.h"
#include "pcidsk_georef.h"
#include "pcidsk_io.h"
#include "core/pcidsk_utils.h"
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdarg>
#include <iostream>

#if !defined(va_copy) && defined(__va_copy)
#define va_copy __va_copy
#endif

using namespace PCIDSK;

/************************************************************************/
/*                         GetCurrentDateTime()                         */
/************************************************************************/

// format we want: "HH:MM DDMMMYYYY \0"

#include <time.h>
#include <sys/types.h>

void    PCIDSK::GetCurrentDateTime( char *out_time )

{
    time_t          clock;
    char            ctime_out[25];

    time( &clock );
    strncpy( ctime_out, ctime(&clock), 24 ); // TODO: reentrance issue?

    // ctime() products: "Wed Jun 30 21:49:08 1993\n"

    ctime_out[24] = '\0';

    out_time[0] = ctime_out[11];
    out_time[1] = ctime_out[12];
    out_time[2] = ':';
    out_time[3] = ctime_out[14];
    out_time[4] = ctime_out[15];
    out_time[5] = ' ';
    out_time[6] = ctime_out[8];
    out_time[7] = ctime_out[9];
    out_time[8] = ctime_out[4];
    out_time[9] = ctime_out[5];
    out_time[10] = ctime_out[6];
    out_time[11] = ctime_out[20];
    out_time[12] = ctime_out[21];
    out_time[13] = ctime_out[22];
    out_time[14] = ctime_out[23];
    out_time[15] = ' ';
    out_time[16] = '\0';
}

/************************************************************************/
/*                              UCaseStr()                              */
/*                                                                      */
/*      Force a string into upper case "in place".                      */
/************************************************************************/

std::string &PCIDSK::UCaseStr( std::string &target )

{
    for( unsigned int i = 0; i < target.size(); i++ )
    {
        if( islower(target[i]) )
            target[i] = (char) toupper(target[i]);
    }

    return target;
}

/************************************************************************/
/*                             atouint64()                              */
/************************************************************************/

uint64 PCIDSK::atouint64( const char *str_value )

{
#if defined(__MSVCRT__) || defined(_MSC_VER)
    return (uint64) _atoi64( str_value );
#else
    return (uint64) atoll( str_value );
#endif
}

/************************************************************************/
/*                              atoint64()                              */
/************************************************************************/

int64 PCIDSK::atoint64( const char *str_value )

{
#if defined(__MSVCRT__) || defined(_MSC_VER)
    return (int64) _atoi64( str_value );
#else
    return (int64) atoll( str_value );
#endif
}

/************************************************************************/
/*                            SwapPixels()                              */
/************************************************************************/
/**
 * @brief Perform an endianness swap for a given buffer of pixels
 *
 * Baed on the provided data type, do an appropriate endianness swap for
 * a buffer of pixels. Deals with the Complex case specially, in
 * particular.
 *
 * @param data the pixels to be swapped
 * @param type the data type of the pixels
 * @param count the count of pixels (not bytes, words, etc.)
 */
void PCIDSK::SwapPixels(void* const data,
                        const eChanType type,
                        const std::size_t count)
{
    switch(type) {
    case CHN_8U:
    case CHN_16U:
    case CHN_16S:
    case CHN_32U:
    case CHN_32S:
    case CHN_32R:
    case CHN_64U:
    case CHN_64S:
    case CHN_64R:
        SwapData(data, DataTypeSize(type), static_cast<int>(count));
        break;
    case CHN_C16U:
    case CHN_C16S:
    case CHN_C32U:
    case CHN_C32S:
    case CHN_C32R:
        SwapData(data, DataTypeSize(type) / 2, static_cast<int>(count) * 2);
        break;
    default:
        return ThrowPCIDSKException("Unknown data type passed to SwapPixels."
            "This is a software bug. Please contact your vendor.");
    }
}

/************************************************************************/
/*                              SwapData()                              */
/************************************************************************/
#if defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ > 3
void PCIDSK::SwapData( void* const data, const int size, const int wcount )
{
    if(size == 2)
    {
        uint16 * data16 = reinterpret_cast<uint16*>(data);
        for(int i = 0; i < wcount; i++)
            data16[i] = __builtin_bswap16(data16[i]);
    }
    else if(size == 4)
    {
        uint32 * data32 = reinterpret_cast<uint32*>(data);
        for(int i = 0; i < wcount; i++)
            data32[i] = __builtin_bswap32(data32[i]);
    }
    else if(size == 8)
    {
        uint64 * data64 = reinterpret_cast<uint64*>(data);
        for(int i = 0; i < wcount; i++)
            data64[i] = __builtin_bswap64(data64[i]);
    }
}
#elif defined(_MSC_VER)
#pragma intrinsic(_byteswap_ushort, _byteswap_ulong, _byteswap_uint64)

void PCIDSK::SwapData( void* const data, const int size, const int wcount )
{
    if(size == 2)
    {
        uint16 * data16 = reinterpret_cast<uint16*>(data);
        for(int i = 0; i < wcount; i++)
            data16[i] = _byteswap_ushort(data16[i]);
    }
    else if(size == 4)
    {
        uint32 * data32 = reinterpret_cast<uint32*>(data);
        for(int i = 0; i < wcount; i++)
            data32[i] = _byteswap_ulong(data32[i]);
    }
    else if(size == 8)
    {
        uint64 * data64 = reinterpret_cast<uint64*>(data);
        for(int i = 0; i < wcount; i++)
            data64[i] = _byteswap_uint64(data64[i]);
    }
}

#else

void PCIDSK::SwapData( void* const data, const int size, const int wcount )

{
    uint8* data8 = reinterpret_cast<uint8*>(data);
    std::size_t count = wcount;

    if( size == 2 )
    {
        uint8 t;

        for( ; count; count-- )
        {
            t = data8[0];
            data8[0] = data8[1];
            data8[1] = t;

            data8 += 2;
        }
    }
    else if( size == 1 )
        /* do nothing */;
    else if( size == 4 )
    {
        uint8 t;

        for( ; count; count-- )
        {
            t = data8[0];
            data8[0] = data8[3];
            data8[3] = t;

            t = data8[1];
            data8[1] = data8[2];
            data8[2] = t;

            data8 += 4;
        }
    }
    else if( size == 8 )
    {
        uint8 t;

        for( ; count; count-- )
        {
            t = data8[0];
            data8[0] = data8[7];
            data8[7] = t;

            t = data8[1];
            data8[1] = data8[6];
            data8[6] = t;

            t = data8[2];
            data8[2] = data8[5];
            data8[5] = t;

            t = data8[3];
            data8[3] = data8[4];
            data8[4] = t;

            data8 += 8;
        }
    }
    else
        return ThrowPCIDSKException( "Unsupported data size in SwapData()" );
}

#endif

/************************************************************************/
/*                          BigEndianSystem()                           */
/************************************************************************/

bool PCIDSK::BigEndianSystem()

{
    unsigned short test_value = 1;
    char test_char_value[2];

    memcpy( test_char_value, &test_value, 2 );

    return test_char_value[0] == 0;
}


/************************************************************************/
/*                          ParseTileFormat()                           */
/*                                                                      */
/*      Parse blocksize and compression out of a TILED interleaving     */
/*      string as passed to the Create() function or stored in          */
/*      _DBLayout metadata.                                             */
/************************************************************************/

void PCIDSK::ParseTileFormat(std::string oOptions,
                             int & nTileSize, std::string & oCompress)
{
    nTileSize = PCIDSK_DEFAULT_TILE_SIZE;
    oCompress = "NONE";

    UCaseStr(oOptions);

    std::string::size_type nStart = oOptions.find_first_not_of(" ");
    std::string::size_type nEnd = oOptions.find_first_of(" ", nStart);

    while (nStart != std::string::npos || nEnd != std::string::npos)
    {
        std::string oToken = oOptions.substr(nStart, nEnd - nStart);

        if (oToken.size() > 5 && STARTS_WITH(oToken.c_str(), "TILED"))
        {
            // the TILED entry can be TILED# or TILED=#
            int nPos = oToken[5] == '=' ? 6 : 5;

            nTileSize = atoi(oToken.substr(nPos).c_str());

            if (nTileSize <= 0)
                ThrowPCIDSKException("Invalid tile option: %s", oToken.c_str());
        }
        else if (oToken == "NONE" || oToken == "RLE" ||
                 STARTS_WITH(oToken.c_str(), "JPEG") ||
                 STARTS_WITH(oToken.c_str(), "QUADTREE"))
        {
            oCompress = oToken;
        }

        nStart = oOptions.find_first_not_of(" ", nEnd);
        nEnd = oOptions.find_first_of(" ", nStart);
    }
}

/************************************************************************/
/*                         ParseLinkedFilename()                        */
/************************************************************************/

std::string PCIDSK::ParseLinkedFilename(std::string oOptions)
{
    std::string oToFind = "FILENOCREATE=";
    std::string oLinkedFileName;

    std::string::size_type nStart = oOptions.find_first_not_of(" ");
    std::string::size_type nEnd = oOptions.find_first_of(" ", nStart);

    while (nStart != std::string::npos || nEnd != std::string::npos)
    {
        std::string oToken = oOptions.substr(nStart, nEnd - nStart);

        if (oToken.size() > oToFind.size() &&
            strncmp(oToken.c_str(), oToFind.c_str(), oToFind.size()) == 0)
        {
            oLinkedFileName = oOptions.substr(nStart+oToFind.size());
            break;
        }

        nStart = oOptions.find_first_not_of(" ", nEnd);
        nEnd = oOptions.find_first_of(" ", nStart);
    }

    return oLinkedFileName;
}

/************************************************************************/
/*                           pci_strcasecmp()                           */
/************************************************************************/

int PCIDSK::pci_strcasecmp( const char *string1, const char *string2 )

{
    int i;

    for( i = 0; string1[i] != '\0' && string2[i] != '\0'; i++ )
    {
        char c1 = string1[i];
        char c2 = string2[i];

        if( islower(c1) )
            c1 = (char) toupper(c1);
        if( islower(c2) )
            c2 = (char) toupper(c2);

        if( c1 < c2 )
            return -1;
        else if( c1 > c2 )
            return 1;
    }

    if( string1[i] == '\0' && string2[i] == '\0' )
        return 0;
    else if( string1[i] == '\0' )
        return 1;
    else
        return -1;
}

/************************************************************************/
/*                          pci_strncasecmp()                           */
/************************************************************************/

int PCIDSK::pci_strncasecmp( const char *string1, const char *string2, size_t len )

{
    for( size_t i = 0; i < len; i++ )
    {
        if( string1[i] == '\0' && string2[i] == '\0' )
            return 0;
        else if( string1[i] == '\0' )
            return 1;
        else if( string2[i] == '\0' )
            return -1;

        char c1 = string1[i];
        char c2 = string2[i];

        if( islower(c1) )
            c1 = (char) toupper(c1);
        if( islower(c2) )
            c2 = (char) toupper(c2);

        if( c1 < c2 )
            return -1;
        else if( c1 > c2 )
            return 1;
    }

    return 0;
}

/************************************************************************/
/*                         ProjParamsFromText()                          */
/*                                                                      */
/*      function to turn a ProjParams string (17 floating point          */
/*      numbers) into an array, as well as attaching the units code     */
/*      derived from the geosys string.                                 */
/************************************************************************/

std::vector<double> PCIDSK::ProjParamsFromText( std::string geosys,
                                               std::string sparms )

{
    std::vector<double> dparms;

    for( const char* next = sparms.c_str(); *next != '\0'; )
    {
        dparms.push_back( CPLAtof(next) );

        // move past this token
        while( *next != '\0' && *next != ' ' )
            next++;

        // move past white space.
        while( *next == ' ' )
            next++;
    }

    dparms.resize(18);

    // This is rather iffy!
    if( STARTS_WITH_CI(geosys.c_str(), "DEG" /* "DEGREE" */) )
        dparms[17] = (double) (int) UNIT_DEGREE;
    else if( STARTS_WITH_CI(geosys.c_str(), "MET") )
        dparms[17] = (double) (int) UNIT_METER;
    else if( STARTS_WITH_CI(geosys.c_str(), "FOOT") )
        dparms[17] = (double) (int) UNIT_US_FOOT;
    else if( STARTS_WITH_CI(geosys.c_str(), "FEET") )
        dparms[17] = (double) (int) UNIT_US_FOOT;
    else if( STARTS_WITH_CI(geosys.c_str(), "INTL " /* "INTL FOOT" */) )
        dparms[17] = (double) (int) UNIT_INTL_FOOT;
    else if( STARTS_WITH_CI(geosys.c_str(), "SPCS") )
        dparms[17] = (double) (int) UNIT_METER;
    else if( STARTS_WITH_CI(geosys.c_str(), "SPIF") )
        dparms[17] = (double) (int) UNIT_INTL_FOOT;
    else if( STARTS_WITH_CI(geosys.c_str(), "SPAF") )
        dparms[17] = (double) (int) UNIT_US_FOOT;
    else
        dparms[17] = -1.0; /* unknown */

    return dparms;
}

/************************************************************************/
/*                          ProjParamsToText()                           */
/************************************************************************/

std::string PCIDSK::ProjParamsToText( std::vector<double> dparms )

{
    std::string sparms;

    for( unsigned int i = 0; i < 17; i++ )
    {
        char value[64];
        double dvalue;

        if( i < dparms.size() )
            dvalue = dparms[i];
        else
            dvalue = 0.0;

        if( dvalue == floor(dvalue) )
            CPLsnprintf( value, sizeof(value), "%d", (int) dvalue );
        else
            CPLsnprintf( value, sizeof(value), "%.15g", dvalue );

        if( i > 0 )
            sparms += " ";

        sparms += value;
    }

    return sparms;
}

/************************************************************************/
/*                            ExtractPath()                             */
/*                                                                      */
/*      Extract the directory path portion of the passed filename.      */
/*      It assumes the last component is a filename and should not      */
/*      be passed a bare path.  The trailing directory delimiter is     */
/*      removed from the result.  The return result is an empty         */
/*      string for a simple filename passed in with no directory        */
/*      component.                                                      */
/************************************************************************/

std::string PCIDSK::ExtractPath( std::string filename )

{
    int i;

    for( i = static_cast<int>(filename.size())-1; i >= 0; i-- )
    {
        if( filename[i] == '\\' || filename[i] == '/' )
            break;
    }

    if( i > 0 )
        return filename.substr(0,i);
    else
        return "";
}

/************************************************************************/
/*                         DefaultMergeRelativePath()                   */
/*                                                                      */
/*      This attempts to take src_filename and make it relative to      */
/*      the base of the file "base", if this evaluates to a new file    */
/*      in the filesystem.  It will not make any change if              */
/*      src_filename appears to be absolute or if the altered path      */
/*      does not resolve to a file in the filesystem.                   */
/************************************************************************/

std::string PCIDSK::DefaultMergeRelativePath(const PCIDSK::IOInterfaces *io_interfaces,
                                             const std::string& base,
                                             const std::string& src_filename)

{
/* -------------------------------------------------------------------- */
/*      Does src_filename appear to be absolute?                        */
/* -------------------------------------------------------------------- */
    if( src_filename.empty() )
        return src_filename; // we can't do anything with a blank.
    else if( src_filename.size() > 2 && src_filename[1] == ':' )
        return src_filename; // has a drive letter?
    else if( src_filename[0] == '/' || src_filename[0] == '\\' )
        return src_filename; // has a leading dir marker.

/* -------------------------------------------------------------------- */
/*      Figure out what path split char we want to use.                 */
/* -------------------------------------------------------------------- */
#if defined(__MSVCRT__) || defined(_MSC_VER)
    const static char  path_split = '\\';
#else
    const static char  path_split = '/';
#endif

/* -------------------------------------------------------------------- */
/*      Merge paths.                                                    */
/* -------------------------------------------------------------------- */
    std::string base_path = ExtractPath( base );
    std::string result;

    if( base_path == "" )
        return src_filename;

    result = base_path;
    result += path_split;
    result += src_filename;

/* -------------------------------------------------------------------- */
/*      Check if the target exists by this name.                        */
/* -------------------------------------------------------------------- */
    try
    {
        void *hFile = io_interfaces->Open( result, "r" );
        // should throw an exception on failure.
        io_interfaces->Close( hFile );
        return result;
    }
    catch( ... )
    {
        return src_filename;
    }
}


/************************************************************************/
/*                            DefaultDebug()                            */
/*                                                                      */
/*      Default implementation of the Debug() output interface.         */
/************************************************************************/

void PCIDSK::DefaultDebug( const char * message )

{
    static bool initialized = false;
    static bool enabled = false;

    if( !initialized )
    {
        if( getenv( "PCIDSK_DEBUG" ) != nullptr )
            enabled = true;

        initialized = true;
    }

    if( enabled )
        std::cerr << message;
}

/************************************************************************/
/*                               vDebug()                               */
/*                                                                      */
/*      Helper function for Debug().                                    */
/************************************************************************/

static void vDebug( void (*pfnDebug)(const char *),
                    const char *fmt, std::va_list args )

{
    std::string message;

/* -------------------------------------------------------------------- */
/*      This implementation for platforms without vsnprintf() will      */
/*      just plain fail if the formatted contents are too large.        */
/* -------------------------------------------------------------------- */
#if defined(MISSING_VSNPRINTF)
    char *pszBuffer = (char *) malloc(30000);
    if( vsprintf( pszBuffer, fmt, args) > 29998 )
    {
        message = "PCIDSK::Debug() ... buffer overrun.";
    }
    else
        message = pszBuffer;

    free( pszBuffer );

/* -------------------------------------------------------------------- */
/*      This should grow a big enough buffer to hold any formatted      */
/*      result.                                                         */
/* -------------------------------------------------------------------- */
#else
    char szModestBuffer[500];
    int nPR;
    va_list wrk_args;

#ifdef va_copy
    va_copy( wrk_args, args );
#else
    wrk_args = args;
#endif

    nPR = vsnprintf( szModestBuffer, sizeof(szModestBuffer), fmt,
                     wrk_args );
    if( nPR == -1 || nPR >= (int) sizeof(szModestBuffer)-1 )
    {
        int nWorkBufferSize = 2000;
        PCIDSKBuffer oWorkBuffer(nWorkBufferSize);

#ifdef va_copy
        va_end( wrk_args );
        va_copy( wrk_args, args );
#else
        wrk_args = args;
#endif
        while( (nPR=vsnprintf( oWorkBuffer.buffer, nWorkBufferSize, fmt, wrk_args))
               >= nWorkBufferSize-1
               || nPR == -1 )
        {
            nWorkBufferSize *= 4;
            oWorkBuffer.SetSize(nWorkBufferSize);
#ifdef va_copy
            va_end( wrk_args );
            va_copy( wrk_args, args );
#else
            wrk_args = args;
#endif
        }
        message = oWorkBuffer.buffer;
    }
    else
    {
        message = szModestBuffer;
    }
    va_end( wrk_args );
#endif

/* -------------------------------------------------------------------- */
/*      Forward the message.                                            */
/* -------------------------------------------------------------------- */
    pfnDebug( message.c_str() );
}

/************************************************************************/
/*                               Debug()                                */
/*                                                                      */
/*      Function to write output to a debug stream if one is            */
/*      enabled.  This is intended to be widely called in the           */
/*      library.                                                        */
/************************************************************************/

void PCIDSK::Debug( void (*pfnDebug)(const char *), const char *fmt, ... )

{
    if( pfnDebug == nullptr )
        return;

    std::va_list args;

    va_start( args, fmt );
    vDebug( pfnDebug, fmt, args );
    va_end( args );
}

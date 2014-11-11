/******************************************************************************
 *
 * Purpose:  Various private (undocumented) utility functions.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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
#include "pcidsk_exception.h"
#include "pcidsk_georef.h"
#include "pcidsk_io.h"
#include "core/pcidsk_utils.h"
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <iostream>

using namespace PCIDSK;

#if defined(_MSC_VER) && (_MSC_VER < 1500)
#  define vsnprintf _vsnprintf
#endif

/************************************************************************/
/*                         GetCurrentDateTime()                         */
/************************************************************************/

// format we want: "HH:MM DDMMMYYYY \0"

#include <time.h>
#include <sys/types.h>

void	PCIDSK::GetCurrentDateTime( char *out_time )

{
    time_t	    clock;
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
 * @brief Perform an endianess swap for a given buffer of pixels
 *
 * Baed on the provided data type, do an appropriate endianess swap for
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
    case CHN_32R:
        SwapData(data, DataTypeSize(type), count);
        break;
    case CHN_C16U:
    case CHN_C16S:
    case CHN_C32R:
        SwapData(data, DataTypeSize(type) / 2, count * 2);
        break;
    default:
        ThrowPCIDSKException("Unknown data type passed to SwapPixels."
            "This is a software bug. Please contact your vendor.");
    }
}

/************************************************************************/
/*                              SwapData()                              */
/************************************************************************/

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
        ThrowPCIDSKException( "Unsupported data size in SwapData()" );
}

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

void PCIDSK::ParseTileFormat( std::string full_text, 
                              int &block_size, std::string &compression )

{
    compression = "NONE";
    block_size = 127;

    UCaseStr( full_text );

/* -------------------------------------------------------------------- */
/*      Only operate on tiled stuff.                                    */
/* -------------------------------------------------------------------- */
    if( strncmp(full_text.c_str(),"TILED",5) != 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Do we have a block size?                                        */
/* -------------------------------------------------------------------- */
    const char *next_text = full_text.c_str() + 5;

    if( isdigit(*next_text) )
    {
        block_size = atoi(next_text);
        while( isdigit(*next_text) )
            next_text++;
    }
    
    while( *next_text == ' ' )
        next_text++;

/* -------------------------------------------------------------------- */
/*      Do we have a compression type?                                  */
/* -------------------------------------------------------------------- */
    if( *next_text != '\0' )
    {
        compression = next_text;
        if (compression == "NO_WARNINGS")
            compression = "";
        else if( compression != "RLE"
            && strncmp(compression.c_str(),"JPEG",4) != 0 
            && compression != "NONE"
            && compression != "QUADTREE" )
        {
            ThrowPCIDSKException( "Unsupported tile compression scheme '%s' requested.",
                                  compression.c_str() );
        }
    }    
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

int PCIDSK::pci_strncasecmp( const char *string1, const char *string2, int len )

{
    int i;

    for( i = 0; i < len; i++ )
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
/*                         ProjParmsFromText()                          */
/*                                                                      */
/*      function to turn a ProjParms string (17 floating point          */
/*      numbers) into an array, as well as attaching the units code     */
/*      derived from the geosys string.                                 */
/************************************************************************/

std::vector<double> PCIDSK::ProjParmsFromText( std::string geosys, 
                                               std::string sparms )

{
    std::vector<double> dparms;
    const char *next = sparms.c_str();

    for( next = sparms.c_str(); *next != '\0'; )
    {
        dparms.push_back( CPLAtof(next) );

        // move past this token
        while( *next != '\0' && *next != ' ' )
            next++;

        // move past white space.
        while( *next != '\0' && *next == ' ' )
            next++;
    }

    dparms.resize(18);

    // This is rather iffy!
    if( EQUALN(geosys.c_str(),"DEGREE",3) )
        dparms[17] = (double) (int) UNIT_DEGREE;
    else if( EQUALN(geosys.c_str(),"MET",3) )
        dparms[17] = (double) (int) UNIT_METER;
    else if( EQUALN(geosys.c_str(),"FOOT",4) )
        dparms[17] = (double) (int) UNIT_US_FOOT;
    else if( EQUALN(geosys.c_str(),"FEET",4) )
        dparms[17] = (double) (int) UNIT_US_FOOT;
    else if( EQUALN(geosys.c_str(),"INTL FOOT",5) )
        dparms[17] = (double) (int) UNIT_INTL_FOOT;
    else if( EQUALN(geosys.c_str(),"SPCS",4) )
        dparms[17] = (double) (int) UNIT_METER;
    else if( EQUALN(geosys.c_str(),"SPIF",4) )
        dparms[17] = (double) (int) UNIT_INTL_FOOT;
    else if( EQUALN(geosys.c_str(),"SPAF",4) )
        dparms[17] = (double) (int) UNIT_US_FOOT;
    else
        dparms[17] = -1.0; /* unknown */
    
    return dparms;
}

/************************************************************************/
/*                          ProjParmsToText()                           */
/************************************************************************/

std::string PCIDSK::ProjParmsToText( std::vector<double> dparms )

{
    unsigned int i;
    std::string sparms;

    for( i = 0; i < 17; i++ )
    {
        char value[64];
        double dvalue;

        if( i < dparms.size() )
            dvalue = dparms[i];
        else
            dvalue = 0.0;

        if( dvalue == floor(dvalue) )
            sprintf( value, "%d", (int) dvalue );
        else
            CPLsprintf( value, "%.15g", dvalue );
        
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
/*      be passed a bare path.  The trailing directory delimeter is     */
/*      removed from the result.  The return result is an empty         */
/*      string for a simple filename passed in with no directory        */
/*      component.                                                      */
/************************************************************************/

std::string PCIDSK::ExtractPath( std::string filename )

{
    int i;

    for( i = filename.size()-1; i >= 0; i-- )
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
/*                         MergeRelativePath()                          */
/*                                                                      */
/*      This attempts to take src_filename and make it relative to      */
/*      the base of the file "base", if this evaluates to a new file    */
/*      in the filesystem.  It will not make any change if              */
/*      src_filename appears to be absolute or if the altered path      */
/*      does not resolve to a file in the filesystem.                   */
/************************************************************************/

std::string PCIDSK::MergeRelativePath( const PCIDSK::IOInterfaces *io_interfaces,
                                       std::string base, 
                                       std::string src_filename )

{
/* -------------------------------------------------------------------- */
/*      Does src_filename appear to be absolute?                        */
/* -------------------------------------------------------------------- */
    if( src_filename.size() == 0 )
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
        if( getenv( "PCIDSK_DEBUG" ) != NULL )
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
        char *pszWorkBuffer = (char *) malloc(nWorkBufferSize);

#ifdef va_copy
        va_end( wrk_args );
        va_copy( wrk_args, args );
#else
        wrk_args = args;
#endif
        while( (nPR=vsnprintf( pszWorkBuffer, nWorkBufferSize, fmt, wrk_args))
               >= nWorkBufferSize-1 
               || nPR == -1 )
        {
            nWorkBufferSize *= 4;
            pszWorkBuffer = (char *) realloc(pszWorkBuffer, 
                                             nWorkBufferSize );
#ifdef va_copy
            va_end( wrk_args );
            va_copy( wrk_args, args );
#else
            wrk_args = args;
#endif
        }
        message = pszWorkBuffer;
        free( pszWorkBuffer );
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
    if( pfnDebug == NULL )
        return;

    std::va_list args;

    va_start( args, fmt );
    vDebug( pfnDebug, fmt, args );
    va_end( args );
}

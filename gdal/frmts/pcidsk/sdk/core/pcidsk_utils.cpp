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
#include "core/pcidsk_utils.h"
#include <cstdlib>
#include <cstring>
#include <cctype>

using namespace PCIDSK;

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
            target[i] = toupper(target[i]);
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
            c1 = toupper(c1);
        if( islower(c2) )
            c2 = toupper(c2);

        if( c1 < c2 )
            return -1;
        else if( c1 > c2 )
            return 1;
        else 
            return 0;
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
            c1 = toupper(c1);
        if( islower(c2) )
            c2 = toupper(c2);

        if( c1 < c2 )
            return -1;
        else if( c1 > c2 )
            return 1;
    }

    return 0;
}


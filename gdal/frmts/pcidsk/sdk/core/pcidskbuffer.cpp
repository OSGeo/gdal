/******************************************************************************
 *
 * Purpose:  Implementation of the PCIDSKBuffer class.  This class is for
 *           convenient parsing and formatting of PCIDSK ASCII headers.
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

#include "pcidsk_buffer.h"
#include "pcidsk_exception.h"
#include "core/pcidsk_utils.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace PCIDSK;

#ifdef _MSC_VER
#ifndef snprintf
#define snprintf _snprintf
#endif // !defined(snprintf)
#endif

/************************************************************************/
/*                            PCIDSKBuffer()                            */
/************************************************************************/

PCIDSKBuffer::PCIDSKBuffer( int size )

{
    buffer_size = 0;
    buffer = NULL;

    if( size > 0 )
        SetSize( size );
}

/************************************************************************/
/*                            PCIDSKBuffer()                            */
/************************************************************************/

PCIDSKBuffer::PCIDSKBuffer( const char *src, int size )

{
    buffer_size = 0;
    buffer = NULL;

    SetSize( size );
    memcpy( buffer, src, size );
}

/************************************************************************/
/*                           ~PCIDSKBuffer()                            */
/************************************************************************/

PCIDSKBuffer::~PCIDSKBuffer()

{
    free( buffer );
}

/************************************************************************/
/*                              SetSize()                               */
/************************************************************************/

void PCIDSKBuffer::SetSize( int size )

{
    buffer_size = size;
    if( buffer == NULL )
        buffer = (char *) malloc(size+1);
    else
        buffer = (char *) realloc(buffer,size+1);

    if( buffer == NULL )
    {
        buffer_size = 0;
        ThrowPCIDSKException( "Out of memory allocating %d byte PCIDSKBuffer.",
                               size );
    }

    buffer[size] = '\0';
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

const char *PCIDSKBuffer::Get( int offset, int size )

{
    Get( offset, size, work_field, 0 );
    return work_field.c_str();
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

void PCIDSKBuffer::Get( int offset, int size, std::string &target, int unpad )

{
    if( offset + size > buffer_size )
        ThrowPCIDSKException( "Get() past end of PCIDSKBuffer." );

    if( unpad )
    {
        while( size > 0 && buffer[offset+size-1] == ' ' )
            size--;
    }

    target.assign( buffer + offset, size );
}

/************************************************************************/
/*                             GetUInt64()                              */
/************************************************************************/

uint64 PCIDSKBuffer::GetUInt64( int offset, int size )

{
    std::string value_str;

    if( offset + size > buffer_size )
        ThrowPCIDSKException( "GetUInt64() past end of PCIDSKBuffer." );

    value_str.assign( buffer + offset, size );

    return atouint64(value_str.c_str());
}

/************************************************************************/
/*                               GetInt()                               */
/************************************************************************/

int PCIDSKBuffer::GetInt( int offset, int size )

{
    std::string value_str;

    if( offset + size > buffer_size )
        ThrowPCIDSKException( "GetInt() past end of PCIDSKBuffer." );

    value_str.assign( buffer + offset, size );

    return atoi(value_str.c_str());
}

/************************************************************************/
/*                             GetDouble()                              */
/************************************************************************/

double PCIDSKBuffer::GetDouble( int offset, int size )

{
    std::string value_str;

    if( offset + size > buffer_size )
        ThrowPCIDSKException( "GetDouble() past end of PCIDSKBuffer." );

    value_str.assign( buffer + offset, size );

/* -------------------------------------------------------------------- */
/*      PCIDSK uses FORTRAN 'D' format for doubles - convert to 'E'     */
/*      (C style) before calling atof.                                  */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; i < size; i++ )
    {
        if( value_str[i] == 'D' )
            value_str[i] = 'E';
    }

    return atof(value_str.c_str());
}

/************************************************************************/
/*                                Put()                                 */
/************************************************************************/

void PCIDSKBuffer::Put( const char *value, int offset, int size )

{
    if( offset + size > buffer_size )
        ThrowPCIDSKException( "Put() past end of PCIDSKBuffer." );

    int v_size = strlen(value);
    if( v_size > size )
        v_size = size;

    if( v_size < size )
        memset( buffer + offset, ' ', size );

    memcpy( buffer + offset, value, v_size );
}

/************************************************************************/
/*                             Put(uint64)                              */
/************************************************************************/

void PCIDSKBuffer::Put( uint64 value, int offset, int size )

{
    char fmt[64];
    char wrk[128];

    sprintf( fmt, "%%%d%sd", size, PCIDSK_FRMT_64_WITHOUT_PREFIX );
    sprintf( wrk, fmt, value );

    Put( wrk, offset, size );
}

/************************************************************************/
/*                             Put(double)                              */
/************************************************************************/

void PCIDSKBuffer::Put( double value, int offset, int size, 
                        const char *fmt )

{
    if( fmt == NULL )
        fmt = "%g";

    char wrk[128];
    snprintf( wrk, 127, fmt, value );

    char *exponent = strstr(wrk,"E");
    if( exponent != NULL )
        *exponent = 'D';

    Put( wrk, offset, size );
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

PCIDSKBuffer &PCIDSKBuffer::operator=( const PCIDSKBuffer &src )

{
    SetSize( src.buffer_size );
    memcpy( buffer, src.buffer, buffer_size );

    return *this;
}

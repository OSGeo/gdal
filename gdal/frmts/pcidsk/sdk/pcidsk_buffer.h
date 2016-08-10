/******************************************************************************
 *
 * Purpose:  Declaration of the PCIDSKBuffer class
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
#ifndef INCLUDE_PCIDSKBUFFER_H
#define INCLUDE_PCIDSKBUFFER_H

#include "pcidsk_config.h"

#include <string>

namespace PCIDSK
{
/************************************************************************/
/*                             PCIDSKBuffer                             */
/*                                                                      */
/*      Convenience class for managing ascii headers of various         */
/*      sorts.  Primarily for internal use.                             */
/************************************************************************/

    class PCIDSKBuffer 
    {
        friend class MetadataSegment; // ?
    public:
        PCIDSKBuffer( int size = 0 );
        PCIDSKBuffer( const char *src, int size );
        ~PCIDSKBuffer();

        char	*buffer;
        int         buffer_size;

        PCIDSKBuffer &operator=(const PCIDSKBuffer& src);

        const char *Get( int offset, int size ) const;
        void        Get( int offset, int size, std::string &target, int unpad=1 ) const;

        double      GetDouble( int offset, int size ) const;
        int         GetInt( int offset, int size ) const;
        int64       GetInt64( int offset, int size ) const;
        uint64      GetUInt64( int offset, int size ) const;

        void        Put( const char *value,  int offset, int size, bool null_term = false );
        void        Put( uint64 value, int offset, int size );
        void        Put( double value, int offset, int size, const char *fmt=NULL );
        void        Put( int value, int offset, int size ) 
            { Put( (uint64) value, offset, size ); }
        void        Put( unsigned int value, int offset, int size ) 
            { Put( (uint64) value, offset, size ); }

        void        PutBin(double value, int offset);

        void        SetSize( int size );
        
    private:
        mutable std::string work_field;
    };
} // end namespace PCIDSK
#endif // INCLUDE_PCIDSKBUFFER_H 

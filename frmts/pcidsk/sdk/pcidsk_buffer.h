/******************************************************************************
 *
 * Purpose:  Declaration of the PCIDSKBuffer class
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

        char        *buffer;
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
        void        Put( double value, int offset, int size, const char *fmt=nullptr );
        void        Put( int value, int offset, int size )
            { Put( (uint64) value, offset, size ); }
        void        Put( unsigned int value, int offset, int size )
            { Put( (uint64) value, offset, size ); }

        void        PutBin(double value, int offset);
        void        PutBin(int16 value, int offset);

        void        SetSize( int size );

    private:
        mutable std::string work_field;
    };
} // end namespace PCIDSK
#endif // INCLUDE_PCIDSKBUFFER_H

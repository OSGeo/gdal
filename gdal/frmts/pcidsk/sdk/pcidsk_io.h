/******************************************************************************
 *
 * Purpose:  PCIDSK I/O Interface declaration. The I/O interfaces for the
 *           library can be overridden by an object implementing this class.
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
#ifndef __INCLUDE_PCIDSK_IO_H
#define __INCLUDE_PCIDSK_IO_H

#include "pcidsk_config.h"

#include <string>

namespace PCIDSK
{
/************************************************************************/
/*                             IOInterfaces                             */
/************************************************************************/

//! IO Interface class.

    class IOInterfaces
    {
    public:
        virtual ~IOInterfaces() {}
        virtual void   *Open( std::string filename, std::string access ) const = 0;
        virtual uint64  Seek( void *io_handle, uint64 offset, int whence ) const = 0;
        virtual uint64  Tell( void *io_handle ) const = 0;
        virtual uint64  Read( void *buffer, uint64 size, uint64 nmemb, void *io_handle ) const = 0;
        virtual uint64  Write( const void *buffer, uint64 size, uint64 nmemb, void *io_handle ) const = 0;
        virtual int     Eof( void *io_handle ) const = 0;
        virtual int     Flush( void *io_handle ) const = 0;
        virtual int     Close( void *io_handle ) const = 0;
    };

    const IOInterfaces PCIDSK_DLL *GetDefaultIOInterfaces();

} // end namespace PCIDSK

#endif // __INCLUDE_PCIDSK_IO_H

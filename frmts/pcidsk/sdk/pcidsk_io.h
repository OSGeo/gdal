/******************************************************************************
 *
 * Purpose:  PCIDSK I/O Interface declaration. The I/O interfaces for the
 *           library can be overridden by an object implementing this class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_IO_H
#define INCLUDE_PCIDSK_IO_H

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
        virtual void   *Open( const std::string& filename, std::string access ) const = 0;
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

#endif // INCLUDE_PCIDSK_IO_H

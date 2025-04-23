/******************************************************************************
 *
 * Purpose:  Implementation of the Open() function.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#include "pcidsk.h"
#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_file.h"
#include "pcidsk_interfaces.h"
#include "core/cpcidskfile.h"
#include <string>
#include <cstring>
#include <cassert>

using namespace PCIDSK;

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

/**
 * Open a PCIDSK (.pix) file.
 *
 * This function attempts to open the named file, with the indicated
 * access and the provided set of system interface methods.
 *
 * @param filename the name of the PCIDSK file to access.
 * @param access either "r" for read-only, or "r+" for read-write access.
 * @param interfaces Either NULL to use default interfaces, or a pointer
 * to a populated interfaces object.
 *
 * @return a pointer to a file object for accessing the PCIDSK file.
 */

PCIDSKFile *PCIDSK::Open( const std::string& filename, const std::string& access,
                          const PCIDSKInterfaces *interfaces,
                          int max_channel_count )

{
/* -------------------------------------------------------------------- */
/*      Use default interfaces if none are passed in.                   */
/* -------------------------------------------------------------------- */
    PCIDSKInterfaces default_interfaces;
    if( interfaces == nullptr )
        interfaces = &default_interfaces;

/* -------------------------------------------------------------------- */
/*      First open the file, and confirm that it is PCIDSK before       */
/*      going further.                                                  */
/* -------------------------------------------------------------------- */
    void *io_handle = interfaces->io->Open( filename, access );

    assert( io_handle != nullptr );

    char header_check[6];

    if( interfaces->io->Read( header_check, 1, 6, io_handle ) != 6
        || memcmp(header_check,"PCIDSK",6) != 0 )
    {
        interfaces->io->Close( io_handle );
        return (PCIDSKFile*)ThrowPCIDSKExceptionPtr( "File %s does not appear to be PCIDSK format.",
                              filename.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Create the PCIDSKFile object.                                   */
/* -------------------------------------------------------------------- */

    CPCIDSKFile *file = new CPCIDSKFile( filename );

    file->interfaces = *interfaces;
    file->io_handle = io_handle;
    file->io_mutex = interfaces->CreateMutex();

    if( strstr(access.c_str(),"+") != nullptr )
        file->updatable = true;

/* -------------------------------------------------------------------- */
/*      Initialize it from the header.                                  */
/* -------------------------------------------------------------------- */
    try
    {
        file->InitializeFromHeader(max_channel_count);
    }
    catch(...)
    {
        delete file;
        throw;
    }

    return file;
}

/******************************************************************************
 * $Id$
 *
 * Project:  PCIDSK Database File
 * Purpose:  PCIDSK SDK compatiable io interface built on VSI.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "pcidsk.h"

CPL_CVSID("$Id$");

using namespace PCIDSK;

EDBFile *GDAL_EDBOpen( std::string osFilename, std::string osAccess );

class VSI_IOInterface : public IOInterfaces
{
    virtual void   *Open( std::string filename, std::string access ) const;
    virtual uint64  Seek( void *io_handle, uint64 offset, int whence ) const;
    virtual uint64  Tell( void *io_handle ) const;
    virtual uint64  Read( void *buffer, uint64 size, uint64 nmemb, void *io_hanle ) const;
    virtual uint64  Write( const void *buffer, uint64 size, uint64 nmemb, void *io_handle ) const;
    virtual int     Eof( void *io_handle ) const;
    virtual int     Flush( void *io_handle ) const;
    virtual int     Close( void *io_handle ) const;

    const char     *LastError() const;
};

/************************************************************************/
/*                       PCIDSK2GetIOInterfaces()                       */
/************************************************************************/

const PCIDSK::PCIDSKInterfaces *PCIDSK2GetInterfaces()
{
    static VSI_IOInterface singleton_vsi_interface;
    static PCIDSKInterfaces singleton_pcidsk2_interfaces;

    singleton_pcidsk2_interfaces.io = &singleton_vsi_interface;
    singleton_pcidsk2_interfaces.OpenEDB = GDAL_EDBOpen;

    return &singleton_pcidsk2_interfaces;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

void *
VSI_IOInterface::Open( std::string filename, std::string access ) const

{
    VSILFILE *fp = VSIFOpenL( filename.c_str(), access.c_str() );

    if( fp == NULL )
        ThrowPCIDSKException( "Failed to open %s: %s", 
                              filename.c_str(), LastError() );

    return fp;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

uint64 
VSI_IOInterface::Seek( void *io_handle, uint64 offset, int whence ) const

{
    VSILFILE *fp = (VSILFILE *) io_handle;

    uint64 result = VSIFSeekL( fp, offset, whence );

    if( result == (uint64) -1 )
        ThrowPCIDSKException( "Seek(%d,%d): %s", 
                              (int) offset, whence, 
                              LastError() );

    return result;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

uint64 VSI_IOInterface::Tell( void *io_handle ) const

{
    VSILFILE *fp = (VSILFILE *) io_handle;

    return VSIFTellL( fp );
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

uint64 VSI_IOInterface::Read( void *buffer, uint64 size, uint64 nmemb, 
                               void *io_handle ) const

{
    VSILFILE *fp = (VSILFILE *) io_handle;

    errno = 0;

    uint64 result = VSIFReadL( buffer, (size_t) size, (size_t) nmemb, fp );

    if( errno != 0 && result == 0 && nmemb != 0 )
        ThrowPCIDSKException( "Read(%d): %s", 
                              (int) size * nmemb,
                              LastError() );

    return result;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

uint64 VSI_IOInterface::Write( const void *buffer, uint64 size, uint64 nmemb, 
                                void *io_handle ) const

{
    VSILFILE *fp = (VSILFILE *) io_handle;

    errno = 0;

    uint64 result = VSIFWriteL( buffer, (size_t) size, (size_t) nmemb, fp );

    if( errno != 0 && result == 0 && nmemb != 0 )
        ThrowPCIDSKException( "Write(%d): %s", 
                              (int) size * nmemb,
                              LastError() );

    return result;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSI_IOInterface::Eof( void *io_handle ) const

{
    return VSIFEofL( (VSILFILE *) io_handle );
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSI_IOInterface::Flush( void *io_handle ) const

{
    return VSIFFlushL( (VSILFILE *) io_handle );
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSI_IOInterface::Close( void *io_handle ) const

{
    return VSIFCloseL( (VSILFILE *) io_handle );
}

/************************************************************************/
/*                             LastError()                              */
/*                                                                      */
/*      Return a string representation of the last error.               */
/************************************************************************/

const char *VSI_IOInterface::LastError() const

{
    return strerror( errno );
}

/************************************************************************/
/*       If we are using the internal copy of the PCIDSK SDK we need    */
/*      to provide stub implementations of GetDefaultIOInterfaces()     */
/*      and GetDefaultMutex()                                           */
/************************************************************************/

#ifdef PCIDSK_INTERNAL

const IOInterfaces *PCIDSK::GetDefaultIOInterfaces()
{
    static VSI_IOInterface singleton_vsi_interface;

    return &singleton_vsi_interface;
}

/************************************************************************/
/*                            CPLThreadMutex                            */
/************************************************************************/

class CPLThreadMutex : public PCIDSK::Mutex

{
private:
    void    *hMutex;

public:
    CPLThreadMutex();
    ~CPLThreadMutex();

    int Acquire(void);
    int Release(void);
};

/************************************************************************/
/*                            CPLThreadMutex()                            */
/************************************************************************/

CPLThreadMutex::CPLThreadMutex()

{
    hMutex = CPLCreateMutex();
    CPLReleaseMutex( hMutex ); // it is created acquired, but we want it free.
}

/************************************************************************/
/*                           ~CPLThreadMutex()                            */
/************************************************************************/

CPLThreadMutex::~CPLThreadMutex()

{
    CPLDestroyMutex( hMutex );
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

int CPLThreadMutex::Release()

{
    CPLReleaseMutex( hMutex );
    return 1;
}

/************************************************************************/
/*                              Acquire()                               */
/************************************************************************/

int CPLThreadMutex::Acquire()

{
    return CPLAcquireMutex( hMutex, 100.0 );
}

/************************************************************************/
/*                         DefaultCreateMutex()                         */
/************************************************************************/

PCIDSK::Mutex *PCIDSK::DefaultCreateMutex(void)

{
    return new CPLThreadMutex();
}

#endif /* def PCIDSK_INTERNAL */

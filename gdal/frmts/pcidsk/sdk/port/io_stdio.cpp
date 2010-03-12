/******************************************************************************
 *
 * Purpose:  Implementation of a stdio based IO layer.
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

#include "pcidsk_io.h"
#include "pcidsk_exception.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>

using namespace PCIDSK;

class StdioIOInterface : public IOInterfaces
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

typedef struct {
    FILE   *fp;
    uint64 offset;
    bool   last_op_write;
} FileInfo;

/************************************************************************/
/*                       GetDefaultIOInterfaces()                       */
/************************************************************************/

/**
 * Fetch default IO interfaces.
 *
 * Returns the default IO interfaces implemented in the PCIDSK library.
 * These are suitable for use in a PCIDSK::PCIDSKInterfaces object.
 *
 * @return pointer to internal IO interfaces class.
 */
const IOInterfaces *PCIDSK::GetDefaultIOInterfaces()
{
    static StdioIOInterface singleton_stdio_interface;

    return &singleton_stdio_interface;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

void *
StdioIOInterface::Open( std::string filename, std::string access ) const

{
    std::string adjusted_access = access;

    adjusted_access += "b";

    FILE *fp = fopen( filename.c_str(), adjusted_access.c_str() );

    if( fp == NULL )
        ThrowPCIDSKException( "Failed to open %s: %s", 
                              filename.c_str(), LastError() );

    FileInfo *fi = new FileInfo();
    fi->fp = fp;
    fi->offset = 0;
    fi->last_op_write = false;

    return fi;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

uint64 
StdioIOInterface::Seek( void *io_handle, uint64 offset, int whence ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    // seeks that do nothing are still surprisingly expensive with MSVCRT.
    // try and short circuit if possible.
    if( whence == SEEK_SET && offset == fi->offset )
        return 0;

    uint64 result = fseek( fi->fp, offset, whence );

    if( result == (uint64) -1 )
        ThrowPCIDSKException( "Seek(%d,%d): %s", 
                              (int) offset, whence, 
                              LastError() );

    if( whence == SEEK_SET )
        fi->offset = offset;
    else if( whence == SEEK_END )
        fi->offset = ftell( fi->fp );
    else if( whence == SEEK_CUR )
        fi->offset += offset;

    fi->last_op_write = false;

    return result;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

uint64 StdioIOInterface::Tell( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    return ftell( fi->fp );
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

uint64 StdioIOInterface::Read( void *buffer, uint64 size, uint64 nmemb, 
                               void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    errno = 0;

/* -------------------------------------------------------------------- */
/*      If a fwrite() is followed by an fread(), the POSIX rules are    */
/*      that some of the write may still be buffered and lost.  We      */
/*      are required to do a seek between to force flushing.   So we    */
/*      keep careful track of what happened last to know if we          */
/*      skipped a flushing seek that we may need to do now.             */
/* -------------------------------------------------------------------- */
    if( fi->last_op_write )
        fseek( fi->fp, fi->offset, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Do the read.                                                    */
/* -------------------------------------------------------------------- */
    uint64 result = fread( buffer, size, nmemb, fi->fp );

    if( errno != 0 && result == 0 && nmemb != 0 )
        ThrowPCIDSKException( "Read(%d): %s", 
                              (int) size * nmemb,
                              LastError() );

    fi->offset += size*result;
    fi->last_op_write = false;

    return result;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

uint64 StdioIOInterface::Write( const void *buffer, uint64 size, uint64 nmemb, 
                                void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;

    errno = 0;

    uint64 result = fwrite( buffer, size, nmemb, fi->fp );

    if( errno != 0 && result == 0 && nmemb != 0 )
        ThrowPCIDSKException( "Write(%d): %s", 
                              (int) size * nmemb,
                              LastError() );

    fi->offset += size*result;
    fi->last_op_write = true;

    return result;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int StdioIOInterface::Eof( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;
    return feof( fi->fp );
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int StdioIOInterface::Flush( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;
    return fflush( fi->fp );
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int StdioIOInterface::Close( void *io_handle ) const

{
    FileInfo *fi = (FileInfo *) io_handle;
    int result = fclose( fi->fp );

    delete fi;

    return result;
}

/************************************************************************/
/*                             LastError()                              */
/*                                                                      */
/*      Return a string representation of the last error.               */
/************************************************************************/

const char *StdioIOInterface::LastError() const

{
    return strerror( errno );
}

/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Private declarations for classes related to the virtual filesystem
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef CPL_VSI_VIRTUAL_H_INCLUDED
#define CPL_VSI_VIRTUAL_H_INCLUDED

#include "cpl_vsi.h"

#if defined(WIN32CE)
#  include "cpl_wince.h"
#  include <wce_errno.h>
#  pragma warning(disable:4786) /* Remove annoying warnings in eVC++ and VC++ 6.0 */
#endif

#include <map>
#include <vector>
#include <string>

/************************************************************************/
/*                           VSIVirtualHandle                           */
/************************************************************************/

class VSIVirtualHandle { 
  public:
    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) = 0;
    virtual vsi_l_offset Tell() = 0;
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb ) = 0;
    virtual size_t    Write( const void *pBuffer, size_t nSize,size_t nMemb)=0;
    virtual int       Eof() = 0;
    virtual int       Flush() {return 0;}
    virtual int       Close() = 0;
    virtual           ~VSIVirtualHandle() { }
};

/************************************************************************/
/*                         VSIFilesystemHandler                         */
/************************************************************************/

class VSIFilesystemHandler {

public:
    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess) = 0;
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf) = 0;
    virtual int      Unlink( const char *pszFilename ) 
        		{ errno=ENOENT; return -1; }
    virtual int      Mkdir( const char *pszDirname, long nMode ) 
        		{ errno=ENOENT; return -1; }
    virtual int      Rmdir( const char *pszDirname ) 
			{ errno=ENOENT; return -1; }
    virtual char   **ReadDir( const char *pszDirname ) 
			{ return NULL; }
    virtual          ~VSIFilesystemHandler() {}
    virtual int      Rename( const char *oldpath, const char *newpath )
        		{ errno=ENOENT; return -1; }
};

/************************************************************************/
/*                            VSIFileManager                            */
/************************************************************************/

class VSIFileManager 
{
private:
    VSIFilesystemHandler         *poDefaultHandler;
    std::map<std::string,VSIFilesystemHandler *>   oHandlers;

    VSIFileManager();

    static VSIFileManager *Get();

public:
    ~VSIFileManager();

    static VSIFilesystemHandler *GetHandler( const char * );
    static void                InstallHandler( std::string osPrefix, 
                                               VSIFilesystemHandler * );
    static void                RemoveHandler( std::string osPrefix );
};

#endif /* ndef CPL_VSI_VIRTUAL_H_INCLUDED */

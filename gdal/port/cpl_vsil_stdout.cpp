/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for stdout
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2010, Even Rouault
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_vsi_virtual.h"

#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdoutFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutFilesystemHandler : public VSIFilesystemHandler
{
public:
    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf );
};

/************************************************************************/
/* ==================================================================== */
/*                        VSIStdoutHandle                               */
/* ==================================================================== */
/************************************************************************/

class VSIStdoutHandle : public VSIVirtualHandle
{
  public:

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
};

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIStdoutHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    CPLError(CE_Failure, CPLE_NotSupported, "Seek() unsupported on /vsistdout");
    return -1;
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIStdoutHandle::Tell()
{
    return ftell(stdout);
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIStdoutHandle::Flush()

{
    return fflush( stdout );
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIStdoutHandle::Read( void * pBuffer, size_t nSize, size_t nCount )

{
    CPLError(CE_Failure, CPLE_NotSupported, "Read() unsupported on /vsistdout");
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIStdoutHandle::Write( const void * pBuffer, size_t nSize, 
                                  size_t nCount )

{
    return fwrite(pBuffer, nSize, nCount, stdout);
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIStdoutHandle::Eof()

{
    return feof(stdout);
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIStdoutHandle::Close()

{
    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIStdoutFilesystemHandler                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIStdoutFilesystemHandler::Open( const char *pszFilename, 
                                  const char *pszAccess )

{
    if ( strchr(pszAccess, 'r') != NULL ||
         strchr(pszAccess, '+') != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Read or update mode not supported on /vsistdout");
        return NULL;
    }

#ifdef WIN32
    if ( strchr(pszAccess, 'b') != NULL )
        setmode( fileno( stdout ), O_BINARY );
#endif

    return new VSIStdoutHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIStdoutFilesystemHandler::Stat( const char * pszFilename,
                                      VSIStatBufL * pStatBuf )

{
    return -1;
}

/************************************************************************/
/*                       VSIInstallStdoutHandler()                      */
/************************************************************************/

void VSIInstallStdoutHandler()

{
    VSIFileManager::InstallHandler( "/vsistdout/", new VSIStdoutFilesystemHandler );
}

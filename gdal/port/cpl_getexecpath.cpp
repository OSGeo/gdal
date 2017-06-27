/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement CPLGetExecPath().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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
#include "cpl_conv.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "cpl_multiproc.h"
#include "cpl_string.h"


CPL_CVSID("$Id$")

#if defined(WIN32)

#define HAVE_IMPLEMENTATION 1

#include <windows.h>

/************************************************************************/
/*                           CPLGetExecPath()                           */
/************************************************************************/

int CPLGetExecPath( char *pszPathBuf, int nMaxLength )
{
    if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszPathBuf = static_cast<wchar_t *>(
            CPLCalloc(nMaxLength + 1, sizeof(wchar_t)));

        if( GetModuleFileNameW( NULL, pwszPathBuf, nMaxLength ) == 0 )
        {
            CPLFree( pwszPathBuf );
            return FALSE;
        }
        else
        {
            char *pszDecoded =
                CPLRecodeFromWChar(pwszPathBuf, CPL_ENC_UCS2, CPL_ENC_UTF8);

            strncpy( pszPathBuf, pszDecoded, nMaxLength );
            CPLFree( pszDecoded );
            CPLFree( pwszPathBuf );
            return TRUE;
        }
    }
    else
    {
        if( GetModuleFileName( NULL, pszPathBuf, nMaxLength ) == 0 )
            return FALSE;
        else
            return TRUE;
    }
}

#endif

/************************************************************************/
/*                           CPLGetExecPath()                           */
/************************************************************************/

#if !defined(HAVE_IMPLEMENTATION) && defined(__linux)

#include "cpl_multiproc.h"

#define HAVE_IMPLEMENTATION 1

int CPLGetExecPath( char *pszPathBuf, int nMaxLength )
{
    long nPID = getpid();
    CPLString osExeLink;

    osExeLink.Printf( "/proc/%ld/exe", nPID );
    ssize_t nResultLen = readlink( osExeLink, pszPathBuf, nMaxLength );
    if( nResultLen >= 0 )
        pszPathBuf[nResultLen] = '\0';
    else
        pszPathBuf[0] = '\0';

    return nResultLen > 0;
}

#endif

/************************************************************************/
/*                           CPLGetExecPath()                           */
/************************************************************************/

/**
 * Fetch path of executable.
 *
 * The path to the executable currently running is returned.  This path
 * includes the name of the executable.   Currently this only works on
 * win32 and linux platforms.  The returned path is UTF-8 encoded.
 *
 * @param pszPathBuf the buffer into which the path is placed.
 * @param nMaxLength the buffer size, MAX_PATH+1 is suggested.
 *
 * @return FALSE on failure or TRUE on success.
 */

#ifndef HAVE_IMPLEMENTATION

int CPLGetExecPath( CPL_UNUSED char * pszPathBuf, CPL_UNUSED int nMaxLength )
{
    return FALSE;
}

#endif

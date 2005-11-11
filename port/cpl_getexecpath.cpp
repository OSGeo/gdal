/**********************************************************************
 * $Id$
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
 **********************************************************************
 *
 * $Log$
 * Revision 1.2  2005/11/11 14:20:59  fwarmerdam
 * Use lower case for windows.h to allow cross compilation for windows
 * on linux (per email from Radim).
 *
 * Revision 1.1  2005/06/10 15:00:07  fwarmerdam
 * New
 *
 */

#include "cpl_conv.h"

CPL_CVSID("$Id$");

#if defined(WIN32)

#include <windows.h>

/************************************************************************/
/*                           CPLGetExecPath()                           */
/************************************************************************/

int CPLGetExecPath( char *pszPathBuf, int nMaxLength )

{
    if( GetModuleFileName( NULL, pszPathBuf, nMaxLength ) == 0 )
        return FALSE;
    else
        return TRUE;
}

#else /* ndef WIN32 */

/************************************************************************/
/*                           CPLGetExecPath()                           */
/************************************************************************/

/**
 * Fetch path of executable. 
 *
 * The path to the executable currently running is returned.  This path
 * includes the name of the executable.   Currently this only works on 
 * win32 platform. 
 *
 * @param pszPathBuf the buffer into which the path is placed.
 * @param nMaxLength the buffer size, MAX_PATH+1 is suggested.
 *
 * @return FALSE on failure or TRUE on success.
 */

int CPLGetExecPath( char *pszPathBuf, int nMaxLength )

{
    return FALSE;
}
#endif


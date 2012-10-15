/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement CPLSystem().
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2012,Even Rouault
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

#include "cpl_conv.h"

CPL_CVSID("$Id$");

#if defined(WIN32)

#include <windows.h>

/************************************************************************/
/*                            CPLSystem()                               */
/************************************************************************/

/**
 * Runs an executable in another process.
 *
 * This function runs an executable, wait for it to finish and returns
 * its exit code.
 *
 * It is implemented as CreateProcess() on Windows platforms, and system()
 * on other platforms.
 *
 * @param pszApplicationName the lpApplicationName for Windows (might be NULL),
 *                           or ignored on other platforms.
 * @param pszCommandLine the command line, starting with the executable name 
 *
 * @return the exit code of the spawned process, or -1 in case of error.
 */

int CPLSystem( const char* pszApplicationName, const char* pszCommandLine )
{
    int nRet = -1;
    PROCESS_INFORMATION processInfo;
    STARTUPINFO startupInfo;
    ZeroMemory( &processInfo, sizeof(PROCESS_INFORMATION) );
    ZeroMemory( &startupInfo, sizeof(STARTUPINFO) );
    startupInfo.cb = sizeof(STARTUPINFO);

    char* pszDupedCommandLine = (pszCommandLine) ? CPLStrdup(pszCommandLine) : NULL;

    if( !CreateProcess( pszApplicationName, 
                        pszDupedCommandLine, 
                        NULL,
                        NULL,
                        FALSE,
                        CREATE_NO_WINDOW|NORMAL_PRIORITY_CLASS,
                        NULL,
                        NULL,
                        &startupInfo,
                        &processInfo) )
    {
        DWORD err = GetLastError();
        CPLDebug("CPL", "'%s' failed : err = %d", pszCommandLine, err);
        nRet = -1;
    }
    else
    {
        WaitForSingleObject( processInfo.hProcess, INFINITE );

        DWORD exitCode;

        // Get the exit code.
        int err = GetExitCodeProcess(processInfo.hProcess, &exitCode);

        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);

        if( !err )
        {
            CPLDebug("CPL", "GetExitCodeProcess() failed : err = %d", err);
        }
        else
            nRet = exitCode;
    }

    CPLFree(pszDupedCommandLine);

    return nRet;
}

#else

/************************************************************************/
/*                            CPLSystem()                               */
/************************************************************************/

int CPLSystem( const char* pszApplicationName, const char* pszCommandLine )
{
    return system(pszCommandLine);
}

#endif

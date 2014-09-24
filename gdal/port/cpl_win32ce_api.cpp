/******************************************************************************
 * $Id$
 *
 * Name:     cpl_win32ce_api.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  ASCII wrappers around only Unicode Windows CE API.
 * Author:   Mateusz £oskot, mloskot@taxussi.com.pl
 *
 ******************************************************************************
 * Copyright (c) 2006, Mateusz £oskot
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
#include "cpl_port.h"
#include "cpl_win32ce_api.h"

#ifdef WIN32CE

CPL_CVSID("$Id$");


/* Assume UNICODE and _UNICODE are defined here and TCHAR is wide-char. */

#include <windows.h>
#include <stdlib.h>
#include <string.h>


HMODULE CE_LoadLibraryA(LPCSTR lpLibFileName)
{
    HMODULE hLib = NULL;
    
    size_t nLen = 0;
    LPTSTR pszWideStr = 0;

    /* Covert filename buffer to Unicode. */
    nLen = MultiByteToWideChar (CP_ACP, 0, lpLibFileName, -1, NULL, 0) ;
    pszWideStr = (wchar_t*)malloc(sizeof(wchar_t) * nLen);
    MultiByteToWideChar(CP_ACP, 0, lpLibFileName, -1, pszWideStr, nLen);

    hLib = LoadLibraryW(pszWideStr);

    /* Free me! */
    free(pszWideStr);

    return hLib;
}

FARPROC CE_GetProcAddressA(HMODULE hModule, LPCSTR lpProcName)
{
    FARPROC proc = NULL;

    size_t nLen = 0;
    LPTSTR pszWideStr = 0;

    /* Covert filename buffer to Unicode. */
    nLen = MultiByteToWideChar (CP_ACP, 0, lpProcName, -1, NULL, 0) ;
    pszWideStr = (wchar_t*)malloc(sizeof(wchar_t) * nLen);
    MultiByteToWideChar(CP_ACP, 0, lpProcName, -1, pszWideStr, nLen);

    proc = GetProcAddressW(hModule, pszWideStr);

    /* Free me! */
    free(pszWideStr);

    return proc;
}


DWORD CE_GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    DWORD dwLen = 0;
    TCHAR szWBuf[MAX_PATH]; /* wide-char buffer */

    if (lpFilename == NULL)
    {
        return 0; /* Error */
    }

    /* Get module filename to wide-char buffer */
    dwLen = GetModuleFileName(hModule, szWBuf, nSize);
   
    /* Covert buffer from Unicode to ANSI string. */
    WideCharToMultiByte(CP_ACP, 0, szWBuf, -1, lpFilename, dwLen, NULL, NULL);

    return dwLen;
}

HANDLE CE_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
                   DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                   HANDLE hTemplateFile)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    
    size_t nLen  = 0;
    wchar_t * pszWideStr = NULL;
    
    /* Covert filename buffer to Unicode. */
    nLen = MultiByteToWideChar (CP_ACP, 0, lpFileName, -1, NULL, 0) ;
    pszWideStr = (wchar_t*)malloc(sizeof(wchar_t) * nLen);
    MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, pszWideStr, nLen);

    hFile = CreateFileW(pszWideStr, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                       dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    /* Free me! */
    free(pszWideStr);

    return hFile;
}


#endif /* #ifdef WIN32CE */

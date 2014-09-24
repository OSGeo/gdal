/******************************************************************************
 * $Id$
 *
 * Name:     cpl_win32ce_api.h
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
#ifndef _CPL_WINCEAPI_H_INCLUDED
#define _CPL_WINCEAPI_H_INCLUDED    1

#define WIN32CE
#if defined(WIN32CE)

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*
 * Windows CE API non-Unicode Wrappers
 */

HMODULE CE_LoadLibraryA(
    LPCSTR lpLibFileName
    );

FARPROC CE_GetProcAddressA(
    HMODULE hModule,
    LPCSTR lpProcName
    );


DWORD CE_GetModuleFileNameA(
    HMODULE hModule,
    LPSTR lpFilename,
    DWORD nSize
    );

HANDLE CE_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
    );


/* Replace Windows CE API calls with our own non-Unicode equivalents. */


/* XXX - mloskot - those defines are quite confusing ! */
/*
#ifdef LoadLibrary
#  undef  LoadLibrary
#  define LoadLibrary CE_LoadLibraryA
#endif

#ifdef GetProcAddress
#  undef  GetProcAddress
#  define GetProcAddress CE_GetProcAddressA
#endif

#ifdef GetModuleFileName
#  undef  GetModuleFileName
#  define GetModuleFileName CE_GetModuleFileNameA
#endif

#ifdef CreateFile
#  undef  CreateFile
#  define CreateFile CE_CreateFileA
#endif
*/

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* #ifdef WIN32CE */

#endif /* #ifndef _CPL_WINCEAPI_H_INCLUDED */

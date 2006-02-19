 /**********************************************************************
 *
 * $Log$
 * Revision 1.1  2006/02/19 21:50:56  mloskot
 * [WCE] non-Unicode wrappers around Unicode-only Windows CE API
 *
 *
 *
 **********************************************************************/
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
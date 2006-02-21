 /**********************************************************************
 *
 * $Log$
 * Revision 1.2  2006/02/21 19:37:06  mloskot
 * [WCE] Small fixes related to char <-> wide-char string conversions
 *
 * Revision 1.1  2006/02/19 21:50:56  mloskot
 * [WCE] non-Unicode wrappers around Unicode-only Windows CE API
 *
 *
 *
 **********************************************************************/
#include "cpl_win32ce_api.h"

#ifdef WIN32CE


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

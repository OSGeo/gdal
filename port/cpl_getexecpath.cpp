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

#if defined(_WIN32)
#include <windows.h>
#elif defined(__MACH__) && defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

/************************************************************************/
/*                           CPLGetExecPath()                           */
/************************************************************************/

/**
 * Fetch path of executable.
 *
 * The path to the executable currently running is returned.  This path
 * includes the name of the executable. Currently this only works on
 * Windows, Linux, MacOS and FreeBSD platforms.  The returned path is UTF-8
 * encoded, and will be nul-terminated if success is reported.
 *
 * @param pszPathBuf the buffer into which the path is placed.
 * @param nMaxLength the buffer size (including the nul-terminating character).
 * MAX_PATH+1 is suggested.
 *
 * @return FALSE on failure or TRUE on success.
 */

int CPLGetExecPath(char *pszPathBuf, int nMaxLength)
{
    if (nMaxLength == 0)
        return FALSE;
    pszPathBuf[0] = '\0';

#if defined(_WIN32)
    if (CPLTestBool(CPLGetConfigOption("GDAL_FILENAME_IS_UTF8", "YES")))
    {
        wchar_t *pwszPathBuf =
            static_cast<wchar_t *>(CPLCalloc(nMaxLength + 1, sizeof(wchar_t)));

        if (GetModuleFileNameW(nullptr, pwszPathBuf, nMaxLength) == 0)
        {
            CPLFree(pwszPathBuf);
            return FALSE;
        }
        else
        {
            char *pszDecoded =
                CPLRecodeFromWChar(pwszPathBuf, CPL_ENC_UCS2, CPL_ENC_UTF8);

            const size_t nStrlenDecoded = strlen(pszDecoded);
            strncpy(pszPathBuf, pszDecoded, nMaxLength);
            int bOK = TRUE;
            if (nStrlenDecoded >= static_cast<size_t>(nMaxLength) - 1)
            {
                pszPathBuf[nMaxLength - 1] = '\0';
                // There is no easy way to detect if the string has been
                // truncated other than testing the existence of the file.
                VSIStatBufL sStat;
                bOK = (VSIStatL(pszPathBuf, &sStat) == 0);
            }
            CPLFree(pszDecoded);
            CPLFree(pwszPathBuf);
            return bOK;
        }
    }
    else
    {
        if (GetModuleFileNameA(nullptr, pszPathBuf, nMaxLength) == 0)
            return FALSE;
        else
        {
            const size_t nStrlenDecoded = strlen(pszPathBuf);
            int bOK = TRUE;
            if (nStrlenDecoded >= static_cast<size_t>(nMaxLength) - 1)
            {
                pszPathBuf[nMaxLength - 1] = '\0';
                // There is no easy way to detect if the string has been
                // truncated other than testing the existence of the file.
                VSIStatBufL sStat;
                bOK = (VSIStatL(pszPathBuf, &sStat) == 0);
            }
            return bOK;
        }
    }
#elif defined(__linux)
    long nPID = getpid();
    CPLString osExeLink;

    osExeLink.Printf("/proc/%ld/exe", nPID);
    ssize_t nResultLen = readlink(osExeLink, pszPathBuf, nMaxLength);
    if (nResultLen == nMaxLength)
        pszPathBuf[nMaxLength - 1] = '\0';
    else if (nResultLen >= 0)
        pszPathBuf[nResultLen] = '\0';

    return nResultLen > 0 && nResultLen < nMaxLength;
#elif defined(__MACH__) && defined(__APPLE__)
    uint32_t size = static_cast<uint32_t>(nMaxLength);
    if (_NSGetExecutablePath(pszPathBuf, &size) == 0)
    {
        return TRUE;
    }
    return FALSE;
#elif defined(__FreeBSD__)
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    size_t size = static_cast<size_t>(nMaxLength);
    if (sysctl(mib, 4, pszPathBuf, &size, nullptr, 0) == 0)
    {
        return TRUE;
    }
    return FALSE;
#else
    return FALSE;
#endif
}

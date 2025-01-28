/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Portable filename/path parsing, and forming ala "Glob API".
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#define ALLOW_DEPRECATED_CPL_PATH_FUNCTIONS

#include "cpl_port.h"
#include "cpl_conv.h"

#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <algorithm>
#include <string>

#include "cpl_atomic_ops.h"
#include "cpl_config.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

// Should be size of larged possible filename.
constexpr int CPL_PATH_BUF_SIZE = 2048;
constexpr int CPL_PATH_BUF_COUNT = 10;

static const char *CPLStaticBufferTooSmall(char *pszStaticResult)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Destination buffer too small");
    if (pszStaticResult == nullptr)
        return "";
    strcpy(pszStaticResult, "");
    return pszStaticResult;
}

/************************************************************************/
/*                         CPLGetStaticResult()                         */
/************************************************************************/

static char *CPLGetStaticResult()

{
    int bMemoryError = FALSE;
    char *pachBufRingInfo =
        static_cast<char *>(CPLGetTLSEx(CTLS_PATHBUF, &bMemoryError));
    if (bMemoryError)
        return nullptr;
    if (pachBufRingInfo == nullptr)
    {
        pachBufRingInfo = static_cast<char *>(VSI_CALLOC_VERBOSE(
            1, sizeof(int) + CPL_PATH_BUF_SIZE * CPL_PATH_BUF_COUNT));
        if (pachBufRingInfo == nullptr)
            return nullptr;
        CPLSetTLS(CTLS_PATHBUF, pachBufRingInfo, TRUE);
    }

    /* -------------------------------------------------------------------- */
    /*      Work out which string in the "ring" we want to use this         */
    /*      time.                                                           */
    /* -------------------------------------------------------------------- */
    int *pnBufIndex = reinterpret_cast<int *>(pachBufRingInfo);
    const size_t nOffset =
        sizeof(int) + static_cast<size_t>(*pnBufIndex * CPL_PATH_BUF_SIZE);
    char *pachBuffer = pachBufRingInfo + nOffset;

    *pnBufIndex = (*pnBufIndex + 1) % CPL_PATH_BUF_COUNT;

    return pachBuffer;
}

/************************************************************************/
/*                        CPLPathReturnTLSString()                      */
/************************************************************************/

static const char *CPLPathReturnTLSString(const std::string &osRes,
                                          const char *pszFuncName)
{
    if (osRes.size() >= CPL_PATH_BUF_SIZE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too long result for %s()",
                 pszFuncName);
        return "";
    }

    char *pszStaticResult = CPLGetStaticResult();
    if (pszStaticResult == nullptr)
        return CPLStaticBufferTooSmall(pszStaticResult);
    memcpy(pszStaticResult, osRes.c_str(), osRes.size() + 1);
    return pszStaticResult;
}

/************************************************************************/
/*                        CPLFindFilenameStart()                        */
/************************************************************************/

static int CPLFindFilenameStart(const char *pszFilename, size_t nStart = 0)

{
    size_t iFileStart = nStart ? nStart : strlen(pszFilename);

    for (; iFileStart > 0 && pszFilename[iFileStart - 1] != '/' &&
           pszFilename[iFileStart - 1] != '\\';
         iFileStart--)
    {
    }

    return static_cast<int>(iFileStart);
}

/************************************************************************/
/*                          CPLGetPathSafe()                            */
/************************************************************************/

/**
 * Extract directory path portion of filename.
 *
 * Returns a string containing the directory path portion of the passed
 * filename.  If there is no path in the passed filename an empty string
 * will be returned (not NULL).
 *
 * \code{.cpp}
 * CPLGetPathSafe( "abc/def.xyz" ) == "abc"
 * CPLGetPathSafe( "/abc/def/" ) == "/abc/def"
 * CPLGetPathSafe( "/" ) == "/"
 * CPLGetPathSafe( "/abc/def" ) == "/abc"
 * CPLGetPathSafe( "abc" ) == ""
 * \endcode
 *
 * @param pszFilename the filename potentially including a path.
 *
 * @return Path.
 *
 * @since 3.11
 */

std::string CPLGetPathSafe(const char *pszFilename)

{
    size_t nSuffixPos = 0;
    if (STARTS_WITH(pszFilename, "/vsicurl/http"))
    {
        const char *pszQuestionMark = strchr(pszFilename, '?');
        if (pszQuestionMark)
            nSuffixPos = static_cast<size_t>(pszQuestionMark - pszFilename);
    }
    else if (STARTS_WITH(pszFilename, "/vsicurl?") &&
             strstr(pszFilename, "url="))
    {
        std::string osRet;
        const CPLStringList aosTokens(
            CSLTokenizeString2(pszFilename + strlen("/vsicurl?"), "&", 0));
        for (int i = 0; i < aosTokens.size(); i++)
        {
            if (osRet.empty())
                osRet = "/vsicurl?";
            else
                osRet += '&';
            if (STARTS_WITH(aosTokens[i], "url=") &&
                !STARTS_WITH(aosTokens[i], "url=/vsicurl"))
            {
                char *pszUnescaped =
                    CPLUnescapeString(aosTokens[i], nullptr, CPLES_URL);
                char *pszPath = CPLEscapeString(
                    CPLGetPathSafe(pszUnescaped + strlen("url=")).c_str(), -1,
                    CPLES_URL);
                osRet += "url=";
                osRet += pszPath;
                CPLFree(pszPath);
                CPLFree(pszUnescaped);
            }
            else
            {
                osRet += aosTokens[i];
            }
        }
        return osRet;
    }

    const int iFileStart = CPLFindFilenameStart(pszFilename, nSuffixPos);
    if (iFileStart == 0)
    {
        return std::string();
    }

    std::string osRet(pszFilename, iFileStart);

    if (iFileStart > 1 && (osRet.back() == '/' || osRet.back() == '\\'))
        osRet.pop_back();

    if (nSuffixPos)
    {
        osRet += (pszFilename + nSuffixPos);
    }

    return osRet;
}

/************************************************************************/
/*                             CPLGetPath()                             */
/************************************************************************/

/**
 * Extract directory path portion of filename.
 *
 * Returns a string containing the directory path portion of the passed
 * filename.  If there is no path in the passed filename an empty string
 * will be returned (not NULL).
 *
 * \code{.cpp}
 * CPLGetPath( "abc/def.xyz" ) == "abc"
 * CPLGetPath( "/abc/def/" ) == "/abc/def"
 * CPLGetPath( "/" ) == "/"
 * CPLGetPath( "/abc/def" ) == "/abc"
 * CPLGetPath( "abc" ) == ""
 * \endcode
 *
 * @param pszFilename the filename potentially including a path.
 *
 * @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.  The returned
 * will generally not contain a trailing path separator.
 *
 * @deprecated If using C++, prefer using CPLGetPathSafe() instead
 */

const char *CPLGetPath(const char *pszFilename)

{
    return CPLPathReturnTLSString(CPLGetPathSafe(pszFilename), __FUNCTION__);
}

/************************************************************************/
/*                             CPLGetDirname()                          */
/************************************************************************/

/**
 * Extract directory path portion of filename.
 *
 * Returns a string containing the directory path portion of the passed
 * filename.  If there is no path in the passed filename the dot will be
 * returned.  It is the only difference from CPLGetPath().
 *
 * \code{.cpp}
 * CPLGetDirnameSafe( "abc/def.xyz" ) == "abc"
 * CPLGetDirnameSafe( "/abc/def/" ) == "/abc/def"
 * CPLGetDirnameSafe( "/" ) == "/"
 * CPLGetDirnameSafe( "/abc/def" ) == "/abc"
 * CPLGetDirnameSafe( "abc" ) == "."
 * \endcode
 *
 * @param pszFilename the filename potentially including a path.
 *
 * @return Path
 *
 * @since 3.11
 */

std::string CPLGetDirnameSafe(const char *pszFilename)

{
    size_t nSuffixPos = 0;
    if (STARTS_WITH(pszFilename, "/vsicurl/http"))
    {
        const char *pszQuestionMark = strchr(pszFilename, '?');
        if (pszQuestionMark)
            nSuffixPos = static_cast<size_t>(pszQuestionMark - pszFilename);
    }
    else if (STARTS_WITH(pszFilename, "/vsicurl?") &&
             strstr(pszFilename, "url="))
    {
        std::string osRet;
        const CPLStringList aosTokens(
            CSLTokenizeString2(pszFilename + strlen("/vsicurl?"), "&", 0));
        for (int i = 0; i < aosTokens.size(); i++)
        {
            if (osRet.empty())
                osRet = "/vsicurl?";
            else
                osRet += '&';
            if (STARTS_WITH(aosTokens[i], "url=") &&
                !STARTS_WITH(aosTokens[i], "url=/vsicurl"))
            {
                char *pszUnescaped =
                    CPLUnescapeString(aosTokens[i], nullptr, CPLES_URL);
                char *pszPath = CPLEscapeString(
                    CPLGetDirname(pszUnescaped + strlen("url=")), -1,
                    CPLES_URL);
                osRet += "url=";
                osRet += pszPath;
                CPLFree(pszPath);
                CPLFree(pszUnescaped);
            }
            else
            {
                osRet += aosTokens[i];
            }
        }
        return osRet;
    }

    const int iFileStart = CPLFindFilenameStart(pszFilename, nSuffixPos);
    if (iFileStart == 0)
    {
        return std::string(".");
    }

    std::string osRet(pszFilename, iFileStart);

    if (iFileStart > 1 && (osRet.back() == '/' || osRet.back() == '\\'))
        osRet.pop_back();

    if (nSuffixPos)
    {
        osRet += (pszFilename + nSuffixPos);
    }

    return osRet;
}

/************************************************************************/
/*                             CPLGetDirname()                          */
/************************************************************************/

/**
 * Extract directory path portion of filename.
 *
 * Returns a string containing the directory path portion of the passed
 * filename.  If there is no path in the passed filename the dot will be
 * returned.  It is the only difference from CPLGetPath().
 *
 * \code{.cpp}
 * CPLGetDirname( "abc/def.xyz" ) == "abc"
 * CPLGetDirname( "/abc/def/" ) == "/abc/def"
 * CPLGetDirname( "/" ) == "/"
 * CPLGetDirname( "/abc/def" ) == "/abc"
 * CPLGetDirname( "abc" ) == "."
 * \endcode
 *
 * @param pszFilename the filename potentially including a path.
 *
 * @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.  The returned
 * will generally not contain a trailing path separator.
 */

const char *CPLGetDirname(const char *pszFilename)

{
    return CPLPathReturnTLSString(CPLGetDirnameSafe(pszFilename), __FUNCTION__);
}

/************************************************************************/
/*                           CPLGetFilename()                           */
/************************************************************************/

/**
 * Extract non-directory portion of filename.
 *
 * Returns a string containing the bare filename portion of the passed
 * filename.  If there is no filename (passed value ends in trailing directory
 * separator) an empty string is returned.
 *
 * \code{.cpp}
 * CPLGetFilename( "abc/def.xyz" ) == "def.xyz"
 * CPLGetFilename( "/abc/def/" ) == ""
 * CPLGetFilename( "abc/def" ) == "def"
 * \endcode
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the non-directory portion of the path (points back into
 * original string).
 */

const char *CPLGetFilename(const char *pszFullFilename)

{
    const int iFileStart = CPLFindFilenameStart(pszFullFilename);

    return pszFullFilename + iFileStart;
}

/************************************************************************/
/*                       CPLGetBasenameSafe()                           */
/************************************************************************/

/**
 * Extract basename (non-directory, non-extension) portion of filename.
 *
 * Returns a string containing the file basename portion of the passed
 * name.  If there is no basename (passed value ends in trailing directory
 * separator, or filename starts with a dot) an empty string is returned.
 *
 * \code{.cpp}
 * CPLGetBasename( "abc/def.xyz" ) == "def"
 * CPLGetBasename( "abc/def" ) == "def"
 * CPLGetBasename( "abc/def/" ) == ""
 * \endcode
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the non-directory, non-extension portion of the path
 *
 * @since 3.11
 */

std::string CPLGetBasenameSafe(const char *pszFullFilename)

{
    const size_t iFileStart =
        static_cast<size_t>(CPLFindFilenameStart(pszFullFilename));

    size_t iExtStart = strlen(pszFullFilename);
    for (; iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart--)
    {
    }

    if (iExtStart == iFileStart)
        iExtStart = strlen(pszFullFilename);

    const size_t nLength = iExtStart - iFileStart;
    return std::string(pszFullFilename + iFileStart, nLength);
}

/************************************************************************/
/*                           CPLGetBasename()                           */
/************************************************************************/

/**
 * Extract basename (non-directory, non-extension) portion of filename.
 *
 * Returns a string containing the file basename portion of the passed
 * name.  If there is no basename (passed value ends in trailing directory
 * separator, or filename starts with a dot) an empty string is returned.
 *
 * \code{.cpp}
 * CPLGetBasename( "abc/def.xyz" ) == "def"
 * CPLGetBasename( "abc/def" ) == "def"
 * CPLGetBasename( "abc/def/" ) == ""
 * \endcode
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the non-directory, non-extension portion of the path in
 * an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 *
 * @deprecated If using C++, prefer using CPLGetBasenameSafe() instead
 */

const char *CPLGetBasename(const char *pszFullFilename)

{
    return CPLPathReturnTLSString(CPLGetBasenameSafe(pszFullFilename),
                                  __FUNCTION__);
}

/************************************************************************/
/*                        CPLGetExtensionSafe()                         */
/************************************************************************/

/**
 * Extract filename extension from full filename.
 *
 * Returns a string containing the extension portion of the passed
 * name.  If there is no extension (the filename has no dot) an empty string
 * is returned.  The returned extension will not include the period.
 *
 * \code{.cpp}
 * CPLGetExtensionSafe( "abc/def.xyz" ) == "xyz"
 * CPLGetExtensionSafe( "abc/def" ) == ""
 * \endcode
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the extension portion of the path.
 *
 * @since 3.11
 */

std::string CPLGetExtensionSafe(const char *pszFullFilename)

{
    if (pszFullFilename[0] == '\0')
        return std::string();

    size_t iFileStart =
        static_cast<size_t>(CPLFindFilenameStart(pszFullFilename));
    size_t iExtStart = strlen(pszFullFilename);
    for (; iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart--)
    {
    }

    if (iExtStart == iFileStart)
        iExtStart = strlen(pszFullFilename) - 1;

    // If the extension is too long, it is very much likely not an extension,
    // but another component of the path
    const size_t knMaxExtensionSize = 10;
    if (strlen(pszFullFilename + iExtStart + 1) > knMaxExtensionSize)
        return "";

    return std::string(pszFullFilename + iExtStart + 1);
}

/************************************************************************/
/*                           CPLGetExtension()                          */
/************************************************************************/

/**
 * Extract filename extension from full filename.
 *
 * Returns a string containing the extension portion of the passed
 * name.  If there is no extension (the filename has no dot) an empty string
 * is returned.  The returned extension will not include the period.
 *
 * \code{.cpp}
 * CPLGetExtension( "abc/def.xyz" ) == "xyz"
 * CPLGetExtension( "abc/def" ) == ""
 * \endcode
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the extension portion of the path in
 * an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 *
 * @deprecated If using C++, prefer using CPLGetExtensionSafe() instead
 */

const char *CPLGetExtension(const char *pszFullFilename)

{
    return CPLPathReturnTLSString(CPLGetExtensionSafe(pszFullFilename),
                                  __FUNCTION__);
}

/************************************************************************/
/*                         CPLGetCurrentDir()                           */
/************************************************************************/

/**
 * Get the current working directory name.
 *
 * @return a pointer to buffer, containing current working directory path
 * or NULL in case of error.  User is responsible to free that buffer
 * after usage with CPLFree() function.
 * If HAVE_GETCWD macro is not defined, the function returns NULL.
 **/

#ifdef _WIN32
char *CPLGetCurrentDir()
{
    const size_t nPathMax = _MAX_PATH;
    wchar_t *pwszDirPath =
        static_cast<wchar_t *>(VSI_MALLOC_VERBOSE(nPathMax * sizeof(wchar_t)));
    char *pszRet = nullptr;
    if (pwszDirPath != nullptr && _wgetcwd(pwszDirPath, nPathMax) != nullptr)
    {
        pszRet = CPLRecodeFromWChar(pwszDirPath, CPL_ENC_UCS2, CPL_ENC_UTF8);
    }
    CPLFree(pwszDirPath);
    return pszRet;
}
#elif defined(HAVE_GETCWD)
char *CPLGetCurrentDir()
{
#if PATH_MAX
    const size_t nPathMax = PATH_MAX;
#else
    const size_t nPathMax = 8192;
#endif

    char *pszDirPath = static_cast<char *>(VSI_MALLOC_VERBOSE(nPathMax));
    if (!pszDirPath)
        return nullptr;

    return getcwd(pszDirPath, nPathMax);
}
#else   // !HAVE_GETCWD
char *CPLGetCurrentDir()
{
    return nullptr;
}
#endif  // HAVE_GETCWD

/************************************************************************/
/*                         CPLResetExtension()                          */
/************************************************************************/

/**
 * Replace the extension with the provided one.
 *
 * @param pszPath the input path, this string is not altered.
 * @param pszExt the new extension to apply to the given path.
 *
 * @return an altered filename with the new extension.
 *
 * @since 3.11
 */

std::string CPLResetExtensionSafe(const char *pszPath, const char *pszExt)

{
    std::string osRet(pszPath);

    /* -------------------------------------------------------------------- */
    /*      First, try and strip off any existing extension.                */
    /* -------------------------------------------------------------------- */

    for (size_t i = osRet.size(); i > 0;)
    {
        --i;
        if (osRet[i] == '.')
        {
            osRet.resize(i);
            break;
        }
        else if (osRet[i] == '/' || osRet[i] == '\\' || osRet[i] == ':')
        {
            break;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Append the new extension.                                       */
    /* -------------------------------------------------------------------- */
    osRet += '.';
    osRet += pszExt;

    return osRet;
}

/************************************************************************/
/*                         CPLResetExtension()                          */
/************************************************************************/

/**
 * Replace the extension with the provided one.
 *
 * @param pszPath the input path, this string is not altered.
 * @param pszExt the new extension to apply to the given path.
 *
 * @return an altered filename with the new extension.    Do not
 * modify or free the returned string.  The string may be destroyed by the
 * next CPL call.
 *
 * @deprecated If using C++, prefer using CPLResetExtensionSafe() instead
 */

const char *CPLResetExtension(const char *pszPath, const char *pszExt)

{
    return CPLPathReturnTLSString(CPLResetExtensionSafe(pszPath, pszExt),
                                  __FUNCTION__);
}

/************************************************************************/
/*                        CPLFormFilenameSafe()                         */
/************************************************************************/

/**
 * Build a full file path from a passed path, file basename and extension.
 *
 * The path, and extension are optional.  The basename may in fact contain
 * an extension if desired.
 *
 * \code{.cpp}
 * CPLFormFilenameSafe("abc/xyz", "def", ".dat" ) == "abc/xyz/def.dat"
 * CPLFormFilenameSafe(NULL,"def", NULL ) == "def"
 * CPLFormFilenameSafe(NULL, "abc/def.dat", NULL ) == "abc/def.dat"
 * CPLFormFilenameSafe("/abc/xyz/", "def.dat", NULL ) == "/abc/xyz/def.dat"
 * CPLFormFilenameSafe("/a/b/c", "../d", NULL ) == "/a/b/d" (since 3.10.1)
 * \endcode
 *
 * @param pszPath directory path to the directory containing the file.  This
 * may be relative or absolute, and may have a trailing path separator or
 * not.  May be NULL.
 *
 * @param pszBasename file basename.  May optionally have path and/or
 * extension.  Must *NOT* be NULL.
 *
 * @param pszExtension file extension, optionally including the period.  May
 * be NULL.
 *
 * @return a fully formed filename.
 *
 * @since 3.11
 */

std::string CPLFormFilenameSafe(const char *pszPath, const char *pszBasename,
                                const char *pszExtension)

{
    if (pszBasename[0] == '.' &&
        (pszBasename[1] == '/' || pszBasename[1] == '\\'))
        pszBasename += 2;

    const char *pszAddedPathSep = "";
    const char *pszAddedExtSep = "";

    if (pszPath == nullptr)
        pszPath = "";
    size_t nLenPath = strlen(pszPath);

    const char *pszQuestionMark = nullptr;
    if (STARTS_WITH_CI(pszPath, "/vsicurl/http"))
    {
        pszQuestionMark = strchr(pszPath, '?');
        if (pszQuestionMark)
        {
            nLenPath = pszQuestionMark - pszPath;
        }
        pszAddedPathSep = "/";
    }

    if (!CPLIsFilenameRelative(pszPath) && pszBasename[0] == '.' &&
        pszBasename[1] == '.' &&
        (pszBasename[2] == 0 || pszBasename[2] == '\\' ||
         pszBasename[2] == '/'))
    {
        // "/a/b/" + "..[/something]" --> "/a[/something]"
        // "/a/b" + "..[/something]" --> "/a[/something]"
        if (pszPath[nLenPath - 1] == '\\' || pszPath[nLenPath - 1] == '/')
            nLenPath--;
        while (true)
        {
            const char *pszBasenameOri = pszBasename;
            const size_t nLenPathOri = nLenPath;
            while (nLenPath > 0 && pszPath[nLenPath - 1] != '\\' &&
                   pszPath[nLenPath - 1] != '/')
            {
                nLenPath--;
            }
            if (nLenPath == 1 && pszPath[0] == '/')
            {
                pszBasename += 2;
                if (pszBasename[0] == '/' || pszBasename[0] == '\\')
                    pszBasename++;
                if (*pszBasename == '.')
                {
                    pszBasename = pszBasenameOri;
                    nLenPath = nLenPathOri;
                    if (pszAddedPathSep[0] == 0)
                        pszAddedPathSep =
                            pszPath[0] == '/'
                                ? "/"
                                : VSIGetDirectorySeparator(pszPath);
                }
                break;
            }
            else if ((nLenPath > 1 && pszPath[0] == '/') ||
                     (nLenPath > 2 && pszPath[1] == ':') ||
                     (nLenPath > 6 && strncmp(pszPath, "\\\\$\\", 4) == 0))
            {
                nLenPath--;
                pszBasename += 2;
                if ((pszBasename[0] == '/' || pszBasename[0] == '\\') &&
                    pszBasename[1] == '.' && pszBasename[2] == '.')
                {
                    pszBasename++;
                }
                else
                {
                    break;
                }
            }
            else
            {
                // cppcheck-suppress redundantAssignment
                pszBasename = pszBasenameOri;
                nLenPath = nLenPathOri;
                if (pszAddedPathSep[0] == 0)
                    pszAddedPathSep = pszPath[0] == '/'
                                          ? "/"
                                          : VSIGetDirectorySeparator(pszPath);
                break;
            }
        }
    }
    else if (nLenPath > 0 && pszPath[nLenPath - 1] != '/' &&
             pszPath[nLenPath - 1] != '\\')
    {
        if (pszAddedPathSep[0] == 0)
            pszAddedPathSep = VSIGetDirectorySeparator(pszPath);
    }

    if (pszExtension == nullptr)
        pszExtension = "";
    else if (pszExtension[0] != '.' && strlen(pszExtension) > 0)
        pszAddedExtSep = ".";

    std::string osRes;
    osRes.reserve(nLenPath + strlen(pszAddedPathSep) + strlen(pszBasename) +
                  strlen(pszAddedExtSep) + strlen(pszExtension) +
                  (pszQuestionMark ? strlen(pszQuestionMark) : 0));
    osRes.assign(pszPath, nLenPath);
    osRes += pszAddedPathSep;
    osRes += pszBasename;
    osRes += pszAddedExtSep;
    osRes += pszExtension;

    if (pszQuestionMark)
    {
        osRes += pszQuestionMark;
    }

    return osRes;
}

/************************************************************************/
/*                          CPLFormFilename()                           */
/************************************************************************/

/**
 * Build a full file path from a passed path, file basename and extension.
 *
 * The path, and extension are optional.  The basename may in fact contain
 * an extension if desired.
 *
 * \code{.cpp}
 * CPLFormFilename("abc/xyz", "def", ".dat" ) == "abc/xyz/def.dat"
 * CPLFormFilename(NULL,"def", NULL ) == "def"
 * CPLFormFilename(NULL, "abc/def.dat", NULL ) == "abc/def.dat"
 * CPLFormFilename("/abc/xyz/", "def.dat", NULL ) == "/abc/xyz/def.dat"
 * CPLFormFilename("/a/b/c", "../d", NULL ) == "/a/b/d" (since 3.10.1)
 * \endcode
 *
 * @param pszPath directory path to the directory containing the file.  This
 * may be relative or absolute, and may have a trailing path separator or
 * not.  May be NULL.
 *
 * @param pszBasename file basename.  May optionally have path and/or
 * extension.  Must *NOT* be NULL.
 *
 * @param pszExtension file extension, optionally including the period.  May
 * be NULL.
 *
 * @return a fully formed filename in an internal static string.  Do not
 * modify or free the returned string.  The string may be destroyed by the
 * next CPL call.
 *
 * @deprecated If using C++, prefer using CPLFormFilenameSafe() instead
 */
const char *CPLFormFilename(const char *pszPath, const char *pszBasename,
                            const char *pszExtension)

{
    return CPLPathReturnTLSString(
        CPLFormFilenameSafe(pszPath, pszBasename, pszExtension), __FUNCTION__);
}

/************************************************************************/
/*                       CPLFormCIFilenameSafe()                        */
/************************************************************************/

/**
 * Case insensitive file searching, returning full path.
 *
 * This function tries to return the path to a file regardless of
 * whether the file exactly matches the basename, and extension case, or
 * is all upper case, or all lower case.  The path is treated as case
 * sensitive.  This function is equivalent to CPLFormFilename() on
 * case insensitive file systems (like Windows).
 *
 * @param pszPath directory path to the directory containing the file.  This
 * may be relative or absolute, and may have a trailing path separator or
 * not.  May be NULL.
 *
 * @param pszBasename file basename.  May optionally have path and/or
 * extension.  May not be NULL.
 *
 * @param pszExtension file extension, optionally including the period.  May
 * be NULL.
 *
 * @return a fully formed filename.
 *
 * @since 3.11
 */

std::string CPLFormCIFilenameSafe(const char *pszPath, const char *pszBasename,
                                  const char *pszExtension)

{
    // On case insensitive filesystems, just default to CPLFormFilename().
    if (!VSIIsCaseSensitiveFS(pszPath))
        return CPLFormFilenameSafe(pszPath, pszBasename, pszExtension);

    const char *pszAddedExtSep = "";
    size_t nLen = strlen(pszBasename) + 2;

    if (pszExtension != nullptr)
        nLen += strlen(pszExtension);

    char *pszFilename = static_cast<char *>(VSI_MALLOC_VERBOSE(nLen));
    if (pszFilename == nullptr)
        return "";

    if (pszExtension == nullptr)
        pszExtension = "";
    else if (pszExtension[0] != '.' && strlen(pszExtension) > 0)
        pszAddedExtSep = ".";

    snprintf(pszFilename, nLen, "%s%s%s", pszBasename, pszAddedExtSep,
             pszExtension);

    std::string osRet = CPLFormFilenameSafe(pszPath, pszFilename, nullptr);
    VSIStatBufL sStatBuf;
    int nStatRet = VSIStatExL(osRet.c_str(), &sStatBuf, VSI_STAT_EXISTS_FLAG);

    if (nStatRet != 0)
    {
        for (size_t i = 0; pszFilename[i] != '\0'; i++)
        {
            pszFilename[i] = static_cast<char>(CPLToupper(pszFilename[i]));
        }

        std::string osTmpPath(
            CPLFormFilenameSafe(pszPath, pszFilename, nullptr));
        nStatRet =
            VSIStatExL(osTmpPath.c_str(), &sStatBuf, VSI_STAT_EXISTS_FLAG);
        if (nStatRet == 0)
            osRet = std::move(osTmpPath);
    }

    if (nStatRet != 0)
    {
        for (size_t i = 0; pszFilename[i] != '\0'; i++)
        {
            pszFilename[i] = static_cast<char>(
                CPLTolower(static_cast<unsigned char>(pszFilename[i])));
        }

        std::string osTmpPath(
            CPLFormFilenameSafe(pszPath, pszFilename, nullptr));
        nStatRet =
            VSIStatExL(osTmpPath.c_str(), &sStatBuf, VSI_STAT_EXISTS_FLAG);
        if (nStatRet == 0)
            osRet = std::move(osTmpPath);
    }

    if (nStatRet != 0)
        osRet = CPLFormFilenameSafe(pszPath, pszBasename, pszExtension);

    CPLFree(pszFilename);

    return osRet;
}

/************************************************************************/
/*                          CPLFormCIFilename()                         */
/************************************************************************/

/**
 * Case insensitive file searching, returning full path.
 *
 * This function tries to return the path to a file regardless of
 * whether the file exactly matches the basename, and extension case, or
 * is all upper case, or all lower case.  The path is treated as case
 * sensitive.  This function is equivalent to CPLFormFilename() on
 * case insensitive file systems (like Windows).
 *
 * @param pszPath directory path to the directory containing the file.  This
 * may be relative or absolute, and may have a trailing path separator or
 * not.  May be NULL.
 *
 * @param pszBasename file basename.  May optionally have path and/or
 * extension.  May not be NULL.
 *
 * @param pszExtension file extension, optionally including the period.  May
 * be NULL.
 *
 * @return a fully formed filename in an internal static string.  Do not
 * modify or free the returned string.  The string may be destroyed by the
 * next CPL call.
 *
 * @deprecated If using C++, prefer using CPLFormCIFilenameSafe() instead
*/

const char *CPLFormCIFilename(const char *pszPath, const char *pszBasename,
                              const char *pszExtension)

{
    return CPLPathReturnTLSString(
        CPLFormCIFilenameSafe(pszPath, pszBasename, pszExtension),
        __FUNCTION__);
}

/************************************************************************/
/*                   CPLProjectRelativeFilenameSafe()                   */
/************************************************************************/

/**
 * Find a file relative to a project file.
 *
 * Given the path to a "project" directory, and a path to a secondary file
 * referenced from that project, build a path to the secondary file
 * that the current application can use.  If the secondary path is already
 * absolute, rather than relative, then it will be returned unaltered.
 *
 * Examples:
 * \code{.cpp}
 * CPLProjectRelativeFilenameSafe("abc/def", "tmp/abc.gif") == "abc/def/tmp/abc.gif"
 * CPLProjectRelativeFilenameSafe("abc/def", "/tmp/abc.gif") == "/tmp/abc.gif"
 * CPLProjectRelativeFilenameSafe("/xy", "abc.gif") == "/xy/abc.gif"
 * CPLProjectRelativeFilenameSafe("/abc/def", "../abc.gif") == "/abc/def/../abc.gif"
 * CPLProjectRelativeFilenameSafe("C:\WIN", "abc.gif") == "C:\WIN\abc.gif"
 * \endcode
 *
 * @param pszProjectDir the directory relative to which the secondary files
 * path should be interpreted.
 * @param pszSecondaryFilename the filename (potentially with path) that
 * is to be interpreted relative to the project directory.
 *
 * @return a composed path to the secondary file.
 *
 * @since 3.11
 */

std::string CPLProjectRelativeFilenameSafe(const char *pszProjectDir,
                                           const char *pszSecondaryFilename)

{
    if (pszProjectDir == nullptr || pszProjectDir[0] == 0 ||
        !CPLIsFilenameRelative(pszSecondaryFilename))
    {
        return pszSecondaryFilename;
    }

    std::string osRes(pszProjectDir);
    if (osRes.back() != '/' && osRes.back() != '\\')
    {
        osRes += VSIGetDirectorySeparator(pszProjectDir);
    }

    osRes += pszSecondaryFilename;
    return osRes;
}

/************************************************************************/
/*                     CPLProjectRelativeFilename()                     */
/************************************************************************/

/**
 * Find a file relative to a project file.
 *
 * Given the path to a "project" directory, and a path to a secondary file
 * referenced from that project, build a path to the secondary file
 * that the current application can use.  If the secondary path is already
 * absolute, rather than relative, then it will be returned unaltered.
 *
 * Examples:
 * \code{.cpp}
 * CPLProjectRelativeFilename("abc/def", "tmp/abc.gif") == "abc/def/tmp/abc.gif"
 * CPLProjectRelativeFilename("abc/def", "/tmp/abc.gif") == "/tmp/abc.gif"
 * CPLProjectRelativeFilename("/xy", "abc.gif") == "/xy/abc.gif"
 * CPLProjectRelativeFilename("/abc/def", "../abc.gif") == "/abc/def/../abc.gif"
 * CPLProjectRelativeFilename("C:\WIN", "abc.gif") == "C:\WIN\abc.gif"
 * \endcode
 *
 * @param pszProjectDir the directory relative to which the secondary files
 * path should be interpreted.
 * @param pszSecondaryFilename the filename (potentially with path) that
 * is to be interpreted relative to the project directory.
 *
 * @return a composed path to the secondary file.  The returned string is
 * internal and should not be altered, freed, or depending on past the next
 * CPL call.
 *
 * @deprecated If using C++, prefer using CPLProjectRelativeFilenameSafe() instead
 */

const char *CPLProjectRelativeFilename(const char *pszProjectDir,
                                       const char *pszSecondaryFilename)

{
    return CPLPathReturnTLSString(
        CPLProjectRelativeFilenameSafe(pszProjectDir, pszSecondaryFilename),
        __FUNCTION__);
}

/************************************************************************/
/*                       CPLIsFilenameRelative()                        */
/************************************************************************/

/**
 * Is filename relative or absolute?
 *
 * The test is filesystem convention agnostic.  That is it will test for
 * Unix style and windows style path conventions regardless of the actual
 * system in use.
 *
 * @param pszFilename the filename with path to test.
 *
 * @return TRUE if the filename is relative or FALSE if it is absolute.
 */

int CPLIsFilenameRelative(const char *pszFilename)

{
    if ((pszFilename[0] != '\0' &&
         (STARTS_WITH(pszFilename + 1, ":\\") ||
          STARTS_WITH(pszFilename + 1, ":/") ||
          strstr(pszFilename + 1, "://")  // http://, ftp:// etc....
          )) ||
        STARTS_WITH(pszFilename, "\\\\?\\")  // Windows extended Length Path.
        || pszFilename[0] == '\\' || pszFilename[0] == '/')
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                       CPLExtractRelativePath()                       */
/************************************************************************/

/**
 * Get relative path from directory to target file.
 *
 * Computes a relative path for pszTarget relative to pszBaseDir.
 * Currently this only works if they share a common base path.  The returned
 * path is normally into the pszTarget string.  It should only be considered
 * valid as long as pszTarget is valid or till the next call to
 * this function, whichever comes first.
 *
 * @param pszBaseDir the name of the directory relative to which the path
 * should be computed.  pszBaseDir may be NULL in which case the original
 * target is returned without relativizing.
 *
 * @param pszTarget the filename to be changed to be relative to pszBaseDir.
 *
 * @param pbGotRelative Pointer to location in which a flag is placed
 * indicating that the returned path is relative to the basename (TRUE) or
 * not (FALSE).  This pointer may be NULL if flag is not desired.
 *
 * @return an adjusted path or the original if it could not be made relative
 * to the pszBaseFile's path.
 **/

const char *CPLExtractRelativePath(const char *pszBaseDir,
                                   const char *pszTarget, int *pbGotRelative)

{
    /* -------------------------------------------------------------------- */
    /*      If we don't have a basedir, then we can't relativize the path.  */
    /* -------------------------------------------------------------------- */
    if (pszBaseDir == nullptr)
    {
        if (pbGotRelative != nullptr)
            *pbGotRelative = FALSE;

        return pszTarget;
    }

    const size_t nBasePathLen = strlen(pszBaseDir);

    /* -------------------------------------------------------------------- */
    /*      One simple case is when the base dir is '.' and the target      */
    /*      filename is relative.                                           */
    /* -------------------------------------------------------------------- */
    if ((nBasePathLen == 0 || EQUAL(pszBaseDir, ".")) &&
        CPLIsFilenameRelative(pszTarget))
    {
        if (pbGotRelative != nullptr)
            *pbGotRelative = TRUE;

        return pszTarget;
    }

    /* -------------------------------------------------------------------- */
    /*      By this point, if we don't have a base path, we can't have a    */
    /*      meaningful common prefix.                                       */
    /* -------------------------------------------------------------------- */
    if (nBasePathLen == 0)
    {
        if (pbGotRelative != nullptr)
            *pbGotRelative = FALSE;

        return pszTarget;
    }

    /* -------------------------------------------------------------------- */
    /*      If we don't have a common path prefix, then we can't get a      */
    /*      relative path.                                                  */
    /* -------------------------------------------------------------------- */
    if (!EQUALN(pszBaseDir, pszTarget, nBasePathLen) ||
        (pszTarget[nBasePathLen] != '\\' && pszTarget[nBasePathLen] != '/'))
    {
        if (pbGotRelative != nullptr)
            *pbGotRelative = FALSE;

        return pszTarget;
    }

    /* -------------------------------------------------------------------- */
    /*      We have a relative path.  Strip it off to get a string to       */
    /*      return.                                                         */
    /* -------------------------------------------------------------------- */
    if (pbGotRelative != nullptr)
        *pbGotRelative = TRUE;

    return pszTarget + nBasePathLen + 1;
}

/************************************************************************/
/*                      CPLCleanTrailingSlashSafe()                     */
/************************************************************************/

/**
 * Remove trailing forward/backward slash from the path for UNIX/Windows resp.
 *
 * Returns a string containing the portion of the passed path string with
 * trailing slash removed. If there is no path in the passed filename
 * an empty string will be returned (not NULL).
 *
 * \code{.cpp}
 * CPLCleanTrailingSlashSafe( "abc/def/" ) == "abc/def"
 * CPLCleanTrailingSlashSafe( "abc/def" ) == "abc/def"
 * CPLCleanTrailingSlashSafe( "c:\\abc\\def\\" ) == "c:\\abc\\def"
 * CPLCleanTrailingSlashSafe( "c:\\abc\\def" ) == "c:\\abc\\def"
 * CPLCleanTrailingSlashSafe( "abc" ) == "abc"
 * \endcode
 *
 * @param pszPath the path to be cleaned up
 *
 * @return Path
 *
 * @since 3.11
 */

std::string CPLCleanTrailingSlashSafe(const char *pszPath)

{
    std::string osRes(pszPath);
    if (!osRes.empty() && (osRes.back() == '\\' || osRes.back() == '/'))
        osRes.pop_back();
    return osRes;
}

/************************************************************************/
/*                            CPLCleanTrailingSlash()                   */
/************************************************************************/

/**
 * Remove trailing forward/backward slash from the path for UNIX/Windows resp.
 *
 * Returns a string containing the portion of the passed path string with
 * trailing slash removed. If there is no path in the passed filename
 * an empty string will be returned (not NULL).
 *
 * \code{.cpp}
 * CPLCleanTrailingSlash( "abc/def/" ) == "abc/def"
 * CPLCleanTrailingSlash( "abc/def" ) == "abc/def"
 * CPLCleanTrailingSlash( "c:\\abc\\def\\" ) == "c:\\abc\\def"
 * CPLCleanTrailingSlash( "c:\\abc\\def" ) == "c:\\abc\\def"
 * CPLCleanTrailingSlash( "abc" ) == "abc"
 * \endcode
 *
 * @param pszPath the path to be cleaned up
 *
 * @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 *
 * @deprecated If using C++, prefer using CPLCleanTrailingSlashSafe() instead
 */

const char *CPLCleanTrailingSlash(const char *pszPath)

{
    return CPLPathReturnTLSString(CPLCleanTrailingSlashSafe(pszPath),
                                  __FUNCTION__);
}

/************************************************************************/
/*                       CPLCorrespondingPaths()                        */
/************************************************************************/

/**
 * Identify corresponding paths.
 *
 * Given a prototype old and new filename this function will attempt
 * to determine corresponding names for a set of other old filenames that
 * will rename them in a similar manner.  This correspondence assumes there
 * are two possibly kinds of renaming going on.  A change of path, and a
 * change of filename stem.
 *
 * If a consistent renaming cannot be established for all the files this
 * function will return indicating an error.
 *
 * The returned file list becomes owned by the caller and should be destroyed
 * with CSLDestroy().
 *
 * @param pszOldFilename path to old prototype file.
 * @param pszNewFilename path to new prototype file.
 * @param papszFileList list of other files associated with pszOldFilename to
 * rename similarly.
 *
 * @return a list of files corresponding to papszFileList but renamed to
 * correspond to pszNewFilename.
 */

char **CPLCorrespondingPaths(const char *pszOldFilename,
                             const char *pszNewFilename, char **papszFileList)

{
    if (CSLCount(papszFileList) == 0)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      There is a special case for a one item list which exactly       */
    /*      matches the old name, to rename to the new name.                */
    /* -------------------------------------------------------------------- */
    if (CSLCount(papszFileList) == 1 &&
        strcmp(pszOldFilename, papszFileList[0]) == 0)
    {
        return CSLAddString(nullptr, pszNewFilename);
    }

    const std::string osOldPath = CPLGetPathSafe(pszOldFilename);
    const std::string osOldBasename = CPLGetBasenameSafe(pszOldFilename);
    const std::string osNewBasename = CPLGetBasenameSafe(pszNewFilename);

    /* -------------------------------------------------------------------- */
    /*      If the basename is changing, verify that all source files       */
    /*      have the same starting basename.                                */
    /* -------------------------------------------------------------------- */
    if (osOldBasename != osNewBasename)
    {
        for (int i = 0; papszFileList[i] != nullptr; i++)
        {
            if (osOldBasename == CPLGetBasenameSafe(papszFileList[i]))
                continue;

            const std::string osFilePath = CPLGetPathSafe(papszFileList[i]);
            const std::string osFileName = CPLGetFilename(papszFileList[i]);

            if (!EQUALN(osFileName.c_str(), osOldBasename.c_str(),
                        osOldBasename.size()) ||
                !EQUAL(osFilePath.c_str(), osOldPath.c_str()) ||
                osFileName[osOldBasename.size()] != '.')
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to rename fileset due irregular basenames.");
                return nullptr;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If the filename portions differs, ensure they only differ in    */
    /*      basename.                                                       */
    /* -------------------------------------------------------------------- */
    if (osOldBasename != osNewBasename)
    {
        const std::string osOldExtra =
            CPLGetFilename(pszOldFilename) + osOldBasename.size();
        const std::string osNewExtra =
            CPLGetFilename(pszNewFilename) + osNewBasename.size();

        if (osOldExtra != osNewExtra)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to rename fileset due to irregular filename "
                     "correspondence.");
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Generate the new filenames.                                     */
    /* -------------------------------------------------------------------- */
    char **papszNewList = nullptr;
    const std::string osNewPath = CPLGetPathSafe(pszNewFilename);

    for (int i = 0; papszFileList[i] != nullptr; i++)
    {
        const std::string osOldFilename = CPLGetFilename(papszFileList[i]);

        const std::string osNewFilename =
            osOldBasename == osNewBasename
                ? CPLFormFilenameSafe(osNewPath.c_str(), osOldFilename.c_str(),
                                      nullptr)
                : CPLFormFilenameSafe(osNewPath.c_str(), osNewBasename.c_str(),
                                      osOldFilename.c_str() +
                                          osOldBasename.size());

        papszNewList = CSLAddString(papszNewList, osNewFilename.c_str());
    }

    return papszNewList;
}

/************************************************************************/
/*                   CPLGenerateTempFilenameSafe()                      */
/************************************************************************/

/**
 * Generate temporary file name.
 *
 * Returns a filename that may be used for a temporary file.  The location
 * of the file tries to follow operating system semantics but may be
 * forced via the CPL_TMPDIR configuration option.
 *
 * @param pszStem if non-NULL this will be part of the filename.
 *
 * @return a filename
 *
 * @since 3.11
 */

std::string CPLGenerateTempFilenameSafe(const char *pszStem)

{
    const char *pszDir = CPLGetConfigOption("CPL_TMPDIR", nullptr);

    if (pszDir == nullptr)
        pszDir = CPLGetConfigOption("TMPDIR", nullptr);

    if (pszDir == nullptr)
        pszDir = CPLGetConfigOption("TEMP", nullptr);

    if (pszDir == nullptr)
        pszDir = ".";

    if (pszStem == nullptr)
        pszStem = "";

    static int nTempFileCounter = 0;
    CPLString osFilename;
    osFilename.Printf("%s_%d_%d", pszStem, CPLGetCurrentProcessID(),
                      CPLAtomicInc(&nTempFileCounter));

    return CPLFormFilenameSafe(pszDir, osFilename.c_str(), nullptr);
}

/************************************************************************/
/*                      CPLGenerateTempFilename()                       */
/************************************************************************/

/**
 * Generate temporary file name.
 *
 * Returns a filename that may be used for a temporary file.  The location
 * of the file tries to follow operating system semantics but may be
 * forced via the CPL_TMPDIR configuration option.
 *
 * @param pszStem if non-NULL this will be part of the filename.
 *
 * @return a filename which is valid till the next CPL call in this thread.
 *
 * @deprecated If using C++, prefer using CPLCleanTrailingSlashSafe() instead
 */

const char *CPLGenerateTempFilename(const char *pszStem)

{
    return CPLPathReturnTLSString(CPLGenerateTempFilenameSafe(pszStem),
                                  __FUNCTION__);
}

/************************************************************************/
/*                        CPLExpandTildeSafe()                          */
/************************************************************************/

/**
 * Expands ~/ at start of filename.
 *
 * Assumes that the HOME configuration option is defined.
 *
 * @param pszFilename filename potentially starting with ~/
 *
 * @return an expanded filename.
 *
 * @since GDAL 3.11
 */

std::string CPLExpandTildeSafe(const char *pszFilename)

{
    if (!STARTS_WITH_CI(pszFilename, "~/"))
        return pszFilename;

    const char *pszHome = CPLGetConfigOption("HOME", nullptr);
    if (pszHome == nullptr)
        return pszFilename;

    return CPLFormFilenameSafe(pszHome, pszFilename + 2, nullptr);
}

/************************************************************************/
/*                         CPLExpandTilde()                             */
/************************************************************************/

/**
 * Expands ~/ at start of filename.
 *
 * Assumes that the HOME configuration option is defined.
 *
 * @param pszFilename filename potentially starting with ~/
 *
 * @return an expanded filename.
 *
 * @since GDAL 2.2
 *
 * @deprecated If using C++, prefer using CPLExpandTildeSafe() instead
 */

const char *CPLExpandTilde(const char *pszFilename)

{
    return CPLPathReturnTLSString(CPLExpandTildeSafe(pszFilename),
                                  __FUNCTION__);
}

/************************************************************************/
/*                         CPLGetHomeDir()                              */
/************************************************************************/

/**
 * Return the path to the home directory
 *
 * That is the value of the USERPROFILE environment variable on Windows,
 * or HOME on other platforms.
 *
 * @return the home directory, or NULL.
 *
 * @since GDAL 2.3
 */

const char *CPLGetHomeDir()

{
#ifdef _WIN32
    return CPLGetConfigOption("USERPROFILE", nullptr);
#else
    return CPLGetConfigOption("HOME", nullptr);
#endif
}

/************************************************************************/
/*                      CPLLaunderForFilenameSafe()                     */
/************************************************************************/

/**
 * Launder a string to be compatible of a filename.
 *
 * @param pszName The input string to launder.
 * @param pszOutputPath The directory where the file would be created.
 *                      Unused for now. May be NULL.
 * @return the laundered name.
 *
 * @since GDAL 3.11
 */

std::string CPLLaunderForFilenameSafe(const char *pszName,
                                      CPL_UNUSED const char *pszOutputPath)
{
    std::string osRet(pszName);
    for (char &ch : osRet)
    {
        // https://docs.microsoft.com/en-us/windows/desktop/fileio/naming-a-file
        if (ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '/' ||
            ch == '\\' || ch == '?' || ch == '*')
        {
            ch = '_';
        }
    }
    return osRet;
}

/************************************************************************/
/*                        CPLLaunderForFilename()                       */
/************************************************************************/

/**
 * Launder a string to be compatible of a filename.
 *
 * @param pszName The input string to launder.
 * @param pszOutputPath The directory where the file would be created.
 *                      Unused for now. May be NULL.
 * @return the laundered name.
 *
 * @since GDAL 3.1
 *
 * @deprecated If using C++, prefer using CPLLaunderForFilenameSafe() instead
 */

const char *CPLLaunderForFilename(const char *pszName,
                                  const char *pszOutputPath)
{
    return CPLPathReturnTLSString(
        CPLLaunderForFilenameSafe(pszName, pszOutputPath), __FUNCTION__);
}

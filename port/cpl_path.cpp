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

CPL_CVSID("$Id$")

// Should be size of larged possible filename.
constexpr int CPL_PATH_BUF_SIZE = 2048;
constexpr int CPL_PATH_BUF_COUNT = 10;

#if defined(WIN32)
constexpr char SEP_STRING[] = "\\";
#else
constexpr char SEP_STRING[] = "/";
#endif

static const char* CPLStaticBufferTooSmall( char *pszStaticResult )
{
    CPLError(CE_Failure, CPLE_AppDefined, "Destination buffer too small");
    if( pszStaticResult == nullptr )
        return "";
    strcpy( pszStaticResult, "" );
    return pszStaticResult;
}

/************************************************************************/
/*                         CPLGetStaticResult()                         */
/************************************************************************/

static char *CPLGetStaticResult()

{
    int bMemoryError = FALSE;
    char *pachBufRingInfo =
        static_cast<char *>( CPLGetTLSEx( CTLS_PATHBUF, &bMemoryError ) );
    if( bMemoryError )
        return nullptr;
    if( pachBufRingInfo == nullptr )
    {
      pachBufRingInfo = static_cast<char *>(
          VSI_CALLOC_VERBOSE(
              1, sizeof(int) + CPL_PATH_BUF_SIZE * CPL_PATH_BUF_COUNT ) );
        if( pachBufRingInfo == nullptr )
            return nullptr;
        CPLSetTLS( CTLS_PATHBUF, pachBufRingInfo, TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Work out which string in the "ring" we want to use this         */
/*      time.                                                           */
/* -------------------------------------------------------------------- */
    int *pnBufIndex = reinterpret_cast<int *>( pachBufRingInfo );
    const size_t nOffset =
        sizeof(int) + static_cast<size_t>( *pnBufIndex * CPL_PATH_BUF_SIZE );
    char *pachBuffer = pachBufRingInfo + nOffset;

    *pnBufIndex = (*pnBufIndex + 1) % CPL_PATH_BUF_COUNT;

    return pachBuffer;
}

/************************************************************************/
/*                        CPLFindFilenameStart()                        */
/************************************************************************/

static int CPLFindFilenameStart( const char * pszFilename )

{
    size_t iFileStart = strlen(pszFilename);

    for( ;
         iFileStart > 0
             && pszFilename[iFileStart-1] != '/'
             && pszFilename[iFileStart-1] != '\\';
         iFileStart-- ) {}

    return static_cast<int>( iFileStart );
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
 * <pre>
 * CPLGetPath( "abc/def.xyz" ) == "abc"
 * CPLGetPath( "/abc/def/" ) == "/abc/def"
 * CPLGetPath( "/" ) == "/"
 * CPLGetPath( "/abc/def" ) == "/abc"
 * CPLGetPath( "abc" ) == ""
 * </pre>
 *
 * @param pszFilename the filename potentially including a path.
 *
 *  @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.  The returned
 * will generally not contain a trailing path separator.
 */

const char *CPLGetPath( const char *pszFilename )

{
    const int iFileStart = CPLFindFilenameStart(pszFilename);
    char *pszStaticResult = CPLGetStaticResult();

    if( pszStaticResult == nullptr || iFileStart >= CPL_PATH_BUF_SIZE )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( !(pszFilename >= pszStaticResult
                 && pszFilename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if( iFileStart == 0 )
    {
        strcpy( pszStaticResult, "" );
        return pszStaticResult;
    }

    CPLStrlcpy( pszStaticResult, pszFilename,
                static_cast<size_t>( iFileStart ) + 1 );

    if( iFileStart > 1
        && (pszStaticResult[iFileStart-1] == '/'
            || pszStaticResult[iFileStart-1] == '\\') )
        pszStaticResult[iFileStart-1] = '\0';

    return pszStaticResult;
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
 * <pre>
 * CPLGetDirname( "abc/def.xyz" ) == "abc"
 * CPLGetDirname( "/abc/def/" ) == "/abc/def"
 * CPLGetDirname( "/" ) == "/"
 * CPLGetDirname( "/abc/def" ) == "/abc"
 * CPLGetDirname( "abc" ) == "."
 * </pre>
 *
 * @param pszFilename the filename potentially including a path.
 *
 * @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.  The returned
 * will generally not contain a trailing path separator.
 */

const char *CPLGetDirname( const char *pszFilename )

{
    const int iFileStart = CPLFindFilenameStart(pszFilename);
    char *pszStaticResult = CPLGetStaticResult();

    if( pszStaticResult == nullptr || iFileStart >= CPL_PATH_BUF_SIZE )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( !(pszFilename >= pszStaticResult
                 && pszFilename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if( iFileStart == 0 )
    {
        strcpy( pszStaticResult, "." );
        return pszStaticResult;
    }

    CPLStrlcpy( pszStaticResult, pszFilename,
                static_cast<size_t>( iFileStart ) + 1 );

    if( iFileStart > 1
        && (pszStaticResult[iFileStart-1] == '/'
            || pszStaticResult[iFileStart-1] == '\\') )
        pszStaticResult[iFileStart-1] = '\0';

    return pszStaticResult;
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
 * <pre>
 * CPLGetFilename( "abc/def.xyz" ) == "def.xyz"
 * CPLGetFilename( "/abc/def/" ) == ""
 * CPLGetFilename( "abc/def" ) == "def"
 * </pre>
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the non-directory portion of the path (points back into
 * original string).
 */

const char *CPLGetFilename( const char *pszFullFilename )

{
    const int iFileStart = CPLFindFilenameStart( pszFullFilename );

    return pszFullFilename + iFileStart;
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
 * <pre>
 * CPLGetBasename( "abc/def.xyz" ) == "def"
 * CPLGetBasename( "abc/def" ) == "def"
 * CPLGetBasename( "abc/def/" ) == ""
 * </pre>
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the non-directory, non-extension portion of the path in
 * an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 */

const char *CPLGetBasename( const char *pszFullFilename )

{
    const size_t iFileStart =
        static_cast<size_t>( CPLFindFilenameStart( pszFullFilename ) );
    char *pszStaticResult = CPLGetStaticResult();
    if( pszStaticResult == nullptr )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( !( pszFullFilename >= pszStaticResult
                  && pszFullFilename < pszStaticResult + CPL_PATH_BUF_SIZE ) );

    size_t iExtStart = strlen(pszFullFilename);
    for( ;
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename);

    const size_t nLength = iExtStart - iFileStart;

    if( nLength >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLStrlcpy( pszStaticResult, pszFullFilename + iFileStart, nLength + 1 );

    return pszStaticResult;
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
 * <pre>
 * CPLGetExtension( "abc/def.xyz" ) == "xyz"
 * CPLGetExtension( "abc/def" ) == ""
 * </pre>
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the extension portion of the path in
 * an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 */

const char *CPLGetExtension( const char *pszFullFilename )

{
    if( pszFullFilename[0] == '\0' )
        return "";

    size_t iFileStart =
        static_cast<size_t>( CPLFindFilenameStart( pszFullFilename ) );
    char *pszStaticResult = CPLGetStaticResult();
    if( pszStaticResult == nullptr )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( !( pszFullFilename >= pszStaticResult
                  && pszFullFilename < pszStaticResult + CPL_PATH_BUF_SIZE ) );

    size_t iExtStart = strlen(pszFullFilename);
    for( ;
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename)-1;

    // If the extension is too long, it is very much likely not an extension,
    // but another component of the path
    const size_t knMaxExtensionSize = 10;
    if( strlen(pszFullFilename+iExtStart+1) > knMaxExtensionSize )
        return "";

    if( CPLStrlcpy( pszStaticResult, pszFullFilename+iExtStart+1,
                    CPL_PATH_BUF_SIZE )
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
        return CPLStaticBufferTooSmall(pszStaticResult);

    return pszStaticResult;
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

#ifdef HAVE_GETCWD
char *CPLGetCurrentDir()
{
#ifdef _MAX_PATH
    const size_t nPathMax = _MAX_PATH;
#elif PATH_MAX
    const size_t nPathMax = PATH_MAX;
#else
    const size_t nPathMax = 8192;
#endif

    char *pszDirPath = static_cast<char *>( VSI_MALLOC_VERBOSE( nPathMax ) );
    if( !pszDirPath )
        return nullptr;

    return getcwd( pszDirPath, nPathMax );
}
#else  // !HAVE_GETCWD
char *CPLGetCurrentDir() { return nullptr; }
#endif // HAVE_GETCWD

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
 */

const char *CPLResetExtension( const char *pszPath, const char *pszExt )

{
    char *pszStaticResult = CPLGetStaticResult();
    if( pszStaticResult == nullptr )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( ! ( pszPath >= pszStaticResult
                   && pszPath < pszStaticResult + CPL_PATH_BUF_SIZE ) );

/* -------------------------------------------------------------------- */
/*      First, try and strip off any existing extension.                */
/* -------------------------------------------------------------------- */
    if( CPLStrlcpy( pszStaticResult, pszPath, CPL_PATH_BUF_SIZE )
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
        return CPLStaticBufferTooSmall(pszStaticResult);

    if( *pszStaticResult )
    {
        for( size_t i = strlen(pszStaticResult) - 1; i > 0; i-- )
        {
            if( pszStaticResult[i] == '.' )
            {
                pszStaticResult[i] = '\0';
                break;
            }

            if( pszStaticResult[i] == '/' || pszStaticResult[i] == '\\'
                || pszStaticResult[i] == ':' )
                break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Append the new extension.                                       */
/* -------------------------------------------------------------------- */
    if( CPLStrlcat( pszStaticResult, ".", CPL_PATH_BUF_SIZE)
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) ||
        CPLStrlcat( pszStaticResult, pszExt, CPL_PATH_BUF_SIZE)
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
    {
        return CPLStaticBufferTooSmall(pszStaticResult);
    }

    return pszStaticResult;
}

/************************************************************************/
/*                       RequiresUnixPathSeparator()                    */
/************************************************************************/

#if defined(WIN32)
static bool RequiresUnixPathSeparator(const char* pszPath)
{
    return strcmp(pszPath, "/vsimem") == 0 ||
            STARTS_WITH(pszPath, "http://") ||
            STARTS_WITH(pszPath, "https://") ||
            STARTS_WITH(pszPath, "/vsimem/") ||
            STARTS_WITH(pszPath, "/vsicurl/") ||
            STARTS_WITH(pszPath, "/vsicurl_streaming/") ||
            STARTS_WITH(pszPath, "/vsis3/") ||
            STARTS_WITH(pszPath, "/vsis3_streaming/") ||
            STARTS_WITH(pszPath, "/vsigs/") ||
            STARTS_WITH(pszPath, "/vsigs_streaming/") ||
            STARTS_WITH(pszPath, "/vsiaz/") ||
            STARTS_WITH(pszPath, "/vsiaz_streaming/") ||
            STARTS_WITH(pszPath, "/vsiadls/") ||
            STARTS_WITH(pszPath, "/vsioss/") ||
            STARTS_WITH(pszPath, "/vsioss_streaming/") ||
            STARTS_WITH(pszPath, "/vsiswift/") ||
            STARTS_WITH(pszPath, "/vsiswift_streaming/") ||
            STARTS_WITH(pszPath, "/vsizip/");
}
#endif

/************************************************************************/
/*                          CPLFormFilename()                           */
/************************************************************************/

/**
 * Build a full file path from a passed path, file basename and extension.
 *
 * The path, and extension are optional.  The basename may in fact contain
 * an extension if desired.
 *
 * <pre>
 * CPLFormFilename("abc/xyz", "def", ".dat" ) == "abc/xyz/def.dat"
 * CPLFormFilename(NULL,"def", NULL ) == "def"
 * CPLFormFilename(NULL, "abc/def.dat", NULL ) == "abc/def.dat"
 * CPLFormFilename("/abc/xyz/", "def.dat", NULL ) == "/abc/xyz/def.dat"
 * </pre>
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
 */

const char *CPLFormFilename( const char * pszPath,
                             const char * pszBasename,
                             const char * pszExtension )

{
    char *pszStaticResult = CPLGetStaticResult();
    if( pszStaticResult == nullptr )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( ! ( pszPath >= pszStaticResult
                   && pszPath < pszStaticResult + CPL_PATH_BUF_SIZE ) );
    CPLAssert( ! ( pszBasename >= pszStaticResult
                   && pszBasename < pszStaticResult + CPL_PATH_BUF_SIZE ) );

    if( pszBasename[0] == '.' && (pszBasename[1] == '/' || pszBasename[1] == '\\') )
        pszBasename += 2;

    const char *pszAddedPathSep = "";
    const char *pszAddedExtSep = "";

    if( pszPath == nullptr )
        pszPath = "";
    size_t nLenPath = strlen(pszPath);
    if( !CPLIsFilenameRelative(pszPath) &&
        strcmp(pszBasename, "..") == 0 )
    {
        // /a/b + .. --> /a
        if( pszPath[nLenPath-1] == '\\' || pszPath[nLenPath-1] == '/' )
            nLenPath--;
        size_t nLenPathOri = nLenPath;
        while( nLenPath > 0 && pszPath[nLenPath-1] != '\\' &&
               pszPath[nLenPath-1] != '/')
        {
            nLenPath--;
        }
        if( nLenPath == 1 && pszPath[0] == '/' )
        {
            pszBasename = "";
        }
        else if( (nLenPath > 1 && pszPath[0] == '/') ||
                 (nLenPath > 2 && pszPath[1] == ':') ||
                 (nLenPath > 6 && strncmp(pszPath, "\\\\$\\", 4) == 0) )
        {
            nLenPath--;
            pszBasename = "";
        }
        else
        {
            nLenPath = nLenPathOri;
            pszAddedPathSep = SEP_STRING;
        }
    }
    else if( nLenPath > 0
             && pszPath[nLenPath-1] != '/'
             && pszPath[nLenPath-1] != '\\' )
    {
#if defined(WIN32)
        // FIXME? Would be better to ask the filesystems what it
        // prefers as directory separator?
        if( RequiresUnixPathSeparator(pszPath) )
            pszAddedPathSep = "/";
        else
#endif
        {
            pszAddedPathSep = SEP_STRING;
        }
    }

    if( pszExtension == nullptr )
        pszExtension = "";
    else if( pszExtension[0] != '.' && strlen(pszExtension) > 0 )
        pszAddedExtSep = ".";

    if( CPLStrlcpy( pszStaticResult, pszPath,
                    std::min(nLenPath + 1,
                             static_cast<size_t>(CPL_PATH_BUF_SIZE)) )
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) ||
        CPLStrlcat( pszStaticResult, pszAddedPathSep, CPL_PATH_BUF_SIZE)
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) ||
        CPLStrlcat( pszStaticResult, pszBasename, CPL_PATH_BUF_SIZE)
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) ||
        CPLStrlcat( pszStaticResult, pszAddedExtSep, CPL_PATH_BUF_SIZE)
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) ||
        CPLStrlcat( pszStaticResult, pszExtension, CPL_PATH_BUF_SIZE)
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
    {
        return CPLStaticBufferTooSmall(pszStaticResult);
    }

    return pszStaticResult;
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
 */

const char *CPLFormCIFilename( const char * pszPath,
                               const char * pszBasename,
                               const char * pszExtension )

{
    // On case insensitive filesystems, just default to CPLFormFilename().
    if( !VSIIsCaseSensitiveFS(pszPath) )
        return CPLFormFilename( pszPath, pszBasename, pszExtension );

    const char *pszAddedExtSep = "";
    size_t nLen = strlen(pszBasename) + 2;

    if( pszExtension != nullptr )
        nLen += strlen(pszExtension);

    char *pszFilename = static_cast<char *>( VSI_MALLOC_VERBOSE(nLen) );
    if( pszFilename == nullptr )
        return "";

    if( pszExtension == nullptr )
        pszExtension = "";
    else if( pszExtension[0] != '.' && strlen(pszExtension) > 0 )
        pszAddedExtSep = ".";

    snprintf( pszFilename, nLen, "%s%s%s",
             pszBasename, pszAddedExtSep, pszExtension );

    const char *pszFullPath = CPLFormFilename( pszPath, pszFilename, nullptr );
    VSIStatBufL sStatBuf;
    int nStatRet = VSIStatExL( pszFullPath, &sStatBuf, VSI_STAT_EXISTS_FLAG );
    if( nStatRet != 0 )
    {
        for( size_t i = 0; pszFilename[i] != '\0'; i++ )
        {
            if( islower(pszFilename[i]) )
                pszFilename[i] = static_cast<char>( toupper(pszFilename[i]) );
        }

        pszFullPath = CPLFormFilename( pszPath, pszFilename, nullptr );
        nStatRet = VSIStatExL( pszFullPath, &sStatBuf, VSI_STAT_EXISTS_FLAG );
    }

    if( nStatRet != 0 )
    {
        for( size_t i = 0; pszFilename[i] != '\0'; i++ )
        {
            if( isupper(pszFilename[i]) )
                pszFilename[i] = static_cast<char>( tolower(pszFilename[i]) );
        }

        pszFullPath = CPLFormFilename( pszPath, pszFilename, nullptr );
        nStatRet = VSIStatExL( pszFullPath, &sStatBuf, VSI_STAT_EXISTS_FLAG );
    }

    if( nStatRet != 0 )
        pszFullPath = CPLFormFilename( pszPath, pszBasename, pszExtension );

    CPLFree( pszFilename );

    return pszFullPath;
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
 * <pre>
 * CPLProjectRelativeFilename("abc/def", "tmp/abc.gif") == "abc/def/tmp/abc.gif"
 * CPLProjectRelativeFilename("abc/def", "/tmp/abc.gif") == "/tmp/abc.gif"
 * CPLProjectRelativeFilename("/xy", "abc.gif") == "/xy/abc.gif"
 * CPLProjectRelativeFilename("/abc/def", "../abc.gif") == "/abc/def/../abc.gif"
 * CPLProjectRelativeFilename("C:\WIN", "abc.gif") == "C:\WIN\abc.gif"
 * </pre>
 *
 * @param pszProjectDir the directory relative to which the secondary files
 * path should be interpreted.
 * @param pszSecondaryFilename the filename (potentially with path) that
 * is to be interpreted relative to the project directory.
 *
 * @return a composed path to the secondary file.  The returned string is
 * internal and should not be altered, freed, or depending on past the next
 * CPL call.
 */

const char *CPLProjectRelativeFilename( const char *pszProjectDir,
                                        const char *pszSecondaryFilename )

{
    char *pszStaticResult = CPLGetStaticResult();
    if( pszStaticResult == nullptr )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( !(pszProjectDir >= pszStaticResult
                 && pszProjectDir < pszStaticResult + CPL_PATH_BUF_SIZE ) );
    CPLAssert( !(pszSecondaryFilename >= pszStaticResult
                 && pszSecondaryFilename
                 < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if( !CPLIsFilenameRelative( pszSecondaryFilename ) )
        return pszSecondaryFilename;

    if( pszProjectDir == nullptr || strlen(pszProjectDir) == 0 )
        return pszSecondaryFilename;

    if( CPLStrlcpy( pszStaticResult, pszProjectDir, CPL_PATH_BUF_SIZE )
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
        return CPLStaticBufferTooSmall(pszStaticResult);

    if( pszProjectDir[strlen(pszProjectDir)-1] != '/'
        && pszProjectDir[strlen(pszProjectDir)-1] != '\\' )
    {
        // FIXME: Better to ask the filesystems what it
        // prefers as directory separator?
        const char* pszAddedPathSep = nullptr;
#if defined(WIN32)
        if( RequiresUnixPathSeparator(pszStaticResult) )
            pszAddedPathSep = "/";
        else
#endif
        {
            pszAddedPathSep = SEP_STRING;
        }
        if( CPLStrlcat( pszStaticResult, pszAddedPathSep, CPL_PATH_BUF_SIZE )
            >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
            return CPLStaticBufferTooSmall(pszStaticResult);
    }

    if( CPLStrlcat( pszStaticResult, pszSecondaryFilename, CPL_PATH_BUF_SIZE )
        >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
        return CPLStaticBufferTooSmall(pszStaticResult);

    return pszStaticResult;
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

int CPLIsFilenameRelative( const char *pszFilename )

{
    if( (pszFilename[0] != '\0'
         && (STARTS_WITH(pszFilename+1, ":\\")
             || STARTS_WITH(pszFilename+1, ":/")
             || strstr(pszFilename+1,"://") // http://, ftp:// etc....
            ))
        || STARTS_WITH(pszFilename, "\\\\?\\")  // Windows extended Length Path.
        || pszFilename[0] == '\\'
        || pszFilename[0] == '/' )
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

const char *CPLExtractRelativePath( const char *pszBaseDir,
                                    const char *pszTarget,
                                    int *pbGotRelative )

{
/* -------------------------------------------------------------------- */
/*      If we don't have a basedir, then we can't relativize the path.  */
/* -------------------------------------------------------------------- */
    if( pszBaseDir == nullptr )
    {
        if( pbGotRelative != nullptr )
            *pbGotRelative = FALSE;

        return pszTarget;
    }

    const size_t nBasePathLen = strlen(pszBaseDir);

/* -------------------------------------------------------------------- */
/*      One simple case is when the base dir is '.' and the target      */
/*      filename is relative.                                           */
/* -------------------------------------------------------------------- */
    if( (nBasePathLen == 0 || EQUAL(pszBaseDir, "."))
        && CPLIsFilenameRelative(pszTarget) )
    {
        if( pbGotRelative != nullptr )
            *pbGotRelative = TRUE;

        return pszTarget;
    }

/* -------------------------------------------------------------------- */
/*      By this point, if we don't have a base path, we can't have a    */
/*      meaningful common prefix.                                       */
/* -------------------------------------------------------------------- */
    if( nBasePathLen == 0 )
    {
        if( pbGotRelative != nullptr )
            *pbGotRelative = FALSE;

        return pszTarget;
    }

/* -------------------------------------------------------------------- */
/*      If we don't have a common path prefix, then we can't get a      */
/*      relative path.                                                  */
/* -------------------------------------------------------------------- */
    if( !EQUALN(pszBaseDir, pszTarget, nBasePathLen)
        || (pszTarget[nBasePathLen] != '\\'
            && pszTarget[nBasePathLen] != '/') )
    {
        if( pbGotRelative != nullptr )
            *pbGotRelative = FALSE;

        return pszTarget;
    }

/* -------------------------------------------------------------------- */
/*      We have a relative path.  Strip it off to get a string to       */
/*      return.                                                         */
/* -------------------------------------------------------------------- */
    if( pbGotRelative != nullptr )
        *pbGotRelative = TRUE;

    return pszTarget + nBasePathLen + 1;
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
 * <pre>
 * CPLCleanTrailingSlash( "abc/def/" ) == "abc/def"
 * CPLCleanTrailingSlash( "abc/def" ) == "abc/def"
 * CPLCleanTrailingSlash( "c:\abc\def\" ) == "c:\abc\def"
 * CPLCleanTrailingSlash( "c:\abc\def" ) == "c:\abc\def"
 * CPLCleanTrailingSlash( "abc" ) == "abc"
 * </pre>
 *
 * @param pszPath the path to be cleaned up
 *
 * @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 */

const char *CPLCleanTrailingSlash( const char *pszPath )

{
    char *pszStaticResult = CPLGetStaticResult();
    if( pszStaticResult == nullptr )
        return CPLStaticBufferTooSmall(pszStaticResult);
    CPLAssert( ! ( pszPath >= pszStaticResult
                   && pszPath < pszStaticResult + CPL_PATH_BUF_SIZE) );

    const size_t iPathLength = strlen(pszPath);
    if( iPathLength >= static_cast<size_t>( CPL_PATH_BUF_SIZE ) )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLStrlcpy( pszStaticResult, pszPath, iPathLength+1 );

    if( iPathLength > 0
        && (pszStaticResult[iPathLength-1] == '\\'
            || pszStaticResult[iPathLength-1] == '/'))
        pszStaticResult[iPathLength-1] = '\0';

    return pszStaticResult;
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

char **CPLCorrespondingPaths( const char *pszOldFilename,
                              const char *pszNewFilename,
                              char **papszFileList )

{
    if( CSLCount(papszFileList) == 0 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      There is a special case for a one item list which exactly       */
/*      matches the old name, to rename to the new name.                */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszFileList) == 1
        && strcmp(pszOldFilename, papszFileList[0]) == 0 )
    {
        return CSLAddString( nullptr, pszNewFilename );
    }

    const CPLString osOldPath = CPLGetPath( pszOldFilename );
    const CPLString osOldBasename = CPLGetBasename( pszOldFilename );
    const CPLString osNewBasename = CPLGetBasename( pszNewFilename );

/* -------------------------------------------------------------------- */
/*      If the basename is changing, verify that all source files       */
/*      have the same starting basename.                                */
/* -------------------------------------------------------------------- */
    if( osOldBasename != osNewBasename )
    {
        for( int i = 0; papszFileList[i] != nullptr; i++ )
        {
            if( osOldBasename == CPLGetBasename( papszFileList[i] ) )
                continue;

            const CPLString osFilePath = CPLGetPath( papszFileList[i] );
            const CPLString osFileName = CPLGetFilename( papszFileList[i] );

            if( !EQUALN(osFileName, osOldBasename, osOldBasename.size())
                || !EQUAL(osFilePath, osOldPath)
                || osFileName[osOldBasename.size()] != '.' )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to rename fileset due irregular basenames.");
                return nullptr;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If the filename portions differs, ensure they only differ in    */
/*      basename.                                                       */
/* -------------------------------------------------------------------- */
    if( osOldBasename != osNewBasename )
    {
        const CPLString osOldExtra =
            CPLGetFilename(pszOldFilename) + osOldBasename.size();
        const CPLString osNewExtra =
            CPLGetFilename(pszNewFilename) + osNewBasename.size();

        if( osOldExtra != osNewExtra )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to rename fileset due to irregular filename "
                      "correspondence." );
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Generate the new filenames.                                     */
/* -------------------------------------------------------------------- */
    char **papszNewList = nullptr;
    const CPLString osNewPath = CPLGetPath( pszNewFilename );

    for( int i = 0; papszFileList[i] != nullptr; i++ )
    {
        const CPLString osOldFilename = CPLGetFilename( papszFileList[i] );

        const CPLString osNewFilename =
            osOldBasename == osNewBasename
            ? CPLFormFilename( osNewPath, osOldFilename, nullptr )
            : CPLFormFilename( osNewPath, osNewBasename,
                               osOldFilename.c_str() + osOldBasename.size());

        papszNewList = CSLAddString( papszNewList, osNewFilename );
    }

    return papszNewList;
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
 */

const char *CPLGenerateTempFilename( const char *pszStem )

{
    const char *pszDir = CPLGetConfigOption( "CPL_TMPDIR", nullptr );

    if( pszDir == nullptr )
        pszDir = CPLGetConfigOption( "TMPDIR", nullptr );

    if( pszDir == nullptr )
        pszDir = CPLGetConfigOption( "TEMP", nullptr );

    if( pszDir == nullptr )
        pszDir = ".";

    if( pszStem == nullptr )
        pszStem = "";

    static int nTempFileCounter = 0;
    CPLString osFilename;
    osFilename.Printf( "%s_%d_%d",
                       pszStem,
                       CPLGetCurrentProcessID(),
                       CPLAtomicInc( &nTempFileCounter ) );

    return CPLFormFilename( pszDir, osFilename, nullptr );
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
 */

const char *CPLExpandTilde( const char *pszFilename )

{
    if( !STARTS_WITH_CI(pszFilename, "~/") )
        return pszFilename;

    const char* pszHome = CPLGetConfigOption("HOME", nullptr);
    if( pszHome == nullptr )
        return pszFilename;

    return CPLFormFilename( pszHome, pszFilename + 2, nullptr );
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
#ifdef WIN32
    return CPLGetConfigOption("USERPROFILE", nullptr);
#else
    return CPLGetConfigOption("HOME", nullptr);
#endif
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
 */

const char* CPLLaunderForFilename(const char* pszName,
                                  CPL_UNUSED const char* pszOutputPath )
{
    std::string osRet(pszName);
    for( char& ch: osRet )
    {
        // https://docs.microsoft.com/en-us/windows/desktop/fileio/naming-a-file
        if( ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '/' || ch == '\\' || ch== '?' || ch == '*' )
        {
            ch = '_';
        }
    }
    return CPLSPrintf("%s", osRet.c_str());
}

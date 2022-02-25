/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Convenience functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_config.h"

#ifdef HAVE_USELOCALE
// For uselocale, define _XOPEN_SOURCE = 700
// but on Solaris, we don't have uselocale and we cannot have
// std=c++11 with _XOPEN_SOURCE != 600
#if defined(__sun__) && __cplusplus >= 201103L
#if _XOPEN_SOURCE != 600
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 600
#endif
#else
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 700
#endif
#endif

// For atoll (at least for NetBSD)
#define _ISOC99_SOURCE

#ifdef MSVC_USE_VLD
#include <vld.h>
#endif

#include "cpl_conv.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if GDAL_HAVE_XLOCALE_H
#include <xlocale.h> // for LC_NUMERIC_MASK on MacOS
#endif

#ifdef DEBUG_CONFIG_OPTIONS
#include <set>
#endif
#include <string>

#include "cpl_config.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsil_curl_priv.h"

#ifdef DEBUG
#define OGRAPISPY_ENABLED
#endif
#ifdef OGRAPISPY_ENABLED
// Keep in sync with ograpispy.cpp
void OGRAPISPYCPLSetConfigOption(const char*, const char*);
void OGRAPISPYCPLSetThreadLocalConfigOption(const char*, const char*);
#endif

// Uncomment to get list of options that have been fetched and set.
// #define DEBUG_CONFIG_OPTIONS

CPL_CVSID("$Id$")

static CPLMutex *hConfigMutex = nullptr;
static volatile char **g_papszConfigOptions = nullptr;

// Used by CPLOpenShared() and friends.
static CPLMutex *hSharedFileMutex = nullptr;
static volatile int nSharedFileCount = 0;
static volatile CPLSharedFileInfo *pasSharedFileList = nullptr;

// Used by CPLsetlocale().
static CPLMutex *hSetLocaleMutex = nullptr;

// Note: ideally this should be added in CPLSharedFileInfo*
// but CPLSharedFileInfo is exposed in the API, hence that trick
// to hide this detail.
typedef struct
{
    GIntBig nPID;  // pid of opening thread.
} CPLSharedFileInfoExtra;

static volatile CPLSharedFileInfoExtra *pasSharedFileListExtra = nullptr;

/************************************************************************/
/*                             CPLCalloc()                              */
/************************************************************************/

/**
 * Safe version of calloc().
 *
 * This function is like the C library calloc(), but raises a CE_Fatal
 * error with CPLError() if it fails to allocate the desired memory.  It
 * should be used for small memory allocations that are unlikely to fail
 * and for which the application is unwilling to test for out of memory
 * conditions.  It uses VSICalloc() to get the memory, so any hooking of
 * VSICalloc() will apply to CPLCalloc() as well.  CPLFree() or VSIFree()
 * can be used free memory allocated by CPLCalloc().
 *
 * @param nCount number of objects to allocate.
 * @param nSize size (in bytes) of object to allocate.
 * @return pointer to newly allocated memory, only NULL if nSize * nCount is
 * NULL.
 */

void *CPLCalloc( size_t nCount, size_t nSize )

{
    if( nSize * nCount == 0 )
        return nullptr;

    void *pReturn = CPLMalloc(nCount * nSize);
    memset(pReturn, 0, nCount * nSize);
    return pReturn;
}

/************************************************************************/
/*                             CPLMalloc()                              */
/************************************************************************/

/**
 * Safe version of malloc().
 *
 * This function is like the C library malloc(), but raises a CE_Fatal
 * error with CPLError() if it fails to allocate the desired memory.  It
 * should be used for small memory allocations that are unlikely to fail
 * and for which the application is unwilling to test for out of memory
 * conditions.  It uses VSIMalloc() to get the memory, so any hooking of
 * VSIMalloc() will apply to CPLMalloc() as well.  CPLFree() or VSIFree()
 * can be used free memory allocated by CPLMalloc().
 *
 * @param nSize size (in bytes) of memory block to allocate.
 * @return pointer to newly allocated memory, only NULL if nSize is zero.
 */

void *CPLMalloc( size_t nSize )

{
    if( nSize == 0 )
        return nullptr;

    CPLVerifyConfiguration();

    if( (nSize >> (8 * sizeof(nSize) - 1)) != 0 )
    {
        // coverity[dead_error_begin]
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CPLMalloc(%ld): Silly size requested.",
                 static_cast<long>(nSize));
        return nullptr;
    }

    void *pReturn = VSIMalloc(nSize);
    if( pReturn == nullptr )
    {
        if( nSize < 2000 )
        {
            CPLEmergencyError("CPLMalloc(): Out of memory allocating a small "
                              "number of bytes.");
        }

        CPLError(CE_Fatal, CPLE_OutOfMemory,
                 "CPLMalloc(): Out of memory allocating %ld bytes.",
                 static_cast<long>(nSize));
    }

    return pReturn;
}

/************************************************************************/
/*                             CPLRealloc()                             */
/************************************************************************/

/**
 * Safe version of realloc().
 *
 * This function is like the C library realloc(), but raises a CE_Fatal
 * error with CPLError() if it fails to allocate the desired memory.  It
 * should be used for small memory allocations that are unlikely to fail
 * and for which the application is unwilling to test for out of memory
 * conditions.  It uses VSIRealloc() to get the memory, so any hooking of
 * VSIRealloc() will apply to CPLRealloc() as well.  CPLFree() or VSIFree()
 * can be used free memory allocated by CPLRealloc().
 *
 * It is also safe to pass NULL in as the existing memory block for
 * CPLRealloc(), in which case it uses VSIMalloc() to allocate a new block.
 *
 * @param pData existing memory block which should be copied to the new block.
 * @param nNewSize new size (in bytes) of memory block to allocate.
 * @return pointer to allocated memory, only NULL if nNewSize is zero.
 */

void * CPLRealloc( void * pData, size_t nNewSize )

{
    if( nNewSize == 0 )
    {
        VSIFree(pData);
        return nullptr;
    }

    if( (nNewSize >> (8 * sizeof(nNewSize) - 1)) != 0 )
    {
        // coverity[dead_error_begin]
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CPLRealloc(%ld): Silly size requested.",
                 static_cast<long>(nNewSize));
        return nullptr;
    }

    void *pReturn = nullptr;

    if( pData == nullptr )
        pReturn = VSIMalloc(nNewSize);
    else
        pReturn = VSIRealloc(pData, nNewSize);

    if( pReturn == nullptr )
    {
        if( nNewSize < 2000 )
        {
            char szSmallMsg[80] = {};

            snprintf(szSmallMsg, sizeof(szSmallMsg),
                     "CPLRealloc(): Out of memory allocating %ld bytes.",
                     static_cast<long>(nNewSize));
            CPLEmergencyError(szSmallMsg);
        }
        else
        {
            CPLError(CE_Fatal, CPLE_OutOfMemory,
                     "CPLRealloc(): Out of memory allocating %ld bytes.",
                     static_cast<long>(nNewSize));
        }
    }

    return pReturn;
}

/************************************************************************/
/*                             CPLStrdup()                              */
/************************************************************************/

/**
 * Safe version of strdup() function.
 *
 * This function is similar to the C library strdup() function, but if
 * the memory allocation fails it will issue a CE_Fatal error with
 * CPLError() instead of returning NULL. Memory
 * allocated with CPLStrdup() can be freed with CPLFree() or VSIFree().
 *
 * It is also safe to pass a NULL string into CPLStrdup().  CPLStrdup()
 * will allocate and return a zero length string (as opposed to a NULL
 * string).
 *
 * @param pszString input string to be duplicated.  May be NULL.
 * @return pointer to a newly allocated copy of the string.  Free with
 * CPLFree() or VSIFree().
 */

char *CPLStrdup( const char * pszString )

{
    if( pszString == nullptr )
        pszString = "";

    const size_t nLen = strlen(pszString);
    char* pszReturn = static_cast<char *>(CPLMalloc(nLen+1));
    memcpy( pszReturn, pszString, nLen+1 );
    return( pszReturn );
}

/************************************************************************/
/*                             CPLStrlwr()                              */
/************************************************************************/

/**
 * Convert each characters of the string to lower case.
 *
 * For example, "ABcdE" will be converted to "abcde".
 * This function is locale dependent.
 *
 * @param pszString input string to be converted.
 * @return pointer to the same string, pszString.
 */

char *CPLStrlwr( char *pszString )

{
    if( pszString == nullptr )
        return nullptr;

    char *pszTemp = pszString;

    while( *pszTemp )
    {
        *pszTemp = static_cast<char>(tolower(*pszTemp));
        pszTemp++;
    }

    return pszString;
}

/************************************************************************/
/*                              CPLFGets()                              */
/*                                                                      */
/*      Note: LF = \n = ASCII 10                                        */
/*            CR = \r = ASCII 13                                        */
/************************************************************************/

// ASCII characters.
constexpr char knLF = 10;
constexpr char knCR = 13;

/**
 * Reads in at most one less than nBufferSize characters from the fp
 * stream and stores them into the buffer pointed to by pszBuffer.
 * Reading stops after an EOF or a newline. If a newline is read, it
 * is _not_ stored into the buffer. A '\\0' is stored after the last
 * character in the buffer. All three types of newline terminators
 * recognized by the CPLFGets(): single '\\r' and '\\n' and '\\r\\n'
 * combination.
 *
 * @param pszBuffer pointer to the targeting character buffer.
 * @param nBufferSize maximum size of the string to read (not including
 * terminating '\\0').
 * @param fp file pointer to read from.
 * @return pointer to the pszBuffer containing a string read
 * from the file or NULL if the error or end of file was encountered.
 */

char *CPLFGets( char *pszBuffer, int nBufferSize, FILE *fp )

{
    if( nBufferSize == 0 || pszBuffer == nullptr || fp == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Let the OS level call read what it things is one line.  This    */
/*      will include the newline.  On windows, if the file happens      */
/*      to be in text mode, the CRLF will have been converted to        */
/*      just the newline (LF).  If it is in binary mode it may well     */
/*      have both.                                                      */
/* -------------------------------------------------------------------- */
    const long nOriginalOffset = VSIFTell(fp);
    if( VSIFGets(pszBuffer, nBufferSize, fp) == nullptr )
        return nullptr;

    int nActuallyRead = static_cast<int>(strlen(pszBuffer));
    if( nActuallyRead == 0 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we found \r and out buffer is full, it is possible there     */
/*      is also a pending \n.  Check for it.                            */
/* -------------------------------------------------------------------- */
    if( nBufferSize == nActuallyRead + 1 &&
        pszBuffer[nActuallyRead - 1] == knCR )
    {
        const int chCheck = fgetc(fp);
        if( chCheck != knLF )
        {
            // unget the character.
            if( VSIFSeek(fp, nOriginalOffset + nActuallyRead, SEEK_SET) == -1 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Unable to unget a character");
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Trim off \n, \r or \r\n if it appears at the end.  We don't     */
/*      need to do any "seeking" since we want the newline eaten.       */
/* -------------------------------------------------------------------- */
    if( nActuallyRead > 1 &&
        pszBuffer[nActuallyRead-1] == knLF &&
        pszBuffer[nActuallyRead-2] == knCR )
    {
        pszBuffer[nActuallyRead - 2] = '\0';
    }
    else if( pszBuffer[nActuallyRead - 1] == knLF ||
             pszBuffer[nActuallyRead - 1] == knCR )
    {
        pszBuffer[nActuallyRead - 1] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Search within the string for a \r (MacOS convention             */
/*      apparently), and if we find it we need to trim the string,      */
/*      and seek back.                                                  */
/* -------------------------------------------------------------------- */
    char *pszExtraNewline = strchr(pszBuffer, knCR);

    if( pszExtraNewline != nullptr )
    {
        nActuallyRead = static_cast<int>(pszExtraNewline - pszBuffer + 1);

        *pszExtraNewline = '\0';
        if( VSIFSeek(fp, nOriginalOffset + nActuallyRead - 1, SEEK_SET) != 0)
            return nullptr;

        // This hackery is necessary to try and find our correct
        // spot on win32 systems with text mode line translation going
        // on.  Sometimes the fseek back overshoots, but it doesn't
        // "realize it" till a character has been read. Try to read till
        // we get to the right spot and get our CR.
        int chCheck = fgetc(fp);
        while( (chCheck != knCR && chCheck != EOF) ||
               VSIFTell(fp) < nOriginalOffset + nActuallyRead )
        {
            static bool bWarned = false;

            if( !bWarned )
            {
                bWarned = true;
                CPLDebug("CPL",
                         "CPLFGets() correcting for DOS text mode translation "
                         "seek problem.");
            }
            chCheck = fgetc(fp);
        }
    }

    return pszBuffer;
}

/************************************************************************/
/*                         CPLReadLineBuffer()                          */
/*                                                                      */
/*      Fetch readline buffer, and ensure it is the desired size,       */
/*      reallocating if needed.  Manages TLS (thread local storage)     */
/*      issues for the buffer.                                          */
/*      We use a special trick to track the actual size of the buffer   */
/*      The first 4 bytes are reserved to store it as a int, hence the  */
/*      -4 / +4 hacks with the size and pointer.                        */
/************************************************************************/
static char *CPLReadLineBuffer( int nRequiredSize )

{

/* -------------------------------------------------------------------- */
/*      A required size of -1 means the buffer should be freed.         */
/* -------------------------------------------------------------------- */
    if( nRequiredSize == -1 )
    {
        int bMemoryError = FALSE;
        void *pRet = CPLGetTLSEx(CTLS_RLBUFFERINFO, &bMemoryError);
        if( pRet != nullptr )
        {
            CPLFree(pRet);
            CPLSetTLS(CTLS_RLBUFFERINFO, nullptr, FALSE);
        }
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If the buffer doesn't exist yet, create it.                     */
/* -------------------------------------------------------------------- */
    int bMemoryError = FALSE;
    GUInt32 *pnAlloc =
        static_cast<GUInt32 *>(CPLGetTLSEx(CTLS_RLBUFFERINFO, &bMemoryError));
    if( bMemoryError )
        return nullptr;

    if( pnAlloc == nullptr )
    {
        pnAlloc = static_cast<GUInt32 *>(VSI_MALLOC_VERBOSE(200));
        if( pnAlloc == nullptr )
            return nullptr;
        *pnAlloc = 196;
        CPLSetTLS(CTLS_RLBUFFERINFO, pnAlloc, TRUE);
    }

/* -------------------------------------------------------------------- */
/*      If it is too small, grow it bigger.                             */
/* -------------------------------------------------------------------- */
    if( static_cast<int>(*pnAlloc) - 1 < nRequiredSize )
    {
        const int nNewSize = nRequiredSize + 4 + 500;
        if( nNewSize <= 0 )
        {
            VSIFree(pnAlloc);
            CPLSetTLS(CTLS_RLBUFFERINFO, nullptr, FALSE);
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "CPLReadLineBuffer(): Trying to allocate more than "
                     "2 GB.");
            return nullptr;
        }

        GUInt32 *pnAllocNew =
            static_cast<GUInt32 *>(VSI_REALLOC_VERBOSE(pnAlloc, nNewSize));
        if( pnAllocNew == nullptr )
        {
            VSIFree(pnAlloc);
            CPLSetTLS(CTLS_RLBUFFERINFO, nullptr, FALSE);
            return nullptr;
        }
        pnAlloc = pnAllocNew;

        *pnAlloc = nNewSize - 4;
        CPLSetTLS(CTLS_RLBUFFERINFO, pnAlloc, TRUE);
    }

    return reinterpret_cast<char *>(pnAlloc + 1);
}

/************************************************************************/
/*                            CPLReadLine()                             */
/************************************************************************/

/**
 * Simplified line reading from text file.
 *
 * Read a line of text from the given file handle, taking care
 * to capture CR and/or LF and strip off ... equivalent of
 * DKReadLine().  Pointer to an internal buffer is returned.
 * The application shouldn't free it, or depend on its value
 * past the next call to CPLReadLine().
 *
 * Note that CPLReadLine() uses VSIFGets(), so any hooking of VSI file
 * services should apply to CPLReadLine() as well.
 *
 * CPLReadLine() maintains an internal buffer, which will appear as a
 * single block memory leak in some circumstances.  CPLReadLine() may
 * be called with a NULL FILE * at any time to free this working buffer.
 *
 * @param fp file pointer opened with VSIFOpen().
 *
 * @return pointer to an internal buffer containing a line of text read
 * from the file or NULL if the end of file was encountered.
 */

const char *CPLReadLine( FILE *fp )

{
/* -------------------------------------------------------------------- */
/*      Cleanup case.                                                   */
/* -------------------------------------------------------------------- */
    if( fp == nullptr )
    {
        CPLReadLineBuffer(-1);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Loop reading chunks of the line till we get to the end of       */
/*      the line.                                                       */
/* -------------------------------------------------------------------- */
    size_t nBytesReadThisTime = 0;
    char *pszRLBuffer = nullptr;
    size_t nReadSoFar = 0;

    do {
/* -------------------------------------------------------------------- */
/*      Grow the working buffer if we have it nearly full.  Fail out    */
/*      of read line if we can't reallocate it big enough (for          */
/*      instance for a _very large_ file with no newlines).             */
/* -------------------------------------------------------------------- */
        if( nReadSoFar > 100 * 1024 * 1024 )
            // It is dubious that we need to read a line longer than 100 MB.
            return nullptr;
        pszRLBuffer = CPLReadLineBuffer(static_cast<int>(nReadSoFar) + 129);
        if( pszRLBuffer == nullptr )
            return nullptr;

/* -------------------------------------------------------------------- */
/*      Do the actual read.                                             */
/* -------------------------------------------------------------------- */
        if( CPLFGets(pszRLBuffer + nReadSoFar, 128, fp) == nullptr &&
            nReadSoFar == 0 )
            return nullptr;

        nBytesReadThisTime = strlen(pszRLBuffer + nReadSoFar);
        nReadSoFar += nBytesReadThisTime;
    } while( nBytesReadThisTime >= 127 &&
             pszRLBuffer[nReadSoFar - 1] != knCR &&
             pszRLBuffer[nReadSoFar - 1] != knLF );

    return pszRLBuffer;
}

/************************************************************************/
/*                            CPLReadLineL()                            */
/************************************************************************/

/**
 * Simplified line reading from text file.
 *
 * Similar to CPLReadLine(), but reading from a large file API handle.
 *
 * @param fp file pointer opened with VSIFOpenL().
 *
 * @return pointer to an internal buffer containing a line of text read
 * from the file or NULL if the end of file was encountered.
 */

const char *CPLReadLineL(VSILFILE *fp) { return CPLReadLine2L(fp, -1, nullptr); }

/************************************************************************/
/*                           CPLReadLine2L()                            */
/************************************************************************/

/**
 * Simplified line reading from text file.
 *
 * Similar to CPLReadLine(), but reading from a large file API handle.
 *
 * @param fp file pointer opened with VSIFOpenL().
 * @param nMaxCars  maximum number of characters allowed, or -1 for no limit.
 * @param papszOptions NULL-terminated array of options. Unused for now.

 * @return pointer to an internal buffer containing a line of text read
 * from the file or NULL if the end of file was encountered or the maximum
 * number of characters allowed reached.
 *
 * @since GDAL 1.7.0
 */

const char *CPLReadLine2L( VSILFILE *fp, int nMaxCars,
                           CPL_UNUSED CSLConstList papszOptions )

{
    int nBufLength;
    return CPLReadLine3L( fp, nMaxCars,
                          &nBufLength,
                          papszOptions );
}

/************************************************************************/
/*                           CPLReadLine3L()                            */
/************************************************************************/

/**
 * Simplified line reading from text file.
 *
 * Similar to CPLReadLine(), but reading from a large file API handle.
 *
 * @param fp file pointer opened with VSIFOpenL().
 * @param nMaxCars  maximum number of characters allowed, or -1 for no limit.
 * @param papszOptions NULL-terminated array of options. Unused for now.
 * @param[out] pnBufLength size of output string (must be non-NULL)

 * @return pointer to an internal buffer containing a line of text read
 * from the file or NULL if the end of file was encountered or the maximum
 * number of characters allowed reached.
 *
 * @since GDAL 2.3.0
 */
const char *CPLReadLine3L( VSILFILE *fp, int nMaxCars,
                           int *pnBufLength,
                           CPL_UNUSED CSLConstList papszOptions )
{
/* -------------------------------------------------------------------- */
/*      Cleanup case.                                                   */
/* -------------------------------------------------------------------- */
    if( fp == nullptr )
    {
        CPLReadLineBuffer(-1);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Loop reading chunks of the line till we get to the end of       */
/*      the line.                                                       */
/* -------------------------------------------------------------------- */
    char *pszRLBuffer = nullptr;
    const size_t nChunkSize = 40;
    char szChunk[nChunkSize] = {};
    size_t nChunkBytesRead = 0;
    size_t nChunkBytesConsumed = 0;

    *pnBufLength = 0;
    szChunk[0] = 0;

    while( true )
    {
/* -------------------------------------------------------------------- */
/*      Read a chunk from the input file.                               */
/* -------------------------------------------------------------------- */
        if( *pnBufLength > INT_MAX - static_cast<int>(nChunkSize) - 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too big line : more than 2 billion characters!.");
            CPLReadLineBuffer(-1);
            return nullptr;
        }

        pszRLBuffer =
            CPLReadLineBuffer(static_cast<int>(*pnBufLength + nChunkSize + 1));
        if( pszRLBuffer == nullptr )
            return nullptr;

        if( nChunkBytesRead == nChunkBytesConsumed + 1 )
        {

            // case where one character is left over from last read.
            szChunk[0] = szChunk[nChunkBytesConsumed];

            nChunkBytesConsumed = 0;
            nChunkBytesRead = VSIFReadL(szChunk + 1, 1, nChunkSize - 1, fp) + 1;
        }
        else
        {
            nChunkBytesConsumed = 0;

            // fresh read.
            nChunkBytesRead = VSIFReadL(szChunk, 1, nChunkSize, fp);
            if( nChunkBytesRead == 0 )
            {
                if( *pnBufLength == 0 )
                    return nullptr;

                break;
            }
        }

/* -------------------------------------------------------------------- */
/*      copy over characters watching for end-of-line.                  */
/* -------------------------------------------------------------------- */
        bool bBreak = false;
        while( nChunkBytesConsumed < nChunkBytesRead - 1 && !bBreak )
        {
            if( (szChunk[nChunkBytesConsumed] == knCR &&
                 szChunk[nChunkBytesConsumed+1] == knLF) ||
                (szChunk[nChunkBytesConsumed] == knLF &&
                 szChunk[nChunkBytesConsumed+1] == knCR) )
            {
                nChunkBytesConsumed += 2;
                bBreak = true;
            }
            else if( szChunk[nChunkBytesConsumed] == knLF ||
                     szChunk[nChunkBytesConsumed] == knCR )
            {
                nChunkBytesConsumed += 1;
                bBreak = true;
            }
            else
            {
                pszRLBuffer[(*pnBufLength)++] = szChunk[nChunkBytesConsumed++];
                if( nMaxCars >= 0 && *pnBufLength == nMaxCars )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Maximum number of characters allowed reached.");
                    return nullptr;
                }
            }
        }

        if( bBreak )
            break;

/* -------------------------------------------------------------------- */
/*      If there is a remaining character and it is not a newline       */
/*      consume it.  If it is a newline, but we are clearly at the      */
/*      end of the file then consume it.                                */
/* -------------------------------------------------------------------- */
        if( nChunkBytesConsumed == nChunkBytesRead - 1 &&
            nChunkBytesRead < nChunkSize )
        {
            if( szChunk[nChunkBytesConsumed] == knLF ||
                szChunk[nChunkBytesConsumed] == knCR )
            {
                nChunkBytesConsumed++;
                break;
            }

            pszRLBuffer[(*pnBufLength)++] = szChunk[nChunkBytesConsumed++];
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have left over bytes after breaking out, seek back to     */
/*      ensure they remain to be read next time.                        */
/* -------------------------------------------------------------------- */
    if( nChunkBytesConsumed < nChunkBytesRead )
    {
        const size_t nBytesToPush = nChunkBytesRead - nChunkBytesConsumed;

        if( VSIFSeekL(fp, VSIFTellL(fp) - nBytesToPush, SEEK_SET) != 0 )
            return nullptr;
    }

    pszRLBuffer[*pnBufLength] = '\0';

    return pszRLBuffer;
}

/************************************************************************/
/*                            CPLScanString()                           */
/************************************************************************/

/**
 * Scan up to a maximum number of characters from a given string,
 * allocate a buffer for a new string and fill it with scanned characters.
 *
 * @param pszString String containing characters to be scanned. It may be
 * terminated with a null character.
 *
 * @param nMaxLength The maximum number of character to read. Less
 * characters will be read if a null character is encountered.
 *
 * @param bTrimSpaces If TRUE, trim ending spaces from the input string.
 * Character considered as empty using isspace(3) function.
 *
 * @param bNormalize If TRUE, replace ':' symbol with the '_'. It is needed if
 * resulting string will be used in CPL dictionaries.
 *
 * @return Pointer to the resulting string buffer. Caller responsible to free
 * this buffer with CPLFree().
 */

char *CPLScanString( const char *pszString, int nMaxLength,
                     int bTrimSpaces, int bNormalize )
{
    if( !pszString )
        return nullptr;

    if( !nMaxLength )
        return CPLStrdup("");

    char *pszBuffer = static_cast<char *>(CPLMalloc(nMaxLength + 1));
    if( !pszBuffer )
        return nullptr;

    strncpy(pszBuffer, pszString, nMaxLength);
    pszBuffer[nMaxLength] = '\0';

    if( bTrimSpaces )
    {
        size_t i = strlen(pszBuffer);
        while( i > 0 )
        {
            i --;
            if( !isspace(static_cast<unsigned char>(pszBuffer[i])) )
                break;
            pszBuffer[i] = '\0';
        }
    }

    if( bNormalize )
    {
        size_t i = strlen(pszBuffer);
        while( i > 0 )
        {
            i --;
            if( pszBuffer[i] == ':' )
                pszBuffer[i] = '_';
        }
    }

    return pszBuffer;
}

/************************************************************************/
/*                             CPLScanLong()                            */
/************************************************************************/

/**
 * Scan up to a maximum number of characters from a string and convert
 * the result to a long.
 *
 * @param pszString String containing characters to be scanned. It may be
 * terminated with a null character.
 *
 * @param nMaxLength The maximum number of character to consider as part
 * of the number. Less characters will be considered if a null character
 * is encountered.
 *
 * @return Long value, converted from its ASCII form.
 */

long CPLScanLong( const char *pszString, int nMaxLength )
{
    CPLAssert(nMaxLength >= 0);
    if( pszString == nullptr )
        return 0;
    const size_t nLength = CPLStrnlen(pszString, nMaxLength);
    const std::string osValue(pszString, nLength);
    return atol(osValue.c_str());
}

/************************************************************************/
/*                            CPLScanULong()                            */
/************************************************************************/

/**
 * Scan up to a maximum number of characters from a string and convert
 * the result to a unsigned long.
 *
 * @param pszString String containing characters to be scanned. It may be
 * terminated with a null character.
 *
 * @param nMaxLength The maximum number of character to consider as part
 * of the number. Less characters will be considered if a null character
 * is encountered.
 *
 * @return Unsigned long value, converted from its ASCII form.
 */

unsigned long CPLScanULong( const char *pszString, int nMaxLength )
{
    CPLAssert(nMaxLength >= 0);
    if( pszString == nullptr )
        return 0;
    const size_t nLength = CPLStrnlen(pszString, nMaxLength);
    const std::string osValue(pszString, nLength);
    return strtoul(osValue.c_str(), nullptr, 10);
}

/************************************************************************/
/*                           CPLScanUIntBig()                           */
/************************************************************************/

/**
 * Extract big integer from string.
 *
 * Scan up to a maximum number of characters from a string and convert
 * the result to a GUIntBig.
 *
 * @param pszString String containing characters to be scanned. It may be
 * terminated with a null character.
 *
 * @param nMaxLength The maximum number of character to consider as part
 * of the number. Less characters will be considered if a null character
 * is encountered.
 *
 * @return GUIntBig value, converted from its ASCII form.
 */

GUIntBig CPLScanUIntBig( const char *pszString, int nMaxLength )
{
    CPLAssert(nMaxLength >= 0);
    if( pszString == nullptr )
        return 0;
    const size_t nLength = CPLStrnlen(pszString, nMaxLength);
    const std::string osValue(pszString, nLength);

/* -------------------------------------------------------------------- */
/*      Fetch out the result                                            */
/* -------------------------------------------------------------------- */
    return strtoull(osValue.c_str(), nullptr, 10);
}

/************************************************************************/
/*                           CPLAtoGIntBig()                            */
/************************************************************************/

/**
 * Convert a string to a 64 bit signed integer.
 *
 * @param pszString String containing 64 bit signed integer.
 * @return 64 bit signed integer.
 * @since GDAL 2.0
 */

GIntBig CPLAtoGIntBig( const char *pszString )
{
    return atoll(pszString);
}

#if defined(__MINGW32__) || defined(__sun__)

// mingw atoll() doesn't return ERANGE in case of overflow
static int CPLAtoGIntBigExHasOverflow(const char* pszString, GIntBig nVal)
{
    if( strlen(pszString) <= 18 )
        return FALSE;
    while( *pszString == ' ' )
        pszString++;
    if( *pszString == '+' )
        pszString++;
    char szBuffer[32] = {};
/* x86_64-w64-mingw32-g++ (GCC) 4.8.2 annoyingly warns */
#ifdef HAVE_GCC_DIAGNOSTIC_PUSH
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif
    snprintf(szBuffer, sizeof(szBuffer), CPL_FRMT_GIB, nVal);
#ifdef HAVE_GCC_DIAGNOSTIC_PUSH
#pragma GCC diagnostic pop
#endif
    return strcmp(szBuffer, pszString) != 0;
}

#endif

/************************************************************************/
/*                          CPLAtoGIntBigEx()                           */
/************************************************************************/

/**
 * Convert a string to a 64 bit signed integer.
 *
 * @param pszString String containing 64 bit signed integer.
 * @param bWarn Issue a warning if an overflow occurs during conversion
 * @param pbOverflow Pointer to an integer to store if an overflow occurred, or
 *        NULL
 * @return 64 bit signed integer.
 * @since GDAL 2.0
 */

GIntBig CPLAtoGIntBigEx( const char* pszString, int bWarn, int *pbOverflow )
{
    errno = 0;
    GIntBig nVal = strtoll(pszString, nullptr, 10);
    if( errno == ERANGE
#if defined(__MINGW32__) || defined(__sun__)
        || CPLAtoGIntBigExHasOverflow(pszString, nVal)
#endif
        )
    {
        if( pbOverflow )
            *pbOverflow = TRUE;
        if( bWarn )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "64 bit integer overflow when converting %s",
                     pszString);
        }
        while( *pszString == ' ' )
            pszString++;
        return (*pszString == '-') ? GINTBIG_MIN : GINTBIG_MAX;
    }
    else if( pbOverflow )
    {
        *pbOverflow = FALSE;
    }
    return nVal;
}

/************************************************************************/
/*                           CPLScanPointer()                           */
/************************************************************************/

/**
 * Extract pointer from string.
 *
 * Scan up to a maximum number of characters from a string and convert
 * the result to a pointer.
 *
 * @param pszString String containing characters to be scanned. It may be
 * terminated with a null character.
 *
 * @param nMaxLength The maximum number of character to consider as part
 * of the number. Less characters will be considered if a null character
 * is encountered.
 *
 * @return pointer value, converted from its ASCII form.
 */

void *CPLScanPointer( const char *pszString, int nMaxLength )
{
    char szTemp[128] = {};

/* -------------------------------------------------------------------- */
/*      Compute string into local buffer, and terminate it.             */
/* -------------------------------------------------------------------- */
    if( nMaxLength > static_cast<int>( sizeof(szTemp) ) - 1 )
        nMaxLength = sizeof(szTemp) - 1;

    strncpy(szTemp, pszString, nMaxLength);
    szTemp[nMaxLength] = '\0';

/* -------------------------------------------------------------------- */
/*      On MSVC we have to scanf pointer values without the 0x          */
/*      prefix.                                                         */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(szTemp, "0x") )
    {
        void *pResult = nullptr;

#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
        // cppcheck-suppress invalidscanf
        sscanf(szTemp + 2, "%p", &pResult);
#else
        // cppcheck-suppress invalidscanf
        sscanf(szTemp, "%p", &pResult);

        // Solaris actually behaves like MSVCRT.
        if( pResult == nullptr )
        {
            // cppcheck-suppress invalidscanf
            sscanf(szTemp + 2, "%p", &pResult);
        }
#endif
        return pResult;
    }

#if SIZEOF_VOIDP == 8
    return reinterpret_cast<void *>(CPLScanUIntBig(szTemp, nMaxLength));
#else
    return reinterpret_cast<void *>(CPLScanULong(szTemp, nMaxLength));
#endif
}

/************************************************************************/
/*                             CPLScanDouble()                          */
/************************************************************************/

/**
 * Extract double from string.
 *
 * Scan up to a maximum number of characters from a string and convert the
 * result to a double. This function uses CPLAtof() to convert string to
 * double value, so it uses a comma as a decimal delimiter.
 *
 * @param pszString String containing characters to be scanned. It may be
 * terminated with a null character.
 *
 * @param nMaxLength The maximum number of character to consider as part
 * of the number. Less characters will be considered if a null character
 * is encountered.
 *
 * @return Double value, converted from its ASCII form.
 */

double CPLScanDouble( const char *pszString, int nMaxLength )
{
    char szValue[32] = {};
    char *pszValue = nullptr;

    if( nMaxLength + 1 < static_cast<int>(sizeof(szValue)) )
        pszValue = szValue;
    else
        pszValue = static_cast<char *>(CPLMalloc(nMaxLength + 1));

/* -------------------------------------------------------------------- */
/*      Compute string into local buffer, and terminate it.             */
/* -------------------------------------------------------------------- */
    strncpy(pszValue, pszString, nMaxLength);
    pszValue[nMaxLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Make a pass through converting 'D's to 'E's.                    */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nMaxLength; i++ )
        if( pszValue[i] == 'd' || pszValue[i] == 'D' )
            pszValue[i] = 'E';

/* -------------------------------------------------------------------- */
/*      The conversion itself.                                          */
/* -------------------------------------------------------------------- */
    const double dfValue = CPLAtof(pszValue);

    if( pszValue != szValue )
        CPLFree(pszValue);
    return dfValue;
}

/************************************************************************/
/*                      CPLPrintString()                                */
/************************************************************************/

/**
 * Copy the string pointed to by pszSrc, NOT including the terminating
 * `\\0' character, to the array pointed to by pszDest.
 *
 * @param pszDest Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string.
 *
 * @param pszSrc Pointer to the source buffer.
 *
 * @param nMaxLen Maximum length of the resulting string. If string length
 * is greater than nMaxLen, it will be truncated.
 *
 * @return Number of characters printed.
 */

int CPLPrintString( char *pszDest, const char *pszSrc, int nMaxLen )
{
    if( !pszDest )
        return 0;

    if( !pszSrc )
    {
        *pszDest = '\0';
        return 1;
    }

    int nChars = 0;
    char *pszTemp = pszDest;

    while( nChars < nMaxLen && *pszSrc )
    {
        *pszTemp++ = *pszSrc++;
        nChars++;
    }

    return nChars;
}

/************************************************************************/
/*                         CPLPrintStringFill()                         */
/************************************************************************/

/**
 * Copy the string pointed to by pszSrc, NOT including the terminating
 * `\\0' character, to the array pointed to by pszDest. Remainder of the
 * destination string will be filled with space characters. This is only
 * difference from the PrintString().
 *
 * @param pszDest Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string.
 *
 * @param pszSrc Pointer to the source buffer.
 *
 * @param nMaxLen Maximum length of the resulting string. If string length
 * is greater than nMaxLen, it will be truncated.
 *
 * @return Number of characters printed.
 */

int CPLPrintStringFill( char *pszDest, const char *pszSrc, int nMaxLen )
{
    if( !pszDest )
        return 0;

    if( !pszSrc )
    {
        memset(pszDest, ' ', nMaxLen);
        return nMaxLen;
    }

    char *pszTemp = pszDest;
    while( nMaxLen && *pszSrc )
    {
        *pszTemp++ = *pszSrc++;
        nMaxLen--;
    }

    if( nMaxLen )
        memset(pszTemp, ' ', nMaxLen);

    return nMaxLen;
}

/************************************************************************/
/*                          CPLPrintInt32()                             */
/************************************************************************/

/**
 * Print GInt32 value into specified string buffer. This string will not
 * be NULL-terminated.
 *
 * @param pszBuffer Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string. Note, that the string will
 * not be NULL-terminated, so user should do this himself, if needed.
 *
 * @param iValue Numerical value to print.
 *
 * @param nMaxLen Maximum length of the resulting string. If string length
 * is greater than nMaxLen, it will be truncated.
 *
 * @return Number of characters printed.
 */

int CPLPrintInt32( char *pszBuffer, GInt32 iValue, int nMaxLen )
{
    if( !pszBuffer )
        return 0;

    if( nMaxLen >= 64 )
        nMaxLen = 63;

    char szTemp[64] = {};

#if UINT_MAX == 65535
    snprintf(szTemp, sizeof(szTemp), "%*ld", nMaxLen, iValue);
#else
    snprintf(szTemp, sizeof(szTemp), "%*d", nMaxLen, iValue);
#endif

    return CPLPrintString(pszBuffer, szTemp, nMaxLen);
}

/************************************************************************/
/*                          CPLPrintUIntBig()                           */
/************************************************************************/

/**
 * Print GUIntBig value into specified string buffer. This string will not
 * be NULL-terminated.
 *
 * @param pszBuffer Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string. Note, that the string will
 * not be NULL-terminated, so user should do this himself, if needed.
 *
 * @param iValue Numerical value to print.
 *
 * @param nMaxLen Maximum length of the resulting string. If string length
 * is greater than nMaxLen, it will be truncated.
 *
 * @return Number of characters printed.
 */

int CPLPrintUIntBig( char *pszBuffer, GUIntBig iValue, int nMaxLen )
{
    if( !pszBuffer )
        return 0;

    if( nMaxLen >= 64 )
        nMaxLen = 63;

    char szTemp[64] = {};

#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
/* x86_64-w64-mingw32-g++ (GCC) 4.8.2 annoyingly warns */
#ifdef HAVE_GCC_DIAGNOSTIC_PUSH
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif
    snprintf(szTemp, sizeof(szTemp), "%*I64u", nMaxLen, iValue);
#ifdef HAVE_GCC_DIAGNOSTIC_PUSH
#pragma GCC diagnostic pop
#endif
#else
    snprintf(szTemp, sizeof(szTemp), "%*llu", nMaxLen, iValue);
#endif

    return CPLPrintString(pszBuffer, szTemp, nMaxLen);
}

/************************************************************************/
/*                          CPLPrintPointer()                           */
/************************************************************************/

/**
 * Print pointer value into specified string buffer. This string will not
 * be NULL-terminated.
 *
 * @param pszBuffer Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string. Note, that the string will
 * not be NULL-terminated, so user should do this himself, if needed.
 *
 * @param pValue Pointer to ASCII encode.
 *
 * @param nMaxLen Maximum length of the resulting string. If string length
 * is greater than nMaxLen, it will be truncated.
 *
 * @return Number of characters printed.
 */

int CPLPrintPointer( char *pszBuffer, void *pValue, int nMaxLen )
{
    if( !pszBuffer )
        return 0;

    if( nMaxLen >= 64 )
        nMaxLen = 63;

    char szTemp[64] = {};

    snprintf(szTemp, sizeof(szTemp), "%p", pValue);

    // On windows, and possibly some other platforms the sprintf("%p")
    // does not prefix things with 0x so it is hard to know later if the
    // value is hex encoded.  Fix this up here.

    if( !STARTS_WITH_CI(szTemp, "0x") )
        snprintf(szTemp, sizeof(szTemp), "0x%p", pValue);

    return CPLPrintString(pszBuffer, szTemp, nMaxLen);
}

/************************************************************************/
/*                          CPLPrintDouble()                            */
/************************************************************************/

/**
 * Print double value into specified string buffer. Exponential character
 * flag 'E' (or 'e') will be replaced with 'D', as in Fortran. Resulting
 * string will not to be NULL-terminated.
 *
 * @param pszBuffer Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string. Note, that the string will
 * not be NULL-terminated, so user should do this himself, if needed.
 *
 * @param pszFormat Format specifier (for example, "%16.9E").
 *
 * @param dfValue Numerical value to print.
 *
 * @param pszLocale Unused.
 *
 * @return Number of characters printed.
 */

int CPLPrintDouble( char *pszBuffer, const char *pszFormat,
                    double dfValue, CPL_UNUSED const char *pszLocale )
{
    if( !pszBuffer )
        return 0;

    const int knDoubleBufferSize = 64;
    char szTemp[knDoubleBufferSize] = {};

    CPLsnprintf(szTemp, knDoubleBufferSize, pszFormat, dfValue);
    szTemp[knDoubleBufferSize - 1] = '\0';

    for( int i = 0; szTemp[i] != '\0'; i++ )
    {
        if( szTemp[i] == 'E' || szTemp[i] == 'e' )
            szTemp[i] = 'D';
    }

    return CPLPrintString( pszBuffer, szTemp, 64 );
}

/************************************************************************/
/*                            CPLPrintTime()                            */
/************************************************************************/

/**
 * Print specified time value accordingly to the format options and
 * specified locale name. This function does following:
 *
 *  - if locale parameter is not NULL, the current locale setting will be
 *  stored and replaced with the specified one;
 *  - format time value with the strftime(3) function;
 *  - restore back current locale, if was saved.
 *
 * @param pszBuffer Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string. Note, that the string will
 * not be NULL-terminated, so user should do this himself, if needed.
 *
 * @param nMaxLen Maximum length of the resulting string. If string length is
 * greater than nMaxLen, it will be truncated.
 *
 * @param pszFormat Controls the output format. Options are the same as
 * for strftime(3) function.
 *
 * @param poBrokenTime Pointer to the broken-down time structure. May be
 * requested with the VSIGMTime() and VSILocalTime() functions.
 *
 * @param pszLocale Pointer to a character string containing locale name
 * ("C", "POSIX", "us_US", "ru_RU.KOI8-R" etc.). If NULL we will not
 * manipulate with locale settings and current process locale will be used for
 * printing. Be aware that it may be unsuitable to use current locale for
 * printing time, because all names will be printed in your native language,
 * as well as time format settings also may be adjusted differently from the
 * C/POSIX defaults. To solve these problems this option was introduced.
 *
 * @return Number of characters printed.
 */

int CPLPrintTime( char *pszBuffer, int nMaxLen, const char *pszFormat,
                  const struct tm *poBrokenTime, const char *pszLocale )
{
    char *pszTemp =
        static_cast<char *>(CPLMalloc((nMaxLen + 1) * sizeof(char)));

    if( pszLocale && EQUAL(pszLocale, "C") &&
        strcmp(pszFormat, "%a, %d %b %Y %H:%M:%S GMT") == 0 )
    {
        // Particular case when formatting RFC822 datetime, to avoid locale change
        static const char* const aszMonthStr[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        static const char* const aszDayOfWeek[] =
            { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        snprintf(pszTemp, nMaxLen + 1,
                 "%s, %02d %s %04d %02d:%02d:%02d GMT",
                 aszDayOfWeek[std::max(0, std::min(6, poBrokenTime->tm_wday))],
                 poBrokenTime->tm_mday,
                 aszMonthStr[std::max(0, std::min(11, poBrokenTime->tm_mon))],
                 poBrokenTime->tm_year + 1900,
                 poBrokenTime->tm_hour,
                 poBrokenTime->tm_min,
                 poBrokenTime->tm_sec);
    }
    else
    {
#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
        char *pszCurLocale = NULL;

        if( pszLocale || EQUAL(pszLocale, "") )
        {
            // Save the current locale.
            pszCurLocale = CPLsetlocale(LC_ALL, NULL);
            // Set locale to the specified value.
            CPLsetlocale(LC_ALL, pszLocale);
        }
#else
        (void)pszLocale;
#endif

        if( !strftime(pszTemp, nMaxLen + 1, pszFormat, poBrokenTime) )
            memset(pszTemp, 0, nMaxLen + 1);

#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
        // Restore stored locale back.
        if( pszCurLocale )
            CPLsetlocale( LC_ALL, pszCurLocale );
#endif
    }

    const int nChars = CPLPrintString(pszBuffer, pszTemp, nMaxLen);

    CPLFree(pszTemp);

    return nChars;
}

/************************************************************************/
/*                       CPLVerifyConfiguration()                       */
/************************************************************************/

void CPLVerifyConfiguration()

{
    static bool verified = false;
    if( verified )
    {
        return;
    }
    verified = true;

/* -------------------------------------------------------------------- */
/*      Verify data types.                                              */
/* -------------------------------------------------------------------- */
    CPL_STATIC_ASSERT(sizeof(GInt32) == 4);
    CPL_STATIC_ASSERT(sizeof(GInt16) == 2);
    CPL_STATIC_ASSERT(sizeof(GByte) == 1);

/* -------------------------------------------------------------------- */
/*      Verify byte order                                               */
/* -------------------------------------------------------------------- */
    GInt32 nTest = 1;

#ifdef CPL_LSB
    if( reinterpret_cast<GByte *>(&nTest)[0] != 1 )
#endif
#ifdef CPL_MSB
    if( reinterpret_cast<GByte *>(&nTest)[3] != 1 )
#endif
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "CPLVerifyConfiguration(): byte order set wrong.");
}

#ifdef DEBUG_CONFIG_OPTIONS

static void *hRegisterConfigurationOptionMutex = 0;
static std::set<CPLString> *paoGetKeys = NULL;
static std::set<CPLString> *paoSetKeys = NULL;

/************************************************************************/
/*                      CPLShowAccessedOptions()                        */
/************************************************************************/

static void CPLShowAccessedOptions()
{
    std::set<CPLString>::iterator aoIter;

    printf("Configuration options accessed in reading : "); /*ok*/
    aoIter = paoGetKeys->begin();
    while( aoIter != paoGetKeys->end() )
    {
        printf("%s, ", (*aoIter).c_str()); /*ok*/
        ++aoIter;
    }
    printf("\n");/*ok*/

    printf("Configuration options accessed in writing : "); /*ok*/
    aoIter = paoSetKeys->begin();
    while( aoIter != paoSetKeys->end() )
    {
        printf("%s, ", (*aoIter).c_str()); /*ok*/
        ++aoIter;
    }
    printf("\n"); /*ok*/

    delete paoGetKeys;
    delete paoSetKeys;
    paoGetKeys = NULL;
    paoSetKeys = NULL;
}

/************************************************************************/
/*                       CPLAccessConfigOption()                        */
/************************************************************************/

static void CPLAccessConfigOption( const char *pszKey, bool bGet )
{
    CPLMutexHolderD(&hRegisterConfigurationOptionMutex);
    if( paoGetKeys == NULL )
    {
        paoGetKeys = new std::set<CPLString>;
        paoSetKeys = new std::set<CPLString>;
        atexit(CPLShowAccessedOptions);
    }
    if( bGet )
        paoGetKeys->insert(pszKey);
    else
        paoSetKeys->insert(pszKey);
}
#endif

/************************************************************************/
/*                         CPLGetConfigOption()                         */
/************************************************************************/

/**
  * Get the value of a configuration option.
  *
  * The value is the value of a (key, value) option set with
  * CPLSetConfigOption(), or CPLSetThreadLocalConfigOption() of the same
  * thread. If the given option was no defined with
  * CPLSetConfigOption(), it tries to find it in environment variables.
  *
  * Note: the string returned by CPLGetConfigOption() might be short-lived, and
  * in particular it will become invalid after a call to CPLSetConfigOption()
  * with the same key.
  *
  * To override temporary a potentially existing option with a new value, you
  * can use the following snippet :
  * <pre>
  *     // backup old value
  *     const char* pszOldValTmp = CPLGetConfigOption(pszKey, NULL);
  *     char* pszOldVal = pszOldValTmp ? CPLStrdup(pszOldValTmp) : NULL;
  *     // override with new value
  *     CPLSetConfigOption(pszKey, pszNewVal);
  *     // do something useful
  *     // restore old value
  *     CPLSetConfigOption(pszKey, pszOldVal);
  *     CPLFree(pszOldVal);
  * </pre>
  *
  * @param pszKey the key of the option to retrieve
  * @param pszDefault a default value if the key does not match existing defined
  *     options (may be NULL)
  * @return the value associated to the key, or the default value if not found
  *
  * @see CPLSetConfigOption(), http://trac.osgeo.org/gdal/wiki/ConfigOptions
  */
const char * CPL_STDCALL
CPLGetConfigOption( const char *pszKey, const char *pszDefault )

{
#ifdef DEBUG_CONFIG_OPTIONS
    CPLAccessConfigOption(pszKey, TRUE);
#endif

    const char *pszResult = nullptr;

    int bMemoryError = FALSE;
    char **papszTLConfigOptions = reinterpret_cast<char **>(
        CPLGetTLSEx(CTLS_CONFIGOPTIONS, &bMemoryError));
    if( papszTLConfigOptions != nullptr )
        pszResult = CSLFetchNameValue(papszTLConfigOptions, pszKey);

    if( pszResult == nullptr )
    {
        CPLMutexHolderD(&hConfigMutex);

        pszResult =
            CSLFetchNameValue(const_cast<char **>(g_papszConfigOptions), pszKey);
    }

    if( pszResult == nullptr )
        pszResult = getenv(pszKey);

    if( pszResult == nullptr )
        return pszDefault;

    return pszResult;
}

/************************************************************************/
/*                         CPLGetConfigOptions()                        */
/************************************************************************/

/**
  * Return the list of configuration options as KEY=VALUE pairs.
  *
  * The list is the one set through the CPLSetConfigOption() API.
  *
  * Options that through environment variables or with
  * CPLSetThreadLocalConfigOption() will *not* be listed.
  *
  * @return a copy of the list, to be freed with CSLDestroy().
  * @since GDAL 2.2
  */
char** CPLGetConfigOptions(void)
{
    CPLMutexHolderD(&hConfigMutex);
    return CSLDuplicate(const_cast<char**>(g_papszConfigOptions));
}

/************************************************************************/
/*                         CPLSetConfigOptions()                        */
/************************************************************************/

/**
  * Replace the full list of configuration options with the passed list of
  * KEY=VALUE pairs.
  *
  * This has the same effect of clearing the existing list, and setting
  * individually each pair with the CPLSetConfigOption() API.
  *
  * This does not affect options set through environment variables or with
  * CPLSetThreadLocalConfigOption().
  *
  * The passed list is copied by the function.
  *
  * @param papszConfigOptions the new list (or NULL).
  *
  * @since GDAL 2.2
  */
void CPLSetConfigOptions(const char* const * papszConfigOptions)
{
    CPLMutexHolderD(&hConfigMutex);
    CSLDestroy(const_cast<char**>(g_papszConfigOptions));
    g_papszConfigOptions = const_cast<volatile char**>(
            CSLDuplicate(const_cast<char**>(papszConfigOptions)));
}

/************************************************************************/
/*                   CPLGetThreadLocalConfigOption()                    */
/************************************************************************/

/** Same as CPLGetConfigOption() but only with options set with
 * CPLSetThreadLocalConfigOption() */
const char * CPL_STDCALL
CPLGetThreadLocalConfigOption( const char *pszKey, const char *pszDefault )

{
#ifdef DEBUG_CONFIG_OPTIONS
    CPLAccessConfigOption(pszKey, TRUE);
#endif

    const char *pszResult = nullptr;

    int bMemoryError = FALSE;
    char **papszTLConfigOptions = reinterpret_cast<char **>(
        CPLGetTLSEx(CTLS_CONFIGOPTIONS, &bMemoryError));
    if( papszTLConfigOptions != nullptr )
        pszResult = CSLFetchNameValue(papszTLConfigOptions, pszKey);

    if( pszResult == nullptr )
        return pszDefault;

    return pszResult;
}

/************************************************************************/
/*                  NotifyOtherComponentsConfigOptionChanged()          */
/************************************************************************/

static void NotifyOtherComponentsConfigOptionChanged( const char *pszKey,
                                                      const char * /*pszValue*/ )
{
    // Hack
    if( STARTS_WITH_CI(pszKey, "AWS_") )
        VSICurlAuthParametersChanged();
}

/************************************************************************/
/*                         CPLSetConfigOption()                         */
/************************************************************************/

/**
  * Set a configuration option for GDAL/OGR use.
  *
  * Those options are defined as a (key, value) couple. The value corresponding
  * to a key can be got later with the CPLGetConfigOption() method.
  *
  * This mechanism is similar to environment variables, but options set with
  * CPLSetConfigOption() overrides, for CPLGetConfigOption() point of view,
  * values defined in the environment.
  *
  * If CPLSetConfigOption() is called several times with the same key, the
  * value provided during the last call will be used.
  *
  * Options can also be passed on the command line of most GDAL utilities
  * with the with '--config KEY VALUE'. For example,
  * ogrinfo --config CPL_DEBUG ON ~/data/test/point.shp
  *
  * This function can also be used to clear a setting by passing NULL as the
  * value (note: passing NULL will not unset an existing environment variable;
  * it will just unset a value previously set by CPLSetConfigOption()).
  *
  * @param pszKey the key of the option
  * @param pszValue the value of the option, or NULL to clear a setting.
  *
  * @see http://trac.osgeo.org/gdal/wiki/ConfigOptions
  */
void CPL_STDCALL
CPLSetConfigOption( const char *pszKey, const char *pszValue )

{
    NotifyOtherComponentsConfigOptionChanged(pszKey, pszValue);

#ifdef DEBUG_CONFIG_OPTIONS
    CPLAccessConfigOption(pszKey, FALSE);
#endif
    CPLMutexHolderD(&hConfigMutex);

#ifdef OGRAPISPY_ENABLED
    OGRAPISPYCPLSetConfigOption(pszKey, pszValue);
#endif

    g_papszConfigOptions = const_cast<volatile char **>(
        CSLSetNameValue(
            const_cast<char **>(g_papszConfigOptions), pszKey, pszValue));
}

/************************************************************************/
/*                   CPLSetThreadLocalTLSFreeFunc()                     */
/************************************************************************/

/* non-stdcall wrapper function for CSLDestroy() (#5590) */
static void CPLSetThreadLocalTLSFreeFunc( void *pData )
{
    CSLDestroy(reinterpret_cast<char **>(pData));
}

/************************************************************************/
/*                   CPLSetThreadLocalConfigOption()                    */
/************************************************************************/

/**
  * Set a configuration option for GDAL/OGR use.
  *
  * Those options are defined as a (key, value) couple. The value corresponding
  * to a key can be got later with the CPLGetConfigOption() method.
  *
  * This function sets the configuration option that only applies in the
  * current thread, as opposed to CPLSetConfigOption() which sets an option
  * that applies on all threads. CPLSetThreadLocalConfigOption() will override
  * the effect of CPLSetConfigOption) for the current thread.
  *
  * This function can also be used to clear a setting by passing NULL as the
  * value (note: passing NULL will not unset an existing environment variable or
  * a value set through CPLSetConfigOption();
  * it will just unset a value previously set by
  * CPLSetThreadLocalConfigOption()).
  *
  * @param pszKey the key of the option
  * @param pszValue the value of the option, or NULL to clear a setting.
  */

void CPL_STDCALL
CPLSetThreadLocalConfigOption( const char *pszKey, const char *pszValue )

{
    NotifyOtherComponentsConfigOptionChanged(pszKey, pszValue);

#ifdef DEBUG_CONFIG_OPTIONS
    CPLAccessConfigOption(pszKey, FALSE);
#endif

#ifdef OGRAPISPY_ENABLED
    OGRAPISPYCPLSetThreadLocalConfigOption(pszKey, pszValue);
#endif

    int bMemoryError = FALSE;
    char **papszTLConfigOptions = reinterpret_cast<char **>(
        CPLGetTLSEx(CTLS_CONFIGOPTIONS, &bMemoryError));
    if( bMemoryError )
        return;

    papszTLConfigOptions =
        CSLSetNameValue(papszTLConfigOptions, pszKey, pszValue);

    CPLSetTLSWithFreeFunc(CTLS_CONFIGOPTIONS, papszTLConfigOptions,
                          CPLSetThreadLocalTLSFreeFunc);
}

/************************************************************************/
/*                   CPLGetThreadLocalConfigOptions()                   */
/************************************************************************/

/**
  * Return the list of thread local configuration options as KEY=VALUE pairs.
  *
  * Options that through environment variables or with
  * CPLSetConfigOption() will *not* be listed.
  *
  * @return a copy of the list, to be freed with CSLDestroy().
  * @since GDAL 2.2
  */
char** CPLGetThreadLocalConfigOptions(void)
{
    int bMemoryError = FALSE;
    char **papszTLConfigOptions = reinterpret_cast<char **>(
        CPLGetTLSEx(CTLS_CONFIGOPTIONS, &bMemoryError));
    if( bMemoryError )
        return nullptr;
    return CSLDuplicate(papszTLConfigOptions);
}

/************************************************************************/
/*                   CPLSetThreadLocalConfigOptions()                   */
/************************************************************************/

/**
  * Replace the full list of thread local configuration options with the
  * passed list of KEY=VALUE pairs.
  *
  * This has the same effect of clearing the existing list, and setting
  * individually each pair with the CPLSetThreadLocalConfigOption() API.
  *
  * This does not affect options set through environment variables or with
  * CPLSetConfigOption().
  *
  * The passed list is copied by the function.
  *
  * @param papszConfigOptions the new list (or NULL).
  *
  * @since GDAL 2.2
  */
void CPLSetThreadLocalConfigOptions(const char* const * papszConfigOptions)
{
    int bMemoryError = FALSE;
    char **papszTLConfigOptions = reinterpret_cast<char **>(
        CPLGetTLSEx(CTLS_CONFIGOPTIONS, &bMemoryError));
    if( bMemoryError )
        return;
    CSLDestroy(papszTLConfigOptions);
    papszTLConfigOptions = CSLDuplicate(const_cast<char**>(papszConfigOptions));
    CPLSetTLSWithFreeFunc(CTLS_CONFIGOPTIONS, papszTLConfigOptions,
                          CPLSetThreadLocalTLSFreeFunc);
}

/************************************************************************/
/*                           CPLFreeConfig()                            */
/************************************************************************/

void CPL_STDCALL CPLFreeConfig()

{
    {
        CPLMutexHolderD(&hConfigMutex);

        CSLDestroy(const_cast<char **>(g_papszConfigOptions));
        g_papszConfigOptions = nullptr;

        int bMemoryError = FALSE;
        char **papszTLConfigOptions = reinterpret_cast<char **>(
            CPLGetTLSEx(CTLS_CONFIGOPTIONS, &bMemoryError));
        if( papszTLConfigOptions != nullptr )
        {
            CSLDestroy(papszTLConfigOptions);
            CPLSetTLS(CTLS_CONFIGOPTIONS, nullptr, FALSE);
        }
    }
    CPLDestroyMutex(hConfigMutex);
    hConfigMutex = nullptr;
}

/************************************************************************/
/*                    CPLLoadConfigOptionsFromFile()                    */
/************************************************************************/

/** Load configuration from a given configuration file.
 *
 * A configuration file is a text file in a .ini style format, that lists
 * configuration options and their values.
 * Lines starting with # are comment lines.
 *
 * Example:
 * <pre>
 * [configoptions]
 * # set BAR as the value of configuration option FOO
 * FOO=BAR
 * </pre>
 *
 * This function is typically called by CPLLoadConfigOptionsFromPredefinedFiles()
 *
 * @param pszFilename File where to load configuration from.
 * @param bOverrideEnvVars Whether configuration options from the configuration
 *                         file should override environment variables.
 * @since GDAL 3.3
 */
void CPLLoadConfigOptionsFromFile(const char* pszFilename, int bOverrideEnvVars)
{
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == nullptr )
        return;
    CPLDebug("CPL", "Loading configuration from %s", pszFilename);
    const char* pszLine;
    bool bInConfigOptions = false;
    while( (pszLine = CPLReadLine2L(fp, -1, nullptr)) != nullptr )
    {
        if( pszLine[0] == '#' )
        {
            // Comment line
        }
        else if( strcmp(pszLine, "[configoptions]") == 0 )
        {
            bInConfigOptions = true;
        }
        else if( pszLine[0] == '[' )
        {
            bInConfigOptions = false;
        }
        else if( bInConfigOptions )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
            if( pszKey && pszValue )
            {
                if( bOverrideEnvVars || getenv(pszKey) == nullptr )
                {
                    CPLDebugOnly("CPL",
                                 "Setting configuration option %s=%s",
                                 pszKey, pszValue);
                    CPLSetConfigOption(pszKey, pszValue);
                }
                else
                {
                    CPLDebugOnly("CPL",
                                 "Ignoring configuration option %s from "
                                 "configuration file as it is already set "
                                 "as an environment variable", pszKey);
                }
            }
            CPLFree(pszKey);
        }
    }
    VSIFCloseL(fp);
}

/************************************************************************/
/*                CPLLoadConfigOptionsFromPredefinedFiles()             */
/************************************************************************/

/** Load configuration from a set of predefined files.
 *
 * If the environment variable (or configuration option) GDAL_CONFIG_FILE is
 * set, then CPLLoadConfigOptionsFromFile() will be called with the value of
 * this configuration option as the file location.
 *
 * Otherwise, for Unix builds, CPLLoadConfigOptionsFromFile() will be called
 * with ${sysconfdir}/gdal/gdalrc first where ${sysconfdir} evaluates
 * to ${prefix}/etc, unless the --sysconfdir switch of configure has been invoked.
 *
 * Then CPLLoadConfigOptionsFromFile() will be called with $(HOME)/.gdal/gdalrc
 * on Unix builds (potentially overriding what was loaded with the sysconfdir)
 * or $(USERPROFILE)/.gdal/gdalrc on Windows builds.
 *
 * CPLLoadConfigOptionsFromFile() will be called with bOverrideEnvVars = false,
 * that is the value of environment variables previously set will be used instead
 * of the value set in the configuration files.
 *
 * This function is automatically called by GDALDriverManager() constructor
 *
 * @since GDAL 3.3
 */
void CPLLoadConfigOptionsFromPredefinedFiles()
{
    const char* pszFile = CPLGetConfigOption("GDAL_CONFIG_FILE", nullptr);
    if( pszFile != nullptr )
    {
        CPLLoadConfigOptionsFromFile(pszFile, false);
    }
    else
    {
#ifdef SYSCONFDIR
        pszFile = CPLFormFilename(CPLFormFilename(SYSCONFDIR, "gdal", nullptr),
                                  "gdalrc", nullptr);
        CPLLoadConfigOptionsFromFile(pszFile, false);
#endif

#ifdef WIN32
        const char* pszHome = CPLGetConfigOption("USERPROFILE", nullptr);
#else
        const char* pszHome = CPLGetConfigOption("HOME", nullptr);
#endif
        if( pszHome != nullptr )
        {
            pszFile = CPLFormFilename(CPLFormFilename( pszHome, ".gdal", nullptr),
                                      "gdalrc", nullptr);
            CPLLoadConfigOptionsFromFile(pszFile, false);
        }
    }
}

/************************************************************************/
/*                              CPLStat()                               */
/************************************************************************/

/** Same as VSIStat() except it works on "C:" as if it were "C:\". */

int CPLStat( const char *pszPath, VSIStatBuf *psStatBuf )

{
    if( strlen(pszPath) == 2 && pszPath[1] == ':' )
    {
        char szAltPath[4] = {pszPath[0], pszPath[1], '\\', '\0'};
        return VSIStat(szAltPath, psStatBuf);
    }

    return VSIStat(pszPath, psStatBuf);
}

/************************************************************************/
/*                            proj_strtod()                             */
/************************************************************************/
static double proj_strtod(char *nptr, char **endptr)

{
    char c = '\0';
    char *cp = nptr;

    // Scan for characters which cause problems with VC++ strtod().
    while( (c = *cp) != '\0')
    {
        if( c == 'd' || c == 'D' )
        {
            // Found one, so NUL it out, call strtod(),
            // then restore it and return.
            *cp = '\0';
            const double result = CPLStrtod(nptr, endptr);
            *cp = c;
            return result;
        }
        ++cp;
    }

    // No offending characters, just handle normally.

    return CPLStrtod(nptr, endptr);
}

/************************************************************************/
/*                            CPLDMSToDec()                             */
/************************************************************************/

static const char *sym = "NnEeSsWw";
constexpr double vm[] = { 1.0, 0.0166666666667, 0.00027777778 };

/** CPLDMSToDec */
double CPLDMSToDec( const char *is )

{
    // Copy string into work space.
    while( isspace(static_cast<unsigned char>(*is)) )
        ++is;

    const char *p = is;
    char work[64] = {};
    char *s = work;
    int n = sizeof(work);
    for( ; isgraph(*p) && --n; )
        *s++ = *p++;
    *s = '\0';
    // It is possible that a really odd input (like lots of leading
    // zeros) could be truncated in copying into work.  But...
    s = work;
    int sign = *s;

    if( sign == '+' || sign == '-' )
        s++;
    else
        sign = '+';

    int nl = 0;
    double v = 0.0;
    for( ; nl < 3; nl = n + 1 )
    {
        if( !(isdigit(*s) || *s == '.') )
            break;
        const double tv = proj_strtod(s, &s);
        if( tv == HUGE_VAL )
            return tv;
        switch( *s )
        {
          case 'D':
          case 'd':
            n = 0; break;
          case '\'':
            n = 1; break;
          case '"':
            n = 2; break;
          case 'r':
          case 'R':
            if( nl )
            {
                return 0.0;
            }
            ++s;
            v = tv;
            goto skip;
          default:
            v += tv * vm[nl];
          skip:
            n = 4;
            continue;
        }
        if( n < nl )
        {
            return 0.0;
        }
        v += tv * vm[n];
        ++s;
    }
    // Postfix sign.
    if( *s && ((p = strchr(sym, *s))) != nullptr )
    {
        sign = (p - sym) >= 4 ? '-' : '+';
        ++s;
    }
    if( sign == '-' )
        v = -v;

    return v;
}

/************************************************************************/
/*                            CPLDecToDMS()                             */
/************************************************************************/

/** Translate a decimal degrees value to a DMS string with hemisphere. */

const char *CPLDecToDMS( double dfAngle, const char * pszAxis,
                         int nPrecision )

{
    VALIDATE_POINTER1(pszAxis, "CPLDecToDMS", "");

    if( CPLIsNan(dfAngle) )
        return "Invalid angle";

    const double dfEpsilon = (0.5 / 3600.0) * pow(0.1, nPrecision);
    const double dfABSAngle = std::abs(dfAngle) + dfEpsilon;
    if( dfABSAngle > 361.0 )
    {
        return "Invalid angle";
    }

    const int nDegrees = static_cast<int>(dfABSAngle);
    const int nMinutes = static_cast<int>((dfABSAngle - nDegrees) * 60);
    double dfSeconds = dfABSAngle * 3600 - nDegrees * 3600 - nMinutes * 60;

    if( dfSeconds > dfEpsilon * 3600.0 )
        dfSeconds -= dfEpsilon * 3600.0;

    const char *pszHemisphere = nullptr;
    if( EQUAL(pszAxis, "Long") && dfAngle < 0.0 )
        pszHemisphere = "W";
    else if( EQUAL(pszAxis, "Long") )
        pszHemisphere = "E";
    else if( dfAngle < 0.0 )
        pszHemisphere = "S";
    else
        pszHemisphere = "N";

    char szFormat[30] = {};
    CPLsnprintf(szFormat, sizeof(szFormat),
                "%%3dd%%2d\'%%%d.%df\"%s",
                nPrecision+3, nPrecision, pszHemisphere);

    static CPL_THREADLOCAL char szBuffer[50] = {};
    CPLsnprintf(szBuffer, sizeof(szBuffer),
                szFormat,
                nDegrees, nMinutes, dfSeconds);

    return szBuffer;
}

/************************************************************************/
/*                         CPLPackedDMSToDec()                          */
/************************************************************************/

/**
 * Convert a packed DMS value (DDDMMMSSS.SS) into decimal degrees.
 *
 * This function converts a packed DMS angle to seconds. The standard
 * packed DMS format is:
 *
 *  degrees * 1000000 + minutes * 1000 + seconds
 *
 * Example:     angle = 120025045.25 yields
 *              deg = 120
 *              min = 25
 *              sec = 45.25
 *
 * The algorithm used for the conversion is as follows:
 *
 * 1.  The absolute value of the angle is used.
 *
 * 2.  The degrees are separated out:
 *     deg = angle/1000000                    (fractional portion truncated)
 *
 * 3.  The minutes are separated out:
 *     min = (angle - deg * 1000000) / 1000   (fractional portion truncated)
 *
 * 4.  The seconds are then computed:
 *     sec = angle - deg * 1000000 - min * 1000
 *
 * 5.  The total angle in seconds is computed:
 *     sec = deg * 3600.0 + min * 60.0 + sec
 *
 * 6.  The sign of sec is set to that of the input angle.
 *
 * Packed DMS values used by the USGS GCTP package and probably by other
 * software.
 *
 * NOTE: This code does not validate input value. If you give the wrong
 * value, you will get the wrong result.
 *
 * @param dfPacked Angle in packed DMS format.
 *
 * @return Angle in decimal degrees.
 *
 */

double CPLPackedDMSToDec( double dfPacked )
{
    const double dfSign = dfPacked < 0.0 ? -1 : 1;

    double dfSeconds = std::abs(dfPacked);
    double dfDegrees = floor(dfSeconds / 1000000.0);
    dfSeconds -= dfDegrees * 1000000.0;
    const double dfMinutes = floor(dfSeconds / 1000.0);
    dfSeconds -= dfMinutes * 1000.0;
    dfSeconds = dfSign * (dfDegrees * 3600.0 + dfMinutes * 60.0 + dfSeconds);
    dfDegrees = dfSeconds / 3600.0;

    return dfDegrees;
}

/************************************************************************/
/*                         CPLDecToPackedDMS()                          */
/************************************************************************/
/**
 * Convert decimal degrees into packed DMS value (DDDMMMSSS.SS).
 *
 * This function converts a value, specified in decimal degrees into
 * packed DMS angle. The standard packed DMS format is:
 *
 *  degrees * 1000000 + minutes * 1000 + seconds
 *
 * See also CPLPackedDMSToDec().
 *
 * @param dfDec Angle in decimal degrees.
 *
 * @return Angle in packed DMS format.
 *
 */

double CPLDecToPackedDMS( double dfDec )
{
    const double dfSign = dfDec < 0.0 ? -1 : 1;

    dfDec = std::abs(dfDec);
    const double dfDegrees = floor(dfDec);
    const double dfMinutes = floor((dfDec - dfDegrees) * 60.0);
    const double dfSeconds = (dfDec - dfDegrees) * 3600.0 - dfMinutes * 60.0;

    return dfSign * (dfDegrees * 1000000.0 + dfMinutes * 1000.0 + dfSeconds);
}

/************************************************************************/
/*                         CPLStringToComplex()                         */
/************************************************************************/

/** Fetch the real and imaginary part of a serialized complex number */
void CPL_DLL CPLStringToComplex( const char *pszString,
                                 double *pdfReal, double *pdfImag )

{
    while( *pszString == ' ' )
        pszString++;

    *pdfReal = CPLAtof(pszString);
    *pdfImag = 0.0;

    int iPlus = -1;
    int iImagEnd = -1;

    for( int i = 0;
         i < 100 && pszString[i] != '\0' && pszString[i] != ' ';
         i++ )
    {
        if( pszString[i] == '+' && i > 0 )
            iPlus = i;
        if( pszString[i] == '-' && i > 0 )
            iPlus = i;
        if( pszString[i] == 'i' )
            iImagEnd = i;
    }

    if( iPlus > -1 && iImagEnd > -1 && iPlus < iImagEnd )
    {
        *pdfImag = CPLAtof(pszString + iPlus);
    }

    return;
}

/************************************************************************/
/*                           CPLOpenShared()                            */
/************************************************************************/

/**
 * Open a shared file handle.
 *
 * Some operating systems have limits on the number of file handles that can
 * be open at one time.  This function attempts to maintain a registry of
 * already open file handles, and reuse existing ones if the same file
 * is requested by another part of the application.
 *
 * Note that access is only shared for access types "r", "rb", "r+" and
 * "rb+".  All others will just result in direct VSIOpen() calls.  Keep in
 * mind that a file is only reused if the file name is exactly the same.
 * Different names referring to the same file will result in different
 * handles.
 *
 * The VSIFOpen() or VSIFOpenL() function is used to actually open the file,
 * when an existing file handle can't be shared.
 *
 * @param pszFilename the name of the file to open.
 * @param pszAccess the normal fopen()/VSIFOpen() style access string.
 * @param bLargeIn If TRUE VSIFOpenL() (for large files) will be used instead of
 * VSIFOpen().
 *
 * @return a file handle or NULL if opening fails.
 */

FILE *CPLOpenShared( const char *pszFilename, const char *pszAccess,
                     int bLargeIn )

{
    const bool bLarge = CPL_TO_BOOL(bLargeIn);
    CPLMutexHolderD(&hSharedFileMutex);
    const GIntBig nPID = CPLGetPID();

/* -------------------------------------------------------------------- */
/*      Is there an existing file we can use?                           */
/* -------------------------------------------------------------------- */
    const bool bReuse = EQUAL(pszAccess, "rb") || EQUAL(pszAccess, "rb+");

    for( int i = 0; bReuse && i < nSharedFileCount; i++ )
    {
        if( strcmp(pasSharedFileList[i].pszFilename, pszFilename) == 0 &&
            !bLarge == !pasSharedFileList[i].bLarge &&
            EQUAL(pasSharedFileList[i].pszAccess, pszAccess) &&
            nPID == pasSharedFileListExtra[i].nPID)
        {
            pasSharedFileList[i].nRefCount++;
            return pasSharedFileList[i].fp;
        }
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    FILE *fp = bLarge
        ? reinterpret_cast<FILE *>(VSIFOpenL(pszFilename, pszAccess))
        : VSIFOpen(pszFilename, pszAccess);

    if( fp == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Add an entry to the list.                                       */
/* -------------------------------------------------------------------- */
    nSharedFileCount++;

    pasSharedFileList = static_cast<CPLSharedFileInfo *>(
        CPLRealloc(const_cast<CPLSharedFileInfo *>(pasSharedFileList),
                   sizeof(CPLSharedFileInfo) * nSharedFileCount));
    pasSharedFileListExtra = static_cast<CPLSharedFileInfoExtra *>(
        CPLRealloc(
            const_cast<CPLSharedFileInfoExtra *>(pasSharedFileListExtra),
            sizeof(CPLSharedFileInfoExtra) * nSharedFileCount));

    pasSharedFileList[nSharedFileCount - 1].fp = fp;
    pasSharedFileList[nSharedFileCount - 1].nRefCount = 1;
    pasSharedFileList[nSharedFileCount - 1].bLarge = bLarge;
    pasSharedFileList[nSharedFileCount - 1].pszFilename =
        CPLStrdup(pszFilename);
    pasSharedFileList[nSharedFileCount - 1].pszAccess = CPLStrdup(pszAccess);
    pasSharedFileListExtra[nSharedFileCount - 1].nPID = nPID;

    return fp;
}

/************************************************************************/
/*                           CPLCloseShared()                           */
/************************************************************************/

/**
 * Close shared file.
 *
 * Dereferences the indicated file handle, and closes it if the reference
 * count has dropped to zero.  A CPLError() is issued if the file is not
 * in the shared file list.
 *
 * @param fp file handle from CPLOpenShared() to deaccess.
 */

void CPLCloseShared( FILE * fp )

{
    CPLMutexHolderD(&hSharedFileMutex);

/* -------------------------------------------------------------------- */
/*      Search for matching information.                                */
/* -------------------------------------------------------------------- */
    int i = 0;
    for( ; i < nSharedFileCount && fp != pasSharedFileList[i].fp; i++ ) {}

    if( i == nSharedFileCount )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find file handle %p in CPLCloseShared().",
                 fp);
        return;
    }

/* -------------------------------------------------------------------- */
/*      Dereference and return if there are still some references.      */
/* -------------------------------------------------------------------- */
    if( --pasSharedFileList[i].nRefCount > 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Close the file, and remove the information.                     */
/* -------------------------------------------------------------------- */
    if( pasSharedFileList[i].bLarge )
    {
        if( VSIFCloseL(reinterpret_cast<VSILFILE *>(pasSharedFileList[i].fp)) !=
            0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Error while closing %s",
                     pasSharedFileList[i].pszFilename);
        }
    }
    else
    {
        VSIFClose(pasSharedFileList[i].fp);
    }

    CPLFree(pasSharedFileList[i].pszFilename);
    CPLFree(pasSharedFileList[i].pszAccess);

    nSharedFileCount--;
    memmove(
        const_cast<CPLSharedFileInfo *>(pasSharedFileList + i),
        const_cast<CPLSharedFileInfo *>(pasSharedFileList + nSharedFileCount),
        sizeof(CPLSharedFileInfo));
    memmove(
        const_cast<CPLSharedFileInfoExtra *>(pasSharedFileListExtra + i),
        const_cast<CPLSharedFileInfoExtra *>(pasSharedFileListExtra +
                                             nSharedFileCount),
        sizeof(CPLSharedFileInfoExtra));

    if( nSharedFileCount == 0 )
    {
        CPLFree(const_cast<CPLSharedFileInfo *>(pasSharedFileList));
        pasSharedFileList = nullptr;
        CPLFree(const_cast<CPLSharedFileInfoExtra *>(pasSharedFileListExtra));
        pasSharedFileListExtra = nullptr;
    }
}

/************************************************************************/
/*                   CPLCleanupSharedFileMutex()                        */
/************************************************************************/

void CPLCleanupSharedFileMutex()
{
    if( hSharedFileMutex != nullptr )
    {
        CPLDestroyMutex(hSharedFileMutex);
        hSharedFileMutex = nullptr;
    }
}

/************************************************************************/
/*                          CPLGetSharedList()                          */
/************************************************************************/

/**
 * Fetch list of open shared files.
 *
 * @param pnCount place to put the count of entries.
 *
 * @return the pointer to the first in the array of shared file info
 * structures.
 */

CPLSharedFileInfo *CPLGetSharedList( int *pnCount )

{
    if( pnCount != nullptr )
        *pnCount = nSharedFileCount;

    return const_cast<CPLSharedFileInfo *>(pasSharedFileList);
}

/************************************************************************/
/*                         CPLDumpSharedList()                          */
/************************************************************************/

/**
 * Report open shared files.
 *
 * Dumps all open shared files to the indicated file handle.  If the
 * file handle is NULL information is sent via the CPLDebug() call.
 *
 * @param fp File handle to write to.
 */

void CPLDumpSharedList( FILE *fp )

{
    if( nSharedFileCount > 0 )
    {
        if( fp == nullptr )
            CPLDebug("CPL", "%d Shared files open.", nSharedFileCount);
        else
            fprintf(fp, "%d Shared files open.", nSharedFileCount);
    }

    for( int i = 0; i < nSharedFileCount; i++ )
    {
        if( fp == nullptr )
            CPLDebug("CPL",
                     "%2d %d %4s %s",
                     pasSharedFileList[i].nRefCount,
                     pasSharedFileList[i].bLarge,
                     pasSharedFileList[i].pszAccess,
                     pasSharedFileList[i].pszFilename);
        else
            fprintf(fp, "%2d %d %4s %s",
                    pasSharedFileList[i].nRefCount,
                    pasSharedFileList[i].bLarge,
                    pasSharedFileList[i].pszAccess,
                    pasSharedFileList[i].pszFilename);
    }
}

/************************************************************************/
/*                           CPLUnlinkTree()                            */
/************************************************************************/

/** Recursively unlink a directory.
 *
 * @return 0 on successful completion, -1 if function fails.
 */

int CPLUnlinkTree( const char *pszPath )

{
/* -------------------------------------------------------------------- */
/*      First, ensure there is such a file.                             */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL(pszPath, &sStatBuf) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "It seems no file system object called '%s' exists.",
                 pszPath);

        return -1;
    }

/* -------------------------------------------------------------------- */
/*      If it is a simple file, just delete it.                         */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(sStatBuf.st_mode) )
    {
        if( VSIUnlink(pszPath) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to unlink %s.",
                     pszPath);

            return -1;
        }

        return 0;
    }

/* -------------------------------------------------------------------- */
/*      If it is a directory recurse then unlink the directory.         */
/* -------------------------------------------------------------------- */
    else if( VSI_ISDIR(sStatBuf.st_mode) )
    {
        char **papszItems = VSIReadDir(pszPath);

        for( int i = 0; papszItems != nullptr && papszItems[i] != nullptr; i++ )
        {
            if( papszItems[i][0] == '\0' ||
                EQUAL(papszItems[i], ".") || EQUAL(papszItems[i], "..") )
                continue;

            const std::string osSubPath =
                CPLFormFilename(pszPath, papszItems[i], nullptr);

            const int nErr = CPLUnlinkTree(osSubPath.c_str());

            if( nErr != 0 )
            {
                CSLDestroy(papszItems);
                return nErr;
            }
        }

        CSLDestroy(papszItems);

        if( VSIRmdir(pszPath) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to unlink %s.",
                     pszPath);

            return -1;
        }

        return 0;
    }

/* -------------------------------------------------------------------- */
/*      otherwise report an error.                                      */
/* -------------------------------------------------------------------- */
    CPLError(CE_Failure, CPLE_AppDefined,
             "Failed to unlink %s.\nUnrecognised filesystem object.",
             pszPath);
    return 1000;
}

/************************************************************************/
/*                            CPLCopyFile()                             */
/************************************************************************/

/** Copy a file */
int CPLCopyFile( const char *pszNewPath, const char *pszOldPath )

{
/* -------------------------------------------------------------------- */
/*      Open old and new file.                                          */
/* -------------------------------------------------------------------- */
    VSILFILE *fpOld = VSIFOpenL(pszOldPath, "rb");
    if( fpOld == nullptr )
        return -1;

    VSILFILE *fpNew = VSIFOpenL(pszNewPath, "wb");
    if( fpNew == nullptr )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpOld));
        return -1;
    }

/* -------------------------------------------------------------------- */
/*      Prepare buffer.                                                 */
/* -------------------------------------------------------------------- */
    const size_t nBufferSize = 1024 * 1024;
    GByte *pabyBuffer = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nBufferSize));
    if( pabyBuffer == nullptr )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpNew));
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpOld));
        return -1;
    }

/* -------------------------------------------------------------------- */
/*      Copy file over till we run out of stuff.                        */
/* -------------------------------------------------------------------- */
    size_t nBytesRead = 0;
    int nRet = 0;
    do
    {
        nBytesRead = VSIFReadL(pabyBuffer, 1, nBufferSize, fpOld);
        if( long(nBytesRead) < 0 )
            nRet = -1;

        if( nRet == 0 &&
            VSIFWriteL(pabyBuffer, 1, nBytesRead, fpNew) < nBytesRead )
            nRet = -1;
    } while( nRet == 0 && nBytesRead == nBufferSize );

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( VSIFCloseL(fpNew) != 0 )
        nRet = -1;
    CPL_IGNORE_RET_VAL(VSIFCloseL(fpOld));

    CPLFree(pabyBuffer);

    return nRet;
}

/************************************************************************/
/*                            CPLCopyTree()                             */
/************************************************************************/

/** Recursively copy a tree */
int CPLCopyTree( const char *pszNewPath, const char *pszOldPath )

{
    VSIStatBufL sStatBuf;
    if( VSIStatL(pszNewPath, &sStatBuf) == 0 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "It seems that a file system object called '%s' already exists.",
            pszNewPath);

        return -1;
    }

    if( VSIStatL(pszOldPath, &sStatBuf) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "It seems no file system object called '%s' exists.",
                 pszOldPath);

        return -1;
    }

    if( VSI_ISDIR(sStatBuf.st_mode) )
    {
        if( VSIMkdir(pszNewPath, 0755) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create directory '%s'.",
                     pszNewPath);

            return -1;
        }

        char **papszItems = VSIReadDir(pszOldPath);

        for( int i = 0; papszItems != nullptr && papszItems[i] != nullptr; i++ )
        {
            if( EQUAL(papszItems[i], ".") || EQUAL(papszItems[i], "..") )
                continue;

            const std::string osNewSubPath =
                CPLFormFilename(pszNewPath, papszItems[i], nullptr);
            const std::string osOldSubPath =
                CPLFormFilename(pszOldPath, papszItems[i], nullptr);

            const int nErr =
                CPLCopyTree(osNewSubPath.c_str(), osOldSubPath.c_str());


            if( nErr != 0 )
            {
                CSLDestroy(papszItems);
                return nErr;
            }
        }
        CSLDestroy(papszItems);

        return 0;
    }
    else if( VSI_ISREG(sStatBuf.st_mode) )
    {
        return CPLCopyFile(pszNewPath, pszOldPath);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unrecognized filesystem object : '%s'.",
                 pszOldPath);
        return -1;
    }
}

/************************************************************************/
/*                            CPLMoveFile()                             */
/************************************************************************/

/** Move a file */
int CPLMoveFile( const char *pszNewPath, const char *pszOldPath )

{
    if( VSIRename(pszOldPath, pszNewPath) == 0 )
        return 0;

    const int nRet = CPLCopyFile(pszNewPath, pszOldPath);

    if( nRet == 0 )
        VSIUnlink(pszOldPath);
    return nRet;
}

/************************************************************************/
/*                             CPLSymlink()                             */
/************************************************************************/

/** Create a symbolic link */
#ifdef WIN32
int CPLSymlink( const char *, const char *, CSLConstList ) { return -1; }
#else
int CPLSymlink( const char *pszOldPath,
                const char *pszNewPath,
                CSLConstList /* papszOptions */ )
{
    return symlink(pszOldPath, pszNewPath);
}
#endif

/************************************************************************/
/* ==================================================================== */
/*                              CPLLocaleC                              */
/* ==================================================================== */
/************************************************************************/

//! @cond Doxygen_Suppress
/************************************************************************/
/*                             CPLLocaleC()                             */
/************************************************************************/

CPLLocaleC::CPLLocaleC() :
    pszOldLocale(nullptr)
{
    if( CPLTestBool(CPLGetConfigOption("GDAL_DISABLE_CPLLOCALEC", "NO")) )
        return;

    pszOldLocale = CPLStrdup(CPLsetlocale(LC_NUMERIC, nullptr));
    if( EQUAL(pszOldLocale, "C")
        || EQUAL(pszOldLocale, "POSIX")
        || CPLsetlocale(LC_NUMERIC, "C") == nullptr )
    {
        CPLFree(pszOldLocale);
        pszOldLocale = nullptr;
    }
}

/************************************************************************/
/*                            ~CPLLocaleC()                             */
/************************************************************************/

CPLLocaleC::~CPLLocaleC()

{
    if( pszOldLocale == nullptr )
        return;

    CPLsetlocale(LC_NUMERIC, pszOldLocale);
    CPLFree(pszOldLocale);
}

/************************************************************************/
/*                        CPLThreadLocaleCPrivate                       */
/************************************************************************/

#ifdef HAVE_USELOCALE

class CPLThreadLocaleCPrivate
{
        locale_t nNewLocale;
        locale_t nOldLocale;

        CPL_DISALLOW_COPY_ASSIGN(CPLThreadLocaleCPrivate)

    public:
        CPLThreadLocaleCPrivate();
       ~CPLThreadLocaleCPrivate();
};

CPLThreadLocaleCPrivate::CPLThreadLocaleCPrivate():
    nNewLocale(newlocale(LC_NUMERIC_MASK, "C", nullptr)),
    nOldLocale(uselocale(nNewLocale))
{
}

CPLThreadLocaleCPrivate::~CPLThreadLocaleCPrivate()
{
    uselocale(nOldLocale);
    freelocale(nNewLocale);
}

#elif defined(_MSC_VER)

class CPLThreadLocaleCPrivate
{
        int   nOldValConfigThreadLocale;
        char *pszOldLocale;

        CPL_DISALLOW_COPY_ASSIGN(CPLThreadLocaleCPrivate)

    public:
        CPLThreadLocaleCPrivate();
       ~CPLThreadLocaleCPrivate();
};

CPLThreadLocaleCPrivate::CPLThreadLocaleCPrivate()
{
    nOldValConfigThreadLocale = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    pszOldLocale = setlocale(LC_NUMERIC, "C");
    if( pszOldLocale )
        pszOldLocale = CPLStrdup(pszOldLocale);
}

CPLThreadLocaleCPrivate::~CPLThreadLocaleCPrivate()
{
    if( pszOldLocale != nullptr )
    {
        setlocale(LC_NUMERIC, pszOldLocale);
        CPLFree(pszOldLocale);
    }
    _configthreadlocale(nOldValConfigThreadLocale);
}

#else

class CPLThreadLocaleCPrivate
{
        char *pszOldLocale;

        CPL_DISALLOW_COPY_ASSIGN(CPLThreadLocaleCPrivate)

    public:
        CPLThreadLocaleCPrivate();
       ~CPLThreadLocaleCPrivate();
};

CPLThreadLocaleCPrivate::CPLThreadLocaleCPrivate():
    pszOldLocale(CPLStrdup(CPLsetlocale(LC_NUMERIC, nullptr)))
{
    if( EQUAL(pszOldLocale, "C")
        || EQUAL(pszOldLocale, "POSIX")
        || CPLsetlocale(LC_NUMERIC, "C") == nullptr )
    {
        CPLFree(pszOldLocale);
        pszOldLocale = nullptr;
    }
}

CPLThreadLocaleCPrivate::~CPLThreadLocaleCPrivate()
{
    if( pszOldLocale != nullptr )
    {
        CPLsetlocale(LC_NUMERIC, pszOldLocale);
        CPLFree(pszOldLocale);
    }
}

#endif


/************************************************************************/
/*                        CPLThreadLocaleC()                            */
/************************************************************************/

CPLThreadLocaleC::CPLThreadLocaleC():
    m_private(new CPLThreadLocaleCPrivate)
{
}

/************************************************************************/
/*                       ~CPLThreadLocaleC()                            */
/************************************************************************/

CPLThreadLocaleC::~CPLThreadLocaleC()

{
    delete m_private;
}
//! @endcond

/************************************************************************/
/*                          CPLsetlocale()                              */
/************************************************************************/

/**
 * Prevents parallel executions of setlocale().
 *
 * Calling setlocale() concurrently from two or more threads is a
 * potential data race. A mutex is used to provide a critical region so
 * that only one thread at a time can be executing setlocale().
 *
 * The return should not be freed, and copied quickly as it may be invalidated
 * by a following next call to CPLsetlocale().
 *
 * @param category See your compiler's documentation on setlocale.
 * @param locale See your compiler's documentation on setlocale.
 *
 * @return See your compiler's documentation on setlocale.
 */
char *CPLsetlocale (int category, const char *locale)
{
    CPLMutexHolder oHolder(&hSetLocaleMutex);
    char *pszRet = setlocale(category, locale);
    if( pszRet == nullptr )
        return pszRet;

    // Make it thread-locale storage.
    return const_cast<char *>(CPLSPrintf("%s", pszRet));
}

/************************************************************************/
/*                       CPLCleanupSetlocaleMutex()                     */
/************************************************************************/

void CPLCleanupSetlocaleMutex(void)
{
    if( hSetLocaleMutex != nullptr )
        CPLDestroyMutex(hSetLocaleMutex);
    hSetLocaleMutex = nullptr;
}

/************************************************************************/
/*                            IsPowerOfTwo()                            */
/************************************************************************/

int CPLIsPowerOfTwo( unsigned int i )
{
    if( i == 0 )
        return FALSE;
    return ( i & ( i - 1 ) ) == 0 ? TRUE : FALSE;
}

/************************************************************************/
/*                          CPLCheckForFile()                           */
/************************************************************************/

/**
 * Check for file existence.
 *
 * The function checks if a named file exists in the filesystem, hopefully
 * in an efficient fashion if a sibling file list is available.   It exists
 * primarily to do faster file checking for functions like GDAL open methods
 * that get a list of files from the target directory.
 *
 * If the sibling file list exists (is not NULL) it is assumed to be a list
 * of files in the same directory as the target file, and it will be checked
 * (case insensitively) for a match.  If a match is found, pszFilename is
 * updated with the correct case and TRUE is returned.
 *
 * If papszSiblingFiles is NULL, a VSIStatL() is used to test for the files
 * existence, and no case insensitive testing is done.
 *
 * @param pszFilename name of file to check for - filename case updated in
 * some cases.
 * @param papszSiblingFiles a list of files in the same directory as
 * pszFilename if available, or NULL. This list should have no path components.
 *
 * @return TRUE if a match is found, or FALSE if not.
 */

int CPLCheckForFile( char *pszFilename, char **papszSiblingFiles )

{
/* -------------------------------------------------------------------- */
/*      Fallback case if we don't have a sibling file list.             */
/* -------------------------------------------------------------------- */
    if( papszSiblingFiles == nullptr )
    {
        VSIStatBufL sStatBuf;

        return VSIStatL(pszFilename, &sStatBuf) == 0;
    }

/* -------------------------------------------------------------------- */
/*      We have sibling files, compare the non-path filename portion    */
/*      of pszFilename too all entries.                                 */
/* -------------------------------------------------------------------- */
    const CPLString osFileOnly = CPLGetFilename(pszFilename);

    for( int i = 0; papszSiblingFiles[i] != nullptr; i++ )
    {
        if( EQUAL(papszSiblingFiles[i], osFileOnly) )
        {
            strcpy( pszFilename + strlen(pszFilename) - osFileOnly.size(),
                    papszSiblingFiles[i] );
            return TRUE;
        }
    }

    return FALSE;
}

/************************************************************************/
/*      Stub implementation of zip services if we don't have libz.      */
/************************************************************************/

#if !defined(HAVE_LIBZ)

void *CPLCreateZip( const char *, char ** )

{
    CPLError(CE_Failure, CPLE_NotSupported,
             "This GDAL/OGR build does not include zlib and zip services.");
    return nullptr;
}

CPLErr CPLCreateFileInZip(void *, const char *, char **) { return CE_Failure; }

CPLErr CPLWriteFileInZip(void *, const void *, int) { return CE_Failure; }

CPLErr CPLCloseFileInZip(void *) { return CE_Failure; }

CPLErr CPLCloseZip(void *) { return CE_Failure; }

void* CPLZLibDeflate( const void *, size_t, int,
                      void *, size_t,
                      size_t *pnOutBytes )
{
    if( pnOutBytes != nullptr )
        *pnOutBytes = 0;
    return nullptr;
}

void *CPLZLibInflate( const void *, size_t, void *, size_t, size_t *pnOutBytes )
{
    if( pnOutBytes != nullptr )
        *pnOutBytes = 0;
    return nullptr;
}

#endif /* !defined(HAVE_LIBZ) */

/************************************************************************/
/* ==================================================================== */
/*                          CPLConfigOptionSetter                       */
/* ==================================================================== */
/************************************************************************/

//! @cond Doxygen_Suppress
/************************************************************************/
/*                         CPLConfigOptionSetter()                      */
/************************************************************************/

CPLConfigOptionSetter::CPLConfigOptionSetter(
                        const char* pszKey, const char* pszValue,
                        bool bSetOnlyIfUndefined ) :
    m_pszKey(CPLStrdup(pszKey)),
    m_pszOldValue(nullptr),
    m_bRestoreOldValue(false)
{
    const char* pszOldValue = CPLGetThreadLocalConfigOption(pszKey, nullptr);
    if( (bSetOnlyIfUndefined && CPLGetConfigOption(pszKey, nullptr) == nullptr) || !bSetOnlyIfUndefined )
    {
        m_bRestoreOldValue = true;
        if( pszOldValue )
            m_pszOldValue = CPLStrdup(pszOldValue);
        CPLSetThreadLocalConfigOption(pszKey, pszValue);
    }
}

/************************************************************************/
/*                        ~CPLConfigOptionSetter()                      */
/************************************************************************/

CPLConfigOptionSetter::~CPLConfigOptionSetter()
{
    if( m_bRestoreOldValue )
    {
        CPLSetThreadLocalConfigOption(m_pszKey, m_pszOldValue);
        CPLFree(m_pszOldValue);
    }
    CPLFree(m_pszKey);
}
//! @endcond

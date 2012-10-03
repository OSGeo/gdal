/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Convenience functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
#ifdef MSVC_USE_VLD
#include <vld.h>
#endif
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

#if defined(WIN32CE)
#  include "cpl_wince.h"
#endif

static void *hConfigMutex = NULL;
static volatile char **papszConfigOptions = NULL;

/* Used by CPLOpenShared() and friends */
static void *hSharedFileMutex = NULL;
static volatile int nSharedFileCount = 0;
static volatile CPLSharedFileInfo *pasSharedFileList = NULL;

/* Note: ideally this should be added in CPLSharedFileInfo* */
/* but CPLSharedFileInfo is exposed in the API, hence that trick */
/* to hide this detail */
typedef struct
{
    GIntBig             nPID; // pid of opening thread
} CPLSharedFileInfoExtra;

static volatile CPLSharedFileInfoExtra *pasSharedFileListExtra = NULL;

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
    void        *pReturn;

    if( nSize * nCount == 0 )
        return NULL;
    
    pReturn = CPLMalloc( nCount * nSize );
    memset( pReturn, 0, nCount * nSize );
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
    void        *pReturn;

    CPLVerifyConfiguration();

    if( nSize == 0 )
        return NULL;

    if( long(nSize) < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "CPLMalloc(%ld): Silly size requested.\n",
                  (long) nSize );
        return NULL;
    }
    
    pReturn = VSIMalloc( nSize );
    if( pReturn == NULL )
    {
        if( nSize > 0 && nSize < 2000 )
        {
            char szSmallMsg[60];

            sprintf( szSmallMsg, 
                     "CPLMalloc(): Out of memory allocating %ld bytes.", 
                     (long) nSize );
            CPLEmergencyError( szSmallMsg ); 
        }
        else
            CPLError( CE_Fatal, CPLE_OutOfMemory,
                      "CPLMalloc(): Out of memory allocating %ld bytes.\n",
                      (long) nSize );
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
    void        *pReturn;

    if ( nNewSize == 0 )
    {
        VSIFree(pData);
        return NULL;
    }

    if( long(nNewSize) < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "CPLRealloc(%ld): Silly size requested.\n",
                  (long) nNewSize );
        return NULL;
    }
    
    if( pData == NULL )
        pReturn = VSIMalloc( nNewSize );
    else
        pReturn = VSIRealloc( pData, nNewSize );
    
    if( pReturn == NULL )
    {
        if( nNewSize > 0 && nNewSize < 2000 )
        {
            char szSmallMsg[60];

            sprintf( szSmallMsg, 
                     "CPLRealloc(): Out of memory allocating %ld bytes.", 
                     (long) nNewSize );
            CPLEmergencyError( szSmallMsg ); 
        }
        else
            CPLError( CE_Fatal, CPLE_OutOfMemory,
                      "CPLRealloc(): Out of memory allocating %ld bytes.\n",
                      (long) nNewSize );
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
 * CPLError() instead of returning NULL.  It uses VSIStrdup(), so any
 * hooking of that function will apply to CPLStrdup() as well.  Memory
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
    char        *pszReturn;

    if( pszString == NULL )
        pszString = "";

    pszReturn = (char *) CPLMalloc(strlen(pszString)+1);
    if( pszReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLStrdup(): Out of memory allocating %ld bytes.\n",
                  (long) strlen(pszString) );
        
    }

    strcpy( pszReturn, pszString );
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
    if (pszString)
    {
        char *pszTemp = pszString;

        while (*pszTemp)
        {
            *pszTemp = (char) tolower (*pszTemp);
            pszTemp++;
        }
    }

    return pszString;
}

/************************************************************************/
/*                              CPLFGets()                              */
/*                                                                      */
/*      Note: CR = \r = ASCII 13                                        */
/*            LF = \n = ASCII 10                                        */
/************************************************************************/

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
 * termonating '\\0').
 * @param fp file pointer to read from.
 * @return pointer to the pszBuffer containing a string read
 * from the file or NULL if the error or end of file was encountered.
 */

char *CPLFGets( char *pszBuffer, int nBufferSize, FILE * fp )

{
    int nActuallyRead, nOriginalOffset;

    if ( nBufferSize == 0 || pszBuffer == NULL || fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Let the OS level call read what it things is one line.  This    */
/*      will include the newline.  On windows, if the file happens      */
/*      to be in text mode, the CRLF will have been converted to        */
/*      just the newline (LF).  If it is in binary mode it may well     */
/*      have both.                                                      */
/* -------------------------------------------------------------------- */
    nOriginalOffset = VSIFTell( fp );
    if( VSIFGets( pszBuffer, nBufferSize, fp ) == NULL )
        return NULL;
    
    nActuallyRead = strlen(pszBuffer);
    if ( nActuallyRead == 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we found \r and out buffer is full, it is possible there     */
/*      is also a pending \n.  Check for it.                            */
/* -------------------------------------------------------------------- */
    if( nBufferSize == nActuallyRead+1
        && pszBuffer[nActuallyRead-1] == 13 )
    {
        int chCheck;
        chCheck = fgetc( fp );
        if( chCheck != 10 )
        {
            // unget the character.
            VSIFSeek( fp, nOriginalOffset+nActuallyRead, SEEK_SET );
        }
    }

/* -------------------------------------------------------------------- */
/*      Trim off \n, \r or \r\n if it appears at the end.  We don't     */
/*      need to do any "seeking" since we want the newline eaten.       */
/* -------------------------------------------------------------------- */
    if( nActuallyRead > 1 
        && pszBuffer[nActuallyRead-1] == 10 
        && pszBuffer[nActuallyRead-2] == 13 )
    {
        pszBuffer[nActuallyRead-2] = '\0';
    }
    else if( pszBuffer[nActuallyRead-1] == 10 
             || pszBuffer[nActuallyRead-1] == 13 )
    {
        pszBuffer[nActuallyRead-1] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Search within the string for a \r (MacOS convention             */
/*      apparently), and if we find it we need to trim the string,      */
/*      and seek back.                                                  */
/* -------------------------------------------------------------------- */
    char *pszExtraNewline = strchr( pszBuffer, 13 );
    
    if( pszExtraNewline != NULL )
    {
        int chCheck;

        nActuallyRead = pszExtraNewline - pszBuffer + 1;
        
        *pszExtraNewline = '\0';
        VSIFSeek( fp, nOriginalOffset + nActuallyRead - 1, SEEK_SET );

        /* 
         * This hackery is necessary to try and find our correct
         * spot on win32 systems with text mode line translation going 
         * on.  Sometimes the fseek back overshoots, but it doesn't
         * "realize it" till a character has been read. Try to read till
         * we get to the right spot and get our CR. 
         */ 
        chCheck = fgetc( fp );
        while( (chCheck != 13 && chCheck != EOF)
               || VSIFTell(fp) < nOriginalOffset + nActuallyRead )
        {
            static volatile int bWarned = FALSE;

            if( !bWarned )
            {
                bWarned = TRUE;
                CPLDebug( "CPL", "CPLFGets() correcting for DOS text mode translation seek problem." );
            }
            chCheck = fgetc( fp );
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
        if( CPLGetTLS( CTLS_RLBUFFERINFO ) != NULL )
        {
            CPLFree( CPLGetTLS( CTLS_RLBUFFERINFO ) );
            CPLSetTLS( CTLS_RLBUFFERINFO, NULL, FALSE );
        }
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the buffer doesn't exist yet, create it.                     */
/* -------------------------------------------------------------------- */
    GUInt32 *pnAlloc = (GUInt32 *) CPLGetTLS( CTLS_RLBUFFERINFO );

    if( pnAlloc == NULL )
    {
        pnAlloc = (GUInt32 *) CPLMalloc(200);
        *pnAlloc = 196;
        CPLSetTLS( CTLS_RLBUFFERINFO, pnAlloc, TRUE );
    }

/* -------------------------------------------------------------------- */
/*      If it is too small, grow it bigger.                             */
/* -------------------------------------------------------------------- */
    if( ((int) *pnAlloc) -1 < nRequiredSize )
    {
        int nNewSize = nRequiredSize + 4 + 500;
        if (nNewSize <= 0)
        {
            VSIFree( pnAlloc );
            CPLSetTLS( CTLS_RLBUFFERINFO, NULL, FALSE );
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "CPLReadLineBuffer(): Trying to allocate more than 2 GB." );
            return NULL;
        }

        GUInt32* pnAllocNew = (GUInt32 *) VSIRealloc(pnAlloc,nNewSize);
        if( pnAllocNew == NULL )
        {
            VSIFree( pnAlloc );
            CPLSetTLS( CTLS_RLBUFFERINFO, NULL, FALSE );
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "CPLReadLineBuffer(): Out of memory allocating %ld bytes.",
                      (long) nNewSize );
            return NULL;
        }
        pnAlloc = pnAllocNew;
            
        *pnAlloc = nNewSize - 4;
        CPLSetTLS( CTLS_RLBUFFERINFO, pnAlloc, TRUE );
    }

    return (char *) (pnAlloc+1);
}

/************************************************************************/
/*                            CPLReadLine()                             */
/************************************************************************/

/**
 * Simplified line reading from text file.
 * 
 * Read a line of text from the given file handle, taking care
 * to capture CR and/or LF and strip off ... equivelent of
 * DKReadLine().  Pointer to an internal buffer is returned.
 * The application shouldn't free it, or depend on it's value
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

const char *CPLReadLine( FILE * fp )

{
    char *pszRLBuffer = CPLReadLineBuffer(1);
    int         nReadSoFar = 0;

/* -------------------------------------------------------------------- */
/*      Cleanup case.                                                   */
/* -------------------------------------------------------------------- */
    if( fp == NULL )
    {
        CPLReadLineBuffer( -1 );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop reading chunks of the line till we get to the end of       */
/*      the line.                                                       */
/* -------------------------------------------------------------------- */
    int nBytesReadThisTime;

    do {
/* -------------------------------------------------------------------- */
/*      Grow the working buffer if we have it nearly full.  Fail out    */
/*      of read line if we can't reallocate it big enough (for          */
/*      instance for a _very large_ file with no newlines).             */
/* -------------------------------------------------------------------- */
        pszRLBuffer = CPLReadLineBuffer( nReadSoFar + 129 );
        if( pszRLBuffer == NULL )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Do the actual read.                                             */
/* -------------------------------------------------------------------- */
        if( CPLFGets( pszRLBuffer+nReadSoFar, 128, fp ) == NULL 
            && nReadSoFar == 0 )
            return NULL;

        nBytesReadThisTime = strlen(pszRLBuffer+nReadSoFar);
        nReadSoFar += nBytesReadThisTime;

    } while( nBytesReadThisTime >= 127
             && pszRLBuffer[nReadSoFar-1] != 13
             && pszRLBuffer[nReadSoFar-1] != 10 );

    return( pszRLBuffer );
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

const char *CPLReadLineL( VSILFILE * fp )
{
    return CPLReadLine2L( fp, -1, NULL );
}

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
 * number of characters allowed readched.
 *
 * @since GDAL 1.7.0
 */

const char *CPLReadLine2L( VSILFILE * fp, int nMaxCars, char** papszOptions )

{
    (void) papszOptions;

/* -------------------------------------------------------------------- */
/*      Cleanup case.                                                   */
/* -------------------------------------------------------------------- */
    if( fp == NULL )
    {
        CPLReadLineBuffer( -1 );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop reading chunks of the line till we get to the end of       */
/*      the line.                                                       */
/* -------------------------------------------------------------------- */
    char *pszRLBuffer;
    const size_t nChunkSize = 40;
    char szChunk[nChunkSize];
    size_t nChunkBytesRead = 0;
    int nBufLength = 0;
    size_t nChunkBytesConsumed = 0;

    while( TRUE )
    {
/* -------------------------------------------------------------------- */
/*      Read a chunk from the input file.                               */
/* -------------------------------------------------------------------- */
        if ( nBufLength > INT_MAX - nChunkSize - 1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Too big line : more than 2 billion characters!." );
            CPLReadLineBuffer( -1 );
            return NULL;
        }

        pszRLBuffer = CPLReadLineBuffer( nBufLength + nChunkSize + 1 );
        if( pszRLBuffer == NULL )
            return NULL;

        if( nChunkBytesRead == nChunkBytesConsumed + 1 )
        {

            // case where one character is left over from last read.
            szChunk[0] = szChunk[nChunkBytesConsumed];

            nChunkBytesConsumed = 0;
            nChunkBytesRead = VSIFReadL( szChunk+1, 1, nChunkSize-1, fp ) + 1;
        }
        else
        {
            nChunkBytesConsumed = 0;

            // fresh read.
            nChunkBytesRead = VSIFReadL( szChunk, 1, nChunkSize, fp );
            if( nChunkBytesRead == 0 )
            {
                if( nBufLength == 0 )
                    return NULL;
                else
                    break;
            }
        }
        
/* -------------------------------------------------------------------- */
/*      copy over characters watching for end-of-line.                  */
/* -------------------------------------------------------------------- */
        int bBreak = FALSE;
        while( nChunkBytesConsumed < nChunkBytesRead-1 && !bBreak )
        {
            if( (szChunk[nChunkBytesConsumed] == 13
                 && szChunk[nChunkBytesConsumed+1] == 10)
                || (szChunk[nChunkBytesConsumed] == 10
                    && szChunk[nChunkBytesConsumed+1] == 13) )
            {
                nChunkBytesConsumed += 2;
                bBreak = TRUE;
            }
            else if( szChunk[nChunkBytesConsumed] == 10
                     || szChunk[nChunkBytesConsumed] == 13 )
            {
                nChunkBytesConsumed += 1;
                bBreak = TRUE;
            }
            else
            {
                pszRLBuffer[nBufLength++] = szChunk[nChunkBytesConsumed++];
                if (nMaxCars >= 0 && nBufLength == nMaxCars)
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                             "Maximum number of characters allowed reached.");
                    return NULL;
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
        if( nChunkBytesConsumed == nChunkBytesRead-1 
            && nChunkBytesRead < nChunkSize )
        {
            if( szChunk[nChunkBytesConsumed] == 10
                || szChunk[nChunkBytesConsumed] == 13 )
            {
                nChunkBytesConsumed++;
                break;
            }

            pszRLBuffer[nBufLength++] = szChunk[nChunkBytesConsumed++];
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have left over bytes after breaking out, seek back to     */
/*      ensure they remain to be read next time.                        */
/* -------------------------------------------------------------------- */
    if( nChunkBytesConsumed < nChunkBytesRead )
    {
        size_t nBytesToPush = nChunkBytesRead - nChunkBytesConsumed;
        
        VSIFSeekL( fp, VSIFTellL( fp ) - nBytesToPush, SEEK_SET );
    }

    pszRLBuffer[nBufLength] = '\0';

    return( pszRLBuffer );
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
    char    *pszBuffer;

    if ( !pszString )
        return NULL;

    if ( !nMaxLength )
        return CPLStrdup( "" );

    pszBuffer = (char *)CPLMalloc( nMaxLength + 1 );
    if ( !pszBuffer )
        return NULL;

    strncpy( pszBuffer, pszString,  nMaxLength );
    pszBuffer[nMaxLength] = '\0';

    if ( bTrimSpaces )
    {
        size_t  i = strlen( pszBuffer );
        while ( i-- > 0 && isspace((unsigned char)pszBuffer[i]) )
            pszBuffer[i] = '\0';
    }

    if ( bNormalize )
    {
        size_t  i = strlen( pszBuffer );
        while ( i-- > 0 )
        {
            if ( pszBuffer[i] == ':' )
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
    long    iValue;
    char    *pszValue = (char *)CPLMalloc( nMaxLength + 1);

/* -------------------------------------------------------------------- */
/*      Compute string into local buffer, and terminate it.             */
/* -------------------------------------------------------------------- */
    strncpy( pszValue, pszString, nMaxLength );
    pszValue[nMaxLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Use atol() to fetch out the result                              */
/* -------------------------------------------------------------------- */
    iValue = atol( pszValue );

    CPLFree( pszValue );
    return iValue;
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
    unsigned long    uValue;
    char    *pszValue = (char *)CPLMalloc( nMaxLength + 1);

/* -------------------------------------------------------------------- */
/*      Compute string into local buffer, and terminate it.             */
/* -------------------------------------------------------------------- */
    strncpy( pszValue, pszString, nMaxLength );
    pszValue[nMaxLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Use strtoul() to fetch out the result                           */
/* -------------------------------------------------------------------- */
    uValue = strtoul( pszValue, NULL, 10 );

    CPLFree( pszValue );
    return uValue;
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
    GUIntBig    iValue;
    char        *pszValue = (char *)CPLMalloc( nMaxLength + 1);

/* -------------------------------------------------------------------- */
/*      Compute string into local buffer, and terminate it.             */
/* -------------------------------------------------------------------- */
    strncpy( pszValue, pszString, nMaxLength );
    pszValue[nMaxLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Fetch out the result                                            */
/* -------------------------------------------------------------------- */
#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
    iValue = (GUIntBig)_atoi64( pszValue );
# elif HAVE_ATOLL
    iValue = atoll( pszValue );
#else
    iValue = atol( pszValue );
#endif

    CPLFree( pszValue );
    return iValue;
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
    void  *pResult;
    char  szTemp[128];

/* -------------------------------------------------------------------- */
/*      Compute string into local buffer, and terminate it.             */
/* -------------------------------------------------------------------- */
    if( nMaxLength > (int) sizeof(szTemp)-1 )
        nMaxLength = sizeof(szTemp)-1;

    strncpy( szTemp, pszString, nMaxLength );
    szTemp[nMaxLength] = '\0';

/* -------------------------------------------------------------------- */
/*      On MSVC we have to scanf pointer values without the 0x          */
/*      prefix.                                                         */
/* -------------------------------------------------------------------- */
    if( EQUALN(szTemp,"0x",2) )
    {
        pResult = NULL;

#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
        sscanf( szTemp+2, "%p", &pResult );
#else
        sscanf( szTemp, "%p", &pResult );

        /* Solaris actually behaves like MSVCRT... */
        if (pResult == NULL)
        {
            sscanf( szTemp+2, "%p", &pResult );
        }
#endif
    }
    
    else
    {
#if SIZEOF_VOIDP == 8
        pResult = (void *) CPLScanUIntBig( szTemp, nMaxLength );
#else
        pResult = (void *) CPLScanULong( szTemp, nMaxLength );
#endif
    }

    return pResult;
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
    int     i;
    double  dfValue;
    char    *pszValue = (char *)CPLMalloc( nMaxLength + 1);

/* -------------------------------------------------------------------- */
/*      Compute string into local buffer, and terminate it.             */
/* -------------------------------------------------------------------- */
    strncpy( pszValue, pszString, nMaxLength );
    pszValue[nMaxLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Make a pass through converting 'D's to 'E's.                    */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nMaxLength; i++ )
        if ( pszValue[i] == 'd' || pszValue[i] == 'D' )
            pszValue[i] = 'E';

/* -------------------------------------------------------------------- */
/*      The conversion itself.                                          */
/* -------------------------------------------------------------------- */
    dfValue = CPLAtof( pszValue );

    CPLFree( pszValue );
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
    char    *pszTemp = pszDest;
    int     nChars = 0;

    if ( !pszDest )
        return 0;

    if ( !pszSrc )
    {
        *pszDest = '\0';
        return 1;
    }

    while ( nChars < nMaxLen && *pszSrc )
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
    char    *pszTemp = pszDest;

    if ( !pszDest )
        return 0;

    if ( !pszSrc )
    {
        memset( pszDest, ' ', nMaxLen );
        return nMaxLen;
    }

    while ( nMaxLen && *pszSrc )
    {
        *pszTemp++ = *pszSrc++;
        nMaxLen--;
    }

    if ( nMaxLen )
        memset( pszTemp, ' ', nMaxLen );

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
    char    szTemp[64];

    if ( !pszBuffer )
        return 0;

    if ( nMaxLen >= 64 )
        nMaxLen = 63;

#if UINT_MAX == 65535
    sprintf( szTemp, "%*ld", nMaxLen, iValue );
#else
    sprintf( szTemp, "%*d", nMaxLen, iValue );
#endif

    return CPLPrintString( pszBuffer, szTemp, nMaxLen );
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
    char    szTemp[64];

    if ( !pszBuffer )
        return 0;

    if ( nMaxLen >= 64 )
        nMaxLen = 63;

#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
    sprintf( szTemp, "%*I64d", nMaxLen, iValue );
# elif HAVE_LONG_LONG
    sprintf( szTemp, "%*lld", nMaxLen, (long long) iValue );
//    sprintf( szTemp, "%*Ld", nMaxLen, (long long) iValue );
#else
    sprintf( szTemp, "%*ld", nMaxLen, iValue );
#endif

    return CPLPrintString( pszBuffer, szTemp, nMaxLen );
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
    char    szTemp[64];

    if ( !pszBuffer )
        return 0;

    if ( nMaxLen >= 64 )
        nMaxLen = 63;

    sprintf( szTemp, "%p", pValue );

    // On windows, and possibly some other platforms the sprintf("%p")
    // does not prefix things with 0x so it is hard to know later if the
    // value is hex encoded.  Fix this up here. 

    if( !EQUALN(szTemp,"0x",2) )
        sprintf( szTemp, "0x%p", pValue );

    return CPLPrintString( pszBuffer, szTemp, nMaxLen );
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
 * @param pszLocale Pointer to a character string containing locale name
 * ("C", "POSIX", "us_US", "ru_RU.KOI8-R" etc.). If NULL we will not
 * manipulate with locale settings and current process locale will be used for
 * printing. With the pszLocale option we can control what exact locale
 * will be used for printing a numeric value to the string (in most cases
 * it should be C/POSIX).
 *
 * @return Number of characters printed.
 */

int CPLPrintDouble( char *pszBuffer, const char *pszFormat,
                    double dfValue, const char *pszLocale )
{

#define DOUBLE_BUFFER_SIZE 64

    char    szTemp[DOUBLE_BUFFER_SIZE];
    int     i;

    if ( !pszBuffer )
        return 0;

#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
    char        *pszCurLocale = NULL;

    if ( pszLocale || EQUAL( pszLocale, "" ) )
    {
        // Save the current locale
        pszCurLocale = setlocale(LC_ALL, NULL );
        // Set locale to the specified value
        setlocale( LC_ALL, pszLocale );
    }
#else
    (void) pszLocale;
#endif

#if defined(HAVE_SNPRINTF)
    snprintf( szTemp, DOUBLE_BUFFER_SIZE, pszFormat, dfValue );
#else
    sprintf( szTemp, pszFormat, dfValue );
#endif
    szTemp[DOUBLE_BUFFER_SIZE - 1] = '\0';

    for( i = 0; szTemp[i] != '\0'; i++ )
    {
        if( szTemp[i] == 'E' || szTemp[i] == 'e' )
            szTemp[i] = 'D';
    }

#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
    // Restore stored locale back
    if ( pszCurLocale )
        setlocale( LC_ALL, pszCurLocale );
#endif

    return CPLPrintString( pszBuffer, szTemp, 64 );

#undef DOUBLE_BUFFER_SIZE

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
 * as well as time format settings also may be ajusted differently from the
 * C/POSIX defaults. To solve these problems this option was introdiced.
 *
 * @return Number of characters printed.
 */

#ifndef WIN32CE /* XXX - mloskot - strftime is not available yet. */

int CPLPrintTime( char *pszBuffer, int nMaxLen, const char *pszFormat,
                  const struct tm *poBrokenTime, const char *pszLocale )
{
    char    *pszTemp = (char *)CPLMalloc( (nMaxLen + 1) * sizeof(char) );
    int     nChars;

#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
    char        *pszCurLocale = NULL;

    if ( pszLocale || EQUAL( pszLocale, "" ) )
    {
        // Save the current locale
        pszCurLocale = setlocale(LC_ALL, NULL );
        // Set locale to the specified value
        setlocale( LC_ALL, pszLocale );
    }
#else
    (void) pszLocale;
#endif
    
    if ( !strftime( pszTemp, nMaxLen + 1, pszFormat, poBrokenTime ) )
        memset( pszTemp, 0, nMaxLen + 1);

#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
    // Restore stored locale back
    if ( pszCurLocale )
        setlocale( LC_ALL, pszCurLocale );
#endif

    nChars = CPLPrintString( pszBuffer, pszTemp, nMaxLen );

    CPLFree( pszTemp );

    return nChars;
}

#endif

/************************************************************************/
/*                       CPLVerifyConfiguration()                       */
/************************************************************************/

void CPLVerifyConfiguration()

{
/* -------------------------------------------------------------------- */
/*      Verify data types.                                              */
/* -------------------------------------------------------------------- */
    CPLAssert( sizeof(GInt32) == 4 );
    CPLAssert( sizeof(GInt16) == 2 );
    CPLAssert( sizeof(GByte) == 1 );

    if( sizeof(GInt32) != 4 )
        CPLError( CE_Fatal, CPLE_AppDefined, 
                  "sizeof(GInt32) == %d ... yow!\n", 
                  (int) sizeof(GInt32) );

/* -------------------------------------------------------------------- */
/*      Verify byte order                                               */
/* -------------------------------------------------------------------- */
    GInt32   nTest;

    nTest = 1;

#ifdef CPL_LSB
    if( ((GByte *) &nTest)[0] != 1 )
#endif
#ifdef CPL_MSB
    if( ((GByte *) &nTest)[3] != 1 )
#endif    
        CPLError( CE_Fatal, CPLE_AppDefined, 
                  "CPLVerifyConfiguration(): byte order set wrong.\n" );
}

/* Uncomment to get list of options that have been fetched and set */
//#define DEBUG_CONFIG_OPTIONS

#ifdef DEBUG_CONFIG_OPTIONS

#include <set>
#include "cpl_multiproc.h"

static void* hRegisterConfigurationOptionMutex = 0;
static std::set<CPLString>* paoGetKeys = NULL;
static std::set<CPLString>* paoSetKeys = NULL;

/************************************************************************/
/*                      CPLShowAccessedOptions()                        */
/************************************************************************/

static void CPLShowAccessedOptions()
{
    std::set<CPLString>::iterator aoIter;

    printf("Configuration options accessed in reading : "),
    aoIter = paoGetKeys->begin();
    while(aoIter != paoGetKeys->end())
    {
        printf("%s, ", (*aoIter).c_str());
        aoIter ++;
    }
    printf("\n");

    printf("Configuration options accessed in writing : "),
    aoIter = paoSetKeys->begin();
    while(aoIter != paoSetKeys->end())
    {
        printf("%s, ", (*aoIter).c_str());
        aoIter ++;
    }
    printf("\n");

    delete paoGetKeys;
    delete paoSetKeys;
    paoGetKeys = paoSetKeys = NULL;
}

/************************************************************************/
/*                       CPLAccessConfigOption()                        */
/************************************************************************/

static void CPLAccessConfigOption(const char* pszKey, int bGet)
{
    CPLMutexHolderD(&hRegisterConfigurationOptionMutex);
    if (paoGetKeys == NULL)
    {
        paoGetKeys = new std::set<CPLString>;
        paoSetKeys = new std::set<CPLString>;
        atexit(CPLShowAccessedOptions);
    }
    if (bGet)
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
  * The value is the value of a (key, value) option set with CPLSetConfigOption().
  * If the given option was no defined with CPLSetConfigOption(), it tries to find
  * it in environment variables.
  *
  * @param pszKey the key of the option to retrieve
  * @param pszDefault a default value if the key does not match existing defined options (may be NULL)
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

    const char *pszResult = NULL;

    char **papszTLConfigOptions = (char **) CPLGetTLS( CTLS_CONFIGOPTIONS );
    if( papszTLConfigOptions != NULL )
        pszResult = CSLFetchNameValue( papszTLConfigOptions, pszKey );

    if( pszResult == NULL )
    {
        CPLMutexHolderD( &hConfigMutex );

        pszResult = CSLFetchNameValue( (char **) papszConfigOptions, pszKey );
    }

#if !defined(WIN32CE) 
    if( pszResult == NULL )
        pszResult = getenv( pszKey );
#endif
    
    if( pszResult == NULL )
        return pszDefault;
    else
        return pszResult;
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
  * @param pszKey the key of the option
  * @param pszValue the value of the option, or NULL to clear a setting.
  * 
  * @see http://trac.osgeo.org/gdal/wiki/ConfigOptions
  */
void CPL_STDCALL 
CPLSetConfigOption( const char *pszKey, const char *pszValue )

{
#ifdef DEBUG_CONFIG_OPTIONS
    CPLAccessConfigOption(pszKey, FALSE);
#endif
    CPLMutexHolderD( &hConfigMutex );

    papszConfigOptions = (volatile char **) 
        CSLSetNameValue( (char **) papszConfigOptions, pszKey, pszValue );
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
  * that applies on all threads.
  *
  * @param pszKey the key of the option
  * @param pszValue the value of the option, or NULL to clear a setting.
  */

void CPL_STDCALL 
CPLSetThreadLocalConfigOption( const char *pszKey, const char *pszValue )

{
#ifdef DEBUG_CONFIG_OPTIONS
    CPLAccessConfigOption(pszKey, FALSE);
#endif

    char **papszTLConfigOptions = (char **) CPLGetTLS( CTLS_CONFIGOPTIONS );

    papszTLConfigOptions = 
        CSLSetNameValue( papszTLConfigOptions, pszKey, pszValue );

    CPLSetTLSWithFreeFunc( CTLS_CONFIGOPTIONS, papszTLConfigOptions, (CPLTLSFreeFunc)CSLDestroy );
}

/************************************************************************/
/*                           CPLFreeConfig()                            */
/************************************************************************/

void CPL_STDCALL CPLFreeConfig()

{
    {
        CPLMutexHolderD( &hConfigMutex );

        CSLDestroy( (char **) papszConfigOptions);
        papszConfigOptions = NULL;
        
        char **papszTLConfigOptions = (char **) CPLGetTLS( CTLS_CONFIGOPTIONS );
        if( papszTLConfigOptions != NULL )
        {
            CSLDestroy( papszTLConfigOptions );
            CPLSetTLS( CTLS_CONFIGOPTIONS, NULL, FALSE );
        }
    }
    CPLDestroyMutex( hConfigMutex );
    hConfigMutex = NULL;
}

/************************************************************************/
/*                              CPLStat()                               */
/*                                                                      */
/*      Same as VSIStat() except it works on "C:" as if it were         */
/*      "C:\".                                                          */
/************************************************************************/

int CPLStat( const char *pszPath, VSIStatBuf *psStatBuf )

{
    if( strlen(pszPath) == 2 && pszPath[1] == ':' )
    {
        char    szAltPath[4];
        
        szAltPath[0] = pszPath[0];
        szAltPath[1] = pszPath[1];
        szAltPath[2] = '\\';
        szAltPath[3] = '\0';
        return VSIStat( szAltPath, psStatBuf );
    }
    else
        return VSIStat( pszPath, psStatBuf );
}

/************************************************************************/
/*                            proj_strtod()                             */
/************************************************************************/
static double
proj_strtod(char *nptr, char **endptr) 

{
    char c, *cp = nptr;
    double result;

    /*
     * Scan for characters which cause problems with VC++ strtod()
     */
    while ((c = *cp) != '\0') {
        if (c == 'd' || c == 'D') {

            /*
             * Found one, so NUL it out, call strtod(),
             * then restore it and return
             */
            *cp = '\0';
            result = strtod(nptr, endptr);
            *cp = c;
            return result;
        }
        ++cp;
    }

    /* no offending characters, just handle normally */

    return strtod(nptr, endptr);
}

/************************************************************************/
/*                            CPLDMSToDec()                             */
/************************************************************************/

static const char*sym = "NnEeSsWw";
static const double vm[] = { 1.0, 0.0166666666667, 0.00027777778 };

double CPLDMSToDec( const char *is )

{
    int sign, n, nl;
    char *p, *s, work[64];
    double v, tv;

    /* copy sting into work space */
    while (isspace((unsigned char)(sign = *is))) ++is;
    for (n = sizeof(work), s = work, p = (char *)is; isgraph(*p) && --n ; )
        *s++ = *p++;
    *s = '\0';
    /* it is possible that a really odd input (like lots of leading
       zeros) could be truncated in copying into work.  But ... */
    sign = *(s = work);
    if (sign == '+' || sign == '-') s++;
    else sign = '+';
    for (v = 0., nl = 0 ; nl < 3 ; nl = n + 1 ) {
        if (!(isdigit(*s) || *s == '.')) break;
        if ((tv = proj_strtod(s, &s)) == HUGE_VAL)
            return tv;
        switch (*s) {
          case 'D': case 'd':
            n = 0; break;
          case '\'':
            n = 1; break;
          case '"':
            n = 2; break;
          case 'r': case 'R':
            if (nl) {
                return 0.0;
            }
            ++s;
            v = tv;
            goto skip;
          default:
            v += tv * vm[nl];
          skip: n = 4;
            continue;
        }
        if (n < nl) {
            return 0.0;
        }
        v += tv * vm[n];
        ++s;
    }
    /* postfix sign */
    if (*s && ((p = (char *) strchr(sym, *s))) != NULL) {
        sign = (p - sym) >= 4 ? '-' : '+';
        ++s;
    }
    if (sign == '-')
        v = -v;

    return v;
}


/************************************************************************/
/*                            CPLDecToDMS()                             */
/*                                                                      */
/*      Translate a decimal degrees value to a DMS string with          */
/*      hemisphere.                                                     */
/************************************************************************/

const char *CPLDecToDMS( double dfAngle, const char * pszAxis,
                         int nPrecision )

{
    VALIDATE_POINTER1( pszAxis, "CPLDecToDMS", "" );

    int         nDegrees, nMinutes;
    double      dfSeconds, dfABSAngle, dfEpsilon;
    char        szFormat[30];
    const char  *pszHemisphere;
    static CPL_THREADLOCAL char szBuffer[50] = { 0 };
    
    
    dfEpsilon = (0.5/3600.0) * pow(0.1,nPrecision);

    dfABSAngle = ABS(dfAngle) + dfEpsilon;
    if (dfABSAngle > 361)
    {
        return "Invalid angle";
    }

    nDegrees = (int) dfABSAngle;
    nMinutes = (int) ((dfABSAngle - nDegrees) * 60);
    dfSeconds = dfABSAngle * 3600 - nDegrees*3600 - nMinutes*60;

    if( dfSeconds > dfEpsilon * 3600.0 )
        dfSeconds -= dfEpsilon * 3600.0;

    if( EQUAL(pszAxis,"Long") && dfAngle < 0.0 )
        pszHemisphere = "W";
    else if( EQUAL(pszAxis,"Long") )
        pszHemisphere = "E";
    else if( dfAngle < 0.0 )
        pszHemisphere = "S";
    else
        pszHemisphere = "N";

    sprintf( szFormat, "%%3dd%%2d\'%%%d.%df\"%s", nPrecision+3, nPrecision, pszHemisphere );
    sprintf( szBuffer, szFormat, nDegrees, nMinutes, dfSeconds );

    return( szBuffer );
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
 * Example:     ang = 120025045.25 yields
 *              deg = 120
 *              min = 25
 *              sec = 45.25
 * 
 * The algorithm used for the conversion is as follows:
 *
 * 1.  The absolute value of the angle is used.
 *
 * 2.  The degrees are separated out:
 *     deg = ang/1000000                    (fractional portion truncated)
 *
 * 3.  The minutes are separated out:
 *     min = (ang - deg * 1000000) / 1000   (fractional portion truncated)
 *
 * 4.  The seconds are then computed:
 *     sec = ang - deg * 1000000 - min * 1000
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
    double  dfDegrees, dfMinutes, dfSeconds, dfSign;

    dfSign = ( dfPacked < 0.0 )? -1 : 1;
        
    dfSeconds = ABS( dfPacked );
    dfDegrees = floor(dfSeconds / 1000000.0);
    dfSeconds = dfSeconds - dfDegrees * 1000000.0;
    dfMinutes = floor(dfSeconds / 1000.0);
    dfSeconds = dfSeconds - dfMinutes * 1000.0;
    dfSeconds = dfSign * ( dfDegrees * 3600.0 + dfMinutes * 60.0 + dfSeconds);
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
    double  dfDegrees, dfMinutes, dfSeconds, dfSign;

    dfSign = ( dfDec < 0.0 )? -1 : 1;

    dfDec = ABS( dfDec );
    dfDegrees = floor( dfDec );
    dfMinutes = floor( ( dfDec - dfDegrees ) * 60.0 );
    dfSeconds = ( dfDec - dfDegrees ) * 3600.0 - dfMinutes * 60.0;

    return dfSign * (dfDegrees * 1000000.0 + dfMinutes * 1000.0 + dfSeconds);
}

/************************************************************************/
/*                         CPLStringToComplex()                         */
/************************************************************************/

void CPL_DLL CPLStringToComplex( const char *pszString, 
                                 double *pdfReal, double *pdfImag )

{
    int  i;
    int  iPlus = -1, iImagEnd = -1;

    while( *pszString == ' ' )
        pszString++;

    *pdfReal = CPLAtof(pszString);
    *pdfImag = 0.0;

    for( i = 0; pszString[i] != '\0' && pszString[i] != ' ' && i < 100; i++ )
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
 * @param bLarge If TRUE VSIFOpenL() (for large files) will be used instead of
 * VSIFOpen(). 
 *
 * @return a file handle or NULL if opening fails. 
 */

FILE *CPLOpenShared( const char *pszFilename, const char *pszAccess,
                     int bLarge )

{
    int i;
    int bReuse;
    CPLMutexHolderD( &hSharedFileMutex );
    GIntBig nPID = CPLGetPID();

/* -------------------------------------------------------------------- */
/*      Is there an existing file we can use?                           */
/* -------------------------------------------------------------------- */
    bReuse = EQUAL(pszAccess,"rb") || EQUAL(pszAccess, "rb+");

    for( i = 0; bReuse && i < nSharedFileCount; i++ )
    {
        if( strcmp(pasSharedFileList[i].pszFilename,pszFilename) == 0 
            && !bLarge == !pasSharedFileList[i].bLarge
            && EQUAL(pasSharedFileList[i].pszAccess,pszAccess)
            && nPID == pasSharedFileListExtra[i].nPID)
        {
            pasSharedFileList[i].nRefCount++;
            return pasSharedFileList[i].fp;
        }
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    FILE *fp;

    if( bLarge )
        fp = (FILE*) VSIFOpenL( pszFilename, pszAccess );
    else
        fp = VSIFOpen( pszFilename, pszAccess );

    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Add an entry to the list.                                       */
/* -------------------------------------------------------------------- */
    nSharedFileCount++;

    pasSharedFileList = (CPLSharedFileInfo *)
        CPLRealloc( (void *) pasSharedFileList, 
                    sizeof(CPLSharedFileInfo) * nSharedFileCount );
    pasSharedFileListExtra = (CPLSharedFileInfoExtra *)
        CPLRealloc( (void *) pasSharedFileListExtra, 
                    sizeof(CPLSharedFileInfoExtra) * nSharedFileCount );

    pasSharedFileList[nSharedFileCount-1].fp = fp;
    pasSharedFileList[nSharedFileCount-1].nRefCount = 1;
    pasSharedFileList[nSharedFileCount-1].bLarge = bLarge;
    pasSharedFileList[nSharedFileCount-1].pszFilename =CPLStrdup(pszFilename);
    pasSharedFileList[nSharedFileCount-1].pszAccess = CPLStrdup(pszAccess);
    pasSharedFileListExtra[nSharedFileCount-1].nPID = nPID;

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
    CPLMutexHolderD( &hSharedFileMutex );
    int i;

/* -------------------------------------------------------------------- */
/*      Search for matching information.                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nSharedFileCount && fp != pasSharedFileList[i].fp; i++ ){}

    if( i == nSharedFileCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to find file handle %p in CPLCloseShared().",
                  fp );
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
        VSIFCloseL( (VSILFILE*) pasSharedFileList[i].fp );
    else
        VSIFClose( pasSharedFileList[i].fp );

    CPLFree( pasSharedFileList[i].pszFilename );
    CPLFree( pasSharedFileList[i].pszAccess );

    nSharedFileCount --;
    memmove( (void *) (pasSharedFileList + i), 
             (void *) (pasSharedFileList + nSharedFileCount), 
             sizeof(CPLSharedFileInfo) );
    memmove( (void *) (pasSharedFileListExtra + i), 
             (void *) (pasSharedFileListExtra + nSharedFileCount), 
             sizeof(CPLSharedFileInfoExtra) );

    if( nSharedFileCount == 0 )
    {
        CPLFree( (void *) pasSharedFileList );
        pasSharedFileList = NULL;
        CPLFree( (void *) pasSharedFileListExtra );
        pasSharedFileListExtra = NULL;
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
    if( pnCount != NULL )
        *pnCount = nSharedFileCount;
        
    return (CPLSharedFileInfo *) pasSharedFileList;
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
    int i;

    if( nSharedFileCount > 0 )
    {
        if( fp == NULL )
            CPLDebug( "CPL", "%d Shared files open.", nSharedFileCount );
        else
            fprintf( fp, "%d Shared files open.", nSharedFileCount );
    }

    for( i = 0; i < nSharedFileCount; i++ )
    {
        if( fp == NULL )
            CPLDebug( "CPL", 
                      "%2d %d %4s %s", 
                      pasSharedFileList[i].nRefCount, 
                      pasSharedFileList[i].bLarge,
                      pasSharedFileList[i].pszAccess,
                      pasSharedFileList[i].pszFilename );
        else
            fprintf( fp, "%2d %d %4s %s", 
                     pasSharedFileList[i].nRefCount, 
                     pasSharedFileList[i].bLarge,
                     pasSharedFileList[i].pszAccess,
                     pasSharedFileList[i].pszFilename );
    }
}

/************************************************************************/
/*                           CPLUnlinkTree()                            */
/************************************************************************/

/**
 * @return 0 on successful completion, -1 if function fails.
 */

int CPLUnlinkTree( const char *pszPath )

{
/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBuf sStatBuf;

    if( VSIStat( pszPath, &sStatBuf ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems no file system object called '%s' exists.",
                  pszPath );

        return -1;
    }

/* -------------------------------------------------------------------- */
/*      If it's a simple file, just delete it.                          */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG( sStatBuf.st_mode ) )
    {
        if( VSIUnlink( pszPath ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to unlink %s.", 
                      pszPath );

            return -1;
        }
        else
            return 0;
    }

/* -------------------------------------------------------------------- */
/*      If it is a directory recurse then unlink the directory.         */
/* -------------------------------------------------------------------- */
    else if( VSI_ISDIR( sStatBuf.st_mode ) )
    {
        char **papszItems = CPLReadDir( pszPath );
        int  i;

        for( i = 0; papszItems != NULL && papszItems[i] != NULL; i++ )
        {
            char *pszSubPath;
            int nErr;

            if( EQUAL(papszItems[i],".") || EQUAL(papszItems[i],"..") )
                continue;

            pszSubPath = CPLStrdup(
                CPLFormFilename( pszPath, papszItems[i], NULL ) );

            nErr = CPLUnlinkTree( pszSubPath );
            CPLFree( pszSubPath );

            if( nErr != 0 )
            {
                CSLDestroy( papszItems );
                return nErr;
            }
        }
        
        CSLDestroy( papszItems );

        if( VSIRmdir( pszPath ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to unlink %s.", 
                      pszPath );

            return -1;
        }
        else
            return 0;
    }

/* -------------------------------------------------------------------- */
/*      otherwise report an error.                                      */
/* -------------------------------------------------------------------- */
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to unlink %s.\nUnrecognised filesystem object.",
                  pszPath );
        return 1000;
    }
}

/************************************************************************/
/*                            CPLCopyFile()                             */
/************************************************************************/

int CPLCopyFile( const char *pszNewPath, const char *pszOldPath )

{
    VSILFILE *fpOld, *fpNew;
    GByte *pabyBuffer;
    size_t nBufferSize;
    size_t nBytesRead;
    int nRet = 0;

/* -------------------------------------------------------------------- */
/*      Open old and new file.                                          */
/* -------------------------------------------------------------------- */
    fpOld = VSIFOpenL( pszOldPath, "rb" );
    if( fpOld == NULL )
        return -1;

    fpNew = VSIFOpenL( pszNewPath, "wb" );
    if( fpNew == NULL )
    {
        VSIFCloseL( fpOld );
        return -1;
    }

/* -------------------------------------------------------------------- */
/*      Prepare buffer.                                                 */
/* -------------------------------------------------------------------- */
    nBufferSize = 1024 * 1024;
    pabyBuffer = (GByte *) CPLMalloc(nBufferSize);

/* -------------------------------------------------------------------- */
/*      Copy file over till we run out of stuff.                        */
/* -------------------------------------------------------------------- */
    do { 
        nBytesRead = VSIFReadL( pabyBuffer, 1, nBufferSize, fpOld );
        if( long(nBytesRead) < 0 )
            nRet = -1;

        if( nRet == 0
            && VSIFWriteL( pabyBuffer, 1, nBytesRead, fpNew ) < nBytesRead )
            nRet = -1;
    } while( nRet == 0 && nBytesRead == nBufferSize );

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fpNew );
    VSIFCloseL( fpOld );

    CPLFree( pabyBuffer );

    return nRet;
}

/************************************************************************/
/*                            CPLMoveFile()                             */
/************************************************************************/

int CPLMoveFile( const char *pszNewPath, const char *pszOldPath )

{
    if( VSIRename( pszOldPath, pszNewPath ) == 0 )
        return 0;

    int nRet = CPLCopyFile( pszNewPath, pszOldPath );

    if( nRet == 0 )
        VSIUnlink( pszOldPath );
    
    return nRet;
}

/************************************************************************/
/* ==================================================================== */
/*                              CPLLocaleC                              */
/* ==================================================================== */
/************************************************************************/

#include <locale.h>

/************************************************************************/
/*                             CPLLocaleC()                             */
/************************************************************************/

CPLLocaleC::CPLLocaleC() : pszOldLocale(CPLStrdup(setlocale(LC_NUMERIC,NULL)))

{
    if( CSLTestBoolean(CPLGetConfigOption("GDAL_DISABLE_CPLLOCALEC","NO"))
        || EQUAL(pszOldLocale,"C")
        || EQUAL(pszOldLocale,"POSIX")
        || setlocale(LC_NUMERIC,"C") == NULL )
    {
        CPLFree( pszOldLocale );
        pszOldLocale = NULL;
    }
}

/************************************************************************/
/*                            ~CPLLocaleC()                             */
/************************************************************************/

CPLLocaleC::~CPLLocaleC()

{
    if( pszOldLocale != NULL )
    {
        setlocale( LC_NUMERIC, pszOldLocale );
        CPLFree( pszOldLocale );
    }
}

/************************************************************************/
/*                          CPLCheckForFile()                           */
/************************************************************************/

/**
 * Check for file existance.
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
 * existance, and no case insensitive testing is done. 
 *
 * @param pszFilename name of file to check for - filename case updated in some cases.
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
    if( papszSiblingFiles == NULL )
    {
        VSIStatBufL sStatBuf;

        return VSIStatL( pszFilename, &sStatBuf ) == 0;
    }

/* -------------------------------------------------------------------- */
/*      We have sibling files, compare the non-path filename portion    */
/*      of pszFilename too all entries.                                 */
/* -------------------------------------------------------------------- */
    CPLString osFileOnly = CPLGetFilename( pszFilename );
    int i;

    for( i = 0; papszSiblingFiles[i] != NULL; i++ )
    {
        if( EQUAL(papszSiblingFiles[i],osFileOnly) )
        {
            strcpy( pszFilename + strlen(pszFilename) - strlen(osFileOnly), 
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

void *CPLCreateZip( const char *pszZipFilename, char **papszOptions )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "This GDAL/OGR build does not include zlib and zip services." );
    return NULL;
}

CPLErr CPLCreateFileInZip( void *hZip, const char *pszFilename, 
                           char **papszOptions )

{
    return CE_Failure;
}

CPLErr CPLWriteFileInZip( void *hZip, const void *pBuffer, int nBufferSize )

{
    return CE_Failure;
}

CPLErr CPLCloseFileInZip( void *hZip )

{
    return CE_Failure;
}

CPLErr CPLCloseZip( void *hZip )

{
    return CE_Failure;
}

#endif /* !defined(HAVE_LIBZ) */

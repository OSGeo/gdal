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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.60  2007/01/04 21:31:44  fwarmerdam
 * Ensure that CPLReadLineL() returns NULL at end of file.
 *
 * Revision 1.59  2006/12/18 17:49:04  dreamil
 * Modified CPLReadLineL() to fix
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1374
 *
 * Revision 1.58  2006/11/18 20:58:23  mloskot
 * Removed WIN32CE ifdef around CPLLocaleC.
 *
 * Revision 1.57  2006/11/16 14:58:33  mloskot
 * Added cast from __int64 to GUIntBig on Windows.
 *
 * Revision 1.56  2006/10/11 08:30:01  dron
 * Added CPLStrlwr() function.
 *
 * Revision 1.55  2006/08/23 15:09:46  fwarmerdam
 * Added --mempreload support, and VSICopyFile
 *
 * Revision 1.54  2006/06/30 18:18:17  dron
 * Avoid warnings.
 *
 * Revision 1.53  2006/06/30 14:55:10  dron
 * Avoid some warnings on win/64.
 *
 * Revision 1.52  2006/03/04 00:46:43  mloskot
 * [WCE] CPLLocaleC class excluded from compilation for Windows CE
 *
 * Revision 1.51  2006/03/03 19:40:53  fwarmerdam
 * added CPLLocaleC class
 *
 * Revision 1.50  2006/02/19 21:54:34  mloskot
 * [WINCE] Changes related to Windows CE port of CPL. Most changes are #ifdef wrappers.
 *
 * Revision 1.49  2006/02/09 05:54:15  fwarmerdam
 * use ll in format for long long on mac, not L
 *
 * Revision 1.48  2005/11/24 19:22:54  fwarmerdam
 * Added silly-value checks in CPLMalloc/Realloc.
 *
 * Revision 1.47  2005/10/17 23:33:51  fwarmerdam
 * CPLFGets() - be very wary about reads ending in the middle of CRLF.
 * Was causing problems with world.mif/world.mid reading.
 *
 * Revision 1.46  2005/09/11 21:08:59  fwarmerdam
 * properly manage TLS ReadLine buffer, added CPLReadLineL()
 *
 * Revision 1.45  2005/07/11 16:41:31  fwarmerdam
 * Protect config list and shared file list with mutex.
 *
 * Revision 1.44  2005/05/23 03:57:57  fwarmerdam
 * make make static buffers threadlocal
 *
 * Revision 1.43  2005/04/04 15:23:30  fwarmerdam
 * some functions now CPL_STDCALL
 *
 * Revision 1.42  2004/11/17 22:57:21  fwarmerdam
 * added CPLScanPointer() and CPLPrintPointer()
 *
 * Revision 1.41  2004/08/16 20:24:07  warmerda
 * added CPLUnlinkTree
 *
 * Revision 1.40  2004/07/31 04:51:36  warmerda
 * added shared file open support
 *
 * Revision 1.39  2004/06/01 20:40:02  warmerda
 * expanded tabs
 *
 * Revision 1.38  2004/03/28 16:22:02  warmerda
 * const correctness changes in scan functions
 *
 * Revision 1.37  2004/03/24 09:01:17  dron
 * Added CPLPrintUIntBig().
 *
 * Revision 1.36  2004/02/25 09:02:25  dron
 * Fixed bug in CPLPackedDMSToDec().
 *
 * Revision 1.35  2004/02/21 10:08:54  dron
 * Fixes in various string handling functions.
 *
 * Revision 1.34  2004/02/07 14:03:30  dron
 * CPLDecToPackedDMS() added.
 *
 * Revision 1.33  2004/02/01 08:37:55  dron
 * Added CPLPackedDMSToDec().
 *
 * Revision 1.32  2003/12/28 17:24:43  warmerda
 * added CPLFreeConfig
 *
 * Revision 1.31  2003/10/17 07:06:06  dron
 * Added locale selection option to CPLScanDouble() and CPLPrintDOuble().
 *
 * Revision 1.30  2003/09/28 14:14:16  dron
 * Added CPLScanString().
 *
 * Revision 1.29  2003/09/12 20:49:24  warmerda
 * reimplement CPLFGets() to avoid textmode problems
 *
 * Revision 1.28  2003/09/08 12:54:42  dron
 * Fixed warnings.
 *
 * Revision 1.27  2003/09/08 11:09:53  dron
 * Added CPLPrintDouble() and CPLPrintTime().
 *
 * Revision 1.26  2003/09/07 14:38:43  dron
 * Added CPLPrintString(), CPLPrintStringFill(), CPLPrintInt32(), CPLPrintUIntBig().
 *
 * Revision 1.25  2003/09/03 13:07:26  warmerda
 * Cleaned up CPLScanLong() a bit to avoid warnings, and
 * unnecessary conversion to/from double.
 *
 * Revision 1.24  2003/08/31 14:48:05  dron
 * Added CPLScanLong() and CPLScanDouble().
 *
 * Revision 1.23  2003/08/25 20:01:58  dron
 * Added CPLFGets() helper function.
 *
 * Revision 1.22  2003/05/08 21:51:14  warmerda
 * added CPL{G,S}etConfigOption() usage
 *
 * Revision 1.21  2003/03/05 16:46:54  warmerda
 * Cast strchr() result for Sun (patch from Graeme).
 *
 * Revision 1.20  2003/03/02 04:44:38  warmerda
 * added CPLStringToComplex
 *
 * Revision 1.19  2003/02/14 22:12:07  warmerda
 * expand tabs
 *
 * Revision 1.18  2002/12/18 20:22:53  warmerda
 * fiddle with roundoff issues in DecToDMS
 *
 * Revision 1.17  2002/12/10 19:46:04  warmerda
 * modified CPLReadLine() to seek back if it overreads past a CR or LF
 *
 * Revision 1.16  2002/12/09 18:52:51  warmerda
 * added DMS conversion
 *
 * Revision 1.15  2002/03/05 14:26:57  warmerda
 * expanded tabs
 *
 * Revision 1.14  2001/12/12 17:06:57  warmerda
 * added CPLStat
 *
 * Revision 1.13  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.12  2001/03/09 03:19:24  danmo
 * Set pszRLBuffer=NULL after freeing it to avoid reallocating an invalid ptr
 *
 * Revision 1.11  2001/03/05 03:37:19  warmerda
 * Improve support for recovering CPLReadLine() working buffer.
 *
 * Revision 1.10  2001/01/19 21:16:41  warmerda
 * expanded tabs
 *
 * Revision 1.9  2000/04/17 15:56:11  warmerda
 * make configuration tests always happen
 *
 * Revision 1.8  2000/04/05 21:02:47  warmerda
 * Added CPLVerifyConfiguration()
 *
 * Revision 1.7  1999/08/27 12:55:39  danmo
 * Support 0 bytes allocations in CPLRealloc()
 *
 * Revision 1.6  1999/06/25 04:38:03  warmerda
 * Fixed CPLReadLine() to work for long lines.
 *
 * Revision 1.5  1999/05/20 02:54:37  warmerda
 * Added API documentation
 *
 * Revision 1.4  1999/01/02 20:29:53  warmerda
 * Allow zero length allocations
 *
 * Revision 1.3  1998/12/15 19:01:07  warmerda
 * Added CPLReadLine().
 *
 * Revision 1.2  1998/12/03 18:30:04  warmerda
 * Use CPLError() instead of GPSError().
 *
 * Revision 1.1  1998/12/02 19:33:23  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

#if defined(WIN32CE)
#  include "cpl_wince.h"
#  include <wce_errno.h>
#endif

static void *hConfigMutex = NULL;
static volatile char **papszConfigOptions = NULL;

static void *hSharedFileMutex = NULL;
static volatile int nSharedFileCount = 0;
static volatile CPLSharedFileInfo *pasSharedFileList = NULL;


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
    
    pReturn = VSICalloc( nCount, nSize );
    if( pReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLCalloc(): Out of memory allocating %d bytes.\n",
                  nSize * nCount );
    }

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

    if( nSize < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "CPLMalloc(%d): Silly size requested.\n",
                  nSize );
        return NULL;
    }
    
    pReturn = VSIMalloc( nSize );
    if( pReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLMalloc(): Out of memory allocating %d bytes.\n",
                  nSize );
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

    if( nNewSize < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "CPLRealloc(%d): Silly size requested.\n",
                  nNewSize );
        return NULL;
    }
    
    if( pData == NULL )
        pReturn = VSIMalloc( nNewSize );
    else
        pReturn = VSIRealloc( pData, nNewSize );
    
    if( pReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLRealloc(): Out of memory allocating %d bytes.\n",
                  nNewSize );
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

    pszReturn = VSIStrdup( pszString );
        
    if( pszReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLStrdup(): Out of memory allocating %d bytes.\n",
                  strlen(pszString) );
        
    }
    
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
            *pszTemp = tolower (*pszTemp);
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
 * is _not_ stored into the buffer. A '\0' is stored after the last
 * character in the buffer. All three types of newline terminators
 * recognized by the CPLFGets(): single '\r' and '\n' and '\r\n'
 * combination.
 *
 * @param pszBuffer pointer to the targeting character buffer.
 * @param nBufferSize maximum size of the string to read (not including
 * termonating '\0').
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
    if( (int) *pnAlloc < nRequiredSize+1 )
    {
        int nNewSize = nRequiredSize + 4 + 500;

        pnAlloc = (GUInt32 *) CPLRealloc(pnAlloc,nNewSize);
        if( pnAlloc == NULL )
        {
            CPLSetTLS( CTLS_RLBUFFERINFO, NULL, FALSE );
            return NULL;
        }
            
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
        if( CPLFGets( pszRLBuffer+nReadSoFar, 128, fp ) == NULL )
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

const char *CPLReadLineL( FILE * fp )

{
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
        pszRLBuffer = CPLReadLineBuffer( nBufLength + nChunkSize + 1 );

        if( nChunkBytesRead == nChunkBytesConsumed + 1 )
        {
            // case where one character is left over from last read.
            szChunk[0] = szChunk[nChunkBytesConsumed];
            nChunkBytesRead = VSIFReadL( szChunk+1, 1, nChunkSize-1, fp ) + 1;
            if( nChunkBytesRead == 1 )
                break;
        }
        else
        {
            // fresh read.
            nChunkBytesRead = VSIFReadL( szChunk, 1, nChunkSize, fp );
            if( nChunkBytesRead == 0 )
                return NULL;
        }
        
/* -------------------------------------------------------------------- */
/*      copy over characters watching for end-of-line.                  */
/* -------------------------------------------------------------------- */
        int bBreak = FALSE;
        nChunkBytesConsumed = 0;
        while( nChunkBytesConsumed <= nChunkBytesRead-1 && !bBreak )
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
                pszRLBuffer[nBufLength++] = szChunk[nChunkBytesConsumed++];
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
        while ( i-- > 0 && isspace(pszBuffer[i]) )
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
#if defined(WIN32) && defined(_MSC_VER)
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
 * the result to a GUIntBig. 
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

#if defined(WIN32) && defined(_MSC_VER)
        sscanf( szTemp+2, "%p", &pResult );
#else
        sscanf( szTemp, "%p", &pResult );
#endif
    }
    
    else
    {
        pResult = (void *) CPLScanUIntBig( szTemp, nMaxLength );
    }

    return pResult;
}

/************************************************************************/
/*                             CPLScanDouble()                          */
/************************************************************************/

/**
 * Scan up to a maximum number of characters from a string and convert
 * the result to a double.
 *
 * @param pszString String containing characters to be scanned. It may be
 * terminated with a null character.
 *
 * @param nMaxLength The maximum number of character to consider as part
 * of the number. Less characters will be considered if a null character
 * is encountered.
 * 
 * @param pszLocale Pointer to a character string containing locale name
 * ("C", "POSIX", "us_US", "ru_RU.KOI8-R" etc.). If NULL, we will not
 * manipulate with locale settings and current process locale will be used for
 * printing. Wee need this setting because in different locales decimal
 * delimiter represented with the different characters. With the pszLocale
 * option we can control what exact locale will be used for scanning a numeric
 * value from the string (in most cases it should be C/POSIX).
 *
 * @return Double value, converted from its ASCII form.
 */

double CPLScanDouble( const char *pszString, int nMaxLength, char *pszLocale )
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
/*      Use atof() to fetch out the result                              */
/* -------------------------------------------------------------------- */
#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
    char        *pszCurLocale = NULL;

    if ( pszLocale || EQUAL( pszLocale, "" ) )
    {
        // Save the current locale
        pszCurLocale = setlocale(LC_ALL, NULL );
        // Set locale to the specified value
        setlocale(LC_ALL, pszLocale );
    }
#endif

    dfValue = atof( pszValue );

#if defined(HAVE_LOCALE_H) && defined(HAVE_SETLOCALE)
    // Restore stored locale back
    if ( pszCurLocale )
        setlocale(LC_ALL, pszCurLocale );
#endif

    CPLFree( pszValue );
    return dfValue;
}

/************************************************************************/
/*                      CPLPrintString()                                */
/************************************************************************/

/**
 * Copy the string pointed to by pszSrc, NOT including the terminating
 * `\0' character, to the array pointed to by pszDest.
 *
 * @param pszDest Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string.
 *
 * @param pszDest Pointer to the source buffer.
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
 * `\0' character, to the array pointed to by pszDest. Remainder of the
 * destination string will be filled with space characters. This is only
 * difference from the PrintString().
 *
 * @param pszDest Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string.
 *
 * @param pszDest Pointer to the source buffer.
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
 * @param Pointer to the destination string buffer. Should be
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
 * @param Pointer to the destination string buffer. Should be
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

#if defined(WIN32) && defined(_MSC_VER)
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
 * @param Pointer to the destination string buffer. Should be
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
 * @param Pointer to the destination string buffer. Should be
 * large enough to hold the resulting string. Note, that the string will
 * not be NULL-terminated, so user should do this himself, if needed.
 *
 * @param Format specifier (for example, "%16.9E").
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
                    double dfValue, char *pszLocale )
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
                  const struct tm *poBrokenTime, char *pszLocale )
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
                  sizeof(GInt32) );

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

/************************************************************************/
/*                         CPLGetConfigOption()                         */
/************************************************************************/

const char * CPL_STDCALL
CPLGetConfigOption( const char *pszKey, const char *pszDefault )

{
    const char *pszResult = NULL;

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

void CPL_STDCALL 
CPLSetConfigOption( const char *pszKey, const char *pszValue )

{
    CPLMutexHolderD( &hConfigMutex );

    papszConfigOptions = (volatile char **) 
        CSLSetNameValue( (char **) papszConfigOptions, pszKey, pszValue );
}

/************************************************************************/
/*                           CPLFreeConfig()                            */
/************************************************************************/

void CPL_STDCALL CPLFreeConfig()

{
    CPLMutexHolderD( &hConfigMutex );

    CSLDestroy( (char **) papszConfigOptions);
    papszConfigOptions = NULL;
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
        char    szAltPath[10];
        
        strncpy( szAltPath, pszPath, 10 );
        strcat( szAltPath, "\\" );
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
    while (isspace(sign = *is)) ++is;
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
    if (*s && (p = (char *) strchr(sym, *s))) {
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
    int         nDegrees, nMinutes;
    double      dfSeconds, dfABSAngle, dfEpsilon;
    char        szFormat[30];
    static CPL_THREADLOCAL char szBuffer[50];
    const char  *pszHemisphere;
    
    dfEpsilon = (0.5/3600.0) * pow(0.1,nPrecision);

    dfABSAngle = ABS(dfAngle) + dfEpsilon;

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

    sprintf( szFormat, "%%3dd%%2d\'%%.%df\"%s", nPrecision, pszHemisphere );
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

    *pdfReal = atof(pszString);
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
        *pdfImag = atof(pszString + iPlus);
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

/* -------------------------------------------------------------------- */
/*      Is there an existing file we can use?                           */
/* -------------------------------------------------------------------- */
    bReuse = EQUAL(pszAccess,"rb") || EQUAL(pszAccess, "rb+");

    for( i = 0; bReuse && i < nSharedFileCount; i++ )
    {
        if( strcmp(pasSharedFileList[i].pszFilename,pszFilename) == 0 
            && !bLarge == !pasSharedFileList[i].bLarge
            && EQUAL(pasSharedFileList[i].pszAccess,pszAccess) )
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
        fp = VSIFOpenL( pszFilename, pszAccess );
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

    pasSharedFileList[nSharedFileCount-1].fp = fp;
    pasSharedFileList[nSharedFileCount-1].nRefCount = 1;
    pasSharedFileList[nSharedFileCount-1].bLarge = bLarge;
    pasSharedFileList[nSharedFileCount-1].pszFilename =CPLStrdup(pszFilename);
    pasSharedFileList[nSharedFileCount-1].pszAccess = CPLStrdup(pszAccess);

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
        VSIFCloseL( pasSharedFileList[i].fp );
    else
        VSIFClose( pasSharedFileList[i].fp );

    CPLFree( pasSharedFileList[i].pszFilename );
    CPLFree( pasSharedFileList[i].pszAccess );

//    pasSharedFileList[i] = pasSharedFileList[--nSharedFileCount];
    memcpy( (void *) (pasSharedFileList + i), 
            (void *) (pasSharedFileList + --nSharedFileCount), 
            sizeof(CPLSharedFileInfo) );

    if( nSharedFileCount == 0 )
    {
        CPLFree( (void *) pasSharedFileList );
        pasSharedFileList = NULL;
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

        return errno;
    }

/* -------------------------------------------------------------------- */
/*      If it's a simple file, just delete it.                          */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG( sStatBuf.st_mode ) )
    {
        if( VSIUnlink( pszPath ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to unlink %s.\n%s", 
                      pszPath, VSIStrerror( errno ) );
            return errno;
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
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to unlink %s.\n%s", 
                      pszPath, VSIStrerror( errno ) );
            return errno;
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
    FILE *fpOld, *fpNew;
    GByte *pabyBuffer;
    size_t nBufferSize;
    size_t nBytesRead;

/* -------------------------------------------------------------------- */
/*      Open old and new file.                                          */
/* -------------------------------------------------------------------- */
    fpOld = VSIFOpenL( pszOldPath, "rb" );
    if( fpOld == NULL )
        return -1;

    fpNew = VSIFOpenL( pszNewPath, "wb" );
    if( fpNew == NULL )
        return -1;

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
        if( nBytesRead < 0 )
            return -1;

        if( VSIFWriteL( pabyBuffer, 1, nBytesRead, fpNew ) < nBytesRead )
            return -1;
    } while( nBytesRead == nBufferSize );

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fpNew );
    VSIFCloseL( fpOld );

    CPLFree( pabyBuffer );

    return 0;
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

CPLLocaleC::CPLLocaleC()

{
    pszOldLocale = strdup(setlocale(LC_NUMERIC,NULL));
    if( setlocale(LC_NUMERIC,"C") == NULL )
        pszOldLocale = NULL;
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


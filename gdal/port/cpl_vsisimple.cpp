/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library 
 * Purpose:  Simple implementation of POSIX VSI functions.
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
 ****************************************************************************
 *
 * NB: Note that in wrappers we are always saving the error state (errno
 * variable) to avoid side effects during debug prints or other possible
 * standard function calls (error states will be overwritten after such
 * a call).
 *
 ****************************************************************************/

#include "cpl_config.h"
#include "cpl_port.h"
#include "cpl_vsi.h"
#include "cpl_error.h"

/* Uncomment to check consistent usage of VSIMalloc(), VSIRealloc(), */
/* VSICalloc(), VSIFree(), VSIStrdup() */
//#define DEBUG_VSIMALLOC

/* Uncomment to compute memory usage statistics. */
/* DEBUG_VSIMALLOC must also be defined */
//#define DEBUG_VSIMALLOC_STATS

/* Uncomment to print every memory allocation or deallocation. */
/* DEBUG_VSIMALLOC must also be defined */
//#define DEBUG_VSIMALLOC_VERBOSE

CPL_CVSID("$Id$");

/* for stat() */

/* Unix or Windows NT/2000/XP */
#if !defined(WIN32) && !defined(WIN32CE)
#  include <unistd.h>
#elif !defined(WIN32CE) /* not Win32 platform */
#  include <io.h>
#  include <fcntl.h>
#  include <direct.h>
#endif

/* Windows CE or other platforms */
#if defined(WIN32CE)
#  include <wce_io.h>
#  include <wce_stat.h>
#  include <wce_stdio.h>
#  include <wce_string.h>
#  include <wce_time.h>
# define time wceex_time
#else
#  include <sys/stat.h>
#  include <time.h>
#endif

/************************************************************************/
/*                              VSIFOpen()                              */
/************************************************************************/

FILE *VSIFOpen( const char * pszFilename, const char * pszAccess )

{
    FILE    *fp = fopen( (char *) pszFilename, (char *) pszAccess );
    int     nError = errno;

    VSIDebug3( "VSIFOpen(%s,%s) = %p", pszFilename, pszAccess, fp );

    errno = nError;
    return( fp );
}

/************************************************************************/
/*                             VSIFClose()                              */
/************************************************************************/

int VSIFClose( FILE * fp )

{
    VSIDebug1( "VSIClose(%p)", fp );

    return( fclose(fp) );
}

/************************************************************************/
/*                              VSIFSeek()                              */
/************************************************************************/

int VSIFSeek( FILE * fp, long nOffset, int nWhence )

{
    int     nResult = fseek( fp, nOffset, nWhence );
    int     nError = errno;

#ifdef VSI_DEBUG
    if( nWhence == SEEK_SET )
    {
        VSIDebug3( "VSIFSeek(%p,%ld,SEEK_SET) = %d", fp, nOffset, nResult );
    }
    else if( nWhence == SEEK_END )
    {
        VSIDebug3( "VSIFSeek(%p,%ld,SEEK_END) = %d", fp, nOffset, nResult );
    }
    else if( nWhence == SEEK_CUR )
    {
        VSIDebug3( "VSIFSeek(%p,%ld,SEEK_CUR) = %d", fp, nOffset, nResult );
    }
    else
    {
        VSIDebug4( "VSIFSeek(%p,%ld,%d-Unknown) = %d",
                   fp, nOffset, nWhence, nResult );
    }
#endif 

    errno = nError;
    return nResult;
}

/************************************************************************/
/*                              VSIFTell()                              */
/************************************************************************/

long VSIFTell( FILE * fp )

{
    long    nOffset = ftell(fp);
    int     nError = errno;

    VSIDebug2( "VSIFTell(%p) = %ld", fp, nOffset );

    errno = nError;
    return nOffset;
}

/************************************************************************/
/*                             VSIRewind()                              */
/************************************************************************/

void VSIRewind( FILE * fp )

{
    VSIDebug1("VSIRewind(%p)", fp );
    rewind( fp );
}

/************************************************************************/
/*                              VSIFRead()                              */
/************************************************************************/

size_t VSIFRead( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    size_t  nResult = fread( pBuffer, nSize, nCount, fp );
    int     nError = errno;

    VSIDebug4( "VSIFRead(%p,%ld,%ld) = %ld", 
               fp, (long)nSize, (long)nCount, (long)nResult );

    errno = nError;
    return nResult;
}

/************************************************************************/
/*                             VSIFWrite()                              */
/************************************************************************/

size_t VSIFWrite( const void *pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    size_t  nResult = fwrite( pBuffer, nSize, nCount, fp );
    int     nError = errno;

    VSIDebug4( "VSIFWrite(%p,%ld,%ld) = %ld", 
               fp, (long)nSize, (long)nCount, (long)nResult );

    errno = nError;
    return nResult;
}

/************************************************************************/
/*                             VSIFFlush()                              */
/************************************************************************/

void VSIFFlush( FILE * fp )

{
    VSIDebug1( "VSIFFlush(%p)", fp );
    fflush( fp );
}

/************************************************************************/
/*                              VSIFGets()                              */
/************************************************************************/

char *VSIFGets( char *pszBuffer, int nBufferSize, FILE * fp )

{
    return( fgets( pszBuffer, nBufferSize, fp ) );
}

/************************************************************************/
/*                              VSIFGetc()                              */
/************************************************************************/

int VSIFGetc( FILE * fp )

{
    return( fgetc( fp ) );
}

/************************************************************************/
/*                             VSIUngetc()                              */
/************************************************************************/

int VSIUngetc( int c, FILE * fp )

{
    return( ungetc( c, fp ) );
}

/************************************************************************/
/*                             VSIFPrintf()                             */
/*                                                                      */
/*      This is a little more complicated than just calling             */
/*      fprintf() because of the variable arguments.  Instead we        */
/*      have to use vfprintf().                                         */
/************************************************************************/

int     VSIFPrintf( FILE * fp, const char * pszFormat, ... )

{
    va_list     args;
    int         nReturn;

    va_start( args, pszFormat );
    nReturn = vfprintf( fp, pszFormat, args );
    va_end( args );

    return( nReturn );
}

/************************************************************************/
/*                              VSIFEof()                               */
/************************************************************************/

int VSIFEof( FILE * fp )

{
    return( feof( fp ) );
}

/************************************************************************/
/*                              VSIFPuts()                              */
/************************************************************************/

int VSIFPuts( const char * pszString, FILE * fp )

{
    return fputs( pszString, fp );
}

/************************************************************************/
/*                              VSIFPutc()                              */
/************************************************************************/

int VSIFPutc( int nChar, FILE * fp )

{
    return( fputc( nChar, fp ) );
}


#ifdef DEBUG_VSIMALLOC_STATS
#include "cpl_multiproc.h"

static void* hMemStatMutex = 0;
static size_t nCurrentTotalAllocs = 0;
static size_t nMaxTotalAllocs = 0;
static GUIntBig nVSIMallocs = 0;
static GUIntBig nVSICallocs = 0;
static GUIntBig nVSIReallocs = 0;
static GUIntBig nVSIFrees = 0;

/************************************************************************/
/*                         VSIShowMemStats()                            */
/************************************************************************/

void VSIShowMemStats()
{
    printf("Current VSI memory usage        : " CPL_FRMT_GUIB " bytes\n",
            (GUIntBig)nCurrentTotalAllocs);
    printf("Maximum VSI memory usage        : " CPL_FRMT_GUIB " bytes\n",
            (GUIntBig)nMaxTotalAllocs);
    printf("Number of calls to VSIMalloc()  : " CPL_FRMT_GUIB "\n",
            nVSIMallocs);
    printf("Number of calls to VSICalloc()  : " CPL_FRMT_GUIB "\n",
            nVSICallocs);
    printf("Number of calls to VSIRealloc() : " CPL_FRMT_GUIB "\n",
            nVSIReallocs);
    printf("Number of calls to VSIFree()    : " CPL_FRMT_GUIB "\n",
            nVSIFrees);
    printf("VSIMalloc + VSICalloc - VSIFree : " CPL_FRMT_GUIB "\n",
            nVSIMallocs + nVSICallocs - nVSIFrees);
}
#endif

/************************************************************************/
/*                             VSICalloc()                              */
/************************************************************************/

void *VSICalloc( size_t nCount, size_t nSize )

{
#ifdef DEBUG_VSIMALLOC
    void* ptr = VSIMalloc(nCount * nSize);
    memset(ptr, 0, nCount * nSize);
#ifdef DEBUG_VSIMALLOC_STATS
    {
        CPLMutexHolderD(&hMemStatMutex);
        nVSICallocs ++;
        nVSIMallocs --;
    }
#endif
    return ptr;
#else
    return( calloc( nCount, nSize ) );
#endif
}

/************************************************************************/
/*                             VSIMalloc()                              */
/************************************************************************/

void *VSIMalloc( size_t nSize )

{
#ifdef DEBUG_VSIMALLOC
    char* ptr = (char*) malloc(4 + sizeof(size_t) + nSize);
    if (ptr == NULL)
        return NULL;
    ptr[0] = 'V';
    ptr[1] = 'S';
    ptr[2] = 'I';
    ptr[3] = 'M';
    memcpy(ptr + 4, &nSize, sizeof(size_t));
#if defined(DEBUG_VSIMALLOC_STATS) || defined(DEBUG_VSIMALLOC_VERBOSE)
    {
        CPLMutexHolderD(&hMemStatMutex);
#ifdef DEBUG_VSIMALLOC_VERBOSE
        fprintf(stderr, "Thread[%p] VSIMalloc(%d) = %p\n",
                (void*)CPLGetPID(), (int)nSize, ptr + 4 + sizeof(size_t));
#endif
#ifdef DEBUG_VSIMALLOC_STATS
        nVSIMallocs ++;
        if (nMaxTotalAllocs == 0)
            atexit(VSIShowMemStats);
        nCurrentTotalAllocs += nSize;
        if (nCurrentTotalAllocs > nMaxTotalAllocs)
            nMaxTotalAllocs = nCurrentTotalAllocs;
#endif
    }
#endif
    return ptr + 4 + sizeof(size_t);
#else
    return( malloc( nSize ) );
#endif
}

#ifdef DEBUG_VSIMALLOC
void VSICheckMarker(char* ptr)
{
    if (memcmp(ptr, "VSIM", 4) != 0)
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "Inconsistant use of VSI memory allocation primitives for %p : %c%c%c%c",
                 ptr, ptr[0], ptr[1], ptr[2], ptr[3]);
    }
}

#endif

/************************************************************************/
/*                             VSIRealloc()                             */
/************************************************************************/      

void * VSIRealloc( void * pData, size_t nNewSize )

{
#ifdef DEBUG_VSIMALLOC
    if (pData == NULL)
        return VSIMalloc(nNewSize);
        
    char* ptr = ((char*)pData) - 4 - sizeof(size_t);
    VSICheckMarker(ptr);
#ifdef DEBUG_VSIMALLOC_STATS
    size_t nOldSize;
    memcpy(&nOldSize, ptr + 4, sizeof(size_t));
#endif

    ptr = (char*) realloc(ptr, nNewSize + 4 + sizeof(size_t));
    if (ptr == NULL)
        return NULL;
    memcpy(ptr + 4, &nNewSize, sizeof(size_t));

#if defined(DEBUG_VSIMALLOC_STATS) || defined(DEBUG_VSIMALLOC_VERBOSE)
    {
        CPLMutexHolderD(&hMemStatMutex);
#ifdef DEBUG_VSIMALLOC_VERBOSE
        fprintf(stderr, "Thread[%p] VSIRealloc(%p, %d) = %p\n",
                (void*)CPLGetPID(), pData, (int)nNewSize, ptr + 4 + sizeof(size_t));
#endif
#ifdef DEBUG_VSIMALLOC_STATS
        nVSIReallocs ++;
        nCurrentTotalAllocs -= nOldSize;
        nCurrentTotalAllocs += nNewSize;
        if (nCurrentTotalAllocs > nMaxTotalAllocs)
            nMaxTotalAllocs = nCurrentTotalAllocs;
#endif
    }
#endif
    return ptr + 4 + sizeof(size_t);
#else
    return( realloc( pData, nNewSize ) );
#endif
}

/************************************************************************/
/*                              VSIFree()                               */
/************************************************************************/

void VSIFree( void * pData )

{
#ifdef DEBUG_VSIMALLOC
    if (pData == NULL)
        return;

    char* ptr = ((char*)pData) - 4 - sizeof(size_t);
    VSICheckMarker(ptr);
    ptr[0] = 'M';
    ptr[1] = 'I';
    ptr[2] = 'S';
    ptr[3] = 'V';
#if defined(DEBUG_VSIMALLOC_STATS) || defined(DEBUG_VSIMALLOC_VERBOSE)
    size_t nOldSize;
    memcpy(&nOldSize, ptr + 4, sizeof(size_t));
    {
        CPLMutexHolderD(&hMemStatMutex);
#ifdef DEBUG_VSIMALLOC_VERBOSE
        fprintf(stderr, "Thread[%p] VSIFree(%p, (%d bytes))\n",
                (void*)CPLGetPID(), pData, (int)nOldSize);
#endif
#ifdef DEBUG_VSIMALLOC_STATS
        nVSIFrees ++;
        nCurrentTotalAllocs -= nOldSize;
#endif
    }
#endif
    free(ptr);
#else
    if( pData != NULL )
        free( pData );
#endif
}

/************************************************************************/
/*                             VSIStrdup()                              */
/************************************************************************/

char *VSIStrdup( const char * pszString )

{
#ifdef DEBUG_VSIMALLOC
    int nSize = strlen(pszString) + 1;
    char* ptr = (char*) VSIMalloc(nSize);
    memcpy(ptr, pszString, nSize);
    return ptr;
#else
    return( strdup( pszString ) );
#endif
}

/************************************************************************/
/*                          VSICheckMul2()                              */
/************************************************************************/

static size_t VSICheckMul2( size_t mul1, size_t mul2, int *pbOverflowFlag)
{
    size_t res = mul1 * mul2;
    if (mul1 != 0)
    {
        if (res / mul1 == mul2)
        {
            if (pbOverflowFlag)
                *pbOverflowFlag = FALSE;
            return res;
        }
        else
        {
            if (pbOverflowFlag)
                *pbOverflowFlag = TRUE;
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Multiplication overflow : %lu * %lu",
                     (unsigned long)mul1, (unsigned long)mul2);
        }
    }
    else
    {
        if (pbOverflowFlag)
             *pbOverflowFlag = FALSE;
    }
    return 0;
}

/************************************************************************/
/*                          VSICheckMul3()                              */
/************************************************************************/

static size_t VSICheckMul3( size_t mul1, size_t mul2, size_t mul3, int *pbOverflowFlag)
{
    if (mul1 != 0)
    {
        size_t res = mul1 * mul2;
        if (res / mul1 == mul2)
        {
            size_t res2 = res * mul3;
            if (mul3 != 0)
            {
                if (res2 / mul3 == res)
                {
                    if (pbOverflowFlag)
                        *pbOverflowFlag = FALSE;
                    return res2;
                }
                else
                {
                    if (pbOverflowFlag)
                        *pbOverflowFlag = TRUE;
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Multiplication overflow : %lu * %lu * %lu",
                     (unsigned long)mul1, (unsigned long)mul2, (unsigned long)mul3);
                }
            }
            else
            {
                if (pbOverflowFlag)
                    *pbOverflowFlag = FALSE;
            }
        }
        else
        {
            if (pbOverflowFlag)
                *pbOverflowFlag = TRUE;
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Multiplication overflow : %lu * %lu * %lu",
                     (unsigned long)mul1, (unsigned long)mul2, (unsigned long)mul3);
        }
    }
    else
    {
        if (pbOverflowFlag)
             *pbOverflowFlag = FALSE;
    }
    return 0;
}



/**
 VSIMalloc2 allocates (nSize1 * nSize2) bytes.
 In case of overflow of the multiplication, or if memory allocation fails, a
 NULL pointer is returned and a CE_Failure error is raised with CPLError().
 If nSize1 == 0 || nSize2 == 0, a NULL pointer will also be returned.
 CPLFree() or VSIFree() can be used to free memory allocated by this function.
*/
void CPL_DLL *VSIMalloc2( size_t nSize1, size_t nSize2 )
{
    int bOverflowFlag = FALSE;
    size_t nSizeToAllocate;
    void* pReturn;

    nSizeToAllocate = VSICheckMul2( nSize1, nSize2, &bOverflowFlag );
    if (bOverflowFlag)
        return NULL;

    if (nSizeToAllocate == 0)
        return NULL;

    pReturn = VSIMalloc(nSizeToAllocate);

    if( pReturn == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "VSIMalloc2(): Out of memory allocating %lu bytes.\n",
                  (unsigned long)nSizeToAllocate );
    }

    return pReturn;
}

/**
 VSIMalloc3 allocates (nSize1 * nSize2 * nSize3) bytes.
 In case of overflow of the multiplication, or if memory allocation fails, a
 NULL pointer is returned and a CE_Failure error is raised with CPLError().
 If nSize1 == 0 || nSize2 == 0 || nSize3 == 0, a NULL pointer will also be returned.
 CPLFree() or VSIFree() can be used to free memory allocated by this function.
*/
void CPL_DLL *VSIMalloc3( size_t nSize1, size_t nSize2, size_t nSize3 )
{
    int bOverflowFlag = FALSE;

    size_t nSizeToAllocate;
    void* pReturn;

    nSizeToAllocate = VSICheckMul3( nSize1, nSize2, nSize3, &bOverflowFlag );
    if (bOverflowFlag)
        return NULL;

    if (nSizeToAllocate == 0)
        return NULL;

    pReturn = VSIMalloc(nSizeToAllocate);

    if( pReturn == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "VSIMalloc3(): Out of memory allocating %lu bytes.\n",
                  (unsigned long)nSizeToAllocate );
    }

    return pReturn;
}


/************************************************************************/
/*                              VSIStat()                               */
/************************************************************************/

int VSIStat( const char * pszFilename, VSIStatBuf * pStatBuf )

{
#if defined(macos_pre10)
    return -1;
#else
    return( stat( pszFilename, pStatBuf ) );
#endif
}

/************************************************************************/
/*                              VSITime()                               */
/************************************************************************/

unsigned long VSITime( unsigned long * pnTimeToSet )

{
    time_t tTime;
        
    tTime = time( NULL );

    if( pnTimeToSet != NULL )
        *pnTimeToSet = (unsigned long) tTime;

    return (unsigned long) tTime;
}

/************************************************************************/
/*                              VSICTime()                              */
/************************************************************************/

const char *VSICTime( unsigned long nTime )

{
    time_t tTime = (time_t) nTime;

    return (const char *) ctime( &tTime );
}

/************************************************************************/
/*                             VSIGMTime()                              */
/************************************************************************/

struct tm *VSIGMTime( const time_t *pnTime, struct tm *poBrokenTime )
{

#if HAVE_GMTIME_R
    gmtime_r( pnTime, poBrokenTime );
#else
    struct tm   *poTime;
    poTime = gmtime( pnTime );
    memcpy( poBrokenTime, poTime, sizeof(tm) );
#endif

    return poBrokenTime;
}

/************************************************************************/
/*                             VSILocalTime()                           */
/************************************************************************/

struct tm *VSILocalTime( const time_t *pnTime, struct tm *poBrokenTime )
{

#if HAVE_LOCALTIME_R
    localtime_r( pnTime, poBrokenTime );
#else
    struct tm   *poTime;
    poTime = localtime( pnTime );
    memcpy( poBrokenTime, poTime, sizeof(tm) );
#endif

    return poBrokenTime;
}

/************************************************************************/
/*                            VSIStrerror()                             */
/************************************************************************/

char *VSIStrerror( int nErrno )

{
    return strerror( nErrno );
}

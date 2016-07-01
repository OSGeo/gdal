/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Simple implementation of POSIX VSI functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_error.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

/* Uncomment to check consistent usage of VSIMalloc(), VSIRealloc(), */
/* VSICalloc(), VSIFree(), VSIStrdup() */
//#define DEBUG_VSIMALLOC

/* Uncomment to compute memory usage statistics. */
/* DEBUG_VSIMALLOC must also be defined */
//#define DEBUG_VSIMALLOC_STATS

/* Highly experimental, and likely buggy. Do not use, except for fixing it! */
/* DEBUG_VSIMALLOC must also be defined */
//#define DEBUG_VSIMALLOC_MPROTECT

#ifdef DEBUG_VSIMALLOC_MPROTECT
#include <sys/mman.h>
#endif

/* Uncomment to print every memory allocation or deallocation. */
/* DEBUG_VSIMALLOC must also be defined */
//#define DEBUG_VSIMALLOC_VERBOSE

/* Number of bytes of the malloc/calloc/free that triggers a debug trace. Can be 0 for all allocs */
#define THRESHOLD_PRINT 10000

/* Uncomment to print GDAL block cache use. */
/* Only used if DEBUG_VSIMALLOC_VERBOSE is enabled */
//#define DEBUG_BLOCK_CACHE_USE

#ifdef DEBUG_BLOCK_CACHE_USE
extern "C" GIntBig CPL_DLL CPL_STDCALL GDALGetCacheUsed64(void);
#endif

CPL_CVSID("$Id$");

/* for stat() */

/* Unix or Windows NT/2000/XP */
#if !defined(WIN32)
#  include <unistd.h>
#else
#  include <io.h>
#  include <fcntl.h>
#  include <direct.h>
#endif

#include <sys/stat.h>
#include <time.h>

/************************************************************************/
/*                              VSIFOpen()                              */
/************************************************************************/

FILE *VSIFOpen( const char * pszFilename, const char * pszAccess )

{
    FILE *fp = NULL;

#if defined(WIN32)
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename =
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );
        wchar_t *pwszAccess =
            CPLRecodeToWChar( pszAccess, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        fp = _wfopen( pwszFilename, pwszAccess );

        CPLFree( pwszFilename );
        CPLFree( pwszAccess );
    }
    else
#endif
    fp = fopen( (char *) pszFilename, (char *) pszAccess );

#ifdef VSI_DEBUG
    // Capture the error from fopen to avoid being overwritten by errors
    // from VSIDebug3.
    const int nError = errno;
    VSIDebug3( "VSIFOpen(%s,%s) = %p", pszFilename, pszAccess, fp );
    errno = nError;
#endif

    return fp;
}

/************************************************************************/
/*                             VSIFClose()                              */
/************************************************************************/

int VSIFClose( FILE * fp )

{
    VSIDebug1( "VSIClose(%p)", fp );

    return fclose(fp);
}

/************************************************************************/
/*                              VSIFSeek()                              */
/************************************************************************/

int VSIFSeek( FILE * fp, long nOffset, int nWhence )

{
#ifdef DEBUG
    /* To workaround Coverity strange warning about potential negative seek */
    /* CID 1340084 when called from dgnwrite.cpp */
    if( nWhence == SEEK_SET && nOffset < 0 )
        return -1;
#endif
    int nResult = fseek( fp, nOffset, nWhence );

#ifdef VSI_DEBUG
    // Capture the error from fseek to avoid being overwritten by errors
    // from VSIDebug.
    const int nError = errno;

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

    errno = nError;
#endif

    return nResult;
}

/************************************************************************/
/*                              VSIFTell()                              */
/************************************************************************/

long VSIFTell( FILE * fp )

{
    const long nOffset = ftell(fp);

#ifdef VSI_DEBUG
    // Capture the error from ftell to avoid being overwritten by errors
    // from VSIDebug.
    const int nError = errno;
    VSIDebug2( "VSIFTell(%p) = %ld", fp, nOffset );
    errno = nError;
#endif

    return nOffset;
}

/************************************************************************/
/*                             VSIRewind()                              */
/************************************************************************/

void VSIRewind( FILE * fp )

{
    VSIDebug1("VSIRewind(%p)", fp );
    rewind( fp );
#ifdef VSI_DEBUG
    // Capture the error rewind ftell to avoid being overwritten by errors
    // from VSIDebug.
    const int nError = errno;
    VSIDebug2( "VSIRewind(%p) errno = %d", fp, nError );
    errno = nError;
#endif
}

/************************************************************************/
/*                              VSIFRead()                              */
/************************************************************************/

size_t VSIFRead( void * pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    const size_t nResult = fread( pBuffer, nSize, nCount, fp );

#ifdef VSI_DEBUG
    // Capture the error from fread to avoid being overwritten by errors
    // from VSIDebug.
    const int nError = errno;
    VSIDebug4( "VSIFRead(%p,%ld,%ld) = %ld",
               fp, static_cast<long>(nSize), static_cast<long>(nCount),
               static_cast<long>(nResult) );
    errno = nError;
#endif

    return nResult;
}

/************************************************************************/
/*                             VSIFWrite()                              */
/************************************************************************/

size_t VSIFWrite( const void *pBuffer, size_t nSize, size_t nCount, FILE * fp )

{
    const size_t nResult = fwrite( pBuffer, nSize, nCount, fp );

#ifdef VSI_DEBUG
    // Capture the error from fwrite to avoid being overwritten by errors
    // from VSIDebug.
    const int nError = errno;
    VSIDebug4( "VSIFWrite(%p,%ld,%ld) = %ld",
               fp, static_cast<long>(nSize), static_cast<long>(nCount),
               static_cast<long>(nResult) );
    errno = nError;
#endif

    return nResult;
}

/************************************************************************/
/*                             VSIFFlush()                              */
/************************************************************************/

void VSIFFlush( FILE * fp )

{
#ifdef VSI_DEBUG
    VSIDebug1( "VSIFFlush(%p)", fp );
    const int result =
#endif
        fflush( fp );

#ifdef VSI_DEBUG
    // Capture the error rewind ftell to avoid being overwritten by errors
    // from VSIDebug.
    const int nError = errno;
    VSIDebug2( "VSIRewind(%p) errno = %d", fp, nError );
    if ( result != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Flush failed.  errno = %d",
                  nError);
    }
    errno = nError;
#endif
}

/************************************************************************/
/*                              VSIFGets()                              */
/************************************************************************/

char *VSIFGets( char *pszBuffer, int nBufferSize, FILE * fp )

{
    return fgets( pszBuffer, nBufferSize, fp );
}

/************************************************************************/
/*                              VSIFGetc()                              */
/************************************************************************/

int VSIFGetc( FILE * fp )

{
    return fgetc( fp );
}

/************************************************************************/
/*                             VSIUngetc()                              */
/************************************************************************/

int VSIUngetc( int c, FILE * fp )

{
    return ungetc( c, fp );
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

    va_start( args, pszFormat );
    const int nReturn = vfprintf( fp, pszFormat, args );
    va_end( args );

    return nReturn;
}

/************************************************************************/
/*                              VSIFEof()                               */
/************************************************************************/

int VSIFEof( FILE * fp )

{
    return feof( fp );
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
    return fputc( nChar, fp );
}


#ifdef DEBUG_VSIMALLOC_STATS
#include "cpl_multiproc.h"

static CPLMutex* hMemStatMutex = 0;
static size_t nCurrentTotalAllocs = 0;
static size_t nMaxTotalAllocs = 0;
static GUIntBig nVSIMallocs = 0;
static GUIntBig nVSICallocs = 0;
static GUIntBig nVSIReallocs = 0;
static GUIntBig nVSIFrees = 0;

/*size_t GetMaxTotalAllocs()
{
    return nMaxTotalAllocs;
}*/

/************************************************************************/
/*                         VSIShowMemStats()                            */
/************************************************************************/

void VSIShowMemStats();

void VSIShowMemStats()
{
    char* pszShowMemStats = getenv("CPL_SHOW_MEM_STATS");
    if (pszShowMemStats == NULL || pszShowMemStats[0] == '\0' )
        return;
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

#ifdef DEBUG_VSIMALLOC
static GIntBig nMaxPeakAllocSize = -1;
static GIntBig nMaxCumulAllocSize = -1;
#endif

/************************************************************************/
/*                             VSICalloc()                              */
/************************************************************************/

void *VSICalloc( size_t nCount, size_t nSize )

{
#ifdef DEBUG_VSIMALLOC
    size_t nMul = nCount * nSize;
    if (nCount != 0 && nMul / nCount != nSize)
    {
        fprintf(stderr, "Overflow in VSICalloc(%d, %d)\n",
                (int)nCount, (int)nSize);
        return NULL;
    }
    if (nMaxPeakAllocSize < 0)
    {
        char* pszMaxPeakAllocSize = getenv("CPL_MAX_PEAK_ALLOC_SIZE");
        nMaxPeakAllocSize = (pszMaxPeakAllocSize) ? atoi(pszMaxPeakAllocSize) : 0;
        char* pszMaxCumulAllocSize = getenv("CPL_MAX_CUMUL_ALLOC_SIZE");
        nMaxCumulAllocSize = (pszMaxCumulAllocSize) ? atoi(pszMaxCumulAllocSize) : 0;
    }
    if (nMaxPeakAllocSize > 0 && (GIntBig)nMul > nMaxPeakAllocSize)
        return NULL;
#ifdef DEBUG_VSIMALLOC_STATS
    if (nMaxCumulAllocSize > 0 && (GIntBig)nCurrentTotalAllocs + (GIntBig)nMul > nMaxCumulAllocSize)
        return NULL;
#endif

#ifdef DEBUG_VSIMALLOC_MPROTECT
    char* ptr = NULL;
    size_t nPageSize = getpagesize();
    posix_memalign((void**)&ptr, nPageSize, (3 * sizeof(void*) + nMul + nPageSize - 1) & ~(nPageSize - 1));
    if (ptr == NULL)
        return NULL;
    memset(ptr + 2 * sizeof(void*), 0, nMul);
#else
    char* ptr = (char*) calloc(1, 3 * sizeof(void*) + nMul);
    if (ptr == NULL)
        return NULL;
#endif

    ptr[0] = 'V';
    ptr[1] = 'S';
    ptr[2] = 'I';
    ptr[3] = 'M';
    memcpy(ptr + sizeof(void*), &nMul, sizeof(void*));
    ptr[2 * sizeof(void*) + nMul + 0] = 'E';
    ptr[2 * sizeof(void*) + nMul + 1] = 'V';
    ptr[2 * sizeof(void*) + nMul + 2] = 'S';
    ptr[2 * sizeof(void*) + nMul + 3] = 'I';
#if defined(DEBUG_VSIMALLOC_STATS) || defined(DEBUG_VSIMALLOC_VERBOSE)
    {
        CPLMutexHolderD(&hMemStatMutex);
#ifdef DEBUG_VSIMALLOC_VERBOSE
        if( nMul > THRESHOLD_PRINT )
        {
            fprintf(stderr, "Thread[%p] VSICalloc(%d,%d) = %p"
#ifdef DEBUG_VSIMALLOC_STATS
                         ", current_cumul = " CPL_FRMT_GUIB
#ifdef DEBUG_BLOCK_CACHE_USE
                         ", block_cache_used = " CPL_FRMT_GIB
#endif
                         ", mal+cal-free = %d"
#endif
                         "\n",
                (void*)CPLGetPID(), (int)nCount, (int)nSize, ptr + 2 * sizeof(void*)
#ifdef DEBUG_VSIMALLOC_STATS
                , (GUIntBig)(nCurrentTotalAllocs + nMul)
#ifdef DEBUG_BLOCK_CACHE_USE
                , GDALGetCacheUsed64()
#endif
                ,(int)(nVSIMallocs + nVSICallocs - nVSIFrees)
#endif
                );
        }
#endif
#ifdef DEBUG_VSIMALLOC_STATS
        nVSICallocs ++;
        if (nMaxTotalAllocs == 0)
            atexit(VSIShowMemStats);
        nCurrentTotalAllocs += nMul;
        if (nCurrentTotalAllocs > nMaxTotalAllocs)
            nMaxTotalAllocs = nCurrentTotalAllocs;
#endif
    }
#endif
    return ptr + 2 * sizeof(void*);
#else
    return calloc( nCount, nSize );
#endif
}

/************************************************************************/
/*                             VSIMalloc()                              */
/************************************************************************/

#ifndef DEBUG_VSIMALLOC
void *VSIMalloc( size_t nSize )

{
    return malloc( nSize );
}

#else  // DEBUG_VSIMALLOC

void *VSIMalloc( size_t nSize )

{
    if (nMaxPeakAllocSize < 0)
    {
        char* pszMaxPeakAllocSize = getenv("CPL_MAX_PEAK_ALLOC_SIZE");
        nMaxPeakAllocSize = (pszMaxPeakAllocSize) ? atoi(pszMaxPeakAllocSize) : 0;
        char* pszMaxCumulAllocSize = getenv("CPL_MAX_CUMUL_ALLOC_SIZE");
        nMaxCumulAllocSize = (pszMaxCumulAllocSize) ? atoi(pszMaxCumulAllocSize) : 0;
    }
    if (nMaxPeakAllocSize > 0 && (GIntBig)nSize > nMaxPeakAllocSize)
        return NULL;
#ifdef DEBUG_VSIMALLOC_STATS
    if (nMaxCumulAllocSize > 0 && (GIntBig)nCurrentTotalAllocs + (GIntBig)nSize > nMaxCumulAllocSize)
        return NULL;
#endif  // DEBUG_VSIMALLOC_STATS

#ifdef DEBUG_VSIMALLOC_MPROTECT
    char* ptr = NULL;
    size_t nPageSize = getpagesize();
    posix_memalign((void**)&ptr, nPageSize, (3 * sizeof(void*) + nSize + nPageSize - 1) & ~(nPageSize - 1));
#else
    char* ptr = (char*) malloc(3 * sizeof(void*) + nSize);
#endif  // DEBUG_VSIMALLOC_MPROTECT
    if (ptr == NULL)
        return NULL;
    ptr[0] = 'V';
    ptr[1] = 'S';
    ptr[2] = 'I';
    ptr[3] = 'M';
    memcpy(ptr + sizeof(void*), &nSize, sizeof(void*));
    ptr[2 * sizeof(void*) + nSize + 0] = 'E';
    ptr[2 * sizeof(void*) + nSize + 1] = 'V';
    ptr[2 * sizeof(void*) + nSize + 2] = 'S';
    ptr[2 * sizeof(void*) + nSize + 3] = 'I';
#if defined(DEBUG_VSIMALLOC_STATS) || defined(DEBUG_VSIMALLOC_VERBOSE)
    {
        CPLMutexHolderD(&hMemStatMutex);
#ifdef DEBUG_VSIMALLOC_VERBOSE
        if( nSize > THRESHOLD_PRINT )
        {
            fprintf(stderr, "Thread[%p] VSIMalloc(%d) = %p"
#ifdef DEBUG_VSIMALLOC_STATS
                         ", current_cumul = " CPL_FRMT_GUIB
#ifdef DEBUG_BLOCK_CACHE_USE
                         ", block_cache_used = " CPL_FRMT_GIB
#endif
                         ", mal+cal-free = %d"
#endif
                         "\n",
                (void*)CPLGetPID(), (int)nSize, ptr + 2 * sizeof(void*)
#ifdef DEBUG_VSIMALLOC_STATS
                , (GUIntBig)(nCurrentTotalAllocs + nSize)
#ifdef DEBUG_BLOCK_CACHE_USE
                , GDALGetCacheUsed64()
#endif
                ,(int)(nVSIMallocs + nVSICallocs - nVSIFrees)
#endif
                );
        }
#endif  // DEBUG_VSIMALLOC_VERBOSE
#ifdef DEBUG_VSIMALLOC_STATS
        nVSIMallocs ++;
        if (nMaxTotalAllocs == 0)
            atexit(VSIShowMemStats);
        nCurrentTotalAllocs += nSize;
        if (nCurrentTotalAllocs > nMaxTotalAllocs)
            nMaxTotalAllocs = nCurrentTotalAllocs;
#endif  // DEBUG_VSIMALLOC_STATS
    }
#endif  // DEBUG_VSIMALLOC_STATS || DEBUG_VSIMALLOC_VERBOSE
    return ptr + 2 * sizeof(void*);
}

static void VSICheckMarkerBegin(char* ptr)
{
    if (memcmp(ptr, "VSIM", 4) != 0)
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "Inconsistent use of VSI memory allocation primitives for %p : %c%c%c%c",
                 ptr, ptr[0], ptr[1], ptr[2], ptr[3]);
    }
}

static void VSICheckMarkerEnd(char* ptr, size_t nEnd)
{
    if (memcmp(ptr + nEnd, "EVSI", 4) != 0)
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "Memory has been written after the end of %p", ptr);
    }
}

#endif  // DEBUG_VSIMALLOC

/************************************************************************/
/*                             VSIRealloc()                             */
/************************************************************************/

void * VSIRealloc( void * pData, size_t nNewSize )

{
#ifdef DEBUG_VSIMALLOC
    if (pData == NULL)
        return VSIMalloc(nNewSize);

    char* ptr = ((char*)pData) - 2 * sizeof(void*);
    VSICheckMarkerBegin(ptr);

    size_t nOldSize = 0;
    memcpy(&nOldSize, ptr + sizeof(void*), sizeof(void*));
    VSICheckMarkerEnd(ptr, 2 * sizeof(void*) + nOldSize);

    if (nMaxPeakAllocSize < 0)
    {
        char* pszMaxPeakAllocSize = getenv("CPL_MAX_PEAK_ALLOC_SIZE");
        nMaxPeakAllocSize = (pszMaxPeakAllocSize) ? atoi(pszMaxPeakAllocSize) : 0;
    }
    if (nMaxPeakAllocSize > 0 && (GIntBig)nNewSize > nMaxPeakAllocSize)
        return NULL;
#ifdef DEBUG_VSIMALLOC_STATS
    if (nMaxCumulAllocSize > 0 && (GIntBig)nCurrentTotalAllocs + (GIntBig)nNewSize - (GIntBig)nOldSize > nMaxCumulAllocSize)
        return NULL;
#endif

    ptr[2 * sizeof(void*) + nOldSize + 0] = 'I';
    ptr[2 * sizeof(void*) + nOldSize + 1] = 'S';
    ptr[2 * sizeof(void*) + nOldSize + 2] = 'V';
    ptr[2 * sizeof(void*) + nOldSize + 3] = 'E';

#ifdef DEBUG_VSIMALLOC_MPROTECT
    char* newptr = NULL;
    size_t nPageSize = getpagesize();
    posix_memalign((void**)&newptr, nPageSize, (nNewSize + 3 * sizeof(void*) + nPageSize - 1) & ~(nPageSize - 1));
    if (newptr == NULL)
    {
        ptr[2 * sizeof(void*) + nOldSize + 0] = 'E';
        ptr[2 * sizeof(void*) + nOldSize + 1] = 'V';
        ptr[2 * sizeof(void*) + nOldSize + 2] = 'S';
        ptr[2 * sizeof(void*) + nOldSize + 3] = 'I';
        return NULL;
    }
    memcpy(newptr + 2 * sizeof(void*), pData, nOldSize);
    ptr[0] = 'M';
    ptr[1] = 'I';
    ptr[2] = 'S';
    ptr[3] = 'V';
    free(ptr);
    newptr[0] = 'V';
    newptr[1] = 'S';
    newptr[2] = 'I';
    newptr[3] = 'M';
#else
    void* newptr = realloc(ptr, nNewSize + 3 * sizeof(void*));
    if (newptr == NULL)
    {
        ptr[2 * sizeof(void*) + nOldSize + 0] = 'E';
        ptr[2 * sizeof(void*) + nOldSize + 1] = 'V';
        ptr[2 * sizeof(void*) + nOldSize + 2] = 'S';
        ptr[2 * sizeof(void*) + nOldSize + 3] = 'I';
        return NULL;
    }
#endif
    ptr = (char*) newptr;
    memcpy(ptr + sizeof(void*), &nNewSize, sizeof(void*));
    ptr[2 * sizeof(void*) + nNewSize + 0] = 'E';
    ptr[2 * sizeof(void*) + nNewSize + 1] = 'V';
    ptr[2 * sizeof(void*) + nNewSize + 2] = 'S';
    ptr[2 * sizeof(void*) + nNewSize + 3] = 'I';

#if defined(DEBUG_VSIMALLOC_STATS) || defined(DEBUG_VSIMALLOC_VERBOSE)
    {
        CPLMutexHolderD(&hMemStatMutex);
#ifdef DEBUG_VSIMALLOC_VERBOSE
        if( nNewSize > THRESHOLD_PRINT )
        {
            fprintf(stderr, "Thread[%p] VSIRealloc(%p, %d) = %p"
#ifdef DEBUG_VSIMALLOC_STATS
                         ", current_cumul = " CPL_FRMT_GUIB
#ifdef DEBUG_BLOCK_CACHE_USE
                         ", block_cache_used = " CPL_FRMT_GIB
#endif
                         ", mal+cal-free = %d"
#endif
                         "\n",
                (void*)CPLGetPID(), pData, (int)nNewSize, ptr + 2 * sizeof(void*)
#ifdef DEBUG_VSIMALLOC_STATS
                , (GUIntBig)(nCurrentTotalAllocs - nOldSize + nNewSize)
#ifdef DEBUG_BLOCK_CACHE_USE
                , GDALGetCacheUsed64()
#endif
                ,(int)(nVSIMallocs + nVSICallocs - nVSIFrees)
#endif
                );
        }
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
    return ptr + 2 * sizeof(void*);
#else
    return realloc( pData, nNewSize );
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

    char* ptr = ((char*)pData) - 2 * sizeof(void*);
    VSICheckMarkerBegin(ptr);
    size_t nOldSize = 0;
    memcpy(&nOldSize, ptr + sizeof(void*), sizeof(void*));
    VSICheckMarkerEnd(ptr, 2 * sizeof(void*) + nOldSize);
    ptr[0] = 'M';
    ptr[1] = 'I';
    ptr[2] = 'S';
    ptr[3] = 'V';
    ptr[2 * sizeof(void*) + nOldSize + 0] = 'I';
    ptr[2 * sizeof(void*) + nOldSize + 1] = 'S';
    ptr[2 * sizeof(void*) + nOldSize + 2] = 'V';
    ptr[2 * sizeof(void*) + nOldSize + 3] = 'E';
#if defined(DEBUG_VSIMALLOC_STATS) || defined(DEBUG_VSIMALLOC_VERBOSE)
    {
        CPLMutexHolderD(&hMemStatMutex);
#ifdef DEBUG_VSIMALLOC_VERBOSE
        if( nOldSize > THRESHOLD_PRINT )
        {
            fprintf(stderr, "Thread[%p] VSIFree(%p, (%d bytes))\n",
                    (void*)CPLGetPID(), pData, (int)nOldSize);
        }
#endif
#ifdef DEBUG_VSIMALLOC_STATS
        nVSIFrees ++;
        nCurrentTotalAllocs -= nOldSize;
#endif
    }
#endif

#ifdef DEBUG_VSIMALLOC_MPROTECT
    mprotect(ptr, nOldSize + 2 * sizeof(void*), PROT_NONE);
#else
    free(ptr);
#endif

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
    const size_t nSize = strlen(pszString) + 1;
    char* ptr = static_cast<char*>( VSIMalloc(nSize) );
    if (ptr == NULL)
        return NULL;
    memcpy(ptr, pszString, nSize);
    return ptr;
}

/************************************************************************/
/*                          VSICheckMul2()                              */
/************************************************************************/

static size_t VSICheckMul2( size_t mul1, size_t mul2, int *pbOverflowFlag,
                            const char* pszFile, int nLine )
{
    const size_t res = mul1 * mul2;
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
                     "%s: %d: Multiplication overflow : " CPL_FRMT_GUIB " * " CPL_FRMT_GUIB,
                     pszFile ? pszFile : "(unknown file)",
                     nLine,
                     (GUIntBig)mul1, (GUIntBig)mul2);
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

static size_t VSICheckMul3( size_t mul1, size_t mul2, size_t mul3, int *pbOverflowFlag,
                            const char* pszFile, int nLine )
{
    if (mul1 != 0)
    {
        const size_t res = mul1 * mul2;
        if (res / mul1 == mul2)
        {
            const size_t res2 = res * mul3;
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
                            "%s: %d: Multiplication overflow : " CPL_FRMT_GUIB " * " CPL_FRMT_GUIB " * " CPL_FRMT_GUIB,
                            pszFile ? pszFile : "(unknown file)",
                            nLine,
                            (GUIntBig)mul1, (GUIntBig)mul2, (GUIntBig)mul3);
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
                    "%s: %d: Multiplication overflow : " CPL_FRMT_GUIB " * " CPL_FRMT_GUIB " * " CPL_FRMT_GUIB,
                    pszFile ? pszFile : "(unknown file)",
                    nLine,
                    (GUIntBig)mul1, (GUIntBig)mul2, (GUIntBig)mul3);
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
    return VSIMalloc2Verbose( nSize1, nSize2, NULL, 0);
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
    return VSIMalloc3Verbose( nSize1, nSize2, nSize3, NULL, 0);
}

/************************************************************************/
/*                          VSIMallocVerbose()                          */
/************************************************************************/

void *VSIMallocVerbose( size_t nSize, const char* pszFile, int nLine )
{
    void* pRet = VSIMalloc(nSize);
    if( pRet == NULL && nSize != 0 )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "%s, %d: cannot allocate " CPL_FRMT_GUIB " bytes",
                 pszFile ? pszFile : "(unknown file)",
                 nLine, (GUIntBig)nSize);
    }
    return pRet;
}

/************************************************************************/
/*                          VSIMalloc2Verbose()                         */
/************************************************************************/

void *VSIMalloc2Verbose( size_t nSize1, size_t nSize2, const char* pszFile, int nLine )
{
    int bOverflowFlag = FALSE;
    size_t nSizeToAllocate = VSICheckMul2( nSize1, nSize2, &bOverflowFlag, pszFile, nLine );
    if (bOverflowFlag || nSizeToAllocate == 0)
        return NULL;

    void* pRet = VSIMalloc(nSizeToAllocate);
    if( pRet == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "%s, %d: cannot allocate " CPL_FRMT_GUIB " bytes",
                 pszFile ? pszFile : "(unknown file)",
                 nLine, (GUIntBig)nSize1 * (GUIntBig)nSize2);
    }
    return pRet;
}

/************************************************************************/
/*                          VSIMalloc3Verbose()                         */
/************************************************************************/

void *VSIMalloc3Verbose( size_t nSize1, size_t nSize2, size_t nSize3,
                         const char* pszFile, int nLine )
{
    int bOverflowFlag = FALSE;
    size_t nSizeToAllocate = VSICheckMul3( nSize1, nSize2, nSize3,
                                           &bOverflowFlag, pszFile, nLine );
    if (bOverflowFlag || nSizeToAllocate == 0)
        return NULL;

    void* pRet = VSIMalloc(nSizeToAllocate);
    if( pRet == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "%s, %d: cannot allocate " CPL_FRMT_GUIB " bytes",
                 pszFile ? pszFile : "(unknown file)",
                 nLine, (GUIntBig)nSize1 * (GUIntBig)nSize2 * (GUIntBig)nSize3);
    }
    return pRet;
}
/************************************************************************/
/*                          VSICallocVerbose()                          */
/************************************************************************/

void *VSICallocVerbose(  size_t nCount, size_t nSize, const char* pszFile, int nLine )
{
    void* pRet = VSICalloc(nCount, nSize);
    if( pRet == NULL && nCount != 0 && nSize != 0 )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "%s, %d: cannot allocate " CPL_FRMT_GUIB " bytes",
                 pszFile ? pszFile : "(unknown file)",
                 nLine, (GUIntBig)nCount * (GUIntBig)nSize);
    }
    return pRet;
}

/************************************************************************/
/*                          VSIReallocVerbose()                         */
/************************************************************************/

void *VSIReallocVerbose( void* pOldPtr, size_t nNewSize, const char* pszFile, int nLine )
{
    void* pRet = VSIRealloc(pOldPtr, nNewSize);
    if( pRet == NULL && nNewSize != 0 )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "%s, %d: cannot allocate " CPL_FRMT_GUIB " bytes",
                 pszFile ? pszFile : "(unknown file)",
                 nLine, (GUIntBig)(nNewSize));
    }
    return pRet;
}

/************************************************************************/
/*                          VSIStrdupVerbose()                          */
/************************************************************************/

char *VSIStrdupVerbose(  const char* pszStr, const char* pszFile, int nLine )
{
    char* pRet = VSIStrdup(pszStr);
    if( pRet == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "%s, %d: cannot allocate " CPL_FRMT_GUIB " bytes",
                 pszFile ? pszFile : "(unknown file)",
                 nLine, (GUIntBig)(strlen(pszStr)+1));
    }
    return pRet;
}

/************************************************************************/
/*                              VSIStat()                               */
/************************************************************************/

int VSIStat( const char * pszFilename, VSIStatBuf * pStatBuf )

{
#if defined(WIN32)
    if( CSLTestBoolean(
            CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        wchar_t *pwszFilename =
            CPLRecodeToWChar( pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2 );

        int nResult = _wstat( pwszFilename, (struct _stat *) pStatBuf );

        CPLFree( pwszFilename );

        return nResult;
    }

#endif
    return stat( pszFilename, pStatBuf );
}

/************************************************************************/
/*                              VSITime()                               */
/************************************************************************/

unsigned long VSITime( unsigned long * pnTimeToSet )

{
    time_t tTime;

    tTime = time( NULL );

    if( pnTimeToSet != NULL )
        *pnTimeToSet = static_cast<unsigned long>( tTime );

    return static_cast<unsigned long>( tTime );
}

/************************************************************************/
/*                              VSICTime()                              */
/************************************************************************/

const char *VSICTime( unsigned long nTime )

{
    time_t tTime = (time_t) nTime;

    return reinterpret_cast<const char *>(ctime( &tTime ));
}

/************************************************************************/
/*                             VSIGMTime()                              */
/************************************************************************/

struct tm *VSIGMTime( const time_t *pnTime, struct tm *poBrokenTime )
{

#if HAVE_GMTIME_R
    gmtime_r( pnTime, poBrokenTime );
#else
    struct tm *poTime = gmtime( pnTime );
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
    struct tm *poTime = localtime( pnTime );
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


/************************************************************************/
/*                        CPLGetPhysicalRAM()                           */
/************************************************************************/

#if HAVE_SC_PHYS_PAGES

/** Return the total physical RAM in bytes.
 *
 * @return the total physical RAM in bytes (or 0 in case of failure).
 * @since GDAL 2.0
 */
GIntBig CPLGetPhysicalRAM(void)
{
    return static_cast<GIntBig>(sysconf(_SC_PHYS_PAGES))
        * sysconf(_SC_PAGESIZE);
}

#elif defined(__MACH__) && defined(__APPLE__)

#include <sys/types.h>
#include <sys/sysctl.h>

GIntBig CPLGetPhysicalRAM(void)
{
    GIntBig nPhysMem = 0;

    int mib[2] = { CTL_HW, HW_MEMSIZE };
    size_t nLengthRes = sizeof(nPhysMem);
    sysctl(mib, CPL_ARRAYSIZE(mib), &nPhysMem, &nLengthRes, NULL, 0);

    return nPhysMem;
}

#elif defined(WIN32)

/* GlobalMemoryStatusEx requires _WIN32_WINNT >= 0x0500 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>

GIntBig CPLGetPhysicalRAM(void)
{
    MEMORYSTATUSEX statex;
    statex.ullTotalPhys = 0;
    statex.dwLength = sizeof (statex);
    GlobalMemoryStatusEx (&statex);
    return static_cast<GIntBig>( statex.ullTotalPhys );
}

#else

GIntBig CPLGetPhysicalRAM(void)
{
    static bool bOnce = false;
    if( !bOnce )
    {
        bOnce = true;
        CPLDebug("PORT", "No implementation for CPLGetPhysicalRAM()");
    }
    return 0;
}
#endif

/************************************************************************/
/*                       CPLGetUsablePhysicalRAM()                      */
/************************************************************************/

/** Return the total physical RAM, usable by a process, in bytes.
 *
 * This is the same as CPLGetPhysicalRAM() except it will limit to 2 GB
 * for 32 bit processes.
 *
 * Note: This memory may already be partly used by other processes.
 *
 * @return the total physical RAM, usable by a process, in bytes (or 0 in case of failure).
 * @since GDAL 2.0
 */
GIntBig  CPLGetUsablePhysicalRAM(void)
{
    GIntBig nRAM = CPLGetPhysicalRAM();
#if SIZEOF_VOIDP == 4
    if( nRAM > INT_MAX )
        nRAM = INT_MAX;
#endif
    return nRAM;
}

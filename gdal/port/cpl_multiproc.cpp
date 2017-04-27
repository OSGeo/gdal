/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Multi-Threading, and process handling portability functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// Include cpl_config.h BEFORE cpl_multiproc.h, as the later may undefine
// CPL_MULTIPROC_PTHREAD for mingw case.

#include "cpl_config.h"
#include "cpl_multiproc.h"

#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
#  include <cassert>
#endif
#include <cerrno>
#ifndef DEBUG_BOOL
#include <cmath>
#endif
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$");

#if defined(CPL_MULTIPROC_STUB) && !defined(DEBUG)
#  define MUTEX_NONE
#endif

// #define DEBUG_MUTEX

#if defined(DEBUG) && (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
#ifndef DEBUG_CONTENTION
#define DEBUG_CONTENTION
#endif
#endif

typedef struct _CPLSpinLock CPLSpinLock;

struct _CPLLock
{
    CPLLockType eType;
    union
    {
        CPLMutex        *hMutex;
        CPLSpinLock     *hSpinLock;
    } u;

#ifdef DEBUG_CONTENTION
    bool     bDebugPerf;
    GUIntBig nStartTime;
    GIntBig  nMaxDiff;
    double   dfAvgDiff;
    GUIntBig nIters;
#endif
};

#ifdef DEBUG_CONTENTION
static GUIntBig CPLrdtsc()
{
    unsigned int a;
    unsigned int d;
    unsigned int x;
    unsigned int y;
    __asm__ volatile ("cpuid" : "=a" (x), "=d" (y) : "a"(0) : "cx", "bx" );
    __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d) );
    return static_cast<GUIntBig>(a) | (static_cast<GUIntBig>(d) << 32);
}

static GUIntBig CPLrdtscp()
{
    unsigned int a;
    unsigned int d;
    unsigned int x;
    unsigned int y;
    __asm__ volatile ("rdtscp" : "=a" (a), "=d" (d) );
    __asm__ volatile ("cpuid"  : "=a" (x), "=d" (y) : "a"(0) : "cx", "bx" );
    return static_cast<GUIntBig>(a) | (static_cast<GUIntBig>(d) << 32);
}
#endif

static CPLSpinLock *CPLCreateSpinLock();  // Returned NON acquired.
static int     CPLCreateOrAcquireSpinLockInternal( CPLLock** );
static int     CPLAcquireSpinLock( CPLSpinLock* );
static void    CPLReleaseSpinLock( CPLSpinLock* );
static void    CPLDestroySpinLock( CPLSpinLock* );

// We don't want it to be publicly used since it solves rather tricky issues
// that are better to remain hidden.
void CPLFinalizeTLS();

/************************************************************************/
/*                           CPLMutexHolder()                           */
/************************************************************************/

#ifdef MUTEX_NONE
CPLMutexHolder::CPLMutexHolder( CPLMutex ** /* phMutex */,
                                double /* dfWaitInSeconds */,
                                const char * /* pszFileIn */,
                                int /* nLineIn */,
                                int /* nOptions */ ) {}

#else
CPLMutexHolder::CPLMutexHolder( CPLMutex **phMutex,
                                double dfWaitInSeconds,
                                const char *pszFileIn,
                                int nLineIn,
                                int nOptions ) :
    hMutex(NULL),
    pszFile(pszFileIn),
    nLine(nLineIn)
{
    if( phMutex == NULL )
    {
        fprintf( stderr, "CPLMutexHolder: phMutex )) NULL !\n" );
        hMutex = NULL;
        return;
    }

#ifdef DEBUG_MUTEX
    // There is no way to use CPLDebug() here because it works with
    // mutexes itself so we will fall in infinite recursion.
    // fprintf() will do the job right.
    fprintf( stderr,
             "CPLMutexHolder: Request %p for pid %ld at %d/%s.\n",
             *phMutex, static_cast<long>(CPLGetPID()), nLine, pszFile );
#else
    // TODO(schwehr): Find a better way to do handle this.
    (void)pszFile;
    (void)nLine;
#endif

    if( !CPLCreateOrAcquireMutexEx( phMutex, dfWaitInSeconds, nOptions ) )
    {
        fprintf( stderr, "CPLMutexHolder: Failed to acquire mutex!\n" );
        hMutex = NULL;
    }
    else
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "CPLMutexHolder: Acquired %p for pid %ld at %d/%s.\n",
                 *phMutex, static_cast<long>(CPLGetPID()), nLine, pszFile );
#endif

        hMutex = *phMutex;
    }
}
#endif  // ndef MUTEX_NONE

/************************************************************************/
/*                           CPLMutexHolder()                           */
/************************************************************************/

#ifdef MUTEX_NONE
CPLMutexHolder::CPLMutexHolder( CPLMutex * /* hMutexIn */,
                                double /* dfWaitInSeconds */,
                                const char * /* pszFileIn */,
                                int /* nLineIn */ ) {}
#else
CPLMutexHolder::CPLMutexHolder( CPLMutex *hMutexIn, double dfWaitInSeconds,
                                const char *pszFileIn,
                                int nLineIn ) :
    hMutex(hMutexIn),
    pszFile(pszFileIn),
    nLine(nLineIn)
{
    if( hMutex != NULL &&
        !CPLAcquireMutex( hMutex, dfWaitInSeconds ) )
    {
        fprintf( stderr, "CPLMutexHolder: Failed to acquire mutex!\n" );
        hMutex = NULL;
    }
}
#endif  // ndef MUTEX_NONE

/************************************************************************/
/*                          ~CPLMutexHolder()                           */
/************************************************************************/

#ifdef MUTEX_NONE
CPLMutexHolder::~CPLMutexHolder() {}
#else
CPLMutexHolder::~CPLMutexHolder()
{
    if( hMutex != NULL )
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "~CPLMutexHolder: Release %p for pid %ld at %d/%s.\n",
                 hMutex, static_cast<long>(CPLGetPID()), nLine, pszFile );
#endif
        CPLReleaseMutex( hMutex );
    }
}
#endif  // ndef MUTEX_NONE

int CPLCreateOrAcquireMutex( CPLMutex **phMutex, double dfWaitInSeconds )
{
    return
        CPLCreateOrAcquireMutexEx(phMutex, dfWaitInSeconds,
                                  CPL_MUTEX_RECURSIVE);
}

/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/

#ifndef CPL_MULTIPROC_PTHREAD

#ifndef MUTEX_NONE
static CPLMutex *hCOAMutex = NULL;
#endif

#ifdef MUTEX_NONE

int CPLCreateOrAcquireMutexEx( CPLMutex **phMutex, double dfWaitInSeconds,
                               int nOptions )
{
    return false;
}
#else
int CPLCreateOrAcquireMutexEx( CPLMutex **phMutex, double dfWaitInSeconds,
                               int nOptions )
{
    bool bSuccess = false;

    // Ironically, creation of this initial mutex is not threadsafe
    // even though we use it to ensure that creation of other mutexes
    // is threadsafe.
    if( hCOAMutex == NULL )
    {
        hCOAMutex = CPLCreateMutex();
        if( hCOAMutex == NULL )
        {
            *phMutex = NULL;
            return FALSE;
        }
    }
    else
    {
        CPLAcquireMutex( hCOAMutex, dfWaitInSeconds );
    }

    if( *phMutex == NULL )
    {
        *phMutex = CPLCreateMutexEx( nOptions );
        bSuccess = *phMutex != NULL;
        CPLReleaseMutex( hCOAMutex );
    }
    else
    {
        CPLReleaseMutex( hCOAMutex );

        bSuccess = CPL_TO_BOOL(CPLAcquireMutex( *phMutex, dfWaitInSeconds ));
    }

    return bSuccess;
}
#endif  // ndef MUTEX_NONE

/************************************************************************/
/*                   CPLCreateOrAcquireMutexInternal()                  */
/************************************************************************/

#ifdef MUTEX_NONE
static
int CPLCreateOrAcquireMutexInternal( CPLLock **phLock, double dfWaitInSeconds,
                                     CPLLockType eType )
{
    return false;
}
#else
static
int CPLCreateOrAcquireMutexInternal( CPLLock **phLock, double dfWaitInSeconds,
                                     CPLLockType eType )

{
    bool bSuccess = false;

    // Ironically, creation of this initial mutex is not threadsafe
    // even though we use it to ensure that creation of other mutexes.
    // is threadsafe.
    if( hCOAMutex == NULL )
    {
        hCOAMutex = CPLCreateMutex();
        if( hCOAMutex == NULL )
        {
            *phLock = NULL;
            return FALSE;
        }
    }
    else
    {
        CPLAcquireMutex( hCOAMutex, dfWaitInSeconds );
    }

    if( *phLock == NULL )
    {
        *phLock = static_cast<CPLLock *>(calloc(1, sizeof(CPLLock)));
        if( *phLock )
        {
            (*phLock)->eType = eType;
            (*phLock)->u.hMutex = CPLCreateMutexEx(
                (eType == LOCK_RECURSIVE_MUTEX)
                ? CPL_MUTEX_RECURSIVE
                : CPL_MUTEX_ADAPTIVE );
            if( (*phLock)->u.hMutex == NULL )
            {
                free(*phLock);
                *phLock = NULL;
            }
        }
        bSuccess = *phLock != NULL;
        CPLReleaseMutex( hCOAMutex );
    }
    else
    {
        CPLReleaseMutex( hCOAMutex );

        bSuccess =
            CPL_TO_BOOL(CPLAcquireMutex( (*phLock)->u.hMutex,
                                         dfWaitInSeconds ));
    }

    return bSuccess;
}
#endif  // ndef MUTEX_NONE

#endif  // CPL_MULTIPROC_PTHREAD

/************************************************************************/
/*                      CPLCleanupMasterMutex()                         */
/************************************************************************/

void CPLCleanupMasterMutex()
{
#ifndef CPL_MULTIPROC_PTHREAD
#ifndef MUTEX_NONE
    if( hCOAMutex != NULL )
    {
        CPLDestroyMutex( hCOAMutex );
        hCOAMutex = NULL;
    }
#endif
#endif
}

/************************************************************************/
/*                        CPLCleanupTLSList()                           */
/*                                                                      */
/*      Free resources associated with a TLS vector (implementation     */
/*      independent).                                                   */
/************************************************************************/

static void CPLCleanupTLSList( void **papTLSList )

{
#ifdef DEBUG_VERBOSE
    printf( "CPLCleanupTLSList(%p)\n", papTLSList );  /*ok*/
#endif

    if( papTLSList == NULL )
        return;

    for( int i = 0; i < CTLS_MAX; i++ )
    {
        if( papTLSList[i] != NULL && papTLSList[i+CTLS_MAX] != NULL )
        {
            CPLTLSFreeFunc pfnFree = (CPLTLSFreeFunc) papTLSList[i + CTLS_MAX];
            pfnFree( papTLSList[i] );
            papTLSList[i] = NULL;
        }
    }

    CPLFree( papTLSList );
}

#if defined(CPL_MULTIPROC_STUB)
/************************************************************************/
/* ==================================================================== */
/*                        CPL_MULTIPROC_STUB                            */
/*                                                                      */
/*      Stub implementation.  Mutexes don't provide exclusion, file     */
/*      locking is achieved with extra "lock files", and thread         */
/*      creation doesn't work.  The PID is always just one.             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             CPLGetNumCPUs()                          */
/************************************************************************/

int CPLGetNumCPUs()
{
    return 1;
}

/************************************************************************/
/*                        CPLGetThreadingModel()                        */
/************************************************************************/

const char *CPLGetThreadingModel()

{
    return "stub";
}

/************************************************************************/
/*                           CPLCreateMutex()                           */
/************************************************************************/

#ifdef MUTEX_NONE
CPLMutex *CPLCreateMutex()
{
    return (CPLMutex *) 0xdeadbeef;
}
#else
CPLMutex *CPLCreateMutex()
{
    unsigned char *pabyMutex = static_cast<unsigned char *>(malloc(4));
    if( pabyMutex == NULL )
        return NULL;

    pabyMutex[0] = 1;
    pabyMutex[1] = 'r';
    pabyMutex[2] = 'e';
    pabyMutex[3] = 'd';

    return (CPLMutex *) pabyMutex;
}
#endif

CPLMutex *CPLCreateMutexEx( int /*nOptions*/ )

{
    return CPLCreateMutex();
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

#ifdef MUTEX_NONE
int CPLAcquireMutex( CPLMutex *hMutex, double /* dfWaitInSeconds */ )
{
    return TRUE;
}
#else
int CPLAcquireMutex( CPLMutex *hMutex, double /*dfWaitInSeconds*/ )
{
    unsigned char *pabyMutex = reinterpret_cast<unsigned char *>(hMutex);

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e'
               && pabyMutex[3] == 'd' );

    pabyMutex[0] += 1;

    return TRUE;
}
#endif  // ! MUTEX_NONE

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

#ifdef MUTEX_NONE
void CPLReleaseMutex( CPLMutex * /* hMutex */ ) {}
#else
void CPLReleaseMutex( CPLMutex *hMutex )
{
    unsigned char *pabyMutex = reinterpret_cast<unsigned char *>(hMutex);

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e'
               && pabyMutex[3] == 'd' );

    if( pabyMutex[0] < 1 )
        CPLDebug( "CPLMultiProc",
                  "CPLReleaseMutex() called on mutex with %d as ref count!",
                  pabyMutex[0] );

    pabyMutex[0] -= 1;
}
#endif

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

#ifdef MUTEX_NONE
void CPLDestroyMutex( CPLMutex * /* hMutex */ ) {}
#else
void CPLDestroyMutex( CPLMutex *hMutex )
{
    unsigned char *pabyMutex = reinterpret_cast<unsigned char *>(hMutex);

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e'
               && pabyMutex[3] == 'd' );

    free( pabyMutex );
}
#endif

/************************************************************************/
/*                            CPLCreateCond()                           */
/************************************************************************/

CPLCond *CPLCreateCond()
{
    return NULL;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

void CPLCondWait( CPLCond * /* hCond */ , CPLMutex* /* hMutex */ ) {}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void CPLCondSignal( CPLCond * /* hCond */ ) {}

/************************************************************************/
/*                           CPLCondBroadcast()                         */
/************************************************************************/

void CPLCondBroadcast( CPLCond * /* hCond */ ) {}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void CPLDestroyCond( CPLCond * /* hCond */ ) {}

/************************************************************************/
/*                            CPLLockFile()                             */
/*                                                                      */
/*      Lock a file.  This implementation has a terrible race           */
/*      condition.  If we don't succeed in opening the lock file, we    */
/*      assume we can create one and own the target file, but other     */
/*      processes might easily try creating the target file at the      */
/*      same time, overlapping us.  Death!  Mayhem!  The traditional    */
/*      solution is to use open() with _O_CREAT|_O_EXCL but this        */
/*      function and these arguments aren't trivially portable.         */
/*      Also, this still leaves a race condition on NFS drivers         */
/*      (apparently).                                                   */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
/* -------------------------------------------------------------------- */
/*      We use a lock file with a name derived from the file we want    */
/*      to lock to represent the file being locked.  Note that for      */
/*      the stub implementation the target file does not even need      */
/*      to exist to be locked.                                          */
/* -------------------------------------------------------------------- */
    char *pszLockFilename = static_cast<char *>(
        CPLMalloc(strlen(pszPath) + 30));
    snprintf( pszLockFilename, strlen(pszPath) + 30, "%s.lock", pszPath );

    FILE *fpLock = fopen( pszLockFilename, "r" );
    while( fpLock != NULL && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( std::min(dfWaitInSeconds, 0.5) );
        dfWaitInSeconds -= 0.5;

        fpLock = fopen( pszLockFilename, "r" );
    }

    if( fpLock != NULL )
    {
        fclose( fpLock );
        CPLFree( pszLockFilename );
        return NULL;
    }

    fpLock = fopen( pszLockFilename, "w" );

    if( fpLock == NULL )
    {
        CPLFree( pszLockFilename );
        return NULL;
    }

    fwrite( "held\n", 1, 5, fpLock );
    fclose( fpLock );

    return pszLockFilename;
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
    char *pszLockFilename = static_cast<char *>(hLock);

    if( hLock == NULL )
        return;

    VSIUnlink( pszLockFilename );

    CPLFree( pszLockFilename );
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

GIntBig CPLGetPID()

{
    return 1;
}

/************************************************************************/
/*                          CPLCreateThread();                          */
/************************************************************************/

int CPLCreateThread( CPLThreadFunc /* pfnMain */, void * /* pArg */)
{
    CPLDebug( "CPLCreateThread", "Fails to dummy implementation" );

    return -1;
}

/************************************************************************/
/*                      CPLCreateJoinableThread()                       */
/************************************************************************/

CPLJoinableThread* CPLCreateJoinableThread( CPLThreadFunc /* pfnMain */,
                                            void * /* pThreadArg */ )
{
    CPLDebug( "CPLCreateJoinableThread", "Fails to dummy implementation" );

    return NULL;
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread( CPLJoinableThread* /* hJoinableThread */ ) {}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )
{
    time_t ltime;

    time( &ltime );
    const time_t ttime = ltime + static_cast<int>(dfWaitInSeconds + 0.5);

    for( ; ltime < ttime; time(&ltime) )
    {
        // Currently we just busy wait.  Perhaps we could at least block on io?
    }
}

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **papTLSList = NULL;

static void **CPLGetTLSList( int *pbMemoryErrorOccurred )

{
    if( pbMemoryErrorOccurred )
        *pbMemoryErrorOccurred = FALSE;
    if( papTLSList == NULL )
    {
        papTLSList =
            static_cast<void **>(VSICalloc(sizeof(void*), CTLS_MAX * 2));
        if( papTLSList == NULL )
        {
            if( pbMemoryErrorOccurred )
            {
                *pbMemoryErrorOccurred = TRUE;
                fprintf(stderr,
                        "CPLGetTLSList() failed to allocate TLS list!\n");
                return NULL;
            }
            CPLEmergencyError("CPLGetTLSList() failed to allocate TLS list!");
        }
    }

    return papTLSList;
}

/************************************************************************/
/*                             CPLFinalizeTLS()                         */
/************************************************************************/

void CPLFinalizeTLS()
{
    CPLCleanupTLS();
}

/************************************************************************/
/*                           CPLCleanupTLS()                            */
/************************************************************************/

void CPLCleanupTLS()

{
    CPLCleanupTLSList( papTLSList );
    papTLSList = NULL;
}

// endif CPL_MULTIPROC_STUB

#elif defined(CPL_MULTIPROC_WIN32)

  /************************************************************************/
  /* ==================================================================== */
  /*                        CPL_MULTIPROC_WIN32                           */
  /*                                                                      */
  /*    WIN32 Implementation of multiprocessing functions.                */
  /* ==================================================================== */
  /************************************************************************/

/* InitializeCriticalSectionAndSpinCount requires _WIN32_WINNT >= 0x403 */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500

#include <windows.h>

/************************************************************************/
/*                             CPLGetNumCPUs()                          */
/************************************************************************/

int CPLGetNumCPUs()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const DWORD dwNum = info.dwNumberOfProcessors;
    if( dwNum < 1 )
        return 1;
    return static_cast<int>(dwNum);
}

/************************************************************************/
/*                        CPLGetThreadingModel()                        */
/************************************************************************/

const char *CPLGetThreadingModel()

{
    return "win32";
}

/************************************************************************/
/*                           CPLCreateMutex()                           */
/************************************************************************/

CPLMutex *CPLCreateMutex()

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex = CreateMutex( NULL, TRUE, NULL );

    return (CPLMutex *) hMutex;
#else

    // Do not use CPLMalloc() since its debugging infrastructure
    // can call the CPL*Mutex functions.
    CRITICAL_SECTION *pcs =
        static_cast<CRITICAL_SECTION *>(malloc(sizeof(*pcs)));
    if( pcs )
    {
      InitializeCriticalSectionAndSpinCount(pcs, 4000);
      EnterCriticalSection(pcs);
    }

    return (CPLMutex *) pcs;
#endif
}

CPLMutex *CPLCreateMutexEx( int /* nOptions */ )

{
    return CPLCreateMutex();
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( CPLMutex *hMutexIn, double dfWaitInSeconds )

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex = (HANDLE) hMutexIn;
    const DWORD hr =
        WaitForSingleObject(hMutex, static_cast<int>(dfWaitInSeconds * 1000));

    return hr != WAIT_TIMEOUT;
#else
    CRITICAL_SECTION *pcs = (CRITICAL_SECTION *)hMutexIn;
    BOOL ret;

    if( dfWaitInSeconds >= 1000.0 )
    {
        // We assume this is the synonymous for infinite, so it is more
        // efficient to use EnterCriticalSection() directly
        EnterCriticalSection(pcs);
        ret = TRUE;
    }
    else
    {
        while( (ret = TryEnterCriticalSection(pcs)) == 0 &&
               dfWaitInSeconds > 0.0 )
        {
            CPLSleep( std::min(dfWaitInSeconds, 0.01) );
            dfWaitInSeconds -= 0.01;
        }
    }

    return ret;
#endif
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( CPLMutex *hMutexIn )

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex = (HANDLE) hMutexIn;

    ReleaseMutex( hMutex );
#else
    CRITICAL_SECTION *pcs = (CRITICAL_SECTION *)hMutexIn;

    LeaveCriticalSection(pcs);
#endif
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( CPLMutex *hMutexIn )

{
#ifdef USE_WIN32_MUTEX
    HANDLE hMutex = (HANDLE) hMutexIn;

    CloseHandle( hMutex );
#else
    CRITICAL_SECTION *pcs = (CRITICAL_SECTION *)hMutexIn;

    DeleteCriticalSection( pcs );
    free( pcs );
#endif
}

/************************************************************************/
/*                            CPLCreateCond()                           */
/************************************************************************/

struct _WaiterItem
{
    HANDLE hEvent;
    struct _WaiterItem* psNext;
};
typedef struct _WaiterItem WaiterItem;

typedef struct
{
    CPLMutex    *hInternalMutex;
    WaiterItem  *psWaiterList;
} Win32Cond;

CPLCond *CPLCreateCond()
{
    Win32Cond* psCond = static_cast<Win32Cond *>(malloc(sizeof(Win32Cond)));
    if( psCond == NULL )
        return NULL;
    psCond->hInternalMutex = CPLCreateMutex();
    if( psCond->hInternalMutex == NULL )
    {
        free(psCond);
        return NULL;
    }
    CPLReleaseMutex(psCond->hInternalMutex);
    psCond->psWaiterList = NULL;
    return (CPLCond*) psCond;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

static void CPLTLSFreeEvent( void* pData )
{
    CloseHandle((HANDLE)pData);
}

void CPLCondWait( CPLCond *hCond, CPLMutex* hClientMutex )
{
    Win32Cond* psCond = (Win32Cond*) hCond;

    HANDLE hEvent = (HANDLE) CPLGetTLS(CTLS_WIN32_COND);
    if( hEvent == NULL )
    {
        hEvent = CreateEvent(NULL, /* security attributes */
                             0,    /* manual reset = no */
                             0,    /* initial state = unsignaled */
                             NULL  /* no name */);
        CPLAssert(hEvent != NULL);

        CPLSetTLSWithFreeFunc(CTLS_WIN32_COND, hEvent, CPLTLSFreeEvent);
    }

    /* Insert the waiter into the waiter list of the condition */
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psItem = static_cast<WaiterItem *>(malloc(sizeof(WaiterItem)));
    CPLAssert(psItem != NULL);

    psItem->hEvent = hEvent;
    psItem->psNext = psCond->psWaiterList;

    psCond->psWaiterList = psItem;

    CPLReleaseMutex(psCond->hInternalMutex);

    // Release the client mutex before waiting for the event being signaled.
    CPLReleaseMutex(hClientMutex);

    // Ideally we would check that we do not get WAIT_FAILED but it is hard
    // to report a failure.
    WaitForSingleObject(hEvent, INFINITE);

    // Reacquire the client mutex.
    CPLAcquireMutex(hClientMutex, 1000.0);
}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void CPLCondSignal( CPLCond *hCond )
{
    Win32Cond* psCond = (Win32Cond*) hCond;

    // Signal the first registered event, and remove it from the list.
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psIter = psCond->psWaiterList;
    if( psIter != NULL )
    {
        SetEvent(psIter->hEvent);
        psCond->psWaiterList = psIter->psNext;
        free(psIter);
    }

    CPLReleaseMutex(psCond->hInternalMutex);
}

/************************************************************************/
/*                           CPLCondBroadcast()                         */
/************************************************************************/

void CPLCondBroadcast( CPLCond *hCond )
{
    Win32Cond* psCond = (Win32Cond*) hCond;

    // Signal all the registered events, and remove them from the list.
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psIter = psCond->psWaiterList;
    while( psIter != NULL )
    {
        WaiterItem* psNext = psIter->psNext;
        SetEvent(psIter->hEvent);
        free(psIter);
        psIter = psNext;
    }
    psCond->psWaiterList = NULL;

    CPLReleaseMutex(psCond->hInternalMutex);
}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void CPLDestroyCond( CPLCond *hCond )
{
    Win32Cond* psCond = (Win32Cond*) hCond;
    CPLDestroyMutex(psCond->hInternalMutex);
    psCond->hInternalMutex = NULL;
    CPLAssert(psCond->psWaiterList == NULL);
    free(psCond);
}

/************************************************************************/
/*                            CPLLockFile()                             */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
    char *pszLockFilename =
        static_cast<char *>(CPLMalloc(strlen(pszPath) + 30));
    snprintf( pszLockFilename, strlen(pszPath) + 30, "%s.lock", pszPath );

    HANDLE hLockFile =
        CreateFile(pszLockFilename, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, NULL);

    while( GetLastError() == ERROR_ALREADY_EXISTS
           && dfWaitInSeconds > 0.0 )
    {
        CloseHandle( hLockFile );
        CPLSleep( std::min(dfWaitInSeconds, 0.125) );
        dfWaitInSeconds -= 0.125;

        hLockFile =
            CreateFile( pszLockFilename, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE,
                        NULL );
    }

    CPLFree( pszLockFilename );

    if( hLockFile == INVALID_HANDLE_VALUE )
        return NULL;

    if( GetLastError() == ERROR_ALREADY_EXISTS )
    {
        CloseHandle( hLockFile );
        return NULL;
    }

    return (void *) hLockFile;
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
    HANDLE    hLockFile = (HANDLE) hLock;

    CloseHandle( hLockFile );
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

GIntBig CPLGetPID()

{
    return static_cast<GIntBig>(GetCurrentThreadId());
}

/************************************************************************/
/*                       CPLStdCallThreadJacket()                       */
/************************************************************************/

typedef struct {
    void *pAppData;
    CPLThreadFunc pfnMain;
    HANDLE hThread;
} CPLStdCallThreadInfo;

static DWORD WINAPI CPLStdCallThreadJacket( void *pData )

{
    CPLStdCallThreadInfo *psInfo = static_cast<CPLStdCallThreadInfo *>(pData);

    psInfo->pfnMain( psInfo->pAppData );

    if( psInfo->hThread == NULL )
        CPLFree( psInfo );  // Only for detached threads.

    CPLCleanupTLS();

    return 0;
}

/************************************************************************/
/*                          CPLCreateThread()                           */
/*                                                                      */
/*      The WIN32 CreateThread() call requires an entry point that      */
/*      has __stdcall conventions, so we provide a jacket function      */
/*      to supply that.                                                 */
/************************************************************************/

int CPLCreateThread( CPLThreadFunc pfnMain, void *pThreadArg )

{
    CPLStdCallThreadInfo *psInfo = static_cast<CPLStdCallThreadInfo *>(
        CPLCalloc(sizeof(CPLStdCallThreadInfo), 1));
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->hThread = NULL;

    DWORD nThreadId = 0;
    HANDLE hThread = CreateThread( NULL, 0, CPLStdCallThreadJacket, psInfo,
                                   0, &nThreadId );

    if( hThread == NULL )
        return -1;

    CloseHandle( hThread );

    return nThreadId;
}

/************************************************************************/
/*                      CPLCreateJoinableThread()                       */
/************************************************************************/

CPLJoinableThread* CPLCreateJoinableThread( CPLThreadFunc pfnMain,
                                            void *pThreadArg )

{
    CPLStdCallThreadInfo *psInfo = static_cast<CPLStdCallThreadInfo *>(
        CPLCalloc(sizeof(CPLStdCallThreadInfo), 1));
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;

    DWORD nThreadId = 0;
    HANDLE hThread = CreateThread( NULL, 0, CPLStdCallThreadJacket, psInfo,
                                   0, &nThreadId );

    if( hThread == NULL )
        return NULL;

    psInfo->hThread = hThread;
    return (CPLJoinableThread*) psInfo;
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread( CPLJoinableThread* hJoinableThread )
{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo *) hJoinableThread;

    WaitForSingleObject(psInfo->hThread, INFINITE);
    CloseHandle( psInfo->hThread );
    CPLFree( psInfo );
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    Sleep( (DWORD) (dfWaitInSeconds * 1000.0) );
}

static bool bTLSKeySetup = false;
static DWORD nTLSKey = 0;

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **CPLGetTLSList( int *pbMemoryErrorOccurred )

{
    void **papTLSList = NULL;

    if( pbMemoryErrorOccurred )
        *pbMemoryErrorOccurred = FALSE;
    if( !bTLSKeySetup )
    {
        nTLSKey = TlsAlloc();
        if( nTLSKey == TLS_OUT_OF_INDEXES )
        {
            if( pbMemoryErrorOccurred )
            {
                *pbMemoryErrorOccurred = TRUE;
                fprintf(stderr, "CPLGetTLSList(): TlsAlloc() failed!\n" );
                return NULL;
            }
            CPLEmergencyError( "CPLGetTLSList(): TlsAlloc() failed!" );
        }
        bTLSKeySetup = true;
    }

    papTLSList = (void **) TlsGetValue( nTLSKey );
    if( papTLSList == NULL )
    {
        papTLSList =
            static_cast<void **>(VSICalloc(sizeof(void*), CTLS_MAX * 2));
        if( papTLSList == NULL )
        {
            if( pbMemoryErrorOccurred )
            {
                *pbMemoryErrorOccurred = TRUE;
                fprintf(stderr,
                        "CPLGetTLSList() failed to allocate TLS list!\n" );
                return NULL;
            }
            CPLEmergencyError("CPLGetTLSList() failed to allocate TLS list!");
        }
        if( TlsSetValue( nTLSKey, papTLSList ) == 0 )
        {
            if( pbMemoryErrorOccurred )
            {
                *pbMemoryErrorOccurred = TRUE;
                fprintf(stderr, "CPLGetTLSList(): TlsSetValue() failed!\n" );
                return NULL;
            }
            CPLEmergencyError( "CPLGetTLSList(): TlsSetValue() failed!" );
        }
    }

    return papTLSList;
}

/************************************************************************/
/*                             CPLFinalizeTLS()                         */
/************************************************************************/

void CPLFinalizeTLS()
{
    CPLCleanupTLS();
}

/************************************************************************/
/*                           CPLCleanupTLS()                            */
/************************************************************************/

void CPLCleanupTLS()

{
    if( !bTLSKeySetup )
        return;

    void **papTLSList = (void **) TlsGetValue( nTLSKey );
    if( papTLSList == NULL )
        return;

    TlsSetValue( nTLSKey, NULL );

    CPLCleanupTLSList( papTLSList );
}

// endif CPL_MULTIPROC_WIN32

#elif defined(CPL_MULTIPROC_PTHREAD)

#include <pthread.h>
#include <time.h>
#include <unistd.h>

  /************************************************************************/
  /* ==================================================================== */
  /*                        CPL_MULTIPROC_PTHREAD                         */
  /*                                                                      */
  /*    PTHREAD Implementation of multiprocessing functions.              */
  /* ==================================================================== */
  /************************************************************************/

/************************************************************************/
/*                             CPLGetNumCPUs()                          */
/************************************************************************/

int CPLGetNumCPUs()
{
#ifdef _SC_NPROCESSORS_ONLN
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return 1;
#endif
}

/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static CPLMutex *CPLCreateMutexInternal( bool bAlreadyInGlobalLock,
                                         int nOptions );

int CPLCreateOrAcquireMutexEx( CPLMutex **phMutex, double dfWaitInSeconds,
                               int nOptions )

{
    bool bSuccess = false;

    pthread_mutex_lock(&global_mutex);
    if( *phMutex == NULL )
    {
        *phMutex = CPLCreateMutexInternal(true, nOptions);
        bSuccess = *phMutex != NULL;
        pthread_mutex_unlock(&global_mutex);
    }
    else
    {
        pthread_mutex_unlock(&global_mutex);

        bSuccess = CPL_TO_BOOL(CPLAcquireMutex( *phMutex, dfWaitInSeconds ));
    }

    return bSuccess;
}

/************************************************************************/
/*                   CPLCreateOrAcquireMutexInternal()                  */
/************************************************************************/

static
int CPLCreateOrAcquireMutexInternal( CPLLock **phLock, double dfWaitInSeconds,
                                     CPLLockType eType )
{
    bool bSuccess = false;

    pthread_mutex_lock(&global_mutex);
    if( *phLock == NULL )
    {
        *phLock = static_cast<CPLLock *>(calloc(1, sizeof(CPLLock)));
        if( *phLock )
        {
            (*phLock)->eType = eType;
            (*phLock)->u.hMutex = CPLCreateMutexInternal(
                true,
                eType == LOCK_RECURSIVE_MUTEX
                ? CPL_MUTEX_RECURSIVE : CPL_MUTEX_ADAPTIVE );
            if( (*phLock)->u.hMutex == NULL )
            {
                free(*phLock);
                *phLock = NULL;
            }
        }
        bSuccess = *phLock != NULL;
        pthread_mutex_unlock(&global_mutex);
    }
    else
    {
        pthread_mutex_unlock(&global_mutex);

        bSuccess = CPL_TO_BOOL(
            CPLAcquireMutex( (*phLock)->u.hMutex, dfWaitInSeconds ));
    }

    return bSuccess;
}

/************************************************************************/
/*                        CPLGetThreadingModel()                        */
/************************************************************************/

const char *CPLGetThreadingModel()

{
    return "pthread";
}

/************************************************************************/
/*                           CPLCreateMutex()                           */
/************************************************************************/

typedef struct _MutexLinkedElt MutexLinkedElt;
struct _MutexLinkedElt
{
    pthread_mutex_t   sMutex;
    int               nOptions;
    _MutexLinkedElt  *psPrev;
    _MutexLinkedElt  *psNext;
};
static MutexLinkedElt* psMutexList = NULL;

static void CPLInitMutex( MutexLinkedElt* psItem )
{
    if( psItem->nOptions == CPL_MUTEX_REGULAR )
    {
        pthread_mutex_t tmp_mutex = PTHREAD_MUTEX_INITIALIZER;
        psItem->sMutex = tmp_mutex;
        return;
    }

    // When an adaptive mutex is required, we can safely fallback to regular
    // mutex if we don't have HAVE_PTHREAD_MUTEX_ADAPTIVE_NP.
    if( psItem->nOptions == CPL_MUTEX_ADAPTIVE )
    {
#if defined(HAVE_PTHREAD_MUTEX_ADAPTIVE_NP)
        pthread_mutexattr_t attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ADAPTIVE_NP );
        pthread_mutex_init( &(psItem->sMutex), &attr );
#else
        pthread_mutex_t tmp_mutex = PTHREAD_MUTEX_INITIALIZER;
        psItem->sMutex = tmp_mutex;
#endif
        return;
    }

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(HAVE_PTHREAD_MUTEX_RECURSIVE)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init( &(psItem->sMutex), &attr );
    }
// BSDs have PTHREAD_MUTEX_RECURSIVE as an enum, not a define.
// But they have #define MUTEX_TYPE_COUNTING_FAST PTHREAD_MUTEX_RECURSIVE
#elif defined(MUTEX_TYPE_COUNTING_FAST)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, MUTEX_TYPE_COUNTING_FAST );
        pthread_mutex_init( &(psItem->sMutex), &attr );
    }
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    pthread_mutex_t tmp_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    psItem->sMutex = tmp_mutex;
#else
#error "Recursive mutexes apparently unsupported, configure --without-threads"
#endif
}

static CPLMutex *CPLCreateMutexInternal( bool bAlreadyInGlobalLock,
                                         int nOptions )
{
    MutexLinkedElt* psItem = static_cast<MutexLinkedElt *>(
        malloc(sizeof(MutexLinkedElt)) );
    if( psItem == NULL )
    {
        fprintf(stderr, "CPLCreateMutexInternal() failed.\n");
        return NULL;
    }

    if( !bAlreadyInGlobalLock )
        pthread_mutex_lock(&global_mutex);
    psItem->psPrev = NULL;
    psItem->psNext = psMutexList;
    if( psMutexList )
        psMutexList->psPrev = psItem;
    psMutexList = psItem;
    if( !bAlreadyInGlobalLock )
        pthread_mutex_unlock(&global_mutex);

    psItem->nOptions = nOptions;
    CPLInitMutex(psItem);

    // Mutexes are implicitly acquired when created.
    CPLAcquireMutex( (CPLMutex*)psItem, 0.0 );

    return (CPLMutex*)psItem;
}

CPLMutex *CPLCreateMutex()
{
    return CPLCreateMutexInternal(false, CPL_MUTEX_RECURSIVE);
}

CPLMutex *CPLCreateMutexEx( int nOptions )
{
    return CPLCreateMutexInternal(false, nOptions);
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( CPLMutex *hMutexIn, double /* dfWaitInSeconds */ )
{
    // TODO: Need to add timeout support.
    MutexLinkedElt* psItem = (MutexLinkedElt *) hMutexIn;
    const int err = pthread_mutex_lock( &(psItem->sMutex) );

    if( err != 0 )
    {
        if( err == EDEADLK )
            fprintf(stderr, "CPLAcquireMutex: Error = %d/EDEADLK\n", err );
        else
            fprintf(stderr, "CPLAcquireMutex: Error = %d (%s)\n", err,
                    strerror(err));

        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( CPLMutex *hMutexIn )

{
    MutexLinkedElt* psItem = (MutexLinkedElt *) hMutexIn;
    const int err = pthread_mutex_unlock( &(psItem->sMutex) );
    if( err != 0 )
    {
        fprintf(stderr, "CPLReleaseMutex: Error = %d (%s)\n", err,
                strerror(err));
    }
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( CPLMutex *hMutexIn )

{
    MutexLinkedElt* psItem = (MutexLinkedElt *) hMutexIn;
    const int err = pthread_mutex_destroy( &(psItem->sMutex) );
        if( err != 0 )
    {
        fprintf(stderr, "CPLDestroyMutex: Error = %d (%s)\n", err,
                strerror(err));
    }
    pthread_mutex_lock(&global_mutex);
    if( psItem->psPrev )
        psItem->psPrev->psNext = psItem->psNext;
    if( psItem->psNext )
        psItem->psNext->psPrev = psItem->psPrev;
    if( psItem == psMutexList )
        psMutexList = psItem->psNext;
    pthread_mutex_unlock(&global_mutex);
    free( hMutexIn );
}

/************************************************************************/
/*                          CPLReinitAllMutex()                         */
/************************************************************************/

// Used by gdalclientserver.cpp just after forking, to avoid
// deadlocks while mixing threads with fork.
void CPLReinitAllMutex();  // TODO(schwehr): Put this in a header.
void CPLReinitAllMutex()
{
    MutexLinkedElt* psItem = psMutexList;
    while( psItem != NULL )
    {
        CPLInitMutex(psItem);
        psItem = psItem->psNext;
    }
    pthread_mutex_t tmp_global_mutex = PTHREAD_MUTEX_INITIALIZER;
    global_mutex = tmp_global_mutex;
}

/************************************************************************/
/*                            CPLCreateCond()                           */
/************************************************************************/

CPLCond *CPLCreateCond()
{
    pthread_cond_t* pCond =
      static_cast<pthread_cond_t *>(malloc(sizeof(pthread_cond_t)));
    if( pCond && pthread_cond_init(pCond, NULL) == 0 )
        return (CPLCond*) pCond;
    fprintf(stderr, "CPLCreateCond() failed.\n");
    free(pCond);
    return NULL;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

void CPLCondWait( CPLCond *hCond, CPLMutex* hMutex )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    MutexLinkedElt* psItem = (MutexLinkedElt *) hMutex;
    pthread_mutex_t * pMutex = &(psItem->sMutex);
    pthread_cond_wait(pCond, pMutex);
}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void CPLCondSignal( CPLCond *hCond )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    pthread_cond_signal(pCond);
}

/************************************************************************/
/*                           CPLCondBroadcast()                         */
/************************************************************************/

void CPLCondBroadcast( CPLCond *hCond )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    pthread_cond_broadcast(pCond);
}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void CPLDestroyCond( CPLCond *hCond )
{
    pthread_cond_t* pCond = (pthread_cond_t* )hCond;
    pthread_cond_destroy(pCond);
    free(hCond);
}

/************************************************************************/
/*                            CPLLockFile()                             */
/*                                                                      */
/*      This is really a stub implementation, see first                 */
/*      CPLLockFile() for caveats.                                      */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
/* -------------------------------------------------------------------- */
/*      We use a lock file with a name derived from the file we want    */
/*      to lock to represent the file being locked.  Note that for      */
/*      the stub implementation the target file does not even need      */
/*      to exist to be locked.                                          */
/* -------------------------------------------------------------------- */
    const size_t nLen = strlen(pszPath) + 30;
    char *pszLockFilename = static_cast<char *>(CPLMalloc(nLen));
    snprintf( pszLockFilename, nLen, "%s.lock", pszPath );

    FILE *fpLock = fopen( pszLockFilename, "r" );
    while( fpLock != NULL && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( std::min(dfWaitInSeconds, 0.5) );
        dfWaitInSeconds -= 0.5;

        fpLock = fopen( pszLockFilename, "r" );
    }

    if( fpLock != NULL )
    {
        fclose( fpLock );
        CPLFree( pszLockFilename );
        return NULL;
    }

    fpLock = fopen( pszLockFilename, "w" );

    if( fpLock == NULL )
    {
        CPLFree( pszLockFilename );
        return NULL;
    }

    fwrite( "held\n", 1, 5, fpLock );
    fclose( fpLock );

    return pszLockFilename;
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
    char *pszLockFilename = static_cast<char *>(hLock);

    if( hLock == NULL )
        return;

    VSIUnlink( pszLockFilename );

    CPLFree( pszLockFilename );
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

GIntBig CPLGetPID()

{
    // TODO(schwehr): What is the correct C++ way to do this cast?
    return (GIntBig)pthread_self();
}

static pthread_key_t oTLSKey;
static pthread_once_t oTLSKeySetup = PTHREAD_ONCE_INIT;

/************************************************************************/
/*                             CPLMake_key()                            */
/************************************************************************/

static void CPLMake_key()

{
    if( pthread_key_create( &oTLSKey,
                            (void (*)(void*)) CPLCleanupTLSList ) != 0 )
    {
        CPLError( CE_Fatal, CPLE_AppDefined, "pthread_key_create() failed!" );
    }
}

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **CPLGetTLSList( int* pbMemoryErrorOccurred )

{
    if( pbMemoryErrorOccurred )
        *pbMemoryErrorOccurred = FALSE;

    if( pthread_once(&oTLSKeySetup, CPLMake_key) != 0 )
    {
        if( pbMemoryErrorOccurred )
        {
            fprintf(stderr, "CPLGetTLSList(): pthread_once() failed!\n" );
            *pbMemoryErrorOccurred = TRUE;
            return NULL;
        }
        CPLEmergencyError( "CPLGetTLSList(): pthread_once() failed!" );
    }

    void **papTLSList = (void **) pthread_getspecific( oTLSKey );
    if( papTLSList == NULL )
    {
        papTLSList =
            static_cast<void **>(VSICalloc(sizeof(void*), CTLS_MAX * 2));
        if( papTLSList == NULL )
        {
            if( pbMemoryErrorOccurred )
            {
                fprintf(stderr,
                        "CPLGetTLSList() failed to allocate TLS list!\n" );
                *pbMemoryErrorOccurred = TRUE;
                return NULL;
            }
            CPLEmergencyError("CPLGetTLSList() failed to allocate TLS list!");
        }
        if( pthread_setspecific( oTLSKey, papTLSList ) != 0 )
        {
            if( pbMemoryErrorOccurred )
            {
                fprintf(stderr,
                        "CPLGetTLSList(): pthread_setspecific() failed!\n" );
                *pbMemoryErrorOccurred = TRUE;
                return NULL;
            }
            CPLEmergencyError(
                "CPLGetTLSList(): pthread_setspecific() failed!" );
        }
    }

    return papTLSList;
}

/************************************************************************/
/*                       CPLStdCallThreadJacket()                       */
/************************************************************************/

typedef struct {
    void *pAppData;
    CPLThreadFunc pfnMain;
    pthread_t hThread;
    bool bJoinable;
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    bool bInitSucceeded;
    bool bInitDone;
    pthread_mutex_t sMutex;
    pthread_cond_t sCond;
#endif
} CPLStdCallThreadInfo;

static void *CPLStdCallThreadJacket( void *pData )

{
    CPLStdCallThreadInfo *psInfo = static_cast<CPLStdCallThreadInfo *>(pData);

#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    int bMemoryError = FALSE;
    CPLGetTLSList(&bMemoryError);
    if( bMemoryError )
        goto error;

    assert( pthread_mutex_lock( &(psInfo->sMutex) ) == 0);
    psInfo->bInitDone = true;
    assert( pthread_cond_signal( &(psInfo->sCond) ) == 0);
    assert( pthread_mutex_unlock( &(psInfo->sMutex) ) == 0);
#endif

    psInfo->pfnMain( psInfo->pAppData );

    if( !psInfo->bJoinable )
        CPLFree( psInfo );

    return NULL;

#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
error:
    assert( pthread_mutex_lock( &(psInfo->sMutex) ) == 0);
    psInfo->bInitSucceeded = false;
    psInfo->bInitDone = true;
    assert( pthread_cond_signal( &(psInfo->sCond) ) == 0);
    assert( pthread_mutex_unlock( &(psInfo->sMutex) ) == 0);
    return NULL;
#endif
}

/************************************************************************/
/*                          CPLCreateThread()                           */
/*                                                                      */
/*      The WIN32 CreateThread() call requires an entry point that      */
/*      has __stdcall conventions, so we provide a jacket function      */
/*      to supply that.                                                 */
/************************************************************************/

int CPLCreateThread( CPLThreadFunc pfnMain, void *pThreadArg )

{
    CPLStdCallThreadInfo *psInfo = static_cast<CPLStdCallThreadInfo *>(
        VSI_CALLOC_VERBOSE(sizeof(CPLStdCallThreadInfo), 1));
    if( psInfo == NULL )
        return -1;
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->bJoinable = false;
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    psInfo->bInitSucceeded = true;
    psInfo->bInitDone = false;
    pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
    psInfo->sMutex = sMutex;
    if( pthread_cond_init(&(psInfo->sCond), NULL) != 0 )
    {
        CPLFree( psInfo );
        fprintf(stderr, "CPLCreateThread() failed.\n");
        return -1;
    }
#endif

    pthread_attr_t hThreadAttr;
    pthread_attr_init( &hThreadAttr );
    pthread_attr_setdetachstate( &hThreadAttr, PTHREAD_CREATE_DETACHED );
    if( pthread_create( &(psInfo->hThread), &hThreadAttr,
                        CPLStdCallThreadJacket, (void *) psInfo ) != 0 )
    {
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
        pthread_cond_destroy(&(psInfo->sCond));
#endif
        CPLFree( psInfo );
        fprintf(stderr, "CPLCreateThread() failed.\n");
        return -1;
    }

#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    bool bInitSucceeded;
    while( true )
    {
        assert(pthread_mutex_lock( &(psInfo->sMutex) ) == 0);
        bool bInitDone = psInfo->bInitDone;
        if( !bInitDone )
            assert(
                pthread_cond_wait( &(psInfo->sCond), &(psInfo->sMutex)) == 0);
        bInitSucceeded = psInfo->bInitSucceeded;
        assert(pthread_mutex_unlock( &(psInfo->sMutex) ) == 0);
        if( bInitDone )
            break;
    }

    pthread_cond_destroy(&(psInfo->sCond));

    if( !bInitSucceeded )
    {
        CPLFree( psInfo );
        fprintf(stderr, "CPLCreateThread() failed.\n");
        return -1;
    }
#endif

    return 1;  // Can we return the actual thread pid?
}

/************************************************************************/
/*                      CPLCreateJoinableThread()                       */
/************************************************************************/

CPLJoinableThread* CPLCreateJoinableThread( CPLThreadFunc pfnMain,
                                            void *pThreadArg )

{
    CPLStdCallThreadInfo *psInfo = static_cast<CPLStdCallThreadInfo *>(
        VSI_CALLOC_VERBOSE(sizeof(CPLStdCallThreadInfo), 1));
    if( psInfo == NULL )
        return NULL;
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->bJoinable = true;
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    psInfo->bInitSucceeded = true;
    psInfo->bInitDone = false;
    pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
    psInfo->sMutex = sMutex;
    if( pthread_cond_init(&(psInfo->sCond), NULL) != 0 )
    {
        CPLFree( psInfo );
        fprintf(stderr, "CPLCreateJoinableThread() failed.\n");
        return NULL;
    }
#endif

    pthread_attr_t hThreadAttr;
    pthread_attr_init( &hThreadAttr );
    pthread_attr_setdetachstate( &hThreadAttr, PTHREAD_CREATE_JOINABLE );
    if( pthread_create( &(psInfo->hThread), &hThreadAttr,
                        CPLStdCallThreadJacket, (void *) psInfo ) != 0 )
    {
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
        pthread_cond_destroy(&(psInfo->sCond));
#endif
        CPLFree( psInfo );
        fprintf(stderr, "CPLCreateJoinableThread() failed.\n");
        return NULL;
    }

#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    bool bInitSucceeded;
    while( true )
    {
        assert(pthread_mutex_lock( &(psInfo->sMutex) ) == 0);
        bool bInitDone = psInfo->bInitDone;
        if( !bInitDone )
            assert(
                pthread_cond_wait( &(psInfo->sCond), &(psInfo->sMutex)) == 0);
        bInitSucceeded = psInfo->bInitSucceeded;
        assert(pthread_mutex_unlock( &(psInfo->sMutex) ) == 0);
        if( bInitDone )
            break;
    }

    pthread_cond_destroy(&(psInfo->sCond));

    if( !bInitSucceeded )
    {
        void* status;
        pthread_join( psInfo->hThread, &status);
        CPLFree( psInfo );
        fprintf(stderr, "CPLCreateJoinableThread() failed.\n");
        return NULL;
    }
#endif

    return (CPLJoinableThread*) psInfo;
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread(CPLJoinableThread* hJoinableThread)
{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo*) hJoinableThread;
    if( psInfo == NULL )
        return;

    void* status;
    pthread_join( psInfo->hThread, &status);

    CPLFree(psInfo);
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    struct timespec sRequest;
    struct timespec sRemain;

    sRequest.tv_sec = static_cast<int>(floor(dfWaitInSeconds));
    sRequest.tv_nsec =
        static_cast<int>((dfWaitInSeconds - sRequest.tv_sec) * 1000000000);
    nanosleep( &sRequest, &sRemain );
}

/************************************************************************/
/*                             CPLFinalizeTLS()                         */
/************************************************************************/

void CPLFinalizeTLS()
{
    CPLCleanupTLS();
    // See #5509 for the explanation why this may be needed.
    pthread_key_delete(oTLSKey);
}

/************************************************************************/
/*                             CPLCleanupTLS()                          */
/************************************************************************/

void CPLCleanupTLS()

{
    void **papTLSList = (void **) pthread_getspecific( oTLSKey );
    if( papTLSList == NULL )
        return;

    pthread_setspecific( oTLSKey, NULL );

    CPLCleanupTLSList( papTLSList );
}

/************************************************************************/
/*                          CPLCreateSpinLock()                         */
/************************************************************************/

#if defined(HAVE_PTHREAD_SPINLOCK)
#define HAVE_SPINLOCK_IMPL

struct _CPLSpinLock
{
    pthread_spinlock_t spin;
};

CPLSpinLock *CPLCreateSpinLock()
{
    CPLSpinLock* psSpin =
        static_cast<CPLSpinLock *>(malloc(sizeof(CPLSpinLock)));
    if( psSpin != NULL &&
        pthread_spin_init(&(psSpin->spin), PTHREAD_PROCESS_PRIVATE) == 0 )
    {
        return psSpin;
    }
    else
    {
        fprintf(stderr, "CPLCreateSpinLock() failed.\n");
        free(psSpin);
        return NULL;
    }
}

/************************************************************************/
/*                        CPLAcquireSpinLock()                          */
/************************************************************************/

int   CPLAcquireSpinLock( CPLSpinLock* psSpin )
{
    return pthread_spin_lock( &(psSpin->spin) ) == 0;
}

/************************************************************************/
/*                   CPLCreateOrAcquireSpinLockInternal()               */
/************************************************************************/

int CPLCreateOrAcquireSpinLockInternal( CPLLock** ppsLock )
{
    pthread_mutex_lock(&global_mutex);
    if( *ppsLock == NULL )
    {
        *ppsLock = static_cast<CPLLock *>(calloc(1, sizeof(CPLLock)));
        if( *ppsLock != NULL )
        {
            (*ppsLock)->eType = LOCK_SPIN;
            (*ppsLock)->u.hSpinLock = CPLCreateSpinLock();
            if( (*ppsLock)->u.hSpinLock == NULL )
            {
                free(*ppsLock);
                *ppsLock = NULL;
            }
        }
    }
    pthread_mutex_unlock(&global_mutex);
    // coverity[missing_unlock]
    return( *ppsLock != NULL && CPLAcquireSpinLock( (*ppsLock)->u.hSpinLock ) );
}

/************************************************************************/
/*                       CPLReleaseSpinLock()                           */
/************************************************************************/

void CPLReleaseSpinLock( CPLSpinLock* psSpin )
{
    pthread_spin_unlock( &(psSpin->spin) );
}

/************************************************************************/
/*                        CPLDestroySpinLock()                          */
/************************************************************************/

void CPLDestroySpinLock( CPLSpinLock* psSpin )
{
    pthread_spin_destroy( &(psSpin->spin) );
    free( psSpin );
}
#endif  // HAVE_PTHREAD_SPINLOCK

#endif  // def CPL_MULTIPROC_PTHREAD

/************************************************************************/
/*                             CPLGetTLS()                              */
/************************************************************************/

void *CPLGetTLS( int nIndex )

{
    void** l_papTLSList = CPLGetTLSList(NULL);

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    return l_papTLSList[nIndex];
}

/************************************************************************/
/*                            CPLGetTLSEx()                             */
/************************************************************************/

void *CPLGetTLSEx( int nIndex, int* pbMemoryErrorOccurred )

{
    void** l_papTLSList = CPLGetTLSList(pbMemoryErrorOccurred);
    if( l_papTLSList == NULL )
        return NULL;

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    return l_papTLSList[nIndex];
}

/************************************************************************/
/*                             CPLSetTLS()                              */
/************************************************************************/

void CPLSetTLS( int nIndex, void *pData, int bFreeOnExit )

{
    CPLSetTLSWithFreeFunc(nIndex, pData, (bFreeOnExit) ? CPLFree : NULL);
}

/************************************************************************/
/*                      CPLSetTLSWithFreeFunc()                         */
/************************************************************************/

// Warning: The CPLTLSFreeFunc must not in any case directly or indirectly
// use or fetch any TLS data, or a terminating thread will hang!
void CPLSetTLSWithFreeFunc( int nIndex, void *pData, CPLTLSFreeFunc pfnFree )

{
    void **l_papTLSList = CPLGetTLSList(NULL);

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    l_papTLSList[nIndex] = pData;
    l_papTLSList[CTLS_MAX + nIndex] = (void*) pfnFree;
}

/************************************************************************/
/*                      CPLSetTLSWithFreeFuncEx()                       */
/************************************************************************/

// Warning: the CPLTLSFreeFunc must not in any case directly or indirectly
// use or fetch any TLS data, or a terminating thread will hang!
void CPLSetTLSWithFreeFuncEx( int nIndex, void *pData,
                              CPLTLSFreeFunc pfnFree,
                              int* pbMemoryErrorOccurred )

{
    void **l_papTLSList = CPLGetTLSList(pbMemoryErrorOccurred);

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    l_papTLSList[nIndex] = pData;
    l_papTLSList[CTLS_MAX + nIndex] = (void*) pfnFree;
}
#ifndef HAVE_SPINLOCK_IMPL

// No spinlock specific API? Fallback to mutex.

/************************************************************************/
/*                          CPLCreateSpinLock()                         */
/************************************************************************/

CPLSpinLock *CPLCreateSpinLock( void )
{
    CPLSpinLock* psSpin = (CPLSpinLock *)CPLCreateMutex();
    if( psSpin )
        CPLReleaseSpinLock(psSpin);
    return psSpin;
}

/************************************************************************/
/*                     CPLCreateOrAcquireSpinLock()                     */
/************************************************************************/

int   CPLCreateOrAcquireSpinLockInternal( CPLLock** ppsLock )
{
    return CPLCreateOrAcquireMutexInternal( ppsLock, 1000, LOCK_ADAPTIVE_MUTEX );
}

/************************************************************************/
/*                        CPLAcquireSpinLock()                          */
/************************************************************************/

int CPLAcquireSpinLock( CPLSpinLock* psSpin )
{
    return CPLAcquireMutex( (CPLMutex*)psSpin, 1000 );
}

/************************************************************************/
/*                       CPLReleaseSpinLock()                           */
/************************************************************************/

void CPLReleaseSpinLock( CPLSpinLock* psSpin )
{
    CPLReleaseMutex( (CPLMutex*)psSpin );
}

/************************************************************************/
/*                        CPLDestroySpinLock()                          */
/************************************************************************/

void CPLDestroySpinLock( CPLSpinLock* psSpin )
{
    CPLDestroyMutex( (CPLMutex*)psSpin );
}

#endif  // HAVE_SPINLOCK_IMPL

/************************************************************************/
/*                            CPLCreateLock()                           */
/************************************************************************/

CPLLock *CPLCreateLock( CPLLockType eType )
{
    switch( eType )
    {
        case LOCK_RECURSIVE_MUTEX:
        case LOCK_ADAPTIVE_MUTEX:
        {
            CPLMutex* hMutex = CPLCreateMutexEx(
                eType == LOCK_RECURSIVE_MUTEX
                ? CPL_MUTEX_RECURSIVE : CPL_MUTEX_ADAPTIVE);
            if( !hMutex )
                return NULL;
            CPLReleaseMutex(hMutex);
            CPLLock* psLock = static_cast<CPLLock *>(malloc(sizeof(CPLLock)));
            if( psLock == NULL )
            {
                fprintf(stderr, "CPLCreateLock() failed.\n");
                CPLDestroyMutex(hMutex);
                return NULL;
            }
            psLock->eType = eType;
            psLock->u.hMutex = hMutex;
#ifdef DEBUG_CONTENTION
            psLock->bDebugPerf = false;
#endif
            return psLock;
        }
        case LOCK_SPIN:
        {
            CPLSpinLock* hSpinLock = CPLCreateSpinLock();
            if( !hSpinLock )
                return NULL;
            CPLLock* psLock = static_cast<CPLLock *>(malloc(sizeof(CPLLock)));
            if( psLock == NULL )
            {
                fprintf(stderr, "CPLCreateLock() failed.\n");
                CPLDestroySpinLock(hSpinLock);
                return NULL;
            }
            psLock->eType = eType;
            psLock->u.hSpinLock = hSpinLock;
#ifdef DEBUG_CONTENTION
            psLock->bDebugPerf = false;
#endif
            return psLock;
        }
        default:
            CPLAssert(false);
            return NULL;
    }
}

/************************************************************************/
/*                       CPLCreateOrAcquireLock()                       */
/************************************************************************/

int   CPLCreateOrAcquireLock( CPLLock** ppsLock, CPLLockType eType )
{
#ifdef DEBUG_CONTENTION
    GUIntBig nStartTime = 0;
    if( (*ppsLock) && (*ppsLock)->bDebugPerf )
        nStartTime = CPLrdtsc();
#endif
    int ret = 0;

    switch( eType )
    {
        case LOCK_RECURSIVE_MUTEX:
        case LOCK_ADAPTIVE_MUTEX:
        {
            ret = CPLCreateOrAcquireMutexInternal( ppsLock, 1000, eType);
            break;
        }
        case LOCK_SPIN:
        {
            ret = CPLCreateOrAcquireSpinLockInternal( ppsLock );
            break;
        }
        default:
            CPLAssert(false);
            return FALSE;
    }
#ifdef DEBUG_CONTENTION
    if( ret && (*ppsLock)->bDebugPerf )
    {
        (*ppsLock)->nStartTime = nStartTime;
    }
#endif
    return ret;
}

/************************************************************************/
/*                          CPLAcquireLock()                            */
/************************************************************************/

int CPLAcquireLock( CPLLock* psLock )
{
#ifdef DEBUG_CONTENTION
    if( psLock->bDebugPerf )
        psLock->nStartTime = CPLrdtsc();
#endif
    if( psLock->eType == LOCK_SPIN )
        return CPLAcquireSpinLock( psLock->u.hSpinLock );
    else
        return CPLAcquireMutex( psLock->u.hMutex, 1000 );
}

/************************************************************************/
/*                         CPLReleaseLock()                             */
/************************************************************************/

void CPLReleaseLock( CPLLock* psLock )
{
#ifdef DEBUG_CONTENTION
    bool bHitMaxDiff = false;
    GIntBig nMaxDiff = 0;
    double dfAvgDiff = 0;
    GUIntBig nIters = 0;
    if( psLock->bDebugPerf && psLock->nStartTime )
    {
        const GUIntBig nStopTime = CPLrdtscp();
        const GIntBig nDiffTime =
            static_cast<GIntBig>(nStopTime - psLock->nStartTime);
        if( nDiffTime > psLock->nMaxDiff )
        {
            bHitMaxDiff = true;
            psLock->nMaxDiff = nDiffTime;
        }
        nMaxDiff = psLock->nMaxDiff;
        psLock->nIters++;
        nIters = psLock->nIters;
        psLock->dfAvgDiff += (nDiffTime - psLock->dfAvgDiff) / nIters;
        dfAvgDiff = psLock->dfAvgDiff;
    }
#endif
    if( psLock->eType == LOCK_SPIN )
        CPLReleaseSpinLock( psLock->u.hSpinLock );
    else
        CPLReleaseMutex( psLock->u.hMutex );
#ifdef DEBUG_CONTENTION
    if( psLock->bDebugPerf &&
        (bHitMaxDiff || (psLock->nIters % 1000000) == (1000000-1) ))
    {
        CPLDebug("LOCK", "Lock contention : max = " CPL_FRMT_GIB ", avg = %.0f",
                 nMaxDiff, dfAvgDiff);
    }
#endif
}

/************************************************************************/
/*                          CPLDestroyLock()                            */
/************************************************************************/

void CPLDestroyLock( CPLLock* psLock )
{
    if( psLock->eType == LOCK_SPIN )
        CPLDestroySpinLock( psLock->u.hSpinLock );
    else
        CPLDestroyMutex( psLock->u.hMutex );
    free( psLock );
}

/************************************************************************/
/*                       CPLLockSetDebugPerf()                          */
/************************************************************************/

#ifdef DEBUG_CONTENTION
void CPLLockSetDebugPerf(CPLLock* psLock, int bEnableIn)
{
    psLock->bDebugPerf = CPL_TO_BOOL(bEnableIn);
}
#else
void CPLLockSetDebugPerf(CPLLock* /* psLock */, int bEnableIn)
{
    if( !bEnableIn )
        return;

    static bool bOnce = false;
    if( !bOnce )
    {
        bOnce = true;
        CPLDebug("LOCK", "DEBUG_CONTENTION not available");
    }
}
#endif

/************************************************************************/
/*                           CPLLockHolder()                            */
/************************************************************************/

CPLLockHolder::CPLLockHolder( CPLLock **phLock,
                              CPLLockType eType,
                              const char *pszFileIn,
                              int nLineIn )

{
#ifndef MUTEX_NONE
    pszFile = pszFileIn;
    nLine = nLineIn;

#ifdef DEBUG_MUTEX
    // XXX: There is no way to use CPLDebug() here because it works with
    // mutexes itself so we will fall in infinite recursion. Good old
    // fprintf() will do the job right.
    fprintf( stderr,
             "CPLLockHolder: Request %p for pid %ld at %d/%s.\n",
             *phLock, static_cast<long>(CPLGetPID()), nLine, pszFile );
#endif

    if( !CPLCreateOrAcquireLock( phLock, eType ) )
    {
        fprintf( stderr, "CPLLockHolder: Failed to acquire lock!\n" );
        hLock = NULL;
    }
    else
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "CPLLockHolder: Acquired %p for pid %ld at %d/%s.\n",
                 *phLock, static_cast<long>(CPLGetPID()), nLine, pszFile );
#endif

        hLock = *phLock;
    }
#endif  // ndef MUTEX_NONE
}

/************************************************************************/
/*                           CPLLockHolder()                            */
/************************************************************************/

CPLLockHolder::CPLLockHolder( CPLLock *hLockIn,
                              const char *pszFileIn,
                              int nLineIn )

{
#ifndef MUTEX_NONE
    pszFile = pszFileIn;
    nLine = nLineIn;
    hLock = hLockIn;

    if( hLock != NULL )
    {
        if( !CPLAcquireLock( hLock ) )
        {
            fprintf( stderr, "CPLLockHolder: Failed to acquire lock!\n" );
            hLock = NULL;
        }
    }
#endif // ndef MUTEX_NONE
}

/************************************************************************/
/*                          ~CPLLockHolder()                            */
/************************************************************************/

CPLLockHolder::~CPLLockHolder()

{
#ifndef MUTEX_NONE
    if( hLock != NULL )
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "~CPLLockHolder: Release %p for pid %ld at %d/%s.\n",
                 hLock, static_cast<long>(CPLGetPID()), nLine, pszFile );
#endif
        CPLReleaseLock( hLock );
    }
#endif  // ndef MUTEX_NONE
}

/************************************************************************/
/*                       CPLGetCurrentProcessID()                       */
/************************************************************************/

#ifdef CPL_MULTIPROC_WIN32

int CPLGetCurrentProcessID()
{
    return GetCurrentProcessId();
}

#else

#include <sys/types.h>
#include <unistd.h>

int CPLGetCurrentProcessID()
{
    return getpid();
}

#endif

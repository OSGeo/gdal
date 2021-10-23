/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Multi-Threading, and process handling portability functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

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
    bool     bDebugPerfAsked;
    bool     bDebugPerf;
    volatile int nCurrentHolders;
    GUIntBig nStartTime;
    GIntBig  nMaxDiff;
    double   dfAvgDiff;
    GUIntBig nIters;
#endif
};

#ifdef DEBUG_CONTENTION

#if defined(__x86_64)
#define GCC_CPUID(level, a, b, c, d)            \
  __asm__ volatile ("xchgq %%rbx, %q1\n"                 \
           "cpuid\n"                            \
           "xchgq %%rbx, %q1"                   \
       : "=a" (a), "=r" (b), "=c" (c), "=d" (d) \
       : "0" (level))
#else
#define GCC_CPUID(level, a, b, c, d)            \
  __asm__ volatile ("xchgl %%ebx, %1\n"                  \
           "cpuid\n"                            \
           "xchgl %%ebx, %1"                    \
       : "=a" (a), "=r" (b), "=c" (c), "=d" (d) \
       : "0" (level))
#endif

static GUIntBig CPLrdtsc()
{
    unsigned int a;
    unsigned int d;
    unsigned int unused1;
    unsigned int unused2;
    unsigned int unused3;
    unsigned int unused4;
    GCC_CPUID(0, unused1, unused2, unused3, unused4);
    __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d) );
    return static_cast<GUIntBig>(a) | (static_cast<GUIntBig>(d) << 32);
}

static GUIntBig CPLrdtscp()
{
    unsigned int a;
    unsigned int d;
    unsigned int unused1;
    unsigned int unused2;
    unsigned int unused3;
    unsigned int unused4;
    __asm__ volatile ("rdtscp" : "=a" (a), "=d" (d) );
    GCC_CPUID(0, unused1, unused2, unused3, unused4);
    return static_cast<GUIntBig>(a) | (static_cast<GUIntBig>(d) << 32);
}
#endif

static CPLSpinLock *CPLCreateSpinLock();  // Returned NON acquired.
static int     CPLCreateOrAcquireSpinLockInternal( CPLLock** );
static int     CPLAcquireSpinLock( CPLSpinLock* );
static void    CPLReleaseSpinLock( CPLSpinLock* );
static void    CPLDestroySpinLock( CPLSpinLock* );

#ifndef CPL_MULTIPROC_PTHREAD
#ifndef MUTEX_NONE
static CPLMutex*    CPLCreateOrAcquireMasterMutex( double );
static CPLMutex*&   CPLCreateOrAcquireMasterMutexInternal( double );
static CPLMutex*    CPLCreateUnacquiredMutex();
#endif
#endif

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
    hMutex(nullptr),
    pszFile(pszFileIn),
    nLine(nLineIn)
{
    if( phMutex == nullptr )
    {
        fprintf( stderr, "CPLMutexHolder: phMutex )) NULL !\n" );
        hMutex = nullptr;
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
        hMutex = nullptr;
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
    if( hMutex != nullptr &&
        !CPLAcquireMutex( hMutex, dfWaitInSeconds ) )
    {
        fprintf( stderr, "CPLMutexHolder: Failed to acquire mutex!\n" );
        hMutex = nullptr;
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
    if( hMutex != nullptr )
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
CPLMutex* CPLCreateUnacquiredMutex()
{
    CPLMutex *hMutex = CPLCreateMutex();
    if (hMutex)
    {
        CPLReleaseMutex(hMutex);
    }
    return hMutex;
}

CPLMutex*& CPLCreateOrAcquireMasterMutexInternal(double dfWaitInSeconds = 1000.0)
{
    // The dynamic initialization of the block scope hCOAMutex
    // with static storage duration is thread-safe in C++11
    static CPLMutex *hCOAMutex = CPLCreateUnacquiredMutex();

    // WARNING: although adding an CPLAssert(hCOAMutex); might seem logical
    // here, do not enable it (see comment below). It calls CPLError that
    // uses the hCOAMutex itself leading to recursive mutex acquisition
    // and likely a stack overflow.

    if ( !hCOAMutex )
    {
        // Fall back to this, ironically, NOT thread-safe re-initialisation of
        // hCOAMutex in case of a memory error or call to CPLCleanupMasterMutex
        // sequenced in an unusual, unexpected or erroneous way.
        // For example, an unusual sequence could be:
        //   GDALDriverManager has been instantiated,
        //   then OGRCleanupAll is called which calls CPLCleanupMasterMutex,
        //   then CPLFreeConfig is called which acquires the hCOAMutex
        //   that has already been released and destroyed.

        hCOAMutex = CPLCreateUnacquiredMutex();
    }

    if( hCOAMutex )
    {
        CPLAcquireMutex( hCOAMutex, dfWaitInSeconds );
    }

    return hCOAMutex;
}

CPLMutex* CPLCreateOrAcquireMasterMutex(double dfWaitInSeconds = 1000.0)
{
    CPLMutex *hCOAMutex = CPLCreateOrAcquireMasterMutexInternal(dfWaitInSeconds);
    return hCOAMutex;
}
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

    CPLMutex* hCOAMutex = CPLCreateOrAcquireMasterMutex( dfWaitInSeconds );
    if( hCOAMutex == nullptr )
    {
        *phMutex = nullptr;
        return FALSE;
    }

    if( *phMutex == nullptr )
    {
        *phMutex = CPLCreateMutexEx( nOptions );
        bSuccess = *phMutex != nullptr;
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

    CPLMutex* hCOAMutex = CPLCreateOrAcquireMasterMutex( dfWaitInSeconds );
    if( hCOAMutex == nullptr )
    {
        *phLock = nullptr;
        return FALSE;
    }

    if( *phLock == nullptr )
    {
        *phLock = static_cast<CPLLock *>(calloc(1, sizeof(CPLLock)));
        if( *phLock )
        {
            (*phLock)->eType = eType;
            (*phLock)->u.hMutex = CPLCreateMutexEx(
                (eType == LOCK_RECURSIVE_MUTEX)
                ? CPL_MUTEX_RECURSIVE
                : CPL_MUTEX_ADAPTIVE );
            if( (*phLock)->u.hMutex == nullptr )
            {
                free(*phLock);
                *phLock = nullptr;
            }
        }
        bSuccess = *phLock != nullptr;
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
    CPLMutex*& hCOAMutex = CPLCreateOrAcquireMasterMutexInternal();
    if( hCOAMutex != nullptr )
    {
        CPLReleaseMutex( hCOAMutex );
        CPLDestroyMutex( hCOAMutex );
        hCOAMutex = nullptr;
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

    if( papTLSList == nullptr )
        return;

    for( int i = 0; i < CTLS_MAX; i++ )
    {
        if( papTLSList[i] != nullptr && papTLSList[i+CTLS_MAX] != nullptr )
        {
            CPLTLSFreeFunc pfnFree = reinterpret_cast<CPLTLSFreeFunc>(papTLSList[i + CTLS_MAX]);
            pfnFree( papTLSList[i] );
            papTLSList[i] = nullptr;
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
    if( pabyMutex == nullptr )
        return nullptr;

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
    return nullptr;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

void CPLCondWait( CPLCond * /* hCond */ , CPLMutex* /* hMutex */ ) {}

/************************************************************************/
/*                         CPLCondTimedWait()                           */
/************************************************************************/

CPLCondTimedWaitReason CPLCondTimedWait( CPLCond * /* hCond */ ,
                                         CPLMutex* /* hMutex */, double )
{
    return COND_TIMED_WAIT_OTHER;
}

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
    while( fpLock != nullptr && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( std::min(dfWaitInSeconds, 0.5) );
        dfWaitInSeconds -= 0.5;

        fpLock = fopen( pszLockFilename, "r" );
    }

    if( fpLock != nullptr )
    {
        fclose( fpLock );
        CPLFree( pszLockFilename );
        return nullptr;
    }

    fpLock = fopen( pszLockFilename, "w" );

    if( fpLock == nullptr )
    {
        CPLFree( pszLockFilename );
        return nullptr;
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

    if( hLock == nullptr )
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

    return nullptr;
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

static void **papTLSList = nullptr;

static void **CPLGetTLSList( int *pbMemoryErrorOccurred )

{
    if( pbMemoryErrorOccurred )
        *pbMemoryErrorOccurred = FALSE;
    if( papTLSList == nullptr )
    {
        papTLSList =
            static_cast<void **>(VSICalloc(sizeof(void*), CTLS_MAX * 2));
        if( papTLSList == nullptr )
        {
            if( pbMemoryErrorOccurred )
            {
                *pbMemoryErrorOccurred = TRUE;
                fprintf(stderr,
                        "CPLGetTLSList() failed to allocate TLS list!\n");
                return nullptr;
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
    papTLSList = nullptr;
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
    HANDLE hMutex = CreateMutex( nullptr, TRUE, nullptr );

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

    return reinterpret_cast<CPLMutex *>(pcs);
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
    CRITICAL_SECTION *pcs = reinterpret_cast<CRITICAL_SECTION*>(hMutexIn);
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
    CRITICAL_SECTION *pcs = reinterpret_cast<CRITICAL_SECTION*>(hMutexIn);

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
    CRITICAL_SECTION *pcs = reinterpret_cast<CRITICAL_SECTION*>(hMutexIn);

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
    if( psCond == nullptr )
        return nullptr;
    psCond->hInternalMutex = CPLCreateMutex();
    if( psCond->hInternalMutex == nullptr )
    {
        free(psCond);
        return nullptr;
    }
    CPLReleaseMutex(psCond->hInternalMutex);
    psCond->psWaiterList = nullptr;
    return reinterpret_cast<CPLCond*>(psCond);
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

static void CPLTLSFreeEvent( void* pData )
{
    CloseHandle(static_cast<HANDLE>(pData));
}

void CPLCondWait( CPLCond *hCond, CPLMutex* hClientMutex )
{
    CPLCondTimedWait(hCond, hClientMutex, -1);
}

/************************************************************************/
/*                         CPLCondTimedWait()                           */
/************************************************************************/

CPLCondTimedWaitReason CPLCondTimedWait( CPLCond *hCond, CPLMutex* hClientMutex,
                                         double dfWaitInSeconds )
{
    Win32Cond* psCond = reinterpret_cast<Win32Cond*>(hCond);

    HANDLE hEvent = static_cast<HANDLE>(CPLGetTLS(CTLS_WIN32_COND));
    if( hEvent == nullptr )
    {
        hEvent = CreateEvent(nullptr, /* security attributes */
                             0,    /* manual reset = no */
                             0,    /* initial state = unsignaled */
                             nullptr  /* no name */);
        CPLAssert(hEvent != nullptr);

        CPLSetTLSWithFreeFunc(CTLS_WIN32_COND, hEvent, CPLTLSFreeEvent);
    }

    /* Insert the waiter into the waiter list of the condition */
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psItem = static_cast<WaiterItem *>(malloc(sizeof(WaiterItem)));
    CPLAssert(psItem != nullptr);

    psItem->hEvent = hEvent;
    psItem->psNext = psCond->psWaiterList;

    psCond->psWaiterList = psItem;

    CPLReleaseMutex(psCond->hInternalMutex);

    // Release the client mutex before waiting for the event being signaled.
    CPLReleaseMutex(hClientMutex);

    // Ideally we would check that we do not get WAIT_FAILED but it is hard
    // to report a failure.
    auto ret = WaitForSingleObject(hEvent, dfWaitInSeconds < 0 ?
                    INFINITE : static_cast<int>(dfWaitInSeconds * 1000));

    // Reacquire the client mutex.
    CPLAcquireMutex(hClientMutex, 1000.0);

    if( ret == WAIT_OBJECT_0 )
        return COND_TIMED_WAIT_COND;
    if( ret == WAIT_TIMEOUT )
        return COND_TIMED_WAIT_TIME_OUT;
    return COND_TIMED_WAIT_OTHER;
}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void CPLCondSignal( CPLCond *hCond )
{
    Win32Cond* psCond = reinterpret_cast<Win32Cond*>(hCond);

    // Signal the first registered event, and remove it from the list.
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psIter = psCond->psWaiterList;
    if( psIter != nullptr )
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
    Win32Cond* psCond = reinterpret_cast<Win32Cond*>(hCond);

    // Signal all the registered events, and remove them from the list.
    CPLAcquireMutex(psCond->hInternalMutex, 1000.0);

    WaiterItem* psIter = psCond->psWaiterList;
    while( psIter != nullptr )
    {
        WaiterItem* psNext = psIter->psNext;
        SetEvent(psIter->hEvent);
        free(psIter);
        psIter = psNext;
    }
    psCond->psWaiterList = nullptr;

    CPLReleaseMutex(psCond->hInternalMutex);
}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void CPLDestroyCond( CPLCond *hCond )
{
    Win32Cond* psCond = reinterpret_cast<Win32Cond*>(hCond);
    CPLDestroyMutex(psCond->hInternalMutex);
    psCond->hInternalMutex = nullptr;
    CPLAssert(psCond->psWaiterList == nullptr);
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

    // FIXME: use CreateFileW()
    HANDLE hLockFile =
        CreateFileA(pszLockFilename, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, nullptr);

    while( GetLastError() == ERROR_ALREADY_EXISTS
           && dfWaitInSeconds > 0.0 )
    {
        CloseHandle( hLockFile );
        CPLSleep( std::min(dfWaitInSeconds, 0.125) );
        dfWaitInSeconds -= 0.125;

        hLockFile =
            CreateFileA( pszLockFilename, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE,
                        nullptr );
    }

    CPLFree( pszLockFilename );

    if( hLockFile == INVALID_HANDLE_VALUE )
        return nullptr;

    if( GetLastError() == ERROR_ALREADY_EXISTS )
    {
        CloseHandle( hLockFile );
        return nullptr;
    }

    return static_cast<void *>(hLockFile);
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
    HANDLE    hLockFile = static_cast<HANDLE>(hLock);

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

    if( psInfo->hThread == nullptr )
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
    psInfo->hThread = nullptr;

    DWORD nThreadId = 0;
    HANDLE hThread = CreateThread( nullptr, 0, CPLStdCallThreadJacket, psInfo,
                                   0, &nThreadId );

    if( hThread == nullptr )
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
    HANDLE hThread = CreateThread( nullptr, 0, CPLStdCallThreadJacket, psInfo,
                                   0, &nThreadId );

    if( hThread == nullptr )
        return nullptr;

    psInfo->hThread = hThread;
    return reinterpret_cast<CPLJoinableThread*>(psInfo);
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread( CPLJoinableThread* hJoinableThread )
{
    CPLStdCallThreadInfo *psInfo =
        reinterpret_cast<CPLStdCallThreadInfo *>(hJoinableThread);

    WaitForSingleObject(psInfo->hThread, INFINITE);
    CloseHandle( psInfo->hThread );
    CPLFree( psInfo );
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    Sleep( static_cast<DWORD>(dfWaitInSeconds * 1000.0) );
}

static bool bTLSKeySetup = false;
static DWORD nTLSKey = 0;

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **CPLGetTLSList( int *pbMemoryErrorOccurred )

{
    void **papTLSList = nullptr;

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
                return nullptr;
            }
            CPLEmergencyError( "CPLGetTLSList(): TlsAlloc() failed!" );
        }
        bTLSKeySetup = true;
    }

    papTLSList = static_cast<void**>(TlsGetValue( nTLSKey ));
    if( papTLSList == nullptr )
    {
        papTLSList =
            static_cast<void **>(VSICalloc(sizeof(void*), CTLS_MAX * 2));
        if( papTLSList == nullptr )
        {
            if( pbMemoryErrorOccurred )
            {
                *pbMemoryErrorOccurred = TRUE;
                fprintf(stderr,
                        "CPLGetTLSList() failed to allocate TLS list!\n" );
                return nullptr;
            }
            CPLEmergencyError("CPLGetTLSList() failed to allocate TLS list!");
        }
        if( TlsSetValue( nTLSKey, papTLSList ) == 0 )
        {
            if( pbMemoryErrorOccurred )
            {
                *pbMemoryErrorOccurred = TRUE;
                fprintf(stderr, "CPLGetTLSList(): TlsSetValue() failed!\n" );
                return nullptr;
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

    void **papTLSList = static_cast<void **>(TlsGetValue( nTLSKey ));
    if( papTLSList == nullptr )
        return;

    TlsSetValue( nTLSKey, nullptr );

    CPLCleanupTLSList( papTLSList );
}

// endif CPL_MULTIPROC_WIN32

#elif defined(CPL_MULTIPROC_PTHREAD)

#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

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
    int nCPUs;
#ifdef _SC_NPROCESSORS_ONLN
    nCPUs = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    nCPUs = 1;
#endif

    // In a Docker/LXC containers the number of CPUs might be limited
    FILE* f = fopen("/sys/fs/cgroup/cpuset/cpuset.cpus", "rb");
    if(f)
    {
        constexpr size_t nMaxCPUs = 8*64; // 8 Sockets * 64 threads = 512
        constexpr size_t nBuffSize(nMaxCPUs*4); // 3 digits + delimiter per CPU
        char*            pszBuffer =
                            reinterpret_cast<char*>(CPLMalloc(nBuffSize));
        const size_t     nRead = fread(pszBuffer, 1, nBuffSize - 1, f);
        pszBuffer[nRead] = 0;
        fclose(f);
        char **papszCPUsList =
            CSLTokenizeStringComplex(pszBuffer, ",", FALSE, FALSE);

        CPLFree(pszBuffer);

        int nCpusetCpus = 0;
        for(int i = 0; papszCPUsList[i] != nullptr; ++i)
        {
            if(strchr(papszCPUsList[i], '-'))
            {
                char **papszCPUsRange =
                  CSLTokenizeStringComplex(papszCPUsList[i], "-", FALSE, FALSE);
                if(CSLCount(papszCPUsRange) == 2)
                {
                    int iBegin(atoi(papszCPUsRange[0]));
                    int iEnd(atoi(papszCPUsRange[1]));
                    nCpusetCpus += (iEnd - iBegin + 1);
                }
                CSLDestroy(papszCPUsRange);
            }
            else
            {
                ++nCpusetCpus;
            }
        }
        CSLDestroy(papszCPUsList);
        nCPUs = std::min(nCPUs, std::max(1, nCpusetCpus));
    }

    return nCPUs;
}

/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif

static CPLMutex *CPLCreateMutexInternal( bool bAlreadyInGlobalLock,
                                         int nOptions );

int CPLCreateOrAcquireMutexEx( CPLMutex **phMutex, double dfWaitInSeconds,
                               int nOptions )

{
    bool bSuccess = false;

    pthread_mutex_lock(&global_mutex);
    if( *phMutex == nullptr )
    {
        *phMutex = CPLCreateMutexInternal(true, nOptions);
        bSuccess = *phMutex != nullptr;
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
    if( *phLock == nullptr )
    {
        *phLock = static_cast<CPLLock *>(calloc(1, sizeof(CPLLock)));
        if( *phLock )
        {
            (*phLock)->eType = eType;
            (*phLock)->u.hMutex = CPLCreateMutexInternal(
                true,
                eType == LOCK_RECURSIVE_MUTEX
                ? CPL_MUTEX_RECURSIVE : CPL_MUTEX_ADAPTIVE );
            if( (*phLock)->u.hMutex == nullptr )
            {
                free(*phLock);
                *phLock = nullptr;
            }
        }
        bSuccess = *phLock != nullptr;
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
static MutexLinkedElt* psMutexList = nullptr;

static void CPLInitMutex( MutexLinkedElt* psItem )
{
    if( psItem->nOptions == CPL_MUTEX_REGULAR )
    {
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
        pthread_mutex_t tmp_mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif
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
    if( psItem == nullptr )
    {
        fprintf(stderr, "CPLCreateMutexInternal() failed.\n");
        return nullptr;
    }

    if( !bAlreadyInGlobalLock )
        pthread_mutex_lock(&global_mutex);
    psItem->psPrev = nullptr;
    psItem->psNext = psMutexList;
    if( psMutexList )
        psMutexList->psPrev = psItem;
    psMutexList = psItem;
    if( !bAlreadyInGlobalLock )
        pthread_mutex_unlock(&global_mutex);

    psItem->nOptions = nOptions;
    CPLInitMutex(psItem);

    // Mutexes are implicitly acquired when created.
    CPLAcquireMutex( reinterpret_cast<CPLMutex*>(psItem), 0.0 );

    return reinterpret_cast<CPLMutex*>(psItem);
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
    MutexLinkedElt* psItem = reinterpret_cast<MutexLinkedElt *>(hMutexIn);
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
    MutexLinkedElt* psItem = reinterpret_cast<MutexLinkedElt *>(hMutexIn);
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
    MutexLinkedElt* psItem = reinterpret_cast<MutexLinkedElt *>(hMutexIn);
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
    while( psItem != nullptr )
    {
        CPLInitMutex(psItem);
        psItem = psItem->psNext;
    }
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
    pthread_mutex_t tmp_global_mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif
    global_mutex = tmp_global_mutex;
}

/************************************************************************/
/*                            CPLCreateCond()                           */
/************************************************************************/

CPLCond *CPLCreateCond()
{
    pthread_cond_t* pCond =
      static_cast<pthread_cond_t *>(malloc(sizeof(pthread_cond_t)));
    if( pCond && pthread_cond_init(pCond, nullptr) == 0 )
        return reinterpret_cast<CPLCond*>(pCond);
    fprintf(stderr, "CPLCreateCond() failed.\n");
    free(pCond);
    return nullptr;
}

/************************************************************************/
/*                            CPLCondWait()                             */
/************************************************************************/

void CPLCondWait( CPLCond *hCond, CPLMutex* hMutex )
{
    pthread_cond_t* pCond = reinterpret_cast<pthread_cond_t*>(hCond);
    MutexLinkedElt* psItem = reinterpret_cast<MutexLinkedElt *>(hMutex);
    pthread_mutex_t * pMutex = &(psItem->sMutex);
    pthread_cond_wait(pCond, pMutex);
}

/************************************************************************/
/*                         CPLCondTimedWait()                           */
/************************************************************************/

CPLCondTimedWaitReason CPLCondTimedWait( CPLCond *hCond, CPLMutex* hMutex,
                                         double dfWaitInSeconds )
{
    pthread_cond_t* pCond = reinterpret_cast<pthread_cond_t*>(hCond);
    MutexLinkedElt* psItem = reinterpret_cast<MutexLinkedElt *>(hMutex);
    pthread_mutex_t * pMutex = &(psItem->sMutex);
    struct timeval tv;
    struct timespec ts;

    gettimeofday(&tv, nullptr);
    ts.tv_sec = time(nullptr) + static_cast<int>(dfWaitInSeconds);
    ts.tv_nsec = tv.tv_usec * 1000 + static_cast<int>(
                            1000 * 1000 * 1000 * fmod(dfWaitInSeconds, 1));
    ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
    ts.tv_nsec %= (1000 * 1000 * 1000);
    int ret = pthread_cond_timedwait(pCond, pMutex, &ts);
    if( ret == 0 )
        return COND_TIMED_WAIT_COND;
    else if( ret == ETIMEDOUT )
        return COND_TIMED_WAIT_TIME_OUT;
    else
        return COND_TIMED_WAIT_OTHER;
}

/************************************************************************/
/*                            CPLCondSignal()                           */
/************************************************************************/

void CPLCondSignal( CPLCond *hCond )
{
    pthread_cond_t* pCond = reinterpret_cast<pthread_cond_t*>(hCond);
    pthread_cond_signal(pCond);
}

/************************************************************************/
/*                           CPLCondBroadcast()                         */
/************************************************************************/

void CPLCondBroadcast( CPLCond *hCond )
{
    pthread_cond_t* pCond = reinterpret_cast<pthread_cond_t*>(hCond);
    pthread_cond_broadcast(pCond);
}

/************************************************************************/
/*                            CPLDestroyCond()                          */
/************************************************************************/

void CPLDestroyCond( CPLCond *hCond )
{
    pthread_cond_t* pCond = reinterpret_cast<pthread_cond_t*>(hCond);
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
    while( fpLock != nullptr && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( std::min(dfWaitInSeconds, 0.5) );
        dfWaitInSeconds -= 0.5;

        fpLock = fopen( pszLockFilename, "r" );
    }

    if( fpLock != nullptr )
    {
        fclose( fpLock );
        CPLFree( pszLockFilename );
        return nullptr;
    }

    fpLock = fopen( pszLockFilename, "w" );

    if( fpLock == nullptr )
    {
        CPLFree( pszLockFilename );
        return nullptr;
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

    if( hLock == nullptr )
        return;

    VSIUnlink( pszLockFilename );

    CPLFree( pszLockFilename );
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

GIntBig CPLGetPID()

{
    return reinterpret_cast<GIntBig>(reinterpret_cast<void*>(pthread_self()));
}

static pthread_key_t oTLSKey;
static pthread_once_t oTLSKeySetup = PTHREAD_ONCE_INIT;

/************************************************************************/
/*                             CPLMake_key()                            */
/************************************************************************/

static void CPLMake_key()

{
    if( pthread_key_create( &oTLSKey,
                reinterpret_cast<void (*)(void*)>(CPLCleanupTLSList) ) != 0 )
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
            return nullptr;
        }
        CPLEmergencyError( "CPLGetTLSList(): pthread_once() failed!" );
    }

    void **papTLSList = static_cast<void **>(pthread_getspecific( oTLSKey ));
    if( papTLSList == nullptr )
    {
        papTLSList =
            static_cast<void **>(VSICalloc(sizeof(void*), CTLS_MAX * 2));
        if( papTLSList == nullptr )
        {
            if( pbMemoryErrorOccurred )
            {
                fprintf(stderr,
                        "CPLGetTLSList() failed to allocate TLS list!\n" );
                *pbMemoryErrorOccurred = TRUE;
                return nullptr;
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
                return nullptr;
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

    return nullptr;

#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
error:
    assert( pthread_mutex_lock( &(psInfo->sMutex) ) == 0);
    psInfo->bInitSucceeded = false;
    psInfo->bInitDone = true;
    assert( pthread_cond_signal( &(psInfo->sCond) ) == 0);
    assert( pthread_mutex_unlock( &(psInfo->sMutex) ) == 0);
    return nullptr;
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
    if( psInfo == nullptr )
        return -1;
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->bJoinable = false;
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    psInfo->bInitSucceeded = true;
    psInfo->bInitDone = false;
    pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
    psInfo->sMutex = sMutex;
    if( pthread_cond_init(&(psInfo->sCond), nullptr) != 0 )
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
                        CPLStdCallThreadJacket, static_cast<void *>(psInfo) ) != 0 )
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
    if( psInfo == nullptr )
        return nullptr;
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;
    psInfo->bJoinable = true;
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
    psInfo->bInitSucceeded = true;
    psInfo->bInitDone = false;
    pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
    psInfo->sMutex = sMutex;
    {
        int err = pthread_cond_init(&(psInfo->sCond), nullptr);
        if( err != 0 )
        {
            CPLFree( psInfo );
            fprintf(stderr, "CPLCreateJoinableThread() failed: %s.\n",
                    strerror(err));
            return nullptr;
        }
    }
#endif

    pthread_attr_t hThreadAttr;
    pthread_attr_init( &hThreadAttr );
    pthread_attr_setdetachstate( &hThreadAttr, PTHREAD_CREATE_JOINABLE );
    int err = pthread_create( &(psInfo->hThread), &hThreadAttr,
                        CPLStdCallThreadJacket, static_cast<void *>(psInfo) );
    if( err != 0 )
    {
#ifdef CHECK_THREAD_CAN_ALLOCATE_TLS
        pthread_cond_destroy(&(psInfo->sCond));
#endif
        CPLFree( psInfo );
        fprintf(stderr, "CPLCreateJoinableThread() failed: %s.\n",
                strerror(err));
        return nullptr;
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
        return nullptr;
    }
#endif

    return reinterpret_cast<CPLJoinableThread*>(psInfo);
}

/************************************************************************/
/*                          CPLJoinThread()                             */
/************************************************************************/

void CPLJoinThread(CPLJoinableThread* hJoinableThread)
{
    CPLStdCallThreadInfo *psInfo =
        reinterpret_cast<CPLStdCallThreadInfo*>(hJoinableThread);
    if( psInfo == nullptr )
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
    void **papTLSList = static_cast<void **>(pthread_getspecific( oTLSKey ));
    if( papTLSList == nullptr )
        return;

    pthread_setspecific( oTLSKey, nullptr );

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
    if( psSpin != nullptr &&
        pthread_spin_init(&(psSpin->spin), PTHREAD_PROCESS_PRIVATE) == 0 )
    {
        return psSpin;
    }
    else
    {
        fprintf(stderr, "CPLCreateSpinLock() failed.\n");
        free(psSpin);
        return nullptr;
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
    if( *ppsLock == nullptr )
    {
        *ppsLock = static_cast<CPLLock *>(calloc(1, sizeof(CPLLock)));
        if( *ppsLock != nullptr )
        {
            (*ppsLock)->eType = LOCK_SPIN;
            (*ppsLock)->u.hSpinLock = CPLCreateSpinLock();
            if( (*ppsLock)->u.hSpinLock == nullptr )
            {
                free(*ppsLock);
                *ppsLock = nullptr;
            }
        }
    }
    pthread_mutex_unlock(&global_mutex);
    // coverity[missing_unlock]
    return( *ppsLock != nullptr && CPLAcquireSpinLock( (*ppsLock)->u.hSpinLock ) );
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
    void** l_papTLSList = CPLGetTLSList(nullptr);

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    return l_papTLSList[nIndex];
}

/************************************************************************/
/*                            CPLGetTLSEx()                             */
/************************************************************************/

void *CPLGetTLSEx( int nIndex, int* pbMemoryErrorOccurred )

{
    void** l_papTLSList = CPLGetTLSList(pbMemoryErrorOccurred);
    if( l_papTLSList == nullptr )
        return nullptr;

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    return l_papTLSList[nIndex];
}

/************************************************************************/
/*                             CPLSetTLS()                              */
/************************************************************************/

void CPLSetTLS( int nIndex, void *pData, int bFreeOnExit )

{
    CPLSetTLSWithFreeFunc(nIndex, pData, (bFreeOnExit) ? CPLFree : nullptr);
}

/************************************************************************/
/*                      CPLSetTLSWithFreeFunc()                         */
/************************************************************************/

// Warning: The CPLTLSFreeFunc must not in any case directly or indirectly
// use or fetch any TLS data, or a terminating thread will hang!
void CPLSetTLSWithFreeFunc( int nIndex, void *pData, CPLTLSFreeFunc pfnFree )

{
    void **l_papTLSList = CPLGetTLSList(nullptr);

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    l_papTLSList[nIndex] = pData;
    l_papTLSList[CTLS_MAX + nIndex] = reinterpret_cast<void*>(pfnFree);
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
    l_papTLSList[CTLS_MAX + nIndex] = reinterpret_cast<void*>(pfnFree);
}
#ifndef HAVE_SPINLOCK_IMPL

// No spinlock specific API? Fallback to mutex.

/************************************************************************/
/*                          CPLCreateSpinLock()                         */
/************************************************************************/

CPLSpinLock *CPLCreateSpinLock( void )
{
    CPLSpinLock* psSpin = reinterpret_cast<CPLSpinLock *>(CPLCreateMutex());
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
    return CPLAcquireMutex( reinterpret_cast<CPLMutex*>(psSpin), 1000 );
}

/************************************************************************/
/*                       CPLReleaseSpinLock()                           */
/************************************************************************/

void CPLReleaseSpinLock( CPLSpinLock* psSpin )
{
    CPLReleaseMutex( reinterpret_cast<CPLMutex*>(psSpin) );
}

/************************************************************************/
/*                        CPLDestroySpinLock()                          */
/************************************************************************/

void CPLDestroySpinLock( CPLSpinLock* psSpin )
{
    CPLDestroyMutex( reinterpret_cast<CPLMutex*>(psSpin) );
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
                return nullptr;
            CPLReleaseMutex(hMutex);
            CPLLock* psLock = static_cast<CPLLock *>(malloc(sizeof(CPLLock)));
            if( psLock == nullptr )
            {
                fprintf(stderr, "CPLCreateLock() failed.\n");
                CPLDestroyMutex(hMutex);
                return nullptr;
            }
            psLock->eType = eType;
            psLock->u.hMutex = hMutex;
#ifdef DEBUG_CONTENTION
            psLock->bDebugPerf = false;
            psLock->bDebugPerfAsked = false;
            psLock->nCurrentHolders = 0;
            psLock->nStartTime = 0;
#endif
            return psLock;
        }
        case LOCK_SPIN:
        {
            CPLSpinLock* hSpinLock = CPLCreateSpinLock();
            if( !hSpinLock )
                return nullptr;
            CPLLock* psLock = static_cast<CPLLock *>(malloc(sizeof(CPLLock)));
            if( psLock == nullptr )
            {
                fprintf(stderr, "CPLCreateLock() failed.\n");
                CPLDestroySpinLock(hSpinLock);
                return nullptr;
            }
            psLock->eType = eType;
            psLock->u.hSpinLock = hSpinLock;
#ifdef DEBUG_CONTENTION
            psLock->bDebugPerf = false;
            psLock->bDebugPerfAsked = false;
            psLock->nCurrentHolders = 0;
            psLock->nStartTime = 0;
#endif
            return psLock;
        }
        default:
            CPLAssert(false);
            return nullptr;
    }
}

/************************************************************************/
/*                       CPLCreateOrAcquireLock()                       */
/************************************************************************/

int   CPLCreateOrAcquireLock( CPLLock** ppsLock, CPLLockType eType )
{
#ifdef DEBUG_CONTENTION
    GUIntBig nStartTime = 0;
    if( (*ppsLock) && (*ppsLock)->bDebugPerfAsked )
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
    if( ret && (*ppsLock)->bDebugPerfAsked &&
        CPLAtomicInc(&((*ppsLock)->nCurrentHolders)) == 1 )
    {
        (*ppsLock)->bDebugPerf = true;
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
    GUIntBig nStartTime = 0;
    if( psLock->bDebugPerfAsked )
        nStartTime = CPLrdtsc();
#endif
    int ret;
    if( psLock->eType == LOCK_SPIN )
        ret = CPLAcquireSpinLock( psLock->u.hSpinLock );
    else
        ret =  CPLAcquireMutex( psLock->u.hMutex, 1000 );
#ifdef DEBUG_CONTENTION
    if( ret && psLock->bDebugPerfAsked &&
        CPLAtomicInc(&(psLock->nCurrentHolders)) == 1 )
    {
        psLock->bDebugPerf = true;
        psLock->nStartTime = nStartTime;
    }
#endif
    return ret;
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
    if( psLock->bDebugPerf &&
        CPLAtomicDec(&(psLock->nCurrentHolders)) == 0 )
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
    psLock->bDebugPerfAsked = CPL_TO_BOOL(bEnableIn);
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
        hLock = nullptr;
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

    if( hLock != nullptr )
    {
        if( !CPLAcquireLock( hLock ) )
        {
            fprintf( stderr, "CPLLockHolder: Failed to acquire lock!\n" );
            hLock = nullptr;
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
    if( hLock != nullptr )
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

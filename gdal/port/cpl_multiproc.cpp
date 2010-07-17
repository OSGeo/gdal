/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Multi-Threading, and process handling portability functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#include "cpl_multiproc.h"
#include "cpl_conv.h"

#if !defined(WIN32CE)
#  include <time.h>
#else
#  include <wce_time.h>
#endif

CPL_CVSID("$Id$");

#if defined(CPL_MULTIPROC_STUB) && !defined(DEBUG)
#  define MUTEX_NONE
#endif

/************************************************************************/
/*                           CPLMutexHolder()                           */
/************************************************************************/

CPLMutexHolder::CPLMutexHolder( void **phMutex, double dfWaitInSeconds,
                                const char *pszFileIn, 
                                int nLineIn )

{
#ifndef MUTEX_NONE
    pszFile = pszFileIn;
    nLine = nLineIn;

#ifdef DEBUG_MUTEX
    /*
     * XXX: There is no way to use CPLDebug() here because it works with
     * mutexes itself so we will fall in infinite recursion. Good old
     * fprintf() will do the job right.
     */
    fprintf( stderr,
             "CPLMutexHolder: Request %p for pid %ld at %d/%s.\n", 
             *phMutex, (long) CPLGetPID(), nLine, pszFile );
#endif

    if( !CPLCreateOrAcquireMutex( phMutex, dfWaitInSeconds ) )
    {
        fprintf( stderr, "CPLMutexHolder: Failed to acquire mutex!\n" );
        hMutex = NULL;
    }
    else
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "CPLMutexHolder: Acquired %p for pid %ld at %d/%s.\n", 
                 *phMutex, (long) CPLGetPID(), nLine, pszFile );
#endif

        hMutex = *phMutex;
    }
#endif /* ndef MUTEX_NONE */
}

/************************************************************************/
/*                          ~CPLMutexHolder()                           */
/************************************************************************/

CPLMutexHolder::~CPLMutexHolder()

{
#ifndef MUTEX_NONE
    if( hMutex != NULL )
    {
#ifdef DEBUG_MUTEX
        fprintf( stderr,
                 "~CPLMutexHolder: Release %p for pid %ld at %d/%s.\n", 
                 hMutex, (long) CPLGetPID(), nLine, pszFile );
#endif
        CPLReleaseMutex( hMutex );
    }
#endif /* ndef MUTEX_NONE */
}


/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/

#ifndef CPL_MULTIPROC_PTHREAD
int CPLCreateOrAcquireMutex( void **phMutex, double dfWaitInSeconds )

{
    int bSuccess = FALSE;

#ifndef MUTEX_NONE
    static void *hCOAMutex = NULL;

    /*
    ** ironically, creation of this initial mutex is not threadsafe
    ** even though we use it to ensure that creation of other mutexes
    ** is threadsafe. 
    */
    if( hCOAMutex == NULL )
    {
        hCOAMutex = CPLCreateMutex();
        if (hCOAMutex == NULL)
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
        *phMutex = CPLCreateMutex();
        bSuccess = *phMutex != NULL;
        CPLReleaseMutex( hCOAMutex );
    }
    else
    {
        CPLReleaseMutex( hCOAMutex );

        bSuccess = CPLAcquireMutex( *phMutex, dfWaitInSeconds );
    }
#endif /* ndef MUTEX_NONE */

    return bSuccess;
}
#endif

/************************************************************************/
/*                        CPLCleanupTLSList()                           */
/*                                                                      */
/*      Free resources associated with a TLS vector (implementation     */
/*      independent).                                                   */
/************************************************************************/

static void CPLCleanupTLSList( void **papTLSList )

{
    int i;

//    printf( "CPLCleanupTLSList(%p)\n", papTLSList );
    
    if( papTLSList == NULL )
        return;

    for( i = 0; i < CTLS_MAX; i++ )
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

#ifdef CPL_MULTIPROC_STUB
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
/*                        CPLGetThreadingModel()                        */
/************************************************************************/

const char *CPLGetThreadingModel()

{
    return "stub";
}

/************************************************************************/
/*                           CPLCreateMutex()                           */
/************************************************************************/

void *CPLCreateMutex()

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) CPLMalloc( 4 );

    pabyMutex[0] = 1;
    pabyMutex[1] = 'r';
    pabyMutex[2] = 'e';
    pabyMutex[3] = 'd';

    return (void *) pabyMutex;
#else
    return (void *) 0xdeadbeef;
#endif 
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( void *hMutex, double dfWaitInSeconds )

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    pabyMutex[0] += 1;

    return TRUE;
#else
    return TRUE;
#endif
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( void *hMutex )

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    if( pabyMutex[0] < 1 )
        CPLDebug( "CPLMultiProc", 
                  "CPLReleaseMutex() called on mutex with %d as ref count!",
                  pabyMutex[0] );

    pabyMutex[0] -= 1;
#endif
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( void *hMutex )

{
#ifndef MUTEX_NONE
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    CPLFree( pabyMutex );
#endif
}

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
    FILE      *fpLock;
    char      *pszLockFilename;
    
/* -------------------------------------------------------------------- */
/*      We use a lock file with a name derived from the file we want    */
/*      to lock to represent the file being locked.  Note that for      */
/*      the stub implementation the target file does not even need      */
/*      to exist to be locked.                                          */
/* -------------------------------------------------------------------- */
    pszLockFilename = (char *) CPLMalloc(strlen(pszPath) + 30);
    sprintf( pszLockFilename, "%s.lock", pszPath );

    fpLock = fopen( pszLockFilename, "r" );
    while( fpLock != NULL && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( MIN(dfWaitInSeconds,0.5) );
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
    char *pszLockFilename = (char *) hLock;

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

int CPLCreateThread( CPLThreadFunc pfnMain, void *pArg )

{
    CPLDebug( "CPLCreateThread", "Fails to dummy implementation" );

    return -1;
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    time_t  ltime;
    time_t  ttime;

    time( &ltime );
    ttime = ltime + (int) (dfWaitInSeconds+0.5);

    for( ; ltime < ttime; time(&ltime) )
    {
        /* currently we just busy wait.  Perhaps we could at least block on 
           io? */
    }
}

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **papTLSList = NULL;

static void **CPLGetTLSList()

{
    if( papTLSList == NULL )
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX*2);

    return papTLSList;
}

/************************************************************************/
/*                           CPLCleanupTLS()                            */
/************************************************************************/

void CPLCleanupTLS()

{
    CPLCleanupTLSList( papTLSList );
    papTLSList = NULL;
}

#endif /* def CPL_MULTIPROC_STUB */

#if defined(CPL_MULTIPROC_WIN32)


  /************************************************************************/
  /* ==================================================================== */
  /*                        CPL_MULTIPROC_WIN32                           */
  /*                                                                      */
  /*    WIN32 Implementation of multiprocessing functions.                */
  /* ==================================================================== */
  /************************************************************************/

#include <windows.h>

/* windows.h header must be included above following lines. */
#if defined(WIN32CE)
#  include "cpl_win32ce_api.h"
#  define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif


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

void *CPLCreateMutex()

{
    HANDLE hMutex;

    hMutex = CreateMutex( NULL, TRUE, NULL );

    return (void *) hMutex;
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( void *hMutexIn, double dfWaitInSeconds )

{
    HANDLE hMutex = (HANDLE) hMutexIn;
    DWORD  hr;

    hr = WaitForSingleObject( hMutex, (int) (dfWaitInSeconds * 1000) );
    
    return hr != WAIT_TIMEOUT;
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( void *hMutexIn )

{
    HANDLE hMutex = (HANDLE) hMutexIn;

    ReleaseMutex( hMutex );
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( void *hMutexIn )

{
    HANDLE hMutex = (HANDLE) hMutexIn;

    CloseHandle( hMutex );
}

/************************************************************************/
/*                            CPLLockFile()                             */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
    char      *pszLockFilename;
    HANDLE    hLockFile;
    
    pszLockFilename = (char *) CPLMalloc(strlen(pszPath) + 30);
    sprintf( pszLockFilename, "%s.lock", pszPath );

    hLockFile = 
        CreateFile( pszLockFilename, GENERIC_WRITE, 0, NULL,CREATE_NEW, 
                    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, NULL );

    while( GetLastError() == ERROR_ALREADY_EXISTS
           && dfWaitInSeconds > 0.0 )
    {
        CloseHandle( hLockFile );
        CPLSleep( MIN(dfWaitInSeconds,0.125) );
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
    return (GIntBig) GetCurrentThreadId();
}

/************************************************************************/
/*                       CPLStdCallThreadJacket()                       */
/************************************************************************/

typedef struct {
    void *pAppData;
    CPLThreadFunc pfnMain;
} CPLStdCallThreadInfo;

static DWORD WINAPI CPLStdCallThreadJacket( void *pData )

{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo *) pData;

    psInfo->pfnMain( psInfo->pAppData );

    CPLFree( psInfo );

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
    HANDLE hThread;
    DWORD  nThreadId;
    CPLStdCallThreadInfo *psInfo;

    psInfo = (CPLStdCallThreadInfo*) CPLCalloc(sizeof(CPLStdCallThreadInfo),1);
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;

    hThread = CreateThread( NULL, 0, CPLStdCallThreadJacket, psInfo, 
                            0, &nThreadId );

    if( hThread == NULL )
        return -1;

    CloseHandle( hThread );

    return nThreadId;
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    Sleep( (DWORD) (dfWaitInSeconds * 1000.0) );
}

static int           bTLSKeySetup = FALSE;
static DWORD         nTLSKey;

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **CPLGetTLSList()

{
    void **papTLSList;

    if( !bTLSKeySetup )
    {
        nTLSKey = TlsAlloc();
        if( nTLSKey == TLS_OUT_OF_INDEXES )
        {
            CPLError( CE_Fatal, CPLE_AppDefined, 
                      "TlsAlloc() failed!" );
        }
        bTLSKeySetup = TRUE;
    }

    papTLSList = (void **) TlsGetValue( nTLSKey );
    if( papTLSList == NULL )
    {
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX*2);
        if( TlsSetValue( nTLSKey, papTLSList ) == 0 )
        {
            CPLError( CE_Fatal, CPLE_AppDefined, 
                      "TlsSetValue() failed!" );
        }
    }

    return papTLSList;
}

/************************************************************************/
/*                           CPLCleanupTLS()                            */
/************************************************************************/

void CPLCleanupTLS()

{
    void **papTLSList;

    if( !bTLSKeySetup )
        return;

    papTLSList = (void **) TlsGetValue( nTLSKey );
    if( papTLSList == NULL )
        return;

    TlsSetValue( nTLSKey, NULL );

    CPLCleanupTLSList( papTLSList );
}

#endif /* def CPL_MULTIPROC_WIN32 */

#ifdef CPL_MULTIPROC_PTHREAD
#include <pthread.h>
#include <time.h>

  /************************************************************************/
  /* ==================================================================== */
  /*                        CPL_MULTIPROC_PTHREAD                         */
  /*                                                                      */
  /*    PTHREAD Implementation of multiprocessing functions.              */
  /* ==================================================================== */
  /************************************************************************/

/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/

int CPLCreateOrAcquireMutex( void **phMutex, double dfWaitInSeconds )

{
    int bSuccess = FALSE;
    static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&global_mutex);
    if( *phMutex == NULL )
    {
        *phMutex = CPLCreateMutex();
        bSuccess = *phMutex != NULL;
        pthread_mutex_unlock(&global_mutex);
    }
    else
    {
        pthread_mutex_unlock(&global_mutex);

        bSuccess = CPLAcquireMutex( *phMutex, dfWaitInSeconds );
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

void *CPLCreateMutex()

{
    pthread_mutex_t *hMutex;

    hMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (hMutex == NULL)
        return NULL;

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(HAVE_PTHREAD_MUTEX_RECURSIVE)
    {
        pthread_mutexattr_t  attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init( hMutex, &attr );
    }
/* BSDs have PTHREAD_MUTEX_RECURSIVE as an enum, not a define. */
/* But they have #define MUTEX_TYPE_COUNTING_FAST	PTHREAD_MUTEX_RECURSIVE */
#elif defined(MUTEX_TYPE_COUNTING_FAST)
    {
        pthread_mutexattr_t  attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, MUTEX_TYPE_COUNTING_FAST );
        pthread_mutex_init( hMutex, &attr );
    }
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    pthread_mutex_t tmp_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    *hMutex = tmp_mutex;
#else
#error "Recursive mutexes apparently unsupported, configure --without-threads" 
#endif

    // mutexes are implicitly acquired when created.
    CPLAcquireMutex( hMutex, 0.0 );

    return (void *) hMutex;
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( void *hMutexIn, double dfWaitInSeconds )

{
    int err;

    /* we need to add timeout support */
    err =  pthread_mutex_lock( (pthread_mutex_t *) hMutexIn );
    
    if( err != 0 )
    {
        if( err == EDEADLK )
            fprintf(stderr, "CPLAcquireMutex: Error = %d/EDEADLK", err );
        else
            fprintf(stderr, "CPLAcquireMutex: Error = %d", err );

        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( void *hMutexIn )

{
    pthread_mutex_unlock( (pthread_mutex_t *) hMutexIn );
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( void *hMutexIn )

{
    pthread_mutex_destroy( (pthread_mutex_t *) hMutexIn );
    free( hMutexIn );
}

/************************************************************************/
/*                            CPLLockFile()                             */
/*                                                                      */
/*      This is really a stub implementation, see first                 */
/*      CPLLockFile() for caveats.                                      */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
    FILE      *fpLock;
    char      *pszLockFilename;
    
/* -------------------------------------------------------------------- */
/*      We use a lock file with a name derived from the file we want    */
/*      to lock to represent the file being locked.  Note that for      */
/*      the stub implementation the target file does not even need      */
/*      to exist to be locked.                                          */
/* -------------------------------------------------------------------- */
    pszLockFilename = (char *) CPLMalloc(strlen(pszPath) + 30);
    sprintf( pszLockFilename, "%s.lock", pszPath );

    fpLock = fopen( pszLockFilename, "r" );
    while( fpLock != NULL && dfWaitInSeconds > 0.0 )
    {
        fclose( fpLock );
        CPLSleep( MIN(dfWaitInSeconds,0.5) );
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
    char *pszLockFilename = (char *) hLock;

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
    return (GIntBig) pthread_self();
}

/************************************************************************/
/*                       CPLStdCallThreadJacket()                       */
/************************************************************************/

typedef struct {
    void *pAppData;
    CPLThreadFunc pfnMain;
    pthread_t hThread;
} CPLStdCallThreadInfo;

static void *CPLStdCallThreadJacket( void *pData )

{
    CPLStdCallThreadInfo *psInfo = (CPLStdCallThreadInfo *) pData;

    psInfo->pfnMain( psInfo->pAppData );

    CPLFree( psInfo );

    return NULL;
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

    CPLStdCallThreadInfo *psInfo;
    pthread_attr_t hThreadAttr;

    psInfo = (CPLStdCallThreadInfo*) CPLCalloc(sizeof(CPLStdCallThreadInfo),1);
    psInfo->pAppData = pThreadArg;
    psInfo->pfnMain = pfnMain;

    pthread_attr_init( &hThreadAttr );
    pthread_attr_setdetachstate( &hThreadAttr, PTHREAD_CREATE_DETACHED );
    if( pthread_create( &(psInfo->hThread), &hThreadAttr, 
                        CPLStdCallThreadJacket, (void *) psInfo ) != 0 )
    {
        CPLFree( psInfo );
        return -1;
    }

    return 1; /* can we return the actual thread pid? */
}

/************************************************************************/
/*                              CPLSleep()                              */
/************************************************************************/

void CPLSleep( double dfWaitInSeconds )

{
    struct timespec sRequest, sRemain;

    sRequest.tv_sec = (int) floor(dfWaitInSeconds);
    sRequest.tv_nsec = (int) ((dfWaitInSeconds - sRequest.tv_sec)*1000000000);
    nanosleep( &sRequest, &sRemain );
}

static pthread_key_t  oTLSKey;
static pthread_once_t oTLSKeySetup = PTHREAD_ONCE_INIT;

/************************************************************************/
/*                             CPLMake_key()                            */
/************************************************************************/

static void CPLMake_key()

{
    if( pthread_key_create( &oTLSKey, (void (*)(void*)) CPLCleanupTLSList ) != 0 )
    {
        CPLError( CE_Fatal, CPLE_AppDefined, "pthread_key_create() failed!" );
    }
}

/************************************************************************/
/*                             CPLCleanupTLS()                          */
/************************************************************************/

void CPLCleanupTLS()

{
    void **papTLSList;

    papTLSList = (void **) pthread_getspecific( oTLSKey );
    if( papTLSList == NULL )
        return;

    pthread_setspecific( oTLSKey, NULL );

    CPLCleanupTLSList( papTLSList );
}

/************************************************************************/
/*                           CPLGetTLSList()                            */
/************************************************************************/

static void **CPLGetTLSList()

{
    void **papTLSList;

    if ( pthread_once(&oTLSKeySetup, CPLMake_key) != 0 )
    {
        CPLError( CE_Fatal, CPLE_AppDefined,
            "pthread_once() failed!" );
    }

    papTLSList = (void **) pthread_getspecific( oTLSKey );
    if( papTLSList == NULL )
    {
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX*2);
        if( pthread_setspecific( oTLSKey, papTLSList ) != 0 )
        {
            CPLError( CE_Fatal, CPLE_AppDefined,
                "pthread_setspecific() failed!" );
        }
    }

    return papTLSList;
}

#endif /* def CPL_MULTIPROC_PTHREAD */

/************************************************************************/
/*                             CPLGetTLS()                              */
/************************************************************************/

void *CPLGetTLS( int nIndex )

{
    void** papTLSList = CPLGetTLSList();

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    return papTLSList[nIndex];
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

/* Warning : the CPLTLSFreeFunc must not in any case directly or indirectly */
/* use or fetch any TLS data, or a terminating thread will hang ! */
void CPLSetTLSWithFreeFunc( int nIndex, void *pData, CPLTLSFreeFunc pfnFree )

{
    void **papTLSList = CPLGetTLSList();

    CPLAssert( nIndex >= 0 && nIndex < CTLS_MAX );

    papTLSList[nIndex] = pData;
    papTLSList[CTLS_MAX + nIndex] = (void*) pfnFree;
}

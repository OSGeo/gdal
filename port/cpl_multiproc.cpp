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
 **********************************************************************
 *
 * $Log$
 * Revision 1.9  2005/07/08 14:35:26  fwarmerdam
 * preliminary TLS support
 *
 * Revision 1.8  2005/05/23 16:00:33  fwarmerdam
 * Make sure that stub implementation of mutex support recursive holds.
 *
 * Revision 1.7  2005/05/23 06:40:40  fwarmerdam
 * fixed flaw in CPLCreateOrAcquireMutex, added mutex holder
 *
 * Revision 1.6  2005/05/20 19:19:00  fwarmerdam
 * added CPLCreateOrAcquireMutex()
 *
 * Revision 1.5  2005/04/26 20:52:10  fwarmerdam
 * use a typedef type for thread mains (for Sun port)
 *
 * Revision 1.4  2003/05/06 18:30:54  warmerda
 * fix unix createmutex to implicitly acquire it
 *
 * Revision 1.3  2003/04/23 04:36:54  warmerda
 * pthreads based implementation
 *
 * Revision 1.2  2002/07/11 19:36:34  warmerda
 * CPLCreateMutex() should implicitly acquire it, fix stub version
 *
 * Revision 1.1  2002/05/24 04:01:01  warmerda
 * New
 *
 **********************************************************************/

#include "cpl_multiproc.h"
#include "cpl_conv.h"
#include <time.h>

CPL_CVSID("$Id$");

#undef DEBUG

/************************************************************************/
/*                           CPLMutexHolder()                           */
/************************************************************************/

CPLMutexHolder::CPLMutexHolder( void **phMutex, double dfWaitInSeconds,
                                const char *pszFileIn, 
                                int nLineIn )

{
    pszFile = pszFileIn;
    nLine = nLineIn;

#ifdef DEBUG
    CPLDebug( "MH", "Request %p for pid %d at %d/%s", 
              *phMutex, CPLGetPID(), nLine, pszFile );
#endif

    if( !CPLCreateOrAcquireMutex( phMutex, dfWaitInSeconds ) )
    {
        CPLDebug( "CPLMutexHolder", "failed to acquire mutex!" );
        hMutex = NULL;
    }
    else
    {
#ifdef DEBUG
        CPLDebug( "MH", "Acquired %p for pid %d at %d/%s", 
                  *phMutex, CPLGetPID(), nLine, pszFile );
#endif

        hMutex = *phMutex;
    }
}

/************************************************************************/
/*                          ~CPLMutexHolder()                           */
/************************************************************************/

CPLMutexHolder::~CPLMutexHolder()

{
    if( hMutex != NULL )
    {
#ifdef DEBUG
        CPLDebug( "MH", "Release %p for pid %d at %d/%s", 
                  hMutex, CPLGetPID(), nLine, pszFile );
#endif
        CPLReleaseMutex( hMutex );
    }
}


/************************************************************************/
/*                      CPLCreateOrAcquireMutex()                       */
/************************************************************************/

int CPLCreateOrAcquireMutex( void **phMutex, double dfWaitInSeconds )

{
    static void *hCOAMutex = NULL;

    /*
    ** ironically, creation of this initial mutex is not threadsafe
    ** even though we use it to ensure that creation of other mutexes
    ** is threadsafe. 
    */
    if( hCOAMutex == NULL )
    {
        hCOAMutex = CPLCreateMutex();
    }
    else
    {
        CPLAcquireMutex( hCOAMutex, dfWaitInSeconds );
    }

    if( *phMutex == NULL )
    {
        *phMutex = CPLCreateMutex();
        CPLReleaseMutex( hCOAMutex );
        return TRUE;
    }
    else
    {
        CPLReleaseMutex( hCOAMutex );

        int bSuccess = CPLAcquireMutex( *phMutex, dfWaitInSeconds );
        
        return bSuccess;
    }
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
    unsigned char *pabyMutex = (unsigned char *) CPLMalloc( 4 );

    pabyMutex[0] = 1;
    pabyMutex[1] = 'r';
    pabyMutex[2] = 'e';
    pabyMutex[3] = 'd';

    return (void *) pabyMutex;
}

/************************************************************************/
/*                          CPLAcquireMutex()                           */
/************************************************************************/

int CPLAcquireMutex( void *hMutex, double dfWaitInSeconds )

{
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    pabyMutex[0] += 1;

    return TRUE;
}

/************************************************************************/
/*                          CPLReleaseMutex()                           */
/************************************************************************/

void CPLReleaseMutex( void *hMutex )

{
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    if( pabyMutex[0] < 1 )
        CPLDebug( "CPLMultiProc", 
                  "CPLReleaseMutex() called on mutex with %d as ref count!",
                  pabyMutex[0] );

    pabyMutex[0] -= 1;
}

/************************************************************************/
/*                          CPLDestroyMutex()                           */
/************************************************************************/

void CPLDestroyMutex( void *hMutex )

{
    unsigned char *pabyMutex = (unsigned char *) hMutex;

    CPLAssert( pabyMutex[1] == 'r' && pabyMutex[2] == 'e' 
               && pabyMutex[3] == 'd' );

    CPLFree( pabyMutex );
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

int CPLGetPID()

{
    return 1;
}

/************************************************************************/
/*                          CPLCreateThread();                          */
/************************************************************************/

int CPLCreateThread( CPLThreadFunc pfnMain, void *pArg )

{
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
/*                             CPLGetTLS()                              */
/************************************************************************/

static void **papTLSList = NULL;

void *CPLGetTLS( int nIndex )

{
    if( papTLSList == NULL )
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX);

    return papTLSList[nIndex];
}

/************************************************************************/
/*                             CPLSetTLS()                              */
/************************************************************************/

void CPLSetTLS( int nIndex, void *pData, int bFreeOnExit )

{
    if( papTLSList == NULL )
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX);

    papTLSList[nIndex] = pData;
}

#endif /* def CPL_MULTIPROC_STUB */

#ifdef CPL_MULTIPROC_WIN32
#include <windows.h>

  /************************************************************************/
  /* ==================================================================== */
  /*                        CPL_MULTIPROC_WIN32                           */
  /*                                                                      */
  /*    WIN32 Implementation of multiprocessing functions.                */
  /* ==================================================================== */
  /************************************************************************/


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

int CPLGetPID()

{
    return GetCurrentThreadId();
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

/************************************************************************/
/*                             CPLGetTLS()                              */
/************************************************************************/

static void **papTLSList = NULL;

void *CPLGetTLS( int nIndex )

{
    if( papTLSList == NULL )
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX);

    return papTLSList[nIndex];
}

/************************************************************************/
/*                             CPLSetTLS()                              */
/************************************************************************/

void CPLSetTLS( int nIndex, void *pData, int bFreeOnExit )

{
    if( papTLSList == NULL )
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX);

    papTLSList[nIndex] = pData;
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
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    pthread_mutex_t hMutexSrc = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#elif defined(PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP)
    pthread_mutex_t hMutexSrc = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
    pthread_mutex_t hMutexSrc = PTHREAD_MUTEX_INITIALIZER;
#endif

    hMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    *hMutex = hMutexSrc;

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
            CPLDebug( "CPLAcquireMutex", "Error = %d/EDEADLK", err );
        else
            CPLDebug( "CPLAcquireMutex", "Error = %d", err );

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
    CPLFree( hMutexIn );
}

/************************************************************************/
/*                            CPLLockFile()                             */
/************************************************************************/

void *CPLLockFile( const char *pszPath, double dfWaitInSeconds )

{
    CPLError( CE_Failure, CPLE_NotSupported, 
              "PThreads CPLLockFile() not implemented yet." );

    return NULL;
}

/************************************************************************/
/*                           CPLUnlockFile()                            */
/************************************************************************/

void CPLUnlockFile( void *hLock )

{
}

/************************************************************************/
/*                             CPLGetPID()                              */
/************************************************************************/

int CPLGetPID()

{
    return (int) pthread_self();
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

/************************************************************************/
/*                             CPLGetTLS()                              */
/************************************************************************/

static void **papTLSList = NULL;

void *CPLGetTLS( int nIndex )

{
    if( papTLSList == NULL )
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX);

    return papTLSList[nIndex];
}

/************************************************************************/
/*                             CPLSetTLS()                              */
/************************************************************************/

void CPLSetTLS( int nIndex, void *pData, int bFreeOnExit )

{
    if( papTLSList == NULL )
        papTLSList = (void **) CPLCalloc(sizeof(void*),CTLS_MAX);

    papTLSList[nIndex] = pData;
}

#endif /* def CPL_MULTIPROC_PTHREAD */


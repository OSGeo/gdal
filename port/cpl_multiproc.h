/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Multi-Threading, and process handling portability functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef CPL_MULTIPROC_H_INCLUDED_
#define CPL_MULTIPROC_H_INCLUDED_

#include "cpl_port.h"

/*
** There are three primary implementations of the multi-process support
** controlled by one of CPL_MULTIPROC_WIN32, CPL_MULTIPROC_PTHREAD or
** CPL_MULTIPROC_STUB being defined.  If none are defined, the stub
** implementation will be used.
*/

#if defined(WIN32) && !defined(CPL_MULTIPROC_STUB)
#  define CPL_MULTIPROC_WIN32
/* MinGW can have pthread support, so disable it to avoid issues */
/* in cpl_multiproc.cpp */
#  undef  CPL_MULTIPROC_PTHREAD
#endif

#if !defined(CPL_MULTIPROC_WIN32) && !defined(CPL_MULTIPROC_PTHREAD) \
 && !defined(CPL_MULTIPROC_STUB) && !defined(CPL_MULTIPROC_NONE)
#  define CPL_MULTIPROC_STUB
#endif

CPL_C_START

typedef void (*CPLThreadFunc)(void *);

void CPL_DLL *CPLLockFile( const char *pszPath, double dfWaitInSeconds );
void  CPL_DLL CPLUnlockFile( void *hLock );

#ifdef DEBUG
typedef struct _CPLMutex  CPLMutex;
typedef struct _CPLCond   CPLCond;
typedef struct _CPLJoinableThread CPLJoinableThread;
#else
#define CPLMutex void
#define CPLCond void
#define CPLJoinableThread void
#endif

/* Options for CPLCreateMutexEx() and CPLCreateOrAcquireMutexEx() */
#define CPL_MUTEX_RECURSIVE         0
#define CPL_MUTEX_ADAPTIVE          1
#define CPL_MUTEX_REGULAR           2

CPLMutex CPL_DLL *CPLCreateMutex( void ); /* returned acquired */
CPLMutex CPL_DLL *CPLCreateMutexEx( int nOptions ); /* returned acquired */
int   CPL_DLL CPLCreateOrAcquireMutex( CPLMutex **, double dfWaitInSeconds );
int   CPL_DLL CPLCreateOrAcquireMutexEx( CPLMutex **, double dfWaitInSeconds, int nOptions  );
int   CPL_DLL CPLAcquireMutex( CPLMutex *hMutex, double dfWaitInSeconds );
void  CPL_DLL CPLReleaseMutex( CPLMutex *hMutex );
void  CPL_DLL CPLDestroyMutex( CPLMutex *hMutex );
void  CPL_DLL CPLCleanupMasterMutex( void );

CPLCond  CPL_DLL *CPLCreateCond( void );
void  CPL_DLL  CPLCondWait( CPLCond *hCond, CPLMutex* hMutex );
typedef enum
{
    COND_TIMED_WAIT_COND,
    COND_TIMED_WAIT_TIME_OUT,
    COND_TIMED_WAIT_OTHER
} CPLCondTimedWaitReason;
CPLCondTimedWaitReason CPL_DLL CPLCondTimedWait( CPLCond *hCond, CPLMutex* hMutex, double dfWaitInSeconds );
void  CPL_DLL  CPLCondSignal( CPLCond *hCond );
void  CPL_DLL  CPLCondBroadcast( CPLCond *hCond );
void  CPL_DLL  CPLDestroyCond( CPLCond *hCond );

/** Contrary to what its name suggests, CPLGetPID() actually returns the thread id */
GIntBig CPL_DLL CPLGetPID( void );
int CPL_DLL CPLGetCurrentProcessID( void );
int   CPL_DLL CPLCreateThread( CPLThreadFunc pfnMain, void *pArg );
CPLJoinableThread  CPL_DLL* CPLCreateJoinableThread( CPLThreadFunc pfnMain, void *pArg );
void  CPL_DLL CPLJoinThread(CPLJoinableThread* hJoinableThread);
void  CPL_DLL CPLSleep( double dfWaitInSeconds );

const char CPL_DLL *CPLGetThreadingModel( void );

int CPL_DLL CPLGetNumCPUs( void );

typedef struct _CPLLock CPLLock;

/* Currently LOCK_ADAPTIVE_MUTEX is Linux-only and LOCK_SPIN only available */
/* on systems with pthread_spinlock API (so not MacOsX). If a requested type */
/* isn't available, it fallbacks to LOCK_RECURSIVE_MUTEX */
typedef enum
{
    LOCK_RECURSIVE_MUTEX,
    LOCK_ADAPTIVE_MUTEX,
    LOCK_SPIN
} CPLLockType;

CPLLock  CPL_DLL *CPLCreateLock( CPLLockType eType ); /* returned NON acquired */
int   CPL_DLL  CPLCreateOrAcquireLock( CPLLock**, CPLLockType eType );
int   CPL_DLL  CPLAcquireLock( CPLLock* );
void  CPL_DLL  CPLReleaseLock( CPLLock* );
void  CPL_DLL  CPLDestroyLock( CPLLock* );
void  CPL_DLL  CPLLockSetDebugPerf( CPLLock*, int bEnableIn ); /* only available on x86/x86_64 with GCC for now */

CPL_C_END

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

/* Instantiates the mutex if not already done. The parameter x should be a (void**). */
#define CPLMutexHolderD(x)  CPLMutexHolder oHolder(x,1000.0,__FILE__,__LINE__)

/* Instantiates the mutex with options if not already done. */
/* The parameter x should be a (void**). */
#define CPLMutexHolderExD(x, nOptions)  CPLMutexHolder oHolder(x,1000.0,__FILE__,__LINE__,nOptions)

/* This variant assumes the mutex has already been created. If not, it will */
/* be a no-op. The parameter x should be a (void*) */
#define CPLMutexHolderOptionalLockD(x)  CPLMutexHolder oHolder(x,1000.0,__FILE__,__LINE__)

/** Object to hold a mutex */
class CPL_DLL CPLMutexHolder
{
  private:
    CPLMutex   *hMutex = nullptr;
    // Only used for debugging.
    const char *pszFile = nullptr;
    int         nLine = 0;

    CPL_DISALLOW_COPY_ASSIGN(CPLMutexHolder)

  public:

    /** Instantiates the mutex if not already done. */
    explicit CPLMutexHolder( CPLMutex **phMutex, double dfWaitInSeconds = 1000.0,
                    const char *pszFile = __FILE__,
                    int nLine = __LINE__,
                    int nOptions = CPL_MUTEX_RECURSIVE);

    /** This variant assumes the mutex has already been created. If not, it will
     * be a no-op */
    explicit CPLMutexHolder( CPLMutex* hMutex, double dfWaitInSeconds = 1000.0,
                    const char *pszFile = __FILE__,
                    int nLine = __LINE__ );

    ~CPLMutexHolder();
};

/* Instantiates the lock if not already done. The parameter x should be a (CPLLock**). */
#define CPLLockHolderD(x, eType)  CPLLockHolder oHolder(x,eType,__FILE__,__LINE__);

/* This variant assumes the lock has already been created. If not, it will */
/* be a no-op. The parameter should be (CPLLock*) */
#define CPLLockHolderOptionalLockD(x)  CPLLockHolder oHolder(x,__FILE__,__LINE__);

/** Object to hold a lock */
class CPL_DLL CPLLockHolder
{
  private:
    CPLLock    *hLock = nullptr;
    const char *pszFile = nullptr;
    int         nLine = 0;

    CPL_DISALLOW_COPY_ASSIGN(CPLLockHolder)

  public:

    /** Instantiates the lock if not already done. */
    CPLLockHolder( CPLLock **phSpin, CPLLockType eType,
                    const char *pszFile = __FILE__,
                    int nLine = __LINE__);

    /** This variant assumes the lock has already been created. If not, it will
     * be a no-op */
    explicit CPLLockHolder( CPLLock* hSpin,
                    const char *pszFile = __FILE__,
                    int nLine = __LINE__ );

    ~CPLLockHolder();
};

#endif /* def __cplusplus */

/* -------------------------------------------------------------------- */
/*      Thread local storage.                                           */
/* -------------------------------------------------------------------- */

#define CTLS_RLBUFFERINFO                1         /* cpl_conv.cpp */
#define CTLS_WIN32_COND                  2         /* cpl_multiproc.cpp */
#define CTLS_CSVTABLEPTR                 3         /* cpl_csv.cpp */
#define CTLS_CSVDEFAULTFILENAME          4         /* cpl_csv.cpp */
#define CTLS_ERRORCONTEXT                5         /* cpl_error.cpp */
#define CTLS_VSICURL_CACHEDCONNECTION    6         /* cpl_vsil_curl.cpp */
#define CTLS_PATHBUF                     7         /* cpl_path.cpp */
#define CTLS_ABSTRACTARCHIVE_SPLIT       8         /* cpl_vsil_abstract_archive.cpp */
#define CTLS_GDALOPEN_ANTIRECURSION      9         /* gdaldataset.cpp */
#define CTLS_CPLSPRINTF                 10         /* cpl_string.h */
#define CTLS_RESPONSIBLEPID             11         /* gdaldataset.cpp */
#define CTLS_VERSIONINFO                12         /* gdal_misc.cpp */
#define CTLS_VERSIONINFO_LICENCE        13         /* gdal_misc.cpp */
#define CTLS_CONFIGOPTIONS              14         /* cpl_conv.cpp */
#define CTLS_FINDFILE                   15         /* cpl_findfile.cpp */
#define CTLS_VSIERRORCONTEXT            16         /* cpl_vsi_error.cpp */
#define CTLS_ERRORHANDLERACTIVEDATA     17         /* cpl_error.cpp */
#define CTLS_PROJCONTEXTHOLDER          18         /* ogr_proj_p.cpp */
#define CTLS_GDALDEFAULTOVR_ANTIREC     19         /* gdaldefaultoverviews.cpp */
#define CTLS_HTTPFETCHCALLBACK          20         /* cpl_http.cpp */

#define CTLS_MAX                        32

CPL_C_START
void CPL_DLL * CPLGetTLS( int nIndex );
void CPL_DLL * CPLGetTLSEx( int nIndex, int* pbMemoryErrorOccurred );
void CPL_DLL CPLSetTLS( int nIndex, void *pData, int bFreeOnExit );

/* Warning : the CPLTLSFreeFunc must not in any case directly or indirectly */
/* use or fetch any TLS data, or a terminating thread will hang ! */
typedef void (*CPLTLSFreeFunc)( void* pData );
void CPL_DLL CPLSetTLSWithFreeFunc( int nIndex, void *pData, CPLTLSFreeFunc pfnFree );
void CPL_DLL CPLSetTLSWithFreeFuncEx( int nIndex, void *pData, CPLTLSFreeFunc pfnFree, int* pbMemoryErrorOccurred );

void CPL_DLL CPLCleanupTLS( void );
CPL_C_END

#endif /* CPL_MULTIPROC_H_INCLUDED_ */

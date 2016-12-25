/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Mutex (thread lock) functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam
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
 *****************************************************************************/


/* projects.h and windows.h conflict - avoid this! */

#if defined(MUTEX_pthread) && !defined(_XOPEN_SOURCE)
/* For pthread_mutexattr_settype */
#define _XOPEN_SOURCE 500
#endif


#ifndef _WIN32
#include "proj_config.h"
#include <projects.h>
#else
#include <proj_api.h>
#endif

/* on win32 we always use win32 mutexes, even if pthreads are available */
#if defined(_WIN32) && !defined(MUTEX_stub)
#ifndef MUTEX_win32
#  define MUTEX_win32
#endif
#  undef  MUTEX_pthread
#endif

#if !defined(MUTEX_stub) && !defined(MUTEX_pthread) && !defined(MUTEX_win32)
#  define MUTEX_stub
#endif

/************************************************************************/
/* ==================================================================== */
/*                      stub mutex implementation                       */
/* ==================================================================== */
/************************************************************************/

#ifdef MUTEX_stub

/************************************************************************/
/*                            pj_acquire_lock()                         */
/*                                                                      */
/*      Acquire the PROJ.4 lock.                                        */
/************************************************************************/

void pj_acquire_lock()
{
}

/************************************************************************/
/*                            pj_release_lock()                         */
/*                                                                      */
/*      Release the PROJ.4 lock.                                        */
/************************************************************************/

void pj_release_lock()
{
}

/************************************************************************/
/*                          pj_cleanup_lock()                           */
/************************************************************************/
void pj_cleanup_lock()
{
}

#endif /* def MUTEX_stub */

/************************************************************************/
/* ==================================================================== */
/*                    pthread mutex implementation                      */
/* ==================================================================== */
/************************************************************************/

#ifdef MUTEX_pthread

#include "pthread.h"

static pthread_mutex_t pj_precreated_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pj_core_lock;
static int pj_core_lock_created = 0;

/************************************************************************/
/*                          pj_acquire_lock()                           */
/*                                                                      */
/*      Acquire the PROJ.4 lock.                                        */
/************************************************************************/

void pj_acquire_lock()
{
    if (!pj_core_lock_created) {
        /*
        ** We need to ensure the core mutex is created in recursive mode
        ** and there is no portable way of doing that using automatic
        ** initialization so we have pj_precreated_lock only for the purpose
        ** of protecting the creation of the core lock.
        */
        pthread_mutexattr_t mutex_attr;

        pthread_mutex_lock( &pj_precreated_lock);

        pthread_mutexattr_init(&mutex_attr);
#ifdef HAVE_PTHREAD_MUTEX_RECURSIVE
        pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
#else
        pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP);
#endif
        pthread_mutex_init(&pj_core_lock, &mutex_attr);
        pj_core_lock_created = 1;

        pthread_mutex_unlock( &pj_precreated_lock );
    }

    pthread_mutex_lock( &pj_core_lock);
}

/************************************************************************/
/*                          pj_release_lock()                           */
/*                                                                      */
/*      Release the PROJ.4 lock.                                        */
/************************************************************************/

void pj_release_lock()
{
    pthread_mutex_unlock( &pj_core_lock );
}

/************************************************************************/
/*                          pj_cleanup_lock()                           */
/************************************************************************/
void pj_cleanup_lock()
{
}

#endif /* def MUTEX_pthread */

/************************************************************************/
/* ==================================================================== */
/*                      win32 mutex implementation                      */
/* ==================================================================== */
/************************************************************************/

#ifdef MUTEX_win32

#include <windows.h>

static HANDLE mutex_lock = NULL;

/************************************************************************/
/*                            pj_init_lock()                            */
/************************************************************************/

static void pj_init_lock()

{
    if( mutex_lock == NULL )
        mutex_lock = CreateMutex( NULL, FALSE, NULL );
}

/************************************************************************/
/*                          pj_acquire_lock()                           */
/*                                                                      */
/*      Acquire the PROJ.4 lock.                                        */
/************************************************************************/

void pj_acquire_lock()
{
    if( mutex_lock == NULL )
        pj_init_lock();

    WaitForSingleObject( mutex_lock, INFINITE );
}

/************************************************************************/
/*                          pj_release_lock()                           */
/*                                                                      */
/*      Release the PROJ.4 lock.                                        */
/************************************************************************/

void pj_release_lock()
{
    if( mutex_lock == NULL )
        pj_init_lock();
    else
        ReleaseMutex( mutex_lock );
}

/************************************************************************/
/*                          pj_cleanup_lock()                           */
/************************************************************************/
void pj_cleanup_lock()
{
    if( mutex_lock != NULL )
    {
        CloseHandle( mutex_lock );
        mutex_lock = NULL;
    }
}

#endif /* def MUTEX_win32 */

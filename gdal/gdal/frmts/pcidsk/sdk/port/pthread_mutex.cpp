/******************************************************************************
 *
 * Purpose:  Implementation of pthreads based mutex.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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

#include "pcidsk_mutex.h"
#include <pthread.h>

#include <cstdlib>


/************************************************************************/
/*                             PThreadMutex                             */
/************************************************************************/

class PThreadMutex : public PCIDSK::Mutex

{
private:
    pthread_mutex_t *hMutex;

public:
    PThreadMutex();
    ~PThreadMutex();

    int Acquire(void);
    int Release(void);
};

/************************************************************************/
/*                            PThreadMutex()                            */
/************************************************************************/

PThreadMutex::PThreadMutex()

{
    hMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));

#if defined(PTHREAD_MUTEX_RECURSIVE)
    {
        pthread_mutexattr_t  attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init( hMutex, &attr );
    }
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    pthread_mutex_t tmp_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    *hMutex = tmp_mutex;
#else
#error "Recursive mutexes apparently unsupported, configure --without-threads" 
#endif
}

/************************************************************************/
/*                           ~PThreadMutex()                            */
/************************************************************************/

PThreadMutex::~PThreadMutex()

{
    pthread_mutex_destroy( hMutex );
    free( hMutex );
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

int PThreadMutex::Release()

{
    pthread_mutex_unlock( hMutex );
    return 1;
}

/************************************************************************/
/*                              Acquire()                               */
/************************************************************************/

int PThreadMutex::Acquire()

{
    return pthread_mutex_lock( hMutex ) == 0;
}

/************************************************************************/
/*                         DefaultCreateMutex()                         */
/************************************************************************/

/**
 * Create a mutex.
 *
 * This function creates the default style of mutex for the currently
 * PCIDSK library build.  The mutex should be destroyed with delete when
 * no longer required.
 *
 * @return a new mutex object pointer.
 */


PCIDSK::Mutex *PCIDSK::DefaultCreateMutex(void)

{
    return new PThreadMutex();
}

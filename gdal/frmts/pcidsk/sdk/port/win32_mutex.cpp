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
#include "pcidsk_exception.h"

#include <windows.h>


/************************************************************************/
/*                              Win32Mutex                              */
/************************************************************************/

class Win32Mutex : public PCIDSK::Mutex

{
private:
    HANDLE hMutex;

public:
    Win32Mutex();
    ~Win32Mutex();

    int Acquire(void);
    int Release(void);
};

/************************************************************************/
/*                             Win32Mutex()                             */
/************************************************************************/

Win32Mutex::Win32Mutex()

{
    hMutex = CreateMutex( NULL, 1, NULL );
    Release(); // it is created acquired, but we want it free.
}

/************************************************************************/
/*                           ~PThreadMutex()                            */
/************************************************************************/

Win32Mutex::~Win32Mutex()

{
    if( hMutex )
        CloseHandle( hMutex );
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

int Win32Mutex::Release()

{
    ReleaseMutex( hMutex );
    return 1;
}

/************************************************************************/
/*                              Acquire()                               */
/************************************************************************/

int Win32Mutex::Acquire()

{
    DWORD  hr;

    hr = WaitForSingleObject( hMutex, (int) (3600 * 1000) );

    if( hr != 0 )
        PCIDSK::ThrowPCIDSKException( "Failed to acquire mutex in 3600s." );

    return 1;
}

/************************************************************************/
/*                         DefaultCreateMutex()                         */
/************************************************************************/

PCIDSK::Mutex *PCIDSK::DefaultCreateMutex(void)

{
    return new Win32Mutex();
}

/******************************************************************************
 *
 * Purpose:  Implementation of pthreads based mutex.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "pcidsk_mutex.h"
#include "pcidsk_exception.h"

#include <windows.h>


/************************************************************************/
/*                              Win32Mutex                              */
/************************************************************************/

class Win32Mutex final : public PCIDSK::Mutex

{
private:
    HANDLE hMutex;

public:
    Win32Mutex();
    ~Win32Mutex();

    int Acquire(void) override;
    int Release(void) override;
};

/************************************************************************/
/*                             Win32Mutex()                             */
/************************************************************************/

Win32Mutex::Win32Mutex(): hMutex(CreateMutex( NULL, 1, NULL ))

{
    Win32Mutex::Release(); // it is created acquired, but we want it free.
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

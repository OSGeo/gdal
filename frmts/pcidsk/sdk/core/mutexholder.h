/******************************************************************************
 *
 * Purpose:  MutexHolder class. Helper class for controlling the acquisition
 *           and release of a Mutex based on current context.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_CORE_MUTEXHOLDER_H
#define INCLUDE_CORE_MUTEXHOLDER_H

#include "pcidsk_mutex.h"

namespace PCIDSK
{
    /************************************************************************/
    /*                             MutexHolder                              */
    /************************************************************************/
    class PCIDSK_DLL MutexHolder
    {
    public:
        MutexHolder( Mutex *mutexIn )
        {
            this->mutex = mutexIn;
            if( mutex != nullptr )
                mutex->Acquire();
        }
        ~MutexHolder()
        {
            if( mutex )
                mutex->Release();
        }

        void Release()
        {
            if (mutex)
            {
                mutex->Release();
                mutex = nullptr;
            }
        }

    private:
        Mutex     *mutex;

    };

} //end namespace PCIDSK

#endif // INCLUDE_CORE_MUTEXHOLDER_H

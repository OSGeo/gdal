/******************************************************************************
 *
 * Purpose:  MutexHolder class. Helper class for controlling the acquisition
 *           and release of a Mutex based on current context.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

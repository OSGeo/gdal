/******************************************************************************
 *
 * Purpose:  Interface. PCIDSK Mutex Class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_MUTEX_H
#define INCLUDE_PCIDSK_MUTEX_H

#include "pcidsk_config.h"

namespace PCIDSK
{
/************************************************************************/
/*                                Mutex                                 */
/************************************************************************/

    class Mutex
    {
    public:
        virtual ~Mutex() {}

        virtual int  Acquire() = 0;
        virtual int  Release() = 0;
    };

    Mutex PCIDSK_DLL *DefaultCreateMutex(void);
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_MUTEX_H

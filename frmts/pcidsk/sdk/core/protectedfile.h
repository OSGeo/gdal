/******************************************************************************
 *
 * Purpose:  Declaration of the Protected File structure
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_CORE_PROTECTEDFILE_H
#define INCLUDE_CORE_PROTECTEDFILE_H

namespace PCIDSK
{

    /************************************************************************/
    /*                            ProtectedFile                             */
    /************************************************************************/
    struct ProtectedFile
    {
        std::string     filename;
        bool            writable;
        void           *io_handle;
        Mutex          *io_mutex;
    };

    /************************************************************************/
    /*                           ProtectedEDBFile                           */
    /************************************************************************/
    struct ProtectedEDBFile
    {
        EDBFile        *file;
        std::string     filename;
        bool            writable;
        Mutex          *io_mutex;
    };

} // end namespace PCIDSK

#endif // INCLUDE_CORE_PROTECTEDFILE_H

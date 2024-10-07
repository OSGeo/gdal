/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  Header file for userfaultfd support.
 * Author:   James McClain <james.mcclain@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Dr. James McClain <james.mcclain@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifdef ENABLE_UFFD

#include "netcdf_mem.h"
#include "cpl_userfaultfd.h"

#ifndef NETCDF_UFFD_UNMAP

#define NETCDF_UFFD_UNMAP(context)                                             \
    {                                                                          \
        CPLDeleteUserFaultMapping(context);                                    \
        context = nullptr;                                                     \
    }

#endif  // NETCDF_UFFD_UNMAP

#endif

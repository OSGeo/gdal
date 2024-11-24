/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Header file for HDF4 compatibility
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HDF4COMPAT_H_INCLUDED_
#define HDF4COMPAT_H_INCLUDED_

/*  New versions of the HDF4 library define H4_xxx instead of xxx to
    avoid potential conflicts with NetCDF-3 library. The following
    tests enable compiling with older versions of HDF4
*/
#ifndef H4_MAX_VAR_DIMS
#define H4_MAX_VAR_DIMS MAX_VAR_DIMS
#endif
#ifndef H4_MAX_NC_DIMS
#define H4_MAX_NC_DIMS MAX_NC_DIMS
#endif
#ifndef H4_MAX_NC_NAME
#define H4_MAX_NC_NAME MAX_NC_NAME
#endif

#endif

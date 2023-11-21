/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  netCDF driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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

#ifndef NETCDFFORMATENUM_H
#define NETCDFFORMATENUM_H

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) ||     \
    defined(ALLOW_FORMAT_DUMPS)
// Whether to support opening a ncdump file as a file dataset
// Useful for fuzzing purposes
#define ENABLE_NCDUMP
#endif

/* netcdf file types, as in libcdi/cdo and compat w/netcdf.h */
typedef enum
{
    NCDF_FORMAT_NONE = 0, /* Not a netCDF file */
    NCDF_FORMAT_NC = 1,   /* netCDF classic format */
    NCDF_FORMAT_NC2 = 2,  /* netCDF version 2 (64-bit)  */
    NCDF_FORMAT_NC4 = 3,  /* netCDF version 4 */
    NCDF_FORMAT_NC4C = 4, /* netCDF version 4 (classic) */
    /* HDF files (HDF5 or HDF4) not supported because of lack of support */
    /* in libnetcdf installation or conflict with other drivers */
    NCDF_FORMAT_HDF5 = 5,    /* HDF5 file, not supported */
    NCDF_FORMAT_HDF4 = 6,    /* HDF4 file, not supported */
    NCDF_FORMAT_UNKNOWN = 10 /* Format not determined (yet) */
} NetCDFFormatEnum;

#endif

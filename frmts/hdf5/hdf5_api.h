/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Import HDF5 public API
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HDF5_API_H
#define HDF5_API_H

#ifndef H5_USE_16_API
#define H5_USE_16_API
#endif

#ifdef _MSC_VER
#pragma warning(push)
// Warning C4005: '_HDF5USEDLL_' : macro redefinition.
#pragma warning(disable : 4005)
#pragma warning(                                                               \
    disable : 4268) /* 'H5O_TOKEN_UNDEF_g': 'const' static/global data         \
                       initialized with compiler generated default constructor \
                       fills the object with zeros */
#endif

#include "hdf5.h"

#if defined(H5T_NATIVE_FLOAT16) && defined(H5_HAVE__FLOAT16)
#define HDF5_HAVE_FLOAT16
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  // HDF5_API_H

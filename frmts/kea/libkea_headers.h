/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes libkea headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef GDAL_LIBKEA_HEADERS_H
#define GDAL_LIBKEA_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#if defined(USE_GCC_VISIBILITY_FLAG) && !defined(DllExport)
#define DllExport CPL_DLL
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(                                                               \
    disable : 4290) /* C++ exception specification ignored except to indicate  \
                       a function is not __declspec(nothrow)*/
#pragma warning(                                                               \
    disable : 4268) /* 'H5O_TOKEN_UNDEF_g': 'const' static/global data         \
                       initialized with compiler generated default constructor \
                       fills the object with zeros */
#endif

#include "libkea/KEACommon.h"
#include "libkea/KEAImageIO.h"
#include "libkea/KEAAttributeTable.h"
#include "libkea/KEAAttributeTableInMem.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* GDAL_LIBKEA_HEADERS_H */

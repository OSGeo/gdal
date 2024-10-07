/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes msodbcsql.h
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef INCLUDE_MSODBCSQL_H_DEFINED
#define INCLUDE_MSODBCSQL_H_DEFINED

#if defined(__GNUC__)
#pragma GCC system_header

// msodbcsql.h uses Microsoft SAL annotations which aren't set by mingw64
#ifndef __in
#define __in
#define __in_z
#define __out
#define __out_ecount_z_opt(x)
#endif

#endif

#include <msodbcsql.h>

#endif

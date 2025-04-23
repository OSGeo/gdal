/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes zlib header
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 201, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef CPL_ZLIB_HEADER_H
#define CPL_ZLIB_HEADER_H

#if defined(__GNUC__) && !defined(_MSC_VER)
#pragma GCC system_header
#endif

#include "zlib.h"

#ifdef ENABLE_DEFLATE64
#include "infback9.h"
#endif

#endif

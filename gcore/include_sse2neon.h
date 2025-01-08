/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes sse2neon.h headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef INCLUDE_SSE2NEON_H
#define INCLUDE_SSE2NEON_H

#if defined(__GNUC__)
#pragma GCC system_header
#endif

// This check is done in sse2neon.h just as a warning. Turn that into an
// error, so that gdal.cmake doesn't try to use it
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 10
#error "sse2neon.h: GCC versions earlier than 10 are not supported."
#endif

#include "sse2neon.h"

#ifndef _MM_SHUFFLE2
#define _MM_SHUFFLE2(fp1, fp0) (((fp1) << 1) | (fp0))
#endif

#endif /* INCLUDE_SSE2NEON_H */

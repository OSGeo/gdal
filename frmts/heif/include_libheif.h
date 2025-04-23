/******************************************************************************
 *
 * Project:  HEIF Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef INCLUDE_LIBHEIF_DEFINED
#define INCLUDE_LIBHEIF_DEFINED

#include "libheif/heif.h"

#define BUILD_LIBHEIF_VERSION(x, y, z)                                         \
    (((x) << 24) | ((y) << 16) | ((z) << 8) | 0)

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 3, 0)
#define HAS_CUSTOM_FILE_READER
#endif

#if LIBHEIF_HAVE_VERSION(1, 1, 0)
#define HAS_CUSTOM_FILE_WRITER
#endif

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 19, 0)
#include "libheif/heif_properties.h"
#endif

#include <iostream>

#endif

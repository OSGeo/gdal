/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes JP2KAK SDK headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef JPIPKAK_HEADERS_H
#define JPIPKAK_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4458)
#endif

#include "kdu_cache.h"
#include "kdu_region_decompressor.h"
#include "kdu_file_io.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* JPIPKAK_HEADERS_H */

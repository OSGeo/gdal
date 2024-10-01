/******************************************************************************
 *
 * Project:  JPEG-XL Driver
 * Purpose:  Implement GDAL JPEG-XL Support based on libjxl
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#if defined(__GNUC__)
#pragma GCC system_header
#endif

#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/decode_cxx.h>
#include <jxl/encode_cxx.h>
#ifdef HAVE_JXL_THREADS
#include <jxl/resizable_parallel_runner.h>
#include <jxl/resizable_parallel_runner_cxx.h>
#endif

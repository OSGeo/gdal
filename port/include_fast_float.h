#ifndef INCLUDE_FAST_FLOAT_H
#define INCLUDE_FAST_FLOAT_H

#if defined(__GNUC__)
#pragma GCC system_header
#endif

// Detect if there's a system fast_float library available
#if defined(__has_include) && !defined(USE_SYSTEM_FAST_FLOAT)
#if __has_include("fast_float/fast_float.h")
#define USE_SYSTEM_FAST_FLOAT 1
#endif
#endif

#ifdef __clang__
#pragma clang attribute push(                                                  \
    __attribute__((no_sanitize("unsigned-integer-overflow"))),                 \
    apply_to = function)
#endif

#if USE_SYSTEM_FAST_FLOAT

#include "fast_float/fast_float.h"

#else

// GDAL wrapper around fast_float to namespace it under gdal_fast_float
// if using vendored version

#define fast_float gdal_fast_float
#include "../third_party/fast_float/fast_float.h"
#endif

#ifdef __clang__
#pragma clang attribute pop
#endif

#endif  // INCLUDE_FAST_FLOAT_H

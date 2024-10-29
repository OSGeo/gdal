/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes PoDoFo headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef PDFSDK_HEADERS_PODOFO_H
#define PDFSDK_HEADERS_PODOFO_H

#if defined(__GNUC__) && !defined(_MSC_VER)
#pragma GCC system_header
#endif

#ifdef HAVE_PODOFO

#ifdef _WIN32
/*
 * Some Windows header defines a GetObject macro that
 * shadows a GetObject() method in PoDoFo. As pdfdataset.cpp includes cpl_spawn.h
 * which includes windows.h, so let's bite the bullet and important windows.h
 * right now, and then undef GetObject. Undef'ing GetObject is done in some
 * source files of PoDoFo itself.
 */
#include <windows.h>
#ifdef GetObject
#undef GetObject
#endif
#endif

// Related fix submitted per https://github.com/podofo/podofo/pull/98
#ifdef HAVE_PODOFO_0_10_OR_LATER
#define USE_HACK_BECAUSE_PdfInputStream_constructor_is_not_exported_in_podofo_0_11
#endif

#ifdef USE_HACK_BECAUSE_PdfInputStream_constructor_is_not_exported_in_podofo_0_11
// If we <sstream> is included after our below #define private public errors out
// with an error like:
// /usr/include/c++/13.2.1/sstream:457:7: error: 'struct std::__cxx11::basic_stringbuf<_CharT, _Traits, _Alloc>::__xfer_bufptrs' redeclared with different access
//  457 |       struct __xfer_bufptrs
// so include it before, as otherwise it would get indirectly included by
// PdfDate.h, which includes <chrono>, which includes <sstream>
#include <sstream>
// Ugly! PfdObjectStream::GetParent() is private but we need it...
#define private public
#endif
#include "podofo.h"
#ifdef private
#undef private
#endif

#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
#define PdfVecObjects PdfIndirectObjectList
#endif

#endif  // HAVE_PODOFO

#endif  // PDFSDK_HEADERS_PODOFO_H

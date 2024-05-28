/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes PDF SDK headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#ifndef PDFSDK_HEADERS_H
#define PDFSDK_HEADERS_H

/* We avoid to include cpl_port.h directly or indirectly */
#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#pragma GCC system_header
#endif

#ifdef HAVE_POPPLER

/* Horrible hack because there's a conflict between struct FlateDecode of */
/* include/poppler/Stream.h and the FlateDecode() function of */
/* pdfium/core/include/fpdfapi/fpdf_parser.h. */
/* The part of Stream.h where struct FlateDecode is defined isn't needed */
/* by GDAL, and is luckily protected by a #ifndef ENABLE_ZLIB section */
#ifdef HAVE_PDFIUM
#define ENABLE_ZLIB
#endif /* HAVE_PDFIUM */

#ifdef _MSC_VER
#pragma warning(push)
// conversion from 'const int' to 'Guchar', possible loss of data
#pragma warning(disable : 4244)
// conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4267)
#endif

/* begin of poppler xpdf includes */
#include <Object.h>
#include <Stream.h>

#define private public /* Ugly! Page::pageObj is private but we need it... */
#include <Page.h>
#undef private

#include <Dict.h>

#define private                                                                \
    public /* Ugly! Catalog::optContent is private but we need it... */
#include <Catalog.h>
#undef private

#define private public /* Ugly! PDFDoc::str is private but we need it... */
#include <PDFDoc.h>
#undef private

#include <splash/SplashBitmap.h>
#include <splash/Splash.h>
#include <SplashOutputDev.h>
#include <GlobalParams.h>
#include <ErrorCodes.h>

/* end of poppler xpdf includes */

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* HAVE_POPPLER */

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

#ifdef HAVE_PDFIUM
#include "cpl_multiproc.h"

#if (!defined(CPL_MULTIPROC_WIN32) && !defined(CPL_MULTIPROC_PTHREAD)) ||      \
    defined(CPL_MULTIPROC_STUB) || defined(CPL_MULTIPROC_NONE)
#error PDF driver compiled with PDFium library requires working threads with mutex locking!
#endif

// Linux ignores timeout, Windows returns if not INFINITE
#ifdef _WIN32
#define PDFIUM_MUTEX_TIMEOUT INFINITE
#else
#define PDFIUM_MUTEX_TIMEOUT 0.0f
#endif

#ifdef _MSC_VER
#pragma warning(push)
// include/pdfium\core/fxcrt/fx_memcpy_wrappers.h(48,30): warning C4244: 'argument': conversion from 'int' to 'wchar_t', possible loss of data
#pragma warning(disable : 4244)
#endif

#include <cstring>
#include "public/fpdfview.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_occontext.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/render/cpdf_pagerendercontext.h"
#include "core/fpdfapi/render/cpdf_progressiverenderer.h"
#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpdf_annotlist.h"
#include "core/fxcrt/bytestring.h"
#include "core/fxge/cfx_defaultrenderdevice.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/agg/fx_agg_driver.h"
#include "core/fxge/renderdevicedriver_iface.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "fpdfsdk/cpdfsdk_pauseadapter.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  // HAVE_PDFIUM

#endif

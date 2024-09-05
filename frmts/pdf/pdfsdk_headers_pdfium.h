/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes PDFium headers
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

#ifndef PDFSDK_HEADERS_PDFIUM_H
#define PDFSDK_HEADERS_PDFIUM_H

#if defined(__GNUC__) && !defined(_MSC_VER)
#pragma GCC system_header
#endif

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
#include "core/fxge/agg/cfx_agg_devicedriver.h"
#include "core/fxge/agg/cfx_agg_imagerenderer.h"
#include "core/fxge/renderdevicedriver_iface.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "fpdfsdk/cpdfsdk_pauseadapter.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  // HAVE_PDFIUM

#endif  // PDFSDK_HEADERS_PDFIUM_H

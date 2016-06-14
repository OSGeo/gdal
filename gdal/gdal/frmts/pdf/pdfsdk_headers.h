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
#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) && !defined(_MSC_VER))
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
#pragma warning( push )
#pragma warning( disable : 4244 ) /* conversion from 'const int' to 'Guchar', possible loss of data */
#endif

/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED
#define OGR_FEATURESTYLE_INCLUDE

#include <goo/gtypes.h>
#include <goo/GooList.h>

/* begin of poppler xpdf includes */
#include <poppler/Object.h>
#include <poppler/Stream.h>

#define private public /* Ugly! Page::pageObj is private but we need it... */
#include <poppler/Page.h>
#undef private

#include <poppler/Dict.h>

#define private public /* Ugly! Catalog::optContent is private but we need it... */
#include <poppler/Catalog.h>
#undef private

#define private public  /* Ugly! PDFDoc::str is private but we need it... */
#include <poppler/PDFDoc.h>
#undef private

#include <poppler/splash/SplashBitmap.h>
#include <poppler/splash/Splash.h>
#include <poppler/SplashOutputDev.h>
#include <poppler/GlobalParams.h>
#include <poppler/ErrorCodes.h>

/* end of poppler xpdf includes */

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif /* HAVE_POPPLER */

#ifdef HAVE_PODOFO

/*
 * Some Windows header defines a GetObject macro that
 * shadows a GetObject() method in PoDoFo. This
 * workaround is documented in the PoDoFo source.
 */
#ifdef GetObject
#undef GetObject
#endif

#include "podofo.h"
#endif // HAVE_PODOFO

#ifdef HAVE_PDFIUM
#include "cpl_multiproc.h"

#if (!defined(CPL_MULTIPROC_WIN32) && !defined(CPL_MULTIPROC_PTHREAD)) || defined(CPL_MULTIPROC_STUB) || defined(CPL_MULTIPROC_NONE)
#error PDF driver compiled with PDFium library requires working threads with mutex locking!
#endif

// Linux ignores timeout, Windows returns if not INFINITE
#ifdef WIN32
#define  PDFIUM_MUTEX_TIMEOUT     INFINITE
#else
#define  PDFIUM_MUTEX_TIMEOUT     0.0f
#endif

#include <cstring>
//#include <fpdfsdk/include/fsdk_define.h>
#include <fpdfview.h>
#include <core/include/fpdfapi/fpdf_page.h>
#include <core/include/fpdfapi/fpdf_objects.h>
#include "fpdfsdk/include/fsdk_rendercontext.h"
#endif // HAVE_PDFIUM

#endif

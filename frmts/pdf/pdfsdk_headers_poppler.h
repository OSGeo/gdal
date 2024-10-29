/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes Poppler headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef PDFSDK_HEADERS_POPPLER_H
#define PDFSDK_HEADERS_POPPLER_H

#if defined(__GNUC__) && !defined(_MSC_VER)
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

#endif  // PDFSDK_HEADERS_POPPLER_H

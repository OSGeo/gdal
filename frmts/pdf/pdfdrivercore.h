/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  PDF driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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
 ****************************************************************************/

#ifndef PDFDRIVERCORE_H
#define PDFDRIVERCORE_H

#include "gdal_priv.h"

#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO) || defined(HAVE_PDFIUM)
#define HAVE_PDF_READ_SUPPORT

#if defined(HAVE_PDFIUM) && defined(HAVE_POPPLER)
#define HAVE_MULTIPLE_PDF_BACKENDS
#elif defined(HAVE_PDFIUM) && defined(HAVE_PODOFO)
#define HAVE_MULTIPLE_PDF_BACKENDS
#elif defined(HAVE_POPPLER) && defined(HAVE_PODOFO)
#define HAVE_MULTIPLE_PDF_BACKENDS
#endif

#endif

constexpr const char *DRIVER_NAME = "PDF";

#define PDFGetOpenOptionList PLUGIN_SYMBOL_NAME(PDFGetOpenOptionList)
#define PDFDatasetIdentify PLUGIN_SYMBOL_NAME(PDFDatasetIdentify)
#define PDFDriverSetCommonMetadata                                             \
    PLUGIN_SYMBOL_NAME(PDFDriverSetCommonMetadata)

const char *PDFGetOpenOptionList();

int PDFDatasetIdentify(GDALOpenInfo *poOpenInfo);

void PDFDriverSetCommonMetadata(GDALDriver *poDriver);

#endif

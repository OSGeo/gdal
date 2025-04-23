/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  PDF driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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

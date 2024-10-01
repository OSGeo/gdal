// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char szCompositionXSD[] = {
#embed "data/pdfcomposition.xsd"
    , 0};

const char *PDFGetCompositionXSD()
{
    return szCompositionXSD;
}

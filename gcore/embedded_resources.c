// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char szLICENSE[] = {
#embed "../LICENSE.TXT"
    , 0};

const char *GDALGetEmbeddedLicense()
{
    return szLICENSE;
}

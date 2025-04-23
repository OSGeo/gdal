// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char szHEADER[] = {
#embed "data/header.dxf"
    , 0};

const char *OGRDXFGetHEADER()
{
    return szHEADER;
}

static const char szTRAILER[] = {
#embed "data/trailer.dxf"
    , 0};

const char *OGRDXFGetTRAILER()
{
    return szTRAILER;
}

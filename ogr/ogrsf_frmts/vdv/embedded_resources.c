// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char vdv452_xml[] = {
#embed "data/vdv452.xml"
    , 0};

const char *VDVGet452XML(void)
{
    return vdv452_xml;
}

// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char szPDS4Template[] = {
#embed "data/pds4_template.xml"
    , 0};

const char *PDS4GetEmbeddedTemplate()
{
    return szPDS4Template;
}

static const char szVICARJson[] = {
#embed "data/vicar.json"
    , 0};

const char *VICARGetEmbeddedConf(void)
{
    return szVICARJson;
}

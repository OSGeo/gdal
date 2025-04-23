// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char szBAGTemplate[] = {
#embed "data/bag_template.xml"
    , 0};

const char *BAGGetEmbeddedTemplateFile()
{
    return szBAGTemplate;
}

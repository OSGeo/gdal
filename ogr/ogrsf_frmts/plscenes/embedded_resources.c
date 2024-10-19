// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char plscenesconf_json[] = {
#embed "data/plscenesconf.json"
    , 0};

const char *PLScenesGetConfJson(void)
{
    return plscenesconf_json;
}

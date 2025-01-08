// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char osmconf_ini[] = {
#embed "data/osmconf.ini"
    , 0};

const char *OSMGetOSMConfIni(void)
{
    return osmconf_ini;
}

// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

#include "embedded_resources_gen1.c"

const char *GRIBGetCSVFileContent(const char* pszFilename)
{
    static const struct
    {
        const char* pszFilename;
        const char* pszContent;
    } asResources[] = {
#include "embedded_resources_gen2.c"
    };
    for( size_t i = 0; i < sizeof(asResources) / sizeof(asResources[0]); ++i )
    {
        if( strcmp(pszFilename, asResources[i].pszFilename) == 0 )
            return asResources[i].pszContent;
    }
    return NULL;
}

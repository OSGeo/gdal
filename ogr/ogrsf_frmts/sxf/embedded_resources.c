// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const unsigned char sxf_default_rsc[] = {
#embed "data/default.rsc"
};

const unsigned char *SXFGetDefaultRSC(int *pnSize)
{
    *pnSize = (int)sizeof(sxf_default_rsc);
    return sxf_default_rsc;
}

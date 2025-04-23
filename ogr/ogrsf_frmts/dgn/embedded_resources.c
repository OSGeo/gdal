// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const unsigned char abySeed2D[] = {
#embed "data/seed_2d.dgn"
};

const unsigned char *DGNGetSeed2D(unsigned int *pnSize)
{
    *pnSize = sizeof(abySeed2D);
    return abySeed2D;
}

static const unsigned char abySeed3D[] = {
#embed "data/seed_3d.dgn"
};

const unsigned char *DGNGetSeed3D(unsigned int *pnSize)
{
    *pnSize = sizeof(abySeed3D);
    return abySeed3D;
}

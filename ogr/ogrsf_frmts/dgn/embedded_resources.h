// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#ifndef DGN_EMBEDDED_RESOURCES_H
#define DGN_EMBEDDED_RESOURCES_H

#include "cpl_port.h"

CPL_C_START

const unsigned char *DGNGetSeed2D(unsigned int *pnSize);
const unsigned char *DGNGetSeed3D(unsigned int *pnSize);

CPL_C_END

#endif

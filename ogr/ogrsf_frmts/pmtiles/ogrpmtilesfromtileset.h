/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRPMTILESFROMTILESET_H_INCLUDED
#define OGRPMTILESFROMTILESET_H_INCLUDED

#include "cpl_port.h"

class GDALDataset;

bool OGRPMTilesConvertFromTileset(const char *pszDestName,
                                  const char *pszSrcDirectory,
                                  GDALDataset *poSrcDS,
                                  CSLConstList papszOptions);

#endif /* OGRPMTILESFROMTILESET_H_INCLUDED */

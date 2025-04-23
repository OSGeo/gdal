/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRPMTILESFROMMBTILES_H_INCLUDED
#define OGRPMTILESFROMMBTILES_H_INCLUDED

#include "gdal_priv.h"

bool OGRPMTilesConvertFromMBTiles(const char *pszDestName,
                                  const char *pszSrcName);

#endif /* OGRPMTILESFROMMBTILES_H_INCLUDED */

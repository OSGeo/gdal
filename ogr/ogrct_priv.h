/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Header for methods of OGRCoordinateTransformation only for GDAL
 *           internal use.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRCT_PRIV_H_INCLUDED
#define OGRCT_PRIV_H_INCLUDED

#include "ogr_spatialref.h"

void OGRProjCTDifferentOperationsStart(OGRCoordinateTransformation *poCT);

void OGRProjCTDifferentOperationsStop(OGRCoordinateTransformation *poCT);

bool OGRProjCTDifferentOperationsUsed(OGRCoordinateTransformation *poCT);

#endif  // OGRCT_PRIV_H_INCLUDED

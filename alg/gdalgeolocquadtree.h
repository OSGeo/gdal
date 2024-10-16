/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements Geolocation array based transformer, using a quadtree
 *           for inverse
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALGEOLOCQUADTREE_H
#define GDALGEOLOCQUADTREE_H

#include "gdal_alg_priv.h"

bool GDALGeoLocBuildQuadTree(GDALGeoLocTransformInfo *psTransform);

void GDALGeoLocInverseTransformQuadtree(
    const GDALGeoLocTransformInfo *psTransform, int nPointCount, double *padfX,
    double *padfY, int *panSuccess);

#endif

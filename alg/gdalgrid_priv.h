/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Gridding API.
 * Purpose:  Prototypes, and definitions for of GDAL scattered data gridder.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALGRID_PRIV_H
#define GDALGRID_PRIV_H

#include "cpl_error.h"
#include "cpl_quad_tree.h"

#include "gdal_alg.h"

//! @cond Doxygen_Suppress

typedef struct
{
    const double *padfX;
    const double *padfY;
} GDALGridXYArrays;

typedef struct
{
    GDALGridXYArrays *psXYArrays;
    int i;
} GDALGridPoint;

typedef struct
{
    CPLQuadTree *hQuadTree;
    double dfInitialSearchRadius;
    float *pafX;  // Aligned to be usable with AVX
    float *pafY;
    float *pafZ;
    GDALTriangulation *psTriangulation;
    int nInitialFacetIdx;
    /*! Weighting power divided by 2 (pre-computation). */
    double dfPowerDiv2PreComp;
    /*! The radius of search circle squared (pre-computation). */
    double dfRadiusPower2PreComp;
} GDALGridExtraParameters;

#ifdef HAVE_SSE_AT_COMPILE_TIME
CPLErr GDALGridInverseDistanceToAPower2NoSmoothingNoSearchSSE(
    const void *poOptions, GUInt32 nPoints, const double *unused_padfX,
    const double *unused_padfY, const double *unused_padfZ, double dfXPoint,
    double dfYPoint, double *pdfValue, void *hExtraParamsIn);
#endif

#ifdef HAVE_AVX_AT_COMPILE_TIME
CPLErr GDALGridInverseDistanceToAPower2NoSmoothingNoSearchAVX(
    const void *poOptions, GUInt32 nPoints, const double *unused_padfX,
    const double *unused_padfY, const double *unused_padfZ, double dfXPoint,
    double dfYPoint, double *pdfValue, void *hExtraParamsIn);
#endif

//! @endcond

#endif  // GDALGRID_PRIV_H

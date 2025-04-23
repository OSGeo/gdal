/******************************************************************************
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Definition of geo-computation functions
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GEO_UTILS_H_INCLUDED
#define OGR_GEO_UTILS_H_INCLUDED

#include "cpl_port.h"

/** The following functions provide computations based on great-circle/
 * orthodromic path, on a sphere with a radius of ~6366707 m.
 * The computations are not necessarily implemented in a very accurate/
 * stable way, and shouldn't be used for points that are too close (less than
 * one meter).
 * They are good enough for example to compute the coordinates of the polygon
 * for an airport runway, from its extreme points, track and length.
 */

// Such that OGR_GREATCIRCLE_DEFAULT_RADIUS * M_PI * 2 == 360 * 60.0 * 1852.0
// that is that one degree == 60 nautical miles
constexpr double OGR_GREATCIRCLE_DEFAULT_RADIUS = 6366707.01949370746;

double OGR_GreatCircle_Distance(double dfLatA_deg, double dfLonA_deg,
                                double dfLatB_deg, double dfLonB_deg,
                                double dfRadius);

double OGR_GreatCircle_InitialHeading(double dfLatA_deg, double dfLonA_deg,
                                      double dfLatB_deg, double dfLonB_deg);

/* such as ExtendPosition(A, Distance(A,B), InitialHeading(A,B)) ~= B */
int CPL_DLL OGR_GreatCircle_ExtendPosition(double dfLatA_deg, double dfLonA_deg,
                                           double dfDistance,
                                           double dfHeadingInA, double dfRadius,
                                           double *pdfLatB_deg,
                                           double *pdfLonB_deg);

#endif /* ndef OGR_GEO_UTILS_H_INCLUDED */

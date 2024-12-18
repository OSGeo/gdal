/**********************************************************************
 *
 * Name:     mitab_geometry.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Geometry manipulation functions.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *           Based on functions from mapprimitive.c/mapsearch.c in the source
 *           of UMN MapServer by Stephen Lime (http://mapserver.gis.umn.edu/)
 **********************************************************************
 * Copyright (c) 1999, 2000, Daniel Morissette
 *
 * SPDX-License-Identifier: MIT
 **********************************************************************/

#ifndef MITAB_GEOMETRY_H_INCLUDED
#define MITAB_GEOMETRY_H_INCLUDED

#include "ogr_geometry.h"

GBool OGRPointInRing(OGRPoint *poPoint, OGRLineString *poRing);
GBool OGRIntersectPointPolygon(OGRPoint *poPoint, OGRPolygon *poPoly);
int OGRPolygonLabelPoint(OGRPolygon *poPoly, OGRPoint *poLabelPoint);
int OGRPolylineCenterPoint(OGRLineString *poLine, OGRPoint *poLabelPoint);
int OGRPolylineLabelPoint(OGRLineString *poLine, OGRPoint *poLabelPoint);

#endif /* ndef MITAB_GEOMETRY_H_INCLUDED */

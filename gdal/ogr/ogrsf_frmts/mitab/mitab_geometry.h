/**********************************************************************
 * $Id: mitab_geometry.h,v 1.2 2004-06-30 20:29:04 dmorissette Exp $
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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: mitab_geometry.h,v $
 * Revision 1.2  2004-06-30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.1  2000/09/19 17:19:40  daniel
 * Initial Revision
 *
 **********************************************************************/

#ifndef _MITAB_GEOMETRY_H_INCLUDED
#define _MITAB_GEOMETRY_H_INCLUDED

#include "ogr_geometry.h"

GBool OGRPointInRing(OGRPoint *poPoint, OGRLineString *poRing);
GBool OGRIntersectPointPolygon(OGRPoint *poPoint, OGRPolygon *poPoly);
int   OGRPolygonLabelPoint(OGRPolygon *poPoly, OGRPoint *poLabelPoint);
int   OGRPolylineCenterPoint(OGRLineString *poLine, OGRPoint *poLabelPoint);
int   OGRPolylineLabelPoint(OGRLineString *poLine, OGRPoint *poLabelPoint);

#endif /* ndef _MITAB_GEOMETRY_H_INCLUDED */

/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements decoder of shapebin geometry for PGeo
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef OGR_PGEOGEOMETRY_H_INCLUDED
#define OGR_PGEOGEOMETRY_H_INCLUDED

#include "ogr_geometry.h"

#define SHPT_NULL          0

#ifndef SHPT_POINT
#define SHPT_POINT         1
#define SHPT_POINTM       21
#define SHPT_POINTZM      11
#define SHPT_POINTZ        9

#define SHPT_MULTIPOINT    8
#define SHPT_MULTIPOINTM  28
#define SHPT_MULTIPOINTZM 18
#define SHPT_MULTIPOINTZ  20

#define SHPT_ARC           3
#define SHPT_ARCM         23
#define SHPT_ARCZM        13
#define SHPT_ARCZ         10

#define SHPT_POLYGON       5
#define SHPT_POLYGONM     25
#define SHPT_POLYGONZM    15
#define SHPT_POLYGONZ     19

#define SHPT_MULTIPATCHM  31
#define SHPT_MULTIPATCH   32
#endif // SHPT_POINT

#define SHPT_GENERALPOLYLINE    50
#define SHPT_GENERALPOLYGON     51
#define SHPT_GENERALPOINT       52
#define SHPT_GENERALMULTIPOINT  53
#define SHPT_GENERALMULTIPATCH  54

/* The following are layers geometry type */
/* They are different from the above shape types */
#define ESRI_LAYERGEOMTYPE_NULL          0
#define ESRI_LAYERGEOMTYPE_POINT         1
#define ESRI_LAYERGEOMTYPE_MULTIPOINT    2
#define ESRI_LAYERGEOMTYPE_POLYLINE      3
#define ESRI_LAYERGEOMTYPE_POLYGON       4
#define ESRI_LAYERGEOMTYPE_MULTIPATCH    9

OGRGeometry* OGRCreateFromMultiPatch( int nParts,
                                      const GInt32* panPartStart,
                                      const GInt32* panPartType,
                                      int nPoints,
                                      const double* padfX,
                                      const double* padfY,
                                      const double* padfZ );

OGRErr CPL_DLL OGRCreateFromShapeBin( GByte *pabyShape,
                              OGRGeometry **ppoGeom,
                              int nBytes );

OGRErr CPL_DLL OGRWriteToShapeBin( const OGRGeometry *poGeom,
                           GByte **ppabyShape,
                           int *pnBytes );

OGRErr OGRCreateMultiPatch( const OGRGeometry *poGeom,
                            int bAllowSHPTTriangle,
                            int& nParts,
                            int*& panPartStart,
                            int*& panPartType,
                            int& nPoints,
                            OGRRawPoint*& poPoints,
                            double*& padfZ );

OGRErr CPL_DLL OGRWriteMultiPatchToShapeBin( const OGRGeometry *poGeom,
                           GByte **ppabyShape,
                           int *pnBytes );

#endif

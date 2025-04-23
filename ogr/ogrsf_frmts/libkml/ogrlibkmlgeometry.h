/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef OGR_LIBKML_GEOMETRY_H
#define OGR_LIBKML_GEOMETRY_H

#include "libkml_headers.h"

/*******************************************************************************
 Function to write out a ogr geometry to km.

args:
            poOgrGeom     the ogr geometry
            extra         used in recursion, just pass -1
            poKmlFactory  pointer to the libkml dom factory

returns:
            ElementPtr to the geometry created

*******************************************************************************/

kmldom::ElementPtr geom2kml(OGRGeometry *poOgrGeom, int extra,
                            kmldom::KmlFactory *poKmlFactory);

/******************************************************************************
 Function to read a kml geometry and translate to ogr.

Args:
            poKmlGeometry   pointer to the kml geometry to translate
            poOgrSRS        pointer to the spatial ref to set on the geometry

Returns:
            pointer to the new ogr geometry object

******************************************************************************/

OGRGeometry *kml2geom(kmldom::GeometryPtr poKmlGeometry,
                      OGRSpatialReference *poOgrSRS);

OGRGeometry *kml2geom_latlonbox(kmldom::LatLonBoxPtr poKmlLatLonBox,
                                OGRSpatialReference *poOgrSRS);

OGRGeometry *kml2geom_latlonquad(kmldom::GxLatLonQuadPtr poKmlLatLonQuad,
                                 OGRSpatialReference *poOgrSRS);

#endif  // OGR_LIBKML_GEOMETRY_H

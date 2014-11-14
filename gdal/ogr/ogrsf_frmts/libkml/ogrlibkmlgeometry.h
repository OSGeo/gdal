/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
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
 *****************************************************************************/

#include <kml/dom.h>

using kmldom::ElementPtr;
using kmldom::KmlFactory;
using kmldom::GeometryPtr;
using kmldom::LatLonBoxPtr;
using kmldom::GxLatLonQuadPtr;

/*******************************************************************************
	funtion to write out a ogr geometry to kml
	
args:
						poOgrGeom		the ogr geometry
						extra		used in recursion, just pass -1
						poKmlFactory	pointer to the libkml dom factory

returns:
						ElementPtr to the geometry created

*******************************************************************************/

ElementPtr geom2kml (
    OGRGeometry * poOgrGeom,
    int extra,
    KmlFactory * poKmlFactory );


/******************************************************************************
 function to read a kml geometry and translate to ogr

Args:
            poKmlGeometry   pointer to the kml geometry to translate
            poOgrSRS        pointer to the spatial ref to set on the geometry 

Returns:
            pointer to the new ogr geometry object

******************************************************************************/

OGRGeometry *kml2geom (
    GeometryPtr poKmlGeometry,
    OGRSpatialReference *poOgrSRS);

OGRGeometry *kml2geom_latlonbox (
    LatLonBoxPtr poKmlLatLonBox,
    OGRSpatialReference *poOgrSRS);

OGRGeometry *kml2geom_latlonquad (
    GxLatLonQuadPtr poKmlLatLonQuad,
    OGRSpatialReference *poOgrSRS);

/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines GeoJSON reader within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#ifndef OGR_GEOJSONWRITER_H_INCLUDED
#define OGR_GEOJSONWRITER_H_INCLUDED

#include <ogr_core.h>
#include <json.h> // JSON-C

/************************************************************************/
/*                         FORWARD DECLARATIONS                         */
/************************************************************************/

class OGRFeature;
class OGRGeometry;
class OGRPoint;
class OGRMultiPoint;
class OGRLineString;
class OGRMultiLineString;
class OGRLinearRing;
class OGRPolygon;
class OGRMultiPolygon;
class OGRGeometryCollection;

json_object* json_object_new_double_with_precision(double dfVal, int nCoordPrecision);

/************************************************************************/
/*                 GeoJSON Geometry Translators                         */
/************************************************************************/

json_object* OGRGeoJSONWriteFeature( OGRFeature* poFeature, int bWriteBBOX, int nCoordPrecision );
json_object* OGRGeoJSONWriteAttributes( OGRFeature* poFeature );
json_object* OGRGeoJSONWriteGeometry( OGRGeometry* poGeometry, int nCoordPrecision );
json_object* OGRGeoJSONWritePoint( OGRPoint* poPoint, int nCoordPrecision );
json_object* OGRGeoJSONWriteLineString( OGRLineString* poLine, int nCoordPrecision );
json_object* OGRGeoJSONWritePolygon( OGRPolygon* poPolygon, int nCoordPrecision );
json_object* OGRGeoJSONWriteMultiPoint( OGRMultiPoint* poGeometry, int nCoordPrecision );
json_object* OGRGeoJSONWriteMultiLineString( OGRMultiLineString* poGeometry, int nCoordPrecision );
json_object* OGRGeoJSONWriteMultiPolygon( OGRMultiPolygon* poGeometry, int nCoordPrecision );
json_object* OGRGeoJSONWriteGeometryCollection( OGRGeometryCollection* poGeometry, int nCoordPrecision );

json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, int nCoordPrecision );
json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, double const& fZ, int nCoordPrecision );
json_object* OGRGeoJSONWriteLineCoords( OGRLineString* poLine, int nCoordPrecision );

#endif /* OGR_GEOJSONWRITER_H_INCLUDED */

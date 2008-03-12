/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines GeoJSON reader within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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
#include <jsonc/json.h> // JSON-C

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

/************************************************************************/
/*                 GeoJSON Geometry Translators                         */
/************************************************************************/

json_object* OGRGeoJSONWriteFeature( OGRFeature* poFeature );
json_object* OGRGeoJSONWriteAttributes( OGRFeature* poFeature );
json_object* OGRGeoJSONWriteGeometry( OGRGeometry* poGeometry );
json_object* OGRGeoJSONWritePoint( OGRPoint* poPoint );
json_object* OGRGeoJSONWriteLineString( OGRLineString* poLine );
json_object* OGRGeoJSONWritePolygon( OGRPolygon* poPolygon );
json_object* OGRGeoJSONWriteMultiPoint( OGRMultiPoint* poGeometry );
json_object* OGRGeoJSONWriteMultiLineString( OGRMultiLineString* poGeometry );
json_object* OGRGeoJSONWriteMultiPolygon( OGRMultiPolygon* poGeometry );
json_object* OGRGeoJSONWriteGeometryCollection( OGRGeometryCollection* poGeometry );

json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY );
json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, double const& fZ );
json_object* OGRGeoJSONWriteLineCoords( OGRLineString* poLine );

#endif /* OGR_GEOJSONWRITER_H_INCLUDED */

/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine (GME) API Driver
 * Purpose:  GME Geo JSON helper functions.
 * Author:   Wolf Bergenheim <wolf+grass@bergenheim.net>
 *           (derived from the GeoJSON driver by Mateusz)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_GME_JSON_H_INCLUDED
#define _OGR_GME_JSON_H_INCLUDED

#include "ogr_feature.h"
#include "ogr_geometry.h"
#include <json.h>

json_object* OGRGMEFeatureToGeoJSON(OGRFeature* poFeature);
json_object* OGRGMEGeometryToGeoJSON(OGRGeometry* poGeometry);
json_object* OGRGMEGeometryCollectionToGeoJSON(OGRGeometryCollection* poGeometryCollection);
json_object* OGRGMEPointToGeoJSON( OGRPoint* poPoint );
json_object* OGRGMEMultiPointToGeoJSON( OGRMultiPoint* poGeometry );
json_object* OGRGMELineStringToGeoJSON( OGRLineString* poLine );
json_object* OGRGMEMultiLineStringToGeoJSON( OGRMultiLineString* poGeometry );
json_object* OGRGMEPolygonToGeoJSON( OGRPolygon* poPolygon );
json_object* OGRGMEMultiPolygonToGeoJSON( OGRMultiPolygon* poGeometry );
json_object* OGRGMECoordsToGeoJSON( double const& fX, double const& fY );
json_object* OGRGMECoordsToGeoJSON( double const& fX, double const& fY, double const& fZ );
json_object* OGRGMELineCoordsToGeoJSON( OGRLineString* poLine );
json_object* OGRGMEAttributesToGeoJSON( OGRFeature* poFeature );

json_object* json_object_new_gme_double(double dfVal);

json_object* OGRGMEParseJSON( const char* pszText );
const char*  OGRGMEGetJSONString(json_object *parent,
                                 const char *field_name,
                                 const char *default_value = NULL);

#endif /* ndef _OGR_GME_JSON_H_INCLUDED */

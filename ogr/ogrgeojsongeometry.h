// SPDX-License-Identifier: MIT
// Copyright 2007, Mateusz Loskot
// Copyright 2008-2024, Even Rouault <even.rouault at spatialys.com>

#ifndef OGRGEOJSONGEOMETRY_H_INCLUDED
#define OGRGEOJSONGEOMETRY_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include "cpl_port.h"
#include "cpl_json_header.h"

#include "ogr_api.h"

class OGRGeometry;
class OGRPolygon;
class OGRSpatialReference;

/************************************************************************/
/*                           GeoJSONObject                              */
/************************************************************************/

struct GeoJSONObject
{
    enum Type
    {
        eUnknown = wkbUnknown,  // non-GeoJSON properties
        ePoint = wkbPoint,
        eLineString = wkbLineString,
        ePolygon = wkbPolygon,
        eMultiPoint = wkbMultiPoint,
        eMultiLineString = wkbMultiLineString,
        eMultiPolygon = wkbMultiPolygon,
        eGeometryCollection = wkbGeometryCollection,
        eFeature,
        eFeatureCollection
    };

    enum CoordinateDimension
    {
        eMinCoordinateDimension = 2,
        eMaxCoordinateDimension = 3
    };
};

/************************************************************************/
/*                 GeoJSON Geometry Translators                         */
/************************************************************************/

GeoJSONObject::Type CPL_DLL OGRGeoJSONGetType(json_object *poObj);

OGRwkbGeometryType CPL_DLL OGRGeoJSONGetOGRGeometryType(json_object *poObj);

OGRGeometry CPL_DLL *
OGRGeoJSONReadGeometry(json_object *poObj,
                       OGRSpatialReference *poParentSRS = nullptr);
OGRSpatialReference CPL_DLL *OGRGeoJSONReadSpatialReference(json_object *poObj);

OGRPolygon *OGRGeoJSONReadPolygon(json_object *poObj, bool bRaw = false);

const char *OGRGeoJSONGetGeometryName(OGRGeometry const *poGeometry);

/*! @endcond */

#endif

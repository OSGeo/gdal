// SPDX-License-Identifier: MIT
// Copyright 2007, Mateusz Loskot
// Copyright 2008-2024, Even Rouault <even.rouault at spatialys.com>

#ifndef OGRGEOJSONGEOMETRY_H_INCLUDED
#define OGRGEOJSONGEOMETRY_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include "cpl_port.h"
#include "cpl_json_header.h"

#include "ogr_api.h"
#include "ogr_geometry.h"

/************************************************************************/
/*                            GeoJSONObject                             */
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
        eCircularString = wkbCircularString,  // JSON-FG extension
        eCompoundCurve = wkbCompoundCurve,    // JSON-FG extension
        eCurvePolygon = wkbCurvePolygon,      // JSON-FG extension
        eMultiCurve = wkbMultiCurve,          // JSON-FG extension
        eMultiSurface = wkbMultiSurface,      // JSON-FG extension
        eFeature,
        eFeatureCollection
    };

    enum CoordinateDimension
    {
        eMinCoordinateDimension = 2,
        eMaxCoordinateDimensionGeoJSON = 3,
        eMaxCoordinateDimensionJSONFG = 4,
    };
};

/************************************************************************/
/*                     GeoJSON Geometry Translators                     */
/************************************************************************/

GeoJSONObject::Type CPL_DLL OGRGeoJSONGetType(json_object *poObj);

bool CPL_DLL OGRJSONFGHasMeasure(json_object *poObj, bool bUpperLevelMValue);

OGRwkbGeometryType CPL_DLL OGRGeoJSONGetOGRGeometryType(json_object *poObj,
                                                        bool bHasM);

std::unique_ptr<OGRGeometry>
    CPL_DLL OGRGeoJSONReadGeometry(json_object *poObj, bool bHasM,
                                   const OGRSpatialReference *poParentSRS);

OGRSpatialReference CPL_DLL *OGRGeoJSONReadSpatialReference(json_object *poObj);

std::unique_ptr<OGRPolygon> OGRGeoJSONReadPolygon(json_object *poObj,
                                                  bool bHasM, bool bRaw);

const char *OGRGeoJSONGetGeometryName(OGRGeometry const *poGeometry);

/*! @endcond */

#endif

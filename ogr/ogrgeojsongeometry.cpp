// SPDX-License-Identifier: MIT
// Copyright 2007, Mateusz Loskot
// Copyright 2008-2024, Even Rouault <even.rouault at spatialys.com>

/*! @cond Doxygen_Suppress */

#include "ogrgeojsongeometry.h"
#include "ogrlibjsonutils.h"

#include "ogr_geometry.h"
#include "ogr_spatialref.h"

static OGRPoint *OGRGeoJSONReadPoint(json_object *poObj);
static OGRMultiPoint *OGRGeoJSONReadMultiPoint(json_object *poObj);
static OGRLineString *OGRGeoJSONReadLineString(json_object *poObj,
                                               bool bRaw = false);
static OGRMultiLineString *OGRGeoJSONReadMultiLineString(json_object *poObj);
static OGRLinearRing *OGRGeoJSONReadLinearRing(json_object *poObj);
static OGRMultiPolygon *OGRGeoJSONReadMultiPolygon(json_object *poObj);
static OGRGeometryCollection *
OGRGeoJSONReadGeometryCollection(json_object *poObj,
                                 OGRSpatialReference *poSRS = nullptr);

/************************************************************************/
/*                           OGRGeoJSONGetType                          */
/************************************************************************/

GeoJSONObject::Type OGRGeoJSONGetType(json_object *poObj)
{
    if (nullptr == poObj)
        return GeoJSONObject::eUnknown;

    json_object *poObjType = OGRGeoJSONFindMemberByName(poObj, "type");
    if (nullptr == poObjType)
        return GeoJSONObject::eUnknown;

    const char *name = json_object_get_string(poObjType);
    if (EQUAL(name, "Point"))
        return GeoJSONObject::ePoint;
    else if (EQUAL(name, "LineString"))
        return GeoJSONObject::eLineString;
    else if (EQUAL(name, "Polygon"))
        return GeoJSONObject::ePolygon;
    else if (EQUAL(name, "MultiPoint"))
        return GeoJSONObject::eMultiPoint;
    else if (EQUAL(name, "MultiLineString"))
        return GeoJSONObject::eMultiLineString;
    else if (EQUAL(name, "MultiPolygon"))
        return GeoJSONObject::eMultiPolygon;
    else if (EQUAL(name, "GeometryCollection"))
        return GeoJSONObject::eGeometryCollection;
    else if (EQUAL(name, "Feature"))
        return GeoJSONObject::eFeature;
    else if (EQUAL(name, "FeatureCollection"))
        return GeoJSONObject::eFeatureCollection;
    else
        return GeoJSONObject::eUnknown;
}

/************************************************************************/
/*                   OGRGeoJSONGetOGRGeometryType()                     */
/************************************************************************/

OGRwkbGeometryType OGRGeoJSONGetOGRGeometryType(json_object *poObj)
{
    if (nullptr == poObj)
        return wkbUnknown;

    json_object *poObjType = CPL_json_object_object_get(poObj, "type");
    if (nullptr == poObjType)
        return wkbUnknown;

    OGRwkbGeometryType eType = wkbUnknown;
    const char *name = json_object_get_string(poObjType);
    if (EQUAL(name, "Point"))
        eType = wkbPoint;
    else if (EQUAL(name, "LineString"))
        eType = wkbLineString;
    else if (EQUAL(name, "Polygon"))
        eType = wkbPolygon;
    else if (EQUAL(name, "MultiPoint"))
        eType = wkbMultiPoint;
    else if (EQUAL(name, "MultiLineString"))
        eType = wkbMultiLineString;
    else if (EQUAL(name, "MultiPolygon"))
        eType = wkbMultiPolygon;
    else if (EQUAL(name, "GeometryCollection"))
        eType = wkbGeometryCollection;
    else
        return wkbUnknown;

    json_object *poCoordinates;
    if (eType == wkbGeometryCollection)
    {
        json_object *poGeometries =
            CPL_json_object_object_get(poObj, "geometries");
        if (poGeometries &&
            json_object_get_type(poGeometries) == json_type_array &&
            json_object_array_length(poGeometries) > 0)
        {
            if (OGR_GT_HasZ(OGRGeoJSONGetOGRGeometryType(
                    json_object_array_get_idx(poGeometries, 0))))
                eType = OGR_GT_SetZ(eType);
        }
    }
    else
    {
        poCoordinates = CPL_json_object_object_get(poObj, "coordinates");
        if (poCoordinates &&
            json_object_get_type(poCoordinates) == json_type_array &&
            json_object_array_length(poCoordinates) > 0)
        {
            while (true)
            {
                auto poChild = json_object_array_get_idx(poCoordinates, 0);
                if (!(poChild &&
                      json_object_get_type(poChild) == json_type_array &&
                      json_object_array_length(poChild) > 0))
                {
                    if (json_object_array_length(poCoordinates) == 3)
                        eType = OGR_GT_SetZ(eType);
                    break;
                }
                poCoordinates = poChild;
            }
        }
    }

    return eType;
}

/************************************************************************/
/*                           OGRGeoJSONReadGeometry                     */
/************************************************************************/

OGRGeometry *OGRGeoJSONReadGeometry(json_object *poObj,
                                    OGRSpatialReference *poParentSRS)
{

    OGRGeometry *poGeometry = nullptr;
    OGRSpatialReference *poSRS = nullptr;
    lh_entry *entry = OGRGeoJSONFindMemberEntryByName(poObj, "crs");
    if (entry != nullptr)
    {
        json_object *poObjSrs =
            static_cast<json_object *>(const_cast<void *>(entry->v));
        if (poObjSrs != nullptr)
        {
            poSRS = OGRGeoJSONReadSpatialReference(poObj);
        }
    }

    OGRSpatialReference *poSRSToAssign = nullptr;
    if (entry != nullptr)
    {
        poSRSToAssign = poSRS;
    }
    else if (poParentSRS)
    {
        poSRSToAssign = poParentSRS;
    }
    else
    {
        // Assign WGS84 if no CRS defined on geometry.
        poSRSToAssign = OGRSpatialReference::GetWGS84SRS();
    }

    GeoJSONObject::Type objType = OGRGeoJSONGetType(poObj);
    if (GeoJSONObject::ePoint == objType)
        poGeometry = OGRGeoJSONReadPoint(poObj);
    else if (GeoJSONObject::eMultiPoint == objType)
        poGeometry = OGRGeoJSONReadMultiPoint(poObj);
    else if (GeoJSONObject::eLineString == objType)
        poGeometry = OGRGeoJSONReadLineString(poObj);
    else if (GeoJSONObject::eMultiLineString == objType)
        poGeometry = OGRGeoJSONReadMultiLineString(poObj);
    else if (GeoJSONObject::ePolygon == objType)
        poGeometry = OGRGeoJSONReadPolygon(poObj);
    else if (GeoJSONObject::eMultiPolygon == objType)
        poGeometry = OGRGeoJSONReadMultiPolygon(poObj);
    else if (GeoJSONObject::eGeometryCollection == objType)
        poGeometry = OGRGeoJSONReadGeometryCollection(poObj, poSRSToAssign);
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unsupported geometry type detected. "
                 "Feature gets NULL geometry assigned.");
    }

    if (poGeometry && GeoJSONObject::eGeometryCollection != objType)
        poGeometry->assignSpatialReference(poSRSToAssign);

    if (poSRS)
        poSRS->Release();

    return poGeometry;
}

/************************************************************************/
/*                       GetJSONConstructName()                         */
/************************************************************************/

static const char *GetJSONConstructName(json_type eType)
{
    switch (eType)
    {
        case json_type_null:
            break;
        case json_type_boolean:
            return "boolean";
        case json_type_double:
            return "double";
        case json_type_int:
            return "int";
        case json_type_object:
            return "object";
        case json_type_array:
            return "array";
        case json_type_string:
            return "string";
    }
    return "null";
}

/************************************************************************/
/*                        OGRGeoJSONGetCoordinate()                     */
/************************************************************************/

static double OGRGeoJSONGetCoordinate(json_object *poObj,
                                      const char *pszCoordName, int nIndex,
                                      bool &bValid)
{
    json_object *poObjCoord = json_object_array_get_idx(poObj, nIndex);
    if (nullptr == poObjCoord)
    {
        CPLDebug("GeoJSON", "Point: got null object for %s.", pszCoordName);
        bValid = false;
        return 0.0;
    }

    const json_type eType = json_object_get_type(poObjCoord);
    if (json_type_double != eType && json_type_int != eType)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONGetCoordinate(): invalid '%s' coordinate. "
                 "Unexpected type %s for '%s'. Expected double or integer.",
                 pszCoordName, GetJSONConstructName(eType),
                 json_object_to_json_string(poObjCoord));
        bValid = false;
        return 0.0;
    }

    return json_object_get_double(poObjCoord);
}

/************************************************************************/
/*                           OGRGeoJSONReadRawPoint                     */
/************************************************************************/

static bool OGRGeoJSONReadRawPoint(json_object *poObj, OGRPoint &point)
{
    if (json_type_array == json_object_get_type(poObj))
    {
        const int nSize = static_cast<int>(json_object_array_length(poObj));

        if (nSize < GeoJSONObject::eMinCoordinateDimension)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "OGRGeoJSONReadRawPoint(): "
                     "Invalid coord dimension for '%s'. "
                     "At least 2 dimensions must be present.",
                     json_object_to_json_string(poObj));
            return false;
        }

        bool bValid = true;
        const double dfX = OGRGeoJSONGetCoordinate(poObj, "x", 0, bValid);
        const double dfY = OGRGeoJSONGetCoordinate(poObj, "y", 1, bValid);
        point.setX(dfX);
        point.setY(dfY);

        // Read Z coordinate.
        if (nSize >= GeoJSONObject::eMaxCoordinateDimension)
        {
            if (nSize > GeoJSONObject::eMaxCoordinateDimension)
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "OGRGeoJSONReadRawPoint(): too many members in "
                             "array '%s': %d. At most %d are handled. Ignoring "
                             "extra members.",
                             json_object_to_json_string(poObj), nSize,
                             GeoJSONObject::eMaxCoordinateDimension);
            }
            // Don't *expect* mixed-dimension geometries, although the
            // spec doesn't explicitly forbid this.
            const double dfZ = OGRGeoJSONGetCoordinate(poObj, "z", 2, bValid);
            point.setZ(dfZ);
        }
        else
        {
            point.flattenTo2D();
        }
        return bValid;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONReadRawPoint(): invalid Point. "
                 "Unexpected type %s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObj)),
                 json_object_to_json_string(poObj));
    }

    return false;
}

/************************************************************************/
/*                           OGRGeoJSONReadPoint                        */
/************************************************************************/

OGRPoint *OGRGeoJSONReadPoint(json_object *poObj)
{
    if (!poObj)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONReadPoint(): invalid Point object. Got null.");
        return nullptr;
    }
    json_object *poObjCoords = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjCoords)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONReadPoint(): invalid Point object. "
                 "Missing \'coordinates\' member.");
        return nullptr;
    }

    auto poPoint = std::make_unique<OGRPoint>();
    if (!OGRGeoJSONReadRawPoint(poObjCoords, *poPoint))
    {
        return nullptr;
    }

    return poPoint.release();
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiPoint                   */
/************************************************************************/

OGRMultiPoint *OGRGeoJSONReadMultiPoint(json_object *poObj)
{
    if (!poObj)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "OGRGeoJSONReadMultiPoint(): invalid MultiPoint object. Got null.");
        return nullptr;
    }
    json_object *poObjPoints = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjPoints)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiPoint object. "
                 "Missing \'coordinates\' member.");
        return nullptr;
    }

    std::unique_ptr<OGRMultiPoint> poMultiPoint;
    if (json_type_array == json_object_get_type(poObjPoints))
    {
        const auto nPoints = json_object_array_length(poObjPoints);

        poMultiPoint = std::make_unique<OGRMultiPoint>();

        for (auto i = decltype(nPoints){0}; i < nPoints; ++i)
        {
            json_object *poObjCoords =
                json_object_array_get_idx(poObjPoints, i);

            OGRPoint pt;
            if (!OGRGeoJSONReadRawPoint(poObjCoords, pt))
            {
                return nullptr;
            }
            poMultiPoint->addGeometry(&pt);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONReadMultiPoint(): invalid MultiPoint. "
                 "Unexpected type %s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObjPoints)),
                 json_object_to_json_string(poObjPoints));
    }

    return poMultiPoint.release();
}

/************************************************************************/
/*                           OGRGeoJSONReadLineString                   */
/************************************************************************/

OGRLineString *OGRGeoJSONReadLineString(json_object *poObj, bool bRaw)
{
    if (!poObj)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "OGRGeoJSONReadLineString(): invalid LineString object. Got null.");
        return nullptr;
    }
    json_object *poObjPoints = nullptr;

    if (!bRaw)
    {
        poObjPoints = OGRGeoJSONFindMemberByName(poObj, "coordinates");
        if (nullptr == poObjPoints)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid LineString object. "
                     "Missing \'coordinates\' member.");
            return nullptr;
        }
    }
    else
    {
        poObjPoints = poObj;
    }

    std::unique_ptr<OGRLineString> poLine;

    if (json_type_array == json_object_get_type(poObjPoints))
    {
        const auto nPoints = json_object_array_length(poObjPoints);

        poLine = std::make_unique<OGRLineString>();
        poLine->setNumPoints(static_cast<int>(nPoints));

        for (auto i = decltype(nPoints){0}; i < nPoints; ++i)
        {
            json_object *poObjCoords =
                json_object_array_get_idx(poObjPoints, i);

            OGRPoint pt;
            if (!OGRGeoJSONReadRawPoint(poObjCoords, pt))
            {
                return nullptr;
            }
            if (pt.getCoordinateDimension() == 2)
            {
                poLine->setPoint(static_cast<int>(i), pt.getX(), pt.getY());
            }
            else
            {
                poLine->setPoint(static_cast<int>(i), pt.getX(), pt.getY(),
                                 pt.getZ());
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONReadLineString(): invalid MultiLineString. "
                 "Unexpected type %s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObjPoints)),
                 json_object_to_json_string(poObjPoints));
    }

    return poLine.release();
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiLineString              */
/************************************************************************/

OGRMultiLineString *OGRGeoJSONReadMultiLineString(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjLines = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjLines)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiLineString object. "
                 "Missing \'coordinates\' member.");
        return nullptr;
    }

    std::unique_ptr<OGRMultiLineString> poMultiLine;

    if (json_type_array == json_object_get_type(poObjLines))
    {
        const auto nLines = json_object_array_length(poObjLines);

        poMultiLine = std::make_unique<OGRMultiLineString>();

        for (auto i = decltype(nLines){0}; i < nLines; ++i)
        {
            json_object *poObjLine = json_object_array_get_idx(poObjLines, i);

            OGRLineString *poLine = OGRGeoJSONReadLineString(poObjLine, true);
            if (poLine)
            {
                poMultiLine->addGeometryDirectly(poLine);
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONReadLineString(): invalid LineString. "
                 "Unexpected type %s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObjLines)),
                 json_object_to_json_string(poObjLines));
    }

    return poMultiLine.release();
}

/************************************************************************/
/*                           OGRGeoJSONReadLinearRing                   */
/************************************************************************/

OGRLinearRing *OGRGeoJSONReadLinearRing(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    std::unique_ptr<OGRLinearRing> poRing;

    if (json_type_array == json_object_get_type(poObj))
    {
        const auto nPoints = json_object_array_length(poObj);

        poRing = std::make_unique<OGRLinearRing>();
        poRing->setNumPoints(static_cast<int>(nPoints));

        for (auto i = decltype(nPoints){0}; i < nPoints; ++i)
        {
            json_object *poObjCoords = json_object_array_get_idx(poObj, i);

            OGRPoint pt;
            if (!OGRGeoJSONReadRawPoint(poObjCoords, pt))
            {
                return nullptr;
            }

            if (2 == pt.getCoordinateDimension())
                poRing->setPoint(static_cast<int>(i), pt.getX(), pt.getY());
            else
                poRing->setPoint(static_cast<int>(i), pt.getX(), pt.getY(),
                                 pt.getZ());
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "OGRGeoJSONReadLinearRing(): unexpected type of JSON "
                 "construct %s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObj)),
                 json_object_to_json_string(poObj));
    }

    return poRing.release();
}

/************************************************************************/
/*                           OGRGeoJSONReadPolygon                      */
/************************************************************************/

OGRPolygon *OGRGeoJSONReadPolygon(json_object *poObj, bool bRaw)
{
    if (!poObj)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRGeoJSONReadPolygon(): invalid Polygon object. Got null.");
        return nullptr;
    }
    json_object *poObjRings = nullptr;

    if (!bRaw)
    {
        poObjRings = OGRGeoJSONFindMemberByName(poObj, "coordinates");
        if (nullptr == poObjRings)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid Polygon object. "
                     "Missing \'coordinates\' member.");
            return nullptr;
        }
    }
    else
    {
        poObjRings = poObj;
    }

    std::unique_ptr<OGRPolygon> poPolygon;

    if (json_type_array == json_object_get_type(poObjRings))
    {
        const auto nRings = json_object_array_length(poObjRings);
        if (nRings > 0)
        {
            json_object *poObjPoints = json_object_array_get_idx(poObjRings, 0);
            if (!poObjPoints)
            {
                poPolygon = std::make_unique<OGRPolygon>();
            }
            else
            {
                OGRLinearRing *poRing = OGRGeoJSONReadLinearRing(poObjPoints);
                if (poRing)
                {
                    poPolygon = std::make_unique<OGRPolygon>();
                    poPolygon->addRingDirectly(poRing);
                }
            }

            for (auto i = decltype(nRings){1};
                 i < nRings && nullptr != poPolygon; ++i)
            {
                poObjPoints = json_object_array_get_idx(poObjRings, i);
                if (poObjPoints)
                {
                    OGRLinearRing *poRing =
                        OGRGeoJSONReadLinearRing(poObjPoints);
                    if (poRing)
                    {
                        poPolygon->addRingDirectly(poRing);
                    }
                }
            }
        }
        else
        {
            poPolygon = std::make_unique<OGRPolygon>();
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "OGRGeoJSONReadPolygon(): unexpected type of JSON construct "
                 "%s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObjRings)),
                 json_object_to_json_string(poObjRings));
    }

    return poPolygon.release();
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiPolygon                 */
/************************************************************************/

OGRMultiPolygon *OGRGeoJSONReadMultiPolygon(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjPolys = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjPolys)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiPolygon object. "
                 "Missing \'coordinates\' member.");
        return nullptr;
    }

    std::unique_ptr<OGRMultiPolygon> poMultiPoly;

    if (json_type_array == json_object_get_type(poObjPolys))
    {
        const auto nPolys = json_object_array_length(poObjPolys);

        poMultiPoly = std::make_unique<OGRMultiPolygon>();

        for (auto i = decltype(nPolys){0}; i < nPolys; ++i)
        {
            json_object *poObjPoly = json_object_array_get_idx(poObjPolys, i);
            if (!poObjPoly)
            {
                poMultiPoly->addGeometryDirectly(
                    std::make_unique<OGRPolygon>().release());
            }
            else
            {
                OGRPolygon *poPoly = OGRGeoJSONReadPolygon(poObjPoly, true);
                if (poPoly)
                {
                    poMultiPoly->addGeometryDirectly(poPoly);
                }
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "OGRGeoJSONReadMultiPolygon(): unexpected type of JSON "
                 "construct %s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObjPolys)),
                 json_object_to_json_string(poObjPolys));
    }

    return poMultiPoly.release();
}

/************************************************************************/
/*                    OGRGeoJSONReadGeometryCollection                  */
/************************************************************************/

OGRGeometryCollection *
OGRGeoJSONReadGeometryCollection(json_object *poObj, OGRSpatialReference *poSRS)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjGeoms = OGRGeoJSONFindMemberByName(poObj, "geometries");
    if (nullptr == poObjGeoms)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid GeometryCollection object. "
                 "Missing \'geometries\' member.");
        return nullptr;
    }

    std::unique_ptr<OGRGeometryCollection> poCollection;

    if (json_type_array == json_object_get_type(poObjGeoms))
    {
        poCollection = std::make_unique<OGRGeometryCollection>();
        poCollection->assignSpatialReference(poSRS);

        const auto nGeoms = json_object_array_length(poObjGeoms);
        for (auto i = decltype(nGeoms){0}; i < nGeoms; ++i)
        {
            json_object *poObjGeom = json_object_array_get_idx(poObjGeoms, i);
            if (!poObjGeom)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "OGRGeoJSONReadGeometryCollection(): skipping null "
                         "sub-geometry");
                continue;
            }

            OGRGeometry *poGeometry = OGRGeoJSONReadGeometry(poObjGeom, poSRS);
            if (poGeometry)
            {
                poCollection->addGeometryDirectly(poGeometry);
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "OGRGeoJSONReadGeometryCollection(): unexpected type of JSON "
                 "construct %s for '%s'. Expected array.",
                 GetJSONConstructName(json_object_get_type(poObjGeoms)),
                 json_object_to_json_string(poObjGeoms));
    }

    return poCollection.release();
}

/************************************************************************/
/*                           OGRGeoJSONGetGeometryName()                */
/************************************************************************/

const char *OGRGeoJSONGetGeometryName(OGRGeometry const *poGeometry)
{
    CPLAssert(nullptr != poGeometry);

    const OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());

    if (wkbPoint == eType)
        return "Point";
    else if (wkbLineString == eType)
        return "LineString";
    else if (wkbPolygon == eType)
        return "Polygon";
    else if (wkbMultiPoint == eType)
        return "MultiPoint";
    else if (wkbMultiLineString == eType)
        return "MultiLineString";
    else if (wkbMultiPolygon == eType)
        return "MultiPolygon";
    else if (wkbGeometryCollection == eType)
        return "GeometryCollection";

    return "Unknown";
}

/************************************************************************/
/*                    OGRGeoJSONReadSpatialReference                    */
/************************************************************************/

OGRSpatialReference *OGRGeoJSONReadSpatialReference(json_object *poObj)
{

    /* -------------------------------------------------------------------- */
    /*      Read spatial reference definition.                              */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = nullptr;

    json_object *poObjSrs = OGRGeoJSONFindMemberByName(poObj, "crs");
    if (nullptr != poObjSrs)
    {
        json_object *poObjSrsType =
            OGRGeoJSONFindMemberByName(poObjSrs, "type");
        if (poObjSrsType == nullptr)
            return nullptr;

        const char *pszSrsType = json_object_get_string(poObjSrsType);

        // TODO: Add URL and URN types support.
        if (STARTS_WITH_CI(pszSrsType, "NAME"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poNameURL =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "name");
            if (poNameURL == nullptr)
                return nullptr;

            const char *pszName = json_object_get_string(poNameURL);

            // Mostly to emulate GDAL 2.x behavior
            // See https://github.com/OSGeo/gdal/issues/2035
            if (EQUAL(pszName, "urn:ogc:def:crs:OGC:1.3:CRS84"))
                pszName = "EPSG:4326";

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE !=
                poSRS->SetFromUserInput(
                    pszName,
                    OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }

        else if (STARTS_WITH_CI(pszSrsType, "EPSG"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poObjCode =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "code");
            if (poObjCode == nullptr)
                return nullptr;

            int nEPSG = json_object_get_int(poObjCode);

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE != poSRS->importFromEPSG(nEPSG))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }

        else if (STARTS_WITH_CI(pszSrsType, "URL") ||
                 STARTS_WITH_CI(pszSrsType, "LINK"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poObjURL =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "url");

            if (nullptr == poObjURL)
            {
                poObjURL = OGRGeoJSONFindMemberByName(poObjSrsProps, "href");
            }
            if (poObjURL == nullptr)
                return nullptr;

            const char *pszURL = json_object_get_string(poObjURL);

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE != poSRS->importFromUrl(pszURL))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }

        else if (EQUAL(pszSrsType, "OGC"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poObjURN =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "urn");
            if (poObjURN == nullptr)
                return nullptr;

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE !=
                poSRS->importFromURN(json_object_get_string(poObjURN)))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }
    }

    // Strip AXIS, since geojson has (easting, northing) / (longitude, latitude)
    // order.  According to http://www.geojson.org/geojson-spec.html#id2 :
    // "Point coordinates are in x, y order (easting, northing for projected
    // coordinates, longitude, latitude for geographic coordinates)".
    if (poSRS != nullptr)
    {
        OGR_SRSNode *poGEOGCS = poSRS->GetAttrNode("GEOGCS");
        if (poGEOGCS != nullptr)
            poGEOGCS->StripNodes("AXIS");
    }

    return poSRS;
}

/************************************************************************/
/*                       OGR_G_CreateGeometryFromJson                   */
/************************************************************************/

/** Create a OGR geometry from a GeoJSON geometry object */
OGRGeometryH OGR_G_CreateGeometryFromJson(const char *pszJson)
{
    if (nullptr == pszJson)
    {
        // Translation failed.
        return nullptr;
    }

    json_object *poObj = nullptr;
    if (!OGRJSonParse(pszJson, &poObj))
        return nullptr;

    OGRGeometry *poGeometry = OGRGeoJSONReadGeometry(poObj);

    // Release JSON tree.
    json_object_put(poObj);

    return OGRGeometry::ToHandle(poGeometry);
}

/*! @endcond */

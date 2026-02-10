// SPDX-License-Identifier: MIT
// Copyright 2007, Mateusz Loskot
// Copyright 2008-2024, Even Rouault <even.rouault at spatialys.com>

/*! @cond Doxygen_Suppress */

#include "ogrgeojsongeometry.h"
#include "ogrlibjsonutils.h"

#include "ogr_geometry.h"
#include "ogr_spatialref.h"

static std::unique_ptr<OGRPoint> OGRGeoJSONReadPoint(json_object *poObj,
                                                     bool bHasM);
static std::unique_ptr<OGRMultiPoint>
OGRGeoJSONReadMultiPoint(json_object *poObj, bool bHasM);
static std::unique_ptr<OGRLineString>
OGRGeoJSONReadLineString(json_object *poObj, bool bHasM, bool bRaw);
static std::unique_ptr<OGRMultiLineString>
OGRGeoJSONReadMultiLineString(json_object *poObj, bool bHasM);
static std::unique_ptr<OGRLinearRing>
OGRGeoJSONReadLinearRing(json_object *poObj, bool bHasM);
static std::unique_ptr<OGRMultiPolygon>
OGRGeoJSONReadMultiPolygon(json_object *poObj, bool bHasM);
static std::unique_ptr<OGRGeometryCollection>
OGRGeoJSONReadGeometryCollection(json_object *poObj, bool bHasM,
                                 const OGRSpatialReference *poSRS);
static std::unique_ptr<OGRCircularString>
OGRGeoJSONReadCircularString(json_object *poObj, bool bHasM);
static std::unique_ptr<OGRCompoundCurve>
OGRGeoJSONReadCompoundCurve(json_object *poObj, bool bHasM,
                            const OGRSpatialReference *poSRS);
static std::unique_ptr<OGRCurvePolygon>
OGRGeoJSONReadCurvePolygon(json_object *poObj, bool bHasM);
static std::unique_ptr<OGRMultiCurve>
OGRGeoJSONReadMultiCurve(json_object *poObj, bool bHasM,
                         const OGRSpatialReference *poSRS);
static std::unique_ptr<OGRMultiSurface>
OGRGeoJSONReadMultiSurface(json_object *poObj, bool bHasM,
                           const OGRSpatialReference *poSRS);

/************************************************************************/
/*                          OGRGeoJSONGetType                           */
/************************************************************************/

GeoJSONObject::Type OGRGeoJSONGetType(json_object *poObj)
{
    if (nullptr == poObj)
        return GeoJSONObject::eUnknown;

    json_object *poObjType = OGRGeoJSONFindMemberByName(poObj, "type");
    if (nullptr == poObjType)
        return GeoJSONObject::eUnknown;

    const char *name = json_object_get_string(poObjType);

#define ASSOC(x)                                                               \
    {                                                                          \
        #x, GeoJSONObject::e##x                                                \
    }

    static const struct
    {
        const char *pszName;
        GeoJSONObject::Type eType;
    } tabAssoc[] = {
        ASSOC(Point),
        ASSOC(LineString),
        ASSOC(Polygon),
        ASSOC(MultiPoint),
        ASSOC(MultiLineString),
        ASSOC(MultiPolygon),
        ASSOC(GeometryCollection),
        ASSOC(CircularString),
        ASSOC(CompoundCurve),
        ASSOC(CurvePolygon),
        ASSOC(MultiCurve),
        ASSOC(MultiSurface),
        ASSOC(Feature),
        ASSOC(FeatureCollection),
    };

#undef ASSOC

    for (const auto &assoc : tabAssoc)
    {
        if (EQUAL(name, assoc.pszName))
            return assoc.eType;
    }

    return GeoJSONObject::eUnknown;
}

/************************************************************************/
/*                        OGRJSONFGHasMeasure()                         */
/************************************************************************/

bool OGRJSONFGHasMeasure(json_object *poObj, bool bUpperLevelMValue)
{
    bool bHasM = bUpperLevelMValue;
    if (json_object *pojMeasures =
            CPL_json_object_object_get(poObj, "measures"))
    {
        json_object *poEnabled =
            CPL_json_object_object_get(pojMeasures, "enabled");
        bHasM = json_object_get_boolean(poEnabled);
    }
    return bHasM;
}

/************************************************************************/
/*                        asAssocGeometryTypes[]                        */
/************************************************************************/

#define ASSOC(x) {#x, wkb##x}

static const struct
{
    const char *pszName;
    OGRwkbGeometryType eType;
} asAssocGeometryTypes[] = {
    ASSOC(Point),
    ASSOC(LineString),
    ASSOC(Polygon),
    ASSOC(MultiPoint),
    ASSOC(MultiLineString),
    ASSOC(MultiPolygon),
    ASSOC(GeometryCollection),
    ASSOC(CircularString),
    ASSOC(CompoundCurve),
    ASSOC(CurvePolygon),
    ASSOC(MultiCurve),
    ASSOC(MultiSurface),
};

#undef ASSOC

/************************************************************************/
/*                    OGRGeoJSONGetOGRGeometryType()                    */
/************************************************************************/

OGRwkbGeometryType OGRGeoJSONGetOGRGeometryType(json_object *poObj, bool bHasM)
{
    if (nullptr == poObj)
        return wkbUnknown;

    json_object *poObjType = CPL_json_object_object_get(poObj, "type");
    if (nullptr == poObjType)
        return wkbUnknown;

    const char *name = json_object_get_string(poObjType);

    OGRwkbGeometryType eType = wkbNone;
    for (const auto &assoc : asAssocGeometryTypes)
    {
        if (EQUAL(name, assoc.pszName))
        {
            eType = assoc.eType;
            break;
        }
    }
    if (eType == wkbNone)
        return wkbUnknown;

    bHasM = OGRJSONFGHasMeasure(poObj, bHasM);

    json_object *poCoordinates;
    if (eType == wkbGeometryCollection || eType == wkbMultiCurve ||
        eType == wkbMultiSurface || eType == wkbCompoundCurve ||
        eType == wkbCurvePolygon)
    {
        json_object *poGeometries =
            CPL_json_object_object_get(poObj, "geometries");
        if (poGeometries &&
            json_object_get_type(poGeometries) == json_type_array &&
            json_object_array_length(poGeometries) > 0)
        {
            const auto subGeomType = OGRGeoJSONGetOGRGeometryType(
                json_object_array_get_idx(poGeometries, 0), bHasM);
            if (OGR_GT_HasZ(subGeomType))
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
                    const auto nLength =
                        json_object_array_length(poCoordinates);
                    if ((bHasM && nLength == 4) || (!bHasM && nLength == 3))
                        eType = OGR_GT_SetZ(eType);
                    break;
                }
                poCoordinates = poChild;
            }
        }
    }
    if (bHasM)
        eType = OGR_GT_SetM(eType);

    return eType;
}

/************************************************************************/
/*                     OGRGeoJSONGetGeometryName()                      */
/************************************************************************/

const char *OGRGeoJSONGetGeometryName(OGRGeometry const *poGeometry)
{
    CPLAssert(nullptr != poGeometry);

    const OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());

    for (const auto &assoc : asAssocGeometryTypes)
    {
        if (eType == assoc.eType)
        {
            return assoc.pszName;
        }
    }
    return "Unknown";
}

/************************************************************************/
/*                        OGRGeoJSONReadGeometry                        */
/************************************************************************/

std::unique_ptr<OGRGeometry>
OGRGeoJSONReadGeometry(json_object *poObj, bool bHasM,
                       const OGRSpatialReference *poParentSRS)
{

    std::unique_ptr<OGRGeometry> poGeometry;
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

    const OGRSpatialReference *poSRSToAssign = nullptr;
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

    bHasM = OGRJSONFGHasMeasure(poObj, bHasM);

    const auto objType = OGRGeoJSONGetType(poObj);
    switch (objType)
    {
        case GeoJSONObject::ePoint:
            poGeometry = OGRGeoJSONReadPoint(poObj, bHasM);
            break;

        case GeoJSONObject::eLineString:
            poGeometry = OGRGeoJSONReadLineString(poObj, bHasM,
                                                  /* bRaw = */ false);
            break;

        case GeoJSONObject::ePolygon:
            poGeometry =
                OGRGeoJSONReadPolygon(poObj, bHasM, /* bRaw = */ false);
            break;

        case GeoJSONObject::eMultiPoint:
            poGeometry = OGRGeoJSONReadMultiPoint(poObj, bHasM);
            break;

        case GeoJSONObject::eMultiLineString:
            poGeometry = OGRGeoJSONReadMultiLineString(poObj, bHasM);
            break;

        case GeoJSONObject::eMultiPolygon:
            poGeometry = OGRGeoJSONReadMultiPolygon(poObj, bHasM);
            break;

        case GeoJSONObject::eGeometryCollection:
            poGeometry =
                OGRGeoJSONReadGeometryCollection(poObj, bHasM, poSRSToAssign);
            break;

        case GeoJSONObject::eCircularString:
            poGeometry = OGRGeoJSONReadCircularString(poObj, bHasM);
            break;

        case GeoJSONObject::eCompoundCurve:
            poGeometry =
                OGRGeoJSONReadCompoundCurve(poObj, bHasM, poSRSToAssign);
            break;

        case GeoJSONObject::eCurvePolygon:
            poGeometry = OGRGeoJSONReadCurvePolygon(poObj, bHasM);
            break;

        case GeoJSONObject::eMultiCurve:
            poGeometry = OGRGeoJSONReadMultiCurve(poObj, bHasM, poSRSToAssign);
            break;

        case GeoJSONObject::eMultiSurface:
            poGeometry =
                OGRGeoJSONReadMultiSurface(poObj, bHasM, poSRSToAssign);
            break;

        case GeoJSONObject::eFeature:
        case GeoJSONObject::eFeatureCollection:
            [[fallthrough]];
        case GeoJSONObject::eUnknown:
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unsupported geometry type detected. "
                     "Feature gets NULL geometry assigned.");
            break;
    }

    if (poGeometry && GeoJSONObject::eGeometryCollection != objType)
        poGeometry->assignSpatialReference(poSRSToAssign);

    if (poSRS)
        poSRS->Release();

    return poGeometry;
}

/************************************************************************/
/*                        GetJSONConstructName()                        */
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
/*                      OGRGeoJSONGetCoordinate()                       */
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
/*                        OGRGeoJSONReadRawPoint                        */
/************************************************************************/

static bool OGRGeoJSONReadRawPoint(json_object *poObj, OGRPoint &point,
                                   bool bHasM)
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

        // Read Z and/or M coordinate.
        if (nSize > GeoJSONObject::eMinCoordinateDimension)
        {
            const int nMaxDim =
                bHasM ? GeoJSONObject::eMaxCoordinateDimensionJSONFG
                      : GeoJSONObject::eMaxCoordinateDimensionGeoJSON;
            if (nSize > nMaxDim)
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "OGRGeoJSONReadRawPoint(): too many members in "
                             "array '%s': %d. At most %d are handled. Ignoring "
                             "extra members.",
                             json_object_to_json_string(poObj), nSize, nMaxDim);
            }
            // Don't *expect* mixed-dimension geometries, although the
            // spec doesn't explicitly forbid this.
            if (nSize == 4 || (nSize == 3 && !bHasM))
            {
                const double dfZ =
                    OGRGeoJSONGetCoordinate(poObj, "z", 2, bValid);
                point.setZ(dfZ);
            }

            if (bHasM)
            {
                const double dfM =
                    OGRGeoJSONGetCoordinate(poObj, "m", nSize - 1, bValid);
                point.setM(dfM);
            }
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
/*                         OGRGeoJSONReadPoint                          */
/************************************************************************/

std::unique_ptr<OGRPoint> OGRGeoJSONReadPoint(json_object *poObj, bool bHasM)
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
    if (!OGRGeoJSONReadRawPoint(poObjCoords, *poPoint, bHasM))
    {
        return nullptr;
    }

    return poPoint;
}

/************************************************************************/
/*                       OGRGeoJSONReadMultiPoint                       */
/************************************************************************/

std::unique_ptr<OGRMultiPoint> OGRGeoJSONReadMultiPoint(json_object *poObj,
                                                        bool bHasM)
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
            if (!OGRGeoJSONReadRawPoint(poObjCoords, pt, bHasM))
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

    return poMultiPoint;
}

/************************************************************************/
/*                      OGRGeoJSONReadSimpleCurve                       */
/************************************************************************/

template <class T>
static std::unique_ptr<T> OGRGeoJSONReadSimpleCurve(const char *pszFuncName,
                                                    json_object *poObj,
                                                    bool bHasM, bool bRaw)
{
    if (!poObj)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s(): invalid LineString object. Got null.", pszFuncName);
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

    std::unique_ptr<T> poLine;

    if (json_type_array == json_object_get_type(poObjPoints))
    {
        const int nPoints =
            static_cast<int>(json_object_array_length(poObjPoints));

        poLine = std::make_unique<T>();
        poLine->setNumPoints(nPoints);

        for (int i = 0; i < nPoints; ++i)
        {
            json_object *poObjCoords =
                json_object_array_get_idx(poObjPoints, i);

            OGRPoint pt;
            if (!OGRGeoJSONReadRawPoint(poObjCoords, pt, bHasM))
            {
                return nullptr;
            }
            if (pt.Is3D())
                poLine->set3D(true);
            if (pt.IsMeasured())
                poLine->setMeasured(true);
            poLine->setPoint(i, &pt);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s(): invalid geometry. "
                 "Unexpected type %s for '%s'. Expected array.",
                 pszFuncName,
                 GetJSONConstructName(json_object_get_type(poObjPoints)),
                 json_object_to_json_string(poObjPoints));
    }

    return poLine;
}

/************************************************************************/
/*                       OGRGeoJSONReadLineString                       */
/************************************************************************/

std::unique_ptr<OGRLineString> OGRGeoJSONReadLineString(json_object *poObj,
                                                        bool bHasM, bool bRaw)
{
    return OGRGeoJSONReadSimpleCurve<OGRLineString>(__func__, poObj, bHasM,
                                                    bRaw);
}

/************************************************************************/
/*                     OGRGeoJSONReadCircularString                     */
/************************************************************************/

std::unique_ptr<OGRCircularString>
OGRGeoJSONReadCircularString(json_object *poObj, bool bHasM)
{
    return OGRGeoJSONReadSimpleCurve<OGRCircularString>(__func__, poObj, bHasM,
                                                        /* bRaw = */ false);
}

/************************************************************************/
/*                    OGRGeoJSONReadMultiLineString                     */
/************************************************************************/

std::unique_ptr<OGRMultiLineString>
OGRGeoJSONReadMultiLineString(json_object *poObj, bool bHasM)
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

            auto poLine =
                OGRGeoJSONReadLineString(poObjLine, bHasM, /* bRaw = */ true);
            if (poLine)
            {
                poMultiLine->addGeometry(std::move(poLine));
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

    return poMultiLine;
}

/************************************************************************/
/*                       OGRGeoJSONReadLinearRing                       */
/************************************************************************/

std::unique_ptr<OGRLinearRing> OGRGeoJSONReadLinearRing(json_object *poObj,
                                                        bool bHasM)
{
    return OGRGeoJSONReadSimpleCurve<OGRLinearRing>(__func__, poObj, bHasM,
                                                    /* bRaw = */ true);
}

/************************************************************************/
/*                        OGRGeoJSONReadPolygon                         */
/************************************************************************/

std::unique_ptr<OGRPolygon> OGRGeoJSONReadPolygon(json_object *poObj,
                                                  bool bHasM, bool bRaw)
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
                auto poRing = OGRGeoJSONReadLinearRing(poObjPoints, bHasM);
                if (poRing)
                {
                    poPolygon = std::make_unique<OGRPolygon>();
                    poPolygon->addRing(std::move(poRing));
                }
            }

            for (auto i = decltype(nRings){1};
                 i < nRings && nullptr != poPolygon; ++i)
            {
                poObjPoints = json_object_array_get_idx(poObjRings, i);
                if (poObjPoints)
                {
                    auto poRing = OGRGeoJSONReadLinearRing(poObjPoints, bHasM);
                    if (poRing)
                    {
                        poPolygon->addRing(std::move(poRing));
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

    return poPolygon;
}

/************************************************************************/
/*                      OGRGeoJSONReadMultiPolygon                      */
/************************************************************************/

std::unique_ptr<OGRMultiPolygon> OGRGeoJSONReadMultiPolygon(json_object *poObj,
                                                            bool bHasM)
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
        const int nPolys =
            static_cast<int>(json_object_array_length(poObjPolys));

        poMultiPoly = std::make_unique<OGRMultiPolygon>();

        for (int i = 0; i < nPolys; ++i)
        {
            json_object *poObjPoly = json_object_array_get_idx(poObjPolys, i);
            if (!poObjPoly)
            {
                poMultiPoly->addGeometryDirectly(
                    std::make_unique<OGRPolygon>().release());
            }
            else
            {
                auto poPoly =
                    OGRGeoJSONReadPolygon(poObjPoly, bHasM, /* bRaw = */ true);
                if (poPoly)
                {
                    poMultiPoly->addGeometry(std::move(poPoly));
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

    return poMultiPoly;
}

/************************************************************************/
/*                       OGRGeoJSONReadCollection                       */
/************************************************************************/

template <class T>
static std::unique_ptr<T>
OGRGeoJSONReadCollection(const char *pszFuncName, const char *pszGeomTypeName,
                         json_object *poObj, bool bHasM,
                         const OGRSpatialReference *poSRS)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjGeoms = OGRGeoJSONFindMemberByName(poObj, "geometries");
    if (nullptr == poObjGeoms)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid %s object. "
                 "Missing \'geometries\' member.",
                 pszGeomTypeName);
        return nullptr;
    }

    std::unique_ptr<T> poCollection;

    if (json_type_array == json_object_get_type(poObjGeoms))
    {
        poCollection = std::make_unique<T>();
        poCollection->assignSpatialReference(poSRS);

        const int nGeoms =
            static_cast<int>(json_object_array_length(poObjGeoms));
        for (int i = 0; i < nGeoms; ++i)
        {
            json_object *poObjGeom = json_object_array_get_idx(poObjGeoms, i);
            if (!poObjGeom)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "%s(): skipping null "
                         "sub-geometry",
                         pszFuncName);
                continue;
            }

            auto poGeometry = OGRGeoJSONReadGeometry(poObjGeom, bHasM, poSRS);
            if (poGeometry)
            {
                if constexpr (std::is_same_v<T, OGRCompoundCurve>)
                {
                    auto eFlatType = wkbFlatten(poGeometry->getGeometryType());
                    if (eFlatType == wkbLineString ||
                        eFlatType == wkbCircularString)
                    {
                        if (poCollection->addCurve(std::unique_ptr<OGRCurve>(
                                poGeometry.release()->toCurve())) !=
                            OGRERR_NONE)
                            return nullptr;
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "%s(): member of a CompoundCurve is not a "
                                 "LineString or CircularString.",
                                 pszFuncName);
                        return nullptr;
                    }
                }
                else if constexpr (std::is_same_v<T, OGRCurvePolygon>)
                {
                    auto eFlatType = wkbFlatten(poGeometry->getGeometryType());
                    if (eFlatType == wkbLineString ||
                        eFlatType == wkbCircularString ||
                        eFlatType == wkbCompoundCurve)
                    {
                        if (poCollection->addRing(std::unique_ptr<OGRCurve>(
                                poGeometry.release()->toCurve())) !=
                            OGRERR_NONE)
                            return nullptr;
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "%s(): member of a CurvePolygon is not a "
                                 "LineString, CircularString or CompoundCurve.",
                                 pszFuncName);
                        return nullptr;
                    }
                }
                else
                {
                    const auto eChildType = poGeometry->getGeometryType();
                    if (poCollection->addGeometry(std::move(poGeometry)) !=
                        OGRERR_NONE)
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "%s(): Invalid child geometry type (%s) for %s",
                            pszFuncName, OGRToOGCGeomType(eChildType),
                            pszGeomTypeName);
                        return nullptr;
                    }
                }
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "%s(): unexpected type of JSON "
                 "construct %s for '%s'. Expected array.",
                 pszFuncName,
                 GetJSONConstructName(json_object_get_type(poObjGeoms)),
                 json_object_to_json_string(poObjGeoms));
    }

    return poCollection;
}

/************************************************************************/
/*                   OGRGeoJSONReadGeometryCollection                   */
/************************************************************************/

std::unique_ptr<OGRGeometryCollection>
OGRGeoJSONReadGeometryCollection(json_object *poObj, bool bHasM,
                                 const OGRSpatialReference *poSRS)
{
    return OGRGeoJSONReadCollection<OGRGeometryCollection>(
        __func__, "GeometryCollection", poObj, bHasM, poSRS);
}

/************************************************************************/
/*                     OGRGeoJSONReadCompoundCurve                      */
/************************************************************************/

std::unique_ptr<OGRCompoundCurve>
OGRGeoJSONReadCompoundCurve(json_object *poObj, bool bHasM,
                            const OGRSpatialReference *poSRS)
{
    return OGRGeoJSONReadCollection<OGRCompoundCurve>(__func__, "CompoundCurve",
                                                      poObj, bHasM, poSRS);
}

/************************************************************************/
/*                      OGRGeoJSONReadCurvePolygon                      */
/************************************************************************/

std::unique_ptr<OGRCurvePolygon> OGRGeoJSONReadCurvePolygon(json_object *poObj,
                                                            bool bHasM)
{
    return OGRGeoJSONReadCollection<OGRCurvePolygon>(
        __func__, "CurvePolygon", poObj, bHasM, /* poSRS = */ nullptr);
}

/************************************************************************/
/*                       OGRGeoJSONReadMultiCurve                       */
/************************************************************************/

std::unique_ptr<OGRMultiCurve>
OGRGeoJSONReadMultiCurve(json_object *poObj, bool bHasM,
                         const OGRSpatialReference *poSRS)
{
    return OGRGeoJSONReadCollection<OGRMultiCurve>(__func__, "MultiCurve",
                                                   poObj, bHasM, poSRS);
}

/************************************************************************/
/*                      OGRGeoJSONReadMultiSurface                      */
/************************************************************************/

std::unique_ptr<OGRMultiSurface>
OGRGeoJSONReadMultiSurface(json_object *poObj, bool bHasM,
                           const OGRSpatialReference *poSRS)
{
    return OGRGeoJSONReadCollection<OGRMultiSurface>(__func__, "MultiSurface",
                                                     poObj, bHasM, poSRS);
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
/*                     OGR_G_CreateGeometryFromJson                     */
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

    OGRGeometry *poGeometry =
        OGRGeoJSONReadGeometry(poObj, /* bHasM = */ false,
                               /* OGRSpatialReference* = */ nullptr)
            .release();

    // Release JSON tree.
    json_object_put(poObj);

    return OGRGeometry::ToHandle(poGeometry);
}

/*! @endcond */

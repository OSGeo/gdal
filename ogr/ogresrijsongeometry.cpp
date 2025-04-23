// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

/*! @cond Doxygen_Suppress */

#include "ogresrijsongeometry.h"

#include "ogrlibjsonutils.h"

#include "ogr_geometry.h"
#include "ogr_spatialref.h"

static OGRPoint *OGRESRIJSONReadPoint(json_object *poObj);
static OGRGeometry *OGRESRIJSONReadLineString(json_object *poObj);
static OGRGeometry *OGRESRIJSONReadPolygon(json_object *poObj);
static OGRMultiPoint *OGRESRIJSONReadMultiPoint(json_object *poObj);

/************************************************************************/
/*                       OGRESRIJSONReadGeometry()                      */
/************************************************************************/

OGRGeometry *OGRESRIJSONReadGeometry(json_object *poObj)
{
    OGRGeometry *poGeometry = nullptr;

    if (OGRGeoJSONFindMemberByName(poObj, "x"))
        poGeometry = OGRESRIJSONReadPoint(poObj);
    else if (OGRGeoJSONFindMemberByName(poObj, "paths"))
        poGeometry = OGRESRIJSONReadLineString(poObj);
    else if (OGRGeoJSONFindMemberByName(poObj, "rings"))
        poGeometry = OGRESRIJSONReadPolygon(poObj);
    else if (OGRGeoJSONFindMemberByName(poObj, "points"))
        poGeometry = OGRESRIJSONReadMultiPoint(poObj);

    return poGeometry;
}

/************************************************************************/
/*                     OGR_G_CreateGeometryFromEsriJson()               */
/************************************************************************/

/** Create a OGR geometry from a ESRIJson geometry object */
OGRGeometryH OGR_G_CreateGeometryFromEsriJson(const char *pszJson)
{
    if (nullptr == pszJson)
    {
        // Translation failed.
        return nullptr;
    }

    json_object *poObj = nullptr;
    if (!OGRJSonParse(pszJson, &poObj))
        return nullptr;

    OGRGeometry *poGeometry = OGRESRIJSONReadGeometry(poObj);

    // Release JSON tree.
    json_object_put(poObj);

    return OGRGeometry::ToHandle(poGeometry);
}

/************************************************************************/
/*                        OGRESRIJSONGetType()                          */
/************************************************************************/

OGRwkbGeometryType OGRESRIJSONGetGeometryType(json_object *poObj)
{
    if (nullptr == poObj)
        return wkbUnknown;

    json_object *poObjType = OGRGeoJSONFindMemberByName(poObj, "geometryType");
    if (nullptr == poObjType)
    {
        return wkbNone;
    }

    const char *name = json_object_get_string(poObjType);
    if (EQUAL(name, "esriGeometryPoint"))
        return wkbPoint;
    else if (EQUAL(name, "esriGeometryPolyline"))
        return wkbLineString;
    else if (EQUAL(name, "esriGeometryPolygon"))
        return wkbPolygon;
    else if (EQUAL(name, "esriGeometryMultiPoint"))
        return wkbMultiPoint;
    else
        return wkbUnknown;
}

/************************************************************************/
/*                     OGRESRIJSONGetCoordinateToDouble()               */
/************************************************************************/

static double OGRESRIJSONGetCoordinateToDouble(json_object *poObjCoord,
                                               const char *pszCoordName,
                                               bool &bValid)
{
    const int iType = json_object_get_type(poObjCoord);
    if (json_type_double != iType && json_type_int != iType)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid '%s' coordinate. "
                 "Type is not double or integer for \'%s\'.",
                 pszCoordName, json_object_to_json_string(poObjCoord));
        bValid = false;
        return 0.0;
    }

    return json_object_get_double(poObjCoord);
}

/************************************************************************/
/*                       OGRESRIJSONGetCoordinate()                     */
/************************************************************************/

static double OGRESRIJSONGetCoordinate(json_object *poObj,
                                       const char *pszCoordName, bool &bValid)
{
    json_object *poObjCoord = OGRGeoJSONFindMemberByName(poObj, pszCoordName);
    if (nullptr == poObjCoord)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid Point object. "
                 "Missing '%s' member.",
                 pszCoordName);
        bValid = false;
        return 0.0;
    }

    return OGRESRIJSONGetCoordinateToDouble(poObjCoord, pszCoordName, bValid);
}

/************************************************************************/
/*                          OGRESRIJSONReadPoint()                      */
/************************************************************************/

OGRPoint *OGRESRIJSONReadPoint(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    bool bValid = true;
    const double dfX = OGRESRIJSONGetCoordinate(poObj, "x", bValid);
    const double dfY = OGRESRIJSONGetCoordinate(poObj, "y", bValid);
    if (!bValid)
        return nullptr;

    json_object *poObjZ = OGRGeoJSONFindMemberByName(poObj, "z");
    if (nullptr == poObjZ)
        return new OGRPoint(dfX, dfY);

    const double dfZ = OGRESRIJSONGetCoordinateToDouble(poObjZ, "z", bValid);
    if (!bValid)
        return nullptr;
    return new OGRPoint(dfX, dfY, dfZ);
}

/************************************************************************/
/*                     OGRESRIJSONReaderParseZM()                      */
/************************************************************************/

static void OGRESRIJSONReaderParseZM(json_object *poObj, bool *bHasZ,
                                     bool *bHasM)
{
    CPLAssert(nullptr != poObj);
    // The ESRI geojson spec states that geometries other than point can
    // have the attributes hasZ and hasM.  A geometry that has a z value
    // implies the 3rd number in the tuple is z.  if hasM is true, but hasZ
    // is not, it is the M value.
    bool bZ = false;
    json_object *poObjHasZ = OGRGeoJSONFindMemberByName(poObj, "hasZ");
    if (poObjHasZ != nullptr)
    {
        if (json_object_get_type(poObjHasZ) == json_type_boolean)
        {
            bZ = CPL_TO_BOOL(json_object_get_boolean(poObjHasZ));
        }
    }

    bool bM = false;
    json_object *poObjHasM = OGRGeoJSONFindMemberByName(poObj, "hasM");
    if (poObjHasM != nullptr)
    {
        if (json_object_get_type(poObjHasM) == json_type_boolean)
        {
            bM = CPL_TO_BOOL(json_object_get_boolean(poObjHasM));
        }
    }
    if (bHasZ != nullptr)
        *bHasZ = bZ;
    if (bHasM != nullptr)
        *bHasM = bM;
}

/************************************************************************/
/*                     OGRESRIJSONReaderParseXYZMArray()                  */
/************************************************************************/

static bool OGRESRIJSONReaderParseXYZMArray(json_object *poObjCoords,
                                            bool /*bHasZ*/, bool bHasM,
                                            double *pdfX, double *pdfY,
                                            double *pdfZ, double *pdfM,
                                            int *pnNumCoords)
{
    if (poObjCoords == nullptr)
    {
        CPLDebug("ESRIJSON",
                 "OGRESRIJSONReaderParseXYZMArray: got null object.");
        return false;
    }

    if (json_type_array != json_object_get_type(poObjCoords))
    {
        CPLDebug("ESRIJSON",
                 "OGRESRIJSONReaderParseXYZMArray: got non-array object.");
        return false;
    }

    const auto coordDimension = json_object_array_length(poObjCoords);

    // Allow 4 coordinates if M is present, but it is eventually ignored.
    if (coordDimension < 2 || coordDimension > 4)
    {
        CPLDebug("ESRIJSON",
                 "OGRESRIJSONReaderParseXYZMArray: got an unexpected "
                 "array object.");
        return false;
    }

    // Read X coordinate.
    json_object *poObjCoord = json_object_array_get_idx(poObjCoords, 0);
    if (poObjCoord == nullptr)
    {
        CPLDebug("ESRIJSON",
                 "OGRESRIJSONReaderParseXYZMArray: got null object.");
        return false;
    }

    bool bValid = true;
    const double dfX =
        OGRESRIJSONGetCoordinateToDouble(poObjCoord, "x", bValid);

    // Read Y coordinate.
    poObjCoord = json_object_array_get_idx(poObjCoords, 1);
    if (poObjCoord == nullptr)
    {
        CPLDebug("ESRIJSON",
                 "OGRESRIJSONReaderParseXYZMArray: got null object.");
        return false;
    }

    const double dfY =
        OGRESRIJSONGetCoordinateToDouble(poObjCoord, "y", bValid);
    if (!bValid)
        return false;

    // Read Z or M or Z and M coordinates.
    if (coordDimension > 2)
    {
        poObjCoord = json_object_array_get_idx(poObjCoords, 2);
        if (poObjCoord == nullptr)
        {
            CPLDebug("ESRIJSON",
                     "OGRESRIJSONReaderParseXYZMArray: got null object.");
            return false;
        }

        const double dfZorM = OGRESRIJSONGetCoordinateToDouble(
            poObjCoord, (coordDimension > 3 || !bHasM) ? "z" : "m", bValid);
        if (!bValid)
            return false;
        if (pdfZ != nullptr)
        {
            if (coordDimension > 3 || !bHasM)
                *pdfZ = dfZorM;
            else
                *pdfZ = 0.0;
        }
        if (pdfM != nullptr && coordDimension == 3)
        {
            if (bHasM)
                *pdfM = dfZorM;
            else
                *pdfM = 0.0;
        }
        if (coordDimension == 4)
        {
            poObjCoord = json_object_array_get_idx(poObjCoords, 3);
            if (poObjCoord == nullptr)
            {
                CPLDebug("ESRIJSON",
                         "OGRESRIJSONReaderParseXYZMArray: got null object.");
                return false;
            }

            const double dfM =
                OGRESRIJSONGetCoordinateToDouble(poObjCoord, "m", bValid);
            if (!bValid)
                return false;
            if (pdfM != nullptr)
                *pdfM = dfM;
        }
    }
    else
    {
        if (pdfZ != nullptr)
            *pdfZ = 0.0;
        if (pdfM != nullptr)
            *pdfM = 0.0;
    }

    if (pnNumCoords != nullptr)
        *pnNumCoords = static_cast<int>(coordDimension);
    if (pdfX != nullptr)
        *pdfX = dfX;
    if (pdfY != nullptr)
        *pdfY = dfY;

    return true;
}

/************************************************************************/
/*                        OGRESRIJSONReadLineString()                   */
/************************************************************************/

OGRGeometry *OGRESRIJSONReadLineString(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    bool bHasZ = false;
    bool bHasM = false;

    OGRESRIJSONReaderParseZM(poObj, &bHasZ, &bHasM);

    json_object *poObjPaths = OGRGeoJSONFindMemberByName(poObj, "paths");
    if (nullptr == poObjPaths)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid LineString object. "
                 "Missing \'paths\' member.");
        return nullptr;
    }

    if (json_type_array != json_object_get_type(poObjPaths))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid LineString object. "
                 "Invalid \'paths\' member.");
        return nullptr;
    }

    OGRMultiLineString *poMLS = nullptr;
    OGRGeometry *poRet = nullptr;
    const auto nPaths = json_object_array_length(poObjPaths);
    for (auto iPath = decltype(nPaths){0}; iPath < nPaths; iPath++)
    {
        json_object *poObjPath = json_object_array_get_idx(poObjPaths, iPath);
        if (poObjPath == nullptr ||
            json_type_array != json_object_get_type(poObjPath))
        {
            delete poRet;
            CPLDebug("ESRIJSON", "LineString: got non-array object.");
            return nullptr;
        }

        OGRLineString *poLine = new OGRLineString();
        if (nPaths > 1)
        {
            if (iPath == 0)
            {
                poMLS = new OGRMultiLineString();
                poRet = poMLS;
            }
            poMLS->addGeometryDirectly(poLine);
        }
        else
        {
            poRet = poLine;
        }
        const auto nPoints = json_object_array_length(poObjPath);
        for (auto i = decltype(nPoints){0}; i < nPoints; i++)
        {
            int nNumCoords = 2;
            json_object *poObjCoords = json_object_array_get_idx(poObjPath, i);
            double dfX = 0.0;
            double dfY = 0.0;
            double dfZ = 0.0;
            double dfM = 0.0;
            if (!OGRESRIJSONReaderParseXYZMArray(poObjCoords, bHasZ, bHasM,
                                                 &dfX, &dfY, &dfZ, &dfM,
                                                 &nNumCoords))
            {
                delete poRet;
                return nullptr;
            }

            if (nNumCoords == 3 && !bHasM)
            {
                poLine->addPoint(dfX, dfY, dfZ);
            }
            else if (nNumCoords == 3)
            {
                poLine->addPointM(dfX, dfY, dfM);
            }
            else if (nNumCoords == 4)
            {
                poLine->addPoint(dfX, dfY, dfZ, dfM);
            }
            else
            {
                poLine->addPoint(dfX, dfY);
            }
        }
    }

    if (poRet == nullptr)
        poRet = new OGRLineString();

    return poRet;
}

/************************************************************************/
/*                          OGRESRIJSONReadPolygon()                    */
/************************************************************************/

OGRGeometry *OGRESRIJSONReadPolygon(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    bool bHasZ = false;
    bool bHasM = false;

    OGRESRIJSONReaderParseZM(poObj, &bHasZ, &bHasM);

    json_object *poObjRings = OGRGeoJSONFindMemberByName(poObj, "rings");
    if (nullptr == poObjRings)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid Polygon object. "
                 "Missing \'rings\' member.");
        return nullptr;
    }

    if (json_type_array != json_object_get_type(poObjRings))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid Polygon object. "
                 "Invalid \'rings\' member.");
        return nullptr;
    }

    const auto nRings = json_object_array_length(poObjRings);
    OGRGeometry **papoGeoms = new OGRGeometry *[nRings];
    for (auto iRing = decltype(nRings){0}; iRing < nRings; iRing++)
    {
        json_object *poObjRing = json_object_array_get_idx(poObjRings, iRing);
        if (poObjRing == nullptr ||
            json_type_array != json_object_get_type(poObjRing))
        {
            for (auto j = decltype(iRing){0}; j < iRing; j++)
                delete papoGeoms[j];
            delete[] papoGeoms;
            CPLDebug("ESRIJSON", "Polygon: got non-array object.");
            return nullptr;
        }

        OGRPolygon *poPoly = new OGRPolygon();
        auto poLine = std::make_unique<OGRLinearRing>();
        papoGeoms[iRing] = poPoly;

        const auto nPoints = json_object_array_length(poObjRing);
        for (auto i = decltype(nPoints){0}; i < nPoints; i++)
        {
            int nNumCoords = 2;
            json_object *poObjCoords = json_object_array_get_idx(poObjRing, i);
            double dfX = 0.0;
            double dfY = 0.0;
            double dfZ = 0.0;
            double dfM = 0.0;
            if (!OGRESRIJSONReaderParseXYZMArray(poObjCoords, bHasZ, bHasM,
                                                 &dfX, &dfY, &dfZ, &dfM,
                                                 &nNumCoords))
            {
                for (auto j = decltype(iRing){0}; j <= iRing; j++)
                    delete papoGeoms[j];
                delete[] papoGeoms;
                return nullptr;
            }

            if (nNumCoords == 3 && !bHasM)
            {
                poLine->addPoint(dfX, dfY, dfZ);
            }
            else if (nNumCoords == 3)
            {
                poLine->addPointM(dfX, dfY, dfM);
            }
            else if (nNumCoords == 4)
            {
                poLine->addPoint(dfX, dfY, dfZ, dfM);
            }
            else
            {
                poLine->addPoint(dfX, dfY);
            }
        }
        poPoly->addRingDirectly(poLine.release());
    }

    OGRGeometry *poRet = OGRGeometryFactory::organizePolygons(
        papoGeoms, static_cast<int>(nRings), nullptr, nullptr);
    delete[] papoGeoms;

    return poRet;
}

/************************************************************************/
/*                        OGRESRIJSONReadMultiPoint()                   */
/************************************************************************/

OGRMultiPoint *OGRESRIJSONReadMultiPoint(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    bool bHasZ = false;
    bool bHasM = false;

    OGRESRIJSONReaderParseZM(poObj, &bHasZ, &bHasM);

    json_object *poObjPoints = OGRGeoJSONFindMemberByName(poObj, "points");
    if (nullptr == poObjPoints)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiPoint object. "
                 "Missing \'points\' member.");
        return nullptr;
    }

    if (json_type_array != json_object_get_type(poObjPoints))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiPoint object. "
                 "Invalid \'points\' member.");
        return nullptr;
    }

    OGRMultiPoint *poMulti = new OGRMultiPoint();

    const auto nPoints = json_object_array_length(poObjPoints);
    for (auto i = decltype(nPoints){0}; i < nPoints; i++)
    {
        int nNumCoords = 2;
        json_object *poObjCoords = json_object_array_get_idx(poObjPoints, i);
        double dfX = 0.0;
        double dfY = 0.0;
        double dfZ = 0.0;
        double dfM = 0.0;
        if (!OGRESRIJSONReaderParseXYZMArray(poObjCoords, bHasZ, bHasM, &dfX,
                                             &dfY, &dfZ, &dfM, &nNumCoords))
        {
            delete poMulti;
            return nullptr;
        }

        if (nNumCoords == 3 && !bHasM)
        {
            poMulti->addGeometryDirectly(new OGRPoint(dfX, dfY, dfZ));
        }
        else if (nNumCoords == 3)
        {
            OGRPoint *poPoint = new OGRPoint(dfX, dfY);
            poPoint->setM(dfM);
            poMulti->addGeometryDirectly(poPoint);
        }
        else if (nNumCoords == 4)
        {
            poMulti->addGeometryDirectly(new OGRPoint(dfX, dfY, dfZ, dfM));
        }
        else
        {
            poMulti->addGeometryDirectly(new OGRPoint(dfX, dfY));
        }
    }

    return poMulti;
}

/************************************************************************/
/*                    OGRESRIJSONReadSpatialReference()                 */
/************************************************************************/

OGRSpatialReference *OGRESRIJSONReadSpatialReference(json_object *poObj)
{
    /* -------------------------------------------------------------------- */
    /*      Read spatial reference definition.                              */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = nullptr;

    json_object *poObjSrs =
        OGRGeoJSONFindMemberByName(poObj, "spatialReference");
    if (nullptr != poObjSrs)
    {
        json_object *poObjWkid =
            OGRGeoJSONFindMemberByName(poObjSrs, "latestWkid");
        if (poObjWkid == nullptr)
            poObjWkid = OGRGeoJSONFindMemberByName(poObjSrs, "wkid");
        if (poObjWkid == nullptr)
        {
            json_object *poObjWkt = OGRGeoJSONFindMemberByName(poObjSrs, "wkt");
            if (poObjWkt == nullptr)
                return nullptr;

            const char *pszWKT = json_object_get_string(poObjWkt);
            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE != poSRS->importFromWkt(pszWKT))
            {
                delete poSRS;
                poSRS = nullptr;
            }
            else
            {
                auto poSRSMatch = poSRS->FindBestMatch(70);
                if (poSRSMatch)
                {
                    poSRS->Release();
                    poSRS = poSRSMatch;
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                }
            }

            return poSRS;
        }

        const int nEPSG = json_object_get_int(poObjWkid);

        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (OGRERR_NONE != poSRS->importFromEPSG(nEPSG))
        {
            delete poSRS;
            poSRS = nullptr;
        }
    }

    return poSRS;
}

/*! @endcond */

/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "libkml_headers.h"

#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogrlibkmlgeometry.h"

using kmlbase::Vec3;
using kmldom::CoordinatesPtr;
using kmldom::ElementPtr;
using kmldom::GeometryPtr;
using kmldom::GxLatLonQuadPtr;
using kmldom::GxMultiTrackPtr;
using kmldom::GxTrackPtr;
using kmldom::InnerBoundaryIsPtr;
using kmldom::KmlFactory;
using kmldom::LatLonBoxPtr;
using kmldom::LinearRingPtr;
using kmldom::LineStringPtr;
using kmldom::MultiGeometryPtr;
using kmldom::OuterBoundaryIsPtr;
using kmldom::PointPtr;
using kmldom::PolygonPtr;

static bool NormalizeLongLat(double &x, double &y)
{
    if (x >= -180 && x <= 180)
    {
        // nominal
    }
    else if (x > 180 && x <= 180 + 360)
        x -= 360;
    else if (x < -180 && x >= -180 - 360)
        x += 360;
    else
    {
        const bool bStrictCompliance =
            CPLTestBool(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE"));
        CPLError(bStrictCompliance ? CE_Failure : CE_Warning, CPLE_AppDefined,
                 "Invalid longitude %g", x);
        if (bStrictCompliance)
            return false;
    }

    constexpr double EPSILON = 1e-8;
    if (y >= -90 && y <= 90)
    {
        // nominal
    }
    else if (y > 90 && y < 90 + EPSILON)
    {
        y = 90;
    }
    else if (y < -90 && y > -90 - EPSILON)
    {
        y = -90;
    }
    else
    {
        const bool bStrictCompliance =
            CPLTestBool(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE"));
        CPLError(bStrictCompliance ? CE_Failure : CE_Warning, CPLE_AppDefined,
                 "Invalid latitude %g", y);
        if (bStrictCompliance)
            return false;
    }
    return true;
}

/******************************************************************************
 Function to write out a ogr geometry to kml.

Args:
          poOgrGeom     the ogr geometry
          extra         used in recursion, just pass -1
          poKmlFactory  pointer to the libkml dom factory

Returns:
          ElementPtr to the geometry created

******************************************************************************/

ElementPtr geom2kml(OGRGeometry *poOgrGeom, int extra, KmlFactory *poKmlFactory)
{
    if (!poOgrGeom)
    {
        return nullptr;
    }

    /***** libkml geom vars *****/
    CoordinatesPtr coordinates = nullptr;

    // This will be the return value.
    ElementPtr poKmlGeometry = nullptr;

    /***** Other vars *****/
    int numpoints = 0;
    const OGRwkbGeometryType type = poOgrGeom->getGeometryType();

    switch (type)
    {
        case wkbPoint:
        {
            const OGRPoint *poOgrPoint = poOgrGeom->toPoint();
            PointPtr poKmlPoint = nullptr;
            if (poOgrPoint->getCoordinateDimension() == 0)
            {
                poKmlPoint = poKmlFactory->CreatePoint();
                poKmlGeometry = poKmlPoint;
            }
            else
            {
                double x = poOgrPoint->getX();
                double y = poOgrPoint->getY();

                if (!NormalizeLongLat(x, y))
                    return nullptr;

                coordinates = poKmlFactory->CreateCoordinates();
                coordinates->add_latlng(y, x);
                poKmlPoint = poKmlFactory->CreatePoint();
                poKmlGeometry = poKmlPoint;
                poKmlPoint->set_coordinates(coordinates);
            }

            break;
        }
        case wkbPoint25D:
        {
            const OGRPoint *poOgrPoint = poOgrGeom->toPoint();

            double x = poOgrPoint->getX();
            double y = poOgrPoint->getY();
            const double z = poOgrPoint->getZ();

            if (!NormalizeLongLat(x, y))
                return nullptr;

            coordinates = poKmlFactory->CreateCoordinates();
            coordinates->add_latlngalt(y, x, z);
            PointPtr poKmlPoint = poKmlFactory->CreatePoint();
            poKmlGeometry = poKmlPoint;
            poKmlPoint->set_coordinates(coordinates);

            break;
        }
        case wkbLineString:
        {
            OGRLineString *poOgrLineString = poOgrGeom->toLineString();

            if (extra >= 0)
            {
                poOgrGeom->toLinearRing()->closeRings();
            }

            numpoints = poOgrLineString->getNumPoints();
            if (extra >= 0)
            {
                if (numpoints < 4 && CPLTestBool(CPLGetConfigOption(
                                         "LIBKML_STRICT_COMPLIANCE", "TRUE")))
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "A linearring should have at least 4 points");
                    return nullptr;
                }
            }
            else
            {
                if (numpoints < 2 && CPLTestBool(CPLGetConfigOption(
                                         "LIBKML_STRICT_COMPLIANCE", "TRUE")))
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "A linestring should have at least 2 points");
                    return nullptr;
                }
            }

            coordinates = poKmlFactory->CreateCoordinates();

            OGRPoint point;

            for (int i = 0; i < numpoints; i++)
            {
                poOgrLineString->getPoint(i, &point);

                double x = point.getX();
                double y = point.getY();

                if (!NormalizeLongLat(x, y))
                    return nullptr;

                coordinates->add_latlng(y, x);
            }

            /***** Check if its a wkbLinearRing *****/
            if (extra < 0)
            {
                LineStringPtr poKmlLineString =
                    poKmlFactory->CreateLineString();
                poKmlGeometry = poKmlLineString;
                poKmlLineString->set_coordinates(coordinates);

                break;
            }
            [[fallthrough]];
        }
            /***** fallthrough *****/

        case wkbLinearRing:  // This case is for readability only.
        {
            LinearRingPtr poKmlLinearRing = poKmlFactory->CreateLinearRing();
            poKmlLinearRing->set_coordinates(coordinates);

            if (!extra)
            {
                OuterBoundaryIsPtr poKmlOuterRing =
                    poKmlFactory->CreateOuterBoundaryIs();
                poKmlOuterRing->set_linearring(poKmlLinearRing);
                poKmlGeometry = poKmlOuterRing;
            }
            else
            {
                InnerBoundaryIsPtr poKmlInnerRing =
                    poKmlFactory->CreateInnerBoundaryIs();
                poKmlGeometry = poKmlInnerRing;
                poKmlInnerRing->set_linearring(poKmlLinearRing);
            }

            break;
        }
        case wkbLineString25D:
        {
            const OGRLineString *poOgrLineString = poOgrGeom->toLineString();

            if (extra >= 0)
            {
                poOgrGeom->toLinearRing()->closeRings();
            }

            numpoints = poOgrLineString->getNumPoints();
            if (extra >= 0)
            {
                if (numpoints < 4 && CPLTestBool(CPLGetConfigOption(
                                         "LIBKML_STRICT_COMPLIANCE", "TRUE")))
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "A linearring should have at least 4 points");
                    return nullptr;
                }
            }
            else
            {
                if (numpoints < 2 && CPLTestBool(CPLGetConfigOption(
                                         "LIBKML_STRICT_COMPLIANCE", "TRUE")))
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "A linestring should have at least 2 points");
                    return nullptr;
                }
            }

            coordinates = poKmlFactory->CreateCoordinates();
            OGRPoint point;

            for (int i = 0; i < numpoints; i++)
            {
                poOgrLineString->getPoint(i, &point);

                double x = point.getX();
                double y = point.getY();
                const double z = point.getZ();

                if (!NormalizeLongLat(x, y))
                    return nullptr;

                coordinates->add_latlngalt(y, x, z);
            }

            /***** Check if its a wkbLinearRing *****/
            if (extra < 0)
            {
                LineStringPtr poKmlLineString =
                    poKmlFactory->CreateLineString();
                poKmlGeometry = poKmlLineString;
                poKmlLineString->set_coordinates(coordinates);

                break;
            }
            /***** fallthrough *****/

            // case wkbLinearRing25D: // This case is for readability only.

            LinearRingPtr poKmlLinearRing = poKmlFactory->CreateLinearRing();
            poKmlLinearRing->set_coordinates(coordinates);

            if (!extra)
            {
                OuterBoundaryIsPtr poKmlOuterRing =
                    poKmlFactory->CreateOuterBoundaryIs();
                poKmlGeometry = poKmlOuterRing;
                poKmlOuterRing->set_linearring(poKmlLinearRing);
            }
            else
            {
                InnerBoundaryIsPtr poKmlInnerRing =
                    poKmlFactory->CreateInnerBoundaryIs();
                poKmlGeometry = poKmlInnerRing;
                poKmlInnerRing->set_linearring(poKmlLinearRing);
            }

            break;
        }
        case wkbPolygon:
        {
            if (CPLTestBool(
                    CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) &&
                OGRGeometryFactory::haveGEOS())
            {
                bool bError;
                {
                    CPLErrorStateBackuper oErrorStateBackuper;
                    bError = !poOgrGeom->IsValid();
                }
                if (bError)
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Invalid polygon");
                    return nullptr;
                }
            }

            PolygonPtr poKmlPolygon = poKmlFactory->CreatePolygon();
            poKmlGeometry = poKmlPolygon;

            OGRPolygon *poOgrPolygon = poOgrGeom->toPolygon();
            ElementPtr poKmlTmpGeometry =
                geom2kml(poOgrPolygon->getExteriorRing(), 0, poKmlFactory);
            if (!poKmlTmpGeometry)
                return nullptr;
            poKmlPolygon->set_outerboundaryis(
                AsOuterBoundaryIs(poKmlTmpGeometry));

            const int nGeom = poOgrPolygon->getNumInteriorRings();
            for (int i = 0; i < nGeom; i++)
            {
                poKmlTmpGeometry = geom2kml(poOgrPolygon->getInteriorRing(i),
                                            i + 1, poKmlFactory);
                if (!poKmlTmpGeometry)
                    return nullptr;
                poKmlPolygon->add_innerboundaryis(
                    AsInnerBoundaryIs(poKmlTmpGeometry));
            }

            break;
        }
        case wkbPolygon25D:
        {
            if (CPLTestBool(
                    CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) &&
                OGRGeometryFactory::haveGEOS())
            {
                bool bError;
                {
                    CPLErrorStateBackuper oErrorStateBackuper;
                    bError = !poOgrGeom->IsValid();
                }
                if (bError)
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Invalid polygon");
                    return nullptr;
                }
            }

            PolygonPtr poKmlPolygon = poKmlFactory->CreatePolygon();
            poKmlGeometry = poKmlPolygon;

            OGRPolygon *poOgrPolygon = poOgrGeom->toPolygon();
            ElementPtr poKmlTmpGeometry =
                geom2kml(poOgrPolygon->getExteriorRing(), 0, poKmlFactory);
            if (!poKmlTmpGeometry)
                return nullptr;
            poKmlPolygon->set_outerboundaryis(
                AsOuterBoundaryIs(poKmlTmpGeometry));

            const int nGeom = poOgrPolygon->getNumInteriorRings();
            for (int i = 0; i < nGeom; i++)
            {
                poKmlTmpGeometry = geom2kml(poOgrPolygon->getInteriorRing(i),
                                            i + 1, poKmlFactory);
                if (!poKmlTmpGeometry)
                    return nullptr;
                poKmlPolygon->add_innerboundaryis(
                    AsInnerBoundaryIs(poKmlTmpGeometry));
            }

            break;
        }
        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        case wkbMultiPoint25D:
        case wkbMultiLineString25D:
        case wkbMultiPolygon25D:
        case wkbGeometryCollection25D:
        {
            OGRGeometryCollection *poOgrMultiGeom =
                poOgrGeom->toGeometryCollection();

            const int nGeom = poOgrMultiGeom->getNumGeometries();

            if (nGeom == 1 && CPLTestBool(CPLGetConfigOption(
                                  "LIBKML_STRICT_COMPLIANCE", "TRUE")))
            {
                CPLDebug("LIBKML",
                         "Turning multiple geometry into single geometry");
                poKmlGeometry = geom2kml(poOgrMultiGeom->getGeometryRef(0), -1,
                                         poKmlFactory);
                if (!poKmlGeometry)
                    return nullptr;
            }
            else
            {
                if (nGeom == 0 && CPLTestBool(CPLGetConfigOption(
                                      "LIBKML_STRICT_COMPLIANCE", "TRUE")))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Empty multi geometry are not recommended");
                }

                MultiGeometryPtr poKmlMultiGeometry =
                    poKmlFactory->CreateMultiGeometry();
                poKmlGeometry = poKmlMultiGeometry;

                for (int i = 0; i < nGeom; i++)
                {
                    ElementPtr poKmlTmpGeometry = geom2kml(
                        poOgrMultiGeom->getGeometryRef(i), -1, poKmlFactory);
                    if (!poKmlTmpGeometry)
                        return nullptr;
                    poKmlMultiGeometry->add_geometry(
                        AsGeometry(std::move(poKmlTmpGeometry)));
                }
            }

            break;
        }
        case wkbUnknown:
        case wkbNone:
        default:
            break;
    }

    return poKmlGeometry;
}

/******************************************************************************
 Recursive function to read a kml geometry and translate to ogr.

Args:
            poKmlGeometry   pointer to the kml geometry to translate
            poOgrSRS        pointer to the spatial ref to set on the geometry

Returns:
            pointer to the new ogr geometry object

******************************************************************************/

static OGRGeometry *kml2geom_rec(const GeometryPtr &poKmlGeometry,
                                 OGRSpatialReference *poOgrSRS)
{
    /***** ogr geom vars *****/
    OGRPoint *poOgrPoint = nullptr;
    OGRLineString *poOgrLineString = nullptr;
    OGRLinearRing *poOgrLinearRing = nullptr;
    OGRPolygon *poOgrPolygon = nullptr;
    OGRGeometryCollection *poOgrMultiGeometry = nullptr;
    OGRGeometry *poOgrGeometry = nullptr;
    OGRGeometry *poOgrTmpGeometry = nullptr;

    switch (poKmlGeometry->Type())
    {
        case kmldom::Type_Point:
        {
            PointPtr poKmlPoint = AsPoint(poKmlGeometry);
            if (poKmlPoint->has_coordinates())
            {
                CoordinatesPtr poKmlCoordinates = poKmlPoint->get_coordinates();
                const size_t nCoords =
                    poKmlCoordinates->get_coordinates_array_size();
                if (nCoords > 0)
                {
                    const Vec3 oKmlVec =
                        poKmlCoordinates->get_coordinates_array_at(0);

                    if (oKmlVec.has_altitude())
                        poOgrPoint = new OGRPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude(),
                                                  oKmlVec.get_altitude());
                    else
                        poOgrPoint = new OGRPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude());

                    poOgrGeometry = poOgrPoint;
                }
                else
                {
                    poOgrGeometry = new OGRPoint();
                }
            }
            else
            {
                poOgrGeometry = new OGRPoint();
            }

            break;
        }
        case kmldom::Type_LineString:
        {
            LineStringPtr poKmlLineString = AsLineString(poKmlGeometry);
            poOgrLineString = new OGRLineString();
            if (poKmlLineString->has_coordinates())
            {
                CoordinatesPtr poKmlCoordinates =
                    poKmlLineString->get_coordinates();

                const size_t nCoords =
                    poKmlCoordinates->get_coordinates_array_size();
                for (size_t i = 0; i < nCoords; i++)
                {
                    const Vec3 oKmlVec =
                        poKmlCoordinates->get_coordinates_array_at(i);
                    if (oKmlVec.has_altitude())
                        poOgrLineString->addPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude(),
                                                  oKmlVec.get_altitude());
                    else
                        poOgrLineString->addPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude());
                }
            }
            poOgrGeometry = poOgrLineString;

            break;
        }
        case kmldom::Type_LinearRing:
        {
            LinearRingPtr poKmlLinearRing = AsLinearRing(poKmlGeometry);
            poOgrLinearRing = new OGRLinearRing();
            if (poKmlLinearRing->has_coordinates())
            {
                CoordinatesPtr poKmlCoordinates =
                    poKmlLinearRing->get_coordinates();

                const size_t nCoords =
                    poKmlCoordinates->get_coordinates_array_size();
                for (size_t i = 0; i < nCoords; i++)
                {
                    const Vec3 oKmlVec =
                        poKmlCoordinates->get_coordinates_array_at(i);
                    if (oKmlVec.has_altitude())
                        poOgrLinearRing->addPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude(),
                                                  oKmlVec.get_altitude());
                    else
                        poOgrLinearRing->addPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude());
                }
            }
            poOgrGeometry = poOgrLinearRing;

            break;
        }
        case kmldom::Type_Polygon:
        {
            PolygonPtr poKmlPolygon = AsPolygon(poKmlGeometry);

            poOgrPolygon = new OGRPolygon();
            if (poKmlPolygon->has_outerboundaryis())
            {
                OuterBoundaryIsPtr poKmlOuterRing =
                    poKmlPolygon->get_outerboundaryis();
                LinearRingPtr poKmlLinearRing =
                    poKmlOuterRing->get_linearring();
                if (poKmlLinearRing)
                {
                    poOgrTmpGeometry = kml2geom_rec(poKmlLinearRing, poOgrSRS);

                    poOgrPolygon->addRingDirectly(
                        (OGRLinearRing *)poOgrTmpGeometry);
                }
            }
            const size_t nRings =
                poKmlPolygon->get_innerboundaryis_array_size();
            for (size_t i = 0; i < nRings; i++)
            {
                InnerBoundaryIsPtr poKmlInnerRing =
                    poKmlPolygon->get_innerboundaryis_array_at(i);
                LinearRingPtr poKmlLinearRing =
                    poKmlInnerRing->get_linearring();
                if (poKmlLinearRing)
                {
                    poOgrTmpGeometry = kml2geom_rec(poKmlLinearRing, poOgrSRS);

                    poOgrPolygon->addRingDirectly(
                        (OGRLinearRing *)poOgrTmpGeometry);
                }
            }
            poOgrGeometry = poOgrPolygon;

            break;
        }
        case kmldom::Type_MultiGeometry:
        {
            MultiGeometryPtr poKmlMultiGeometry =
                AsMultiGeometry(poKmlGeometry);
            const size_t nGeom = poKmlMultiGeometry->get_geometry_array_size();

            // Detect subgeometry type to instantiate appropriate
            // multi geometry type.
            kmldom::KmlDomType type = kmldom::Type_Unknown;
            for (size_t i = 0; i < nGeom; i++)
            {
                GeometryPtr poKmlTmpGeometry =
                    poKmlMultiGeometry->get_geometry_array_at(i);
                if (type == kmldom::Type_Unknown)
                {
                    type = poKmlTmpGeometry->Type();
                }
                else if (type != poKmlTmpGeometry->Type())
                {
                    type = kmldom::Type_Unknown;
                    break;
                }
            }

            if (type == kmldom::Type_Point)
                poOgrMultiGeometry = new OGRMultiPoint();
            else if (type == kmldom::Type_LineString)
                poOgrMultiGeometry = new OGRMultiLineString();
            else if (type == kmldom::Type_Polygon)
                poOgrMultiGeometry = new OGRMultiPolygon();
            else
                poOgrMultiGeometry = new OGRGeometryCollection();

            for (size_t i = 0; i < nGeom; i++)
            {
                GeometryPtr poKmlTmpGeometry =
                    poKmlMultiGeometry->get_geometry_array_at(i);
                poOgrTmpGeometry = kml2geom_rec(poKmlTmpGeometry, poOgrSRS);

                poOgrMultiGeometry->addGeometryDirectly(poOgrTmpGeometry);
            }
            poOgrGeometry = poOgrMultiGeometry;
            break;
        }
        case kmldom::Type_GxTrack:
        {
            GxTrackPtr poKmlGxTrack = AsGxTrack(poKmlGeometry);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
            const size_t nCoords = poKmlGxTrack->get_gx_coord_array_size();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
            poOgrLineString = new OGRLineString();
            for (size_t i = 0; i < nCoords; i++)
            {
                const Vec3 oKmlVec = poKmlGxTrack->get_gx_coord_array_at(i);
                if (oKmlVec.has_altitude())
                    poOgrLineString->addPoint(oKmlVec.get_longitude(),
                                              oKmlVec.get_latitude(),
                                              oKmlVec.get_altitude());
                else
                    poOgrLineString->addPoint(oKmlVec.get_longitude(),
                                              oKmlVec.get_latitude());
            }
            poOgrGeometry = poOgrLineString;
            break;
        }
        case kmldom::Type_GxMultiTrack:
        {
            GxMultiTrackPtr poKmlGxMultiTrack = AsGxMultiTrack(poKmlGeometry);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
            const size_t nGeom = poKmlGxMultiTrack->get_gx_track_array_size();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
            poOgrMultiGeometry = new OGRMultiLineString();
            for (size_t j = 0; j < nGeom; j++)
            {
                GxTrackPtr poKmlGxTrack =
                    poKmlGxMultiTrack->get_gx_track_array_at(j);
                const size_t nCoords = poKmlGxTrack->get_gx_coord_array_size();
                poOgrLineString = new OGRLineString();
                for (size_t i = 0; i < nCoords; i++)
                {
                    const Vec3 oKmlVec = poKmlGxTrack->get_gx_coord_array_at(i);
                    if (oKmlVec.has_altitude())
                        poOgrLineString->addPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude(),
                                                  oKmlVec.get_altitude());
                    else
                        poOgrLineString->addPoint(oKmlVec.get_longitude(),
                                                  oKmlVec.get_latitude());
                }
                poOgrMultiGeometry->addGeometryDirectly(poOgrLineString);
            }
            poOgrGeometry = poOgrMultiGeometry;
            break;
        }

        default:
        {
            break;
        }
    }

    if (poOgrGeometry)
        poOgrGeometry->assignSpatialReference(poOgrSRS);

    return poOgrGeometry;
}

static OGRGeometry *kml2geom_latlonbox_int(const LatLonBoxPtr &poKmlLatLonBox,
                                           OGRSpatialReference *poOgrSRS)
{
    if (!poKmlLatLonBox->has_north() || !poKmlLatLonBox->has_south() ||
        !poKmlLatLonBox->has_east() || !poKmlLatLonBox->has_west())
    {
        return nullptr;
    }
    const double north = poKmlLatLonBox->get_north();
    const double south = poKmlLatLonBox->get_south();
    const double east = poKmlLatLonBox->get_east();
    const double west = poKmlLatLonBox->get_west();

    OGRLinearRing *poOgrRing = new OGRLinearRing();
    poOgrRing->addPoint(east, north, 0.0);
    poOgrRing->addPoint(east, south, 0.0);
    poOgrRing->addPoint(west, south, 0.0);
    poOgrRing->addPoint(west, north, 0.0);
    poOgrRing->addPoint(east, north, 0.0);

    OGRPolygon *poOgrPolygon = new OGRPolygon();
    poOgrPolygon->addRingDirectly(poOgrRing);
    poOgrPolygon->assignSpatialReference(poOgrSRS);

    return poOgrPolygon;
}

static OGRGeometry *
kml2geom_latlonquad_int(const GxLatLonQuadPtr &poKmlLatLonQuad,
                        OGRSpatialReference *poOgrSRS)
{
    if (!poKmlLatLonQuad->has_coordinates())
        return nullptr;

    const CoordinatesPtr &poKmlCoordinates = poKmlLatLonQuad->get_coordinates();

    OGRLinearRing *poOgrLinearRing = new OGRLinearRing();

    size_t nCoords = poKmlCoordinates->get_coordinates_array_size();
    for (size_t i = 0; i < nCoords; i++)
    {
        Vec3 oKmlVec = poKmlCoordinates->get_coordinates_array_at(i);
        if (oKmlVec.has_altitude())
            poOgrLinearRing->addPoint(oKmlVec.get_longitude(),
                                      oKmlVec.get_latitude(),
                                      oKmlVec.get_altitude());
        else
            poOgrLinearRing->addPoint(oKmlVec.get_longitude(),
                                      oKmlVec.get_latitude());
    }
    poOgrLinearRing->closeRings();

    OGRPolygon *poOgrPolygon = new OGRPolygon();
    poOgrPolygon->addRingDirectly(poOgrLinearRing);
    poOgrPolygon->assignSpatialReference(poOgrSRS);

    return poOgrPolygon;
}

/******************************************************************************
 Main function to read a kml geometry and translate to ogr.

Args:
            poKmlGeometry   pointer to the kml geometry to translate
            poOgrSRS        pointer to the spatial ref to set on the geometry

Returns:
            pointer to the new ogr geometry object

******************************************************************************/

OGRGeometry *kml2geom(GeometryPtr poKmlGeometry, OGRSpatialReference *poOgrSRS)
{
    /***** Get the geometry *****/
    OGRGeometry *poOgrGeometry =
        kml2geom_rec(std::move(poKmlGeometry), poOgrSRS);

    /***** Split the geometry at the dateline? *****/
    const char *pszWrap = CPLGetConfigOption("LIBKML_WRAPDATELINE", "no");
    if (!CPLTestBool(pszWrap))
        return poOgrGeometry;

    char **papszTransformOptions = CSLAddString(nullptr, "WRAPDATELINE=YES");

    /***** Transform *****/
    OGRGeometry *poOgrDstGeometry = OGRGeometryFactory::transformWithOptions(
        poOgrGeometry, nullptr, papszTransformOptions);

    /***** Replace the original geom *****/
    if (poOgrDstGeometry)
    {
        delete poOgrGeometry;
        poOgrGeometry = poOgrDstGeometry;
    }

    CSLDestroy(papszTransformOptions);

    return poOgrGeometry;
}

OGRGeometry *kml2geom_latlonbox(LatLonBoxPtr poKmlLatLonBox,
                                OGRSpatialReference *poOgrSRS)
{
    /***** Get the geometry *****/
    OGRGeometry *poOgrGeometry =
        kml2geom_latlonbox_int(poKmlLatLonBox, poOgrSRS);

    /***** Split the geometry at the dateline? *****/
    const char *pszWrap = CPLGetConfigOption("LIBKML_WRAPDATELINE", "no");

    if (!CPLTestBool(pszWrap))
        return poOgrGeometry;

    char **papszTransformOptions = CSLAddString(nullptr, "WRAPDATELINE=YES");

    /***** Transform *****/
    OGRGeometry *poOgrDstGeometry = OGRGeometryFactory::transformWithOptions(
        poOgrGeometry, nullptr, papszTransformOptions);

    /***** Replace the original geom *****/
    if (poOgrDstGeometry)
    {
        delete poOgrGeometry;
        poOgrGeometry = poOgrDstGeometry;
    }

    CSLDestroy(papszTransformOptions);

    return poOgrGeometry;
}

OGRGeometry *kml2geom_latlonquad(GxLatLonQuadPtr poKmlLatLonQuad,
                                 OGRSpatialReference *poOgrSRS)
{
    /***** Get the geometry *****/
    OGRGeometry *poOgrGeometry =
        kml2geom_latlonquad_int(poKmlLatLonQuad, poOgrSRS);

    /***** Split the geometry at the dateline? *****/
    const char *pszWrap = CPLGetConfigOption("LIBKML_WRAPDATELINE", "no");
    if (!CPLTestBool(pszWrap))
        return poOgrGeometry;

    char **papszTransformOptions = CSLAddString(nullptr, "WRAPDATELINE=YES");

    /***** Transform *****/
    OGRGeometry *poOgrDstGeometry = OGRGeometryFactory::transformWithOptions(
        poOgrGeometry, nullptr, papszTransformOptions);

    /***** Replace the original geom *****/
    if (poOgrDstGeometry)
    {
        delete poOgrGeometry;
        poOgrGeometry = poOgrDstGeometry;
    }

    CSLDestroy(papszTransformOptions);

    return poOgrGeometry;
}

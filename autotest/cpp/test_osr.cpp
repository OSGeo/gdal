///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  OGR Spatial Reference general features test.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////

#include "gdal_unit_test.h"

#include "cpl_string.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace tut
{

    // Common fixture with test data
    struct test_osr_data
    {
        OGRErr err_;
        OGRSpatialReferenceH srs_;

        test_osr_data()
            : err_(OGRERR_NONE), srs_(nullptr)
        {
            srs_ = OSRNewSpatialReference(nullptr);
        }

        ~test_osr_data()
        {
            OSRDestroySpatialReference(srs_);
        }
    };

    // Register test group
    typedef test_group<test_osr_data> group;
    typedef group::object object;
    group test_osr_group("OSR");

    // Test UTM WGS84 coordinate system and its various items
    template<>
    template<>
    void object::test<1>()
    {
        ensure("SRS handle is NULL", nullptr != srs_);

        err_ = OSRSetUTM(srs_, 11, TRUE);
        ensure_equals("Can't set UTM zone", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        double val = 0;

        val = OSRGetProjParm(srs_, SRS_PP_CENTRAL_MERIDIAN, -1111, &err_);
        ensure("Invalid UTM central meridian",
            std::fabs(val - (-117.0)) <= .00000000000010);

        val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_ORIGIN, -1111, &err_);
        ensure("Invalid UTM latitude of origin",
            std::fabs(val - 0.0) <= .00000000000010);

        val = OSRGetProjParm(srs_, SRS_PP_SCALE_FACTOR, -1111, &err_);
        ensure("Invalid UTM scale factor",
            std::fabs(val - 0.9996) <= .00000000000010);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
        ensure("Invalid UTM false easting",
            std::fabs(val - 500000.0) <= .00000000000010);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
        ensure("Invalid UTM false northing",
            std::fabs(val - 0.0) <= .00000000000010);

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "GEOGCS")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "GEOGCS")), std::string("4326"));

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "DATUM")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "DATUM")), std::string("6326"));
    }

    // Simple default NAD83 State Plane zone
    template<>
    template<>
    void object::test<2>()
    {
        ensure("SRS handle is NULL", nullptr != srs_);

        // California III NAD83
        OSRSetStatePlane(srs_, 403, 1);

        double val = 0;

        val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_1, -1111, &err_);
        ensure_approx_equals(val, 38.43333333333333);

        val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_2, -1111, &err_);
        ensure_approx_equals(val, 37.06666666666667);

        val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_ORIGIN, -1111, &err_);
        ensure_approx_equals(val, 36.5);

        val = OSRGetProjParm(srs_, SRS_PP_CENTRAL_MERIDIAN, -1111, &err_);
        ensure_approx_equals(val, -120.5);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
        ensure_approx_equals(val, 2000000.0);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
        ensure_approx_equals(val, 500000.0);

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "GEOGCS")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "GEOGCS")), std::string("4269"));

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "DATUM")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "DATUM")), std::string("6269"));

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "PROJCS")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "PROJCS")), std::string("26943"));

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "PROJCS|UNIT")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "PROJCS|UNIT")), std::string("9001"));
    }

    // NAD83 State Plane zone, but overridden to be in Feet
    template<>
    template<>
    void object::test<3>()
    {
        ensure("SRS handle is NULL", nullptr != srs_);

        // California III NAD83 (feet)
        OSRSetStatePlaneWithUnits(srs_, 403, 1, "Foot", 0.3048006096012192);

        double val = 0;

        val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_1, -1111, &err_);
        ensure_approx_equals(val, 38.43333333333333);

        val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_2, -1111, &err_);
        ensure_approx_equals(val, 37.06666666666667);

        val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_ORIGIN, -1111, &err_);
        ensure_approx_equals(val, 36.5);

        val = OSRGetProjParm(srs_, SRS_PP_CENTRAL_MERIDIAN, -1111, &err_);
        ensure_approx_equals(val, -120.5);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
        ensure_approx_equals(val, 6561666.666666667);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
        ensure_approx_equals(val, 1640416.666666667);

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "GEOGCS")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "GEOGCS")), std::string("4269"));

        ensure_equals("Invalid authority name",
            std::string(OSRGetAuthorityName(srs_, "DATUM")), std::string("EPSG"));
        ensure_equals("Invalid authority code",
            std::string(OSRGetAuthorityCode(srs_, "DATUM")), std::string("6269"));

        ensure("Got a PROJCS Authority but we should not",
            nullptr == OSRGetAuthorityName(srs_, "PROJCS"));

        ensure("Got METER authority code on linear units",
            nullptr == OSRGetAuthorityCode(srs_, "PROJCS|UNIT"));

        char* unitsName = nullptr;
        val = OSRGetLinearUnits(srs_, &unitsName);
        ensure("Units name is NULL", nullptr != unitsName);
        ensure( "Did not get Foot linear units",
                std::string("Foot") == unitsName);
    }

    // Translate a coordinate system with NAD shift into to PROJ.4 and back.
    // Also, verify that the TOWGS84 parameters are preserved.
    template<>
    template<>
    void object::test<4>()
    {
        ensure("SRS handle is NULL", nullptr != srs_);

        err_ = OSRSetGS(srs_, -117.0, 100000.0, 100000);
        ensure_equals("OSRSetGS failed", err_, OGRERR_NONE);

        err_ = OSRSetGeogCS(srs_, "Test GCS", "Test Datum", "WGS84",
            SRS_WGS84_SEMIMAJOR, SRS_WGS84_INVFLATTENING,
            nullptr, 0, nullptr, 0);
        ensure_equals("OSRSetGeogCS failed", err_, OGRERR_NONE);

        err_ = OSRSetTOWGS84(srs_, 1, 2, 3, 0, 0, 0, 0);
        ensure_equals("OSRSetTOWGS84 failed", err_, OGRERR_NONE);

        const int coeffSize = 7;
        double coeff[coeffSize] = { 0 };
        const double expect[coeffSize] = { 1, 2, 3, 0, 0, 0, 0 };

        err_ = OSRGetTOWGS84(srs_, coeff, 7);
        ensure_equals("OSRSetTOWGS84 failed", err_, OGRERR_NONE);
        ensure("GetTOWGS84 result is wrong",
            std::equal(coeff, coeff + coeffSize, expect));
        OSRSetLinearUnits(srs_, "Metre", 1);

        char* proj4 = nullptr;
        err_ = OSRExportToProj4(srs_, &proj4);
        ensure_equals("OSRExportToProj4 failed", err_, OGRERR_NONE);

        OGRSpatialReferenceH srs2 = nullptr;
        srs2 = OSRNewSpatialReference(nullptr);

        err_ = OSRImportFromProj4(srs2, proj4);
        ensure_equals("OSRImportFromProj4 failed", err_, OGRERR_NONE);

        err_ = OSRGetTOWGS84(srs2, coeff, 7);
        ensure_equals("OSRSetTOWGS84 failed", err_, OGRERR_NONE);
        ensure("GetTOWGS84 result is wrong",
            std::equal(coeff, coeff + coeffSize, expect));

        OSRDestroySpatialReference(srs2);
        CPLFree(proj4);
    }

    // Test URN support for OGC:CRS84
    template<>
    template<>
    void object::test<5>()
    {
        ensure("SRS handle is NULL", nullptr != srs_);

        err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:OGC:1.3:CRS84");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt1 = nullptr;
        err_ = OSRExportToWkt(srs_, &wkt1);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", nullptr != wkt1);

        CPLFree(wkt1);
    }

    // Test URN support for EPSG
    template<>
    template<>
    void object::test<6>()
    {
        ensure("SRS handle is NULL", nullptr != srs_);

        err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:EPSG::4326");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt1 = nullptr;
        err_ = OSRExportToWkt(srs_, &wkt1);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", nullptr != wkt1);

        err_ = OSRSetFromUserInput(srs_, "EPSGA:4326");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt2 = nullptr;
        err_ = OSRExportToWkt(srs_, &wkt2);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", nullptr != wkt2);

        ensure_equals("EPSG:4326 urn lookup not as expected",
            std::string(wkt1), std::string(wkt2));
        CPLFree(wkt1);
        CPLFree(wkt2);
    }

    // Test URN support for auto projection
    template<>
    template<>
    void object::test<7 >()
    {
        ensure("SRS handle is NULL", nullptr != srs_);

        err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:OGC::AUTO42001:-117:33");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(32611);

        ensure(oSRS.IsSame(OGRSpatialReference::FromHandle(srs_)));
    }

    // Test StripTOWGS84IfKnownDatum
    template<>
    template<>
    void object::test<8 >()
    {
        // Not a boundCRS
        {
            OGRSpatialReference oSRS;
            oSRS.importFromEPSG(4326);
            ensure(!oSRS.StripTOWGS84IfKnownDatum());
        }
        // Custom boundCRS --> do not strip TOWGS84
        {
            OGRSpatialReference oSRS;
            oSRS.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=1,2,3,4,5,6,7");
            ensure(!oSRS.StripTOWGS84IfKnownDatum());
            double vals[7] = { 0 };
            ensure(oSRS.GetTOWGS84(vals, 7) == OGRERR_NONE);
        }
        // BoundCRS whose base CRS has a known code --> strip TOWGS84
        {
            OGRSpatialReference oSRS;
            oSRS.importFromEPSG(4326);
            oSRS.SetTOWGS84(1,2,3,4,5,6,7);
            ensure(oSRS.StripTOWGS84IfKnownDatum());
            double vals[7] = { 0 };
            ensure(oSRS.GetTOWGS84(vals, 7) != OGRERR_NONE);
        }
        // BoundCRS whose datum code is known --> strip TOWGS84
        {
            OGRSpatialReference oSRS;
            oSRS.SetFromUserInput(
                "GEOGCS[\"bar\","
                "DATUM[\"foo\","
                "SPHEROID[\"WGS 84\",6378137,298.257223563],"
                "TOWGS84[1,2,3,4,5,6,7],"
                "AUTHORITY[\"FOO\",\"1\"]],"
                "PRIMEM[\"Greenwich\",0],"
                "UNIT[\"degree\",0.0174532925199433]]");
            ensure(oSRS.StripTOWGS84IfKnownDatum());
            double vals[7] = { 0 };
            ensure(oSRS.GetTOWGS84(vals, 7) != OGRERR_NONE);
        }
        // BoundCRS whose datum name is known --> strip TOWGS84
        {
            OGRSpatialReference oSRS;
            oSRS.SetFromUserInput(
                "GEOGCS[\"WGS 84\","
                "DATUM[\"WGS_1984\","
                "SPHEROID[\"WGS 84\",6378137,298.257223563],"
                "TOWGS84[1,2,3,4,5,6,7]],"
                "PRIMEM[\"Greenwich\",0],"
                "UNIT[\"degree\",0.0174532925199433]]");
            ensure(oSRS.StripTOWGS84IfKnownDatum());
            double vals[7] = { 0 };
            ensure(oSRS.GetTOWGS84(vals, 7) != OGRERR_NONE);
        }
        // BoundCRS whose datum name is unknown --> do not strip TOWGS84
        {
            OGRSpatialReference oSRS;
            oSRS.SetFromUserInput(
                "GEOGCS[\"WGS 84\","
                "DATUM[\"i am unknown\","
                "SPHEROID[\"WGS 84\",6378137,298.257223563],"
                "TOWGS84[1,2,3,4,5,6,7]],"
                "PRIMEM[\"Greenwich\",0],"
                "UNIT[\"degree\",0.0174532925199433]]");
            ensure(!oSRS.StripTOWGS84IfKnownDatum());
            double vals[7] = { 0 };
            ensure(oSRS.GetTOWGS84(vals, 7) == OGRERR_NONE);
        }
    }

    // Test GetEPSGGeogCS
    template<>
    template<>
    void object::test<9 >()
    {
        // When export to WKT1 is not possible
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput(
            "PROJCRS[\"World_Vertical_Perspective\",\n"
            "    BASEGEOGCRS[\"WGS 84\",\n"
            "        DATUM[\"World Geodetic System 1984\",\n"
            "            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n"
            "                LENGTHUNIT[\"metre\",1]]],\n"
            "        PRIMEM[\"Greenwich\",0,\n"
            "            ANGLEUNIT[\"Degree\",0.0174532925199433]]],\n"
            "    CONVERSION[\"World_Vertical_Perspective\",\n"
            "        METHOD[\"Vertical Perspective\",\n"
            "            ID[\"EPSG\",9838]],\n"
            "        PARAMETER[\"Latitude of topocentric origin\",0,\n"
            "            ANGLEUNIT[\"Degree\",0.0174532925199433],\n"
            "            ID[\"EPSG\",8834]],\n"
            "        PARAMETER[\"Longitude of topocentric origin\",0,\n"
            "            ANGLEUNIT[\"Degree\",0.0174532925199433],\n"
            "            ID[\"EPSG\",8835]],\n"
            "        PARAMETER[\"Viewpoint height\",35800000,\n"
            "            LENGTHUNIT[\"metre\",1],\n"
            "            ID[\"EPSG\",8840]]],\n"
            "    CS[Cartesian,2],\n"
            "        AXIS[\"(E)\",east,\n"
            "            ORDER[1],\n"
            "            LENGTHUNIT[\"metre\",1]],\n"
            "        AXIS[\"(N)\",north,\n"
            "            ORDER[2],\n"
            "            LENGTHUNIT[\"metre\",1]],\n"
            "    USAGE[\n"
            "        SCOPE[\"Not known.\"],\n"
            "        AREA[\"World.\"],\n"
            "        BBOX[-90,-180,90,180]],\n"
            "    ID[\"ESRI\",54049]]");
        ensure_equals(oSRS.GetEPSGGeogCS(), 4326);
    }

} // namespace tut

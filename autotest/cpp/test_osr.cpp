///////////////////////////////////////////////////////////////////////////////
// $Id: test_osr.cpp,v 1.5 2006/12/06 15:39:13 mloskot Exp $
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
//
//  $Log: test_osr.cpp,v $
//  Revision 1.5  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////

// See Bronek Kozicki's comments posted here:
// http://lists.boost.org/Archives/boost/2005/07/89697.php
#pragma warning(disable: 4996)

#include <tut.h>
#include <tut_gdal.h>
#include <ogr_srs_api.h> // OGR/OSR API
#include <algorithm>
#include <cmath>
#include <string>
#include <cpl_string.h>

namespace tut
{

    // Common fixture with test data
    struct test_osr_data
    {
        OGRErr err_;
        OGRSpatialReferenceH srs_;

        test_osr_data()
            : err_(OGRERR_NONE), srs_(NULL)
        {
            srs_ = OSRNewSpatialReference(NULL);
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
        ensure("SRS handle is NULL", NULL != srs_);

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
        ensure("SRS handle is NULL", NULL != srs_);

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
        ensure("SRS handle is NULL", NULL != srs_);

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

        ensure("Got a PROJCS Authority but we shouldn\'t",
            NULL == OSRGetAuthorityName(srs_, "PROJCS"));

        ensure("Got METER authority code on linear units",
            NULL == OSRGetAuthorityCode(srs_, "PROJCS|UNIT"));
        
        char* unitsName = NULL;
        val = OSRGetLinearUnits(srs_, &unitsName);
        ensure("Units name is NULL", NULL != unitsName);
        ensure("Didn\'t get Foot linear units", std::string("Foot") == unitsName);
    }

    // Translate a coordinate system with NAD shift into to PROJ.4 and back.
    // Also, verify that the TOWGS84 parameters are preserved.
    template<>
    template<>
    void object::test<4>()
    {
        ensure("SRS handle is NULL", NULL != srs_);

        err_ = OSRSetGS(srs_, -117.0, 100000.0, 100000);
        ensure_equals("OSRSetGS failed", err_, OGRERR_NONE);

        err_ = OSRSetGeogCS(srs_, "Test GCS", "Test Datum", "WGS84",
            SRS_WGS84_SEMIMAJOR, SRS_WGS84_INVFLATTENING,
            NULL, 0, NULL, 0);
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

        char* proj4 = NULL;
        err_ = OSRExportToProj4(srs_, &proj4);
        ensure_equals("OSRExportToProj4 failed", err_, OGRERR_NONE);

        OGRSpatialReferenceH srs2 = NULL;
        srs2 = OSRNewSpatialReference(NULL);

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
        ensure("SRS handle is NULL", NULL != srs_);

        err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:OGC:1.3:CRS84");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt1 = NULL;
        err_ = OSRExportToWkt(srs_, &wkt1);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", NULL != wkt1);

        err_ = OSRSetFromUserInput(srs_, "WGS84");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt2 = NULL;
        err_ = OSRExportToWkt(srs_, &wkt2);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", NULL != wkt2);

        ensure_equals("CRS84 lookup not as expected",
            std::string(wkt1), std::string(wkt2));
        CPLFree(wkt1);
        CPLFree(wkt2);
    }

    // Test URN support for EPSG
    template<>
    template<>
    void object::test<6>()
    {
        ensure("SRS handle is NULL", NULL != srs_);

        err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:EPSG::4326");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt1 = NULL;
        err_ = OSRExportToWkt(srs_, &wkt1);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", NULL != wkt1); 

        err_ = OSRSetFromUserInput(srs_, "EPSGA:4326");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt2 = NULL;
        err_ = OSRExportToWkt(srs_, &wkt2);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", NULL != wkt2);

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
        ensure("SRS handle is NULL", NULL != srs_);

        err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:OGC::AUTO42001:-117:33");
        ensure_equals("OSRSetFromUserInput failed", err_, OGRERR_NONE);

        char* wkt1 = NULL;
        err_ = OSRExportToWkt(srs_, &wkt1);
        ensure_equals("OSRExportToWkt failed", err_, OGRERR_NONE);
        ensure("OSRExportToWkt returned NULL", NULL != wkt1); 

        std::string expect("PROJCS[\"UTM Zone 11, Northern Hemisphere\","
                           "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\","
                           "SPHEROID[\"WGS 84\",6378137,298.257223563,"
                           "AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],"
                           "AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,"
                           "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,"
                           "AUTHORITY[\"EPSG\",\"9108\"]],"
                           "AUTHORITY[\"EPSG\",\"4326\"]],"
                           "PROJECTION[\"Transverse_Mercator\"],"
                           "PARAMETER[\"latitude_of_origin\",0],"
                           "PARAMETER[\"central_meridian\",-117],"
                           "PARAMETER[\"scale_factor\",0.9996],"
                           "PARAMETER[\"false_easting\",500000],"
                           "PARAMETER[\"false_northing\",0],"
                           "UNIT[\"Meter\",1,AUTHORITY[\"EPSG\",\"9001\"]]]");

        ensure_equals("AUTO42001 urn lookup not as expected", std::string(wkt1), expect);
        CPLFree(wkt1);
    }


} // namespace tut

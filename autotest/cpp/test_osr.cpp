///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  OGR Spatial Reference general features test.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
/*
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

#include "gdal_unit_test.h"

#include "cpl_string.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_osr : public ::testing::Test
{
    OGRErr err_ = OGRERR_NONE;
    OGRSpatialReferenceH srs_ = nullptr;

    void SetUp() override
    {
        srs_ = OSRNewSpatialReference(nullptr);
        ASSERT_TRUE(nullptr != srs_);
    }

    void TearDown() override
    {
        OSRDestroySpatialReference(srs_);
        srs_ = nullptr;
    }
};

// Test UTM WGS84 coordinate system and its various items
TEST_F(test_osr, UTM_WGS84)
{
    err_ = OSRSetUTM(srs_, 11, TRUE);
    ASSERT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetWellKnownGeogCS(srs_, "WGS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    double val = 0;

    val = OSRGetProjParm(srs_, SRS_PP_CENTRAL_MERIDIAN, -1111, &err_);
    EXPECT_NEAR(val, -117.0, .00000000000010);

    val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_ORIGIN, -1111, &err_);
    EXPECT_NEAR(val, 0.0, .00000000000010);

    val = OSRGetProjParm(srs_, SRS_PP_SCALE_FACTOR, -1111, &err_);
    EXPECT_NEAR(val, 0.9996, .00000000000010);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
    EXPECT_NEAR(val, 500000, .00000000000010);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
    EXPECT_NEAR(val, 0.0, .00000000000010);

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "GEOGCS"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "GEOGCS"), "4326");

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "DATUM"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "DATUM"), "6326");
}

// Simple default NAD83 State Plane zone
TEST_F(test_osr, NAD83_State_Plane)
{
    // California III NAD83
    OSRSetStatePlane(srs_, 403, 1);

    double val = 0;

    val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_1, -1111, &err_);
    EXPECT_NEAR(val, 38.43333333333333, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_2, -1111, &err_);
    EXPECT_NEAR(val, 37.06666666666667, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_ORIGIN, -1111, &err_);
    EXPECT_NEAR(val, 36.5, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_CENTRAL_MERIDIAN, -1111, &err_);
    EXPECT_NEAR(val, -120.5, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
    EXPECT_NEAR(val, 2000000.0, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
    EXPECT_NEAR(val, 500000.0, 1e-12);

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "GEOGCS"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "GEOGCS"), "4269");

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "DATUM"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "DATUM"), "6269");

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "PROJCS"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "PROJCS"), "26943");

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "PROJCS|UNIT"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "PROJCS|UNIT"), "9001");
}

// NAD83 State Plane zone, but overridden to be in Feet
TEST_F(test_osr, NAD83_State_Plane_Feet)
{
    // California III NAD83 (feet)
    OSRSetStatePlaneWithUnits(srs_, 403, 1, "Foot", 0.3048006096012192);

    double val = 0;

    val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_1, -1111, &err_);
    EXPECT_NEAR(val, 38.43333333333333, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_2, -1111, &err_);
    EXPECT_NEAR(val, 37.06666666666667, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_ORIGIN, -1111, &err_);
    EXPECT_NEAR(val, 36.5, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_CENTRAL_MERIDIAN, -1111, &err_);
    EXPECT_NEAR(val, -120.5, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
    EXPECT_NEAR(val, 6561666.666666667, 1e-12);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
    EXPECT_NEAR(val, 1640416.666666667, 1e-12);

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "GEOGCS"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "GEOGCS"), "4269");

    EXPECT_STREQ(OSRGetAuthorityName(srs_, "DATUM"), "EPSG");
    EXPECT_STREQ(OSRGetAuthorityCode(srs_, "DATUM"), "6269");

    EXPECT_TRUE(nullptr == OSRGetAuthorityName(srs_, "PROJCS"));
    EXPECT_TRUE(nullptr == OSRGetAuthorityCode(srs_, "PROJCS|UNIT"));

    char *unitsName = nullptr;
    val = OSRGetLinearUnits(srs_, &unitsName);
    ASSERT_TRUE(nullptr != unitsName);
    EXPECT_STREQ(unitsName, "Foot");
}

// Translate a coordinate system with NAD shift into to PROJ.4 and back.
// Also, verify that the TOWGS84 parameters are preserved.
TEST_F(test_osr, NAD_shift)
{
    err_ = OSRSetGS(srs_, -117.0, 100000.0, 100000);
    EXPECT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetGeogCS(srs_, "Test GCS", "Test Datum", "WGS84",
                        SRS_WGS84_SEMIMAJOR, SRS_WGS84_INVFLATTENING, nullptr,
                        0, nullptr, 0);
    EXPECT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetTOWGS84(srs_, 1, 2, 3, 0, 0, 0, 0);
    EXPECT_EQ(err_, OGRERR_NONE);

    const int coeffSize = 7;
    double coeff[coeffSize] = {0};
    const double expect[coeffSize] = {1, 2, 3, 0, 0, 0, 0};

    err_ = OSRGetTOWGS84(srs_, coeff, 7);
    EXPECT_EQ(err_, OGRERR_NONE);
    EXPECT_TRUE(std::equal(coeff, coeff + coeffSize, expect));
    OSRSetLinearUnits(srs_, "Metre", 1);

    char *proj4 = nullptr;
    err_ = OSRExportToProj4(srs_, &proj4);
    EXPECT_EQ(err_, OGRERR_NONE);

    OGRSpatialReferenceH srs2 = nullptr;
    srs2 = OSRNewSpatialReference(nullptr);

    err_ = OSRImportFromProj4(srs2, proj4);
    EXPECT_EQ(err_, OGRERR_NONE);

    err_ = OSRGetTOWGS84(srs2, coeff, 7);
    EXPECT_EQ(err_, OGRERR_NONE);
    EXPECT_TRUE(std::equal(coeff, coeff + coeffSize, expect));

    OSRDestroySpatialReference(srs2);
    CPLFree(proj4);
}

// Test URN support for OGC:CRS84
TEST_F(test_osr, URN_OGC_CRS84)
{
    err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:OGC:1.3:CRS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    char *wkt1 = nullptr;
    err_ = OSRExportToWkt(srs_, &wkt1);
    EXPECT_EQ(err_, OGRERR_NONE);
    EXPECT_TRUE(nullptr != wkt1);

    CPLFree(wkt1);
}

// Test URN support for EPSG
TEST_F(test_osr, URN_EPSG)
{
    err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:EPSG::4326");
    ASSERT_EQ(err_, OGRERR_NONE);

    char *wkt1 = nullptr;
    err_ = OSRExportToWkt(srs_, &wkt1);
    EXPECT_EQ(err_, OGRERR_NONE);
    EXPECT_TRUE(nullptr != wkt1);

    err_ = OSRSetFromUserInput(srs_, "EPSGA:4326");
    EXPECT_EQ(err_, OGRERR_NONE);

    char *wkt2 = nullptr;
    err_ = OSRExportToWkt(srs_, &wkt2);
    EXPECT_EQ(err_, OGRERR_NONE);
    EXPECT_TRUE(nullptr != wkt2);

    EXPECT_STREQ(wkt1, wkt2);
    CPLFree(wkt1);
    CPLFree(wkt2);
}

// Test URN support for auto projection
TEST_F(test_osr, URN_AUTO)
{
    err_ = OSRSetFromUserInput(srs_, "urn:ogc:def:crs:OGC::AUTO42001:-117:33");
    ASSERT_EQ(err_, OGRERR_NONE);

    OGRSpatialReference oSRS;
    oSRS.importFromEPSG(32611);

    EXPECT_TRUE(oSRS.IsSame(OGRSpatialReference::FromHandle(srs_)));
}

// Test StripTOWGS84IfKnownDatum
TEST_F(test_osr, StripTOWGS84IfKnownDatum)
{
    // Not a boundCRS
    {
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(4326);
        EXPECT_TRUE(!oSRS.StripTOWGS84IfKnownDatum());
    }
    // Custom boundCRS --> do not strip TOWGS84
    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput(
            "+proj=longlat +ellps=GRS80 +towgs84=1,2,3,4,5,6,7");
        EXPECT_TRUE(!oSRS.StripTOWGS84IfKnownDatum());
        double vals[7] = {0};
        EXPECT_TRUE(oSRS.GetTOWGS84(vals, 7) == OGRERR_NONE);
    }
    // BoundCRS whose base CRS has a known code --> strip TOWGS84
    {
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(4326);
        oSRS.SetTOWGS84(1, 2, 3, 4, 5, 6, 7);
        EXPECT_TRUE(oSRS.StripTOWGS84IfKnownDatum());
        double vals[7] = {0};
        EXPECT_TRUE(oSRS.GetTOWGS84(vals, 7) != OGRERR_NONE);
    }
    // BoundCRS whose datum code is known --> strip TOWGS84
    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput("GEOGCS[\"bar\","
                              "DATUM[\"foo\","
                              "SPHEROID[\"WGS 84\",6378137,298.257223563],"
                              "TOWGS84[1,2,3,4,5,6,7],"
                              "AUTHORITY[\"FOO\",\"1\"]],"
                              "PRIMEM[\"Greenwich\",0],"
                              "UNIT[\"degree\",0.0174532925199433]]");
        EXPECT_TRUE(oSRS.StripTOWGS84IfKnownDatum());
        double vals[7] = {0};
        EXPECT_TRUE(oSRS.GetTOWGS84(vals, 7) != OGRERR_NONE);
    }
    // BoundCRS whose datum name is known --> strip TOWGS84
    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput("GEOGCS[\"WGS 84\","
                              "DATUM[\"WGS_1984\","
                              "SPHEROID[\"WGS 84\",6378137,298.257223563],"
                              "TOWGS84[1,2,3,4,5,6,7]],"
                              "PRIMEM[\"Greenwich\",0],"
                              "UNIT[\"degree\",0.0174532925199433]]");
        EXPECT_TRUE(oSRS.StripTOWGS84IfKnownDatum());
        double vals[7] = {0};
        EXPECT_TRUE(oSRS.GetTOWGS84(vals, 7) != OGRERR_NONE);
    }
    // BoundCRS whose datum name is unknown --> do not strip TOWGS84
    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput("GEOGCS[\"WGS 84\","
                              "DATUM[\"i am unknown\","
                              "SPHEROID[\"WGS 84\",6378137,298.257223563],"
                              "TOWGS84[1,2,3,4,5,6,7]],"
                              "PRIMEM[\"Greenwich\",0],"
                              "UNIT[\"degree\",0.0174532925199433]]");
        EXPECT_TRUE(!oSRS.StripTOWGS84IfKnownDatum());
        double vals[7] = {0};
        EXPECT_TRUE(oSRS.GetTOWGS84(vals, 7) == OGRERR_NONE);
    }
}

// Test GetEPSGGeogCS
TEST_F(test_osr, GetEPSGGeogCS)
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
    EXPECT_EQ(oSRS.GetEPSGGeogCS(), 4326);
}

// Test GetOGCURN
TEST_F(test_osr, GetOGCURN)
{
    {
        OGRSpatialReference oSRS;
        char *pszRet = oSRS.GetOGCURN();
        EXPECT_TRUE(pszRet == nullptr);
        CPLFree(pszRet);
    }
    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput("+proj=longlat");
        char *pszRet = oSRS.GetOGCURN();
        EXPECT_TRUE(pszRet == nullptr);
        CPLFree(pszRet);
    }

    {
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(32631);
        char *pszRet = oSRS.GetOGCURN();
        EXPECT_TRUE(pszRet != nullptr);
        if (pszRet)
        {
            EXPECT_STREQ(pszRet, "urn:ogc:def:crs:EPSG::32631");
        }
        CPLFree(pszRet);
    }

    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput("EPSG:32631+5773");
        char *pszRet = oSRS.GetOGCURN();
        EXPECT_TRUE(pszRet != nullptr);
        if (pszRet)
        {
            EXPECT_STREQ(pszRet,
                         "urn:ogc:def:crs,crs:EPSG::32631,crs:EPSG::5773");
        }
        CPLFree(pszRet);
    }
}

// Test constructors and assignment operators
TEST_F(test_osr, constructors_assignment_operators)
{
    OGRSpatialReference oSRS;
    oSRS.importFromEPSG(32631);

    OGRSpatialReference oSRS2(oSRS);
    ASSERT_TRUE(oSRS2.GetAuthorityCode(nullptr) != nullptr);

    OGRSpatialReference oSRS3;
    OGRSpatialReference &oSRSRef(oSRS);
    oSRS = oSRSRef;
    ASSERT_TRUE(oSRS.GetAuthorityCode(nullptr) != nullptr);
    oSRS3 = oSRS;
    ASSERT_TRUE(oSRS3.GetAuthorityCode(nullptr) != nullptr);

    OGRSpatialReference oSRS4(std::move(oSRS));
    ASSERT_TRUE(oSRS4.GetAuthorityCode(nullptr) != nullptr);

    OGRSpatialReference oSRS5;
    OGRSpatialReference &oSRS4Ref(oSRS4);
    oSRS4 = std::move(oSRS4Ref);
    ASSERT_TRUE(oSRS4.GetAuthorityCode(nullptr) != nullptr);
    oSRS5 = std::move(oSRS4);
    ASSERT_TRUE(oSRS5.GetAuthorityCode(nullptr) != nullptr);
}

static int GetEPSGCode(OGRSpatialReference &oSRS)
{
    int nEPSG = 0;
    auto pszEPSG = oSRS.GetAuthorityCode("PROJCS");
    if (pszEPSG == nullptr)
    {
        pszEPSG = oSRS.GetAuthorityCode("GEOGCS");
    }
    if (pszEPSG == nullptr)
    {
        pszEPSG = oSRS.GetAuthorityCode("VERT_CS");
    }
    if (pszEPSG != nullptr)
    {
        nEPSG = atoi(pszEPSG);
    }
    return nEPSG;
}

// Test exportVertCSToPanorama
TEST_F(test_osr, exportVertCSToPanorama)
{
    OGRSpatialReference oSRS;
    oSRS.importFromEPSG(28407);

    OGRSpatialReference oVertSRS;
    oVertSRS.importFromEPSG(5705);
    EXPECT_TRUE(oVertSRS.IsVertical() == TRUE);
    EXPECT_STRNE(oVertSRS.GetAttrValue("VERT_CS"), "");
    EXPECT_STRNE(oVertSRS.GetAttrValue("VERT_DATUM"), "");
    EXPECT_EQ(GetEPSGCode(oVertSRS), 5705);

    oSRS.SetVertCS(oVertSRS.GetAttrValue("VERT_CS"),
                   oVertSRS.GetAttrValue("VERT_DATUM"));

    int nVertID = 0;
    oSRS.exportVertCSToPanorama(&nVertID);
    EXPECT_EQ(nVertID, 25);
}

// Test importFromPanorama
TEST_F(test_osr, importFromPanorama)
{
    OGRSpatialReference oSRS;
    oSRS.importFromPanorama(35, 0, 45, nullptr);
    EXPECT_EQ(GetEPSGCode(oSRS), 3857);

    oSRS.importFromPanorama(35, 0, 9, nullptr);
    EXPECT_EQ(GetEPSGCode(oSRS), 3395);

    constexpr double TO_RADIANS = 0.017453292519943295769;
    {
        // WGS 84 / UTM zone 1
        double adfPrjParams[8] = {0.0,    0.0,      0.0, -177 * TO_RADIANS,
                                  0.9996, 500000.0, 0.0, 0.0};
        oSRS.importFromPanorama(17, 2, 9, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 32601);

        oSRS.importFromPanorama(17, 2, 9, adfPrjParams, FALSE);
        EXPECT_EQ(GetEPSGCode(oSRS), 32701);
    }
    {
        // WGS 84 / UTM zone 37
        double adfPrjParams[8] = {0.0,    0.0,      0.0, 39 * TO_RADIANS,
                                  0.9996, 500000.0, 0.0, 0.0};
        oSRS.importFromPanorama(17, 2, 9, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 32637);

        oSRS.importFromPanorama(17, 2, 9, adfPrjParams, FALSE);
        EXPECT_EQ(GetEPSGCode(oSRS), 32737);
    }
    {
        // Pulkovo 1942 / Gauss-Kruger zone 4
        double adfPrjParams[8] = {0.0, 0.0,       0.0, 21 * TO_RADIANS,
                                  1.0, 4500000.0, 0.0, 0.0};
        oSRS.importFromPanorama(1, 1, 1, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 28404);
        oSRS.importFromPanorama(1, 0, 0, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 28404);
        adfPrjParams[7] = 4;
        oSRS.importFromPanorama(1, 1, 1, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 28404);
    }
    {
        // Pulkovo 1942 / Gauss-Kruger zone 31
        double adfPrjParams[8] = {0.0, 0.0,        0.0, -177 * TO_RADIANS,
                                  1.0, 31500000.0, 0.0, 0.0};
        oSRS.importFromPanorama(1, 1, 1, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 28431);
        oSRS.importFromPanorama(1, 0, 0, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 28431);
        adfPrjParams[7] = 31;
        oSRS.importFromPanorama(1, 1, 1, adfPrjParams);
        EXPECT_EQ(GetEPSGCode(oSRS), 28431);
    }
    {
        // Invalid data
        double adfPrjParams[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        oSRS.importFromPanorama(0, 0, 0, adfPrjParams);
        EXPECT_EQ(oSRS.IsLocal(), true);
        EXPECT_EQ(GetEPSGCode(oSRS), 0);
    }
}

// Test exportToPanorama
TEST_F(test_osr, exportToPanorama)
{
    constexpr double RAD = 0.017453292519943295769;
    constexpr double EPS = 1e-12;

    {
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(32601);  // WGS 84 / UTM zone 1N
        EXPECT_EQ(GetEPSGCode(oSRS), 32601);

        long iProjSys = 0;
        long iDatum = 0;
        long iEllips = 0;
        long iZone = 0;
        double adfParams[7] = {0};
        oSRS.exportToPanorama(&iProjSys, &iDatum, &iEllips, &iZone, adfParams);
        EXPECT_EQ(iProjSys, 17);
        EXPECT_EQ(iDatum, 6);
        EXPECT_EQ(iEllips, 9);
        EXPECT_EQ(iZone, 1);
        EXPECT_NEAR(adfParams[2], 0.0, EPS);         //latitude_of_origin
        EXPECT_NEAR(adfParams[3], -177 * RAD, EPS);  //central_meridian
        EXPECT_NEAR(adfParams[4], 0.9996, EPS);      //scale_factor
        EXPECT_NEAR(adfParams[5], 500000, EPS);      //false_easting
        EXPECT_NEAR(adfParams[6], 0, EPS);           //false_northing
    }

    {
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(32660);  // WGS 84 / UTM zone 1N
        EXPECT_EQ(GetEPSGCode(oSRS), 32660);

        long iProjSys = 0;
        long iDatum = 0;
        long iEllips = 0;
        long iZone = 0;
        double adfParams[7] = {0};
        oSRS.exportToPanorama(&iProjSys, &iDatum, &iEllips, &iZone, adfParams);
        EXPECT_EQ(iProjSys, 17);
        EXPECT_EQ(iDatum, 6);
        EXPECT_EQ(iEllips, 9);
        EXPECT_EQ(iZone, 60);
        EXPECT_NEAR(adfParams[2], 0.0, EPS);        //latitude_of_origin
        EXPECT_NEAR(adfParams[3], 177 * RAD, EPS);  //central_meridian
        EXPECT_NEAR(adfParams[4], 0.9996, EPS);     //scale_factor
        EXPECT_NEAR(adfParams[5], 500000, EPS);     //false_easting
        EXPECT_NEAR(adfParams[6], 0, EPS);          //false_northing
    }

    {
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(28404);  // Pulkovo 1942 / Gauss-Kruger zone 4
        EXPECT_EQ(GetEPSGCode(oSRS), 28404);

        long iProjSys = 0;
        long iDatum = 0;
        long iEllips = 0;
        long iZone = 0;
        double adfParams[7] = {0};
        oSRS.exportToPanorama(&iProjSys, &iDatum, &iEllips, &iZone, adfParams);
        EXPECT_EQ(iProjSys, 1);
        EXPECT_EQ(iDatum, 1);
        EXPECT_EQ(iEllips, 1);
        EXPECT_EQ(iZone, 4);
        EXPECT_NEAR(adfParams[2], 0.0, EPS);       //latitude_of_origin
        EXPECT_NEAR(adfParams[3], 21 * RAD, EPS);  //central_meridian
        EXPECT_NEAR(adfParams[4], 1.0, EPS);       //scale_factor
        EXPECT_NEAR(adfParams[5], 4500000, EPS);   //false_easting
        EXPECT_NEAR(adfParams[6], 0, EPS);         //false_northing
    }
    {
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(28431);  // Pulkovo 1942 / Gauss-Kruger zone 31
        EXPECT_EQ(GetEPSGCode(oSRS), 28431);

        long iProjSys = 0;
        long iDatum = 0;
        long iEllips = 0;
        long iZone = 0;
        double adfParams[7] = {0};
        oSRS.exportToPanorama(&iProjSys, &iDatum, &iEllips, &iZone, adfParams);
        EXPECT_EQ(iProjSys, 1);
        EXPECT_EQ(iDatum, 1);
        EXPECT_EQ(iEllips, 1);
        EXPECT_EQ(iZone, 31);
        EXPECT_NEAR(adfParams[2], 0.0, EPS);         //latitude_of_origin
        EXPECT_NEAR(adfParams[3], -177 * RAD, EPS);  //central_meridian
        EXPECT_NEAR(adfParams[4], 1.0, EPS);         //scale_factor
        EXPECT_NEAR(adfParams[5], 31500000, EPS);    //false_easting
        EXPECT_NEAR(adfParams[6], 0, EPS);           //false_northing
    }
}
}  // namespace

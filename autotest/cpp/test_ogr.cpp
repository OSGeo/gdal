///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general OGR features.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "ogr_p.h"
#include "ogrsf_frmts.h"
#include "../../ogr/ogrsf_frmts/osm/gpb.h"
#include "ogr_recordbatch.h"
#include "ogrlayerarrow.h"

#include <string>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_ogr : public ::testing::Test
{
    std::string data_{tut::common::data_basedir};
    std::string data_tmp_{tut::common::tmp_basedir};
};

// Test OGR driver registrar access
TEST_F(test_ogr, GetGDALDriverManager)
{
    ASSERT_TRUE(nullptr != GetGDALDriverManager());
}

template <class T>
void testSpatialReferenceLeakOnCopy(OGRSpatialReference *poSRS)
{
    ASSERT_EQ(1, poSRS->GetReferenceCount());
    {
        int nCurCount;
        int nLastCount = 1;
        T value;
        value.assignSpatialReference(poSRS);
        nCurCount = poSRS->GetReferenceCount();
        ASSERT_GT(nCurCount, nLastCount);
        nLastCount = nCurCount;

        T value2(value);
        nCurCount = poSRS->GetReferenceCount();
        ASSERT_GT(nCurCount, nLastCount);
        nLastCount = nCurCount;

        T value3;
        value3 = value;
        nCurCount = poSRS->GetReferenceCount();
        ASSERT_GT(nCurCount, nLastCount);
        nLastCount = nCurCount;

        value3 = value;
        // avoid Coverity Scan warning about above assignment being better
        // replaced with a move.
        EXPECT_NE(value.getSpatialReference(), nullptr);
        ASSERT_EQ(nLastCount, poSRS->GetReferenceCount());
    }
    ASSERT_EQ(1, poSRS->GetReferenceCount());
}

// Test if copy does not leak or double delete the spatial reference
TEST_F(test_ogr, SpatialReference_leak)
{
    OGRSpatialReference *poSRS = new OGRSpatialReference();
    ASSERT_TRUE(nullptr != poSRS);

    testSpatialReferenceLeakOnCopy<OGRPoint>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRLineString>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRLinearRing>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRCircularString>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRCompoundCurve>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRCurvePolygon>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRPolygon>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRGeometryCollection>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRMultiSurface>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRMultiPolygon>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRMultiPoint>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRMultiCurve>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRMultiLineString>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRTriangle>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRPolyhedralSurface>(poSRS);
    testSpatialReferenceLeakOnCopy<OGRTriangulatedSurface>(poSRS);

    delete poSRS;

    // Check that assignSpatialReference() works when passed the SRS
    // object it already owns and whose has a single reference.
    poSRS = new OGRSpatialReference();
    OGRPoint oPoint;
    oPoint.assignSpatialReference(poSRS);
    poSRS->Release();
    oPoint.assignSpatialReference(oPoint.getSpatialReference());
}

template <class T> T *make();

template <> OGRPoint *make()
{
    return new OGRPoint(1.0, 2.0, 3.0);
}

template <> OGRLineString *make()
{
    OGRLineString *poLineString = new OGRLineString();

    poLineString->addPoint(1.0, 2.0, 3.0);
    poLineString->addPoint(1.1, 2.1, 3.1);
    poLineString->addPoint(1.2, 2.2, 3.2);

    return poLineString;
}

template <> OGRLinearRing *make()
{
    OGRLinearRing *poLinearRing = new OGRLinearRing();

    poLinearRing->addPoint(1.0, 2.0, 3.0);
    poLinearRing->addPoint(1.1, 2.1, 3.1);
    poLinearRing->addPoint(1.2, 2.2, 3.2);
    poLinearRing->addPoint(1.0, 2.0, 3.0);

    return poLinearRing;
}

template <> OGRCircularString *make()
{
    OGRCircularString *poCircularString = new OGRCircularString();

    poCircularString->addPoint(1.0, 2.0, 3.0);
    poCircularString->addPoint(1.1, 2.1, 3.1);
    poCircularString->addPoint(1.2, 2.2, 3.2);

    return poCircularString;
}

template <> OGRCompoundCurve *make()
{
    OGRCompoundCurve *poCompoundCurve = new OGRCompoundCurve();

    poCompoundCurve->addCurveDirectly(make<OGRLineString>());
    OGRCircularString *poCircularString = make<OGRCircularString>();
    poCircularString->reversePoints();
    poCompoundCurve->addCurveDirectly(poCircularString);

    return poCompoundCurve;
}

template <> OGRCurvePolygon *make()
{
    OGRCurvePolygon *poCurvePolygon = new OGRCurvePolygon();

    poCurvePolygon->addRingDirectly(make<OGRCompoundCurve>());
    poCurvePolygon->addRingDirectly(make<OGRCompoundCurve>());

    return poCurvePolygon;
}

template <> OGRPolygon *make()
{
    OGRPolygon *poPolygon = new OGRPolygon();

    poPolygon->addRingDirectly(make<OGRLinearRing>());
    poPolygon->addRingDirectly(make<OGRLinearRing>());

    return poPolygon;
}

template <> OGRGeometryCollection *make()
{
    OGRGeometryCollection *poCollection = new OGRGeometryCollection();

    poCollection->addGeometryDirectly(make<OGRPoint>());
    poCollection->addGeometryDirectly(make<OGRLinearRing>());

    return poCollection;
}

template <> OGRMultiSurface *make()
{
    OGRMultiSurface *poCollection = new OGRMultiSurface();

    poCollection->addGeometryDirectly(make<OGRPolygon>());
    poCollection->addGeometryDirectly(make<OGRCurvePolygon>());

    return poCollection;
}

template <> OGRMultiPolygon *make()
{
    OGRMultiPolygon *poCollection = new OGRMultiPolygon();

    poCollection->addGeometryDirectly(make<OGRPolygon>());

    return poCollection;
}

template <> OGRMultiPoint *make()
{
    OGRMultiPoint *poCollection = new OGRMultiPoint();

    poCollection->addGeometryDirectly(make<OGRPoint>());

    return poCollection;
}

template <> OGRMultiCurve *make()
{
    OGRMultiCurve *poCollection = new OGRMultiCurve();

    poCollection->addGeometryDirectly(make<OGRLineString>());
    poCollection->addGeometryDirectly(make<OGRCompoundCurve>());

    return poCollection;
}

template <> OGRMultiLineString *make()
{
    OGRMultiLineString *poCollection = new OGRMultiLineString();

    poCollection->addGeometryDirectly(make<OGRLineString>());
    poCollection->addGeometryDirectly(make<OGRLinearRing>());

    return poCollection;
}

template <> OGRTriangle *make()
{
    OGRPoint p1(0, 0), p2(0, 1), p3(1, 1);
    return new OGRTriangle(p1, p2, p3);
}

template <> OGRTriangulatedSurface *make()
{
    OGRTriangulatedSurface *poTS = new OGRTriangulatedSurface();
    poTS->addGeometryDirectly(make<OGRTriangle>());
    return poTS;
}

template <> OGRPolyhedralSurface *make()
{
    OGRPolyhedralSurface *poPS = new OGRPolyhedralSurface();
    poPS->addGeometryDirectly(make<OGRPolygon>());
    return poPS;
}

template <class T> void testCopyEquals()
{
    auto poOrigin = std::unique_ptr<T>(make<T>());
    ASSERT_TRUE(nullptr != poOrigin);

    T value2(*poOrigin);

    ASSERT_TRUE(CPL_TO_BOOL(poOrigin->Equals(&value2)))
        << poOrigin->getGeometryName() << ": copy constructor changed a value";

    T value3;
    value3 = *poOrigin;
    value3 = *poOrigin;
    auto &value3Ref(value3);
    value3 = value3Ref;

#ifdef DEBUG_VERBOSE
    char *wkt1 = NULL, *wkt2 = NULL;
    poOrigin->exportToWkt(&wkt1);
    value3.exportToWkt(&wkt2);
    printf("%s %s\n", wkt1, wkt2);
    CPLFree(wkt1);
    CPLFree(wkt2);
#endif
    ASSERT_TRUE(CPL_TO_BOOL(poOrigin->Equals(&value3)))
        << poOrigin->getGeometryName()
        << ": assignment operator changed a value";

    value3 = T();
    ASSERT_TRUE(value3.IsEmpty());
}

// Test if copy constructor and assignment operators succeeds on copying the
// geometry data
TEST_F(test_ogr, SpatialReference_leak_copy_constructor)
{
    testCopyEquals<OGRPoint>();
    testCopyEquals<OGRLineString>();
    testCopyEquals<OGRLinearRing>();
    testCopyEquals<OGRCircularString>();
    testCopyEquals<OGRCompoundCurve>();
    testCopyEquals<OGRCurvePolygon>();
    testCopyEquals<OGRPolygon>();
    testCopyEquals<OGRGeometryCollection>();
    testCopyEquals<OGRMultiSurface>();
    testCopyEquals<OGRMultiPolygon>();
    testCopyEquals<OGRMultiPoint>();
    testCopyEquals<OGRMultiCurve>();
    testCopyEquals<OGRMultiLineString>();
    testCopyEquals<OGRTriangle>();
    testCopyEquals<OGRPolyhedralSurface>();
    testCopyEquals<OGRTriangulatedSurface>();
}

// Test crazy usage of OGRGeometryCollection copy constructor
TEST_F(test_ogr, OGRGeometryCollection_copy_constructor_illegal_use)
{
    OGRGeometryCollection gc;
    gc.addGeometryDirectly(new OGRPoint(1, 2));

    OGRMultiPolygon mp;
    mp.addGeometryDirectly(new OGRPolygon());

    OGRGeometryCollection *mp_as_gc = &mp;
    CPLErrorReset();
    {
        CPLErrorHandlerPusher oPusher(CPLQuietErrorHandler);
        *mp_as_gc = gc;
        // avoid Coverity Scan warning
        EXPECT_EQ(gc.getSpatialReference(), nullptr);
    }
    EXPECT_STREQ(CPLGetLastErrorMsg(),
                 "Illegal use of OGRGeometryCollection::operator=(): trying to "
                 "assign an incompatible sub-geometry");
    EXPECT_TRUE(mp.IsEmpty());
}

// Test crazy usage of OGRCurvePolygon copy constructor
TEST_F(test_ogr, OGRCurvePolygon_copy_constructor_illegal_use)
{
    OGRCurvePolygon cp;
    auto poCC = new OGRCircularString();
    poCC->addPoint(0, 0);
    poCC->addPoint(1, 1);
    poCC->addPoint(2, 0);
    poCC->addPoint(1, -1);
    poCC->addPoint(0, 0);
    cp.addRingDirectly(poCC);

    OGRPolygon poly;
    auto poLR = new OGRLinearRing();
    poLR->addPoint(0, 0);
    poLR->addPoint(1, 1);
    poLR->addPoint(2, 0);
    poLR->addPoint(1, -1);
    poLR->addPoint(0, 0);
    poly.addRingDirectly(poLR);

    OGRCurvePolygon *poly_as_cp = &poly;
    CPLErrorReset();
    {
        CPLErrorHandlerPusher oPusher(CPLQuietErrorHandler);
        *poly_as_cp = cp;
        // avoid Coverity Scan warning
        EXPECT_EQ(cp.getSpatialReference(), nullptr);
    }
    EXPECT_STREQ(CPLGetLastErrorMsg(),
                 "Illegal use of OGRCurvePolygon::operator=(): trying to "
                 "assign an incompatible sub-geometry");
    EXPECT_TRUE(poly.IsEmpty());
}

template <class T> void testMove()
{
    auto poSRS = new OGRSpatialReference();
    {
        auto poOrigin = std::unique_ptr<T>(make<T>());
        ASSERT_TRUE(nullptr != poOrigin);
        poOrigin->assignSpatialReference(poSRS);

        T valueCopy(*poOrigin);
        const int refCountBefore = poSRS->GetReferenceCount();
        T fromMoved(std::move(*poOrigin));
        EXPECT_EQ(poSRS->GetReferenceCount(), refCountBefore);

        ASSERT_TRUE(CPL_TO_BOOL(fromMoved.Equals(&valueCopy)))
            << valueCopy.getGeometryName()
            << ": move constructor changed a value";
        EXPECT_EQ(fromMoved.getSpatialReference(), poSRS);

        T valueCopy2(valueCopy);
        EXPECT_EQ(valueCopy.getSpatialReference(), poSRS);
        T value3;
        const int refCountBefore2 = poSRS->GetReferenceCount();
        value3 = std::move(valueCopy);
        EXPECT_EQ(poSRS->GetReferenceCount(), refCountBefore2);

        ASSERT_TRUE(CPL_TO_BOOL(value3.Equals(&valueCopy2)))
            << valueCopy2.getGeometryName()
            << ": move assignment operator changed a value";
        EXPECT_EQ(value3.getSpatialReference(), poSRS);
    }
    EXPECT_EQ(poSRS->GetReferenceCount(), 1);
    poSRS->Release();
}

TEST_F(test_ogr, geometry_move)
{
    testMove<OGRPoint>();
    testMove<OGRLineString>();
    testMove<OGRLinearRing>();
    testMove<OGRCircularString>();
    testMove<OGRCompoundCurve>();
    testMove<OGRCurvePolygon>();
    testMove<OGRPolygon>();
    testMove<OGRGeometryCollection>();
    testMove<OGRMultiSurface>();
    testMove<OGRMultiPolygon>();
    testMove<OGRMultiPoint>();
    testMove<OGRMultiCurve>();
    testMove<OGRMultiLineString>();
    testMove<OGRTriangle>();
    testMove<OGRPolyhedralSurface>();
    testMove<OGRTriangulatedSurface>();
}

TEST_F(test_ogr, geometry_get_point)
{
    {
        OGRPoint p;
        double x = 1, y = 2;
        OGR_G_SetPoints((OGRGeometryH)&p, 1, &x, 0, &y, 0, nullptr, 0);
        ASSERT_EQ(p.getCoordinateDimension(), 2);
        ASSERT_EQ(p.getX(), 1);
        ASSERT_EQ(p.getY(), 2);
        ASSERT_EQ(p.getZ(), 0);
    }

    {
        OGRPoint p;
        double x = 1, y = 2, z = 3;
        OGR_G_SetPoints((OGRGeometryH)&p, 1, &x, 0, &y, 0, &z, 0);
        ASSERT_EQ(p.getCoordinateDimension(), 3);
        ASSERT_EQ(p.getX(), 1);
        ASSERT_EQ(p.getY(), 2);
        ASSERT_EQ(p.getZ(), 3);
    }

    {
        OGRPoint p;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGR_G_SetPoints((OGRGeometryH)&p, 1, nullptr, 0, nullptr, 0, nullptr,
                        0);
        CPLPopErrorHandler();
    }

    {
        OGRLineString ls;
        double x = 1, y = 2;
        OGR_G_SetPoints((OGRGeometryH)&ls, 1, &x, 0, &y, 0, nullptr, 0);
        ASSERT_EQ(ls.getCoordinateDimension(), 2);
        ASSERT_EQ(ls.getX(0), 1);
        ASSERT_EQ(ls.getY(0), 2);
        ASSERT_EQ(ls.getZ(0), 0);
    }

    {
        OGRLineString ls;
        double x = 1, y = 2;
        OGR_G_SetPoints((OGRGeometryH)&ls, 1, &x, 0, &y, 0, nullptr, 0);
        ASSERT_EQ(ls.getCoordinateDimension(), 2);
        ASSERT_EQ(ls.getX(0), 1);
        ASSERT_EQ(ls.getY(0), 2);
        ASSERT_EQ(ls.getZ(0), 0);
    }

    {
        OGRLineString ls;
        double x = 1, y = 2;
        OGR_G_SetPoints((OGRGeometryH)&ls, 1, &x, 8, &y, 8, nullptr, 0);
        ASSERT_EQ(ls.getCoordinateDimension(), 2);
        ASSERT_EQ(ls.getX(0), 1);
        ASSERT_EQ(ls.getY(0), 2);
        ASSERT_EQ(ls.getZ(0), 0);
    }

    {
        OGRLineString ls;
        double x = 1, y = 2, z = 3;
        OGR_G_SetPoints((OGRGeometryH)&ls, 1, &x, 0, &y, 0, &z, 0);
        ASSERT_EQ(ls.getCoordinateDimension(), 3);
        ASSERT_EQ(ls.getX(0), 1);
        ASSERT_EQ(ls.getY(0), 2);
        ASSERT_EQ(ls.getZ(0), 3);
    }

    {
        OGRLineString ls;
        double x = 1, y = 2, z = 3;
        OGR_G_SetPoints((OGRGeometryH)&ls, 1, &x, 8, &y, 8, &z, 8);
        ASSERT_EQ(ls.getCoordinateDimension(), 3);
        ASSERT_EQ(ls.getX(0), 1);
        ASSERT_EQ(ls.getY(0), 2);
        ASSERT_EQ(ls.getZ(0), 3);
    }

    {
        OGRLineString ls;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGR_G_SetPoints((OGRGeometryH)&ls, 1, nullptr, 0, nullptr, 0, nullptr,
                        0);
        CPLPopErrorHandler();
    }
}

TEST_F(test_ogr, OGR_G_CreateGeometry_unknown)
{
    EXPECT_EQ(OGR_G_CreateGeometry(wkbUnknown), nullptr);
}

TEST_F(test_ogr, style_manager)
{
    OGRStyleMgrH hSM = OGR_SM_Create(nullptr);
    EXPECT_TRUE(OGR_SM_InitStyleString(
        hSM, "PEN(w:2px,c:#000000,id:\"mapinfo-pen-2,ogr-pen-0\")"));
    OGRStyleToolH hTool = OGR_SM_GetPart(hSM, 0, nullptr);
    EXPECT_TRUE(hTool != nullptr);
    if (hTool)
    {
        int bValueIsNull;

        EXPECT_NEAR(OGR_ST_GetParamDbl(hTool, OGRSTPenWidth, &bValueIsNull),
                    2.0 * (1.0 / (72.0 * 39.37)) * 1000, 1e-6);
        EXPECT_EQ(OGR_ST_GetUnit(hTool), OGRSTUMM);

        OGR_ST_SetUnit(hTool, OGRSTUPixel, 1.0);
        EXPECT_EQ(OGR_ST_GetParamDbl(hTool, OGRSTPenWidth, &bValueIsNull), 2.0);
        EXPECT_EQ(OGR_ST_GetUnit(hTool), OGRSTUPixel);
        OGR_ST_Destroy(hTool);
    }

    OGR_SM_Destroy(hSM);
}

TEST_F(test_ogr, OGRParseDate)
{
    const auto OGRParseDateWrapper =
        [](const char *str, OGRField *psField, int nFlags)
    {
        // Putting inside a std::string helps Valgrind and other checkers to
        // detect out-of-bounds access
        return OGRParseDate(std::string(str).c_str(), psField, nFlags);
    };

    OGRField sField;
    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:56", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 11);
    EXPECT_EQ(sField.Date.Day, 31);
    EXPECT_EQ(sField.Date.Hour, 12);
    EXPECT_EQ(sField.Date.Minute, 34);
    EXPECT_EQ(sField.Date.Second, 56.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:56+00", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.TZFlag, 100);

    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:56+12:00", &sField, 0),
              TRUE);
    EXPECT_EQ(sField.Date.TZFlag, 100 + 12 * 4);

    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:56+1200", &sField, 0),
              TRUE);
    EXPECT_EQ(sField.Date.TZFlag, 100 + 12 * 4);

    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:56+815", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.TZFlag, 100 + 8 * 4 + 1);

    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:56-12:00", &sField, 0),
              TRUE);
    EXPECT_EQ(sField.Date.TZFlag, 100 - 12 * 4);

    EXPECT_EQ(OGRParseDateWrapper(" 2017/11/31 12:34:56", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);

    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:56.789", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Second, 56.789f);

    // Leap second
    EXPECT_EQ(OGRParseDateWrapper("2017/11/31 12:34:60", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Second, 60.0f);

    EXPECT_EQ(OGRParseDateWrapper("2017-11-31T12:34:56", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 11);
    EXPECT_EQ(sField.Date.Day, 31);
    EXPECT_EQ(sField.Date.Hour, 12);
    EXPECT_EQ(sField.Date.Minute, 34);
    EXPECT_EQ(sField.Date.Second, 56.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("2017-11-31T12:34:56Z", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Second, 56.0f);
    EXPECT_EQ(sField.Date.TZFlag, 100);

    EXPECT_EQ(OGRParseDateWrapper("2017-11-31T12:34:56.789Z", &sField, 0),
              TRUE);
    EXPECT_EQ(sField.Date.Second, 56.789f);
    EXPECT_EQ(sField.Date.TZFlag, 100);

    EXPECT_EQ(OGRParseDateWrapper("2017-11-31T23:59:59.999999Z", &sField, 0),
              TRUE);
    EXPECT_EQ(sField.Date.Hour, 23);
    EXPECT_EQ(sField.Date.Minute, 59);
    EXPECT_EQ(sField.Date.Second, 59.999f);

    EXPECT_EQ(OGRParseDateWrapper("2017-11-31", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 11);
    EXPECT_EQ(sField.Date.Day, 31);
    EXPECT_EQ(sField.Date.Hour, 0);
    EXPECT_EQ(sField.Date.Minute, 0);
    EXPECT_EQ(sField.Date.Second, 0.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("2017-11-31Z", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 11);
    EXPECT_EQ(sField.Date.Day, 31);
    EXPECT_EQ(sField.Date.Hour, 0);
    EXPECT_EQ(sField.Date.Minute, 0);
    EXPECT_EQ(sField.Date.Second, 0.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("12:34", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 0);
    EXPECT_EQ(sField.Date.Month, 0);
    EXPECT_EQ(sField.Date.Day, 0);
    EXPECT_EQ(sField.Date.Hour, 12);
    EXPECT_EQ(sField.Date.Minute, 34);
    EXPECT_EQ(sField.Date.Second, 0.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("12:34:56", &sField, 0), TRUE);
    EXPECT_EQ(OGRParseDateWrapper("12:34:56.789", &sField, 0), TRUE);

    EXPECT_EQ(OGRParseDateWrapper("T12:34:56", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 0);
    EXPECT_EQ(sField.Date.Month, 0);
    EXPECT_EQ(sField.Date.Day, 0);
    EXPECT_EQ(sField.Date.Hour, 12);
    EXPECT_EQ(sField.Date.Minute, 34);
    EXPECT_EQ(sField.Date.Second, 56.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("T123456", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 0);
    EXPECT_EQ(sField.Date.Month, 0);
    EXPECT_EQ(sField.Date.Day, 0);
    EXPECT_EQ(sField.Date.Hour, 12);
    EXPECT_EQ(sField.Date.Minute, 34);
    EXPECT_EQ(sField.Date.Second, 56.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("T123456.789", &sField, 0), TRUE);
    EXPECT_EQ(sField.Date.Year, 0);
    EXPECT_EQ(sField.Date.Month, 0);
    EXPECT_EQ(sField.Date.Day, 0);
    EXPECT_EQ(sField.Date.Hour, 12);
    EXPECT_EQ(sField.Date.Minute, 34);
    EXPECT_EQ(sField.Date.Second, 56.789f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!OGRParseDateWrapper("123456-01-01", &sField, 0));
    CPLPopErrorHandler();
    EXPECT_TRUE(!OGRParseDateWrapper("2017", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017x-01-01", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-1-01", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-1", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01x", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("12:", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("12:3", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("1:23", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("12:34:5", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("1a:34", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-a-31T12:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-00-31T12:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-13-31T12:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-00T12:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-aT12:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-32T12:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("a:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01Ta:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01T25:34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01T00:a:00", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01T00: 34:56", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01T00:61:00", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01T00:00:61", &sField, 0));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-01-01T00:00:a", &sField, 0));

    // Test OGRPARSEDATE_OPTION_LAX
    EXPECT_EQ(OGRParseDateWrapper("2017-1-9", &sField, OGRPARSEDATE_OPTION_LAX),
              TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 1);
    EXPECT_EQ(sField.Date.Day, 9);

    EXPECT_EQ(
        OGRParseDateWrapper("2017-1-31", &sField, OGRPARSEDATE_OPTION_LAX),
        TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 1);
    EXPECT_EQ(sField.Date.Day, 31);

    EXPECT_EQ(OGRParseDateWrapper("2017-1-31T1:2:3", &sField,
                                  OGRPARSEDATE_OPTION_LAX),
              TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 1);
    EXPECT_EQ(sField.Date.Day, 31);
    EXPECT_EQ(sField.Date.Hour, 1);
    EXPECT_EQ(sField.Date.Minute, 2);
    EXPECT_EQ(sField.Date.Second, 3.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(
        OGRParseDateWrapper("2017-1-31T1:3", &sField, OGRPARSEDATE_OPTION_LAX),
        TRUE);
    EXPECT_EQ(sField.Date.Year, 2017);
    EXPECT_EQ(sField.Date.Month, 1);
    EXPECT_EQ(sField.Date.Day, 31);
    EXPECT_EQ(sField.Date.Hour, 1);
    EXPECT_EQ(sField.Date.Minute, 3);
    EXPECT_EQ(sField.Date.Second, 0.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_EQ(OGRParseDateWrapper("1:3", &sField, OGRPARSEDATE_OPTION_LAX),
              TRUE);
    EXPECT_EQ(sField.Date.Year, 0);
    EXPECT_EQ(sField.Date.Month, 0);
    EXPECT_EQ(sField.Date.Day, 0);
    EXPECT_EQ(sField.Date.Hour, 1);
    EXPECT_EQ(sField.Date.Minute, 3);
    EXPECT_EQ(sField.Date.Second, 0.0f);
    EXPECT_EQ(sField.Date.TZFlag, 0);

    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-a-01", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-0-01", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-1", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-1-", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-1-a", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-1-0", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-1-32", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-1-1Ta:00:00", &sField,
                                     OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-1-1T1", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-1-1T00:a:00", &sField,
                                     OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(
        !OGRParseDateWrapper("2017-1-1T1:", &sField, OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(!OGRParseDateWrapper("2017-1-1T00:00:a", &sField,
                                     OGRPARSEDATE_OPTION_LAX));
    EXPECT_TRUE(!OGRParseDateWrapper("1a:3", &sField, OGRPARSEDATE_OPTION_LAX));
}

// Test OGRPolygon::IsPointOnSurface()
TEST_F(test_ogr, IsPointOnSurface)
{
    OGRPolygon oPoly;

    OGRPoint oEmptyPoint;
    ASSERT_TRUE(!oPoly.IsPointOnSurface(&oEmptyPoint));

    OGRPoint oPoint;
    oPoint.setX(1);
    oPoint.setY(1);
    ASSERT_TRUE(!oPoly.IsPointOnSurface(&oPoint));

    const char *pszPoly =
        "POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))";
    oPoly.importFromWkt(&pszPoly);

    ASSERT_TRUE(!oPoly.IsPointOnSurface(&oEmptyPoint));

    ASSERT_EQ(oPoly.IsPointOnSurface(&oPoint), TRUE);

    oPoint.setX(5);
    oPoint.setY(5);
    ASSERT_TRUE(!oPoly.IsPointOnSurface(&oPoint));
}

// Test gpb.h
TEST_F(test_ogr, gpb_h)
{
    ASSERT_EQ(GetVarUIntSize(0), 1);
    ASSERT_EQ(GetVarUIntSize(127), 1);
    ASSERT_EQ(GetVarUIntSize(128), 2);
    ASSERT_EQ(GetVarUIntSize((1 << 14) - 1), 2);
    ASSERT_EQ(GetVarUIntSize(1 << 14), 3);
    ASSERT_EQ(GetVarUIntSize(GUINT64_MAX), 10);

    ASSERT_EQ(GetVarIntSize(0), 1);
    ASSERT_EQ(GetVarIntSize(127), 1);
    ASSERT_EQ(GetVarIntSize(128), 2);
    ASSERT_EQ(GetVarIntSize((1 << 14) - 1), 2);
    ASSERT_EQ(GetVarIntSize(1 << 14), 3);
    ASSERT_EQ(GetVarIntSize(GINT64_MAX), 9);
    ASSERT_EQ(GetVarIntSize(-1), 10);
    ASSERT_EQ(GetVarIntSize(GINT64_MIN), 10);

    ASSERT_EQ(GetVarSIntSize(0), 1);
    ASSERT_EQ(GetVarSIntSize(63), 1);
    ASSERT_EQ(GetVarSIntSize(64), 2);
    ASSERT_EQ(GetVarSIntSize(-1), 1);
    ASSERT_EQ(GetVarSIntSize(-64), 1);
    ASSERT_EQ(GetVarSIntSize(-65), 2);
    ASSERT_EQ(GetVarSIntSize(GINT64_MIN), 10);
    ASSERT_EQ(GetVarSIntSize(GINT64_MAX), 10);

    ASSERT_EQ(GetTextSize(""), 1);
    ASSERT_EQ(GetTextSize(" "), 2);
    ASSERT_EQ(GetTextSize(std::string(" ")), 2);

    GByte abyBuffer[11] = {0};
    GByte *pabyBuffer;
    const GByte *pabyBufferRO;

    pabyBuffer = abyBuffer;
    WriteVarUInt(&pabyBuffer, 0);
    ASSERT_EQ(pabyBuffer - abyBuffer, 1);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadVarUInt64(&pabyBufferRO), 0U);

    pabyBuffer = abyBuffer;
    WriteVarUInt(&pabyBuffer, 127);
    ASSERT_EQ(pabyBuffer - abyBuffer, 1);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadVarUInt64(&pabyBufferRO), 127U);

    pabyBuffer = abyBuffer;
    WriteVarUInt(&pabyBuffer, 0xDEADBEEFU);
    ASSERT_EQ(pabyBuffer - abyBuffer, 5);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadVarUInt64(&pabyBufferRO), 0xDEADBEEFU);

    pabyBuffer = abyBuffer;
    WriteVarUInt(&pabyBuffer, GUINT64_MAX);
    ASSERT_EQ(pabyBuffer - abyBuffer, 10);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadVarUInt64(&pabyBufferRO), GUINT64_MAX);

    pabyBuffer = abyBuffer;
    WriteVarInt(&pabyBuffer, GINT64_MAX);
    ASSERT_EQ(pabyBuffer - abyBuffer, 9);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadVarInt64(&pabyBufferRO), GINT64_MAX);

    pabyBuffer = abyBuffer;
    WriteVarInt(&pabyBuffer, -1);
    ASSERT_EQ(pabyBuffer - abyBuffer, 10);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadVarInt64(&pabyBufferRO), -1);

    pabyBuffer = abyBuffer;
    WriteVarInt(&pabyBuffer, GINT64_MIN);
    ASSERT_EQ(pabyBuffer - abyBuffer, 10);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadVarInt64(&pabyBufferRO), GINT64_MIN);

    pabyBuffer = abyBuffer;
    WriteVarSInt(&pabyBuffer, 0);
    ASSERT_EQ(pabyBuffer - abyBuffer, 1);
    {
        GIntBig nVal;
        pabyBufferRO = abyBuffer;
        READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
        ASSERT_EQ(nVal, 0);
    }

    pabyBuffer = abyBuffer;
    WriteVarSInt(&pabyBuffer, 1);
    ASSERT_EQ(pabyBuffer - abyBuffer, 1);
    {
        GIntBig nVal;
        pabyBufferRO = abyBuffer;
        READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
        ASSERT_EQ(nVal, 1);
    }

    pabyBuffer = abyBuffer;
    WriteVarSInt(&pabyBuffer, -1);
    ASSERT_EQ(pabyBuffer - abyBuffer, 1);
    {
        GIntBig nVal;
        pabyBufferRO = abyBuffer;
        READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
        ASSERT_EQ(nVal, -1);
    }

    pabyBuffer = abyBuffer;
    WriteVarSInt(&pabyBuffer, GINT64_MAX);
    ASSERT_EQ(pabyBuffer - abyBuffer, 10);
    {
        GIntBig nVal;
        pabyBufferRO = abyBuffer;
        READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
        ASSERT_EQ(nVal, GINT64_MAX);
    }

    pabyBuffer = abyBuffer;
    WriteVarSInt(&pabyBuffer, GINT64_MIN);
    ASSERT_EQ(pabyBuffer - abyBuffer, 10);
    {
        GIntBig nVal;
        pabyBufferRO = abyBuffer;
        READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
        ASSERT_EQ(nVal, GINT64_MIN);
    }

    pabyBuffer = abyBuffer;
    WriteText(&pabyBuffer, "x");
    ASSERT_EQ(pabyBuffer - abyBuffer, 2);
    ASSERT_EQ(abyBuffer[0], 1);
    ASSERT_EQ(abyBuffer[1], 'x');

    pabyBuffer = abyBuffer;
    WriteText(&pabyBuffer, std::string("x"));
    ASSERT_EQ(pabyBuffer - abyBuffer, 2);
    ASSERT_EQ(abyBuffer[0], 1);
    ASSERT_EQ(abyBuffer[1], 'x');

    pabyBuffer = abyBuffer;
    WriteFloat32(&pabyBuffer, 1.25f);
    ASSERT_EQ(pabyBuffer - abyBuffer, 4);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadFloat32(&pabyBufferRO, abyBuffer + 4), 1.25f);

    pabyBuffer = abyBuffer;
    WriteFloat64(&pabyBuffer, 1.25);
    ASSERT_EQ(pabyBuffer - abyBuffer, 8);
    pabyBufferRO = abyBuffer;
    ASSERT_EQ(ReadFloat64(&pabyBufferRO, abyBuffer + 8), 1.25);
}

// Test OGRGeometry::toXXXXX()
TEST_F(test_ogr, OGRGeometry_toXXXXX)
{
#define CONCAT(X, Y) X##Y
#define TEST_OGRGEOMETRY_TO(X)                                                 \
    {                                                                          \
        CONCAT(OGR, X) o;                                                      \
        OGRGeometry *poGeom = &o;                                              \
        ASSERT_EQ(poGeom->CONCAT(to, X)(), &o);                                \
    }

    TEST_OGRGEOMETRY_TO(Point);
    TEST_OGRGEOMETRY_TO(LineString);
    TEST_OGRGEOMETRY_TO(LinearRing);
    TEST_OGRGEOMETRY_TO(CircularString);
    TEST_OGRGEOMETRY_TO(CompoundCurve);
    TEST_OGRGEOMETRY_TO(CurvePolygon);
    TEST_OGRGEOMETRY_TO(Polygon);
    TEST_OGRGEOMETRY_TO(GeometryCollection);
    TEST_OGRGEOMETRY_TO(MultiSurface);
    TEST_OGRGEOMETRY_TO(MultiPolygon);
    TEST_OGRGEOMETRY_TO(MultiPoint);
    TEST_OGRGEOMETRY_TO(MultiCurve);
    TEST_OGRGEOMETRY_TO(MultiLineString);
    TEST_OGRGEOMETRY_TO(Triangle);
    TEST_OGRGEOMETRY_TO(PolyhedralSurface);
    TEST_OGRGEOMETRY_TO(TriangulatedSurface);
    {
        OGRLineString o;
        OGRGeometry *poGeom = &o;
        ASSERT_EQ(poGeom->toCurve(), &o);
    }
    {
        OGRPolygon o;
        OGRGeometry *poGeom = &o;
        ASSERT_EQ(poGeom->toSurface(), &o);
    }

    {
        OGRPoint o;
        // ASSERT_EQ(o.toPoint(), &o);
    }

    {
        OGRLineString o;
        ASSERT_EQ(o.toCurve(), &o);
        ASSERT_EQ(o.toSimpleCurve(), &o);
        // ASSERT_EQ(o.toLineString(), &o);

        {
            OGRCurve &oRef = o;
            ASSERT_EQ(oRef.toLineString(), &o);
        }

        {
            OGRSimpleCurve &oRef = o;
            ASSERT_EQ(oRef.toLineString(), &o);
        }
    }

    {
        OGRLinearRing o;
        ASSERT_EQ(o.toCurve(), &o);
        ASSERT_EQ(o.toSimpleCurve(), &o);
        // ASSERT_EQ(o.toLinearRing(), &o);

        {
            OGRCurve &oRef = o;
            ASSERT_EQ(oRef.toLinearRing(), &o);
        }
        {
            OGRSimpleCurve &oRef = o;
            ASSERT_EQ(oRef.toLinearRing(), &o);
        }
        {
            OGRLineString &oRef = o;
            ASSERT_EQ(oRef.toLinearRing(), &o);
        }
    }

    {
        OGRCircularString o;
        ASSERT_EQ(o.toCurve(), &o);
        ASSERT_EQ(o.toSimpleCurve(), &o);
        // ASSERT_EQ(o.toCircularString(), &o);

        {
            OGRCurve &oRef = o;
            ASSERT_EQ(oRef.toCircularString(), &o);
        }

        {
            OGRSimpleCurve &oRef = o;
            ASSERT_EQ(oRef.toCircularString(), &o);
        }
    }

    {
        OGRCompoundCurve o;
        ASSERT_EQ(o.toCurve(), &o);
        // ASSERT_EQ(o.toCompoundCurve(), &o);

        {
            OGRCurve &oRef = o;
            ASSERT_EQ(oRef.toCompoundCurve(), &o);
        }
    }

    {
        OGRCurvePolygon o;
        ASSERT_EQ(o.toSurface(), &o);
        // ASSERT_EQ(o.toCurvePolygon(), &o);

        {
            OGRSurface &oRef = o;
            ASSERT_EQ(oRef.toCurvePolygon(), &o);
        }
    }

    {
        OGRPolygon o;
        ASSERT_EQ(o.toSurface(), &o);
        ASSERT_EQ(o.toCurvePolygon(), &o);
        // ASSERT_EQ(o.toPolygon(), &o);

        {
            OGRSurface &oRef = o;
            ASSERT_EQ(oRef.toPolygon(), &o);
        }

        {
            OGRCurvePolygon &oRef = o;
            ASSERT_EQ(oRef.toPolygon(), &o);
        }
    }

    {
        OGRTriangle o;
        ASSERT_EQ(o.toSurface(), &o);
        ASSERT_EQ(o.toCurvePolygon(), &o);
        ASSERT_EQ(o.toPolygon(), &o);
        // ASSERT_EQ(o.toTriangle(), &o);

        {
            OGRSurface &oRef = o;
            ASSERT_EQ(oRef.toTriangle(), &o);
        }

        {
            OGRCurvePolygon &oRef = o;
            ASSERT_EQ(oRef.toTriangle(), &o);
        }

        {
            OGRPolygon &oRef = o;
            ASSERT_EQ(oRef.toTriangle(), &o);
        }
    }

    {
        OGRMultiPoint o;
        ASSERT_EQ(o.toGeometryCollection(), &o);
        // ASSERT_EQ(o.toMultiPoint(), &o);

        {
            OGRGeometryCollection &oRef = o;
            ASSERT_EQ(oRef.toMultiPoint(), &o);
        }
    }

    {
        OGRMultiCurve o;
        ASSERT_EQ(o.toGeometryCollection(), &o);
        // ASSERT_EQ(o.toMultiCurve(), &o);

        {
            OGRGeometryCollection &oRef = o;
            ASSERT_EQ(oRef.toMultiCurve(), &o);
        }
    }

    {
        OGRMultiLineString o;
        ASSERT_EQ(o.toGeometryCollection(), &o);
        ASSERT_EQ(o.toMultiCurve(), &o);
        // ASSERT_EQ(o.toMultiLineString(), &o);

        {
            OGRMultiCurve &oRef = o;
            ASSERT_EQ(oRef.toMultiLineString(), &o);
        }

        {
            OGRGeometryCollection &oRef = o;
            ASSERT_EQ(oRef.toMultiLineString(), &o);
        }
    }

    {
        OGRMultiSurface o;
        ASSERT_EQ(o.toGeometryCollection(), &o);
        // ASSERT_EQ(o.toMultiSurface(), &o);

        {
            OGRGeometryCollection &oRef = o;
            ASSERT_EQ(oRef.toMultiSurface(), &o);
        }
    }

    {
        OGRMultiPolygon o;
        ASSERT_EQ(o.toGeometryCollection(), &o);
        ASSERT_EQ(o.toMultiSurface(), &o);
        // ASSERT_EQ(o.toMultiPolygon(), &o);

        {
            OGRMultiSurface &oRef = o;
            ASSERT_EQ(oRef.toMultiPolygon(), &o);
        }

        {
            OGRGeometryCollection &oRef = o;
            ASSERT_EQ(oRef.toMultiPolygon(), &o);
        }
    }

    {
        OGRPolyhedralSurface o;
        ASSERT_EQ(o.toSurface(), &o);
        // ASSERT_EQ(o.toPolyhedralSurface(), &o);

        {
            OGRSurface &oRef = o;
            ASSERT_EQ(oRef.toPolyhedralSurface(), &o);
        }
    }

    {
        OGRTriangulatedSurface o;
        ASSERT_EQ(o.toSurface(), &o);
        ASSERT_EQ(o.toPolyhedralSurface(), &o);
        // ASSERT_EQ(o.toTriangulatedSurface(), &o);

        {
            OGRSurface &oRef = o;
            ASSERT_EQ(oRef.toTriangulatedSurface(), &o);
        }

        {
            OGRPolyhedralSurface &oRef = o;
            ASSERT_EQ(oRef.toTriangulatedSurface(), &o);
        }
    }
}

template <typename T> void TestIterator(T *obj, int nExpectedPointCount)
{
    int nCount = 0;
    for (auto &elt : obj)
    {
        nCount++;
        CPL_IGNORE_RET_VAL(elt);
    }
    ASSERT_EQ(nCount, nExpectedPointCount);

    nCount = 0;
    const T *const_obj(obj);
    for (const auto &elt : const_obj)
    {
        nCount++;
        CPL_IGNORE_RET_VAL(elt);
    }
    ASSERT_EQ(nCount, nExpectedPointCount);
}

template <typename Concrete, typename Abstract = Concrete>
void TestIterator(const char *pszWKT = nullptr, int nExpectedPointCount = 0)
{
    Concrete obj;
    if (pszWKT)
    {
        obj.importFromWkt(&pszWKT);
    }
    TestIterator<Abstract>(&obj, nExpectedPointCount);
}

// Test geometry visitor
TEST_F(test_ogr, OGRGeometry_visitor)
{
    static const struct
    {
        const char *pszWKT;
        int nExpectedPointCount;
    } asTests[] = {
        {"POINT(0 0)", 1},
        {"LINESTRING(0 0)", 1},
        {"POLYGON((0 0),(0 0))", 2},
        {"MULTIPOINT(0 0)", 1},
        {"MULTILINESTRING((0 0))", 1},
        {"MULTIPOLYGON(((0 0)))", 1},
        {"GEOMETRYCOLLECTION(POINT(0 0))", 1},
        {"CIRCULARSTRING(0 0,1 1,0 0)", 3},
        {"COMPOUNDCURVE((0 0,1 1))", 2},
        {"CURVEPOLYGON((0 0,1 1,1 0,0 0))", 4},
        {"MULTICURVE((0 0))", 1},
        {"MULTISURFACE(((0 0)))", 1},
        {"TRIANGLE((0 0,0 1,1 1,0 0))", 4},
        {"POLYHEDRALSURFACE(((0 0,0 1,1 1,0 0)))", 4},
        {"TIN(((0 0,0 1,1 1,0 0)))", 4},
    };

    class PointCounterVisitor : public OGRDefaultGeometryVisitor
    {
        int m_nPoints = 0;

      public:
        PointCounterVisitor()
        {
        }

        using OGRDefaultGeometryVisitor::visit;

        void visit(OGRPoint *) override
        {
            m_nPoints++;
        }

        int getNumPoints() const
        {
            return m_nPoints;
        }
    };

    class PointCounterConstVisitor : public OGRDefaultConstGeometryVisitor
    {
        int m_nPoints = 0;

      public:
        PointCounterConstVisitor()
        {
        }

        using OGRDefaultConstGeometryVisitor::visit;

        void visit(const OGRPoint *) override
        {
            m_nPoints++;
        }

        int getNumPoints() const
        {
            return m_nPoints;
        }
    };

    for (size_t i = 0; i < CPL_ARRAYSIZE(asTests); i++)
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(asTests[i].pszWKT, nullptr, &poGeom);
        PointCounterVisitor oVisitor;
        poGeom->accept(&oVisitor);
        ASSERT_EQ(oVisitor.getNumPoints(), asTests[i].nExpectedPointCount);
        PointCounterConstVisitor oConstVisitor;
        poGeom->accept(&oConstVisitor);
        ASSERT_EQ(oConstVisitor.getNumPoints(), asTests[i].nExpectedPointCount);
        delete poGeom;
    }

    {
        OGRLineString ls;
        ls.setNumPoints(2);
        auto oIter1 = ls.begin();
        EXPECT_TRUE(oIter1 != ls.end());
        EXPECT_TRUE(!(oIter1 != ls.begin()));
        auto oIter2 = ls.begin();
        EXPECT_TRUE(!(oIter1 != oIter2));
        ++oIter2;
        EXPECT_TRUE(oIter1 != oIter2);
        ++oIter2;
        EXPECT_TRUE(oIter1 != oIter2);
    }

    {
        OGRLineString ls;
        EXPECT_TRUE(!(ls.begin() != ls.end()));
    }

    TestIterator<OGRLineString>();
    TestIterator<OGRLineString>("LINESTRING(0 0)", 1);
    TestIterator<OGRLineString, OGRCurve>("LINESTRING(0 0)", 1);
    TestIterator<OGRLineString, OGRCurve>();
    TestIterator<OGRLinearRing>();
    TestIterator<OGRCircularString>();
    TestIterator<OGRCircularString>("CIRCULARSTRING(0 0,1 1,0 0)", 3);
    TestIterator<OGRCircularString, OGRCurve>("CIRCULARSTRING(0 0,1 1,0 0)", 3);
    TestIterator<OGRCompoundCurve>();
    TestIterator<OGRCompoundCurve>("COMPOUNDCURVE((0 0,1 1))", 1);
    TestIterator<OGRCompoundCurve, OGRCurve>(
        "COMPOUNDCURVE((0 0,1 1),CIRCULARSTRING(1 1,2 2,3 3))", 4);
    TestIterator<OGRCompoundCurve>("COMPOUNDCURVE(CIRCULARSTRING EMPTY)", 1);
    TestIterator<OGRCurvePolygon>();
    TestIterator<OGRCurvePolygon>("CURVEPOLYGON((0 0,1 1,1 0,0 0))", 1);
    TestIterator<OGRPolygon>();
    TestIterator<OGRPolygon>("POLYGON((0 0,1 1,1 0,0 0))", 1);
    TestIterator<OGRGeometryCollection>();
    TestIterator<OGRGeometryCollection>("GEOMETRYCOLLECTION(POINT(0 0))", 1);
    TestIterator<OGRMultiSurface>();
    TestIterator<OGRMultiSurface>("MULTISURFACE(((0 0)))", 1);
    TestIterator<OGRMultiPolygon>();
    TestIterator<OGRMultiPolygon>("MULTIPOLYGON(((0 0)))", 1);
    TestIterator<OGRMultiPoint>();
    TestIterator<OGRMultiPoint>("MULTIPOINT(0 0)", 1);
    TestIterator<OGRMultiCurve>();
    TestIterator<OGRMultiCurve>("MULTICURVE((0 0))", 1);
    TestIterator<OGRMultiLineString>();
    TestIterator<OGRMultiLineString>("MULTILINESTRING((0 0))", 1);
    TestIterator<OGRTriangle>();
    TestIterator<OGRTriangle>("TRIANGLE((0 0,0 1,1 1,0 0))", 1);
    TestIterator<OGRPolyhedralSurface>();
    TestIterator<OGRPolyhedralSurface>("POLYHEDRALSURFACE(((0 0,0 1,1 1,0 0)))",
                                       1);
    TestIterator<OGRTriangulatedSurface>();
    TestIterator<OGRTriangulatedSurface>("TIN(((0 0,0 1,1 1,0 0)))", 1);

    // Test that the update of the iterated point of a linestring is
    // immediately taken into account
    // (https://github.com/OSGeo/gdal/issues/6215)
    {
        OGRLineString oLS;
        oLS.addPoint(1, 2);
        oLS.addPoint(3, 4);
        int i = 0;
        for (auto &&p : oLS)
        {
            p.setX(i * 10);
            p.setY(i * 10 + 1);
            p.setZ(i * 10 + 2);
            p.setM(i * 10 + 3);
            ASSERT_EQ(oLS.getX(i), p.getX());
            ASSERT_EQ(oLS.getY(i), p.getY());
            ASSERT_EQ(oLS.getZ(i), p.getZ());
            ASSERT_EQ(oLS.getM(i), p.getM());
            ++i;
        }
    }

    {
        class PointCounterVisitorAndUpdate : public OGRDefaultGeometryVisitor
        {
          public:
            PointCounterVisitorAndUpdate() = default;

            using OGRDefaultGeometryVisitor::visit;

            void visit(OGRPoint *poPoint) override
            {
                poPoint->setZ(100);
                poPoint->setM(1000);
            }
        };

        OGRLineString oLS;
        oLS.addPoint(1, 2);
        oLS.addPoint(3, 4);
        PointCounterVisitorAndUpdate oVisitor;
        oLS.accept(&oVisitor);

        ASSERT_EQ(oLS.getZ(0), 100.0);
        ASSERT_EQ(oLS.getZ(1), 100.0);
        ASSERT_EQ(oLS.getM(0), 1000.0);
        ASSERT_EQ(oLS.getM(1), 1000.0);
    }
}

// Test OGRToOGCGeomType()
TEST_F(test_ogr, OGRToOGCGeomType)
{
    EXPECT_STREQ(OGRToOGCGeomType(wkbPoint), "POINT");
    EXPECT_STREQ(OGRToOGCGeomType(wkbPointM), "POINT");
    EXPECT_STREQ(OGRToOGCGeomType(wkbPoint, /*bCamelCase=*/true), "Point");
    EXPECT_STREQ(
        OGRToOGCGeomType(wkbPoint, /*bCamelCase=*/true, /*bAddZM=*/true),
        "Point");
    EXPECT_STREQ(
        OGRToOGCGeomType(wkbPoint25D, /*bCamelCase=*/true, /*bAddZM=*/true),
        "PointZ");
    EXPECT_STREQ(
        OGRToOGCGeomType(wkbPointM, /*bCamelCase=*/true, /*bAddZM=*/true),
        "PointM");
    EXPECT_STREQ(
        OGRToOGCGeomType(wkbPointZM, /*bCamelCase=*/true, /*bAddZM=*/true),
        "PointZM");
    EXPECT_STREQ(OGRToOGCGeomType(wkbPointZM, /*bCamelCase=*/true,
                                  /*bAddZM=*/true, /*bAddSpaceBeforeZM=*/true),
                 "Point ZM");
}

// Test layer, dataset-feature and layer-feature iterators
TEST_F(test_ogr, DatasetFeature_and_LayerFeature_iterators)
{
    if (!GDALGetDriverByName("ESRI Shapefile"))
    {
        GTEST_SKIP() << "ESRI Shapefile driver missing";
    }
    else
    {
        std::string file(data_ + SEP + "poly.shp");
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(file.c_str(), GDAL_OF_VECTOR));
        ASSERT_TRUE(poDS != nullptr);

        {
            GIntBig nExpectedFID = 0;
            for (const auto &oFeatureLayerPair : poDS->GetFeatures())
            {
                ASSERT_EQ(oFeatureLayerPair.feature->GetFID(), nExpectedFID);
                nExpectedFID++;
                ASSERT_EQ(oFeatureLayerPair.layer, poDS->GetLayer(0));
            }
            ASSERT_EQ(nExpectedFID, 10);
        }

        ASSERT_EQ(poDS->GetLayers().size(), 1U);
        ASSERT_EQ(poDS->GetLayers()[0], poDS->GetLayer(0));
        ASSERT_EQ(poDS->GetLayers()[static_cast<size_t>(0)], poDS->GetLayer(0));
        ASSERT_EQ(poDS->GetLayers()["poly"], poDS->GetLayer(0));

        for (auto poLayer : poDS->GetLayers())
        {
            GIntBig nExpectedFID = 0;
            for (const auto &poFeature : poLayer)
            {
                ASSERT_EQ(poFeature->GetFID(), nExpectedFID);
                nExpectedFID++;
            }
            ASSERT_EQ(nExpectedFID, 10);

            nExpectedFID = 0;
            for (const auto &oFeatureLayerPair : poDS->GetFeatures())
            {
                ASSERT_EQ(oFeatureLayerPair.feature->GetFID(), nExpectedFID);
                nExpectedFID++;
                ASSERT_EQ(oFeatureLayerPair.layer, poLayer);
            }
            ASSERT_EQ(nExpectedFID, 10);

            nExpectedFID = 0;
            OGR_FOR_EACH_FEATURE_BEGIN(hFeat,
                                       reinterpret_cast<OGRLayerH>(poLayer))
            {
                if (nExpectedFID == 0)
                {
                    nExpectedFID = 1;
                    continue;
                }
                ASSERT_EQ(OGR_F_GetFID(hFeat), nExpectedFID);
                nExpectedFID++;
                if (nExpectedFID == 5)
                    break;
            }
            OGR_FOR_EACH_FEATURE_END(hFeat)
            ASSERT_EQ(nExpectedFID, 5);

            auto oIter = poLayer->begin();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            // Only one feature iterator can be active at a time
            auto oIter2 = poLayer->begin();
            CPLPopErrorHandler();
            ASSERT_TRUE(!(oIter2 != poLayer->end()));
            ASSERT_TRUE(oIter != poLayer->end());
        }

        poDS.reset(GetGDALDriverManager()->GetDriverByName("MEM")->Create(
            "", 0, 0, 0, GDT_Unknown, nullptr));
        int nCountLayers = 0;
        for (auto poLayer : poDS->GetLayers())
        {
            CPL_IGNORE_RET_VAL(poLayer);
            nCountLayers++;
        }
        ASSERT_EQ(nCountLayers, 0);

        poDS->CreateLayer("foo");
        poDS->CreateLayer("bar", nullptr);
        for (auto poLayer : poDS->GetLayers())
        {
            if (nCountLayers == 0)
            {
                EXPECT_STREQ(poLayer->GetName(), "foo")
                    << "layer " << poLayer->GetName();
            }
            else if (nCountLayers == 1)
            {
                EXPECT_STREQ(poLayer->GetName(), "bar")
                    << "layer " << poLayer->GetName();
            }
            nCountLayers++;
        }
        ASSERT_EQ(nCountLayers, 2);

        auto layers = poDS->GetLayers();
        {
            // std::copy requires a InputIterator
            std::vector<OGRLayer *> oTarget;
            oTarget.resize(2);
            std::copy(layers.begin(), layers.end(), oTarget.begin());
            ASSERT_EQ(oTarget[0], layers[0]);
            ASSERT_EQ(oTarget[1], layers[1]);

            // but in practice not necessarily uses the postincrement iterator.
            oTarget.clear();
            oTarget.resize(2);
            auto input_iterator = layers.begin();
            auto output_iterator = oTarget.begin();
            while (input_iterator != layers.end())
            {
                *output_iterator++ = *input_iterator++;
            }
            ASSERT_EQ(oTarget[0], layers[0]);
            ASSERT_EQ(oTarget[1], layers[1]);
        }

        // Test copy constructor
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter(srcIter);
            srcIter = layers.begin();  // avoid Coverity Scan warning
            ASSERT_EQ(*newIter, layers[1]);
        }

        // Test assignment operator
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter;
            newIter = srcIter;
            srcIter = layers.begin();  // avoid Coverity Scan warning
            ASSERT_EQ(*newIter, layers[1]);
        }

        // Test move constructor
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter(std::move(srcIter));
            ASSERT_EQ(*newIter, layers[1]);
        }

        // Test move assignment operator
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter;
            newIter = std::move(srcIter);
            ASSERT_EQ(*newIter, layers[1]);
        }

        const GDALDataset *poConstDS = poDS.get();
        ASSERT_EQ(poConstDS->GetLayers().size(), 2U);
        ASSERT_EQ(poConstDS->GetLayers()[0], poConstDS->GetLayer(0));
        ASSERT_EQ(poConstDS->GetLayers()[static_cast<size_t>(0)],
                  poConstDS->GetLayer(0));
        ASSERT_EQ(poConstDS->GetLayers()["foo"], poConstDS->GetLayer(0));
        nCountLayers = 0;
        for (auto &&poLayer : poDS->GetLayers())
        {
            if (nCountLayers == 0)
            {
                EXPECT_STREQ(poLayer->GetName(), "foo")
                    << "layer " << poLayer->GetName();
            }
            else if (nCountLayers == 1)
            {
                EXPECT_STREQ(poLayer->GetName(), "bar")
                    << "layer " << poLayer->GetName();
            }
            nCountLayers++;
        }
        ASSERT_EQ(nCountLayers, 2);

        auto constLayers = poConstDS->GetLayers();
        {
            // std::copy requires a InputIterator
            std::vector<const OGRLayer *> oTarget;
            oTarget.resize(2);
            std::copy(constLayers.begin(), constLayers.end(), oTarget.begin());
            ASSERT_EQ(oTarget[0], constLayers[0]);
            ASSERT_EQ(oTarget[1], constLayers[1]);

            // but in practice not necessarily uses the postincrement iterator.
            oTarget.clear();
            oTarget.resize(2);
            auto input_iterator = constLayers.begin();
            auto output_iterator = oTarget.begin();
            while (input_iterator != constLayers.end())
            {
                *output_iterator++ = *input_iterator++;
            }
            ASSERT_EQ(oTarget[0], constLayers[0]);
            ASSERT_EQ(oTarget[1], constLayers[1]);
        }

        // Test copy constructor
        {
            auto srcIter(poConstDS->GetLayers().begin());
            ++srcIter;
            auto newIter(srcIter);
            srcIter = constLayers.begin();  // avoid Coverity Scan warning
            ASSERT_EQ(*newIter, constLayers[1]);
        }

        // Test assignment operator
        {
            auto srcIter(poConstDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::ConstLayers::Iterator newIter;
            newIter = srcIter;
            srcIter = constLayers.begin();  // avoid Coverity Scan warning
            ASSERT_EQ(*newIter, constLayers[1]);
        }

        // Test move constructor
        {
            auto srcIter(poConstDS->GetLayers().begin());
            ++srcIter;
            auto newIter(std::move(srcIter));
            ASSERT_EQ(*newIter, constLayers[1]);
        }

        // Test move assignment operator
        {
            auto srcIter(poConstDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::ConstLayers::Iterator newIter;
            newIter = std::move(srcIter);
            ASSERT_EQ(*newIter, constLayers[1]);
        }
    }
}

// Test field iterator
TEST_F(test_ogr, field_iterator)
{
    OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn();
    poFeatureDefn->Reference();
    {
        OGRFieldDefn oFieldDefn("str_field", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("int_field", OFTInteger);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("int64_field", OFTInteger64);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("double_field", OFTReal);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("null_field", OFTReal);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("unset_field", OFTReal);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("dt_field", OFTDateTime);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("strlist_field", OFTStringList);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("intlist_field", OFTIntegerList);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("int64list_field", OFTInteger64List);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("doublelist_field", OFTRealList);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    OGRFeature oFeature(poFeatureDefn);

    {
        OGRFeature oFeatureTmp(poFeatureDefn);
        oFeatureTmp[0] = "bar";
        ASSERT_STREQ(oFeatureTmp[0].GetString(), "bar");
        {
            // Proxy reference
            auto &&x = oFeatureTmp[0];
            auto &xRef(x);
            x = xRef;
            ASSERT_STREQ(oFeatureTmp[0].GetString(), "bar");
        }
        {
            oFeatureTmp[0] = oFeatureTmp[0];
            ASSERT_STREQ(oFeatureTmp[0].GetString(), "bar");
        }
        {
            // Proxy reference
            auto &&x = oFeatureTmp[0];
            x = "baz";
            ASSERT_STREQ(x.GetString(), "baz");
        }
        oFeatureTmp["str_field"] = std::string("foo");
        oFeatureTmp["int_field"] = 123;
        oFeatureTmp["int64_field"] = oFeatureTmp["int_field"];
        ASSERT_EQ(oFeatureTmp["int64_field"].GetInteger64(), 123);
        oFeatureTmp["int64_field"] = static_cast<GIntBig>(1234567890123);
        oFeatureTmp["double_field"] = 123.45;
        oFeatureTmp["null_field"].SetNull();
        oFeatureTmp["unset_field"].clear();
        oFeatureTmp["unset_field"].Unset();
        oFeatureTmp["dt_field"].SetDateTime(2018, 4, 5, 12, 34, 56.75f, 0);
        oFeatureTmp["strlist_field"] = CPLStringList().List();
        oFeatureTmp["strlist_field"] = std::vector<std::string>();
        oFeatureTmp["strlist_field"] = std::vector<std::string>{"foo", "bar"};
        oFeatureTmp["strlist_field"] =
            static_cast<CSLConstList>(oFeatureTmp["strlist_field"]);
        ASSERT_EQ(
            CSLCount(static_cast<CSLConstList>(oFeatureTmp["strlist_field"])),
            2);
        oFeatureTmp["intlist_field"] = std::vector<int>();
        oFeatureTmp["intlist_field"] = std::vector<int>{12, 34};
        oFeatureTmp["int64list_field"] = std::vector<GIntBig>();
        oFeatureTmp["int64list_field"] =
            std::vector<GIntBig>{1234567890123, 34};
        oFeatureTmp["doublelist_field"] = std::vector<double>();
        oFeatureTmp["doublelist_field"] = std::vector<double>{12.25, 56.75};

        for (const auto &oField : oFeatureTmp)
        {
            oFeature[oField.GetIndex()] = oField;
        }
    }

    {
        int x = oFeature[1];
        ASSERT_EQ(x, 123);
    }
    {
        int x = oFeature["int_field"];
        ASSERT_EQ(x, 123);
    }
    {
        GIntBig x = oFeature["int64_field"];
        ASSERT_EQ(x, static_cast<GIntBig>(1234567890123));
    }
    {
        double x = oFeature["double_field"];
        ASSERT_EQ(x, 123.45);
    }
    {
        const char *x = oFeature["str_field"];
        ASSERT_STREQ(x, "foo");
    }
    bool bExceptionHit = false;
    try
    {
        oFeature["inexisting_field"];
    }
    catch (const OGRFeature::FieldNotFoundException &)
    {
        bExceptionHit = true;
    }
    ASSERT_TRUE(bExceptionHit);

    int iIter = 0;
    const OGRFeature *poConstFeature = &oFeature;
    for (const auto &oField : *poConstFeature)
    {
        ASSERT_EQ(oField.GetIndex(), iIter);
        ASSERT_EQ(oField.GetDefn(), poFeatureDefn->GetFieldDefn(iIter));
        ASSERT_EQ(CPLString(oField.GetName()),
                  CPLString(oField.GetDefn()->GetNameRef()));
        ASSERT_EQ(oField.GetType(), oField.GetDefn()->GetType());
        ASSERT_EQ(oField.GetSubType(), oField.GetDefn()->GetSubType());
        if (iIter == 0)
        {
            ASSERT_EQ(oField.IsUnset(), false);
            ASSERT_EQ(oField.IsNull(), false);
            ASSERT_EQ(CPLString(oField.GetRawValue()->String),
                      CPLString("foo"));
            ASSERT_EQ(CPLString(oField.GetString()), CPLString("foo"));
            ASSERT_EQ(CPLString(oField.GetAsString()), CPLString("foo"));
        }
        else if (iIter == 1)
        {
            ASSERT_EQ(oField.GetRawValue()->Integer, 123);
            ASSERT_EQ(oField.GetInteger(), 123);
            ASSERT_EQ(oField.GetAsInteger(), 123);
            ASSERT_EQ(oField.GetAsInteger64(), 123);
            ASSERT_EQ(oField.GetAsDouble(), 123.0);
            ASSERT_EQ(CPLString(oField.GetAsString()), CPLString("123"));
        }
        else if (iIter == 2)
        {
            ASSERT_EQ(oField.GetRawValue()->Integer64, 1234567890123);
            ASSERT_EQ(oField.GetInteger64(), 1234567890123);
            ASSERT_EQ(oField.GetAsInteger(), 2147483647);
            ASSERT_EQ(oField.GetAsInteger64(), 1234567890123);
            ASSERT_EQ(oField.GetAsDouble(), 1234567890123.0);
            ASSERT_EQ(CPLString(oField.GetAsString()),
                      CPLString("1234567890123"));
        }
        else if (iIter == 3)
        {
            ASSERT_EQ(oField.GetRawValue()->Real, 123.45);
            ASSERT_EQ(oField.GetDouble(), 123.45);
            ASSERT_EQ(oField.GetAsInteger(), 123);
            ASSERT_EQ(oField.GetAsInteger64(), 123);
            ASSERT_EQ(oField.GetAsDouble(), 123.45);
            ASSERT_EQ(CPLString(oField.GetAsString()), CPLString("123.45"));
        }
        else if (iIter == 4)
        {
            ASSERT_EQ(oField.IsUnset(), false);
            ASSERT_EQ(oField.IsNull(), true);
        }
        else if (iIter == 5)
        {
            ASSERT_EQ(oField.IsUnset(), true);
            ASSERT_EQ(oField.empty(), true);
            ASSERT_EQ(oField.IsNull(), false);
        }
        else if (iIter == 6)
        {
            int nYear, nMonth, nDay, nHour, nMin, nTZFlag;
            float fSec;
            ASSERT_EQ(oField.GetDateTime(&nYear, &nMonth, &nDay, &nHour, &nMin,
                                         &fSec, &nTZFlag),
                      true);
            ASSERT_EQ(nYear, 2018);
            ASSERT_EQ(nMonth, 4);
            ASSERT_EQ(nDay, 5);
            ASSERT_EQ(nHour, 12);
            ASSERT_EQ(nMin, 34);
            ASSERT_EQ(fSec, 56.75f);
            ASSERT_EQ(nTZFlag, 0);
        }
        else if (iIter == 7)
        {
            std::vector<std::string> oExpected{std::string("foo"),
                                               std::string("bar")};
            decltype(oExpected) oGot = oField;
            ASSERT_EQ(oGot.size(), oExpected.size());
            for (size_t i = 0; i < oExpected.size(); i++)
                ASSERT_EQ(oGot[i], oExpected[i]);
        }
        else if (iIter == 8)
        {
            std::vector<int> oExpected{12, 34};
            decltype(oExpected) oGot = oField;
            ASSERT_EQ(oGot.size(), oExpected.size());
            for (size_t i = 0; i < oExpected.size(); i++)
                ASSERT_EQ(oGot[i], oExpected[i]);
        }
        else if (iIter == 9)
        {
            std::vector<GIntBig> oExpected{1234567890123, 34};
            decltype(oExpected) oGot = oField;
            ASSERT_EQ(oGot.size(), oExpected.size());
            for (size_t i = 0; i < oExpected.size(); i++)
                ASSERT_EQ(oGot[i], oExpected[i]);
        }
        else if (iIter == 10)
        {
            std::vector<double> oExpected{12.25, 56.75};
            decltype(oExpected) oGot = oField;
            ASSERT_EQ(oGot.size(), oExpected.size());
            for (size_t i = 0; i < oExpected.size(); i++)
                ASSERT_EQ(oGot[i], oExpected[i]);
        }
        iIter++;
    }
    poFeatureDefn->Release();
}

// Test OGRLinearRing::isPointOnRingBoundary()
TEST_F(test_ogr, isPointOnRingBoundary)
{
    OGRPolygon oPoly;
    const char *pszPoly = "POLYGON((10 9,11 10,10 11,9 10,10 9))";
    oPoly.importFromWkt(&pszPoly);
    auto poRing = oPoly.getExteriorRing();

    // On first vertex
    {
        OGRPoint p(10, 9);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }

    // On second vertex
    {
        OGRPoint p(11, 10);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }

    // Middle of first segment
    {
        OGRPoint p(10.5, 9.5);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }

    // "Before" first segment
    {
        OGRPoint p(10 - 1, 9 - 1);
        ASSERT_TRUE(!poRing->isPointOnRingBoundary(&p, false));
    }

    // "After" first segment
    {
        OGRPoint p(11 + 1, 10 + 1);
        ASSERT_TRUE(!poRing->isPointOnRingBoundary(&p, false));
    }

    // On third vertex
    {
        OGRPoint p(10, 11);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }

    // Middle of second segment
    {
        OGRPoint p(10.5, 10.5);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }

    // On fourth vertex
    {
        OGRPoint p(9, 10);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }

    // Middle of third segment
    {
        OGRPoint p(9.5, 10.5);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }

    // Middle of fourth segment
    {
        OGRPoint p(9.5, 9.5);
        ASSERT_TRUE(poRing->isPointOnRingBoundary(&p, false));
    }
}

// Test OGRGeometry::exportToWkt()
TEST_F(test_ogr, OGRGeometry_exportToWkt)
{
    char *pszWKT = nullptr;
    OGRPoint p(1, 2);
    p.exportToWkt(&pszWKT);
    ASSERT_TRUE(pszWKT != nullptr);
    EXPECT_STREQ(pszWKT, "POINT (1 2)");
    CPLFree(pszWKT);
}

// Test OGRGeometry::clone()
TEST_F(test_ogr, OGRGeometry_clone)
{
    const char *apszWKT[] = {
        "POINT (0 0)",
        "POINT ZM EMPTY",
        "LINESTRING (0 0)",
        "LINESTRING ZM EMPTY",
        "POLYGON ((0 0),(0 0))",
        "MULTIPOLYGON ZM EMPTY",
        "MULTIPOINT ((0 0))",
        "MULTIPOINT ZM EMPTY",
        "MULTILINESTRING ((0 0))",
        "MULTILINESTRING ZM EMPTY",
        "MULTIPOLYGON (((0 0)))",
        "MULTIPOLYGON ZM EMPTY",
        "GEOMETRYCOLLECTION (POINT (0 0))",
        "GEOMETRYCOLLECTION ZM EMPTY",
        "CIRCULARSTRING (0 0,1 1,0 0)",
        "CIRCULARSTRING Z EMPTY",
        "CIRCULARSTRING ZM EMPTY",
        "COMPOUNDCURVE ((0 0,1 1))",
        "COMPOUNDCURVE ZM EMPTY",
        "CURVEPOLYGON ((0 0,1 1,1 0,0 0))",
        "CURVEPOLYGON ZM EMPTY",
        "MULTICURVE ((0 0))",
        "MULTICURVE ZM EMPTY",
        "MULTISURFACE (((0 0)))",
        "MULTISURFACE ZM EMPTY",
        "TRIANGLE ((0 0,0 1,1 1,0 0))",
        "TRIANGLE ZM EMPTY",
        "POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))",
        "POLYHEDRALSURFACE ZM EMPTY",
        "TIN (((0 0,0 1,1 1,0 0)))",
        "TIN ZM EMPTY",
    };
    OGRSpatialReference oSRS;
    for (const char *pszWKT : apszWKT)
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(pszWKT, &oSRS, &poGeom);
        auto poClone = poGeom->clone();
        ASSERT_TRUE(poClone != nullptr);
        char *outWKT = nullptr;
        poClone->exportToWkt(&outWKT, wkbVariantIso);
        EXPECT_STREQ(pszWKT, outWKT);
        CPLFree(outWKT);
        delete poClone;
        delete poGeom;
    }
}

// Test OGRLineString::removePoint()
TEST_F(test_ogr, OGRLineString_removePoint)
{
    {
        OGRLineString ls;
        ls.addPoint(0, 1);
        ls.addPoint(2, 3);
        ls.addPoint(4, 5);
        ASSERT_TRUE(!ls.removePoint(-1));
        ASSERT_TRUE(!ls.removePoint(3));
        ASSERT_EQ(ls.getNumPoints(), 3);
        ASSERT_TRUE(ls.removePoint(1));
        ASSERT_EQ(ls.getNumPoints(), 2);
        ASSERT_EQ(ls.getX(0), 0.0);
        ASSERT_EQ(ls.getY(0), 1.0);
        ASSERT_EQ(ls.getX(1), 4.0);
        ASSERT_EQ(ls.getY(1), 5.0);
        ASSERT_TRUE(ls.removePoint(1));
        ASSERT_EQ(ls.getNumPoints(), 1);
        ASSERT_TRUE(ls.removePoint(0));
        ASSERT_EQ(ls.getNumPoints(), 0);
    }
    {
        // With Z, M
        OGRLineString ls;
        ls.addPoint(0, 1, 20, 30);
        ls.addPoint(2, 3, 40, 50);
        ls.addPoint(4, 5, 60, 70);
        ASSERT_TRUE(!ls.removePoint(-1));
        ASSERT_TRUE(!ls.removePoint(3));
        ASSERT_EQ(ls.getNumPoints(), 3);
        ASSERT_TRUE(ls.removePoint(1));
        ASSERT_EQ(ls.getNumPoints(), 2);
        ASSERT_EQ(ls.getX(0), 0.0);
        ASSERT_EQ(ls.getY(0), 1.0);
        ASSERT_EQ(ls.getZ(0), 20.0);
        ASSERT_EQ(ls.getM(0), 30.0);
        ASSERT_EQ(ls.getX(1), 4.0);
        ASSERT_EQ(ls.getY(1), 5.0);
        ASSERT_EQ(ls.getZ(1), 60.0);
        ASSERT_EQ(ls.getM(1), 70.0);
        ASSERT_TRUE(ls.removePoint(1));
        ASSERT_EQ(ls.getNumPoints(), 1);
        ASSERT_TRUE(ls.removePoint(0));
        ASSERT_EQ(ls.getNumPoints(), 0);
    }
}

// Test effect of MarkSuppressOnClose() on DXF
TEST_F(test_ogr, DXF_MarkSuppressOnClose)
{
    CPLString tmpFilename(CPLGenerateTempFilename(nullptr));
    tmpFilename += ".dxf";
    auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("DXF"));
    if (poDrv)
    {
        auto poDS(GDALDatasetUniquePtr(
            poDrv->Create(tmpFilename, 0, 0, 0, GDT_Unknown, nullptr)));
        ASSERT_TRUE(poDS != nullptr);

        OGRLayer *poLayer =
            poDS->CreateLayer("test", nullptr, wkbPoint, nullptr);
        ASSERT_TRUE(poLayer != nullptr);

        for (double x = 0; x < 100; x++)
        {
            OGRFeature *poFeature =
                OGRFeature::CreateFeature(poLayer->GetLayerDefn());
            ASSERT_TRUE(poFeature != nullptr);
            OGRPoint pt(x, 42);
            ASSERT_EQ(OGRERR_NONE, poFeature->SetGeometry(&pt));
            ASSERT_EQ(OGRERR_NONE, poLayer->CreateFeature(poFeature));
            OGRFeature::DestroyFeature(poFeature);
        }

        poDS->MarkSuppressOnClose();

        poDS.reset();
        VSIStatBufL sStat;
        ASSERT_TRUE(0 != VSIStatL(tmpFilename, &sStat));
    }
}

// Test OGREnvelope
TEST_F(test_ogr, OGREnvelope)
{
    OGREnvelope s1;
    ASSERT_TRUE(!s1.IsInit());
    {
        OGREnvelope s2(s1);
        ASSERT_TRUE(s1 == s2);
        ASSERT_TRUE(!(s1 != s2));
    }

    s1.MinX = 0;
    s1.MinY = 1;
    s1.MaxX = 2;
    s1.MaxY = 3;
    ASSERT_TRUE(s1.IsInit());
    {
        OGREnvelope s2(s1);
        ASSERT_TRUE(s1 == s2);
        ASSERT_TRUE(!(s1 != s2));
        s2.MinX += 1;
        ASSERT_TRUE(s1 != s2);
        ASSERT_TRUE(!(s1 == s2));
    }
}

// Test OGREnvelope3D
TEST_F(test_ogr, OGREnvelope3D)
{
    OGREnvelope3D s1;
    EXPECT_TRUE(!s1.IsInit());
    {
        OGREnvelope3D s2(s1);
        EXPECT_TRUE(s1 == s2);
        EXPECT_TRUE(!(s1 != s2));
    }

    s1.MinX = 0;
    s1.MinY = 1;
    s1.MaxX = 2;
    s1.MaxY = 3;
    EXPECT_TRUE(s1.IsInit());
    EXPECT_FALSE(s1.Is3D());
    s1.MinZ = 4;
    s1.MaxZ = 5;
    EXPECT_TRUE(s1.Is3D());
    {
        OGREnvelope3D s2(s1);
        EXPECT_TRUE(s1 == s2);
        EXPECT_TRUE(!(s1 != s2));
        s2.MinX += 1;
        EXPECT_TRUE(s1 != s2);
        EXPECT_TRUE(!(s1 == s2));
    }
}

// Test OGRStyleMgr::InitStyleString() with a style name
// (https://github.com/OSGeo/gdal/issues/5555)
TEST_F(test_ogr, InitStyleString_with_style_name)
{
    OGRStyleTableH hStyleTable = OGR_STBL_Create();
    OGR_STBL_AddStyle(hStyleTable, "@my_style", "PEN(c:#FF0000,w:5px)");
    OGRStyleMgrH hSM = OGR_SM_Create(hStyleTable);
    EXPECT_EQ(OGR_SM_GetPartCount(hSM, nullptr), 0);
    EXPECT_TRUE(OGR_SM_InitStyleString(hSM, "@my_style"));
    EXPECT_EQ(OGR_SM_GetPartCount(hSM, nullptr), 1);
    EXPECT_TRUE(!OGR_SM_InitStyleString(hSM, "@i_do_not_exist"));
    OGR_SM_Destroy(hSM);
    OGR_STBL_Destroy(hStyleTable);
}

// Test OGR_L_GetArrowStream
TEST_F(test_ogr, OGR_L_GetArrowStream)
{
    auto poDS = std::unique_ptr<GDALDataset>(
        GetGDALDriverManager()->GetDriverByName("MEM")->Create(
            "", 0, 0, 0, GDT_Unknown, nullptr));
    auto poLayer = poDS->CreateLayer("test");
    {
        OGRFieldDefn oFieldDefn("str", OFTString);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("bool", OFTInteger);
        oFieldDefn.SetSubType(OFSTBoolean);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("int16", OFTInteger);
        oFieldDefn.SetSubType(OFSTInt16);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("int32", OFTInteger);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("int64", OFTInteger64);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("float32", OFTReal);
        oFieldDefn.SetSubType(OFSTFloat32);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("float64", OFTReal);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("date", OFTDate);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("time", OFTTime);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("datetime", OFTDateTime);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("binary", OFTBinary);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("strlist", OFTStringList);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("boollist", OFTIntegerList);
        oFieldDefn.SetSubType(OFSTBoolean);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("int16list", OFTIntegerList);
        oFieldDefn.SetSubType(OFSTInt16);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("int32list", OFTIntegerList);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("int64list", OFTInteger64List);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("float32list", OFTRealList);
        oFieldDefn.SetSubType(OFSTFloat32);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    {
        OGRFieldDefn oFieldDefn("float64list", OFTRealList);
        EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    }
    auto poFDefn = poLayer->GetLayerDefn();
    struct ArrowArrayStream stream;
    ASSERT_TRUE(
        OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream, nullptr));
    {
        // Cannot start a new stream while one is active
        struct ArrowArrayStream stream2;
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ASSERT_TRUE(OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream2,
                                         nullptr) == false);
        CPLPopErrorHandler();
    }
    ASSERT_TRUE(stream.release != nullptr);

    struct ArrowSchema schema;
    CPLErrorReset();
    ASSERT_TRUE(stream.get_last_error(&stream) == nullptr);
    ASSERT_EQ(stream.get_schema(&stream, &schema), 0);
    ASSERT_TRUE(stream.get_last_error(&stream) == nullptr);
    ASSERT_TRUE(schema.release != nullptr);
    ASSERT_EQ(schema.n_children,
              1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
    schema.release(&schema);

    struct ArrowArray array;
    // Next batch ==> End of stream
    ASSERT_EQ(stream.get_next(&stream, &array), 0);
    ASSERT_TRUE(array.release == nullptr);

    // Release stream
    stream.release(&stream);

    {
        auto poFeature = std::unique_ptr<OGRFeature>(new OGRFeature(poFDefn));
        poFeature->SetField("bool", 1);
        poFeature->SetField("int16", -12345);
        poFeature->SetField("int32", 12345678);
        poFeature->SetField("int64", static_cast<GIntBig>(12345678901234));
        poFeature->SetField("float32", 1.25);
        poFeature->SetField("float64", 1.250123);
        poFeature->SetField("str", "abc");
        poFeature->SetField("date", "2022-05-31");
        poFeature->SetField("time", "12:34:56.789");
        poFeature->SetField("datetime", "2022-05-31T12:34:56.789Z");
        poFeature->SetField("boollist", "[False,True]");
        poFeature->SetField("int16list", "[-12345,12345]");
        poFeature->SetField("int32list", "[-12345678,12345678]");
        poFeature->SetField("int64list", "[-12345678901234,12345678901234]");
        poFeature->SetField("float32list", "[-1.25,1.25]");
        poFeature->SetField("float64list", "[-1.250123,1.250123]");
        poFeature->SetField("strlist", "[\"abc\",\"defghi\"]");
        poFeature->SetField(poFDefn->GetFieldIndex("binary"), 2, "\xDE\xAD");
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POINT(1 2)", nullptr, &poGeom);
        poFeature->SetGeometryDirectly(poGeom);
        ASSERT_EQ(poLayer->CreateFeature(poFeature.get()), OGRERR_NONE);
    }

    // Get a new stream now that we've released it
    ASSERT_TRUE(
        OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream, nullptr));
    ASSERT_TRUE(stream.release != nullptr);

    ASSERT_EQ(stream.get_next(&stream, &array), 0);
    ASSERT_TRUE(array.release != nullptr);
    ASSERT_EQ(array.n_children,
              1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
    ASSERT_EQ(array.length, poLayer->GetFeatureCount(false));
    ASSERT_EQ(array.null_count, 0);
    ASSERT_EQ(array.n_buffers, 1);
    ASSERT_TRUE(array.buffers[0] == nullptr);  // no bitmap
    for (int i = 0; i < array.n_children; i++)
    {
        ASSERT_TRUE(array.children[i]->release != nullptr);
        ASSERT_EQ(array.children[i]->length, array.length);
        ASSERT_TRUE(array.children[i]->n_buffers >= 2);
        ASSERT_TRUE(array.children[i]->buffers[0] == nullptr);  // no bitmap
        ASSERT_EQ(array.children[i]->null_count, 0);
        ASSERT_TRUE(array.children[i]->buffers[1] != nullptr);
        if (array.children[i]->n_buffers == 3)
        {
            ASSERT_TRUE(array.children[i]->buffers[2] != nullptr);
        }
    }
    array.release(&array);

    // Next batch ==> End of stream
    ASSERT_EQ(stream.get_next(&stream, &array), 0);
    ASSERT_TRUE(array.release == nullptr);

    // Release stream
    stream.release(&stream);

    // Insert 2 empty features
    {
        auto poFeature = std::unique_ptr<OGRFeature>(new OGRFeature(poFDefn));
        ASSERT_EQ(poLayer->CreateFeature(poFeature.get()), OGRERR_NONE);
    }

    {
        auto poFeature = std::unique_ptr<OGRFeature>(new OGRFeature(poFDefn));
        ASSERT_EQ(poLayer->CreateFeature(poFeature.get()), OGRERR_NONE);
    }

    // Get a new stream now that we've released it
    {
        char **papszOptions =
            CSLSetNameValue(nullptr, "MAX_FEATURES_IN_BATCH", "2");
        ASSERT_TRUE(OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream,
                                         papszOptions));
        CSLDestroy(papszOptions);
    }
    ASSERT_TRUE(stream.release != nullptr);

    ASSERT_EQ(stream.get_next(&stream, &array), 0);
    ASSERT_TRUE(array.release != nullptr);
    ASSERT_EQ(array.n_children,
              1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
    ASSERT_EQ(array.length, 2);
    for (int i = 0; i < array.n_children; i++)
    {
        ASSERT_TRUE(array.children[i]->release != nullptr);
        ASSERT_EQ(array.children[i]->length, array.length);
        ASSERT_TRUE(array.children[i]->n_buffers >= 2);
        if (i > 0)
        {
            ASSERT_TRUE(array.children[i]->buffers[0] !=
                        nullptr);  // we have a bitmap
            ASSERT_EQ(array.children[i]->null_count, 1);
        }
        ASSERT_TRUE(array.children[i]->buffers[1] != nullptr);
        if (array.children[i]->n_buffers == 3)
        {
            ASSERT_TRUE(array.children[i]->buffers[2] != nullptr);
        }
    }
    array.release(&array);

    // Next batch
    ASSERT_EQ(stream.get_next(&stream, &array), 0);
    ASSERT_TRUE(array.release != nullptr);
    ASSERT_EQ(array.n_children,
              1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
    ASSERT_EQ(array.length, 1);
    array.release(&array);

    // Next batch ==> End of stream
    ASSERT_EQ(stream.get_next(&stream, &array), 0);
    ASSERT_TRUE(array.release == nullptr);

    // Release stream
    stream.release(&stream);

    // Get a new stream now that we've released it
    ASSERT_TRUE(
        OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream, nullptr));
    ASSERT_TRUE(stream.release != nullptr);

    // Free dataset & layer
    poDS.reset();

    // Test releasing the stream after the dataset/layer has been closed
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLErrorReset();
    ASSERT_TRUE(stream.get_schema(&stream, &schema) != 0);
    ASSERT_TRUE(stream.get_last_error(&stream) != nullptr);
    ASSERT_TRUE(stream.get_next(&stream, &array) != 0);
    CPLPopErrorHandler();
    stream.release(&stream);
}

// Test field domain cloning
TEST_F(test_ogr, field_domain_cloning)
{
    // range domain
    OGRField min;
    min.Real = 5.5;
    OGRField max;
    max.Real = 6.5;
    OGRRangeFieldDomain oRange("name", "description", OGRFieldType::OFTReal,
                               OGRFieldSubType::OFSTBoolean, min, true, max,
                               true);
    oRange.SetMergePolicy(OGRFieldDomainMergePolicy::OFDMP_GEOMETRY_WEIGHTED);
    oRange.SetSplitPolicy(OGRFieldDomainSplitPolicy::OFDSP_GEOMETRY_RATIO);
    std::unique_ptr<OGRRangeFieldDomain> poClonedRange(oRange.Clone());
    ASSERT_EQ(poClonedRange->GetName(), oRange.GetName());
    ASSERT_EQ(poClonedRange->GetDescription(), oRange.GetDescription());
    bool originalInclusive = false;
    bool cloneInclusive = false;
    ASSERT_EQ(poClonedRange->GetMin(originalInclusive).Real,
              oRange.GetMin(cloneInclusive).Real);
    ASSERT_EQ(originalInclusive, cloneInclusive);
    ASSERT_EQ(poClonedRange->GetMax(originalInclusive).Real,
              oRange.GetMax(cloneInclusive).Real);
    ASSERT_EQ(originalInclusive, cloneInclusive);
    ASSERT_EQ(poClonedRange->GetFieldType(), oRange.GetFieldType());
    ASSERT_EQ(poClonedRange->GetFieldSubType(), oRange.GetFieldSubType());
    ASSERT_EQ(poClonedRange->GetSplitPolicy(), oRange.GetSplitPolicy());
    ASSERT_EQ(poClonedRange->GetMergePolicy(), oRange.GetMergePolicy());

    // glob domain
    OGRGlobFieldDomain oGlob("name", "description", OGRFieldType::OFTString,
                             OGRFieldSubType::OFSTBoolean, "*a*");
    oGlob.SetMergePolicy(OGRFieldDomainMergePolicy::OFDMP_GEOMETRY_WEIGHTED);
    oGlob.SetSplitPolicy(OGRFieldDomainSplitPolicy::OFDSP_GEOMETRY_RATIO);
    std::unique_ptr<OGRGlobFieldDomain> poClonedGlob(oGlob.Clone());
    ASSERT_EQ(poClonedGlob->GetName(), oGlob.GetName());
    ASSERT_EQ(poClonedGlob->GetDescription(), oGlob.GetDescription());
    ASSERT_EQ(poClonedGlob->GetGlob(), oGlob.GetGlob());
    ASSERT_EQ(poClonedGlob->GetFieldType(), oGlob.GetFieldType());
    ASSERT_EQ(poClonedGlob->GetFieldSubType(), oGlob.GetFieldSubType());
    ASSERT_EQ(poClonedGlob->GetSplitPolicy(), oGlob.GetSplitPolicy());
    ASSERT_EQ(poClonedGlob->GetMergePolicy(), oGlob.GetMergePolicy());

    // coded value domain
    OGRCodedFieldDomain oCoded("name", "description", OGRFieldType::OFTString,
                               OGRFieldSubType::OFSTBoolean, {OGRCodedValue()});
    oCoded.SetMergePolicy(OGRFieldDomainMergePolicy::OFDMP_GEOMETRY_WEIGHTED);
    oCoded.SetSplitPolicy(OGRFieldDomainSplitPolicy::OFDSP_GEOMETRY_RATIO);
    std::unique_ptr<OGRCodedFieldDomain> poClonedCoded(oCoded.Clone());
    ASSERT_EQ(poClonedCoded->GetName(), oCoded.GetName());
    ASSERT_EQ(poClonedCoded->GetDescription(), oCoded.GetDescription());
    ASSERT_EQ(poClonedCoded->GetFieldType(), oCoded.GetFieldType());
    ASSERT_EQ(poClonedCoded->GetFieldSubType(), oCoded.GetFieldSubType());
    ASSERT_EQ(poClonedCoded->GetSplitPolicy(), oCoded.GetSplitPolicy());
    ASSERT_EQ(poClonedCoded->GetMergePolicy(), oCoded.GetMergePolicy());
}

// Test field comments
TEST_F(test_ogr, field_comments)
{
    OGRFieldDefn oFieldDefn("field1", OFTString);
    ASSERT_EQ(oFieldDefn.GetComment(), "");
    oFieldDefn.SetComment("my comment");
    ASSERT_EQ(oFieldDefn.GetComment(), "my comment");

    OGRFieldDefn oFieldDefn2(&oFieldDefn);
    ASSERT_EQ(oFieldDefn2.GetComment(), "my comment");
    ASSERT_TRUE(oFieldDefn.IsSame(&oFieldDefn2));

    oFieldDefn2.SetComment("my comment 2");
    ASSERT_FALSE(oFieldDefn.IsSame(&oFieldDefn2));
}

// Test OGRFeatureDefn C++ GetFields() iterator
TEST_F(test_ogr, feature_defn_fields_iterator)
{
    OGRFeatureDefn oFDefn;
    {
        OGRFieldDefn oFieldDefn("field1", OFTString);
        oFDefn.AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field2", OFTString);
        oFDefn.AddFieldDefn(&oFieldDefn);
    }
    EXPECT_EQ(oFDefn.GetFields().size(),
              static_cast<size_t>(oFDefn.GetFieldCount()));
    int i = 0;
    for (const auto *poFieldDefn : oFDefn.GetFields())
    {
        EXPECT_EQ(oFDefn.GetFields()[i], oFDefn.GetFieldDefn(i));
        EXPECT_EQ(poFieldDefn, oFDefn.GetFieldDefn(i));
        ++i;
    }
    EXPECT_EQ(i, oFDefn.GetFieldCount());
}

// Test OGRFeatureDefn C++ GetGeomFields() iterator
TEST_F(test_ogr, feature_defn_geomfields_iterator)
{
    OGRFeatureDefn oFDefn;
    {
        OGRGeomFieldDefn oGeomFieldDefn("field1", wkbUnknown);
        oFDefn.AddGeomFieldDefn(&oGeomFieldDefn);
    }
    {
        OGRGeomFieldDefn oGeomFieldDefn("field2", wkbUnknown);
        oFDefn.AddGeomFieldDefn(&oGeomFieldDefn);
    }
    EXPECT_EQ(oFDefn.GetGeomFields().size(),
              static_cast<size_t>(oFDefn.GetGeomFieldCount()));
    int i = 0;
    for (const auto *poGeomFieldDefn : oFDefn.GetGeomFields())
    {
        EXPECT_EQ(oFDefn.GetGeomFields()[i], oFDefn.GetGeomFieldDefn(i));
        EXPECT_EQ(poGeomFieldDefn, oFDefn.GetGeomFieldDefn(i));
        ++i;
    }
    EXPECT_EQ(i, oFDefn.GetGeomFieldCount());
}

// Test OGRGeomFieldDefn copy constructor
TEST_F(test_ogr, geom_field_defn_copy_constructor)
{
    {
        OGRGeomFieldDefn oGeomFieldDefn("field1", wkbPoint);
        oGeomFieldDefn.SetNullable(false);
        OGRGeomFieldDefn oGeomFieldDefn2("field2", wkbLineString);
        oGeomFieldDefn2 = oGeomFieldDefn;
        EXPECT_TRUE(oGeomFieldDefn2.IsSame(&oGeomFieldDefn));
    }

    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput("WGS84");
        EXPECT_EQ(oSRS.GetReferenceCount(), 1);
        OGRGeomFieldDefn oGeomFieldDefn("field1", wkbPoint);
        oGeomFieldDefn.SetSpatialRef(&oSRS);
        EXPECT_EQ(oSRS.GetReferenceCount(), 2);
        OGRGeomFieldDefn oGeomFieldDefn2("field2", wkbLineString);
        oGeomFieldDefn2 = oGeomFieldDefn;
        EXPECT_EQ(oSRS.GetReferenceCount(), 3);
        EXPECT_TRUE(oGeomFieldDefn2.IsSame(&oGeomFieldDefn));

        // oGeomFieldDefn2 already points to oSRS
        oGeomFieldDefn2 = oGeomFieldDefn;
        EXPECT_EQ(oSRS.GetReferenceCount(), 3);
        EXPECT_TRUE(oGeomFieldDefn2.IsSame(&oGeomFieldDefn));
    }
}

// Test GDALDataset QueryLoggerFunc callback
TEST_F(test_ogr, GDALDatasetSetQueryLoggerFunc)
{
    if (GDALGetDriverByName("GPKG") == nullptr)
    {
        GTEST_SKIP() << "GPKG driver missing";
    }

    auto tmpGPKG{testing::TempDir() + "/poly-1-feature.gpkg"};
    {
        std::string srcfilename(data_ + SEP + "poly-1-feature.gpkg");
        std::ifstream src(srcfilename, std::ios::binary);
        std::ofstream dst(tmpGPKG, std::ios::binary);
        dst << src.rdbuf();
    }

    struct QueryLogEntry
    {
        std::string sql;
        std::string error;
        int64_t numRecords;
        int64_t executionTimeMilliseconds;
    };

    // Note: this must be constructed before poDS or the order
    //       of destruction will make the callback call the already
    //       destructed vector
    std::vector<QueryLogEntry> queryLog;

    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(tmpGPKG.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE));
    ASSERT_TRUE(poDS);
    auto hDS = GDALDataset::ToHandle(poDS.get());
    ASSERT_TRUE(hDS);

    const bool retVal = GDALDatasetSetQueryLoggerFunc(
        hDS,
        [](const char *pszSQL, const char *pszError, int64_t lNumRecords,
           int64_t lExecutionTimeMilliseconds, void *pQueryLoggerArg)
        {
            std::vector<QueryLogEntry> *queryLogLocal{
                reinterpret_cast<std::vector<QueryLogEntry> *>(
                    pQueryLoggerArg)};
            QueryLogEntry entryLocal;
            if (pszSQL)
            {
                entryLocal.sql = pszSQL;
            }
            entryLocal.numRecords = lNumRecords;
            entryLocal.executionTimeMilliseconds = lExecutionTimeMilliseconds;
            if (pszError)
            {
                entryLocal.error = pszError;
            }
            queryLogLocal->push_back(std::move(entryLocal));
        },
        &queryLog);

    ASSERT_TRUE(retVal);
    auto hLayer{GDALDatasetGetLayer(hDS, 0)};
    ASSERT_TRUE(hLayer);
    ASSERT_STREQ(OGR_L_GetName(hLayer), "poly");
    auto poFeature = std::unique_ptr<OGRFeature>(
        OGRFeature::FromHandle(OGR_L_GetNextFeature(hLayer)));
    auto hFeature = OGRFeature::ToHandle(poFeature.get());
    ASSERT_TRUE(hFeature);
    ASSERT_GT(queryLog.size(), 1U);

    QueryLogEntry entry{queryLog.back()};
    ASSERT_EQ(entry.sql.find("SELECT", 0), 0);
    ASSERT_TRUE(entry.executionTimeMilliseconds >= 0);
    ASSERT_EQ(entry.numRecords, -1);
    ASSERT_TRUE(entry.error.empty());

    // Test erroneous query
    OGRLayerH queryResultLayerH{GDALDatasetExecuteSQL(
        hDS, "SELECT * FROM not_existing_table", nullptr, nullptr)};
    GDALDatasetReleaseResultSet(hDS, queryResultLayerH);
    ASSERT_FALSE(queryResultLayerH);

    entry = queryLog.back();
    ASSERT_EQ(entry.sql.find("SELECT * FROM not_existing_table", 0), 0);
    ASSERT_EQ(entry.executionTimeMilliseconds, -1);
    ASSERT_EQ(entry.numRecords, -1);
    ASSERT_FALSE(entry.error.empty());

    // Test prepared arg substitution
    hFeature = OGR_F_Create(OGR_L_GetLayerDefn(hLayer));
    poFeature.reset(OGRFeature::FromHandle(hFeature));
    OGR_F_SetFieldInteger(hFeature, 1, 123);
    OGRErr err = OGR_L_CreateFeature(hLayer, hFeature);
    ASSERT_EQ(OGRERR_NONE, err);

    auto insertEntry = std::find_if(
        queryLog.cbegin(), queryLog.cend(), [](const QueryLogEntry &e)
        { return e.sql.find(R"sql(INSERT INTO "poly")sql", 0) == 0; });

    ASSERT_TRUE(insertEntry != queryLog.end());
    ASSERT_EQ(
        insertEntry->sql.find(
            R"sql(INSERT INTO "poly" ( "geom", "AREA", "EAS_ID", "PRFEDEA") VALUES (NULL, NULL, 123, NULL))sql",
            0),
        0);
}

TEST_F(test_ogr, OGRParseDateTimeYYYYMMDDTHHMMZ)
{
    {
        char szInput[] = "2023-07-11T17:27Z";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMZ(szInput, &sField), true);
        EXPECT_EQ(sField.Date.Year, 2023);
        EXPECT_EQ(sField.Date.Month, 7);
        EXPECT_EQ(sField.Date.Day, 11);
        EXPECT_EQ(sField.Date.Hour, 17);
        EXPECT_EQ(sField.Date.Minute, 27);
        EXPECT_EQ(sField.Date.Second, 0.0f);
        EXPECT_EQ(sField.Date.TZFlag, 100);
    }
    {
        char szInput[] = "2023-07-11T17:27";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMZ(szInput, &sField), true);
        EXPECT_EQ(sField.Date.Year, 2023);
        EXPECT_EQ(sField.Date.Month, 7);
        EXPECT_EQ(sField.Date.Day, 11);
        EXPECT_EQ(sField.Date.Hour, 17);
        EXPECT_EQ(sField.Date.Minute, 27);
        EXPECT_EQ(sField.Date.Second, 0.0f);
        EXPECT_EQ(sField.Date.TZFlag, 0);
    }
    {
        // Invalid
        char szInput[] = "2023-07-11T17:2";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMZ(szInput, &sField), false);
    }
    {
        // Invalid
        char szInput[] = "2023-07-11T17:99";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMZ(szInput, &sField), false);
    }
}

TEST_F(test_ogr, OGRParseDateTimeYYYYMMDDTHHMMSSZ)
{
    {
        char szInput[] = "2023-07-11T17:27:34Z";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSZ(szInput, &sField), true);
        EXPECT_EQ(sField.Date.Year, 2023);
        EXPECT_EQ(sField.Date.Month, 7);
        EXPECT_EQ(sField.Date.Day, 11);
        EXPECT_EQ(sField.Date.Hour, 17);
        EXPECT_EQ(sField.Date.Minute, 27);
        EXPECT_EQ(sField.Date.Second, 34.0f);
        EXPECT_EQ(sField.Date.TZFlag, 100);
    }
    {
        char szInput[] = "2023-07-11T17:27:34";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSZ(szInput, &sField), true);
        EXPECT_EQ(sField.Date.Year, 2023);
        EXPECT_EQ(sField.Date.Month, 7);
        EXPECT_EQ(sField.Date.Day, 11);
        EXPECT_EQ(sField.Date.Hour, 17);
        EXPECT_EQ(sField.Date.Minute, 27);
        EXPECT_EQ(sField.Date.Second, 34.0f);
        EXPECT_EQ(sField.Date.TZFlag, 0);
    }
    {
        // Invalid
        char szInput[] = "2023-07-11T17:27:3";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSZ(szInput, &sField), false);
    }
    {
        // Invalid
        char szInput[] = "2023-07-11T17:27:99";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSZ(szInput, &sField), false);
    }
}

TEST_F(test_ogr, OGRParseDateTimeYYYYMMDDTHHMMSSsssZ)
{
    {
        char szInput[] = "2023-07-11T17:27:34.123Z";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSsssZ(szInput, &sField), true);
        EXPECT_EQ(sField.Date.Year, 2023);
        EXPECT_EQ(sField.Date.Month, 7);
        EXPECT_EQ(sField.Date.Day, 11);
        EXPECT_EQ(sField.Date.Hour, 17);
        EXPECT_EQ(sField.Date.Minute, 27);
        EXPECT_EQ(sField.Date.Second, 34.123f);
        EXPECT_EQ(sField.Date.TZFlag, 100);
    }
    {
        char szInput[] = "2023-07-11T17:27:34.123";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSsssZ(szInput, &sField), true);
        EXPECT_EQ(sField.Date.Year, 2023);
        EXPECT_EQ(sField.Date.Month, 7);
        EXPECT_EQ(sField.Date.Day, 11);
        EXPECT_EQ(sField.Date.Hour, 17);
        EXPECT_EQ(sField.Date.Minute, 27);
        EXPECT_EQ(sField.Date.Second, 34.123f);
        EXPECT_EQ(sField.Date.TZFlag, 0);
    }
    {
        // Invalid
        char szInput[] = "2023-07-11T17:27:34.12";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSsssZ(szInput, &sField), false);
    }
    {
        // Invalid
        char szInput[] = "2023-07-11T17:27:99.123";
        OGRField sField;
        EXPECT_EQ(OGRParseDateTimeYYYYMMDDTHHMMSSsssZ(szInput, &sField), false);
    }
}

TEST_F(test_ogr, OGRGetISO8601DateTime)
{
    OGRField sField;
    sField.Date.Year = 2023;
    sField.Date.Month = 7;
    sField.Date.Day = 11;
    sField.Date.Hour = 17;
    sField.Date.Minute = 27;
    sField.Date.Second = 34.567f;
    sField.Date.TZFlag = 100;
    {
        char szResult[OGR_SIZEOF_ISO8601_DATETIME_BUFFER];
        OGRISO8601Format sFormat;
        sFormat.ePrecision = OGRISO8601Precision::AUTO;
        OGRGetISO8601DateTime(&sField, sFormat, szResult);
        EXPECT_STREQ(szResult, "2023-07-11T17:27:34.567Z");
    }
    {
        char szResult[OGR_SIZEOF_ISO8601_DATETIME_BUFFER];
        OGRISO8601Format sFormat;
        sFormat.ePrecision = OGRISO8601Precision::MILLISECOND;
        OGRGetISO8601DateTime(&sField, sFormat, szResult);
        EXPECT_STREQ(szResult, "2023-07-11T17:27:34.567Z");
    }
    {
        char szResult[OGR_SIZEOF_ISO8601_DATETIME_BUFFER];
        OGRISO8601Format sFormat;
        sFormat.ePrecision = OGRISO8601Precision::SECOND;
        OGRGetISO8601DateTime(&sField, sFormat, szResult);
        EXPECT_STREQ(szResult, "2023-07-11T17:27:35Z");
    }
    {
        char szResult[OGR_SIZEOF_ISO8601_DATETIME_BUFFER];
        OGRISO8601Format sFormat;
        sFormat.ePrecision = OGRISO8601Precision::MINUTE;
        OGRGetISO8601DateTime(&sField, sFormat, szResult);
        EXPECT_STREQ(szResult, "2023-07-11T17:27Z");
    }
    sField.Date.Second = 34.0f;
    {
        char szResult[OGR_SIZEOF_ISO8601_DATETIME_BUFFER];
        OGRISO8601Format sFormat;
        sFormat.ePrecision = OGRISO8601Precision::AUTO;
        OGRGetISO8601DateTime(&sField, sFormat, szResult);
        EXPECT_STREQ(szResult, "2023-07-11T17:27:34Z");
    }
}

// Test calling importFromWkb() multiple times on the same geometry object
TEST_F(test_ogr, importFromWkbReuse)
{
    {
        OGRPoint oPoint;
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oPoint.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x01\x00\x00\x00"                // Point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"    // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          21, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 21);
            EXPECT_EQ(oPoint.getX(), 1.0);
            EXPECT_EQ(oPoint.getY(), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oPoint.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x01\x00\x00\x00"              // Point
                              "\x00\x00\x00\x00\x00\x00\x00\x40"  // 2.0
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"  // 1.0
                              ),
                          21, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 21);
            EXPECT_EQ(oPoint.getX(), 2.0);
            EXPECT_EQ(oPoint.getY(), 1.0);
        }
    }

    {
        OGRLineString oLS;
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oLS.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x02\x00\x00\x00"              // LineString
                              "\x01\x00\x00\x00"                  // 1 point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"  // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          25, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 25);
            ASSERT_EQ(oLS.getNumPoints(), 1);
            EXPECT_EQ(oLS.getX(0), 1.0);
            EXPECT_EQ(oLS.getY(0), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oLS.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x02\x00\x00\x00"              // LineString
                              "\x02\x00\x00\x00"                  // 2 points
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"  // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"  // 2.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"  // 2.0
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"),  // 1.0
                          41, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 41);
            ASSERT_EQ(oLS.getNumPoints(), 2);
            EXPECT_EQ(oLS.getX(0), 1.0);
            EXPECT_EQ(oLS.getY(0), 2.0);
            EXPECT_EQ(oLS.getX(1), 2.0);
            EXPECT_EQ(oLS.getY(1), 1.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oLS.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x02\x00\x00\x00"              // LineString
                              "\x01\x00\x00\x00"                  // 1 point
                              "\x00\x00\x00\x00\x00\x00\x00\x40"  // 2.0
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"),  // 1.0
                          25, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 25);
            ASSERT_EQ(oLS.getNumPoints(), 1);
            EXPECT_EQ(oLS.getX(0), 2.0);
            EXPECT_EQ(oLS.getY(0), 1.0);
        }
    }

    {
        OGRPolygon oPoly;
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oPoly.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x03\x00\x00\x00"                // Polygon
                              "\x01\x00\x00\x00"                    // 1 ring
                              "\x01\x00\x00\x00"                    // 1 point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"    // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          29, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 29);
            ASSERT_TRUE(oPoly.getExteriorRing() != nullptr);
            ASSERT_EQ(oPoly.getNumInteriorRings(), 0);
            auto poLS = oPoly.getExteriorRing();
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 1.0);
            EXPECT_EQ(poLS->getY(0), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oPoly.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x03\x00\x00\x00"                // Polygon
                              "\x01\x00\x00\x00"                    // 1 ring
                              "\x01\x00\x00\x00"                    // 1 point
                              "\x00\x00\x00\x00\x00\x00\x00\x40"    // 2.0
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"),  // 1.0
                          29, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 29);
            ASSERT_TRUE(oPoly.getExteriorRing() != nullptr);
            ASSERT_EQ(oPoly.getNumInteriorRings(), 0);
            auto poLS = oPoly.getExteriorRing();
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 2.0);
            EXPECT_EQ(poLS->getY(0), 1.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oPoly.importFromWkb(reinterpret_cast<const GByte *>(
                                              "\x01\x03\x00\x00\x00"  // Polygon
                                              "\x00\x00\x00\x00"),    // 0 ring
                                          9, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 9);
            ASSERT_TRUE(oPoly.getExteriorRing() == nullptr);
            ASSERT_EQ(oPoly.getNumInteriorRings(), 0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oPoly.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x03\x00\x00\x00"                // Polygon
                              "\x01\x00\x00\x00"                    // 1 ring
                              "\x01\x00\x00\x00"                    // 1 point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"    // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          29, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 29);
            ASSERT_TRUE(oPoly.getExteriorRing() != nullptr);
            ASSERT_EQ(oPoly.getNumInteriorRings(), 0);
            auto poLS = oPoly.getExteriorRing();
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 1.0);
            EXPECT_EQ(poLS->getY(0), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oPoly.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x03\x00\x00\x00"                // Polygon
                              "\x01\x00\x00\x00"                    // 1 ring
                              "\x01\x00\x00\x00"                    // 1 point
                              "\x00\x00\x00\x00\x00\x00\x00\x40"    // 2.0
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"),  // 1.0
                          static_cast<size_t>(-1), wkbVariantIso,
                          nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 29);
            ASSERT_TRUE(oPoly.getExteriorRing() != nullptr);
            ASSERT_EQ(oPoly.getNumInteriorRings(), 0);
            auto poLS = oPoly.getExteriorRing();
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 2.0);
            EXPECT_EQ(poLS->getY(0), 1.0);
        }
        {
            size_t nBytesConsumed = 0;
            // Truncated WKB
            EXPECT_NE(oPoly.importFromWkb(reinterpret_cast<const GByte *>(
                                              "\x01\x03\x00\x00\x00"  // Polygon
                                              "\x01\x00\x00\x00"      // 1 ring
                                              "\x01\x00\x00\x00"),    // 1 point
                                          13, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            ASSERT_TRUE(oPoly.getExteriorRing() == nullptr);
            ASSERT_EQ(oPoly.getNumInteriorRings(), 0);
        }
    }

    {
        OGRMultiLineString oMLS;
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oMLS.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x05\x00\x00\x00"  // MultiLineString
                              "\x01\x00\x00\x00"      // 1-part
                              "\x01\x02\x00\x00\x00"  // LineString
                              "\x01\x00\x00\x00"      // 1 point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"    // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          34, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 34);
            ASSERT_EQ(oMLS.getNumGeometries(), 1);
            auto poLS = oMLS.getGeometryRef(0);
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 1.0);
            EXPECT_EQ(poLS->getY(0), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oMLS.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x05\x00\x00\x00"  // MultiLineString
                              "\x01\x00\x00\x00"      // 1-part
                              "\x01\x02\x00\x00\x00"  // LineString
                              "\x01\x00\x00\x00"      // 1 point
                              "\x00\x00\x00\x00\x00\x00\x00\x40"    // 2.0
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"),  // 1.0
                          34, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 34);
            ASSERT_EQ(oMLS.getNumGeometries(), 1);
            auto poLS = oMLS.getGeometryRef(0);
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 2.0);
            EXPECT_EQ(poLS->getY(0), 1.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oMLS.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x05\x00\x00\x00"  // MultiLineString
                              "\x01\x00\x00\x00"      // 1-part
                              "\x01\x02\x00\x00\x00"  // LineString
                              "\x01\x00\x00\x00"      // 1 point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"    // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          static_cast<size_t>(-1), wkbVariantIso,
                          nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 34);
            ASSERT_EQ(oMLS.getNumGeometries(), 1);
            auto poLS = oMLS.getGeometryRef(0);
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 1.0);
            EXPECT_EQ(poLS->getY(0), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            // Truncated WKB
            EXPECT_NE(oMLS.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x05\x00\x00\x00"  // MultiLineString
                              "\x01\x00\x00\x00"      // 1-part
                              "\x01\x02\x00\x00\x00"  // LineString
                              "\x01\x00\x00\x00"      // 1 point
                              ),
                          18, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            ASSERT_EQ(oMLS.getNumGeometries(), 0);
        }
    }

    {
        OGRMultiPolygon oMP;
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oMP.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x06\x00\x00\x00"  // MultiPolygon
                              "\x01\x00\x00\x00"      // 1-part
                              "\x01\x03\x00\x00\x00"  // Polygon
                              "\x01\x00\x00\x00"      // 1 ring
                              "\x01\x00\x00\x00"      // 1 point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"    // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          38, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 38);
            ASSERT_EQ(oMP.getNumGeometries(), 1);
            auto poPoly = oMP.getGeometryRef(0);
            ASSERT_TRUE(poPoly->getExteriorRing() != nullptr);
            ASSERT_EQ(poPoly->getNumInteriorRings(), 0);
            auto poLS = poPoly->getExteriorRing();
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 1.0);
            EXPECT_EQ(poLS->getY(0), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oMP.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x06\x00\x00\x00"  // MultiPolygon
                              "\x01\x00\x00\x00"      // 1-part
                              "\x01\x03\x00\x00\x00"  // Polygon
                              "\x01\x00\x00\x00"      // 1 ring
                              "\x01\x00\x00\x00"      // 1 point
                              "\x00\x00\x00\x00\x00\x00\x00\x40"    // 2.0
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"),  // 1.0
                          38, wkbVariantIso, nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 38);
            ASSERT_EQ(oMP.getNumGeometries(), 1);
            auto poPoly = oMP.getGeometryRef(0);
            ASSERT_TRUE(poPoly->getExteriorRing() != nullptr);
            ASSERT_EQ(poPoly->getNumInteriorRings(), 0);
            auto poLS = poPoly->getExteriorRing();
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 2.0);
            EXPECT_EQ(poLS->getY(0), 1.0);
        }
        {
            size_t nBytesConsumed = 0;
            EXPECT_EQ(oMP.importFromWkb(
                          reinterpret_cast<const GByte *>(
                              "\x01\x06\x00\x00\x00"  // MultiPolygon
                              "\x01\x00\x00\x00"      // 1-part
                              "\x01\x03\x00\x00\x00"  // Polygon
                              "\x01\x00\x00\x00"      // 1 ring
                              "\x01\x00\x00\x00"      // 1 point
                              "\x00\x00\x00\x00\x00\x00\xf0\x3f"    // 1.0
                              "\x00\x00\x00\x00\x00\x00\x00\x40"),  // 2.0
                          static_cast<size_t>(-1), wkbVariantIso,
                          nBytesConsumed),
                      OGRERR_NONE);
            EXPECT_EQ(nBytesConsumed, 38);
            ASSERT_EQ(oMP.getNumGeometries(), 1);
            auto poPoly = oMP.getGeometryRef(0);
            ASSERT_TRUE(poPoly->getExteriorRing() != nullptr);
            ASSERT_EQ(poPoly->getNumInteriorRings(), 0);
            auto poLS = poPoly->getExteriorRing();
            ASSERT_EQ(poLS->getNumPoints(), 1);
            EXPECT_EQ(poLS->getX(0), 1.0);
            EXPECT_EQ(poLS->getY(0), 2.0);
        }
        {
            size_t nBytesConsumed = 0;
            // Truncated WKB
            EXPECT_NE(
                oMP.importFromWkb(reinterpret_cast<const GByte *>(
                                      "\x01\x06\x00\x00\x00"  // MultiPolygon
                                      "\x01\x00\x00\x00"      // 1-part
                                      "\x01\x03\x00\x00\x00"  // Polygon
                                      "\x01\x00\x00\x00"      // 1 ring
                                      "\x01\x00\x00\x00"      // 1 point
                                      ),
                                  22, wkbVariantIso, nBytesConsumed),
                OGRERR_NONE);
            ASSERT_EQ(oMP.getNumGeometries(), 0);
        }
    }
}

// Test sealing functionality on OGRFieldDefn
TEST_F(test_ogr, OGRFieldDefn_sealing)
{
    OGRFieldDefn oFieldDefn("test", OFTString);
    oFieldDefn.Seal();

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetType(OFTInteger);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetSubType(OFSTJSON);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetWidth(1);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetPrecision(1);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetDefault("");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetUnique(true);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetNullable(false);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetComment("");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetAlternativeName("");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetDomainName("");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetTZFlag(0);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        auto oTemporaryUnsealer(oFieldDefn.GetTemporaryUnsealer());
        CPLErrorReset();
        oFieldDefn.SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") == nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        whileUnsealing(&oFieldDefn)->SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }
}

// Test sealing functionality on OGRGeomFieldDefn
TEST_F(test_ogr, OGRGeomFieldDefn_sealing)
{
    OGRGeomFieldDefn oFieldDefn("test", wkbUnknown);

    oFieldDefn.Seal();

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetType(wkbPoint);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetSpatialRef(nullptr);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetNullable(false);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        auto oTemporaryUnsealer(oFieldDefn.GetTemporaryUnsealer());
        CPLErrorReset();
        oFieldDefn.SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFieldDefn.SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        whileUnsealing(&oFieldDefn)->SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }
}

// Test sealing functionality on OGRFeatureDefn
TEST_F(test_ogr, OGRFeatureDefn_sealing)
{
    OGRFeatureDefn oFDefn;
    CPLErrorReset();
    {
        oFDefn.SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }
    {
        OGRFieldDefn oFieldDefn("test", OFTString);
        oFDefn.AddFieldDefn(&oFieldDefn);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }
    {
        OGRGeomFieldDefn oFieldDefn("test", wkbUnknown);
        oFDefn.AddGeomFieldDefn(&oFieldDefn);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFDefn.Unseal(true);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "unsealed") != nullptr);

        CPLErrorReset();
        auto oTemporaryUnsealer1(oFDefn.GetTemporaryUnsealer(false));
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "unsealed") != nullptr);
        CPLErrorReset();
        auto oTemporaryUnsealer2(oFDefn.GetTemporaryUnsealer(false));
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }

    oFDefn.Seal(true);

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFDefn.Seal(true);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFDefn.SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        OGRFieldDefn oFieldDefn("test2", OFTString);
        oFDefn.AddFieldDefn(&oFieldDefn);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFDefn.DeleteFieldDefn(0);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        int map[] = {0};
        oFDefn.ReorderFieldDefns(map);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        OGRGeomFieldDefn oFieldDefn("test2", wkbUnknown);
        oFDefn.AddGeomFieldDefn(&oFieldDefn);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        oFDefn.DeleteGeomFieldDefn(0);
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        auto oTemporaryUnsealer(oFDefn.GetTemporaryUnsealer(false));
        CPLErrorReset();
        oFDefn.SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);

        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        oFDefn.GetFieldDefn(0)->SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);

        CPLErrorReset();
        oFDefn.GetGeomFieldDefn(0)->SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {

        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        oFDefn.GetFieldDefn(0)->SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);

        CPLErrorReset();
        oFDefn.GetGeomFieldDefn(0)->SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        auto oTemporaryUnsealer(oFDefn.GetTemporaryUnsealer(true));
        CPLErrorReset();
        oFDefn.SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);

        oFDefn.GetFieldDefn(0)->SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);

        auto oTemporaryUnsealer2(oFDefn.GetTemporaryUnsealer(true));

        oFDefn.GetGeomFieldDefn(0)->SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }

    {

        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        oFDefn.GetFieldDefn(0)->SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);

        CPLErrorReset();
        oFDefn.GetGeomFieldDefn(0)->SetName("new_name");
        EXPECT_TRUE(strstr(CPLGetLastErrorMsg(), "sealed") != nullptr);
    }

    {
        CPLErrorReset();
        whileUnsealing(&oFDefn)->SetName("new_name");
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }
}

// Test wkbExportOptions
TEST_F(test_ogr, wkbExportOptions_default)
{
    OGRwkbExportOptions *psOptions = OGRwkbExportOptionsCreate();
    ASSERT_TRUE(psOptions != nullptr);
    OGRPoint p(1.23456789012345678, 2.23456789012345678, 3);
    std::vector<GByte> abyWKB(p.WkbSize());
    OGR_G_ExportToWkbEx(OGRGeometry::ToHandle(&p), &abyWKB[0], psOptions);
    OGRwkbExportOptionsDestroy(psOptions);

    std::vector<GByte> abyRegularWKB(p.WkbSize());
    OGR_G_ExportToWkb(OGRGeometry::ToHandle(&p), wkbNDR, &abyRegularWKB[0]);

    EXPECT_TRUE(abyWKB == abyRegularWKB);
}

// Test wkbExportOptions
TEST_F(test_ogr, wkbExportOptions)
{
    OGRwkbExportOptions *psOptions = OGRwkbExportOptionsCreate();
    ASSERT_TRUE(psOptions != nullptr);
    OGRwkbExportOptionsSetByteOrder(psOptions, wkbXDR);
    OGRwkbExportOptionsSetVariant(psOptions, wkbVariantIso);

    auto hPrec = OGRGeomCoordinatePrecisionCreate();
    OGRGeomCoordinatePrecisionSet(hPrec, 1e-1, 1e-2, 1e-4);
    OGRwkbExportOptionsSetPrecision(psOptions, hPrec);
    OGRGeomCoordinatePrecisionDestroy(hPrec);

    OGRPoint p(1.23456789012345678, -1.23456789012345678, 1.23456789012345678,
               1.23456789012345678);
    std::vector<GByte> abyWKB(p.WkbSize());
    OGR_G_ExportToWkbEx(OGRGeometry::ToHandle(&p), &abyWKB[0], psOptions);
    OGRwkbExportOptionsDestroy(psOptions);

    const std::vector<GByte> expectedWKB{
        0x00, 0x00, 0x00, 0x0B, 0xB9, 0x3F, 0xF3, 0x80, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xBF, 0xF3, 0x80, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x3F, 0xF3, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F,
        0xF3, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(abyWKB == expectedWKB);

    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    ASSERT_NE(poGeom, nullptr);
    EXPECT_NEAR(poGeom->toPoint()->getX(), 1.2, 1e-1);
    EXPECT_NEAR(poGeom->toPoint()->getY(), -1.2, 1e-1);
    EXPECT_NEAR(poGeom->toPoint()->getZ(), 1.23, 1e-2);
    EXPECT_NEAR(poGeom->toPoint()->getM(), 1.2346, 1e-4);
    delete poGeom;
}

// Test OGRGeometry::roundCoordinatesIEEE754()
TEST_F(test_ogr, roundCoordinatesIEEE754)
{
    OGRLineString oLS;
    oLS.addPoint(1.2345678901234, -1.2345678901234, -1.2345678901234, 0.012345);
    oLS.addPoint(-1.2345678901234, 1.2345678901234, 1.2345678901234, -0.012345);
    oLS.addPoint(std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::quiet_NaN());
    OGRGeomCoordinateBinaryPrecision sBinaryPrecision;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.dfXYResolution = 1e-10;
    sPrecision.dfZResolution = 1e-3;
    sPrecision.dfMResolution = 1e-5;
    sBinaryPrecision.SetFrom(sPrecision);
    OGRLineString oLSOri(oLS);
    oLS.roundCoordinatesIEEE754(sBinaryPrecision);
    EXPECT_NE(oLS.getX(0), oLSOri.getX(0));
    EXPECT_NE(oLS.getY(0), oLSOri.getY(0));
    EXPECT_NE(oLS.getZ(0), oLSOri.getZ(0));
    EXPECT_NE(oLS.getM(0), oLSOri.getM(0));
    EXPECT_NEAR(oLS.getX(0), oLSOri.getX(0), sPrecision.dfXYResolution);
    EXPECT_NEAR(oLS.getY(0), oLSOri.getY(0), sPrecision.dfXYResolution);
    EXPECT_NEAR(oLS.getZ(0), oLSOri.getZ(0), sPrecision.dfZResolution);
    EXPECT_NEAR(oLS.getM(0), oLSOri.getM(0), sPrecision.dfMResolution);
    EXPECT_NEAR(oLS.getX(1), oLSOri.getX(1), sPrecision.dfXYResolution);
    EXPECT_NEAR(oLS.getY(1), oLSOri.getY(1), sPrecision.dfXYResolution);
    EXPECT_NEAR(oLS.getZ(1), oLSOri.getZ(1), sPrecision.dfZResolution);
    EXPECT_NEAR(oLS.getM(1), oLSOri.getM(1), sPrecision.dfMResolution);
    EXPECT_EQ(oLS.getX(2), std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(oLS.getY(2)));
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_linestring_2d_xy_precision)
{
    OGRLineString oLS;
    oLS.addPoint(1.2345678901234, -1.2345678901234);
    oLS.addPoint(-1.2345678901234, 1.2345678901234);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.dfXYResolution = 1e-10;
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oLS.WkbSize());
    oLS.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toLineString()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toLineString()->getY(0), oLS.getY(0));
    EXPECT_NEAR(poGeom->toLineString()->getX(0), oLS.getX(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(0), oLS.getY(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getX(1), oLS.getX(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(1), oLS.getY(1),
                sPrecision.dfXYResolution);
    delete poGeom;
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_linestring_3d_discard_lsb_bits)
{
    OGRLineString oLS;
    oLS.addPoint(1.2345678901234, -1.2345678901234, -1.2345678901234);
    oLS.addPoint(-1.2345678901234, 1.2345678901234, 1.2345678901234);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.dfXYResolution = 1e-10;
    sPrecision.dfZResolution = 1e-3;
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oLS.WkbSize());
    oLS.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toLineString()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toLineString()->getY(0), oLS.getY(0));
    EXPECT_NE(poGeom->toLineString()->getZ(0), oLS.getZ(0));
    EXPECT_NEAR(poGeom->toLineString()->getX(0), oLS.getX(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(0), oLS.getY(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getZ(0), oLS.getZ(0),
                sPrecision.dfZResolution);
    EXPECT_NEAR(poGeom->toLineString()->getX(1), oLS.getX(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(1), oLS.getY(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getZ(1), oLS.getZ(1),
                sPrecision.dfZResolution);
    delete poGeom;
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_linestring_xym_discard_lsb_bits)
{
    OGRLineString oLS;
    oLS.addPointM(1.2345678901234, -1.2345678901234, -1.2345678901234);
    oLS.addPointM(-1.2345678901234, 1.2345678901234, 1.2345678901234);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.dfXYResolution = 1e-10;
    sPrecision.dfMResolution = 1e-3;
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oLS.WkbSize());
    oLS.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toLineString()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toLineString()->getY(0), oLS.getY(0));
    EXPECT_NE(poGeom->toLineString()->getM(0), oLS.getM(0));
    EXPECT_NEAR(poGeom->toLineString()->getX(0), oLS.getX(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(0), oLS.getY(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getM(0), oLS.getM(0),
                sPrecision.dfMResolution);
    EXPECT_NEAR(poGeom->toLineString()->getX(1), oLS.getX(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(1), oLS.getY(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getM(1), oLS.getM(1),
                sPrecision.dfMResolution);
    delete poGeom;
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_linestring_xyzm_discard_lsb_bits)
{
    OGRLineString oLS;
    oLS.addPoint(1.2345678901234, -1.2345678901234, -1.2345678901234, 0.012345);
    oLS.addPoint(-1.2345678901234, 1.2345678901234, 1.2345678901234, 0.012345);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.dfXYResolution = 1e-10;
    sPrecision.dfZResolution = 1e-3;
    sPrecision.dfMResolution = 1e-5;
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oLS.WkbSize());
    oLS.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toLineString()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toLineString()->getY(0), oLS.getY(0));
    EXPECT_NE(poGeom->toLineString()->getZ(0), oLS.getZ(0));
    EXPECT_NE(poGeom->toLineString()->getM(0), oLS.getM(0));
    EXPECT_NEAR(poGeom->toLineString()->getX(0), oLS.getX(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(0), oLS.getY(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getZ(0), oLS.getZ(0),
                sPrecision.dfZResolution);
    EXPECT_NEAR(poGeom->toLineString()->getM(0), oLS.getM(0),
                sPrecision.dfMResolution);
    EXPECT_NEAR(poGeom->toLineString()->getX(1), oLS.getX(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getY(1), oLS.getY(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toLineString()->getZ(1), oLS.getZ(1),
                sPrecision.dfZResolution);
    EXPECT_NEAR(poGeom->toLineString()->getM(1), oLS.getM(1),
                sPrecision.dfMResolution);
    delete poGeom;
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_polygon_2d_xy_precision)
{
    OGRLinearRing oLS;
    oLS.addPoint(1.2345678901234, -1.2345678901234);
    oLS.addPoint(-1.2345678901234, -1.2345678901234);
    oLS.addPoint(-2.2345678901234, 1.2345678901234);
    oLS.addPoint(1.2345678901234, -1.2345678901234);
    OGRPolygon oPoly;
    oPoly.addRing(&oLS);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.dfXYResolution = 1e-10;
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oPoly.WkbSize());
    oPoly.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0));
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(1), oLS.getX(1),
                sPrecision.dfXYResolution);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(1), oLS.getY(1),
                sPrecision.dfXYResolution);
    delete poGeom;
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_polygon_3d_discard_lsb_bits)
{
    OGRLinearRing oLS;
    oLS.addPoint(1.2345678901234, -1.2345678901234, 1.2345678901234);
    oLS.addPoint(-1.2345678901234, -1.2345678901234, -1.2345678901234);
    oLS.addPoint(-2.2345678901234, 1.2345678901234, -1.2345678901234);
    oLS.addPoint(1.2345678901234, -1.2345678901234, 1.2345678901234);
    OGRPolygon oPoly;
    oPoly.addRing(&oLS);
    OGRSpatialReference oSRS;
    oSRS.importFromEPSG(4326);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.SetFromMeter(&oSRS, 1e-3, 1e-3, 0);
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oPoly.WkbSize());
    oPoly.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getZ(0), oLS.getZ(0));
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getZ(0), oLS.getZ(0),
                1e-3);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(1), oLS.getX(1),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(1), oLS.getY(1),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getZ(1), oLS.getZ(1),
                1e-3);
    delete poGeom;
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_polygon_xym_discard_lsb_bits)
{
    OGRLinearRing oLS;
    oLS.addPointM(1.2345678901234, -1.2345678901234, 1.2345678901234);
    oLS.addPointM(-1.2345678901234, -1.2345678901234, -1.2345678901234);
    oLS.addPointM(-2.2345678901234, 1.2345678901234, -1.2345678901234);
    oLS.addPointM(1.2345678901234, -1.2345678901234, 1.2345678901234);
    OGRPolygon oPoly;
    oPoly.addRing(&oLS);
    OGRSpatialReference oSRS;
    oSRS.importFromEPSG(4326);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.SetFromMeter(&oSRS, 1e-3, 0, 1e-3);
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oPoly.WkbSize());
    oPoly.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getM(0), oLS.getM(0));
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getM(0), oLS.getM(0),
                1e-3);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(1), oLS.getX(1),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(1), oLS.getY(1),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getM(1), oLS.getM(1),
                1e-3);
    delete poGeom;
}

// Test discarding of bits in WKB export
TEST_F(test_ogr, wkb_polygon_xyzm_discard_lsb_bits)
{
    OGRLinearRing oLS;
    oLS.addPoint(1.2345678901234, -1.2345678901234, 1.2345678901234, 0.012345);
    oLS.addPoint(-1.2345678901234, -1.2345678901234, -1.2345678901234, 12345);
    oLS.addPoint(-2.2345678901234, 1.2345678901234, -1.2345678901234, 0.012345);
    oLS.addPoint(1.2345678901234, -1.2345678901234, 1.2345678901234, 0.012345);
    OGRPolygon oPoly;
    oPoly.addRing(&oLS);
    OGRSpatialReference oSRS;
    oSRS.importFromEPSG(4326);
    OGRwkbExportOptions sOptions;
    OGRGeomCoordinatePrecision sPrecision;
    sPrecision.SetFromMeter(&oSRS, 1e-3, 1e-3, 1e-4);
    sOptions.sPrecision.SetFrom(sPrecision);
    std::vector<GByte> abyWKB(oPoly.WkbSize());
    oPoly.exportToWkb(&abyWKB[0], &sOptions);
    for (int i = 0; i < oLS.getDimension() * oLS.getNumPoints(); ++i)
    {
        EXPECT_EQ(abyWKB[5 + 4 + 4 + 0 + 8 * i], 0);
    }
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWKB.data(), nullptr, &poGeom);
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getZ(0), oLS.getZ(0));
    EXPECT_NE(poGeom->toPolygon()->getExteriorRing()->getM(0), oLS.getM(0));
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(0), oLS.getX(0),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(0), oLS.getY(0),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getZ(0), oLS.getZ(0),
                1e-3);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getM(0), oLS.getM(0),
                1e-4);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getX(1), oLS.getX(1),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getY(1), oLS.getY(1),
                8.9e-9);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getZ(1), oLS.getZ(1),
                1e-3);
    EXPECT_NEAR(poGeom->toPolygon()->getExteriorRing()->getM(1), oLS.getM(1),
                1e-4);
    delete poGeom;
}

// Test OGRFeature::SerializeToBinary() and DeserializeFromBinary();
TEST_F(test_ogr, OGRFeature_SerializeToBinary)
{
    {
        OGRFeatureDefn oFDefn;
        oFDefn.SetGeomType(wkbNone);
        oFDefn.Reference();

        {
            OGRFeature oFeatSrc(&oFDefn);
            oFeatSrc.SetFID(1);
            std::vector<GByte> abyBuffer;

            EXPECT_TRUE(oFeatSrc.SerializeToBinary(abyBuffer));
            EXPECT_EQ(abyBuffer.size(), 1);
            EXPECT_EQ(abyBuffer[0], 1);

            OGRFeature oFeatDst(&oFDefn);
            EXPECT_FALSE(oFeatDst.DeserializeFromBinary(abyBuffer.data(), 0));
            EXPECT_TRUE(oFeatDst.DeserializeFromBinary(abyBuffer.data(),
                                                       abyBuffer.size()));
            EXPECT_EQ(oFeatDst.GetFID(), 1);
        }

        {
            OGRFeature oFeatSrc(&oFDefn);
            oFeatSrc.SetFID(static_cast<GIntBig>(-12345678901234));
            std::vector<GByte> abyBuffer;

            EXPECT_TRUE(oFeatSrc.SerializeToBinary(abyBuffer));

            OGRFeature oFeatDst(&oFDefn);
            // Try truncated buffers
            for (size_t i = 0; i < abyBuffer.size(); ++i)
            {
                EXPECT_FALSE(
                    oFeatDst.DeserializeFromBinary(abyBuffer.data(), i));
            }
            EXPECT_TRUE(oFeatDst.DeserializeFromBinary(abyBuffer.data(),
                                                       abyBuffer.size()));
            EXPECT_EQ(oFeatDst.GetFID(), static_cast<GIntBig>(-12345678901234));
        }
    }

    {
        OGRFeatureDefn oFDefn;
        oFDefn.Reference();
        {
            OGRFieldDefn oFieldDefn("int", OFTInteger);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int64", OFTInteger64);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("real", OFTReal);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("str", OFTString);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("binary", OFTBinary);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("intlist", OFTIntegerList);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int64list", OFTInteger64List);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("reallist", OFTRealList);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("strlist", OFTStringList);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("date", OFTDate);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("time", OFTTime);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("datetime", OFTDateTime);
            oFDefn.AddFieldDefn(&oFieldDefn);
        }

        {
            OGRFeature oFeatSrc(&oFDefn);
            std::vector<GByte> abyBuffer;

            EXPECT_TRUE(oFeatSrc.SerializeToBinary(abyBuffer));
            EXPECT_EQ(abyBuffer.size(), 5);

            OGRFeature oFeatDst(&oFDefn);
            for (size_t i = 0; i < abyBuffer.size(); ++i)
            {
                EXPECT_FALSE(
                    oFeatDst.DeserializeFromBinary(abyBuffer.data(), i));
            }
            EXPECT_TRUE(oFeatDst.DeserializeFromBinary(abyBuffer.data(),
                                                       abyBuffer.size()));
            EXPECT_TRUE(oFeatDst.Equal(&oFeatSrc));
        }

        {
            OGRFeature oFeatSrc(&oFDefn);
            std::vector<GByte> abyBuffer;

            const int iFieldInt = oFDefn.GetFieldIndex("int");
            ASSERT_TRUE(iFieldInt >= 0);
            oFeatSrc.SetFieldNull(iFieldInt);
            EXPECT_TRUE(oFeatSrc.SerializeToBinary(abyBuffer));
            EXPECT_EQ(abyBuffer.size(), 5);

            OGRFeature oFeatDst(&oFDefn);

            // Try truncated buffers
            for (size_t i = 0; i < abyBuffer.size(); ++i)
            {
                EXPECT_FALSE(
                    oFeatDst.DeserializeFromBinary(abyBuffer.data(), i));
            }

            EXPECT_TRUE(oFeatDst.DeserializeFromBinary(abyBuffer.data(),
                                                       abyBuffer.size()));
            EXPECT_TRUE(oFeatDst.Equal(&oFeatSrc));
        }

        {
            OGRFeature oFeatSrc(&oFDefn);
            oFeatSrc.SetFID(1);
            oFeatSrc.SetField("int", -123);
            oFeatSrc.SetField("int64", static_cast<GIntBig>(-12345678901234));
            oFeatSrc.SetField("real", 1.25);
            oFeatSrc.SetField("str", "foo");
            const int iFieldBinary = oFDefn.GetFieldIndex("binary");
            ASSERT_TRUE(iFieldBinary >= 0);
            oFeatSrc.SetField(iFieldBinary, 3,
                              static_cast<const void *>("abc"));
            oFeatSrc.SetField("intlist", 2,
                              std::vector<int>{1, -123456}.data());
            oFeatSrc.SetField("int64list", 2,
                              std::vector<GIntBig>{1, -12345678901234}.data());
            oFeatSrc.SetField("reallist", 2,
                              std::vector<double>{1.5, -2.5}.data());
            CPLStringList aosList;
            aosList.AddString("foo");
            aosList.AddString("barbaz");
            oFeatSrc.SetField("strlist", aosList.List());
            oFeatSrc.SetField("date", 2023, 1, 3);
            oFeatSrc.SetField("time", 0, 0, 0, 12, 34, 56.789f);
            oFeatSrc.SetField("datetime", 2023, 1, 3, 12, 34, 56.789f);
            OGRPoint p(1, 2);
            oFeatSrc.SetGeometry(&p);
            std::vector<GByte> abyBuffer;

            EXPECT_TRUE(oFeatSrc.SerializeToBinary(abyBuffer));

            OGRFeature oFeatDst(&oFDefn);

            // Try truncated buffers
            for (size_t i = 0; i < abyBuffer.size(); ++i)
            {
                EXPECT_FALSE(
                    oFeatDst.DeserializeFromBinary(abyBuffer.data(), i));
            }

            // Try corrupted buffers
            {
                CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
                for (size_t i = 0; i < abyBuffer.size(); ++i)
                {
                    // Might succeed or fail, but shouldn't crash..
                    const GByte backup = abyBuffer[i];
                    abyBuffer[i] = static_cast<GByte>(~abyBuffer[i]);
                    (void)oFeatDst.DeserializeFromBinary(abyBuffer.data(),
                                                         abyBuffer.size());
                    abyBuffer[i] = backup;
                }
            }

            EXPECT_TRUE(oFeatDst.DeserializeFromBinary(abyBuffer.data(),
                                                       abyBuffer.size()));
            // oFeatSrc.DumpReadable(stdout);
            // oFeatDst.DumpReadable(stdout);
            EXPECT_TRUE(oFeatDst.Equal(&oFeatSrc));
        }
    }
}

// Test OGRGeometry::IsRectangle()
TEST_F(test_ogr, OGRGeometry_IsRectangle)
{
    // Not a polygon
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POINT EMPTY", nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Polygon empty
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON EMPTY", nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Polygon with inner ring
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(
            "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 "
            "0.2,0.2 0.2))",
            nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Polygon with 3 points
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON ((0 0,0 1,1 1))", nullptr,
                                          &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Polygon with 6 points
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(
            "POLYGON ((0 0,0.1 0,0.2 0,0.3 0,1 1,0 0))", nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Polygon with 5 points, but last one not matching first (invalid)
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(
            "POLYGON ((0 0,0 1,1 1,1 0,-999 -999))", nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Polygon with 5 points, but not rectangle
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON ((0 0,0 1.1,1 1,1 0,0 0))",
                                          nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Rectangle (type 1)
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))",
                                          nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_TRUE(poGeom->IsRectangle());
        delete poGeom;
    }
    // Rectangle2(type 1)
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON ((0 0,1 0,1 1,0 1,0 0))",
                                          nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_TRUE(poGeom->IsRectangle());
        delete poGeom;
    }
}

// Test OGRGeometry::removeEmptyParts()
TEST_F(test_ogr, OGRGeometry_removeEmptyParts)
{
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POINT EMPTY", nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_TRUE(poGeom->IsEmpty());
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON ((0 0,0 1,1 0,0 0))",
                                          nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_NE(poGeom->toPolygon()->getExteriorRing(), nullptr);
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON ((0 0,0 1,1 0,0 0))",
                                          nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        poGeom->toPolygon()->addRingDirectly(new OGRLinearRing());
        EXPECT_EQ(poGeom->toPolygon()->getNumInteriorRings(), 1);
        EXPECT_TRUE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_NE(poGeom->toPolygon()->getExteriorRing(), nullptr);
        EXPECT_EQ(poGeom->toPolygon()->getNumInteriorRings(), 0);
        EXPECT_FALSE(poGeom->hasEmptyParts());
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("COMPOUNDCURVE ((0 0,1 1))", nullptr,
                                          &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_EQ(poGeom->toCompoundCurve()->getNumCurves(), 1);
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("COMPOUNDCURVE ((0 0,1 1),(1 1,2 2))",
                                          nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        poGeom->toCompoundCurve()->getCurve(1)->empty();
        EXPECT_EQ(poGeom->toCompoundCurve()->getNumCurves(), 2);
        EXPECT_TRUE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_FALSE(poGeom->hasEmptyParts());
        EXPECT_EQ(poGeom->toCompoundCurve()->getNumCurves(), 1);
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("GEOMETRYCOLLECTION (POINT(0 0))",
                                          nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_EQ(poGeom->toGeometryCollection()->getNumGeometries(), 1);
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(
            "GEOMETRYCOLLECTION (POINT EMPTY,POINT(0 0),POINT EMPTY)", nullptr,
            &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_EQ(poGeom->toGeometryCollection()->getNumGeometries(), 3);
        EXPECT_TRUE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_FALSE(poGeom->hasEmptyParts());
        EXPECT_EQ(poGeom->toGeometryCollection()->getNumGeometries(), 1);
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt("GEOMETRYCOLLECTION EMPTY", nullptr,
                                          &poGeom);
        ASSERT_NE(poGeom, nullptr);
        OGRGeometry *poPoly = nullptr;
        OGRGeometryFactory::createFromWkt("POLYGON ((0 0,0 1,1 0,0 0))",
                                          nullptr, &poPoly);
        EXPECT_NE(poPoly, nullptr);
        if (poPoly)
        {
            poPoly->toPolygon()->addRingDirectly(new OGRLinearRing());
            poGeom->toGeometryCollection()->addGeometryDirectly(poPoly);
            EXPECT_EQ(poGeom->toGeometryCollection()->getNumGeometries(), 1);
            EXPECT_TRUE(poGeom->hasEmptyParts());
            poGeom->removeEmptyParts();
            EXPECT_FALSE(poGeom->hasEmptyParts());
            EXPECT_EQ(poGeom->toGeometryCollection()->getNumGeometries(), 1);
        }
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(
            "POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))", nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        EXPECT_FALSE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_EQ(poGeom->toPolyhedralSurface()->getNumGeometries(), 1);
        delete poGeom;
    }
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(
            "POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))", nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        poGeom->toPolyhedralSurface()->addGeometryDirectly(new OGRPolygon());
        EXPECT_EQ(poGeom->toPolyhedralSurface()->getNumGeometries(), 2);
        EXPECT_TRUE(poGeom->hasEmptyParts());
        poGeom->removeEmptyParts();
        EXPECT_FALSE(poGeom->hasEmptyParts());
        EXPECT_EQ(poGeom->toPolyhedralSurface()->getNumGeometries(), 1);
        delete poGeom;
    }
}

// Test OGRCurve::reversePoints()
TEST_F(test_ogr, OGRCurve_reversePoints)
{
    {
        OGRGeometry *poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(
            "COMPOUNDCURVE ZM (CIRCULARSTRING ZM (0 0 10 20,1 1 11 21,2 0 12 "
            "22),(2 0 12 22,3 0 13 2))",
            nullptr, &poGeom);
        ASSERT_NE(poGeom, nullptr);
        poGeom->toCurve()->reversePoints();
        char *pszWKT = nullptr;
        poGeom->exportToWkt(&pszWKT, wkbVariantIso);
        EXPECT_TRUE(pszWKT != nullptr);
        if (pszWKT)
        {
            EXPECT_STREQ(
                pszWKT, "COMPOUNDCURVE ZM ((3 0 13 2,2 0 12 22),CIRCULARSTRING "
                        "ZM (2 0 12 22,1 1 11 21,0 0 10 20))");
        }
        CPLFree(pszWKT);
        delete poGeom;
    }
}

// Test OGRGeometryFactory::transformWithOptions()
TEST_F(test_ogr, transformWithOptions)
{
    // Projected CRS to national geographic CRS (not including poles or antimeridian)
    auto [poGeom, err] = OGRGeometryFactory::createFromWkt(
        "LINESTRING(700000 6600000, 700001 6600001)");
    ASSERT_NE(poGeom, nullptr);

    OGRSpatialReference oEPSG_2154;
    oEPSG_2154.importFromEPSG(2154);  // "RGF93 v1 / Lambert-93"
    OGRSpatialReference oEPSG_4171;
    oEPSG_4171.importFromEPSG(4171);  // "RGF93 v1"
    oEPSG_4171.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
        OGRCreateCoordinateTransformation(&oEPSG_2154, &oEPSG_4171));
    OGRGeometryFactory::TransformWithOptionsCache oCache;
    auto poNewGeom =
        std::unique_ptr<OGRGeometry>(OGRGeometryFactory::transformWithOptions(
            poGeom.get(), poCT.get(), nullptr, oCache));
    ASSERT_NE(poNewGeom, nullptr);
    EXPECT_NEAR(poNewGeom->toLineString()->getX(0), 3, 1e-8);
    EXPECT_NEAR(poNewGeom->toLineString()->getY(0), 46.5, 1e-8);
}

#ifdef HAVE_GEOS

// Test OGRGeometryFactory::transformWithOptions()
TEST_F(test_ogr, transformWithOptions_GEOS)
{
    // Projected CRS to national geographic CRS including antimeridian
    auto [poGeom, err] = OGRGeometryFactory::createFromWkt(
        "LINESTRING(657630.64 4984896.17,815261.43 4990738.26)");
    ASSERT_NE(poGeom, nullptr);

    OGRSpatialReference oEPSG_6329;
    oEPSG_6329.importFromEPSG(6329);  // "NAD83(2011) / UTM zone 60N"
    OGRSpatialReference oEPSG_6318;
    oEPSG_6318.importFromEPSG(6318);  // "NAD83(2011)"
    oEPSG_6318.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
        OGRCreateCoordinateTransformation(&oEPSG_6329, &oEPSG_6318));
    OGRGeometryFactory::TransformWithOptionsCache oCache;
    auto poNewGeom =
        std::unique_ptr<OGRGeometry>(OGRGeometryFactory::transformWithOptions(
            poGeom.get(), poCT.get(), nullptr, oCache));
    ASSERT_NE(poNewGeom, nullptr);
    EXPECT_EQ(poNewGeom->getGeometryType(), wkbMultiLineString);
    if (poNewGeom->getGeometryType() == wkbMultiLineString)
    {
        const auto poMLS = poNewGeom->toMultiLineString();
        EXPECT_EQ(poMLS->getNumGeometries(), 2);
        if (poMLS->getNumGeometries() == 2)
        {
            const auto poLS = poMLS->getGeometryRef(0);
            EXPECT_EQ(poLS->getNumPoints(), 2);
            if (poLS->getNumPoints() == 2)
            {
                EXPECT_NEAR(poLS->getX(0), 179, 1e-6);
                EXPECT_NEAR(poLS->getY(0), 45, 1e-6);
                EXPECT_NEAR(poLS->getX(1), 180, 1e-6);
                EXPECT_NEAR(poLS->getY(1), 45.004384301691303, 1e-6);
            }
        }
    }
}
#endif

// Test OGRCurvePolygon::addRingDirectly
TEST_F(test_ogr, OGRCurvePolygon_addRingDirectly)
{
    OGRCurvePolygon cp;
    OGRGeometry *ring;

    // closed CircularString
    OGRGeometryFactory::createFromWkt(
        "CIRCULARSTRING (0 0, 1 1, 2 0, 1 -1, 0 0)", nullptr, &ring);
    ASSERT_TRUE(ring);
    EXPECT_EQ(cp.addRingDirectly(ring->toCurve()), OGRERR_NONE);

    // open CircularString
    OGRGeometryFactory::createFromWkt("CIRCULARSTRING (0 0, 1 1, 2 0)", nullptr,
                                      &ring);
    ASSERT_TRUE(ring);
    {
        CPLConfigOptionSetter oSetter("OGR_GEOMETRY_ACCEPT_UNCLOSED_RING", "NO",
                                      false);
        ASSERT_EQ(cp.addRingDirectly(ring->toCurve()),
                  OGRERR_UNSUPPORTED_GEOMETRY_TYPE);
    }
    EXPECT_EQ(cp.addRingDirectly(ring->toCurve()), OGRERR_NONE);

    // closed CompoundCurve
    OGRGeometryFactory::createFromWkt(
        "COMPOUNDCURVE( CIRCULARSTRING (0 0, 1 1, 2 0), (2 0, 0 0))", nullptr,
        &ring);
    ASSERT_TRUE(ring);
    EXPECT_EQ(cp.addRingDirectly(ring->toCurve()), OGRERR_NONE);

    // closed LineString
    OGRGeometryFactory::createFromWkt("LINESTRING (0 0, 1 0, 1 1, 0 1, 0 0)",
                                      nullptr, &ring);
    ASSERT_TRUE(ring);
    EXPECT_EQ(cp.addRingDirectly(ring->toCurve()), OGRERR_NONE);

    // LinearRing
    auto lr = std::make_unique<OGRLinearRing>();
    lr->addPoint(0, 0);
    lr->addPoint(1, 0);
    lr->addPoint(1, 1);
    lr->addPoint(0, 1);
    lr->addPoint(0, 0);
    ASSERT_TRUE(ring);
    ASSERT_EQ(cp.addRingDirectly(lr.get()), OGRERR_UNSUPPORTED_GEOMETRY_TYPE);
}

// Test OGRPolygon::addRingDirectly
TEST_F(test_ogr, OGRPolygon_addRingDirectly)
{
    OGRPolygon p;
    OGRGeometry *ring;

    // closed CircularString
    OGRGeometryFactory::createFromWkt(
        "CIRCULARSTRING (0 0, 1 1, 2 0, 1 -1, 0 0)", nullptr, &ring);
    ASSERT_TRUE(ring);
    EXPECT_EQ(p.addRingDirectly(ring->toCurve()),
              OGRERR_UNSUPPORTED_GEOMETRY_TYPE);
    delete ring;

    // closed LineString
    OGRGeometryFactory::createFromWkt("LINESTRING (0 0, 1 0, 1 1, 0 1, 0 0)",
                                      nullptr, &ring);
    ASSERT_TRUE(ring);
    EXPECT_EQ(p.addRingDirectly(ring->toCurve()),
              OGRERR_UNSUPPORTED_GEOMETRY_TYPE);
    delete ring;

    // open LineString
    OGRGeometryFactory::createFromWkt("LINESTRING (0 0, 1 0)", nullptr, &ring);
    ASSERT_TRUE(ring);
    EXPECT_EQ(p.addRingDirectly(ring->toCurve()),
              OGRERR_UNSUPPORTED_GEOMETRY_TYPE);
    delete ring;

    // LinearRing
    auto lr = std::make_unique<OGRLinearRing>();
    lr->addPoint(0, 0);
    lr->addPoint(1, 0);
    lr->addPoint(1, 1);
    lr->addPoint(0, 1);
    lr->addPoint(0, 0);
    ASSERT_EQ(p.addRingDirectly(lr.release()), OGRERR_NONE);
}

TEST_F(test_ogr, OGRFeature_SetGeometry)
{
    OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn();
    poFeatureDefn->Reference();

    OGRFeature oFeat(poFeatureDefn);
    auto [poGeom, err] = OGRGeometryFactory::createFromWkt("POINT (3 7)");
    ASSERT_EQ(err, OGRERR_NONE);

    ASSERT_EQ(oFeat.SetGeometry(std::move(poGeom)), OGRERR_NONE);
    EXPECT_EQ(oFeat.GetGeometryRef()->toPoint()->getX(), 3);
    EXPECT_EQ(oFeat.GetGeometryRef()->toPoint()->getY(), 7);

    // set it again to make sure previous feature geometry is freed
    std::tie(poGeom, err) = OGRGeometryFactory::createFromWkt("POINT (2 8)");
    ASSERT_EQ(err, OGRERR_NONE);
    ASSERT_EQ(oFeat.SetGeometry(std::move(poGeom)), OGRERR_NONE);
    EXPECT_EQ(oFeat.GetGeometryRef()->toPoint()->getX(), 2);
    EXPECT_EQ(oFeat.GetGeometryRef()->toPoint()->getY(), 8);

    poFeatureDefn->Release();
}

TEST_F(test_ogr, OGRFeature_SetGeomField)
{
    OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn();
    poFeatureDefn->Reference();

    OGRGeomFieldDefn oGeomField("second", wkbPoint);
    poFeatureDefn->AddGeomFieldDefn(&oGeomField);

    OGRFeature oFeat(poFeatureDefn);

    // failure
    {
        auto [poGeom, err] = OGRGeometryFactory::createFromWkt("POINT (3 7)");
        ASSERT_EQ(err, OGRERR_NONE);
        EXPECT_EQ(oFeat.SetGeomField(13, std::move(poGeom)), OGRERR_FAILURE);
    }

    // success
    {
        auto [poGeom, err] = OGRGeometryFactory::createFromWkt("POINT (3 7)");
        ASSERT_EQ(err, OGRERR_NONE);
        EXPECT_EQ(oFeat.SetGeomField(1, std::move(poGeom)), OGRERR_NONE);
    }

    poFeatureDefn->Release();
}

TEST_F(test_ogr, GetArrowStream_DateTime_As_String)
{
    auto poDS = std::unique_ptr<GDALDataset>(
        GetGDALDriverManager()->GetDriverByName("MEM")->Create(
            "", 0, 0, 0, GDT_Unknown, nullptr));
    auto poLayer = poDS->CreateLayer("test", nullptr, wkbNone);
    OGRFieldDefn oFieldDefn("dt", OFTDateTime);
    EXPECT_EQ(poLayer->CreateField(&oFieldDefn), OGRERR_NONE);
    struct ArrowArrayStream stream;
    CPLStringList aosOptions;
    aosOptions.SetNameValue("INCLUDE_FID", "NO");
    aosOptions.SetNameValue("DATETIME_AS_STRING", "YES");
    ASSERT_TRUE(poLayer->GetArrowStream(&stream, aosOptions.List()));
    struct ArrowSchema schema;
    memset(&schema, 0, sizeof(schema));
    EXPECT_EQ(stream.get_schema(&stream, &schema), 0);
    EXPECT_TRUE(schema.n_children == 1 &&
                strcmp(schema.children[0]->format, "u") == 0)
        << schema.n_children;
    if (schema.n_children == 1 && strcmp(schema.children[0]->format, "u") == 0)
    {
        EXPECT_TRUE(schema.children[0]->metadata != nullptr);
        if (schema.children[0]->metadata)
        {
            auto oMapKeyValue =
                OGRParseArrowMetadata(schema.children[0]->metadata);
            EXPECT_EQ(oMapKeyValue.size(), 1);
            if (oMapKeyValue.size() == 1)
            {
                EXPECT_STREQ(oMapKeyValue.begin()->first.c_str(),
                             "GDAL:OGR:type");
                EXPECT_STREQ(oMapKeyValue.begin()->second.c_str(), "DateTime");
            }
        }
    }
    schema.release(&schema);
    stream.release(&stream);
}

// Test OGRFeatureDefn::GetFieldSubTypeByName()
TEST_F(test_ogr, OGRFieldDefnGetFieldSubTypeByName)
{
    for (int i = 0; i < OFSTMaxSubType; i++)
    {
        const char *pszName =
            OGRFieldDefn::GetFieldSubTypeName(static_cast<OGRFieldSubType>(i));
        if (pszName != nullptr)
        {
            EXPECT_EQ(OGRFieldDefn::GetFieldSubTypeByName(pszName), i);
        }
    }
}

// Test OGRFeatureDefn::GetFieldTypeByName()
TEST_F(test_ogr, OGRFieldDefnGetFieldTypeByName)
{
    for (int i = 0; i < OFTMaxType; i++)
    {
        // deprecated types
        if (i == OFTWideString || i == OFTWideStringList)
        {
            continue;
        }
        const char *pszName =
            OGRFieldDefn::GetFieldTypeName(static_cast<OGRFieldType>(i));
        if (pszName != nullptr)
        {
            EXPECT_EQ(OGRFieldDefn::GetFieldTypeByName(pszName), i);
        }
    }
}

// Test OGRGeometryFactory::GetDefaultArcStepSize()
TEST_F(test_ogr, GetDefaultArcStepSize)
{
    if (CPLGetConfigOption("OGR_ARC_STEPSIZE", nullptr) == nullptr)
    {
        EXPECT_EQ(OGRGeometryFactory::GetDefaultArcStepSize(), 4.0);
    }
    {
        CPLConfigOptionSetter oSetter("OGR_ARC_STEPSIZE", "0.00001",
                                      /* bSetOnlyIfUndefined = */ false);
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        EXPECT_EQ(OGRGeometryFactory::GetDefaultArcStepSize(), 1e-2);
        EXPECT_TRUE(
            strstr(CPLGetLastErrorMsg(),
                   "Too small value for OGR_ARC_STEPSIZE. Clamping it to"));
    }
    {
        CPLConfigOptionSetter oSetter("OGR_ARC_STEPSIZE", "190",
                                      /* bSetOnlyIfUndefined = */ false);
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        EXPECT_EQ(OGRGeometryFactory::GetDefaultArcStepSize(), 180);
        EXPECT_TRUE(
            strstr(CPLGetLastErrorMsg(),
                   "Too large value for OGR_ARC_STEPSIZE. Clamping it to"));
    }
}

TEST_F(test_ogr, OGRPolygon_two_vertex_constructor)
{
    OGRPolygon p(1, 2, 3, 4);
    char *outWKT = nullptr;
    p.exportToWkt(&outWKT, wkbVariantIso);
    EXPECT_STREQ(outWKT, "POLYGON ((1 2,1 4,3 4,3 2,1 2))");
    CPLFree(outWKT);
}

}  // namespace

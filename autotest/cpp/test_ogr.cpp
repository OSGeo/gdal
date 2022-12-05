///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general OGR features.
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

#include "ogr_p.h"
#include "ogrsf_frmts.h"
#include "../../ogr/ogrsf_frmts/osm/gpb.h"
#include "ogr_recordbatch.h"

#include <string>

#include "gtest_include.h"

namespace
{

    // Common fixture with test data
    struct test_ogr: public ::testing::Test
    {
        std::string drv_shape_{"ESRI Shapefile"};
        std::string data_{tut::common::data_basedir};
        std::string data_tmp_{tut::common::tmp_basedir};
    };

    // Test OGR driver registrar access
    TEST_F(test_ogr, GetGDALDriverManager)
    {
        ASSERT_TRUE(nullptr != GetGDALDriverManager());
    }

    // Test if Shapefile driver is registered
    TEST_F(test_ogr, Shapefile_driver)
    {
        GDALDriver* drv = GetGDALDriverManager()->GetDriverByName(drv_shape_.c_str());
        ASSERT_TRUE(nullptr != drv);
    }

    template<class T>
    void testSpatialReferenceLeakOnCopy(OGRSpatialReference* poSRS)
    {
        ASSERT_EQ(1, poSRS->GetReferenceCount());
        {
            int nCurCount;
            int nLastCount = 1;
            T value;
            value.assignSpatialReference(poSRS);
            nCurCount = poSRS->GetReferenceCount();
            ASSERT_GT(nCurCount, nLastCount );
            nLastCount = nCurCount;

            T value2(value);
            nCurCount = poSRS->GetReferenceCount();
            ASSERT_GT(nCurCount, nLastCount );
            nLastCount = nCurCount;

            T value3;
            value3 = value;
            nCurCount = poSRS->GetReferenceCount();
            ASSERT_GT(nCurCount, nLastCount );
            nLastCount = nCurCount;

            value3 = value;
            ASSERT_EQ( nLastCount, poSRS->GetReferenceCount() );

        }
        ASSERT_EQ( 1, poSRS->GetReferenceCount() );
    }

    // Test if copy does not leak or double delete the spatial reference
    TEST_F(test_ogr, SpatialReference_leak)
    {
        OGRSpatialReference* poSRS = new OGRSpatialReference();
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

    template<class T>
    T* make();

    template<>
    OGRPoint* make()
    {
        return new OGRPoint(1.0, 2.0, 3.0);
    }

    template<>
    OGRLineString* make()
    {
        OGRLineString* poLineString = new OGRLineString();

        poLineString->addPoint(1.0, 2.0, 3.0);
        poLineString->addPoint(1.1, 2.1, 3.1);
        poLineString->addPoint(1.2, 2.2, 3.2);

        return poLineString;
    }

    template<>
    OGRLinearRing* make()
    {
        OGRLinearRing* poLinearRing = new OGRLinearRing();

        poLinearRing->addPoint(1.0, 2.0, 3.0);
        poLinearRing->addPoint(1.1, 2.1, 3.1);
        poLinearRing->addPoint(1.2, 2.2, 3.2);
        poLinearRing->addPoint(1.0, 2.0, 3.0);

        return poLinearRing;
    }

    template<>
    OGRCircularString* make()
    {
        OGRCircularString* poCircularString = new OGRCircularString();

        poCircularString->addPoint(1.0, 2.0, 3.0);
        poCircularString->addPoint(1.1, 2.1, 3.1);
        poCircularString->addPoint(1.2, 2.2, 3.2);

        return poCircularString;
    }

    template<>
    OGRCompoundCurve* make()
    {
        OGRCompoundCurve* poCompoundCurve = new OGRCompoundCurve();

        poCompoundCurve->addCurveDirectly(make<OGRLineString>());
        OGRCircularString* poCircularString = make<OGRCircularString>();
        poCircularString->reversePoints();
        poCompoundCurve->addCurveDirectly(poCircularString);

        return poCompoundCurve;
    }

    template<>
    OGRCurvePolygon* make()
    {
        OGRCurvePolygon* poCurvePolygon = new OGRCurvePolygon();

        poCurvePolygon->addRingDirectly(make<OGRCompoundCurve>());
        poCurvePolygon->addRingDirectly(make<OGRCompoundCurve>());

        return poCurvePolygon;
    }

    template<>
    OGRPolygon* make()
    {
        OGRPolygon* poPolygon = new OGRPolygon();

        poPolygon->addRingDirectly(make<OGRLinearRing>());
        poPolygon->addRingDirectly(make<OGRLinearRing>());

        return poPolygon;
    }

    template<>
    OGRGeometryCollection* make()
    {
        OGRGeometryCollection* poCollection = new OGRGeometryCollection();

        poCollection->addGeometryDirectly(make<OGRPoint>());
        poCollection->addGeometryDirectly(make<OGRLinearRing>());

        return poCollection;
    }

    template<>
    OGRMultiSurface* make()
    {
        OGRMultiSurface* poCollection = new OGRMultiSurface();

        poCollection->addGeometryDirectly(make<OGRPolygon>());
        poCollection->addGeometryDirectly(make<OGRCurvePolygon>());

        return poCollection;
    }

    template<>
    OGRMultiPolygon* make()
    {
        OGRMultiPolygon* poCollection = new OGRMultiPolygon();

        poCollection->addGeometryDirectly(make<OGRPolygon>());

        return poCollection;
    }

    template<>
    OGRMultiPoint* make()
    {
        OGRMultiPoint* poCollection = new OGRMultiPoint();

        poCollection->addGeometryDirectly(make<OGRPoint>());

        return poCollection;
    }

    template<>
    OGRMultiCurve* make()
    {
        OGRMultiCurve* poCollection = new OGRMultiCurve();

        poCollection->addGeometryDirectly(make<OGRLineString>());
        poCollection->addGeometryDirectly(make<OGRCompoundCurve>());

        return poCollection;
    }

    template<>
    OGRMultiLineString* make()
    {
        OGRMultiLineString* poCollection = new OGRMultiLineString();

        poCollection->addGeometryDirectly(make<OGRLineString>());
        poCollection->addGeometryDirectly(make<OGRLinearRing>());

        return poCollection;
    }

    template<>
    OGRTriangle* make()
    {
        OGRPoint p1(0, 0), p2(0, 1), p3(1, 1);
        return new OGRTriangle(p1, p2, p3);
    }

    template<>
    OGRTriangulatedSurface* make()
    {
        OGRTriangulatedSurface* poTS = new OGRTriangulatedSurface();
        poTS->addGeometryDirectly(make<OGRTriangle>());
        return poTS;
    }

    template<>
    OGRPolyhedralSurface* make()
    {
        OGRPolyhedralSurface* poPS = new OGRPolyhedralSurface();
        poPS->addGeometryDirectly(make<OGRPolygon>());
        return poPS;
    }

    template<class T>
    void testCopyEquals()
    {
        T* poOrigin = make<T>();
        ASSERT_TRUE( nullptr != poOrigin);

        T value2( *poOrigin );

        ASSERT_TRUE(CPL_TO_BOOL(poOrigin->Equals(&value2))) << poOrigin->getGeometryName() << ": copy constructor changed a value";

        T value3;
        value3 = *poOrigin;
        value3 = *poOrigin;
        auto& value3Ref(value3);
        value3 = value3Ref;

#ifdef DEBUG_VERBOSE
        char* wkt1 = NULL, *wkt2 = NULL;
        poOrigin->exportToWkt(&wkt1);
        value3.exportToWkt(&wkt2);
        printf("%s %s\n", wkt1, wkt2);
        CPLFree(wkt1);
        CPLFree(wkt2);
#endif
        ASSERT_TRUE(CPL_TO_BOOL(poOrigin->Equals(&value3))) << poOrigin->getGeometryName() << ": assignment operator changed a value";

        OGRGeometryFactory::destroyGeometry(poOrigin);
    }

    // Test if copy constructor and assignment operators succeeds on copying the geometry data
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

    TEST_F(test_ogr, geometry_get_point)
    {
        {
            OGRPoint p;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&p, 1, &x, 0, &y, 0, nullptr, 0 );
            ASSERT_EQ(p.getCoordinateDimension(), 2);
            ASSERT_EQ(p.getX(), 1);
            ASSERT_EQ(p.getY(), 2);
            ASSERT_EQ(p.getZ(), 0);
        }

        {
            OGRPoint p;
            double x = 1, y = 2, z = 3;
            OGR_G_SetPoints( (OGRGeometryH)&p, 1, &x, 0, &y, 0, &z, 0 );
            ASSERT_EQ(p.getCoordinateDimension(), 3);
            ASSERT_EQ(p.getX(), 1);
            ASSERT_EQ(p.getY(), 2);
            ASSERT_EQ(p.getZ(), 3);
        }

        {
            OGRPoint p;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            OGR_G_SetPoints( (OGRGeometryH)&p, 1, nullptr, 0, nullptr, 0, nullptr, 0 );
            CPLPopErrorHandler();
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 0, &y, 0, nullptr, 0 );
            ASSERT_EQ(ls.getCoordinateDimension(), 2);
            ASSERT_EQ(ls.getX(0), 1);
            ASSERT_EQ(ls.getY(0), 2);
            ASSERT_EQ(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 0, &y, 0, nullptr, 0 );
            ASSERT_EQ(ls.getCoordinateDimension(), 2);
            ASSERT_EQ(ls.getX(0), 1);
            ASSERT_EQ(ls.getY(0), 2);
            ASSERT_EQ(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 8, &y, 8, nullptr, 0 );
            ASSERT_EQ(ls.getCoordinateDimension(), 2);
            ASSERT_EQ(ls.getX(0), 1);
            ASSERT_EQ(ls.getY(0), 2);
            ASSERT_EQ(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2, z = 3;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 0, &y, 0, &z, 0 );
            ASSERT_EQ(ls.getCoordinateDimension(), 3);
            ASSERT_EQ(ls.getX(0), 1);
            ASSERT_EQ(ls.getY(0), 2);
            ASSERT_EQ(ls.getZ(0), 3);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2, z = 3;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 8, &y, 8, &z, 8 );
            ASSERT_EQ(ls.getCoordinateDimension(), 3);
            ASSERT_EQ(ls.getX(0), 1);
            ASSERT_EQ(ls.getY(0), 2);
            ASSERT_EQ(ls.getZ(0), 3);
        }

        {
            OGRLineString ls;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, nullptr, 0, nullptr, 0, nullptr, 0 );
            CPLPopErrorHandler();
        }
    }

    TEST_F(test_ogr, style_manager)
    {
      OGRStyleMgrH hSM = OGR_SM_Create(nullptr);
      EXPECT_TRUE(OGR_SM_InitStyleString(hSM, "PEN(w:2px,c:#000000,id:\"mapinfo-pen-2,ogr-pen-0\")"));
      OGRStyleToolH hTool = OGR_SM_GetPart(hSM, 0, nullptr);
      EXPECT_TRUE(hTool != nullptr);
      if( hTool )
      {
          int bValueIsNull;

          EXPECT_NEAR(OGR_ST_GetParamDbl(hTool, OGRSTPenWidth, &bValueIsNull), 2.0 * (1.0 / (72.0 * 39.37)) * 1000, 1e-6);
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
        OGRField sField;
        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:56", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Year, 2017);
        ASSERT_EQ(sField.Date.Month, 11);
        ASSERT_EQ(sField.Date.Day, 31);
        ASSERT_EQ(sField.Date.Hour, 12);
        ASSERT_EQ(sField.Date.Minute, 34);
        ASSERT_EQ(sField.Date.Second, 56.0f);
        ASSERT_EQ(sField.Date.TZFlag, 0);

        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:56+00", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.TZFlag, 100);

        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:56+12:00", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.TZFlag, 100 + 12 * 4);

        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:56+1200", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.TZFlag, 100 + 12 * 4);

        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:56+815", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.TZFlag, 100 + 8 * 4 + 1);

        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:56-12:00", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.TZFlag, 100 - 12 * 4);

        ASSERT_EQ(OGRParseDate(" 2017/11/31 12:34:56", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Year, 2017);

        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:56.789", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Second, 56.789f);

        // Leap second
        ASSERT_EQ(OGRParseDate("2017/11/31 12:34:60", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Second, 60.0f);

        ASSERT_EQ(OGRParseDate("2017-11-31T12:34:56", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Year, 2017);
        ASSERT_EQ(sField.Date.Month, 11);
        ASSERT_EQ(sField.Date.Day, 31);
        ASSERT_EQ(sField.Date.Hour, 12);
        ASSERT_EQ(sField.Date.Minute, 34);
        ASSERT_EQ(sField.Date.Second, 56.0f);
        ASSERT_EQ(sField.Date.TZFlag, 0);

        ASSERT_EQ(OGRParseDate("2017-11-31T12:34:56Z", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Second, 56.0f);
        ASSERT_EQ(sField.Date.TZFlag, 100);

        ASSERT_EQ(OGRParseDate("2017-11-31T12:34:56.789Z", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Second, 56.789f);
        ASSERT_EQ(sField.Date.TZFlag, 100);

        ASSERT_EQ(OGRParseDate("2017-11-31", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Year, 2017);
        ASSERT_EQ(sField.Date.Month, 11);
        ASSERT_EQ(sField.Date.Day, 31);
        ASSERT_EQ(sField.Date.Hour, 0);
        ASSERT_EQ(sField.Date.Minute, 0);
        ASSERT_EQ(sField.Date.Second, 0.0f);
        ASSERT_EQ(sField.Date.TZFlag, 0);

        ASSERT_EQ(OGRParseDate("2017-11-31Z", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Year, 2017);
        ASSERT_EQ(sField.Date.Month, 11);
        ASSERT_EQ(sField.Date.Day, 31);
        ASSERT_EQ(sField.Date.Hour, 0);
        ASSERT_EQ(sField.Date.Minute, 0);
        ASSERT_EQ(sField.Date.Second, 0.0f);
        ASSERT_EQ(sField.Date.TZFlag, 0);

        ASSERT_EQ(OGRParseDate("12:34", &sField, 0), TRUE);
        ASSERT_EQ(sField.Date.Year, 0);
        ASSERT_EQ(sField.Date.Month, 0);
        ASSERT_EQ(sField.Date.Day, 0);
        ASSERT_EQ(sField.Date.Hour, 12);
        ASSERT_EQ(sField.Date.Minute, 34);
        ASSERT_EQ(sField.Date.Second, 0.0f);
        ASSERT_EQ(sField.Date.TZFlag, 0);

        ASSERT_EQ(OGRParseDate("12:34:56", &sField, 0), TRUE);
        ASSERT_EQ(OGRParseDate("12:34:56.789", &sField, 0), TRUE);

        ASSERT_TRUE(!OGRParseDate("2017", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("12:", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-a-31T12:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-00-31T12:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-13-31T12:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-00T12:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-aT12:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-32T12:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("a:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-01Ta:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-01T25:34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-01T00:a:00", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-01T00: 34:56", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-01T00:61:00", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-01T00:00:61", &sField, 0));
        ASSERT_TRUE(!OGRParseDate("2017-01-01T00:00:a", &sField, 0));
    }

    // Test OGRPolygon::IsPointOnSurface()
    TEST_F(test_ogr, IsPointOnSurface)
    {
        OGRPolygon oPoly;

        OGRPoint oEmptyPoint;
        ASSERT_TRUE( !oPoly.IsPointOnSurface(&oEmptyPoint) );

        OGRPoint oPoint;
        oPoint.setX(1);
        oPoint.setY(1);
        ASSERT_TRUE( !oPoly.IsPointOnSurface(&oPoint) );

        const char* pszPoly = "POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))";
        oPoly.importFromWkt(&pszPoly);

        ASSERT_TRUE( !oPoly.IsPointOnSurface(&oEmptyPoint) );

        ASSERT_EQ( oPoly.IsPointOnSurface(&oPoint), TRUE );

        oPoint.setX(5);
        oPoint.setY(5);
        ASSERT_TRUE( !oPoly.IsPointOnSurface(&oPoint) );
    }

    // Test gpb.h
    TEST_F(test_ogr, gpb_h)
    {
        ASSERT_EQ( GetVarUIntSize(0), 1 );
        ASSERT_EQ( GetVarUIntSize(127), 1 );
        ASSERT_EQ( GetVarUIntSize(128), 2 );
        ASSERT_EQ( GetVarUIntSize((1 << 14) - 1), 2 );
        ASSERT_EQ( GetVarUIntSize(1 << 14), 3 );
        ASSERT_EQ( GetVarUIntSize(GUINT64_MAX), 10 );

        ASSERT_EQ( GetVarIntSize(0), 1 );
        ASSERT_EQ( GetVarIntSize(127), 1 );
        ASSERT_EQ( GetVarIntSize(128), 2 );
        ASSERT_EQ( GetVarIntSize((1 << 14) - 1), 2 );
        ASSERT_EQ( GetVarIntSize(1 << 14), 3 );
        ASSERT_EQ( GetVarIntSize(GINT64_MAX), 9 );
        ASSERT_EQ( GetVarIntSize(-1), 10 );
        ASSERT_EQ( GetVarIntSize(GINT64_MIN), 10 );

        ASSERT_EQ( GetVarSIntSize(0), 1 );
        ASSERT_EQ( GetVarSIntSize(63), 1 );
        ASSERT_EQ( GetVarSIntSize(64), 2 );
        ASSERT_EQ( GetVarSIntSize(-1), 1 );
        ASSERT_EQ( GetVarSIntSize(-64), 1 );
        ASSERT_EQ( GetVarSIntSize(-65), 2 );
        ASSERT_EQ( GetVarSIntSize(GINT64_MIN), 10 );
        ASSERT_EQ( GetVarSIntSize(GINT64_MAX), 10 );

        ASSERT_EQ( GetTextSize(""), 1 );
        ASSERT_EQ( GetTextSize(" "), 2 );
        ASSERT_EQ( GetTextSize(std::string(" ")), 2 );

        GByte abyBuffer[11] = { 0 };
        GByte* pabyBuffer;
        const GByte* pabyBufferRO;

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
        #define CONCAT(X,Y) X##Y
        #define TEST_OGRGEOMETRY_TO(X) { \
            CONCAT(OGR,X) o; \
            OGRGeometry* poGeom = &o; \
            ASSERT_EQ( poGeom->CONCAT(to,X)(), &o ); }

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
            OGRGeometry* poGeom = &o;
            ASSERT_EQ( poGeom->toCurve(), &o );
        }
        {
            OGRPolygon o;
            OGRGeometry* poGeom = &o;
            ASSERT_EQ( poGeom->toSurface(), &o );
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
                OGRCurve& oRef = o;
                ASSERT_EQ(oRef.toLineString(), &o);
            }

            {
                OGRSimpleCurve& oRef = o;
                ASSERT_EQ(oRef.toLineString(), &o);
            }
        }

        {
            OGRLinearRing o;
            ASSERT_EQ(o.toCurve(), &o);
            ASSERT_EQ(o.toSimpleCurve(), &o);
            // ASSERT_EQ(o.toLinearRing(), &o);

            {
                OGRCurve& oRef = o;
                ASSERT_EQ(oRef.toLinearRing(), &o);
            }
            {
                OGRSimpleCurve& oRef = o;
                ASSERT_EQ(oRef.toLinearRing(), &o);
            }
            {
                OGRLineString& oRef = o;
                ASSERT_EQ(oRef.toLinearRing(), &o);
            }
        }

        {
            OGRCircularString o;
            ASSERT_EQ(o.toCurve(), &o);
            ASSERT_EQ(o.toSimpleCurve(), &o);
            // ASSERT_EQ(o.toCircularString(), &o);

            {
                OGRCurve& oRef = o;
                ASSERT_EQ(oRef.toCircularString(), &o);
            }

            {
                OGRSimpleCurve& oRef = o;
                ASSERT_EQ(oRef.toCircularString(), &o);
            }
        }

        {
            OGRCompoundCurve o;
            ASSERT_EQ(o.toCurve(), &o);
            // ASSERT_EQ(o.toCompoundCurve(), &o);

            {
                OGRCurve& oRef = o;
                ASSERT_EQ(oRef.toCompoundCurve(), &o);
            }
        }

        {
            OGRCurvePolygon o;
            ASSERT_EQ(o.toSurface(), &o);
            // ASSERT_EQ(o.toCurvePolygon(), &o);

            {
                OGRSurface& oRef = o;
                ASSERT_EQ(oRef.toCurvePolygon(), &o);
            }
        }

        {
            OGRPolygon o;
            ASSERT_EQ(o.toSurface(), &o);
            ASSERT_EQ(o.toCurvePolygon(), &o);
            // ASSERT_EQ(o.toPolygon(), &o);

            {
                OGRSurface& oRef = o;
                ASSERT_EQ(oRef.toPolygon(), &o);
            }

            {
                OGRCurvePolygon& oRef = o;
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
                OGRSurface& oRef = o;
                ASSERT_EQ(oRef.toTriangle(), &o);
            }

            {
                OGRCurvePolygon& oRef = o;
                ASSERT_EQ(oRef.toTriangle(), &o);
            }

            {
                OGRPolygon& oRef = o;
                ASSERT_EQ(oRef.toTriangle(), &o);
            }
        }

        {
            OGRMultiPoint o;
            ASSERT_EQ(o.toGeometryCollection(), &o);
            // ASSERT_EQ(o.toMultiPoint(), &o);

            {
                OGRGeometryCollection& oRef = o;
                ASSERT_EQ(oRef.toMultiPoint(), &o);
            }
        }

        {
            OGRMultiCurve o;
            ASSERT_EQ(o.toGeometryCollection(), &o);
            // ASSERT_EQ(o.toMultiCurve(), &o);

            {
                OGRGeometryCollection& oRef = o;
                ASSERT_EQ(oRef.toMultiCurve(), &o);
            }
        }

        {
            OGRMultiLineString o;
            ASSERT_EQ(o.toGeometryCollection(), &o);
            ASSERT_EQ(o.toMultiCurve(), &o);
            // ASSERT_EQ(o.toMultiLineString(), &o);

            {
                OGRMultiCurve& oRef = o;
                ASSERT_EQ(oRef.toMultiLineString(), &o);
            }

            {
                OGRGeometryCollection& oRef = o;
                ASSERT_EQ(oRef.toMultiLineString(), &o);
            }
        }

        {
            OGRMultiSurface o;
            ASSERT_EQ(o.toGeometryCollection(), &o);
            // ASSERT_EQ(o.toMultiSurface(), &o);

            {
                OGRGeometryCollection& oRef = o;
                ASSERT_EQ(oRef.toMultiSurface(), &o);
            }
        }

        {
            OGRMultiPolygon o;
            ASSERT_EQ(o.toGeometryCollection(), &o);
            ASSERT_EQ(o.toMultiSurface(), &o);
            // ASSERT_EQ(o.toMultiPolygon(), &o);

            {
                OGRMultiSurface& oRef = o;
                ASSERT_EQ(oRef.toMultiPolygon(), &o);
            }

            {
                OGRGeometryCollection& oRef = o;
                ASSERT_EQ(oRef.toMultiPolygon(), &o);
            }
        }

        {
            OGRPolyhedralSurface o;
            ASSERT_EQ(o.toSurface(), &o);
            // ASSERT_EQ(o.toPolyhedralSurface(), &o);

            {
                OGRSurface& oRef = o;
                ASSERT_EQ(oRef.toPolyhedralSurface(), &o);
            }
        }

        {
            OGRTriangulatedSurface o;
            ASSERT_EQ(o.toSurface(), &o);
            ASSERT_EQ(o.toPolyhedralSurface(), &o);
            // ASSERT_EQ(o.toTriangulatedSurface(), &o);

            {
                OGRSurface& oRef = o;
                ASSERT_EQ(oRef.toTriangulatedSurface(), &o);
            }

            {
                OGRPolyhedralSurface& oRef = o;
                ASSERT_EQ(oRef.toTriangulatedSurface(), &o);
            }
        }

    }

    template<typename T> void TestIterator(T* obj,
                                           int nExpectedPointCount)
    {
        int nCount = 0;
        for( auto& elt: obj )
        {
            nCount ++;
            CPL_IGNORE_RET_VAL(elt);
        }
        ASSERT_EQ(nCount, nExpectedPointCount);

        nCount = 0;
        const T* const_obj(obj);
        for( const auto& elt: const_obj)
        {
            nCount ++;
            CPL_IGNORE_RET_VAL(elt);
        }
        ASSERT_EQ(nCount, nExpectedPointCount);
    }

    template<typename Concrete, typename Abstract = Concrete> void TestIterator(
                                           const char* pszWKT = nullptr,
                                           int nExpectedPointCount = 0)
    {
        Concrete obj;
        if( pszWKT )
        {
            obj.importFromWkt(&pszWKT);
        }
        TestIterator<Abstract>(&obj, nExpectedPointCount);
    }

    // Test geometry visitor
    TEST_F(test_ogr, OGRGeometry_visitor)
    {
        static const struct {
            const char* pszWKT;
            int nExpectedPointCount;
        } asTests[] = {
            { "POINT(0 0)", 1},
            { "LINESTRING(0 0)", 1},
            { "POLYGON((0 0),(0 0))", 2},
            { "MULTIPOINT(0 0)", 1},
            { "MULTILINESTRING((0 0))", 1},
            { "MULTIPOLYGON(((0 0)))", 1},
            { "GEOMETRYCOLLECTION(POINT(0 0))", 1},
            { "CIRCULARSTRING(0 0,1 1,0 0)", 3},
            { "COMPOUNDCURVE((0 0,1 1))", 2},
            { "CURVEPOLYGON((0 0,1 1,1 0,0 0))", 4},
            { "MULTICURVE((0 0))", 1},
            { "MULTISURFACE(((0 0)))", 1},
            { "TRIANGLE((0 0,0 1,1 1,0 0))", 4},
            { "POLYHEDRALSURFACE(((0 0,0 1,1 1,0 0)))", 4},
            { "TIN(((0 0,0 1,1 1,0 0)))", 4},
        };

        class PointCounterVisitor: public OGRDefaultGeometryVisitor
        {
                int m_nPoints = 0;

            public:
                PointCounterVisitor() {}

                using OGRDefaultGeometryVisitor::visit;

                void visit(OGRPoint*) override
                {
                    m_nPoints++;
                }

                int getNumPoints() const { return m_nPoints; }
        };


        class PointCounterConstVisitor: public OGRDefaultConstGeometryVisitor
        {
                int m_nPoints = 0;

            public:
                PointCounterConstVisitor() {}

                using OGRDefaultConstGeometryVisitor::visit;

                void visit(const OGRPoint*) override
                {
                    m_nPoints++;
                }

                int getNumPoints() const { return m_nPoints; }
        };

        for( size_t i = 0; i < CPL_ARRAYSIZE(asTests); i++ )
        {
            OGRGeometry* poGeom = nullptr;
            OGRGeometryFactory::createFromWkt(asTests[i].pszWKT, nullptr, &poGeom);
            PointCounterVisitor oVisitor;
            poGeom->accept(&oVisitor);
            ASSERT_EQ(oVisitor.getNumPoints(), asTests[i].nExpectedPointCount);
            PointCounterConstVisitor oConstVisitor;
            poGeom->accept(&oConstVisitor);
            ASSERT_EQ(oConstVisitor.getNumPoints(), asTests[i].nExpectedPointCount);
            delete poGeom;
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
        TestIterator<OGRCompoundCurve, OGRCurve>("COMPOUNDCURVE((0 0,1 1),CIRCULARSTRING(1 1,2 2,3 3))", 4);
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
        TestIterator<OGRPolyhedralSurface>("POLYHEDRALSURFACE(((0 0,0 1,1 1,0 0)))", 1);
        TestIterator<OGRTriangulatedSurface>();
        TestIterator<OGRTriangulatedSurface>("TIN(((0 0,0 1,1 1,0 0)))", 1);

        // Test that the update of the iterated point of a linestring is
        // immediately taken into account (https://github.com/OSGeo/gdal/issues/6215)
        {
            OGRLineString oLS;
            oLS.addPoint(1, 2);
            oLS.addPoint(3, 4);
            int i = 0;
            for( auto&& p: oLS )
            {
                p.setX(i * 10);
                p.setY(i * 10 + 1);
                p.setZ(i * 10 + 2);
                p.setM(i * 10 + 3);
                ASSERT_EQ( oLS.getX(i), p.getX() );
                ASSERT_EQ( oLS.getY(i), p.getY() );
                ASSERT_EQ( oLS.getZ(i), p.getZ() );
                ASSERT_EQ( oLS.getM(i), p.getM() );
                ++i;
            }
        }

        {
            class PointCounterVisitorAndUpdate: public OGRDefaultGeometryVisitor
            {
                public:
                    PointCounterVisitorAndUpdate() = default;

                    using OGRDefaultGeometryVisitor::visit;

                    void visit(OGRPoint* poPoint) override
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

            ASSERT_EQ( oLS.getZ(0), 100.0 );
            ASSERT_EQ( oLS.getZ(1), 100.0 );
            ASSERT_EQ( oLS.getM(0), 1000.0 );
            ASSERT_EQ( oLS.getM(1), 1000.0 );
        }
    }

    // Test OGRToOGCGeomType()
    TEST_F(test_ogr, OGRToOGCGeomType)
    {
        EXPECT_STREQ(OGRToOGCGeomType(wkbPoint), "POINT");
        EXPECT_STREQ(OGRToOGCGeomType(wkbPointM), "POINT");
        EXPECT_STREQ(OGRToOGCGeomType(wkbPoint, /*bCamelCase=*/ true), "Point");
        EXPECT_STREQ(OGRToOGCGeomType(wkbPoint, /*bCamelCase=*/ true, /*bAddZM=*/true), "Point");
        EXPECT_STREQ(OGRToOGCGeomType(wkbPoint25D, /*bCamelCase=*/ true, /*bAddZM=*/true), "PointZ");
        EXPECT_STREQ(OGRToOGCGeomType(wkbPointM, /*bCamelCase=*/ true, /*bAddZM=*/true), "PointM");
        EXPECT_STREQ(OGRToOGCGeomType(wkbPointZM, /*bCamelCase=*/ true, /*bAddZM=*/true), "PointZM");
        EXPECT_STREQ(OGRToOGCGeomType(wkbPointZM, /*bCamelCase=*/ true, /*bAddZM=*/true, /*bAddSpaceBeforeZM=*/true), "Point ZM");
    }

    // Test layer, dataset-feature and layer-feature iterators
    TEST_F(test_ogr, DatasetFeature_and_LayerFeature_iterators)
    {
        std::string file(data_ + SEP + "poly.shp");
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(file.c_str(), GDAL_OF_VECTOR));
        ASSERT_TRUE( poDS != nullptr );

        {
            GIntBig nExpectedFID = 0;
            for( const auto& oFeatureLayerPair: poDS->GetFeatures() )
            {
                ASSERT_EQ( oFeatureLayerPair.feature->GetFID(), nExpectedFID );
                nExpectedFID ++;
                ASSERT_EQ( oFeatureLayerPair.layer, poDS->GetLayer(0) );
            }
            ASSERT_EQ(nExpectedFID, 10);
        }

        ASSERT_EQ( poDS->GetLayers().size(), 1U );
        ASSERT_EQ( poDS->GetLayers()[0], poDS->GetLayer(0) );
        ASSERT_EQ( poDS->GetLayers()[static_cast<size_t>(0)], poDS->GetLayer(0) );
        ASSERT_EQ( poDS->GetLayers()["poly"], poDS->GetLayer(0) );

        for( auto poLayer: poDS->GetLayers() )
        {
            GIntBig nExpectedFID = 0;
            for( const auto& poFeature: poLayer )
            {
                ASSERT_EQ( poFeature->GetFID(), nExpectedFID );
                nExpectedFID ++;
            }
            ASSERT_EQ(nExpectedFID, 10);

            nExpectedFID = 0;
            for(const  auto& oFeatureLayerPair: poDS->GetFeatures() )
            {
                ASSERT_EQ( oFeatureLayerPair.feature->GetFID(), nExpectedFID );
                nExpectedFID ++;
                ASSERT_EQ( oFeatureLayerPair.layer, poLayer );
            }
            ASSERT_EQ(nExpectedFID, 10);

            nExpectedFID = 0;
            OGR_FOR_EACH_FEATURE_BEGIN(hFeat, reinterpret_cast<OGRLayerH>(poLayer))
            {
                if( nExpectedFID == 0 )
                {
                    nExpectedFID = 1;
                    continue;
                }
                ASSERT_EQ( OGR_F_GetFID(hFeat), nExpectedFID );
                nExpectedFID ++;
                if( nExpectedFID == 5 )
                    break;
            }
            OGR_FOR_EACH_FEATURE_END(hFeat)
            ASSERT_EQ(nExpectedFID, 5);

            auto oIter = poLayer->begin();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            // Only one feature iterator can be active at a time
            auto oIter2 = poLayer->begin();
            CPLPopErrorHandler();
            ASSERT_TRUE( !(oIter2 != poLayer->end()) );
            ASSERT_TRUE( oIter != poLayer->end() );
        }

        poDS.reset(GetGDALDriverManager()->GetDriverByName("Memory")->
            Create("", 0, 0, 0, GDT_Unknown, nullptr));
        int nCountLayers = 0;
        for( auto poLayer: poDS->GetLayers() )
        {
            CPL_IGNORE_RET_VAL(poLayer);
            nCountLayers++;
        }
        ASSERT_EQ(nCountLayers, 0);

        poDS->CreateLayer("foo");
        poDS->CreateLayer("bar");
        for( auto poLayer: poDS->GetLayers() )
        {
            if( nCountLayers == 0 )
            {
                EXPECT_STREQ( poLayer->GetName(), "foo" ) << "layer " << poLayer->GetName();
            }
            else if( nCountLayers == 1 )
            {
                EXPECT_STREQ( poLayer->GetName(), "bar" ) << "layer " << poLayer->GetName();
            }
            nCountLayers++;
        }
        ASSERT_EQ(nCountLayers, 2);

        // std::copy requires a InputIterator
        std::vector<OGRLayer*> oTarget;
        oTarget.resize(2);
        auto layers = poDS->GetLayers();
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

        // Test copy constructor
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter(srcIter);
            ASSERT_EQ(*newIter, layers[1]);
        }

        // Test assignment operator
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter;
            newIter = srcIter;
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

    }

    // Test field iterator
    TEST_F(test_ogr, field_iterator)
    {
        OGRFeatureDefn* poFeatureDefn = new OGRFeatureDefn();
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
            ASSERT_STREQ( oFeatureTmp[0].GetString(), "bar" );
            {
                // Proxy reference
                auto&& x= oFeatureTmp[0];
                auto& xRef(x);
                x = xRef;
                ASSERT_STREQ( oFeatureTmp[0].GetString(), "bar" );
            }
            {
                oFeatureTmp[0] = oFeatureTmp[0];
                ASSERT_STREQ( oFeatureTmp[0].GetString(), "bar" );
            }
            {
                // Proxy reference
                auto&& x= oFeatureTmp[0];
                x = "baz";
                ASSERT_STREQ( x.GetString(), "baz" );
            }
            oFeatureTmp["str_field"] = std::string("foo");
            oFeatureTmp["int_field"] = 123;
            oFeatureTmp["int64_field"] = oFeatureTmp["int_field"];
            ASSERT_EQ( oFeatureTmp["int64_field"].GetInteger(), 123 );
            oFeatureTmp["int64_field"] = static_cast<GIntBig>(1234567890123);
            oFeatureTmp["double_field"] = 123.45;
            oFeatureTmp["null_field"].SetNull();
            oFeatureTmp["unset_field"].clear();
            oFeatureTmp["unset_field"].Unset();
            oFeatureTmp["dt_field"].SetDateTime(2018, 4, 5, 12, 34, 56.75f, 0);
            oFeatureTmp["strlist_field"] = CPLStringList().List();
            oFeatureTmp["strlist_field"] = std::vector<std::string>();
            oFeatureTmp["strlist_field"] = std::vector<std::string>{ "foo", "bar" };
            oFeatureTmp["strlist_field"] = static_cast<CSLConstList>(oFeatureTmp["strlist_field"]);
            ASSERT_EQ( CSLCount(static_cast<CSLConstList>(oFeatureTmp["strlist_field"])), 2 );
            oFeatureTmp["intlist_field"] = std::vector<int>();
            oFeatureTmp["intlist_field"] = std::vector<int>{ 12, 34 };
            oFeatureTmp["int64list_field"] = std::vector<GIntBig>();
            oFeatureTmp["int64list_field"] = std::vector<GIntBig>{ 1234567890123,34 };
            oFeatureTmp["doublelist_field"] = std::vector<double>();
            oFeatureTmp["doublelist_field"] = std::vector<double>{ 12.25,56.75 };

            for( const auto& oField: oFeatureTmp )
            {
                oFeature[oField.GetIndex()] = oField;
            }
        }

        {
            int x = oFeature[1];
            ASSERT_EQ( x, 123 );
        }
        {
            int x = oFeature["int_field"];
            ASSERT_EQ( x, 123 );
        }
        {
            GIntBig x = oFeature["int64_field"];
            ASSERT_EQ( x, static_cast<GIntBig>(1234567890123) );
        }
        {
            double x = oFeature["double_field"];
            ASSERT_EQ( x, 123.45 );
        }
        {
            const char* x = oFeature["str_field"];
            ASSERT_STREQ( x, "foo" );
        }
        bool bExceptionHit = false;
        try
        {
            oFeature["inexisting_field"];
        }
        catch( const OGRFeature::FieldNotFoundException& )
        {
            bExceptionHit = true;
        }
        ASSERT_TRUE(bExceptionHit);

        int iIter = 0;
        const OGRFeature* poConstFeature = &oFeature;
        for( const auto& oField: *poConstFeature )
        {
            ASSERT_EQ( oField.GetIndex(), iIter );
            ASSERT_EQ( oField.GetDefn(), poFeatureDefn->GetFieldDefn(iIter) );
            ASSERT_EQ( CPLString(oField.GetName()), CPLString(oField.GetDefn()->GetNameRef()) );
            ASSERT_EQ( oField.GetType(), oField.GetDefn()->GetType() );
            ASSERT_EQ( oField.GetSubType(), oField.GetDefn()->GetSubType() );
            if( iIter == 0 )
            {
                ASSERT_EQ( oField.IsUnset(), false );
                ASSERT_EQ( oField.IsNull(), false );
                ASSERT_EQ( CPLString(oField.GetRawValue()->String), CPLString("foo") );
                ASSERT_EQ( CPLString(oField.GetString()), CPLString("foo") );
                ASSERT_EQ( CPLString(oField.GetAsString()), CPLString("foo") );
            }
            else if( iIter == 1 )
            {
                ASSERT_EQ( oField.GetRawValue()->Integer, 123 );
                ASSERT_EQ( oField.GetInteger(), 123 );
                ASSERT_EQ( oField.GetAsInteger(), 123 );
                ASSERT_EQ( oField.GetAsInteger64(), 123 );
                ASSERT_EQ( oField.GetAsDouble(), 123.0 );
                ASSERT_EQ( CPLString(oField.GetAsString()), CPLString("123") );
            }
            else if( iIter == 2 )
            {
                ASSERT_EQ( oField.GetRawValue()->Integer64, 1234567890123 );
                ASSERT_EQ( oField.GetInteger64(), 1234567890123 );
                ASSERT_EQ( oField.GetAsInteger(), 2147483647 );
                ASSERT_EQ( oField.GetAsInteger64(), 1234567890123 );
                ASSERT_EQ( oField.GetAsDouble(), 1234567890123.0 );
                ASSERT_EQ( CPLString(oField.GetAsString()), CPLString("1234567890123") );
            }
            else if( iIter == 3 )
            {
                ASSERT_EQ( oField.GetRawValue()->Real, 123.45 );
                ASSERT_EQ( oField.GetDouble(), 123.45 );
                ASSERT_EQ( oField.GetAsInteger(), 123 );
                ASSERT_EQ( oField.GetAsInteger64(), 123 );
                ASSERT_EQ( oField.GetAsDouble(), 123.45 );
                ASSERT_EQ( CPLString(oField.GetAsString()), CPLString("123.45") );
            }
            else if( iIter == 4 )
            {
                ASSERT_EQ( oField.IsUnset(), false );
                ASSERT_EQ( oField.IsNull(), true );
            }
            else if( iIter == 5 )
            {
                ASSERT_EQ( oField.IsUnset(), true );
                ASSERT_EQ( oField.empty(), true );
                ASSERT_EQ( oField.IsNull(), false );
            }
            else if( iIter == 6 )
            {
                int nYear, nMonth, nDay, nHour, nMin, nTZFlag;
                float fSec;
                ASSERT_EQ( oField.GetDateTime(&nYear, &nMonth, &nDay,
                                                  &nHour, &nMin, &fSec,
                                                  &nTZFlag), true );
                ASSERT_EQ( nYear, 2018 );
                ASSERT_EQ( nMonth, 4 );
                ASSERT_EQ( nDay, 5 );
                ASSERT_EQ( nHour, 12 );
                ASSERT_EQ( nMin, 34 );
                ASSERT_EQ( fSec, 56.75f );
                ASSERT_EQ( nTZFlag, 0 );
            }
            else if( iIter == 7 )
            {
                std::vector<std::string> oExpected{ std::string("foo"),
                                                    std::string("bar") };
                decltype(oExpected) oGot = oField;
                ASSERT_EQ( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ASSERT_EQ( oGot[i], oExpected[i] );
            }
            else if( iIter == 8 )
            {
                std::vector<int> oExpected{ 12, 34 };
                decltype(oExpected) oGot = oField;
                ASSERT_EQ( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ASSERT_EQ( oGot[i], oExpected[i] );
            }
            else if( iIter == 9 )
            {
                std::vector<GIntBig> oExpected{ 1234567890123, 34 };
                decltype(oExpected) oGot = oField;
                ASSERT_EQ( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ASSERT_EQ( oGot[i], oExpected[i] );
            }
            else if( iIter == 10 )
            {
                std::vector<double> oExpected{ 12.25, 56.75 };
                decltype(oExpected) oGot = oField;
                ASSERT_EQ( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ASSERT_EQ( oGot[i], oExpected[i] );
            }
            iIter ++;

        }
        poFeatureDefn->Release();
    }

    // Test OGRLinearRing::isPointOnRingBoundary()
    TEST_F(test_ogr, isPointOnRingBoundary)
    {
        OGRPolygon oPoly;
        const char* pszPoly = "POLYGON((10 9,11 10,10 11,9 10,10 9))";
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
            OGRPoint p(10-1, 9-1);
            ASSERT_TRUE(!poRing->isPointOnRingBoundary(&p, false));
        }

        // "After" first segment
        {
            OGRPoint p(11+1, 10+1);
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
        char* pszWKT = nullptr;
        OGRPoint p(1, 2);
        p.exportToWkt(&pszWKT);
        ASSERT_TRUE(pszWKT != nullptr);
        EXPECT_STREQ(pszWKT, "POINT (1 2)");
        CPLFree(pszWKT);
    }

    // Test OGRGeometry::clone()
    TEST_F(test_ogr, OGRGeometry_clone)
    {
        const char* apszWKT[] =
        {
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
        for( const char* pszWKT: apszWKT )
        {
            OGRGeometry* poGeom = nullptr;
            OGRGeometryFactory::createFromWkt(pszWKT, &oSRS, &poGeom);
            auto poClone = poGeom->clone();
            ASSERT_TRUE(poClone != nullptr);
            char* outWKT = nullptr;
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
            ls.addPoint(0,1);
            ls.addPoint(2,3);
            ls.addPoint(4,5);
            ASSERT_TRUE( !ls.removePoint(-1) );
            ASSERT_TRUE( !ls.removePoint(3) );
            ASSERT_EQ( ls.getNumPoints(), 3 );
            ASSERT_TRUE( ls.removePoint(1) );
            ASSERT_EQ( ls.getNumPoints(), 2 );
            ASSERT_EQ( ls.getX(0), 0.0 );
            ASSERT_EQ( ls.getY(0), 1.0 );
            ASSERT_EQ( ls.getX(1), 4.0 );
            ASSERT_EQ( ls.getY(1), 5.0 );
            ASSERT_TRUE( ls.removePoint(1) );
            ASSERT_EQ( ls.getNumPoints(), 1 );
            ASSERT_TRUE( ls.removePoint(0) );
            ASSERT_EQ( ls.getNumPoints(), 0 );
        }
        {
            // With Z, M
            OGRLineString ls;
            ls.addPoint(0,1,20,30);
            ls.addPoint(2,3,40,50);
            ls.addPoint(4,5,60,70);
            ASSERT_TRUE( !ls.removePoint(-1) );
            ASSERT_TRUE( !ls.removePoint(3) );
            ASSERT_EQ( ls.getNumPoints(), 3 );
            ASSERT_TRUE( ls.removePoint(1) );
            ASSERT_EQ( ls.getNumPoints(), 2 );
            ASSERT_EQ( ls.getX(0), 0.0 );
            ASSERT_EQ( ls.getY(0), 1.0 );
            ASSERT_EQ( ls.getZ(0), 20.0 );
            ASSERT_EQ( ls.getM(0), 30.0 );
            ASSERT_EQ( ls.getX(1), 4.0 );
            ASSERT_EQ( ls.getY(1), 5.0 );
            ASSERT_EQ( ls.getZ(1), 60.0 );
            ASSERT_EQ( ls.getM(1), 70.0 );
            ASSERT_TRUE( ls.removePoint(1) );
            ASSERT_EQ( ls.getNumPoints(), 1 );
            ASSERT_TRUE( ls.removePoint(0) );
            ASSERT_EQ( ls.getNumPoints(), 0 );
        }
    }


    // Test effect of MarkSuppressOnClose() on DXF
    TEST_F(test_ogr, DXF_MarkSuppressOnClose)
    {
        CPLString tmpFilename(CPLGenerateTempFilename(nullptr));
        tmpFilename += ".dxf";
        auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("DXF"));
        if( poDrv )
        {
            auto poDS(GDALDatasetUniquePtr(poDrv->Create(
                tmpFilename, 0, 0, 0, GDT_Unknown, nullptr )));
            ASSERT_TRUE( poDS != nullptr );

            OGRLayer *poLayer = poDS->CreateLayer("test", nullptr, wkbPoint, nullptr);
            ASSERT_TRUE ( poLayer != nullptr );

            for (double x = 0; x < 100; x++)
            {
                OGRFeature *poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
                ASSERT_TRUE ( poFeature != nullptr );
                OGRPoint pt(x, 42);
                ASSERT_EQ( OGRERR_NONE, poFeature->SetGeometry(&pt) );
                ASSERT_EQ( OGRERR_NONE, poLayer->CreateFeature(poFeature) );
                OGRFeature::DestroyFeature( poFeature );
            }

            poDS->MarkSuppressOnClose();

            poDS.reset();
            VSIStatBufL sStat;
            ASSERT_TRUE( 0 != VSIStatL(tmpFilename, &sStat) );
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
            GetGDALDriverManager()->GetDriverByName("Memory")->
                Create("", 0, 0, 0, GDT_Unknown, nullptr));
        auto poLayer = poDS->CreateLayer("test");
        {
            OGRFieldDefn oFieldDefn("str", OFTString);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("bool", OFTInteger);
            oFieldDefn.SetSubType(OFSTBoolean);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int16", OFTInteger);
            oFieldDefn.SetSubType(OFSTInt16);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int32", OFTInteger);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int64", OFTInteger64);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("float32", OFTReal);
            oFieldDefn.SetSubType(OFSTFloat32);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("float64", OFTReal);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("date", OFTDate);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("time", OFTTime);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("datetime", OFTDateTime);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("binary", OFTBinary);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("strlist", OFTStringList);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("boollist", OFTIntegerList);
            oFieldDefn.SetSubType(OFSTBoolean);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int16list", OFTIntegerList);
            oFieldDefn.SetSubType(OFSTInt16);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int32list", OFTIntegerList);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("int64list", OFTInteger64List);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("float32list", OFTRealList);
            oFieldDefn.SetSubType(OFSTFloat32);
            poLayer->CreateField(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn("float64list", OFTRealList);
            poLayer->CreateField(&oFieldDefn);
        }
        auto poFDefn = poLayer->GetLayerDefn();
        struct ArrowArrayStream stream;
        ASSERT_TRUE(OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream, nullptr));
        {
            // Cannot start a new stream while one is active
            struct ArrowArrayStream stream2;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            ASSERT_TRUE(OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream2, nullptr) == false);
            CPLPopErrorHandler();
        }
        ASSERT_TRUE(stream.release != nullptr);

        struct ArrowSchema schema;
        CPLErrorReset();
        ASSERT_TRUE(stream.get_last_error(&stream) == nullptr);
        ASSERT_EQ(stream.get_schema(&stream, &schema), 0);
        ASSERT_TRUE(stream.get_last_error(&stream) == nullptr);
        ASSERT_TRUE(schema.release != nullptr);
        ASSERT_EQ(schema.n_children, 1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
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
            poFeature->SetField( poFDefn->GetFieldIndex("binary"), 2, "\xDE\xAD");
            OGRGeometry* poGeom = nullptr;
            OGRGeometryFactory::createFromWkt( "POINT(1 2)", nullptr, &poGeom);
            poFeature->SetGeometryDirectly(poGeom);
            ASSERT_EQ(poLayer->CreateFeature(poFeature.get()), OGRERR_NONE);
        }

        // Get a new stream now that we've released it
        ASSERT_TRUE(OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream, nullptr));
        ASSERT_TRUE(stream.release != nullptr);

        ASSERT_EQ(stream.get_next(&stream, &array), 0);
        ASSERT_TRUE(array.release != nullptr);
        ASSERT_EQ(array.n_children, 1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
        ASSERT_EQ(array.length, poLayer->GetFeatureCount(false));
        ASSERT_EQ(array.null_count, 0);
        ASSERT_EQ(array.n_buffers, 1);
        ASSERT_TRUE(array.buffers[0] == nullptr); // no bitmap
        for( int i = 0; i < array.n_children; i++ )
        {
            ASSERT_TRUE(array.children[i]->release != nullptr);
            ASSERT_EQ(array.children[i]->length, array.length);
            ASSERT_TRUE(array.children[i]->n_buffers >= 2);
            ASSERT_TRUE(array.children[i]->buffers[0] == nullptr); // no bitmap
            ASSERT_EQ(array.children[i]->null_count, 0);
            ASSERT_TRUE(array.children[i]->buffers[1] != nullptr);
            if(array.children[i]->n_buffers == 3 )
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
            char** papszOptions = CSLSetNameValue(nullptr, "MAX_FEATURES_IN_BATCH", "2");
            ASSERT_TRUE(OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream, papszOptions));
            CSLDestroy(papszOptions);
        }
        ASSERT_TRUE(stream.release != nullptr);

        ASSERT_EQ(stream.get_next(&stream, &array), 0);
        ASSERT_TRUE(array.release != nullptr);
        ASSERT_EQ(array.n_children, 1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
        ASSERT_EQ(array.length, 2);
        for( int i = 0; i < array.n_children; i++ )
        {
            ASSERT_TRUE(array.children[i]->release != nullptr);
            ASSERT_EQ(array.children[i]->length, array.length);
            ASSERT_TRUE(array.children[i]->n_buffers >= 2);
            if( i > 0 )
            {
                ASSERT_TRUE(array.children[i]->buffers[0] != nullptr); // we have a bitmap
                ASSERT_EQ(array.children[i]->null_count, 1);
            }
            ASSERT_TRUE(array.children[i]->buffers[1] != nullptr);
            if(array.children[i]->n_buffers == 3 )
            {
                ASSERT_TRUE(array.children[i]->buffers[2] != nullptr);
            }
        }
        array.release(&array);

        // Next batch
        ASSERT_EQ(stream.get_next(&stream, &array), 0);
        ASSERT_TRUE(array.release != nullptr);
        ASSERT_EQ(array.n_children, 1 + poFDefn->GetFieldCount() + poFDefn->GetGeomFieldCount());
        ASSERT_EQ(array.length, 1);
        array.release(&array);

        // Next batch ==> End of stream
        ASSERT_EQ(stream.get_next(&stream, &array), 0);
        ASSERT_TRUE(array.release == nullptr);

        // Release stream
        stream.release(&stream);

        // Get a new stream now that we've released it
        ASSERT_TRUE(OGR_L_GetArrowStream(OGRLayer::ToHandle(poLayer), &stream, nullptr));
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
        OGRRangeFieldDomain oRange("name", "description", OGRFieldType::OFTReal, OGRFieldSubType::OFSTBoolean, min, true, max, true);
        oRange.SetMergePolicy(OGRFieldDomainMergePolicy::OFDMP_GEOMETRY_WEIGHTED);
        oRange.SetSplitPolicy(OGRFieldDomainSplitPolicy::OFDSP_GEOMETRY_RATIO);
        std::unique_ptr< OGRRangeFieldDomain > poClonedRange( oRange.Clone() );
        ASSERT_EQ(poClonedRange->GetName(), oRange.GetName());
        ASSERT_EQ(poClonedRange->GetDescription(), oRange.GetDescription());
        bool originalInclusive = false;
        bool cloneInclusive = false;
        ASSERT_EQ(poClonedRange->GetMin(originalInclusive).Real, oRange.GetMin(cloneInclusive).Real);
        ASSERT_EQ(originalInclusive, cloneInclusive);
        ASSERT_EQ(poClonedRange->GetMax(originalInclusive).Real, oRange.GetMax(cloneInclusive).Real);
        ASSERT_EQ(originalInclusive, cloneInclusive);
        ASSERT_EQ(poClonedRange->GetFieldType(), oRange.GetFieldType());
        ASSERT_EQ(poClonedRange->GetFieldSubType(), oRange.GetFieldSubType());
        ASSERT_EQ(poClonedRange->GetSplitPolicy(), oRange.GetSplitPolicy());
        ASSERT_EQ(poClonedRange->GetMergePolicy(), oRange.GetMergePolicy());

        // glob domain
        OGRGlobFieldDomain oGlob("name", "description", OGRFieldType::OFTString, OGRFieldSubType::OFSTBoolean, "*a*");
        oGlob.SetMergePolicy(OGRFieldDomainMergePolicy::OFDMP_GEOMETRY_WEIGHTED);
        oGlob.SetSplitPolicy(OGRFieldDomainSplitPolicy::OFDSP_GEOMETRY_RATIO);
        std::unique_ptr< OGRGlobFieldDomain > poClonedGlob( oGlob.Clone() );
        ASSERT_EQ(poClonedGlob->GetName(), oGlob.GetName());
        ASSERT_EQ(poClonedGlob->GetDescription(), oGlob.GetDescription());
        ASSERT_EQ(poClonedGlob->GetGlob(), oGlob.GetGlob());
        ASSERT_EQ(poClonedGlob->GetFieldType(), oGlob.GetFieldType());
        ASSERT_EQ(poClonedGlob->GetFieldSubType(), oGlob.GetFieldSubType());
        ASSERT_EQ(poClonedGlob->GetSplitPolicy(), oGlob.GetSplitPolicy());
        ASSERT_EQ(poClonedGlob->GetMergePolicy(), oGlob.GetMergePolicy());

        // coded value domain
        OGRCodedFieldDomain oCoded("name", "description", OGRFieldType::OFTString, OGRFieldSubType::OFSTBoolean, {OGRCodedValue()});
        oCoded.SetMergePolicy(OGRFieldDomainMergePolicy::OFDMP_GEOMETRY_WEIGHTED);
        oCoded.SetSplitPolicy(OGRFieldDomainSplitPolicy::OFDSP_GEOMETRY_RATIO);
        std::unique_ptr< OGRCodedFieldDomain > poClonedCoded( oCoded.Clone() );
        ASSERT_EQ(poClonedCoded->GetName(), oCoded.GetName());
        ASSERT_EQ(poClonedCoded->GetDescription(), oCoded.GetDescription());
        ASSERT_EQ(poClonedCoded->GetFieldType(), oCoded.GetFieldType());
        ASSERT_EQ(poClonedCoded->GetFieldSubType(), oCoded.GetFieldSubType());
        ASSERT_EQ(poClonedCoded->GetSplitPolicy(), oCoded.GetSplitPolicy());
        ASSERT_EQ(poClonedCoded->GetMergePolicy(), oCoded.GetMergePolicy());
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
        EXPECT_EQ(oFDefn.GetFields().size(), oFDefn.GetFieldCount());
        int i = 0;
        for( const auto* poFieldDefn: oFDefn.GetFields() )
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
        EXPECT_EQ(oFDefn.GetGeomFields().size(), oFDefn.GetGeomFieldCount());
        int i = 0;
        for( const auto* poGeomFieldDefn: oFDefn.GetGeomFields() )
        {
            EXPECT_EQ(oFDefn.GetGeomFields()[i], oFDefn.GetGeomFieldDefn(i));
            EXPECT_EQ(poGeomFieldDefn, oFDefn.GetGeomFieldDefn(i));
            ++i;
        }
        EXPECT_EQ(i, oFDefn.GetGeomFieldCount());
    }

} // namespace

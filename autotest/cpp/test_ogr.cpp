///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general OGR features.
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

#include "ogr_p.h"
#include "ogrsf_frmts.h"
#include "../../gdal/ogr/ogrsf_frmts/osm/gpb.h"

#include <string>

namespace tut
{

    // Common fixture with test data
    struct test_ogr_data
    {
        // Expected number of drivers
        GDALDriverManager* drv_reg_;
        int drv_count_;
        std::string drv_shape_;
        bool has_geos_support_;
        std::string data_;
        std::string data_tmp_;

        test_ogr_data()
            : drv_reg_(nullptr),
            drv_count_(0),
            drv_shape_("ESRI Shapefile"),
            data_(tut::common::data_basedir),
            data_tmp_(tut::common::tmp_basedir)
        {
            drv_reg_ = GetGDALDriverManager();

            // Windows CE port builds with fixed number of drivers
#ifdef OGR_ENABLED
#ifdef CSV_ENABLED
            drv_count_++;
#endif
#ifdef GML_ENABLED
            drv_count_++;
#endif
#ifdef SHAPE_ENABLED
            drv_count_++;
#endif
#ifdef SQLITE_ENABLED
            drv_count_++;
#endif
#ifdef TAB_ENABLED
            drv_count_++;
#endif
#endif /* OGR_ENABLED */

        }
    };

    // Register test group
    typedef test_group<test_ogr_data> group;
    typedef group::object object;
    group test_ogr_group("OGR");

    // Test OGR driver registrar access
    template<>
    template<>
    void object::test<1>()
    {
        ensure("GetGDALDriverManager() is NULL", nullptr != drv_reg_);
    }

    // Test if Shapefile driver is registered
    template<>
    template<>
    void object::test<3>()
    {
        GDALDriverManager* manager = GetGDALDriverManager();
        ensure(nullptr != manager);

        GDALDriver* drv = manager->GetDriverByName(drv_shape_.c_str());
        ensure("Shapefile driver is not registered", nullptr != drv);
    }

    template<class T>
    void testSpatialReferenceLeakOnCopy(OGRSpatialReference* poSRS)
    {
        ensure("GetReferenceCount expected to be 1 before copies", 1 == poSRS->GetReferenceCount());
        {
            int nCurCount;
            int nLastCount = 1;
            T value;
            value.assignSpatialReference(poSRS);
            nCurCount = poSRS->GetReferenceCount();
            ensure("SRS reference count not incremented by assignSpatialReference", nCurCount > nLastCount );
            nLastCount = nCurCount;

            T value2(value);
            nCurCount = poSRS->GetReferenceCount();
            ensure("SRS reference count not incremented by copy constructor", nCurCount > nLastCount );
            nLastCount = nCurCount;

            T value3;
            value3 = value;
            nCurCount = poSRS->GetReferenceCount();
            ensure("SRS reference count not incremented by assignment operator", nCurCount > nLastCount );
            nLastCount = nCurCount;

            value3 = value;
            ensure( "SRS reference count incremented by assignment operator",
                    nLastCount == poSRS->GetReferenceCount() );

        }
        ensure( "GetReferenceCount expected to be decremented by destructors",
                1 == poSRS->GetReferenceCount() );
    }

    // Test if copy does not leak or double delete the spatial reference
    template<>
    template<>
    void object::test<4>()
    {
        OGRSpatialReference* poSRS = new OGRSpatialReference();
        ensure(nullptr != poSRS);

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
        ensure( nullptr != poOrigin);

        T value2( *poOrigin );

        std::ostringstream strErrorCopy;
        strErrorCopy << poOrigin->getGeometryName() << ": copy constructor changed a value";
        ensure(strErrorCopy.str().c_str(), CPL_TO_BOOL(poOrigin->Equals(&value2)));

        T value3;
        value3 = *poOrigin;
        value3 = *poOrigin;
        auto& value3Ref(value3);
        value3 = value3Ref;

        std::ostringstream strErrorAssign;
        strErrorAssign << poOrigin->getGeometryName() << ": assignment operator changed a value";
#ifdef DEBUG_VERBOSE
        char* wkt1 = NULL, *wkt2 = NULL;
        poOrigin->exportToWkt(&wkt1);
        value3.exportToWkt(&wkt2);
        printf("%s %s\n", wkt1, wkt2);
        CPLFree(wkt1);
        CPLFree(wkt2);
#endif
        ensure(strErrorAssign.str().c_str(), CPL_TO_BOOL(poOrigin->Equals(&value3)));

        OGRGeometryFactory::destroyGeometry(poOrigin);
    }

    // Test if copy constructor and assignment operators succeeds on copying the geometry data
    template<>
    template<>
    void object::test<5>()
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

    template<>
    template<>
    void object::test<6>()
    {
        {
            OGRPoint p;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&p, 1, &x, 0, &y, 0, nullptr, 0 );
            ensure_equals(p.getCoordinateDimension(), 2);
            ensure_equals(p.getX(), 1);
            ensure_equals(p.getY(), 2);
            ensure_equals(p.getZ(), 0);
        }

        {
            OGRPoint p;
            double x = 1, y = 2, z = 3;
            OGR_G_SetPoints( (OGRGeometryH)&p, 1, &x, 0, &y, 0, &z, 0 );
            ensure_equals(p.getCoordinateDimension(), 3);
            ensure_equals(p.getX(), 1);
            ensure_equals(p.getY(), 2);
            ensure_equals(p.getZ(), 3);
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
            ensure_equals(ls.getCoordinateDimension(), 2);
            ensure_equals(ls.getX(0), 1);
            ensure_equals(ls.getY(0), 2);
            ensure_equals(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 0, &y, 0, nullptr, 0 );
            ensure_equals(ls.getCoordinateDimension(), 2);
            ensure_equals(ls.getX(0), 1);
            ensure_equals(ls.getY(0), 2);
            ensure_equals(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 8, &y, 8, nullptr, 0 );
            ensure_equals(ls.getCoordinateDimension(), 2);
            ensure_equals(ls.getX(0), 1);
            ensure_equals(ls.getY(0), 2);
            ensure_equals(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2, z = 3;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 0, &y, 0, &z, 0 );
            ensure_equals(ls.getCoordinateDimension(), 3);
            ensure_equals(ls.getX(0), 1);
            ensure_equals(ls.getY(0), 2);
            ensure_equals(ls.getZ(0), 3);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2, z = 3;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 8, &y, 8, &z, 8 );
            ensure_equals(ls.getCoordinateDimension(), 3);
            ensure_equals(ls.getX(0), 1);
            ensure_equals(ls.getY(0), 2);
            ensure_equals(ls.getZ(0), 3);
        }

        {
            OGRLineString ls;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, nullptr, 0, nullptr, 0, nullptr, 0 );
            CPLPopErrorHandler();
        }
    }

    template<>
    template<>
    void object::test<7>()
    {
      OGRStyleMgrH hSM = OGR_SM_Create(nullptr);
      OGR_SM_InitStyleString(hSM, "PEN(w:2px,c:#000000,id:\"mapinfo-pen-2,ogr-pen-0\")");
      OGRStyleToolH hTool = OGR_SM_GetPart(hSM, 0, nullptr);
      int bValueIsNull;

      ensure_distance(OGR_ST_GetParamDbl(hTool, OGRSTPenWidth, &bValueIsNull), 2.0 * (1.0 / (72.0 * 39.37)) * 1000, 1e-6);
      ensure_equals(OGR_ST_GetUnit(hTool), OGRSTUMM);

      OGR_ST_SetUnit(hTool, OGRSTUPixel, 1.0);
      ensure_equals(OGR_ST_GetParamDbl(hTool, OGRSTPenWidth, &bValueIsNull), 2.0);
      ensure_equals(OGR_ST_GetUnit(hTool), OGRSTUPixel);
      OGR_ST_Destroy(hTool);

      OGR_SM_Destroy(hSM);
    }

    template<>
    template<>
    void object::test<8>()
    {
        OGRField sField;
        ensure_equals(OGRParseDate("2017/11/31 12:34:56", &sField, 0), TRUE);
        ensure_equals(sField.Date.Year, 2017);
        ensure_equals(sField.Date.Month, 11);
        ensure_equals(sField.Date.Day, 31);
        ensure_equals(sField.Date.Hour, 12);
        ensure_equals(sField.Date.Minute, 34);
        ensure_equals(sField.Date.Second, 56.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure_equals(OGRParseDate("2017/11/31 12:34:56+00", &sField, 0), TRUE);
        ensure_equals(sField.Date.TZFlag, 100);

        ensure_equals(OGRParseDate("2017/11/31 12:34:56+12:00", &sField, 0), TRUE);
        ensure_equals(sField.Date.TZFlag, 100 + 12 * 4);

        ensure_equals(OGRParseDate("2017/11/31 12:34:56+1200", &sField, 0), TRUE);
        ensure_equals(sField.Date.TZFlag, 100 + 12 * 4);

        ensure_equals(OGRParseDate("2017/11/31 12:34:56+815", &sField, 0), TRUE);
        ensure_equals(sField.Date.TZFlag, 100 + 8 * 4 + 1);

        ensure_equals(OGRParseDate("2017/11/31 12:34:56-12:00", &sField, 0), TRUE);
        ensure_equals(sField.Date.TZFlag, 100 - 12 * 4);

        ensure_equals(OGRParseDate(" 2017/11/31 12:34:56", &sField, 0), TRUE);
        ensure_equals(sField.Date.Year, 2017);

        ensure_equals(OGRParseDate("2017/11/31 12:34:56.789", &sField, 0), TRUE);
        ensure_equals(sField.Date.Second, 56.789f);

        // Leap second
        ensure_equals(OGRParseDate("2017/11/31 12:34:60", &sField, 0), TRUE);
        ensure_equals(sField.Date.Second, 60.0f);

        ensure_equals(OGRParseDate("2017-11-31T12:34:56", &sField, 0), TRUE);
        ensure_equals(sField.Date.Year, 2017);
        ensure_equals(sField.Date.Month, 11);
        ensure_equals(sField.Date.Day, 31);
        ensure_equals(sField.Date.Hour, 12);
        ensure_equals(sField.Date.Minute, 34);
        ensure_equals(sField.Date.Second, 56.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure_equals(OGRParseDate("2017-11-31T12:34:56Z", &sField, 0), TRUE);
        ensure_equals(sField.Date.Second, 56.0f);
        ensure_equals(sField.Date.TZFlag, 100);

        ensure_equals(OGRParseDate("2017-11-31T12:34:56.789Z", &sField, 0), TRUE);
        ensure_equals(sField.Date.Second, 56.789f);
        ensure_equals(sField.Date.TZFlag, 100);

        ensure_equals(OGRParseDate("2017-11-31", &sField, 0), TRUE);
        ensure_equals(sField.Date.Year, 2017);
        ensure_equals(sField.Date.Month, 11);
        ensure_equals(sField.Date.Day, 31);
        ensure_equals(sField.Date.Hour, 0);
        ensure_equals(sField.Date.Minute, 0);
        ensure_equals(sField.Date.Second, 0.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure_equals(OGRParseDate("2017-11-31Z", &sField, 0), TRUE);
        ensure_equals(sField.Date.Year, 2017);
        ensure_equals(sField.Date.Month, 11);
        ensure_equals(sField.Date.Day, 31);
        ensure_equals(sField.Date.Hour, 0);
        ensure_equals(sField.Date.Minute, 0);
        ensure_equals(sField.Date.Second, 0.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure_equals(OGRParseDate("12:34", &sField, 0), TRUE);
        ensure_equals(sField.Date.Year, 0);
        ensure_equals(sField.Date.Month, 0);
        ensure_equals(sField.Date.Day, 0);
        ensure_equals(sField.Date.Hour, 12);
        ensure_equals(sField.Date.Minute, 34);
        ensure_equals(sField.Date.Second, 0.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure_equals(OGRParseDate("12:34:56", &sField, 0), TRUE);
        ensure_equals(OGRParseDate("12:34:56.789", &sField, 0), TRUE);

        ensure(!OGRParseDate("2017", &sField, 0));
        ensure(!OGRParseDate("12:", &sField, 0));
        ensure(!OGRParseDate("2017-a-31T12:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-00-31T12:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-13-31T12:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-01-00T12:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-01-aT12:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-01-32T12:34:56", &sField, 0));
        ensure(!OGRParseDate("a:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-01-01Ta:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-01-01T25:34:56", &sField, 0));
        ensure(!OGRParseDate("2017-01-01T00:a:00", &sField, 0));
        ensure(!OGRParseDate("2017-01-01T00: 34:56", &sField, 0));
        ensure(!OGRParseDate("2017-01-01T00:61:00", &sField, 0));
        ensure(!OGRParseDate("2017-01-01T00:00:61", &sField, 0));
        ensure(!OGRParseDate("2017-01-01T00:00:a", &sField, 0));
    }

    // Test OGRPolygon::IsPointOnSurface()
    template<>
    template<>
    void object::test<9>()
    {
        OGRPolygon oPoly;

        OGRPoint oEmptyPoint;
        ensure( !oPoly.IsPointOnSurface(&oEmptyPoint) );

        OGRPoint oPoint;
        oPoint.setX(1);
        oPoint.setY(1);
        ensure( !oPoly.IsPointOnSurface(&oPoint) );

        const char* pszPoly = "POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))";
        oPoly.importFromWkt(&pszPoly);

        ensure( !oPoly.IsPointOnSurface(&oEmptyPoint) );

        ensure_equals( oPoly.IsPointOnSurface(&oPoint), TRUE );

        oPoint.setX(5);
        oPoint.setY(5);
        ensure( !oPoly.IsPointOnSurface(&oPoint) );
    }

    // Test gpb.h
    template<>
    template<>
    void object::test<10>()
    {
        ensure_equals( GetVarUIntSize(0), 1 );
        ensure_equals( GetVarUIntSize(127), 1 );
        ensure_equals( GetVarUIntSize(128), 2 );
        ensure_equals( GetVarUIntSize((1 << 14) - 1), 2 );
        ensure_equals( GetVarUIntSize(1 << 14), 3 );
        ensure_equals( GetVarUIntSize(GUINT64_MAX), 10 );

        ensure_equals( GetVarIntSize(0), 1 );
        ensure_equals( GetVarIntSize(127), 1 );
        ensure_equals( GetVarIntSize(128), 2 );
        ensure_equals( GetVarIntSize((1 << 14) - 1), 2 );
        ensure_equals( GetVarIntSize(1 << 14), 3 );
        ensure_equals( GetVarIntSize(GINT64_MAX), 9 );
        ensure_equals( GetVarIntSize(-1), 10 );
        ensure_equals( GetVarIntSize(GINT64_MIN), 10 );

        ensure_equals( GetVarSIntSize(0), 1 );
        ensure_equals( GetVarSIntSize(63), 1 );
        ensure_equals( GetVarSIntSize(64), 2 ); 
        ensure_equals( GetVarSIntSize(-1), 1 );
        ensure_equals( GetVarSIntSize(-64), 1 );
        ensure_equals( GetVarSIntSize(-65), 2 );
        ensure_equals( GetVarSIntSize(GINT64_MIN), 10 );
        ensure_equals( GetVarSIntSize(GINT64_MAX), 10 );

        ensure_equals( GetTextSize(""), 1 );
        ensure_equals( GetTextSize(" "), 2 );
        ensure_equals( GetTextSize(std::string(" ")), 2 );

        GByte abyBuffer[11] = { 0 };
        GByte* pabyBuffer;
        const GByte* pabyBufferRO;

        pabyBuffer = abyBuffer;
        WriteVarUInt(&pabyBuffer, 0);
        ensure_equals(pabyBuffer - abyBuffer, 1);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadVarUInt64(&pabyBufferRO), 0U);

        pabyBuffer = abyBuffer;
        WriteVarUInt(&pabyBuffer, 127);
        ensure_equals(pabyBuffer - abyBuffer, 1);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadVarUInt64(&pabyBufferRO), 127U);

        pabyBuffer = abyBuffer;
        WriteVarUInt(&pabyBuffer, 0xDEADBEEFU);
        ensure_equals(pabyBuffer - abyBuffer, 5);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadVarUInt64(&pabyBufferRO), 0xDEADBEEFU);

        pabyBuffer = abyBuffer;
        WriteVarUInt(&pabyBuffer, GUINT64_MAX);
        ensure_equals(pabyBuffer - abyBuffer, 10);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadVarUInt64(&pabyBufferRO), GUINT64_MAX);


        pabyBuffer = abyBuffer;
        WriteVarInt(&pabyBuffer, GINT64_MAX);
        ensure_equals(pabyBuffer - abyBuffer, 9);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadVarInt64(&pabyBufferRO), GINT64_MAX);

        pabyBuffer = abyBuffer;
        WriteVarInt(&pabyBuffer, -1);
        ensure_equals(pabyBuffer - abyBuffer, 10);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadVarInt64(&pabyBufferRO), -1);

        pabyBuffer = abyBuffer;
        WriteVarInt(&pabyBuffer, GINT64_MIN);
        ensure_equals(pabyBuffer - abyBuffer, 10);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadVarInt64(&pabyBufferRO), GINT64_MIN);


        pabyBuffer = abyBuffer;
        WriteVarSInt(&pabyBuffer, 0);
        ensure_equals(pabyBuffer - abyBuffer, 1);
        {
            GIntBig nVal;
            pabyBufferRO = abyBuffer;
            READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
            ensure_equals(nVal, 0);
        }

        pabyBuffer = abyBuffer;
        WriteVarSInt(&pabyBuffer, 1);
        ensure_equals(pabyBuffer - abyBuffer, 1);
        {
            GIntBig nVal;
            pabyBufferRO = abyBuffer;
            READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
            ensure_equals(nVal, 1);
        }

        pabyBuffer = abyBuffer;
        WriteVarSInt(&pabyBuffer, -1);
        ensure_equals(pabyBuffer - abyBuffer, 1);
        {
            GIntBig nVal;
            pabyBufferRO = abyBuffer;
            READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
            ensure_equals(nVal, -1);
        }

        pabyBuffer = abyBuffer;
        WriteVarSInt(&pabyBuffer, GINT64_MAX);
        ensure_equals(pabyBuffer - abyBuffer, 10);
        {
            GIntBig nVal;
            pabyBufferRO = abyBuffer;
            READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
            ensure_equals(nVal, GINT64_MAX);
        }

        pabyBuffer = abyBuffer;
        WriteVarSInt(&pabyBuffer, GINT64_MIN);
        ensure_equals(pabyBuffer - abyBuffer, 10);
        {
            GIntBig nVal;
            pabyBufferRO = abyBuffer;
            READ_VARSINT64(pabyBufferRO, abyBuffer + 10, nVal);
            ensure_equals(nVal, GINT64_MIN);
        }

        pabyBuffer = abyBuffer;
        WriteText(&pabyBuffer, "x");
        ensure_equals(pabyBuffer - abyBuffer, 2);
        ensure_equals(abyBuffer[0], 1);
        ensure_equals(abyBuffer[1], 'x');

        pabyBuffer = abyBuffer;
        WriteText(&pabyBuffer, std::string("x"));
        ensure_equals(pabyBuffer - abyBuffer, 2);
        ensure_equals(abyBuffer[0], 1);
        ensure_equals(abyBuffer[1], 'x');

        pabyBuffer = abyBuffer;
        WriteFloat32(&pabyBuffer, 1.25f);
        ensure_equals(pabyBuffer - abyBuffer, 4);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadFloat32(&pabyBufferRO, abyBuffer + 4), 1.25f);

        pabyBuffer = abyBuffer;
        WriteFloat64(&pabyBuffer, 1.25);
        ensure_equals(pabyBuffer - abyBuffer, 8);
        pabyBufferRO = abyBuffer;
        ensure_equals(ReadFloat64(&pabyBufferRO, abyBuffer + 8), 1.25);
    }

    // Test OGRGeometry::toXXXXX()
    template<>
    template<>
    void object::test<11>()
    {
        #define CONCAT(X,Y) X##Y
        #define TEST_OGRGEOMETRY_TO(X) { \
            CONCAT(OGR,X) o; \
            OGRGeometry* poGeom = &o; \
            ensure_equals( poGeom->CONCAT(to,X)(), &o ); }

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
            ensure_equals( poGeom->toCurve(), &o );
        }
        {
            OGRPolygon o;
            OGRGeometry* poGeom = &o;
            ensure_equals( poGeom->toSurface(), &o );
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
        ensure_equals(nCount, nExpectedPointCount);

        nCount = 0;
        const T* const_obj(obj);
        for( const auto& elt: const_obj)
        {
            nCount ++;
            CPL_IGNORE_RET_VAL(elt);
        }
        ensure_equals(nCount, nExpectedPointCount);
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
    template<>
    template<>
    void object::test<12>()
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
            ensure_equals(oVisitor.getNumPoints(), asTests[i].nExpectedPointCount);
            PointCounterConstVisitor oConstVisitor;
            poGeom->accept(&oConstVisitor);
            ensure_equals(oConstVisitor.getNumPoints(), asTests[i].nExpectedPointCount);
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
    }

    // Test layer, dataset-feature and layer-feature iterators
    template<>
    template<>
    void object::test<13>()
    {
        std::string file(data_ + SEP + "poly.shp");
        GDALDatasetUniquePtr poDS(
            GDALDataset::Open(file.c_str(), GDAL_OF_VECTOR));
        ensure( poDS != nullptr );

        {
            GIntBig nExpectedFID = 0;
            for( const auto& oFeatureLayerPair: poDS->GetFeatures() )
            {
                ensure_equals( oFeatureLayerPair.feature->GetFID(), nExpectedFID );
                nExpectedFID ++;
                ensure_equals( oFeatureLayerPair.layer, poDS->GetLayer(0) );
            }
            ensure_equals(nExpectedFID, 10);
        }

        ensure_equals( poDS->GetLayers().size(), 1U );
        ensure_equals( poDS->GetLayers()[0], poDS->GetLayer(0) );
        ensure_equals( poDS->GetLayers()[static_cast<size_t>(0)], poDS->GetLayer(0) );
        ensure_equals( poDS->GetLayers()["poly"], poDS->GetLayer(0) );

        for( auto poLayer: poDS->GetLayers() )
        {
            GIntBig nExpectedFID = 0;
            for( const auto& poFeature: poLayer )
            {
                ensure_equals( poFeature->GetFID(), nExpectedFID );
                nExpectedFID ++;
            }
            ensure_equals(nExpectedFID, 10);

            nExpectedFID = 0;
            for(const  auto& oFeatureLayerPair: poDS->GetFeatures() )
            {
                ensure_equals( oFeatureLayerPair.feature->GetFID(), nExpectedFID );
                nExpectedFID ++;
                ensure_equals( oFeatureLayerPair.layer, poLayer );
            }
            ensure_equals(nExpectedFID, 10);

            nExpectedFID = 0;
            OGR_FOR_EACH_FEATURE_BEGIN(hFeat, reinterpret_cast<OGRLayerH>(poLayer))
            {
                if( nExpectedFID == 0 )
                {
                    nExpectedFID = 1;
                    continue;
                }
                ensure_equals( OGR_F_GetFID(hFeat), nExpectedFID );
                nExpectedFID ++;
                if( nExpectedFID == 5 )
                    break;
            }
            OGR_FOR_EACH_FEATURE_END(hFeat)
            ensure_equals(nExpectedFID, 5);

            auto oIter = poLayer->begin();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            // Only one feature iterator can be active at a time
            auto oIter2 = poLayer->begin();
            CPLPopErrorHandler();
            ensure( !(oIter2 != poLayer->end()) );
            ensure( oIter != poLayer->end() );
        }

        poDS.reset(GetGDALDriverManager()->GetDriverByName("Memory")->
            Create("", 0, 0, 0, GDT_Unknown, nullptr));
        int nCountLayers = 0;
        for( auto poLayer: poDS->GetLayers() )
        {
            CPL_IGNORE_RET_VAL(poLayer);
            nCountLayers++;
        }
        ensure_equals(nCountLayers, 0);

        poDS->CreateLayer("foo");
        poDS->CreateLayer("bar");
        for( auto poLayer: poDS->GetLayers() )
        {
            if( nCountLayers == 0 )
                ensure_equals( poLayer->GetName(), "foo" );
            else if( nCountLayers == 1 )
                ensure_equals( poLayer->GetName(), "bar" );
            nCountLayers++;
        }
        ensure_equals(nCountLayers, 2);

        // std::copy requires a InputIterator
        std::vector<OGRLayer*> oTarget;
        oTarget.resize(2);
        auto layers = poDS->GetLayers();
        std::copy(layers.begin(), layers.end(), oTarget.begin());
        ensure_equals(oTarget[0], layers[0]);
        ensure_equals(oTarget[1], layers[1]);

        // but in practice not necessarily uses the postincrement iterator.
        oTarget.clear();
        oTarget.resize(2);
        auto input_iterator = layers.begin();
        auto output_iterator = oTarget.begin();
        while (input_iterator != layers.end())
        {
            *output_iterator++ = *input_iterator++;
        }
        ensure_equals(oTarget[0], layers[0]);
        ensure_equals(oTarget[1], layers[1]);

        // Test copy constructor
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter(srcIter);
            ensure_equals(*newIter, layers[1]);
        }

        // Test assignment operator
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter;
            newIter = srcIter;
            ensure_equals(*newIter, layers[1]);
        }

        // Test move constructor
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter(std::move(srcIter));
            ensure_equals(*newIter, layers[1]);
        }

        // Test move assignment operator
        {
            GDALDataset::Layers::Iterator srcIter(poDS->GetLayers().begin());
            ++srcIter;
            GDALDataset::Layers::Iterator newIter;
            newIter = std::move(srcIter);
            ensure_equals(*newIter, layers[1]);
        }

    }

    // Test field iterator
    template<>
    template<>
    void object::test<14>()
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
            ensure_equals( oFeatureTmp[0].GetString(), "bar" );
            {
                // Proxy reference
                auto&& x= oFeatureTmp[0];
                auto& xRef(x);
                x = xRef;
                ensure_equals( oFeatureTmp[0].GetString(), "bar" );
            }
            {
                oFeatureTmp[0] = oFeatureTmp[0];
                ensure_equals( oFeatureTmp[0].GetString(), "bar" );
            }
            {
                // Proxy reference
                auto&& x= oFeatureTmp[0];
                x = "baz";
                ensure_equals( x.GetString(), "baz" );
            }
            oFeatureTmp["str_field"] = std::string("foo");
            oFeatureTmp["int_field"] = 123;
            oFeatureTmp["int64_field"] = oFeatureTmp["int_field"];
            ensure_equals( oFeatureTmp["int64_field"].GetInteger(), 123 );
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
            ensure_equals( CSLCount(static_cast<CSLConstList>(oFeatureTmp["strlist_field"])), 2 );
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
            ensure_equals( x, 123 );
        }
        {
            int x = oFeature["int_field"];
            ensure_equals( x, 123 );
        }
        {
            GIntBig x = oFeature["int64_field"];
            ensure_equals( x, static_cast<GIntBig>(1234567890123) );
        }
        {
            double x = oFeature["double_field"];
            ensure_equals( x, 123.45 );
        }
        {
            const char* x = oFeature["str_field"];
            ensure_equals( x, "foo" );
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
        ensure(bExceptionHit);

        int iIter = 0;
        const OGRFeature* poConstFeature = &oFeature;
        for( const auto& oField: *poConstFeature )
        {
            ensure_equals( oField.GetIndex(), iIter );
            ensure_equals( oField.GetDefn(), poFeatureDefn->GetFieldDefn(iIter) );
            ensure_equals( CPLString(oField.GetName()), CPLString(oField.GetDefn()->GetNameRef()) );
            ensure_equals( oField.GetType(), oField.GetDefn()->GetType() );
            ensure_equals( oField.GetSubType(), oField.GetDefn()->GetSubType() );
            if( iIter == 0 )
            {
                ensure_equals( oField.IsUnset(), false );
                ensure_equals( oField.IsNull(), false );
                ensure_equals( CPLString(oField.GetRawValue()->String), CPLString("foo") );
                ensure_equals( CPLString(oField.GetString()), CPLString("foo") );
                ensure_equals( CPLString(oField.GetAsString()), CPLString("foo") );
            }
            else if( iIter == 1 )
            {
                ensure_equals( oField.GetRawValue()->Integer, 123 );
                ensure_equals( oField.GetInteger(), 123 );
                ensure_equals( oField.GetAsInteger(), 123 );
                ensure_equals( oField.GetAsInteger64(), 123 );
                ensure_equals( oField.GetAsDouble(), 123.0 );
                ensure_equals( CPLString(oField.GetAsString()), CPLString("123") );
            }
            else if( iIter == 2 )
            {
                ensure_equals( oField.GetRawValue()->Integer64, 1234567890123 );
                ensure_equals( oField.GetInteger64(), 1234567890123 );
                ensure_equals( oField.GetAsInteger(), 2147483647 );
                ensure_equals( oField.GetAsInteger64(), 1234567890123 );
                ensure_equals( oField.GetAsDouble(), 1234567890123.0 );
                ensure_equals( CPLString(oField.GetAsString()), CPLString("1234567890123") );
            }
            else if( iIter == 3 )
            {
                ensure_equals( oField.GetRawValue()->Real, 123.45 );
                ensure_equals( oField.GetDouble(), 123.45 );
                ensure_equals( oField.GetAsInteger(), 123 );
                ensure_equals( oField.GetAsInteger64(), 123 );
                ensure_equals( oField.GetAsDouble(), 123.45 );
                ensure_equals( CPLString(oField.GetAsString()), CPLString("123.45") );
            }
            else if( iIter == 4 )
            {
                ensure_equals( oField.IsUnset(), false );
                ensure_equals( oField.IsNull(), true );
            }
            else if( iIter == 5 )
            {
                ensure_equals( oField.IsUnset(), true );
                ensure_equals( oField.empty(), true );
                ensure_equals( oField.IsNull(), false );
            }
            else if( iIter == 6 )
            {
                int nYear, nMonth, nDay, nHour, nMin, nTZFlag;
                float fSec;
                ensure_equals( oField.GetDateTime(&nYear, &nMonth, &nDay,
                                                  &nHour, &nMin, &fSec,
                                                  &nTZFlag), true );
                ensure_equals( nYear, 2018 );
                ensure_equals( nMonth, 4 );
                ensure_equals( nDay, 5 );
                ensure_equals( nHour, 12 );
                ensure_equals( nMin, 34 );
                ensure_equals( fSec, 56.75f );
                ensure_equals( nTZFlag, 0 );
            }
            else if( iIter == 7 )
            {
                std::vector<std::string> oExpected{ std::string("foo"),
                                                    std::string("bar") };
                decltype(oExpected) oGot = oField;
                ensure_equals( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ensure_equals( oGot[i], oExpected[i] );
            }
            else if( iIter == 8 )
            {
                std::vector<int> oExpected{ 12, 34 };
                decltype(oExpected) oGot = oField;
                ensure_equals( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ensure_equals( oGot[i], oExpected[i] );
            }
            else if( iIter == 9 )
            {
                std::vector<GIntBig> oExpected{ 1234567890123, 34 };
                decltype(oExpected) oGot = oField;
                ensure_equals( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ensure_equals( oGot[i], oExpected[i] );
            }
            else if( iIter == 10 )
            {
                std::vector<double> oExpected{ 12.25, 56.75 };
                decltype(oExpected) oGot = oField;
                ensure_equals( oGot.size(), oExpected.size() );
                for( size_t i = 0; i < oExpected.size(); i++ )
                    ensure_equals( oGot[i], oExpected[i] );
            }
            iIter ++;

        }
        poFeatureDefn->Release();
    }

    // Test OGRGetXMLDateTime()
    template<>
    template<>
    void object::test<15>()
    {
        OGRField sField;
        char *pszDateTime;

        sField.Date.Year = 2001;
        sField.Date.Month = 2;
        sField.Date.Day = 3;
        sField.Date.Hour = 4;
        sField.Date.Minute = 5;

        // Unknown time zone (TZFlag = 0), no millisecond count
        sField.Date.TZFlag = 0;
        sField.Date.Second = 6.0f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "unknown time zone, no millisecond count",
               strcmp("2001-02-03T04:05:06", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // Unknown time zone (TZFlag = 0), millisecond count
        sField.Date.TZFlag = 0;
        sField.Date.Second = 6.789f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "unknown time zone, millisecond count",
               strcmp("2001-02-03T04:05:06.789", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // Local time zone (TZFlag = 1), no millisecond count
        sField.Date.TZFlag = 1;
        sField.Date.Second = 6.0f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "local time zone, no millisecond count",
               strcmp("2001-02-03T04:05:06", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // Local time zone (TZFlag = 1), millisecond count
        sField.Date.TZFlag = 1;
        sField.Date.Second = 6.789f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "local time zone, millisecond count",
               strcmp("2001-02-03T04:05:06.789", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // GMT time zone (TZFlag = 100), no millisecond count
        sField.Date.TZFlag = 100;
        sField.Date.Second = 6.0f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "GMT time zone, no millisecond count",
               strcmp("2001-02-03T04:05:06Z", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // GMT time zone (TZFlag = 100), millisecond count
        sField.Date.TZFlag = 100;
        sField.Date.Second = 6.789f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "GMT time zone, millisecond count",
               strcmp("2001-02-03T04:05:06.789Z", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // Positive time-zone offset, no millisecond count
        sField.Date.TZFlag = 111;
        sField.Date.Second = 6.0f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "positive time-zone offset, no millisecond count",
               strcmp("2001-02-03T04:05:06+02:45", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // Positive time-zone offset, millisecond count
        sField.Date.TZFlag = 111;
        sField.Date.Second = 6.789f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "positive time-zone offset, millisecond count",
               strcmp("2001-02-03T04:05:06.789+02:45", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // Negative time-zone offset, no millisecond count
        sField.Date.TZFlag = 88;
        sField.Date.Second = 6.0f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "negative time-zone offset, no millisecond count",
               strcmp("2001-02-03T04:05:06-03:00", pszDateTime) == 0);
        CPLFree(pszDateTime);

        // Negative time-zone offset, millisecond count
        sField.Date.TZFlag = 88;
        sField.Date.Second = 6.789f;
        pszDateTime = OGRGetXMLDateTime(&sField);
        ensure(nullptr != pszDateTime);
        ensure("OGRGetXMLDateTime formats date/time field with "
               "negative time-zone offset, millisecond count",
               strcmp("2001-02-03T04:05:06.789-03:00", pszDateTime) == 0);
        CPLFree(pszDateTime);
    }
} // namespace tut

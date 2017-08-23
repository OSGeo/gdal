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

#include <ogrsf_frmts.h>

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

        test_ogr_data()
            : drv_reg_(NULL),
            drv_count_(0),
            drv_shape_("ESRI Shapefile")
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
        ensure("GetGDALDriverManager() is NULL", NULL != drv_reg_);
    }

    // Test if Shapefile driver is registered
    template<>
    template<>
    void object::test<3>()
    {
        GDALDriverManager* manager = GetGDALDriverManager();
        ensure(NULL != manager);

        GDALDriver* drv = manager->GetDriverByName(drv_shape_.c_str());
        ensure("Shapefile driver is not registered", NULL != drv);
    }

    template<class T>
    void testSpatialReferenceLeakOnCopy(OGRSpatialReference* poSRS)
    {
        ensure("GetReferenceCount expected to be 1 before copies", 1 == poSRS->GetReferenceCount());
        {
            T value;
            value.assignSpatialReference(poSRS);
            ensure("SRS reference count not incremented by assignSpatialReference", 2 == poSRS->GetReferenceCount());

            T value2(value);
            ensure("SRS reference count not incremented by copy constructor", 3 == poSRS->GetReferenceCount());

            T value3;
            value3 = value;
            ensure("SRS reference count not incremented by assignment operator", 4 == poSRS->GetReferenceCount());

            value3 = value;
            ensure( "SRS reference count incremented by assignment operator",
                    4 == poSRS->GetReferenceCount() );

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
        ensure(NULL != poSRS);

        testSpatialReferenceLeakOnCopy<OGRPoint>(poSRS);
        testSpatialReferenceLeakOnCopy<OGRLineString>(poSRS);
        testSpatialReferenceLeakOnCopy<OGRLinearRing>(poSRS);
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
        ensure( NULL != poOrigin);

        T value2( *poOrigin );

        std::ostringstream strErrorCopy;
        strErrorCopy << poOrigin->getGeometryName() << ": copy constructor changed a value";
        ensure(strErrorCopy.str().c_str(), CPL_TO_BOOL(poOrigin->Equals(&value2)));

        T value3;
        value3 = *poOrigin;
        value3 = *poOrigin;
        value3 = value3;

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
            OGR_G_SetPoints( (OGRGeometryH)&p, 1, &x, 0, &y, 0, NULL, 0 );
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
            OGR_G_SetPoints( (OGRGeometryH)&p, 1, NULL, 0, NULL, 0, NULL, 0 );
            CPLPopErrorHandler();
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 0, &y, 0, NULL, 0 );
            ensure_equals(ls.getCoordinateDimension(), 2);
            ensure_equals(ls.getX(0), 1);
            ensure_equals(ls.getY(0), 2);
            ensure_equals(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 0, &y, 0, NULL, 0 );
            ensure_equals(ls.getCoordinateDimension(), 2);
            ensure_equals(ls.getX(0), 1);
            ensure_equals(ls.getY(0), 2);
            ensure_equals(ls.getZ(0), 0);
        }

        {
            OGRLineString ls;
            double x = 1, y = 2;
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, &x, 8, &y, 8, NULL, 0 );
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
            OGR_G_SetPoints( (OGRGeometryH)&ls, 1, NULL, 0, NULL, 0, NULL, 0 );
            CPLPopErrorHandler();
        }
    }

    template<>
    template<>
    void object::test<7>()
    {
      OGRStyleMgrH hSM = OGR_SM_Create(NULL);
      OGR_SM_InitStyleString(hSM, "PEN(w:2px,c:#000000,id:\"mapinfo-pen-2,ogr-pen-0\")");
      OGRStyleToolH hTool = OGR_SM_GetPart(hSM, 0, NULL);
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
        ensure(OGRParseDate("2017/11/31 12:34:56", &sField, 0));
        ensure_equals(sField.Date.Year, 2017);
        ensure_equals(sField.Date.Month, 11);
        ensure_equals(sField.Date.Day, 31);
        ensure_equals(sField.Date.Hour, 12);
        ensure_equals(sField.Date.Minute, 34);
        ensure_equals(sField.Date.Second, 56.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure(OGRParseDate("2017/11/31 12:34:56+00", &sField, 0));
        ensure_equals(sField.Date.TZFlag, 100);

        ensure(OGRParseDate("2017/11/31 12:34:56+12:00", &sField, 0));
        ensure_equals(sField.Date.TZFlag, 100 + 12 * 4);

        ensure(OGRParseDate("2017/11/31 12:34:56+1200", &sField, 0));
        ensure_equals(sField.Date.TZFlag, 100 + 12 * 4);

        ensure(OGRParseDate("2017/11/31 12:34:56+815", &sField, 0));
        ensure_equals(sField.Date.TZFlag, 100 + 8 * 4 + 1);

        ensure(OGRParseDate("2017/11/31 12:34:56-12:00", &sField, 0));
        ensure_equals(sField.Date.TZFlag, 100 - 12 * 4);

        ensure(OGRParseDate(" 2017/11/31 12:34:56", &sField, 0));
        ensure_equals(sField.Date.Year, 2017);

        ensure(OGRParseDate("2017/11/31 12:34:56.789", &sField, 0));
        ensure_equals(sField.Date.Second, 56.789f);

        ensure(OGRParseDate("2017-11-31T12:34:56", &sField, 0));
        ensure_equals(sField.Date.Year, 2017);
        ensure_equals(sField.Date.Month, 11);
        ensure_equals(sField.Date.Day, 31);
        ensure_equals(sField.Date.Hour, 12);
        ensure_equals(sField.Date.Minute, 34);
        ensure_equals(sField.Date.Second, 56.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure(OGRParseDate("2017-11-31T12:34:56Z", &sField, 0));
        ensure_equals(sField.Date.Second, 56.0f);
        ensure_equals(sField.Date.TZFlag, 100);

        ensure(OGRParseDate("2017-11-31T12:34:56.789Z", &sField, 0));
        ensure_equals(sField.Date.Second, 56.789f);
        ensure_equals(sField.Date.TZFlag, 100);

        ensure(OGRParseDate("2017-11-31", &sField, 0));
        ensure_equals(sField.Date.Year, 2017);
        ensure_equals(sField.Date.Month, 11);
        ensure_equals(sField.Date.Day, 31);
        ensure_equals(sField.Date.Hour, 0);
        ensure_equals(sField.Date.Minute, 0);
        ensure_equals(sField.Date.Second, 0.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure(OGRParseDate("12:34", &sField, 0));
        ensure_equals(sField.Date.Year, 0);
        ensure_equals(sField.Date.Month, 0);
        ensure_equals(sField.Date.Day, 0);
        ensure_equals(sField.Date.Hour, 12);
        ensure_equals(sField.Date.Minute, 34);
        ensure_equals(sField.Date.Second, 0.0f);
        ensure_equals(sField.Date.TZFlag, 0);

        ensure(OGRParseDate("12:34:56", &sField, 0));
        ensure(OGRParseDate("12:34:56.789", &sField, 0));

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
        ensure(!OGRParseDate("2017-01-01T00:00:62", &sField, 0));
        ensure(!OGRParseDate("2017-01-01T00:00:a", &sField, 0));
    }

} // namespace tut

///////////////////////////////////////////////////////////////////////////////
// $Id: test_ogr.cpp,v 1.4 2006/12/06 15:39:13 mloskot Exp $
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
//
//  $Log: test_ogr.cpp,v $
//  Revision 1.4  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////
#include <tut.h>
#include <ogrsf_frmts.h>
#include <string>

namespace tut
{

    // Common fixture with test data
    struct test_ogr_data
    {
        // Expected number of drivers
        OGRSFDriverRegistrar* drv_reg_;
        int drv_count_;
        std::string drv_shape_;
        bool has_geos_support_;

        test_ogr_data()
            : drv_reg_(NULL),
            drv_count_(0),
            drv_shape_("ESRI Shapefile")
        {
            drv_reg_ = OGRSFDriverRegistrar::GetRegistrar();

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
        ensure("OGRSFDriverRegistrar::GetRegistrar() is NULL", NULL != drv_reg_);
    }

    // Test number of registered OGR drivers
    template<>
    template<>
    void object::test<2>()
    {
        OGRSFDriverRegistrar* reg = NULL;
        reg = OGRSFDriverRegistrar::GetRegistrar();
        ensure(NULL != reg);

#ifdef WIN32CE
        // This is only restricted on WIN32CE. 
        ensure_equals("OGR registered drivers count doesn't match",
            reg->GetDriverCount(), drv_count_);
#endif
    }

    // Test if Shapefile driver is registered
    template<>
    template<>
    void object::test<3>()
    {
        OGRSFDriverRegistrar* reg = NULL;
        reg = OGRSFDriverRegistrar::GetRegistrar();
        ensure(NULL != reg);

        GDALDriver* drv = reg->GetDriverByName(drv_shape_.c_str());
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
            ensure("SRS reference count incremented by assignment operator", 4 == poSRS->GetReferenceCount());
            
        }
        ensure("GetReferenceCount expected to be decremented by destructions", 1 == poSRS->GetReferenceCount());
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
    
    template<class T>
    void testCopyEquals()
    {
        T* poOrigin = make<T>();
        ensure( NULL != poOrigin);
        
        T value2( *poOrigin );
        
        std::ostringstream strErrorCopy; 
        strErrorCopy << poOrigin->getGeometryName() << ": copy constructor changed a value";
        ensure(strErrorCopy.str().c_str(), poOrigin->Equals(&value2));
        
        T value3;
        value3 = *poOrigin;
        value3 = *poOrigin;
        
        std::ostringstream strErrorAssign; 
        strErrorAssign << poOrigin->getGeometryName() << ": assignment operator changed a value";
        ensure(strErrorAssign.str().c_str(), poOrigin->Equals(&value3));
        
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
        
    }
    

} // namespace tut

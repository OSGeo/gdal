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

        OGRSFDriver* drv = reg->GetDriverByName(drv_shape_.c_str());
        ensure("Shapefile driver is not registered", NULL != drv);
    }

    // Test OGRFieldDefn assignment operator and copy constructors
    template<>
    template<>
    void object::test<4>()
    {
        OGRFieldDefn a("foo", OFTInteger);
        a = a;
        {
            ensure(strcmp(a.GetNameRef(), "foo") == 0);
            ensure(a.GetType() == OFTInteger);
        }
        {
            a = OGRFieldDefn("baz", OFTReal);
        }
        {
            ensure(strcmp(a.GetNameRef(), "baz") == 0);
            ensure(a.GetType() == OFTReal);
        }
        {
            OGRFieldDefn b("bar", OFTString);
            a = b;
        }
        {
            ensure(strcmp(a.GetNameRef(), "bar") == 0);
            ensure(a.GetType() == OFTString);
        }
        {
            OGRFieldDefn c(a);
            ensure(strcmp(c.GetNameRef(), "bar") == 0);
            ensure(c.GetType() == OFTString);
        }
        {
            OGRFieldDefn d(&a);
            ensure(strcmp(d.GetNameRef(), "bar") == 0);
            ensure(d.GetType() == OFTString);
        }
    }

} // namespace tut

///////////////////////////////////////////////////////////////////////////////
// $Id: test_gdal.cpp,v 1.4 2006/12/06 15:39:13 mloskot Exp $
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general GDAL features.
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
//  $Log: test_gdal.cpp,v $
//  Revision 1.4  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////
#include <tut.h>
#include <gdal.h>
#include <gdal_priv.h>
#include <string>

namespace tut
{
    // Common fixture with test data
    struct test_gdal_data
    {
        // Expected number of drivers
        int drv_count_;

        test_gdal_data()
            : drv_count_(0)
        {
            // Windows CE port builds with fixed number of drivers
#ifdef FRMT_aaigrid
            drv_count_++;
#endif
#ifdef FRMT_dted
            drv_count_++;
#endif
#ifdef FRMT_gtiff
            drv_count_++;
#endif
        }
    };

    // Register test group
    typedef test_group<test_gdal_data> group;
    typedef group::object object;
    group test_gdal_group("GDAL");

    // Test GDAL driver manager access
    template<>
    template<>
    void object::test<1>()
    {
        GDALDriverManager* drv_mgr = NULL;
        drv_mgr = GetGDALDriverManager();
        ensure("GetGDALDriverManager() is NULL", NULL != drv_mgr);
    }

    // Test number of registered GDAL drivers
    template<>
    template<>
    void object::test<2>()
    {
#ifdef WIN32CE
        ensure_equals("GDAL registered drivers count doesn't match",
            GDALGetDriverCount(), drv_count_);
#endif
    }

    // Test if AAIGrid driver is registered
    template<>
    template<>
    void object::test<3>()
    {
        GDALDriverH drv = GDALGetDriverByName("AAIGrid");

#ifdef FRMT_aaigrid
        ensure("AAIGrid driver is not registered", NULL != drv);
#else
        ensure(true); // Skip
#endif
    }

    // Test if DTED driver is registered
    template<>
    template<>
    void object::test<4>()
    {
        GDALDriverH drv = GDALGetDriverByName("DTED");

#ifdef FRMT_dted
        ensure("DTED driver is not registered", NULL != drv);
#else
        ensure(true); // Skip
#endif
    }

    // Test if GeoTIFF driver is registered
    template<>
    template<>
    void object::test<5>()
    {
        GDALDriverH drv = GDALGetDriverByName("GTiff");

#ifdef FRMT_gtiff
        ensure("GTiff driver is not registered", NULL != drv);
#else
        ensure(true); // Skip
#endif
    }

} // namespace tut

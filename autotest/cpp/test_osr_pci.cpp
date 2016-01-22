///////////////////////////////////////////////////////////////////////////////
// $Id: test_osr_pci.cpp,v 1.2 2006/12/06 15:39:13 mloskot Exp $
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test some PCI specific translation issues.
//           Ported from osr/osr_pci.py.
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
//  $Log: test_osr_pci.cpp,v $
//  Revision 1.2  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////

// See Bronek Kozicki's comments posted here:
// http://lists.boost.org/Archives/boost/2005/07/89697.php
#pragma warning(disable: 4996)

#include <tut.h>
#include <tut_gdal.h>
#include <ogr_srs_api.h> // OSR
#include <ogr_api.h> // OGR
#include <cpl_error.h> // CPL
#include <cpl_string.h>
#include <algorithm>
#include <cmath>
#include <string>
    
namespace tut
{

    // Common fixture with test data
    struct test_osr_pci_data
    {
        OGRErr err_;
        OGRSpatialReferenceH srs_;

        test_osr_pci_data()
            : err_(OGRERR_NONE), srs_(NULL)
        {
            srs_ = OSRNewSpatialReference(NULL);
        }

        ~test_osr_pci_data()
        {
            OSRDestroySpatialReference(srs_);
        }
    };

    // Register test group
    typedef test_group<test_osr_pci_data> group;
    typedef group::object object;
    group test_osr_pci_group("OSR::PCI");

    // Test the OGRSpatialReference::importFromPCI() and OSRImportFromPCI()
    template<>
    template<>
    void object::test<1>()
    {
        ensure("SRS handle is NULL", NULL != srs_);

        const int size = 17;
        double params[size] = { 
            0.0, 0.0, 45.0, 54.5, 47.0, 62.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        };

        err_ = OSRImportFromPCI(srs_, "EC          E015", "METRE", params);
        ensure_equals("Can't import Equidistant Conic projection",
            err_, OGRERR_NONE);

        const double maxError = 0.0000005;
        double val = 0;
        
        val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_1, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);
        ensure("Standard parallel 1 is invalid",
               std::fabs(val - 47.0) <= maxError);

        val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_2, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);
        ensure("Standard parallel 2 is invalid",
               std::fabs(val - 62.0) <= maxError);

        val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_CENTER, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);
        ensure("Latitude of center is invalid",
               std::fabs(val - 54.5) <= maxError);

        val = OSRGetProjParm(srs_, SRS_PP_LONGITUDE_OF_CENTER, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);
        ensure("Longtitude of center is invalid",
               std::fabs(val - 45.0) <= maxError);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);
        ensure("False easting is invalid", std::fabs(val - 0.0) <= maxError);

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);
        ensure("False northing is invalid", std::fabs(val - 0.0) <= maxError);
    }

    // Test the OGRSpatialReference::exportToPCI() and OSRExportToPCI()
    template<>
    template<>
    void object::test<2>()
    {
        ensure("SRS handle is NULL", NULL != srs_);
        
        const char* wkt = "\"\"PROJCS[\"unnamed\",GEOGCS[\"NAD27\","
            "DATUM[\"North_American_Datum_1927\","
            "SPHEROID[\"Clarke 1866\",6378206.4,294.9786982139006,"
            "AUTHORITY[\"EPSG\",\"7008\"]],AUTHORITY[\"EPSG\",\"6267\"]],"
            "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433],"
            "AUTHORITY[\"EPSG\",\"4267\"]],PROJECTION[\"Lambert_Conformal_Conic_2SP\"],"
            "PARAMETER[\"standard_parallel_1\",33.90363402777778],"
            "PARAMETER[\"standard_parallel_2\",33.62529002777778],"
            "PARAMETER[\"latitude_of_origin\",33.76446202777777],"
            "PARAMETER[\"central_meridian\",-117.4745428888889],"
            "PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],"
            "UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]]]\"\"";

        err_ = OSRImportFromWkt(srs_, (char**) &wkt);
        ensure_equals("Can't import Lambert Conformal Conic projection",
            err_, OGRERR_NONE);

        char* proj = NULL;
        char* units = NULL;
        double* params = NULL;
        const int size = 17;

        err_ = OSRExportToPCI(srs_, &proj, &units, &params);
        ensure_equals("OSRExportToPCI() failed", err_, OGRERR_NONE);

        ensure_equals("Invalid projection definition",
            std::string(proj), std::string("LCC         D-01"));
        
        ensure_equals("Invalid projection units",
            std::string(units), std::string("METRE"));

        const double maxError = 0.0000005;

        ensure("Invalid 2nd projection parameter",
            std::fabs(params[2] - (-117.4745429)) <= maxError);

        ensure("Invalid 3rd projection parameter",
            std::fabs(params[3] - 33.76446203) <= maxError);

        ensure("Invalid 4th projection parameter",
            std::fabs(params[4] - 33.90363403) <= maxError);

        ensure("Invalid 5th projection parameter",
            std::fabs(params[5] - 33.62529003) <= maxError);

        CPLFree(proj);
        CPLFree(units);
        CPLFree(params);
    }

} // namespace tut

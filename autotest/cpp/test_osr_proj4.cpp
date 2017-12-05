///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test some PROJ.4 specific translation issues.
//           Ported from osr/osr_proj4.py.
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

#include <cpl_error.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace tut
{

    // Common fixture with test data
    struct test_osr_proj4_data
    {
        OGRErr err_;
        OGRSpatialReferenceH srs_;

        test_osr_proj4_data()
            : err_(OGRERR_NONE), srs_(NULL)
        {
            srs_ = OSRNewSpatialReference(NULL);
        }

        ~test_osr_proj4_data()
        {
            OSRDestroySpatialReference(srs_);
        }
    };

    // Register test group
    typedef test_group<test_osr_proj4_data> group;
    typedef group::object object;
    group test_osr_proj4_group("OSR::PROJ.4");

    // Test the +k_0 flag works as well as +k when
    // consuming PROJ.4 format
    template<>
    template<>
    void object::test<1>()
    {
        ensure("SRS handle is NULL", NULL != srs_);

        std::string wkt("+proj=tmerc +lat_0=53.5000000000 +lon_0=-8.0000000000"
              "+k_0=1.0000350000 +x_0=200000.0000000000 +y_0=250000.0000000000"
              "+a=6377340.189000 +rf=299.324965 +towgs84=482.530,"
              "-130.596,564.557,-1.042,-0.214,-0.631,8.15");

        err_ = OSRImportFromProj4(srs_, wkt.c_str());
        ensure_equals("OSRImportFromProj4)( failed", err_, OGRERR_NONE);

        // TODO: Check max error value
        const double maxError = 0.00005; // 0.0000005
        double val = 0;

        val = OSRGetProjParm(srs_, SRS_PP_SCALE_FACTOR, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);

        ensure("+k_0 not supported on import from PROJ.4",
               std::fabs(val - 1.000035) <= maxError);
    }

    // Verify that we can import strings with parameter values
    // that are exponents and contain a plus sign
    template<>
    template<>
    void object::test<2>()
    {
        ensure("SRS handle is NULL", NULL != srs_);

        std::string wkt("+proj=lcc +x_0=0.6096012192024384e+06 +y_0=0"
            "+lon_0=90dw +lat_0=42dn +lat_1=44d4'n +lat_2=42d44'n"
            "+a=6378206.400000 +rf=294.978698 +nadgrids=conus,ntv1_can.dat");

        err_ = OSRImportFromProj4(srs_, wkt.c_str());
        ensure_equals("OSRImportFromProj4)( failed", err_, OGRERR_NONE);

        const double maxError = 0.0005;
        double val = 0;

        val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
        ensure_equals("OSRGetProjParm() failed", err_, OGRERR_NONE);
        ensure("Parsing exponents not supported",
               std::fabs(val - 609601.219) <= maxError);
    }

} // namespace tut

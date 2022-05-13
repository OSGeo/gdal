///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test some PCI specific translation issues.
//           Ported from osr/osr_pci.py.
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

#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"

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
            : err_(OGRERR_NONE), srs_(nullptr)
        {
            srs_ = OSRNewSpatialReference(nullptr);
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
        ensure("SRS handle is NULL", nullptr != srs_);

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
        ensure("Longitude of center is invalid",
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
        ensure("SRS handle is NULL", nullptr != srs_);

        const char* wkt = "PROJCS[\"unnamed\",GEOGCS[\"NAD27\","
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
            "UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]]]";

        err_ = OSRImportFromWkt(srs_, (char**) &wkt);
        ensure_equals("Can't import Lambert Conformal Conic projection",
            err_, OGRERR_NONE);

        char* proj = nullptr;
        char* units = nullptr;
        double* params = nullptr;

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

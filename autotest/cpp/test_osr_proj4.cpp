///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test some PROJ.4 specific translation issues.
//           Ported from osr/osr_proj4.py.
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
#include "ogr_api.h"
#include "ogr_srs_api.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_osr_proj4 : public ::testing::Test
{
    OGRErr err_ = OGRERR_NONE;
    OGRSpatialReferenceH srs_ = nullptr;

    void SetUp() override
    {
        srs_ = OSRNewSpatialReference(nullptr);
        ASSERT_TRUE(nullptr != srs_);
        OSRSetAxisMappingStrategy(srs_, OAMS_TRADITIONAL_GIS_ORDER);
    }

    void TearDown() override
    {
        OSRDestroySpatialReference(srs_);
        srs_ = nullptr;
    }
};

// Test the +k_0 flag works as well as +k when
// consuming PROJ.4 format
TEST_F(test_osr_proj4, k_0)
{
    std::string wkt(
        "+proj=tmerc +lat_0=53.5000000000 +lon_0=-8.0000000000 "
        "+k_0=1.0000350000 +x_0=200000.0000000000 +y_0=250000.0000000000 "
        "+a=6377340.189000 +rf=299.324965 +towgs84=482.530,"
        "-130.596,564.557,-1.042,-0.214,-0.631,8.15");

    err_ = OSRImportFromProj4(srs_, wkt.c_str());
    ASSERT_EQ(err_, OGRERR_NONE);

    // TODO: Check max error value
    const double maxError = 0.00005;  // 0.0000005
    double val = 0;

    val = OSRGetProjParm(srs_, SRS_PP_SCALE_FACTOR, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);

    EXPECT_NEAR(val, 1.000035, maxError);
}

// Verify that we can import strings with parameter values
// that are exponents and contain a plus sign
TEST_F(test_osr_proj4, proj_strings_with_exponents)
{
    std::string wkt(
        "+proj=lcc +x_0=0.6096012192024384e+06 +y_0=0 "
        "+lon_0=90dw +lat_0=42dn +lat_1=44d4'n +lat_2=42d44'n "
        "+a=6378206.400000 +rf=294.978698 +nadgrids=conus,ntv1_can.dat");

    err_ = OSRImportFromProj4(srs_, wkt.c_str());
    ASSERT_EQ(err_, OGRERR_NONE);

    const double maxError = 0.0005;
    double val = 0;

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);

    EXPECT_NEAR(val, 609601.219, maxError);
}

}  // namespace

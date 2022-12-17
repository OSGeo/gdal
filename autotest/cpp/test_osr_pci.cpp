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

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_osr_pci : public ::testing::Test
{
    OGRErr err_ = OGRERR_NONE;
    OGRSpatialReferenceH srs_ = nullptr;

    void SetUp() override
    {
        srs_ = OSRNewSpatialReference(nullptr);
        ASSERT_TRUE(nullptr != srs_);
    }

    void TearDown() override
    {
        OSRDestroySpatialReference(srs_);
        srs_ = nullptr;
    }
};

// Test the OGRSpatialReference::importFromPCI() and OSRImportFromPCI()
TEST_F(test_osr_pci, importFromPCI)
{
    const int size = 17;
    double params[size] = {0.0, 0.0, 45.0, 54.5, 47.0, 62.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0,  0.0,  0.0,  0.0,  0.0, 0.0};

    err_ = OSRImportFromPCI(srs_, "EC          E015", "METRE", params);
    ASSERT_EQ(err_, OGRERR_NONE);

    const double maxError = 0.0000005;
    double val = 0;

    val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_1, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);
    EXPECT_NEAR(val, 47.0, maxError);

    val = OSRGetProjParm(srs_, SRS_PP_STANDARD_PARALLEL_2, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);
    EXPECT_NEAR(val, 62.0, maxError);

    val = OSRGetProjParm(srs_, SRS_PP_LATITUDE_OF_CENTER, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);
    EXPECT_NEAR(val, 54.5, maxError);

    val = OSRGetProjParm(srs_, SRS_PP_LONGITUDE_OF_CENTER, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);
    EXPECT_NEAR(val, 45.0, maxError);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_EASTING, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);
    EXPECT_NEAR(val, 0.0, maxError);

    val = OSRGetProjParm(srs_, SRS_PP_FALSE_NORTHING, -1111, &err_);
    ASSERT_EQ(err_, OGRERR_NONE);
    EXPECT_NEAR(val, 0.0, maxError);
}

// Test the OGRSpatialReference::exportToPCI() and OSRExportToPCI()
TEST_F(test_osr_pci, exportToPCI)
{
    const char *wkt =
        "PROJCS[\"unnamed\",GEOGCS[\"NAD27\","
        "DATUM[\"North_American_Datum_1927\","
        "SPHEROID[\"Clarke 1866\",6378206.4,294.9786982139006,"
        "AUTHORITY[\"EPSG\",\"7008\"]],AUTHORITY[\"EPSG\",\"6267\"]],"
        "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433],"
        "AUTHORITY[\"EPSG\",\"4267\"]],PROJECTION[\"Lambert_Conformal_Conic_"
        "2SP\"],"
        "PARAMETER[\"standard_parallel_1\",33.90363402777778],"
        "PARAMETER[\"standard_parallel_2\",33.62529002777778],"
        "PARAMETER[\"latitude_of_origin\",33.76446202777777],"
        "PARAMETER[\"central_meridian\",-117.4745428888889],"
        "PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],"
        "UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]]]";

    err_ = OSRImportFromWkt(srs_, (char **)&wkt);
    ASSERT_EQ(err_, OGRERR_NONE);

    char *proj = nullptr;
    char *units = nullptr;
    double *params = nullptr;

    err_ = OSRExportToPCI(srs_, &proj, &units, &params);
    EXPECT_EQ(err_, OGRERR_NONE);

    EXPECT_STREQ(proj, "LCC         D-01");

    EXPECT_STREQ(units, "METRE");

    const double maxError = 0.0000005;

    EXPECT_NEAR(params[2], -117.47454290, maxError);

    EXPECT_NEAR(params[3], 33.76446203, maxError);

    EXPECT_NEAR(params[4], 33.90363403, maxError);

    EXPECT_NEAR(params[5], 33.62529003, maxError);

    CPLFree(proj);
    CPLFree(units);
    CPLFree(params);
}

}  // namespace

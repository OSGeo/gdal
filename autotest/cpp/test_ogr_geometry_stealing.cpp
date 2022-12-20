/******************************************************************************
 *
 * Project:  C++ Test Suite for GDAL/OGR
 * Purpose:  Test geometry stealing from OGR feature
 * Author:   mathieu17g (https://github.com/mathieu17g/)
 *
 ******************************************************************************
 * Copyright (C) 2022 mathieu17g
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************/

#include "gdal_unit_test.h"
#include "gdal.h"
#include "ogr_api.h"

#include "gtest_include.h"

namespace
{

// Test data
struct test_ogr_geometry_stealing : public ::testing::Test
{
    GDALDatasetH hDS = nullptr;
    OGRLayerH hLayer = nullptr;

    test_ogr_geometry_stealing()
    {
        // Build data file path name
        std::string test_data_file_name(tut::common::data_basedir);
        test_data_file_name += SEP;
        test_data_file_name += "multi_geom.csv";

        // Open data file with multi geometries feature layer
        const char *const papszOpenOptions[] = {
            "AUTODETECT_TYPE=YES", "GEOM_POSSIBLE_NAMES=point,linestring",
            "KEEP_GEOM_COLUMNS=NO", nullptr};
        hDS = GDALOpenEx(test_data_file_name.c_str(), GDAL_OF_VECTOR, nullptr,
                         papszOpenOptions, nullptr);
        if (hDS == nullptr)
        {
            printf("Can't open layer file %s.\n", test_data_file_name.c_str());
            return;
        }
        hLayer = GDALDatasetGetLayer(hDS, 0);
        if (hLayer == nullptr)
        {
            printf("Can't get layer in file %s.\n",
                   test_data_file_name.c_str());
            return;
        }
    }

    ~test_ogr_geometry_stealing()
    {
        GDALClose(hDS);
    }

    void SetUp() override
    {
        if (hLayer == nullptr)
            GTEST_SKIP() << "Cannot open source file";
    }
};

// Test 1st geometry stealing from a multigeom csv file
TEST_F(test_ogr_geometry_stealing, first_geometry)
{
    OGRFeatureH hFeature = OGR_L_GetNextFeature(hLayer);
    OGRGeometryH hGeometryOrig = OGR_G_Clone(OGR_F_GetGeometryRef(hFeature));
    OGRGeometryH hGeometryStolen = OGR_F_StealGeometry(hFeature);
    ASSERT_TRUE(hGeometryOrig);
    ASSERT_TRUE(hGeometryStolen);
    ASSERT_TRUE(OGR_G_Equals(hGeometryOrig, hGeometryStolen));
    ASSERT_TRUE(OGR_F_GetGeometryRef(hFeature) == nullptr);
    OGR_G_DestroyGeometry(hGeometryOrig);
    OGR_G_DestroyGeometry(hGeometryStolen);
    OGR_F_Destroy(hFeature);
}

// Test 2nd geometry stealing from a multigeom csv file
TEST_F(test_ogr_geometry_stealing, second_geometry)
{
    OGRFeatureH hFeature = OGR_L_GetNextFeature(hLayer);
    OGRGeometryH hGeometryOrig =
        OGR_G_Clone(OGR_F_GetGeomFieldRef(hFeature, 1));
    OGRGeometryH hGeometryStolen = OGR_F_StealGeometryEx(hFeature, 1);
    ASSERT_TRUE(hGeometryOrig);
    ASSERT_TRUE(hGeometryStolen);
    ASSERT_TRUE(OGR_G_Equals(hGeometryOrig, hGeometryStolen));
    ASSERT_TRUE(OGR_F_GetGeomFieldRef(hFeature, 1) == nullptr);
    OGR_G_DestroyGeometry(hGeometryOrig);
    OGR_G_DestroyGeometry(hGeometryStolen);
    OGR_F_Destroy(hFeature);
}

}  // namespace

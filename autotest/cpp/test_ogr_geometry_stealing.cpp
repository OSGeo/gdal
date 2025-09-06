/******************************************************************************
 *
 * Project:  C++ Test Suite for GDAL/OGR
 * Purpose:  Test geometry stealing from OGR feature
 * Author:   mathieu17g (https://github.com/mathieu17g/)
 *
 ******************************************************************************
 * Copyright (C) 2022 mathieu17g
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gdal_unit_test.h"
#include "gdal.h"
#include "ogr_api.h"

#include "gtest_include.h"

namespace
{

// Test data
struct test_ogr_geometry_stealing /* non final */ : public ::testing::Test
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

    ~test_ogr_geometry_stealing() override
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

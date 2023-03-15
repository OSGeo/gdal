///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Shapefile driver testing. Ported from ogr/ogr_shape.py.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
// Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_api.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "gtest_include.h"

namespace
{
using namespace tut;  // for CheckEqualGeometries

// Test data
struct test_ogr_shape : public ::testing::Test
{
    OGRSFDriverH drv_ = nullptr;
    const char *drv_name_ = "ESRI Shapefile";
    std::string data_{};
    std::string data_tmp_{};
    const char *test_name_ = nullptr;

    test_ogr_shape()
    {
        drv_ = OGRGetDriverByName(drv_name_);

        // Compose data path for test group
        data_ = tut::common::data_basedir;
        data_tmp_ = tut::common::tmp_basedir;
    }

    void TearDown() override
    {
        OGRDataSourceH ds =
            OGR_Dr_CreateDataSource(drv_, data_tmp_.c_str(), nullptr);
        if (ds == nullptr)
        {
            return;
        }

        if (test_name_)
        {
            const int nlyr = OGR_DS_GetLayerCount(ds);
            for (int i = 0; i < nlyr; i++)
            {
                OGRLayerH lyr = OGR_DS_GetLayer(ds, i);
                if (EQUAL(OGR_L_GetName(lyr), test_name_))
                {
                    OGR_DS_DeleteLayer(ds, i);
                }
            }
        }
    }

    void SetUp() override
    {
        if (drv_ == nullptr)
        {
            GTEST_SKIP() << "ESRI Shapefile driver missing";
            return;
        }

        test_name_ =
            ::testing::UnitTest::GetInstance()->current_test_info()->name();

        OGRErr err = OGRERR_NONE;

        OGRDataSourceH ds = nullptr;
        ds = OGR_Dr_CreateDataSource(drv_, data_tmp_.c_str(), nullptr);
        ASSERT_TRUE(nullptr != ds);

        // Create memory Layer
        OGRLayerH lyr = nullptr;
        lyr = OGR_DS_CreateLayer(ds, test_name_, nullptr, wkbPolygon, nullptr);
        EXPECT_TRUE(nullptr != lyr);
        if (lyr == nullptr)
        {
            OGR_DS_Destroy(ds);
            return;
        }

        // Create schema
        OGRFieldDefnH fld = nullptr;

        fld = OGR_Fld_Create("AREA", OFTReal);
        err = OGR_L_CreateField(lyr, fld, true);
        OGR_Fld_Destroy(fld);
        EXPECT_EQ(OGRERR_NONE, err);

        fld = OGR_Fld_Create("EAS_ID", OFTInteger);
        err = OGR_L_CreateField(lyr, fld, true);
        OGR_Fld_Destroy(fld);
        EXPECT_EQ(OGRERR_NONE, err);

        fld = OGR_Fld_Create("PRFEDEA", OFTString);
        err = OGR_L_CreateField(lyr, fld, true);
        OGR_Fld_Destroy(fld);
        EXPECT_EQ(OGRERR_NONE, err);

        // Check schema
        OGRFeatureDefnH featDefn = OGR_L_GetLayerDefn(lyr);
        ASSERT_TRUE(nullptr != featDefn);
        EXPECT_EQ(3, OGR_FD_GetFieldCount(featDefn));

        // Copy ogr/poly.shp to temporary layer
        OGRFeatureH featDst = OGR_F_Create(featDefn);
        EXPECT_TRUE(nullptr != featDst);
        if (featDst)
        {
            std::string source(data_);
            source += SEP;
            source += "poly.shp";
            OGRDataSourceH dsSrc = OGR_Dr_Open(drv_, source.c_str(), false);
            EXPECT_TRUE(nullptr != dsSrc);
            if (dsSrc)
            {
                OGRLayerH lyrSrc = OGR_DS_GetLayer(dsSrc, 0);
                EXPECT_TRUE(nullptr != lyrSrc);
                if (lyrSrc)
                {
                    OGRFeatureH featSrc = nullptr;
                    while (nullptr != (featSrc = OGR_L_GetNextFeature(lyrSrc)))
                    {
                        err = OGR_F_SetFrom(featDst, featSrc, true);
                        EXPECT_EQ(OGRERR_NONE, err);

                        err = OGR_L_CreateFeature(lyr, featDst);
                        EXPECT_EQ(OGRERR_NONE, err);

                        OGR_F_Destroy(featSrc);
                    }
                }
                // Release and close resources

                OGR_DS_Destroy(dsSrc);
            }
            OGR_F_Destroy(featDst);
        }
        OGR_DS_Destroy(ds);
    }
};

//
// Template of attribute reading function and its specializations
//
template <typename T> inline void read_feature_attribute(OGRFeatureH, int, T &)
{
    assert(!"Can't find read_feature_attribute specialization for given type");
}

template <>
inline void read_feature_attribute(OGRFeatureH feature, int index, int &val)
{
    val = OGR_F_GetFieldAsInteger(feature, index);
}

#ifdef unused
template <>
inline void read_feature_attribute(OGRFeatureH feature, int index, double &val)
{
    val = OGR_F_GetFieldAsDouble(feature, index);
}
#endif

template <>
inline void read_feature_attribute(OGRFeatureH feature, int index,
                                   std::string &val)
{
    val = OGR_F_GetFieldAsString(feature, index);
}

//
// Test layer attributes from given field against expected list of values
//
template <typename T>
::testing::AssertionResult
CheckEqualAttributes(OGRLayerH layer, std::string const &field, T const &list)
{
    // Test raw pointers
    if (nullptr == layer)
    {
        return ::testing::AssertionFailure() << "Layer is NULL";
    }

    OGRFeatureDefnH featDefn = OGR_L_GetLayerDefn(layer);
    if (nullptr == featDefn)
    {
        return ::testing::AssertionFailure() << "Layer schema is NULL";
    }

    int fldIndex = OGR_FD_GetFieldIndex(featDefn, field.c_str());
    if (fldIndex < 0)
    {
        return ::testing::AssertionFailure() << "Can't find field " << field;
    }

    // Test value in tested field from subsequent features
    OGRFeatureH feat = nullptr;
    OGRFieldDefnH fldDefn = nullptr;
    typename T::value_type attrVal;

    for (const auto &attr : list)
    {
        feat = OGR_L_GetNextFeature(layer);

        fldDefn = OGR_F_GetFieldDefnRef(feat, fldIndex);
        if (nullptr == fldDefn)
        {
            return ::testing::AssertionFailure() << "Field schema is NULL";
        }

        read_feature_attribute(feat, fldIndex, attrVal);

        OGR_F_Destroy(feat);

        // Test attribute against expected value
        if (attr != attrVal)
        {
            return ::testing::AssertionFailure()
                   << "Attributes not equal. Expected " << attr << ", got "
                   << attrVal;
        }
    }

    // Check if not too many features filtered
    feat = OGR_L_GetNextFeature(layer);
    OGR_F_Destroy(feat);

    if (nullptr != feat)
    {
        return ::testing::AssertionFailure()
               << "Got more features than expected";
    }
    return ::testing::AssertionSuccess();
}

// Test Create/Destroy empty directory datasource
TEST_F(test_ogr_shape, create)
{
    // Try to remove tmp and ignore error code
    OGR_Dr_DeleteDataSource(drv_, data_tmp_.c_str());

    OGRDataSourceH ds = nullptr;
    ds = OGR_Dr_CreateDataSource(drv_, data_tmp_.c_str(), nullptr);
    ASSERT_TRUE(nullptr != ds);

    OGR_DS_Destroy(ds);
}

// Test attributes written to new table
TEST_F(test_ogr_shape, attributes)
{
    OGRErr err = OGRERR_NONE;
    const int size = 5;
    const int expect[size] = {168, 169, 166, 158, 165};

    std::string source(data_tmp_);
    source += SEP;
    source += test_name_;
    source += ".shp";
    OGRDataSourceH ds = OGR_Dr_Open(drv_, source.c_str(), false);
    ASSERT_TRUE(nullptr != ds);

    OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
    EXPECT_TRUE(nullptr != lyr);
    if (lyr)
    {
        err = OGR_L_SetAttributeFilter(lyr, "eas_id < 170");
        EXPECT_EQ(OGRERR_NONE, err);

        // Prepare tester collection
        std::vector<int> list;
        std::copy(expect, expect + size, std::back_inserter(list));

        EXPECT_TRUE(CheckEqualAttributes(lyr, "eas_id", list));
    }
    OGR_DS_Destroy(ds);
}

// Test geometries written to new shapefile
TEST_F(test_ogr_shape, geometries)
{
    // Original shapefile
    std::string orig(data_);
    orig += SEP;
    orig += "poly.shp";
    OGRDataSourceH dsOrig = OGR_Dr_Open(drv_, orig.c_str(), false);
    ASSERT_TRUE(nullptr != dsOrig);

    OGRLayerH lyrOrig = OGR_DS_GetLayer(dsOrig, 0);
    EXPECT_TRUE(nullptr != lyrOrig);
    if (lyrOrig)
    {
        // Copied shapefile
        std::string tmp(data_tmp_);
        tmp += SEP;
        tmp += test_name_;
        tmp += ".shp";
        OGRDataSourceH dsTmp = OGR_Dr_Open(drv_, tmp.c_str(), false);
        EXPECT_TRUE(nullptr != dsTmp);

        OGRLayerH lyrTmp = OGR_DS_GetLayer(dsTmp, 0);
        EXPECT_TRUE(nullptr != lyrTmp);
        if (lyrTmp)
        {
            // Iterate through features and compare geometries
            OGRFeatureH featOrig = OGR_L_GetNextFeature(lyrOrig);
            OGRFeatureH featTmp = OGR_L_GetNextFeature(lyrTmp);

            while (nullptr != featOrig && nullptr != featTmp)
            {
                OGRGeometryH lhs = OGR_F_GetGeometryRef(featOrig);
                OGRGeometryH rhs = OGR_F_GetGeometryRef(featTmp);

                EXPECT_TRUE(CheckEqualGeometries(lhs, rhs, 0.000000001));

                // TODO: add ensure_equal_attributes()

                OGR_F_Destroy(featOrig);
                OGR_F_Destroy(featTmp);

                // Move to next feature
                featOrig = OGR_L_GetNextFeature(lyrOrig);
                featTmp = OGR_L_GetNextFeature(lyrTmp);
            }
        }
        OGR_DS_Destroy(dsTmp);
    }

    OGR_DS_Destroy(dsOrig);
}

// Write a feature without a geometry, then read it back
TEST_F(test_ogr_shape, no_geometry)
{
    // Create feature without geometry
    std::string tmp(data_tmp_);
    tmp += SEP;
    tmp += test_name_;
    tmp += ".shp";

    // Write the feature
    {
        OGRDataSourceH ds = OGR_Dr_Open(drv_, tmp.c_str(), true);
        ASSERT_TRUE(nullptr != ds);

        OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
        EXPECT_TRUE(nullptr != lyr);
        if (lyr != nullptr)
        {
            OGRFeatureDefnH featDefn = OGR_L_GetLayerDefn(lyr);
            EXPECT_TRUE(nullptr != featDefn);

            OGRFeatureH featNonSpatial = OGR_F_Create(featDefn);
            EXPECT_TRUE(nullptr != featNonSpatial);
            if (featDefn && featNonSpatial)
            {
                int fldIndex = OGR_FD_GetFieldIndex(featDefn, "PRFEDEA");
                EXPECT_TRUE(fldIndex >= 0);
                if (fldIndex >= 0)
                {
                    OGR_F_SetFieldString(featNonSpatial, fldIndex, "nulled");

                    OGRErr err = OGR_L_CreateFeature(lyr, featNonSpatial);
                    EXPECT_EQ(OGRERR_NONE, err);
                }
            }

            OGR_F_Destroy(featNonSpatial);
        }
        OGR_DS_Destroy(ds);
    }

    // Read back the non-spatial feature and get the geometry
    {
        OGRDataSourceH ds = OGR_Dr_Open(drv_, tmp.c_str(), false);
        ASSERT_TRUE(nullptr != ds);

        OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
        EXPECT_TRUE(nullptr != lyr);
        if (lyr != nullptr)
        {
            OGRErr err = OGR_L_SetAttributeFilter(lyr, "PRFEDEA = 'nulled'");
            EXPECT_EQ(OGRERR_NONE, err);

            // Fetch feature without geometry
            OGRFeatureH featNonSpatial = OGR_L_GetNextFeature(lyr);
            EXPECT_TRUE(nullptr != featNonSpatial);
            if (featNonSpatial != nullptr)
            {
                // Null geometry is expected
                OGRGeometryH nonGeom = OGR_F_GetGeometryRef(featNonSpatial);
                EXPECT_TRUE(nullptr == nonGeom);

                OGR_F_Destroy(featNonSpatial);
            }
        }
        OGR_DS_Destroy(ds);
    }
}

// Test ExecuteSQL() results layers without geometry
TEST_F(test_ogr_shape, ExecuteSQL_no_geometry)
{
    const int size = 10;
    const int expect[size] = {179, 173, 172, 171, 170, 169, 168, 166, 165, 158};

    // Open directory as a datasource
    OGRDataSourceH ds = OGR_Dr_Open(drv_, data_.c_str(), false);
    ASSERT_TRUE(nullptr != ds);

    std::string sql("select distinct eas_id from poly order by eas_id desc");
    OGRLayerH lyr = OGR_DS_ExecuteSQL(ds, sql.c_str(), nullptr, nullptr);
    EXPECT_TRUE(nullptr != lyr);
    if (lyr)
    {
        // Prepare tester collection
        std::vector<int> list;
        std::copy(expect, expect + size, std::back_inserter(list));

        EXPECT_TRUE(CheckEqualAttributes(lyr, "eas_id", list));

        OGR_DS_ReleaseResultSet(ds, lyr);
    }
    OGR_DS_Destroy(ds);
}

// Test ExecuteSQL() results layers with geometry
TEST_F(test_ogr_shape, ExecuteSQL_geometry)
{
    // Open directory as a datasource
    OGRDataSourceH ds = OGR_Dr_Open(drv_, data_.c_str(), false);
    ASSERT_TRUE(nullptr != ds);

    std::string sql("select * from poly where prfedea = '35043413'");
    OGRLayerH lyr = OGR_DS_ExecuteSQL(ds, sql.c_str(), nullptr, nullptr);
    EXPECT_TRUE(nullptr != lyr);
    if (lyr)
    {
        // Prepare tester collection
        std::vector<std::string> list;
        list.push_back("35043413");

        // Test attributes
        EXPECT_TRUE(CheckEqualAttributes(lyr, "prfedea", list));

        // Test geometry
        const char *wkt =
            "POLYGON ((479750.688 4764702.000,479658.594 4764670.000,"
            "479640.094 4764721.000,479735.906 4764752.000,"
            "479750.688 4764702.000))";

        OGRGeometryH testGeom = nullptr;
        OGRErr err = OGR_G_CreateFromWkt((char **)&wkt, nullptr, &testGeom);
        EXPECT_EQ(OGRERR_NONE, err);
        if (testGeom)
        {
            OGR_L_ResetReading(lyr);
            OGRFeatureH feat = OGR_L_GetNextFeature(lyr);
            EXPECT_TRUE(nullptr != feat);
            if (feat)
            {
                EXPECT_TRUE(CheckEqualGeometries(OGR_F_GetGeometryRef(feat),
                                                 testGeom, 0.001));
                OGR_F_Destroy(feat);
            }
            OGR_G_DestroyGeometry(testGeom);
        }
        OGR_DS_ReleaseResultSet(ds, lyr);
    }
    OGR_DS_Destroy(ds);
}

// Test spatial filtering
TEST_F(test_ogr_shape, spatial_filtering)
{
    OGRErr err = OGRERR_NONE;

    // Read feature without geometry
    std::string tmp(data_tmp_);
    tmp += SEP;
    tmp += "poly.shp";
    OGRDataSourceH ds = OGR_Dr_Open(drv_, data_.c_str(), false);
    ASSERT_TRUE(nullptr != ds);

    OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
    EXPECT_TRUE(nullptr != lyr);
    if (lyr)
    {
        // Set empty filter for attributes
        err = OGR_L_SetAttributeFilter(lyr, nullptr);
        EXPECT_EQ(OGRERR_NONE, err);

        // Set spatial filter
        const char *wkt = "LINESTRING(479505 4763195,480526 4762819)";
        OGRGeometryH filterGeom = nullptr;
        err = OGR_G_CreateFromWkt((char **)&wkt, nullptr, &filterGeom);
        ASSERT_EQ(OGRERR_NONE, err);
        if (filterGeom)
        {
            OGR_L_SetSpatialFilter(lyr, filterGeom);

            // Prepare tester collection
            std::vector<int> list;
            list.push_back(158);

            // Test attributes
            EXPECT_TRUE(CheckEqualAttributes(lyr, "eas_id", list));

            OGR_G_DestroyGeometry(filterGeom);
        }
    }
    OGR_DS_Destroy(ds);
}

TEST(test_ogr_shape_gdal, create)
{
    GDALDriver *shpDriver =
        GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    GDALDataset *pShpDst =
        shpDriver->Create("/vsimem/test.shp", 0, 0, 0, GDT_Unknown, nullptr);
    EXPECT_EQ(pShpDst->GetAccess(), GA_Update);
    GDALClose(pShpDst);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// $Id: test_ogr_shape.cpp,v 1.3 2006/12/06 15:39:13 mloskot Exp $
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Shapefile driver testing. Ported from ogr/ogr_shape.py.
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
//  $Log: test_ogr_shape.cpp,v $
//  Revision 1.3  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////
#include <tut.h>
#include <tut_gdal.h>
#include <gdal_common.h>
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace tut
{

    // Test data
    struct test_shape_data
    {
        OGRSFDriverH drv_;
        std::string drv_name_;
        std::string data_;
        std::string data_tmp_;

        test_shape_data()
            : drv_(NULL), drv_name_("ESRI Shapefile")
        {
            drv_ = OGRGetDriverByName(drv_name_.c_str());

            // Compose data path for test group
            data_ = tut::common::data_basedir;
            data_tmp_ = tut::common::tmp_basedir;
        }
    };

    // Register test group
    typedef test_group<test_shape_data> group;
    typedef group::object object;
    group test_shape_group("OGR::Shape");

    // Test driver availability
    template<>
    template<>
    void object::test<1>()
    {
        ensure("OGR::Shape driver not available", NULL != drv_);
    }

    // Test Create/Destroy empty directory datasource
    template<>
    template<>
    void object::test<2>()
    {
        // Try to remove tmp and ignore error code
        OGR_Dr_DeleteDataSource(drv_, data_tmp_.c_str());

        OGRDataSourceH ds = NULL;
        ds = OGR_Dr_CreateDataSource(drv_, data_tmp_.c_str(), NULL);
        ensure("OGR_Dr_CreateDataSource return NULL", NULL != ds);

        OGR_DS_Destroy(ds);
    }

    // Create table from ogr/poly.shp
    template<>
    template<>
    void object::test<3>()
    {
        OGRErr err = OGRERR_NONE;

        OGRDataSourceH ds = NULL;
        ds = OGR_Dr_CreateDataSource(drv_, data_tmp_.c_str(), NULL);
        ensure("Can't open or create data source", NULL != ds);

        // Create memory Layer
        OGRLayerH lyr = NULL;
        lyr = OGR_DS_CreateLayer(ds, "tpoly", NULL, wkbPolygon, NULL);
        ensure("Can't create layer", NULL != lyr);

        // Create schema
        OGRFieldDefnH fld = NULL;

        fld = OGR_Fld_Create("AREA", OFTReal);
        err = OGR_L_CreateField(lyr, fld, true);
        OGR_Fld_Destroy(fld);
        ensure_equals("Can't create field", OGRERR_NONE, err);

        fld = OGR_Fld_Create("EAS_ID", OFTInteger);
        err = OGR_L_CreateField(lyr, fld, true);
        OGR_Fld_Destroy(fld);
        ensure_equals("Can't create field", OGRERR_NONE, err);

        fld = OGR_Fld_Create("PRFEDEA", OFTString);
        err = OGR_L_CreateField(lyr, fld, true);
        OGR_Fld_Destroy(fld);
        ensure_equals("Can't create field", OGRERR_NONE, err);

        // Check schema
        OGRFeatureDefnH featDefn = OGR_L_GetLayerDefn(lyr);
        ensure("Layer schema is NULL", NULL != featDefn);
        ensure_equals("Fields creation failed", 3, OGR_FD_GetFieldCount(featDefn));

        // Copy ogr/poly.shp to temporary layer
        OGRFeatureH featDst = OGR_F_Create(featDefn);
        ensure("Can't create empty feature", NULL != featDst);

        std::string source(data_);
        source += SEP;
        source += "poly.shp";
        OGRDataSourceH dsSrc = OGR_Dr_Open(drv_, source.c_str(), false);
        ensure("Can't open source layer", NULL != dsSrc);

        OGRLayerH lyrSrc = OGR_DS_GetLayer(dsSrc, 0);
        ensure("Can't get source layer", NULL != lyrSrc);

        OGRFeatureH featSrc = NULL;
        while (NULL != (featSrc = OGR_L_GetNextFeature(lyrSrc)))
        {
            err = OGR_F_SetFrom(featDst, featSrc, true);
            ensure_equals("Can't set festure from source", OGRERR_NONE, err);

            err = OGR_L_CreateFeature(lyr, featDst);
            ensure_equals("Can't write feature to layer", OGRERR_NONE, err);

            OGR_F_Destroy(featSrc);
        }

        // Release and close resources
        OGR_F_Destroy(featDst);
        OGR_DS_Destroy(dsSrc);
        OGR_DS_Destroy(ds);
    }

    // Test attributes written to new table
    template<>
    template<>
    void object::test<4>()
    {
        OGRErr err = OGRERR_NONE;
        const int size = 5; 
        const int expect[size] = { 168, 169, 166, 158, 165 };

        std::string source(data_tmp_);
        source += SEP;
        source += "tpoly.shp";
        OGRDataSourceH ds = OGR_Dr_Open(drv_, source.c_str(), false);
        ensure("Can't open layer", NULL != ds);

        OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
        ensure("Can't get source layer", NULL != lyr);

        err = OGR_L_SetAttributeFilter(lyr, "eas_id < 170");
        ensure_equals("Can't set attribute filter", OGRERR_NONE, err);

        // Prepare tester collection
        std::vector<int> list;
        std::copy(expect, expect + size, std::back_inserter(list));

        ensure_equal_attributes(lyr, "eas_id", list);

        OGR_DS_Destroy(ds);
    }

    // Test geometries written to new shapefile
    template<>
    template<>
    void object::test<5>()
    {
        // Original shapefile
        std::string orig(data_);
        orig += SEP;
        orig += "poly.shp";
        OGRDataSourceH dsOrig = OGR_Dr_Open(drv_, orig.c_str(), false);
        ensure("Can't open layer", NULL != dsOrig);

        OGRLayerH lyrOrig = OGR_DS_GetLayer(dsOrig, 0);
        ensure("Can't get layer", NULL != lyrOrig);

        // Copied shapefile
        std::string tmp(data_tmp_);
        tmp += SEP;
        tmp += "tpoly.shp";
        OGRDataSourceH dsTmp = OGR_Dr_Open(drv_, tmp.c_str(), false);
        ensure("Can't open layer", NULL != dsTmp);

        OGRLayerH lyrTmp = OGR_DS_GetLayer(dsTmp, 0);
        ensure("Can't get layer", NULL != lyrTmp);

        // Iterate through features and compare geometries
        OGRFeatureH featOrig = OGR_L_GetNextFeature(lyrOrig);
        OGRFeatureH featTmp = OGR_L_GetNextFeature(lyrTmp);

        while (NULL != featOrig && NULL != featTmp)
        {
            OGRGeometryH lhs = OGR_F_GetGeometryRef(featOrig);
            OGRGeometryH rhs = OGR_F_GetGeometryRef(featTmp);

            ensure_equal_geometries(lhs, rhs, 0.000000001);

            // TODO: add ensure_equal_attributes()

            OGR_F_Destroy(featOrig);
            OGR_F_Destroy(featTmp);

            // Move to next feature
            featOrig = OGR_L_GetNextFeature(lyrOrig);
            featTmp = OGR_L_GetNextFeature(lyrTmp);
        }

        OGR_DS_Destroy(dsOrig);
        OGR_DS_Destroy(dsTmp);
    }

    // Write a feature without a geometry
    template<>
    template<>
    void object::test<6>()
    {
        // Create feature without geometry
        std::string tmp(data_tmp_);
        tmp += SEP;
        tmp += "tpoly.shp";
        OGRDataSourceH ds = OGR_Dr_Open(drv_, tmp.c_str(), true);
        ensure("Can't open layer", NULL != ds);

        OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
        ensure("Can't get layer", NULL != lyr);

        OGRFeatureDefnH featDefn = OGR_L_GetLayerDefn(lyr);
        ensure("Layer schema is NULL", NULL != featDefn);

        OGRFeatureH featNonSpatial = OGR_F_Create(featDefn);
        ensure("Can't create non-spatial feature", NULL != featNonSpatial);

        int fldIndex = OGR_FD_GetFieldIndex(featDefn, "PRFEDEA");
        ensure("Can't find field 'PRFEDEA'", fldIndex >= 0);

        OGR_F_SetFieldString(featNonSpatial, fldIndex, "nulled");
       
        OGRErr err = OGR_L_CreateFeature(lyr, featNonSpatial);
        ensure_equals("Can't write non-spatial feature to layer", OGRERR_NONE, err);

        OGR_F_Destroy(featNonSpatial);
        OGR_DS_Destroy(ds);
    }

    // Read back the non-spatial feature and get the geometry
    template<>
    template<>
    void object::test<7>()
    {
        OGRErr err = OGRERR_NONE;

        // Read feature without geometry
        std::string tmp(data_tmp_);
        tmp += SEP;
        tmp += "tpoly.shp";
        OGRDataSourceH ds = OGR_Dr_Open(drv_, tmp.c_str(), false);
        ensure("Can't open layer", NULL != ds);

        OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
        ensure("Can't get layer", NULL != lyr);

        err = OGR_L_SetAttributeFilter(lyr, "PRFEDEA = 'nulled'");
        ensure_equals("Can't set attribute filter", OGRERR_NONE, err);

        // Fetch feature without geometry
        OGRFeatureH featNonSpatial = OGR_L_GetNextFeature(lyr);
        ensure("Didnt get feature with null geometry back", NULL != featNonSpatial);

        // Null geometry is expected
        OGRGeometryH nonGeom = OGR_F_GetGeometryRef(featNonSpatial);
        ensure("Didnt get null geometry as expected", NULL == nonGeom);

        OGR_F_Destroy(featNonSpatial);
        OGR_DS_Destroy(ds);
    }

    // Test ExecuteSQL() results layers without geometry
    template<>
    template<>
    void object::test<8>()
    {
        const int size = 11;
        const int expect[size] = { 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, 0 };

        // Open directory as a datasource
        OGRDataSourceH ds = OGR_Dr_Open(drv_, data_tmp_ .c_str(), false);
        ensure("Can't open datasource", NULL != ds);

        std::string sql("select distinct eas_id from tpoly order by eas_id desc");
        OGRLayerH lyr = OGR_DS_ExecuteSQL(ds, sql.c_str(), NULL, NULL);
        ensure("Can't create layer from query", NULL != lyr);

        // Prepare tester collection
        std::vector<int> list;
        std::copy(expect, expect + size, std::back_inserter(list));

        ensure_equal_attributes(lyr, "eas_id", list);

        OGR_DS_ReleaseResultSet(ds, lyr);
        OGR_DS_Destroy(ds);
    }

    // Test ExecuteSQL() results layers with geometry
    template<>
    template<>
    void object::test<9>()
    {
        // Open directory as a datasource
        OGRDataSourceH ds = OGR_Dr_Open(drv_, data_tmp_ .c_str(), false);
        ensure("Can't open datasource", NULL != ds);

        std::string sql("select * from tpoly where prfedea = '35043413'");
        OGRLayerH lyr = OGR_DS_ExecuteSQL(ds, sql.c_str(), NULL, NULL);
        ensure("Can't create layer from query", NULL != lyr);

        // Prepare tester collection
        std::vector<std::string> list;
        list.push_back("35043413");
       
        // Test attributes
        ensure_equal_attributes(lyr, "prfedea", list);

        // Test geometry
        const char* wkt = "POLYGON ((479750.688 4764702.000,479658.594 4764670.000,"
            "479640.094 4764721.000,479735.906 4764752.000,"
            "479750.688 4764702.000))";

        OGRGeometryH testGeom = NULL;
        OGRErr err = OGR_G_CreateFromWkt((char**) &wkt, NULL, &testGeom);
        ensure_equals("Can't create geometry from WKT", OGRERR_NONE, err);

        OGR_L_ResetReading(lyr);
        OGRFeatureH feat = OGR_L_GetNextFeature(lyr);
        ensure("Can't featch feature", NULL != feat);

        ensure_equal_geometries(OGR_F_GetGeometryRef(feat), testGeom, 0.001);

        OGR_F_Destroy(feat);
        OGR_G_DestroyGeometry(testGeom);
        OGR_DS_ReleaseResultSet(ds, lyr);
        OGR_DS_Destroy(ds);
    }

    // Test spatial filtering
    template<>
    template<>
    void object::test<10>()
    {
        OGRErr err = OGRERR_NONE;

        // Read feature without geometry
        std::string tmp(data_tmp_);
        tmp += SEP;
        tmp += "tpoly.shp";
        OGRDataSourceH ds = OGR_Dr_Open(drv_, tmp.c_str(), false);
        ensure("Can't open layer", NULL != ds);

        OGRLayerH lyr = OGR_DS_GetLayer(ds, 0);
        ensure("Can't get layer", NULL != lyr);

        // Set empty filter for attributes
        err = OGR_L_SetAttributeFilter(lyr, NULL);
        ensure_equals("Can't set attribute filter", OGRERR_NONE, err);

        // Set spatial filter
        const char* wkt = "LINESTRING(479505 4763195,480526 4762819)";
        OGRGeometryH filterGeom = NULL;
        err = OGR_G_CreateFromWkt((char**) &wkt, NULL, &filterGeom);
        ensure_equals("Can't create geometry from WKT", OGRERR_NONE, err);

        OGR_L_SetSpatialFilter(lyr, filterGeom);

        // Prepare tester collection
        std::vector<int> list;
        list.push_back(158);
        list.push_back(0);
       
        // Test attributes
        ensure_equal_attributes(lyr, "eas_id", list);

        OGR_G_DestroyGeometry(filterGeom);
        OGR_DS_Destroy(ds);
    }

} // namespace tut

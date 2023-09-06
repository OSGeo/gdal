#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Memory driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################


import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture()
def mem_ds(request, poly_feat):

    ds = ogr.GetDriverByName("Memory").CreateDataSource(request.node.name)

    assert (
        ds.TestCapability(ogr.ODsCCreateLayer) != 0
    ), "ODsCCreateLayer TestCapability failed."

    #######################################################
    # Create memory Layer
    lyr = ds.CreateLayer("tpoly")

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString),
            ("WHEN", ogr.OFTDateTime),
        ],
    )

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    for feat in poly_feat:

        dst_feat.SetFrom(feat)
        ret = lyr.CreateFeature(dst_feat)
        assert ret == 0, "CreateFeature() failed."

    return ds


###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_mem_3(mem_ds, poly_feat):

    expect = [168, 169, 166, 158, 165]

    mem_lyr = mem_ds.GetLayer(0)

    with ogrtest.attribute_filter(mem_lyr, "eas_id < 170"):
        ogrtest.check_features_against_list(mem_lyr, "eas_id", expect)

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        read_feat = mem_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            read_feat, orig_feat.GetGeometryRef(), max_error=0.000000001
        )

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), (
                "Attribute %d does not match" % fld
            )


###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def test_ogr_mem_4(mem_ds):

    mem_lyr = mem_ds.GetLayer(0)

    dst_feat = ogr.Feature(feature_def=mem_lyr.GetLayerDefn())
    wkt_list = ["10", "2", "1", "3d_1", "4", "5", "6"]

    for item in wkt_list:

        wkt = open("data/wkb_wkt/" + item + ".wkt").read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new memory feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField("PRFEDEA", item)
        mem_lyr.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        mem_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = mem_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, geom)


###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_mem_5(mem_ds):

    mem_lyr = mem_ds.GetLayer(0)

    # add two empty features to test DISTINCT
    for _ in range(2):
        dst_feat = ogr.Feature(feature_def=mem_lyr.GetLayerDefn())
        mem_lyr.CreateFeature(dst_feat)

    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]

    with mem_ds.ExecuteSQL(
        "select distinct eas_id from tpoly order by eas_id desc"
    ) as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "eas_id", expect)


###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_mem_6(mem_ds):

    with mem_ds.ExecuteSQL("select * from tpoly where prfedea = '35043415'") as sql_lyr:

        ogrtest.check_features_against_list(sql_lyr, "prfedea", ["35043415"])

        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            feat_read,
            "POLYGON ((479029.71875 4765110.5,479046.53125 4765117.0,479123.28125 4765015.0,479196.8125 4764879.0,479117.8125 4764847.0,479029.71875 4765110.5))",
        )


###############################################################################
# Test spatial filtering.


def test_ogr_mem_7(mem_ds):

    mem_lyr = mem_ds.GetLayer(0)

    with ogrtest.spatial_filter(mem_lyr, "LINESTRING(479505 4763195,480526 4762819)"):

        assert not mem_lyr.TestCapability(
            ogr.OLCFastSpatialFilter
        ), "OLCFastSpatialFilter capability test should have failed."

        ogrtest.check_features_against_list(mem_lyr, "eas_id", [158])


###############################################################################
# Test adding a new field.


def test_ogr_mem_8(mem_ds):

    mem_lyr = mem_ds.GetLayer(0)

    ####################################################################
    # Add new string field.

    field_defn = ogr.FieldDefn("new_string", ogr.OFTString)
    mem_lyr.CreateField(field_defn)

    ####################################################################
    # Apply a value to this field in one feature.

    mem_lyr.SetAttributeFilter("PRFEDEA = '35043416'")
    feat_read = mem_lyr.GetNextFeature()
    feat_read.SetField("new_string", "test1")
    mem_lyr.SetFeature(feat_read)

    # Test expected failed case of SetFeature()
    new_feat = ogr.Feature(mem_lyr.GetLayerDefn())
    new_feat.SetFID(-2)
    with gdal.quiet_errors():
        ret = mem_lyr.SetFeature(new_feat)
    assert ret != 0
    new_feat = None

    ####################################################################
    # Now fetch two features and verify the new column works OK.

    with ogrtest.attribute_filter(mem_lyr, "PRFEDEA IN ( '35043415', '35043416' )"):

        ogrtest.check_features_against_list(mem_lyr, "new_string", ["test1", None])


###############################################################################
# Test deleting a feature.


def test_ogr_mem_9(mem_ds):

    mem_lyr = mem_ds.GetLayer(0)

    assert mem_lyr.TestCapability(
        ogr.OLCDeleteFeature
    ), "OLCDeleteFeature capability test failed."

    assert mem_lyr.TestCapability(
        ogr.OLCFastFeatureCount
    ), "OLCFastFeatureCount capability test failed."

    old_count = mem_lyr.GetFeatureCount()

    ####################################################################
    # Delete target feature.

    target_fid = 2
    assert mem_lyr.DeleteFeature(target_fid) == 0, "DeleteFeature returned error code."

    assert (
        mem_lyr.DeleteFeature(target_fid) != 0
    ), "DeleteFeature should have returned error code."

    ####################################################################
    # Verify that count has dropped by one, and that the feature in question
    # can't be fetched.
    new_count = mem_lyr.GetFeatureCount()
    assert new_count == (old_count - 1), "unexpected feature count"

    assert mem_lyr.TestCapability(
        ogr.OLCRandomRead
    ), "OLCRandomRead capability test failed."

    assert mem_lyr.GetFeature(target_fid) is None, "Got deleted feature!"

    assert mem_lyr.GetFeature(-1) is None, "GetFeature() should have failed"

    assert mem_lyr.GetFeature(1000) is None, "GetFeature() should have failed"


###############################################################################
# Test GetDriver() / name bug (#1674)
#
# Mostly we are verifying that this doesn't still cause a crash.


def test_ogr_mem_10():

    d = ogr.GetDriverByName("Memory")
    ds = d.CreateDataSource("xxxxxx")

    d2 = ds.GetDriver()
    assert (
        d2 is not None and d2.GetName() == "Memory"
    ), "Did not get expected driver name."


###############################################################################
# Verify that we can delete layers properly


def test_ogr_mem_11(mem_ds):

    assert (
        mem_ds.TestCapability("DeleteLayer") != 0
    ), "Deletelayer TestCapability failed."

    mem_ds.CreateLayer("extra")
    mem_ds.CreateLayer("extra2")
    layer_count = mem_ds.GetLayerCount()

    gdaltest.mem_lyr = None
    # Delete extra layer
    assert mem_ds.DeleteLayer(layer_count - 2) == 0, "DeleteLayer() failed"

    assert mem_ds.DeleteLayer(-1) != 0, "DeleteLayer() should have failed"

    assert (
        mem_ds.DeleteLayer(mem_ds.GetLayerCount()) != 0
    ), "DeleteLayer() should have failed"

    assert mem_ds.GetLayer(-1) is None, "GetLayer() should have failed"

    assert (
        mem_ds.GetLayer(mem_ds.GetLayerCount()) is None
    ), "GetLayer() should have failed"

    lyr = mem_ds.GetLayer(mem_ds.GetLayerCount() - 1)

    assert lyr.GetName() == "extra2", "delete layer seems iffy"


###############################################################################
# Test some date handling


def test_ogr_mem_12(mem_ds):

    #######################################################
    # Create memory Layer
    lyr = mem_ds.GetLayerByName("tpoly")
    assert lyr is not None

    # Set the date of the first feature
    f = lyr.GetFeature(1)
    f.SetField("WHEN", 2008, 3, 19, 16, 15, 00, 0)
    lyr.SetFeature(f)
    f = lyr.GetFeature(1)
    idx = f.GetFieldIndex("WHEN")
    expected = [2008, 3, 19, 16, 15, 0.0, 0]
    result = f.GetFieldAsDateTime(idx)
    for i, value in enumerate(result):
        assert value == expected[i], "%s != %s" % (result, expected)


###############################################################################
# Test Get/Set on StringList, IntegerList, RealList


def test_ogr_mem_13(mem_ds):

    lyr = mem_ds.CreateLayer("listlayer")
    field_defn = ogr.FieldDefn("stringlist", ogr.OFTStringList)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("intlist", ogr.OFTIntegerList)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("reallist", ogr.OFTRealList)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    try:
        feat.SetFieldStringList
    except AttributeError:
        # OG python bindings
        pytest.skip()

    feat.SetFieldStringList(0, ["a", "b"])
    assert feat.GetFieldAsStringList(0) == ["a", "b"]

    feat.SetFieldIntegerList(1, [2, 3])
    assert feat.GetFieldAsIntegerList(1) == [2, 3]

    feat.SetFieldDoubleList(2, [4.0, 5.0])
    assert feat.GetFieldAsDoubleList(2) == [4.0, 5.0]


###############################################################################
# Test SetNextByIndex


def test_ogr_mem_14(mem_ds):

    lyr = mem_ds.CreateLayer("SetNextByIndex")
    field_defn = ogr.FieldDefn("foo", ogr.OFTString)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetField(0, "first feature")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetField(0, "second feature")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetField(0, "third feature")
    lyr.CreateFeature(feat)

    assert lyr.TestCapability(
        ogr.OLCFastSetNextByIndex
    ), "OLCFastSetNextByIndex capability test failed."

    assert lyr.SetNextByIndex(1) == 0, "SetNextByIndex() failed"
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString(0) == "second feature", "did not get expected feature"

    assert lyr.SetNextByIndex(-1) != 0, "SetNextByIndex() should have failed"

    assert lyr.SetNextByIndex(100) != 0, "SetNextByIndex() should have failed"

    lyr.SetAttributeFilter("foo != 'second feature'")

    assert not lyr.TestCapability(
        ogr.OLCFastSetNextByIndex
    ), "OLCFastSetNextByIndex capability test should have failed."

    assert lyr.SetNextByIndex(1) == 0, "SetNextByIndex() failed"
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString(0) == "third feature", "did not get expected feature"


###############################################################################
# Test non-linear geometries


def test_ogr_mem_15(mem_ds):

    lyr = mem_ds.CreateLayer("wkbCircularString", geom_type=ogr.wkbCircularString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CIRCULARSTRING(0 0,1 0,0 0)"))
    lyr.CreateFeature(f)
    f = None

    assert lyr.GetGeomType() == ogr.wkbCircularString
    assert lyr.GetLayerDefn().GetGeomType() == ogr.wkbCircularString
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbCircularString
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbCircularString

    # Test SetNonLinearGeometriesEnabledFlag(False)
    old_val = ogr.GetNonLinearGeometriesEnabledFlag()
    ogr.SetNonLinearGeometriesEnabledFlag(False)
    try:
        assert lyr.GetGeomType() == ogr.wkbLineString
        assert lyr.GetLayerDefn().GetGeomType() == ogr.wkbLineString
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbLineString
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        assert g.GetGeometryType() == ogr.wkbLineString

        lyr.ResetReading()
        f = lyr.GetNextFeature()
        g = f.GetGeomFieldRef(0)
        assert g.GetGeometryType() == ogr.wkbLineString
    finally:
        ogr.SetNonLinearGeometriesEnabledFlag(old_val)


###############################################################################
# Test map implementation


def test_ogr_mem_16(mem_ds):

    lyr = mem_ds.CreateLayer("ogr_mem_16")
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 1

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(100000000)
    ret = lyr.CreateFeature(f)
    assert ret == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(100000000)
    ret = lyr.SetFeature(f)
    assert ret == 0

    assert lyr.GetFeatureCount() == 3

    assert lyr.GetFeature(0) is not None
    assert lyr.GetFeature(1) is not None
    assert lyr.GetFeature(2) is None
    assert lyr.GetFeature(100000000) is not None

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    f = lyr.GetNextFeature()
    assert f.GetFID() == 100000000
    f = lyr.GetNextFeature()
    assert f is None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(100000000)
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 2

    assert lyr.GetFeatureCount() == 4

    ret = lyr.DeleteFeature(1)
    assert ret == 0

    assert lyr.GetFeatureCount() == 3

    ret = lyr.DeleteFeature(1)
    assert ret != 0

    assert lyr.GetFeatureCount() == 3

    # Test first feature with huge ID
    lyr = mem_ds.CreateLayer("ogr_mem_16_bis")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1234567890123)
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 1234567890123
    f = None  # Important we must not have dangling references before modifying the schema !

    # Create a field so as to test OGRMemLayerIteratorMap
    lyr.CreateField(ogr.FieldDefn("foo"))


###############################################################################
# Test Dataset.GetNextFeature() implementation


def test_ogr_mem_17():

    ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("ogr_mem_1")
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer("ogr_mem_2")
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer("ogr_mem_3")
    # Empty layer

    lyr = ds.CreateLayer("ogr_mem_4")
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == "ogr_mem_1"

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == "ogr_mem_1"

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == "ogr_mem_2"

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == "ogr_mem_4"

    f, lyr = ds.GetNextFeature()
    assert f is None and lyr is None

    f, lyr = ds.GetNextFeature()
    assert f is None and lyr is None

    ds.ResetReading()

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == "ogr_mem_1"

    ds.ResetReading()

    f, lyr, pct = ds.GetNextFeature(include_pct=True)
    assert f is not None and lyr.GetName() == "ogr_mem_1" and pct == 0.25

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is not None and pct == 0.50

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is not None and pct == 0.75

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is not None and pct == 1.0

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is None and pct == 1.0

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is None and pct == 1.0

    ds.ResetReading()

    f = ds.GetNextFeature(include_layer=False)
    assert f is not None


###############################################################################


def test_ogr_mem_coordinate_epoch():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(2021.3)

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("foo", srs=srs)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4326"
    assert srs.GetCoordinateEpoch() == 2021.3
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


###############################################################################


def test_ogr_mem_alter_geom_field_defn():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    assert lyr.TestCapability(ogr.OLCAlterGeomFieldDefn)

    new_geom_field_defn = ogr.GeomFieldDefn("my_name", ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4269)
    new_geom_field_defn.SetSpatialRef(srs)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef().IsSame(srs)

    srs.SetCoordinateEpoch(2022)
    new_geom_field_defn.SetSpatialRef(srs)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 2022

    srs.SetCoordinateEpoch(0)
    new_geom_field_defn.SetSpatialRef(srs)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 0

    srs.SetCoordinateEpoch(2022)
    new_geom_field_defn.SetSpatialRef(srs)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 2022

    new_geom_field_defn.SetSpatialRef(None)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 2022

    new_geom_field_defn.SetSpatialRef(None)
    assert (
        lyr.AlterGeomFieldDefn(
            0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG
        )
        == ogr.OGRERR_NONE
    )
    assert lyr.GetSpatialRef() is None


###############################################################################


def test_ogr_mem_arrow_stream_numpy():
    pytest.importorskip("osgeo.gdal_array")
    numpy = pytest.importorskip("numpy")
    import datetime

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    stream = lyr.GetArrowStreamAsNumPy()

    with pytest.raises(Exception):
        with gdal.quiet_errors():
            lyr.GetArrowStreamAsNumPy()

    it = iter(stream)
    with pytest.raises(StopIteration):
        next(it)

    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 1
    assert batches[0].keys() == {"OGC_FID", "wkb_geometry"}
    assert batches[0]["OGC_FID"][0] == 0
    assert batches[0]["wkb_geometry"][0] is None

    field = ogr.FieldDefn("str", ogr.OFTString)
    lyr.CreateField(field)

    field = ogr.FieldDefn("bool", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int16", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int32", ogr.OFTInteger)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int64", ogr.OFTInteger64)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float32", ogr.OFTReal)
    field.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float64", ogr.OFTReal)
    lyr.CreateField(field)

    field = ogr.FieldDefn("date", ogr.OFTDate)
    lyr.CreateField(field)

    field = ogr.FieldDefn("time", ogr.OFTTime)
    lyr.CreateField(field)

    field = ogr.FieldDefn("datetime", ogr.OFTDateTime)
    lyr.CreateField(field)

    field = ogr.FieldDefn("binary", ogr.OFTBinary)
    lyr.CreateField(field)

    field = ogr.FieldDefn("strlist", ogr.OFTStringList)
    lyr.CreateField(field)

    field = ogr.FieldDefn("boollist", ogr.OFTIntegerList)
    field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int16list", ogr.OFTIntegerList)
    field.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int32list", ogr.OFTIntegerList)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int64list", ogr.OFTInteger64List)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float32list", ogr.OFTRealList)
    field.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float64list", ogr.OFTRealList)
    lyr.CreateField(field)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("bool", 1)
    f.SetField("int16", -12345)
    f.SetField("int32", 12345678)
    f.SetField("int64", 12345678901234)
    f.SetField("float32", 1.25)
    f.SetField("float64", 1.250123)
    f.SetField("str", "abc")
    f.SetField("date", "2022-05-31")
    f.SetField("time", "12:34:56.789")
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    f.SetField("boollist", "[False,True]")
    f.SetField("int16list", "[-12345,12345]")
    f.SetField("int32list", "[-12345678,12345678]")
    f.SetField("int64list", "[-12345678901234,12345678901234]")
    f.SetField("float32list", "[-1.25,1.25]")
    f.SetField("float64list", "[-1.250123,1.250123]")
    f.SetField("strlist", '["abc","defghi"]')
    f.SetField("binary", b"\xDE\xAD")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert batch.keys() == {
        "OGC_FID",
        "str",
        "bool",
        "int16",
        "int32",
        "int64",
        "float32",
        "float64",
        "date",
        "time",
        "datetime",
        "binary",
        "strlist",
        "boollist",
        "int16list",
        "int32list",
        "int64list",
        "float32list",
        "float64list",
        "wkb_geometry",
    }
    assert batch["OGC_FID"][1] == 1
    for fieldname in ("bool", "int16", "int32", "int64", "float32", "float64"):
        assert batch[fieldname][1] == f.GetField(fieldname)
    assert batch["str"][1] == f.GetField("str").encode("utf-8")
    assert batch["date"][1] == numpy.datetime64("2022-05-31")
    assert batch["time"][1] == datetime.time(12, 34, 56, 789000)
    assert batch["datetime"][1] == numpy.datetime64("2022-05-31T12:34:56.789")
    assert numpy.array_equal(batch["boollist"][1], numpy.array([False, True]))
    assert numpy.array_equal(batch["int16list"][1], numpy.array([-12345, 12345]))
    assert numpy.array_equal(batch["int32list"][1], numpy.array([-12345678, 12345678]))
    assert numpy.array_equal(
        batch["int64list"][1], numpy.array([-12345678901234, 12345678901234])
    )
    assert numpy.array_equal(batch["float32list"][1], numpy.array([-1.25, 1.25]))
    assert numpy.array_equal(
        batch["float64list"][1], numpy.array([-1.250123, 1.250123])
    )
    assert numpy.array_equal(
        batch["strlist"][1], numpy.array([b"abc", b"defghi"], dtype="|S6")
    )
    assert batch["binary"][1] == b"\xDE\xAD"
    assert len(batch["wkb_geometry"][1]) == 21


###############################################################################
# Test optimization to save memory on string fields with huge strings compared
# to the average size


def test_ogr_mem_arrow_stream_numpy_huge_string():
    pytest.importorskip("osgeo.gdal_array")
    pytest.importorskip("numpy")

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    field = ogr.FieldDefn("str", ogr.OFTString)
    lyr.CreateField(field)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "X")
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull("str")
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str", "Y" * (1000 * 1000))
    lyr.CreateFeature(f)

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert str(batch["str"].dtype) == "object"
    assert isinstance(batch["str"][0], str)
    assert [x for x in batch["str"]] == ["X", None, "Y" * (1000 * 1000)]


###############################################################################


def test_ogr_mem_arrow_stream_pyarrow():
    pytest.importorskip("pyarrow")

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    stream = lyr.GetArrowStreamAsPyArrow()

    with pytest.raises(Exception):
        with gdal.quiet_errors():
            lyr.GetArrowStreamAsPyArrow()

    it = iter(stream)
    with pytest.raises(StopIteration):
        next(it)

    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    stream = lyr.GetArrowStreamAsPyArrow()
    assert str(stream.schema) == "struct<OGC_FID: int64 not null, wkb_geometry: binary>"
    md = stream.schema["wkb_geometry"].metadata
    assert b"ARROW:extension:name" in md
    assert md[b"ARROW:extension:name"] == b"ogc.wkb"
    batches = [batch for batch in stream]
    assert len(batches) == 1
    arrays = batches[0].flatten()
    assert len(arrays) == 2


###############################################################################
# Test upserting a feature.


def test_ogr_mem_upsert_feature(mem_ds):
    # Create a memory layer
    lyr = mem_ds.CreateLayer("upsert_feature")

    # Add a string field
    lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))

    # Create a feature with some data
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("test", "original")
    assert lyr.CreateFeature(f) == 0
    assert f.GetFID() == 0

    # Upsert an existing feature
    assert lyr.TestCapability(ogr.OLCUpsertFeature) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(0)
    f.SetField("test", "updated")
    assert lyr.UpsertFeature(f) == 0

    # Verify that we have set an existing feature
    f = lyr.GetFeature(0)
    assert f is not None
    assert f.GetField("test") == "updated"

    # Upsert a new feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    assert lyr.UpsertFeature(f) == 0

    # Verify that we have created a feature
    assert lyr.GetFeature(1) is not None


###############################################################################
# Test updating a feature.


def test_ogr_mem_update_feature(mem_ds):
    # Create a memory layer
    lyr = mem_ds.CreateLayer("update_feature")

    # Add a string field
    lyr.CreateField(ogr.FieldDefn("test", ogr.OFTString))

    # Create a feature with some data
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("test", "original")
    assert lyr.CreateFeature(f) == 0
    assert f.GetFID() == 0

    # Update an existing feature
    assert lyr.TestCapability(ogr.OLCUpdateFeature) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(0)
    f.SetField("test", "updated")
    assert (
        lyr.UpdateFeature(f, [lyr.GetLayerDefn().GetFieldIndex("test")], [], False)
        == ogr.OGRERR_NONE
    )

    # Verify that we have set an existing feature
    f = lyr.GetFeature(0)
    assert f is not None
    assert f.GetField("test") == "updated"

    # Test updating a unset field
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(0)
    assert (
        lyr.UpdateFeature(f, [lyr.GetLayerDefn().GetFieldIndex("test")], [], False)
        == ogr.OGRERR_NONE
    )

    f = lyr.GetFeature(0)
    assert not f.IsFieldSet("test")

    # Update a feature without fid
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("test", "updated")
    assert (
        lyr.UpdateFeature(f, [lyr.GetLayerDefn().GetFieldIndex("test")], [], False)
        == ogr.OGRERR_NON_EXISTING_FEATURE
    )

    # Update a non-existing feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    f.SetField("test", "updated")
    assert (
        lyr.UpdateFeature(f, [lyr.GetLayerDefn().GetFieldIndex("test")], [], False)
        == ogr.OGRERR_NON_EXISTING_FEATURE
    )


###############################################################################


def test_ogr_mem_get_supported_srs_list():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    assert lyr.GetSupportedSRSList() is None
    assert lyr.SetActiveSRS(0, None) != ogr.OGRERR_NONE

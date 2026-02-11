#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Memory driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import json

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

    ds = ogr.GetDriverByName("MEM").CreateDataSource(request.node.name)

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
    assert lyr.GetNextFeature() is None

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

    ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
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

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo", srs=srs)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4326"
    assert srs.GetCoordinateEpoch() == 2021.3
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


###############################################################################


def test_ogr_mem_alter_geom_field_defn():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
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
# Test ogr.Layer.__arrow_c_stream__() interface.
# Cf https://arrow.apache.org/docs/format/CDataInterface/PyCapsuleInterface.html


@gdaltest.enable_exceptions()
def test_ogr_mem_arrow_stream_pycapsule_interface():
    import ctypes

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo")

    stream = lyr.__arrow_c_stream__()
    assert stream
    t = type(stream)
    assert t.__module__ == "builtins"
    assert t.__name__ == "PyCapsule"
    capsule_get_name = ctypes.pythonapi.PyCapsule_GetName
    capsule_get_name.argtypes = [ctypes.py_object]
    capsule_get_name.restype = ctypes.c_char_p
    assert capsule_get_name(ctypes.py_object(stream)) == b"arrow_array_stream"

    with pytest.raises(
        Exception, match="An arrow Arrow Stream is in progress on that layer"
    ):
        lyr.__arrow_c_stream__()

    del stream

    stream = lyr.__arrow_c_stream__()
    assert stream
    del stream

    with pytest.raises(Exception, match="requested_schema != None not implemented"):
        # "something" should rather by a PyCapsule with an ArrowSchema...
        lyr.__arrow_c_stream__(requested_schema="something")

    # Also test GetArrowArrayStreamInterface() to be able to specify options
    stream = lyr.GetArrowArrayStreamInterface(
        {"INCLUDE_FID": "NO"}
    ).__arrow_c_stream__()
    assert stream
    t = type(stream)
    assert t.__module__ == "builtins"
    assert t.__name__ == "PyCapsule"
    del stream


###############################################################################
# Test consuming __arrow_c_stream__() interface.
# Cf https://arrow.apache.org/docs/format/CDataInterface/PyCapsuleInterface.html


@gdaltest.enable_exceptions()
def test_ogr_mem_consume_arrow_stream_pycapsule_interface():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo", geom_type=ogr.wkbNone)
    lyr.CreateGeomField(ogr.GeomFieldDefn("my_geometry"))
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    lyr2 = ds.CreateLayer("foo2")
    lyr2.WriteArrow(lyr)

    f = lyr2.GetNextFeature()
    assert f["foo"] == "bar"
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"


###############################################################################
# Test consuming __arrow_c_array__() interface.
# Cf https://arrow.apache.org/docs/format/CDataInterface/PyCapsuleInterface.html


@gdaltest.enable_exceptions()
def test_ogr_mem_consume_arrow_array_pycapsule_interface():
    pyarrow = pytest.importorskip("pyarrow")
    if int(pyarrow.__version__.split(".")[0]) < 14:
        pytest.skip("pyarrow >= 14 needed")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    table = pyarrow.table(lyr)

    lyr2 = ds.CreateLayer("foo2")
    batches = table.to_batches()
    for batch in batches:
        array = batch.to_struct_array()
        if not hasattr(array, "__arrow_c_array__"):
            pytest.skip("table does not declare __arrow_c_array__")

        lyr2.WriteArrow(array)

    f = lyr2.GetNextFeature()
    assert f["foo"] == "bar"
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"


###############################################################################


def test_ogr_mem_arrow_stream_numpy():
    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")
    import datetime

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
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
    f.SetField("date", "1899-05-31")
    f.SetField("time", "12:34:56.789")
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    f.SetField("boollist", "[False,True]")
    f.SetField("int16list", "[-12345,12345]")
    f.SetField("int32list", "[-12345678,12345678]")
    f.SetField("int64list", "[-12345678901234,12345678901234]")
    f.SetField("float32list", "[-1.25,1.25]")
    f.SetField("float64list", "[-1.250123,1.250123]")
    f.SetField("strlist", '["abc","defghi"]')
    f.SetField("binary", b"\xde\xad")
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
    assert batch["date"][1] == numpy.datetime64("1899-05-31")
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
    assert batch["binary"][1] == b"\xde\xad"
    assert len(batch["wkb_geometry"][1]) == 21

    # Test fast FID filtering
    lyr.SetAttributeFilter("FID IN (1, -2, 1, 0)")
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetAttributeFilter(None)
    assert len(batches) == 1
    batch = batches[0]
    assert len(batch["OGC_FID"]) == 2
    assert batch["OGC_FID"][0] == 1
    assert batch["OGC_FID"][1] == 0

    lyr.SetAttributeFilter("FID = 2")
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    lyr.SetAttributeFilter(None)
    assert len(batches) == 0


###############################################################################
# Test DATETIME_AS_STRING=YES GetArrowStream() option


def test_ogr_mem_arrow_stream_numpy_datetime_as_string():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo")

    field = ogr.FieldDefn("datetime", ogr.OFTDateTime)
    lyr.CreateField(field)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56")
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("datetime", "2022-05-31T12:34:56+12:30")
    lyr.CreateFeature(f)

    # Test DATETIME_AS_STRING=YES
    stream = lyr.GetArrowStreamAsNumPy(
        options=["USE_MASKED_ARRAYS=NO", "DATETIME_AS_STRING=YES"]
    )
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert len(batch["datetime"]) == 4
    assert batch["datetime"][0] == b""
    assert batch["datetime"][1] == b"2022-05-31T12:34:56.789Z"
    assert batch["datetime"][2] == b"2022-05-31T12:34:56"
    assert batch["datetime"][3] == b"2022-05-31T12:34:56+12:30"


###############################################################################
# Test CreateFieldFromArrowSchema() when there is a GDAL:OGR:type=DateTime
# Arrow schema metadata.


@gdaltest.enable_exceptions()
def test_ogr_mem_arrow_write_with_datetime_as_string():

    src_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("src_lyr", geom_type=ogr.wkbNone)

    field = ogr.FieldDefn("dt", ogr.OFTDateTime)
    src_lyr.CreateField(field)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("dt", "2022-05-31T12:34:56.789Z")
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("dt", "2022-05-31T12:34:56")
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetField("dt", "2022-05-31T12:34:56+12:30")
    src_lyr.CreateFeature(f)

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    dst_lyr = ds.CreateLayer("dst_lyr")

    stream = src_lyr.GetArrowStream(["DATETIME_AS_STRING=YES"])
    schema = stream.GetSchema()

    for i in range(schema.GetChildrenCount()):
        dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        dst_lyr.WriteArrowBatch(schema, array)

    assert [f.GetField("dt") for f in dst_lyr] == [
        None,
        "2022/05/31 12:34:56.789+00",
        "2022/05/31 12:34:56",
        "2022/05/31 12:34:56+1230",
    ]


###############################################################################


@pytest.mark.parametrize(
    "limited_field",
    [
        "str",
        "strlist",
        "int32list",
        "int64list",
        "float64list",
        "boollist",
        "binary",
        "binary_fixed_width",
        "geometry",
    ],
)
def test_ogr_mem_arrow_stream_numpy_memlimit(limited_field):
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo")

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

    if limited_field == "binary_fixed_width":
        field = ogr.FieldDefn("binary_fixed_width", ogr.OFTBinary)
        field.SetWidth(50)
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
    f.SetField("binary", b"\xde\xad")
    if limited_field == "binary_fixed_width":
        f.SetField("binary_fixed_width", b"\xde\xad" * 25)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    if limited_field == "str":
        f["str"] = "x" * 100
    elif limited_field == "strlist":
        f["strlist"] = ["x" * 100]
    elif limited_field == "int32list":
        f["int32list"] = [0] * 100
    elif limited_field == "int64list":
        f["int64list"] = [0] * 100
    elif limited_field == "float64list":
        f["float64list"] = [0] * 100
    elif limited_field == "boollist":
        f["boollist"] = [False] * 100
    elif limited_field == "binary":
        f["binary"] = b"x" * 100
    elif limited_field == "binary_fixed_width":
        f.SetField("binary_fixed_width", b"\xde\xad" * 25)
    elif limited_field == "geometry":
        g = ogr.Geometry(ogr.wkbLineString)
        sizeof_first_point = 21
        sizeof_linestring_preamble = 9
        sizeof_coord_pair = 2 * 8
        g.SetPoint_2D(
            (100 - sizeof_linestring_preamble - sizeof_first_point)
            // sizeof_coord_pair,
            0,
            0,
        )
        f.SetGeometry(g)
    lyr.CreateFeature(f)

    with gdaltest.config_option("OGR_ARROW_MEM_LIMIT", "100", thread_local=False):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
    assert len(batches) == 2
    assert [x for x in batches[0]["OGC_FID"]] == [0, 1]
    assert [x for x in batches[1]["OGC_FID"]] == [2]

    if limited_field not in ("binary_fixed_width", "geometry"):
        lyr.SetNextByIndex(2)
        with gdaltest.config_option("OGR_ARROW_MEM_LIMIT", "99", thread_local=False):
            stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
            with gdaltest.enable_exceptions():
                with pytest.raises(
                    Exception,
                    match="Too large feature: not even a single feature can be returned",
                ):
                    [batch for batch in stream]

    with gdaltest.config_option("OGR_ARROW_MEM_LIMIT", "1", thread_local=False):

        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        with gdal.quiet_errors():
            gdal.ErrorReset()
            batches = [batch for batch in stream]
            assert (
                gdal.GetLastErrorMsg()
                == "Too large feature: not even a single feature can be returned"
            )
            assert len(batches) == 0

        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        with gdaltest.enable_exceptions():
            with pytest.raises(
                Exception,
                match="Too large feature: not even a single feature can be returned",
            ):
                [batch for batch in stream]


###############################################################################
# Test optimization to save memory on string fields with huge strings compared
# to the average size


def test_ogr_mem_arrow_stream_numpy_huge_string():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
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

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
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


def test_ogr_mem_arrow_stream_pyarrow_geoarrow_no_crs_metadata():
    pytest.importorskip("pyarrow")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo")

    stream = lyr.GetArrowStreamAsPyArrow(["GEOMETRY_METADATA_ENCODING=GEOARROW"])
    if (
        str(stream.schema)
        != "struct<OGC_FID: int64 not null, wkb_geometry: extension<geoarrow.wkb>>"
    ):
        assert (
            str(stream.schema)
            == "struct<OGC_FID: int64 not null, wkb_geometry: binary>"
        )
        md = stream.schema["wkb_geometry"].metadata
        assert b"ARROW:extension:name" in md
        assert md[b"ARROW:extension:name"] == b"geoarrow.wkb"
        assert b"ARROW:extension:metadata" not in md


###############################################################################


def test_ogr_mem_arrow_stream_pyarrow_geoarrow_metadata():
    pytest.importorskip("pyarrow")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("foo", srs=srs)

    stream = lyr.GetArrowStreamAsPyArrow(["GEOMETRY_METADATA_ENCODING=GEOARROW"])
    if (
        str(stream.schema)
        != "struct<OGC_FID: int64 not null, wkb_geometry: extension<geoarrow.wkb>>"
    ):
        assert (
            str(stream.schema)
            == "struct<OGC_FID: int64 not null, wkb_geometry: binary>"
        )
        md = stream.schema["wkb_geometry"].metadata
        assert b"ARROW:extension:name" in md
        assert md[b"ARROW:extension:name"] == b"geoarrow.wkb"
        assert b"ARROW:extension:metadata" in md
        metadata = json.loads(md[b"ARROW:extension:metadata"])
        assert "crs" in metadata
        assert metadata["crs"]["id"] == {"authority": "EPSG", "code": 32631}


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

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    assert lyr.GetSupportedSRSList() is None
    assert lyr.SetActiveSRS(0, None) != ogr.OGRERR_NONE


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_mem_write_arrow():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = ds.CreateLayer("src_lyr")

    field_def = ogr.FieldDefn("field_bool", ogr.OFTInteger)
    field_def.SetSubType(ogr.OFSTBoolean)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integer", ogr.OFTInteger)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_int16", ogr.OFTInteger)
    field_def.SetSubType(ogr.OFSTInt16)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integer64", ogr.OFTInteger64)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_float32", ogr.OFTReal)
    field_def.SetSubType(ogr.OFSTFloat32)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_real", ogr.OFTReal)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_string", ogr.OFTString)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_binary", ogr.OFTBinary)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_date", ogr.OFTDate)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_time", ogr.OFTTime)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_datetime", ogr.OFTDateTime)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_boollist", ogr.OFTIntegerList)
    field_def.SetSubType(ogr.OFSTBoolean)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integerlist", ogr.OFTIntegerList)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_int16list", ogr.OFTIntegerList)
    field_def.SetSubType(ogr.OFSTInt16)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_integer64list", ogr.OFTInteger64List)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_float32list", ogr.OFTRealList)
    field_def.SetSubType(ogr.OFSTFloat32)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_reallist", ogr.OFTRealList)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_stringlist", ogr.OFTStringList)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_json", ogr.OFTString)
    field_def.SetSubType(ogr.OFSTJSON)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_uuid", ogr.OFTString)
    field_def.SetSubType(ogr.OFSTUUID)
    src_lyr.CreateField(field_def)

    field_def = ogr.FieldDefn("field_with_width", ogr.OFTString)
    field_def.SetWidth(10)
    src_lyr.CreateField(field_def)

    feat_def = src_lyr.GetLayerDefn()
    src_feature = ogr.Feature(feat_def)
    src_feature.SetField("field_bool", True)
    src_feature.SetField("field_integer", 17)
    src_feature.SetField("field_int16", -17)
    src_feature.SetField("field_integer64", 9876543210)
    src_feature.SetField("field_float32", 1.5)
    src_feature.SetField("field_real", 18.4)
    src_feature.SetField("field_string", "abc def")
    src_feature.SetFieldBinary("field_binary", b"\x00\x01")
    src_feature.SetField("field_binary", b"\x01\x23\x46\x57\x89\xab\xcd\xef")
    src_feature.SetField("field_date", "2011/11/11")
    src_feature.SetField("field_time", "14:10:35")
    src_feature.SetField("field_datetime", 2011, 11, 11, 14, 10, 35.123, 0)
    src_feature.field_boollist = [False, True]
    src_feature.field_integerlist = [10, 20, 30]
    src_feature.field_int16list = [10, -20, 30]
    src_feature.field_integer64list = [9876543210]
    src_feature.field_float32list = [1.5, -1.5]
    src_feature.field_reallist = [123.5, 567.0]
    src_feature.field_stringlist = ["abc", "def"]
    src_feature["field_json"] = '{"foo":"bar"}'
    src_feature["field_uuid"] = "INVALID_UUID"
    src_feature["field_with_width"] = "foo"
    src_feature.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))

    src_lyr.CreateFeature(src_feature)

    src_feature2 = ogr.Feature(feat_def)
    for i in range(feat_def.GetFieldCount()):
        src_feature2.SetFieldNull(i)
    src_lyr.CreateFeature(src_feature2)

    dst_lyr = ds.CreateLayer("dst_lyr")

    stream = src_lyr.GetArrowStream(["INCLUDE_FID=NO"])
    schema = stream.GetSchema()

    success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
    assert success, error_msg

    for i in range(schema.GetChildrenCount()):
        if schema.GetChild(i).GetName() != "wkb_geometry":
            dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

    idx = dst_lyr.GetLayerDefn().GetFieldIndex("field_json")
    assert dst_lyr.GetLayerDefn().GetFieldDefn(idx).GetSubType() == ogr.OFSTJSON

    idx = dst_lyr.GetLayerDefn().GetFieldIndex("field_uuid")
    assert dst_lyr.GetLayerDefn().GetFieldDefn(idx).GetSubType() == ogr.OFSTUUID

    idx = dst_lyr.GetLayerDefn().GetFieldIndex("field_with_width")
    assert dst_lyr.GetLayerDefn().GetFieldDefn(idx).GetWidth() == 10

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    dst_lyr.ResetReading()

    dst_feature = dst_lyr.GetNextFeature()
    assert str(src_feature) == str(dst_feature).replace("dst_lyr", "src_lyr")

    dst_feature = dst_lyr.GetNextFeature()
    assert str(src_feature2) == str(dst_feature).replace("dst_lyr", "src_lyr")


###############################################################################
# Test various data types for FID column in WriteArrowBatch()


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("fid_type", [ogr.OFTInteger, ogr.OFTInteger64, ogr.OFTString])
def test_ogr_mem_write_arrow_types_of_fid(fid_type):

    src_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("src_lyr")

    src_lyr.CreateField(ogr.FieldDefn("id", fid_type))

    src_feature = ogr.Feature(src_lyr.GetLayerDefn())
    src_feature["id"] = 2
    src_lyr.CreateFeature(src_feature)

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    dst_lyr = ds.CreateLayer("dst_lyr")

    stream = src_lyr.GetArrowStream()
    schema = stream.GetSchema()

    for i in range(schema.GetChildrenCount()):
        if schema.GetChild(i).GetName() != "wkb_geometry":
            dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        if fid_type == ogr.OFTString:
            with pytest.raises(
                Exception,
                match=r"FID column 'id' should be of Arrow format 'i' \(int32\) or 'l' \(int64\)",
            ):
                dst_lyr.WriteArrowBatch(schema, array, ["FID=id"])
        else:
            dst_lyr.WriteArrowBatch(schema, array, ["FID=id"])

    if fid_type != ogr.OFTString:
        assert dst_lyr.GetFeature(2)


###############################################################################
# Test failure of CreateFeature() in WriteArrowBatch()


@gdaltest.enable_exceptions()
def test_ogr_mem_write_arrow_error_negative_fid():

    src_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("src_lyr")

    src_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))

    src_feature = ogr.Feature(src_lyr.GetLayerDefn())
    src_feature["id"] = -2
    src_lyr.CreateFeature(src_feature)

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    dst_lyr = ds.CreateLayer("dst_lyr")

    stream = src_lyr.GetArrowStream()
    schema = stream.GetSchema()

    for i in range(schema.GetChildrenCount()):
        if schema.GetChild(i).GetName() != "wkb_geometry":
            dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        with pytest.raises(Exception, match="negative FID are not supported"):
            dst_lyr.WriteArrowBatch(schema, array, ["FID=id"])


###############################################################################
# Test writing a ArrowArray into a OGR field whose types don't fully match


@pytest.mark.parametrize("IF_FIELD_NOT_PRESERVED", [None, "ERROR"])
@pytest.mark.parametrize(
    "input_type,output_type,input_vals,output_vals",
    [
        [ogr.OFTInteger64, ogr.OFTInteger, [123456, None], [123456, None]],
        [ogr.OFTInteger64, ogr.OFTInteger, [1234567890123], [(1 << 31) - 1]],
        [ogr.OFTReal, ogr.OFTInteger, [1], [1]],
        [ogr.OFTReal, ogr.OFTInteger, [1.23], [1]],
        [ogr.OFTReal, ogr.OFTInteger, [float("nan")], [-(1 << 31)]],
        [ogr.OFTReal, ogr.OFTInteger64, [1], [1]],
        [ogr.OFTReal, ogr.OFTInteger64, [1.23], [1]],
        [ogr.OFTReal, ogr.OFTInteger64, [float("nan")], [-(1 << 63)]],
        [ogr.OFTInteger64, ogr.OFTReal, [1234567890123, None], [1234567890123, None]],
        [
            ogr.OFTInteger64,
            ogr.OFTReal,
            [((1 << 63) - 1), None],
            [float((1 << 63) - 1), None],
        ],
        # below is never lossy
        [ogr.OFTInteger, ogr.OFTInteger64, [123456, None], [123456, None]],
        [ogr.OFTInteger, ogr.OFTReal, [123456, None], [123456, None]],
    ],
)
@gdaltest.enable_exceptions()
def test_ogr_mem_write_arrow_accepted_field_type_mismatch(
    input_type, output_type, input_vals, output_vals, IF_FIELD_NOT_PRESERVED
):

    if input_vals[0] == ((1 << 63) - 1) and output_type == ogr.OFTReal:
        # This conversion from INT64_MAX to double doesn't seem to be lossy
        # on arm64 or s390x (weird...)
        import platform

        if platform.machine() not in ("x86_64", "AMD64"):
            pytest.skip("Skipping test on platform.machine() = " + platform.machine())

    src_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = src_ds.CreateLayer("src_lyr")

    src_lyr.CreateField(ogr.FieldDefn("my_field", input_type))

    for v in input_vals:
        src_feature = ogr.Feature(src_lyr.GetLayerDefn())
        if v:
            src_feature["my_field"] = v
        src_lyr.CreateFeature(src_feature)

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    dst_lyr = ds.CreateLayer("dst_lyr")
    dst_lyr.CreateField(ogr.FieldDefn("my_field", output_type))

    stream = src_lyr.GetArrowStream(["INCLUDE_FID=NO"])
    schema = stream.GetSchema()

    lossy_conversion = (input_vals != output_vals) or (
        input_vals[0] == ((1 << 63) - 1) and output_type == ogr.OFTReal
    )

    while True:
        array = stream.GetNextRecordBatch()
        if array is None:
            break
        if IF_FIELD_NOT_PRESERVED:
            if lossy_conversion:
                with gdal.quiet_errors(), pytest.raises(
                    Exception, match="value of field my_field cannot not preserved"
                ):
                    dst_lyr.WriteArrowBatch(
                        schema,
                        array,
                        {"IF_FIELD_NOT_PRESERVED": IF_FIELD_NOT_PRESERVED},
                    )
                return
            else:
                dst_lyr.WriteArrowBatch(
                    schema, array, {"IF_FIELD_NOT_PRESERVED": IF_FIELD_NOT_PRESERVED}
                )
        else:
            if lossy_conversion:
                with gdal.quiet_errors():
                    gdal.ErrorReset()
                    dst_lyr.WriteArrowBatch(schema, array)
                    assert gdal.GetLastErrorType() == gdal.CE_Warning
            else:
                gdal.ErrorReset()
                dst_lyr.WriteArrowBatch(schema, array)
                assert gdal.GetLastErrorType() == gdal.CE_None

    dst_lyr.ResetReading()

    for v in output_vals:
        f = dst_lyr.GetNextFeature()
        assert f["my_field"] == v


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_mem_write_pyarrow():
    pa = pytest.importorskip("pyarrow")
    np = pytest.importorskip("numpy")  # for float16

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test")

    fid = pa.array([None for i in range(5)], type=pa.int32())

    boolean = pa.array([True, False, None, False, True], type=pa.bool_())
    uint8 = pa.array([None if i == 2 else 1 + i for i in range(5)], type=pa.uint8())
    int8 = pa.array([None if i == 2 else -2 + i for i in range(5)], type=pa.int8())
    uint16 = pa.array(
        [None if i == 2 else 1 + i * 10000 for i in range(5)], type=pa.uint16()
    )
    int16 = pa.array(
        [None if i == 2 else -20000 + i * 10000 for i in range(5)], type=pa.int16()
    )
    uint32 = pa.array(
        [None if i == 2 else 1 + i * 1000000000 for i in range(5)], type=pa.uint32()
    )
    int32 = pa.array(
        [None if i == 2 else -2000000000 + i * 1000000000 for i in range(5)],
        type=pa.int32(),
    )
    uint64 = pa.array(
        [None if i == 2 else 1 + i * 100000000000 for i in range(5)], type=pa.uint64()
    )
    int64 = pa.array(
        [None if i == 2 else -200000000000 + i * 100000000000 for i in range(5)],
        type=pa.int64(),
    )
    float16 = pa.array(
        [None if i == 2 else np.float16(1.5 + i) for i in range(5)], type=pa.float16()
    )
    float32 = pa.array(
        [None if i == 2 else 1.5 + i for i in range(5)], type=pa.float32()
    )
    float64 = pa.array(
        [None if i == 2 else 1.5 + i for i in range(5)], type=pa.float64()
    )
    string = pa.array(["abcd", "", None, "c", "def"], type=pa.string())
    large_string = pa.array(["abcd", "", None, "c", "def"], type=pa.large_string())
    binary = pa.array([b"\x00\x01"] * 5, type=pa.binary())
    large_binary = pa.array([b"\x00\x01"] * 5, type=pa.large_binary())
    fixed_size_binary = pa.array(
        [b"\x00\x01", b"\x00\x00", b"\x01\x01", b"\x01\x00", b"\x00\x01"],
        type=pa.binary(2),
    )

    date32 = pa.array([1, 2, 3, 4, 5], type=pa.date32())
    date64 = pa.array([86400 * 1000, 2, 3, 4, 5], type=pa.date64())
    import datetime

    gmt_plus_2 = datetime.timezone(datetime.timedelta(hours=2))
    timestamp_ms_gmt_plus_2 = pa.array(
        [1000, 2000, 3000, 4000, 5000],
        type=pa.timestamp("ms", tz=gmt_plus_2),
    )

    struct_field = pa.array([{"a": 1, "b": 2.5}] * 5)

    list_boolean = pa.array(
        [
            (
                None
                if i == 2
                else [
                    None if j == 0 else True if (j % 2) == 0 else False
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.bool_()),
    )
    list_uint8 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint8()),
    )
    list_int8 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int8()),
    )
    list_uint16 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint16()),
    )
    list_int16 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int16()),
    )
    list_uint32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint32()),
    )
    list_int32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int32()),
    )
    list_uint64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint64()),
    )
    list_int64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int64()),
    )
    list_float16 = pa.array(
        [
            (
                None
                if i == 2
                else [
                    None if j == 0 else np.float16(0.5 + j + i * (i - 1) // 2)
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.float16()),
    )
    list_float32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else 0.5 + j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.float32()),
    )
    list_float64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else 0.5 + j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.float64()),
    )
    list_string = pa.array(
        [
            (
                None
                if i == 2
                else [
                    "".join(["%c" % (65 + j + k) for k in range(1 + j)])
                    for j in range(i)
                ]
            )
            for i in range(5)
        ]
    )
    list_large_string = pa.array(
        [
            (
                None
                if i == 2
                else [
                    "".join(["%c" % (65 + j + k) for k in range(1 + j)])
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.large_string()),
    )

    large_list_boolean = pa.array(
        [
            (
                None
                if i == 2
                else [
                    None if j == 0 else True if (j % 2) == 0 else False
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.bool_()),
    )
    large_list_uint8 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.uint8()),
    )
    large_list_int8 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.int8()),
    )
    large_list_uint16 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.uint16()),
    )
    large_list_int16 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.int16()),
    )
    large_list_uint32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.uint32()),
    )
    large_list_int32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.int32()),
    )
    large_list_uint64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.uint64()),
    )
    large_list_int64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.int64()),
    )
    large_list_float16 = pa.array(
        [
            (
                None
                if i == 2
                else [
                    None if j == 0 else np.float16(0.5 + j + i * (i - 1) // 2)
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.float16()),
    )
    large_list_float32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else 0.5 + j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.float32()),
    )
    large_list_float64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else 0.5 + j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.float64()),
    )
    large_list_string = pa.array(
        [
            (
                None
                if i == 2
                else [
                    "".join(["%c" % (65 + j + k) for k in range(1 + j)])
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.string()),
    )
    large_list_large_string = pa.array(
        [
            (
                None
                if i == 2
                else [
                    "".join(["%c" % (65 + j + k) for k in range(1 + j)])
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.large_list(pa.large_string()),
    )

    fixed_size_list_boolean = pa.array(
        [[True, False], [False, True], [True, False], [False, True], [True, False]],
        type=pa.list_(pa.bool_(), 2),
    )
    fixed_size_list_uint8 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint8(), 2)
    )
    fixed_size_list_int8 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int8(), 2)
    )
    fixed_size_list_uint16 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint16(), 2)
    )
    fixed_size_list_int16 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int16(), 2)
    )
    fixed_size_list_uint32 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint32(), 2)
    )
    fixed_size_list_int32 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int32(), 2)
    )
    fixed_size_list_uint64 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint64(), 2)
    )
    fixed_size_list_int64 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int64(), 2)
    )
    fixed_size_list_float16 = pa.array(
        [
            [np.float16(0), None],
            [np.float16(2), np.float16(3)],
            [np.float16(4), np.float16(5)],
            [np.float16(6), np.float16(7)],
            [np.float16(8), np.float16(9)],
        ],
        type=pa.list_(pa.float16(), 2),
    )
    fixed_size_list_float32 = pa.array(
        [[0, None], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.float32(), 2)
    )
    fixed_size_list_float64 = pa.array(
        [[0, None], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.float64(), 2)
    )
    fixed_size_list_string = pa.array(
        [["ax", "b"], ["cy", "d"], ["ez", "f"], ["gv", "h"], ["iw", "j"]],
        type=pa.list_(pa.string(), 2),
    )
    fixed_size_list_large_string = pa.array(
        [["ax", "b"], ["cy", "d"], ["ez", "f"], ["gv", "h"], ["iw", "j"]],
        type=pa.list_(pa.large_string(), 2),
    )

    dictionary = pa.array(["foo", "bar", "baz"])

    indices_int8 = pa.array([0, 1, 2, None, 2], type=pa.int8())
    dict_int8 = pa.DictionaryArray.from_arrays(indices_int8, dictionary)

    indices_uint8 = pa.array([0, 1, 2, None, 2], type=pa.uint8())
    dict_uint8 = pa.DictionaryArray.from_arrays(indices_uint8, dictionary)

    indices_int16 = pa.array([0, 1, 2, None, 2], type=pa.int16())
    dict_int16 = pa.DictionaryArray.from_arrays(indices_int16, dictionary)

    indices_uint16 = pa.array([0, 1, 2, None, 2], type=pa.uint16())
    dict_uint16 = pa.DictionaryArray.from_arrays(indices_uint16, dictionary)

    indices_int32 = pa.array([0, 1, 2, None, 2], type=pa.int32())
    dict_int32 = pa.DictionaryArray.from_arrays(indices_int32, dictionary)

    indices_uint32 = pa.array([0, 1, 2, None, 2], type=pa.uint32())
    dict_uint32 = pa.DictionaryArray.from_arrays(indices_uint32, dictionary)

    indices_int64 = pa.array([0, 1, 2, None, 2], type=pa.int64())
    dict_int64 = pa.DictionaryArray.from_arrays(indices_int64, dictionary)

    indices_uint64 = pa.array([0, 1, 2, None, 2], type=pa.uint64())
    dict_uint64 = pa.DictionaryArray.from_arrays(indices_uint64, dictionary)

    dictionary_list_uint8 = pa.array([[0, 1], [2, 3]], pa.list_(pa.uint8()))
    indices_dict_list_uint8 = pa.array([0, 1, 0, None, 1], type=pa.int32())
    dict_list_uint8 = pa.DictionaryArray.from_arrays(
        indices_dict_list_uint8, dictionary_list_uint8
    )

    import decimal

    d = decimal.Decimal("123.45")
    decimal128 = pa.array([d, d, d, d, d], pa.decimal128(5, 2))
    decimal256 = pa.array([d, d, d, d, d], pa.decimal256(5, 2))

    list_decimal128 = pa.array(
        [
            [],
            [decimal.Decimal("123.45")],
            [decimal.Decimal("123.45"), decimal.Decimal("-123.45")],
            None,
            [decimal.Decimal("123.45"), decimal.Decimal("234.56")],
        ],
        type=pa.list_(pa.decimal128(5, 2)),
    )

    large_list_decimal128 = pa.array(
        [
            [],
            [decimal.Decimal("123.45")],
            [decimal.Decimal("123.45"), decimal.Decimal("-123.45")],
            None,
            [decimal.Decimal("123.45"), decimal.Decimal("234.56")],
        ],
        type=pa.large_list(pa.decimal128(5, 2)),
    )

    fixed_size_list_decimal128 = pa.array(
        [
            [decimal.Decimal("123.45"), decimal.Decimal("-123.45")],
            [decimal.Decimal("-123.45"), decimal.Decimal("123.45")],
            None,
            [decimal.Decimal("234.56"), decimal.Decimal("-234.56")],
            [decimal.Decimal("345.67"), decimal.Decimal("-345.67")],
        ],
        type=pa.list_(pa.decimal128(5, 2), 2),
    )

    map_boolean = pa.array(
        [[("z", False)], None, [], [], [("x", None), ("yz", True)]],
        type=pa.map_(pa.string(), pa.bool_()),
    )

    map_uint8 = pa.array(
        [[("z", 2)], None, [], [], [("x", None), ("yz", 4)]],
        type=pa.map_(pa.string(), pa.uint8()),
    )

    map_int8 = pa.array(
        [[("z", -2)], None, [], [], [("x", None), ("yz", -4)]],
        type=pa.map_(pa.string(), pa.int8()),
    )

    map_uint16 = pa.array(
        [[("z", 2)], None, [], [], [("x", None), ("yz", 4)]],
        type=pa.map_(pa.string(), pa.uint16()),
    )

    map_int16 = pa.array(
        [[("z", -2)], None, [], [], [("x", None), ("yz", -4)]],
        type=pa.map_(pa.string(), pa.int16()),
    )

    map_uint32 = pa.array(
        [[("z", 2)], None, [], [], [("x", None), ("yz", 4)]],
        type=pa.map_(pa.string(), pa.uint32()),
    )

    map_int32 = pa.array(
        [[("z", -2)], None, [], [], [("x", None), ("yz", -4)]],
        type=pa.map_(pa.string(), pa.int32()),
    )

    map_uint64 = pa.array(
        [[("z", 2)], None, [], [], [("x", None), ("yz", 4)]],
        type=pa.map_(pa.string(), pa.uint64()),
    )

    map_int64 = pa.array(
        [[("z", -2)], None, [], [], [("x", None), ("yz", -4)]],
        type=pa.map_(pa.string(), pa.int64()),
    )

    map_float16 = pa.array(
        [[("z", np.float16(2))], None, [], [], [("x", None), ("yz", np.float16(4))]],
        type=pa.map_(pa.string(), pa.float16()),
    )

    map_float32 = pa.array(
        [[("z", 2)], None, [], [], [("x", None), ("yz", 4)]],
        type=pa.map_(pa.string(), pa.float32()),
    )

    map_float64 = pa.array(
        [[("z", 2)], None, [], [], [("x", None), ("yz", 4)]],
        type=pa.map_(pa.string(), pa.float64()),
    )

    map_string = pa.array(
        [[("z", "foo")], None, [], [], [("x", None), ("yz", "bar")]],
        type=pa.map_(pa.string(), pa.string()),
    )

    map_large_string = pa.array(
        [[("z", "foo")], None, [], [], [("x", None), ("yz", "bar")]],
        type=pa.map_(pa.string(), pa.large_string()),
    )

    map_binary = pa.array(
        [
            [("z", b"\x01\x01\x00\x00\x00")],
            None,
            [],
            [],
            [("x", None), ("yz", b"\x00\x00\x01\x00\x00")],
        ],
        type=pa.map_(pa.string(), pa.binary()),
    )

    map_large_binary = pa.array(
        [
            [("z", b"\x01\x01\x00\x00\x00")],
            None,
            [],
            [],
            [("x", None), ("yz", b"\x00\x00\x01\x00\x00")],
        ],
        type=pa.map_(pa.string(), pa.large_binary()),
    )

    map_fixed_size_binary = pa.array(
        [
            [("z", b"\x01\x01\x00\x00\x00")],
            None,
            [],
            [],
            [("x", None), ("yz", b"\x00\x00\x01\x00\x00")],
        ],
        type=pa.map_(pa.string(), pa.binary(5)),
    )

    map_decimal = pa.array(
        [[("z", d)], None, [], [], [("x", None), ("yz", d)]],
        type=pa.map_(pa.string(), pa.decimal128(5, 2)),
    )

    map_structure = pa.array(
        [None, None, [], [], [("x", None), ("y", {}), ("z", {"f1": "foo", "f2": 2})]],
        type=pa.map_(pa.string(), pa.struct([("f1", pa.string()), ("f2", pa.int32())])),
    )

    map_map_string = pa.array(
        [
            None,
            [("f", [("g", "h")])],
            None,
            None,
            [("a", [("b", "c"), ("d", None)]), ("e", None)],
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.string())),
    )

    map_list_bool = pa.array(
        [
            [("x", [True]), ("y", [False, True])],
            [("z", [])],
            None,
            [],
            [("w", [True, False]), ("x", None), ("z", [False, None, True])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.bool_())),
    )

    map_list_uint8 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, 8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.uint8())),
    )

    map_list_int8 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, -8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.int8())),
    )

    map_list_uint16 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, 8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.uint16())),
    )

    map_list_int16 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, -8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.int16())),
    )

    map_list_uint32 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, 8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.uint32())),
    )

    map_list_int32 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, -8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.int32())),
    )

    map_list_uint64 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, 8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.uint64())),
    )

    map_list_int64 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, -8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.int64())),
    )

    map_list_float16 = pa.array(
        [
            [("x", [np.float16(2)]), ("y", [np.float16(3), np.float16(4)])],
            [("z", [])],
            None,
            [],
            [
                ("w", [np.float16(5), np.float16(6)]),
                ("x", None),
                ("z", [np.float16(7), None, np.float16(8)]),
            ],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.float16())),
    )

    map_list_float32 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, 8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.float32())),
    )

    map_list_float64 = pa.array(
        [
            [("x", [2]), ("y", [3, 4])],
            [("z", [])],
            None,
            [],
            [("w", [5, 6]), ("x", None), ("z", [7, None, -8])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.float64())),
    )

    map_list_decimal = pa.array(
        [
            [("x", [d]), ("y", [d, d])],
            [("z", [])],
            None,
            [],
            [("w", [d, d]), ("x", None), ("z", [d, None, d])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.decimal128(5, 2))),
    )

    map_fixed_size_list_uint8 = pa.array(
        [
            [("z", [4, 5])],
            None,
            None,
            None,
            [("x", [2, 3]), ("y", None), ("z", [4, 5])],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.uint8(), 2)),
    )

    list_list_string = pa.array(
        [None, [["efg"]], [], [], [["a"], None, ["b", None, "cd"]]],
        type=pa.list_(pa.list_(pa.string())),
    )

    list_large_list_large_string = pa.array(
        [None, [["efg"]], [], [], [["a"], None, ["b", None, "cd"]]],
        type=pa.list_(pa.large_list(pa.large_string())),
    )

    list_map_string = pa.array(
        [None, [None], [], [], [[("a", "b"), ("c", "d")], [("e", "f")]]],
        type=pa.list_(pa.map_(pa.string(), pa.string())),
    )

    list_binary = pa.array(
        [None, [None], [], [], [b"\x01\x01\x00\x00\x00", b"\x00\x00\x01\x00\x00"]],
        type=pa.list_(pa.binary()),
    )

    list_large_binary = pa.array(
        [None, [None], [], [], [b"\x01\x01\x00\x00\x00", b"\x00\x00\x01\x00\x00"]],
        type=pa.list_(pa.large_binary()),
    )

    list_fixed_size_binary = pa.array(
        [None, [None], [], [], [b"\x01\x01\x00\x00\x00", b"\x00\x00\x01\x00\x00"]],
        type=pa.list_(pa.binary(5)),
    )

    import struct

    wkb_geometry = pa.array(
        [
            None if i == 1 else (b"\x01\x01\x00\x00\x00" + struct.pack("<dd", i, 2))
            for i in range(5)
        ],
        type=pa.binary(),
    )

    names = [
        "boolean",
        "int8",
        "uint8",
        "int16",
        "uint16",
        "int32",
        "uint32",
        "int64",
        "uint64",
        "float16",
        "float32",
        "float64",
        "string",
        "large_string",
        "binary",
        "large_binary",
        "fixed_size_binary",
        "date32",
        "date64",
        "timestamp_ms_gmt_plus_2",
        "struct_field",
        "list_boolean",
        "list_int8",
        "list_uint8",
        "list_int16",
        "list_uint16",
        "list_int32",
        "list_uint32",
        "list_int64",
        "list_uint64",
        "list_float16",
        "list_float32",
        "list_float64",
        "list_string",
        "list_large_string",
        "large_list_boolean",
        "large_list_int8",
        "large_list_uint8",
        "large_list_int16",
        "large_list_uint16",
        "large_list_int32",
        "large_list_uint32",
        "large_list_int64",
        "large_list_uint64",
        "large_list_float16",
        "large_list_float32",
        "large_list_float64",
        "large_list_string",
        "large_list_large_string",
        "fixed_size_list_boolean",
        "fixed_size_list_int8",
        "fixed_size_list_uint8",
        "fixed_size_list_int16",
        "fixed_size_list_uint16",
        "fixed_size_list_int32",
        "fixed_size_list_uint32",
        "fixed_size_list_int64",
        "fixed_size_list_uint64",
        "fixed_size_list_float16",
        "fixed_size_list_float32",
        "fixed_size_list_float64",
        "fixed_size_list_string",
        "fixed_size_list_large_string",
        "dict_int8",
        "dict_uint8",
        "dict_int16",
        "dict_uint16",
        "dict_int32",
        "dict_uint32",
        "dict_int64",
        "dict_uint64",
        "dict_list_uint8",
        "decimal128",
        "decimal256",
        "list_decimal128",
        "large_list_decimal128",
        "fixed_size_list_decimal128",
        "map_boolean",
        "map_int8",
        "map_uint8",
        "map_int16",
        "map_uint16",
        "map_int32",
        "map_uint32",
        "map_int64",
        "map_uint64",
        "map_float16",
        "map_float32",
        "map_float64",
        "map_string",
        "map_large_string",
        "map_binary",
        "map_large_binary",
        "map_fixed_size_binary",
        "map_decimal",
        "map_structure",
        "map_map_string",
        "map_list_bool",
        "map_list_int8",
        "map_list_uint8",
        "map_list_int16",
        "map_list_uint16",
        "map_list_int32",
        "map_list_uint32",
        "map_list_int64",
        "map_list_uint64",
        "map_list_float16",
        "map_list_float32",
        "map_list_float64",
        "map_list_decimal",
        "map_fixed_size_list_uint8",
        "list_list_string",
        "list_large_list_large_string",
        "list_map_string",
        "list_binary",
        "list_large_binary",
        "list_fixed_size_binary",
        "wkb_geometry",
        "fid",
    ]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    success, error_msg = lyr.IsPyArrowSchemaSupported(table.schema)
    assert success, error_msg

    success, error_msg = lyr.IsPyArrowSchemaSupported(pa.int32())
    assert not success
    assert (
        error_msg
        == "IsArrowSchemaSupported() should be called on a schema that is a struct of fields"
    )

    for i in range(len(table.schema)):
        if table.schema.field(i).name not in ("wkb_geometry", "fid"):
            lyr.CreateFieldFromPyArrowSchema(table.schema.field(i))

    idx = lyr.GetLayerDefn().GetFieldIndex("decimal128")
    assert lyr.GetLayerDefn().GetFieldDefn(idx).GetWidth() == 5 + 2
    assert lyr.GetLayerDefn().GetFieldDefn(idx).GetPrecision() == 2

    idx = lyr.GetLayerDefn().GetFieldIndex("decimal256")
    assert lyr.GetLayerDefn().GetFieldDefn(idx).GetWidth() == 5 + 2
    assert lyr.GetLayerDefn().GetFieldDefn(idx).GetPrecision() == 2

    assert lyr.WritePyArrow(table, options=["FID=fid"]) == ogr.OGRERR_NONE
    assert fid.to_pylist() == [0, 1, 2, 3, 4]

    f = lyr.GetFeature(0)
    assert f["map_uint8"] == """{"z":2}"""

    f = lyr.GetFeature(1)
    assert f["map_uint8"] is None

    f = lyr.GetFeature(2)
    assert f["map_uint8"] == "{}"

    f = lyr.GetFeature(3)
    assert f["map_uint8"] == "{}"

    f = lyr.GetFeature(4)
    assert str(f) == """OGRFeature(test):4
  boolean (Integer(Boolean)) = 1
  int8 (Integer(Int16)) = 2
  uint8 (Integer(Int16)) = 5
  int16 (Integer(Int16)) = 20000
  uint16 (Integer) = 40001
  int32 (Integer) = 2000000000
  uint32 (Integer64) = 4000000001
  int64 (Integer64) = 200000000000
  uint64 (Real) = 400000000001
  float16 (Real(Float32)) = 5.5
  float32 (Real(Float32)) = 5.5
  float64 (Real) = 5.5
  string (String) = def
  large_string (String) = def
  binary (Binary) = 0001
  large_binary (Binary) = 0001
  fixed_size_binary (Binary) = 0001
  date32 (Date) = 1970/01/06
  date64 (Date) = 1970/01/01
  timestamp_ms_gmt_plus_2 (DateTime) = 1970/01/01 02:00:05+02
  struct_field.a (Integer64) = 1
  struct_field.b (Real) = 2.5
  list_boolean (IntegerList(Boolean)) = (4:0,0,1,0)
  list_int8 (IntegerList(Int16)) = (4:0,7,8,9)
  list_uint8 (IntegerList(Int16)) = (4:0,7,8,9)
  list_int16 (IntegerList(Int16)) = (4:0,7,8,9)
  list_uint16 (IntegerList) = (4:0,7,8,9)
  list_int32 (IntegerList) = (4:0,7,8,9)
  list_uint32 (Integer64List) = (4:0,7,8,9)
  list_int64 (Integer64List) = (4:0,7,8,9)
  list_uint64 (RealList) = (4:0,7,8,9)
  list_float16 (RealList(Float32)) = (4:0.0,7.5,8.5,9.5)
  list_float32 (RealList(Float32)) = (4:0.0,7.5,8.5,9.5)
  list_float64 (RealList) = (4:0,7.5,8.5,9.5)
  list_string (StringList) = (4:A,BC,CDE,DEFG)
  list_large_string (StringList) = (4:A,BC,CDE,DEFG)
  large_list_boolean (IntegerList(Boolean)) = (4:0,0,1,0)
  large_list_int8 (IntegerList(Int16)) = (4:0,7,8,9)
  large_list_uint8 (IntegerList(Int16)) = (4:0,7,8,9)
  large_list_int16 (IntegerList(Int16)) = (4:0,7,8,9)
  large_list_uint16 (IntegerList) = (4:0,7,8,9)
  large_list_int32 (IntegerList) = (4:0,7,8,9)
  large_list_uint32 (Integer64List) = (4:0,7,8,9)
  large_list_int64 (Integer64List) = (4:0,7,8,9)
  large_list_uint64 (RealList) = (4:0,7,8,9)
  large_list_float16 (RealList(Float32)) = (4:0.0,7.5,8.5,9.5)
  large_list_float32 (RealList(Float32)) = (4:0.0,7.5,8.5,9.5)
  large_list_float64 (RealList) = (4:0,7.5,8.5,9.5)
  large_list_string (StringList) = (4:A,BC,CDE,DEFG)
  large_list_large_string (StringList) = (4:A,BC,CDE,DEFG)
  fixed_size_list_boolean (IntegerList(Boolean)) = (2:1,0)
  fixed_size_list_int8 (IntegerList(Int16)) = (2:8,9)
  fixed_size_list_uint8 (IntegerList(Int16)) = (2:8,9)
  fixed_size_list_int16 (IntegerList(Int16)) = (2:8,9)
  fixed_size_list_uint16 (IntegerList) = (2:8,9)
  fixed_size_list_int32 (IntegerList) = (2:8,9)
  fixed_size_list_uint32 (Integer64List) = (2:8,9)
  fixed_size_list_int64 (Integer64List) = (2:8,9)
  fixed_size_list_uint64 (RealList) = (2:8,9)
  fixed_size_list_float16 (RealList(Float32)) = (2:8.0,9.0)
  fixed_size_list_float32 (RealList(Float32)) = (2:8.0,9.0)
  fixed_size_list_float64 (RealList) = (2:8,9)
  fixed_size_list_string (StringList) = (2:iw,j)
  fixed_size_list_large_string (StringList) = (2:iw,j)
  dict_int8 (String) = baz
  dict_uint8 (String) = baz
  dict_int16 (String) = baz
  dict_uint16 (String) = baz
  dict_int32 (String) = baz
  dict_uint32 (String) = baz
  dict_int64 (String) = baz
  dict_uint64 (String) = baz
  dict_list_uint8 (IntegerList(Int16)) = (2:2,3)
  decimal128 (Real) = 123.45
  decimal256 (Real) = 123.45
  list_decimal128 (RealList) = (2: 123.45, 234.56)
  large_list_decimal128 (RealList) = (2: 123.45, 234.56)
  fixed_size_list_decimal128 (RealList) = (2: 345.67,-345.67)
  map_boolean (String(JSON)) = {"x":null,"yz":true}
  map_int8 (String(JSON)) = {"x":null,"yz":-4}
  map_uint8 (String(JSON)) = {"x":null,"yz":4}
  map_int16 (String(JSON)) = {"x":null,"yz":-4}
  map_uint16 (String(JSON)) = {"x":null,"yz":4}
  map_int32 (String(JSON)) = {"x":null,"yz":-4}
  map_uint32 (String(JSON)) = {"x":null,"yz":4}
  map_int64 (String(JSON)) = {"x":null,"yz":-4}
  map_uint64 (String(JSON)) = {"x":null,"yz":4}
  map_float16 (String(JSON)) = {"x":null,"yz":4.0}
  map_float32 (String(JSON)) = {"x":null,"yz":4.0}
  map_float64 (String(JSON)) = {"x":null,"yz":4.0}
  map_string (String(JSON)) = {"x":null,"yz":"bar"}
  map_large_string (String(JSON)) = {"x":null,"yz":"bar"}
  map_binary (String(JSON)) = {"x":null,"yz":"AAABAAA="}
  map_large_binary (String(JSON)) = {"x":null,"yz":"AAABAAA="}
  map_fixed_size_binary (String(JSON)) = {"x":null,"yz":"AAABAAA="}
  map_decimal (String(JSON)) = {"x":null,"yz":123.45}
  map_structure (String(JSON)) = {"x":null,"y":{"f1":null,"f2":null},"z":{"f1":"foo","f2":2}}
  map_map_string (String(JSON)) = {"a":{"b":"c","d":null},"e":null}
  map_list_bool (String(JSON)) = {"w":[true,false],"x":null,"z":[false,null,true]}
  map_list_int8 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,-8]}
  map_list_uint8 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,8]}
  map_list_int16 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,-8]}
  map_list_uint16 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,8]}
  map_list_int32 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,-8]}
  map_list_uint32 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,8]}
  map_list_int64 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,-8]}
  map_list_uint64 (String(JSON)) = {"w":[5,6],"x":null,"z":[7,null,8]}
  map_list_float16 (String(JSON)) = {"w":[5.0,6.0],"x":null,"z":[7.0,null,8.0]}
  map_list_float32 (String(JSON)) = {"w":[5.0,6.0],"x":null,"z":[7.0,null,8.0]}
  map_list_float64 (String(JSON)) = {"w":[5.0,6.0],"x":null,"z":[7.0,null,-8.0]}
  map_list_decimal (String(JSON)) = {"w":[123.45,123.45],"x":null,"z":[123.45,null,123.45]}
  map_fixed_size_list_uint8 (String(JSON)) = {"x":[2,3],"y":null,"z":[4,5]}
  list_list_string (String(JSON)) = [["a"],null,["b",null,"cd"]]
  list_large_list_large_string (String(JSON)) = [["a"],null,["b",null,"cd"]]
  list_map_string (String(JSON)) = [{"a":"b","c":"d"},{"e":"f"}]
  list_binary (String(JSON)) = ["AQEAAAA=","AAABAAA="]
  list_large_binary (String(JSON)) = ["AQEAAAA=","AAABAAA="]
  list_fixed_size_binary (String(JSON)) = ["AQEAAAA=","AAABAAA="]
  POINT (4 2)

"""


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_mem_write_pyarrow_geometry_in_large_binary():
    pa = pytest.importorskip("pyarrow")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test")

    import struct

    wkb_geometry = pa.array(
        [
            None if i == 1 else (b"\x01\x01\x00\x00\x00" + struct.pack("<dd", i, 2))
            for i in range(5)
        ],
        type=pa.large_binary(),
    )

    names = ["wkb_geometry"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    success, error_msg = lyr.IsPyArrowSchemaSupported(table.schema)
    assert success, error_msg

    assert lyr.WritePyArrow(table) == ogr.OGRERR_NONE

    f = lyr.GetFeature(4)
    assert str(f) == """OGRFeature(test):4
  POINT (4 2)

"""


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("dict_values", [["foo", "bar", "baz"], [10, 20, 30]])
def test_ogr_mem_write_pyarrow_invalid_dict_index(dict_values):
    pa = pytest.importorskip("pyarrow")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("test")

    dictionary = pa.array(dict_values)
    indices_invalid_index = pa.array([0, 1, 2, None, 2], type=pa.int32())
    dict_invalid_index = pa.DictionaryArray.from_arrays(
        indices_invalid_index, dictionary
    )
    # Super hacky: we alter the last value of indices_invalid_index after
    # building the DictionaryArray to point to an invalid item in dictionary
    import ctypes

    idx = len(dict_invalid_index.indices) - 1
    sizeof_int32 = 4
    raw_tab = ctypes.c_int32.from_address(
        dict_invalid_index.indices.buffers()[1].address + idx * sizeof_int32
    )
    raw_tab.value = len(dictionary)

    names = [
        "dict_invalid_index",
    ]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    lyr.CreateFieldFromPyArrowSchema(table.schema.field(0))

    with pytest.raises(
        Exception,
        match="Feature 4, field dict_invalid_index: invalid dictionary index: 3",
    ):
        lyr.WritePyArrow(table)


###############################################################################


def test_ogr_mem_arrow_json():
    pytest.importorskip("pyarrow")

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("foo")
    field_def = ogr.FieldDefn("field_json", ogr.OFTString)
    field_def.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(field_def)

    stream = lyr.GetArrowStreamAsPyArrow()
    field_schema = stream.schema["field_json"]
    # Since pyarrow 18, the field type is extension<arrow.json>
    if str(field_schema.type) != "extension<arrow.json>":
        md = field_schema.metadata
        assert b"ARROW:extension:name" in md
        assert md[b"ARROW:extension:name"] == b"arrow.json"


###############################################################################
# Test Layer.GetDataset()


def test_ogr_mem_lyr_get_dataset():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("foo")
    lyr = ds.CreateLayer("test")
    assert lyr.GetDataset().GetDescription() == "foo"


###############################################################################
# Test Dataset.GetExtent()


def test_ogr_mem_extent():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 10)
    assert ds.GetExtent() is None
    assert ds.GetExtentWGS84LongLat() is None

    srs = osr.SpatialReference(epsg=3857)
    lyr = ds.CreateLayer("test", srs=srs)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((1000 1980,1000 2000,1002 2000,1002 1980,1000 1980))"
        )
    )
    lyr.CreateFeature(f)

    # OGRMemLayer doesn't report OLCFastGetExtent...
    assert not ds.TestCapability(gdal.GDsCFastGetExtent)
    assert ds.GetExtent() == (1000, 1002, 1980, 2000)
    assert not ds.TestCapability(gdal.GDsCFastGetExtentWGS84LongLat)
    assert ds.GetExtentWGS84LongLat() == pytest.approx(
        (
            0.008983152841195215,
            0.009001119146877604,
            0.017786642339884726,
            0.01796630538796444,
        )
    )


###############################################################################

# Test support for https://github.com/apache/arrow/blob/main/docs/source/format/CanonicalExtensions.rst#timestamp-with-offset


def test_ogr_mem_timestamp_with_offset_arrow_api(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    fld_defn = ogr.FieldDefn("dt", ogr.OFTDateTime)
    fld_defn.SetTZFlag(ogr.TZFLAG_MIXED_TZ)
    src_lyr.CreateField(fld_defn)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["id"] = 1
    f["dt"] = "2025-12-20T12:34:56+0345"
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["id"] = 2
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["id"] = 3
    f["dt"] = "2025-12-20T12:34:56-0745"
    src_lyr.CreateFeature(f)

    # Test without timezone
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["id"] = 4
    f["dt"] = "2025-12-20T22:30:56"
    src_lyr.CreateFeature(f)

    with gdal.config_option("OGR2OGR_USE_ARROW_API", "YES"):
        ds = gdal.VectorTranslate("", src_ds, format="MEM")

    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(1)
    assert fld_defn.GetType() == ogr.OFTDateTime

    f = lyr.GetNextFeature()
    assert f["id"] == 1
    assert f["dt"] == "2025/12/20 12:34:56+0345"

    f = lyr.GetNextFeature()
    assert f["id"] == 2
    assert f["dt"] is None

    f = lyr.GetNextFeature()
    assert f["id"] == 3
    assert f["dt"] == "2025/12/20 12:34:56-0745"

    f = lyr.GetNextFeature()
    assert f["id"] == 4
    assert f["dt"] == "2025/12/20 22:30:56"

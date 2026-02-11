#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic OGR functionality against test shapefiles.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import math
import os
import struct
import sys

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################


def test_ogr_basic_1():

    ds = ogr.Open("data/poly.shp")

    assert ds is not None

    assert isinstance(ds, ogr.DataSource)


###############################################################################
# Test Feature counting.


def test_ogr_basic_2():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayerByName("poly")

    assert lyr.GetName() == "poly"
    assert lyr.GetGeomType() == ogr.wkbPolygon

    assert lyr.GetLayerDefn().GetName() == "poly"
    assert lyr.GetLayerDefn().GetGeomType() == ogr.wkbPolygon

    count = lyr.GetFeatureCount()
    assert count == 10, (
        "Got wrong count with GetFeatureCount() - %d, expecting 10" % count
    )

    # Now actually iterate through counting the features and ensure they agree.
    lyr.ResetReading()

    count2 = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        count2 = count2 + 1
        feat = lyr.GetNextFeature()

    assert count2 == 10, (
        "Got wrong count with GetNextFeature() - %d, expecting 10" % count2
    )


###############################################################################
# Test Spatial Query.


def test_ogr_basic_3():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayerByName("poly")

    minx = 479405
    miny = 4762826
    maxx = 480732
    maxy = 4763590

    ###########################################################################
    # Create query geometry.

    ring = ogr.Geometry(type=ogr.wkbLinearRing)
    ring.AddPoint(minx, miny)
    ring.AddPoint(maxx, miny)
    ring.AddPoint(maxx, maxy)
    ring.AddPoint(minx, maxy)
    ring.AddPoint(minx, miny)

    poly = ogr.Geometry(type=ogr.wkbPolygon)
    poly.AddGeometryDirectly(ring)

    lyr.SetSpatialFilter(poly)
    lyr.SetSpatialFilter(lyr.GetSpatialFilter())
    lyr.ResetReading()

    count = lyr.GetFeatureCount()
    assert count == 1, (
        "Got wrong feature count with spatial filter, expected 1, got %d" % count
    )

    feat1 = lyr.GetNextFeature()
    feat2 = lyr.GetNextFeature()

    assert (
        feat1 is not None and feat2 is None
    ), "Got too few or too many features with spatial filter."

    lyr.SetSpatialFilter(None)
    count = lyr.GetFeatureCount()
    assert count == 10, (
        "Clearing spatial query may not have worked properly, getting\n%d features instead of expected 10 features."
        % count
    )


###############################################################################
# Test GetDriver().


def test_ogr_basic_4():

    ds = ogr.Open("data/poly.shp")
    driver = ds.GetDriver()
    assert driver is not None, "GetDriver() returns None"

    assert driver.GetName() == "ESRI Shapefile", (
        "Got wrong driver name: " + driver.GetName()
    )


###############################################################################
# Test attribute query on special field fid - per bug 1468.


def test_ogr_basic_5():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayerByName("poly")

    lyr.SetAttributeFilter("FID = 3")
    lyr.ResetReading()

    feat1 = lyr.GetNextFeature()
    feat2 = lyr.GetNextFeature()

    lyr.SetAttributeFilter(None)

    assert feat1 is not None and feat2 is None, "unexpected result count."

    assert feat1.GetFID() == 3, "got wrong feature."


###############################################################################
# Test opening a dataset with an empty string and a non existing dataset
def test_ogr_basic_6():

    with pytest.raises(Exception):
        ogr.Open("")

    with pytest.raises(Exception):
        ogr.Open("non_existing")


###############################################################################
# Test ogr.Feature.Equal()


def test_ogr_basic_7():

    feat_defn = ogr.FeatureDefn()
    feat = ogr.Feature(feat_defn)
    assert feat.Equal(feat)

    try:
        feat.SetFieldIntegerList
    except AttributeError:
        pytest.skip()

    feat_clone = feat.Clone()
    assert feat.Equal(feat_clone)

    # We MUST delete now as we are changing the feature defn afterwards!
    # Crash guaranteed otherwise
    feat = None
    feat_clone = None

    field_defn = ogr.FieldDefn("field1", ogr.OFTInteger)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field2", ogr.OFTReal)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field3", ogr.OFTString)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field4", ogr.OFTIntegerList)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field5", ogr.OFTRealList)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field6", ogr.OFTStringList)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field7", ogr.OFTDate)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field8", ogr.OFTTime)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field9", ogr.OFTDateTime)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field10", ogr.OFTBinary)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field11", ogr.OFTInteger64)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn("field12", ogr.OFTReal)
    feat_defn.AddFieldDefn(field_defn)

    feat = ogr.Feature(feat_defn)
    feat.SetFID(100)
    feat.SetField(0, 1)
    feat.SetField(1, 1.2)
    feat.SetField(2, "A")
    feat.SetFieldIntegerList(3, [1, 2])
    feat.SetFieldDoubleList(4, [1.2, 3.4, math.nan])
    feat.SetFieldStringList(5, ["A", "B"])
    feat.SetField(6, 2010, 1, 8, 22, 48, 15, 4)
    feat.SetField(7, 2010, 1, 8, 22, 48, 15, 4)
    feat.SetField(8, 2010, 1, 8, 22, 48, 15, 4)
    feat.SetFieldBinaryFromHexString(9, "012345678ABCDEF")
    feat.SetField(10, 1234567890123)
    feat.SetField(11, math.nan)

    feat_clone = feat.Clone()
    if not feat.Equal(feat_clone):
        feat.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    geom = ogr.CreateGeometryFromWkt("POINT(0 1)")
    feat_almost_clone.SetGeometry(geom)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    geom = ogr.CreateGeometryFromWkt("POINT(0 1)")
    feat.SetGeometry(geom)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_clone = feat.Clone()
    if not feat.Equal(feat_clone):
        feat.DumpReadable()
        feat_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFID(99)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(0, 2)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(1, 2.2)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(2, "B")
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldIntegerList(3, [1, 2, 3])
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldIntegerList(3, [1, 3])
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldDoubleList(4, [1.2, 3.4])
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldDoubleList(4, [1.2, 3.5, math.nan])
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldDoubleList(4, [1.2, 3.4, 0])
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldStringList(5, ["A", "B", "C"])
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldStringList(5, ["A", "D"])
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    for num_field in [6, 7, 8]:
        for i in range(7):
            feat_almost_clone = feat.Clone()
            feat_almost_clone.SetField(
                num_field,
                2010 + (i == 0),
                1 + (i == 1),
                8 + (i == 2),
                22 + (i == 3),
                48 + (i == 4),
                15 + (i == 5),
                4 + (i == 6),
            )
            if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
                feat.DumpReadable()
                feat_almost_clone.DumpReadable()
                pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldBinaryFromHexString(9, "00")
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(10, 2)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(10, 2)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(11, 0)
    if feat.Equal(feat_almost_clone) or feat_almost_clone.Equal(feat):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        pytest.fail()


###############################################################################
# Issue several RegisterAll() to check that OGR drivers are good citizens


def test_ogr_basic_8():

    ogr.RegisterAll()
    ogr.RegisterAll()
    ogr.RegisterAll()


###############################################################################
# Test ogr.GeometryTypeToName (#4871)


def test_ogr_basic_9():

    geom_type_tuples = [
        [ogr.wkbUnknown, "Unknown (any)"],
        [ogr.wkbPoint, "Point"],
        [ogr.wkbLineString, "Line String"],
        [ogr.wkbPolygon, "Polygon"],
        [ogr.wkbMultiPoint, "Multi Point"],
        [ogr.wkbMultiLineString, "Multi Line String"],
        [ogr.wkbMultiPolygon, "Multi Polygon"],
        [ogr.wkbGeometryCollection, "Geometry Collection"],
        [ogr.wkbNone, "None"],
        [ogr.wkbUnknown | ogr.wkb25DBit, "3D Unknown (any)"],
        [ogr.wkbPoint25D, "3D Point"],
        [ogr.wkbLineString25D, "3D Line String"],
        [ogr.wkbPolygon25D, "3D Polygon"],
        [ogr.wkbMultiPoint25D, "3D Multi Point"],
        [ogr.wkbMultiLineString25D, "3D Multi Line String"],
        [ogr.wkbMultiPolygon25D, "3D Multi Polygon"],
        [ogr.wkbGeometryCollection25D, "3D Geometry Collection"],
        [123456, "Unrecognized: 123456"],
    ]

    for geom_type_tuple in geom_type_tuples:
        assert ogr.GeometryTypeToName(geom_type_tuple[0]) == geom_type_tuple[1]


###############################################################################
# Run test_ogrsf -all_drivers


def test_ogr_basic_10():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    # --config OPENFILEGDB_REPRODUCIBLE_UUID=YES helps avoiding unsigned-integer-overflow
    # under UBSAN.
    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -all_drivers --config OPENFILEGDB_REPRODUCIBLE_UUID=YES"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret, ret


###############################################################################
# Test OFSTBoolean, OFSTInt16 and OFSTFloat32


def test_ogr_basic_12():

    # boolean integer
    feat_def = ogr.FeatureDefn()
    assert ogr.GetFieldSubTypeName(ogr.OFSTBoolean) == "Boolean"
    field_def = ogr.FieldDefn("fld", ogr.OFTInteger)
    field_def.SetSubType(ogr.OFSTBoolean)
    assert field_def.GetSubType() == ogr.OFSTBoolean
    feat_def.AddFieldDefn(field_def)

    f = ogr.Feature(feat_def)
    f.SetField("fld", 0)
    f.SetField("fld", 1)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f.SetField("fld", 2)
    assert gdal.GetLastErrorMsg() != ""
    assert isinstance(f.GetField("fld"), bool)
    assert f.GetField("fld") == True

    f.SetField("fld", "0")
    f.SetField("fld", "1")
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f.SetField("fld", "2")
    assert gdal.GetLastErrorMsg() != ""
    assert f.GetField("fld") == True

    gdal.ErrorReset()
    with gdal.quiet_errors():
        field_def = ogr.FieldDefn("fld", ogr.OFTString)
        field_def.SetSubType(ogr.OFSTBoolean)
    assert gdal.GetLastErrorMsg() != ""
    assert field_def.GetSubType() == ogr.OFSTNone

    # boolean list
    feat_def = ogr.FeatureDefn()
    field_def = ogr.FieldDefn("fld", ogr.OFTIntegerList)
    field_def.SetSubType(ogr.OFSTBoolean)
    assert field_def.GetSubType() == ogr.OFSTBoolean
    feat_def.AddFieldDefn(field_def)

    f = ogr.Feature(feat_def)
    f.SetFieldIntegerList(0, [False, True])
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f.SetFieldIntegerList(0, [0, 1, 2, 1])
    assert gdal.GetLastErrorMsg() != ""
    for x in f.GetField("fld"):
        assert isinstance(x, bool)
    assert f.GetField("fld") == [False, True, True, True]

    # int16 integer
    feat_def = ogr.FeatureDefn()
    assert ogr.GetFieldSubTypeName(ogr.OFSTInt16) == "Int16"
    field_def = ogr.FieldDefn("fld", ogr.OFTInteger)
    field_def.SetSubType(ogr.OFSTInt16)
    assert field_def.GetSubType() == ogr.OFSTInt16
    feat_def.AddFieldDefn(field_def)

    f = ogr.Feature(feat_def)
    f.SetField("fld", -32768)
    f.SetField("fld", 32767)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f.SetField("fld", -32769)
    assert gdal.GetLastErrorMsg() != ""
    assert f.GetField("fld") == -32768
    gdal.ErrorReset()
    with gdal.quiet_errors():
        f.SetField("fld", 32768)
    assert gdal.GetLastErrorMsg() != ""
    assert f.GetField("fld") == 32767

    gdal.ErrorReset()
    with gdal.quiet_errors():
        field_def = ogr.FieldDefn("fld", ogr.OFTString)
        field_def.SetSubType(ogr.OFSTInt16)
    assert gdal.GetLastErrorMsg() != ""
    assert field_def.GetSubType() == ogr.OFSTNone

    # float32
    feat_def = ogr.FeatureDefn()
    assert ogr.GetFieldSubTypeName(ogr.OFSTFloat32) == "Float32"
    field_def = ogr.FieldDefn("fld", ogr.OFTReal)
    field_def.SetSubType(ogr.OFSTFloat32)
    assert field_def.GetSubType() == ogr.OFSTFloat32
    feat_def.AddFieldDefn(field_def)

    if False:  # pylint: disable=using-constant-test
        f = ogr.Feature(feat_def)
        gdal.ErrorReset()
        f.SetField("fld", "1.23")
        assert gdal.GetLastErrorMsg() == ""
        gdal.ErrorReset()
        with gdal.quiet_errors():
            f.SetField("fld", 1.230000000001)
        assert gdal.GetLastErrorMsg() != ""
        if f.GetField("fld") == pytest.approx(1.23, abs=1e-8):
            f.DumpReadable()
            pytest.fail()

    gdal.ErrorReset()
    with gdal.quiet_errors():
        field_def = ogr.FieldDefn("fld", ogr.OFSTFloat32)
        field_def.SetSubType(ogr.OFSTInt16)
    assert gdal.GetLastErrorMsg() != ""
    assert field_def.GetSubType() == ogr.OFSTNone


###############################################################################
# Test OGRParseDate (#6452)


@pytest.mark.parametrize(
    "input,expected",
    [
        ("2016/01/01", "2016/01/01 00:00:00"),
        ("2016/01/01 12:34", "2016/01/01 12:34:00"),
        ("2016/01/01 12:34:56", "2016/01/01 12:34:56"),
        ("2016/01/01 12:34:56.789", "2016/01/01 12:34:56.789"),
        ("2016/12/31", "2016/12/31 00:00:00"),
        ("-2016/12/31", "-2016/12/31 00:00:00"),
        ("2016-12-31", "2016/12/31 00:00:00"),
        ("0080/01/01", "0080/01/01 00:00:00"),
        ("80/01/01", "1980/01/01 00:00:00"),
        ("0010/01/01", "0010/01/01 00:00:00"),
        ("9/01/01", "2009/01/01 00:00:00"),
        ("10/01/01", "2010/01/01 00:00:00"),
        ("2016-13-31", None),
        ("2016-00-31", None),
        ("2016-01-32", None),
        ("2016-01-00", None),
        ("0/01/01", "2000/01/01 00:00:00"),
        ("00/01/01", "2000/01/01 00:00:00"),
        ("00/00/00", None),
        ("000/00/00", None),
        ("0000/00/00", None),
        ("//foo", None),
    ],
)
def test_ogr_basic_13(input, expected):
    feat_defn = ogr.FeatureDefn("test")
    field_defn = ogr.FieldDefn("date", ogr.OFTDateTime)
    feat_defn.AddFieldDefn(field_defn)
    f = ogr.Feature(feat_defn)
    f.SetField("date", input)
    assert f.GetField("date") == expected


###############################################################################
# Test ogr.Open(.) in an empty directory


def test_ogr_basic_14(tmp_path):

    old_dir = os.getcwd()
    os.chdir(tmp_path)
    try:
        with pytest.raises(Exception):
            ogr.Open(".")
    finally:
        os.chdir(old_dir)


###############################################################################
# Test exceptions with OGRErr return code


def test_ogr_basic_15():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)

    with gdal.ExceptionMgr(useExceptions=True):
        with pytest.raises(
            Exception,
            match=r".*CreateFeature : unsupported operation on a read-only datasource.*",
        ):
            lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))


###############################################################################
# Test issue with Python 3.5 and older SWIG (#6749)


def ogr_basic_16_make_geom():
    geom = ogr.Geometry(ogr.wkbPoint)
    geom.AddPoint_2D(0, 0)
    return geom


def ogr_basic_16_gen_list(N):
    for i in range(N):
        ogr_basic_16_make_geom()
        yield i


def test_ogr_basic_16():

    assert list(ogr_basic_16_gen_list(2)) == [0, 1]


def test_ogr_basic_getfielddefn():

    defn = ogr.FeatureDefn()
    defn.AddFieldDefn(ogr.FieldDefn("intfield", ogr.OFTInteger))

    by_index = defn.GetFieldDefn(0)
    by_name = defn.GetFieldDefn("intfield")

    assert by_index.this == by_name.this

    with gdaltest.enable_exceptions():
        with pytest.raises(Exception, match="Invalid"):
            defn.GetFieldDefn("does_not_exist")


def test_ogr_basic_invalid_unicode():

    val = "\udcfc"

    try:
        ogr.Open(val)
    except Exception:
        pass

    data_source = ogr.GetDriverByName("MEM").CreateDataSource("")
    layer = data_source.CreateLayer("test")
    layer.CreateField(ogr.FieldDefn("attr", ogr.OFTString))
    feature = ogr.Feature(layer.GetLayerDefn())
    try:
        feature.SetField("attr", val)
    except Exception:
        pass


def test_ogr_basic_dataset_slice():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ds.CreateLayer("lyr1")
    ds.CreateLayer("lyr2")
    ds.CreateLayer("lyr3")

    lyrs = [lyr.GetName() for lyr in ds[1:3]]
    assert lyrs == ["lyr2", "lyr3"]

    lyrs = [lyr.GetName() for lyr in ds[0:4]]
    assert lyrs == ["lyr1", "lyr2", "lyr3"]

    lyrs = [lyr.GetName() for lyr in ds[0:3:2]]
    assert lyrs == ["lyr1", "lyr3"]


def test_ogr_basic_dataset_iter():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ds.CreateLayer("lyr1")
    ds.CreateLayer("lyr2")
    ds.CreateLayer("lyr3")

    layers = []

    assert len(ds) == 3

    for lyr in ds:
        layers.append(lyr)

    assert len(layers) == 3


def test_ogr_basic_dataset_getitem():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ds.CreateLayer("lyr1")
    ds.CreateLayer("lyr2")
    ds.CreateLayer("lyr3")

    assert ds[2].this == ds.GetLayer(2).this

    with pytest.raises(IndexError):
        ds[3]


def test_ogr_basic_feature_iterator():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)

    count = 0
    for f in lyr:
        count += 1
    assert count == 10

    count = 0
    for f in lyr:
        count += 1
    assert count == 10


def test_ogr_basic_dataset_copy_layer_dst_srswkt():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = ds.CreateLayer("lyr1")
    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    out_lyr = ds.CopyLayer(src_lyr, "lyr2", options=["DST_SRSWKT=" + sr.ExportToWkt()])
    assert out_lyr.GetSpatialRef() is not None
    assert out_lyr.GetSpatialRef().IsSame(sr)


def test_ogr_basic_dataset_copy_layer_metadata():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = ds.CreateLayer("lyr1")
    src_lyr.SetMetadataItem("foo", "bar")
    out_lyr = ds.CopyLayer(src_lyr, "lyr2")
    assert out_lyr.GetMetadata() == {"foo": "bar"}


def test_ogr_basic_dataset_no_copy_layer_metadata():

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    src_lyr = ds.CreateLayer("lyr1")
    src_lyr.SetMetadataItem("foo", "bar")
    out_lyr = ds.CopyLayer(src_lyr, "lyr2", options=["COPY_MD=NO"])
    assert out_lyr.GetMetadata() == {}


def test_ogr_basic_field_alternative_name():
    field_defn = ogr.FieldDefn("test")

    assert field_defn.GetAlternativeName() == ""

    field_defn.SetAlternativeName("my alias")
    assert field_defn.GetAlternativeName() == "my alias"


def test_ogr_basic_float32_formatting():
    def cast_as_float(x):
        return struct.unpack("f", struct.pack("f", x))[0]

    feat_defn = ogr.FeatureDefn("test")
    fldn_defn = ogr.FieldDefn("float32", ogr.OFTReal)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    feat_defn.AddFieldDefn(fldn_defn)

    f = ogr.Feature(feat_defn)
    for x in ("0.35", "0.15", "123.0", "0.12345678", "1.2345678e-15"):
        f["float32"] = cast_as_float(float(x))
        assert (
            f.GetFieldAsString("float32").replace("e+0", "e+").replace("e-0", "e-") == x
        )

    feat_defn = ogr.FeatureDefn("test")
    fldn_defn = ogr.FieldDefn("float32_list", ogr.OFTRealList)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    feat_defn.AddFieldDefn(fldn_defn)

    f = ogr.Feature(feat_defn)
    f["float32_list"] = [cast_as_float(0.35), math.nan, math.inf, -math.inf]
    assert f.GetFieldAsString("float32_list") == "(4:0.35,nan,inf,-inf)"


###############################################################################


def test_ogr_basic_get_geometry_types():
    """Test Layer.GetGeometryTypes()"""

    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ds.CreateLayer("layer")
    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbUnknown))

    assert lyr.GetGeometryTypes() == {}

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    lyr.SetIgnoredFields(["str2", ""])
    assert lyr.GetGeometryTypes() == {ogr.wkbNone: 1}

    # Test that ignored column status is properly reset
    assert lyr.GetLayerDefn().GetFieldDefn(0).IsIgnored() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(1).IsIgnored() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsIgnored() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).IsIgnored() == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes(callback=lambda x, y, z: 1) == {ogr.wkbNone: 2}

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {ogr.wkbNone: 2, ogr.wkbPoint: 1}

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON EMPTY"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
    }
    assert lyr.GetGeometryTypes(flags=ogr.GGT_STOP_IF_MIXED) == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
    }

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING EMPTY"))
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
    }
    assert lyr.GetGeometryTypes(geom_field=0, flags=ogr.GGT_STOP_IF_MIXED) == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
    }

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION Z(TIN Z(((0 0 0,0 1 0,1 1 0,0 0 0))))"
        )
    )
    lyr.CreateFeature(f)
    assert lyr.GetGeometryTypes() == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
        ogr.wkbGeometryCollection25D: 1,
    }
    assert lyr.GetGeometryTypes(flags=ogr.GGT_GEOMCOLLECTIONZ_TINZ) == {
        ogr.wkbNone: 2,
        ogr.wkbPoint: 1,
        ogr.wkbPolygon: 1,
        ogr.wkbLineString: 1,
        ogr.wkbTINZ: 1,
    }

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            lyr.GetGeometryTypes(callback=lambda x, y, z: 0)

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            lyr.GetGeometryTypes(geom_field=2)


###############################################################################
# Test ogr.ExceptionMgr()


def test_ogr_exceptions():

    with pytest.raises(Exception):
        with ogr.ExceptionMgr():
            ogr.CreateGeometryFromWkt("invalid")


def test_ogr_basic_test_future_warning_exceptions():

    python_exe = sys.executable
    cmd = '%s -c "from osgeo import ogr; ' % python_exe + (
        "ogr.Open('data/poly.shp');" ' " '
    )
    try:
        _, err = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        pytest.skip("got exception %s" % str(e))
    assert "FutureWarning: Neither ogr.UseExceptions()" in err


###############################################################################
# check feature defn access after layer has been destroyed


def test_feature_defn_use_after_layer_del():
    with ogr.Open("data/poly.shp") as ds:
        lyr = ds.GetLayer(0)

        defn1 = lyr.GetLayerDefn()
        defn2 = lyr.GetLayerDefn()

        with pytest.raises(
            Exception,
            match=r"OGRFeatureDefn::AddFieldDefn\(\) not allowed on a sealed object",
        ):
            lyr.GetLayerDefn().AddFieldDefn(ogr.FieldDefn("cookie", ogr.OFTInteger))

        assert defn1.GetReferenceCount() == 3
        del defn2
        assert defn1.GetReferenceCount() == 2

    del lyr

    assert defn1.GetReferenceCount() == 1
    assert defn1.GetFieldCount() == 3


###############################################################################
# Test CreateDataSource context manager


def test_ogr_basic_create_data_source_context_manager(tmp_path):
    fname = str(tmp_path / "out.shp")

    drv = ogr.GetDriverByName("ESRI Shapefile")
    with drv.CreateDataSource(fname) as ds:
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (0 0)"))
        lyr.CreateFeature(f)

    # Make sure we don't crash when accessing ds after it has been closed
    with pytest.raises(Exception):
        ds.GetLayerByName("test")

    # Make sure the feature has actually been written
    ds_in = ogr.Open(fname)
    lyr_in = ds_in.GetLayer(0)

    assert lyr_in.GetFeatureCount() == 1


###############################################################################
# check access after datasource has closed


def test_datasource_use_after_close_1():
    ds = ogr.Open("data/poly.shp")

    assert ds is not None

    ds.Close()

    with pytest.raises(Exception):
        ds.GetLayer(0)


def test_datasource_use_after_close_2():
    ds = ogr.Open("data/poly.shp")

    ds2 = ds

    ds.Close()

    with pytest.raises(Exception):
        ds.GetLayer(0)

    with pytest.raises(Exception):
        ds2.GetLayer(0)


@pytest.mark.filterwarnings("ignore::DeprecationWarning")
def test_datasource_use_after_destroy():
    ds = ogr.Open("data/poly.shp")

    ds2 = ds

    ds.Destroy()

    with pytest.raises(Exception):
        ds.GetLayer(0)

    with pytest.raises(Exception):
        ds2.GetLayer(0)


@pytest.mark.filterwarnings("ignore::DeprecationWarning")
def test_datasource_use_after_release():
    ds = ogr.Open("data/poly.shp")

    ds2 = ds

    ds.Release()

    with pytest.raises(Exception):
        ds.GetLayer(0)

    with pytest.raises(Exception):
        ds2.GetLayer(0)


def test_layer_use_after_datasource_close_1():
    with ogr.Open("data/poly.shp") as ds:
        lyr = ds.GetLayer(0)

    # Make sure ds.__exit__() has invalidated "lyr" so we don't crash here
    with pytest.raises(Exception):
        lyr.GetFeatureCount()


def test_layer_use_after_datasource_close_2():
    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayerByName("poly")

    del ds
    # Make sure ds.__del__() has invalidated "lyr" so we don't crash here
    with pytest.raises(Exception):
        lyr.GetFeatureCount()


def test_layer_use_after_datasource_close_3(tmp_path):
    fname = str(tmp_path / "test.shp")

    drv = ogr.GetDriverByName("ESRI Shapefile")

    with drv.CreateDataSource(fname) as ds:
        lyr = ds.CreateLayer("test")
        lyr2 = ds.CopyLayer(lyr, "test2")

    # Make sure ds.__exit__() has invalidated "lyr" so we don't crash here
    with pytest.raises(Exception):
        lyr.GetFeatureCount()

    # Make sure ds.__exit__() has invalidated "lyr2" so we don't crash here
    with pytest.raises(Exception):
        lyr2.GetFeatureCount()


@pytest.mark.filterwarnings("ignore::DeprecationWarning")
def test_layer_use_after_datasource_destroy():
    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayerByName("poly")

    ds.Destroy()

    # Make sure ds.Destroy() has invalidated "lyr" so we don't crash here
    with pytest.raises(Exception):
        lyr.GetFeatureCount()


@pytest.mark.filterwarnings("ignore::DeprecationWarning")
def test_layer_use_after_datasource_release():
    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayerByName("poly")

    ds.Release()

    # Make sure ds.Release() has invalidated "lyr" so we don't crash here
    with pytest.raises(Exception):
        lyr.GetFeatureCount()


def test_feature_use_after_destroy():

    defn = ogr.FeatureDefn()
    feature = ogr.Feature(defn)
    feature.Destroy()

    with pytest.raises(Exception):
        feature.DumpReadable()


def test_feature_defn_use_after_feature_delete():

    defn = ogr.FeatureDefn("mydef")
    feature = ogr.Feature(defn)
    del defn

    defn2 = feature.GetDefnRef()
    del feature

    assert defn2.GetName() == "mydef"


def test_layer_get_defn_refcount():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)

    for i in range(10):
        defn = lyr.GetLayerDefn()

    assert defn.GetReferenceCount() == 2


def test_feature_get_defn_refcount():

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)
    feature = lyr.GetNextFeature()

    for i in range(10):
        defn = feature.GetDefnRef()

    assert defn.GetReferenceCount() == 3


@pytest.mark.parametrize("destroy_method", ("del", "Destroy"))
def test_geom_use_after_feature_delete_1(destroy_method):

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if destroy_method == "del":
        del feat
    elif destroy_method == "Destroy":
        feat.Destroy()

    with pytest.raises(Exception):
        geom.ExportToWkt()


@pytest.mark.parametrize("arg_type", ("int", "string"))
def test_geom_use_after_feature_delete_2(arg_type):

    ds = ogr.Open("data/poly.shp")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    if arg_type == "int":
        geom = feat.GetGeomFieldRef(0)
    elif arg_type == "string":
        geom = feat.GetGeomFieldRef("")

    del feat

    with pytest.raises(Exception):
        geom.ExportToWkt()


def test_geom_use_after_transfer_to_feature_1(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(tmp_vsimem / "test.shp")
    lyr = ds.CreateLayer("test")

    point = ogr.Geometry(ogr.wkbPoint)
    feature = ogr.Feature(lyr.GetLayerDefn())
    feature.SetGeometryDirectly(point)
    del feature

    with pytest.raises(Exception):
        point.ExportToWkt()


@pytest.mark.parametrize("arg_type", ("int", "string"))
def test_geom_use_after_transfer_to_feature(tmp_vsimem, arg_type):

    lyr_defn = ogr.FeatureDefn()

    gfld_defn = ogr.GeomFieldDefn()
    gfld_defn.SetName("geom_obj")

    lyr_defn.AddGeomFieldDefn(gfld_defn)

    point = ogr.Geometry(ogr.wkbPoint)
    feature = ogr.Feature(lyr_defn)
    if arg_type == "string":
        with pytest.raises(RuntimeError, match="Invalid field name"):
            feature.SetGeomFieldDirectly("wrong_name", point) == ogr.OGRERR_FAILURE

        feature.SetGeomFieldDirectly("geom_obj", point)
    elif arg_type == "int":
        feature.SetGeomFieldDirectly(0, point)

    del feature

    with pytest.raises(Exception):
        point.ExportToWkt()


def test_general_cmd_line_processor(tmp_path):

    processed = ogr.GeneralCmdLineProcessor(
        ["program", 2, tmp_path / "a_path", "a_string"]
    )
    assert processed == ["program", "2", str(tmp_path / "a_path"), "a_string"]


def test_driver_open_throw_1():

    with gdaltest.enable_exceptions():
        drv = ogr.GetDriverByName("ESRI Shapefile")

        assert isinstance(drv, ogr.Driver)

        with pytest.raises(RuntimeError, match="No such file or directory"):
            drv.Open("does_not_exist.shp")


def test_driver_open_throw_2():

    with gdaltest.enable_exceptions():
        drv = ogr.GetDriverByName("MapInfo File")
        if not drv:
            pytest.skip("MapInfo driver not available")

        assert isinstance(drv, ogr.Driver)

        with pytest.raises(RuntimeError, match="not recognized"):
            drv.Open("data/poly.shp")


def test_ogr_basic_dataset_get_spatial_ref():

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    assert ds.GetSpatialRef() is None

    srs = osr.SpatialReference(epsg=4326)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    srs2 = osr.SpatialReference(epsg=4258)
    srs2.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CreateLayer("one", srs=srs)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = ds.CreateLayer("one", srs=srs)
    geom_field_defn = ogr.GeomFieldDefn("second")
    geom_field_defn.SetSpatialRef(srs)
    lyr.CreateGeomField(geom_field_defn)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = ds.CreateLayer("one", srs=srs)
    geom_field_defn = ogr.GeomFieldDefn("second")
    geom_field_defn.SetSpatialRef(srs2)
    lyr.CreateGeomField(geom_field_defn)
    assert ds.GetSpatialRef() is None

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CreateLayer("one", srs=srs)
    ds.CreateLayer("two", srs=srs)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CreateLayer("one", srs=srs)
    ds.CreateLayer("two", srs=srs2)
    assert ds.GetSpatialRef() is None

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CreateLayer("one", srs=None)
    ds.CreateLayer("two", srs=srs2)
    assert ds.GetSpatialRef() is None

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CreateLayer("one", srs=None)
    ds.CreateLayer("two", srs=None)
    assert ds.GetSpatialRef() is None

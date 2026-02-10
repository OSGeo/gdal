#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FGDB driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = [
    pytest.mark.require_driver("FileGDB"),
    pytest.mark.random_order(disabled=True),
]


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################


@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    yield

    # The SDK messes somehow with the locale, which cause issues in
    # other test files, such as gcore/basic_test.py, which assumes English
    # error messages
    import locale

    locale.setlocale(locale.LC_ALL, "C")


###############################################################################


@pytest.fixture(autouse=True, scope="module")
def openfilegdb_drv():
    drv = ogr.GetDriverByName("OpenFileGDB")
    yield drv


###############################################################################


@pytest.fixture(autouse=True, scope="module")
def fgdb_drv(openfilegdb_drv):
    drv = ogr.GetDriverByName("FileGDB")
    if drv is None:
        pytest.skip("FileGDB driver not available", allow_module_level=True)

    if openfilegdb_drv is not None:
        openfilegdb_drv.Deregister()

    yield drv

    if openfilegdb_drv is not None:
        drv.Deregister()
        # Force OpenFileGDB first
        openfilegdb_drv.Register()
        drv.Register()


###############################################################################


@pytest.fixture()
def ogrsf_path():
    import test_cli_utilities

    path = test_cli_utilities.get_test_ogrsf_path()
    if path is None:
        pytest.skip("ogrsf test utility not found")

    return path


###############################################################################
# Write and read back various geometry types


@pytest.fixture()
def test_gdb_datalist():
    return [
        ["none", ogr.wkbNone, None],
        ["point", ogr.wkbPoint, "POINT (1 2)"],
        ["multipoint", ogr.wkbMultiPoint, "MULTIPOINT (1 2,3 4)"],
        [
            "linestring",
            ogr.wkbLineString,
            "LINESTRING (1 2,3 4)",
            "MULTILINESTRING ((1 2,3 4))",
        ],
        [
            "multilinestring",
            ogr.wkbMultiLineString,
            "MULTILINESTRING ((1 2,3 4),(5 6,7 8))",
        ],
        [
            "polygon",
            ogr.wkbPolygon,
            "POLYGON ((0 0,0 1,1 1,1 0,0 0))",
            "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))",
        ],
        [
            "multipolygon",
            ogr.wkbMultiPolygon,
            "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))",
        ],
        ["point25D", ogr.wkbPoint25D, "POINT (1 2 3)"],
        ["multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT (1 2 -10,3 4 -20)"],
        [
            "linestring25D",
            ogr.wkbLineString25D,
            "LINESTRING (1 2 -10,3 4 -20)",
            "MULTILINESTRING ((1 2 -10,3 4 -20))",
        ],
        [
            "multilinestring25D",
            ogr.wkbMultiLineString25D,
            "MULTILINESTRING ((1 2 -10,3 4 -20))",
        ],
        [
            "polygon25D",
            ogr.wkbPolygon25D,
            "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))",
            "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))",
        ],
        [
            "multipolygon25D",
            ogr.wkbMultiPolygon25D,
            "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))",
        ],
        [
            "multipatch",
            ogr.wkbMultiPolygon25D,
            "GEOMETRYCOLLECTION Z (TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0))),TIN Z (((10.0 0.0 0,10.0 1.0 0,11.0 0.0 0,10.0 0.0 0)),((10.0 0.0 0,11.0 0.0 0,10.0 -1.0 0,10.0 0.0 0))),TIN Z (((5.0 0.0 0,5.0 1.0 0,6.0 0.0 0,5.0 0.0 0))),MULTIPOLYGON Z (((100.0 0.0 0,100.0 1.0 0,101.0 1.0 0,101.0 0.0 0,100.0 0.0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0))))",
        ],
        [
            "tin",
            ogr.wkbTINZ,
            "TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0)))",
        ],
        ["null_polygon", ogr.wkbPolygon, None],
        ["empty_polygon", ogr.wkbPolygon, "POLYGON EMPTY", None],
    ]


@pytest.fixture()
def test_gdb(openfilegdb_drv, tmp_path, test_gdb_datalist):
    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = openfilegdb_drv.CreateDataSource(tmp_path / "test.gdb")

    options = [
        "COLUMN_TYPES=smallint=esriFieldTypeSmallInteger,float=esriFieldTypeSingle,guid=esriFieldTypeGUID,xml=esriFieldTypeXML"
    ]

    for data in test_gdb_datalist:
        if data[1] == ogr.wkbNone:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], options=options)
        elif data[0] == "multipatch":
            lyr = ds.CreateLayer(
                data[0],
                geom_type=data[1],
                srs=srs,
                options=["CREATE_MULTIPATCH=YES", options[0]],
            )
        else:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options=options)
        lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("smallint", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("float", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("adate", ogr.OFTDateTime))
        lyr.CreateField(ogr.FieldDefn("guid", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("xml", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("binary", ogr.OFTBinary))
        lyr.CreateField(ogr.FieldDefn("binary2", ogr.OFTBinary))
        fld_defn = ogr.FieldDefn("smallint2", ogr.OFTInteger)
        fld_defn.SetSubType(ogr.OFSTInt16)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn("float2", ogr.OFTReal)
        fld_defn.SetSubType(ogr.OFSTFloat32)
        lyr.CreateField(fld_defn)

        # We need at least 5 features so that test_ogrsf can test SetFeature()
        for i in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            if data[1] != ogr.wkbNone and data[2] is not None:
                feat.SetGeometry(ogr.CreateGeometryFromWkt(data[2]))
            feat.SetField("id", i + 1)
            feat.SetField("str", "foo_\xc3\xa9")
            feat.SetField("smallint", -13)
            feat.SetField("int", 123)
            feat.SetField("float", 1.5)
            feat.SetField("real", 4.56)
            feat.SetField("adate", "2013/12/26 12:34:56")
            feat.SetField("guid", "{12345678-9abc-DEF0-1234-567890ABCDEF}")
            feat.SetField("xml", "<foo></foo>")
            feat.SetField("binary", b"\x00\xff\x7f")
            feat.SetField("binary2", b"\x12\x34\x56")
            feat.SetField("smallint2", -32768)
            feat.SetField("float2", 1.5)
            lyr.CreateFeature(feat)

    del ds

    yield tmp_path / "test.gdb"


def test_ogr_fgdb_1(test_gdb, test_gdb_datalist):

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogr.Open(test_gdb)

    for data in test_gdb_datalist:
        lyr = ds.GetLayerByName(data[0])
        assert lyr.GetDataset().GetDescription() == ds.GetDescription()
        assert lyr.GetDataset().GetDriver().GetDescription() == "FileGDB"
        if data[1] != ogr.wkbNone:
            assert (
                lyr.GetSpatialRef().IsSame(
                    srs, options=["IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES"]
                )
                == 1
            )
        feat = lyr.GetNextFeature()
        if data[1] != ogr.wkbNone:
            try:
                expected_wkt = data[3]
            except IndexError:
                expected_wkt = data[2]
            ogrtest.check_feature_geometry(feat, expected_wkt)

        if (
            feat.GetField("id") != 1
            or feat.GetField("smallint") != -13
            or feat.GetField("int") != 123
            or feat.GetField("float") != 1.5
            or feat.GetField("real") != 4.56
            or feat.GetField("adate") != "2013/12/26 12:34:56+00"
            or feat.GetField("guid") != "{12345678-9ABC-DEF0-1234-567890ABCDEF}"
            or feat.GetField("xml") != "<foo></foo>"
            or feat.GetField("binary") != "00FF7F"
            or feat.GetField("binary2") != "123456"
            or feat.GetField("smallint2") != -32768
        ):
            feat.DumpReadable()
            pytest.fail()

        sql_lyr = ds.ExecuteSQL("GetLayerDefinition %s" % lyr.GetName())
        assert sql_lyr is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is None
        lyr.ResetReading()
        lyr.TestCapability("foo")
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("GetLayerMetadata %s" % lyr.GetName())
        assert sql_lyr is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition foo")
    assert sql_lyr is None

    sql_lyr = ds.ExecuteSQL("GetLayerMetadata foo")
    assert sql_lyr is None

    ds = None


###############################################################################
# Run test_ogrsf


def test_ogr_fgdb_2(ogrsf_path, test_gdb):
    ret = gdaltest.runexternal(
        ogrsf_path + f" -ro {test_gdb} --config OGR_SKIP OpenFileGDB"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Run ogr2ogr


@pytest.fixture()
def poly_gdb(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(
        test_cli_utilities.get_ogr2ogr_path()
        + f" -f filegdb {tmp_path}/poly.gdb data/poly.shp -nlt MULTIPOLYGON -a_srs None"
    )

    return tmp_path / "poly.gdb"


def test_ogr_fgdb_3(openfilegdb_drv, poly_gdb):

    import test_cli_utilities

    ds = ogr.Open(poly_gdb)
    assert not (ds is None or ds.GetLayerCount() == 0), "ogr2ogr failed"
    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    if openfilegdb_drv is None:
        # OpenFileGDB is required for CreateFeature() with a FID set
        pytest.skip("skipping test_ogrsf due to missing OpenFileGDB driver")

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f" {poly_gdb} --config DRIVER_WISHED FileGDB"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test SQL support


def test_ogr_fgdb_sql(poly_gdb):

    import test_cli_utilities

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    ds = ogr.Open(poly_gdb)

    ds.ExecuteSQL("CREATE INDEX idx_poly_eas_id ON poly(EAS_ID)")

    sql_lyr = ds.ExecuteSQL("SELECT * FROM POLY WHERE EAS_ID = 170", dialect="FileGDB")
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)
    ds = None


###############################################################################
# Test DeleteDataSource()


def test_ogr_fgdb_5(fgdb_drv, poly_gdb):

    assert fgdb_drv.DeleteDataSource(poly_gdb) == 0, "DeleteDataSource() failed"

    assert not os.path.exists(poly_gdb)


###############################################################################
# Test failed Open()


@gdaltest.disable_exceptions()
def test_ogr_fgdb_12(tmp_path):

    ds = ogr.Open(tmp_path / "non_existing.gdb")
    assert ds is None


@gdaltest.disable_exceptions()
def test_ogr_fgdb_12_bis(tmp_path):

    f = open(tmp_path / "dummy.gdb", "wb")
    f.close()

    ds = ogr.Open(tmp_path / "dummy.gdb")
    assert ds is None


@gdaltest.disable_exceptions()
def test_ogr_fgdb_12_ter(tmp_path):

    os.mkdir(tmp_path / "dummy.gdb")

    with gdal.quiet_errors():
        ds = ogr.Open(tmp_path / "dummy.gdb")
    assert ds is None


###############################################################################
# Test interleaved opening and closing of databases (#4270)


def test_ogr_fgdb_14(poly_gdb):

    for _ in range(3):
        ds1 = ogr.Open(poly_gdb)
        assert ds1 is not None
        ds2 = ogr.Open(poly_gdb)
        assert ds2 is not None
        ds2 = None
        ds1 = None


###############################################################################
# Test opening a FGDB with both SRID and LatestSRID set (#5638)


def test_ogr_fgdb_15(tmp_path):

    gdaltest.unzip(tmp_path, "data/filegdb/test3005.gdb.zip")
    ds = ogr.Open(tmp_path / "test3005.gdb")
    lyr = ds.GetLayer(0)
    got_wkt = lyr.GetSpatialRef().ExportToWkt()
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3005)
    expected_wkt = sr.ExportToWkt()
    assert got_wkt == expected_wkt
    ds = None


###############################################################################
# Test fix for #5674


def test_ogr_fgdb_16(openfilegdb_drv, fgdb_drv):
    if fgdb_drv is None or openfilegdb_drv is None:
        pytest.skip()

    try:
        gdaltest.unzip("tmp/cache", "data/filegdb/ESSENCE_NAIPF_ORI_PROV_sub93.gdb.zip")
    except OSError:
        pass
    try:
        os.stat("tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb")
    except OSError:
        pytest.skip()

    fgdb_drv.Deregister()

    # Force FileGDB first
    fgdb_drv.Register()
    openfilegdb_drv.Register()

    try:
        ds = ogr.Open("tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb")
        assert ds is not None
    finally:
        # Deregister OpenFileGDB again
        openfilegdb_drv.Deregister()

        shutil.rmtree("tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb")


###############################################################################
# Test not nullable fields


def test_ogr_fgdb_17(openfilegdb_drv, tmp_path):

    ds = openfilegdb_drv.CreateDataSource(tmp_path / "test.gdb")
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(
        "test", geom_type=ogr.wkbPoint, srs=sr, options=["GEOMETRY_NULLABLE=NO"]
    )
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    field_defn = ogr.FieldDefn("field_not_nullable", ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("field_nullable", ogr.OFTString)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_not_nullable", "not_null")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    ret = lyr.CreateFeature(f)
    assert ret == 0
    f = None

    # Error case: missing geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("field_not_nullable", "not_null")
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(f)
    assert ret != 0
    f = None

    # Error case: missing non-nullable field
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(f)
    assert ret != 0
    f = None

    ds = None

    ds = ogr.Open(tmp_path / "test.gdb")
    lyr = ds.GetLayerByName("test")
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_not_nullable"))
        .IsNullable()
        == 0
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("field_nullable"))
        .IsNullable()
        == 1
    )
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0

    ds = None


###############################################################################
# Read curves


@pytest.mark.require_driver("CSV")
def test_ogr_fgdb_22():

    ds = ogr.Open("data/filegdb/curves.gdb")
    lyr = ds.GetLayerByName("line")
    ds_ref = ogr.Open("data/filegdb/curves_line.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())

    lyr = ds.GetLayerByName("polygon")
    ds_ref = ogr.Open("data/filegdb/curves_polygon.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())

    ds = ogr.Open("data/filegdb/curve_circle_by_center.gdb")
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open("data/filegdb/curve_circle_by_center.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())


###############################################################################
# Test opening '.'


def test_ogr_fgdb_23():

    os.chdir("data/filegdb/curves.gdb")
    ds = ogr.Open(".")
    os.chdir("../../..")
    assert ds is not None


###############################################################################
# Read polygons with M component where the M of the closing point is not the
# one of the starting point (#7017)


@pytest.mark.require_driver("CSV")
def test_ogr_fgdb_24():

    ds = ogr.Open("data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb")
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open(
        "data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb.csv"
    )
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())

    ds = ogr.Open("data/filegdb/filegdb_polygonzm_nan_m_with_curves.gdb")
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open("data/filegdb/filegdb_polygonzm_nan_m_with_curves.gdb.csv")
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef())


###############################################################################
# Test selecting FID column with OGRSQL


def test_ogr_fgdb_25():

    ds = ogr.Open("data/filegdb/curves.gdb")
    sql_lyr = ds.ExecuteSQL("SELECT OBJECTID FROM polygon WHERE OBJECTID = 2")
    assert sql_lyr is not None
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 2:
        f.DumpReadable()
        pytest.fail()
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayerByName("polygon")
    lyr.SetAttributeFilter("OBJECTID = 2")
    f = lyr.GetNextFeature()
    if f.GetFID() != 2:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1369
# where a polygon with inner rings has its exterior ring with wrong orientation


@pytest.mark.require_geos
def test_ogr_fgdb_weird_winding_order(tmp_path):

    gdaltest.unzip(tmp_path, "data/filegdb/weird_winding_order_fgdb.zip")

    ds = ogr.Open(tmp_path / "roads_clip Drawing.gdb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryCount() == 1
    assert g.GetGeometryRef(0).GetGeometryCount() == 17


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1369
# where a polygon with inner rings has its exterior ring with wrong orientation


def test_ogr_fgdb_utc_datetime():

    ds = ogr.Open("data/filegdb/testdatetimeutc.gdb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    # Check that the timezone +00 is present
    assert f.GetFieldAsString("EditDate") == "2020/06/22 07:49:36+00"


###############################################################################
# Test reading field domains


def _check_domains(ds):

    assert set(ds.GetFieldDomainNames()) == {
        "MedianType",
        "RoadSurfaceType",
        "SpeedLimit",
    }

    with gdal.quiet_errors():
        assert ds.GetFieldDomain("i_dont_exist") is None
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("MaxSpeed"))
    assert fld_defn.GetDomainName() == "SpeedLimit"

    domain = ds.GetFieldDomain("SpeedLimit")
    assert domain is not None
    assert domain.GetName() == "SpeedLimit"
    assert domain.GetDescription() == "The maximun speed of the road"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetMinAsDouble() == 40.0
    assert domain.GetMaxAsDouble() == 100.0

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("MedianType"))
    assert fld_defn.GetDomainName() == "MedianType"

    domain = ds.GetFieldDomain("MedianType")
    assert domain is not None
    assert domain.GetName() == "MedianType"
    assert domain.GetDescription() == "Road median types."
    assert domain.GetDomainType() == ogr.OFDT_CODED
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetEnumeration() == {"0": "None", "1": "Cement"}


def test_ogr_fgdb_read_domains():

    ds = gdal.OpenEx("data/filegdb/Domains.gdb", gdal.OF_VECTOR)
    _check_domains(ds)


###############################################################################
# Test reading layer hierarchy


@gdaltest.disable_exceptions()
def test_ogr_fgdb_read_layer_hierarchy():

    if False:
        # Test dataset produced with:
        from osgeo import ogr, osr

        srs = osr.SpatialReference()
        srs.SetFromUserInput("WGS84")
        ds = ogr.GetDriverByName("FileGDB").CreateDataSource("featuredataset.gdb")
        ds.CreateLayer(
            "fd1_lyr1", srs=srs, geom_type=ogr.wkbPoint, options=["FEATURE_DATASET=fd1"]
        )
        ds.CreateLayer(
            "fd1_lyr2", srs=srs, geom_type=ogr.wkbPoint, options=["FEATURE_DATASET=fd1"]
        )
        srs2 = osr.SpatialReference()
        srs2.ImportFromEPSG(32631)
        ds.CreateLayer("standalone", srs=srs2, geom_type=ogr.wkbPoint)
        srs3 = osr.SpatialReference()
        srs3.ImportFromEPSG(32632)
        ds.CreateLayer(
            "fd2_lyr", srs=srs3, geom_type=ogr.wkbPoint, options=["FEATURE_DATASET=fd2"]
        )

    ds = gdal.OpenEx("data/filegdb/featuredataset.gdb")
    rg = ds.GetRootGroup()

    assert rg.GetGroupNames() == ["fd1", "fd2"]
    assert rg.OpenGroup("not_existing") is None

    fd1 = rg.OpenGroup("fd1")
    assert fd1 is not None
    assert fd1.GetVectorLayerNames() == ["fd1_lyr1", "fd1_lyr2"]
    assert fd1.OpenVectorLayer("not_existing") is None
    assert len(fd1.GetGroupNames()) == 0

    fd1_lyr1 = fd1.OpenVectorLayer("fd1_lyr1")
    assert fd1_lyr1 is not None
    assert fd1_lyr1.GetName() == "fd1_lyr1"

    fd1_lyr2 = fd1.OpenVectorLayer("fd1_lyr2")
    assert fd1_lyr2 is not None
    assert fd1_lyr2.GetName() == "fd1_lyr2"

    fd2 = rg.OpenGroup("fd2")
    assert fd2 is not None
    assert fd2.GetVectorLayerNames() == ["fd2_lyr"]
    fd2_lyr = fd2.OpenVectorLayer("fd2_lyr")
    assert fd2_lyr is not None

    assert rg.GetVectorLayerNames() == ["standalone"]
    standalone = rg.OpenVectorLayer("standalone")
    assert standalone is not None


###############################################################################
# Test that non-spatial tables which are not present in GDB_Items are listed
# see https://github.com/OSGeo/gdal/issues/4463


@pytest.mark.require_driver("OpenFileGDB")
def test_ogr_filegdb_non_spatial_table_outside_gdb_items(openfilegdb_drv, fgdb_drv):
    openfilegdb_drv.Deregister()
    fgdb_drv.Deregister()

    # Force FileGDB first
    fgdb_drv.Register()
    openfilegdb_drv.Register()

    ds = ogr.Open("data/filegdb/table_outside_gdbitems.gdb")
    assert ds is not None
    assert ds.GetDriver().GetName() == "FileGDB"

    assert ds.GetLayerCount() == 3, "did not get expected layer count"
    layer_names = set(ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount()))
    assert layer_names == {"aquaduct", "flat_table1", "flat_table2"}


###############################################################################
# Test reading .gdb where the CRS in the XML definition of the feature
# table is not consistent with the one of the feature dataset


def test_ogr_filegdb_inconsistent_crs_feature_dataset_and_feature_table():
    ds = ogr.Open("data/filegdb/inconsistent_crs_feature_dataset_and_feature_table.gdb")
    assert ds is not None
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"


###############################################################################
# Test reading .gdb with LengthFieldName / AreaFieldName


def test_ogr_filegdb_shape_length_shape_area_as_default_in_field_defn(fgdb_drv):
    ds = ogr.Open("data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb")
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Area")).GetDefault()
        == "FILEGEODATABASE_SHAPE_AREA"
    )
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Length")).GetDefault()
        == "FILEGEODATABASE_SHAPE_LENGTH"
    )


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit(fgdb_drv, tmp_path):

    dirname = (
        tmp_path / "test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit.gdb"
    )
    ds = fgdb_drv.CreateDataSource(dirname)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer(
        "line",
        srs=srs,
        geom_type=ogr.wkbLineString,
        options=["CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(0 0,2 0)"))
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer(
        "area",
        srs=srs,
        geom_type=ogr.wkbPolygon,
        options=["CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES"],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "POLYGON((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2))"
        )
    )
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open(dirname)

    lyr = ds.GetLayerByName("line")
    f = lyr.GetNextFeature()
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldIndex("Shape_Length") >= 0
    assert lyr_defn.GetFieldIndex("Shape_Area") < 0
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Length")).GetDefault()
        == "FILEGEODATABASE_SHAPE_LENGTH"
    )
    assert f["Shape_Length"] == 2

    lyr = ds.GetLayerByName("area")
    f = lyr.GetNextFeature()
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldIndex("Shape_Length") >= 0
    assert lyr_defn.GetFieldIndex("Shape_Area") >= 0
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Area")).GetDefault()
        == "FILEGEODATABASE_SHAPE_AREA"
    )
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Length")).GetDefault()
        == "FILEGEODATABASE_SHAPE_LENGTH"
    )
    assert f["Shape_Length"] == pytest.approx(6.4)
    assert f["Shape_Area"] == pytest.approx(0.64)

    ds = None


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit(fgdb_drv, tmp_path):

    dirname = (
        tmp_path / "test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit.gdb"
    )
    gdal.VectorTranslate(
        dirname,
        "data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb",
        options="-f FileGDB -unsetfid -fid 1",
    )

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Area")).GetDefault()
        == "FILEGEODATABASE_SHAPE_AREA"
    )
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("Shape_Length")).GetDefault()
        == "FILEGEODATABASE_SHAPE_LENGTH"
    )

    ds = None


@pytest.mark.require_driver("OpenFileGDB")
def test_ogr_filegdb_read_relationships(openfilegdb_drv, fgdb_drv):
    openfilegdb_drv.Deregister()
    fgdb_drv.Deregister()

    # Force FileGDB first
    fgdb_drv.Register()
    openfilegdb_drv.Register()

    # no relationships
    ds = gdal.OpenEx("data/filegdb/Domains.gdb", gdal.OF_VECTOR)
    assert ds.GetRelationshipNames() is None

    # has relationships
    ds = gdal.OpenEx("data/filegdb/relationships.gdb", gdal.OF_VECTOR)
    assert ds.GetDriver().GetDescription() == "FileGDB"
    assert set(ds.GetRelationshipNames()) == {
        "composite_many_to_many",
        "composite_one_to_many",
        "composite_one_to_one",
        "simple_attributed",
        "simple_backward_message_direction",
        "simple_both_message_direction",
        "simple_forward_message_direction",
        "simple_many_to_many",
        "simple_one_to_many",
        "simple_relationship_one_to_one",
        "points__ATTACHREL",
    }

    assert ds.GetRelationship("xxxx") is None

    rel = ds.GetRelationship("simple_relationship_one_to_one")
    assert rel is not None
    assert rel.GetName() == "simple_relationship_one_to_one"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table2"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_ONE
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetLeftMappingTableFields() is None
    assert rel.GetRightMappingTableFields() is None
    assert rel.GetForwardPathLabel() == "my forward path label"
    assert rel.GetBackwardPathLabel() == "my backward path label"
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("simple_one_to_many")
    assert rel is not None
    assert rel.GetName() == "simple_one_to_many"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table2"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("simple_many_to_many")
    assert rel is not None
    assert rel.GetName() == "simple_many_to_many"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table2"
    assert rel.GetMappingTableName() == "simple_many_to_many"
    assert rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert rel.GetType() == gdal.GRT_ASSOCIATION
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetLeftMappingTableFields() == ["origin_foreign_key"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRightMappingTableFields() == ["destination_foreign_key"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("composite_one_to_one")
    assert rel is not None
    assert rel.GetName() == "composite_one_to_one"
    assert rel.GetLeftTableName() == "table1"
    assert rel.GetRightTableName() == "table3"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_ONE
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("composite_one_to_many")
    assert rel is not None
    assert rel.GetName() == "composite_one_to_many"
    assert rel.GetLeftTableName() == "table5"
    assert rel.GetRightTableName() == "table4"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("composite_many_to_many")
    assert rel is not None
    assert rel.GetName() == "composite_many_to_many"
    assert rel.GetLeftTableName() == "table6"
    assert rel.GetRightTableName() == "table7"
    assert rel.GetMappingTableName() == "composite_many_to_many"
    assert rel.GetCardinality() == gdal.GRC_MANY_TO_MANY
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["pk"]
    assert rel.GetLeftMappingTableFields() == ["origin_foreign_key"]
    assert rel.GetRightTableFields() == ["parent_pk"]
    assert rel.GetRightMappingTableFields() == ["dest_foreign_key"]
    assert rel.GetRelatedTableType() == "features"

    rel = ds.GetRelationship("points__ATTACHREL")
    assert rel is not None
    assert rel.GetName() == "points__ATTACHREL"
    assert rel.GetLeftTableName() == "points"
    assert rel.GetRightTableName() == "points__ATTACH"
    assert rel.GetMappingTableName() == ""
    assert rel.GetCardinality() == gdal.GRC_ONE_TO_MANY
    assert rel.GetType() == gdal.GRT_COMPOSITE
    assert rel.GetLeftTableFields() == ["OBJECTID"]
    assert rel.GetRightTableFields() == ["REL_OBJECTID"]
    assert rel.GetForwardPathLabel() == "attachment"
    assert rel.GetBackwardPathLabel() == "object"
    assert rel.GetRelatedTableType() == "media"


###############################################################################
# Test reading an empty polygon


def test_ogr_filegdb_read_empty_polygon():

    # Dataset generated by OpenFileGDB driver
    ds = ogr.Open("data/filegdb/empty_polygon.gdb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().IsEmpty()


###############################################################################
# Test reading a database with a compressed layer (.cdf)


@pytest.mark.require_driver("OpenFileGDB")
def test_ogr_filegdb_read_cdf_if_openfilegdb(openfilegdb_drv, fgdb_drv):
    openfilegdb_drv.Deregister()
    fgdb_drv.Deregister()

    # Force OpenFileGDB first
    openfilegdb_drv.Register()
    fgdb_drv.Register()

    ds = ogr.Open("data/filegdb/with_cdf.gdb")
    assert ds.GetDriver().GetDescription() == "FileGDB"
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 3


###############################################################################
# Test reading a database with a compressed layer (.cdf)


def test_ogr_filegdb_read_cdf():

    ds = ogr.Open("data/filegdb/with_cdf.gdb")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 3

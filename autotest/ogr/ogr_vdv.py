#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR VDV driver.
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import sys

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("VDV")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Basic test of .idf file


def test_ogr_idf_1():

    ds = ogr.Open("data/vdv/test.idf")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if (
        f["NODE_ID"] != 1
        or f["foo"] != "U"
        or f.GetGeometryRef().ExportToWkt() != "POINT (2 49)"
    ):
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != "LINESTRING (2 49,2.5 49.5,2.7 49.7,3 50)":
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(2)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != "POINT (2.5 49.5)":
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(3)
    f = lyr.GetNextFeature()
    if f["FOO"] != 1:
        f.DumpReadable()
        pytest.fail()


###############################################################################
#


@pytest.mark.require_driver("SQLite")
def test_ogr_idf_1_with_temp_sqlite_db():
    options = {"OGR_IDF_TEMP_DB_THRESHOLD": "0"}
    with gdaltest.config_options(options):
        return test_ogr_idf_1()


###############################################################################
# Basic test of .idf file


def test_ogr_idf_3d():

    ds = ogr.Open("data/vdv/test_3d.idf")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if (
        f["NODE_ID"] != 1
        or f["foo"] != "U"
        or f.GetGeometryRef().ExportToWkt() != "POINT (2 49 10)"
    ):
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if (
        f.GetGeometryRef().ExportToWkt()
        != "LINESTRING (2 49 10,2.5 49.5 10,2.7 49.7 20,3 50 20)"
    ):
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(2)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != "POINT (2.5 49.5 10)":
        f.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayer(3)
    f = lyr.GetNextFeature()
    if f["FOO"] != 1:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Run test_ogrsf on .idf


def test_ogr_idf_2():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/vdv/test.idf"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Create a VDV file


def ogr_create_vdv(filename, dsco=None, lco=None):

    dsco = [] if dsco is None else dsco
    lco = [] if lco is None else lco

    ds = ogr.GetDriverByName("VDV").CreateDataSource(filename, options=dsco)
    ds.CreateLayer("empty", options=lco)
    lyr = ds.CreateLayer("lyr_1", options=lco)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64_field", ogr.OFTInteger64))

    bool_field = ogr.FieldDefn("bool_field", ogr.OFTInteger)
    bool_field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(bool_field)

    fld = ogr.FieldDefn("str2_field", ogr.OFTString)
    fld.SetWidth(2)
    lyr.CreateField(fld)

    fld = ogr.FieldDefn("int2_field", ogr.OFTInteger)
    fld.SetWidth(2)
    lyr.CreateField(fld)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("str_field", 'a"b')
    f.SetField("int_field", 12)
    f.SetField("bool_field", 1)
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer("another_layer", options=lco)
    lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("str_field", i)
        lyr.CreateFeature(f)

    ds = None

    # Do nothing
    ds = ogr.Open(filename, update=1)
    ds = None

    ds = ogr.Open(filename, update=1)
    ds.CreateLayer("empty2", options=lco)
    ds = None


@pytest.fixture()
def test_vdv(request, tmp_path):

    fmt = request.param

    if fmt == "single_file":
        src_filename = tmp_path / "test.x10"
        dsco = None
        lco = None
    elif fmt == "directory":
        src_filename = tmp_path / "test_x10"
        dsco = ["SINGLE_FILE=NO"]
        lco = ["EXTENSION=txt"]

    ogr_create_vdv(src_filename, dsco, lco)

    return src_filename


###############################################################################
# Read it


@pytest.mark.parametrize("test_vdv", ("single_file", "directory"), indirect=True)
def test_ogr_vdv_2(tmp_vsimem, test_vdv):

    out_filename = tmp_vsimem / "ogr_vdv_2.x10"

    src_ds = ogr.Open(test_vdv)
    out_ds = ogr.GetDriverByName("VDV").CreateDataSource(out_filename)
    layer_names = [
        src_ds.GetLayer(idx).GetName() for idx in range(src_ds.GetLayerCount())
    ]
    layer_names.sort()
    for layer_name in layer_names:
        src_lyr = src_ds.GetLayer(layer_name)
        options = [
            "HEADER_SRC_DATE=01.01.1970",
            "HEADER_SRC_TIME=00.00.00",
            "HEADER_foo=bar",
        ]
        dst_lyr = out_ds.CreateLayer(src_lyr.GetName(), options=options)
        for field_idx in range(src_lyr.GetLayerDefn().GetFieldCount()):
            dst_lyr.CreateField(src_lyr.GetLayerDefn().GetFieldDefn(field_idx))
        for src_f in src_lyr:
            dst_f = ogr.Feature(dst_lyr.GetLayerDefn())
            dst_f.SetFrom(src_f)
            dst_lyr.CreateFeature(dst_f)
    out_ds = None

    expected = """mod; DD.MM.YYYY; HH:MM:SS; free
src; "UNKNOWN"; "01.01.1970"; "00.00.00"
chs; "ISO8859-1"
ver; "1.4"
ifv; "1.4"
dve; "1.4"
fft; ""
foo; "bar"
tbl; another_layer
atr; str_field
frm; char[80]
rec; "0"
rec; "1"
rec; "2"
rec; "3"
rec; "4"
end; 5
tbl; lyr_1
atr; str_field; int_field; int64_field; bool_field; str2_field; int2_field
frm; char[80]; num[10.0]; num[19.0]; boolean; char[2]; num[1.0]
rec; "a""b"; 12; NULL; 1; NULL; NULL
rec; NULL; NULL; NULL; NULL; NULL; NULL
end; 2
tbl; empty
atr;
frm;
end; 0
tbl; empty2
atr;
frm;
end; 0
eof; 4
"""

    f = gdal.VSIFOpenL(out_filename, "rb")
    got = gdal.VSIFReadL(1, 10000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert got == expected


###############################################################################
# Run test_ogrsf on it


@pytest.mark.parametrize("test_vdv", ("single_file", "directory"), indirect=True)
def test_ogr_vdv_3(tmp_path, test_vdv):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        f"{test_cli_utilities.get_test_ogrsf_path()} -ro {test_vdv}"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Run VDV452


@pytest.mark.parametrize(
    "profile,lyrname,longname,latname",
    [
        ("VDV-452", "STOP", "POINT_LONGITUDE", "POINT_LATITUDE"),
        ("VDV-452-ENGLISH", "STOP", "POINT_LONGITUDE", "POINT_LATITUDE"),
        ("VDV-452", "REC_ORT", "ORT_POS_LAENGE", "ORT_POS_BREITE"),
        ("VDV-452-GERMAN", "REC_ORT", "ORT_POS_LAENGE", "ORT_POS_BREITE"),
    ],
)
def test_ogr_vdv_7(tmp_vsimem, profile, lyrname, longname, latname):

    out_filename = tmp_vsimem / "ogr_vdv_7.x10"

    ds = ogr.GetDriverByName("VDV").CreateDataSource(out_filename)
    lyr = ds.CreateLayer(
        lyrname, geom_type=ogr.wkbPoint, options=["PROFILE=" + profile]
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    lng = -(123 + 45.0 / 60 + 56.789 / 3600)
    lat = -(23 + 45.0 / 60 + 56.789 / 3600)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(%.10f %.10f)" % (lng, lat)))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    f = lyr.GetNextFeature()
    assert f[longname] == -1234556789
    assert f[latname] == -234556789
    ogrtest.check_feature_geometry(f, "POINT (-123.765774722222 -23.7657747222222)")
    ds = None


@pytest.mark.parametrize(
    "profile,strict",
    [("VDV-452", True), ("VDV-452-ENGLISH", False), ("VDV-452-GERMAN", False)],
)
def test_ogr_vdv_7bis(tmp_vsimem, profile, strict):

    out_filename = tmp_vsimem / "ogr_vdv_7.x10"

    ds = ogr.GetDriverByName("VDV").CreateDataSource(out_filename)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        lyr = ds.CreateLayer(
            "UNKNOWN",
            options=["PROFILE=" + profile, "PROFILE_STRICT=" + str(strict)],
        )
    assert gdal.GetLastErrorMsg() != ""
    if strict and lyr is not None:
        pytest.fail()
    elif not strict and lyr is None:
        pytest.fail()

    if profile == "VDV-452-GERMAN":
        lyr_name = "REC_ORT"
    else:
        lyr_name = "STOP"
    lyr = ds.CreateLayer(
        lyr_name, options=["PROFILE=" + profile, "PROFILE_STRICT=" + str(strict)]
    )
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ret = lyr.CreateField(ogr.FieldDefn("UNKNOWN"))
    assert gdal.GetLastErrorMsg() != ""
    if strict and ret == 0:
        pytest.fail()
    elif not strict and ret != 0:
        pytest.fail()

    ds = None


###############################################################################
# Test a few error cases


def test_ogr_vdv_8a():

    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("VDV").CreateDataSource("/does/not_exist")
    assert ds is None


def test_ogr_vdv_8b():
    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("VDV").CreateDataSource(
            "/does/not_exist", options=["SINGLE_FILE=FALSE"]
        )
    assert ds is None


def test_ogr_vdv_8c(tmp_path):
    if not sys.platform.startswith("linux"):
        pytest.skip("Test requires Linux")

    # Add layer in non writable directory
    os.mkdir(tmp_path / "ogr_vdv_8")
    open(tmp_path / "ogr_vdv_8" / "empty.x10", "wb").write(
        "tbl; foo\natr;\nfrm;\n".encode("latin1")
    )
    os.chmod(tmp_path / "ogr_vdv_8", 0o555)
    try:
        open(tmp_path / "ogr_vdv_8" / "another_file", "wb").close()
        shutil.rmtree(tmp_path / "ogr_vdv_8")
        do_test = False
    except Exception:
        do_test = True
    if do_test:
        ds = ogr.Open(tmp_path / "ogr_vdv_8", update=1)
        with gdal.quiet_errors():
            lyr = ds.CreateLayer("another_layer")
        os.chmod(tmp_path / "ogr_vdv_8", 0o755)
        ds = None
        shutil.rmtree(tmp_path / "ogr_vdv_8")
        assert lyr is None


def test_ogr_vdv_8d(tmp_vsimem):
    out_filename = tmp_vsimem / "ogr_vdv_8.x10"
    ds = ogr.GetDriverByName("VDV").CreateDataSource(out_filename)

    # File already exists
    with gdal.quiet_errors():
        ds2 = ogr.GetDriverByName("VDV").CreateDataSource(out_filename)
    assert ds2 is None

    assert ds.TestCapability(ogr.ODsCCreateLayer) == 1

    lyr1 = ds.CreateLayer("lyr1")
    assert lyr1.TestCapability(ogr.OLCSequentialWrite) == 1
    assert lyr1.TestCapability(ogr.OLCCreateField) == 1

    lyr1.ResetReading()

    with gdal.quiet_errors():
        lyr1.GetNextFeature()

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    # Layer structure is now frozen
    assert lyr1.TestCapability(ogr.OLCCreateField) == 0

    with gdal.quiet_errors():
        ret = lyr1.CreateField(ogr.FieldDefn("not_allowed"))
    assert ret != 0

    lyr2 = ds.CreateLayer("lyr2")
    lyr2.CreateFeature(ogr.Feature(lyr2.GetLayerDefn()))

    # Test interleaved writing

    assert lyr1.TestCapability(ogr.OLCSequentialWrite) == 0

    with gdal.quiet_errors():
        ret = lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    assert ret != 0

    assert lyr1.GetFeatureCount() == 1

    ds = None


def test_ogr_vdv_8e(tmp_vsimem):
    out_filename = tmp_vsimem / "ogr_vdv_8.x10"

    # Test appending new layer to file without eof
    gdal.FileFromMemBuffer(
        out_filename, 'tbl; foo\natr; atr\nfrm; char[40]\nrec; "foo"\n'
    )
    ds = ogr.Open(out_filename, update=1)
    lyr = ds.CreateLayer("new_layer")
    lyr.CreateField(ogr.FieldDefn("atr"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["atr"] = "bar"
    lyr.CreateFeature(f)
    f = None
    ds = None

    expected = """tbl; foo
atr; atr
frm; char[40]
rec; "foo"
tbl; new_layer
atr; atr
frm; char[80]
rec; "bar"
end; 1
eof; 2
"""

    f = gdal.VSIFOpenL(out_filename, "rb")
    got = gdal.VSIFReadL(1, 10000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert got == expected

    # Test we are robust against missing end;
    ds = ogr.Open(out_filename)
    for i in range(2):
        lyr = ds.GetLayer(i)
        assert lyr.GetFeatureCount() == 1
        lyr.ResetReading()
        fc = 0
        for f in lyr:
            fc += 1
        assert fc == 1
        lyr = None
    ds = None

    # Test appending new layer to file without terminating \n
    gdal.FileFromMemBuffer(
        out_filename, 'tbl; foo\natr; atr\nfrm; char[40]\nrec; "foo"\neof; 1'
    )
    ds = ogr.Open(out_filename, update=1)
    lyr = ds.CreateLayer("new_layer")
    lyr.CreateField(ogr.FieldDefn("atr"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["atr"] = "bar"
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL(out_filename, "rb")
    got = gdal.VSIFReadL(1, 10000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert got == expected

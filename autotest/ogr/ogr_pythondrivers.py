#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Python drivers
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal, ogr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture(autouse=True, scope="module")
def setup_and_cleanup():
    with gdaltest.config_option("GDAL_PYTHON_DRIVER_PATH", "data/pydrivers"):
        gdal.AllRegister()
    assert ogr.GetDriverByName("DUMMY")

    yield

    with gdaltest.config_option("GDAL_SKIP", "DUMMY"):
        gdal.AllRegister()
    assert not ogr.GetDriverByName("DUMMY")


@pytest.mark.parametrize("geomformat", ["WKT", "WKB", "WKB/bytearray"])
def test_pythondrivers_test_dummy(geomformat):
    assert not ogr.Open("UNRELATED:")

    ds = gdal.OpenEx("DUMMY:", open_options=["GEOMFORMAT=" + geomformat])
    assert ds
    assert ds.GetLayerCount() == 1
    assert not ds.GetLayer(-1)
    assert not ds.GetLayer(1)
    lyr = ds.GetLayer(0)
    assert lyr
    assert lyr.GetName() == "my_layer"
    assert lyr.GetFIDColumn() == "my_fid"
    assert lyr.GetLayerDefn()
    assert lyr.GetLayerDefn().GetFieldCount() == 13
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 1
    assert lyr.GetFeatureCount() == 5
    count = 0
    for f in lyr:
        assert f.GetFID() == count + 1
        assert f["boolField"]
        assert f["int16Field"] == 32767
        assert f["int32Field"] == count + 2
        assert f["int64Field"] == 1234567890123
        assert f["realField"] == 1.23
        assert f["floatField"] == 1.2
        assert f["strField"] == "foo"
        assert f["strNullField"] is None
        assert f.GetFieldAsBinary("binaryField") == b"\x01\x00\x02"
        assert f["timeField"] == "12:34:56.789"
        assert f["dateField"] == "2017/04/26"
        assert f["datetimeField"] == "2017/04/26 12:34:56.789+00"
        g = f.GetGeometryRef()
        assert g is not None
        assert g.GetPoint() == (2.0, 49.0, 0.0)
        count += 1
    assert count == 5
    assert lyr.TestCapability(ogr.OLCFastFeatureCount)
    assert lyr.GetFeature(1).GetFID() == 1

    lyr.SetAttributeFilter("1 = 0")
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
    assert lyr.GetFeatureCount() == 0
    count = 0
    for f in lyr:
        count += 1
    assert count == 0
    lyr.SetAttributeFilter(None)

    lyr.SetSpatialFilter(ogr.CreateGeometryFromWkt("POINT (-100 -100)"))
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
    assert lyr.GetFeatureCount() == 0
    count = 0
    for f in lyr:
        count += 1
    assert count == 0
    lyr.SetSpatialFilter(None)


def test_pythondrivers_missing_metadata():
    count_before = gdal.GetDriverCount()
    with gdaltest.config_option(
        "GDAL_PYTHON_DRIVER_PATH", "data/pydrivers/missingmetadata"
    ):
        with gdal.quiet_errors():
            gdal.AllRegister()
    assert gdal.GetLastErrorMsg() != ""
    assert gdal.GetDriverCount() == count_before


def test_pythondrivers_unsupported_api_version():
    count_before = gdal.GetDriverCount()
    with gdaltest.config_option(
        "GDAL_PYTHON_DRIVER_PATH", "data/pydrivers/unsupportedapiversion"
    ):
        gdal.AllRegister()
    assert gdal.GetDriverCount() == count_before


def test_pythondrivers_no_driver_class():
    with gdaltest.config_option(
        "GDAL_PYTHON_DRIVER_PATH", "data/pydrivers/nodriverclass"
    ):
        gdal.AllRegister()
    drv = ogr.GetDriverByName("NO_DRIVER_CLASS")
    assert drv
    with gdal.quiet_errors():
        ogr.Open("FOO:")
    assert gdal.GetLastErrorMsg() != ""

    with gdaltest.config_option("GDAL_SKIP", "NO_DRIVER_CLASS"):
        gdal.AllRegister()


def test_pythondrivers_missing_identify():
    with gdaltest.config_option(
        "GDAL_PYTHON_DRIVER_PATH", "data/pydrivers/missingidentify"
    ):
        gdal.AllRegister()
    drv = ogr.GetDriverByName("MISSING_IDENTIFY")
    assert drv
    with gdal.quiet_errors():
        ogr.Open("FOO:")
    assert gdal.GetLastErrorMsg() != ""

    with gdaltest.config_option("GDAL_SKIP", "MISSING_IDENTIFY"):
        gdal.AllRegister()

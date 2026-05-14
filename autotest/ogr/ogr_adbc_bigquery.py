#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for BigQuery support through OGR ADBC driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr


def _has_adbc_driver_manager():
    drv = gdal.GetDriverByName("ADBC")
    return drv and drv.GetMetadataItem("HAS_ADBC_DRIVER_MANAGER")


def _has_bigquery_driver():
    import ctypes

    try:
        return ctypes.cdll.LoadLibrary("libadbc_driver_bigquery.so") is not None
    except Exception:
        return False


pytestmark = [
    pytest.mark.require_driver("ADBC"),
    pytest.mark.skipif(
        not _has_adbc_driver_manager(),
        reason="ADBC driver built without adbc_driver_manager",
    ),
    pytest.mark.skipif(
        not _has_bigquery_driver(), reason="libadbc_driver_bigquery.so not available"
    ),
    pytest.mark.skipif(
        gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_PROJECT_ID") is None,
        reason="GDAL_AUTOTEST_BIGQUERY_PROJECT_ID config option not set",
    ),
    pytest.mark.skipif(
        gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_JSON_CREDENTIAL_FILE") is None,
        reason="GDAL_AUTOTEST_BIGQUERY_JSON_CREDENTIAL_FILE config option not set",
    ),
    pytest.mark.skipif(
        gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_DATASET_ID") is None,
        reason="GDAL_AUTOTEST_BIGQUERY_DATASET_ID config option not set",
    ),
    pytest.mark.skipif(
        gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_EXISTING_TABLE") is None,
        reason="GDAL_AUTOTEST_BIGQUERY_EXISTING_TABLE config option not set",
    ),
]


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    gdal.SetConfigOption(
        "BIGQUERY_PROJECT_ID", gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_PROJECT_ID")
    )
    gdal.SetConfigOption(
        "BIGQUERY_JSON_CREDENTIAL_FILE",
        gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_JSON_CREDENTIAL_FILE"),
    )
    gdal.SetConfigOption(
        "BIGQUERY_DATASET_ID", gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_DATASET_ID")
    )
    yield


###############################################################################


def test_ogr_adbc_bigquery_read_only_basic():
    ds = gdal.OpenEx("ADBC:", open_options=["ADBC_DRIVER=adbc_driver_bigquery"])
    assert ds.GetLayerCount() > 0
    assert (
        ds.GetLayer(gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_EXISTING_TABLE"))
        is not None
    )


###############################################################################


def test_ogr_adbc_bigquery_read_only_test_ogrsf():
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    cmdline = (
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro ADBC: "
        + gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_EXISTING_TABLE")
    )
    cmdline += " -oo ADBC_DRIVER=adbc_driver_bigquery"
    cmdline += " -oo BIGQUERY_PROJECT_ID=" + gdal.GetConfigOption(
        "GDAL_AUTOTEST_BIGQUERY_PROJECT_ID"
    )
    cmdline += " -oo BIGQUERY_JSON_CREDENTIAL_FILE=" + gdal.GetConfigOption(
        "GDAL_AUTOTEST_BIGQUERY_JSON_CREDENTIAL_FILE"
    )
    cmdline += " -oo BIGQUERY_DATASET_ID=" + gdal.GetConfigOption(
        "GDAL_AUTOTEST_BIGQUERY_DATASET_ID"
    )
    ret = gdaltest.runexternal(cmdline)

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################


@pytest.mark.skipif(
    gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_NEW_TABLE") is None,
    reason="GDAL_AUTOTEST_BIGQUERY_NEW_TABLE config option not set",
)
def test_ogr_adbc_bigquery_write():

    lyr_name = gdal.GetConfigOption("GDAL_AUTOTEST_BIGQUERY_NEW_TABLE")
    ds = gdal.OpenEx(
        "ADBC:",
        gdal.OF_VECTOR | gdal.OF_UPDATE,
        open_options=["ADBC_DRIVER=adbc_driver_bigquery"],
    )
    lyr_count_before = ds.GetLayerCount()
    lyr = ds.CreateLayer(lyr_name)
    assert lyr.GetFIDColumn() == "ogc_fid"
    assert lyr.GetGeometryColumn() == "geog"
    assert lyr.TestCapability(ogr.OLCCreateField)
    assert lyr.TestCapability(ogr.OLCSequentialWrite)

    lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("nullfield", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("double", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("time", ogr.OFTTime))
    lyr.CreateField(ogr.FieldDefn("date", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("datetime", ogr.OFTDateTime))
    fld_defn = ogr.FieldDefn("json", ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("bool", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("binary", ogr.OFTBinary)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("strlist", ogr.OFTStringList)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("intlist", ogr.OFTIntegerList)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("int64list", ogr.OFTInteger64List)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("doublelist", ogr.OFTRealList)
    lyr.CreateField(fld_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str"] = "foo"
    f["int"] = 12345
    f["int64"] = 123467890123456
    f["double"] = 1.25
    f["time"] = "12:34:56.789"
    f["date"] = "2025/08/12"
    f["datetime"] = "2025/08/12 12:34:56.789+00"
    f["json"] = '{"foo":"bar"}'
    f["bool"] = True
    f.SetFieldBinaryFromHexString("binary", "DEADBEEF")
    f["strlist"] = ["bar", "baz"]
    f["intlist"] = [1, 2]
    f["int64list"] = [123467890123456, 6543210987654321]
    f["doublelist"] = [1.25, 2.5]
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 49)"))
    lyr.CreateFeature(f)
    assert f.GetFID() == 1

    assert lyr.GetFeatureCount() == 1
    ds.Close()

    ds = gdal.OpenEx(
        "ADBC:",
        gdal.OF_VECTOR | gdal.OF_UPDATE,
        open_options=["ADBC_DRIVER=adbc_driver_bigquery"],
    )
    assert ds.GetLayerCount() > lyr_count_before
    lyr = ds.GetLayerByName(lyr_name)
    assert lyr.GetFIDColumn() == "ogc_fid"
    assert lyr.GetGeometryColumn() == "geog"

    f = lyr.GetNextFeature()
    assert f["str"] == "foo"
    assert f.IsFieldNull("nullfield")
    assert f["int"] == 12345
    assert f["int64"] == 123467890123456
    assert f["double"] == 1.25
    assert f["time"] == "12:34:56.789"
    assert f["date"] == "2025/08/12"
    assert f["datetime"] == "2025/08/12 12:34:56.789+00"
    assert f["json"] == '{"foo":"bar"}'
    assert f["bool"] == True
    assert f["binary"] == "DEADBEEF"
    assert f["strlist"] == ["bar", "baz"]
    assert f["intlist"] == [1, 2]
    assert f["int64list"] == [123467890123456, 6543210987654321]
    assert f["doublelist"] == [1.25, 2.5]
    assert f.GetGeometryRef().ExportToWkt() == "POINT (2.0 49.0)"
    assert f.GetFID() == 1

    f["str"] = "bar"
    lyr.SetFeature(f)

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["str"] == "bar"
    assert f.IsFieldNull("nullfield")
    assert f["int"] == 12345
    assert f["int64"] == 123467890123456
    assert f["double"] == 1.25
    assert f["time"] == "12:34:56.789"
    assert f["date"] == "2025/08/12"
    assert f["datetime"] == "2025/08/12 12:34:56.789+00"
    assert f["json"] == '{"foo":"bar"}'
    assert f["bool"] == True
    assert f["binary"] == "DEADBEEF"
    assert f["strlist"] == ["bar", "baz"]
    assert f["intlist"] == [1, 2]
    assert f["int64list"] == [123467890123456, 6543210987654321]
    assert f["doublelist"] == [1.25, 2.5]
    assert f.GetGeometryRef().ExportToWkt() == "POINT (2.0 49.0)"
    assert f.GetFID() == 1

    lyr.DeleteFeature(f.GetFID())

    assert lyr.GetFeatureCount() == 0

    i = 0
    while i < ds.GetLayerCount():
        if ds.GetLayer(i).GetName() == lyr_name:
            break
        i += 1
    ds.DeleteLayer(i)

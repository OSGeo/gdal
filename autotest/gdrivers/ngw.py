#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NGW support.
# Author:   Dmitry Baryshnikov <polimax@mail.ru>
#
###############################################################################
# Copyright (c) 2018-2021, NextGIS <info@nextgis.com>
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

import os
import shutil
import sys

from osgeo import gdal

sys.path.append("../pymod")

import json
import random
import time
from datetime import datetime

import gdaltest
import pytest

pytestmark = [
    pytest.mark.require_driver("NGW"),
    pytest.mark.skipif(
        "CI" in os.environ,
        reason="NGW tests are flaky. See https://github.com/OSGeo/gdal/issues/4453",
    ),
]

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    gdaltest.ngw_test_server = "https://sandbox.nextgis.com"

    if check_availability(gdaltest.ngw_test_server) == False:
        pytest.skip()

    yield

    if gdaltest.group_id is not None:
        delete_url = (
            "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.group_id
        )

        gdaltest.ngw_ds = None

        assert gdal.GetDriverByName("NGW").Delete(delete_url) == gdal.CE_None, (
            "Failed to delete datasource " + delete_url + "."
        )

    gdaltest.ngw_ds = None
    gdaltest.clean_tmp()

    try:
        shutil.rmtree("gdalwmscache")
    except OSError:
        pass


def check_availability(url):
    # Sandbox cleans at 1:05 on monday (UTC)
    now = datetime.utcnow()
    if now.weekday() == 0:
        if now.hour >= 0 and now.hour < 4:
            return False

    version_url = url + "/api/component/pyramid/pkg_version"

    if gdaltest.gdalurlopen(version_url) is None:
        return False

    # Check quota
    quota_url = url + "/api/resource/quota"
    quota_conn = gdaltest.gdalurlopen(quota_url)
    try:
        quota_json = json.loads(quota_conn.read())
        quota_conn.close()
        if quota_json is None:
            return False
        limit = quota_json["limit"]
        count = quota_json["count"]
        if limit is None or count is None:
            return True
        return limit - count > 15
    except Exception:
        return False


def get_new_name():
    return "gdaltest_group_" + str(int(time.time())) + "_" + str(random.randint(10, 99))


###############################################################################
# Check create datasource.


def test_ngw_2():

    create_url = "NGW:" + gdaltest.ngw_test_server + "/resource/0/" + get_new_name()
    gdal.PushErrorHandler()
    description = "GDAL Raster test group"
    gdaltest.ngw_ds = gdal.GetDriverByName("NGW").Create(
        create_url,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=[
            "DESCRIPTION=" + description,
        ],
    )
    gdal.PopErrorHandler()

    assert gdaltest.ngw_ds is not None, "Create datasource failed."
    assert (
        gdaltest.ngw_ds.GetMetadataItem("description", "") == description
    ), "Did not get expected datasource description."

    assert (
        int(gdaltest.ngw_ds.GetMetadataItem("id", "")) > 0
    ), "Did not get expected datasource identifier."


###############################################################################
# Check rename datasource.


def test_ngw_3():

    new_name = get_new_name() + "_2"
    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    rename_url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + ds_resource_id

    assert (
        gdal.GetDriverByName("NGW").Rename(new_name, rename_url) == gdal.CE_None
    ), "Rename datasource failed."


###############################################################################
# Create the NGW raster layer


def test_ngw_4():

    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    src_ds = gdal.Open("data/rgbsmall.tif")
    resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + resource_id + "/rgbsmall"
    ds = gdal.GetDriverByName("NGW").CreateCopy(
        url, src_ds, options=["DESCRIPTION=Test raster create"]
    )
    src_ds = None

    assert ds is not None, "Raster create failed"

    ds_resource_id = ds.GetMetadataItem("id", "")
    gdaltest.raster_id = ds_resource_id
    gdaltest.group_id = resource_id

    ds = None

    # Upload 16bit raster
    src_ds = gdal.Open("data/int16.tif")
    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + resource_id + "/int16"
    ds = gdal.GetDriverByName("NGW").CreateCopy(
        url,
        src_ds,
        options=[
            "DESCRIPTION=Test 16bit raster create",
            "RASTER_QML_PATH=data/ngw/96.qml",
        ],
    )
    src_ds = None

    assert ds is not None, "Raster create failed"
    ds = None


###############################################################################
# Open the NGW dataset


def test_ngw_5():

    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    if gdaltest.raster_id is None:
        pytest.skip()

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.raster_id
    gdaltest.ngw_ds = gdal.OpenEx(url, gdal.OF_RASTER)

    assert gdaltest.ngw_ds is not None, "Open {} failed.".format(url)


###############################################################################
# Check various things about the configuration.


def test_ngw_6():

    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    assert (
        gdaltest.ngw_ds.RasterXSize == 1073741824
        and gdaltest.ngw_ds.RasterYSize == 1073741824
        and gdaltest.ngw_ds.RasterCount == 4
    ), "Wrong size or band count."

    wkt = gdaltest.ngw_ds.GetProjectionRef()
    assert wkt[:33] == 'PROJCS["WGS 84 / Pseudo-Mercator"', "Got wrong SRS: " + wkt

    gt = gdaltest.ngw_ds.GetGeoTransform()
    # -20037508.34, 0.037322767712175846, 0.0, 20037508.34, 0.0, -0.037322767712175846
    assert (
        gt[0] == pytest.approx(-20037508.34, abs=0.00001)
        or gt[3] == pytest.approx(20037508.34, abs=0.00001)
        or gt[1] == pytest.approx(0.037322767712175846, abs=0.00001)
        or gt[2] == pytest.approx(0.0, abs=0.00001)
        or gt[5] == pytest.approx(-0.037322767712175846, abs=0.00001)
        or gt[4] == pytest.approx(0.0, abs=0.00001)
    ), "Wrong geotransform. {}".format(gt)

    assert gdaltest.ngw_ds.GetRasterBand(1).GetOverviewCount() > 0, "No overviews!"
    assert (
        gdaltest.ngw_ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    ), "Wrong band data type."


###############################################################################
# Check checksum execute success for a small region.


def test_ngw_7():

    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    gdal.ErrorReset()
    gdal.SetConfigOption("CPL_ACCUM_ERROR_MSG", "ON")
    gdal.PushErrorHandler("CPLQuietErrorHandler")

    ovr_band = gdaltest.ngw_ds.GetRasterBand(1).GetOverview(21)
    assert ovr_band is not None
    ovr_band.Checksum()

    gdal.PopErrorHandler()
    gdal.SetConfigOption("CPL_ACCUM_ERROR_MSG", "OFF")
    msg = gdal.GetLastErrorMsg()

    assert gdal.GetLastErrorType() != gdal.CE_Failure, msg
    gdal.ErrorReset()


###############################################################################
# Test getting subdatasets from GetCapabilities


def test_ngw_8():

    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.group_id
    ds = gdal.OpenEx(url, gdal.OF_VECTOR | gdal.OF_RASTER)
    assert ds is not None, "Open of {} failed.".format(url)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert (
        subdatasets
    ), "Did not get expected subdataset count. Get {} subdatasets. Url: {}".format(
        len(subdatasets), url
    )

    name = subdatasets["SUBDATASET_0_NAME"]
    ds = gdal.OpenEx(name, gdal.OF_RASTER)
    assert ds is not None, "Open of {} failed.".format(name)

    ds = None

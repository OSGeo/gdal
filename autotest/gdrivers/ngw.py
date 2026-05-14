#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NGW support.
# Author:   Dmitry Baryshnikov <polimax@mail.ru>
#
###############################################################################
# Copyright (c) 2018-2025, NextGIS <info@nextgis.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import shutil
import sys

from osgeo import gdal

sys.path.append("../pymod")

import random
import time

import gdaltest
import pytest

NET_TIMEOUT = 130
NET_MAX_RETRY = 5
NET_RETRY_DELAY = 2

pytestmark = [
    pytest.mark.require_driver("NGW"),
    pytest.mark.random_order(disabled=True),
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

    version_url = url + "/api/component/pyramid/pkg_version"

    if gdaltest.gdalurlopen(version_url, timeout=NET_TIMEOUT) is None:
        return False

    return True


def get_new_name():
    return "gdaltest_group_" + str(int(time.time())) + "_" + str(random.randint(10, 99))


def check_tms(sub):
    ds = gdal.OpenEx(
        sub[0],
        gdal.OF_RASTER,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )

    assert ds is not None, f"Open {sub[0]} failed."

    assert (
        ds.RasterXSize == 1073741824
        and ds.RasterYSize == 1073741824
        and ds.RasterCount == 4
    ), f"Wrong size or band count for raster {sub[0]} [{sub[1]}]."

    wkt = ds.GetProjectionRef()
    assert 'PROJCS["WGS 84 / Pseudo-Mercator"' in wkt, "Got wrong SRS: " + wkt

    gt = ds.GetGeoTransform()
    expected_gt = [
        -20037508.34,
        0.037322767712175846,
        0.0,
        20037508.34,
        0.0,
        -0.037322767712175846,
    ]
    assert gt == pytest.approx(expected_gt, abs=0.00001), f"Wrong geotransform. {gt}"
    assert ds.GetRasterBand(1).GetOverviewCount() > 0, "No overviews!"
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8, "Wrong band data type."


###############################################################################
# Check create datasource.


def test_ngw_2():

    create_url = "NGW:" + gdaltest.ngw_test_server + "/resource/0/" + get_new_name()
    with gdal.quiet_errors():
        description = "GDAL Raster test group"
        gdaltest.ngw_ds = gdal.GetDriverByName("NGW").Create(
            create_url,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=[
                "DESCRIPTION=" + description,
                f"TIMEOUT={NET_TIMEOUT}",
                f"MAX_RETRY={NET_MAX_RETRY}",
                f"RETRY_DELAY={NET_RETRY_DELAY}",
            ],
        )

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
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("NGW").CreateCopy(
            url, src_ds, options=["DESCRIPTION=Test raster create"]
        )
        src_ds = None

        assert ds is not None, "Raster create failed"

        ds_resource_id = ds.GetMetadataItem("id", "")
        gdaltest.raster1_id = ds_resource_id
        ds = None

    # Upload 16bit raster
    src_ds = gdal.Open("data/int16.tif")
    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + resource_id + "/int16"
    with gdal.quiet_errors():
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
        ds_resource_id = ds.GetMetadataItem("id", "")
        gdaltest.raster2_id = ds_resource_id
        ds = None

    gdaltest.group_id = resource_id


###############################################################################
# Open the NGW dataset - rgbsmall


def test_ngw_5():

    # FIXME: depends on previous test
    if gdaltest.raster1_id is None:
        pytest.skip()

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.raster1_id
    ds = gdal.OpenEx(
        url,
        gdal.OF_RASTER,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )

    assert ds is not None, f"Open {url} failed."

    assert (
        ds.RasterXSize == 48 and ds.RasterYSize == 52 and ds.RasterCount == 4
    ), "Wrong size or band count."

    # Get subdatasets count
    assert len(ds.GetSubDatasets()) > 0


###############################################################################
# Open the NGW dataset - int16


def test_ngw_6():

    # FIXME: depends on previous test
    if gdaltest.raster2_id is None:
        pytest.skip()

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.raster2_id
    ds = gdal.OpenEx(
        url,
        gdal.OF_RASTER,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )

    assert ds is not None, f"Open {url} failed."

    assert (
        ds.RasterXSize == 20 and ds.RasterYSize == 20 and ds.RasterCount == 2
    ), "Wrong size or band count."

    # Get subdatasets count
    assert len(ds.GetSubDatasets()) > 0


###############################################################################
# Test getting subdatasets


def test_ngw_7():

    # FIXME: depends on previous test
    if gdaltest.raster1_id is None:
        pytest.skip()

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.raster1_id
    ds = gdal.OpenEx(
        url,
        gdal.OF_RASTER,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )

    # Get first style
    assert ds is not None, f"Open {url} failed."

    sub = ds.GetSubDatasets()[0]

    check_tms(sub)


###############################################################################
# Check checksum execute success for a small region.


def test_ngw_8():

    # FIXME: depends on previous test
    if gdaltest.raster1_id is None:
        pytest.skip()

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.raster1_id
    ds = gdal.OpenEx(
        url,
        gdal.OF_RASTER,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )

    assert ds is not None, f"Open {url} failed."

    band = ds.GetRasterBand(1)
    assert band is not None, "GetRasterBand 1 failed."

    assert (
        band.GetOverviewCount() > 0
    ), f"Expected overviews > 0, got {band.GetOverviewCount()}"
    for band_idx in range(band.GetOverviewCount()):
        ovr_band = band.GetOverview(band_idx)
        assert ovr_band is not None
        ovr_band.Checksum()


###############################################################################
# Check webmap as raster and basemap


def test_ngw_9():
    url = gdaltest.ngw_test_server + "/api/resource/search/?cls=webmap"

    result = gdaltest.gdalurlopen(url, timeout=NET_TIMEOUT)

    if result is not None:
        data = json.loads(result.read())

        for item in data:
            # Test first webmap
            url = f"NGW:{gdaltest.ngw_test_server}/resource/{item['resource']['id']}"
            check_tms([url, "webmap"])
            break

    url = gdaltest.ngw_test_server + "/api/resource/search/?cls=basemap_layer"

    result = gdaltest.gdalurlopen(url, timeout=NET_TIMEOUT)

    if result is not None:
        data = json.loads(result.read())

        for item in data:
            # Test first basemap_layer
            url = f"NGW:{gdaltest.ngw_test_server}/resource/{item['resource']['id']}"
            check_tms([url, "basemap"])
            break

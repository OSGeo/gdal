#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PlanetLabs mosaic driver test suite.
# Author:   Even Rouault, even dot rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2015, Planet Labs
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import shutil
import struct

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_driver("PLMosaic")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture(autouse=True)
def setup_and_cleanup(tmp_path):

    with gdal.config_option("PL_CACHE_PATH", str(tmp_path)):
        yield


@pytest.fixture(scope="module")
def server():

    process, port = webserver.launch(handler=webserver.DispatcherHttpHandler)

    if port == 0:
        pytest.skip()

    import collections

    WebServer = collections.namedtuple("WebServer", "process port")

    yield WebServer(process, port)

    webserver.server_stop(process, port)


###############################################################################
# Error: no API_KEY


def test_plmosaic_2():

    with gdal.config_option("PL_URL", "/vsimem/root"), gdal.quiet_errors():
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER)
    assert ds is None

    assert "API_KEY" in gdal.GetLastErrorMsg()


###############################################################################
# Error case: invalid root URL


def test_plmosaic_3():

    with gdal.config_option("PL_URL", "/vsimem/does_not_exist/"), gdal.quiet_errors():
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds is None

    assert "404" in gdal.GetLastErrorMsg()


###############################################################################
# Error case: invalid JSON


def test_plmosaic_4(tmp_vsimem):

    root = str(tmp_vsimem / "root")

    gdal.FileFromMemBuffer(root, """{""")

    with gdal.config_option("PL_URL", root), gdal.quiet_errors():
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds is None

    assert "JSON parsing error" in gdal.GetLastErrorMsg()


###############################################################################
# Error case: not a JSON dictionary


def test_plmosaic_5(tmp_vsimem):

    root = str(tmp_vsimem / "root")

    gdal.FileFromMemBuffer(root, """null""")

    with gdal.config_option("PL_URL", root), gdal.quiet_errors():
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds is None

    assert "JSON parsing error" in gdal.GetLastErrorMsg()


###############################################################################
# Error case: missing "mosaics" element


def test_plmosaic_6(tmp_vsimem):

    root = str(tmp_vsimem / "root")

    gdal.FileFromMemBuffer(root, """{}""")

    with gdal.config_option("PL_URL", root), gdal.quiet_errors():
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds is None


###############################################################################
# Valid root but no mosaics


def test_plmosaic_7(tmp_vsimem):

    root = str(tmp_vsimem / "root")

    gdal.FileFromMemBuffer(
        root,
        """{
    "mosaics": [],
}""",
    )

    with gdal.config_option("PL_URL", root):
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds is None


###############################################################################
# Valid root with 2 mosaics


@pytest.fixture()
def valid_root_with_two_mosaics(server):

    url_root = f"http://localhost:{server.port}"

    handler = webserver.FileHandler(
        {
            "/root": json.dumps(
                {
                    "_links": {"_next": f"{url_root}/root/?page=2"},
                    "mosaics": [
                        {
                            "id": "my_mosaic_id",
                            "name": "my_mosaic_name",
                            "coordinate_system": "EPSG:3857",
                            "_links": {"_self": f"{url_root}/root/my_mosaic"},
                            "quad_download": "true",
                        }
                    ],
                }
            ).encode(),
            "/root/?page=2": json.dumps(
                {
                    "_links": {"_next": None},
                    "mosaics": [
                        {
                            "id": "another_mosaic_id",
                            "name": "another_mosaic_name",
                            "coordinate_system": "EPSG:3857",
                            "_links": {"_self": f"{url_root}/root/another_mosaic"},
                            "quad_download": True,
                        },
                        {
                            "id": "this_one_will_be_ignored",
                            "name": "this_one_will_be_ignored",
                            "coordinate_system": "EPSG:1234",
                            "_links": {
                                "_self": f"{url_root}/root/this_one_will_be_ignored"
                            },
                            "quad_download": True,
                        },
                    ],
                }
            ).encode(),
        }
    )

    with webserver.install_http_handler(handler):
        yield


@pytest.mark.usefixtures("valid_root_with_two_mosaics")
def test_plmosaic_8(server):

    with gdal.config_option("PL_URL", f"http://localhost:{server.port}/root"):
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds.GetMetadata("SUBDATASETS") == {
        "SUBDATASET_2_NAME": "PLMOSAIC:mosaic=another_mosaic_name",
        "SUBDATASET_2_DESC": "Mosaic another_mosaic_name",
        "SUBDATASET_1_NAME": "PLMOSAIC:mosaic=my_mosaic_name",
        "SUBDATASET_1_DESC": "Mosaic my_mosaic_name",
    }
    ds = None


###############################################################################
# Error case: invalid mosaic


@pytest.mark.usefixtures("valid_root_with_two_mosaics")
def test_plmosaic_9(tmp_vsimem):

    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=does_not_exist"],
        )

    assert ds is None
    assert f"{tmp_vsimem}/root/?name__is=does_not_exist" in gdal.GetLastErrorMsg()


###############################################################################
# Invalid mosaic definition: invalid JSON


@pytest.mark.usefixtures("valid_root_with_two_mosaics")
def test_plmosaic_9bis(tmp_vsimem):

    gdal.FileFromMemBuffer(f"{tmp_vsimem}/root/?name__is=my_mosaic", """{""")
    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic"],
        )
    assert ds is None and gdal.GetLastErrorMsg().find("JSON parsing error") >= 0


###############################################################################
# Invalid mosaic definition: JSON without mosaics array


@pytest.mark.usefixtures("valid_root_with_two_mosaics")
def test_plmosaic_9ter(tmp_vsimem):

    gdal.FileFromMemBuffer(f"{tmp_vsimem}/root/?name__is=my_mosaic", """{}""")
    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic"],
        )
    assert ds is None and gdal.GetLastErrorMsg().find("No mosaic my_mosaic") >= 0


###############################################################################
# Invalid mosaic definition: missing parameters


def test_plmosaic_10(tmp_vsimem):

    gdal.FileFromMemBuffer(
        f"{tmp_vsimem}/root/?name__is=my_mosaic",
        """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic"
}]
}""",
    )
    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic"],
        )
    assert ds is None and gdal.GetLastErrorMsg().find("Missing required parameter") >= 0


###############################################################################
# Invalid mosaic definition: unsupported projection


def test_plmosaic_11(tmp_vsimem):

    gdal.FileFromMemBuffer(
        f"{tmp_vsimem}/root/?name__is=my_mosaic",
        """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic",
    "coordinate_system": "EPSG:1234",
    "datatype": "byte",
    "grid": {
        "quad_size": 4096,
        "resolution": 4.77731426716
    }
}]
}""",
    )
    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic"],
        )
    assert (
        ds is None and gdal.GetLastErrorMsg().find("Unsupported coordinate_system") >= 0
    )


###############################################################################
# Invalid mosaic definition: unsupported datatype


def test_plmosaic_12(tmp_vsimem):

    gdal.FileFromMemBuffer(
        f"{tmp_vsimem}/root/?name__is=my_mosaic",
        """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "blabla",
    "grid": {
        "quad_size": 4096,
        "resolution": 4.77731426716
    }
}]
}""",
    )
    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic"],
        )
    assert ds is None and gdal.GetLastErrorMsg().find("Unsupported data_type") >= 0


###############################################################################
# Invalid mosaic definition: unsupported resolution


def test_plmosaic_13(tmp_vsimem):

    gdal.FileFromMemBuffer(
        f"{tmp_vsimem}/root/?name__is=my_mosaic",
        """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "grid": {
        "quad_size": 4096,
        "resolution": 1.1234
    }
}]
}""",
    )
    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic"],
        )
    assert ds is None and gdal.GetLastErrorMsg().find("Unsupported resolution") >= 0


###############################################################################
# Invalid mosaic definition: unsupported quad_size


def test_plmosaic_14(tmp_vsimem):

    gdal.FileFromMemBuffer(
        f"{tmp_vsimem}/root/?name__is=my_mosaic",
        """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "grid": {
        "quad_size": 1234,
        "resolution": 4.77731426716
    }
}]
}""",
    )
    with gdal.config_option("PL_URL", f"{tmp_vsimem}/root"), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic"],
        )
    assert ds is None and gdal.GetLastErrorMsg().find("Unsupported quad_size") >= 0


###############################################################################
# Nearly valid mosaic definition. Warning about invalid links.tiles


def test_plmosaic_15(tmp_vsimem):

    root = str(tmp_vsimem / "root")

    gdal.FileFromMemBuffer(
        f"{root}/?name__is=my_mosaic",
        json.dumps(
            {
                "mosaics": [
                    {
                        "id": "my_mosaic_id",
                        "name": "my_mosaic",
                        "coordinate_system": "EPSG:3857",
                        "datatype": "byte",
                        "grid": {"quad_size": 4096, "resolution": 4.77731426716},
                        "first_acquired": "first_date",
                        "last_acquired": "last_date",
                        "_links": {"tiles": f"{root}/my_mosaic/tiles/foo"},
                        "quad_download": True,
                    }
                ]
            }
        ),
    )
    with gdal.config_option("PL_URL", root), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic", "CACHE_PATH=tmp"],
        )
    assert gdal.GetLastErrorMsg().find("Invalid _links.tiles") >= 0
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetOverview(0) is None
    ds = None


###############################################################################
# Valid mosaic definition


@pytest.fixture()
def valid_mosaic_handler(server):

    url_root = f"http://localhost:{server.port}"

    handler = webserver.FileHandler(
        {
            "/root/?name__is=my_mosaic": json.dumps(
                {
                    "mosaics": [
                        {
                            "id": "my_mosaic_id",
                            "name": "my_mosaic",
                            "coordinate_system": "EPSG:3857",
                            "datatype": "byte",
                            "grid": {"quad_size": 4096, "resolution": 4.77731426716},
                            "first_acquired": "first_date",
                            "last_acquired": "last_date",
                            "_links": {
                                "tiles": url_root
                                + "/root/my_mosaic/tiles{0-3}/{z}/{x}/{y}.png"
                            },
                            "quad_download": True,
                        }
                    ]
                }
            ).encode(),
            # Valid root: one single mosaic, should open the dataset directly
            "/root": json.dumps(
                {
                    "mosaics": [
                        {
                            "id": "my_mosaic_id",
                            "name": "my_mosaic",
                            "coordinate_system": "EPSG:3857",
                            "_links": {"_self": url_root + "/root/my_mosaic"},
                            "quad_download": True,
                        }
                    ],
                }
            ).encode(),
        }
    )

    with webserver.install_http_handler(handler):
        yield handler


@pytest.fixture()
def valid_mosaic(valid_mosaic_handler, server):
    yield f"http://localhost:{server.port}/root"


def test_plmosaic_16(valid_mosaic):

    with gdal.config_option("PL_URL", valid_mosaic), gdaltest.error_handler():
        ds = gdal.OpenEx("PLMosaic:api_key=foo,unsupported_option=val", gdal.OF_RASTER)
    assert (
        ds is None
        and gdal.GetLastErrorMsg().find("Unsupported option unsupported_option") >= 0
    )

    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds.GetMetadata("SUBDATASETS") == {}
    assert ds.GetMetadata() == {
        "LAST_ACQUIRED": "last_date",
        "NAME": "my_mosaic",
        "FIRST_ACQUIRED": "first_date",
    }
    ds = None


###############################################################################
# Open with explicit MOSAIC dataset open option


@pytest.mark.require_driver("PNG")
@pytest.mark.require_driver("WMS")
def test_plmosaic_17(tmp_path, tmp_vsimem, valid_mosaic, valid_mosaic_handler):

    cache_path = tmp_path / "plmosaic_cache"

    valid_mosaic_handler.set_fallback(tmp_vsimem)

    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic", f"CACHE_PATH={tmp_path}"],
        )
    assert ds is not None
    assert ds.GetMetadata() == {
        "LAST_ACQUIRED": "last_date",
        "NAME": "my_mosaic",
        "FIRST_ACQUIRED": "first_date",
    }
    assert ds.GetProjectionRef().find("3857") >= 0
    assert ds.RasterXSize == 8388608
    assert ds.RasterYSize == 8388608
    got_gt = ds.GetGeoTransform()
    expected_gt = (
        -20037508.34,
        4.7773142671600004,
        0.0,
        20037508.34,
        0.0,
        -4.7773142671600004,
    )
    for i in range(6):
        assert got_gt[i] == pytest.approx(
            expected_gt[i], abs=1e-8
        ), ds.GetGeoTransform()
    assert (
        ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == "PIXEL"
    ), ds.GetMetadata("IMAGE_STRUCTURE")
    assert ds.GetRasterBand(1).GetOverviewCount() == 15
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert (
        ds.GetRasterBand(1).GetOverview(ds.GetRasterBand(1).GetOverviewCount()) is None
    )
    assert ds.GetRasterBand(1).GetOverview(0) is not None

    try:
        shutil.rmtree(cache_path)
    except OSError:
        pass

    for i in range(12):
        # Read at one nonexistent position.
        ds.GetRasterBand(1).ReadRaster(4096 * i, 0, 1, 1)
        # assert gdal.GetLastErrorMsg() == ""
    for i in range(11, -1, -1):
        # Again in the same quad, but in different block, to test cache
        ds.GetRasterBand(1).ReadRaster(4096 * i + 256, 0, 1, 1)
        # assert gdal.GetLastErrorMsg() == ""
    for i in range(12):
        # Again in the same quad, but in different block, to test cache
        ds.GetRasterBand(1).ReadRaster(4096 * i + 512, 256, 1, 1)
        # assert gdal.GetLastErrorMsg() == ""

    ds.FlushCache()

    # Invalid tile content
    # valid_mosaic_handler.add_file("/root/my_mosaic_id/quads/0-2047/full", b"garbage")
    # gdal.FileFromMemBuffer(f"{valid_mosaic}/my_mosaic_id/quads/0-2047/full", "garbage")
    gdal.FileFromMemBuffer(
        tmp_vsimem / "root/my_mosaic_id/quads/0-2047/full", "garbage"
    )
    with gdal.quiet_errors():
        ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)

    os.stat(f"{cache_path}/my_mosaic/my_mosaic_0-2047.tif")

    ds.FlushCache()
    shutil.rmtree(cache_path)

    # GeoTIFF but with wrong dimensions
    gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "/my_mosaic_id/quads/0-2047/full", 1, 1, 1
    )
    with gdal.quiet_errors():
        ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)

    os.stat(f"{cache_path}/my_mosaic/my_mosaic_0-2047.tif")

    ds.FlushCache()
    shutil.rmtree(cache_path)

    # Good GeoTIFF
    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "root/my_mosaic_id/quads/0-2047/full",
        4096,
        4096,
        4,
        options=["INTERLEAVE=BAND", "SPARSE_OK=YES"],
    )
    tmp_ds.GetRasterBand(1).Fill(255)
    tmp_ds = None

    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack("B", val)[0]
    assert val == 255

    os.stat(f"{cache_path}/my_mosaic/my_mosaic_0-2047.tif")

    ds.FlushCache()

    # Read again from file cache.
    # We change the file behind the scene (but not changing its size)
    # to demonstrate that the cached tile is still use
    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "my_mosaic_id/quads/0-2047/full",
        4096,
        4096,
        4,
        options=["INTERLEAVE=BAND", "SPARSE_OK=YES"],
    )
    tmp_ds.GetRasterBand(1).Fill(1)
    tmp_ds = None
    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack("B", val)[0]
    assert val == 255

    ds = None

    # Read again from file cache, but with TRUST_CACHE=YES
    # delete the full GeoTIFF before
    gdal.Unlink(tmp_vsimem / "my_mosaic_id/quads/0-2047/full")
    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx(
            f"PLMosaic:API_KEY=foo,MOSAIC=my_mosaic,CACHE_PATH={tmp_path},TRUST_CACHE=YES",
            gdal.OF_RASTER,
        )

    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack("B", val)[0]
    assert val == 255
    ds = None

    # Read again from file cache but the metatile has changed in between
    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic", f"CACHE_PATH={tmp_path}"],
        )

    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "root/my_mosaic_id/quads/0-2047/full",
        4096,
        4096,
        4,
        options=["INTERLEAVE=BAND", "SPARSE_OK=YES"],
    )
    tmp_ds.SetMetadataItem("foo", "bar")
    tmp_ds.GetRasterBand(1).Fill(254)
    tmp_ds = None

    val = ds.ReadRaster(0, 0, 1, 1)
    val = struct.unpack("B" * 4, val)
    assert val == (254, 0, 0, 0)


###############################################################################
# Test location info


def test_plmosaic_18(valid_mosaic, valid_mosaic_handler):

    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic", "CACHE_PATH=tmp"],
        )

    ret = ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
    assert ret == """<LocationInfo />
"""
    old_ret = ret
    ret = ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
    assert ret == old_ret

    valid_mosaic_handler.add_file(
        "/root/my_mosaic_id/quads/0-2047/items",
        b"""{
    "items": [
        { "link": "foo" }
    ]
}""",
    )

    ds.FlushCache()

    ret = ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
    assert ret == """<LocationInfo>
  <Scenes>
    <Scene>
      <link>foo</link>
    </Scene>
  </Scenes>
</LocationInfo>
"""
    ds = None


###############################################################################
# Try error in saving in cache


def test_plmosaic_19(tmp_vsimem, valid_mosaic, valid_mosaic_handler):

    valid_mosaic_handler.set_fallback(tmp_vsimem)

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "root/my_mosaic_id/quads/0-2047/full",
        4096,
        4096,
        4,
        options=["INTERLEAVE=BAND", "SPARSE_OK=YES"],
    ) as tmp_ds:
        tmp_ds.GetRasterBand(1).Fill(254)

    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=[
                "API_KEY=foo",
                "MOSAIC=my_mosaic",
                "CACHE_PATH=/does_not_exist",
            ],
        )
    with gdal.quiet_errors():
        val = ds.ReadRaster(0, 0, 1, 1)
    val = struct.unpack("B" * 4, val)
    assert val == (254, 0, 0, 0)

    val = ds.ReadRaster(256, 0, 1, 1)
    val = struct.unpack("B" * 4, val)
    assert val == (254, 0, 0, 0)
    ds = None


###############################################################################
# Try disabling cache


def test_plmosaic_20(tmp_vsimem, valid_mosaic, valid_mosaic_handler):

    valid_mosaic_handler.set_fallback(tmp_vsimem)

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "root/my_mosaic_id/quads/0-2047/full",
        4096,
        4096,
        4,
        options=["INTERLEAVE=BAND", "SPARSE_OK=YES"],
    ) as tmp_ds:
        tmp_ds.GetRasterBand(1).Fill(254)

    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=["API_KEY=foo", "MOSAIC=my_mosaic", "CACHE_PATH="],
        )
    val = ds.ReadRaster(0, 0, 1, 1)
    val = struct.unpack("B" * 4, val)
    assert val == (254, 0, 0, 0)

    val = ds.ReadRaster(256, 0, 1, 1)
    val = struct.unpack("B" * 4, val)
    assert val == (254, 0, 0, 0)
    ds = None


###############################################################################
# Try use_tiles


@pytest.mark.require_driver("PNG")
@pytest.mark.require_driver("WMS")
def test_plmosaic_21(valid_mosaic, valid_mosaic_handler):

    with gdal.config_option("PL_URL", valid_mosaic):
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=[
                "API_KEY=foo",
                "MOSAIC=my_mosaic",
                "CACHE_PATH=",
                "USE_TILES=YES",
            ],
        )

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds.ReadRaster(256, 512, 1, 1)
    assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds.GetRasterBand(1).ReadRaster(256, 512, 1, 1)
    assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds.GetRasterBand(1).ReadBlock(1, 2)
    assert gdal.GetLastErrorMsg() != ""

    valid_mosaic_handler.add_file(
        "/root/?name__is=mosaic_uint16",
        json.dumps(
            {
                "mosaics": [
                    {
                        "id": "mosaic_uint16",
                        "name": "mosaic_uint16",
                        "coordinate_system": "EPSG:3857",
                        "datatype": "uint16",
                        "grid": {"quad_size": 4096, "resolution": 4.77731426716},
                        "first_acquired": "first_date",
                        "last_acquired": "last_date",
                        "_links": {
                            "tiles": valid_mosaic
                            + "/mosaic_uint16/tiles{0-3}/{z}/{x}/{y}.png"
                        },
                        "quad_download": True,
                    }
                ]
            }
        ).encode(),
    )

    # Should emit a warning
    gdal.ErrorReset()
    with gdal.config_option("PL_URL", valid_mosaic), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=[
                "API_KEY=foo",
                "MOSAIC=mosaic_uint16",
                "CACHE_PATH=",
                "USE_TILES=YES",
            ],
        )
    assert (
        gdal.GetLastErrorMsg().find(
            "Cannot use tile API for full resolution data on non Byte mosaic"
        )
        >= 0
    )

    valid_mosaic_handler.add_file(
        "/root/?name__is=mosaic_without_tiles",
        b"""{
"mosaics": [{
    "id": "mosaic_without_tiles",
    "name": "mosaic_without_tiles",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "grid": {
        "quad_size": 4096,
        "resolution": 4.77731426716
    },
    "first_acquired": "first_date",
    "last_acquired": "last_date",
    "quad_download": true
}]
}""",
    )

    # Should emit a warning
    gdal.ErrorReset()
    with gdal.config_option("PL_URL", valid_mosaic), gdaltest.error_handler():
        ds = gdal.OpenEx(
            "PLMosaic:",
            gdal.OF_RASTER,
            open_options=[
                "API_KEY=foo",
                "MOSAIC=mosaic_without_tiles",
                "CACHE_PATH=",
                "USE_TILES=YES",
            ],
        )
    assert (
        gdal.GetLastErrorMsg().find(
            "Cannot find tile definition, so use_tiles will be ignored"
        )
        >= 0
    )


###############################################################################
# Valid mosaic definition with bbox


def test_plmosaic_with_bbox(tmp_vsimem):

    root = str(tmp_vsimem / "root")

    gdal.FileFromMemBuffer(
        f"{root}/?name__is=my_mosaic",
        json.dumps(
            {
                "mosaics": [
                    {
                        "id": "my_mosaic_id",
                        "name": "my_mosaic",
                        "coordinate_system": "EPSG:3857",
                        "datatype": "byte",
                        "grid": {"quad_size": 4096, "resolution": 4.77731426716},
                        "bbox": [-100, 30, -90, 40],
                        "first_acquired": "first_date",
                        "last_acquired": "last_date",
                        "_links": {
                            "tiles": root + "/my_mosaic/tiles{0-3}/{z}/{x}/{y}.png"
                        },
                        "quad_download": True,
                    }
                ]
            }
        ),
    )

    # Valid root: one single mosaic, should open the dataset directly
    gdal.FileFromMemBuffer(
        root,
        json.dumps(
            {
                "mosaics": [
                    {
                        "id": "my_mosaic_id",
                        "name": "my_mosaic",
                        "coordinate_system": "EPSG:3857",
                        "_links": {"_self": root + "/my_mosaic"},
                        "quad_download": True,
                    }
                ],
            }
        ),
    )

    with gdal.config_option("PL_URL", root):
        ds = gdal.OpenEx("PLMosaic:", gdal.OF_RASTER, open_options=["API_KEY=foo"])
    assert ds.RasterXSize == 233472
    assert ds.RasterYSize == 286720
    got_gt = ds.GetGeoTransform()
    expected_gt = (
        -11134123.286585508,
        4.77731426716,
        0.0,
        4872401.930333553,
        0.0,
        -4.77731426716,
    )
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)

    # Good GeoTIFF
    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        f"{root}/my_mosaic_id/quads/455-1272/full",
        4096,
        4096,
        4,
        options=["INTERLEAVE=BAND", "SPARSE_OK=YES"],
    )
    tmp_ds.GetRasterBand(1).Fill(125)
    tmp_ds = None

    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack("B", val)[0]
    assert val == 125

    gdal.FileFromMemBuffer(
        f"{root}/my_mosaic_id/quads/455-1272/items",
        """{
    "items": [
        { "link": "bar" }
    ]
}""",
    )

    ret = ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
    assert ret == """<LocationInfo>
  <Scenes>
    <Scene>
      <link>bar</link>
    </Scene>
  </Scenes>
</LocationInfo>
"""

#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for MBTiles driver.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012-2016, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

import gdaltest
import pytest
import webserver

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("MBTILES")


###############################################################################
#


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
# Basic test


@pytest.mark.require_driver("JPEG")
def test_mbtiles_2():

    ds = gdal.OpenEx("data/mbtiles/world_l1.mbtiles", open_options=["USE_BOUNDS=NO"])
    assert ds is not None

    assert ds.RasterCount == 4, "expected 3 bands"

    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 1
    ), "did not get expected overview count"

    expected_cs_tab = [6324, 19386, 45258]
    expected_cs_tab_jpeg8 = [6016, 13996, 45168]
    expected_cs_tab_jpeg9b = [6016, 14034, 45168]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert (
            ds.GetRasterBand(i + 1).GetColorInterpretation() == gdal.GCI_RedBand + i
        ), "bad color interpretation"
        expected_cs = expected_cs_tab[i]
        assert (
            cs == expected_cs
            or cs == expected_cs_tab_jpeg8[i]
            or cs == expected_cs_tab_jpeg9b[i]
        ), "for band %d, cs = %d, different from expected_cs = %d" % (
            i + 1,
            cs,
            expected_cs,
        )

    expected_cs_tab = [16642, 15772, 10029]
    expected_cs_tab_jpeg8 = [16621, 14725, 8988]
    expected_cs_tab_jpeg9b = [16621, 14723, 8988]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        expected_cs = expected_cs_tab[i]
        assert (
            cs == expected_cs
            or cs == expected_cs_tab_jpeg8[i]
            or cs == expected_cs_tab_jpeg9b[i]
        ), "for overview of band %d, cs = %d, different from expected_cs = %d" % (
            i + 1,
            cs,
            expected_cs,
        )

    assert ds.GetProjectionRef().find("3857") != -1, (
        "projection_ref = %s" % ds.GetProjectionRef()
    )

    gt = ds.GetGeoTransform()
    expected_gt = (
        -20037508.342789244,
        78271.516964020484,
        0.0,
        20037508.342789244,
        0.0,
        -78271.516964020484,
    )
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-15), "bad gt"

    md = ds.GetMetadata()
    assert md["bounds"] == "-180.0,-85,180,85", "bad metadata"

    ds = None


###############################################################################
# Open a /vsicurl/ DB


@pytest.mark.require_curl()
@pytest.mark.skipif(
    sys.platform == "darwin" and gdal.GetConfigOption("TRAVIS", None) is not None,
    reason="Hangs on MacOSX Travis sometimes. Not sure why.",
)
@gdaltest.disable_exceptions()
def test_mbtiles_3():

    # Check that we have SQLite VFS support
    with gdal.quiet_errors():
        ds = ogr.GetDriverByName("SQLite").CreateDataSource("/vsimem/mbtiles_3.db")
    if ds is None:
        pytest.skip()
    ds = None
    gdal.Unlink("/vsimem/mbtiles_3.db")

    ds = gdal.Open(
        "/vsicurl/http://a.tiles.mapbox.com/v3/mapbox.geography-class.mbtiles"
    )
    if ds is None:
        # Just skip. The service isn't perfectly reliable sometimes
        pytest.skip(
            "Cannot access http://a.tiles.mapbox.com/v3/mapbox.geography-class.mbtiles"
        )

    # long=2,lat=49 in WGS 84 --> x=222638,y=6274861 in Google Mercator
    locationInfo = ds.GetRasterBand(1).GetMetadataItem(
        "GeoPixel_222638_6274861", "LocationInfo"
    )
    if locationInfo is None or locationInfo.find("France") == -1:
        print(locationInfo)
        gdaltest.skip_on_travis()
        pytest.fail("did not get expected LocationInfo")

    locationInfo2 = (
        ds.GetRasterBand(1)
        .GetOverview(5)
        .GetMetadataItem("GeoPixel_222638_6274861", "LocationInfo")
    )
    if locationInfo2 != locationInfo:
        print(locationInfo2)
        gdaltest.skip_on_travis()
        pytest.fail("did not get expected LocationInfo on overview")


###############################################################################
#


@pytest.mark.require_curl()
@pytest.mark.require_driver("JPEG")
def test_mbtiles_http_jpeg_three_bands(server):

    handler = webserver.FileHandler(
        {"/world_l1.mbtiles": open("data/mbtiles/world_l1.mbtiles", "rb").read()}
    )
    with webserver.install_http_handler(handler):
        ds = gdal.Open("/vsicurl/http://localhost:%d/world_l1.mbtiles" % server.port)
    assert ds is not None


###############################################################################
#


@pytest.mark.require_curl()
@pytest.mark.require_driver("JPEG")
def test_mbtiles_http_jpeg_single_band(server):

    handler = webserver.FileHandler(
        {"/byte_jpeg.mbtiles": open("data/mbtiles/byte_jpeg.mbtiles", "rb").read()}
    )
    with webserver.install_http_handler(handler):
        ds = gdal.Open("/vsicurl/http://localhost:%d/byte_jpeg.mbtiles" % server.port)
    assert ds is not None


###############################################################################
#


@pytest.mark.require_curl()
@pytest.mark.require_driver("JPEG")
def test_mbtiles_http_png(server):

    handler = webserver.FileHandler(
        {"/byte.mbtiles": open("data/mbtiles/byte.mbtiles", "rb").read()}
    )
    with webserver.install_http_handler(handler):
        ds = gdal.Open("/vsicurl/http://localhost:%d/byte.mbtiles" % server.port)
    assert ds is not None


###############################################################################
# Basic test without any option


@pytest.mark.require_driver("JPEG")
def test_mbtiles_4():

    ds = gdal.Open("data/mbtiles/world_l1.mbtiles")
    assert ds is not None

    assert ds.RasterCount == 4, "expected 4 bands"

    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 1
    ), "did not get expected overview count"

    assert ds.RasterXSize == 512 and ds.RasterYSize == 510, "bad dimensions"

    gt = ds.GetGeoTransform()
    expected_gt = (
        -20037508.342789244,
        78271.516964020484,
        0.0,
        19971868.880408563,
        0.0,
        -78271.516964020484,
    )
    assert gt == pytest.approx(expected_gt, rel=1e-15), "bad gt"

    ds = None


###############################################################################
# Test write support of a single band dataset


@pytest.mark.require_driver("PNG")
def test_mbtiles_5():

    src_ds = gdal.Open("data/byte.tif")
    gdaltest.mbtiles_drv.CreateCopy("/vsimem/mbtiles_5.mbtiles", src_ds)
    src_ds = None

    ds = gdal.OpenEx("/vsimem/mbtiles_5.mbtiles", open_options=["BAND_COUNT=2"])
    assert ds.RasterXSize == 19 and ds.RasterYSize == 19
    assert ds.RasterCount == 2
    got_gt = ds.GetGeoTransform()
    expected_gt = (
        -13095853.550435878,
        76.437028285176254,
        0.0,
        4015708.8887064462,
        0.0,
        -76.437028285176254,
    )
    for i in range(6):
        assert expected_gt[i] == pytest.approx(got_gt[i], rel=1e-6)
    got_cs = ds.GetRasterBand(1).Checksum()
    assert got_cs == 4118
    got_cs = ds.GetRasterBand(2).Checksum()
    assert got_cs == 4406
    got_md = ds.GetMetadata()
    expected_md = {
        "ZOOM_LEVEL": "11",
        "minzoom": "11",
        "maxzoom": "11",
        "name": "mbtiles_5",
        "format": "png",
        "bounds": "-117.6420540294745,33.89160566594387,-117.6290077648261,33.90243460427036",
        "version": "1.1",
        "type": "overlay",
        "description": "mbtiles_5",
    }
    assert set(got_md.keys()) == set(expected_md.keys())
    for key in got_md:
        assert key == "bounds" or got_md[key] == expected_md[key]
    ds = None

    gdal.Unlink("/vsimem/mbtiles_5.mbtiles")


###############################################################################
# Test write support with options


@pytest.mark.require_driver("JPEG")
def test_mbtiles_6():

    # Test options
    src_ds = gdal.Open("data/byte.tif")
    options = []
    options += ["TILE_FORMAT=JPEG"]
    options += ["QUALITY=50"]
    options += ["NAME=name"]
    options += ["DESCRIPTION=description"]
    options += ["TYPE=baselayer"]
    options += ["VERSION=version"]
    options += ["WRITE_BOUNDS=no"]
    gdaltest.mbtiles_drv.CreateCopy("tmp/mbtiles_6.mbtiles", src_ds, options=options)
    src_ds = None

    ds = gdal.Open("tmp/mbtiles_6.mbtiles")
    got_cs = ds.GetRasterBand(1).Checksum()
    assert got_cs != 0
    got_md = ds.GetMetadata()
    expected_md = {
        "ZOOM_LEVEL": "11",
        "minzoom": "11",
        "maxzoom": "11",
        "format": "jpg",
        "version": "version",
        "type": "baselayer",
        "name": "name",
        "description": "description",
    }
    assert got_md == expected_md
    ds = None

    gdal.Unlink("tmp/mbtiles_6.mbtiles")


###############################################################################
# Test building overview


@pytest.mark.require_driver("PNG")
def test_mbtiles_7():

    src_ds = gdal.Open("data/small_world.tif")
    data = src_ds.ReadRaster()
    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", src_ds.RasterXSize * 2, src_ds.RasterYSize * 2, src_ds.RasterCount
    )
    mem_ds.SetProjection(src_ds.GetProjectionRef())
    gt = src_ds.GetGeoTransform()
    gt = [gt[i] for i in range(6)]
    gt[1] /= 2
    gt[5] /= 2
    mem_ds.SetGeoTransform(gt)
    mem_ds.WriteRaster(
        0,
        0,
        mem_ds.RasterXSize,
        mem_ds.RasterYSize,
        data,
        src_ds.RasterXSize,
        src_ds.RasterYSize,
    )
    src_ds = None

    gdaltest.mbtiles_drv.CreateCopy(
        "/vsimem/mbtiles_7.mbtiles",
        mem_ds,
        options=["TILE_FORMAT=PNG8", "DITHER=YES", "RESAMPLING=NEAREST"],
    )
    mem_ds = None

    ds = gdal.Open("/vsimem/mbtiles_7.mbtiles", gdal.GA_Update)
    ds.BuildOverviews("NEAR", [2, 4])
    ds = None

    ds = gdal.Open("/vsimem/mbtiles_7.mbtiles")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    got_ovr_cs = [
        ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(ds.RasterCount)
    ]
    assert got_ovr_cs in (
        [21179, 22577, 11996, 17849],
        [24072, 24971, 15966, 17849],
    )  # that one got on Tamas' x64 configuration
    assert ds.GetMetadataItem("minzoom") == "0", ds.GetMetadata()
    ds = None

    ds = gdal.Open("/vsimem/mbtiles_7.mbtiles", gdal.GA_Update)
    ds.BuildOverviews("NONE", [])
    ds = None

    ds = gdal.Open("/vsimem/mbtiles_7.mbtiles")
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetMetadataItem("minzoom") == "1", ds.GetMetadata()
    ds = None

    gdal.Unlink("/vsimem/mbtiles_7.mbtiles")


###############################################################################
# Test building overview


@pytest.mark.require_driver("PNG")
def test_mbtiles_overview_minzoom():

    filename = "/vsimem/test_mbtiles_overview_minzoom.mbtiles"

    gdal.Translate(filename, "data/byte.tif", options="-outsize 1024 0")
    ds = gdal.Open(filename, gdal.GA_Update)
    assert ds.GetMetadataItem("minzoom") == "17"
    assert ds.GetMetadataItem("maxzoom") == "17"
    ds.BuildOverviews("NEAR", [2])
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetMetadataItem("minzoom") == "16"
    assert ds.GetMetadataItem("maxzoom") == "17"
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    ds.BuildOverviews("NEAR", [8])
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetOverviewCount() == 3
    assert ds.GetMetadataItem("minzoom") == "14"
    assert ds.GetMetadataItem("maxzoom") == "17"
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Single band with 24 bit color table, PNG


@pytest.mark.require_driver("PNG")
def test_mbtiles_8():

    src_ds = gdal.Open("data/small_world_pct.tif")
    out_ds = gdaltest.mbtiles_drv.CreateCopy(
        "/vsimem/mbtiles_8.mbtiles", src_ds, options=["RESAMPLING=NEAREST"]
    )
    out_ds = None
    src_ds = None

    expected_cs = [993, 50461, 64354]
    out_ds = gdal.Open("/vsimem/mbtiles_8.mbtiles")
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None
    assert out_ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    out_ds = None

    # 512 pixel tiles
    src_ds = gdal.Open("data/small_world_pct.tif")
    out_ds = gdaltest.mbtiles_drv.CreateCopy(
        "/vsimem/mbtiles_8.mbtiles",
        src_ds,
        options=["RESAMPLING=NEAREST", "BLOCKSIZE=512"],
    )
    out_ds = None
    src_ds = None

    expected_cs = [580, 8742, 54747]
    out_ds = gdal.Open("/vsimem/mbtiles_8.mbtiles")
    assert out_ds.RasterXSize == 512
    assert out_ds.RasterYSize == 512
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None
    assert out_ds.GetRasterBand(1).GetBlockSize() == [512, 512]
    out_ds = None

    gdal.Unlink("/vsimem/mbtiles_8.mbtiles")


###############################################################################
# Test we are robust to invalid bounds


@pytest.mark.require_driver("PNG")
def test_mbtiles_9():

    src_ds = gdal.Open("data/byte.tif")
    gdaltest.mbtiles_drv.CreateCopy(
        "/vsimem/mbtiles_9.mbtiles", src_ds, options=["RESAMPLING=NEAREST"]
    )
    src_ds = None
    ds = ogr.Open("SQLITE:/vsimem/mbtiles_9.mbtiles", update=1)
    ds.ExecuteSQL("UPDATE metadata SET value='invalid' WHERE name='bounds'")
    ds = None

    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/mbtiles_9.mbtiles")
    assert ds.RasterXSize == 256 and ds.RasterYSize == 256
    assert ds.GetGeoTransform()[0] == pytest.approx(-13110479.091473430395126, abs=1e-6)
    ds = None

    gdal.Unlink("/vsimem/mbtiles_9.mbtiles")


###############################################################################
# Test compaction of temporary database


@pytest.mark.require_driver("PNG")
def test_mbtiles_10():

    with gdal.config_option(
        "GPKG_FORCE_TEMPDB_COMPACTION", "YES"
    ), gdaltest.SetCacheMax(0):
        gdal.Translate(
            "/vsimem/mbtiles_10.mbtiles",
            "../gcore/data/byte.tif",
            options="-of MBTILES -outsize 512 512",
        )

    ds = gdal.Open("/vsimem/mbtiles_10.mbtiles")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == pytest.approx(30000, abs=200)
    ds = None

    gdal.Unlink("/vsimem/mbtiles_10.mbtiles")


###############################################################################
# Test opening a .mbtiles.sql file


@pytest.mark.require_driver("PNG")
def test_mbtiles_11():

    if gdaltest.mbtiles_drv.GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != "YES":
        pytest.skip()

    ds = gdal.Open("data/mbtiles/byte.mbtiles.sql")
    assert ds.GetRasterBand(1).Checksum() == 4118, "validation failed"


###############################################################################


def test_mbtiles_raster_open_in_vector_mode():

    with pytest.raises(Exception):
        ogr.Open("data/mbtiles/byte.mbtiles")


###############################################################################


@gdaltest.disable_exceptions()
def test_mbtiles_create():

    filename = "/vsimem/mbtiles_create.mbtiles"
    gdaltest.mbtiles_drv.Create(filename, 1, 1, 1)
    with gdal.quiet_errors():
        assert gdal.Open(filename) is None

    # Nominal case
    gdal.Unlink(filename)
    src_ds = gdal.Open("data/mbtiles/byte.mbtiles")
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    assert ds.GetMetadata() == {}
    assert ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo") is None
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    ds.SetProjection(src_ds.GetProjectionRef())

    # Cannot modify geotransform once set"
    with gdal.quiet_errors():
        ret = ds.SetGeoTransform(src_ds.GetGeoTransform())
    assert ret != 0
    ds = None

    ds = gdal.Open("data/mbtiles/byte.mbtiles")
    # SetGeoTransform() not supported on read-only dataset"
    with gdal.quiet_errors():
        ret = ds.SetGeoTransform(src_ds.GetGeoTransform())
    assert ret != 0
    # SetProjection() not supported on read-only dataset
    with gdal.quiet_errors():
        ret = ds.SetProjection(src_ds.GetProjectionRef())
    assert ret != 0
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Only EPSG:3857 supported on MBTiles dataset
    with gdal.quiet_errors():
        ret = ds.SetProjection('LOCAL_CS["foo"]')
    assert ret != 0
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Only north-up non rotated geotransform supported
    with gdal.quiet_errors():
        ret = ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    assert ret != 0
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Could not find an appropriate zoom level that matches raster pixel size
    with gdal.quiet_errors():
        ret = ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    assert ret != 0
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test read support of WEBP compressed dataset


@pytest.mark.require_driver("WEBP")
def test_mbtiles_webp_read():

    ds = gdal.Open("data/mbtiles/world_l1_webp.mbtiles")
    assert ds is not None

    assert ds.RasterCount == 4, "expected 4 bands"
    assert ds.RasterXSize == 512 and ds.RasterYSize == 510, "bad dimensions"
    assert ds.GetRasterBand(1).Checksum() == 37747
    assert ds.GetRasterBand(2).Checksum() == 54303
    assert ds.GetRasterBand(3).Checksum() == 13117
    assert ds.GetRasterBand(4).Checksum() == 58907

    gt = ds.GetGeoTransform()
    expected_gt = (
        -20037508.342789244,
        78271.516964020484,
        0.0,
        19971868.880408563,
        0.0,
        -78271.516964020484,
    )
    assert gt == pytest.approx(expected_gt, rel=1e-15), "bad gt"

    ds = None


###############################################################################
# Test write support of WEBP compressed dataset


@pytest.mark.require_driver("WEBP")
def test_mbtiles_webp_write():

    # Test options
    src_ds = gdal.Open("data/byte.tif")
    options = []
    options += ["TILE_FORMAT=WEBP"]
    options += ["QUALITY=50"]
    options += ["NAME=webp_mbtiles_name"]
    options += ["DESCRIPTION=webp_mbtiles_dsc"]
    options += ["TYPE=baselayer"]
    options += ["WRITE_BOUNDS=no"]
    gdaltest.mbtiles_drv.CreateCopy(
        "/vsimem/mbtiles_webp_write.mbtiles", src_ds, options=options
    )
    src_ds = None

    ds = gdal.Open("/vsimem/mbtiles_webp_write.mbtiles")
    got_cs = ds.GetRasterBand(1).Checksum()
    assert got_cs != 0
    got_md = ds.GetMetadata()
    expected_md = {
        "ZOOM_LEVEL": "11",
        "minzoom": "11",
        "maxzoom": "11",
        "format": "webp",
        "version": "1.3",
        "type": "baselayer",
        "name": "webp_mbtiles_name",
        "description": "webp_mbtiles_dsc",
    }
    assert got_md == expected_md
    ds = None

    gdal.Unlink("/vsimem/mbtiles_webp_write.mbtiles")


###############################################################################


def test_mbtiles_vector_field_type_from_values():

    ds = ogr.Open("data/mbtiles/field_type_from_values.mbtiles")
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("geo_x")).GetType() == ogr.OFTReal
    )
    assert (
        lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("geo_y")).GetType() == ogr.OFTReal
    )

#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WMS client support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import base64
import hashlib
import json
import os
from time import sleep

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_driver("WMS")

###############################################################################
# Open the WMS dataset


@pytest.fixture()
def gpwv3_wms():

    import xml.etree.ElementTree as ET

    wms_xml = "data/wms/pop_wms.xml"
    tree = ET.parse(wms_xml)
    srv = next(tree.iter("ServerUrl")).text

    wms_srv1_ok = gdaltest.gdalurlopen(srv, timeout=5) is not None

    if not wms_srv1_ok:
        pytest.skip(f"Could not read from {srv}")

    wms_ds = gdal.Open(wms_xml)

    if wms_ds is None:
        pytest.fail("open failed.")

    return wms_ds


###############################################################################
# Check various things about the configuration.


@pytest.mark.network
def test_wms_3(gpwv3_wms):

    assert (
        gpwv3_wms.RasterXSize == 36000
        and gpwv3_wms.RasterYSize == 14500
        and gpwv3_wms.RasterCount == 3
    ), "wrong size or bands"

    wkt = gpwv3_wms.GetProjectionRef()
    assert wkt[:14] == 'GEOGCS["WGS 84', "Got wrong SRS: " + wkt

    gt = gpwv3_wms.GetGeoTransform()
    assert (
        gt[0] == pytest.approx(-180, abs=0.00001)
        and gt[3] == pytest.approx(85, abs=0.00001)
        and gt[1] == pytest.approx(0.01, abs=0.00001)
        and gt[2] == pytest.approx(0, abs=0.00001)
        and gt[5] == pytest.approx(-0.01, abs=0.00001)
        and gt[4] == pytest.approx(0, abs=0.00001)
    ), "wrong geotransform"

    assert gpwv3_wms.GetRasterBand(1).GetOverviewCount() >= 1, "no overviews!"

    assert gpwv3_wms.GetRasterBand(1).DataType >= gdal.GDT_UInt8, "wrong band data type"


###############################################################################
# Check checksum for a small region.


@pytest.mark.network
def test_wms_4(gpwv3_wms):

    with gdal.config_option("CPL_ACCUM_ERROR_MSG", "ON"), gdaltest.error_handler():
        cs = gpwv3_wms.GetRasterBand(1).Checksum(0, 0, 100, 100)

    msg = gdal.GetLastErrorMsg()
    gdal.ErrorReset()

    if msg and "Service denied due to system overload" not in msg:
        pytest.fail(msg)

    assert cs == 57182, "Wrong checksum: " + str(cs)


###############################################################################
# Open the WMS service using XML as filename.


def test_wms_5():
    # We don't need to check if the remote service is online as we
    # don't need a connection for this test

    fn = '<GDAL_WMS><Service name="WMS"><Version>1.1.1</Version><ServerUrl>http://onearth.jpl.nasa.gov/wms.cgi?</ServerUrl><SRS>EPSG:4326</SRS><ImageFormat>image/jpeg</ImageFormat><Layers>modis,global_mosaic</Layers><Styles></Styles></Service><DataWindow><UpperLeftX>-180.0</UpperLeftX><UpperLeftY>90.0</UpperLeftY><LowerRightX>180.0</LowerRightX><LowerRightY>-90.0</LowerRightY><SizeX>2666666</SizeX><SizeY>1333333</SizeY></DataWindow><Projection>EPSG:4326</Projection><BandsCount>3</BandsCount></GDAL_WMS>'
    ds = gdal.Open(fn)

    assert ds is not None, "open failed."

    assert (
        ds.RasterXSize == 2666666 and ds.RasterYSize == 1333333 and ds.RasterCount == 3
    ), "wrong size or bands"

    ds = None


###############################################################################
# Test TileService


def test_wms_6():

    # We don't need to check if the remote service is online as we
    # don't need a connection for this test

    fn = '<GDAL_WMS><Service name="TileService"><Version>1</Version><ServerUrl>http://s0.tileservice.worldwindcentral.com/getTile?</ServerUrl><Dataset>za.johannesburg_2006_20cm</Dataset></Service><DataWindow><UpperLeftX>-180.0</UpperLeftX><UpperLeftY>90.0</UpperLeftY><LowerRightX>180.0</LowerRightX><LowerRightY>-90.0</LowerRightY><SizeX>268435456</SizeX><SizeY>134217728</SizeY><TileLevel>19</TileLevel></DataWindow><Projection>EPSG:4326</Projection><OverviewCount>16</OverviewCount><BlockSizeX>512</BlockSizeX><BlockSizeY>512</BlockSizeY><BandsCount>3</BandsCount></GDAL_WMS>'
    ds = gdal.Open(fn)

    assert ds is not None, "open failed."

    assert (
        ds.RasterXSize == 268435456
        and ds.RasterYSize == 134217728
        and ds.RasterCount == 3
    ), "wrong size or bands"

    ds = None


###############################################################################
# Test TMS


@pytest.fixture()
def metacarta_tms():

    srv = "http://tilecache.osgeo.org/wms-c/Basic.py"

    if gdaltest.gdalurlopen(srv) is None:
        pytest.skip(f"Could not read from {srv}")

    tms = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>http://tilecache.osgeo.org/wms-c/Basic.py</ServerUrl>
        <Layer>basic</Layer>
        <Format>png</Format>
    </Service>
    <DataWindow>
        <UpperLeftX>-180.0</UpperLeftX>
        <UpperLeftY>90.0</UpperLeftY>
        <LowerRightX>180.0</LowerRightX>
        <LowerRightY>-90.0</LowerRightY>
        <TileLevel>19</TileLevel>
        <TileCountX>2</TileCountX>
        <TileCountY>1</TileCountY>
    </DataWindow>
    <Projection>EPSG:4326</Projection>
    <BlockSizeX>256</BlockSizeX>
    <BlockSizeY>256</BlockSizeY>
    <BandsCount>3</BandsCount>
</GDAL_WMS>"""

    return gdal.Open(tms)


@pytest.mark.network
def test_wms_7(metacarta_tms):

    ds = metacarta_tms

    assert ds is not None, "open failed."

    assert (
        ds.RasterXSize == 268435456
        and ds.RasterYSize == 134217728
        and ds.RasterCount == 3
    ), "wrong size or bands"

    assert (
        ds.GetRasterBand(1).GetOverview(18).XSize == 512
        and ds.GetRasterBand(1).GetOverview(18).YSize == 256
    )

    ds.GetRasterBand(1).GetOverview(18).ReadRaster(0, 0, 512, 256)

    ds = None


###############################################################################
# Test TMS with cache


@pytest.mark.network
def test_wms_8(tmp_path):

    # server_url = 'http://tilecache.osgeo.org/wms-c/Basic.py'
    # wmstms_version = '/1.0.0/basic'
    # zero_tile = wmstms_version + '/0/0/0.png'
    # server_url_mask = server_url
    # ovr_upper_level = 18
    #     tms = """<GDAL_WMS>
    #     <Service name="TMS">
    #         <ServerUrl>%s</ServerUrl>
    #         <Layer>basic</Layer>
    #         <Format>png</Format>
    #     </Service>
    #     <DataWindow>
    #         <UpperLeftX>-180.0</UpperLeftX>
    #         <UpperLeftY>90.0</UpperLeftY>
    #         <LowerRightX>180.0</LowerRightX>
    #         <LowerRightY>-90.0</LowerRightY>
    #         <TileLevel>19</TileLevel>
    #         <TileCountX>2</TileCountX>
    #         <TileCountY>1</TileCountY>
    #     </DataWindow>
    #     <Projection>EPSG:4326</Projection>
    #     <BlockSizeX>256</BlockSizeX>
    #     <BlockSizeY>256</BlockSizeY>
    #     <BandsCount>3</BandsCount>
    #     <Cache><Path>./tmp/gdalwmscache</Path></Cache>
    # </GDAL_WMS>""" % server_url_mask

    #     tms_nocache = """<GDAL_WMS>
    #     <Service name="TMS">
    #         <ServerUrl>%s</ServerUrl>
    #         <Layer>basic</Layer>
    #         <Format>png</Format>
    #     </Service>
    #     <DataWindow>
    #         <UpperLeftX>-180.0</UpperLeftX>
    #         <UpperLeftY>90.0</UpperLeftY>
    #         <LowerRightX>180.0</LowerRightX>
    #         <LowerRightY>-90.0</LowerRightY>
    #         <TileLevel>19</TileLevel>
    #         <TileCountX>2</TileCountX>
    #         <TileCountY>1</TileCountY>
    #     </DataWindow>
    #     <Projection>EPSG:4326</Projection>
    #     <BlockSizeX>256</BlockSizeX>
    #     <BlockSizeY>256</BlockSizeY>
    #     <BandsCount>3</BandsCount>
    #     <Cache/> <!-- this is needed for GDAL_DEFAULT_WMS_CACHE_PATH to be triggered -->
    # </GDAL_WMS>""" % server_url_mask

    server_url = "http://tile.openstreetmap.org"
    cache_dir = tmp_path / "gdalwmscache"
    wmstms_version = ""
    zero_tile = "/0/0/0.png"
    server_url_mask = server_url + "/${z}/${x}/${y}.png"
    ovr_upper_level = 16
    tms = f"""<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>{server_url_mask}</ServerUrl>
    </Service>
    <DataWindow>
        <UpperLeftX>-20037508.34</UpperLeftX>
        <UpperLeftY>20037508.34</UpperLeftY>
        <LowerRightX>20037508.34</LowerRightX>
        <LowerRightY>-20037508.34</LowerRightY>
        <TileLevel>18</TileLevel>
        <TileCountX>1</TileCountX>
        <TileCountY>1</TileCountY>
        <YOrigin>top</YOrigin>
    </DataWindow>
    <Projection>EPSG:3857</Projection>
    <BlockSizeX>256</BlockSizeX>
    <BlockSizeY>256</BlockSizeY>
    <BandsCount>3</BandsCount>
    <Cache><Path>{cache_dir}</Path></Cache>
</GDAL_WMS>"""

    tms_nocache = f"""<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>{server_url_mask}</ServerUrl>
    </Service>
    <DataWindow>
        <UpperLeftX>-20037508.34</UpperLeftX>
        <UpperLeftY>20037508.34</UpperLeftY>
        <LowerRightX>20037508.34</LowerRightX>
        <LowerRightY>-20037508.34</LowerRightY>
        <TileLevel>18</TileLevel>
        <TileCountX>1</TileCountX>
        <TileCountY>1</TileCountY>
        <YOrigin>top</YOrigin>
    </DataWindow>
    <Projection>EPSG:3857</Projection>
    <BlockSizeX>256</BlockSizeX>
    <BlockSizeY>256</BlockSizeY>
    <BandsCount>3</BandsCount>
    <Cache/> <!-- this is needed for GDAL_DEFAULT_WMS_CACHE_PATH to be triggered -->
</GDAL_WMS>"""

    ds = gdal.Open(tms)

    assert ds is not None, "open failed."

    # Check cache metadata item
    cache_path = ds.GetMetadataItem("CACHE_PATH")
    assert cache_path, "did not get expected cache path metadata item"

    cache_subfolder = hashlib.md5(server_url_mask.encode("utf-8")).hexdigest()

    gdal.ErrorReset()
    data = ds.GetRasterBand(1).GetOverview(ovr_upper_level).ReadRaster(0, 0, 512, 512)
    if gdal.GetLastErrorMsg() != "":
        if gdaltest.gdalurlopen(server_url + zero_tile) is None:
            pytest.skip(f"Could not read from {server_url + zero_tile}")

    ds = None

    file1 = hashlib.md5(
        (server_url + wmstms_version + "/1/0/0.png").encode("utf-8")
    ).hexdigest()
    file2 = hashlib.md5(
        (server_url + wmstms_version + "/1/1/0.png").encode("utf-8")
    ).hexdigest()
    file3 = hashlib.md5(
        (server_url + wmstms_version + "/1/0/1.png").encode("utf-8")
    ).hexdigest()
    file4 = hashlib.md5(
        (server_url + wmstms_version + "/1/1/1.png").encode("utf-8")
    ).hexdigest()

    expected_files = [
        cache_dir / cache_subfolder / file1[0] / file1[1] / file1,
        cache_dir / cache_subfolder / file2[0] / file2[1] / file2,
        cache_dir / cache_subfolder / file3[0] / file3[1] / file3,
        cache_dir / cache_subfolder / file4[0] / file4[1] / file4,
    ]
    for expected_file in expected_files:
        assert expected_file.exists()

    # Now, we should read from the cache
    ds = gdal.Open(tms)
    cached_data = (
        ds.GetRasterBand(1).GetOverview(ovr_upper_level).ReadRaster(0, 0, 512, 512)
    )
    ds = None

    assert data == cached_data, "data != cached_data"

    # Replace the cache with fake data
    for expected_file in expected_files:

        ds = gdal.GetDriverByName("GTiff").Create(str(expected_file), 256, 256, 4)
        ds.GetRasterBand(1).Fill(0)
        ds.GetRasterBand(2).Fill(0)
        ds.GetRasterBand(3).Fill(0)
        ds.GetRasterBand(4).Fill(255)
        ds = None

    # Read again from the cache, and check that it is actually used
    ds = gdal.Open(tms)
    cs = ds.GetRasterBand(1).GetOverview(ovr_upper_level).Checksum()
    ds = None
    assert cs == 0, "cs != 0"

    # Test with GDAL_DEFAULT_WMS_CACHE_PATH
    # Now, we should read from the cache
    with gdaltest.config_option("GDAL_DEFAULT_WMS_CACHE_PATH", str(cache_dir)):

        ds = gdal.Open(tms_nocache)
        cs = ds.GetRasterBand(1).GetOverview(ovr_upper_level).Checksum()
        ds = None
        assert cs == 0, "cs != 0"

        # Test with GDAL_ENABLE_WMS_CACHE=NO
        # Now, we should not read from the cache anymore
        with gdaltest.config_option("GDAL_ENABLE_WMS_CACHE", "NO"):
            ds = gdal.Open(tms_nocache)
            cs = ds.GetRasterBand(1).GetOverview(ovr_upper_level).Checksum()
            ds = None
            assert cs != 0, "cs == 0"

    # Check maxsize and expired tags
    tms_expires = f"""<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>{server_url_mask}</ServerUrl>
    </Service>
    <DataWindow>
        <UpperLeftX>-20037508.34</UpperLeftX>
        <UpperLeftY>20037508.34</UpperLeftY>
        <LowerRightX>20037508.34</LowerRightX>
        <LowerRightY>-20037508.34</LowerRightY>
        <TileLevel>18</TileLevel>
        <TileCountX>1</TileCountX>
        <TileCountY>1</TileCountY>
        <YOrigin>top</YOrigin>
    </DataWindow>
    <Projection>EPSG:3857</Projection>
    <BlockSizeX>256</BlockSizeX>
    <BlockSizeY>256</BlockSizeY>
    <BandsCount>3</BandsCount>
    <Cache><Path>{cache_dir}</Path><Expires>1</Expires></Cache>
</GDAL_WMS>"""

    mod_time = 0
    for expected_file in expected_files:
        tm = os.path.getmtime(expected_file)
        if tm > mod_time:
            mod_time = tm

    ds = gdal.Open(tms_expires)
    sleep(1.05)
    data = ds.GetRasterBand(1).GetOverview(ovr_upper_level).ReadRaster(0, 0, 512, 512)

    # tiles should be overwritten by new ones
    for expected_file in expected_files:
        assert os.path.getmtime(expected_file) > mod_time


###############################################################################
# Test OnEarth Tiled WMS minidriver


# Permanently down
def wms_9():

    tms = """<GDAL_WMS>
    <Service name="TiledWMS">
        <ServerUrl>http://onearth.jpl.nasa.gov/wms.cgi?</ServerUrl>
        <TiledGroupName>Global SRTM Elevation</TiledGroupName>
    </Service>
</GDAL_WMS>
"""
    ds = gdal.Open(tms)

    if ds is None:
        srv = "http://onearth.jpl.nasa.gov/wms.cgi?"
        if gdaltest.gdalurlopen(srv) is None:
            pytest.skip()
        pytest.fail("open failed.")

    expected_cs = 5478
    cs = ds.GetRasterBand(1).GetOverview(9).Checksum()

    assert cs == expected_cs, "Did not get expected SRTM checksum."

    ds = None


###############################################################################
# Test getting subdatasets from GetCapabilities


def wms_10():

    if not gdaltest.wms_srv1_ok:
        pytest.skip()

    name = "WMS:http://sedac.ciesin.columbia.edu/mapserver/map/GPWv3?"
    ds = gdal.Open(name)
    assert ds is not None, "open of %s failed." % name

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, "did not get expected subdataset count"

    ds = None

    name = subdatasets["SUBDATASET_1_NAME"]
    ds = gdal.Open(name)
    assert ds is not None, "open of %s failed." % name

    ds = None


###############################################################################
# Test getting subdatasets from GetTileService


# Permanently down
def wms_11():

    gdaltest.skip_on_travis()

    srv = "http://onearth.jpl.nasa.gov/wms.cgi"
    if gdaltest.gdalurlopen(srv) is None:
        pytest.skip()

    name = "WMS:http://onearth.jpl.nasa.gov/wms.cgi?request=GetTileService"
    ds = gdal.Open(name)
    assert ds is not None, "open of %s failed." % name

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, "did not get expected subdataset count"

    ds = None

    name = subdatasets["SUBDATASET_1_NAME"]
    ds = gdal.Open(name)
    assert ds is not None, "open of %s failed." % name

    ds = None


###############################################################################
# Test getting subdatasets from a TMS server


@pytest.mark.network
@pytest.mark.usefixtures("metacarta_tms")
def test_wms_12():

    name = "http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/"
    ds = gdal.Open(name)
    if ds is None:
        if (
            gdaltest.gdalurlopen(
                "http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/basic/0/0/0.png"
            )
            is None
        ):
            pytest.skip()
        pytest.fail("open of %s failed." % name)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, "did not get expected subdataset count"

    ds = None

    for i in range(len(subdatasets) // 2):
        desc = subdatasets["SUBDATASET_%d_DESC" % (i + 1)]
        if desc == "basic":
            name = subdatasets["SUBDATASET_%d_NAME" % (i + 1)]
            ds = gdal.Open(name)
            if ds is None:
                if (
                    gdaltest.gdalurlopen(
                        "http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/basic/0/0/0.png"
                    )
                    is None
                ):
                    pytest.skip()
                pytest.fail("open of %s failed." % name)
            ds = None


###############################################################################
# Test reading WMS through VRT (test effect of r21866)


@pytest.mark.network
@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@gdaltest.disable_exceptions()
def test_wms_13():

    with gdal.config_option("GDAL_HTTP_TIMEOUT", "5"):

        ds = gdal.Open("data/wms/DNEC_250K.vrt")
        if ds.ReadRaster(0, 0, 1024, 682) is None:
            srv = "http://wms.geobase.ca/wms-bin/cubeserv.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetCapabilities"
            if gdaltest.gdalurlopen(srv) is None:
                pytest.skip(f"Could not read from {srv}")
            pytest.fail()
        ds = None


###############################################################################
# Test reading Virtual Earth layer


def test_wms_14():

    ds = gdal.Open("""<GDAL_WMS>
  <Service name="VirtualEarth">
    <ServerUrl>http://a${server_num}.ortho.tiles.virtualearth.net/tiles/a${quadkey}.jpeg?g=90</ServerUrl>
  </Service>
</GDAL_WMS>""")
    if ds is None:
        return "fail"

    assert (
        ds.RasterXSize == 536870912
        and ds.RasterYSize == 536870912
        and ds.RasterCount == 3
    ), "wrong size or bands"

    wkt = ds.GetProjectionRef()
    assert (
        'EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs"]'
        in wkt
    ), ("Got wrong SRS: " + wkt)

    gt = ds.GetGeoTransform()
    assert (
        abs(
            gt[0] - -20037508.34278924,
        )
        <= 0.00001
        and abs(
            gt[3] - 20037508.34278924,
        )
        <= 0.00001
        and gt[1] == pytest.approx(0.07464553543474242, abs=0.00001)
        and gt[2] == pytest.approx(0, abs=0.00001)
        and abs(
            gt[5] - -0.07464553543474242,
        )
        <= 0.00001
        and gt[4] == pytest.approx(0, abs=0.00001)
    ), "wrong geotransform"

    assert ds.GetRasterBand(1).GetOverviewCount() == 20, "bad overview count"

    block_xsize, block_ysize = ds.GetRasterBand(1).GetBlockSize()
    if block_xsize != 256 or block_ysize != 256:
        print("(%d, %d)" % (block_xsize, block_ysize))
        pytest.fail("bad block size")


###############################################################################
# Test reading ArcGIS MapServer JSon definition and CreateCopy()


@pytest.mark.network
def test_wms_15():

    srv = "http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer?f=json&pretty=true"
    try:
        src_ds = gdal.Open(srv)
    except Exception:
        if gdaltest.gdalurlopen(srv) is None:
            pytest.skip(f"{srv} not available")
        pytest.fail()

    ds = gdal.GetDriverByName("WMS").CreateCopy("/vsimem/wms.xml", src_ds)
    src_ds = None
    assert ds, "failed to copy"

    assert (
        ds.RasterXSize == 1073741824
        and ds.RasterYSize == 1073741824
        and ds.RasterCount == 3
    ), "wrong size or bands"

    wkt = ds.GetProjectionRef()
    assert wkt.startswith('PROJCS["WGS 84 / Pseudo-Mercator"'), "Got wrong SRS: " + wkt

    corner = 20037508.342787001
    res = 0.037322767717361482
    assert ds.GetGeoTransform() == pytest.approx(
        [-corner, res, 0, corner, 0, -res], abs=0.00001
    ), "wrong geotransform"

    assert ds.GetRasterBand(1).GetOverviewCount() == 22, "bad overview count"
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]

    ds = None
    gdal.Unlink("/vsimem/wms.xml")


###############################################################################
# Test getting subdatasets from WMS-C Capabilities


# server often returns a 504 after ages; this test can take minutes
@pytest.mark.network
@gdaltest.disable_exceptions()
@pytest.mark.slow()
def test_wms_16():

    name = "WMS:http://demo.opengeo.org/geoserver/gwc/service/wms?tiled=TRUE"
    ds = gdal.Open(name)
    if ds is None:
        srv = "http://demo.opengeo.org/geoserver/gwc/service/wms?"
        if gdaltest.gdalurlopen(srv) is None:
            pytest.skip(f"Cannot read from {srv}")
        pytest.fail("open of %s failed." % name)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, "did not get expected subdataset count"

    ds = None

    name = None
    for key in subdatasets:
        if key[-5:] == "_NAME" and subdatasets[key].find("bugsites") != -1:
            name = subdatasets[key]
            break
    assert name is not None

    name = "http://demo.opengeo.org/geoserver/wms?SERVICE=WMS&request=GetMap&version=1.1.1&layers=og:bugsites&styles=&srs=EPSG:26713&bbox=599351.50000000,4914096.00000000,608471.00000000,4920512.00000000"
    ds = gdal.Open(name)
    assert ds is not None, "open of %s failed." % name

    # check that the default bbox works for srs != EPSG:4326
    name = "http://demo.opengeo.org/geoserver/wms?SERVICE=WMS&request=GetMap&version=1.1.1&layers=og:bugsites&styles=&srs=EPSG:26713"
    ds = gdal.Open(name)
    assert ds is not None, "open of %s failed." % name

    # Matches feature of "WFS:http://demo.opengeo.org/geoserver/wfs?SRSNAME=EPSG:900913" og:bugsites
    # OGRFeature(og:bugsites):68846
    #   gml_id (String) = bugsites.68846
    #   cat (Integer) = 86
    #   str1 (String) = Beetle site
    #   POINT (-11547069.564865021035075 5528605.849725087173283)

    pixel = "GeoPixel_601228_4917635"
    val = ds.GetRasterBand(1).GetMetadataItem(pixel, "LocationInfo")

    # Some bug in GeoServer ?
    if (
        val is not None
        and "java.lang.NoSuchMethodError: org.geoserver.wms.WMS.pixelToWorld" in val
    ):
        pytest.skip(val)

    if val is not None and ("Gateway Time-out" in val or "HTTP error code : 5" in val):
        pytest.skip()

    if val is None or val.find("<og:cat>86</og:cat>") == -1:
        if (
            "java.lang.NullPointerException" in val
            or "504 Gateway Time-out" in val
            or "java.lang.OutOfMemoryError" in val
        ):
            pytest.skip(val)

        print(val)
        pytest.fail("expected a value")

    # Ask again. Should be cached
    val_again = ds.GetRasterBand(1).GetMetadataItem(pixel, "LocationInfo")
    assert val_again == val, "expected a value"

    # Ask another band. Should be cached
    val2 = ds.GetRasterBand(2).GetMetadataItem(pixel, "LocationInfo")
    assert val2 == val, "expected a value"

    # Ask an overview band
    val2 = ds.GetRasterBand(1).GetOverview(0).GetMetadataItem(pixel, "LocationInfo")
    if val2 != val:
        if (
            "java.lang.NullPointerException" in val2
            or "504 Gateway Time-out" in val2
            or "java.lang.OutOfMemoryError" in val2
        ):
            pytest.skip(val2)

        print(val2)
        pytest.fail("expected a value")

    ds = None


###############################################################################
# Test a TiledWMS dataset with a color table (#4613)


# Permanently down
def wms_17():

    srv = "http://onmoon.lmmp.nasa.gov/sites/wms.cgi?"
    if gdaltest.gdalurlopen(srv) is None:
        pytest.skip()

    name = '<GDAL_WMS><Service name="TiledWMS"><ServerUrl>http://onmoon.lmmp.nasa.gov/sites/wms.cgi?</ServerUrl><TiledGroupName>King Crater DEM Color Confidence, LMMP</TiledGroupName></Service></GDAL_WMS>'
    ds = gdal.Open(name)
    assert ds is not None, "open of %s failed." % name

    band = ds.GetRasterBand(1)
    assert band.GetColorTable() is not None

    ds = None


###############################################################################
# Test an ArcGIS Server


@pytest.mark.network
def test_wms_18():

    # We don't need to check if the remote service is online as we
    # don't need a connection for this test.
    srv = "http://sampleserver6.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer"

    fn = f'<GDAL_WMS><Service name="AGS"><ServerUrl>{srv}</ServerUrl><BBoxOrder>xyXY</BBoxOrder><SRS>EPSG:3857</SRS></Service><DataWindow><UpperLeftX>-20037508.34</UpperLeftX><UpperLeftY>20037508.34</UpperLeftY><LowerRightX>20037508.34</LowerRightX><LowerRightY>-20037508.34</LowerRightY><SizeX>512</SizeX><SizeY>512</SizeY></DataWindow></GDAL_WMS>'

    ds = gdal.Open(fn)

    assert ds is not None, "open failed."

    assert (
        ds.RasterXSize == 512 and ds.RasterYSize == 512 and ds.RasterCount == 3
    ), "wrong size or bands"

    # todo: add locationinfo test

    # add getting image test
    if not gdaltest.gdalurlopen(srv):
        ds = None
        pytest.skip(f"Could not read from {srv}")

    expected_cs = 17845
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs, "Did not get expected checksum."
    ds = None

    # Alternative url with additional parameters
    fn = f'<GDAL_WMS><Service name="AGS"><ServerUrl>{srv}/export?dpi=96&amp;layerdefs=&amp;layerTimeOptions=&amp;dynamicLayers=&amp;</ServerUrl><BBoxOrder>xyXY</BBoxOrder><SRS>EPSG:3857</SRS></Service><DataWindow><UpperLeftX>-20037508.34</UpperLeftX><UpperLeftY>20037508.34</UpperLeftY><LowerRightX>20037508.34</LowerRightX><LowerRightY>-20037508.34</LowerRightY><SizeX>512</SizeX><SizeY>512</SizeY></DataWindow></GDAL_WMS>'

    ds = gdal.Open(fn)

    assert ds is not None, "open failed."
    assert (
        ds.RasterXSize == 512 and ds.RasterYSize == 512 and ds.RasterCount == 3
    ), "wrong size or bands"
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs, "Did not get expected checksum."
    ds = None


###############################################################################
# Test a IIP server


@pytest.mark.network
def test_wms_19():

    if (
        gdaltest.gdalurlopen(
            "http://merovingio.c2rmf.cnrs.fr/fcgi-bin/iipsrv.fcgi?FIF=globe.256x256.tif&obj=Basic-Info",
            timeout=5,
        )
        is None
    ):
        pytest.skip("Server not reachable")

    ds = gdal.Open(
        "IIP:http://merovingio.c2rmf.cnrs.fr/fcgi-bin/iipsrv.fcgi?FIF=globe.256x256.tif"
    )
    assert ds

    assert (
        ds.RasterXSize == 86400 and ds.RasterYSize == 43200 and ds.RasterCount == 3
    ), "wrong size or bands"

    # Expected checksum seems to change over time. Hum...
    cs = (
        ds.GetRasterBand(1)
        .GetOverview(ds.GetRasterBand(1).GetOverviewCount() - 1)
        .Checksum()
    )
    assert cs != 0, "Did not get expected checksum."

    ds = None


###############################################################################
# Test reading data via MRF/LERC


@pytest.mark.network
@pytest.mark.require_creation_option("MRF", "LERC")
def test_wms_data_via_mrf():

    srv = "http://astro.arcgis.com"
    if gdaltest.gdalurlopen(srv, timeout=5) is None:
        pytest.skip(reason=f"{srv} is down")

    url = (
        srv + "/arcgis/rest/services/OnMars/HiRISE_DEM/ImageServer/tile/${z}/${y}/${x}"
    )
    dstemplate = """<GDAL_WMS>
<Service name="TMS" ServerUrl="{url}"/>
<DataWindow SizeX="513" SizeY="513"/>
<BandsCount>1</BandsCount><BlockSizeX>513</BlockSizeX><BlockSizeY>513</BlockSizeY>
<DataType>{dt}</DataType><DataValues NoData="{ndv}"/>
</GDAL_WMS>"""

    # This is a LERC1 format tile service, DEM in floating point, it can be read as any type
    # The returned no data value can also be set on read, which affects the checksum
    testlist = [
        ("Byte", 0, 10253),  # Same as the default type, NDV not defined
        ("Float32", 0, 56058),  # float, default NDV
        ("Float32", 32768.32, 33927),  # float, Forced NDV
    ]

    for dt, ndv, expected_cs in testlist:
        ds = gdal.Open(dstemplate.format(url=url, dt=dt, ndv=ndv))
        assert (
            ds.GetRasterBand(1).Checksum() == expected_cs
        ), "datatype {} and ndv {}".format(dt, ndv)
        ds = None


def test_twms_wmsmetadriver():
    ds = gdal.Open("data/wms/gibs_twms.xml")
    assert ds is not None, "Open tiledWMS failed"
    gdaltest.subdatasets = ds.GetMetadata("SUBDATASETS")
    assert gdaltest.subdatasets, "Expected subdatasets"
    ds = None


# This test requires the GIBS server to be available
@pytest.mark.network
@gdaltest.disable_exceptions()
def test_twms_GIBS():

    baseURL = "https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?"

    with gdal.Open("data/wms/gibs_twms.xml") as ds:
        subdatasets = ds.GetMetadata("SUBDATASETS")

    # Connects to the server
    ds = gdal.Open(subdatasets["SUBDATASET_1_NAME"])
    if ds is None and gdaltest.gdalurlopen(baseURL + "request=GetTileService"):
        pytest.xfail(
            "May fail because of SSL issue. See https://github.com/OSGeo/gdal/issues/3511#issuecomment-840718083"
        )
    ds = None

    # Connects to the server
    options = ["Change=time:2021-02-10"]
    ds = gdal.OpenEx(subdatasets["SUBDATASET_1_NAME"], open_options=options)
    if ds is None and gdaltest.gdalurlopen(baseURL + "request=GetTileService"):
        pytest.xfail(
            "May fail because of SSL issue. See https://github.com/OSGeo/gdal/issues/3511#issuecomment-840718083"
        )
    ds = None


def test_twms_inline_configuration():

    baseURL = "https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?"

    # Try inline base64 configuration
    wms_base64gts = base64.b64encode(open("data/wms/gibs_twms.xml").read().encode())
    twms_string = '<GDAL_WMS><Service name="TiledWMS" ServerUrl="{}"><Configuration encoding="base64">{}</Configuration></Service></GDAL_WMS>'.format(
        baseURL, wms_base64gts.decode()
    )
    gdal.FileFromMemBuffer("/vsimem/ttms.xml", twms_string)

    # Open fails without a TiledGroupName
    with gdaltest.disable_exceptions():
        ds = gdal.Open("/vsimem/ttms.xml")
    assert ds is None, "Expected failure to open"

    tiled_group_name = "MODIS Aqua CorrectedReflectance TrueColor tileset"
    date = "2021-02-10"
    options = [
        "TiledGroupName={}".format(tiled_group_name),
        "Change=time:{}".format(date),
    ]
    ds = gdal.OpenEx("/vsimem/ttms.xml", open_options=options)
    assert ds is not None, "Open twms with open options failed"
    metadata = ds.GetMetadata("")
    assert metadata["Change"] == "${time}=2021-02-10", "Change parameter not captured"
    assert metadata["TiledGroupName"] == tiled_group_name, "TIledGroupName not captured"
    ds = None


###############################################################################
# Test gdal subdataset informational functions


@pytest.mark.parametrize(
    "filename,subdataset_component",
    (
        (
            "WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES&SRS=EPSG:4326&BBOX=-180,-90,180,90",
            "LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES",
        ),
        (
            "WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&SRS=EPSG:4326&BBOX=-180,-90,180,90&LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES",
            "LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES",
        ),
        (
            "WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&SRS=EPSG:4326&BBOX=-180,-90,180,90",
            "LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES",
        ),
        ("WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?", ""),
        ("", ""),
    ),
)
def test_gdal_subdataset_get_filename(filename, subdataset_component):

    info = gdal.GetSubdatasetInfo(filename)
    if "LAYERS=" not in filename:
        assert info is None
    else:
        assert (
            info.GetPathComponent()
            == filename.replace(subdataset_component, "").replace("&&", "&")[4:]
        )
        assert info.GetSubdatasetComponent() == subdataset_component


@pytest.mark.parametrize(
    "filename",
    (
        (
            "WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES&SRS=EPSG:4326&BBOX=-180,-90,180,90"
        ),
        (
            "WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&SRS=EPSG:4326&BBOX=-180,-90,180,90&LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES"
        ),
        (
            "WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&SRS=EPSG:4326&BBOX=-180,-90,180,90"
        ),
        ("WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?"),
        (""),
    ),
)
def test_gdal_subdataset_modify_filename(filename):

    info = gdal.GetSubdatasetInfo(filename)
    if "LAYERS=" not in filename:
        assert info is None
    else:
        assert (
            info.ModifyPathComponent(
                "https://xxxx/?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap"
            )
            == "WMS:https://xxxx/?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&LAYERS=MODIS_Aqua_L3_Land_Surface_Temp_Monthly_CMG_Night_TES"
        )


def test_wms_cache_path():

    with gdaltest.config_option("GDAL_DEFAULT_WMS_CACHE_PATH", "/vsimem/foo"):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH")
            == "/vsimem/foo/b37af3c29458379e6fdf4ed73300f54e"
        )

    with gdaltest.config_options(
        {"GDAL_DEFAULT_WMS_CACHE_PATH": "", "XDG_CACHE_HOME": "/vsimem/foo"}
    ):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH")
            == "/vsimem/foo/gdalwmscache/b37af3c29458379e6fdf4ed73300f54e"
        )

    with gdaltest.config_options(
        {
            "GDAL_DEFAULT_WMS_CACHE_PATH": "",
            "XDG_CACHE_HOME": "",
            "HOME": "/vsimem/foo",
            "USERPROFILE": "/vsimem/foo",
        }
    ):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH")
            == "/vsimem/foo/.cache/gdalwmscache/b37af3c29458379e6fdf4ed73300f54e"
        )

    with gdaltest.config_options(
        {
            "GDAL_DEFAULT_WMS_CACHE_PATH": "",
            "XDG_CACHE_HOME": "",
            "HOME": "",
            "USERPROFILE": "",
            "CPL_TMPDIR": "/vsimem/foo",
            "USERNAME": "",
            "USER": "user",
        }
    ):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH")
            == "/vsimem/foo/gdalwmscache_user/b37af3c29458379e6fdf4ed73300f54e"
        )

    with gdaltest.config_options(
        {
            "GDAL_DEFAULT_WMS_CACHE_PATH": "",
            "XDG_CACHE_HOME": "",
            "HOME": "",
            "USERPROFILE": "",
            "CPL_TMPDIR": "/vsimem/foo",
            "USER": "",
            "USERNAME": "user",
        }
    ):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH")
            == "/vsimem/foo/gdalwmscache_user/b37af3c29458379e6fdf4ed73300f54e"
        )

    with gdaltest.config_options(
        {
            "GDAL_DEFAULT_WMS_CACHE_PATH": "",
            "XDG_CACHE_HOME": "",
            "HOME": "",
            "USERPROFILE": "",
            "CPL_TMPDIR": "",
            "TMPDIR": "/vsimem/foo",
            "USERNAME": "",
            "USER": "user",
        }
    ):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH")
            == "/vsimem/foo/gdalwmscache_user/b37af3c29458379e6fdf4ed73300f54e"
        )

    with gdaltest.config_options(
        {
            "GDAL_DEFAULT_WMS_CACHE_PATH": "",
            "XDG_CACHE_HOME": "",
            "HOME": "",
            "USERPROFILE": "",
            "CPL_TMPDIR": "",
            "TMPDIR": "",
            "TEMP": "/vsimem/foo",
            "USERNAME": "",
            "USER": "user",
        }
    ):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH")
            == "/vsimem/foo/gdalwmscache_user/b37af3c29458379e6fdf4ed73300f54e"
        )

    with gdaltest.config_options(
        {
            "GDAL_DEFAULT_WMS_CACHE_PATH": "",
            "XDG_CACHE_HOME": "",
            "HOME": "",
            "USERPROFILE": "",
            "CPL_TMPDIR": "",
            "TMPDIR": "",
            "TEMP": "",
            "USERNAME": "",
            "USER": "",
        }
    ):
        ds = gdal.Open("data/wms/frmt_wms_openstreetmap_tms.xml")
        assert (
            ds.GetMetadataItem("CACHE_PATH").replace("\\", "/")
            == "./gdalwmscache_b37af3c29458379e6fdf4ed73300f54e/b37af3c29458379e6fdf4ed73300f54e"
        )

    with pytest.raises(Exception):
        gdal.Open("<GDAL_WMS><Service/><Cache/></GDAL_WMS>")


# Launch a single webserver in a module-scoped fixture.
@pytest.fixture(scope="module")
def webserver_launch():

    process, port = webserver.launch(handler=webserver.DispatcherHttpHandler)

    yield process, port

    webserver.server_stop(process, port)


@pytest.fixture(scope="function")
def webserver_port(webserver_launch):

    webserver_process, webserver_port = webserver_launch

    if webserver_port == 0:
        pytest.skip()
    yield webserver_port


@pytest.mark.require_curl
@gdaltest.enable_exceptions()
def test_wms_force_opening_url(tmp_vsimem, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetCapabilities",
        200,
        {"Content-type": "application/xml"},
        open("data/wms/demo_mapserver_org.xml", "rb").read(),
    )
    with webserver.install_http_handler(handler):
        gdal.OpenEx(f"http://localhost:{webserver_port}", allowed_drivers=["WMS"])


@pytest.mark.network
@pytest.mark.require_curl
@pytest.mark.require_driver("JPEG")
@gdaltest.enable_exceptions()
def test_wms_iiif_online(tmp_vsimem):

    url = "https://asge.jarvis.memooria.org/images/iiif/db/6431901e-7474-4c51-aa57-41dfddf13604"

    with gdaltest.config_option("GDAL_DEFAULT_WMS_CACHE_PATH", str(tmp_vsimem)):
        try:
            ds = gdal.Open(f"IIIF:{url}")
        except Exception:
            if gdaltest.gdalurlopen(f"{url}/info.json", timeout=5) is None:
                pytest.skip(f"Could not read from {url}")

        assert ds.RasterXSize == 7236
        assert ds.RasterYSize == 5238
        assert ds.GetRasterBand(1).GetOverviewCount() == 6
        ds.ReadRaster(0, 0, 256, 256)
        ds.GetRasterBand(1).GetOverview(5).ReadRaster()


@pytest.mark.require_curl
@pytest.mark.require_driver("JPEG")
@gdaltest.enable_exceptions()
def test_wms_iiif_fake_nominal(tmp_vsimem, webserver_port):

    with gdaltest.config_option("GDAL_DEFAULT_WMS_CACHE_PATH", str(tmp_vsimem)):

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/my_image/info.json",
            200,
            {"Content-type": "application/json"},
            json.dumps(
                {
                    "@context": "http://iiif.io/api/image/3/context.json",
                    "protocol": "http://iiif.io/api/image",
                    "width": 7236,
                    "height": 5238,
                    "sizes": [
                        {"width": 226, "height": 163},
                        {"width": 452, "height": 327},
                        {"width": 904, "height": 654},
                        {"width": 1809, "height": 1309},
                        {"width": 3618, "height": 2619},
                    ],
                    "tiles": [
                        {
                            "width": 256,
                            "height": 256,
                            "scaleFactors": [1, 2, 4, 8, 16, 32],
                        }
                    ],
                    "id": f"http://localhost:{webserver_port}/my_image",
                    "type": "ImageService3",
                    "profile": "level1",
                    "maxWidth": 8000,
                    "maxHeight": 8000,
                    "service": [
                        {
                            "@context": "http://iiif.io/api/annex/services/physdim/1/context.json",
                            "profile": "http://iiif.io/api/annex/services/physdim",
                            "physicalScale": 0.1,
                            "physicalUnits": "cm",
                        }
                    ],
                }
            ),
        )

        src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 3)
        src_ds.GetRasterBand(1).Fill(127)
        src_ds.GetRasterBand(2).Fill(127)
        src_ds.GetRasterBand(3).Fill(127)
        gdal.Translate(tmp_vsimem / "tmp.jpg", src_ds)
        with gdal.VSIFile(tmp_vsimem / "tmp.jpg", "rb") as f:
            data = f.read()
        handler.add(
            "GET",
            "/my_image/1024,2048,256,256/256,256/0/default.jpg",
            200,
            {"Content-type": "image/jpeg"},
            data,
        )

        src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 3)
        src_ds.GetRasterBand(1).Fill(63)
        src_ds.GetRasterBand(2).Fill(63)
        src_ds.GetRasterBand(3).Fill(63)
        gdal.Translate(tmp_vsimem / "tmp.jpg", src_ds)
        with gdal.VSIFile(tmp_vsimem / "tmp.jpg", "rb") as f:
            data = f.read()
        handler.add(
            "GET",
            "/my_image/1024,2048,512,512/256,256/0/default.jpg",
            200,
            {"Content-type": "image/jpeg"},
            data,
        )

        with webserver.install_http_handler(handler):
            ds = gdal.Open(f"IIIF:http://localhost:{webserver_port}/my_image")

            assert ds.RasterXSize == 7236
            assert ds.RasterYSize == 5238
            assert ds.GetRasterBand(1).GetOverviewCount() == 6

            assert ds.ReadRaster(1024, 2048, 256, 256) == b"\x7f" * (256 * 256 * 3)

            assert ds.ReadRaster(
                1024, 2048, 256, 256, buf_xsize=128, buf_ysize=128
            ) == b"\x3f" * (128 * 128 * 3)


@pytest.mark.require_curl
@gdaltest.enable_exceptions()
def test_wms_iiif_fake_errors(tmp_vsimem, webserver_port):

    with gdaltest.config_option("GDAL_DEFAULT_WMS_CACHE_PATH", str(tmp_vsimem)):

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/my_image/info.json",
            200,
            {"Content-type": "application/json"},
            json.dumps(
                {
                    "@context": "http://iiif.io/api/image/3/context.json",
                    "protocol": "http://iiif.io/api/image",
                }
            ),
        )

        with pytest.raises(
            Exception, match="'width' and/or 'height' missing or invalid"
        ):
            with webserver.install_http_handler(handler):
                gdal.Open(f"IIIF:http://localhost:{webserver_port}/my_image")

    with gdaltest.config_option("GDAL_DEFAULT_WMS_CACHE_PATH", str(tmp_vsimem)):

        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/my_image/info.json",
            200,
            {"Content-type": "application/json"},
            json.dumps(
                {
                    "@context": "http://iiif.io/api/image/3/context.json",
                    "protocol": "http://iiif.io/api/image",
                    "width": 100,
                    "height": 100,
                    "tiles": [{}],
                }
            ),
        )

        with pytest.raises(
            Exception,
            match=r"tiles\[0\].width' and/or 'tiles\[0\].height' missing or invalid",
        ):
            with webserver.install_http_handler(handler):
                gdal.Open(f"IIIF:http://localhost:{webserver_port}/my_image")

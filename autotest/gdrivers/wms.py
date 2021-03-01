#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WMS client support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
from time import sleep
import hashlib
import base64
from osgeo import gdal
import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('WMS')
    
###############################################################################
# Open the WMS dataset


def wms_2():
    #Set the driver so we don't have to search it by name again
    gdaltest.wms_drv = gdal.GetDriverByName('WMS')

    # NOTE - mloskot:
    # This is a dirty hack checking if remote WMS service is online.
    # Nothing genuine but helps to keep the buildbot waterfall green.

    srv = 'http://sedac.ciesin.columbia.edu/mapserver/map/GPWv3?'
    gdaltest.wms_srv1_ok = gdaltest.gdalurlopen(srv) is not None
    gdaltest.wms_ds = None

    if not gdaltest.wms_srv1_ok:
        pytest.skip()

    gdaltest.wms_ds = gdal.Open('data/wms/pop_wms.xml')

    if gdaltest.wms_ds is None:
        pytest.fail('open failed.')

###############################################################################
# Check various things about the configuration.


def wms_3():

    if gdaltest.wms_ds is None or not gdaltest.wms_srv1_ok:
        pytest.skip()

    assert gdaltest.wms_ds.RasterXSize == 36000 and gdaltest.wms_ds.RasterYSize == 14500 and gdaltest.wms_ds.RasterCount == 3, \
        'wrong size or bands'

    wkt = gdaltest.wms_ds.GetProjectionRef()
    assert wkt[:14] == 'GEOGCS["WGS 84', ('Got wrong SRS: ' + wkt)

    gt = gdaltest.wms_ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-180, abs=0.00001) and gt[3] == pytest.approx(85, abs=0.00001) and gt[1] == pytest.approx(0.01, abs=0.00001) and gt[2] == pytest.approx(0, abs=0.00001) and gt[5] == pytest.approx(-0.01, abs=0.00001) and gt[4] == pytest.approx(0, abs=0.00001), \
        'wrong geotransform'

    assert gdaltest.wms_ds.GetRasterBand(1).GetOverviewCount() >= 1, 'no overviews!'

    assert gdaltest.wms_ds.GetRasterBand(1).DataType >= gdal.GDT_Byte, \
        'wrong band data type'

###############################################################################
# Check checksum for a small region.


def wms_4():

    if gdaltest.wms_ds is None or not gdaltest.wms_srv1_ok:
        pytest.skip()

    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'ON')
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    cs = gdaltest.wms_ds.GetRasterBand(1).Checksum(0, 0, 100, 100)

    gdal.PopErrorHandler()
    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'OFF')
    msg = gdal.GetLastErrorMsg()
    gdal.ErrorReset()

    if msg is not None and msg.find('Service denied due to system overload') != -1:
        pytest.skip(msg)

    assert cs == 57182, ('Wrong checksum: ' + str(cs))

###############################################################################
# Open the WMS service using XML as filename.


def test_wms_5():
    # We don't need to check if the remote service is online as we
    # don't need a connection for this test

    fn = '<GDAL_WMS><Service name="WMS"><Version>1.1.1</Version><ServerUrl>http://onearth.jpl.nasa.gov/wms.cgi?</ServerUrl><SRS>EPSG:4326</SRS><ImageFormat>image/jpeg</ImageFormat><Layers>modis,global_mosaic</Layers><Styles></Styles></Service><DataWindow><UpperLeftX>-180.0</UpperLeftX><UpperLeftY>90.0</UpperLeftY><LowerRightX>180.0</LowerRightX><LowerRightY>-90.0</LowerRightY><SizeX>2666666</SizeX><SizeY>1333333</SizeY></DataWindow><Projection>EPSG:4326</Projection><BandsCount>3</BandsCount></GDAL_WMS>'
    ds = gdal.Open(fn)

    assert ds is not None, 'open failed.'

    assert ds.RasterXSize == 2666666 and ds.RasterYSize == 1333333 and ds.RasterCount == 3, \
        'wrong size or bands'

    ds = None

###############################################################################
# Test TileService


def test_wms_6():

    # We don't need to check if the remote service is online as we
    # don't need a connection for this test

    fn = '<GDAL_WMS><Service name="TileService"><Version>1</Version><ServerUrl>http://s0.tileservice.worldwindcentral.com/getTile?</ServerUrl><Dataset>za.johannesburg_2006_20cm</Dataset></Service><DataWindow><UpperLeftX>-180.0</UpperLeftX><UpperLeftY>90.0</UpperLeftY><LowerRightX>180.0</LowerRightX><LowerRightY>-90.0</LowerRightY><SizeX>268435456</SizeX><SizeY>134217728</SizeY><TileLevel>19</TileLevel></DataWindow><Projection>EPSG:4326</Projection><OverviewCount>16</OverviewCount><BlockSizeX>512</BlockSizeX><BlockSizeY>512</BlockSizeY><BandsCount>3</BandsCount></GDAL_WMS>'
    ds = gdal.Open(fn)

    assert ds is not None, 'open failed.'

    assert ds.RasterXSize == 268435456 and ds.RasterYSize == 134217728 and ds.RasterCount == 3, \
        'wrong size or bands'

    ds = None

###############################################################################
# Test TMS


def test_wms_7():

    srv = 'http://tilecache.osgeo.org/wms-c/Basic.py'
    gdaltest.metacarta_tms = False
    if gdaltest.gdalurlopen(srv) is None:
        pytest.skip()
    gdaltest.metacarta_tms = True

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

    ds = gdal.Open(tms)

    assert ds is not None, 'open failed.'

    assert ds.RasterXSize == 268435456 and ds.RasterYSize == 134217728 and ds.RasterCount == 3, \
        'wrong size or bands'

    assert ds.GetRasterBand(1).GetOverview(18).XSize == 512 and ds.GetRasterBand(1).GetOverview(18).YSize == 256

    ds.GetRasterBand(1).GetOverview(18).ReadRaster(0, 0, 512, 256)

    ds = None


###############################################################################
# Test TMS with cache

def test_wms_8():

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

    server_url = 'http://tile.openstreetmap.org'
    wmstms_version = ''
    zero_tile = '/0/0/0.png'
    server_url_mask = server_url + '/${z}/${x}/${y}.png'
    ovr_upper_level = 16
    tms = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>%s</ServerUrl>
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
    <Cache><Path>./tmp/gdalwmscache</Path></Cache>
</GDAL_WMS>""" % server_url_mask

    tms_nocache = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>%s</ServerUrl>
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
</GDAL_WMS>""" % server_url_mask

    if gdaltest.gdalurlopen(server_url) is None:
        pytest.skip()

    try:
        shutil.rmtree('tmp/gdalwmscache')
    except OSError:
        pass

    ds = gdal.Open(tms)

    assert ds is not None, 'open failed.'

    # Check cache metadata item
    cache_path = ds.GetMetadataItem("CACHE_PATH")
    assert cache_path, 'did not get expected cache path metadata item'

    cache_subfolder = hashlib.md5(server_url_mask.encode('utf-8')).hexdigest()

    gdal.ErrorReset()
    data = ds.GetRasterBand(1).GetOverview(ovr_upper_level).ReadRaster(0, 0, 512, 512)
    if gdal.GetLastErrorMsg() != '':
        if gdaltest.gdalurlopen(server_url + zero_tile) is None:
            pytest.skip()

    ds = None

    file1 = hashlib.md5((server_url + wmstms_version + '/1/0/0.png').encode('utf-8')).hexdigest()
    file2 = hashlib.md5((server_url + wmstms_version + '/1/1/0.png').encode('utf-8')).hexdigest()
    file3 = hashlib.md5((server_url + wmstms_version + '/1/0/1.png').encode('utf-8')).hexdigest()
    file4 = hashlib.md5((server_url + wmstms_version + '/1/1/1.png').encode('utf-8')).hexdigest()

    expected_files = ['tmp/gdalwmscache/%s/%s/%s/%s' % (cache_subfolder, file1[0], file1[1], file1),
                      'tmp/gdalwmscache/%s/%s/%s/%s' % (cache_subfolder, file2[0], file2[1], file2),
                      'tmp/gdalwmscache/%s/%s/%s/%s' % (cache_subfolder, file3[0], file3[1], file3),
                      'tmp/gdalwmscache/%s/%s/%s/%s' % (cache_subfolder, file4[0], file4[1], file4)]
    for expected_file in expected_files:
        try:
            os.stat(expected_file)
        except OSError:
            pytest.fail('%s should exist' % expected_file)

    # Now, we should read from the cache
    ds = gdal.Open(tms)
    cached_data = ds.GetRasterBand(1).GetOverview(ovr_upper_level).ReadRaster(0, 0, 512, 512)
    ds = None

    assert data == cached_data, 'data != cached_data'

    # Replace the cache with fake data
    for expected_file in expected_files:

        ds = gdal.GetDriverByName('GTiff').Create(expected_file, 256, 256, 4)
        ds.GetRasterBand(1).Fill(0)
        ds.GetRasterBand(2).Fill(0)
        ds.GetRasterBand(3).Fill(0)
        ds.GetRasterBand(4).Fill(255)
        ds = None

    # Read again from the cache, and check that it is actually used
    ds = gdal.Open(tms)
    cs = ds.GetRasterBand(1).GetOverview(ovr_upper_level).Checksum()
    ds = None
    assert cs == 0, 'cs != 0'

    # Test with GDAL_DEFAULT_WMS_CACHE_PATH
    # Now, we should read from the cache
    gdal.SetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", "./tmp/gdalwmscache")
    ds = gdal.Open(tms_nocache)
    cs = ds.GetRasterBand(1).GetOverview(ovr_upper_level).Checksum()
    ds = None
    gdal.SetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", None)
    assert cs == 0, 'cs != 0'

    # Check maxsize and expired tags
    tms_expires = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>%s</ServerUrl>
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
    <Cache><Path>./tmp/gdalwmscache</Path><Expires>1</Expires></Cache>
</GDAL_WMS>""" % server_url_mask

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
        srv = 'http://onearth.jpl.nasa.gov/wms.cgi?'
        if gdaltest.gdalurlopen(srv) is None:
            pytest.skip()
        pytest.fail('open failed.')

    expected_cs = 5478
    cs = ds.GetRasterBand(1).GetOverview(9).Checksum()

    assert cs == expected_cs, 'Did not get expected SRTM checksum.'

    ds = None

###############################################################################
# Test getting subdatasets from GetCapabilities

def wms_10():

    if not gdaltest.wms_srv1_ok:
        pytest.skip()

    name = "WMS:http://sedac.ciesin.columbia.edu/mapserver/map/GPWv3?"
    ds = gdal.Open(name)
    assert ds is not None, ('open of %s failed.' % name)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, 'did not get expected subdataset count'

    ds = None

    name = subdatasets['SUBDATASET_1_NAME']
    ds = gdal.Open(name)
    assert ds is not None, ('open of %s failed.' % name)

    ds = None

###############################################################################
# Test getting subdatasets from GetTileService


# Permanently down
def wms_11():
    
    if gdaltest.skip_on_travis():
        pytest.skip()

    srv = 'http://onearth.jpl.nasa.gov/wms.cgi'
    if gdaltest.gdalurlopen(srv) is None:
        pytest.skip()

    name = "WMS:http://onearth.jpl.nasa.gov/wms.cgi?request=GetTileService"
    ds = gdal.Open(name)
    assert ds is not None, ('open of %s failed.' % name)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, 'did not get expected subdataset count'

    ds = None

    name = subdatasets['SUBDATASET_1_NAME']
    ds = gdal.Open(name)
    assert ds is not None, ('open of %s failed.' % name)

    ds = None

###############################################################################
# Test getting subdatasets from a TMS server


def test_wms_12():

    if gdaltest.metacarta_tms is not True:
        pytest.skip()

    name = "http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/"
    ds = gdal.Open(name)
    if ds is None:
        if gdaltest.gdalurlopen('http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/basic/0/0/0.png') is None:
            pytest.skip()
        pytest.fail('open of %s failed.' % name)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, 'did not get expected subdataset count'

    ds = None

    for i in range(len(subdatasets) // 2):
        desc = subdatasets['SUBDATASET_%d_DESC' % (i + 1)]
        if desc == 'basic':
            name = subdatasets['SUBDATASET_%d_NAME' % (i + 1)]
            ds = gdal.Open(name)
            if ds is None:
                if gdaltest.gdalurlopen('http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/basic/0/0/0.png') is None:
                    pytest.skip()
                pytest.fail('open of %s failed.' % name)
            ds = None

    
###############################################################################
# Test reading WMS through VRT (test effect of r21866)


def test_wms_13():

    ds = gdal.Open("data/wms/DNEC_250K.vrt")
    if ds.ReadRaster(0, 0, 1024, 682) is None:
        srv = 'http://wms.geobase.ca/wms-bin/cubeserv.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GeCapabilities'
        if gdaltest.gdalurlopen(srv) is None:
            pytest.skip()
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
        return' fail'

    assert ds.RasterXSize == 536870912 and ds.RasterYSize == 536870912 and ds.RasterCount == 3, \
        'wrong size or bands'

    wkt = ds.GetProjectionRef()
    assert 'EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs"]' in wkt, \
        ('Got wrong SRS: ' + wkt)

    gt = ds.GetGeoTransform()
    assert abs(gt[0] - -20037508.34278924,) <= 0.00001 and abs(gt[3] - 20037508.34278924,) <= 0.00001 and gt[1] == pytest.approx(0.07464553543474242, abs=0.00001) and gt[2] == pytest.approx(0, abs=0.00001) and abs(gt[5] - -0.07464553543474242,) <= 0.00001 and gt[4] == pytest.approx(0, abs=0.00001), \
        'wrong geotransform'

    assert ds.GetRasterBand(1).GetOverviewCount() == 20, 'bad overview count'

    (block_xsize, block_ysize) = ds.GetRasterBand(1).GetBlockSize()
    if block_xsize != 256 or block_ysize != 256:
        print("(%d, %d)" % (block_xsize, block_ysize))
        pytest.fail('bad block size')

    
###############################################################################
# Test reading ArcGIS MapServer JSon definition and CreateCopy()


def test_wms_15():

    srv = 'http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer?f=json&pretty=true'
    src_ds = gdal.Open(srv)
    if src_ds is None:
        if gdaltest.gdalurlopen(srv) is None:
            pytest.skip()
        pytest.fail()
    ds = gdal.GetDriverByName("WMS").CreateCopy("/vsimem/wms.xml", src_ds)
    src_ds = None
    assert ds, 'failed to copy'

    assert ds.RasterXSize == 1073741824 and ds.RasterYSize == 1073741824 and ds.RasterCount == 3, \
        'wrong size or bands'

    wkt = ds.GetProjectionRef()
    assert wkt.startswith('PROJCS["WGS 84 / Pseudo-Mercator"'), ('Got wrong SRS: ' + wkt)

    corner = 20037508.342787001
    res = 0.037322767717361482
    assert ds.GetGeoTransform() == pytest.approx([-corner, res, 0, corner, 0, -res], abs = 0.00001), 'wrong geotransform'

    assert ds.GetRasterBand(1).GetOverviewCount() == 22, 'bad overview count'
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]

    ds = None
    gdal.Unlink("/vsimem/wms.xml")

###############################################################################
# Test getting subdatasets from WMS-C Capabilities


def test_wms_16():

    if not gdaltest.run_slow_tests():
        # server often returns a 504 after ages; this test can take minutes
        pytest.skip()

    name = "WMS:http://demo.opengeo.org/geoserver/gwc/service/wms?tiled=TRUE"
    ds = gdal.Open(name)
    if ds is None:
        srv = 'http://demo.opengeo.org/geoserver/gwc/service/wms?'
        if gdaltest.gdalurlopen(srv) is None:
            pytest.skip()
        pytest.fail('open of %s failed.' % name)

    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert subdatasets, 'did not get expected subdataset count'

    ds = None

    name = None
    for key in subdatasets:
        if key[-5:] == '_NAME' and subdatasets[key].find('bugsites') != -1:
            name = subdatasets[key]
            break
    assert name is not None

    name = 'http://demo.opengeo.org/geoserver/wms?SERVICE=WMS&request=GetMap&version=1.1.1&layers=og:bugsites&styles=&srs=EPSG:26713&bbox=599351.50000000,4914096.00000000,608471.00000000,4920512.00000000'
    ds = gdal.Open(name)
    assert ds is not None, ('open of %s failed.' % name)

    # Matches feature of "WFS:http://demo.opengeo.org/geoserver/wfs?SRSNAME=EPSG:900913" og:bugsites
    # OGRFeature(og:bugsites):68846
    #   gml_id (String) = bugsites.68846
    #   cat (Integer) = 86
    #   str1 (String) = Beetle site
    #   POINT (-11547069.564865021035075 5528605.849725087173283)

    pixel = "GeoPixel_601228_4917635"
    val = ds.GetRasterBand(1).GetMetadataItem(pixel, "LocationInfo")

    # Some bug in GeoServer ?
    if val is not None and 'java.lang.NoSuchMethodError: org.geoserver.wms.WMS.pixelToWorld' in val:
        pytest.skip(val)

    if val is not None and ('Gateway Time-out' in val or
                            'HTTP error code : 5' in val):
        pytest.skip()

    if val is None or val.find('<og:cat>86</og:cat>') == -1:
        if 'java.lang.NullPointerException' in val or '504 Gateway Time-out' in val or 'java.lang.OutOfMemoryError' in val:
            pytest.skip(val)

        print(val)
        pytest.fail('expected a value')

    # Ask again. Should be cached
    val_again = ds.GetRasterBand(1).GetMetadataItem(pixel, "LocationInfo")
    assert val_again == val, 'expected a value'

    # Ask another band. Should be cached
    val2 = ds.GetRasterBand(2).GetMetadataItem(pixel, "LocationInfo")
    assert val2 == val, 'expected a value'

    # Ask an overview band
    val2 = ds.GetRasterBand(1).GetOverview(0).GetMetadataItem(pixel, "LocationInfo")
    if val2 != val:
        if 'java.lang.NullPointerException' in val2 or '504 Gateway Time-out' in val2 or 'java.lang.OutOfMemoryError' in val2:
            pytest.skip(val2)

        print(val2)
        pytest.fail('expected a value')

    ds = None

###############################################################################
# Test a TiledWMS dataset with a color table (#4613)

# Permanently down
def wms_17():

    srv = 'http://onmoon.lmmp.nasa.gov/sites/wms.cgi?'
    if gdaltest.gdalurlopen(srv) is None:
        pytest.skip()

    name = '<GDAL_WMS><Service name="TiledWMS"><ServerUrl>http://onmoon.lmmp.nasa.gov/sites/wms.cgi?</ServerUrl><TiledGroupName>King Crater DEM Color Confidence, LMMP</TiledGroupName></Service></GDAL_WMS>'
    ds = gdal.Open(name)
    assert ds is not None, ('open of %s failed.' % name)

    band = ds.GetRasterBand(1)
    assert band.GetColorTable() is not None

    ds = None

###############################################################################
# Test an ArcGIS Server


def test_wms_18():

    # We don't need to check if the remote service is online as we
    # don't need a connection for this test.

    fn = '<GDAL_WMS><Service name="AGS"><ServerUrl>http://sampleserver1.arcgisonline.com/ArcGIS/rest/services/Specialty/ESRI_StateCityHighway_USA/MapServer</ServerUrl><BBoxOrder>xyXY</BBoxOrder><SRS>EPSG:3857</SRS></Service><DataWindow><UpperLeftX>-20037508.34</UpperLeftX><UpperLeftY>20037508.34</UpperLeftY><LowerRightX>20037508.34</LowerRightX><LowerRightY>-20037508.34</LowerRightY><SizeX>512</SizeX><SizeY>512</SizeY></DataWindow></GDAL_WMS>'

    ds = gdal.Open(fn)

    assert ds is not None, 'open failed.'

    assert ds.RasterXSize == 512 and ds.RasterYSize == 512 and ds.RasterCount == 3, \
        'wrong size or bands'

    # todo: add locationinfo test

    # add getting image test
    if not gdaltest.gdalurlopen('http://sampleserver1.arcgisonline.com/ArcGIS/rest/services/Specialty/ESRI_StateCityHighway_USA/MapServer'):
        ds = None
        pytest.skip()

    expected_cs = 12824
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs, 'Did not get expected checksum.'
    ds = None

    # Alternative url with additional parameters
    fn = '<GDAL_WMS><Service name="AGS"><ServerUrl>http://sampleserver1.arcgisonline.com/ArcGIS/rest/services/Specialty/ESRI_StateCityHighway_USA/MapServer/export?dpi=96&amp;layerdefs=&amp;layerTimeOptions=&amp;dynamicLayers=&amp;</ServerUrl><BBoxOrder>xyXY</BBoxOrder><SRS>EPSG:3857</SRS></Service><DataWindow><UpperLeftX>-20037508.34</UpperLeftX><UpperLeftY>20037508.34</UpperLeftY><LowerRightX>20037508.34</LowerRightX><LowerRightY>-20037508.34</LowerRightY><SizeX>512</SizeX><SizeY>512</SizeY></DataWindow></GDAL_WMS>'

    ds = gdal.Open(fn)

    assert ds is not None, 'open failed.'
    assert ds.RasterXSize == 512 and ds.RasterYSize == 512 and ds.RasterCount == 3, \
        'wrong size or bands'
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs, 'Did not get expected checksum.'
    ds = None

###############################################################################
# Test a IIP server


def test_wms_19():

    ds = gdal.Open('IIP:http://merovingio.c2rmf.cnrs.fr/fcgi-bin/iipsrv.fcgi?FIF=globe.256x256.tif')

    if ds is None:
        if gdaltest.gdalurlopen('http://merovingio.c2rmf.cnrs.fr/fcgi-bin/iipsrv.fcgi?FIF=globe.256x256.tif&obj=Basic-Info') is None:
            pytest.skip()
        pytest.fail('open failed.')

    assert ds.RasterXSize == 86400 and ds.RasterYSize == 43200 and ds.RasterCount == 3, \
        'wrong size or bands'

    # Expected checksum seems to change over time. Hum...
    cs = ds.GetRasterBand(1).GetOverview(ds.GetRasterBand(1).GetOverviewCount() - 1).Checksum()
    assert cs != 0, 'Did not get expected checksum.'

    ds = None

###############################################################################
# Test reading data via MRF/LERC

def test_wms_data_via_mrf():

    mrfdrv = gdal.GetDriverByName("MRF")

    if mrfdrv is None or 'LERC' not in mrfdrv.GetMetadataItem('DMD_CREATIONOPTIONLIST'):
        pytest.skip()
    
    url = "http://astro.arcgis.com/arcgis/rest/services/OnMars/HiRISE_DEM/ImageServer/tile/${z}/${y}/${x}"
    dstemplate = """<GDAL_WMS>
<Service name="TMS" ServerUrl="{url}"/>
<DataWindow SizeX="513" SizeY="513"/>
<BandsCount>1</BandsCount><BlockSizeX>513</BlockSizeX><BlockSizeY>513</BlockSizeY>
<DataType>{dt}</DataType><DataValues NoData="{ndv}"/>
</GDAL_WMS>"""

    # This is a LERC1 format tile service, DEM in floating point, it can be read as any type
    # The returned no data value can also be set on read, which affects the checksum
    testlist = [
        ("Byte", 0, 29585), # Same as the default type, NDV not defined
        ("Float32", 0, 56047), # float, default NDV
        ("Float32", 32768.32, 33595), # float, Forced NDV
    ]
    for dt, ndv, expected_cs in testlist:
        ds = gdal.Open(dstemplate.format(url = url, dt = dt, ndv=ndv))
        assert ds.GetRasterBand(1).Checksum() == expected_cs, "datatype {} and ndv {}".format(dt, ndv)
        ds = None
 
def test_twms_wmsmetadriver():
    gdaltest.gts = """<WMS_Tile_Service version="0.1.0">
  <TiledPatterns>
    <OnlineResource xlink:href="https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?"/>
    <LatLonBoundingBox minx="-180" miny="-90" maxx="180" maxy="90" />
    <TiledGroup>
        <Name>MODIS Terra CorrectedReflectance TrueColor tileset</Name>
        <Title>Corrected Reflectance (True Color, MODIS, Terra)</Title>
        <Abstract>MODIS_Terra_CorrectedReflectance_TrueColor abstract</Abstract>
        <Projection>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]</Projection>
        <Pad>0</Pad>
        <Bands>3</Bands>
        <LatLonBoundingBox minx="-180" miny="-90" maxx="180" maxy="90" />
        <Key>${time}</Key>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,88.875,-178.875,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,88.875,-178.875,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,87.75,-177.75,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,87.75,-177.75,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,85.5,-175.5,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,85.5,-175.5,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,81,-171,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,81,-171,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,72,-162,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,72,-162,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,54,-144,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,54,-144,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,18,-108,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,18,-108,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,-54,-36,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,-54,-36,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,-198,108,90 request=GetMap&layers=MODIS_Terra_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,-198,108,90]]></TilePattern>
    </TiledGroup>
    <TiledGroup>
        <Name>MODIS Aqua CorrectedReflectance TrueColor tileset</Name>
        <Title>Corrected Reflectance (True Color, MODIS, Aqua)</Title>
        <Abstract>MODIS_Aqua_CorrectedReflectance_TrueColor abstract</Abstract>
        <Projection>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]</Projection>
        <Pad>0</Pad>
        <Bands>3</Bands>
        <LatLonBoundingBox minx="-180" miny="-90" maxx="180" maxy="90" />
        <Key>${time}</Key>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,88.875,-178.875,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,88.875,-178.875,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,87.75,-177.75,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,87.75,-177.75,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,85.5,-175.5,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,85.5,-175.5,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,81,-171,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,81,-171,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,72,-162,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,72,-162,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,54,-144,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,54,-144,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,18,-108,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,18,-108,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,-54,-36,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,-54,-36,90]]></TilePattern>
        <TilePattern><![CDATA[request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&time=${time}&width=512&height=512&bbox=-180,-198,108,90 request=GetMap&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&srs=EPSG:4326&format=image%2Fjpeg&styles=&width=512&height=512&bbox=-180,-198,108,90]]></TilePattern>
    </TiledGroup>
  </TiledPatterns>
</WMS_Tile_Service>"""
    gdal.FileFromMemBuffer("/vsimem/twms.xml", gdaltest.gts)

    ds = gdal.Open("/vsimem/twms.xml")
    assert ds is not None, "Open tiledWMS failed"
    gdaltest.subdatasets = ds.GetMetadata("SUBDATASETS")
    assert gdaltest.subdatasets, "Expected subdatasets"
    ds = None
    gdal.Unlink("/vsimem/twms.xml")


# This test requires the GIBS server to be available
def test_twms_GIBS():

    # if not gdaltest.run_slow_tests():
    #     pytest.skip()
    baseURL = "https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?"

    try:
        subdatasets = gdaltest.subdatasets
    except:
        pytest.skip()

    # Connects to the server
    ds = gdal.Open(subdatasets['SUBDATASET_1_NAME'])
    if ds is None and gdaltest.gdalurlopen(baseURL + "request=GetTileService"):
        pytest.fail()
    ds = None

    # Connects to the server
    options = [ "Change=time:2021-02-10" ]
    ds = gdal.OpenEx(subdatasets['SUBDATASET_1_NAME'], open_options = options)
    if ds is None and gdaltest.gdalurlopen(baseURL + "request=GetTileService"):
        pytest.fail()
    ds = None


def test_twms_inline_configuration():

    baseURL = "https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?"
    try:
        gts = gdaltest.gts
        baseURL = gdaltest.baseURL
    except:
        pytest.skip()

    # Try inline base64 configuration 
    wms_base64gts = base64.b64encode(gts.encode())
    twms_string = '<GDAL_WMS><Service name="TiledWMS" ServerUrl="{}"><Configuration encoding="base64">{}</Configuration></Service></GDAL_WMS>'.format(
            baseURL, wms_base64gts.decode())
    gdal.FileFromMemBuffer("/vsimem/twms.xml", twms_string)
    
    # Open fails without a TiledGroupName
    ds = gdal.Open("/vsimem/twms.xml")
    assert ds is None, "Expected failure to open"

    tiled_group_name = "MODIS Aqua CorrectedReflectance TrueColor tileset"
    date = "2021-02-10"
    options = [ "TiledGroupName={}".format(tiled_group_name), "Change=time:{}".format(date) ]
    ds = gdal.OpenEx("/vsimem/twms.xml", open_options = options )
    assert ds is not None, "Open twms with open options failed"
    metadata = ds.GetMetadata("")
    assert metadata["Change"] == "${time}=2021-02-10", "Change parameter not captured"
    assert metadata["TiledGroupName"] == tiled_group_name, "TIledGroupName not captured"
    ds = None


def test_wms_cleanup():

    gdaltest.wms_ds = None
    gdaltest.gts = None
    gdaltest.subdatasets = None
    gdaltest.clean_tmp()

    gdal.UnlinkBatch(["/vsimem/twms.xml", "/vsimem/twms.xml.aux.xml"])
    try:
        shutil.rmtree('gdalwmscache')
    except OSError:
        pass


    
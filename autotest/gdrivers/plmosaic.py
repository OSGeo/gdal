#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PlanetLabs mosaic driver test suite.
# Author:   Even Rouault, even dot rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2015, Planet Labs
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
import struct


from osgeo import gdal

import gdaltest
import pytest

###############################################################################
# Find PLMosaic driver


def test_plmosaic_1():

    gdaltest.plmosaic_drv = gdal.GetDriverByName('PLMosaic')

    if gdaltest.plmosaic_drv is not None:
        return
    pytest.skip()

###############################################################################
# Error: no API_KEY


def test_plmosaic_2():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER)
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error case: invalid root URL


def test_plmosaic_3():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/does_not_exist/')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error case: invalid JSON


def test_plmosaic_4():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root', """{""")

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error case: not a JSON dictionary


def test_plmosaic_5():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root', """null""")

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error case: missing "mosaics" element


def test_plmosaic_6():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root', """{}""")

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Valid root but no mosaics


def test_plmosaic_7():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root', """{
    "mosaics": [],
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None
    ds = None

###############################################################################
# Valid root with 2 mosaics


def test_plmosaic_8():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root', """{
    "_links" : { "_next": "/vsimem/root/?page=2" },
    "mosaics": [
        {
            "id": "my_mosaic_id",
            "name": "my_mosaic_name",
            "coordinate_system": "EPSG:3857",
            "_links" : {
                "_self": "/vsimem/root/my_mosaic"
            },
            "quad_download": true
        }
    ],
}""")
    gdal.FileFromMemBuffer('/vsimem/root/?page=2', """{
    "_links" : { "_next": null },
    "mosaics": [
        {
            "id": "another_mosaic_id",
            "name": "another_mosaic_name",
            "coordinate_system": "EPSG:3857",
            "_links" : {
                "_self": "/vsimem/root/another_mosaic"
            },
            "quad_download": true
        },
        {
            "id": "this_one_will_be_ignored",
            "name": "this_one_will_be_ignored",
            "coordinate_system": "EPSG:1234",
            "_links" : {
                "_self": "/vsimem/root/this_one_will_be_ignored"
            },
            "quad_download": true
        }
    ],
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    assert ds.GetMetadata("SUBDATASETS") == {'SUBDATASET_2_NAME': 'PLMOSAIC:mosaic=another_mosaic_name', 'SUBDATASET_2_DESC': 'Mosaic another_mosaic_name', 'SUBDATASET_1_NAME': 'PLMOSAIC:mosaic=my_mosaic_name', 'SUBDATASET_1_DESC': 'Mosaic my_mosaic_name'}
    ds = None

###############################################################################
# Error case: invalid mosaic


def test_plmosaic_9():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=does_not_exist'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert ds is None and gdal.GetLastErrorMsg().find('/vsimem/root/?name__is=does_not_exist') >= 0

###############################################################################
# Invalid mosaic definition: invalid JSON


def test_plmosaic_9bis():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('JSON parsing error') >= 0

###############################################################################
# Invalid mosaic definition: JSON without mosaics array


def test_plmosaic_9ter():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('No mosaic my_mosaic') >= 0

###############################################################################
# Invalid mosaic definition: missing parameters


def test_plmosaic_10():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic"
}]
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('Missing required parameter') >= 0

###############################################################################
# Invalid mosaic definition: unsupported projection


def test_plmosaic_11():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
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
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('Unsupported coordinate_system') >= 0

###############################################################################
# Invalid mosaic definition: unsupported datatype


def test_plmosaic_12():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
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
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('Unsupported data_type') >= 0

###############################################################################
# Invalid mosaic definition: unsupported resolution


def test_plmosaic_13():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
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
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('Unsupported resolution') >= 0

###############################################################################
# Invalid mosaic definition: unsupported quad_size


def test_plmosaic_14():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
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
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('Unsupported quad_size') >= 0

###############################################################################
# Nearly valid mosaic definition. Warning about invalid links.tiles


def test_plmosaic_15():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "grid": {
        "quad_size": 4096,
        "resolution": 4.77731426716
    },
    "first_acquired": "first_date",
    "last_acquired": "last_date",
    "_links" : {
        "tiles" : "/vsimem/root/my_mosaic/tiles/foo"
    },
    "quad_download": true
}]
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert gdal.GetLastErrorMsg().find('Invalid _links.tiles') >= 0
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetOverview(0) is None
    ds = None

###############################################################################
# Valid mosaic definition


def test_plmosaic_16():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    try:
        shutil.rmtree('tmp/plmosaic_cache')
    except OSError:
        pass

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "grid": {
        "quad_size": 4096,
        "resolution": 4.77731426716
    },
    "first_acquired": "first_date",
    "last_acquired": "last_date",
    "_links" : {
        "tiles" : "/vsimem/root/my_mosaic/tiles{0-3}/{z}/{x}/{y}.png"
    },
    "quad_download": true
}]
}""")

    # Valid root: one single mosaic, should open the dataset directly
    gdal.FileFromMemBuffer('/vsimem/root', """{
    "mosaics": [
        {
            "id": "my_mosaic_id",
            "name": "my_mosaic",
            "coordinate_system": "EPSG:3857",
            "_links" : {
                "_self": "/vsimem/root/my_mosaic"
            },
            "quad_download": true
        }
    ],
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:api_key=foo,unsupported_option=val', gdal.OF_RASTER)
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    assert ds is None and gdal.GetLastErrorMsg().find('Unsupported option unsupported_option') >= 0

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    assert ds.GetMetadata("SUBDATASETS") == {}
    assert ds.GetMetadata() == {'LAST_ACQUIRED': 'last_date', 'NAME': 'my_mosaic', 'FIRST_ACQUIRED': 'first_date'}
    ds = None

###############################################################################
# Open with explicit MOSAIC dataset open option


def test_plmosaic_17():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.SetConfigOption('PL_URL', None)
    assert ds is not None
    assert ds.GetMetadata() == {'LAST_ACQUIRED': 'last_date', 'NAME': 'my_mosaic', 'FIRST_ACQUIRED': 'first_date'}
    assert ds.GetProjectionRef().find('3857') >= 0
    assert ds.RasterXSize == 8388608
    assert ds.RasterYSize == 8388608
    got_gt = ds.GetGeoTransform()
    expected_gt = (-20037508.34, 4.7773142671600004, 0.0, 20037508.34, 0.0, -4.7773142671600004)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8), ds.GetGeoTransform()
    assert ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') == 'PIXEL', \
        ds.GetMetadata('IMAGE_STRUCTURE')
    assert ds.GetRasterBand(1).GetOverviewCount() == 15
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(ds.GetRasterBand(1).GetOverviewCount()) is None
    assert ds.GetRasterBand(1).GetOverview(0) is not None

    try:
        shutil.rmtree('tmp/plmosaic_cache')
    except OSError:
        pass

    for i in range(12):
        # Read at one nonexistent position.
        ds.GetRasterBand(1).ReadRaster(4096 * i, 0, 1, 1)
        assert gdal.GetLastErrorMsg() == ''
    for i in range(11, -1, -1):
        # Again in the same quad, but in different block, to test cache
        ds.GetRasterBand(1).ReadRaster(4096 * i + 256, 0, 1, 1)
        assert gdal.GetLastErrorMsg() == ''
    for i in range(12):
        # Again in the same quad, but in different block, to test cache
        ds.GetRasterBand(1).ReadRaster(4096 * i + 512, 256, 1, 1)
        assert gdal.GetLastErrorMsg() == ''

    ds.FlushCache()

    # Invalid tile content
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic_id/quads/0-2047/full', 'garbage')
    gdal.PushErrorHandler()
    ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    gdal.PopErrorHandler()

    os.stat('tmp/plmosaic_cache/my_mosaic/my_mosaic_0-2047.tif')

    ds.FlushCache()
    shutil.rmtree('tmp/plmosaic_cache')

    # GeoTIFF but with wrong dimensions
    gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic_id/quads/0-2047/full', 1, 1, 1)
    gdal.PushErrorHandler()
    ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    gdal.PopErrorHandler()

    os.stat('tmp/plmosaic_cache/my_mosaic/my_mosaic_0-2047.tif')

    ds.FlushCache()
    shutil.rmtree('tmp/plmosaic_cache')

    # Good GeoTIFF
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic_id/quads/0-2047/full', 4096, 4096, 4, options=['INTERLEAVE=BAND', 'SPARSE_OK=YES'])
    tmp_ds.GetRasterBand(1).Fill(255)
    tmp_ds = None

    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack('B', val)[0]
    assert val == 255

    os.stat('tmp/plmosaic_cache/my_mosaic/my_mosaic_0-2047.tif')

    ds.FlushCache()

    # Read again from file cache.
    # We change the file behind the scene (but not changing its size)
    # to demonstrate that the cached tile is still use
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic_id/quads/0-2047/full', 4096, 4096, 4, options=['INTERLEAVE=BAND', 'SPARSE_OK=YES'])
    tmp_ds.GetRasterBand(1).Fill(1)
    tmp_ds = None
    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack('B', val)[0]
    assert val == 255

    ds = None

    # Read again from file cache, but with TRUST_CACHE=YES
    # delete the full GeoTIFF before
    gdal.Unlink('/vsimem/root/my_mosaic_id/quads/0-2047/full')
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:API_KEY=foo,MOSAIC=my_mosaic,CACHE_PATH=tmp,TRUST_CACHE=YES', gdal.OF_RASTER)
    gdal.SetConfigOption('PL_URL', None)

    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack('B', val)[0]
    assert val == 255
    ds = None

    # Read again from file cache but the metatile has changed in between
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.SetConfigOption('PL_URL', None)

    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic_id/quads/0-2047/full', 4096, 4096, 4, options=['INTERLEAVE=BAND', 'SPARSE_OK=YES'])
    tmp_ds.SetMetadataItem('foo', 'bar')
    tmp_ds.GetRasterBand(1).Fill(254)
    tmp_ds = None

    val = ds.ReadRaster(0, 0, 1, 1)
    val = struct.unpack('B' * 4, val)
    assert val == (254, 0, 0, 0)

###############################################################################
# Test location info


def test_plmosaic_18():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    shutil.rmtree('tmp/plmosaic_cache')

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.SetConfigOption('PL_URL', None)

    ret = ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo')
    assert ret == """<LocationInfo />
"""
    old_ret = ret
    ret = ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo')
    assert ret == old_ret

    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic_id/quads/0-2047/items', """{
    "items": [
        { "link": "foo" }
    ]
}""")

    ds.FlushCache()

    ret = ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo')
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


def test_plmosaic_19():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=/does_not_exist'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PushErrorHandler()
    val = ds.ReadRaster(0, 0, 1, 1)
    gdal.PopErrorHandler()
    val = struct.unpack('B' * 4, val)
    assert val == (254, 0, 0, 0)

    val = ds.ReadRaster(256, 0, 1, 1)
    val = struct.unpack('B' * 4, val)
    assert val == (254, 0, 0, 0)
    ds = None

###############################################################################
# Try disabling cache


def test_plmosaic_20():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH='])
    gdal.SetConfigOption('PL_URL', None)
    val = ds.ReadRaster(0, 0, 1, 1)
    val = struct.unpack('B' * 4, val)
    assert val == (254, 0, 0, 0)

    val = ds.ReadRaster(256, 0, 1, 1)
    val = struct.unpack('B' * 4, val)
    assert val == (254, 0, 0, 0)
    ds = None

###############################################################################
# Try use_tiles


def test_plmosaic_21():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=', 'USE_TILES=YES'])
    gdal.SetConfigOption('PL_URL', None)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ds.ReadRaster(256, 512, 1, 1)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ds.GetRasterBand(1).ReadRaster(256, 512, 1, 1)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ds.GetRasterBand(1).ReadBlock(1, 2)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=mosaic_uint16', """{
"mosaics": [{
    "id": "mosaic_uint16",
    "name": "mosaic_uint16",
    "coordinate_system": "EPSG:3857",
    "datatype": "uint16",
    "grid": {
        "quad_size": 4096,
        "resolution": 4.77731426716
    },
    "first_acquired": "first_date",
    "last_acquired": "last_date",
    "_links" : {
        "tiles" : "/vsimem/root/mosaic_uint16/tiles{0-3}/{z}/{x}/{y}.png"
    },
    "quad_download": true
}]
}""")

    # Should emit a warning
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=mosaic_uint16', 'CACHE_PATH=', 'USE_TILES=YES'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('Cannot use tile API for full resolution data on non Byte mosaic') >= 0

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=mosaic_without_tiles', """{
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
}""")

    # Should emit a warning
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo', 'MOSAIC=mosaic_without_tiles', 'CACHE_PATH=', 'USE_TILES=YES'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('Cannot find tile definition, so use_tiles will be ignored') >= 0

###############################################################################
# Valid mosaic definition with bbox


def test_plmosaic_with_bbox():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    try:
        shutil.rmtree('tmp/plmosaic_cache')
    except OSError:
        pass

    gdal.FileFromMemBuffer('/vsimem/root/?name__is=my_mosaic', """{
"mosaics": [{
    "id": "my_mosaic_id",
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "grid": {
        "quad_size": 4096,
        "resolution": 4.77731426716
    },
    "bbox" : [
        -100,
        30,
        -90,
        40
    ],
    "first_acquired": "first_date",
    "last_acquired": "last_date",
    "_links" : {
        "tiles" : "/vsimem/root/my_mosaic/tiles{0-3}/{z}/{x}/{y}.png"
    },
    "quad_download": true
}]
}""")

    # Valid root: one single mosaic, should open the dataset directly
    gdal.FileFromMemBuffer('/vsimem/root', """{
    "mosaics": [
        {
            "id": "my_mosaic_id",
            "name": "my_mosaic",
            "coordinate_system": "EPSG:3857",
            "_links" : {
                "_self": "/vsimem/root/my_mosaic"
            },
            "quad_download": true
        }
    ],
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options=['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    assert ds.RasterXSize == 233472
    assert ds.RasterYSize == 286720
    got_gt = ds.GetGeoTransform()
    expected_gt = (-11134123.286585508, 4.77731426716, 0.0, 4872401.930333553, 0.0, -4.77731426716)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)

    # Good GeoTIFF
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic_id/quads/455-1272/full', 4096, 4096, 4, options=['INTERLEAVE=BAND', 'SPARSE_OK=YES'])
    tmp_ds.GetRasterBand(1).Fill(125)
    tmp_ds = None

    val = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    val = struct.unpack('B', val)[0]
    assert val == 125

    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic_id/quads/455-1272/items', """{
    "items": [
        { "link": "bar" }
    ]
}""")

    ret = ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo')
    assert ret == """<LocationInfo>
  <Scenes>
    <Scene>
      <link>bar</link>
    </Scene>
  </Scenes>
</LocationInfo>
"""

###############################################################################
#


def test_plmosaic_cleanup():

    if gdaltest.plmosaic_drv is None:
        pytest.skip()

    gdal.Unlink('/vsimem/root_no_mosaics')
    gdal.Unlink('/vsimem/root')
    gdal.Unlink('/vsimem/root/?page=2')
    gdal.Unlink('/vsimem/root/?name__is=my_mosaic')
    gdal.Unlink('/vsimem/root/my_mosaic_id/quads/0-2047/full')
    gdal.Unlink('/vsimem/root/my_mosaic_id/quads/0-2047/items')
    gdal.Unlink('/vsimem/root/my_mosaic_id/quads/455-1272/full')
    gdal.Unlink('/vsimem/root/my_mosaic_id/quads/455-1272/items')
    gdal.Unlink('/vsimem/root/?name__is=mosaic_uint16')
    gdal.Unlink('/vsimem/root/?name__is=mosaic_without_tiles')

    if gdal.ReadDir('/vsimem/root') is not None:
        print(gdal.ReadDir('/vsimem/root'))

    try:
        shutil.rmtree('tmp/plmosaic_cache')
    except OSError:
        pass

    



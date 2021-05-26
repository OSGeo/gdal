#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  RDA mosaic driver test suite.
# Author:   Even Rouault, even dot rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2017, DigitalGlobe
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
import struct
import json


from osgeo import gdal

import gdaltest
import webserver
import pytest

###############################################################################
# Find RDA driver


def test_rda_test_presence():

    gdaltest.rda_drv = gdal.GetDriverByName('RDA')

    gdal.SetConfigOption('RDA_CACHE_DIR', '/vsimem/cache_dir')
    gdal.SetConfigOption('GDBX_CONFIG_FILE', '')

    if gdaltest.rda_drv is None:
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()

    
###############################################################################


def test_rda_bad_connection_string():

    if gdaltest.rda_drv is None:
        pytest.skip()

    # Bad json
    with gdaltest.error_handler():
        ds = gdal.Open('graph-id node-id')
    assert ds is None

    # Missing graph-id
    with gdaltest.error_handler():
        ds = gdal.Open('"graph-id node-id"')
    assert ds is None

    # Missing node-id
    with gdaltest.error_handler():
        ds = gdal.Open('{"graph-id": "node-id"}')
    assert ds is None

###############################################################################


def test_rda_missing_credentials():

    if gdaltest.rda_drv is None:
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

###############################################################################


def test_rda_failed_authentication():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    # invalid characters in env variable
    with gdaltest.config_options({'GBDX_AUTH_URL': '\\',
                                  'GBDX_RDA_API_URL': '\\',
                                  'GBDX_USERNAME': 'user_name',
                                  'GBDX_PASSWORD': 'password'}):
        with gdaltest.error_handler():
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # invalid URL
    with gdaltest.config_options({'GBDX_AUTH_URL': '/vsimem/auth_url',
                                  'GBDX_USERNAME': 'user_name',
                                  'GBDX_PASSWORD': 'password'}):
        with gdaltest.error_handler():
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # 404
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 404)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # Invalid json
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, 'invalid_json')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # No access token
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{}')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

###############################################################################


def test_rda_error_metadata():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    # 404
    handler = webserver.SequentialHandler()
    # caching of access_token not possible since no expires_in
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token"}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 404)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # 404 with payload
    handler = webserver.SequentialHandler()
    # caching of access_token not possible since no expires_in
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token"}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 404, {}, '{"error": "some error"}')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # invalid json
    handler = webserver.SequentialHandler()
    # caching possible now
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, 'invalid json')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # a lot of missing elements
    handler = webserver.SequentialHandler()
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, '{}')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # Bad dataType
    handler = webserver.SequentialHandler()
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, """{"imageMetadata":{
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 2,
        "numYTiles": 2,
        "tileXSize": 256,
        "tileYSize": 256,
        "numBands": 1,
        "dataType": "UNHANDLED",
        "imageHeight": 300,
        "imageWidth": 320,
        "minX": 2,
        "minY": 3,
        "maxX": 322,
        "maxY": 303,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1,
        "colorInterpretation": "FOO"
    }}""")
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # Huge numBands
    handler = webserver.SequentialHandler()
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, """{"imageMetadata":{
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 2,
        "numYTiles": 2,
        "tileXSize": 256,
        "tileYSize": 256,
        "numBands": 1000000000,
        "dataType": "BYTE",
        "imageHeight": 300,
        "imageWidth": 320,
        "minX": 2,
        "minY": 3,
        "maxX": 322,
        "maxY": 303,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1,
        "colorInterpretation": "FOO"
    }}""")
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

    # Invalid dataset dimensions
    handler = webserver.SequentialHandler()
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, """{"imageMetadata":{
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 2,
        "numYTiles": 2,
        "tileXSize": 256,
        "tileYSize": 256,
        "numBands": 1,
        "dataType": "BYTE",
        "imageHeight": 0,
        "imageWidth": 320,
        "minX": 2,
        "minY": 3,
        "maxX": 322,
        "maxY": 303,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1,
        "colorInterpretation": "FOO"
    }}""")
    with webserver.install_http_handler(handler):
        with gdaltest.config_options({'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
                                      'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
                                      'GBDX_USERNAME': 'user_name',
                                      'GBDX_PASSWORD': 'password'}):
            with gdaltest.error_handler():
                ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
    assert ds is None

###############################################################################


def test_rda_graph_nominal():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    metadata_json = {"imageMetadata": {
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 2,
        "numYTiles": 2,
        "tileXSize": 256,
        "tileYSize": 256,
        "numBands": 3,
        "dataType": "BYTE",
        "imageHeight": 300,
        "imageWidth": 320,
        "minX": 2,
        "minY": 3,
        "maxX": 322,
        "maxY": 303,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1,
        "colorInterpretation": "RGB",
        "acquisitionDate": "ACQUISITION_DATE",
        "sensorName": "SENSOR_NAME",
        "sensorPlatformName": "SENSOR_PLATFORM_NAME",
        "groundSampleDistanceMeters": 1.25,
        "cloudCover": 1.2,
        "sunAzimuth": 2.2,
        "sunElevation": 3.2,
        "satAzimuth": 4.2,
        "satElevation": 5.2
    }}
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))

    config_options = {
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
    assert ds is not None
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 320
    assert ds.RasterYSize == 300
    expected_md = {
        'SENSOR_NAME': 'SENSOR_NAME',
        'CLOUD_COVER': '1.2',
        'SUN_AZIMUTH': '2.2',
        'SAT_AZIMUTH': '4.2',
        'ACQUISITION_DATE': 'ACQUISITION_DATE',
        'SENSOR_PLATFORM_NAME': 'SENSOR_PLATFORM_NAME',
        'SUN_ELEVATION': '3.2',
        'GSD': '1.250 m',
        'SAT_ELEVATION': '5.2'
    }
    got_md = ds.GetMetadata()
    assert got_md == expected_md
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand

    assert ds.GetProjectionRef() == ''

    ds = None

    # File open
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            with open('tmp/rda_test1.dgrda', "w") as fd:
                fd.write('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
            ds = gdal.Open('tmp/rda_test1.dgrda')
    assert ds is not None
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 320
    assert ds.RasterYSize == 300
    expected_md = {
        'SENSOR_NAME': 'SENSOR_NAME',
        'CLOUD_COVER': '1.2',
        'SUN_AZIMUTH': '2.2',
        'SAT_AZIMUTH': '4.2',
        'ACQUISITION_DATE': 'ACQUISITION_DATE',
        'SENSOR_PLATFORM_NAME': 'SENSOR_PLATFORM_NAME',
        'SUN_ELEVATION': '3.2',
        'GSD': '1.250 m',
        'SAT_ELEVATION': '5.2'
    }
    got_md = ds.GetMetadata()
    assert got_md == expected_md
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand

    assert ds.GetProjectionRef() == ''

    try:
        os.remove('tmp/rda_test1.dgrda')
    except OSError:
        pass

    ds = None
    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
    assert ds is not None
    assert ds.RasterCount == 3

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json["imageGeoreferencing"] = {"spatialReferenceSystemCode": "EPSG:32631", "scaleX": 1.0, "scaleY": 2.0,
                                            "translateX": 123.0, "translateY": 456.0, "shearX": 0.0, "shearY": 0.0}

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        assert ds is not None
        assert ds.GetProjectionRef().find('32631') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (125.0, 1.0, 0.0, 462.0, 0.0, 2.0)

    ds = None

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
    got_gt = ds.GetGeoTransform()
    assert got_gt == (125.0, 1.0, 0.0, 462.0, 0.0, 2.0)

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # RPC should be ignored since there is a valid geotransform
    metadata_json["rpcSensorModel"] = {"spatialReferenceSystem": "EPSG:4326", "upperLeftCorner": {"x": 2.07724378, "y": 48.84065078}, "upperRightCorner": {"x": 2.31489579, "y": 48.84057427}, "lowerRightCorner": {"x": 2.31360304, "y": 48.69084146}, "lowerLeftCorner": {"x": 2.07783336, "y": 48.69170554}, "gsd": 5.641394178943586E-6, "postScaleFactorX": 1.0, "postScaleFactorY": 1.0, "lineOffset": 13313.0, "sampleOffset": 13775.0, "latOffset": 48.7657, "lonOffset": 2.1959, "heightOffset": 151.0, "lineScale": 13314.0, "sampleScale": 13776.0, "latScale": 0.075, "lonScale": 0.1203, "heightScale": 500.0, "lineNumCoefs": [0.003462388, -0.003319885, -1.004173, -5.694582E-4, 0.00138283, 1.973615E-6, 3.606842E-4, -8.287262E-4, -0.001348337, 3.399036E-7, -1.431479E-6, 1.058794E-6, 2.705906E-5, -9.732266E-8, -2.988015E-5, -1.20553E-4, -2.956054E-5, 2.817489E-7, 0.0, -1.663039E-8], "lineDenCoefs": [1.0, 0.001393466, 0.002175939, 3.615903E-4, 1.188453E-5, -5.55041E-7, -2.20758E-6, 1.688344E-5, -6.33621E-5, 2.911688E-5, -2.333141E-8, 1.367653E-7, 1.006995E-6, 8.290656E-8, -7.302754E-7, -1.959288E-4, 2.555922E-8, 2.074273E-8, 6.766787E-7, 2.106776E-8], "sampleNumCoefs": [-0.003627584, 1.015469, -0.001694738, -0.0107359, -0.001958667, 5.325142E-4, -4.003552E-4, 0.003666871, 2.035126E-4, -5.427884E-6, -1.176796E-6, -1.536131E-5, -6.907792E-5, -1.670626E-5, 7.908289E-5, -4.442762E-6, 1.143467E-7, 3.322555E-6, 8.624531E-7, 1.741671E-7], "sampleDenCoefs": [1.0, -3.707032E-5, 0.001978281, -5.804113E-4, -4.497994E-5, -2.659572E-6, 9.73096E-7, -1.250655E-5, 4.714011E-5, -1.697617E-5, -6.041848E-8, 0.0, 2.173017E-7, 5.608078E-8, -2.660194E-7, -2.020556E-7, -6.347383E-8, 1.321956E-8, -8.626535E-8, 1.908747E-8]}
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        assert ds is not None
        assert ds.GetMetadata('RPC') == {}

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # RPC should be ignored regardless if the profile is georectified_image
    temp_metadata = json.loads(json.dumps(metadata_json))
    del temp_metadata["imageGeoreferencing"]
    temp_metadata["imageMetadata"]["profileName"] = "georectified_image"
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(temp_metadata))

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        assert ds is not None
        assert ds.GetMetadata('RPC') == {}

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_00.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(255)
    tile_ds.GetRasterBand(2).Fill(250)
    tile_ds.GetRasterBand(3).Fill(245)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_00.tif', 'rb')
    tile00_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_00.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_01.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(240)
    tile_ds.GetRasterBand(2).Fill(235)
    tile_ds.GetRasterBand(3).Fill(230)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_01.tif', 'rb')
    tile01_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_01.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_10.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(225)
    tile_ds.GetRasterBand(2).Fill(220)
    tile_ds.GetRasterBand(3).Fill(215)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_10.tif', 'rb')
    tile10_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_10.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_11.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(210)
    tile_ds.GetRasterBand(2).Fill(205)
    tile_ds.GetRasterBand(3).Fill(200)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_11.tif', 'rb')
    tile11_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_11.tif')

    handler = webserver.SequentialHandler()
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, tile00_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/0.tif', 200, {}, tile01_data)
    handler.add('GET', '/rda_api/tile/foo/bar/0/1.tif', 200, {}, tile10_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/1.tif', 200, {}, tile11_data)
    with webserver.install_http_handler(handler):
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [54287, 50451, 21110]

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [54287, 50451, 21110]

    data = struct.unpack('B' * (320 * 300 * 3), ds.ReadRaster())
    assert data[0] == 255

    assert data[0 + 320 * 300] == 250

    assert data[0 + 320 * 300 * 2] == 245

    assert data[319] == 240

    assert data[320 * 299] == 225

    assert data[320 * 299 + 319] == 210

    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # Test Dataset::AdviseRead
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, tile00_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/0.tif', 200, {}, tile01_data)
    handler.add('GET', '/rda_api/tile/foo/bar/0/1.tif', 200, {}, tile10_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/1.tif', 200, {}, tile11_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        ds.AdviseRead(0, 0, 320, 300)
        ds.ReadRaster(0, 0, 1, 1)
    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # Test RasterBand::AdviseRead
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, tile00_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/0.tif', 200, {}, tile01_data)
    handler.add('GET', '/rda_api/tile/foo/bar/0/1.tif', 200, {}, tile10_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/1.tif', 200, {}, tile11_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        ds.GetRasterBand(1).AdviseRead(0, 0, 320, 300)
        ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # Test ReadBlock directly
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, tile00_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/0.tif', 200, {}, tile01_data)
    handler.add('GET', '/rda_api/tile/foo/bar/0/1.tif', 200, {}, tile10_data)
    handler.add('GET', '/rda_api/tile/foo/bar/1/1.tif', 200, {}, tile11_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        data = ds.GetRasterBand(1).ReadBlock(0, 0)

    data = struct.unpack('B' * (256 * 256), data)
    assert data[0] == 255
    assert data[256 * (256 - 2)] == 225


    # Try IReadBlock() when data for other bands is already cached
    ds.GetRasterBand(1).FlushCache()
    data = ds.GetRasterBand(1).ReadBlock(0, 0)
    data = struct.unpack('B' * (256 * 256), data)
    assert data[0] == 255

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # Test 502
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 502, {}, json.dumps({"Error": "502"}))

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
            with gdaltest.error_handler():
                data = ds.GetRasterBand(1).ReadBlock(0, 0)
                assert data is None

    gdal.RmdirRecursive('/vsimem/cache_dir')

###############################################################################


def test_rda_template_nominal():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    metadata_json = {"imageMetadata": {
        "imageId": "imageId",
        "profileName": "profileName",
        "version": "1.1",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 2,
        "numYTiles": 2,
        "tileXSize": 256,
        "tileYSize": 256,
        "numBands": 3,
        "dataType": "BYTE",
        "imageBoundsWGS84": "POLYGON ((38.40860454495335 -7.930519355111963, 38.58835646791296 -7.930735794243954, 38.5882467165898 -8.04033318299107, 38.40844687441741 -8.040113715005461, 38.40860454495335 -7.930519355111963))",
        "imageHeight": 300,
        "imageWidth": 320,
        "minX": 2,
        "minY": 3,
        "maxX": 322,
        "maxY": 303,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1
    }}
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/template/foo/metadata?nodeId=bar', 200, {}, json.dumps(metadata_json))

    config_options = {
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"template-id": "foo","params": [{"nodeId": "bar"}],"options":{"delete-on-close":false}}')
    assert ds is not None
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 320
    assert ds.RasterYSize == 300
    assert ds.GetProjectionRef() == ''

    ds = None

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"template-id": "foo","params": [{"nodeId": "bar"}],"options":{"delete-on-close":false}}')
    assert ds is not None
    assert ds.RasterCount == 3

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json["imageGeoreferencing"] = {"spatialReferenceSystemCode": "EPSG:32631", "scaleX": 1.0, "scaleY": 2.0,
                                            "translateX": 123.0, "translateY": 456.0, "shearX": 0.0, "shearY": 0.0}

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/template/foo/metadata?nodeId=bar', 200, {}, json.dumps(metadata_json))
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"template-id": "foo","params": [{"nodeId": "bar"}],"options":{"delete-on-close":false}}')
        assert ds is not None
        assert ds.GetProjectionRef().find('32631') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (125.0, 1.0, 0.0, 462.0, 0.0, 2.0)

    ds = None

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"template-id": "foo","params": [{"nodeId": "bar"}],"options":{"delete-on-close":false}}')
    got_gt = ds.GetGeoTransform()
    assert got_gt == (125.0, 1.0, 0.0, 462.0, 0.0, 2.0)

    gdal.RmdirRecursive('/vsimem/cache_dir')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_00.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(255)
    tile_ds.GetRasterBand(2).Fill(250)
    tile_ds.GetRasterBand(3).Fill(245)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_00.tif', 'rb')
    tile00_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_00.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_01.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(240)
    tile_ds.GetRasterBand(2).Fill(235)
    tile_ds.GetRasterBand(3).Fill(230)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_01.tif', 'rb')
    tile01_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_01.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_10.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(225)
    tile_ds.GetRasterBand(2).Fill(220)
    tile_ds.GetRasterBand(3).Fill(215)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_10.tif', 'rb')
    tile10_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_10.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_11.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(210)
    tile_ds.GetRasterBand(2).Fill(205)
    tile_ds.GetRasterBand(3).Fill(200)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_11.tif', 'rb')
    tile11_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_11.tif')

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/template/foo/metadata?nodeId=bar', 200, {}, json.dumps(metadata_json))
    handler.add('GET', '/rda_api/template/foo/tile/0/0?nodeId=bar', 200, {}, tile00_data)
    handler.add('GET', '/rda_api/template/foo/tile/1/0?nodeId=bar', 200, {}, tile01_data)
    handler.add('GET', '/rda_api/template/foo/tile/0/1?nodeId=bar', 200, {}, tile10_data)
    handler.add('GET', '/rda_api/template/foo/tile/1/1?nodeId=bar', 200, {}, tile11_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"template-id": "foo","params": [{"nodeId": "bar"}],"options":{"delete-on-close":false}}')

        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
        assert cs == [54287, 50451, 21110]

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"template-id": "foo","params": [{"nodeId": "bar"}],"options":{"delete-on-close":false}}')
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [54287, 50451, 21110]

    data = struct.unpack('B' * (320 * 300 * 3), ds.ReadRaster())
    assert data[0] == 255

    assert data[0 + 320 * 300] == 250

    assert data[0 + 320 * 300 * 2] == 245

    assert data[319] == 240

    assert data[320 * 299] == 225

    assert data[320 * 299 + 319] == 210

    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')

###############################################################################

def test_rda_template_image_reference_nominal():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    metadata_json = {"imageMetadata": {
        "imageId": "imageId",
        "profileName": "profileName",
        "version": "1.1",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 2,
        "numYTiles": 2,
        "tileXSize": 256,
        "tileYSize": 256,
        "numBands": 3,
        "dataType": "BYTE",
        "imageBoundsWGS84": "POLYGON ((38.40860454495335 -7.930519355111963, 38.58835646791296 -7.930735794243954, 38.5882467165898 -8.04033318299107, 38.40844687441741 -8.040113715005461, 38.40860454495335 -7.930519355111963))",
        "imageHeight": 300,
        "imageWidth": 320,
        "minX": 2,
        "minY": 3,
        "maxX": 322,
        "maxY": 303,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1
    }}
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/template/foo/metadata?nodeId=bar&p1=baz', 200, {}, json.dumps(metadata_json))

    config_options = {
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"templateId": "foo", "nodeId": "bar", "parameters": {"p1": "baz"},"options":{"delete-on-close":false}}')
    assert ds is not None
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 320
    assert ds.RasterYSize == 300
    assert ds.GetProjectionRef() == ''

    ds = None

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"templateId": "foo", "nodeId": "bar", "parameters": {"p1": "baz"},"options":{"delete-on-close":false}}')
    assert ds is not None
    assert ds.RasterCount == 3

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json["imageGeoreferencing"] = {"spatialReferenceSystemCode": "EPSG:32631", "scaleX": 1.0, "scaleY": 2.0,
                                            "translateX": 123.0, "translateY": 456.0, "shearX": 0.0, "shearY": 0.0}

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/template/foo/metadata?nodeId=bar&p1=baz', 200, {}, json.dumps(metadata_json))
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"templateId": "foo", "nodeId": "bar", "parameters": {"p1": "baz"},"options":{"delete-on-close":false}}')
        assert ds is not None
        assert ds.GetProjectionRef().find('32631') >= 0

    got_gt = ds.GetGeoTransform()
    assert got_gt == (125.0, 1.0, 0.0, 462.0, 0.0, 2.0)

    ds = None

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"templateId": "foo", "nodeId": "bar", "parameters": {"p1": "baz"},"options":{"delete-on-close":false}}')
    got_gt = ds.GetGeoTransform()
    assert got_gt == (125.0, 1.0, 0.0, 462.0, 0.0, 2.0)

    gdal.RmdirRecursive('/vsimem/cache_dir')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_00.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(255)
    tile_ds.GetRasterBand(2).Fill(250)
    tile_ds.GetRasterBand(3).Fill(245)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_00.tif', 'rb')
    tile00_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_00.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_01.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(240)
    tile_ds.GetRasterBand(2).Fill(235)
    tile_ds.GetRasterBand(3).Fill(230)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_01.tif', 'rb')
    tile01_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_01.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_10.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(225)
    tile_ds.GetRasterBand(2).Fill(220)
    tile_ds.GetRasterBand(3).Fill(215)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_10.tif', 'rb')
    tile10_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_10.tif')

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile_11.tif', 256, 256, 3)
    tile_ds.GetRasterBand(1).Fill(210)
    tile_ds.GetRasterBand(2).Fill(205)
    tile_ds.GetRasterBand(3).Fill(200)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile_11.tif', 'rb')
    tile11_data = gdal.VSIFReadL(1, 10000 + 256 * 256 * 3, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile_11.tif')

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/template/foo/metadata?nodeId=bar&p1=baz', 200, {}, json.dumps(metadata_json))
    handler.add('GET', '/rda_api/template/foo/tile/0/0?nodeId=bar&p1=baz', 200, {}, tile00_data)
    handler.add('GET', '/rda_api/template/foo/tile/1/0?nodeId=bar&p1=baz', 200, {}, tile01_data)
    handler.add('GET', '/rda_api/template/foo/tile/0/1?nodeId=bar&p1=baz', 200, {}, tile10_data)
    handler.add('GET', '/rda_api/template/foo/tile/1/1?nodeId=bar&p1=baz', 200, {}, tile11_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"templateId": "foo", "nodeId": "bar", "parameters": {"p1": "baz"},"options":{"delete-on-close":false}}')

        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
        assert cs == [54287, 50451, 21110]

    # Retry without any network setup to test caching
    with gdaltest.config_options(config_options):
        ds = gdal.Open('{"templateId": "foo", "nodeId": "bar", "parameters": {"p1": "baz"},"options":{"delete-on-close":false}}')
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [54287, 50451, 21110]

    data = struct.unpack('B' * (320 * 300 * 3), ds.ReadRaster())
    assert data[0] == 255

    assert data[0 + 320 * 300] == 250

    assert data[0 + 320 * 300 * 2] == 245

    assert data[319] == 240

    assert data[320 * 299] == 225

    assert data[320 * 299 + 319] == 210

    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')


def test_rda_read_gbdx_config():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json = """{"imageMetadata":{
        "imageId": "imageId",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 2,
        "numYTiles": 2,
        "tileXSize": 256,
        "tileYSize": 256,
        "numBands": 3,
        "dataType": "BYTE",
        "imageHeight": 300,
        "imageWidth": 320,
        "minX": 2,
        "minY": 3,
        "maxX": 322,
        "maxY": 303,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1,
        "colorInterpretation": "RGB",
    }}"""

    gdal.FileFromMemBuffer('/vsimem/.gbdx-config', """
[gbdx]
auth_url = 127.0.0.1:%d/auth_url
user_name = user_name
user_password = password
idaho_api_url = 127.0.0.1:%d/rda_api
""" % (gdaltest.webserver_port, gdaltest.webserver_port))

    config_options = {
        'GDBX_CONFIG_FILE': '/vsimem/.gbdx-config'
    }
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
    assert ds is not None

    gdal.Unlink('/vsimem/.gbdx-config')

###############################################################################


def test_rda_download_queue():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json = """{"imageMetadata":{
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 5,
        "numYTiles": 4,
        "tileXSize": 1,
        "tileYSize": 1,
        "numBands": 1,
        "dataType": "BYTE",
        "imageHeight": 5,
        "imageWidth": 4,
        "minX": 0,
        "minY": 0,
        "maxX": 4,
        "maxY": 3,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 4,
        "maxTileY": 3,
        "colorInterpretation": "PAN",
    }}"""

    config_options = {
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile.tif', 1, 1)
    tile_ds.GetRasterBand(1).Fill(255)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile.tif', 'rb')
    tile_data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile.tif')

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    for y in range(5):
        for x in range(4):
            handler.add_unordered('GET', '/rda_api/tile/foo/bar/%d/%d.tif' % (x, y), 200, {}, tile_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            # We need at least (width=5) <= MAXCONNECT so that AdviseRead(all_raster) is honoured
            ds = gdal.OpenEx('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}', open_options=['MAXCONNECT=8'])
        ds.AdviseRead(0, 0, 4, 5)
        ds.ReadRaster(0, 0, 4, 3)
        ref_data = ds.ReadRaster(0, 1, 4, 2)

    # Test that we have the last 8 tiles cached
    ds.FlushCache()
    gdal.RmdirRecursive('/vsimem/cache_dir')

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        data = ds.ReadRaster(0, 1, 4, 2)
    assert data == ref_data

###############################################################################


def test_rda_rpc():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # No RPC
    metadata_json = {
        "imageMetadata": {
            "imageId": "imageId",
            "profileName": "profileName",
            "nativeTileFileFormat": "TIF",
            "tileXOffset": 0,
            "tileYOffset": 0,
            "numXTiles": 5,
            "numYTiles": 4,
            "tileXSize": 1,
            "tileYSize": 1,
            "numBands": 1,
            "dataType": "BYTE",
            "imageHeight": 5,
            "imageWidth": 4,
            "minX": 0,
            "minY": 0,
            "maxX": 4,
            "maxY": 3,
            "minTileX": 0,
            "minTileY": 0,
            "maxTileX": 4,
            "maxTileY": 3,
            "colorInterpretation": "PAN"
        }
    }

    config_options = {
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        md = ds.GetMetadata('RPC')
    assert md == {}

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # Invalid RPC
    metadata_json["rpcSensorModel"] = {"spatialReferenceSystem": "EPSG:4326", "upperLeftCorner": {"x": 2.07724378, "y": 48.84065078}, "upperRightCorner": {"x": 2.31489579, "y": 48.84057427}, "lowerRightCorner": {"x": 2.31360304, "y": 48.69084146}, "lowerLeftCorner": {"x": 2.07783336, "y": 48.69170554}, "gsd": 5.641394178943586E-6, "postScaleFactorX": 2.0, "postScaleFactorY": 2.0, "lineOffset": 13313.0, "sampleOffset": 13775.0, "latOffset": 48.7657, "lonOffset": 2.1959, "heightOffset": 151.0, "lineScale": 13314.0, "sampleScale": 13776.0, "latScale": 0.075, "lonScale": 0.1203, "heightScale": 500.0, "lineNumCoefs": [], "lineDenCoefs": [], "sampleNumCoefs": [], "sampleDenCoefs": []}
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))

    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        with gdaltest.error_handler():
            md = ds.GetMetadata('RPC')
    assert md == {}

    gdal.RmdirRecursive('/vsimem/cache_dir')

    # Good RPC
    metadata_json["rpcSensorModel"] = {"spatialReferenceSystem": "EPSG:4326", "upperLeftCorner": {"x": 2.07724378, "y": 48.84065078}, "upperRightCorner": {"x": 2.31489579, "y": 48.84057427}, "lowerRightCorner": {"x": 2.31360304, "y": 48.69084146}, "lowerLeftCorner": {"x": 2.07783336, "y": 48.69170554}, "gsd": 5.641394178943586E-6, "postScaleFactorX": 1.0, "postScaleFactorY": 1.0, "lineOffset": 13313.0, "sampleOffset": 13775.0, "latOffset": 48.7657, "lonOffset": 2.1959, "heightOffset": 151.0, "lineScale": 13314.0, "sampleScale": 13776.0, "latScale": 0.075, "lonScale": 0.1203, "heightScale": 500.0, "lineNumCoefs": [0.003462388, -0.003319885, -1.004173, -5.694582E-4, 0.00138283, 1.973615E-6, 3.606842E-4, -8.287262E-4, -0.001348337, 3.399036E-7, -1.431479E-6, 1.058794E-6, 2.705906E-5, -9.732266E-8, -2.988015E-5, -1.20553E-4, -2.956054E-5, 2.817489E-7, 0.0, -1.663039E-8], "lineDenCoefs": [1.0, 0.001393466, 0.002175939, 3.615903E-4, 1.188453E-5, -5.55041E-7, -2.20758E-6, 1.688344E-5, -6.33621E-5, 2.911688E-5, -2.333141E-8, 1.367653E-7, 1.006995E-6, 8.290656E-8, -7.302754E-7, -1.959288E-4, 2.555922E-8, 2.074273E-8, 6.766787E-7, 2.106776E-8], "sampleNumCoefs": [-0.003627584, 1.015469, -0.001694738, -0.0107359, -0.001958667, 5.325142E-4, -4.003552E-4, 0.003666871, 2.035126E-4, -5.427884E-6, -1.176796E-6, -1.536131E-5, -6.907792E-5, -1.670626E-5, 7.908289E-5, -4.442762E-6, 1.143467E-7, 3.322555E-6, 8.624531E-7, 1.741671E-7], "sampleDenCoefs": [1.0, -3.707032E-5, 0.001978281, -5.804113E-4, -4.497994E-5, -2.659572E-6, 9.73096E-7, -1.250655E-5, 4.714011E-5, -1.697617E-5, -6.041848E-8, 0.0, 2.173017E-7, 5.608078E-8, -2.660194E-7, -2.020556E-7, -6.347383E-8, 1.321956E-8, -8.626535E-8, 1.908747E-8]}
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        md = ds.GetMetadata('RPC')
    expected_md = {'ERR_BIAS': "0", 'ERR_RAND': "0", 'HEIGHT_OFF': '151', 'SAMP_OFF': '13775', 'LINE_NUM_COEFF': '0.00346238799999999984 -0.00331988499999999985 -1.00417299999999998 -0.000569458199999999968 0.00138283000000000004 1.97361499999999982e-06 0.000360684200000000019 -0.000828726199999999967 -0.00134833700000000002 3.39903600000000002e-07 -1.43147900000000003e-06 1.05879399999999997e-06 2.70590600000000004e-05 -9.73226600000000015e-08 -2.98801499999999988e-05 -0.000120552999999999996 -2.95605399999999987e-05 2.81748900000000025e-07 0 -1.66303899999999993e-08', 'LONG_OFF': '2.19589999999999996', 'MIN_LAT': '48.6908414600000015', 'MAX_LONG': '2.31489579000000001', 'LINE_SCALE': '13314', 'SAMP_NUM_COEFF': '-0.0036275840000000001 1.01546899999999996 -0.0016947380000000001 -0.0107358999999999996 -0.00195866699999999987 0.000532514199999999951 -0.000400355199999999985 0.00366687099999999993 0.0002035126 -5.42788399999999972e-06 -1.176796e-06 -1.53613099999999988e-05 -6.90779199999999995e-05 -1.67062600000000002e-05 7.90828899999999939e-05 -4.44276200000000027e-06 1.14346699999999995e-07 3.32255499999999999e-06 8.62453100000000019e-07 1.74167099999999993e-07', 'LONG_SCALE': '0.120300000000000004', 'SAMP_DEN_COEFF': '1 -3.70703199999999978e-05 0.00197828100000000014 -0.000580411299999999963 -4.49799400000000022e-05 -2.65957199999999999e-06 9.73095999999999941e-07 -1.25065499999999997e-05 4.71401100000000002e-05 -1.69761699999999998e-05 -6.04184799999999996e-08 0 2.17301699999999993e-07 5.60807800000000004e-08 -2.6601939999999999e-07 -2.02055599999999998e-07 -6.34738299999999952e-08 1.32195600000000006e-08 -8.62653499999999976e-08 1.90874699999999984e-08', 'MIN_LONG': '2.07724377999999987', 'SAMP_SCALE': '13776', 'MAX_LAT': '48.8406507799999972', 'LAT_SCALE': '0.0749999999999999972', 'LAT_OFF': '48.7657000000000025', 'LINE_OFF': '13313', 'LINE_DEN_COEFF': '1 0.00139346600000000011 0.002175939 0.000361590300000000006 1.18845299999999999e-05 -5.55040999999999947e-07 -2.20758000000000013e-06 1.68834400000000011e-05 -6.33621000000000011e-05 2.9116880000000001e-05 -2.33314100000000006e-08 1.36765299999999997e-07 1.00699499999999992e-06 8.29065599999999946e-08 -7.30275400000000041e-07 -0.0001959288 2.55592200000000014e-08 2.07427299999999992e-08 6.76678700000000044e-07 2.106776e-08', 'HEIGHT_SCALE': '500'}
    for key in expected_md:
        if not key.endswith('_COEFF'):
            assert float(expected_md[key]) == pytest.approx(float(md[key]), abs=1e-8 * abs(float(expected_md[key])))
        else:
            expected_vals = [float(v) for v in expected_md[key].split(' ')]
            got_vals = [float(v) for v in md[key].split(' ')]
            for x in range(20):
                assert expected_vals[x] == pytest.approx(got_vals[x], abs=1e-8 * abs(expected_vals[x])), (key, md)
    ds = None
    gdal.RmdirRecursive('/vsimem/cache_dir')

    # Good RPC with RPC error coeffs
    metadata_json["rpcSensorModel"] = {"spatialReferenceSystem": "EPSG:4326", "upperLeftCorner": {"x": 2.07724378, "y": 48.84065078}, "upperRightCorner": {"x": 2.31489579, "y": 48.84057427}, "lowerRightCorner": {"x": 2.31360304, "y": 48.69084146}, "lowerLeftCorner": {"x": 2.07783336, "y": 48.69170554}, "gsd": 5.641394178943586E-6, "postScaleFactorX": 1.0, "postScaleFactorY": 1.0, "errBias": 1.0, "errRand": 2.0, "lineOffset": 13313.0, "sampleOffset": 13775.0, "latOffset": 48.7657, "lonOffset": 2.1959, "heightOffset": 151.0, "lineScale": 13314.0, "sampleScale": 13776.0, "latScale": 0.075, "lonScale": 0.1203, "heightScale": 500.0, "lineNumCoefs": [0.003462388, -0.003319885, -1.004173, -5.694582E-4, 0.00138283, 1.973615E-6, 3.606842E-4, -8.287262E-4, -0.001348337, 3.399036E-7, -1.431479E-6, 1.058794E-6, 2.705906E-5, -9.732266E-8, -2.988015E-5, -1.20553E-4, -2.956054E-5, 2.817489E-7, 0.0, -1.663039E-8], "lineDenCoefs": [1.0, 0.001393466, 0.002175939, 3.615903E-4, 1.188453E-5, -5.55041E-7, -2.20758E-6, 1.688344E-5, -6.33621E-5, 2.911688E-5, -2.333141E-8, 1.367653E-7, 1.006995E-6, 8.290656E-8, -7.302754E-7, -1.959288E-4, 2.555922E-8, 2.074273E-8, 6.766787E-7, 2.106776E-8], "sampleNumCoefs": [-0.003627584, 1.015469, -0.001694738, -0.0107359, -0.001958667, 5.325142E-4, -4.003552E-4, 0.003666871, 2.035126E-4, -5.427884E-6, -1.176796E-6, -1.536131E-5, -6.907792E-5, -1.670626E-5, 7.908289E-5, -4.442762E-6, 1.143467E-7, 3.322555E-6, 8.624531E-7, 1.741671E-7], "sampleDenCoefs": [1.0, -3.707032E-5, 0.001978281, -5.804113E-4, -4.497994E-5, -2.659572E-6, 9.73096E-7, -1.250655E-5, 4.714011E-5, -1.697617E-5, -6.041848E-8, 0.0, 2.173017E-7, 5.608078E-8, -2.660194E-7, -2.020556E-7, -6.347383E-8, 1.321956E-8, -8.626535E-8, 1.908747E-8]}
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 3600}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, json.dumps(metadata_json))
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')
        md = ds.GetMetadata('RPC')
    expected_md = {'ERR_BIAS': "1.0", 'ERR_RAND': "2.0", 'HEIGHT_OFF': '151', 'SAMP_OFF': '13775', 'LINE_NUM_COEFF': '0.00346238799999999984 -0.00331988499999999985 -1.00417299999999998 -0.000569458199999999968 0.00138283000000000004 1.97361499999999982e-06 0.000360684200000000019 -0.000828726199999999967 -0.00134833700000000002 3.39903600000000002e-07 -1.43147900000000003e-06 1.05879399999999997e-06 2.70590600000000004e-05 -9.73226600000000015e-08 -2.98801499999999988e-05 -0.000120552999999999996 -2.95605399999999987e-05 2.81748900000000025e-07 0 -1.66303899999999993e-08', 'LONG_OFF': '2.19589999999999996', 'MIN_LAT': '48.6908414600000015', 'MAX_LONG': '2.31489579000000001', 'LINE_SCALE': '13314', 'SAMP_NUM_COEFF': '-0.0036275840000000001 1.01546899999999996 -0.0016947380000000001 -0.0107358999999999996 -0.00195866699999999987 0.000532514199999999951 -0.000400355199999999985 0.00366687099999999993 0.0002035126 -5.42788399999999972e-06 -1.176796e-06 -1.53613099999999988e-05 -6.90779199999999995e-05 -1.67062600000000002e-05 7.90828899999999939e-05 -4.44276200000000027e-06 1.14346699999999995e-07 3.32255499999999999e-06 8.62453100000000019e-07 1.74167099999999993e-07', 'LONG_SCALE': '0.120300000000000004', 'SAMP_DEN_COEFF': '1 -3.70703199999999978e-05 0.00197828100000000014 -0.000580411299999999963 -4.49799400000000022e-05 -2.65957199999999999e-06 9.73095999999999941e-07 -1.25065499999999997e-05 4.71401100000000002e-05 -1.69761699999999998e-05 -6.04184799999999996e-08 0 2.17301699999999993e-07 5.60807800000000004e-08 -2.6601939999999999e-07 -2.02055599999999998e-07 -6.34738299999999952e-08 1.32195600000000006e-08 -8.62653499999999976e-08 1.90874699999999984e-08', 'MIN_LONG': '2.07724377999999987', 'SAMP_SCALE': '13776', 'MAX_LAT': '48.8406507799999972', 'LAT_SCALE': '0.0749999999999999972', 'LAT_OFF': '48.7657000000000025', 'LINE_OFF': '13313', 'LINE_DEN_COEFF': '1 0.00139346600000000011 0.002175939 0.000361590300000000006 1.18845299999999999e-05 -5.55040999999999947e-07 -2.20758000000000013e-06 1.68834400000000011e-05 -6.33621000000000011e-05 2.9116880000000001e-05 -2.33314100000000006e-08 1.36765299999999997e-07 1.00699499999999992e-06 8.29065599999999946e-08 -7.30275400000000041e-07 -0.0001959288 2.55592200000000014e-08 2.07427299999999992e-08 6.76678700000000044e-07 2.106776e-08', 'HEIGHT_SCALE': '500'}
    for key in expected_md:
        if not key.endswith('_COEFF'):
            assert float(expected_md[key]) == pytest.approx(float(md[key]), abs=1e-8 * abs(float(expected_md[key])))
        else:
            expected_vals = [float(v) for v in expected_md[key].split(' ')]
            got_vals = [float(v) for v in md[key].split(' ')]
            for x in range(20):
                assert expected_vals[x] == pytest.approx(got_vals[x], abs=1e-8 * abs(expected_vals[x])), (key, md)
    ds = None
    gdal.RmdirRecursive('/vsimem/cache_dir')

###############################################################################


def test_rda_real_cache_dir():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    home = gdal.GetConfigOption('HOME', gdal.GetConfigOption('USERPROFILE', None))
    if home is None:
        pytest.skip()

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json = """{"imageMetadata":{
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 5,
        "numYTiles": 4,
        "tileXSize": 1,
        "tileYSize": 1,
        "numBands": 1,
        "dataType": "BYTE",
        "imageHeight": 5,
        "imageWidth": 4,
        "minX": 0,
        "minY": 0,
        "maxX": 4,
        "maxY": 3,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 4,
        "maxTileY": 3,
        "colorInterpretation": "PAN",
    }}"""

    config_options = {
        'RDA_CACHE_DIR': '',
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }

    cached_file = os.path.join(home, '.gdal', 'rda_cache', 'foo', 'bar', 'metadata.json')
    if os.path.exists(cached_file):
        gdal.RmdirRecursive(os.path.join(home, '.gdal', 'rda_cache', 'foo'))

    handler = webserver.SequentialHandler()
    if not os.path.exists(os.path.join(home, '.gdal', 'rda_cache', 'authorization.json')):
        handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token" }')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            gdal.Open('{"graph-id":"foo","node-id":"bar","options":{"delete-on-close":false}}')

    assert os.path.exists(cached_file)
    gdal.RmdirRecursive(os.path.join(home, '.gdal', 'rda_cache', 'foo'))

    handler = webserver.SequentialHandler()
    if not os.path.exists(os.path.join(home, '.gdal', 'rda_cache', 'authorization.json')):
        handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token" }')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            gdal.Open('{"graph-id":"foo","node-id":"bar"}')

    assert not os.path.exists(os.path.join(home, '.gdal', 'rda_cache', 'foo'))

    # 493 = 0755
    gdal.MkdirRecursive(os.path.join(home, '.gdal', 'rda_cache', 'foo', 'baz'), 493)

    handler = webserver.SequentialHandler()
    if not os.path.exists(os.path.join(home, '.gdal', 'rda_cache', 'authorization.json')):
        handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token" }')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            gdal.Open('{"graph-id":"foo","node-id":"bar"}')

    assert os.path.exists(os.path.join(home, '.gdal', 'rda_cache', 'foo', 'baz'))

    gdal.RmdirRecursive(os.path.join(home, '.gdal', 'rda_cache', 'foo'))

###############################################################################


def test_rda_real_expired_authentication():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json = """{"imageMetadata":{
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 5,
        "numYTiles": 4,
        "tileXSize": 1,
        "tileYSize": 1,
        "numBands": 1,
        "dataType": "BYTE",
        "imageHeight": 5,
        "imageWidth": 4,
        "minX": 0,
        "minY": 0,
        "maxX": 4,
        "maxY": 3,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 4,
        "maxTileY": 3,
        "colorInterpretation": "PAN",
    }}"""

    config_options = {
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }

    handler = webserver.SequentialHandler()
    # As we have a 60 second security margin, expires_in=1 will already have expired
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token", "expires_in": 1 }')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            gdal.Open('{"graph-id":"foo","node-id":"bar"}')

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token" }')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            gdal.Open('{"graph-id":"foo","node-id":"bar"}')

    
###############################################################################


def test_rda_bad_tile():

    if gdaltest.rda_drv is None:
        pytest.skip()
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.RmdirRecursive('/vsimem/cache_dir')

    metadata_json = """{"imageMetadata":{
        "imageId": "imageId",
        "profileName": "profileName",
        "nativeTileFileFormat": "TIF",
        "tileXOffset": 0,
        "tileYOffset": 0,
        "numXTiles": 1,
        "numYTiles": 1,
        "tileXSize": 1,
        "tileYSize": 1,
        "numBands": 1,
        "dataType": "BYTE",
        "imageHeight": 1,
        "imageWidth": 1,
        "minX": 0,
        "minY": 0,
        "maxX": 1,
        "maxY": 1,
        "minTileX": 0,
        "minTileY": 0,
        "maxTileX": 1,
        "maxTileY": 1,
        "colorInterpretation": "PAN",
    }}"""

    config_options = {
        'GBDX_AUTH_URL': '127.0.0.1:%d/auth_url' % gdaltest.webserver_port,
        'GBDX_RDA_API_URL': '127.0.0.1:%d/rda_api' % gdaltest.webserver_port,
        'GBDX_USERNAME': 'user_name',
        'GBDX_PASSWORD': 'password'
    }

    tile_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tile.tif', 2, 1)
    tile_ds.GetRasterBand(1).Fill(255)
    tile_ds = None
    f = gdal.VSIFOpenL('/vsimem/tile.tif', 'rb')
    tile_data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/tile.tif')

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token"}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, tile_data)
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, tile_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
        with gdaltest.error_handler():
            data = ds.ReadRaster()
        assert data is None
    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token"}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, tile_data)
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
        with gdaltest.error_handler():
            data = ds.GetRasterBand(1).ReadBlock(0, 0)
        assert data is None
    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')
    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token"}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, 'foo')
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
        with gdaltest.error_handler():
            data = ds.ReadRaster()
        assert data is None
    ds = None

    gdal.RmdirRecursive('/vsimem/cache_dir')

    handler = webserver.SequentialHandler()
    handler.add('POST', '/auth_url', 200, {}, '{"access_token": "token"}')
    handler.add('GET', '/rda_api/metadata/foo/bar/metadata.json', 200, {}, metadata_json)
    handler.add('GET', '/rda_api/tile/foo/bar/0/0.tif', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(config_options):
            ds = gdal.Open('{"graph-id":"foo","node-id":"bar"}')
        with gdaltest.error_handler():
            data = ds.GetRasterBand(1).ReadBlock(0, 0)
        assert data is None
    ds = None

###############################################################################
#


def test_rda_cleanup():

    if gdaltest.rda_drv is None:
        pytest.skip()

    if gdaltest.webserver_port != 0:
        webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    gdal.RmdirRecursive('/vsimem/cache_dir')




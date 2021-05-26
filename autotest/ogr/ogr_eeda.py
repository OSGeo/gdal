#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Earth Engine Data API driver test suite.
# Author:   Even Rouault, even dot rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2017, Planet Labs
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

import json


from osgeo import gdal, ogr

import gdaltest
import pytest


pytestmark = pytest.mark.require_driver('EEDA')


###############################################################################

@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')
    yield
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM',  None)
    gdal.SetConfigOption('EEDA_BEARER', None)
    gdal.SetConfigOption('EEDA_URL', None)
    gdal.SetConfigOption('EEDA_PRIVATE_KEY', None)
    gdal.SetConfigOption('EEDA_CLIENT_EMAIL', None)
    gdal.SetConfigOption('GO2A_AUD', None)
    gdal.SetConfigOption('GOA2_NOW', None)

    gdal.Unlink('/vsimem/ee/')
    gdal.Unlink('/vsimem/ee/projects/earthengine-public/assets/collection:listImages?pageSize=1')
    gdal.Unlink('/vsimem/ee/projects/earthengine-public/assets/collection:listImages?pageToken=myToken')
    gdal.Unlink('/vsimem/ee/projects/earthengine-public/assets/collection:listImages?filter=raw%5Ffilter')


###############################################################################
# Nominal case


def test_eeda_2():

    gdal.FileFromMemBuffer('/vsimem/ee/projects/earthengine-public/assets/collection:listImages?pageSize=1', json.dumps({
        'images': [
            {
                'properties':
                {
                    'string_field': 'bar',
                    'int_field': 1,
                    'int64_field': 123456789012,
                    'double_field': 1.23
                }
            }
        ]
    }))

    # To please the unregistering of the persistent connection
    gdal.FileFromMemBuffer('/vsimem/ee/', '')

    gdal.SetConfigOption('EEDA_BEARER', 'mybearer')
    gdal.SetConfigOption('EEDA_URL', '/vsimem/ee/')
    ds = ogr.Open('EEDA:collection')
    gdal.SetConfigOption('EEDA_URL', None)

    lyr = ds.GetLayer(0)

    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1

    assert lyr.TestCapability('foo') == 0

    assert lyr.GetLayerDefn().GetFieldCount() == 8 + 7 + 4

    assert lyr.GetExtent() == (-180.0, 180.0, -90.0, 90.0)

    assert lyr.GetFeatureCount() == -1

    gdal.FileFromMemBuffer('/vsimem/ee/projects/earthengine-public/assets/collection:listImages', json.dumps({
        'images': [
            {
                'name': 'projects/earthengine-public/assets/collection/first_feature',
                'id': 'collection/first_feature',
                'updateTime': '2017-01-04T12:34:56.789Z',
                'startTime': '2017-01-02T12:34:56.789Z',
                'endTime': '2017-01-03T12:34:56.789Z',
                'sizeBytes': 1,
                'geometry': {'type': 'Polygon', 'coordinates': [[[2, 49], [2.1, 49], [2.1, 49.1], [2, 49.1], [2, 49]]]},
                'properties':
                {
                    'string_field': 'bar',
                    'int_field': 1,
                    'int64_field': 123456789012,
                    'double_field': 1.23,
                    'another_prop': 3
                },
                'bands': [
                    {
                        "id": "B1",
                        "dataType": {
                            "precision": "INT",
                            "range": {
                                "max": 255
                            }
                        },
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60
                            },
                            "dimensions": {
                                "width": 1830,
                                "height": 1831
                            }
                        }
                    }
                ]
            },
            {
                'name': 'projects/earthengine-public/assets/collection/second_feature'
            }
        ],
        'nextPageToken': 'myToken'
    }))

    f = lyr.GetNextFeature()
    if f.GetField('name') != 'projects/earthengine-public/assets/collection/first_feature' or \
       f.GetField('id') != 'collection/first_feature' or \
       f.GetField('gdal_dataset') != 'EEDAI:projects/earthengine-public/assets/collection/first_feature' or \
       f.GetField('updateTime') != '2017/01/04 12:34:56.789+00' or \
       f.GetField('startTime') != '2017/01/02 12:34:56.789+00' or \
       f.GetField('endTime') != '2017/01/03 12:34:56.789+00' or \
       f.GetField('sizeBytes') != 1 or \
       f.GetField('band_count') != 1 or \
       f.GetField('band_max_width') != 1830 or \
       f.GetField('band_max_height') != 1831 or \
       f.GetField('band_min_pixel_size') != 60 or \
       f.GetField('band_upper_left_x') != 499980 or \
       f.GetField('band_upper_left_y') != 4200000 or \
       f.GetField('band_crs') != 'EPSG:32610' or \
       f.GetField('string_field') != 'bar' or \
       f.GetField('int_field') != 1 or \
       f.GetField('int64_field') != 123456789012 or \
       f.GetField('double_field') != 1.23 or \
       f.GetField('other_properties') != '{ "another_prop": 3 }' or \
       f.GetGeometryRef().ExportToWkt() != 'MULTIPOLYGON (((2 49,2.1 49.0,2.1 49.1,2.0 49.1,2 49)))':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f.GetField('name') != 'projects/earthengine-public/assets/collection/second_feature':
        f.DumpReadable()
        pytest.fail()

    gdal.FileFromMemBuffer('/vsimem/ee/projects/earthengine-public/assets/collection:listImages?pageToken=myToken', json.dumps({
        'images': [
            {
                'name': 'projects/earthengine-public/assets/collection/third_feature'
            }
        ]
    }))

    f = lyr.GetNextFeature()
    if f.GetField('name') != 'projects/earthengine-public/assets/collection/third_feature':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    assert f is None

    lyr.ResetReading()

    f = lyr.GetNextFeature()
    if f.GetField('name') != 'projects/earthengine-public/assets/collection/first_feature':
        f.DumpReadable()
        pytest.fail()

    lyr.SetAttributeFilter('EEDA:raw_filter')

    gdal.FileFromMemBuffer('/vsimem/ee/projects/earthengine-public/assets/collection:listImages?filter=raw%5Ffilter', json.dumps({
        'images': [
            {
                'name': 'projects/earthengine-public/assets/collection/raw_filter'
            }
        ]
    }))

    f = lyr.GetNextFeature()
    assert f.GetField('name') == 'projects/earthengine-public/assets/collection/raw_filter'

    lyr.SetAttributeFilter(None)
    lyr.SetAttributeFilter("startTime >= '1980-01-01T00:00:00Z' AND " +
                           "string_field = 'bar' AND " +
                           "int_field > 0 AND " +
                           "int_field < 2 AND " +
                           "int64_field >= 0 AND " +
                           "int64_field <= 9999999999999 AND " +
                           "double_field != 3.5 AND " +
                           "string_field IN ('bar', 'baz') AND " +
                           "NOT( int_field IN (0) OR double_field IN (3.5) )")

    tmpfile = '/vsimem/ee/projects/earthengine-public/assets/collection:listImages?region=%7B%20%22type%22%3A%20%22Polygon%22%2C%20%22coordinates%22%3A%20%5B%20%5B%20%5B%20%2D180%2E0%2C%20%2D90%2E0%20%5D%2C%20%5B%20%2D180%2E0%2C%2090%2E0%20%5D%2C%20%5B%20180%2E0%2C%2090%2E0%20%5D%2C%20%5B%20180%2E0%2C%20%2D90%2E0%20%5D%2C%20%5B%20%2D180%2E0%2C%20%2D90%2E0%20%5D%20%5D%20%5D%20%7D&filter=%28%28%28string%5Ffield%20%3D%20%22bar%22%20AND%20%28int%5Ffield%20%3E%200%20AND%20int%5Ffield%20%3C%202%29%29%20AND%20%28%28int64%5Ffield%20%3E%3D%200%20AND%20int64%5Ffield%20%3C%3D%209999999999999%29%20AND%20%28double%5Ffield%20%21%3D%203%2E5%20AND%20string%5Ffield%20%3D%20%22bar%22%20OR%20string%5Ffield%20%3D%20%22baz%22%29%29%29%20AND%20%28NOT%20%28int%5Ffield%20%3D%200%20OR%20double%5Ffield%20%3D%203%2E5%29%29%29&startTime=1980%2D01%2D01T00%3A00%3A00Z'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/earthengine-public/assets/collection/filtered_feature',
                    'updateTime': '2017-01-03T12:34:56.789Z',
                    'startTime': '2017-01-02T12:34:56.789Z',
                    'sizeBytes': 1,
                    'geometry': {'type': 'Polygon', 'coordinates': [[[2, 49], [2.1, 49], [2.1, 49.1], [2, 49.1], [2, 49]]]},
                    'properties':
                    {
                        'string_field': 'bar',
                        'int_field': 1,
                        'int64_field': 123456789012,
                        'double_field': 1.23,
                        'another_prop': 3
                    }
                },
                {
                    'name': 'projects/earthengine-public/assets/collection/second_feature'
                }
            ]
        })):

        lyr.SetSpatialFilterRect(-180, -90, 180, 90)

        f = lyr.GetNextFeature()

    assert f.GetField('name') == 'projects/earthengine-public/assets/collection/filtered_feature'

    lyr.SetSpatialFilter(None)

    # Test time equality with second granularity
    lyr.SetAttributeFilter("startTime >= '1980-01-01T00:00:00Z' AND endTime <= '1980-01-02T23:59:59Z'")

    tmpfile = '/vsimem/ee/projects/earthengine-public/assets/collection:listImages?startTime=1980%2D01%2D01T00%3A00%3A00Z&endTime=1980%2D01%2D02T23%3A59%3A59Z'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/earthengine-public/assets/collection/filtered_feature',
                    'startTime': '1980-01-01T00:00:00Z',
                    'endTime': '1980-01-02T23:59:59Z'
                },
                {
                    'name': 'projects/earthengine-public/assets/collection/second_feature'
                }
            ]
        })):

        f = lyr.GetNextFeature()

    assert f.GetField('name') == 'projects/earthengine-public/assets/collection/filtered_feature'

    # Test time equality with day granularity
    lyr.SetAttributeFilter("startTime = '1980-01-01' AND endTime = '1980-01-02'")

    tmpfile = '/vsimem/ee/projects/earthengine-public/assets/collection:listImages?startTime=1980%2D01%2D01T00%3A00%3A00Z&endTime=1980%2D01%2D02T23%3A59%3A59Z'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/earthengine-public/assets/collection/filtered_feature',
                    'startTime': '1980-01-01T12:00:00Z',
                    'endTime': '1980-01-02T23:59:59Z'
                },
                {
                    'name': 'projects/earthengine-public/assets/collection/second_feature'
                }
            ]
        })):

        f = lyr.GetNextFeature()

    assert f.GetField('name') == 'projects/earthengine-public/assets/collection/filtered_feature'

    ds = None

    gdal.SetConfigOption('EEDA_BEARER', None)

###############################################################################
# Nominal case where collection is in eedaconf.json


def test_eeda_3():

    gdal.SetConfigOption('EEDA_BEARER', 'mybearer')
    gdal.SetConfigOption('EEDA_URL', '/vsimem/ee/')
    ds = ogr.Open('EEDA:##example_collection/example_subcollection')
    gdal.SetConfigOption('EEDA_URL', None)

    lyr = ds.GetLayer(0)

    assert lyr.GetLayerDefn().GetFieldCount() == 8 + 7 + 4
    ds = None

    gdal.SetConfigOption('EEDA_BEARER', None)

###############################################################################
# Test that name and id variants are handled correctly.


def test_eeda_4():

    gdal.SetConfigOption('EEDA_BEARER', 'mybearer')
    gdal.SetConfigOption('EEDA_URL', '/vsimem/ee/')

    # User asset ID ("users/**").
    tmpfile = '/vsimem/ee/projects/earthengine-legacy/assets/users/foo:listImages?pageSize=1'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/earthengine-legacy/assets/users/foo/bar'
                }
            ]
        })):
        assert ogr.Open('EEDA:users/foo').GetLayer(0)

    # Project asset ID ("projects/**").
    tmpfile = '/vsimem/ee/projects/earthengine-legacy/assets/projects/foo:listImages?pageSize=1'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/earthengine-legacy/assets/projects/foo/bar'
                }
            ]
        })):
        ds = ogr.Open('EEDA:projects/foo')
        assert ds.GetLayer(0)
    ds = None

    # Multi-folder project asset ID ("projects/foo/bar/baz").
    tmpfile = '/vsimem/ee/projects/earthengine-legacy/assets/projects/foo/bar/baz:listImages?pageSize=1'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/earthengine-legacy/assets/projects/foo/bar/baz/qux'
                }
            ]
        })):
        ds = ogr.Open('EEDA:projects/foo/bar/baz')
        assert ds.GetLayer(0)
    ds = None

    # Public-catalog asset ID (e.g. "LANDSAT").
    tmpfile = '/vsimem/ee/projects/earthengine-public/assets/foo:listImages?pageSize=1'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/earthengine-public/assets/foo/bar'
                }
            ]
        })):
        ds = ogr.Open('EEDA:foo')
        assert ds.GetLayer(0)
    ds = None

    # Asset name ("projects/*/assets/**").
    tmpfile = '/vsimem/ee/projects/foo/assets/bar:listImages?pageSize=1'
    with gdaltest.tempfile(tmpfile, json.dumps({
            'images': [
                {
                    'name': 'projects/foo/assets/bar/baz'
                }
            ]
        })):
        ds = ogr.Open('EEDA:projects/foo/assets/bar')
        assert ds.GetLayer(0)
    ds = None

    gdal.SetConfigOption('EEDA_BEARER', None)
    gdal.SetConfigOption('EEDA_URL', None)

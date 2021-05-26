#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OAPIF driver testing.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys.com>
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



import gdaltest
from osgeo import ogr
import webserver
import pytest

###############################################################################
# Init
#


def test_ogr_opaif_init():

    gdaltest.opaif_drv = ogr.GetDriverByName('OAPIF')
    if gdaltest.opaif_drv is None:
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = \
        webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()


###############################################################################


def test_ogr_opaif_errors():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 404)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is None

    # No Content-Type
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is None

    # Unexpected Content-Type
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200,
                {'Content-Type': 'text/html'}, 'foo')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is None

    # Invalid JSON
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200,
                {'Content-Type': 'application/json'}, 'foo bar')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is None

    # Valid JSON but not collections array
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200,
                {'Content-Type': 'application/json'}, '{}')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is None

    # Valid JSON but collections is not an array
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200,
                {'Content-Type': 'application/json'},
                '{ "collections" : null }')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is None

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200,
                {'Content-Type': 'application/json'},
                '{ "collections" : [ null, {} ] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is None

###############################################################################


def test_ogr_opaif_collections_paging():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200,
                {'Content-Type': 'application/json'},
                """{ "collections" : [ { "id": "foo" } ],
                   "links": [ { "rel": "next", "href": "http://localhost:%d/oapif/collections?next=my_mark", "type": "application/json" } ]}""" % gdaltest.webserver_port)
    handler.add('GET', '/oapif/collections?next=my_mark', 200,
                {'Content-Type': 'application/json'},
                '{ "collections" : [ { "id": "bar" } ]}')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    assert ds is not None
    assert ds.GetLayerCount() == 2
    assert ds.GetLayer(0).GetName() == 'foo'
    assert ds.GetLayer(1).GetName() == 'bar'

###############################################################################


def test_ogr_opaif_empty_layer_and_user_query_parameters():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections?FOO=BAR', 200,
                {'Content-Type': 'application/json'},
                '{ "collections" : [ { "name": "foo" }] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif?FOO=BAR' % gdaltest.webserver_port)
    assert ds is not None
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&FOO=BAR', 200,
                {'Content-Type': 'application/geo+json'},
                '{ "type": "FeatureCollection", "features": [] }')
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 0


###############################################################################


def test_ogr_opaif_open_by_collection_and_legacy_wfs3_prefix():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo', 200,
                {'Content-Type': 'application/json'},
                '{ "id": "foo" }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/oapif/collections/foo' % gdaltest.webserver_port)
    assert ds is not None
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                '{ "type": "FeatureCollection", "features": [] }')
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 0


###############################################################################


def test_ogr_opaif_fc_links_next_geojson():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                '{ "collections" : [ { "name": "foo" }] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection",
                    "links" : [
                        { "rel": "next", "type": "application/geo+json", "href": "http://localhost:%d/oapif/foo_next" }
                    ],
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""" % gdaltest.webserver_port)
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    if f['foo'] != 'bar':
        f.DumpReadable()
        pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/foo_next', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection",
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "baz"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    if f['foo'] != 'baz':
        f.DumpReadable()
        pytest.fail()

###############################################################################


def test_ogr_opaif_id_is_integer():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                '{ "collections" : [ { "name": "foo" }] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "id": 100,
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "id": 100,
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f.GetFID() == 100

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items/100', 200,
                {'Content-Type': 'application/geo+json'},
                """{
                        "id": 100,
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetFeature(100)
    assert f

###############################################################################


def NO_LONGER_USED_test_ogr_opaif_fc_links_next_headers():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                '{ "collections" : [ { "name": "foo" }] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    link_val = '<http://data.example.org/buildings.json>; rel="self"; type="application/geo+json"\r\nLink: <http://localhost:%d/oapif/foo_next>; rel="next"; type="application/geo+json"' % gdaltest.webserver_port
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json',
                 'Link': link_val},
                """{ "type": "FeatureCollection",
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    if f['foo'] != 'bar':
        f.DumpReadable()
        pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/foo_next', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection",
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "baz"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    if f['foo'] != 'baz':
        f.DumpReadable()
        pytest.fail()


###############################################################################


def test_ogr_opaif_spatial_filter():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Deprecated API
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "name": "foo",
                    "extent": {
                        "spatial": [ -10, 40, 15, 50 ]
                    }
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastGetExtent)
    assert lyr.GetExtent() == (-10.0, 15.0, 40.0, 50.0)

    # Nominal API
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "id": "foo",
                    "extent": {
                        "spatial": {
                            "bbox": [
                                [ -10, 40, -100, 15, 50, 100 ]
                            ]
                        }
                    }
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)
    assert lyr.GetExtent() == (-10.0, 15.0, 40.0, 50.0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    lyr.SetSpatialFilterRect(2, 49, 3, 50)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&bbox=2,49,3,50', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar",
                        },
                        "geometry": {
                            "type": "Point",
                            "coordinates": [ 2.5, 49.5 ]
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    # Test clamping of bounds
    lyr.SetSpatialFilterRect(-200, 49, 200, 50)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&bbox=-180,49,180,50', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar",
                        },
                        "geometry": {
                            "type": "Point",
                            "coordinates": [ 2.5, 49.5 ]
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.SetSpatialFilterRect(2, -100, 3, 100)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&bbox=2,-90,3,90', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar",
                        },
                        "geometry": {
                            "type": "Point",
                            "coordinates": [ 2.5, 49.5 ]
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.SetSpatialFilterRect(-200, -100, 200, 100)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar",
                        },
                        "geometry": {
                            "type": "Point",
                            "coordinates": [ 2.5, 49.5 ]
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.SetSpatialFilter(None)
    lyr.ResetReading()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar",
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_opaif_get_feature_count():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "id": "foo"
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200, {'Content-Type': 'application/json'}, '{}')
    handler.add('GET', '/oapif', 200, {'Content-Type': 'application/json'},
                """{ "links" : [ { "rel": "service-desc",
                                   "href" : "http://localhost:%d/oapif/my_api",
                                   "type": "application/vnd.oai.openapi+json;version=3.0" } ] }""" % gdaltest.webserver_port )
    # Fake openapi response
    handler.add('GET', '/oapif/my_api', 200, {'Content-Type': 'application/vnd.oai.openapi+json; charset=utf-8; version=3.0'},
                """{
            "openapi": "3.0.0",
            "paths" : {
                "/collections/foo/items": {
                    "get": {
                        "parameters": [
                            {
                                "$ref" : "#/components/parameters/resultType"
                            }
                        ]
                    }
                }
            },
            "components": {
                "parameters": {
                    "resultType": {
                        "name": "resultType",
                        "in": "query",
                        "schema" : {
                            "type" : "string",
                            "enum" : [ "hits", "results" ]
                        }
                    }
                }
            }
        }""")
    handler.add('GET', '/oapif/collections/foo/items?resultType=hits', 200,
                {'Content-Type': 'application/json'},
                '{ "numberMatched": 1234 }')
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 1234

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?resultType=hits', 200,
                {'Content-Type': 'application/json'},
                '{ "numberMatched": 1234 }')
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 1234

###############################################################################


def test_ogr_opaif_get_feature_count_from_numberMatched():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "id": "foo"
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                '{ "type": "FeatureCollection", "features": [], "numberMatched": 1234 }')
    with webserver.install_http_handler(handler):
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0 # Cannot know yet
        assert lyr.GetFeatureCount() == 1234
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1


###############################################################################


def test_ogr_opaif_attribute_filter():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "name": "foo"
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter(None) # should not cause network request

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "id": "my_id",
                        "type": "Feature",
                        "properties": {
                            "attr1": "",
                            "attr2": 0,
                            "attr3": "",
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""")
    # Fake openapi response
    handler.add('GET', '/oapif', 200, {'Content-Type': 'application/json'},
                """{ "links" : [ { "rel": "service-desc",
                                   "href" : "http://localhost:%d/oapif/my_api",
                                   "type": "application/vnd.oai.openapi+json;version=3.0" } ] }""" % gdaltest.webserver_port )

    handler.add('GET', '/oapif/my_api', 200, {'Content-Type': 'application/json'},
                """{
            "openapi": "3.0.0",
            "paths" : {
                "/collections/foo/items": {
                    "get": {
                        "parameters": [
                            {
                                "name": "attr1",
                                "in": "query"
                            },
                            {
                                "name": "attr2",
                                "in": "query"
                            }
                        ]
                    }
                }
            }
        }""")
    with webserver.install_http_handler(handler):
        lyr.SetAttributeFilter("(attr1 = 'foo' AND attr2 = 2) AND attr3 = 'bar'")

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&attr1=foo&attr2=2', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "attr1": "foo",
                            "attr2": 2,
                            "attr3": "foo"
                        }
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "attr1": "foo",
                            "attr2": 2,
                            "attr3": "bar"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.ResetReading()
    lyr.SetAttributeFilter("attr1 = 'foo' OR attr3 = 'bar'")
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "attr1": "foo",
                            "attr3": "foo"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


    # Date =
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime = '2019-10-01T12:34:56Z'")
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&datetime=2019-10-01T12:34:56Z', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


    # Date >=
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime >= '2019-10-01T12:34:56Z'")
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&datetime=2019-10-01T12:34:56Z%2F..', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


    # Date <=
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime <= '2019-10-01T12:34:56Z'")
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&datetime=..%2F2019-10-01T12:34:56Z', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


    # Date >= and <=
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime BETWEEN '2019-10-01T' AND '2019-10-02T'")
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10&datetime=2019-10-01T%2F2019-10-02T', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


    # id
    lyr.ResetReading()
    lyr.SetAttributeFilter("id = 'my_id'")
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items/my_id', 200,
                {'Content-Type': 'application/geo+json'},
                """{
                        "type": "Feature",
                        "id": "my_id",
                        "properties": {
                        }
                    }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


    lyr.ResetReading()
    lyr.SetAttributeFilter(None)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "attr1": "foo",
                            "attr3": "foo"
                        }
                    }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_opaif_schema_from_xml_schema():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "name": "foo",
                    "links": [
                        { "rel": "describedBy",
                          "type": "application/xml",
                          "href": "http://localhost:%d/oapif/collections/foo/xmlschema"
                        }
                    ]
                 } ] }""" % gdaltest.webserver_port)

    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/xmlschema', 200, {'Content-Type': 'application/xml'},
"""<?xml version="1.0" encoding="UTF-8"?>
<xs:schema targetNamespace="http://ogr.maptools.org/" xmlns:ogr="http://ogr.maptools.org/" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:gml="http://www.opengis.net/gml" elementFormDefault="qualified" version="1.0">
<xs:import namespace="http://www.opengis.net/gml" schemaLocation="http://schemas.opengis.net/gml/2.1.2/feature.xsd"/>
<xs:element name="FeatureCollection" type="ogr:FeatureCollectionType" substitutionGroup="gml:_FeatureCollection"/>
<xs:complexType name="FeatureCollectionType">
  <xs:complexContent>
    <xs:extension base="gml:AbstractFeatureCollectionType">
      <xs:attribute name="lockId" type="xs:string" use="optional"/>
      <xs:attribute name="scope" type="xs:string" use="optional"/>
    </xs:extension>
  </xs:complexContent>
</xs:complexType>
<xs:element name="foo" type="ogr:foo_Type" substitutionGroup="gml:_Feature"/>
<xs:complexType name="foo_Type">
  <xs:complexContent>
    <xs:extension base="gml:AbstractFeatureType">
      <xs:sequence>
        <xs:element name="geometryProperty" type="gml:PolygonPropertyType" nillable="true" minOccurs="0" maxOccurs="1"/>
        <xs:element name="some_int" nillable="true" minOccurs="0" maxOccurs="1">
          <xs:simpleType>
            <xs:restriction base="xs:decimal">
              <xs:totalDigits value="12"/>
              <xs:fractionDigits value="3"/>
            </xs:restriction>
          </xs:simpleType>
        </xs:element>
      </xs:sequence>
    </xs:extension>
  </xs:complexContent>
</xs:complexType>
</xs:schema>""")
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                '{ "type": "FeatureCollection", "features": [], "numberMatched": 1234 }')
    with webserver.install_http_handler(handler):
        feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetGeomType() == ogr.wkbPolygon
    assert feat_defn.GetFieldCount() == 1
    assert feat_defn.GetFieldDefn(0).GetName() == 'some_int'

###############################################################################


def test_ogr_opaif_schema_from_json_schema():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "name": "foo",
                    "links": [
                        { "rel": "describedBy",
                          "type": "application/schema+json",
                          "href": "http://localhost:%d/oapif/collections/foo/jsonschema"
                        }
                    ]
                 } ] }""" % gdaltest.webserver_port)

    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/jsonschema', 200,
                {'Content-Type': 'application/schema+json'},
                open('data/oapif/oapif_json_schema_eo.jsonschema', 'rt').read())
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                {
                    "type": "Feature",
                    "properties": {
                        "unexpected": 123
                    }
                }
                ] }""")
    with webserver.install_http_handler(handler):
        feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetGeomType() == ogr.wkbUnknown
    assert feat_defn.GetFieldCount() == 19

    idx = feat_defn.GetFieldIndex("type")
    assert idx >= 0
    assert feat_defn.GetFieldDefn(idx).GetType() == ogr.OFTString

    idx = feat_defn.GetFieldIndex("updated")
    assert idx >= 0
    assert feat_defn.GetFieldDefn(idx).GetType() == ogr.OFTDateTime

    idx = feat_defn.GetFieldIndex("unexpected")
    assert idx >= 0
    assert feat_defn.GetFieldDefn(idx).GetType() == ogr.OFTInteger

###############################################################################


def test_ogr_opaif_stac_catalog():
    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo', 200, {'Content-Type': 'application/json'},
                """{
                    "name": "foo",
                    "item_assets": {
                      "my_asset": {},
                      "my_asset2": {}
                    }
                   }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('OAPIF:http://localhost:%d/oapif/collections/foo' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                {
                    "type": "Feature",
                    "properties": { "foo": "bar" },
                    "assets": { "my_asset": { "href": "my_url" } }
                },
                {
                    "type": "Feature",
                    "properties": { "foo": "bar2" },
                    "assets": { "my_asset": { "href": "my_url2" } }
                }
                ] }""")
    handler.add('GET', '/oapif/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                {
                    "type": "Feature",
                    "properties": { "foo": "bar" },
                    "assets": { "my_asset": { "href": "my_url" } }
                },
                {
                    "type": "Feature",
                    "properties": { "foo": "bar2" },
                    "assets": { "my_asset2": { "href": "my_url2" } }
                }
                ] }""")
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f['foo'] == 'bar'
        assert f['asset_my_asset_href'] == 'my_url'

        f = lyr.GetNextFeature()
        assert f['foo'] == 'bar2'
        assert f['asset_my_asset2_href'] == 'my_url2'


###############################################################################


def test_ogr_opaif_cleanup():

    if gdaltest.opaif_drv is None:
        pytest.skip()

    if gdaltest.webserver_port != 0:
        webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)





#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  WFS3 driver testing.
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


def test_ogr_wfs3_init():

    gdaltest.wfs3_drv = ogr.GetDriverByName('WFS3')
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = \
        webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()

    
###############################################################################


def test_ogr_wfs3_errors():
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 404)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is None

    # No Content-Type
    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is None

    # Unexpected Content-Type
    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200,
                {'Content-Type': 'text/html'}, 'foo')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is None

    # Invalid JSON
    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200,
                {'Content-Type': 'application/json'}, 'foo bar')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is None

    # Valid JSON but not collections array
    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200,
                {'Content-Type': 'application/json'}, '{}')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is None

    # Valid JSON but collections is not an array
    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200,
                {'Content-Type': 'application/json'},
                '{ "collections" : null }')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is None

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200,
                {'Content-Type': 'application/json'},
                '{ "collections" : [ null, {} ] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is not None
    assert ds.GetLayerCount() == 0
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(0) is None

###############################################################################


def test_ogr_wfs3_empty_layer():
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200,
                {'Content-Type': 'application/json'},
                '{ "collections" : [ { "name": "foo" }] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    assert ds is not None
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                '{ "type": "FeatureCollection", "features": [] }')
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 0

    
###############################################################################


def test_ogr_wfs3_fc_links_next_geojson():
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200, {'Content-Type': 'application/json'},
                '{ "collections" : [ { "name": "foo" }] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
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
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection",
                    "links" : [
                        { "rel": "next", "type": "application/geo+json", "href": "http://localhost:%d/wfs3/foo_next" }
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
    handler.add('GET', '/wfs3/foo_next', 200,
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


def test_ogr_wfs3_fc_links_next_headers():
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200, {'Content-Type': 'application/json'},
                '{ "collections" : [ { "name": "foo" }] }')
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
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
    link_val = '<http://data.example.org/buildings.json>; rel="self"; type="application/geo+json"\r\nLink: <http://localhost:%d/wfs3/foo_next>; rel="next"; type="application/geo+json"' % gdaltest.webserver_port
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
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
    handler.add('GET', '/wfs3/foo_next', 200,
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


def test_ogr_wfs3_spatial_filter():
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "name": "foo",
                    "extent": {
                        "spatial": [ -10, 40, 15, 50 ]
                    }
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)
    assert lyr.GetExtent() == (-10.0, 15.0, 40.0, 50.0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
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
    handler.add('GET', '/wfs3/collections/foo/items?limit=10&bbox=2,49,3,50', 200,
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
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
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


def test_ogr_wfs3_get_feature_count():
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "name": "foo",
                    "extent": {
                        "spatial": [ -10, 40, 15, 50 ]
                    }
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    # Fake openapi response
    handler.add('GET', '/wfs3/api', 200, {'Content-Type': 'application/json'},
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
    handler.add('GET', '/wfs3/collections/foo/items?resultType=hits', 200,
                {'Content-Type': 'application/json'},
                '{ "numberMatched": 1234 }')
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 1234

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections/foo/items?resultType=hits', 200,
                {'Content-Type': 'application/json'},
                '{ "numberMatched": 1234 }')
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 1234

    
###############################################################################


def test_ogr_wfs3_attribute_filter():
    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections', 200, {'Content-Type': 'application/json'},
                """{ "collections" : [ {
                    "name": "foo"
                 }] }""")
    with webserver.install_http_handler(handler):
        ds = ogr.Open('WFS3:http://localhost:%d/wfs3' % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
                {'Content-Type': 'application/geo+json'},
                """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "attr1": "",
                            "attr2": 0,
                            "attr3": ""
                        }
                    }
                ] }""")
    # Fake openapi response
    handler.add('GET', '/wfs3/api', 200, {'Content-Type': 'application/json'},
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
    handler.add('GET', '/wfs3/collections/foo/items?limit=10&attr1=foo&attr2=2', 200,
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
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
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

    lyr.ResetReading()
    lyr.SetAttributeFilter(None)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/wfs3/collections/foo/items?limit=10', 200,
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


def test_ogr_wfs3_cleanup():

    if gdaltest.wfs3_drv is None:
        pytest.skip()

    if gdaltest.webserver_port != 0:
        webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    



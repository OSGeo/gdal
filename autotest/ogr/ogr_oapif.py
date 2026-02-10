#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OAPIF driver testing.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import json

import gdaltest
import pytest
import webserver

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("OAPIF")

###############################################################################
# Init
#


@pytest.fixture(scope="module", autouse=True)
def init():

    gdaltest.webserver_process, gdaltest.webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if gdaltest.webserver_port == 0:
        pytest.skip()
    yield

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)


###############################################################################


def test_ogr_oapif_errors():

    handler = webserver.SequentialHandler()
    handler.add("GET", "/oapif/collections", 404)
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception, match="HTTP error code : 404"):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)

    handler = webserver.SequentialHandler()
    handler.add("GET", "/oapif/collections", 404, {}, "unavailable resource")
    with webserver.install_http_handler(handler):
        with pytest.raises(
            Exception, match="HTTP error code : 404, unavailable resource"
        ):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)

    # No Content-Type
    handler = webserver.SequentialHandler()
    handler.add("GET", "/oapif/collections", 200, {}, "foo")
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)

    # Unexpected Content-Type
    handler = webserver.SequentialHandler()
    handler.add("GET", "/oapif/collections", 200, {"Content-Type": "text/html"}, "foo")
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)

    # Invalid JSON
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        "foo bar",
    )
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)

    # Valid JSON but not collections array
    handler = webserver.SequentialHandler()
    handler.add(
        "GET", "/oapif/collections", 200, {"Content-Type": "application/json"}, "{}"
    )
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)

    # Valid JSON but collections is not an array
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : null }',
    )
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ null, {} ] }',
    )
    with webserver.install_http_handler(handler):
        with pytest.raises(Exception):
            ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)


###############################################################################


def test_ogr_oapif_collections_paging():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ { "id": "foo" } ],
                   "links": [ { "rel": "next", "href": "http://localhost:%d/oapif/collections?next=my_mark", "type": "application/json" } ]}"""
        % gdaltest.webserver_port,
    )
    handler.add(
        "GET",
        "/oapif/collections?next=my_mark",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "id": "bar" } ]}',
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    assert ds is not None
    assert ds.GetLayerCount() == 2
    assert ds.GetLayer(0).GetName() == "foo"
    assert ds.GetLayer(1).GetName() == "bar"


###############################################################################


def test_ogr_oapif_empty_layer_and_user_query_parameters():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections?FOO=BAR",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open(
            "OAPIF:http://localhost:%d/oapif?FOO=BAR" % gdaltest.webserver_port
        )
    assert ds is not None
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "foo"

    handler = webserver.SequentialHandler()
    handler.add("GET", "/oapif?FOO=BAR", 200, {}, "{}")
    handler.add("GET", "/oapif/api?FOO=BAR", 200, {}, "{}")
    handler.add("GET", "/oapif/api/?FOO=BAR", 200, {}, "{}")
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20&FOO=BAR",
        200,
        {"Content-Type": "application/geo+json"},
        '{ "type": "FeatureCollection", "features": [] }',
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 0


###############################################################################


def _add_dummy_root_and_api_pages(handler, prefix=""):
    handler.add("GET", prefix + "/oapif", 404, {}, "{}")
    handler.add("GET", prefix + "/oapif/api", 404, {}, "{}")
    handler.add("GET", prefix + "/oapif/api/", 404, {}, "{}")


###############################################################################


def test_ogr_oapif_open_by_collection_and_legacy_wfs3_prefix():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo",
        200,
        {"Content-Type": "application/json"},
        '{ "id": "foo" }',
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open(
            "WFS3:http://localhost:%d/oapif/collections/foo" % gdaltest.webserver_port
        )
    assert ds is not None
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "foo"

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        '{ "type": "FeatureCollection", "features": [] }',
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 0


###############################################################################


def test_ogr_oapif_fc_links_next_geojson():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/subdir/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open(
            "OAPIF:http://localhost:%d/subdir/oapif" % gdaltest.webserver_port
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler, "/subdir")
    handler.add(
        "GET",
        "/subdir/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    # Test relative links
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/subdir/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection",
                    "links" : [
                        { "rel": "next", "type": "application/geo+json", "href": "/subdir/oapif/foo_next" }
                    ],
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f["foo"] == "bar"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/subdir/oapif/foo_next",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection",
                    "links" : [
                        { "rel": "next", "type": "application/geo+json", "href": "./foo_next2" }
                    ],
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "baz"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f["foo"] == "baz"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/subdir/oapif/foo_next2",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection",
                    "links" : [
                        { "rel": "next", "type": "application/geo+json", "href": "../oapif/foo_next3" }
                    ],
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "baz2"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f["foo"] == "baz2"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/subdir/oapif/foo_next3",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection",
                    "links" : [
                        { "rel": "next", "type": "application/geo+json", "href": "foo_next4" }
                    ],
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "baz3"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f["foo"] == "baz3"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/subdir/oapif/foo_next4",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection",
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "baz4"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f["foo"] == "baz4"


###############################################################################


def test_ogr_oapif_id_is_integer():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "id": 100,
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "id": 100,
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f.GetFID() == 100

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items/100",
        200,
        {"Content-Type": "application/geo+json"},
        """{
                        "id": 100,
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetFeature(100)
    assert f


###############################################################################


def NO_LONGER_USED_test_ogr_oapif_fc_links_next_headers():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    link_val = (
        '<http://data.example.org/buildings.json>; rel="self"; type="application/geo+json"\r\nLink: <http://localhost:%d/oapif/foo_next>; rel="next"; type="application/geo+json"'
        % gdaltest.webserver_port
    )
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json", "Link": link_val},
        """{ "type": "FeatureCollection",
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    if f["foo"] != "bar":
        f.DumpReadable()
        pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/foo_next",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection",
                    "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "baz"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    if f["foo"] != "baz":
        f.DumpReadable()
        pytest.fail()


###############################################################################


def test_ogr_oapif_spatial_filter_deprecated_api():

    # Deprecated API
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "name": "foo",
                    "extent": {
                        "spatial": [ -10, 40, 15, 50 ]
                    }
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastGetExtent)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetExtent() == (-10.0, 15.0, 40.0, 50.0)


###############################################################################


def test_ogr_oapif_spatial_filter():

    # Nominal API
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "id": "foo",
                    "extent": {
                        "spatial": {
                            "bbox": [
                                [ -10, 40, -100, 15, 50, 100 ]
                            ]
                        }
                    }
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetExtent() == (-10.0, 15.0, 40.0, 50.0)
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    lyr.SetSpatialFilterRect(2, 49, 3, 50)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&bbox=2,49,3,50",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    # Test clamping of bounds
    lyr.SetSpatialFilterRect(-200, 49, 200, 50)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&bbox=-180,49,180,50",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.SetSpatialFilterRect(2, -100, 3, 100)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&bbox=2,-90,3,90",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.SetSpatialFilterRect(-200, -100, 200, 100)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.SetSpatialFilter(None)
    lyr.ResetReading()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar",
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


###############################################################################


def test_ogr_oapif_limit():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "id": "foo"
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif",
        200,
        {"Content-Type": "application/json"},
        """{ "links" : [ { "rel": "service-desc",
                                   "href" : "http://localhost:%d/oapif/my_api",
                                   "type": "application/vnd.oai.openapi+json;version=3.0" } ] }"""
        % gdaltest.webserver_port,
    )
    # Fake openapi response
    handler.add(
        "GET",
        "/oapif/my_api",
        200,
        {
            "Content-Type": "application/vnd.oai.openapi+json; charset=utf-8; version=3.0"
        },
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
        }""",
    )
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/json"},
        "{}",
    )
    handler.add(
        "GET",
        "/oapif/collections/foo/items?resultType=hits",
        200,
        {"Content-Type": "application/json"},
        '{ "numberMatched": 1234 }',
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 1234

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?resultType=hits",
        200,
        {"Content-Type": "application/json"},
        '{ "numberMatched": 1234 }',
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 1234


###############################################################################


def test_ogr_oapif_limit_from_numberMatched():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "id": "foo"
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        '{ "type": "FeatureCollection", "features": [], "numberMatched": 1234 }',
    )
    with webserver.install_http_handler(handler):
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0  # Cannot know yet
        assert lyr.GetFeatureCount() == 1234
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 1234


###############################################################################


def test_ogr_oapif_feature_count_from_ldproxy_itemCount():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "id": "foo",
                    "itemCount": 1234
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert lyr.GetFeatureCount() == 1234


###############################################################################


def test_ogr_oapif_attribute_filter():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "name": "foo"
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter(None)  # should not cause network request

    handler = webserver.SequentialHandler()

    # Fake openapi response
    handler.add(
        "GET",
        "/oapif",
        200,
        {"Content-Type": "application/json"},
        """{ "links" : [ { "rel": "service-desc",
                                   "href" : "http://localhost:%d/oapif/my_api",
                                   "type": "application/vnd.oai.openapi+json;version=3.0" } ] }"""
        % gdaltest.webserver_port,
    )

    handler.add(
        "GET",
        "/oapif/my_api",
        200,
        {"Content-Type": "application/json"},
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
        }""",
    )

    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )

    with webserver.install_http_handler(handler):
        lyr.SetAttributeFilter("(attr1 = 'foo' AND attr2 = 2) AND attr3 = 'bar'")

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&attr1=foo&attr2=2",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.ResetReading()
    lyr.SetAttributeFilter("attr1 = 'foo' OR attr3 = 'bar'")
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "attr1": "foo",
                            "attr3": "foo"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    # Date =
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime = '2019-10-01T12:34:56Z'")
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&datetime=2019-10-01T12:34:56Z",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    # Date >=
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime >= '2019-10-01T12:34:56Z'")
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&datetime=2019-10-01T12:34:56Z%2F..",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    # Date <=
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime <= '2019-10-01T12:34:56Z'")
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&datetime=..%2F2019-10-01T12:34:56Z",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    # Date >= and <=
    lyr.ResetReading()
    lyr.SetAttributeFilter("mydatetime BETWEEN '2019-10-01T' AND '2019-10-02T'")
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&datetime=2019-10-01T%2F2019-10-02T",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "mydatetime": "2019-10-01T12:34:56Z"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    # id
    lyr.ResetReading()
    lyr.SetAttributeFilter("id = 'my_id'")
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items/my_id",
        200,
        {"Content-Type": "application/geo+json"},
        """{
                        "type": "Feature",
                        "id": "my_id",
                        "properties": {
                        }
                    }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None

    lyr.ResetReading()
    lyr.SetAttributeFilter(None)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "attr1": "foo",
                            "attr3": "foo"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
    assert f is not None


###############################################################################


def test_ogr_oapif_schema_from_xml_schema():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "name": "foo",
                    "links": [
                        { "rel": "describedby",
                          "type": "application/xml",
                          "href": "http://localhost:%d/oapif/collections/foo/xmlschema"
                        }
                    ]
                 } ] }""" % gdaltest.webserver_port,
    )

    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/xmlschema",
        200,
        {"Content-Type": "application/xml"},
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
</xs:schema>""",
    )
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        '{ "type": "FeatureCollection", "features": [], "numberMatched": 1234 }',
    )
    with webserver.install_http_handler(handler):
        feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetGeomType() == ogr.wkbPolygon
    assert feat_defn.GetFieldCount() == 1
    assert feat_defn.GetFieldDefn(0).GetName() == "some_int"


###############################################################################


def test_ogr_oapif_schema_from_json_schema():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "name": "foo",
                    "links": [
                        { "rel": "describedby",
                          "type": "application/schema+json",
                          "href": "http://localhost:%d/oapif/collections/foo/jsonschema"
                        }
                    ]
                 } ] }""" % gdaltest.webserver_port,
    )

    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/jsonschema",
        200,
        {"Content-Type": "application/schema+json"},
        open("data/oapif/oapif_json_schema_eo.jsonschema", "rt").read(),
    )
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                {
                    "type": "Feature",
                    "properties": {
                        "unexpected": 123
                    }
                }
                ] }""",
    )
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


def test_ogr_oapif_stac_catalog():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo",
        200,
        {"Content-Type": "application/json"},
        """{
                    "name": "foo",
                    "item_assets": {
                      "my_asset": {},
                      "my_asset2": {}
                    }
                   }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open(
            "OAPIF:http://localhost:%d/oapif/collections/foo" % gdaltest.webserver_port
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
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
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f["foo"] == "bar"
        assert f["asset_my_asset_href"] == "my_url"

        f = lyr.GetNextFeature()
        assert f["foo"] == "bar2"
        assert f["asset_my_asset2_href"] == "my_url2"


###############################################################################


def test_ogr_oapif_storage_crs_easting_northing():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "name": "foo",
                    "extent": {
                        "spatial": [ -10, 40, 15, 50 ]
                    },
                    "storageCrs": "http://www.opengis.net/def/crs/EPSG/0/32631"
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [2, 49]}
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx(
        (-611288.854779237, 4427761.561734099, 1525592.2813932528, 5620112.89047953),
        abs=1e-3,
    )

    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == "32631"
    assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/32631",
        200,
        {
            "Content-Type": "application/geo+json",
            "Content-Crs": "<http://www.opengis.net/def/crs/EPSG/0/32631>",
        },
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [500000, 4500000]}
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToWkt() == "POINT (500000 4500000)"

    lyr.SetSpatialFilterRect(400000, 4000000, 600000, 5000000)
    lyr.ResetReading()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&bbox=400000,4000000,600000,5000000&bbox-crs=http://www.opengis.net/def/crs/EPSG/0/32631&crs=http://www.opengis.net/def/crs/EPSG/0/32631",
        200,
        {
            "Content-Type": "application/geo+json",
            "Content-Crs": "<http://www.opengis.net/def/crs/EPSG/0/32631>",
        },
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [500000, 4500000]}
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToWkt() == "POINT (500000 4500000)"


###############################################################################


def test_ogr_oapif_storage_crs_latitude_longitude():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "name": "foo",
                    "extent": {
                        "spatial": [ -10, 40, 15, 50 ]
                    },
                    "storageCrs": "http://www.opengis.net/def/crs/EPSG/0/4326",
                    "storageCrsCoordinateEpoch": 2022.5
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = ogr.Open("OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port)
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [2, 49]}
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx((-10, 40, 15, 50), abs=1e-3)

    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == "4326"
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]
    assert srs.GetCoordinateEpoch() == 2022.5
    assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    # Coordinates must be in lat, lon order in the GeoJSON answer
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/4326",
        200,
        {
            "Content-Type": "application/geo+json",
            "Content-Crs": "<http://www.opengis.net/def/crs/EPSG/0/4326>",
        },
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [49, 2]}
                    }
                ] }""",
    )
    # Check that we swap them back to GIS friendly order
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToWkt() == "POINT (2 49)"

    lyr.SetSpatialFilterRect(1, 48, 3, 50)
    lyr.ResetReading()

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&bbox=48,1,50,3&bbox-crs=http://www.opengis.net/def/crs/EPSG/0/4326&crs=http://www.opengis.net/def/crs/EPSG/0/4326",
        200,
        {
            "Content-Type": "application/geo+json",
            "Content-Crs": "<http://www.opengis.net/def/crs/EPSG/0/4326>",
        },
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [49, 2]}
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToWkt() == "POINT (2 49)"


###############################################################################


def test_ogr_oapif_storage_crs_latitude_longitude_non_compliant_server():

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        """{ "collections" : [ {
                    "name": "foo",
                    "extent": {
                        "spatial": [ -10, 40, 15, 50 ]
                    },
                    "storageCrs": "http://www.opengis.net/def/crs/EPSG/0/4326",
                    "storageCrsCoordinateEpoch": 2022.5
                 }] }""",
    )
    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
            open_options=["SERVER_FEATURE_AXIS_ORDER=GIS_FRIENDLY"],
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [2, 49]}
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx((-10, 40, 15, 50), abs=1e-3)

    supported_srs_list = lyr.GetSupportedSRSList()
    assert supported_srs_list is None

    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == "4326"
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]
    assert srs.GetCoordinateEpoch() == 2022.5
    assert lyr.GetLayerDefn().GetFieldCount() == 1

    handler = webserver.SequentialHandler()
    # Coordinates must be in lat, lon order in the GeoJSON answer
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/4326",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "$comment": "Server not compliant here, returning longitude=2, latitude=49 in GIS friendly order",
                        "geometry": { "type": "Point", "coordinates" : [2, 49]}
                    }
                ] }""",
    )
    # Check GIS friendly order is preserved.
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToWkt() == "POINT (2 49)"


###############################################################################


def test_ogr_oapif_crs_and_preferred_crs_open_options():
    def get_collections_handler():
        handler = webserver.SequentialHandler()
        handler.add(
            "GET",
            "/oapif/collections",
            200,
            {"Content-Type": "application/json"},
            """{ "crs" : [ "http://www.opengis.net/def/crs/EPSG/0/32631" ],
                  "collections" : [ {
                        "name": "foo",
                        "extent": {
                            "spatial": [ -10, 40, 15, 50 ]
                        },
                        "crs": [ "#/crs", "http://www.opengis.net/def/crs/OGC/1.3/CRS84" ]
                     }] }""",
        )
        return handler

    with webserver.install_http_handler(get_collections_handler()):
        with pytest.raises(Exception):
            gdal.OpenEx(
                "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
                open_options=["CRS=EPSG:32632"],
            )

    with webserver.install_http_handler(get_collections_handler()):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
            open_options=["CRS=EPSG:32631"],
        )
        assert ds
        lyr = ds.GetLayer(0)

    def get_items_handler():
        handler = webserver.SequentialHandler()
        _add_dummy_root_and_api_pages(handler)
        handler.add(
            "GET",
            "/oapif/collections/foo/items?limit=20",
            200,
            {"Content-Type": "application/geo+json"},
            """{ "type": "FeatureCollection", "features": [
                        {
                            "type": "Feature",
                            "properties": {
                                "foo": "bar"
                            },
                            "geometry": { "type": "Point", "coordinates" : [2, 49]}
                        }
                    ] }""",
        )
        return handler

    with webserver.install_http_handler(get_items_handler()):
        minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx(
        (-611288.854779237, 4427761.561734099, 1525592.2813932528, 5620112.89047953),
        abs=1e-3,
    )

    supported_srs_list = lyr.GetSupportedSRSList()
    assert supported_srs_list
    assert len(supported_srs_list) == 2
    assert supported_srs_list[0].GetAuthorityCode(None) == "32631"
    # Below doesn't work with early PROJ 6 versions
    # assert supported_srs_list[1].GetAuthorityCode(None) == "CRS84"

    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == "32631"

    json_info = gdal.VectorInfo(ds, format="json", featureCount=False)
    assert "supportedSRSList" in json_info["layers"][0]["geometryFields"][0]
    assert len(json_info["layers"][0]["geometryFields"][0]["supportedSRSList"]) == 2
    assert json_info["layers"][0]["geometryFields"][0]["supportedSRSList"][0] == {
        "id": {"authority": "EPSG", "code": "32631"}
    }

    text_info = gdal.VectorInfo(ds, featureCount=False)
    assert "Supported SRS: EPSG:32631, " in text_info

    # Test changing active SRS
    assert lyr.SetActiveSRS(0, supported_srs_list[1]) == ogr.OGRERR_NONE
    with pytest.raises(Exception):
        lyr.SetActiveSRS(0, None)
    srs_other = osr.SpatialReference()
    srs_other.ImportFromEPSG(32632)
    with pytest.raises(Exception):
        lyr.SetActiveSRS(0, srs_other)
    assert lyr.GetSpatialRef().IsGeographic()
    minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx(
        (-10.0, 40.0, 15.0, 50.0),
        abs=1e-3,
    )

    assert lyr.SetActiveSRS(0, supported_srs_list[0]) == ogr.OGRERR_NONE
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
    minx, maxx, miny, maxy = lyr.GetExtent()
    assert (minx, miny, maxx, maxy) == pytest.approx(
        (-611288.854779237, 4427761.561734099, 1525592.2813932528, 5620112.89047953),
        abs=1e-3,
    )

    with webserver.install_http_handler(get_collections_handler()):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
            open_options=["PREFERRED_CRS=EPSG:32631"],
        )
        assert ds
        lyr = ds.GetLayer(0)
    with webserver.install_http_handler(get_items_handler()):
        srs = lyr.GetSpatialRef()
        assert srs
        assert srs.GetAuthorityCode(None) == "32631"

    with webserver.install_http_handler(get_collections_handler()):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
            open_options=["PREFERRED_CRS=EPSG:32632"],
        )
        assert ds
        lyr = ds.GetLayer(0)
    with webserver.install_http_handler(get_items_handler()):
        srs = lyr.GetSpatialRef()
        assert srs
        assert srs.GetAuthorityCode(None) == "4326"

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&crs=http://www.opengis.net/def/crs/EPSG/0/32631",
        200,
        {
            "Content-Type": "application/geo+json",
            "Content-Crs": "<http://www.opengis.net/def/crs/EPSG/0/32631>",
        },
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        },
                        "geometry": { "type": "Point", "coordinates" : [500000, 4500000]}
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        out_ds = gdal.VectorTranslate(
            "", ds, format="MEM", dstSRS="EPSG:32631", reproject=True
        )
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "32631"
    f = out_lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (500000 4500000)"


def test_ogr_oapif_collection_items_page_size():
    """Test getting limit from api description. Issue GH #8522"""

    schema = b"""
    { "components":{
        "parameters":{
          "limit":{
            "schema": {
              "default": 10,
              "maximum": 10000,
              "minimum": 1
            }
          }
        }
      },
      "paths":{
        "/collections/castles/items":{
          "get":{
            "description":"",
            "operationId":"getcastlesFeatures",
            "parameters":[ ]
          }
        }
      }
    }
    """ % {b"port": gdaltest.webserver_port}

    itemsdata = b"""
    { "type":"FeatureCollection",
      "features":[
        {
          "type":"Feature",
          "geometry":{
            "type":"Point",
            "coordinates":[
              5.890354724945141,
              50.922380110626314
            ]
          },
          "properties":{
            "gid":1
          },
          "id":"kastelen.1"
        },
        {
          "type":"Feature",
          "geometry":{
            "type":"Point",
            "coordinates":[
              5.90354724945141,
              50.22380110626314
            ]
          },
          "properties":{
            "gid":2
          },
          "id":"kastelen.2"
        }
      ]
    }
    """ % {b"port": gdaltest.webserver_port}

    filedata = {
        "/oapif": b"""
    { "links":[
        {
          "rel":"service-desc",
          "type":"application/vnd.oai.openapi+json;version=3.0",
          "href":"http://localhost:%(port)d/oapif/openapi"
        },
        {
          "rel":"data",
          "type":"application/json",
          "href":"http://localhost:%(port)d/oapif/collections"
        }
      ]
    }
    """
        % {b"port": gdaltest.webserver_port},
        "/oapif/collections": b"""
        { "collections":[
            {
            "id":"castles",
            "itemType":"feature",
            "crs":[
                "http://www.opengis.net/def/crs/OGC/1.3/CRS84",
                "http://www.opengis.net/def/crs/EPSG/0/4326"
            ]
        }
         ]}"""
        % {b"port": gdaltest.webserver_port},
        "/oapif/openapi": schema,
        "/oapif/collections/castles/items": itemsdata,
        "/oapif/collections/castles/items?limit=100": itemsdata,
        "/oapif/collections/castles/items?limit=20": itemsdata,
        "/oapif/collections/castles/items?limit=1000": itemsdata,
        "/oapif/collections/castles/items?limit=5000": itemsdata,
        "/oapif/openapi/ogcapi-features-1.json": b"""
        { "components": {
            "parameters": {
              "limit": {
                "name": "limit",
                "schema": {
                  "default": 10,
                  "maximum": 5000,
                  "minimum": 1
                }
              }
            }
          }
        }
        """,
    }

    # Check for json syntax
    for i in filedata.values():
        try:
            json.loads(i)
        except Exception:
            print(i.decode("utf8"))

    external_json_limit = {
        "$ref": "http://localhost:%(port)d/oapif/openapi/ogcapi-features-1.json#/components/parameters/limit"
        % {"port": gdaltest.webserver_port}
    }
    internal_json_component_limit = {"$ref": "#/components/parameters/limit"}

    class LoggingHandler(webserver.FileHandler):
        def do_GET(self, request):
            self.last_path = request.path
            return super().do_GET(request)

    handler = LoggingHandler(filedata, content_type="application/json")

    # Test default page size 1000
    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%(port)d/oapif" % {"port": gdaltest.webserver_port}
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        feature = lyr.GetNextFeature()
        assert feature is not None
        assert handler.last_path == "/oapif/collections/castles/items?limit=1000"

    # Test numberMatched it does not affect limit
    j_data = json.loads(itemsdata)
    j_data.update({"numberMatched": 2})
    j_data = json.dumps(j_data).encode("utf8")
    for k in filedata:
        if k.startswith("/oapif/collections/castles/items"):
            filedata[k] = j_data
    handler = LoggingHandler(filedata, content_type="application/json")

    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%(port)d/oapif" % {"port": gdaltest.webserver_port}
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        feature = lyr.GetNextFeature()
        assert feature is not None
        assert handler.last_path == "/oapif/collections/castles/items?limit=1000"

    # Internal component limit, check that the GDAL default is used
    j_data = json.loads(schema)
    j_data["paths"]["/collections/castles/items"]["get"]["parameters"] = [
        {"$ref": "#/components/parameters/limit"}
    ]
    j_data = json.dumps(j_data).encode("utf8")
    filedata["/oapif/openapi"] = j_data
    handler = LoggingHandler(filedata, content_type="application/json")

    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%(port)d/oapif" % {"port": gdaltest.webserver_port}
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        feature = lyr.GetNextFeature()
        assert feature is not None
        assert handler.last_path == "/oapif/collections/castles/items?limit=1000"

    # External JSON component limit, check that the GDAL default is used
    j_data = json.loads(schema)
    j_data["paths"]["/collections/castles/items"]["get"]["parameters"] = [
        external_json_limit
    ]
    j_data = json.dumps(j_data).encode("utf8")
    filedata["/oapif/openapi"] = j_data
    handler = LoggingHandler(filedata, content_type="application/json")

    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%(port)d/oapif" % {"port": gdaltest.webserver_port}
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        feature = lyr.GetNextFeature()
        assert feature is not None
        assert handler.last_path == "/oapif/collections/castles/items?limit=1000"

    # Internal component limit, check that the schema default (5000) is used
    j_data = json.loads(schema)
    j_data["paths"]["/collections/castles/items"]["get"]["parameters"] = [
        internal_json_component_limit
    ]
    j_data["components"]["parameters"]["limit"]["schema"]["default"] = 5000
    j_data = json.dumps(j_data).encode("utf8")
    filedata["/oapif/openapi"] = j_data
    handler = LoggingHandler(filedata, content_type="application/json")

    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%(port)d/oapif" % {"port": gdaltest.webserver_port}
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        feature = lyr.GetNextFeature()
        assert feature is not None
        assert handler.last_path == "/oapif/collections/castles/items?limit=5000"

    # Internal component limit, check that the schema maximum (100) is used
    j_data = json.loads(schema)
    j_data["paths"]["/collections/castles/items"]["get"]["parameters"] = [
        internal_json_component_limit
    ]
    j_data["components"]["parameters"]["limit"]["schema"]["default"] = 50
    j_data["components"]["parameters"]["limit"]["schema"]["maximum"] = 100
    j_data = json.dumps(j_data).encode("utf8")
    filedata["/oapif/openapi"] = j_data
    handler = LoggingHandler(filedata, content_type="application/json")

    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%(port)d/oapif" % {"port": gdaltest.webserver_port}
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        feature = lyr.GetNextFeature()
        assert feature is not None
        assert handler.last_path == "/oapif/collections/castles/items?limit=100"


def test_ogr_oapif_initial_request_page_size():
    """Test initial request page size. Issue GH #4556"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "http://localhost:%d/oapif" % gdaltest.webserver_port,
            gdal.OF_VECTOR,
            allowed_drivers=["OAPIF"],
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    # Use custom INITIAL_REQUEST_PAGE_SIZE
    # Case 1: invalid (< 1)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
            gdal.OF_VECTOR,
            open_options=["INITIAL_REQUEST_PAGE_SIZE=0"],
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    # Case 2: invalid (> max page size)
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
            gdal.OF_VECTOR,
            open_options=["INITIAL_REQUEST_PAGE_SIZE=2000"],
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1

    # Case 3: valid
    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "OAPIF:http://localhost:%d/oapif" % gdaltest.webserver_port,
            open_options=["INITIAL_REQUEST_PAGE_SIZE=30"],
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=30",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetLayerDefn().GetFieldCount() == 1


def test_ogr_oapif_datetime_open_option():
    """Test DATETIME open option"""

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections",
        200,
        {"Content-Type": "application/json"},
        '{ "collections" : [ { "name": "foo" }] }',
    )
    with webserver.install_http_handler(handler):
        ds = gdal.OpenEx(
            "http://localhost:%d/oapif" % gdaltest.webserver_port,
            gdal.OF_VECTOR,
            open_options=["DATETIME=2011-01-03T12:31:00Z"],
            allowed_drivers=["OAPIF"],
        )
    lyr = ds.GetLayer(0)

    handler = webserver.SequentialHandler()
    _add_dummy_root_and_api_pages(handler)
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=20",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&datetime=2011-01-03T12:31:00Z",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    },
                    {
                        "type": "Feature",
                        "properties": {
                            "bar": "baz"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        assert lyr.GetFeatureCount() == 2

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/oapif/collections/foo/items?limit=1000&datetime=2011-01-03T12:31:00Z",
        200,
        {"Content-Type": "application/geo+json"},
        """{ "type": "FeatureCollection", "features": [
                    {
                        "type": "Feature",
                        "properties": {
                            "foo": "bar"
                        }
                    }
                ] }""",
    )
    with webserver.install_http_handler(handler):
        f = lyr.GetNextFeature()
        assert f["foo"] == "bar"

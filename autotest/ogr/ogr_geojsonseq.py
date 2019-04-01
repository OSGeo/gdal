#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGRGeoJSONSeq driver.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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



from osgeo import gdal
from osgeo import osr
from osgeo import ogr

import gdaltest
import pytest


def _ogr_geojsonseq_create(filename, lco, expect_rs):

    ds = ogr.GetDriverByName('GeoJSONSeq').CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('WGS84')
    lyr = ds.CreateLayer('test', srs=sr, options=lco)
    lyr.CreateField(ogr.FieldDefn('foo'))

    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = 'bar"d'
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = 'baz'
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(3 4)'))
    lyr.CreateFeature(f)

    assert not ds.TestCapability(ogr.ODsCCreateLayer)

    with gdaltest.error_handler():
        assert ds.CreateLayer('foo') is None

    ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    first = gdal.VSIFReadL(1, 1, f).decode('ascii')
    gdal.VSIFCloseL(f)
    if expect_rs:
        assert first == '\x1e'
    else:
        assert first == '{'

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar"d' or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 4)':
        f.DumpReadable()
        pytest.fail()
    assert lyr.GetNextFeature() is None
    ds = None

    ogr.GetDriverByName('GeoJSONSeq').DeleteDataSource(filename)


def test_ogr_geojsonseq_lf():
    return _ogr_geojsonseq_create('/vsimem/test', [], False)


def test_ogr_geojsonseq_rs():
    return _ogr_geojsonseq_create('/vsimem/test', ['RS=YES'], True)


def test_ogr_geojsonseq_rs_auto():
    return _ogr_geojsonseq_create('/vsimem/test.geojsons', [], True)


def test_ogr_geojsonseq_inline():

    ds = ogr.Open("""{"type":"Feature","properties":{},"geometry":null}
{"type":"Feature","properties":{},"geometry":null}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2


def test_ogr_geojsonseq_prefix():

    ds = ogr.Open("""GeoJSONSeq:data/test.geojsonl""")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2


def test_ogr_geojsonseq_seq_geometries():

    with gdaltest.config_option('OGR_GEOJSONSEQ_CHUNK_SIZE', '10'):
        ds = ogr.Open("""{"type":"Point","coordinates":[2,49]}
    {"type":"Point","coordinates":[3,50]}""")
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
            f.DumpReadable()
            pytest.fail()


def test_ogr_geojsonseq_seq_geometries_with_errors():

    with gdaltest.error_handler():
        ds = ogr.Open("""{"type":"Point","coordinates":[2,49]}
    {"type":"Point","coordinates":[3,50]}
    foo
    "bar"
    null

    {"type":"Point","coordinates":[3,51]}""")
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
            f.DumpReadable()
            pytest.fail()
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
            f.DumpReadable()
            pytest.fail()
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != 'POINT (3 51)':
            f.DumpReadable()
            pytest.fail()


def test_ogr_geojsonseq_reprojection():

    filename = '/vsimem/ogr_geojsonseq_reprojection.geojsonl'
    ds = ogr.GetDriverByName('GeoJSONSeq').CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=merc +datum=WGS84')
    lyr = ds.CreateLayer('test', srs=sr)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt(
        'POINT(222638.981586547 6242595.9999532)'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    ogr.GetDriverByName('GeoJSONSeq').DeleteDataSource(filename)


def test_ogr_geojsonseq_read_rs_json_pretty():

    ds = ogr.Open('data/test.geojsons')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar' or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 4)':
        f.DumpReadable()
        pytest.fail()
    assert lyr.GetNextFeature() is None


def test_ogr_geojsonseq_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.geojsonl')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1




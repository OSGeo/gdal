#!/usr/bin/env python
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

import sys

sys.path.append('../pymod')

from osgeo import gdal
from osgeo import osr
from osgeo import ogr

import gdaltest


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

    if ds.TestCapability(ogr.ODsCCreateLayer):
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        if ds.CreateLayer('foo') is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    ds = None

    f = gdal.VSIFOpenL(filename, 'rb')
    first = gdal.VSIFReadL(1, 1, f).decode('ascii')
    gdal.VSIFCloseL(f)
    if expect_rs:
        if first != '\x1e':
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if first != '{':
            gdaltest.post_reason('fail')
            return 'fail'

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar"d' or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetNextFeature() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ogr.GetDriverByName('GeoJSONSeq').DeleteDataSource(filename)

    return 'success'


def ogr_geojsonseq_lf():
    return _ogr_geojsonseq_create('/vsimem/test', [], False)


def ogr_geojsonseq_rs():
    return _ogr_geojsonseq_create('/vsimem/test', ['RS=YES'], True)


def ogr_geojsonseq_rs_auto():
    return _ogr_geojsonseq_create('/vsimem/test.geojsons', [], True)


def ogr_geojsonseq_inline():

    ds = ogr.Open("""{"type":"Feature","properties":{},"geometry":null}
{"type":"Feature","properties":{},"geometry":null}""")
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 2:
        print(lyr.GetFeatureCount())
        return 'fail'
    return 'success'


def ogr_geojsonseq_prefix():

    ds = ogr.Open("""GeoJSONSeq:data/test.geojsonl""")
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 2:
        print(lyr.GetFeatureCount())
        return 'fail'
    return 'success'


def ogr_geojsonseq_seq_geometries():

    with gdaltest.config_option('OGR_GEOJSONSEQ_CHUNK_SIZE', '10'):
        ds = ogr.Open("""{"type":"Point","coordinates":[2,49]}
    {"type":"Point","coordinates":[3,50]}""")
        lyr = ds.GetLayer(0)
        if lyr.GetFeatureCount() != 2:
            print(lyr.GetFeatureCount())
            return 'fail'
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
            gdaltest.post_reason('fail')
            f.DumpReadable()
            return 'fail'

    return 'success'


def ogr_geojsonseq_reprojection():

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
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    ogr.GetDriverByName('GeoJSONSeq').DeleteDataSource(filename)

    return 'success'


def ogr_geojsonseq_read_rs_json_pretty():

    ds = ogr.Open('data/test.geojsons')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar' or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetNextFeature() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    return 'success'


def ogr_geojsonseq_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.geojsonl')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'


gdaltest_list = [
    ogr_geojsonseq_lf,
    ogr_geojsonseq_rs,
    ogr_geojsonseq_rs_auto,
    ogr_geojsonseq_inline,
    ogr_geojsonseq_prefix,
    ogr_geojsonseq_seq_geometries,
    ogr_geojsonseq_reprojection,
    ogr_geojsonseq_read_rs_json_pretty,
    ogr_geojsonseq_test_ogrsf,
]

if __name__ == '__main__':

    gdaltest.setup_run('ogr_geojsonseq')

    gdaltest.run_tests(gdaltest_list)

    sys.exit(gdaltest.summarize())

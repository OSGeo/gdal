#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GPX driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
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


import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import gdal
import pytest

pytestmark = pytest.mark.require_driver('GPX')


###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    if ogr.Open('data/gpx/test.gpx') is None:
        pytest.skip()

    yield

    try:
        os.remove('tmp/gpx.gpx')
    except OSError:
        pass

###############################################################################
# Test waypoints gpx layer.


def test_ogr_gpx_1():
    gpx_ds = ogr.Open('data/gpx/test.gpx')

    assert gpx_ds.GetLayerCount() == 5, 'wrong number of layers'

    lyr = gpx_ds.GetLayerByName('waypoints')

    expect = [2, None]

    with gdaltest.error_handler():
        tr = ogrtest.check_features_against_list(lyr, 'ele', expect)
    assert tr

    lyr.ResetReading()

    expect = ['waypoint name', None]

    tr = ogrtest.check_features_against_list(lyr, 'name', expect)
    assert tr

    lyr.ResetReading()

    expect = ['href', None]

    tr = ogrtest.check_features_against_list(lyr, 'link1_href', expect)
    assert tr

    lyr.ResetReading()

    expect = ['text', None]

    tr = ogrtest.check_features_against_list(lyr, 'link1_text', expect)
    assert tr

    lyr.ResetReading()

    expect = ['type', None]

    tr = ogrtest.check_features_against_list(lyr, 'link1_type', expect)
    assert tr

    lyr.ResetReading()

    expect = ['href2', None]

    tr = ogrtest.check_features_against_list(lyr, 'link2_href', expect)
    assert tr

    lyr.ResetReading()

    expect = ['text2', None]

    tr = ogrtest.check_features_against_list(lyr, 'link2_text', expect)
    assert tr

    lyr.ResetReading()

    expect = ['type2', None]

    tr = ogrtest.check_features_against_list(lyr, 'link2_type', expect)
    assert tr

    lyr.ResetReading()

    expect = ['2007/11/25 17:58:00+01', None]

    tr = ogrtest.check_features_against_list(lyr, 'time', expect)
    assert tr

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT (1 0)',
                                      max_error=0.0001) == 0)

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT (4 3)',
                                      max_error=0.0001) == 0)

###############################################################################
# Test routes gpx layer.


def test_ogr_gpx_2():
    gpx_ds = ogr.Open('data/gpx/test.gpx')

    lyr = gpx_ds.GetLayerByName('routes')

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (6 5,9 8,12 11)', max_error=0.0001) == 0

    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING EMPTY', max_error=0.0001) == 0


###############################################################################
# Test route_points gpx layer.

def test_ogr_gpx_3():
    gpx_ds = ogr.Open('data/gpx/test.gpx')

    lyr = gpx_ds.GetLayerByName('route_points')

    expect = ['route point name', None, None]

    tr = ogrtest.check_features_against_list(lyr, 'name', expect)
    assert tr

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'POINT (6 5)', max_error=0.0001) == 0

###############################################################################
# Test tracks gpx layer.


def test_ogr_gpx_4():
    gpx_ds = ogr.Open('data/gpx/test.gpx')

    lyr = gpx_ds.GetLayerByName('tracks')

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'MULTILINESTRING ((15 14,18 17),(21 20,24 23))', max_error=0.0001) == 0

    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'MULTILINESTRING EMPTY', max_error=0.0001) == 0

    feat = lyr.GetNextFeature()
    f_geom = feat.GetGeometryRef()
    assert f_geom.ExportToWkt() == 'MULTILINESTRING EMPTY'

###############################################################################
# Test route_points gpx layer.


def test_ogr_gpx_5():
    gpx_ds = ogr.Open('data/gpx/test.gpx')

    lyr = gpx_ds.GetLayerByName('track_points')

    expect = ['track point name', None, None, None]

    tr = ogrtest.check_features_against_list(lyr, 'name', expect)
    assert tr

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'POINT (15 14)', max_error=0.0001) == 0

###############################################################################
# Copy our small gpx file to a new gpx file.


def test_ogr_gpx_6():
    gpx_ds = ogr.Open('data/gpx/test.gpx')
    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ogr.GetDriverByName('CSV').DeleteDataSource('tmp/gpx.gpx')
        gdal.PopErrorHandler()
    except:
        pass

    co_opts = []

    # Duplicate waypoints
    gpx_lyr = gpx_ds.GetLayerByName('waypoints')

    gpx2_ds = ogr.GetDriverByName('GPX').CreateDataSource('tmp/gpx.gpx',
                                                          options=co_opts)

    gpx2_lyr = gpx2_ds.CreateLayer('waypoints', geom_type=ogr.wkbPoint)

    gpx_lyr.ResetReading()

    dst_feat = ogr.Feature(feature_def=gpx2_lyr.GetLayerDefn())

    feat = gpx_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx2_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

        feat = gpx_lyr.GetNextFeature()

    # Duplicate routes
    gpx_lyr = gpx_ds.GetLayerByName('routes')

    gpx2_lyr = gpx2_ds.CreateLayer('routes', geom_type=ogr.wkbLineString)

    gpx_lyr.ResetReading()

    dst_feat = ogr.Feature(feature_def=gpx2_lyr.GetLayerDefn())

    feat = gpx_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx2_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

        feat = gpx_lyr.GetNextFeature()

    # Duplicate tracks
    gpx_lyr = gpx_ds.GetLayerByName('tracks')

    gpx2_lyr = gpx2_ds.CreateLayer('tracks', geom_type=ogr.wkbMultiLineString)

    gpx_lyr.ResetReading()

    dst_feat = ogr.Feature(feature_def=gpx2_lyr.GetLayerDefn())

    feat = gpx_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx2_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

        feat = gpx_lyr.GetNextFeature()

###############################################################################
# Output extra fields as <extensions>.


def test_ogr_gpx_7():

    bna_ds = ogr.Open('data/gpx/csv_for_gpx.csv')

    try:
        os.remove('tmp/gpx.gpx')
    except OSError:
        pass

    co_opts = ['GPX_USE_EXTENSIONS=yes']

    # Duplicate waypoints
    bna_lyr = bna_ds.GetLayerByName('csv_for_gpx')

    gpx_ds = ogr.GetDriverByName('GPX').CreateDataSource('tmp/gpx.gpx',
                                                                  options=co_opts)

    gpx_lyr = gpx_ds.CreateLayer('waypoints', geom_type=ogr.wkbPoint)

    bna_lyr.ResetReading()

    for i in range(bna_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = bna_lyr.GetLayerDefn().GetFieldDefn(i)
        gpx_lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=gpx_lyr.GetLayerDefn())

    feat = bna_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

        feat = bna_lyr.GetNextFeature()

    gpx_ds = None

# Now check that the extensions fields have been well written
    gpx_ds = ogr.Open('tmp/gpx.gpx')
    gpx_lyr = gpx_ds.GetLayerByName('waypoints')

    expect = ['PID1', 'PID2']

    tr = ogrtest.check_features_against_list(gpx_lyr, 'ogr_Primary_ID', expect)
    assert tr

    gpx_lyr.ResetReading()

    expect = ['SID1', 'SID2']

    tr = ogrtest.check_features_against_list(gpx_lyr, 'ogr_Secondary_ID', expect)
    assert tr

    gpx_lyr.ResetReading()

    expect = ['TID1', None]

    tr = ogrtest.check_features_against_list(gpx_lyr, 'ogr_Third_ID', expect)
    assert tr

###############################################################################
# Output extra fields as <extensions>.


def test_ogr_gpx_8():

    try:
        os.remove('tmp/gpx.gpx')
    except OSError:
        pass

    gpx_ds = ogr.GetDriverByName('GPX').CreateDataSource('tmp/gpx.gpx', options=['LINEFORMAT=LF'])

    lyr = gpx_ds.CreateLayer('route_points', geom_type=ogr.wkbPoint)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    feat.SetField('route_name', 'ROUTE_NAME')
    feat.SetField('route_fid', 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(3 50)')
    feat.SetField('route_name', '--ignored--')
    feat.SetField('route_fid', 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(3 51)')
    feat.SetField('route_name', 'ROUTE_NAME2')
    feat.SetField('route_fid', 1)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(3 49)')
    feat.SetField('route_fid', 1)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    lyr = gpx_ds.CreateLayer('track_points', geom_type=ogr.wkbPoint)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    feat.SetField('track_name', 'TRACK_NAME')
    feat.SetField('track_fid', 0)
    feat.SetField('track_seg_id', 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(3 50)')
    feat.SetField('track_name', '--ignored--')
    feat.SetField('track_fid', 0)
    feat.SetField('track_seg_id', 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(3 51)')
    feat.SetField('track_fid', 0)
    feat.SetField('track_seg_id', 1)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(3 49)')
    feat.SetField('track_name', 'TRACK_NAME2')
    feat.SetField('track_fid', 1)
    feat.SetField('track_seg_id', 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    gpx_ds = None

    f = open('tmp/gpx.gpx', 'rb')
    f_ref = open('data/gpx/ogr_gpx_8_ref.txt', 'rb')
    f_content = f.read()
    f_ref_content = f_ref.read()
    f.close()
    f_ref.close()

    assert f_content.find(f_ref_content) != -1, 'did not get expected result'

###############################################################################
# Parse file with a <time> extension at track level (#6237)


def test_ogr_gpx_9():

    ds = ogr.Open('data/gpx/track_with_time_extension.gpx')
    lyr = ds.GetLayerByName('tracks')
    f = lyr.GetNextFeature()
    if f['time'] != '2015-10-11T15:06:33Z':
        f.DumpReadable()
        pytest.fail('did not get expected result')

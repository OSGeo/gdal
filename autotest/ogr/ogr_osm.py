#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR OSM driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import ogr
from osgeo import gdal


import gdaltest
import ogrtest
import pytest

###############################################################################
# Test .pbf


def test_ogr_osm_1(filename='data/osm/test.pbf'):

    ogrtest.osm_drv = ogr.GetDriverByName('OSM')
    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = ogr.Open(filename)
    if ds is None:
        if filename == 'data/osm/test.osm':
            ogrtest.osm_drv_parse_osm = False
            if gdal.GetLastErrorMsg().find('OSM XML detected, but Expat parser not available') == 0:
                pytest.skip()

        pytest.fail()
    else:
        if filename == 'data/osm/test.osm':
            ogrtest.osm_drv_parse_osm = True

    # Test points
    lyr = ds.GetLayer('points')
    assert lyr.GetGeomType() == ogr.wkbPoint

    sr = lyr.GetSpatialRef()
    assert (sr.ExportToWkt().find('GEOGCS["WGS 84",DATUM["WGS_1984",') == 0 or \
       sr.ExportToWkt().find('GEOGCS["GCS_WGS_1984",DATUM["WGS_1984"') == 0)

    if filename == 'data/osm/test.osm':
        assert lyr.GetExtent() == (2.0, 3.0, 49.0, 50.0)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '3' or \
       feat.GetFieldAsString('name') != 'Some interesting point' or \
       feat.GetFieldAsString('other_tags') != '"foo"=>"bar","bar"=>"baz"':
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (3.0 49.5)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        pytest.fail()

    # Test lines
    lyr = ds.GetLayer('lines')
    assert lyr.GetGeomType() == ogr.wkbLineString

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '1' or \
       feat.GetFieldAsString('highway') != 'motorway' or \
       feat.GetFieldAsInteger('z_order') != 9 or \
       feat.GetFieldAsString('other_tags') != '"foo"=>"bar"':
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '6':
        feat.DumpReadable()
        pytest.fail()

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 49,3 50,2 50,2 49)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        pytest.fail()

    # Test multipolygons
    lyr = ds.GetLayer('multipolygons')
    if filename == 'tmp/ogr_osm_3':
        assert lyr.GetGeomType() == ogr.wkbPolygon
    else:
        assert lyr.GetGeomType() == ogr.wkbMultiPolygon

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '1' or \
       feat.GetFieldAsString('type') != 'multipolygon' or \
       feat.GetFieldAsString('natural') != 'forest':
        feat.DumpReadable()
        pytest.fail()

    if filename == 'tmp/ogr_osm_3':
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((2 49,2 50,3 50,3 49,2 49),(2.1 49.1,2.2 49.1,2.2 49.2,2.1 49.2,2.1 49.1))')) != 0:
            feat.DumpReadable()
            pytest.fail()
    else:
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTIPOLYGON (((2 49,3 49,3 50,2 50,2 49),(2.1 49.1,2.2 49.1,2.2 49.2,2.1 49.2,2.1 49.1)))')) != 0:
            feat.DumpReadable()
            pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '5' or \
       feat.GetFieldAsString('type') != 'multipolygon' or \
       feat.GetFieldAsString('natural') != 'wood':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_way_id') != '8' or \
       feat.GetFieldAsString('name') != 'standalone_polygon':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        pytest.fail()

    # Test multilinestrings
    lyr = ds.GetLayer('multilinestrings')
    if filename == 'tmp/ogr_osm_3':
        assert lyr.GetGeomType() == ogr.wkbLineString
    else:
        assert lyr.GetGeomType() == ogr.wkbMultiLineString

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '3' or \
       feat.GetFieldAsString('type') != 'route':
        feat.DumpReadable()
        pytest.fail()

    if filename == 'tmp/ogr_osm_3':
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')) != 0:
            feat.DumpReadable()
            pytest.fail()
    else:
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTILINESTRING ((2 49,3 50))')) != 0:
            feat.DumpReadable()
            pytest.fail()

    feat = lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        pytest.fail()

    # Test other_relations
    lyr = ds.GetLayer('other_relations')
    if filename == 'tmp/ogr_osm_3':
        assert lyr is None
    else:
        assert lyr.GetGeomType() == ogr.wkbGeometryCollection

        feat = lyr.GetNextFeature()
        if feat.GetFieldAsString('osm_id') != '4' or \
           feat.GetFieldAsString('type') != 'other_type':
            feat.DumpReadable()
            pytest.fail()

        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (2 49),LINESTRING (2 49,3 50))')) != 0:
            feat.DumpReadable()
            pytest.fail()

        feat = lyr.GetNextFeature()
        if feat is not None:
            feat.DumpReadable()
            pytest.fail()

    if ds.GetDriver().GetName() == 'OSM':
        sql_lyr = ds.ExecuteSQL("GetBytesRead()")
        assert sql_lyr is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is None
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        sql_lyr.GetLayerDefn()
        sql_lyr.TestCapability("foo")
        ds.ReleaseResultSet(sql_lyr)

    ds = None

###############################################################################
# Test .osm


def test_ogr_osm_2():
    return test_ogr_osm_1('data/osm/test.osm')


###############################################################################
# Test reaching the max number of indexed keys


def test_ogr_osm_limit_keys():
    with gdaltest.config_option('OSM_MAX_INDEXED_KEYS', '0'):
        return test_ogr_osm_1('data/osm/test.osm')


###############################################################################
# Test reaching the max number of indexed values per key


def test_ogr_osm_limit_values_per_key():
    with gdaltest.config_option('OSM_MAX_INDEXED_VALUES_PER_KEY', '0'):
        return test_ogr_osm_1('data/osm/test.osm')

###############################################################################
# Test ogr2ogr


def test_ogr_osm_3(options=None, all_layers=False):

    if ogrtest.osm_drv is None:
        pytest.skip()

    filepath = 'tmp/ogr_osm_3'
    if os.path.exists(filepath):
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource(filepath)

    if options is not None:
        options = ' ' + options
    else:
        options = ''
    if all_layers:
        layers = ''
    else:
        layers = 'points lines multipolygons multilinestrings '
    with gdaltest.error_handler():
        gdal.VectorTranslate('tmp/ogr_osm_3', 'data/osm/test.pbf', options=layers + options)

    ret = test_ogr_osm_1(filepath)

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource(filepath)

    return ret

###############################################################################
# Test ogr2ogr with --config OSM_USE_CUSTOM_INDEXING NO and -skip


def test_ogr_osm_3_sqlite_nodes():
    gdal.SetConfigOption('OSM_USE_CUSTOM_INDEXING', 'NO')
    ret = test_ogr_osm_3(options='-skip')
    gdal.SetConfigOption('OSM_USE_CUSTOM_INDEXING', None)
    return ret

###############################################################################
# Test ogr2ogr with --config OSM_COMPRESS_NODES YES


def test_ogr_osm_3_custom_compress_nodes():
    gdal.SetConfigOption('OSM_COMPRESS_NODES', 'YES')
    ret = test_ogr_osm_3()
    gdal.SetConfigOption('OSM_COMPRESS_NODES', None)
    return ret

###############################################################################
# Test ogr2ogr with all layers


def test_ogr_osm_3_all_layers():
    return test_ogr_osm_3(options='-skip', all_layers=True)

###############################################################################
# Test optimization when reading only the points layer through a SQL request


def test_ogr_osm_4():

    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = ogr.Open('data/osm/test.pbf')
    assert ds is not None

    sql_lyr = ds.ExecuteSQL('SELECT * FROM points')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    assert not is_none

    # Test spatial filter

    lyr = ds.GetLayerByName('points')
    lyr.SetSpatialFilterRect(0, 0, 0, 0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is None, 'Zero filter '

    with gdaltest.error_handler():
        lyr.SetSpatialFilter(None)

        # Change layer
        sql_lyr = ds.ExecuteSQL('SELECT * FROM lines')

        feat = sql_lyr.GetNextFeature()
        is_none = feat is None

        ds.ReleaseResultSet(sql_lyr)

    assert not is_none

    # Change layer
    sql_lyr = ds.ExecuteSQL('SELECT * FROM points')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    assert not is_none

###############################################################################
# Test optimizations for early attribute filter evaluation


def test_ogr_osm_5():

    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = ogr.Open('data/osm/test.pbf')
    assert ds is not None

    tests = [['points', '3', True],
             ['points', 'foo', False],
             ['lines', '1', True],
             ['lines', 'foo', False],
             ['multipolygons', '1', True],
             ['multipolygons', 'foo', False],
             ['multilinestrings', '3', True],
             ['multilinestrings', 'foo', False]]

    for test in tests:
        sql_lyr = ds.ExecuteSQL("SELECT * FROM %s WHERE osm_id = '%s'" % (test[0], test[1]))
        feat = sql_lyr.GetNextFeature()
        is_none = feat is None
        feat = None
        ds.ReleaseResultSet(sql_lyr)

        assert (test[2] ^ is_none)

    sql_lyr = ds.ExecuteSQL("select * from multipolygons where type = 'multipolygon'")
    feat = sql_lyr.GetNextFeature()
    is_none = feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert not is_none, test

###############################################################################
# Test ogr2ogr -sql


def test_ogr_osm_6():

    if ogrtest.osm_drv is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/ogr_osm_6')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_6')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/ogr_osm_6 data/osm/test.pbf -sql "select * from multipolygons" -progress')

    ds = ogr.Open('tmp/ogr_osm_6')
    lyr = ds.GetLayer(0)
    count = lyr.GetFeatureCount()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_6')

    assert count == 3

###############################################################################
# Test optimization when reading only the points layer through a SQL request
# with SQLite dialect (#4825)


def test_ogr_osm_7():

    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = ogr.Open('data/osm/test.pbf')
    assert ds is not None

    sql_lyr = ds.ExecuteSQL('SELECT * FROM points LIMIT 10', dialect='SQLite')
    if sql_lyr is None and gdal.GetLastErrorMsg().find('automatic extension loading failed') != 0:
        pytest.skip()
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    assert count == 1

###############################################################################
# Test 64-bit ids


def test_ogr_osm_8():

    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = ogr.Open('data/osm/base-64.osm.pbf')
    assert ds is not None

    lyr = ds.GetLayerByName('points')
    lyr.SetAttributeFilter("osm_id = '4294967934'")
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'Treetops' or \
       ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (-61.7964321 17.1498319)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('multipolygons')
    feat = lyr.GetFeature(1113)

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTIPOLYGON (((-61.7780345 17.140634,-61.7777002 17.1406069,-61.7776854 17.1407739,-61.7779131 17.1407923,-61.7779158 17.1407624,-61.7780224 17.140771,-61.7780345 17.140634)))')) != 0:
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Same as ogr_osm_8 but with OSM_USE_CUSTOM_INDEXING=NO


def test_ogr_osm_9():

    if ogrtest.osm_drv is None:
        pytest.skip()

    old_val = gdal.GetConfigOption('OSM_USE_CUSTOM_INDEXING')
    gdal.SetConfigOption('OSM_USE_CUSTOM_INDEXING', 'NO')
    ret = test_ogr_osm_8()
    gdal.SetConfigOption('OSM_USE_CUSTOM_INDEXING', old_val)

    return ret

###############################################################################
# Some error conditions


def test_ogr_osm_10():

    if ogrtest.osm_drv is None:
        pytest.skip()

    # A file that does not exist.
    ds = ogr.Open('/nonexistent/foo.osm')
    assert ds is None

    # Empty .osm file
    f = gdal.VSIFOpenL('/vsimem/foo.osm', 'wb')
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/foo.osm')
    assert ds is None

    gdal.Unlink('/vsimem/foo.osm')

    # Empty .pbf file
    f = gdal.VSIFOpenL('/vsimem/foo.pbf', 'wb')
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/foo.pbf')
    assert ds is None

    gdal.Unlink('/vsimem/foo.pbf')

    if ogrtest.osm_drv_parse_osm:
        # Invalid .osm file
        f = gdal.VSIFOpenL('/vsimem/foo.osm', 'wb')
        data = "<osm>"
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

        ds = ogr.Open('/vsimem/foo.osm')
        lyr = ds.GetLayer(0)
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        feat = lyr.GetNextFeature()
        gdal.PopErrorHandler()
        assert gdal.GetLastErrorMsg() != ''
        ds = None

        gdal.Unlink('/vsimem/foo.osm')

    # Invalid .pbf file
    f = gdal.VSIFOpenL('/vsimem/foo.pbf', 'wb')
    data = "OSMHeader\n"
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/foo.pbf')
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    ds = None

    gdal.Unlink('/vsimem/foo.pbf')

    # Test million laugh pattern
    if ogrtest.osm_drv_parse_osm:
        ds = ogr.Open('data/osm/billionlaugh.osm')
        lyr = ds.GetLayer(0)
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        feat = lyr.GetNextFeature()
        gdal.PopErrorHandler()
        assert feat is None and gdal.GetLastErrorMsg() != ''


###############################################################################
# Test all_tags


def test_ogr_osm_11():

    if ogrtest.osm_drv is None:
        pytest.skip()

    gdal.SetConfigOption('OSM_CONFIG_FILE', 'data/osm/osmconf_alltags.ini')
    ds = ogr.Open('data/osm/test.pbf')
    gdal.SetConfigOption('OSM_CONFIG_FILE', None)
    lyr = ds.GetLayerByName('points')
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '3' or \
       feat.GetFieldAsString('name') != 'Some interesting point' or \
       feat.GetFieldAsString('all_tags') != '"name"=>"Some interesting point","foo"=>"bar","bar"=>"baz"':
        feat.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('lines')
    feat = lyr.GetNextFeature()
    if feat.GetField('z_order') != 9:
        feat.DumpReadable()
        pytest.fail()



###############################################################################
# Test that attribute filter set on a line layer is well taken into
# account

def test_ogr_osm_12():

    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = ogr.Open('data/osm/test.pbf')
    for i in range(2):
        lay = ds.GetLayerByIndex(i)
        lay.SetAttributeFilter("highway IS NOT NULL")
        # lay.GetNextFeature()
        lay.ResetReading()
        feat = lay.GetNextFeature()
        count = 0
        while feat is not None:
            count = count + 1
            feat = lay.GetNextFeature()
        assert i != 1 or count == 1
    ds = None

###############################################################################
# Test test_uncompressed_dense_true_nometadata.pbf


def test_ogr_osm_test_uncompressed_dense_true_nometadata_pbf():
    return test_ogr_osm_1('data/osm/test_uncompressed_dense_true_nometadata.pbf')

###############################################################################
# Test test_uncompressed_dense_false.pbf


def test_ogr_osm_test_uncompressed_dense_false_pbf():
    return test_ogr_osm_1('data/osm/test_uncompressed_dense_false.pbf')

# Special case: if an object has a 'osm_id' key, then do not use it to override
# "our" osm_id field. But put it in other_fields (#6347)


def test_ogr_osm_13():

    if ogrtest.osm_drv is None or not ogrtest.osm_drv_parse_osm:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_osm_13.osm',
                           """<osm><node id="123" lon="2" lat="49"><tag k="osm_id" v="0"/></node></osm>""")

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_osm_13.osm')
    if ds is None:
        gdal.Unlink('/vsimem/ogr_osm_13.osm')
        pytest.skip()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['osm_id'] != '123' or f['other_tags'] != '"osm_id"=>"0"':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_osm_13.osm')

###############################################################################
# Test that we handle polygons in other_relations (#6475)


def test_ogr_osm_14():

    if ogrtest.osm_drv is None or not ogrtest.osm_drv_parse_osm:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_osm_14.osm',
                           """<osm>
  <node id="1" lat="2" lon="49"/>
  <node id="2" lat="2.1" lon="49"/>
  <node id="3" lat="2.1" lon="49.1"/>
  <way id="1">
    <nd ref="1"/>
    <nd ref="2"/>
    <nd ref="3"/>
    <nd ref="1"/>
    <tag k="area" v="yes"/>
    <tag k="bus" v="yes"/>
  </way>
  <relation id="1">
    <member type="way" ref="1" role=""/>
    <tag k="route" v="bus"/>
  </relation>
</osm>""")

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_osm_14.osm')
    if ds is None:
        gdal.Unlink('/vsimem/ogr_osm_14.osm')
        pytest.skip()
    sql_lyr = ds.ExecuteSQL('SELECT * FROM other_relations')
    f = sql_lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'GEOMETRYCOLLECTION (POLYGON ((49 2,49.0 2.1,49.1 2.1,49 2)))':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/ogr_osm_14.osm')

###############################################################################
# Test Dataset.GetNextFeature()


def ogr_osm_15_progresscbk_return_true(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def ogr_osm_15_progresscbk_return_false(pct, msg, user_data):
    # pylint: disable=unused-argument
    return 0


def test_ogr_osm_15():

    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = gdal.OpenEx('data/osm/test.pbf')

    assert ds.TestCapability(ogr.ODsCRandomLayerRead) == 1

    count = 0
    last_pct = 0
    while True:
        f, l, pct = ds.GetNextFeature(include_pct=True)
        assert pct >= last_pct
        last_pct = pct
        if f is None:
            assert l is None
            break
        # f.DumpReadable()
        count += 1
        if f.GetDefnRef().GetName() != l.GetName():
            f.DumpReadable()
            pytest.fail(l.GetName())

    assert count == 8

    assert last_pct == 1.0

    f, l, pct = ds.GetNextFeature(include_pct=True)
    assert f is None and l is None and pct == 1.0

    ds.ResetReading()
    for i in range(count):
        f, lyr = ds.GetNextFeature()
        # f.DumpReadable()
        assert not (f is None or lyr is None), i

    ds.ResetReading()
    f, lyr = ds.GetNextFeature(callback=ogr_osm_15_progresscbk_return_false)
    assert f is None and lyr is None

    ds.ResetReading()
    pct_array = [0]
    f, lyr = ds.GetNextFeature(callback=ogr_osm_15_progresscbk_return_true, callback_data=pct_array)
    assert not (f is None or lyr is None)
    assert pct_array[0] == 1.0

    # ds = gdal.OpenEx('/home/even/gdal/data/osm/france.osm.pbf')
    # ds.ExecuteSQL('SET interest_layers = relations')
    # def test(pct, msg, unused):
    #    print(pct)
    # f, l = ds.GetNextFeature( callback = test)
    # print(f)

    ds = None

###############################################################################
# Test laundering of tags (https://github.com/OSGeo/gdal/pull/161)


def test_ogr_osm_16():

    if ogrtest.osm_drv is None or not ogrtest.osm_drv_parse_osm:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_osm_16.osm',
                           """<osm>
  <node id="1" lat="2" lon="49">
    <tag k="foo:baar" v="val"/>
    <tag k="foo:bar" v="val2"/>
  </node>
</osm>""")

    gdal.FileFromMemBuffer('/vsimem/ogr_osm_16_conf.ini',
                           """#
attribute_name_laundering=yes

[points]
attributes=foo:baar,foo:bar
""")

    ds = gdal.OpenEx('/vsimem/ogr_osm_16.osm', open_options=['CONFIG_FILE=/vsimem/ogr_osm_16_conf.ini'])
    lyr = ds.GetLayerByName('points')
    f = lyr.GetNextFeature()
    if f['foo_baar'] != 'val' or f['foo_bar'] != 'val2':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_osm_16.osm')
    gdal.Unlink('/vsimem/ogr_osm_16_conf.ini')

###############################################################################
# Test converting an empty OSM file (this essentially tests the behaviour of
# GDALVectorTranslate() in random feature mode, when there is no feature)


def test_ogr_osm_17():

    if ogrtest.osm_drv is None or not ogrtest.osm_drv_parse_osm:
        pytest.skip()

    with gdaltest.error_handler():
        gdal.VectorTranslate('/vsimem/ogr_osm_17', 'data/osm/empty.osm', options='-skip')

    ds = ogr.Open('/vsimem/ogr_osm_17')
    layer_count = ds.GetLayerCount()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/ogr_osm_17')

    assert layer_count == 4

###############################################################################
# Test correct reading of .pbf files with multiple densenode blocks and
# regarding EOF


def test_ogr_osm_18():

    if ogrtest.osm_drv is None:
        pytest.skip()

    ds = ogr.Open('data/osm/two_points.pbf')
    lyr = ds.GetLayerByName('points')
    count = 0
    for f in lyr:
        count += 1
    ds = None

    assert count == 2




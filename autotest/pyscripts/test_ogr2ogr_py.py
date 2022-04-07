#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogr2ogr.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
import os
import shutil
import pytest

sys.path.append('../ogr')

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest
import ogrtest
import test_cli_utilities
import test_py_scripts

###############################################################################
# Simple test


def test_ogr2ogr_py_1():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')
    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    feat0 = ds.GetLayer(0).GetFeature(0)
    assert feat0.GetFieldAsDouble('AREA') == 215229.266, \
        'Did not get expected value for field AREA'
    assert feat0.GetFieldAsString('PRFEDEA') == '35043411', \
        'Did not get expected value for field PRFEDEA'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -sql


def test_ogr2ogr_py_2():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -sql "select * from poly"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -spat


def test_ogr2ogr_py_3():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -spat 479609 4764629 479764 4764817')

    ds = ogr.Open('tmp/poly.shp')
    if ogrtest.have_geos():
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4
    else:
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 5
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -where


def test_ogr2ogr_py_4():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -where "EAS_ID=171"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -append


def test_ogr2ogr_py_5():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')
    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-update -append tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20

    feat10 = ds.GetLayer(0).GetFeature(10)
    assert feat10.GetFieldAsDouble('AREA') == 215229.266, \
        'Did not get expected value for field AREA'
    assert feat10.GetFieldAsString('PRFEDEA') == '35043411', \
        'Did not get expected value for field PRFEDEA'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -overwrite


def test_ogr2ogr_py_6():

    ogr_pg = pytest.importorskip('ogr_pg')

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ogr_pg.test_ogr_pg_1()
    if gdaltest.pg_ds is None:
        pytest.skip()
    gdaltest.pg_ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-f PostgreSQL PG:"' + gdaltest.pg_connection_string + '" '+test_py_scripts.get_data_path('ogr')+'poly.shp -nln tpoly')
    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-update -overwrite -f PostgreSQL PG:"' + gdaltest.pg_connection_string + '" '+test_py_scripts.get_data_path('ogr')+'poly.shp -nln tpoly')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    assert ds is not None and ds.GetLayerByName('tpoly').GetFeatureCount() == 10
    ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

###############################################################################
# Test -gt


def test_ogr2ogr_py_7():

    ogr_pg = pytest.importorskip('ogr_pg')

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ogr_pg.test_ogr_pg_1()
    if gdaltest.pg_ds is None:
        pytest.skip()
    gdaltest.pg_ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-f PostgreSQL PG:"' + gdaltest.pg_connection_string + '" '+test_py_scripts.get_data_path('ogr')+'poly.shp -nln tpoly -gt 1')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    assert ds is not None and ds.GetLayerByName('tpoly').GetFeatureCount() == 10
    ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

###############################################################################
# Test -t_srs


def test_ogr2ogr_py_8():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-t_srs EPSG:4326 tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert str(ds.GetLayer(0).GetSpatialRef()).find('1984') != -1
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -a_srs


def test_ogr2ogr_py_9():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-a_srs EPSG:4326 tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert str(ds.GetLayer(0).GetSpatialRef()).find('1984') != -1
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -select


def test_ogr2ogr_py_10():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    # Voluntary don't use the exact case of the source field names (#4502)
    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-select eas_id,prfedea tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    feat = lyr.GetNextFeature()
    ret = 'success'
    if feat.GetFieldAsDouble('EAS_ID') != 168:
        gdaltest.post_reason('did not get expected value for EAS_ID')
        print(feat.GetFieldAsDouble('EAS_ID'))
        ret = 'fail'
    elif feat.GetFieldAsString('PRFEDEA') != '35043411':
        gdaltest.post_reason('did not get expected value for PRFEDEA')
        print(feat.GetFieldAsString('PRFEDEA'))
        ret = 'fail'
    feat = None
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

    return ret

###############################################################################
# Test -lco


def test_ogr2ogr_py_11():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-lco SHPT=POLYGONZ tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -nlt


def test_ogr2ogr_py_12():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-nlt POLYGON25D tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Add explicit source layer name


def test_ogr2ogr_py_13():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp poly')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -segmentize


@pytest.mark.skip()
def test_ogr2ogr_py_14():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-segmentize 100 tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp poly')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryRef(0).GetPointCount() == 36
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -overwrite with a shapefile


def test_ogr2ogr_py_15():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    # Overwrite
    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-overwrite tmp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -fid


def test_ogr2ogr_py_16():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-fid 8 tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    src_ds = ogr.Open(test_py_scripts.get_data_path('ogr') + 'poly.shp')
    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    src_feat = src_ds.GetLayer(0).GetFeature(8)
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetField("EAS_ID") == src_feat.GetField("EAS_ID")
    ds.Destroy()
    src_ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -progress


def test_ogr2ogr_py_17():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    ret = test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-progress tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')
    assert ret.find('0...10...20...30...40...50...60...70...80...90...100 - done.') != -1

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -wrapdateline


@pytest.mark.skip()
def test_ogr2ogr_py_18():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/wrapdateline_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_src.shp')
    except OSError:
        pass

    try:
        os.stat('tmp/wrapdateline_dst.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_dst.shp')
    except OSError:
        pass

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/wrapdateline_src.shp')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32660)
    lyr = ds.CreateLayer('wrapdateline_src', srs=srs)
    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POLYGON((700000 4000000,800000 4000000,800000 3000000,700000 3000000,700000 4000000))')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)
    feat.Destroy()
    ds.Destroy()

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-wrapdateline -t_srs EPSG:4326 tmp/wrapdateline_dst.shp tmp/wrapdateline_src.shp')

    expected_wkt = 'MULTIPOLYGON (((179.222391385437419 36.124095832129363,180.0 36.10605558800065,180.0 27.090340569400169,179.017505655195095 27.107979523625211,179.222391385437419 36.124095832129363)),((-180.0 36.10605558800065,-179.667822828781084 36.098349195413753,-179.974688335419557 27.089886143076747,-180.0 27.090340569400169,-180.0 36.10605558800065)))'
    expected_geom = ogr.CreateGeometryFromWkt(expected_wkt)
    ds = ogr.Open('tmp/wrapdateline_dst.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    ret = ogrtest.check_feature_geometry(feat, expected_geom)
    feat.Destroy()
    expected_geom.Destroy()
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_src.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_dst.shp')

    assert ret == 0

###############################################################################
# Test -clipsrc


def test_ogr2ogr_py_19():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -clipsrc spat_extent -spat 479609 4764629 479764 4764817')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test correct remap of fields when laundering to Shapefile format
# Test that the data is going into the right field
# FIXME: Any field is skipped if a subsequent field with same name is found.


def test_ogr2ogr_py_20():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.remove('tmp/Fields.dbf')
    except OSError:
        pass

    expected_fields = ['a',
                       'A_1',
                       'a_1_2',
                       'aaaaaAAAAA',
                       'aAaaaAAA_1',
                       'aaaaaAAAAB',
                       'aaaaaAAA_2',
                       'aaaaaAAA_3',
                       'aaaaaAAA_4',
                       'aaaaaAAA_5',
                       'aaaaaAAA_6',
                       'aaaaaAAA_7',
                       'aaaaaAAA_8',
                       'aaaaaAAA_9',
                       'aaaaaAAA10']
    expected_data = ['1',
                     '2',
                     '3',
                     '4',
                     '5',
                     '6',
                     '7',
                     '8',
                     '9',
                     '10',
                     '11',
                     '12',
                     '13',
                     '14',
                     '15']

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp '+test_py_scripts.get_data_path('utilities')+'Fields.csv')

    ds = ogr.Open('tmp/Fields.dbf')

    assert ds is not None
    layer_defn = ds.GetLayer(0).GetLayerDefn()
    if layer_defn.GetFieldCount() != 15:
        ds.Destroy()
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/Fields.dbf')
        pytest.fail('Unexpected field count: ' + str(ds.GetLayer(0).GetLayerDefn().GetFieldCount()))

    error_occurred = False
    feat = ds.GetLayer(0).GetNextFeature()
    for i in range(layer_defn.GetFieldCount()):
        if layer_defn.GetFieldDefn(i).GetNameRef() != expected_fields[i]:
            print('Expected ', expected_fields[i], ',but got', layer_defn.GetFieldDefn(i).GetNameRef())
            error_occurred = True
        if feat.GetFieldAsString(i) != expected_data[i]:
            print('Expected the value ', expected_data[i], ',but got', feat.GetFieldAsString(i))
            error_occurred = True

    ds.Destroy()
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/Fields.dbf')

    assert not error_occurred

###############################################################################
# Test ogr2ogr when the output driver has already created the fields
# at dataset creation (#3247)


def test_ogr2ogr_py_21():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()


    try:
        os.remove('tmp/testogr2ogr21.gpx')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr',
                         ' -f GPX tmp/testogr2ogr21.gpx '+test_py_scripts.get_data_path('utilities')+'dataforogr2ogr21.csv ' +
                         '-sql "SELECT name AS route_name, 0 as route_fid FROM dataforogr2ogr21" -nlt POINT -nln route_points')
    assert '<name>NAME</name>' in open('tmp/testogr2ogr21.gpx', 'rt').read()
    os.remove('tmp/testogr2ogr21.gpx')

###############################################################################
# Test ogr2ogr when the output driver delays the destination layer defn creation (#3384)

def test_ogr2ogr_py_22():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'ogr2ogr',
                                  '-f "MapInfo File" tmp/testogr2ogr22.mif '+test_py_scripts.get_data_path('utilities')+'dataforogr2ogr21.csv ' +
                                  '-sql "SELECT comment, name FROM dataforogr2ogr21" -nlt POINT')
    ds = ogr.Open('tmp/testogr2ogr22.mif')

    assert ds is not None
    ds.GetLayer(0).GetLayerDefn()
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('name') != 'NAME' or \
       feat.GetFieldAsString('comment') != 'COMMENT':
        print(feat.GetFieldAsString('comment'))
        ds.Destroy()
        ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testogr2ogr22.mif')
        pytest.fail(feat.GetFieldAsString('name'))

    ds.Destroy()
    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testogr2ogr22.mif')

###############################################################################
# Same as previous but with -select


def test_ogr2ogr_py_23():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() +
                         ' -f "MapInfo File" tmp/testogr2ogr23.mif '+test_py_scripts.get_data_path('utilities')+'dataforogr2ogr21.csv ' +
                         '-sql "SELECT comment, name FROM dataforogr2ogr21" -select comment,name -nlt POINT')
    ds = ogr.Open('tmp/testogr2ogr23.mif')

    assert ds is not None
    ds.GetLayer(0).GetLayerDefn()
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('name') != 'NAME' or \
       feat.GetFieldAsString('comment') != 'COMMENT':
        print(feat.GetFieldAsString('comment'))
        ds.Destroy()
        ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testogr2ogr23.mif')
        pytest.fail(feat.GetFieldAsString('name'))

    ds.Destroy()
    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testogr2ogr23.mif')

###############################################################################
# Test -clipsrc with WKT geometry (#3530)


def test_ogr2ogr_py_24():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -clipsrc "POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -clipsrc with clip from external datasource


def test_ogr2ogr_py_25():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    f = open('tmp/clip.csv', 'wt')
    f.write('foo,WKT\n')
    f.write('foo,"POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"\n')
    f.close()

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -clipsrc tmp/clip.csv -clipsrcwhere foo=\'foo\'')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    os.remove('tmp/clip.csv')

###############################################################################
# Test -clipdst with WKT geometry (#3530)


def test_ogr2ogr_py_26():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -clipdst "POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -clipdst with clip from external datasource


def test_ogr2ogr_py_27():
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    f = open('tmp/clip.csv', 'wt')
    f.write('foo,WKT\n')
    f.write('foo,"POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"\n')
    f.close()

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', '-nlt MULTIPOLYGON tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -clipdst tmp/clip.csv -clipdstsql "SELECT * from clip"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    os.remove('tmp/clip.csv')

###############################################################################
# Test that -overwrite work if the output file doesn't yet exist (#3825)


def test_ogr2ogr_py_31():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -overwrite tmp/poly.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test that -append/-overwrite to a single-file shapefile work without specifying -nln


def test_ogr2ogr_py_32():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_32.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_32.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_32.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')
    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -append tmp/test_ogr2ogr_32.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_32.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, '-append failed'
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -overwrite tmp/test_ogr2ogr_32.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_32.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        '-overwrite failed'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_32.shp')

###############################################################################
# Test -explodecollections


def test_ogr2ogr_py_33():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_33_src.csv')
        ogr.GetDriverByName('CSV').DeleteDataSource('tmp/test_ogr2ogr_33_src.csv')
    except OSError:
        pass

    try:
        os.stat('tmp/test_ogr2ogr_33_dst.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_33_dst.shp')
    except OSError:
        pass

    f = open('tmp/test_ogr2ogr_33_src.csv', 'wt')
    f.write('foo,WKT\n')
    f.write('bar,"MULTIPOLYGON (((10 10,10 11,11 11,11 10,10 10)),((100 100,100 200,200 200,200 100,100 100),(125 125,175 125,175 175,125 175,125 125)))"\n')
    f.write('baz,"POLYGON ((0 0,0 1,1 1,1 0,0 0))"\n')
    f.close()

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -explodecollections tmp/test_ogr2ogr_33_dst.shp tmp/test_ogr2ogr_33_src.csv -select foo')

    ds = ogr.Open('tmp/test_ogr2ogr_33_dst.shp')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 3, '-explodecollections failed'

    feat = lyr.GetFeature(0)
    if feat.GetField("foo") != 'bar':
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'POLYGON ((10 10,10 11,11 11,11 10,10 10))':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetFeature(1)
    if feat.GetField("foo") != 'bar':
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'POLYGON ((100 100,100 200,200 200,200 100,100 100),(125 125,175 125,175 175,125 175,125 125))':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetFeature(2)
    if feat.GetField("foo") != 'baz':
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        pytest.fail()

    ds = None

    ogr.GetDriverByName('CSV').DeleteDataSource('tmp/test_ogr2ogr_33_src.csv')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_33_dst.shp')

###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist src.shp -nln someDirThatDoesNotExist'
# This should result in creating a someDirThatDoesNotExist directory with
# someDirThatDoesNotExist.shp/dbf/shx inside this directory


def test_ogr2ogr_py_34():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_34_dir')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_34_dir')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_34_dir '+test_py_scripts.get_data_path('ogr')+'poly.shp -nln test_ogr2ogr_34_dir')

    ds = ogr.Open('tmp/test_ogr2ogr_34_dir/test_ogr2ogr_34_dir.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        'initial shapefile creation failed'
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -append tmp/test_ogr2ogr_34_dir '+test_py_scripts.get_data_path('ogr')+'poly.shp -nln test_ogr2ogr_34_dir')

    ds = ogr.Open('tmp/test_ogr2ogr_34_dir/test_ogr2ogr_34_dir.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, '-append failed'
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -overwrite tmp/test_ogr2ogr_34_dir '+test_py_scripts.get_data_path('ogr')+'poly.shp -nln test_ogr2ogr_34_dir')

    ds = ogr.Open('tmp/test_ogr2ogr_34_dir/test_ogr2ogr_34_dir.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        '-overwrite failed'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_34_dir')

###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist src.shp'


def test_ogr2ogr_py_35():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_35_dir')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_35_dir')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_35_dir '+test_py_scripts.get_data_path('ogr')+'poly.shp ')

    ds = ogr.Open('tmp/test_ogr2ogr_35_dir/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        'initial shapefile creation failed'
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -append tmp/test_ogr2ogr_35_dir '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_35_dir/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, '-append failed'
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -overwrite tmp/test_ogr2ogr_35_dir '+test_py_scripts.get_data_path('ogr')+'poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_35_dir/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        '-overwrite failed'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_35_dir')

###############################################################################
# Test ogr2ogr -zfield


def test_ogr2ogr_py_36():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_36.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_36.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_36.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -zfield EAS_ID')

    ds = ogr.Open('tmp/test_ogr2ogr_36.shp')
    feat = ds.GetLayer(0).GetNextFeature()
    wkt = feat.GetGeometryRef().ExportToWkt()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_36.shp')

    assert wkt.find(' 168,') != -1

###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist.shp dataSourceWithMultipleLayer'


def test_ogr2ogr_py_37():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_37_dir.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_37_dir.shp')
    except OSError:
        pass

    try:
        os.mkdir('tmp/test_ogr2ogr_37_src')
    except OSError:
        pass
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'poly.shp', 'tmp/test_ogr2ogr_37_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'poly.shx', 'tmp/test_ogr2ogr_37_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'poly.dbf', 'tmp/test_ogr2ogr_37_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'shp/testpoly.shp', 'tmp/test_ogr2ogr_37_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'shp/testpoly.shx', 'tmp/test_ogr2ogr_37_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'shp/testpoly.dbf', 'tmp/test_ogr2ogr_37_src')

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_37_dir.shp tmp/test_ogr2ogr_37_src')

    ds = ogr.Open('tmp/test_ogr2ogr_37_dir.shp')
    assert ds is not None and ds.GetLayerCount() == 2
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_37_src')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_37_dir.shp')

###############################################################################
# Test that we take into account the fields by the where clause when combining
# -select and -where (#4015)


def test_ogr2ogr_py_38():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_38.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_38.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_38.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -select AREA -where "EAS_ID = 170"')

    ds = ogr.Open('tmp/test_ogr2ogr_38.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_38.shp')

###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist.shp dataSourceWithMultipleLayer -sql "select * from alayer"' (#4268)


def test_ogr2ogr_py_39():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_39_dir.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_39.shp')
    except OSError:
        pass

    try:
        os.mkdir('tmp/test_ogr2ogr_39_src')
    except OSError:
        pass
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'poly.shp', 'tmp/test_ogr2ogr_39_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'poly.shx', 'tmp/test_ogr2ogr_39_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'poly.dbf', 'tmp/test_ogr2ogr_39_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'shp/testpoly.shp', 'tmp/test_ogr2ogr_39_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'shp/testpoly.shx', 'tmp/test_ogr2ogr_39_src')
    shutil.copy(test_py_scripts.get_data_path('ogr') + 'shp/testpoly.dbf', 'tmp/test_ogr2ogr_39_src')

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_39.shp tmp/test_ogr2ogr_39_src -sql "select * from poly"')

    ds = ogr.Open('tmp/test_ogr2ogr_39.shp')
    assert ds is not None and ds.GetLayerCount() == 1
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_39_src')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_39.shp')

###############################################################################
# Test -dim 3 and -dim 2


def test_ogr2ogr_py_43():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_43_3d.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_3d.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_43_3d.shp '+test_py_scripts.get_data_path('ogr')+'poly.shp -dim 3')

    ds = ogr.Open('tmp/test_ogr2ogr_43_3d.shp')
    lyr = ds.GetLayerByIndex(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon25D
    ds = None

    try:
        os.stat('tmp/test_ogr2ogr_43_2d.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_2d.shp')
    except OSError:
        pass

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' tmp/test_ogr2ogr_43_2d.shp tmp/test_ogr2ogr_43_3d.shp -dim 2')

    ds = ogr.Open('tmp/test_ogr2ogr_43_2d.shp')
    lyr = ds.GetLayerByIndex(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_2d.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_3d.shp')

###############################################################################
# Test -nlt PROMOTE_TO_MULTI for polygon/multipolygon


def test_ogr2ogr_py_44():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_44_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_44_src.shp')
    except OSError:
        pass

    gdal.Unlink('tmp/test_ogr2ogr_44.gml')
    gdal.Unlink('tmp/test_ogr2ogr_44.xsd')

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test_ogr2ogr_44_src.shp')
    lyr = ds.CreateLayer('test_ogr2ogr_44_src', geom_type=ogr.wkbPolygon)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0)))'))
    lyr.CreateFeature(feat)
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -f GML -dsco FORMAT=GML2 tmp/test_ogr2ogr_44.gml tmp/test_ogr2ogr_44_src.shp -nlt PROMOTE_TO_MULTI')

    f = open('tmp/test_ogr2ogr_44.xsd')
    data = f.read()
    f.close()

    assert data.find('type="gml:MultiPolygonPropertyType"') != -1

    f = open('tmp/test_ogr2ogr_44.gml')
    data = f.read()
    f.close()

    assert data.find('<ogr:geometryProperty><gml:MultiPolygon><gml:polygonMember><gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:coordinates>0,0 0,1 1,1 0,0</gml:coordinates></gml:LinearRing></gml:outerBoundaryIs></gml:Polygon></gml:polygonMember></gml:MultiPolygon></ogr:geometryProperty>') != -1

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_44_src.shp')
    os.unlink('tmp/test_ogr2ogr_44.gml')
    os.unlink('tmp/test_ogr2ogr_44.xsd')

###############################################################################
# Test -nlt PROMOTE_TO_MULTI for polygon/multipolygon


def test_ogr2ogr_py_45():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_44_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_44_src.shp')
    except OSError:
        pass

    gdal.Unlink('tmp/test_ogr2ogr_44.gml')
    gdal.Unlink('tmp/test_ogr2ogr_44.xsd')

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test_ogr2ogr_44_src.shp')
    lyr = ds.CreateLayer('test_ogr2ogr_44_src', geom_type=ogr.wkbPolygon)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0)))'))
    lyr.CreateFeature(feat)
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -f GML -dsco FORMAT=GML2 tmp/test_ogr2ogr_44.gml tmp/test_ogr2ogr_44_src.shp -nlt PROMOTE_TO_MULTI')

    f = open('tmp/test_ogr2ogr_44.xsd')
    data = f.read()
    f.close()

    assert data.find('type="gml:MultiPolygonPropertyType"') != -1

    f = open('tmp/test_ogr2ogr_44.gml')
    data = f.read()
    f.close()

    assert data.find('<ogr:geometryProperty><gml:MultiPolygon><gml:polygonMember><gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:coordinates>0,0 0,1 1,1 0,0</gml:coordinates></gml:LinearRing></gml:outerBoundaryIs></gml:Polygon></gml:polygonMember></gml:MultiPolygon></ogr:geometryProperty>') != -1

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_44_src.shp')
    os.unlink('tmp/test_ogr2ogr_44.gml')
    os.unlink('tmp/test_ogr2ogr_44.xsd')

###############################################################################
# Test -nlt PROMOTE_TO_MULTI for linestring/multilinestring


def test_ogr2ogr_py_46():

    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_45_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_45_src.shp')
    except OSError:
        pass

    gdal.Unlink('tmp/test_ogr2ogr_45.gml')
    gdal.Unlink('tmp/test_ogr2ogr_45.xsd')

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test_ogr2ogr_45_src.shp')
    lyr = ds.CreateLayer('test_ogr2ogr_45_src', geom_type=ogr.wkbLineString)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,0 1,1 1,0 0)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 0,0 1,1 1,0 0),(10 0,10 1,11 1,10 0))'))
    lyr.CreateFeature(feat)
    ds = None

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', ' -f GML -dsco FORMAT=GML2 tmp/test_ogr2ogr_45.gml tmp/test_ogr2ogr_45_src.shp -nlt PROMOTE_TO_MULTI')

    f = open('tmp/test_ogr2ogr_45.xsd')
    data = f.read()
    f.close()

    assert data.find('type="gml:MultiLineStringPropertyType"') != -1

    f = open('tmp/test_ogr2ogr_45.gml')
    data = f.read()
    f.close()

    assert data.find('<ogr:geometryProperty><gml:MultiLineString><gml:lineStringMember><gml:LineString><gml:coordinates>0,0 0,1 1,1 0,0</gml:coordinates></gml:LineString></gml:lineStringMember></gml:MultiLineString></ogr:geometryProperty>') != -1

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_45_src.shp')
    gdal.Unlink('tmp/test_ogr2ogr_45.gml')
    gdal.Unlink('tmp/test_ogr2ogr_45.xsd')




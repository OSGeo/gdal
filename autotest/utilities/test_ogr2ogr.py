#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogr2ogr testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal, ogr, osr
import gdaltest
import ogrtest
import test_cli_utilities

###############################################################################
# Simple test


def test_ogr2ogr_1():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp')
    assert (err is None or err == ''), 'got error/warning'

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


def test_ogr2ogr_2():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp -sql "select * from poly"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -spat


def test_ogr2ogr_3():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp -spat 479609 4764629 479764 4764817')

    ds = ogr.Open('tmp/poly.shp')
    if ogrtest.have_geos():
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4
    else:
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 5
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -where


def test_ogr2ogr_4():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp -where "EAS_ID=171"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -append


def test_ogr2ogr_5():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp')
    # All 3 variants below should be equivalent
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -update -append tmp/poly.shp ../ogr/data/poly.shp')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append tmp/poly.shp ../ogr/data/poly.shp')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append -update tmp/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 40

    feat10 = ds.GetLayer(0).GetFeature(10)
    assert feat10.GetFieldAsDouble('AREA') == 215229.266, \
        'Did not get expected value for field AREA'
    assert feat10.GetFieldAsString('PRFEDEA') == '35043411', \
        'Did not get expected value for field PRFEDEA'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')


def check_if_has_ogr_pg():
    path = '../ogr'
    if path not in sys.path:
        sys.path.append(path)
    try:
        import ogr_pg
    except:
        pytest.skip()
    ogr_pg.test_ogr_pg_1()
    if gdaltest.pg_ds is None:
        pytest.skip()
    gdaltest.pg_ds.Destroy()

###############################################################################
# Test -overwrite


def test_ogr2ogr_6():

    check_if_has_ogr_pg()

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL PG:"' + gdaltest.pg_connection_string + '" ../ogr/data/poly.shp -nln tpoly')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -update -overwrite -f PostgreSQL PG:"' + gdaltest.pg_connection_string + '" ../ogr/data/poly.shp -nln tpoly')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    assert ds is not None and ds.GetLayerByName('tpoly').GetFeatureCount() == 10
    ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

###############################################################################
# Test -gt


def test_ogr2ogr_7():

    check_if_has_ogr_pg()

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL PG:"' + gdaltest.pg_connection_string + '" ../ogr/data/poly.shp -nln tpoly -gt 1')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    assert ds is not None and ds.GetLayerByName('tpoly').GetFeatureCount() == 10
    ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' PG:"' + gdaltest.pg_connection_string + '" -sql "DELLAYER:tpoly"')

###############################################################################
# Test -t_srs


def test_ogr2ogr_8():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -t_srs EPSG:4326 tmp/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert str(ds.GetLayer(0).GetSpatialRef()).find('1984') != -1
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -a_srs


def test_ogr2ogr_9():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -a_srs EPSG:4326 tmp/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert str(ds.GetLayer(0).GetSpatialRef()).find('1984') != -1
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -select


def test_ogr2ogr_10():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    # Voluntary don't use the exact case of the source field names (#4502)
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -select eas_id,prfedea tmp/poly.shp ../ogr/data/poly.shp')

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


def test_ogr2ogr_11():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -lco SHPT=POLYGONZ tmp/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -nlt


def test_ogr2ogr_12():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -nlt POLYGON25D tmp/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Add explicit source layer name


def test_ogr2ogr_13():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp poly')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -segmentize


def test_ogr2ogr_14():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -segmentize 100 tmp/poly.shp ../ogr/data/poly.shp poly')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryRef(0).GetPointCount() == 36
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -overwrite with a shapefile


def test_ogr2ogr_15():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    # Overwrite
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -overwrite tmp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -fid


def test_ogr2ogr_16():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -fid 8 tmp/poly.shp ../ogr/data/poly.shp')

    src_ds = ogr.Open('../ogr/data/poly.shp')
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


def test_ogr2ogr_17():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    ret = gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -progress tmp/poly.shp ../ogr/data/poly.shp')
    assert ret.find('0...10...20...30...40...50...60...70...80...90...100 - done.') != -1

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -wrapdateline


def test_ogr2ogr_18():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/wrapdateline_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_src.shp')
    except (OSError, AttributeError):
        pass

    try:
        os.stat('tmp/wrapdateline_dst.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_dst.shp')
    except (OSError, AttributeError):
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

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -wrapdateline -t_srs EPSG:4326 tmp/wrapdateline_dst.shp tmp/wrapdateline_src.shp')

    expected_wkt = 'MULTIPOLYGON (((179.222391385437 36.124095832137,180.0 36.1071354434926,180.0 36.107135443432,180.0 27.0904291237556,179.017505655194 27.1079795236266,179.222391385437 36.124095832137)),((-180 36.1071354434425,-179.667822828784 36.0983491954849,-179.974688335432 27.0898861430914,-180 27.0904291237129,-180 27.090429123727,-180 36.107135443432,-180 36.1071354434425)))'
    expected_wkt2 = 'MULTIPOLYGON (((179.017505655194 27.1079795236266,179.222391385437 36.124095832137,180.0 36.1071354434926,180.0 36.107135443432,180.0 27.0904291237556,179.017505655194 27.1079795236266)),((-180 27.090429123727,-180 36.107135443432,-180 36.1071354434425,-179.667822828784 36.0983491954849,-179.974688335432 27.0898861430914,-180 27.0904291237129,-180 27.090429123727)))' # with geos OverlayNG

    ds = ogr.Open('tmp/wrapdateline_dst.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    got_wkt = feat.GetGeometryRef().ExportToWkt()
    ok = ogrtest.check_feature_geometry(feat, expected_wkt) == 0 or ogrtest.check_feature_geometry(feat, expected_wkt2) == 0
    feat.Destroy()
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_src.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_dst.shp')

    assert ok, got_wkt

###############################################################################
# Test -clipsrc


def test_ogr2ogr_19():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp -clipsrc spat_extent -spat 479609 4764629 479764 4764817')

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


def test_ogr2ogr_20():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

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

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp data/Fields.csv')

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


def test_ogr2ogr_21():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.remove('tmp/testogr2ogr21.gtm')
    except OSError:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() +
                         ' -f GPSTrackMaker tmp/testogr2ogr21.gtm data/dataforogr2ogr21.csv ' +
                         '-sql "SELECT comment, name FROM dataforogr2ogr21" -nlt POINT')
    ds = ogr.Open('tmp/testogr2ogr21.gtm')

    assert ds is not None
    ds.GetLayer(0).GetLayerDefn()
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('name') != 'NAME' or \
       feat.GetFieldAsString('comment') != 'COMMENT':
        print(feat.GetFieldAsString('comment'))
        ds.Destroy()
        os.remove('tmp/testogr2ogr21.gtm')
        pytest.fail(feat.GetFieldAsString('name'))

    ds.Destroy()
    os.remove('tmp/testogr2ogr21.gtm')


###############################################################################
# Test ogr2ogr when the output driver delays the destination layer defn creation (#3384)

def test_ogr2ogr_22():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() +
                         ' -f "MapInfo File" tmp/testogr2ogr22.mif data/dataforogr2ogr21.csv ' +
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


def test_ogr2ogr_23():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() +
                         ' -f "MapInfo File" tmp/testogr2ogr23.mif data/dataforogr2ogr21.csv ' +
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


def test_ogr2ogr_24():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp -clipsrc "POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -clipsrc with clip from external datasource


def test_ogr2ogr_25():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    f = open('tmp/clip.csv', 'wt')
    f.write('foo,WKT\n')
    f.write('foo,"POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp -clipsrc tmp/clip.csv -clipsrcwhere foo=\'foo\'')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    os.remove('tmp/clip.csv')

###############################################################################
# Test -clipdst with WKT geometry (#3530)


def test_ogr2ogr_26():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/poly.shp ../ogr/data/poly.shp -clipdst "POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test -clipdst with clip from external datasource


def test_ogr2ogr_27():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    f = open('tmp/clip.csv', 'wt')
    f.write('foo,WKT\n')
    f.write('foo,"POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -nlt MULTIPOLYGON tmp/poly.shp ../ogr/data/poly.shp -clipdst tmp/clip.csv -clipdstsql "SELECT * from clip"')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (479609, 479764, 4764629, 4764817), \
        'unexpected extent'

    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    os.remove('tmp/clip.csv')


###############################################################################
# Test -wrapdateline on linestrings

def test_ogr2ogr_28():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/wrapdateline_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_src.shp')
    except (OSError, AttributeError):
        pass

    try:
        os.stat('tmp/wrapdateline_dst.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_dst.shp')
    except (OSError, AttributeError):
        pass

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/wrapdateline_src.shp')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('wrapdateline_src', srs=srs)
    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('LINESTRING(160 0,165 1,170 2,175 3,177 4,-177 5,-175 6,-170 7,-177 8,177 9,170 10)')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)
    feat.Destroy()
    ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -wrapdateline tmp/wrapdateline_dst.shp tmp/wrapdateline_src.shp')

    expected_wkt = 'MULTILINESTRING ((160 0,165 1,170 2,175 3,177 4,180 4.5),(-180 4.5,-177 5,-175 6,-170 7,-177 8,-180 8.5),(180 8.5,177 9,170 10))'
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
# Test -wrapdateline on polygons


def test_ogr2ogr_29():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    for i in range(2):
        try:
            os.stat('tmp/wrapdateline_src.shp')
            ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_src.shp')
        except (OSError, AttributeError):
            pass

        try:
            os.stat('tmp/wrapdateline_dst.shp')
            ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_dst.shp')
        except (OSError, AttributeError):
            pass

        ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/wrapdateline_src.shp')
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        lyr = ds.CreateLayer('wrapdateline_src', srs=srs)
        feat = ogr.Feature(lyr.GetLayerDefn())

        if i == 0:
            geom = ogr.CreateGeometryFromWkt('POLYGON((179 40,179.5 40,-179.5 40,-179 40,-170 40,-165 40,-165 30,-170 30,-179 30,-179.5 30,179.5 30,179 30,179 40))')
        else:
            geom = ogr.CreateGeometryFromWkt('POLYGON((-165 30,-170 30,-179 30,-179.5 30,179.5 30,179 30,179 40,179.5 40,-179.5 40,-179 40,-170 40,-165 40,-165 30))')
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)
        feat.Destroy()
        ds.Destroy()

        gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -wrapdateline tmp/wrapdateline_dst.shp tmp/wrapdateline_src.shp')

        expected_wkt = 'MULTIPOLYGON (((180 30,179.5 30.0,179 30,179 40,179.5 40.0,180 40,180 30)),((-180 40,-179.5 40.0,-179 40,-170 40,-165 40,-165 30,-170 30,-179 30,-179.5 30.0,-180 30,-180 40)))'
        expected_geom = ogr.CreateGeometryFromWkt(expected_wkt)
        ds = ogr.Open('tmp/wrapdateline_dst.shp')
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        ret = ogrtest.check_feature_geometry(feat, expected_geom)
        if ret != 0:
            print('src is : %s' % geom.ExportToWkt())
            print('got    : %s' % feat.GetGeometryRef().ExportToWkt())

        feat.Destroy()
        expected_geom.Destroy()
        ds.Destroy()

        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/wrapdateline_dst.shp')

        assert ret == 0

    
###############################################################################
# Test -splitlistfields option


def test_ogr2ogr_30():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    ds = ogr.Open('../ogr/data/gml/testlistfields.gml')
    if ds is None:
        pytest.skip()
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -splitlistfields tmp/test_ogr2ogr_30.dbf ../ogr/data/gml/testlistfields.gml')
    gdal.Unlink('../ogr/data/gml/testlistfields.gfs')

    ds = ogr.Open('tmp/test_ogr2ogr_30.dbf')
    assert ds is not None
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if feat.GetField('attrib11') != 'value1' or \
       feat.GetField('attrib12') != 'value2' or \
       feat.GetField('attrib2') != 'value3' or \
       feat.GetField('attrib31') != 4 or \
       feat.GetField('attrib32') != 5 or \
       feat.GetField('attrib41') != 6.1 or \
       feat.GetField('attrib42') != 7.1:
        feat.DumpReadable()
        pytest.fail('did not get expected attribs')

    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_30.dbf')

###############################################################################
# Test that -overwrite work if the output file doesn't yet exist (#3825)


def test_ogr2ogr_31():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -overwrite tmp/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')

###############################################################################
# Test that -append/-overwrite to a single-file shapefile work without specifying -nln


def test_ogr2ogr_32():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_32.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_32.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_32.shp ../ogr/data/poly.shp')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append tmp/test_ogr2ogr_32.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_32.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, '-append failed'
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -overwrite tmp/test_ogr2ogr_32.shp ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_32.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        '-overwrite failed'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_32.shp')

###############################################################################
# Test -explodecollections


def test_ogr2ogr_33():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_33_src.csv')
        ogr.GetDriverByName('CSV').DeleteDataSource('tmp/test_ogr2ogr_33_src.csv')
    except (OSError, AttributeError):
        pass

    try:
        os.stat('tmp/test_ogr2ogr_33_dst.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_33_dst.shp')
    except (OSError, AttributeError):
        pass

    f = open('tmp/test_ogr2ogr_33_src.csv', 'wt')
    f.write('foo,WKT\n')
    f.write('bar,"MULTIPOLYGON (((10 10,10 11,11 11,11 10,10 10)),((100 100,100 200,200 200,200 100,100 100),(125 125,175 125,175 175,125 175,125 125)))"\n')
    f.write('baz,"POLYGON ((0 0,0 1,1 1,1 0,0 0))"\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -explodecollections tmp/test_ogr2ogr_33_dst.shp tmp/test_ogr2ogr_33_src.csv -select foo')

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


def test_ogr2ogr_34():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_34_dir')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_34_dir')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_34_dir ../ogr/data/poly.shp -nln test_ogr2ogr_34_dir')

    ds = ogr.Open('tmp/test_ogr2ogr_34_dir/test_ogr2ogr_34_dir.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        'initial shapefile creation failed'
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append tmp/test_ogr2ogr_34_dir ../ogr/data/poly.shp -nln test_ogr2ogr_34_dir')

    ds = ogr.Open('tmp/test_ogr2ogr_34_dir/test_ogr2ogr_34_dir.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, '-append failed'
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -overwrite tmp/test_ogr2ogr_34_dir ../ogr/data/poly.shp -nln test_ogr2ogr_34_dir')

    ds = ogr.Open('tmp/test_ogr2ogr_34_dir/test_ogr2ogr_34_dir.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        '-overwrite failed'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_34_dir')

###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist src.shp'


def test_ogr2ogr_35():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_35_dir')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_35_dir')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_35_dir ../ogr/data/poly.shp ')

    ds = ogr.Open('tmp/test_ogr2ogr_35_dir/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        'initial shapefile creation failed'
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append tmp/test_ogr2ogr_35_dir ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_35_dir/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, '-append failed'
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -overwrite tmp/test_ogr2ogr_35_dir ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_35_dir/poly.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10, \
        '-overwrite failed'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_35_dir')

###############################################################################
# Test ogr2ogr -zfield


def test_ogr2ogr_36():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_36.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_36.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_36.shp ../ogr/data/poly.shp -zfield EAS_ID')

    ds = ogr.Open('tmp/test_ogr2ogr_36.shp')
    feat = ds.GetLayer(0).GetNextFeature()
    wkt = feat.GetGeometryRef().ExportToWkt()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_36.shp')

    assert wkt.find(' 168,') != -1

###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist.shp dataSourceWithMultipleLayer'


def test_ogr2ogr_37():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_37_dir.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_37_dir.shp')
    except (OSError, AttributeError):
        pass

    try:
        os.mkdir('tmp/test_ogr2ogr_37_src')
    except OSError:
        pass
    shutil.copy('../ogr/data/poly.shp', 'tmp/test_ogr2ogr_37_src')
    shutil.copy('../ogr/data/poly.shx', 'tmp/test_ogr2ogr_37_src')
    shutil.copy('../ogr/data/poly.dbf', 'tmp/test_ogr2ogr_37_src')
    shutil.copy('../ogr/data/shp/testpoly.shp', 'tmp/test_ogr2ogr_37_src')
    shutil.copy('../ogr/data/shp/testpoly.shx', 'tmp/test_ogr2ogr_37_src')
    shutil.copy('../ogr/data/shp/testpoly.dbf', 'tmp/test_ogr2ogr_37_src')

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_37_dir.shp tmp/test_ogr2ogr_37_src')

    ds = ogr.Open('tmp/test_ogr2ogr_37_dir.shp')
    assert ds is not None and ds.GetLayerCount() == 2
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_37_src')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_37_dir.shp')

###############################################################################
# Test that we take into account the fields by the where clause when combining
# -select and -where (#4015)


def test_ogr2ogr_38():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_38.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_38.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_38.shp ../ogr/data/poly.shp -select AREA -where "EAS_ID = 170"')

    ds = ogr.Open('tmp/test_ogr2ogr_38.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_38.shp')

###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist.shp dataSourceWithMultipleLayer -sql "select * from alayer"' (#4268)


def test_ogr2ogr_39():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_39_dir.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_39.shp')
    except (OSError, AttributeError):
        pass

    try:
        os.mkdir('tmp/test_ogr2ogr_39_src')
    except OSError:
        pass
    shutil.copy('../ogr/data/poly.shp', 'tmp/test_ogr2ogr_39_src')
    shutil.copy('../ogr/data/poly.shx', 'tmp/test_ogr2ogr_39_src')
    shutil.copy('../ogr/data/poly.dbf', 'tmp/test_ogr2ogr_39_src')
    shutil.copy('../ogr/data/shp/testpoly.shp', 'tmp/test_ogr2ogr_39_src')
    shutil.copy('../ogr/data/shp/testpoly.shx', 'tmp/test_ogr2ogr_39_src')
    shutil.copy('../ogr/data/shp/testpoly.dbf', 'tmp/test_ogr2ogr_39_src')

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_39.shp tmp/test_ogr2ogr_39_src -sql "select * from poly"')

    ds = ogr.Open('tmp/test_ogr2ogr_39.shp')
    assert ds is not None and ds.GetLayerCount() == 1
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_39_src')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_39.shp')

###############################################################################
# Test 'ogr2ogr -update asqlite.db asqlite.db layersrc -nln layerdst' (#4270)


def test_ogr2ogr_40():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    drv = ogr.GetDriverByName('SQLite')
    if drv is None:
        pytest.skip()

    try:
        ogr.GetDriverByName('SQLite').DeleteDataSource('tmp/test_ogr2ogr_40.db')
    except AttributeError:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f SQlite tmp/test_ogr2ogr_40.db ../ogr/data/poly.shp')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -update tmp/test_ogr2ogr_40.db tmp/test_ogr2ogr_40.db poly -nln poly2')

    ds = ogr.Open('tmp/test_ogr2ogr_40.db')
    lyr = ds.GetLayerByName('poly2')
    assert lyr.GetFeatureCount() == 10
    ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('tmp/test_ogr2ogr_40.db')

###############################################################################
# Test 'ogr2ogr -update PG:xxxx PG:xxxx layersrc -nln layerdst' (#4270)


def test_ogr2ogr_41():

    check_if_has_ogr_pg()

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    ds.ExecuteSQL('DELLAYER:test_ogr2ogr_41_src')
    ds.ExecuteSQL('DELLAYER:test_ogr2ogr_41_target')
    lyr = ds.CreateLayer('test_ogr2ogr_41_src')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.StartTransaction()
    for i in range(501):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat['foo'] = '%d' % i
        lyr.CreateFeature(feat)
        feat = None
    lyr.CommitTransaction()
    lyr = None
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -update PG:"' + gdaltest.pg_connection_string + '" PG:"' + gdaltest.pg_connection_string + '" test_ogr2ogr_41_src -nln test_ogr2ogr_41_target')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('test_ogr2ogr_41_target')
    assert lyr.GetFeatureCount() == 501
    ds.ExecuteSQL('DELLAYER:test_ogr2ogr_41_src')
    ds.ExecuteSQL('DELLAYER:test_ogr2ogr_41_target')
    ds = None

###############################################################################
# Test combination of -select and -where FID=xx (#4500)


def test_ogr2ogr_42():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_42.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_42.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_42.shp ../ogr/data/poly.shp -select AREA -where "FID = 0"')

    ds = ogr.Open('tmp/test_ogr2ogr_42.shp')
    lyr = ds.GetLayerByIndex(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_42.shp')

###############################################################################
# Test -dim 3 and -dim 2


def test_ogr2ogr_43():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_43_3d.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_3d.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_43_3d.shp ../ogr/data/poly.shp -dim 3')

    ds = ogr.Open('tmp/test_ogr2ogr_43_3d.shp')
    lyr = ds.GetLayerByIndex(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon25D
    ds = None

    try:
        os.stat('tmp/test_ogr2ogr_43_2d.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_2d.shp')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_43_2d.shp tmp/test_ogr2ogr_43_3d.shp -dim 2')

    ds = ogr.Open('tmp/test_ogr2ogr_43_2d.shp')
    lyr = ds.GetLayerByIndex(0)
    assert lyr.GetGeomType() == ogr.wkbPolygon
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_2d.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_43_3d.shp')

###############################################################################
# Test -nlt PROMOTE_TO_MULTI for polygon/multipolygon


def test_ogr2ogr_44():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_44_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_44_src.shp')
    except (OSError, AttributeError):
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

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/test_ogr2ogr_44.gml tmp/test_ogr2ogr_44_src.shp -nlt PROMOTE_TO_MULTI')

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


def test_ogr2ogr_45():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_45_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_45_src.shp')
    except (OSError, AttributeError):
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

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/test_ogr2ogr_45.gml tmp/test_ogr2ogr_45_src.shp -nlt PROMOTE_TO_MULTI')

    f = open('tmp/test_ogr2ogr_45.xsd')
    data = f.read()
    f.close()

    assert data.find('type="gml:MultiLineStringPropertyType"') != -1

    f = open('tmp/test_ogr2ogr_45.gml')
    data = f.read()
    f.close()

    assert data.find('<ogr:geometryProperty><gml:MultiLineString><gml:lineStringMember><gml:LineString><gml:coordinates>0,0 0,1 1,1 0,0</gml:coordinates></gml:LineString></gml:lineStringMember></gml:MultiLineString></ogr:geometryProperty>') != -1

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_45_src.shp')
    os.unlink('tmp/test_ogr2ogr_45.gml')
    os.unlink('tmp/test_ogr2ogr_45.xsd')

###############################################################################
# Test -gcp (#4604)


def test_ogr2ogr_46():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/test_ogr2ogr_46_src.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_46_src.shp')
    except (OSError, AttributeError):
        pass

    gdal.Unlink('tmp/test_ogr2ogr_46.gml')
    gdal.Unlink('tmp/test_ogr2ogr_46.xsd')

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test_ogr2ogr_46_src.shp')
    lyr = ds.CreateLayer('test_ogr2ogr_46_src', geom_type=ogr.wkbPoint)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
    lyr.CreateFeature(feat)
    ds = None

    for option in ['', ' -tps', ' -order 1', ' -a_srs EPSG:4326', ' -s_srs EPSG:4326 -t_srs EPSG:3857']:
        gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/test_ogr2ogr_46.gml tmp/test_ogr2ogr_46_src.shp -gcp 0 0 2 49 -gcp 0 1 2 50 -gcp 1 0 3 49%s' % option)

        f = open('tmp/test_ogr2ogr_46.gml')
        data = f.read()
        f.close()

        assert not (data.find('2,49') == -1 and data.find('2.0,49.0') == -1 and data.find('222638.') == -1), \
            option

        assert not (data.find('3,50') == -1 and data.find('3.0,50.0') == -1 and data.find('333958.') == -1), \
            option

        os.unlink('tmp/test_ogr2ogr_46.gml')
        os.unlink('tmp/test_ogr2ogr_46.xsd')

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_46_src.shp')

###############################################################################
# Test reprojection with features with different SRS


def test_ogr2ogr_47():

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_47_src.gml', 'wt')
    f.write("""<foo xmlns:gml="http://www.opengis.net/gml">
   <gml:featureMember>
      <features>
         <geometry>
            <gml:Point srsName="http://www.opengis.net/gml/srs/epsg.xml#32630">
               <gml:coordinates>500000,4500000</gml:coordinates>
            </gml:Point>
         </geometry>
      </features>
   </gml:featureMember>
   <gml:featureMember>
      <features >
         <geometry>
            <gml:Point srsName="http://www.opengis.net/gml/srs/epsg.xml#32631">
               <gml:coordinates>500000,4500000</gml:coordinates>
            </gml:Point>
         </geometry>
      </features>
   </gml:featureMember>
</foo>""")
    f.close()

    gdal.Unlink('tmp/test_ogr2ogr_47_src.gfs')

    ds = ogr.Open('tmp/test_ogr2ogr_47_src.gml')

    if ds is None:
        os.unlink('tmp/test_ogr2ogr_47_src.gml')
        pytest.skip()
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML -t_srs EPSG:4326 tmp/test_ogr2ogr_47_dst.gml tmp/test_ogr2ogr_47_src.gml')

    f = open('tmp/test_ogr2ogr_47_dst.gml')
    data = f.read()
    f.close()

    assert ('>-3.0,40.65' in data and '>3.0,40.65' in data) or \
           ('>-3,40.65' in data and '>3.0,40.65' in data) or \
           ('>-2.99999999999999,40.65' in data and '>2.99999999999999,40.65' in data), data

    os.unlink('tmp/test_ogr2ogr_47_dst.gml')
    os.unlink('tmp/test_ogr2ogr_47_dst.xsd')

    os.unlink('tmp/test_ogr2ogr_47_src.gml')
    os.unlink('tmp/test_ogr2ogr_47_src.gfs')

###############################################################################
# Test fieldmap option


def test_ogr2ogr_48():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp data/Fields.csv')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append -fieldmap identity tmp data/Fields.csv')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append -fieldmap 14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 tmp data/Fields.csv')

    ds = ogr.Open('tmp/Fields.dbf')

    assert ds is not None
    layer_defn = ds.GetLayer(0).GetLayerDefn()
    if layer_defn.GetFieldCount() != 15:
        ds.Destroy()
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/Fields.dbf')
        pytest.fail('Unexpected field count: ' + str(ds.GetLayer(0).GetLayerDefn().GetFieldCount()))

    error_occurred = False
    lyr = ds.GetLayer(0)
    lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    for i in range(layer_defn.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(i + 1):
            print('Expected the value ', str(i + 1), ',but got', feat.GetFieldAsString(i))
            error_occurred = True
    feat = lyr.GetNextFeature()
    for i in range(layer_defn.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(layer_defn.GetFieldCount() - i):
            print('Expected the value ', str(layer_defn.GetFieldCount() - i), ',but got', feat.GetFieldAsString(i))
            error_occurred = True

    ds.Destroy()
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/Fields.dbf')

    assert not error_occurred

###############################################################################
# Test detection of duplicated field names in source layer and renaming
# in target layer


def test_ogr2ogr_49():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f CSV tmp/test_ogr2ogr_49.csv data/duplicatedfields.csv')
    f = open('tmp/test_ogr2ogr_49.csv')
    lines = f.readlines()
    f.close()

    os.unlink('tmp/test_ogr2ogr_49.csv')

    assert (lines[0].find('foo,bar,foo3,foo2,baz,foo4') == 0 and \
       lines[1].find('val_foo,val_bar,val_foo3,val_foo2,val_baz,val_foo4') == 0)

###############################################################################
# Test detection of duplicated field names is case insensitive (#5208)


def test_ogr2ogr_49_bis():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f KML tmp/test_ogr2ogr_49_bis.kml data/grid.csv -sql "SELECT field_1 AS name FROM grid WHERE fid = 1"')
    f = open('tmp/test_ogr2ogr_49_bis.kml')
    lines = f.readlines()
    f.close()

    os.unlink('tmp/test_ogr2ogr_49_bis.kml')

    expected_lines = [
        """<?xml version="1.0" encoding="utf-8" ?>""",
        """<kml xmlns="http://www.opengis.net/kml/2.2">""",
        """<Document id="root_doc">""",
        """<Folder><name>grid</name>""",
        """  <Placemark>""",
        """        <name>440750.000</name>""",
        """  </Placemark>""",
        """</Folder>""",
        """</Document></kml>"""]

    assert len(lines) == len(expected_lines)
    for i, line in enumerate(lines):
        assert line.strip() == expected_lines[i].strip(), lines

    
###############################################################################
# Test -addfields


def test_ogr2ogr_50():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_50_1.csv', 'wt')
    f.write('id,field1\n')
    f.write('1,foo\n')
    f.close()

    f = open('tmp/test_ogr2ogr_50_2.csv', 'wt')
    f.write('id,field1,field2\n')
    f.write('2,bar,baz\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_50.dbf tmp/test_ogr2ogr_50_1.csv -nln test_ogr2ogr_50')
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -addfields tmp/test_ogr2ogr_50.dbf tmp/test_ogr2ogr_50_2.csv -nln test_ogr2ogr_50')

    ds = ogr.Open('tmp/test_ogr2ogr_50.dbf')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('field1') != 'foo' or not feat.IsFieldNull('field2'):
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('field1') != 'bar' or feat.GetField('field2') != 'baz':
        feat.DumpReadable()
        pytest.fail()
    ds = None

    os.unlink('tmp/test_ogr2ogr_50.dbf')
    os.unlink('tmp/test_ogr2ogr_50_1.csv')
    os.unlink('tmp/test_ogr2ogr_50_2.csv')

###############################################################################
# Test RFC 41 support


def test_ogr2ogr_51():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_51_src.csv', 'wt')
    f.write('id,_WKTgeom1_EPSG_4326,foo,_WKTgeom2_EPSG_32631\n')
    f.write('1,"POINT(1 2)","bar","POINT(3 4)"\n')
    f.close()

    # Test conversion from a multi-geometry format into a multi-geometry format
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f CSV tmp/test_ogr2ogr_51_dst.csv tmp/test_ogr2ogr_51_src.csv -nln test_ogr2ogr_51_dst -dsco GEOMETRY=AS_WKT -lco STRING_QUOTING=ALWAYS')

    f = open('tmp/test_ogr2ogr_51_dst.csv', 'rt')
    lines = f.readlines()
    f.close()

    expected_lines = ['"_WKTgeom1_EPSG_4326","_WKTgeom2_EPSG_32631","id","foo"', '"POINT (1 2)","POINT (3 4)","1","bar"']
    for i in range(2):
        assert lines[i].strip() == expected_lines[i]

    # Test conversion from a multi-geometry format into a single-geometry format
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_51_dst.shp tmp/test_ogr2ogr_51_src.csv -nln test_ogr2ogr_51_dst')

    ds = ogr.Open('tmp/test_ogr2ogr_51_dst.shp')
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    assert sr is not None and sr.ExportToWkt().find('GEOGCS["WGS 84"') == 0
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (1 2)'
    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_51_dst.shp')

    # Test -append into a multi-geometry format
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append tmp/test_ogr2ogr_51_dst.csv tmp/test_ogr2ogr_51_src.csv -nln test_ogr2ogr_51_dst')

    f = open('tmp/test_ogr2ogr_51_dst.csv', 'rt')
    lines = f.readlines()
    f.close()

    expected_lines = ['"_WKTgeom1_EPSG_4326","_WKTgeom2_EPSG_32631","id","foo"',
                      '"POINT (1 2)","POINT (3 4)","1","bar"',
                      '"POINT (1 2)","POINT (3 4)","1","bar"']
    for i in range(3):
        assert lines[i].strip() == expected_lines[i]

    os.unlink('tmp/test_ogr2ogr_51_dst.csv')

    # Test -select with geometry field names
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -select foo,geom__WKTgeom2_EPSG_32631,id,geom__WKTgeom1_EPSG_4326 -f CSV tmp/test_ogr2ogr_51_dst.csv tmp/test_ogr2ogr_51_src.csv -nln test_ogr2ogr_51_dst -dsco GEOMETRY=AS_WKT -lco STRING_QUOTING=ALWAYS')

    f = open('tmp/test_ogr2ogr_51_dst.csv', 'rt')
    lines = f.readlines()
    f.close()

    expected_lines = ['"_WKTgeom2_EPSG_32631","_WKTgeom1_EPSG_4326","foo","id"', '"POINT (3 4)","POINT (1 2)","bar","1"']
    for i in range(2):
        assert lines[i].strip() == expected_lines[i]

    # Test -geomfield option
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append tmp/test_ogr2ogr_51_dst.csv tmp/test_ogr2ogr_51_src.csv -nln test_ogr2ogr_51_dst -spat 1 2 1 2 -geomfield geom__WKTgeom1_EPSG_4326')

    f = open('tmp/test_ogr2ogr_51_dst.csv', 'rt')
    lines = f.readlines()
    f.close()

    expected_lines = ['"_WKTgeom2_EPSG_32631","_WKTgeom1_EPSG_4326","foo","id"',
                      '"POINT (3 4)","POINT (1 2)","bar","1"',
                      '"POINT (3 4)","POINT (1 2)","bar","1"']
    for i in range(2):
        assert lines[i].strip() == expected_lines[i]

    os.unlink('tmp/test_ogr2ogr_51_src.csv')
    os.unlink('tmp/test_ogr2ogr_51_dst.csv')

###############################################################################
# Test -nlt CONVERT_TO_LINEAR and -nlt CONVERT_TO_CURVE


def test_ogr2ogr_52():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_52_src.csv', 'wt')
    f.write('id,WKT\n')
    f.write('1,"CIRCULARSTRING(0 0,1 0,0 0)"\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f CSV tmp/test_ogr2ogr_52_dst.csv tmp/test_ogr2ogr_52_src.csv -select id -nln test_ogr2ogr_52_dst -dsco GEOMETRY=AS_WKT -nlt CONVERT_TO_LINEAR')

    f = open('tmp/test_ogr2ogr_52_dst.csv', 'rt')
    content = f.read()
    f.close()

    assert 'LINESTRING (0 0,' in content

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f CSV tmp/test_ogr2ogr_52_dst2.csv tmp/test_ogr2ogr_52_dst.csv -select id -nln test_ogr2ogr_52_dst2 -dsco GEOMETRY=AS_WKT -nlt CONVERT_TO_CURVE')

    f = open('tmp/test_ogr2ogr_52_dst2.csv', 'rt')
    content = f.read()
    f.close()

    assert 'COMPOUNDCURVE ((0 0,' in content

    os.unlink('tmp/test_ogr2ogr_52_src.csv')
    os.unlink('tmp/test_ogr2ogr_52_dst.csv')
    os.unlink('tmp/test_ogr2ogr_52_dst2.csv')

###############################################################################
# Test -mapFieldType and 64 bit integers


def test_ogr2ogr_53():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_53.csv', 'wt')
    f.write('id,i64,b,WKT\n')
    f.write('1,123456789012,true,"POINT(0 0)"\n')
    f.close()
    f = open('tmp/test_ogr2ogr_53.csvt', 'wt')
    f.write('Integer,Integer64,Integer(Boolean),String\n')
    f.close()

    # Default behaviour with a driver that declares GDAL_DMD_CREATIONFIELDDATATYPES
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f KML tmp/test_ogr2ogr_53.kml tmp/test_ogr2ogr_53.csv -mapFieldType "Integer(Boolean)=String"')

    f = open('tmp/test_ogr2ogr_53.kml', 'rt')
    content = f.read()
    f.close()

    assert ('<SimpleField name="id" type="int"></SimpleField>' in content and \
       '<SimpleData name="id">1</SimpleData>' in content and \
       '<SimpleField name="i64" type="float"></SimpleField>' in content and \
       '<SimpleData name="i64">123456789012</SimpleData>' in content and \
       '<SimpleField name="b" type="string"></SimpleField>' in content and \
       '<SimpleData name="b">1</SimpleData>' in content)

    os.unlink('tmp/test_ogr2ogr_53.kml')

    # Default behaviour with a driver that does not GDAL_DMD_CREATIONFIELDDATATYPES
    #gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f BNA tmp/test_ogr2ogr_53.bna tmp/test_ogr2ogr_53.csv -nlt POINT')

    #f = open('tmp/test_ogr2ogr_53.bna', 'rt')
    #content = f.read()
    #f.close()

    #assert '"123456789012.0"' in content

    #os.unlink('tmp/test_ogr2ogr_53.bna')

    # with -mapFieldType
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f KML tmp/test_ogr2ogr_53.kml tmp/test_ogr2ogr_53.csv -mapFieldType Integer64=String')

    f = open('tmp/test_ogr2ogr_53.kml', 'rt')
    content = f.read()
    f.close()

    assert ('<SimpleField name="i64" type="string"></SimpleField>' in content and \
       '<SimpleData name="i64">123456789012</SimpleData>' in content)

    os.unlink('tmp/test_ogr2ogr_53.kml')

    os.unlink('tmp/test_ogr2ogr_53.csv')
    os.unlink('tmp/test_ogr2ogr_53.csvt')

###############################################################################
# Test behaviour with nullable fields


def test_ogr2ogr_54():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_54.csv', 'wt')
    f.write('fld1,fld2,WKT\n')
    f.write('1,2,"POINT(0 0)"\n')
    f.close()

    f = open('tmp/test_ogr2ogr_54.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
  <OGRVRTLayer name="test_ogr2ogr_54">
    <SrcDataSource relativeToVRT="1" shared="1">test_ogr2ogr_54.csv</SrcDataSource>
    <SrcLayer>test_ogr2ogr_54</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <GeometryField name="WKT" nullable="false"/>
    <Field name="fld1" type="String" src="fld1" nullable="no"/>
    <Field name="fld2" type="String" src="fld2"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
""")
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/test_ogr2ogr_54.gml tmp/test_ogr2ogr_54.vrt')

    f = open('tmp/test_ogr2ogr_54.xsd', 'rt')
    content = f.read()
    f.close()

    assert ('<xs:element name="WKT" type="gml:GeometryPropertyType" nillable="true" minOccurs="1" maxOccurs="1"/>' in content and \
       '<xs:element name="fld1" nillable="true" minOccurs="1" maxOccurs="1">' in content and \
       '<xs:element name="fld2" nillable="true" minOccurs="0" maxOccurs="1">' in content)

    os.unlink('tmp/test_ogr2ogr_54.gml')
    os.unlink('tmp/test_ogr2ogr_54.xsd')

    # Test -forceNullable
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -forceNullable -f GML tmp/test_ogr2ogr_54.gml tmp/test_ogr2ogr_54.vrt')

    f = open('tmp/test_ogr2ogr_54.xsd', 'rt')
    content = f.read()
    f.close()

    assert ('<xs:element name="WKT" type="gml:GeometryPropertyType" nillable="true" minOccurs="0" maxOccurs="1"/>' in content and \
       '<xs:element name="fld1" nillable="true" minOccurs="0" maxOccurs="1">' in content and \
       '<xs:element name="fld2" nillable="true" minOccurs="0" maxOccurs="1">' in content)

    os.unlink('tmp/test_ogr2ogr_54.gml')
    os.unlink('tmp/test_ogr2ogr_54.xsd')

    os.unlink('tmp/test_ogr2ogr_54.csv')
    os.unlink('tmp/test_ogr2ogr_54.vrt')

###############################################################################
# Test behaviour with default values


def test_ogr2ogr_55():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_55.csv', 'wt')
    f.write('fld1,fld2,WKT\n')
    f.write('1,,"POINT(0 0)"\n')
    f.close()

    f = open('tmp/test_ogr2ogr_55.csvt', 'wt')
    f.write('Integer,Integer,String\n')
    f.close()

    f = open('tmp/test_ogr2ogr_55.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
  <OGRVRTLayer name="test_ogr2ogr_55">
    <SrcDataSource relativeToVRT="1" shared="1">test_ogr2ogr_55.csv</SrcDataSource>
    <SrcLayer>test_ogr2ogr_55</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <GeometryField name="WKT"/>
    <Field name="fld1" type="Integer" src="fld1"/>
    <Field name="fld2" type="Integer" src="fld2" nullable="false" default="2"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
""")
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/test_ogr2ogr_55.gml tmp/test_ogr2ogr_55.vrt')

    f = open('tmp/test_ogr2ogr_55.gml', 'rt')
    content = f.read()
    f.close()

    assert '<ogr:fld2>2</ogr:fld2>' in content

    os.unlink('tmp/test_ogr2ogr_55.gml')
    os.unlink('tmp/test_ogr2ogr_55.xsd')

    # Test -unsetDefault
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -forceNullable -unsetDefault -f GML tmp/test_ogr2ogr_55.gml tmp/test_ogr2ogr_55.vrt')

    f = open('tmp/test_ogr2ogr_55.gml', 'rt')
    content = f.read()
    f.close()

    assert '<ogr:fld2>' not in content

    os.unlink('tmp/test_ogr2ogr_55.gml')
    os.unlink('tmp/test_ogr2ogr_55.xsd')

    os.unlink('tmp/test_ogr2ogr_55.csv')
    os.unlink('tmp/test_ogr2ogr_55.csvt')
    os.unlink('tmp/test_ogr2ogr_55.vrt')

###############################################################################
# Test behaviour when creating a field with same name as FID column.


def test_ogr2ogr_56():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_56.csv', 'wt')
    f.write('str,myid,WKT\n')
    f.write('aaa,10,"POINT(0 0)"\n')
    f.close()

    f = open('tmp/test_ogr2ogr_56.csvt', 'wt')
    f.write('String,Integer,String\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PGDump tmp/test_ogr2ogr_56.sql tmp/test_ogr2ogr_56.csv -lco FID=myid --config PGDUMP_DEBUG_ALLOW_CREATION_FIELD_WITH_FID_NAME NO')

    f = open('tmp/test_ogr2ogr_56.sql', 'rt')
    content = f.read()
    f.close()

    assert ("""ALTER TABLE "public"."test_ogr2ogr_56" ADD COLUMN "myid"" """ not in content and \
       """INSERT INTO "public"."test_ogr2ogr_56" ("wkb_geometry" , "myid" , "str", "wkt") VALUES ('010100000000000000000000000000000000000000', 10, 'aaa', 'POINT(0 0)');""" in content)

    os.unlink('tmp/test_ogr2ogr_56.sql')
    os.unlink('tmp/test_ogr2ogr_56.csv')
    os.unlink('tmp/test_ogr2ogr_56.csvt')

###############################################################################
# Test default propagation of FID column name and values, and -unsetFid


def test_ogr2ogr_57():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_57.csv', 'wt')
    f.write('id,str,WKT\n')
    f.write('10,a,"POINT(0 0)"\n')
    f.close()

    f = open('tmp/test_ogr2ogr_57.csvt', 'wt')
    f.write('Integer,String,String\n')
    f.close()

    f = open('tmp/test_ogr2ogr_57.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
  <OGRVRTLayer name="test_ogr2ogr_57">
    <SrcDataSource relativeToVRT="1" shared="1">test_ogr2ogr_57.csv</SrcDataSource>
    <SrcLayer>test_ogr2ogr_57</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <GeometryField name="WKT"/>
    <FID name="id">id</FID>
    <Field name="str"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
""")
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PGDump tmp/test_ogr2ogr_57.sql tmp/test_ogr2ogr_57.vrt')

    f = open('tmp/test_ogr2ogr_57.sql', 'rt')
    content = f.read()
    f.close()

    assert ("""CREATE TABLE "public"."test_ogr2ogr_57" (    "id" SERIAL,    CONSTRAINT "test_ogr2ogr_57_pk" PRIMARY KEY ("id") )""" in content and \
       """INSERT INTO "public"."test_ogr2ogr_57" ("wkt" , "id" , "str") VALUES ('010100000000000000000000000000000000000000', 10, 'a')""" in content)

    os.unlink('tmp/test_ogr2ogr_57.sql')

    # Test -unsetFid
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PGDump tmp/test_ogr2ogr_57.sql tmp/test_ogr2ogr_57.vrt -unsetFid')

    f = open('tmp/test_ogr2ogr_57.sql', 'rt')
    content = f.read()
    f.close()

    assert ("""CREATE TABLE "public"."test_ogr2ogr_57" (    "ogc_fid" SERIAL,    CONSTRAINT "test_ogr2ogr_57_pk" PRIMARY KEY ("ogc_fid") )""" in content and \
       """INSERT INTO "public"."test_ogr2ogr_57" ("wkt" , "str") VALUES ('010100000000000000000000000000000000000000', 'a')""" in content)

    os.unlink('tmp/test_ogr2ogr_57.sql')

    os.unlink('tmp/test_ogr2ogr_57.csv')
    os.unlink('tmp/test_ogr2ogr_57.csvt')
    os.unlink('tmp/test_ogr2ogr_57.vrt')

###############################################################################
# Test datasource transactions


def test_ogr2ogr_58():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()
    if ogr.GetDriverByName('SQLite') is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -gt 3 -f SQLite tmp/test_ogr2ogr_58.sqlite ../ogr/data/poly.shp')

    ds = ogr.Open('tmp/test_ogr2ogr_58.sqlite')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('tmp/test_ogr2ogr_58.sqlite')

###############################################################################
# Test metadata support


def test_ogr2ogr_59():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()
    if ogr.GetDriverByName('GPKG') is None:
        pytest.skip()

    ds = ogr.GetDriverByName('GPKG').CreateDataSource('tmp/test_ogr2ogr_59_src.gpkg')
    ds.SetMetadataItem('FOO', 'BAR')
    ds.SetMetadataItem('BAR', 'BAZ', 'another_domain')
    lyr = ds.CreateLayer('mylayer')
    lyr.SetMetadataItem('lyr_FOO', 'lyr_BAR')
    lyr.SetMetadataItem('lyr_BAR', 'lyr_BAZ', 'lyr_another_domain')
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GPKG tmp/test_ogr2ogr_59_dest.gpkg tmp/test_ogr2ogr_59_src.gpkg -mo BAZ=BAW')

    ds = ogr.Open('tmp/test_ogr2ogr_59_dest.gpkg')
    assert ds.GetMetadata() == {'FOO': 'BAR', 'BAZ': 'BAW'}
    assert ds.GetMetadata('another_domain') == {'BAR': 'BAZ'}
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata() == {'lyr_FOO': 'lyr_BAR'}
    assert lyr.GetMetadata('lyr_another_domain') == {'lyr_BAR': 'lyr_BAZ'}
    ds = None

    ogr.GetDriverByName('GPKG').DeleteDataSource('tmp/test_ogr2ogr_59_dest.gpkg')

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GPKG tmp/test_ogr2ogr_59_dest.gpkg tmp/test_ogr2ogr_59_src.gpkg -nomd')
    ds = ogr.Open('tmp/test_ogr2ogr_59_dest.gpkg')
    assert ds.GetMetadata() == {}
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata() == {}
    ds = None

    ogr.GetDriverByName('GPKG').DeleteDataSource('tmp/test_ogr2ogr_59_dest.gpkg')

    ogr.GetDriverByName('GPKG').DeleteDataSource('tmp/test_ogr2ogr_59_src.gpkg')

###############################################################################
# Test forced datasource transactions


def test_ogr2ogr_60():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()
    if ogr.GetDriverByName('FileGDB') is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -ds_transaction -f FileGDB tmp/test_ogr2ogr_60.gdb ../ogr/data/poly.shp -mapFieldType Integer64=Integer')

    ds = ogr.Open('tmp/test_ogr2ogr_60.gdb')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None

    ogr.GetDriverByName('FileGDB').DeleteDataSource('tmp/test_ogr2ogr_60.gdb')

###############################################################################
# Test -spat_srs


def test_ogr2ogr_61():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/test_ogr2ogr_61.csv', 'wt')
    f.write('foo,WKT\n')
    f.write('1,"POINT(2 49)"\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_61.shp tmp/test_ogr2ogr_61.csv -spat 426857 5427937 426858 5427938 -spat_srs EPSG:32631 -s_srs EPSG:4326 -a_srs EPSG:4326')

    ds = ogr.Open('tmp/test_ogr2ogr_61.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    ds.Destroy()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/test_ogr2ogr_61_2.shp tmp/test_ogr2ogr_61.shp -spat 426857 5427937 426858 5427938 -spat_srs EPSG:32631')

    ds = ogr.Open('tmp/test_ogr2ogr_61_2.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_61.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_ogr2ogr_61_2.shp')
    os.unlink('tmp/test_ogr2ogr_61.csv')

###############################################################################
# Test -noNativeData


def test_ogr2ogr_62():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    # Default behaviour

    fp = open('tmp/test_ogr2ogr_62_in.json', 'wt')
    fp.write('{"type": "FeatureCollection", "foo": "bar", "features":[ { "type": "Feature", "bar": "baz", "properties": { "myprop": "myvalue" }, "geometry": null } ]}')
    fp = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + """ -f GeoJSON tmp/test_ogr2ogr_62.json tmp/test_ogr2ogr_62_in.json""")
    fp = gdal.VSIFOpenL('tmp/test_ogr2ogr_62.json', 'rb')
    assert fp is not None
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)
    os.unlink('tmp/test_ogr2ogr_62.json')

    assert 'bar' in data and 'baz' in data

    # Test -noNativeData
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + """ -f GeoJSON tmp/test_ogr2ogr_62.json tmp/test_ogr2ogr_62_in.json -noNativeData""")
    fp = gdal.VSIFOpenL('tmp/test_ogr2ogr_62.json', 'rb')
    assert fp is not None
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)
    os.unlink('tmp/test_ogr2ogr_62.json')
    os.unlink('tmp/test_ogr2ogr_62_in.json')

    assert 'bar' not in data and 'baz' not in data

###############################################################################
# Test --formats


def test_ogr2ogr_63():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/poly.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/poly.shp')
    except (OSError, AttributeError):
        pass

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogr2ogr_path() + ' --formats')
    assert 'Supported Formats' in ret, err
    assert 'ERROR' not in err, ret

###############################################################################
# Test appending multiple layers, whose one already exists (#6345)


def test_ogr2ogr_64():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        shutil.rmtree('tmp/in_csv')
    except OSError:
        pass
    try:
        shutil.rmtree('tmp/out_csv')
    except OSError:
        pass

    os.mkdir('tmp/in_csv')
    open('tmp/in_csv/lyr1.csv', 'wt').write("id,col\n1,1\n")
    open('tmp/in_csv/lyr2.csv', 'wt').write("id,col\n1,1\n")

    ds = ogr.Open('tmp/in_csv')
    first_layer = ds.GetLayer(0).GetName()
    second_layer = ds.GetLayer(1).GetName()
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f CSV tmp/out_csv tmp/in_csv ' + second_layer)
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append tmp/out_csv tmp/in_csv')

    ds = ogr.Open('tmp/out_csv')
    assert ds.GetLayerByName(first_layer).GetFeatureCount() == 1
    assert ds.GetLayerByName(second_layer).GetFeatureCount() == 2
    ds = None

    shutil.rmtree('tmp/in_csv')
    shutil.rmtree('tmp/out_csv')

###############################################################################
# Test detection of extension


def test_ogr2ogr_65():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/out.csv ../ogr/data/poly.shp')
    ds = gdal.OpenEx('tmp/out.csv')
    assert ds.GetDriver().ShortName == 'CSV'
    ds = None
    gdal.Unlink('tmp/out.csv')

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogr2ogr_path() + ' /vsimem/out.xxx ../ogr/data/poly.shp')
    if "Cannot guess" not in err:
        print(ret)
        pytest.fail('expected a warning about probably wrong extension')

    
###############################################################################
# Test accidental overriding of dataset when dst and src filenames are the same (#1465)


def test_ogr2ogr_66():
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogr2ogr_path() + ' ../ogr/data/poly.shp ../ogr/data/poly.shp')
    assert "Source and destination datasets must be different in non-update mode" in err, \
        ret


def hexify_double(val):
    val = hex(val)
    # On 32bit Linux, we might get a trailing L
    return val.rstrip('L').lstrip('0x').zfill(16).upper()


def check_identity_transformation(x, y, srid):
    import struct

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    for output_shp in ['tmp/output_point.shp', 'tmp/output_point2.shp']:
        try:
            os.stat(output_shp)
            shape_drv.DeleteDataSource(output_shp)
        except OSError:
            pass

    # Generate CSV file with test point
    xy_wkb = '0101000000' + ''.join(hexify_double(q) for q in struct.unpack('>QQ', struct.pack("<dd", x, y)))
    f = open('tmp/input_point.csv', 'wt')
    f.write('id,wkb_geom\n')
    f.write('1,' + xy_wkb + '\n')
    f.close()

    # To check that the transformed values are identical to the original ones we need
    # to use a binary format with the same accuracy as the source (WKB).
    # CSV cannot be used for this purpose because WKB is not supported as a geometry output format.

    # Note that when transforming CSV to SHP the same internal definition of EPSG:srid is being used for source and target,
    # so that this transformation will have identically defined input and output units
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + " tmp/output_point.shp tmp/input_point.csv -oo GEOM_POSSIBLE_NAMES=wkb_geom -s_srs EPSG:%(srid)d  -t_srs EPSG:%(srid)d" % locals())

    ds = ogr.Open('tmp/output_point.shp')
    feat = ds.GetLayer(0).GetNextFeature()
    ok = feat.GetGeometryRef().GetX() == x and feat.GetGeometryRef().GetY() == y
    feat.Destroy()
    ds.Destroy()

    if ok:
        # Now, transforming SHP to SHP will have a different definition of the SRS (EPSG:srid) which comes from the previously saved .prj file
        # For angular units in degrees the .prj is saved with greater precision than the internally used value.
        # We perform this additional transformation to exercise the case of units defined with different precision
        gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + " tmp/output_point2.shp tmp/output_point.shp -t_srs EPSG:%(srid)d" % locals())
        ds = ogr.Open('tmp/output_point2.shp')
        feat = ds.GetLayer(0).GetNextFeature()
        ok = feat.GetGeometryRef().GetX() == x and feat.GetGeometryRef().GetY() == y
        feat.Destroy()
        ds.Destroy()
        shape_drv.DeleteDataSource('tmp/output_point2.shp')

    shape_drv.DeleteDataSource('tmp/output_point.shp')
    os.remove('tmp/input_point.csv')

    assert ok

###############################################################################
# Test coordinates values are preserved for identity transformations


def test_ogr2ogr_67():

    # Test coordinates
    # The x value is such that x * k * (1/k) != x with k the common factor used in degrees unit definition
    # If the coordinates are converted to radians and back to degrees the value of x will be altered
    x = float.fromhex('0x1.5EB3ED959A307p6')
    y = 0.0

    # Now we will check the value of x is preserved in a transformation with same target and source SRS,
    # both as latitutude/longitude in degrees.
    ret = check_identity_transformation(x, y, 4326)
    return ret

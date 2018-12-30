#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
################################################################################
#  Project: OGR NextGIS Web Driver
#  Purpose: Tests OGR NGW Driver capabilities
#  Author: Dmitry Baryshnikov, polimax@mail.ru
#  Language: Python
################################################################################
#  The MIT License (MIT)
#
#  Copyright (c) 2018, NextGIS <info@nextgis.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
################################################################################

import sys

sys.path.append('../pymod')

import gdaltest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import time
import json
import pytest

def check_availability(url):
    version_url = url + '/api/component/pyramid/pkg_version'
    if gdaltest.gdalurlopen(version_url) is None:
        return False

    # Check quota
    quota_url = url + '/api/resource/quota'
    quota_conn = gdaltest.gdalurlopen(quota_url)
    try:
        quota_json = json.loads(quota_conn.read())
        quota_conn.close()
        if quota_json is None:
            return False
        limit = quota_json['limit']
        count = quota_json['count']
        return limit - count > 10
    except:
        return False

###############################################################################
# Check driver existence.

def test_ogr_ngw_1():

    gdaltest.ngw_ds = None
    gdaltest.ngw_drv = None

    gdaltest.ngw_drv = gdal.GetDriverByName('NGW')
    if gdaltest.ngw_drv is None:
        pytest.skip()

    gdaltest.ngw_test_server = 'http://dev.nextgis.com/sandbox'

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

###############################################################################
# Check create datasource.

def test_ogr_ngw_2():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    create_url = 'NGW:' + gdaltest.ngw_test_server + '/resource/0/gdaltest_group_' + str(int(time.time()))
    gdal.PushErrorHandler()
    gdaltest.ngw_ds = gdaltest.ngw_drv.Create(create_url, 0, 0, 0, gdal.GDT_Unknown, \
        options=['DESCRIPTION=GDAL Test group',])
    gdal.PopErrorHandler()

    assert gdaltest.ngw_ds is not None, 'Create datasource failed.'
    assert gdaltest.ngw_ds.GetMetadataItem('description', '') == 'GDAL Test group', \
        'Did not get expected datasource description.'

    assert int(gdaltest.ngw_ds.GetMetadataItem('id', '')) > 0, \
        'Did not get expected datasource identifier.'
    gdaltest.group_id = gdaltest.ngw_ds.GetMetadataItem('id', '')

###############################################################################
# Check rename datasource.

def test_ogr_ngw_3():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    new_name = 'gdaltest_group_' + str(int(time.time()) - 2)
    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    rename_url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id

    assert gdaltest.ngw_drv.Rename(new_name, rename_url) == gdal.CE_None, \
        'Rename datasource failed.'

###############################################################################
# Check datasource metadata.

def test_ogr_ngw_4():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    gdaltest.ngw_ds.SetMetadataItem('test_int.d', '777', 'NGW')
    gdaltest.ngw_ds.SetMetadataItem('test_float.f', '777.555', 'NGW')
    gdaltest.ngw_ds.SetMetadataItem('test_string', 'metadata test', 'NGW')

    gdaltest.ngw_ds = None
    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(url, gdal.OF_UPDATE) # gdaltest.ngw_drv.Open(url, update=1)
    assert gdaltest.ngw_ds is not None, \
        'Open datasource failed.'

    md_item = gdaltest.ngw_ds.GetMetadataItem('test_int.d', 'NGW')
    assert md_item == '777', \
        'Did not get expected datasource metadata item. test_int.d is equal {}, but should {}.'.format(md_item, '777')

    md_item = gdaltest.ngw_ds.GetMetadataItem('test_float.f', 'NGW')
    assert abs(float(md_item) - 777.555) < 0.00001, \
        'Did not get expected datasource metadata item. test_float.f is equal {}, but should {}.'.format(md_item, '777.555')

    md_item = gdaltest.ngw_ds.GetMetadataItem('test_string', 'NGW')
    assert md_item == 'metadata test', \
        'Did not get expected datasource metadata item. test_string is equal {}, but should {}.'.format(md_item, 'metadata test')

def create_fields(lyr):
    fld_defn = ogr.FieldDefn('STRFIELD', ogr.OFTString)
    lyr.CreateField(fld_defn)
    lyr.SetMetadataItem('FIELD_0_ALIAS', 'String field test')
    fld_defn = ogr.FieldDefn('DECFIELD', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    lyr.SetMetadataItem('FIELD_1_ALIAS', 'Integer field test')
    fld_defn = ogr.FieldDefn('BIGDECFIELD', ogr.OFTInteger64)
    lyr.CreateField(fld_defn)
    lyr.SetMetadataItem('FIELD_2_ALIAS', 'Integer64 field test')
    fld_defn = ogr.FieldDefn('REALFIELD', ogr.OFTReal)
    lyr.CreateField(fld_defn)
    lyr.SetMetadataItem('FIELD_3_ALIAS', 'Real field test')
    fld_defn = ogr.FieldDefn('DATEFIELD', ogr.OFTDate)
    lyr.CreateField(fld_defn)
    lyr.SetMetadataItem('FIELD_4_ALIAS', 'Date field test')
    fld_defn = ogr.FieldDefn('TIMEFIELD', ogr.OFTTime)
    lyr.CreateField(fld_defn)
    lyr.SetMetadataItem('FIELD_5_ALIAS', 'Time field test')
    fld_defn = ogr.FieldDefn('DATETIMEFLD', ogr.OFTDateTime)
    lyr.CreateField(fld_defn)
    lyr.SetMetadataItem('FIELD_6_ALIAS', 'Date & time field test')

def fill_fields(f):
    f.SetField('STRFIELD', "fo_o")
    f.SetField('DECFIELD', 123)
    f.SetField('BIGDECFIELD', 12345678901234)
    f.SetField('REALFIELD', 1.23)
    f.SetField('DATETIMEFLD', '2014/12/04 12:34:56')

def fill_fields2(f):
    f.SetField('STRFIELD', "русский")
    f.SetField('DECFIELD', 321)
    f.SetField('BIGDECFIELD', 32145678901234)
    f.SetField('REALFIELD', 21.32)
    f.SetField('DATETIMEFLD', '2019/12/31 21:43:56')

def add_metadata(lyr):
    lyr.SetMetadataItem('test_int.d', '777', 'NGW')
    lyr.SetMetadataItem('test_float.f', '777,555', 'NGW')
    lyr.SetMetadataItem('test_string', 'metadata test', 'NGW')

###############################################################################
# Check create vector layers.

def test_ogr_ngw_5():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    lyr = gdaltest.ngw_ds.CreateLayer('test_pt_layer', srs=sr, geom_type=ogr.wkbMultiPoint, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_ln_layer', srs=sr, geom_type=ogr.wkbMultiLineString, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_pl_layer', srs=sr, geom_type=ogr.wkbMultiPolygon, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    # Test overwrite
    lyr = gdaltest.ngw_ds.CreateLayer('test_pt_layer', srs=sr, geom_type=ogr.wkbPoint, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_ln_layer', srs=sr, geom_type=ogr.wkbLineString, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_pl_layer', srs=sr, geom_type=ogr.wkbPolygon, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    gdaltest.ngw_ds = None

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id

    gdaltest.ngw_ds = gdal.OpenEx(url, gdal.OF_UPDATE) # gdaltest.ngw_drv.Open(url, update=1)
    assert gdaltest.ngw_ds is not None, 'Open datasource failed.'

    for layer_name in ['test_pt_layer', 'test_ln_layer', 'test_pl_layer']:
        lyr = gdaltest.ngw_ds.GetLayerByName(layer_name)
        assert lyr is not None, 'Get layer {} failed.'.format(layer_name)

        md_item = lyr.GetMetadataItem('test_int.d', 'NGW')
        assert md_item == '777', \
            'Did not get expected datasource metadata item. test_int.d is equal {}, but should {}.'.format(md_item, '777')

        md_item = lyr.GetMetadataItem('test_float.f', 'NGW')
        assert abs(float(md_item) - 777.555) < 0.00001, \
            'Did not get expected datasource metadata item. test_float.f is equal {}, but should {}.'.format(md_item, '777.555')

        md_item = lyr.GetMetadataItem('test_string', 'NGW')
        assert md_item == 'metadata test', \
            'Did not get expected datasource metadata item. test_string is equal {}, but should {}.'.format(md_item, 'metadata test')

###############################################################################
# Check open single vector layer.

def test_ogr_ngw_6():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')
    lyr_resource_id = lyr.GetMetadataItem('id', '')
    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + lyr_resource_id
    ds = gdal.OpenEx(url)
    assert ds is not None and ds.GetLayerCount() == 1, \
        'Failed to open single vector layer.'

###############################################################################
# Check insert, update and delete features.

def test_ogr_ngw_7():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() >= 0, \
        'Create feature failed. Expected FID greater or equal 0, got {}.'.format(f.GetFID())

    fill_fields2(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    ret = lyr.SetFeature(f)
    assert ret == 0, 'Failed to update feature #{}.'.format(f.GetFID())

    lyr.DeleteFeature(f.GetFID())

    # Expected fail to get feature
    gdal.PushErrorHandler()
    f = lyr.GetFeature(f.GetFID())
    gdal.PopErrorHandler()
    assert f is None, 'Failed to delete feature #{}.'.format(f.GetFID())

###############################################################################
# Check insert, update features in batch mode.

def test_ogr_ngw_8():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    gdaltest.ngw_ds = None

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(url, gdal.OF_UPDATE, open_options=['BATCH_SIZE=2'])

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')
    f1 = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f1)
    f1.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    ret = lyr.CreateFeature(f1)
    assert ret == 0 and f1.GetFID() < 0

    f2 = ogr.Feature(lyr.GetLayerDefn())
    fill_fields2(f2)
    f2.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 3)'))
    ret = lyr.CreateFeature(f2)
    assert ret == 0 and f2.GetFID() < 0

    f3 = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f3)
    f3.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    ret = lyr.CreateFeature(f3)
    assert ret == 0

    ret = lyr.SyncToDisk()
    assert ret == 0

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    counter = 0
    while feat is not None:
        counter += 1
        assert feat.GetFID() >= 0, 'Expected FID greater or equal 0, got {}.'.format(feat.GetFID())

        feat = lyr.GetNextFeature()

    assert counter >= 3, 'Expected 3 or greater feature count, got {}.'.format(counter)

###############################################################################
# Check ExecuteSQL.

def test_ogr_ngw_9():
    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    gdaltest.ngw_ds.ExecuteSQL('DELLAYER:test_ln_layer')
    lyr = gdaltest.ngw_ds.GetLayerByName('test_ln_layer')
    assert lyr is None, 'Expected fail to get layer test_ln_layer.'

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pl_layer')

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 0,0 0))'))
    ret = lyr.CreateFeature(f)
    assert ret == 0, 'Failed to create feature in test_ln_layer.'
    assert lyr.GetFeatureCount() == 1, 'Expected feature count is 1, got {}.'.format(lyr.GetFeatureCount())

    gdaltest.ngw_ds.ExecuteSQL('DELETE FROM test_pl_layer;')
    assert lyr.GetFeatureCount() == 0, 'Expected feature count is 0, got {}.'.format(lyr.GetFeatureCount())

###############################################################################
#  Run test_ogrsf

def test_ogr_ngw_test_ogrsf():
    if gdaltest.ngw_drv is None or gdal.GetConfigOption('SKIP_SLOW') is not None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_drv = None
        pytest.skip()

    if gdaltest.skip_on_travis():
        pytest.skip()

    if gdaltest.ngw_ds is None:
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' ' + url)

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Cleanup

def test_ogr_ngw_cleanup():

    if gdaltest.ngw_drv is None:
        pytest.skip()

    if gdaltest.group_id is not None:
        delete_url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + gdaltest.group_id

        gdaltest.ngw_layer = None
        gdaltest.ngw_ds = None

        assert gdaltest.ngw_drv.Delete(delete_url) == gdal.CE_None, \
            'Failed to delete datasource ' + delete_url + '.'

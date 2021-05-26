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
#  Copyright (c) 2018-2021, NextGIS <info@nextgis.com>
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
import random
from datetime import datetime

def check_availability(url):
    # Sandbox cleans at 1:05 on monday (UTC)
    now = datetime.utcnow()
    if now.weekday() == 0:
        if now.hour >= 0 and now.hour < 4:
            return False

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
        if limit is None or count is None:
            return True
        return limit - count > 15
    except:
        return False

def get_new_name():
    return 'gdaltest_group_' + str(int(time.time())) + '_' + str(random.randint(10, 99))

###############################################################################
# Check driver existence.

def test_ogr_ngw_1():

    gdaltest.ngw_ds = None
    gdaltest.ngw_drv = None

    gdaltest.ngw_drv = gdal.GetDriverByName('NGW')
    if gdaltest.ngw_drv is None:
        pytest.skip()

    gdaltest.ngw_test_server = 'https://sandbox.nextgis.com'

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

    create_url = 'NGW:' + gdaltest.ngw_test_server + '/resource/0/' + get_new_name()
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
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    new_name = get_new_name() + '_2'
    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    rename_url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id

    assert gdaltest.ngw_drv.Rename(new_name, rename_url) == gdal.CE_None, \
        'Rename datasource failed.'

###############################################################################
# Check datasource metadata.

def test_ogr_ngw_4():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
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
    assert float(md_item) == pytest.approx(777.555, abs=0.00001), \
        'Did not get expected datasource metadata item. test_float.f is equal {}, but should {}.'.format(md_item, '777.555')

    md_item = gdaltest.ngw_ds.GetMetadataItem('test_string', 'NGW')
    assert md_item == 'metadata test', \
        'Did not get expected datasource metadata item. test_string is equal {}, but should {}.'.format(md_item, 'metadata test')

    resource_type = gdaltest.ngw_ds.GetMetadataItem('resource_type', '')
    assert resource_type is not None, 'Did not get expected datasource metadata item. Resourse type should be present.'

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
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    lyr = gdaltest.ngw_ds.CreateLayer('test_pt_layer', srs=sr, geom_type=ogr.wkbMultiPoint, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)

    # Test duplicated names.
    fld_defn = ogr.FieldDefn('STRFIELD', ogr.OFTString)
    assert lyr.CreateField(fld_defn) != 0, 'Expected not to create duplicated field'

    # Test forbidden field names.
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fld_defn = ogr.FieldDefn('id', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'Expecting a warning'

    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_ln_layer', srs=sr, geom_type=ogr.wkbMultiLineString, options=['OVERWRITE=YES', 'DESCRIPTION=Test line layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_pl_layer', srs=sr, geom_type=ogr.wkbMultiPolygon, options=['OVERWRITE=YES', 'DESCRIPTION=Test polygon layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    # Test overwrite
    lyr = gdaltest.ngw_ds.CreateLayer('test_pt_layer', srs=sr, geom_type=ogr.wkbPoint, options=['OVERWRITE=YES', 'DESCRIPTION=Test point layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_ln_layer', srs=sr, geom_type=ogr.wkbLineString, options=['OVERWRITE=YES', 'DESCRIPTION=Test line layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer('test_pl_layer', srs=sr, geom_type=ogr.wkbPolygon, options=['OVERWRITE=YES', 'DESCRIPTION=Test polygon layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    # Test without overwrite
    lyr = gdaltest.ngw_ds.CreateLayer('test_pl_layer', srs=sr, geom_type=ogr.wkbMultiPolygon, options=['OVERWRITE=NO', 'DESCRIPTION=Test polygon layer 1'])
    assert lyr is None, 'Create layer without overwrite should fail.'
    lyr = gdaltest.ngw_ds.CreateLayer('test_pl_layer', srs=sr, geom_type=ogr.wkbMultiPolygon, options=['DESCRIPTION=Test point layer 1'])
    assert lyr is None, 'Create layer without overwrite should fail.'

    # Test geometry with Z
    lyr = gdaltest.ngw_ds.CreateLayer('test_plz_layer', srs=sr, geom_type=ogr.wkbMultiPolygon25D, options=['OVERWRITE=YES', 'DESCRIPTION=Test polygonz layer'])
    assert lyr is not None, 'Create layer failed.'

    create_fields(lyr)
    add_metadata(lyr)

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    gdaltest.ngw_ds = None

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id

    gdaltest.ngw_ds = gdal.OpenEx(url, gdal.OF_UPDATE) # gdaltest.ngw_drv.Open(url, update=1)
    assert gdaltest.ngw_ds is not None, 'Open datasource failed.'

    for layer_name in ['test_pt_layer', 'test_ln_layer', 'test_pl_layer', 'test_plz_layer']:
        lyr = gdaltest.ngw_ds.GetLayerByName(layer_name)
        assert lyr is not None, 'Get layer {} failed.'.format(layer_name)

        md_item = lyr.GetMetadataItem('test_int.d', 'NGW')
        assert md_item == '777', \
            'Did not get expected layer metadata item. test_int.d is equal {}, but should {}.'.format(md_item, '777')

        md_item = lyr.GetMetadataItem('test_float.f', 'NGW')
        assert float(md_item) == pytest.approx(777.555, abs=0.00001), \
            'Did not get expected layer metadata item. test_float.f is equal {}, but should {}.'.format(md_item, '777.555')

        md_item = lyr.GetMetadataItem('test_string', 'NGW')
        assert md_item == 'metadata test', \
            'Did not get expected layer metadata item. test_string is equal {}, but should {}.'.format(md_item, 'metadata test')

        resource_type = lyr.GetMetadataItem('resource_type', '')
        assert resource_type is not None, 'Did not get expected layer metadata item. Resourse type should be present.'

        assert lyr.GetGeomType() != ogr.wkbUnknown and lyr.GetGeomType() != ogr.wkbNone

###############################################################################
# Check open single vector layer.

def test_ogr_ngw_6():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
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
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
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
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
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
# Check paging while GetNextFeature.

def test_ogr_ngw_9():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    gdaltest.ngw_ds = None

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(url, gdal.OF_UPDATE, open_options=['PAGE_SIZE=2'])

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    counter = 0
    while feat is not None:
        counter += 1
        assert feat.GetFID() >= 0, 'Expected FID greater or equal 0, got {}.'.format(feat.GetFID())

        feat = lyr.GetNextFeature()

    assert counter >= 3, 'Expected 3 or greater feature count, got {}.'.format(counter)

###############################################################################
# Check native data.

def test_ogr_ngw_10():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem('id', '')
    gdaltest.ngw_ds = None

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(url, gdal.OF_UPDATE, open_options=['NATIVE_DATA=YES', 'EXTENSIONS=description,attachment'])
    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    feature_id = feat.GetFID()
    native_data = feat.GetNativeData()
    assert native_data is not None, 'Feature #{} native data should not be empty'.format(feature_id)
    # {"description":null,"attachment":null}
    assert feat.GetNativeMediaType() == 'application/json', 'Unsupported native media type'

    # Set description
    feat.SetNativeData('{"description":"Test feature description"}')
    ret = lyr.SetFeature(feat)
    assert ret == 0, 'Failed to update feature #{}.'.format(feature_id)

    feat = lyr.GetFeature(feature_id)
    native_data = feat.GetNativeData()
    assert native_data is not None and native_data.find('Test feature description') != -1, 'Expected feature description text, got {}'.format(native_data)

###############################################################################
# Check ignored fields works ok

def test_ogr_ngw_11():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')
    lyr.SetIgnoredFields(['STRFIELD'])

    feat = lyr.GetNextFeature()

    assert not feat.IsFieldSet('STRFIELD'), 'got STRFIELD despite request to ignore it.'

    assert feat.GetFieldAsInteger('DECFIELD') == 123, 'missing or wrong DECFIELD'

    fd = lyr.GetLayerDefn()
    fld = fd.GetFieldDefn(0)  # STRFIELD
    assert fld.IsIgnored(), 'STRFIELD unexpectedly not marked as ignored.'

    fld = fd.GetFieldDefn(1)  # DECFIELD
    assert not fld.IsIgnored(), 'DECFIELD unexpectedly marked as ignored.'

    assert not fd.IsGeometryIgnored(), 'geometry unexpectedly ignored.'

    assert not fd.IsStyleIgnored(), 'style unexpectedly ignored.'

    feat = None
    lyr = None

###############################################################################
# Check attribute filter.

def test_ogr_ngw_12():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')
    lyr.SetAttributeFilter("STRFIELD = 'русский'")
    fc = lyr.GetFeatureCount()
    assert fc == 1, 'Expected feature count is 1, got {}.'.format(fc)

    lyr.SetAttributeFilter("STRFIELD = 'fo_o' AND DECFIELD = 321")
    fc = lyr.GetFeatureCount()
    assert fc == 0, 'Expected feature count is 0, got {}.'.format(fc)

    lyr.SetAttributeFilter('NGW:fld_STRFIELD=fo_o&fld_DECFIELD=123')
    fc = lyr.GetFeatureCount()
    assert fc == 2, 'Expected feature count is 2, got {}.'.format(fc)

    lyr.SetAttributeFilter("DECFIELD < 321")
    fc = lyr.GetFeatureCount()
    assert fc == 2, 'Expected feature count is 2, got {}.'.format(fc)

    lyr.SetAttributeFilter('NGW:fld_REALFIELD__gt=1.5')
    fc = lyr.GetFeatureCount()
    assert fc == 1, 'Expected feature count is 1, got {}.'.format(fc)

    lyr.SetAttributeFilter("STRFIELD ILIKE '%O_O'")
    fc = lyr.GetFeatureCount()
    assert fc == 2, 'Expected feature count is 2, got {}.'.format(fc)

###############################################################################
# Check spatial filter.

def test_ogr_ngw_13():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')

    # Reset any attribute filters
    lyr.SetAttributeFilter(None)

    # Check intersecting POINT(3 4)
    lyr.SetSpatialFilter(ogr.CreateGeometryFromWkt('POLYGON ((2.5 3.5,2.5 6,6 6,6 3.5,2.5 3.5))'))
    fc = lyr.GetFeatureCount()
    assert fc == 1, 'Expected feature count is 1, got {}.'.format(fc)

###############################################################################
# Check ignore geometry.


def test_ogr_ngw_14():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName('test_pt_layer')

    # Reset any attribute filters
    lyr.SetAttributeFilter(None)
    lyr.SetSpatialFilter(None)

    fd = lyr.GetLayerDefn()
    fd.SetGeometryIgnored(1)

    assert fd.IsGeometryIgnored(), 'geometry unexpectedly not ignored.'

    feat = lyr.GetNextFeature()

    assert feat.GetGeometryRef() is None, 'Unexpectedly got a geometry on feature 2.'

    feat = None

###############################################################################
# Check ExecuteSQL.

def test_ogr_ngw_15():
    if gdaltest.ngw_drv is None or gdaltest.ngw_ds is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
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
    assert ret == 0, 'Failed to create feature in test_pl_layer.'
    assert lyr.GetFeatureCount() == 1, 'Expected feature count is 1, got {}.'.format(lyr.GetFeatureCount())

    gdaltest.ngw_ds.ExecuteSQL('DELETE FROM test_pl_layer')
    assert lyr.GetFeatureCount() == 0, 'Expected feature count is 0, got {}.'.format(lyr.GetFeatureCount())

    gdaltest.ngw_ds.ExecuteSQL('ALTER TABLE test_pl_layer RENAME TO test_pl_layer777')
    lyr = gdaltest.ngw_ds.GetLayerByName('test_pl_layer777')
    assert lyr is not None, 'Get layer test_pl_layer777 failed.'

    # Create 2 new features

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 0,0 0))'))
    ret = lyr.CreateFeature(f)
    assert ret == 0, 'Failed to create feature in test_pl_layer777.'

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields2(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((1 1,1 2,2 1,1 1))'))
    ret = lyr.CreateFeature(f)
    assert ret == 0, 'Failed to create feature in test_pl_layer777.'

    lyr = gdaltest.ngw_ds.ExecuteSQL("SELECT STRFIELD,DECFIELD FROM test_pl_layer777 WHERE STRFIELD = 'fo_o'")
    assert lyr is not None, 'ExecuteSQL: SELECT STRFIELD,DECFIELD FROM test_pl_layer777 WHERE STRFIELD = "fo_o"; failed.'
    assert lyr.GetFeatureCount() == 2, 'Expected feature count is 2, got {}.'.format(lyr.GetFeatureCount())

    gdaltest.ngw_ds.ReleaseResultSet(lyr)

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

    url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + gdaltest.group_id

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' ' + url)
    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' ' + url + ' -oo PAGE_SIZE=100')
    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' ' + url + ' -oo BATCH_SIZE=5')
    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' ' + url + ' -oo BATCH_SIZE=5 -oo PAGE_SIZE=100')
    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Cleanup

def test_ogr_ngw_cleanup():

    if gdaltest.ngw_drv is None:
        pytest.skip()

    if check_availability(gdaltest.ngw_test_server) == False:
        gdaltest.ngw_ds = None
        gdaltest.ngw_drv = None
        pytest.skip()

    if gdaltest.group_id is not None:
        delete_url = 'NGW:' + gdaltest.ngw_test_server + '/resource/' + gdaltest.group_id

        gdaltest.ngw_layer = None
        gdaltest.ngw_ds = None

        assert gdaltest.ngw_drv.Delete(delete_url) == gdal.CE_None, \
            'Failed to delete datasource ' + delete_url + '.'

    gdaltest.ngw_ds = None

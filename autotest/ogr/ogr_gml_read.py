#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GML Reading Driver testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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

import os
import shutil

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.have_gml_reader = ogr.Open('data/gml/ionic_wfs.gml') is not None

    yield

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)
    gdal.SetConfigOption('GML_SAVE_RESOLVED_TO', None)

    gdaltest.clean_tmp()

    fl = gdal.ReadDir('/vsimem/')
    if fl is not None:
        print(fl)

    try:
        os.remove('data/gml/bom.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/utf8.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/ticket_2349_test_1.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/citygml.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/citygml_compound_crs.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/gnis_pop_100.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/gnis_pop_110.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/paris_typical_strike_demonstration.gfs')
    except OSError:
        pass
    try:
        os.remove('data/gml/global_geometry.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/global_geometry.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/global_geometry.xml')
    except OSError:
        pass
    try:
        os.remove('data/gml/curveProperty.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/ogr_gml_26.gml')
        os.remove('tmp/ogr_gml_26.xsd')
    except OSError:
        pass
    try:
        os.remove('tmp/ogr_gml_27.gml')
        os.remove('tmp/ogr_gml_27.xsd')
    except OSError:
        pass
    try:
        os.remove('tmp/ogr_gml_28.gml')
        os.remove('tmp/ogr_gml_28.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/GmlTopo-sample.sqlite')
    except OSError:
        pass
    try:
        os.remove('tmp/GmlTopo-sample.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/GmlTopo-sample.resolved.gml')
    except OSError:
        pass
    try:
        os.remove('tmp/GmlTopo-sample.xml')
    except OSError:
        pass
    try:
        os.remove('tmp/sample_gml_face_hole_negative_no.sqlite')
    except OSError:
        pass
    try:
        os.remove('tmp/sample_gml_face_hole_negative_no.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/sample_gml_face_hole_negative_no.resolved.gml')
    except OSError:
        pass
    try:
        os.remove('tmp/sample_gml_face_hole_negative_no.xml')
    except OSError:
        pass
    try:
        os.remove('data/gml/wfs_typefeature.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/ogr_gml_51.gml')
        os.remove('tmp/ogr_gml_51.xsd')
    except OSError:
        pass
    try:
        os.remove('tmp/gmlattributes.gml')
        os.remove('tmp/gmlattributes.gfs')
    except OSError:
        pass
    files = os.listdir('data')
    for filename in files:
        if len(filename) > 13 and filename[-13:] == '.resolved.gml':
            os.unlink('data/gml/' + filename)
    gdal.Unlink('data/gml/test_xsi_nil_gfs.gfs')

###############################################################################
# Test reading geometry and attribute from ionic wfs gml file.
#


def test_ogr_gml_1():
    if not gdaltest.have_gml_reader:
        pytest.skip()

    gml_ds = ogr.Open('data/gml/ionic_wfs.gml')

    assert gml_ds.GetLayerCount() == 1, 'wrong number of layers'

    lyr = gml_ds.GetLayerByName('GEM')
    feat = lyr.GetNextFeature()

    assert feat.GetField('Name') == 'Aartselaar', 'Wrong name field value'

    wkt = 'POLYGON ((44038 511549,44015 511548,43994 511522,43941 511539,43844 511514,43754 511479,43685 511521,43594 511505,43619 511452,43645 511417,4363 511387,437 511346,43749 511298,43808 511229,43819 511205,4379 511185,43728 511167,43617 511175,43604 511151,43655 511125,43746 511143,43886 511154,43885 511178,43928 511186,43977 511217,4404 511223,44008 511229,44099 51131,44095 511335,44106 51135,44127 511379,44124 511435,44137 511455,44105 511467,44098 511484,44086 511499,4407 511506,44067 511535,44038 511549))'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is None, 'got unexpected feature.'

###############################################################################
# Do the same test somewhere without a .gfs file.


def test_ogr_gml_2():
    if not gdaltest.have_gml_reader:
        pytest.skip()

    # copy gml file (but not .gfs file)
    open('tmp/ionic_wfs.gml', 'w').write(open('data/gml/ionic_wfs.gml').read())

    gml_ds = ogr.Open('tmp/ionic_wfs.gml')

    assert gml_ds.GetLayerCount() == 1, 'wrong number of layers'

    lyr = gml_ds.GetLayerByName('GEM')
    assert lyr.GetGeometryColumn() == 'Geometry'
    feat = lyr.GetNextFeature()

    assert feat.GetField('Name') == 'Aartselaar', 'Wrong name field value'

    wkt = 'POLYGON ((44038 511549,44015 511548,43994 511522,43941 511539,43844 511514,43754 511479,43685 511521,43594 511505,43619 511452,43645 511417,4363 511387,437 511346,43749 511298,43808 511229,43819 511205,4379 511185,43728 511167,43617 511175,43604 511151,43655 511125,43746 511143,43886 511154,43885 511178,43928 511186,43977 511217,4404 511223,44008 511229,44099 51131,44095 511335,44106 51135,44127 511379,44124 511435,44137 511455,44105 511467,44098 511484,44086 511499,4407 511506,44067 511535,44038 511549))'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is None, 'got unexpected feature.'

###############################################################################
# Similar test for RNF style line data.


def test_ogr_gml_3():
    if not gdaltest.have_gml_reader:
        pytest.skip()

    gml_ds = ogr.Open('data/gml/rnf_eg.gml')

    assert gml_ds.GetLayerCount() == 1, 'wrong number of layers'

    lyr = gml_ds.GetLayerByName('RoadSegment')
    feat = lyr.GetNextFeature()

    assert feat.GetField('ngd_id') == 817792, 'Wrong ngd_id field value'

    assert feat.GetField('type') == 'HWY', 'Wrong type field value'

    wkt = 'LINESTRING (-63.500411040289066 46.240122507771368,-63.501009714909742 46.240344881690326,-63.502170462373471 46.241041855639622,-63.505862621395394 46.24195250605576,-63.506719184531178 46.242002742901576,-63.507197272602212 46.241931577811606,-63.508403092799554 46.241752283460158,-63.509946573455622 46.241745397977233)'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is None, 'got unexpected feature.'

###############################################################################
# Test of read GML file with UTF-8 BOM indicator.
# Test also support for nested GML elements (#3680)


def test_ogr_gml_4():
    if not gdaltest.have_gml_reader:
        pytest.skip()

    gml_ds = ogr.Open('data/gml/bom.gml')

    assert gml_ds.GetLayerCount() == 1, 'wrong number of layers'

    lyr = gml_ds.GetLayerByName('CartographicText')

    assert lyr.GetFeatureCount() == 3, 'wrong number of features'

    # Test 1st feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('featureCode') == 10198, 'Wrong featureCode field value'

    assert feat.GetField('anchorPosition') == 8, 'Wrong anchorPosition field value'

    wkt = 'POINT (347243.85 461299.5)'

    assert not ogrtest.check_feature_geometry(feat, wkt)

    # Test 2nd feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('featureCode') == 10069, 'Wrong featureCode field value'

    wkt = 'POINT (347251.45 461250.85)'

    assert not ogrtest.check_feature_geometry(feat, wkt)


###############################################################################
# Test of read GML file that triggeered bug #2349

def test_ogr_gml_5():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gml_ds = ogr.Open('data/gml/ticket_2349_test_1.gml')

    lyr = gml_ds.GetLayerByName('MyPolyline')

    lyr.SetAttributeFilter('height > 300')

    lyr.GetNextFeature()

###############################################################################
# Test of various FIDs (various prefixes and lengths) (Ticket#1017)


def test_ogr_gml_6():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    files = ['test_point1', 'test_point2', 'test_point3', 'test_point4']
    fids = []

    for filename in files:
        fids[:] = []
        gml_ds = ogr.Open(os.path.join('data', 'gml', filename + '.gml'))
        lyr = gml_ds.GetLayer()
        feat = lyr.GetNextFeature()
        while feat is not None:
            if (feat.GetFID() < 0) or (feat.GetFID() in fids):
                gml_ds = None
                os.remove(os.path.join('data', 'gml', filename + '.gfs'))
                pytest.fail('Wrong FID value')
            fids.append(feat.GetFID())
            feat = lyr.GetNextFeature()
        gml_ds = None
        os.remove(os.path.join('data', 'gml', filename + '.gfs'))


###############################################################################
# Test of colon terminated prefixes for attribute values (Ticket#2493)


def test_ogr_gml_7():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.SetConfigOption('GML_EXPOSE_FID', 'FALSE')
    gml_ds = ogr.Open('data/gml/test_point.gml')
    gdal.SetConfigOption('GML_EXPOSE_FID', None)
    lyr = gml_ds.GetLayer()
    ldefn = lyr.GetLayerDefn()

    # Test fix for #2969
    assert lyr.GetFeatureCount() == 5, 'Bad feature count'

    try:
        ldefn.GetFieldDefn(0).GetFieldTypeName
    except:
        pytest.skip()

    assert ldefn.GetFieldDefn(0).GetFieldTypeName(ldefn.GetFieldDefn(0).GetType()) == 'Real'
    assert ldefn.GetFieldDefn(1).GetFieldTypeName(ldefn.GetFieldDefn(1).GetType()) == 'Integer'
    assert ldefn.GetFieldDefn(2).GetFieldTypeName(ldefn.GetFieldDefn(2).GetType()) == 'String'

###############################################################################
# Test a GML file with some non-ASCII UTF-8 content that triggered a bug (Ticket#2948)


def test_ogr_gml_8():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gml_ds = ogr.Open('data/gml/utf8.gml')
    lyr = gml_ds.GetLayer()
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('name') == 'Āliamanu'


###############################################################################
# Test writing invalid UTF-8 content in a GML file (ticket #2971)


def test_ogr_gml_9():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('tmp/broken_utf8.gml')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('test', ogr.OFTString))

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetFieldBinaryFromHexString('test', '80626164')  # \x80bad'

    # Avoid the warning
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(dst_feat)
    gdal.PopErrorHandler()

    assert ret == 0, 'CreateFeature failed.'

    ds = None

    ds = ogr.Open('tmp/broken_utf8.gml')
    lyr = ds.GetLayerByName('test')
    feat = lyr.GetNextFeature()
    assert feat.GetField('test') == '?bad', 'Unexpected content.'
    ds = None

    os.remove('tmp/broken_utf8.gml')
    os.remove('tmp/broken_utf8.xsd')

###############################################################################
# Test writing different data types in a GML file (ticket #2857)
# TODO: Add test for other data types as they are added to the driver.


def test_ogr_gml_10():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('tmp/fields.gml')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('string', ogr.OFTString)
    field_defn.SetWidth(100)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('real', ogr.OFTReal)
    field_defn.SetWidth(4)
    field_defn.SetPrecision(2)
    lyr.CreateField(field_defn)
    lyr.CreateField(ogr.FieldDefn('float', ogr.OFTReal))
    field_defn = ogr.FieldDefn('integer', ogr.OFTInteger)
    field_defn.SetWidth(5)
    lyr.CreateField(field_defn)
    lyr.CreateField(ogr.FieldDefn('date', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('time', ogr.OFTTime))
    lyr.CreateField(ogr.FieldDefn('datetime', ogr.OFTDateTime))

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('string', 'test string of length 24')
    dst_feat.SetField('real', 12.34)
    dst_feat.SetField('float', 1234.5678)
    dst_feat.SetField('integer', '1234')
    dst_feat.SetField('date', '2019/11/06')
    dst_feat.SetField('time', '12:34:56')
    dst_feat.SetField('datetime', '2019/11/06 12:34:56+00')

    ret = lyr.CreateFeature(dst_feat)

    assert ret == 0, 'CreateFeature failed.'

    ds = None

    ds = ogr.Open('tmp/fields.gml')
    lyr = ds.GetLayerByName('test')
    feat = lyr.GetNextFeature()

    assert feat.GetFieldDefnRef(feat.GetFieldIndex('string')).GetType() == ogr.OFTString, \
        ('String type is reported wrong. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('string')).GetType()))
    assert feat.GetFieldDefnRef(feat.GetFieldIndex('real')).GetType() == ogr.OFTReal, \
        ('Real type is reported wrong. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('real')).GetType()))
    assert feat.GetFieldDefnRef(feat.GetFieldIndex('float')).GetType() == ogr.OFTReal, \
        ('Float type is not reported as OFTReal. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('float')).GetType()))
    assert feat.GetFieldDefnRef(feat.GetFieldIndex('integer')).GetType() == ogr.OFTInteger, \
        ('Integer type is reported wrong. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('integer')).GetType()))
    assert feat.GetFieldDefnRef(feat.GetFieldIndex('date')).GetType() == ogr.OFTDate
    assert feat.GetFieldDefnRef(feat.GetFieldIndex('time')).GetType() == ogr.OFTTime
    assert feat.GetFieldDefnRef(feat.GetFieldIndex('datetime')).GetType() == ogr.OFTDateTime

    assert feat.GetField('string') == 'test string of length 24', \
        ('Unexpected string content.' + feat.GetField('string'))
    assert feat.GetFieldAsDouble('real') == 12.34, 'Unexpected real content.'
    assert feat.GetField('float') == 1234.5678, 'Unexpected float content.'
    assert feat.GetField('integer') == 1234, 'Unexpected integer content.'
    assert feat.GetField('date') == '2019/11/06'
    assert feat.GetField('time') == '12:34:56'
    assert feat.GetField('datetime') == '2019/11/06 12:34:56+00'

    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('string')).GetWidth() == 100, \
        'Unexpected width of string field.'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('real')).GetWidth() == 4, \
        'Unexpected width of real field.'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('real')).GetPrecision() == 2, \
        'Unexpected precision of real field.'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('integer')).GetWidth() == 5, \
        'Unexpected width of integer field.'
    ds = None

    os.remove('tmp/fields.gml')
    os.remove('tmp/fields.xsd')

###############################################################################
# Test reading a geometry element specified with <GeometryElementPath>


def test_ogr_gml_11():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # Make sure the .gfs file is more recent that the .gml one
    try:
        gml_mtime = os.stat('data/gml/testgeometryelementpath.gml').st_mtime
        gfs_mtime = os.stat('data/gml/testgeometryelementpath.gfs').st_mtime
        touch_gfs = gfs_mtime <= gml_mtime
    except:
        touch_gfs = True
    if touch_gfs:
        print('Touching .gfs file')
        f = open('data/gml/testgeometryelementpath.gfs', 'rb+')
        data = f.read(1)
        f.seek(0, 0)
        f.write(data)
        f.close()

    ds = ogr.Open('data/gml/testgeometryelementpath.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == 'location1container|location1', \
        'did not get expected geometry column name'

    feat = lyr.GetNextFeature()
    assert feat.GetField('attrib1') == 'attrib1_value', \
        'did not get expected value for attrib1'
    assert feat.GetField('attrib2') == 'attrib2_value', \
        'did not get expected value for attrib2'
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (3 50)', 'did not get expected geometry'
    ds = None

###############################################################################
# Test reading a virtual GML file


def test_ogr_gml_12():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('/vsizip/data/gml/testgeometryelementpath.zip/testgeometryelementpath.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == 'location1container|location1', \
        'did not get expected geometry column name'

    feat = lyr.GetNextFeature()
    assert feat.GetField('attrib1') == 'attrib1_value', \
        'did not get expected value for attrib1'
    assert feat.GetField('attrib2') == 'attrib2_value', \
        'did not get expected value for attrib2'
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (3 50)', 'did not get expected geometry'
    ds = None

###############################################################################
# Test reading GML with StringList, IntegerList and RealList fields


def test_ogr_gml_13():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    for _ in range(2):
        ds = ogr.Open('data/gml/testlistfields.gml')
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        assert feat.GetFieldAsStringList(feat.GetFieldIndex('attrib1')) == ['value1', 'value2'], \
            'did not get expected value for attrib1'
        assert feat.GetField(feat.GetFieldIndex('attrib2')) == 'value3', \
            'did not get expected value for attrib2'
        assert feat.GetFieldAsIntegerList(feat.GetFieldIndex('attrib3')) == [4, 5], \
            'did not get expected value for attrib3'
        assert feat.GetFieldAsDoubleList(feat.GetFieldIndex('attrib4')) == [6.1, 7.1], \
            'did not get expected value for attrib4'
        ds = None
    gdal.Unlink('data/gml/testlistfields.gfs')

###############################################################################
# Test xlink resolution


def test_ogr_gml_14():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # We need CURL for xlink resolution, and a sign that Curl is available
    # is the availability of the WMS driver
    gdaltest.wms_drv = gdal.GetDriverByName('WMS')
    if gdaltest.wms_drv is None:
        pytest.skip()

    if gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/gml/xlink3.gml') is None:
        pytest.skip('cannot open URL')

    files = ['xlink1.gml', 'xlink2.gml', 'expected1.gml', 'expected2.gml']
    for f in files:
        if not gdaltest.download_file('http://download.osgeo.org/gdal/data/gml/' + f, f):
            pytest.skip()

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', 'NONE')
    gdal.SetConfigOption('GML_SAVE_RESOLVED_TO', 'tmp/cache/xlink1resolved.gml')
    with gdaltest.error_handler():
        gml_ds = ogr.Open('tmp/cache/xlink1.gml')
    gml_ds = None
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', 'gml:directedNode')
    gdal.SetConfigOption('GML_SAVE_RESOLVED_TO', 'tmp/cache/xlink2resolved.gml')
    gml_ds = ogr.Open('tmp/cache/xlink1.gml')
    del gml_ds
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)
    gdal.SetConfigOption('GML_SAVE_RESOLVED_TO', None)

    try:
        fp = open('tmp/cache/xlink1resolved.gml', 'r')
        text = fp.read()
        fp.close()
        os.remove('tmp/cache/xlink1resolved.gml')
        fp = open('tmp/cache/expected1.gml', 'r')
        expectedtext = fp.read()
        fp.close()
    except (IOError, OSError):
        pytest.fail()

    assert text == expectedtext, 'Problem with file 1'

    try:
        fp = open('tmp/cache/xlink2resolved.gml', 'r')
        text = fp.read()
        fp.close()
        os.remove('tmp/cache/xlink2resolved.gml')
        fp = open('tmp/cache/expected2.gml', 'r')
        expectedtext = fp.read()
        fp.close()
    except (IOError, OSError):
        pytest.fail()

    assert text == expectedtext, 'Problem with file 2'

###############################################################################
# Run test_ogrsf


def test_ogr_gml_15():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/gml/test_point.gml')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Read CityGML generic attributes


def test_ogr_gml_16():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/citygml.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if feat.GetField('Name_') != 'aname' or \
       feat.GetField('a_int_attr') != 2 or \
       feat.GetField('a_double_attr') != 3.45:
        feat.DumpReadable()
        pytest.fail('did not get expected values')

###############################################################################
# Test reading CityGML of Project PLATEAU


def test_gml_read_compound_crs_lat_long():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # open CityGML file
    gml = ogr.Open('data/gml/citygml_compound_crs.gml')

    # check number of layers
    assert gml.GetLayerCount() == 1, 'Wrong layer count'

    lyr = gml.GetLayer(0)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(6668)  # JGD2011
    assert sr.IsSame(lyr.GetSpatialRef(), options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), 'Wrong SRS'

    wkt = 'POLYHEDRALSURFACE Z (((139.812484938717 35.7092130413279 0.15,139.812489071491 35.7091641446533 0.15,139.812444202746 35.7091610722245 0.15,139.812439721473 35.7092112956502 0.15,139.812436111402 35.7092517484017 0.15,139.812481422309 35.7092546406366 0.15,139.812484938717 35.7092130413279 0.15)),((139.812484938717 35.7092130413279 0.15,139.812481422309 35.7092546406366 0.15,139.812481422309 35.7092546406366 12.08,139.812484938717 35.7092130413279 12.08,139.812484938717 35.7092130413279 0.15)),((139.812481422309 35.7092546406366 0.15,139.812436111402 35.7092517484017 0.15,139.812436111402 35.7092517484017 12.08,139.812481422309 35.7092546406366 12.08,139.812481422309 35.7092546406366 0.15)),((139.812436111402 35.7092517484017 0.15,139.812439721473 35.7092112956502 0.15,139.812439721473 35.7092112956502 12.08,139.812436111402 35.7092517484017 12.08,139.812436111402 35.7092517484017 0.15)),((139.812439721473 35.7092112956502 0.15,139.812444202746 35.7091610722245 0.15,139.812444202746 35.7091610722245 12.08,139.812439721473 35.7092112956502 12.08,139.812439721473 35.7092112956502 0.15)),((139.812444202746 35.7091610722245 0.15,139.812489071491 35.7091641446533 0.15,139.812489071491 35.7091641446533 12.08,139.812444202746 35.7091610722245 12.08,139.812444202746 35.7091610722245 0.15)),((139.812489071491 35.7091641446533 0.15,139.812484938717 35.7092130413279 0.15,139.812484938717 35.7092130413279 12.08,139.812489071491 35.7091641446533 12.08,139.812489071491 35.7091641446533 0.15)),((139.812484938717 35.7092130413279 12.08,139.812481422309 35.7092546406366 12.08,139.812436111402 35.7092517484017 12.08,139.812439721473 35.7092112956502 12.08,139.812444202746 35.7091610722245 12.08,139.812489071491 35.7091641446533 12.08,139.812484938717 35.7092130413279 12.08)))'

    # check the first feature
    feat = lyr.GetNextFeature()
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Wrong geometry'

###############################################################################
# Read layer SRS for WFS 1.0.0 return


def test_ogr_gml_17():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/gnis_pop_100.gml')
    lyr = ds.GetLayer(0)

    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    assert got_wkt.find('GEOGCS["WGS 84"') != -1, 'did not get expected SRS'

    assert lyr.GetExtent() == (-80.17, 76.58, -13.32, 51.0)

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    assert got_wkt == 'POINT (2.09 34.12)', 'did not get expected geometry'

###############################################################################
# Read layer SRS for WFS 1.1.0 return


def test_ogr_gml_18():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/gnis_pop_110.gml')
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    assert got_wkt.find('GEOGCS["WGS 84"') != -1, 'did not get expected SRS'
    assert sr.GetDataAxisToSRSAxisMapping() == [2, 1]

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    assert got_wkt == 'POINT (2.09 34.12)', 'did not get expected geometry'

###############################################################################
# Read layer SRS for WFS 1.1.0 return, but without trying to restore
# (long, lat) order. So we should get EPSGA:4326 and (lat, long) order


def test_ogr_gml_19():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    try:
        os.remove('data/gml/gnis_pop_110.gfs')
    except OSError:
        pass

    gdal.SetConfigOption('GML_INVERT_AXIS_ORDER_IF_LAT_LONG', 'NO')
    ds = ogr.Open('data/gml/gnis_pop_110.gml')
    gdal.SetConfigOption('GML_INVERT_AXIS_ORDER_IF_LAT_LONG', None)

    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    assert 'GEOGCS["WGS 84"' in got_wkt, \
        'did not get expected SRS'
    assert sr.GetDataAxisToSRSAxisMapping() == [1, 2]

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    assert got_wkt == 'POINT (34.12 2.09)', 'did not get expected geometry'

###############################################################################
# Test parsing a .xsd where the type definition is before its reference


def test_ogr_gml_20():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    try:
        os.remove('data/gml/archsites.gfs')
    except OSError:
        pass

    ds = ogr.Open('data/gml/archsites.gml')
    lyr = ds.GetLayer(0)
    ldefn = lyr.GetLayerDefn()

    try:
        ldefn.GetFieldDefn(0).GetFieldTypeName
    except:
        pytest.skip()

    idx = ldefn.GetFieldIndex("gml_id")
    assert idx != -1, 'did not get expected column "gml_id"'

    idx = ldefn.GetFieldIndex("cat")
    fddefn = ldefn.GetFieldDefn(idx)
    assert fddefn.GetFieldTypeName(fddefn.GetType()) == 'Integer64', \
        'did not get expected column type for col "cat"'
    idx = ldefn.GetFieldIndex("str1")
    fddefn = ldefn.GetFieldDefn(idx)
    assert fddefn.GetFieldTypeName(fddefn.GetType()) == 'String', \
        'did not get expected column type for col "str1"'

    assert lyr.GetGeometryColumn() == 'the_geom', \
        'did not get expected geometry column name'

    assert ldefn.GetGeomType() == ogr.wkbPoint, 'did not get expected geometry type'

    ds = None

    try:
        os.stat('data/gml/archsites.gfs')
        pytest.fail('did not expected .gfs -> XSD parsing failed')
    except OSError:
        return

###############################################################################
# Test writing GML3


@pytest.mark.parametrize('frmt,base_filename',
                         [('GML3', 'expected_gml_gml3'),
                          ('GML3Deegree', 'expected_gml_gml3degree'),
                          ('GML3.2', 'expected_gml_gml32')
                         ])
def test_ogr_gml_21(frmt,base_filename):

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # Create GML3 file
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    for ext in ('gml', 'gfs', 'xsd'):
        gdal.Unlink('tmp/' + base_filename + '.' + ext)

    filename = 'tmp/' + base_filename + '.gml'
    ds = ogr.GetDriverByName('GML').CreateDataSource(filename, options=['FORMAT=' + frmt])
    lyr = ds.CreateLayer('firstlayer', srs=sr)
    lyr.CreateField(ogr.FieldDefn('string_field', ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo')
    geom = ogr.CreateGeometryFromWkt('POINT (3 48)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    ds = None

    # Reopen the file
    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (2 49)', \
        'did not get expected geometry'
    ds = None

    # Test that .gml and .xsd are identical to what is expected
    f1 = open(filename, 'rt')
    f2 = open('data/gml/' + base_filename + '.gml', 'rt')
    line1 = f1.readline()
    line2 = f2.readline()
    while line1 != '':
        line1 = line1.strip()
        line2 = line2.strip()
        if line1 != line2:
            print(open(filename, 'rt').read())
            pytest.fail('.gml file not identical to expected')
        line1 = f1.readline()
        line2 = f2.readline()
    f1.close()
    f2.close()

    xsd_filename = filename[0:-3] + 'xsd'
    f1 = open(xsd_filename, 'rt')
    f2 = open('tmp/' + base_filename + '.xsd', 'rt')
    line1 = f1.readline()
    line2 = f2.readline()
    while line1 != '':
        line1 = line1.strip()
        line2 = line2.strip()
        if line1 != line2:
            print(open(xsd_filename, 'rt').read())
            pytest.fail('.xsd file not identical to expected')
        line1 = f1.readline()
        line2 = f2.readline()
    f1.close()
    f2.close()

    for ext in ('gml', 'gfs', 'xsd'):
        gdal.Unlink('tmp/' + base_filename + '.' + ext)

###############################################################################
# Read a OpenLS DetermineRouteResponse document


def test_ogr_gml_22():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/paris_typical_strike_demonstration.xml')
    lyr = ds.GetLayerByName('RouteGeometry')
    assert lyr is not None, 'cannot find RouteGeometry'
    lyr = ds.GetLayerByName('RouteInstruction')
    assert lyr is not None, 'cannot find RouteInstruction'
    count = lyr.GetFeatureCount()
    assert count == 9, 'did not get expected feature count'

    ds = None

###############################################################################
# Test that use SRS defined in global gml:Envelope if no SRS is set for any
# feature geometry


def test_ogr_gml_23():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    try:
        os.remove('tmp/global_geometry.gfs')
    except OSError:
        pass

    shutil.copy('data/gml/global_geometry.xml', 'tmp/global_geometry.xml')

    # Here we use only the .xml file
    ds = ogr.Open('tmp/global_geometry.xml')

    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    assert 'GEOGCS["WGS 84"' in got_wkt, \
        'did not get expected SRS'
    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2, 1]

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    assert got_wkt == 'POINT (2 49)', 'did not get expected geometry'

    extent = lyr.GetExtent()
    assert extent == (2.0, 3.0, 49.0, 50.0), 'did not get expected layer extent'

###############################################################################
# Test that use SRS defined in global gml:Envelope if no SRS is set for any
# feature geometry


def test_ogr_gml_24():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    try:
        os.remove('data/gml/global_geometry.gfs')
    except OSError:
        pass

    # Here we use only the .xml file and the .xsd file
    ds = ogr.Open('data/gml/global_geometry.xml')

    lyr = ds.GetLayer(0)

    # Because we read the .xsd, we (currently) don't find the SRS

    # sr = lyr.GetSpatialRef()
    # got_wkt = sr.ExportToWkt()
    # if got_wkt.find('GEOGCS["WGS 84"') == -1 or \
    #   got_wkt.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') != -1:
    #    gdaltest.post_reason('did not get expected SRS')
    #    print(got_wkt)
    #    return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    assert got_wkt == 'POINT (2 49)', 'did not get expected geometry'

    extent = lyr.GetExtent()
    assert extent == (2.0, 3.0, 49.0, 50.0), 'did not get expected layer extent'

###############################################################################
# Test fixes for #3934 and #3935


def test_ogr_gml_25():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        pytest.skip('would crash')

    try:
        os.remove('data/gml/curveProperty.gfs')
    except OSError:
        pass

    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', 'YES')
    ds = ogr.Open('data/gml/curveProperty.xml')
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)

    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    assert got_wkt == 'POLYGON ((14 21,6 21,6 9,14 9,22 9,22 21,14 21))', \
        'did not get expected geometry'

###############################################################################
# Test writing and reading 3D geoms (GML2)


def test_ogr_gml_26():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML -dsco FORMAT=GML2 tmp/ogr_gml_26.gml data/poly.shp -zfield eas_id')

    f = open('tmp/ogr_gml_26.gml', 'rt')
    content = f.read()
    f.close()
    assert content.find("<gml:coord><gml:X>478315.53125</gml:X><gml:Y>4762880.5</gml:Y><gml:Z>158</gml:Z></gml:coord>") != -1

    ds = ogr.Open('tmp/ogr_gml_26.gml')

    lyr = ds.GetLayer(0)

    assert lyr.GetGeomType() == ogr.wkbPolygon25D

    ds = None

###############################################################################
# Test writing and reading 3D geoms (GML3)


def test_ogr_gml_27():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/ogr_gml_27.gml data/poly.shp -zfield eas_id -dsco FORMAT=GML3')

    f = open('tmp/ogr_gml_27.gml', 'rt')
    content = f.read()
    f.close()
    assert content.find("<gml:lowerCorner>478315.53125 4762880.5 158</gml:lowerCorner>") != -1

    ds = ogr.Open('tmp/ogr_gml_27.gml')

    lyr = ds.GetLayer(0)

    assert lyr.GetGeomType() == ogr.wkbPolygon25D

    ds = None

###############################################################################
# Test writing and reading layers of type wkbNone (#4154)


def test_ogr_gml_28():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/ogr_gml_28.gml data/idlink.dbf')

    # Try with .xsd
    ds = ogr.Open('tmp/ogr_gml_28.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbNone
    ds = None

    os.unlink('tmp/ogr_gml_28.xsd')

    ds = ogr.Open('tmp/ogr_gml_28.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbNone
    ds = None

    # Try with .gfs
    ds = ogr.Open('tmp/ogr_gml_28.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbNone
    ds = None

###############################################################################
# Test reading FME GMLs


def test_ogr_gml_29():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/testfmegml.gml')

    expected_results = [[ogr.wkbMultiPoint, 'MULTIPOINT (2 49)'],
                        [ogr.wkbMultiPolygon, 'MULTIPOLYGON (((2 49,3 49,3 50,2 50,2 49)))'],
                        [ogr.wkbMultiLineString, 'MULTILINESTRING ((2 49,3 50))'],
                       ]

    for j, expected_result in enumerate(expected_results):
        lyr = ds.GetLayer(j)
        assert lyr.GetGeomType() == expected_result[0], \
            ('layer %d, did not get expected layer geometry type' % j)
        for _ in range(2):
            feat = lyr.GetNextFeature()
            geom = feat.GetGeometryRef()
            got_wkt = geom.ExportToWkt()
            assert got_wkt == expected_result[1], \
                ('layer %d, did not get expected geometry' % j)

    ds = None

###############################################################################
# Test reading a big field and a big geometry


def test_ogr_gml_30():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    field1 = " "
    for _ in range(11):
        field1 = field1 + field1

    geom = "0 1 " * 512

    data = """<FeatureCollection xmlns:gml="http://www.opengis.net/gml">
  <gml:featureMember>
    <layer1>
      <geometry><gml:LineString><gml:posList>%s</gml:posList></gml:LineString></geometry>
      <field1>A%sZ</field1>
    </layer1>
  </gml:featureMember>
</FeatureCollection>""" % (geom, field1)

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_30.gml", "wb")
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open("/vsimem/ogr_gml_30.gml")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    field1 = feat.GetField(0)
    geom_wkt = feat.GetGeometryRef().ExportToWkt()
    ds = None

    gdal.Unlink("/vsimem/ogr_gml_30.gml")
    gdal.Unlink("/vsimem/ogr_gml_30.gfs")

    assert len(field1) == 2050, 'did not get expected len(field1)'

    assert len(geom_wkt) == 2060, 'did not get expected len(geom_wkt)'

###############################################################################
# Test SEQUENTIAL_LAYERS


def test_ogr_gml_31():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.SetConfigOption('GML_READ_MODE', 'SEQUENTIAL_LAYERS')
    test_ogr_gml_29()
    gdal.SetConfigOption('GML_READ_MODE', None)

    # Test reading second layer and then first layer
    gdal.SetConfigOption('GML_READ_MODE', 'SEQUENTIAL_LAYERS')
    ds = ogr.Open('data/gml/testfmegml.gml')
    gdal.SetConfigOption('GML_READ_MODE', None)

    lyr = ds.GetLayer(1)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert feat.GetFID() == 1, 'did not get feature when reading directly second layer'

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert feat.GetFID() == 1, 'did not get feature when reading back first layer'

###############################################################################
# Test SEQUENTIAL_LAYERS without a .gfs


def test_ogr_gml_32():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # Test without .xsd or .gfs
    f = gdal.VSIFOpenL("data/gml/testfmegml.gml", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_31.gml", "wb")
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/ogr_gml_31.gml')

    lyr = ds.GetLayer(1)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert feat.GetFID() == 1, 'did not get feature when reading directly second layer'

    ds = None

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_31.gfs", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    data = str(data)

    assert data.find("<SequentialLayers>true</SequentialLayers>") != -1, \
        'did not find <SequentialLayers>true</SequentialLayers> in .gfs'

    gdal.Unlink("/vsimem/ogr_gml_31.gml")
    gdal.Unlink("/vsimem/ogr_gml_31.gfs")

###############################################################################
# Test INTERLEAVED_LAYERS


def test_ogr_gml_33():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # Test reading second layer and then first layer
    gdal.SetConfigOption('GML_READ_MODE', 'INTERLEAVED_LAYERS')
    ds = ogr.Open('data/gml/testfmegml_interleaved.gml')
    gdal.SetConfigOption('GML_READ_MODE', None)

    read_sequence = [[0, 1],
                     [0, None],
                     [1, 3],
                     [2, 5],
                     [2, None],
                     [0, 2],
                     [1, 4],
                     [1, None],
                     [2, 6],
                     [2, None],
                     [0, None],
                     [1, None],
                     [2, None]]

    for i, read_seq in enumerate(read_sequence):
        lyr = ds.GetLayer(read_seq[0])
        feat = lyr.GetNextFeature()
        if feat is None:
            fid = None
        else:
            fid = feat.GetFID()
        expected_fid = read_seq[1]
        assert fid == expected_fid, ('failed at step %d' % i)


###############################################################################
# Test writing non-ASCII UTF-8 content (#4117, #4299)


def test_ogr_gml_34():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('/vsimem/ogr_gml_34.gml')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '\xc4\x80liamanu<&')
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_gml_34.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('name') == '\xc4\x80liamanu<&'
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_34.gml')
    gdal.Unlink('/vsimem/ogr_gml_34.xsd')

###############################################################################
# Test GML_SKIP_RESOLVE_ELEMS=HUGE (#4380)


def test_ogr_gml_35():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    if ogr.GetDriverByName('SQLite') is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.remove('tmp/GmlTopo-sample.sqlite')
    except OSError:
        pass
    try:
        os.remove('tmp/GmlTopo-sample.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/GmlTopo-sample.resolved.gml')
    except OSError:
        pass

    shutil.copy('data/gml/GmlTopo-sample.xml', 'tmp/GmlTopo-sample.xml')

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', 'HUGE')
    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)

    assert not os.path.exists('tmp/GmlTopo-sample.sqlite')

    assert gdal.GetLastErrorMsg() == '', 'did not expect error'
    assert ds.GetLayerCount() == 3, ('expected 3 layers, got %d' % ds.GetLayerCount())

    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    wkt = 'MULTIPOLYGON (((-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6)))'
    assert not ogrtest.check_feature_geometry(feat, wkt), feat.GetGeometryRef()

    ds = None

    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    assert not ogrtest.check_feature_geometry(feat, wkt), feat.GetGeometryRef()

    ds = None

###############################################################################
# Test GML_SKIP_RESOLVE_ELEMS=NONE (and new GMLTopoSurface interpretation)


def test_ogr_gml_36(GML_FACE_HOLE_NEGATIVE='NO'):

    if not gdaltest.have_gml_reader:
        pytest.skip()

    if GML_FACE_HOLE_NEGATIVE == 'NO':
        if not ogrtest.have_geos():
            pytest.skip()

    try:
        os.remove('tmp/GmlTopo-sample.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/GmlTopo-sample.resolved.gml')
    except OSError:
        pass

    shutil.copy('data/gml/GmlTopo-sample.xml', 'tmp/GmlTopo-sample.xml')

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', 'NONE')
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', GML_FACE_HOLE_NEGATIVE)
    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)
    assert gdal.GetLastErrorMsg() == '', 'did not expect error'

    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    if GML_FACE_HOLE_NEGATIVE == 'NO':
        wkt = 'MULTIPOLYGON (((-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6)))'
    else:
        wkt = 'POLYGON ((-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6),(0.2 0.2,0.2 0.4,0.4 0.4,0.5 0.2,0.5 0.1,0.5 0.0,0.2 0.0,0.2 0.2),(0.6 0.1,0.8 0.1,0.8 -0.1,0.6 -0.1,0.6 0.1))'
    assert not ogrtest.check_feature_geometry(feat, wkt), feat.GetGeometryRef()

    ds = None

    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', GML_FACE_HOLE_NEGATIVE)
    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)
    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    assert not ogrtest.check_feature_geometry(feat, wkt), feat.GetGeometryRef()

    ds = None

###############################################################################
# Test GML_SKIP_RESOLVE_ELEMS=NONE with old GMLTopoSurface interpretation


def test_ogr_gml_37():
    return test_ogr_gml_36('YES')

###############################################################################
# Test new GMLTopoSurface interpretation (#3934) with HUGE xlink resolver


def test_ogr_gml_38(resolver='HUGE'):

    if not gdaltest.have_gml_reader:
        pytest.skip()

    if resolver == 'HUGE':
        if ogr.GetDriverByName('SQLite') is None:
            pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    try:
        os.remove('tmp/sample_gml_face_hole_negative_no.sqlite')
    except OSError:
        pass
    try:
        os.remove('tmp/sample_gml_face_hole_negative_no.gfs')
    except OSError:
        pass
    try:
        os.remove('tmp/sample_gml_face_hole_negative_no.resolved.gml')
    except OSError:
        pass

    shutil.copy('data/gml/sample_gml_face_hole_negative_no.xml', 'tmp/sample_gml_face_hole_negative_no.xml')

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', resolver)
    ds = ogr.Open('tmp/sample_gml_face_hole_negative_no.xml')
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)

    if resolver == 'HUGE':
        assert not os.path.exists('tmp/sample_gml_face_hole_negative_no.sqlite')

    assert gdal.GetLastErrorMsg() == '', 'did not expect error'

    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    wkt = 'MULTIPOLYGON (((0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6),(0.6 0.1,0.6 -0.1,0.8 -0.1,0.8 0.1,0.6 0.1),(0.2 0.4,0.2 0.2,0.2 0.0,0.5 0.0,0.5 0.1,0.5 0.2,0.4 0.4,0.2 0.4)))'
    assert not ogrtest.check_feature_geometry(feat, wkt), feat.GetGeometryRef()

    ds = None

###############################################################################
# Test new GMLTopoSurface interpretation (#3934) with standard xlink resolver


def test_ogr_gml_39():
    return test_ogr_gml_38('NONE')

###############################################################################
# Test parsing XSD where simpleTypes not inlined, but defined elsewhere in the .xsd (#4328)


def test_ogr_gml_40():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/testLookForSimpleType.xml')
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('CITYNAME'))
    assert fld_defn.GetWidth() == 26

###############################################################################
# Test validating against .xsd


def test_ogr_gml_41():

    gdaltest.have_gml_validation = False

    #if gdal.GetDriverByName('GMLAS'):
    #    gdaltest.have_gml_validation = True
    #    return

    if not gdaltest.have_gml_reader:
        pytest.skip()

    if not gdaltest.download_file('http://schemas.opengis.net/SCHEMAS_OPENGIS_NET.zip', 'SCHEMAS_OPENGIS_NET.zip'):
        pytest.skip()

    ds = ogr.Open('data/gml/expected_gml_gml3.gml')

    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', '/vsizip/./tmp/cache/SCHEMAS_OPENGIS_NET.zip')
    lyr = ds.ExecuteSQL('SELECT ValidateSchema()')
    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', None)

    feat = lyr.GetNextFeature()
    val = feat.GetFieldAsInteger(0)
    feat = None

    ds.ReleaseResultSet(lyr)

    if val == 0:
        assert gdal.GetLastErrorMsg().find('not implemented due to missing libxml2 support') != -1
        pytest.skip()

    gdaltest.have_gml_validation = True

###############################################################################

def validate(filename):

    if not gdaltest.have_gml_validation:
        pytest.skip()

    #if gdal.GetDriverByName('GMLAS'):
    #    assert gdal.OpenEx('GMLAS:' + filename, open_options=['VALIDATE=YES', 'FAIL_IF_VALIDATION_ERROR=YES']) is not None
    #    return

    try:
        os.mkdir('tmp/cache/SCHEMAS_OPENGIS_NET')
    except OSError:
        pass

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/gml')
    except OSError:
        gdaltest.unzip('tmp/cache/SCHEMAS_OPENGIS_NET', 'tmp/cache/SCHEMAS_OPENGIS_NET.zip')

    ds = ogr.Open(filename)

    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', './tmp/cache/SCHEMAS_OPENGIS_NET')
    lyr = ds.ExecuteSQL('SELECT ValidateSchema()')
    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', None)

    feat = lyr.GetNextFeature()
    val = feat.GetFieldAsInteger(0)
    feat = None

    ds.ReleaseResultSet(lyr)

    assert val != 0

###############################################################################
# Test validating against .xsd


def test_ogr_gml_42():

    validate('data/gml/expected_gml_gml32.gml')

###############################################################################
# Test automated downloading of WFS schema


def test_ogr_gml_43():

    # The service times out
    pytest.skip()

    # pylint: disable=unreachable
    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/wfs_typefeature.gml')
    assert ds is not None
    ds = None

    try:
        os.stat('data/gml/wfs_typefeature.gfs')
        gfs_found = True
    except OSError:
        gfs_found = False

    if gfs_found:
        if gdaltest.gdalurlopen('http://testing.deegree.org:80/deegree-wfs/services?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=app:Springs&NAMESPACE=xmlns(app=http://www.deegree.org/app)') is None:
            can_download_schema = False
        else:
            can_download_schema = gdal.GetDriverByName('HTTP') is not None

        assert not can_download_schema, '.gfs found, but schema could be downloaded'


###############################################################################
# Test providing a custom XSD filename


def test_ogr_gml_44():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    xsd_content = """<?xml version="1.0" encoding="UTF-8"?>
<xs:schema targetNamespace="http://ogr.maptools.org/" xmlns:ogr="http://ogr.maptools.org/" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:gml="http://www.opengis.net/gml" elementFormDefault="qualified" version="1.0">
<xs:import namespace="http://www.opengis.net/gml" schemaLocation="http://schemas.opengeospatial.net/gml/2.1.2/feature.xsd"/><xs:element name="FeatureCollection" type="ogr:FeatureCollectionType" substitutionGroup="gml:_FeatureCollection"/>
<xs:complexType name="FeatureCollectionType">
  <xs:complexContent>
    <xs:extension base="gml:AbstractFeatureCollectionType">
      <xs:attribute name="lockId" type="xs:string" use="optional"/>
      <xs:attribute name="scope" type="xs:string" use="optional"/>
    </xs:extension>
  </xs:complexContent>
</xs:complexType>
<xs:element name="test_point" type="ogr:test_point_Type" substitutionGroup="gml:_Feature"/>
<xs:complexType name="test_point_Type">
  <xs:complexContent>
    <xs:extension base="gml:AbstractFeatureType">
      <xs:sequence>
<xs:element name="geometryProperty" type="gml:GeometryPropertyType" nillable="true" minOccurs="1" maxOccurs="1"/>
    <xs:element name="dbl" nillable="true" minOccurs="0" maxOccurs="1">
      <xs:simpleType>
        <xs:restriction base="xs:decimal">
          <xs:totalDigits value="32"/>
          <xs:fractionDigits value="3"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:element>
      </xs:sequence>
    </xs:extension>
  </xs:complexContent>
</xs:complexType>
</xs:schema>"""

    gdal.FileFromMemBuffer('/vsimem/ogr_gml_44.xsd', xsd_content)

    ds = ogr.Open('data/gml/test_point.gml,xsd=/vsimem/ogr_gml_44.xsd')
    lyr = ds.GetLayer(0)

    # fid and dbl
    assert lyr.GetLayerDefn().GetFieldCount() == 2

    ds = None

    gdal.Unlink('/vsimem/ogr_gml_44.xsd')

###############################################################################
# Test PREFIX and TARGET_NAMESPACE creation options


def test_ogr_gml_45():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('/vsimem/ogr_gml_45.gml', options=['PREFIX=foo', 'TARGET_NAMESPACE=http://bar/'])
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('dbl', ogr.OFTReal))

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('str', 'str')
    dst_feat.SetField('int', 1)
    dst_feat.SetField('dbl', 2.34)

    lyr.CreateFeature(dst_feat)

    dst_feat = None
    ds = None

    try:
        validate('/vsimem/ogr_gml_45.gml')
    finally:
        gdal.Unlink('/vsimem/ogr_gml_45.gml')
        gdal.Unlink('/vsimem/ogr_gml_45.xsd')


###############################################################################
# Validate different kinds of GML files

def test_ogr_gml_46():

    if not gdaltest.have_gml_validation:
        pytest.skip()

    wkt_list = ['',
                'POINT (0 1)',
                # 'POINT (0 1 2)',
                'LINESTRING (0 1,2 3)',
                # 'LINESTRING (0 1 2,3 4 5)',
                'POLYGON ((0 0,0 1,1 1,1 0,0 0))',
                # 'POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))',
                'MULTIPOINT (0 1)',
                # 'MULTIPOINT (0 1 2)',
                'MULTILINESTRING ((0 1,2 3))',
                # 'MULTILINESTRING ((0 1 2,3 4 5))',
                'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))',
                # 'MULTIPOLYGON (((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10)))',
                'GEOMETRYCOLLECTION (POINT (0 1))',
                # 'GEOMETRYCOLLECTION (POINT (0 1 2))'
                ]

    format_list = ['GML2', 'GML3', 'GML3Deegree', 'GML3.2']

    for wkt in wkt_list:
        for frmt in format_list:
            drv = ogr.GetDriverByName('GML')
            ds = drv.CreateDataSource('/vsimem/ogr_gml_46.gml', options=['FORMAT=%s' % frmt])
            if wkt != '':
                geom = ogr.CreateGeometryFromWkt(wkt)
                geom_type = geom.GetGeometryType()
                srs = osr.SpatialReference()
                srs.ImportFromEPSG(4326)
            else:
                geom = None
                geom_type = ogr.wkbNone
                srs = None

            lyr = ds.CreateLayer('test', geom_type=geom_type, srs=srs)

            lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
            lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
            lyr.CreateField(ogr.FieldDefn('dbl', ogr.OFTReal))

            dst_feat = ogr.Feature(lyr.GetLayerDefn())
            dst_feat.SetField('str', 'str')
            dst_feat.SetField('int', 1)
            dst_feat.SetField('dbl', 2.34)
            dst_feat.SetGeometry(geom)

            lyr.CreateFeature(dst_feat)

            dst_feat = None
            ds = None

            # Validate document
            try:
                validate('/vsimem/ogr_gml_46.gml')
            except:
                print('validation failed for format=%s, wkt=%s' % (frmt, wkt))

                f = gdal.VSIFOpenL('/vsimem/ogr_gml_46.gml', 'rb')
                content = gdal.VSIFReadL(1, 10000, f)
                gdal.VSIFCloseL(f)
                print(content)

                f = gdal.VSIFOpenL('/vsimem/ogr_gml_46.xsd', 'rb')
                content = gdal.VSIFReadL(1, 10000, f)
                gdal.VSIFCloseL(f)
                print(content)
            finally:
                gdal.Unlink('/vsimem/ogr_gml_46.gml')
                gdal.Unlink('/vsimem/ogr_gml_46.xsd')

        # Only minor schema changes
        if frmt == 'GML3Deegree':
            break


###############################################################################
# Test validation of WFS GML documents

@pytest.mark.parametrize('filename', ['data/gml/wfs10.xml', 'data/gml/wfs11.xml', 'data/gml/wfs20.xml'])
def test_ogr_gml_validate_wfs(filename):
    validate(filename)

###############################################################################
# Test that we can parse some particular .xsd files that have the geometry
# field declared as :
#    <xsd:element name="geometry" minOccurs="0" maxOccurs="1">
#    <xsd:complexType>
#        <xsd:sequence>
#        <xsd:element ref="gml:_Geometry"/>
#        </xsd:sequence>
#    </xsd:complexType>
#    </xsd:element>


def test_ogr_gml_48():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/schema_with_geom_in_complextype.gfs')

    ds = ogr.Open('data/gml/schema_with_geom_in_complextype.xml')
    lyr = ds.GetLayer(0)

    assert lyr.GetGeomType() == ogr.wkbUnknown

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString

    ds = None

###############################################################################
# Test a pseudo Inspire GML file


def test_ogr_gml_49():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    xsd_content = """<ogr:FeatureCollection xmlns:gml="http://www.opengis.net/gml" xmlns:ogr="http://ogr.maptools.org/">
  <gml:featureMember>
    <ogr:test>
      <ogr:geometry><gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:coordinates>2,49 2,50 3,50 3,49 2,49</gml:coordinates></gml:LinearRing></gml:outerBoundaryIs></gml:Polygon></ogr:geometry>
      <ogr:otherGeometry><gml:Point><gml:pos>-2 -49</gml:pos></gml:Point></ogr:otherGeometry>
    </ogr:test>
  </gml:featureMember>
</ogr:FeatureCollection>
"""

    gdal.FileFromMemBuffer('/vsimem/ogr_gml_49.gml', xsd_content)

    ds = ogr.Open('/vsimem/ogr_gml_49.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon
    ds = None

    # Now with .gfs file present (#6247)
    ds = ogr.Open('/vsimem/ogr_gml_49.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_49.gml')
    gdal.Unlink('/vsimem/ogr_gml_49.gfs')

###############################################################################
# Test support for StringList, IntegerList, RealList


def test_ogr_gml_50():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('/vsimem/ogr_gml_50.gml')
    lyr = ds.CreateLayer('listlayer')
    field_defn = ogr.FieldDefn('stringlist', ogr.OFTStringList)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('intlist', ogr.OFTIntegerList)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('reallist', ogr.OFTRealList)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetFieldStringList(0, ['a', 'b'])
    feat.SetFieldIntegerList(1, [2, 3])
    feat.SetFieldDoubleList(2, [4.56, 5.67])
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gml_50.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsStringList(lyr.GetLayerDefn().GetFieldIndex('stringlist')) != ['a', 'b']:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetFieldAsIntegerList(lyr.GetLayerDefn().GetFieldIndex('intlist')) != [2, 3]:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetFieldAsDoubleList(lyr.GetLayerDefn().GetFieldIndex('reallist')) != [4.56, 5.67]:
        feat.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_50.gml')
    gdal.Unlink('/vsimem/ogr_gml_50.xsd')

###############################################################################
# Test -dsco WRITE_FEATURE_BOUNDED_BY=no -dsco STRIP_PREFIX=YES


def test_ogr_gml_51():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    for frmt in ['GML2', 'GML3']:

        gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/ogr_gml_51.gml data/poly.shp -dsco FORMAT=%s -dsco WRITE_FEATURE_BOUNDED_BY=no -dsco STRIP_PREFIX=YES' % frmt)

        f = open('tmp/ogr_gml_51.gml', 'rt')
        content = f.read()
        f.close()
        assert content.find("<FeatureCollection") != -1
        if frmt == 'GML3':
            assert content.find("<featureMember>") != -1
        assert content.find("""<poly""") != -1
        assert content.find("""<AREA>215229.266</AREA>""") != -1

        assert content.find("""<gml:boundedBy><gml:Envelope><gml:lowerCorner>479647""") == -1

        ds = ogr.Open('tmp/ogr_gml_51.gml')
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        assert feat is not None
        ds = None


###############################################################################
# Test reading MTKGML files


def test_ogr_gml_52():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    try:
        os.remove('data/gml/fake_mtkgml.gfs')
    except OSError:
        pass

    for _ in range(2):

        ds = ogr.Open('data/gml/fake_mtkgml.xml')

        lyr = ds.GetLayerByName('A')
        assert lyr.GetGeomType() == ogr.wkbPoint25D
        srs = lyr.GetSpatialRef()
        assert srs is not None
        wkt = srs.ExportToWkt()
        assert '3067' in wkt

        assert lyr.GetExtent() == (280000,280000,7000000,7000000)

        feat = lyr.GetNextFeature()
        if feat.GetField('gid') != '1' or \
                feat.GetField('regular_attribute') != 5 or \
                feat.GetField('foo_href') != 'some_ref' or \
                feat.GetField('teksti') != 'En francais !' or \
                feat.GetField('teksti_kieli') != 'fr' or \
                ogrtest.check_feature_geometry(feat, 'POINT (280000 7000000 0)') != 0:
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('B')
        assert lyr.GetGeomType() == ogr.wkbPolygon25D
        srs = lyr.GetSpatialRef()
        assert srs is not None
        feat = lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat, 'POLYGON ((280000 7000000 0,281000 7000000 0,281000 7001000 0,280000 7001000 0,280000 7000000 0))') != 0:
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('C')
        assert lyr.GetGeomType() == ogr.wkbLineString25D
        feat = lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat, 'LINESTRING (280000 7000000 0,281000 7000000 0,281000 7001000 0,280000 7001000 0,280000 7000000 0)') != 0:
            feat.DumpReadable()
            pytest.fail()

        ds = None

    os.remove('data/gml/fake_mtkgml.gfs')

###############################################################################
# Test that we don't recognize .xsd files themselves


def test_ogr_gml_53():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/archsites.xsd')
    assert ds is None
    ds = None

###############################################################################
# Test that we can open an empty GML datasource (#249, #5205)


def test_ogr_gml_54():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/empty.gfs')

    ds = ogr.Open('data/gml/empty.gml')
    assert ds is not None
    ds = None

    # with .gfs now
    ds = ogr.Open('data/gml/empty.gml')
    assert ds is not None
    ds = None

    gdal.Unlink('data/gml/empty.gfs')

###############################################################################
# Test support for <xs:include> in schemas
# Necessary for Finnish NLS data


def test_ogr_gml_55():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/ogr_gml_55.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    ds = None

    with pytest.raises(OSError):
        os.unlink('data/gml/ogr_gml_55.gfs')



###############################################################################
# Test support for gml:FeaturePropertyType and multiple geometry field
# Necessary for Finnish NLS data


def test_ogr_gml_56():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/ogr_gml_56.gfs')

    gdal.SetConfigOption('GML_REGISTRY', 'data/gml/ogr_gml_56_registry.xml')
    ds = ogr.Open('data/gml/ogr_gml_56.gml')
    gdal.SetConfigOption('GML_REGISTRY', None)
    lyr = ds.GetLayerByName('mainFeature')
    assert lyr.GetSpatialRef() is not None
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString(feat.GetFieldIndex('subFeatureProperty_href')) == '#subFeature.0'
    assert feat.GetFieldAsStringList(feat.GetFieldIndex('subFeatureRepeatedProperty_href')) == ['#subFeatureRepeated.0', '#subFeatureRepeated.1']
    assert feat.GetGeomFieldRef(0).ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,1 0,0 0))'
    assert feat.GetGeomFieldRef(1).ExportToWkt() == 'POINT (10 10)'

    lyr = ds.GetLayerByName('subFeature')
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 0
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsStringList(feat.GetFieldIndex('subFeatureRepeatedProperty_href')) == ['#subFeatureRepeated.2']
    assert feat.GetField('foo') == 'bar'

    lyr = ds.GetLayerByName('subFeatureRepeated')
    feat = lyr.GetNextFeature()
    assert feat.GetField('gml_id') == 'subFeatureRepeated.2'
    assert feat.GetField('bar') == 'baz'
    feat = lyr.GetNextFeature()
    assert feat.GetField('gml_id') == 'subFeatureRepeated.0'
    feat = lyr.GetNextFeature()
    assert feat.GetField('gml_id') == 'subFeatureRepeated.1'
    ds = None

    with pytest.raises(OSError):
        os.unlink('data/gml/ogr_gml_56.gfs')



###############################################################################
# Test write support for multiple geometry field


def test_ogr_gml_57():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    for i in range(4):
        options = []
        if i == 3:
            options = ['FORMAT=GML3.2']
        ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_57.gml', options=options)
        assert ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) == 1
        lyr = ds.CreateLayer('myLayer', geom_type=ogr.wkbNone)
        assert lyr.TestCapability(ogr.OLCCreateGeomField) == 1
        geomfielddefn = ogr.GeomFieldDefn('first_geometry', ogr.wkbPoint)
        if i == 1 or i == 2:
            sr = osr.SpatialReference()
            sr.ImportFromEPSG(32630)
            geomfielddefn.SetSpatialRef(sr)
        lyr.CreateGeomField(geomfielddefn)
        geomfielddefn = ogr.GeomFieldDefn('second_geometry', ogr.wkbLineString)
        if i == 1:
            sr = osr.SpatialReference()
            sr.ImportFromEPSG(32630)
            geomfielddefn.SetSpatialRef(sr)
        elif i == 2:
            sr = osr.SpatialReference()
            sr.ImportFromEPSG(32631)
            geomfielddefn.SetSpatialRef(sr)
        lyr.CreateGeomField(geomfielddefn)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt('POINT (0 1)'))
        feat.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt('LINESTRING (2 3,4 5)'))
        lyr.CreateFeature(feat)
        feat = None
        ds = None

        if False:  # pylint: disable=using-constant-test
            f = gdal.VSIFOpenL('/vsimem/ogr_gml_57.gml', 'rb')
            print(gdal.VSIFReadL(1, 1000, f))
            gdal.VSIFCloseL(f)

        ds = ogr.Open('/vsimem/ogr_gml_57.gml')
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        assert not (i == 1 and feat.GetGeomFieldRef(0).GetSpatialReference().ExportToWkt().find('32630') < 0)
        assert not (i == 1 and feat.GetGeomFieldRef(1).GetSpatialReference().ExportToWkt().find('32630') < 0)
        assert not (i == 2 and feat.GetGeomFieldRef(1).GetSpatialReference().ExportToWkt().find('32631') < 0)
        assert feat.GetGeomFieldRef(0).ExportToWkt() == 'POINT (0 1)'
        assert feat.GetGeomFieldRef(1).ExportToWkt() == 'LINESTRING (2 3,4 5)'
        ds = None

        gdal.Unlink('/vsimem/ogr_gml_57.gml')
        gdal.Unlink('/vsimem/ogr_gml_57.xsd')


###############################################################################
# Test support for Inspire Cadastral schemas


def test_ogr_gml_58():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/inspire_cadastralparcel.gfs')

    ds = ogr.Open('data/gml/inspire_cadastralparcel.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetGeomFieldCount() == 2
    assert lyr_defn.GetGeomFieldDefn(0).GetName() == 'geometry'
    assert lyr_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbMultiPolygon
    assert lyr_defn.GetGeomFieldDefn(1).GetName() == 'referencePoint'
    assert lyr_defn.GetGeomFieldDefn(1).GetType() == ogr.wkbPoint

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'CadastralParcel-01'),
                ('areaValue', 10.0),
                ('areaValue_uom', 'm2'),
                ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                ('endLifespanVersion', '2001-01-01T00:00:00.0Z'),
                ('inspireId_localId', 'CadastralParcel-01-localId'),
                ('inspireId_namespace', 'namespace'),
                ('label', 'label'),
                ('nationalCadastralReference', 'nationalCadastralReference'),
                ('validFrom', '2002-01-01T00:00:00.0Z'),
                ('validTo', '2003-01-01T00:00:00.0Z'),
                ('basicPropertyUnit_href', ['#BPU.1', '#BPU.2']),
                ('administrativeUnit_href', '#AU.1'),
                ('zoning_href', '#CZ.1')]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    assert feat.GetGeomFieldRef(0).ExportToWkt() == 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))'
    assert feat.GetGeomFieldRef(1).ExportToWkt() == 'POINT (2.5 49.5)'

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'CadastralParcel-02'),
                ('areaValue', None),
                ('areaValue_uom', None),
                ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                ('endLifespanVersion', None),
                ('inspireId_localId', 'CadastralParcel-02-localId'),
                ('inspireId_namespace', 'namespace'),
                ('label', 'label'),
                ('nationalCadastralReference', 'nationalCadastralReference'),
                ('validFrom', None),
                ('validTo', None),
                ('basicPropertyUnit_href', None),
                ('administrativeUnit_href', None),
                ('zoning_href', None)]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    assert feat.GetGeomFieldRef(0).ExportToWkt() == 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))'
    assert feat.GetGeomFieldRef(1) is None
    feat = None
    lyr = None
    ds = None

    ds = ogr.Open('data/gml/inspire_basicpropertyunit.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetGeomFieldCount() == 0

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'BasicPropertyUnit-01'),
                ('inspireId_localId', 'BasicPropertyUnit-01-localId'),
                ('inspireId_namespace', 'namespace'),
                ('nationalCadastralReference', 'nationalCadastralReference'),
                ('areaValue', 10.0),
                ('areaValue_uom', 'm2'),
                ('validFrom', '2000-01-01T00:00:00.0Z'),
                ('validTo', '2001-01-01T00:00:00.0Z'),
                ('beginLifespanVersion', '2002-01-01T00:00:00.0Z'),
                ('endLifespanVersion', '2003-01-01T00:00:00.0Z'),
                ('administrativeUnit_href', '#AU.1')]
    for (key, val) in expected:
        assert feat.GetField(key) == val

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'BasicPropertyUnit-02'),
                ('inspireId_localId', 'BasicPropertyUnit-02-localId'),
                ('inspireId_namespace', 'namespace'),
                ('nationalCadastralReference', 'nationalCadastralReference'),
                ('areaValue', None),
                ('areaValue_uom', None),
                ('validFrom', '2000-01-01T00:00:00.0Z'),
                ('validTo', None),
                ('beginLifespanVersion', '2002-01-01T00:00:00.0Z'),
                ('endLifespanVersion', None),
                ('administrativeUnit_href', None)]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    feat = None
    lyr = None
    ds = None

    ds = ogr.Open('data/gml/inspire_cadastralboundary.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetGeomFieldCount() == 1
    assert lyr_defn.GetGeomFieldDefn(0).GetName() == 'geometry'
    assert lyr_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbLineString

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'CadastralBoundary-01'),
                ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                ('endLifespanVersion', '2001-01-01T00:00:00.0Z'),
                ('estimatedAccuracy', 1.0),
                ('estimatedAccuracy_uom', 'm'),
                ('inspireId_localId', 'CadastralBoundary-01-localId'),
                ('inspireId_namespace', 'namespace'),
                ('validFrom', '2002-01-01T00:00:00.0Z'),
                ('validTo', '2003-01-01T00:00:00.0Z'),
                ('parcel_href', ['#Parcel.1', '#Parcel.2'])]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    assert feat.GetGeomFieldRef(0).ExportToWkt() == 'LINESTRING (2 49,3 50)'

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'CadastralBoundary-02'),
                ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                ('endLifespanVersion', None),
                ('estimatedAccuracy', None),
                ('estimatedAccuracy_uom', None),
                ('inspireId_localId', 'CadastralBoundary-02-localId'),
                ('inspireId_namespace', 'namespace'),
                ('validFrom', None),
                ('validTo', None),
                ('parcel_href', None)]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    assert feat.GetGeomFieldRef(0).ExportToWkt() == 'LINESTRING (2 49,3 50)'
    feat = None
    lyr = None
    ds = None

    ds = ogr.Open('data/gml/inspire_cadastralzoning.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetGeomFieldCount() == 2
    assert lyr_defn.GetGeomFieldDefn(0).GetName() == 'geometry'
    assert lyr_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbMultiPolygon
    assert lyr_defn.GetGeomFieldDefn(1).GetName() == 'referencePoint'
    assert lyr_defn.GetGeomFieldDefn(1).GetType() == ogr.wkbPoint

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'CadastralZoning-01'),
                ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                ('endLifespanVersion', '2001-01-01T00:00:00.0Z'),
                ('estimatedAccuracy', 1.0),
                ('estimatedAccuracy_uom', 'm'),
                ('inspireId_localId', 'CadastralZoning-01-localId'),
                ('inspireId_namespace', 'namespace'),
                ('label', 'label'),
                ('level', '3'),
                ('levelName', ['English', 'Francais', 'Deutsch']),
                ('levelName_locale', ['en', 'fr', 'de']),
                ('name_language', ['language']),
                ('name_nativeness', ['nativeness']),
                ('name_nameStatus', ['nameStatus']),
                ('name_pronunciation', None),
                ('name_spelling_text', ['text']),
                ('name_spelling_script', ['script']),
                ('nationalCadastalZoningReference', 'nationalCadastalZoningReference'),
                ('validFrom', '2002-01-01T00:00:00.0Z'),
                ('validTo', '2003-01-01T00:00:00.0Z'),
                ('upperLevelUnit_href', '#ulu.1')]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    assert feat.GetGeomFieldRef(0).ExportToWkt() == 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))'
    assert feat.GetGeomFieldRef(1).ExportToWkt() == 'POINT (2.5 49.5)'

    feat = lyr.GetNextFeature()
    expected = [('gml_id', 'CadastralZoning-02'),
                ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                ('endLifespanVersion', None),
                ('estimatedAccuracy', None),
                ('estimatedAccuracy_uom', None),
                ('inspireId_localId', None),
                ('inspireId_namespace', None),
                ('label', 'label'),
                ('level', '3'),
                ('levelName', ['English']),
                ('levelName_locale', ['en']),
                ('name_language', None),
                ('name_nativeness', None),
                ('name_nameStatus', None),
                ('name_pronunciation', None),
                ('name_spelling_text', None),
                ('name_spelling_script', None),
                ('nationalCadastalZoningReference', 'nationalCadastalZoningReference'),
                ('validFrom', None),
                ('validTo', None),
                ('upperLevelUnit_href', None)]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    assert feat.GetGeomFieldRef(0).ExportToWkt() == 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))'
    assert feat.GetGeomFieldRef(1) is None
    feat = None
    lyr = None
    ds = None

###############################################################################
# Test GFS conditions


def test_ogr_gml_59():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # Make sure the .gfs file is more recent that the .gml one
    try:
        gml_mtime = os.stat('data/gml/testcondition.gml').st_mtime
        gfs_mtime = os.stat('data/gml/testcondition.gfs').st_mtime
        touch_gfs = gfs_mtime <= gml_mtime
    except:
        touch_gfs = True
    if touch_gfs:
        print('Touching .gfs file')
        f = open('data/gml/testcondition.gfs', 'rb+')
        data = f.read(1)
        f.seek(0, 0)
        f.write(data)
        f.close()

    ds = ogr.Open('data/gml/testcondition.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    expected = [('name_en', 'English name'),
                ('name_fr', 'Nom francais'),
                ('name_others_lang', ['de']),
                ('name_others', ['Deutsche name'])]
    for (key, val) in expected:
        assert feat.GetField(key) == val
    feat = None
    lyr = None
    ds = None

########################################################
# Test reading WFS 2.0 GetFeature documents with wfs:FeatureCollection
# as a wfs:member of the top wfs:FeatureCollection


def test_ogr_gml_60():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # Make sure the .gfs file is more recent that the .gml one
    gdal.Unlink('data/gml/wfs_200_multiplelayers.gfs')

    for _ in range(2):
        ds = ogr.Open('data/gml/wfs_200_multiplelayers.gml')
        lyr = ds.GetLayerByName('road')
        assert lyr.GetFeatureCount() == 1
        feat = lyr.GetNextFeature()
        assert feat.GetField('gml_id') == 'road.21'
        lyr = ds.GetLayerByName('popplace')
        assert lyr.GetFeatureCount() == 1
        feat = lyr.GetNextFeature()
        assert feat.GetField('gml_id') == 'popplace.BACMK'
        ds = None

    gdal.Unlink('data/gml/wfs_200_multiplelayers.gfs')

###############################################################################
# Test reading a element specified with a full path in <ElementPath>


def test_ogr_gml_61():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # Make sure the .gfs file is more recent that the .gml one
    try:
        gml_mtime = os.stat('data/gml/gmlsubfeature.gml').st_mtime
        gfs_mtime = os.stat('data/gml/gmlsubfeature.gfs').st_mtime
        touch_gfs = gfs_mtime <= gml_mtime
    except:
        touch_gfs = True
    if touch_gfs:
        print('Touching .gfs file')
        f = open('data/gml/gmlsubfeature.gfs', 'rb+')
        data = f.read(1)
        f.seek(0, 0)
        f.write(data)
        f.close()

    ds = ogr.Open('data/gml/gmlsubfeature.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2, 'did not get expected geometry column name'

    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'Object.1' or feat.GetField('foo') != 'bar':
        feat.DumpReadable()
        pytest.fail()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POLYGON ((2 48,2 49,3 49,3 48,2 48))':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'Object.2' or feat.GetField('foo') != 'baz':
        feat.DumpReadable()
        pytest.fail()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POLYGON ((2 -48,2 -49,3 -49,3 -48,2 -48))':
        feat.DumpReadable()
        pytest.fail()

    ds = None

###############################################################################
# Test GML_ATTRIBUTES_TO_OGR_FIELDS option


def test_ogr_gml_62():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('tmp/gmlattributes.gfs')

    shutil.copy('data/gml/gmlattributes.gml', 'tmp/gmlattributes.gml')

    # Default behaviour
    ds = ogr.Open('tmp/gmlattributes.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    ds = None

    # Test GML_ATTRIBUTES_TO_OGR_FIELDS=YES
    gdal.Unlink('tmp/gmlattributes.gfs')

    # Without and then with .gfs
    for i in range(2):
        if i == 0:
            gdal.SetConfigOption('GML_ATTRIBUTES_TO_OGR_FIELDS', 'YES')
        ds = ogr.Open('tmp/gmlattributes.gml')
        if i == 0:
            gdal.SetConfigOption('GML_ATTRIBUTES_TO_OGR_FIELDS', None)
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 4, i
        feat = lyr.GetNextFeature()
        if feat.GetField('element_attr1') != '1' or \
                feat.GetField('element2_attr1') != 'a' or \
                feat.GetField('element2') != 'foo' or \
                feat.IsFieldSet('element3_attr1'):
            feat.DumpReadable()
            pytest.fail(i)
        feat = lyr.GetNextFeature()
        if feat.IsFieldSet('element_attr1') or \
                feat.IsFieldSet('element2_attr1') or \
                feat.IsFieldSet('element2') or \
                feat.GetField('element3_attr1') != 1:
            feat.DumpReadable()
            pytest.fail(i)
        feat = lyr.GetNextFeature()
        if feat.GetField('element_attr1') != 'a' or \
                feat.IsFieldSet('element2_attr1') or \
                feat.IsFieldSet('element2') or \
                feat.IsFieldSet('element3_attr1'):
            feat.DumpReadable()
            pytest.fail(i)
        feat = None
        ds = None


###############################################################################
# Test reading RUIAN VFR files


def test_ogr_gml_63():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # test ST file type
    ds = ogr.Open('data/gml/ruian_st_v1.xml.gz')

    # check number of layers
    nlayers = ds.GetLayerCount()
    assert nlayers == 14

    # check name of first layer
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Staty'

    # check geometry column name
    assert lyr.GetGeometryColumn() == 'DefinicniBod'

    ds = None

    # test OB file type
    ds = ogr.Open('data/gml/ruian_ob_v1.xml.gz')

    # check number of layers
    nlayers = ds.GetLayerCount()
    assert nlayers == 11

    # check number of features
    nfeatures = 0
    for i in range(nlayers):
        lyr = ds.GetLayer(i)
        nfeatures += lyr.GetFeatureCount()
    assert nfeatures == 7

###############################################################################
# Test multiple instances of parsers (#5571)


def test_ogr_gml_64():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    for parser in ['XERCES', 'EXPAT']:
        for _ in range(2):
            gdal.SetConfigOption('GML_PARSER', parser)
            ds = ogr.Open('data/gml/rnf_eg.gml')
            gdal.SetConfigOption('GML_PARSER', None)
            lyr = ds.GetLayer(0)
            feat = lyr.GetNextFeature()
            assert feat is not None, parser


###############################################################################
# Test SRSDIMENSION_LOC=GEOMETRY option (#5606)


def test_ogr_gml_65():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    option_expected_list = [['SRSDIMENSION_LOC=GEOMETRY', '<ogr:geometryProperty><gml:MultiSurface srsDimension="3"><gml:surfaceMember><gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>0 1 2 3 4 5 6 7 8 0 1 2</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember></gml:MultiSurface></ogr:geometryProperty>'],
                            ['SRSDIMENSION_LOC=POSLIST', '<ogr:geometryProperty><gml:MultiSurface><gml:surfaceMember><gml:Polygon><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 1 2 3 4 5 6 7 8 0 1 2</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember></gml:MultiSurface></ogr:geometryProperty>'],
                            ['SRSDIMENSION_LOC=GEOMETRY,POSLIST', '<ogr:geometryProperty><gml:MultiSurface srsDimension="3"><gml:surfaceMember><gml:Polygon><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 1 2 3 4 5 6 7 8 0 1 2</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember></gml:MultiSurface></ogr:geometryProperty>'],
                            ]
    for (option, expected) in option_expected_list:
        filename = '/vsimem/ogr_gml_65.gml'
        # filename = 'ogr_gml_65.gml'
        ds = ogr.GetDriverByName('GML').CreateDataSource(filename, options=['FORMAT=GML3', option])
        lyr = ds.CreateLayer('lyr')
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOLYGON (((0 1 2,3 4 5,6 7 8,0 1 2)))"))
        lyr.CreateFeature(feat)
        ds = None

        f = gdal.VSIFOpenL(filename, 'rb')
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)

        assert expected in data

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != "MULTIPOLYGON (((0 1 2,3 4 5,6 7 8,0 1 2)))":
            feat.DumpReadable()
            pytest.fail()
        ds = None

        gdal.Unlink(filename)
        gdal.Unlink(filename[0:-3] + "xsd")


###############################################################################
# Test curve geometries


def test_ogr_gml_66():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    filename = '/vsimem/ogr_gml_66.gml'
    # filename = 'ogr_gml_66.gml'
    ds = ogr.GetDriverByName('GML').CreateDataSource(filename, options=['FORMAT=GML3'])
    lyr = ds.CreateLayer('compoundcurve', geom_type=ogr.wkbCompoundCurve)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,1 1,2 0)'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1,2 0)'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('curvepolygon', geom_type=ogr.wkbCurvePolygon)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CURVEPOLYGON ( CIRCULARSTRING(0 0,1 0,0 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('multisurface', geom_type=ogr.wkbMultiSurface)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTISURFACE (CURVEPOLYGON ( CIRCULARSTRING(0 0,1 0,0 0)))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('multicurve', geom_type=ogr.wkbMultiCurve)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTICURVE ( CIRCULARSTRING(0 0,1 0,0 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTICURVE ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('polygon', geom_type=ogr.wkbPolygon)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('linestring', geom_type=ogr.wkbLineString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (0 0,0 1,1 1,0 0)'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('multipolygon', geom_type=ogr.wkbMultiPolygon)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('multilinestring', geom_type=ogr.wkbMultiLineString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('compoundcurve_untyped')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1,2 0)'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1,2 0)'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('curvepolygon_untyped')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CURVEPOLYGON ( CIRCULARSTRING(0 0,1 0,0 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('multisurface_untyped')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTISURFACE (CURVEPOLYGON ( CIRCULARSTRING(0 0,1 0,0 0)))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'))
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('multicurve_untyped')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTICURVE (CIRCULARSTRING (0 0,1 1,2 0))'))
    lyr.CreateFeature(f)
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None

    ds = None

    # Test first with .xsd and then without
    for i in range(3):

        ds = ogr.Open(filename)

        lyr = ds.GetLayerByName('compoundcurve')
        assert lyr.GetGeomType() == ogr.wkbCompoundCurve
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0))':
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0))':
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'COMPOUNDCURVE ((0 0,1 1,2 0))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('curvepolygon')
        assert lyr.GetGeomType() == ogr.wkbCurvePolygon
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'CURVEPOLYGON (CIRCULARSTRING (0 0,0.5 0.5,1 0,0.5 -0.5,0 0))':
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('multisurface')
        assert lyr.GetGeomType() == ogr.wkbMultiSurface
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,0.5 0.5,1 0,0.5 -0.5,0 0)))':
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTISURFACE (((0 0,0 1,1 1,0 0)))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('multicurve')
        assert lyr.GetGeomType() == ogr.wkbMultiCurve
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTICURVE (CIRCULARSTRING (0 0,0.5 0.5,1 0,0.5 -0.5,0 0))':
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTICURVE ((0 0,0 1,1 1,0 0))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('polygon')
        assert lyr.GetGeomType() == ogr.wkbPolygon
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('linestring')
        assert lyr.GetGeomType() == ogr.wkbLineString
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (0 0,0 1,1 1,0 0)':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('multipolygon')
        assert lyr.GetGeomType() == ogr.wkbMultiPolygon
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('multilinestring')
        assert lyr.GetGeomType() == ogr.wkbMultiLineString
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTILINESTRING ((0 0,0 1,1 1,0 0))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('compoundcurve_untyped')
        if i != 0:
            assert lyr.GetGeomType() == ogr.wkbCompoundCurve
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'COMPOUNDCURVE ((0 0,1 1,2 0))':
                feat.DumpReadable()
                pytest.fail()
        else:
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (0 0,1 1,2 0)':
                feat.DumpReadable()
                pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('curvepolygon_untyped')
        if i != 0:
            assert lyr.GetGeomType() == ogr.wkbCurvePolygon
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'CURVEPOLYGON ((0 0,0 1,1 1,0 0))':
                feat.DumpReadable()
                pytest.fail()
        else:
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
                feat.DumpReadable()
                pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'CURVEPOLYGON (CIRCULARSTRING (0 0,0.5 0.5,1 0,0.5 -0.5,0 0))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('multisurface_untyped')
        if i != 0:
            assert lyr.GetGeomType() == ogr.wkbMultiSurface
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'MULTISURFACE (((0 0,0 1,1 1,0 0)))':
                feat.DumpReadable()
                pytest.fail()
        else:
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))':
                feat.DumpReadable()
                pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,0.5 0.5,1 0,0.5 -0.5,0 0)))':
            feat.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('multicurve_untyped')
        if i != 0:
            assert lyr.GetGeomType() == ogr.wkbMultiCurve
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'MULTICURVE ((0 0,0 1,1 1,0 0))':
                feat.DumpReadable()
                pytest.fail()
        else:
            feat = lyr.GetNextFeature()
            if feat.GetGeometryRef().ExportToWkt() != 'MULTILINESTRING ((0 0,0 1,1 1,0 0))':
                feat.DumpReadable()
                pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'MULTICURVE (CIRCULARSTRING (0 0,1 1,2 0))':
            feat.DumpReadable()
            pytest.fail()

        ds = None

        gdal.Unlink(filename[0:-3] + "xsd")

    gdal.Unlink(filename)
    gdal.Unlink(filename[0:-3] + "gfs")

###############################################################################
# Test boolean, int16, integer64 type


def test_ogr_gml_67():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    filename = '/vsimem/ogr_gml_67.gml'
    ds = ogr.GetDriverByName('GML').CreateDataSource(filename)
    lyr = ds.CreateLayer('test')
    fld_defn = ogr.FieldDefn('b1', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('b2', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('bool_list', ogr.OFTIntegerList)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('short', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('float', ogr.OFTReal)
    fld_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('int64', ogr.OFTInteger64)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('int64list', ogr.OFTInteger64List)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    f.SetField(1, 0)
    f.SetFieldIntegerList(2, [1, 0])
    f.SetField(3, -32768)
    f.SetField(4, 1.23)
    f.SetField(5, 1)
    f.SetFieldInteger64List(6, [1])
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1234567890123)
    f.SetField(5, 1234567890123)
    f.SetFieldInteger64List(6, [1, 1234567890123])
    lyr.CreateFeature(f)
    f = None

    ds = None

    # Test first with .xsd and then without
    for i in range(3):

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert (lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('b1')).GetType() == ogr.OFTInteger and \
           lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('b1')).GetSubType() == ogr.OFSTBoolean), \
            i
        assert (lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('bool_list')).GetType() == ogr.OFTIntegerList and \
           lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('bool_list')).GetSubType() == ogr.OFSTBoolean), \
            i
        if i == 0:
            assert (lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('short')).GetType() == ogr.OFTInteger and \
                    lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('short')).GetSubType() == ogr.OFSTInt16), \
                i
        if i == 0:
            assert (lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float')).GetType() == ogr.OFTReal and \
                    lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float')).GetSubType() == ogr.OFSTFloat32), \
                i
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('int64')).GetType() == ogr.OFTInteger64, \
            i
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('int64list')).GetType() == ogr.OFTInteger64List, \
            i
        f = lyr.GetNextFeature()
        if f.GetField('b1') != 1 or f.GetField('b2') != 0 or f.GetFieldAsString('bool_list') != '(2:1,0)' or f.GetField('short') != -32768 or f.GetField('float') != 1.23:
            f.DumpReadable()
            pytest.fail(i)
        f = lyr.GetNextFeature()
        if f.GetFID() != 1234567890123 or f.GetField('int64') != 1234567890123 or f.GetField('int64list') != [1, 1234567890123]:
            f.DumpReadable()
            pytest.fail(i)
        ds = None

        gdal.Unlink(filename[0:-3] + "xsd")

    gdal.Unlink(filename)
    gdal.Unlink(filename[0:-3] + "gfs")

###############################################################################
# Test reading GML with xsd with a choice of geometry properites


def test_ogr_gml_68():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/choicepolygonmultipolygon.gml')

    expected_results = ['MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))',
                        'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)),((10 0,10 1,11 1,11 0,10 0)))']

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon, \
        ' did not get expected layer geometry type'
    for i in range(2):
        feat = lyr.GetNextFeature()
        geom = feat.GetGeometryRef()
        got_wkt = geom.ExportToWkt()
        assert got_wkt == expected_results[i], 'did not get expected geometry'

    ds = None

###############################################################################
# Test not nullable fields


def test_ogr_gml_69():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_69.gml')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn = ogr.GeomFieldDefn('geomfield_not_nullable', ogr.wkbPoint)
    field_defn.SetNullable(0)
    lyr.CreateGeomField(field_defn)
    field_defn = ogr.GeomFieldDefn('geomfield_nullable', ogr.wkbPoint)
    lyr.CreateGeomField(field_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeomFieldDirectly('geomfield_not_nullable', ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None

    # Error case: missing geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0
    f = None

    # Error case: missing non-nullable field
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0
    f = None

    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_gml_69.gml', open_options=['EMPTY_AS_NULL=NO'])
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_nullable')).IsNullable() == 1
    ds = None

    gdal.Unlink("/vsimem/ogr_gml_69.gml")
    gdal.Unlink("/vsimem/ogr_gml_69.xsd")

###############################################################################
# Test default fields (not really supported, but we must do something as we
# support not nullable fields)


def test_ogr_gml_70():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_70.gml')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)

    field_defn = ogr.FieldDefn('field_string', ogr.OFTString)
    field_defn.SetDefault("'a'")
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    ds = None

    ds = ogr.Open('/vsimem/ogr_gml_70.gml')
    lyr = ds.GetLayerByName('test')
    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink("/vsimem/ogr_gml_70.gml")
    gdal.Unlink("/vsimem/ogr_gml_70.xsd")

###############################################################################
# Test reading WFS 2.0 layer resulting from a join operation


def ogr_gml_71_helper(ds):

    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'join_table1_table2'
    fields = [('table1.gml_id', ogr.OFTString),
              ('table1.foo', ogr.OFTInteger),
              ('table1.bar', ogr.OFTInteger),
              ('table2.gml_id', ogr.OFTString),
              ('table2.bar', ogr.OFTInteger),
              ('table2.baz', ogr.OFTString)]
    layer_defn = lyr.GetLayerDefn()
    assert layer_defn.GetFieldCount() == len(fields)
    for i, field in enumerate(fields):
        fld_defn = layer_defn.GetFieldDefn(i)
        assert fld_defn.GetName() == field[0], i
        assert fld_defn.GetType() == field[1], i
    assert layer_defn.GetGeomFieldCount() == 2
    assert layer_defn.GetGeomFieldDefn(0).GetName() == 'table1.geometry'
    assert layer_defn.GetGeomFieldDefn(1).GetName() == 'table2.geometry'
    f = lyr.GetNextFeature()
    if f.GetField('table1.gml_id') != 'table1-1' or \
       f.GetField('table1.foo') != 1 or \
       f.IsFieldSet('table1.bar') or \
       f.GetField('table2.gml_id') != 'table2-1' or \
       f.GetField('table2.bar') != 2 or \
       f.GetField('table2.baz') != 'foo' or \
       f.GetGeomFieldRef(0) is not None or \
       f.GetGeomFieldRef(1).ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('table1.gml_id') != 'table1-2' or \
       f.IsFieldSet('table1.foo') or \
       f.GetField('table1.bar') != 2 or \
       f.GetField('table2.gml_id') != 'table2-2' or \
       f.GetField('table2.bar') != 2 or \
       f.GetField('table2.baz') != 'bar' or \
       f.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 50)' or \
       f.GetGeomFieldRef(1).ExportToWkt() != 'POINT (2 50)':
        f.DumpReadable()
        pytest.fail()



def test_ogr_gml_71():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # With .xsd
    gdal.Unlink('data/gml/wfsjointlayer.gfs')
    ds = ogr.Open('data/gml/wfsjointlayer.gml')
    ogr_gml_71_helper(ds)
    ds = None

    with pytest.raises(OSError):
        os.unlink('data/gml/wfsjointlayer.gfs')


    # With .xsd but that is only partially understood
    ds = gdal.OpenEx('data/gml/wfsjointlayer.gml', open_options=['XSD=data/gml/wfsjointlayer_not_understood.xsd'])
    ogr_gml_71_helper(ds)
    ds = None

    try:
        os.unlink('data/gml/wfsjointlayer.gfs')
    except OSError:
        pytest.fail()

    # Without .xsd nor .gfs
    shutil.copy('data/gml/wfsjointlayer.gml', 'tmp/wfsjointlayer.gml')
    gdal.Unlink('tmp/wfsjointlayer.gfs')
    ds = ogr.Open('tmp/wfsjointlayer.gml')
    ogr_gml_71_helper(ds)
    ds = None

    try:
        os.stat('tmp/wfsjointlayer.gfs')
    except OSError:
        pytest.fail()

    # With .gfs
    ds = ogr.Open('tmp/wfsjointlayer.gml')
    ogr_gml_71_helper(ds)
    ds = None

###############################################################################
# Test name and description


def test_ogr_gml_72():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_72.gml', options=['NAME=name', 'DESCRIPTION=description'])
    ds.SetMetadata({'NAME': 'ignored', 'DESCRIPTION': 'ignored'})
    ds = None

    ds = ogr.Open('/vsimem/ogr_gml_72.gml')
    assert ds.GetMetadata() == {'NAME': 'name', 'DESCRIPTION': 'description'}
    ds = None

    gdal.Unlink("/vsimem/ogr_gml_72.gml")
    gdal.Unlink("/vsimem/ogr_gml_72.xsd")
    gdal.Unlink("/vsimem/ogr_gml_72.gfs")

    ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_72.gml')
    ds.SetMetadata({'NAME': 'name', 'DESCRIPTION': 'description'})
    ds = None

    ds = ogr.Open('/vsimem/ogr_gml_72.gml')
    assert ds.GetMetadata() == {'NAME': 'name', 'DESCRIPTION': 'description'}
    ds = None

    gdal.Unlink("/vsimem/ogr_gml_72.gml")
    gdal.Unlink("/vsimem/ogr_gml_72.xsd")
    gdal.Unlink("/vsimem/ogr_gml_72.gfs")

###############################################################################
# Read a CSW GetRecordsResponse document


def test_ogr_gml_73():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    try:
        os.remove('data/gml/cswresults.gfs')
    except OSError:
        pass

    ds = ogr.Open('data/gml/cswresults.xml')
    for i in range(3):
        lyr = ds.GetLayer(i)
        sr = lyr.GetSpatialRef()
        got_wkt = sr.ExportToWkt()
        assert '4326' in got_wkt, 'did not get expected SRS'

        feat = lyr.GetNextFeature()
        geom = feat.GetGeometryRef()
        got_wkt = geom.ExportToWkt()
        assert got_wkt == 'POLYGON ((-180 -90,-180 90,180 90,180 -90,-180 -90))', \
            'did not get expected geometry'

    ds = None

    try:
        os.remove('data/gml/cswresults.gfs')
    except OSError:
        pass


###############################################################################
# Test FORCE_SRS_DETECTION open option


def test_ogr_gml_74():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    # With .xsd
    ds = gdal.OpenEx('data/gml/expected_gml_gml32.gml', open_options=['FORCE_SRS_DETECTION=YES'])
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef() is not None, 'did not get expected SRS'
    assert lyr.GetFeatureCount() == 2, 'did not get expected feature count'

    shutil.copy('data/gml/expected_gml_gml32.gml', 'tmp/ogr_gml_74.gml')
    if os.path.exists('tmp/ogr_gml_74.gfs'):
        os.unlink('tmp/ogr_gml_74.gfs')

    # Without .xsd or .gfs
    ds = gdal.OpenEx('tmp/ogr_gml_74.gml', open_options=['FORCE_SRS_DETECTION=YES'])
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef() is not None, 'did not get expected SRS'
    assert lyr.GetFeatureCount() == 2, 'did not get expected feature count'

    # With .gfs
    ds = gdal.OpenEx('tmp/ogr_gml_74.gml', open_options=['FORCE_SRS_DETECTION=YES'])
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef() is not None, 'did not get expected SRS'
    assert lyr.GetFeatureCount() == 2, 'did not get expected feature count'
    ds = None

    os.unlink('tmp/ogr_gml_74.gml')
    os.unlink('tmp/ogr_gml_74.gfs')

###############################################################################
# Test we don't open a WMTS Capabilities doc


def test_ogr_gml_75():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.FileFromMemBuffer("/vsimem/ogr_gml_75.xml",
                           """<?xml version="1.0" encoding="UTF-8"?>
<Capabilities xmlns="http://www.opengis.net/wmts/1.0"
xmlns:ows="http://www.opengis.net/ows/1.1"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
xmlns:gml="http://www.opengis.net/gml"
xsi:schemaLocation="http://www.opengis.net/wmts/1.0 http://somewhere"
version="1.0.0">
        <ows:OperationsMetadata>
                <ows:Operation name="GetCapabilities">
                        <ows:DCP>
                                <ows:HTTP>
                                        <ows:Get xlink:href="http://foo"/>
                                </ows:HTTP>
                        </ows:DCP>
                </ows:Operation>
                <ows:Operation name="GetTile">
                        <ows:DCP>
                                <ows:HTTP>
                                        <ows:Get xlink:href="http://foo"/>
                                </ows:HTTP>
                        </ows:DCP>
                </ows:Operation>
        </ows:OperationsMetadata>
</Capabilities>""")

    ds = ogr.Open('/vsimem/ogr_gml_75.xml')
    assert ds is None
    gdal.Unlink('/vsimem/ogr_gml_75.xml')

###############################################################################
# Test we are robust to content of XML elements bigger than 2 GB


def test_ogr_gml_76():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    if not gdaltest.run_slow_tests():
        pytest.skip()

    with gdaltest.error_handler():
        ds = ogr.Open('/vsisparse/data/gml/huge_attribute_gml_sparse.xml')
        if ds is not None:
            lyr = ds.GetLayer(0)
            lyr.GetNextFeature()

        ds = ogr.Open('/vsisparse/data/gml/huge_geom_gml_sparse.xml')
        if ds is not None:
            lyr = ds.GetLayer(0)
            lyr.GetNextFeature()


###############################################################################
# Test interpretation of http://www.opengis.net/def/crs/EPSG/0/ URLs (#6678)


def test_ogr_gml_77():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.FileFromMemBuffer("/vsimem/ogr_gml_77.xml",
                           """<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml">
  <ogr:featureMember>
    <ogr:point gml:id="point.0">
      <ogr:geometryProperty><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326"><gml:pos>49 2</gml:pos></gml:Point></ogr:geometryProperty>
      <ogr:id>1</ogr:id>
    </ogr:point>
  </ogr:featureMember>
</ogr:FeatureCollection>
""")

    ds = ogr.Open('/vsimem/ogr_gml_77.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2, 1]
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (2 49)'
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_gml_77.xml', open_options=['SWAP_COORDINATES=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (2 49)'
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_gml_77.xml', open_options=['SWAP_COORDINATES=NO'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (49 2)'
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_77.xml')
    gdal.Unlink('/vsimem/ogr_gml_77.gfs')

###############################################################################
# Test effect of SWAP_COORDINATES (#6678)


def test_ogr_gml_78():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.FileFromMemBuffer("/vsimem/ogr_gml_78.xml",
                           """<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml">
  <ogr:featureMember>
    <ogr:point gml:id="point.0">
      <ogr:geometryProperty><gml:Point srsName="EPSG:4326"><gml:pos>2 49</gml:pos></gml:Point></ogr:geometryProperty>
      <ogr:id>1</ogr:id>
    </ogr:point>
  </ogr:featureMember>
</ogr:FeatureCollection>
""")

    ds = ogr.Open('/vsimem/ogr_gml_78.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2, 1]
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (2 49)'
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_gml_78.xml', open_options=['SWAP_COORDINATES=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (49 2)'
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_gml_78.xml', open_options=['SWAP_COORDINATES=NO'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (2 49)'
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_78.xml')
    gdal.Unlink('/vsimem/ogr_gml_78.gfs')

###############################################################################
# Test SRSNAME_FORMAT


def test_ogr_gml_79():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    tests = [['SHORT', 'EPSG:4326', '2 49'],
             ['OGC_URN', 'urn:ogc:def:crs:EPSG::4326', '49 2'],
             ['OGC_URL', 'http://www.opengis.net/def/crs/EPSG/0/4326', '49 2']
            ]
    for (srsname_format, expected_srsname, expected_coords) in tests:

        ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_79.xml',
                                                         options=['FORMAT=GML3', 'SRSNAME_FORMAT=' + srsname_format])
        lyr = ds.CreateLayer('firstlayer', srs=sr)
        feat = ogr.Feature(lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)
        ds = None

        f = gdal.VSIFOpenL("/vsimem/ogr_gml_79.xml", "rb")
        if f is not None:
            data = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
            gdal.VSIFCloseL(f)

        assert expected_srsname in data and expected_coords in data, \
            srsname_format

    gdal.Unlink('/vsimem/ogr_gml_79.xml')
    gdal.Unlink('/vsimem/ogr_gml_79.xsd')

###############################################################################
# Test null / unset


def test_ogr_gml_80():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_80.xml')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('int_field', ogr.OFTInteger))

    f = ogr.Feature(lyr.GetLayerDefn())
    f['int_field'] = 4
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull('int_field')
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_gml_80.xml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['int_field'] != 4:
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['int_field'] is not None:
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f.IsFieldSet('int_field'):
        f.DumpReadable()
        pytest.fail()
    f = None
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_80.xml')
    gdal.Unlink('/vsimem/ogr_gml_80.xsd')


###############################################################################
# Test building a .gfs with a field with xsi:nil="true" (#7027)

def test_ogr_gml_81():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/test_xsi_nil_gfs.gfs')
    ds = ogr.Open('data/gml/test_xsi_nil_gfs.gml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('intval') != 1:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('data/gml/test_xsi_nil_gfs.gfs')

###############################################################################
# Test GML_FEATURE_COLLECTION=YES


def test_ogr_gml_82():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.VectorTranslate('/vsimem/ogr_gml_82.gml', 'data/poly.shp',
                         format='GML',
                         datasetCreationOptions=['FORMAT=GML3',
                                                 'GML_FEATURE_COLLECTION=YES'])

    ds = ogr.Open('/vsimem/ogr_gml_82.gml')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_82.gml", "rb")
    if f is not None:
        data = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
        gdal.VSIFCloseL(f)
    assert 'gml:FeatureCollection' in data

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_82.xsd", "rb")
    if f is not None:
        data = gdal.VSIFReadL(1, 10000, f).decode('utf-8')
        gdal.VSIFCloseL(f)
    assert 'name = "FeatureCollection"' not in data
    assert 'gmlsf' not in data

    gdal.Unlink('/vsimem/ogr_gml_82.gml')
    gdal.Unlink('/vsimem/ogr_gml_82.xsd')

###############################################################################


def test_ogr_gml_gml2_write_geometry_error():

    ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_83.gml', options = ['FORMAT=GML2'])
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 0), TIN EMPTY)'))
    with gdaltest.error_handler():
        lyr.CreateFeature(f)
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_83.gml')
    gdal.Unlink('/vsimem/ogr_gml_83.xsd')

###############################################################################


def test_ogr_gml_srsname_only_on_top_bounded_by():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    tmpname = '/vsimem/test_ogr_gml_srsname_only_on_top_bounded_by.xml'
    gdal.FileFromMemBuffer(tmpname, """<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://ogr.maptools.org/ out.xsd"
     xmlns:ogr="http://ogr.maptools.org/"
     xmlns:gml="http://www.opengis.net/gml">
  <gml:boundedBy>
    <gml:Envelope srsName="EPSG:27700">
      <gml:lowerCorner>0 0</gml:lowerCorner>
      <gml:upperCorner>1 1</gml:upperCorner>
    </gml:Envelope>
  </gml:boundedBy>
  <gml:featureMember>
    <ogr:poly fid="poly.0">
      <ogr:geometryProperty><gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:coordinates>0,0 0,1 1,1 1,0 0,0</gml:coordinates></gml:LinearRing></gml:outerBoundaryIs></gml:Polygon></ogr:geometryProperty>
    </ogr:poly>
  </gml:featureMember>
</<ogr:FeatureCollection>""")

    # Open once to generate .gfs
    ds = ogr.Open(tmpname)
    lyr = ds.GetLayer(0)
    assert '27700' in lyr.GetSpatialRef().ExportToWkt()
    ds = None

    # Open another time to read .gfs
    ds = ogr.Open(tmpname)
    lyr = ds.GetLayer(0)
    assert '27700' in lyr.GetSpatialRef().ExportToWkt()
    ds = None

    gdal.Unlink(tmpname)
    gdal.Unlink(tmpname[0:-3] + "gfs")

###############################################################################
# Test understanding of XSD that uses 'FeatureType' suffix instead of 'Type'.
# If schema was understood, fields 2/3/4 will be 'Real' rather than 'Integer'.

def test_ogr_gml_featuretype_suffix_in_xsd():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/arcgis-world-wfs.gfs')

    ds = ogr.Open('data/gml/arcgis-world-wfs.gml,xsd=data/gml/arcgis-world-wfs.xsd')
    lyr = ds.GetLayer(0)

    for i in range(2, 4):
      assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTReal

    gdal.Unlink('data/gml/arcgis-world-wfs.gfs')

###############################################################################


def test_ogr_gml_standalone_geom():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    ds = ogr.Open('data/gml/standalone_geometry.gml')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POLYGON ((2 49,3 49,3 48,2 48,2 49))'


###############################################################################
# Test unique fields

@pytest.mark.parametrize('gml_format', ['GML2','GML3','GML3.2'])
@pytest.mark.parametrize('constraint_met', [True, False])
def test_ogr_gml_unique(gml_format, constraint_met):

    if not gdaltest.have_gml_reader:
        pytest.skip()

    try:
        ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/test_ogr_gml_unique.gml', options=['FORMAT='+gml_format])
        lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
        field_defn = ogr.FieldDefn('field_not_unique', ogr.OFTString)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('field_unique', ogr.OFTString)
        field_defn.SetUnique(True)
        lyr.CreateField(field_defn)
        f = ogr.Feature(lyr.GetLayerDefn())
        f['field_unique'] = 'foo'
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f['field_unique'] = 'bar' if constraint_met else 'foo'
        lyr.CreateFeature(f)
        f = None
        ds = None

        ds = gdal.OpenEx('/vsimem/test_ogr_gml_unique.gml')
        lyr = ds.GetLayerByName('test')
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_unique')).IsUnique() == 0
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_unique')).IsUnique() == 1
        ds = None

        if gdaltest.have_gml_validation:
            if constraint_met:
                validate("/vsimem/test_ogr_gml_unique.gml")
            else:
                with gdaltest.error_handler():
                    with pytest.raises(Exception):
                        validate("/vsimem/test_ogr_gml_unique.gml")

    finally:
        gdal.Unlink("/vsimem/test_ogr_gml_unique.gml")
        gdal.Unlink("/vsimem/test_ogr_gml_unique.xsd")

###############################################################################


def test_ogr_gml_write_gfs_no():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('/vsimem/test.gfs')
    gdal.Unlink('/vsimem/test.xsd')
    gdal.FileFromMemBuffer('/vsimem/test.gml',
                           open('data/gml/expected_gml_gml32.gml', 'rb').read())

    assert gdal.OpenEx('/vsimem/test.gml') is not None
    assert gdal.VSIStatL('/vsimem/test.gfs') is not None
    gdal.Unlink('/vsimem/test.gfs')

    assert gdal.OpenEx('/vsimem/test.gml', open_options = ['WRITE_GFS=NO']) is not None
    assert gdal.VSIStatL('/vsimem/test.gfs') is None

    gdal.Unlink('/vsimem/test.gml')


###############################################################################


def test_ogr_gml_write_gfs_yes():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('/vsimem/test.gfs')
    gdal.FileFromMemBuffer('/vsimem/test.gml',
                           open('data/gml/expected_gml_gml32.gml', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/test.xsd',
                           open('data/gml/expected_gml_gml32.xsd', 'rb').read())

    assert gdal.OpenEx('/vsimem/test.gml') is not None
    assert gdal.VSIStatL('/vsimem/test.gfs') is None

    assert gdal.OpenEx('/vsimem/test.gml', open_options = ['WRITE_GFS=YES']) is not None
    assert gdal.VSIStatL('/vsimem/test.gfs') is not None

    gdal.Unlink('/vsimem/test.gml')
    gdal.Unlink('/vsimem/test.gfs')
    gdal.Unlink('/vsimem/test.xsd')

###############################################################################


def test_ogr_gml_no_gfs_rewriting():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('/vsimem/test.gfs')
    gdal.Unlink('/vsimem/test.xsd')
    gdal.FileFromMemBuffer('/vsimem/test.gml',
                           open('data/gml/expected_gml_gml32.gml', 'rb').read())

    assert gdal.OpenEx('/vsimem/test.gml') is not None
    assert gdal.VSIStatL('/vsimem/test.gfs') is not None

    f = gdal.VSIFOpenL('/vsimem/test.gfs', 'rb+')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFSeekL(f, 0, 0)
    data += b'<!-- mycomment -->'
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    assert gdal.OpenEx('/vsimem/test.gml') is not None

    f = gdal.VSIFOpenL('/vsimem/test.gfs', 'rb+')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert b'<!-- mycomment -->' in data

    gdal.Unlink('/vsimem/test.gml')
    gdal.Unlink('/vsimem/test.gfs')

###############################################################################
# Read AIXM ElevatedSurface


def test_ogr_gml_aixm_elevated_surface():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/aixm_ElevatedSurface.gfs')
    ds = ogr.Open('data/gml/aixm_ElevatedSurface.xml')
    lyr = ds.GetLayer(0)

    assert lyr.GetExtent() == (2, 3, 49, 50)

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    assert got_wkt == 'POLYGON ((2 49,3 49,3 50,2 49))'

    ds = None
    gdal.Unlink('data/gml/aixm_ElevatedSurface.gfs')


###############################################################################
# Test support for XML comment srsName="" in .xsd


@pytest.mark.parametrize('gml_format', ['GML2','GML3','GML3.2'])
def test_ogr_gml_srs_name_in_xsd(gml_format):

    if not gdaltest.have_gml_reader:
        pytest.skip()

    filename = '/vsimem/test_ogr_gml_srs_name_in_xsd.gml'
    xsdfilename = filename[0:-4] + '.xsd'

    ds = ogr.GetDriverByName('GML').CreateDataSource(filename, options=['FORMAT='+gml_format])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('test', srs=srs, geom_type=ogr.wkbMultiPolygon)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((2 49,2 50,3 50,2 49)))'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL(xsdfilename, 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    if gml_format == 'GML2':
        assert b'<!-- srsName="EPSG:4326" -->' in data
    else:
        assert b'<!-- srsName="urn:ogc:def:crs:EPSG::4326" -->' in data

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == '4326'
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'MULTIPOLYGON (((2 49,2 50,3 50,2 49)))'
    f = None
    ds = None

    gdal.Unlink(filename)
    gdal.Unlink(xsdfilename)


###############################################################################


def test_ogr_gml_too_nested():

    if not gdaltest.have_gml_reader:
        pytest.skip()

    gdal.Unlink('data/gml/too_nested.gfs')

    with gdaltest.error_handler():
        ds = ogr.Open('data/gml/too_nested.gml')
        lyr = ds.GetLayer(0)
        assert lyr.GetNextFeature() is None

    gdal.Unlink('data/gml/too_nested.gfs')

    with gdaltest.config_option('OGR_GML_NESTING_LEVEL', 'UNLIMITED'):
        ds = ogr.Open('data/gml/too_nested.gml')
        lyr = ds.GetLayer(0)
        assert lyr.GetNextFeature() is not None

    gdal.Unlink('data/gml/too_nested.gfs')

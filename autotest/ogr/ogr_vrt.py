#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR VRT driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os


import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import test_cli_utilities
import pytest

###############################################################################
# Open VRT datasource.


def test_ogr_vrt_1():

    with gdaltest.error_handler():
        # Complains about dummySrcDataSource as expected.
        gdaltest.vrt_ds = ogr.Open('data/vrt/vrt_test.vrt')

    if gdaltest.vrt_ds is not None:
        return
    pytest.fail()

###############################################################################
# Verify the geometries, in the "test2" layer based on x,y,z columns.
#
# Also tests FID-copied-from-source.


def test_ogr_vrt_2():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    lyr = gdaltest.vrt_ds.GetLayerByName('test2')

    extent = lyr.GetExtent()
    assert extent == (12.5, 100.0, 17.0, 200.0), 'wrong extent'

    expect = ['First', 'Second']

    tr = ogrtest.check_features_against_list(lyr, 'other', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(12.5 17 1.2)',
                                      max_error=0.000000001) == 0)

    assert feat.GetFID() == 0, 'Unexpected fid'

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT Z (100 200 0)',
                                      max_error=0.000000001) == 0), feat.GetGeometryRef().ExportToIsoWkt()

    assert feat.GetFID() == 1, 'Unexpected fid'

###############################################################################
# Same test on layer 3 derived from WKT column.
#
# Also tests FID-from-attribute.


def test_ogr_vrt_3():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    lyr = gdaltest.vrt_ds.GetLayerByName('test3')

    expect = ['First', 'Second']

    tr = ogrtest.check_features_against_list(lyr, 'other', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(12.5 17 1.2)',
                                      max_error=0.000000001) == 0)

    assert feat.GetFID() == 1, 'Unexpected fid'

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(100 200 0)',
                                      max_error=0.000000001) == 0), feat.GetGeometryRef().ExportToIsoWkt()

    assert feat.GetFID() == 2, 'Unexpected fid'


###############################################################################
# Test a spatial query.

def test_ogr_vrt_4():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    lyr = gdaltest.vrt_ds.GetLayerByName('test3')
    lyr.ResetReading()

    lyr.SetSpatialFilterRect(90, 90, 300, 300)

    expect = ['Second']

    tr = ogrtest.check_features_against_list(lyr, 'other', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(100 200 0)',
                                      max_error=0.000000001) == 0), feat.GetGeometryRef().ExportToIsoWkt()

    lyr.SetSpatialFilter(None)


###############################################################################
# Test an attribute query.

def test_ogr_vrt_5():

    lyr = gdaltest.vrt_ds.GetLayerByName('test3')
    lyr.ResetReading()

    lyr.SetAttributeFilter('x < 50')

    expect = ['First']

    tr = ogrtest.check_features_against_list(lyr, 'other', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(12.5 17 1.2)',
                                      max_error=0.000000001) == 0)

    lyr.SetAttributeFilter(None)

###############################################################################
# Test GetFeature() on layer with FID coming from a column.


def test_ogr_vrt_6():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    lyr = gdaltest.vrt_ds.GetLayerByName('test3')
    lyr.ResetReading()

    feat = lyr.GetFeature(2)
    assert feat.GetField('other') == 'Second', 'GetFeature() did not work properly.'

###############################################################################
# Same as test 3, but on the result of an SQL query.
#


def test_ogr_vrt_7():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    lyr = gdaltest.vrt_ds.GetLayerByName('test4')

    expect = ['First', 'Second']

    tr = ogrtest.check_features_against_list(lyr, 'other', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(12.5 17 1.2)',
                                      max_error=0.000000001) == 0)

    assert feat.GetFID() == 1, 'Unexpected fid'

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(100 200 0)',
                                      max_error=0.000000001) == 0), feat.GetGeometryRef().ExportToIsoWkt()

    assert feat.GetFID() == 2, 'Unexpected fid'

###############################################################################
# Similar test, but now we put the whole VRT contents directly into the
# "filename".
#


def test_ogr_vrt_8():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    vrt_xml = '<OGRVRTDataSource><OGRVRTLayer name="test4"><SrcDataSource relativeToVRT="0">data/flat.dbf</SrcDataSource><SrcSQL>SELECT * FROM flat</SrcSQL><FID>fid</FID><GeometryType>wkbPoint</GeometryType><GeometryField encoding="PointFromColumns" x="x" y="y" z="z"/></OGRVRTLayer></OGRVRTDataSource>'
    ds = ogr.Open(vrt_xml)
    lyr = ds.GetLayerByName('test4')

    expect = ['First', 'Second']

    tr = ogrtest.check_features_against_list(lyr, 'other', expect)
    assert tr

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(12.5 17 1.2)',
                                      max_error=0.000000001) == 0)

    assert feat.GetFID() == 1, 'Unexpected fid'

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT(100 200 0)',
                                      max_error=0.000000001) == 0), feat.GetGeometryRef().ExportToIsoWkt()

    assert feat.GetFID() == 2, 'Unexpected fid'


###############################################################################
# Test that attribute filters are passed through to an underlying layer.

def test_ogr_vrt_9():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    lyr = gdaltest.vrt_ds.GetLayerByName('test3')
    lyr.SetAttributeFilter("other = 'Second'")
    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    assert feat.GetField('other') == 'Second', 'attribute filter did not work.'

    sub_ds = ogr.OpenShared('data/flat.dbf')
    sub_layer = sub_ds.GetLayerByName('flat')

    sub_layer.ResetReading()
    assert sub_layer.GetFeatureCount() == 1, 'attribute filter not passed to sublayer.'

    lyr.SetAttributeFilter(None)

    sub_ds.Release()
    sub_ds = None

###############################################################################
# Test capabilities
#


def test_ogr_vrt_10():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    vrt_xml = '<OGRVRTDataSource><OGRVRTLayer name="test"><SrcDataSource relativeToVRT="0">data/shp/testpoly.shp</SrcDataSource><SrcLayer>testpoly</SrcLayer></OGRVRTLayer></OGRVRTDataSource>'
    vrt_ds = ogr.Open(vrt_xml)
    vrt_lyr = vrt_ds.GetLayerByName('test')
    src_ds = ogr.Open('data/shp/testpoly.shp')
    src_lyr = src_ds.GetLayer(0)

    assert vrt_lyr.TestCapability(ogr.OLCFastFeatureCount) == src_lyr.TestCapability(ogr.OLCFastFeatureCount)
    assert vrt_lyr.TestCapability(ogr.OLCFastGetExtent) == src_lyr.TestCapability(ogr.OLCFastGetExtent)
    assert vrt_lyr.TestCapability(ogr.OLCRandomRead) == src_lyr.TestCapability(ogr.OLCRandomRead)

###############################################################################
# Test VRT write capabilities with PointFromColumns geometries
# Test also the reportGeomSrcColumn attribute


def test_ogr_vrt_11():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    f = open('tmp/test.csv', 'wb')
    f.write('x,val1,y,val2,style\n'.encode('ascii'))
    f.write('2,"val11",49,"val12","PEN(c:#FF0000,w:5pt,p:""2px 1pt"")"\n'.encode('ascii'))
    f.close()

    try:
        os.remove('tmp/test.csvt')
    except OSError:
        pass

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.csv</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <GeometryField encoding="PointFromColumns" x="x" y="y" reportSrcColumn="false"/>
        <Style>style</Style>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open(vrt_xml, update=1)
    vrt_lyr = vrt_ds.GetLayerByName('test')

    # Only val1, val2, style attributes should be reported
    assert vrt_lyr.GetLayerDefn().GetFieldCount() == 3
    assert vrt_lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() == 'val1'
    assert vrt_lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef() == 'val2'

    feat = vrt_lyr.GetNextFeature()
    if feat.GetStyleString() != 'PEN(c:#FF0000,w:5pt,p:"2px 1pt")':
        feat.DumpReadable()
        pytest.fail()

    feat = ogr.Feature(vrt_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (3 50)')
    feat.SetGeometryDirectly(geom)
    feat.SetField('val1', 'val21')
    vrt_lyr.CreateFeature(feat)

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetFeature(2)
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (3 50)':
        feat.DumpReadable()
        pytest.fail()
    assert feat.GetFieldAsString('val1') == 'val21'

    # The x and y fields are considered as string by default, so spatial
    # filter cannot be turned into attribute filter
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    vrt_lyr.SetSpatialFilterRect(0, 40, 10, 49.5)
    ret = vrt_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('not declared as numeric fields') != -1
    assert ret == 1

    vrt_ds = None

    # Add a .csvt file to specify the x and y columns as reals
    f = open('tmp/test.csvt', 'wb')
    f.write('Real,String,Real,String\n'.encode('ascii'))
    f.close()

    vrt_ds = ogr.Open(vrt_xml, update=1)
    vrt_lyr = vrt_ds.GetLayerByName('test')
    vrt_lyr.SetSpatialFilterRect(0, 40, 10, 49.5)
    assert vrt_lyr.GetFeatureCount() == 1
    assert gdal.GetLastErrorMsg() == ''

    vrt_lyr.SetAttributeFilter("1 = 1")
    assert vrt_lyr.GetFeatureCount() == 1

    vrt_lyr.SetAttributeFilter("1 = 0")
    assert vrt_lyr.GetFeatureCount() == 0

    vrt_ds = None

    os.remove('tmp/test.csv')
    os.remove('tmp/test.csvt')

###############################################################################
# Test VRT write capabilities with WKT geometries


def test_ogr_vrt_12():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    f = open('tmp/test.csv', 'wb')
    f.write('wkt_geom,val1,val2\n'.encode('ascii'))
    f.write('POINT (2 49),"val11","val12"\n'.encode('ascii'))
    f.close()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.csv</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <GeometryField encoding="WKT" field="wkt_geom"/>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open(vrt_xml, update=1)
    vrt_lyr = vrt_ds.GetLayerByName('test')

    feat = ogr.Feature(vrt_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (3 50)')
    feat.SetGeometryDirectly(geom)
    feat.SetField('val1', 'val21')
    vrt_lyr.CreateFeature(feat)

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetFeature(2)
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (3 50)'
    assert feat.GetFieldAsString('val1') == 'val21'

    vrt_ds = None

    os.remove('tmp/test.csv')

###############################################################################
# Test VRT write capabilities with WKB geometries


def test_ogr_vrt_13():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    f = open('tmp/test.csv', 'wb')
    f.write('wkb_geom,val1,val2\n'.encode('ascii'))
    f.close()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.csv</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <GeometryField encoding="WKB" field="wkb_geom"/>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open(vrt_xml, update=1)
    vrt_lyr = vrt_ds.GetLayerByName('test')

    feat = ogr.Feature(vrt_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (3 50)')
    feat.SetGeometryDirectly(geom)
    feat.SetField('val1', 'val21')
    vrt_lyr.CreateFeature(feat)

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetFeature(1)
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (3 50)'
    assert feat.GetFieldAsString('val1') == 'val21'

    vrt_ds = None

    os.remove('tmp/test.csv')

###############################################################################
# Test SrcRegion element for VGS_Direct


def test_ogr_vrt_14():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
    except AttributeError:
        pass
    gdal.PopErrorHandler()

    shp_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test.shp')
    shp_lyr = shp_ds.CreateLayer('test')

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    shp_ds.ExecuteSQL('CREATE SPATIAL INDEX on test')

    shp_ds = None

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="mytest">
        <SrcDataSource relativeToVRT="0">tmp/test.shp</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <SrcRegion>POLYGON((0 40,0 50,10 50,10 40,0 40))</SrcRegion>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open(vrt_xml)
    vrt_lyr = vrt_ds.GetLayerByName('mytest')

    assert vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1, 'Fast filter not set.'

    extent = vrt_lyr.GetExtent()
    assert extent == (2.0, 2.0, 49.0, 49.0), 'wrong extent'

    assert vrt_lyr.GetFeatureCount() == 1, 'Feature count not one as expected.'

    feat = vrt_lyr.GetNextFeature()
    assert feat.GetFID() == 2, 'did not get fid 2.'

    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (2 49)', 'did not get expected point geometry.'

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        if gdal.GetLastErrorMsg().find('GEOS support not enabled') != -1:
            vrt_ds = None
            ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
            pytest.skip()

        print(vrt_lyr.GetFeatureCount())
        pytest.fail('did not get one feature on rect spatial filter.')

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 48.5)
    assert vrt_lyr.GetFeatureCount() == 0, 'Did not get expected zero feature count.'

    vrt_lyr.SetSpatialFilter(None)
    assert vrt_lyr.GetFeatureCount() == 1, \
        'Did not get expected one feature count with no filter.'

    vrt_ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')


###############################################################################
# Test SrcRegion element for VGS_WKT

def test_ogr_vrt_15():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    f = open('tmp/test.csv', 'wb')
    f.write('wkt_geom,val1,val2\n'.encode('ascii'))
    f.write('POINT (-10 49),,\n'.encode('ascii'))
    f.write('POINT (-10 49),,\n'.encode('ascii'))
    f.write('POINT (2 49),,\n'.encode('ascii'))
    f.write('POINT (-10 49),,\n'.encode('ascii'))
    f.close()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.csv</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <GeometryField encoding="WKT" field="wkt_geom"/>
        <SrcRegion>POLYGON((0 40,0 50,10 50,10 40,0 40))</SrcRegion>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open(vrt_xml)
    vrt_lyr = vrt_ds.GetLayerByName('test')

    assert vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0

    assert vrt_lyr.GetFeatureCount() == 1

    feat = vrt_lyr.GetNextFeature()
    assert feat.GetFID() == 3

    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (2 49)'

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 49.5)
    assert vrt_lyr.GetFeatureCount() == 1

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 48.5)
    assert vrt_lyr.GetFeatureCount() == 0

    vrt_lyr.SetSpatialFilter(None)
    assert vrt_lyr.GetFeatureCount() == 1

    vrt_ds = None

    os.remove('tmp/test.csv')


###############################################################################
# Test SrcRegion element for VGS_PointFromColumns

def test_ogr_vrt_16():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    f = open('tmp/test.csvt', 'wb')
    f.write('Real,Real,String,String\n'.encode('ascii'))
    f.close()

    f = open('tmp/test.csv', 'wb')
    f.write('x,y,val1,val2\n'.encode('ascii'))
    f.write('-10,49,,\n'.encode('ascii'))
    f.write('-10,49,,\n'.encode('ascii'))
    f.write('2,49,,\n'.encode('ascii'))
    f.write('-10,49,,\n'.encode('ascii'))
    f.close()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.csv</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <GeometryField encoding="PointFromColumns" x="x" y="y"/>
        <SrcRegion>POLYGON((0 40,0 50,10 50,10 40,0 40))</SrcRegion>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open(vrt_xml)
    vrt_lyr = vrt_ds.GetLayerByName('test')

    assert vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0

    assert vrt_lyr.GetFeatureCount() == 1

    feat = vrt_lyr.GetNextFeature()
    assert feat.GetFID() == 3

    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (2 49)'

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        if gdal.GetLastErrorMsg().find('GEOS support not enabled') != -1:
            vrt_ds = None
            os.remove('tmp/test.csv')
            os.remove('tmp/test.csvt')
            pytest.skip()
        pytest.fail()

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 48.5)
    assert vrt_lyr.GetFeatureCount() == 0

    vrt_lyr.SetSpatialFilter(None)
    assert vrt_lyr.GetFeatureCount() == 1

    vrt_ds = None

    os.remove('tmp/test.csv')
    os.remove('tmp/test.csvt')


###############################################################################
# Test explicit field definitions.

def test_ogr_vrt_17():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">data/prime_meridian.csv</SrcDataSource>
        <SrcLayer>prime_meridian</SrcLayer>
        <Field name="pm_code" src="PRIME_MERIDIAN_CODE" type="integer" width="4" />
        <Field name="prime_meridian_name" width="24" />
        <Field name="new_col" type="Real" width="12" precision="3" />
        <Field name="DEPRECATED" type="Integer" subtype="Boolean" />
    </OGRVRTLayer>
</OGRVRTDataSource>"""

    vrt_ds = ogr.Open(vrt_xml)
    vrt_lyr = vrt_ds.GetLayerByName('test')

    assert vrt_lyr.GetLayerDefn().GetFieldCount() == 4, 'unexpected field count.'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(0)
    assert flddef.GetName() == 'pm_code' and flddef.GetType() == ogr.OFTInteger and flddef.GetWidth() == 4 and flddef.GetPrecision() == 0, \
        'pm_code field definition wrong.'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(1)
    assert flddef.GetName() == 'prime_meridian_name' and flddef.GetType() == ogr.OFTString and flddef.GetWidth() == 24 and flddef.GetPrecision() == 0, \
        'prime_meridian_name field definition wrong.'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(2)
    assert flddef.GetName() == 'new_col' and flddef.GetType() == ogr.OFTReal and flddef.GetWidth() == 12 and flddef.GetPrecision() == 3, \
        'new_col field definition wrong.'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(3)
    assert flddef.GetName() == 'DEPRECATED' and flddef.GetType() == ogr.OFTInteger and flddef.GetSubType() == ogr.OFSTBoolean, \
        'DEPRECATED field definition wrong.'

    feat = vrt_lyr.GetNextFeature()

    assert feat.GetField(0) == 8901 and feat.GetField(1) == "Greenwich" and feat.GetField(2) is None, \
        'did not get expected field value(s).'

###############################################################################
# Test that attribute filters are *not* passed to sublayer by default
# when explicit fields are defined.


def test_ogr_vrt_18():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">data/prime_meridian.csv</SrcDataSource>
        <SrcLayer>prime_meridian</SrcLayer>
        <Field name="pm_code" src="PRIME_MERIDIAN_CODE" type="integer" width="4" />
        <Field name="prime_meridian_name" width="24" />
        <Field name="new_col" type="Real" width="12" precision="3" />
    </OGRVRTLayer>
</OGRVRTDataSource>"""

    vrt_ds = ogr.Open(vrt_xml)
    vrt_lyr = vrt_ds.GetLayerByName('test')
    vrt_lyr.SetAttributeFilter('pm_code=8904')

    feat = vrt_lyr.GetNextFeature()

    assert feat.GetField(0) == 8904, 'Attribute filter not working properly'

###############################################################################
# Run test_ogrsf (optimized path)


def test_ogr_vrt_19_optimized():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/vrt/poly_vrt.vrt')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Run test_ogrsf (non optimized path)


def test_ogr_vrt_19_nonoptimized():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/vrt/poly_nonoptimized_vrt.vrt')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test VGS_Direct


def test_ogr_vrt_20():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
    except AttributeError:
        pass
    gdal.PopErrorHandler()

    shp_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test.shp')
    shp_lyr = shp_ds.CreateLayer('test')

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 45)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)

    shp_ds.ExecuteSQL('CREATE SPATIAL INDEX on test')

    shp_ds = None

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="mytest">
        <SrcDataSource relativeToVRT="0">tmp/test.shp</SrcDataSource>
        <SrcLayer>test</SrcLayer>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open(vrt_xml, update=1)
    vrt_lyr = vrt_ds.GetLayerByName('mytest')

    assert vrt_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1, \
        'Fast feature count not set.'

    assert vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1, 'Fast filter not set.'

    assert vrt_lyr.TestCapability(ogr.OLCFastGetExtent) == 1, 'Fast extent not set.'

    extent = vrt_lyr.GetExtent()
    assert extent == (-10.0, 2.0, 45.0, 49.0), 'wrong extent'

    assert vrt_lyr.GetFeatureCount() == 4, 'Feature count not 4 as expected.'

    vrt_lyr.SetSpatialFilterRect(1, 48.5, 3, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        if gdal.GetLastErrorMsg().find('GEOS support not enabled') != -1:
            vrt_ds = None
            ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
            pytest.skip()

        print(vrt_lyr.GetFeatureCount())
        pytest.fail('did not get one feature on rect spatial filter.')

    assert vrt_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1, \
        'Fast feature count not set.'

    assert vrt_lyr.TestCapability(ogr.OLCFastGetExtent) == 1, 'Fast extent not set.'

    extent = vrt_lyr.GetExtent()
    # the shapefile driver currently doesn't change the extent even in the
    # presence of a spatial filter, so that could change in the future
    assert extent == (-10.0, 2.0, 45.0, 49.0), 'wrong extent'

    vrt_lyr.SetSpatialFilterRect(1, 48, 3, 48.5)
    assert vrt_lyr.GetFeatureCount() == 0, 'Did not get expected zero feature count.'

    vrt_lyr.SetSpatialFilter(None)
    assert vrt_lyr.GetFeatureCount() == 4, \
        'Feature count not 4 as expected with no filter.'

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetNextFeature()
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    vrt_lyr.SetFeature(feat)
    feat = None

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetNextFeature()
    if feat.GetGeometryRef() is None or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        feat.DumpReadable()
        pytest.fail()

    vrt_ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')

###############################################################################
# Test lazy initialization with valid layer


def ogr_vrt_21_internal():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetName() == 'test3'
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetGeomType() == ogr.wkbPoint
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetSpatialRef().ExportToWkt().find('84') != -1
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.ResetReading()
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetNextFeature() is not None
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetFeature(1) is not None
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetFeatureCount() != 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.SetNextByIndex(1) == 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetLayerDefn().GetFieldCount() != 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.SetAttributeFilter('') == 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.SetSpatialFilter(None)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetExtent() is not None
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetFIDColumn() == 'fid'
    ds = None

    feature_defn = ogr.FeatureDefn()
    feat = ogr.Feature(feature_defn)
    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.SetFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.DeleteFeature(0)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.SyncToDisk()
    ds = None


def test_ogr_vrt_21():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        ret = ogr_vrt_21_internal()
    except:
        ret = 'fail'
    gdal.PopErrorHandler()
    return ret

###############################################################################
# Test lazy initialization with invalid layer


def ogr_vrt_22_internal():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetName() == 'test5'
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetGeomType() == ogr.wkbPoint
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetSpatialRef().ExportToWkt().find('84') != -1
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.ResetReading()
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetNextFeature() is None
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetFeature(1) is None
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetFeatureCount() == 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.SetNextByIndex(1) != 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetLayerDefn().GetFieldCount() == 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.SetAttributeFilter('') != 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.SetSpatialFilter(None)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    assert lyr.GetFIDColumn() == ''
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.GetExtent()
    ds = None

    feature_defn = ogr.FeatureDefn()
    feat = ogr.Feature(feature_defn)
    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.SetFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.DeleteFeature(0)
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.SyncToDisk()
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.StartTransaction()
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.CommitTransaction()
    ds = None

    ds = ogr.Open('data/vrt/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.RollbackTransaction()
    ds = None


def test_ogr_vrt_22():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        ret = ogr_vrt_22_internal()
    except:
        ret = 'fail'
    gdal.PopErrorHandler()
    return ret

###############################################################################
# Test anti-recursion mechanism


def test_ogr_vrt_23(shared_ds_flag=''):

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        pytest.skip('would crash')

    rec1 = """<OGRVRTDataSource>
    <OGRVRTLayer name="rec1">
        <SrcDataSource%s>/vsimem/rec2.vrt</SrcDataSource>
        <SrcLayer>rec2</SrcLayer>
    </OGRVRTLayer>
</OGRVRTDataSource>""" % shared_ds_flag

    rec2 = """<OGRVRTDataSource>
    <OGRVRTLayer name="rec2">
        <SrcDataSource%s>/vsimem/rec1.vrt</SrcDataSource>
        <SrcLayer>rec1</SrcLayer>
    </OGRVRTLayer>
</OGRVRTDataSource>""" % shared_ds_flag

    gdal.FileFromMemBuffer('/vsimem/rec1.vrt', rec1)
    gdal.FileFromMemBuffer('/vsimem/rec2.vrt', rec2)

    ds = ogr.Open('/vsimem/rec1.vrt')
    assert ds is not None

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetLayer(0).GetLayerDefn()
    ds.GetLayer(0).GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'error expected !'

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')

###############################################################################
# Test anti-recursion mechanism on shared DS


def test_ogr_vrt_24():

    test_ogr_vrt_23(' shared="1"')

    rec1 = """<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource shared="1">/vsimem/rec2.vrt</SrcDataSource>
    </OGRVRTLayer>
</OGRVRTDataSource>"""

    rec2 = """<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource shared="1">/vsimem/rec2.vrt</SrcDataSource>
    </OGRVRTLayer>
</OGRVRTDataSource>"""

    gdal.FileFromMemBuffer('/vsimem/rec1.vrt', rec1)
    gdal.FileFromMemBuffer('/vsimem/rec2.vrt', rec2)

    ds = ogr.Open('/vsimem/rec1.vrt')
    assert ds is not None

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetLayer(0).GetLayerDefn()
    ds.GetLayer(0).GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'error expected !'

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')


###############################################################################
# Test GetFIDColumn()

def test_ogr_vrt_25():

    with gdaltest.error_handler():
        ds = ogr.Open('data/vrt/vrt_test.vrt')

    # test3 layer just declares fid, and implicit fields (so all source
    # fields are taken as VRT fields), we can report the fid column
    lyr = ds.GetLayerByName('test3')
    assert lyr.GetFIDColumn() == 'fid'

    # test6 layer declares fid, and explicit fields without the fid
    lyr = ds.GetLayerByName('test6')
    assert lyr.GetFIDColumn() == 'fid'

    # test7 layer just declares fid with an external visible name
    lyr = ds.GetLayerByName('test7')
    assert lyr.GetFIDColumn() == 'bar'

    # test2 layer does not declare fid, and source layer has no fid column
    # so nothing to report
    lyr = ds.GetLayerByName('test2')
    assert lyr.GetFIDColumn() == ''

    ds = None

###############################################################################
# Test transaction support


def test_ogr_vrt_26():

    if ogr.GetDriverByName('SQLite') is None:
        pytest.skip()

    sqlite_ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_vrt_26.db')
    if sqlite_ds is None:
        pytest.skip()

    lyr = sqlite_ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr = None
    sqlite_ds.SyncToDisk()

    vrt_ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource>/vsimem/ogr_vrt_26.db</SrcDataSource>
    </OGRVRTLayer>
</OGRVRTDataSource>""", update=1)

    lyr = vrt_ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCTransactions) != 0

    lyr.StartTransaction()
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo')
    lyr.CreateFeature(feat)
    feat = None

    assert lyr.GetFeatureCount() == 1

    lyr.RollbackTransaction()

    assert lyr.GetFeatureCount() == 0

    lyr.StartTransaction()
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)
    feat = None
    lyr.CommitTransaction()

    assert lyr.GetFeatureCount() == 1

    vrt_ds = None

    sqlite_ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('/vsimem/ogr_vrt_26.db')

###############################################################################
# Test shapebin geometry


def test_ogr_vrt_27():

    csv = """dummy,shapebin
"dummy","01000000000000000000F03F0000000000000040"
"dummy","0300000000000000000008400000000000001040000000000000144000000000000018400100000002000000000000000000000000000840000000000000104000000000000014400000000000001840"
"dummy","0500000000000000000000000000000000000000000000000000F03F000000000000F03F010000000500000000000000000000000000000000000000000000000000000000000000000000000000F03F000000000000F03F000000000000F03F000000000000F03F000000000000000000000000000000000000000000000000"
"""

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_27.csv', csv)

    ds = ogr.Open("""<OGRVRTDataSource>
  <OGRVRTLayer name="ogr_vrt_27">
    <SrcDataSource relativeToVRT="0" shared="0">/vsimem/ogr_vrt_27.csv</SrcDataSource>
    <GeometryField encoding="shape" field="shapebin"/>
    <Field name="foo"/>
  </OGRVRTLayer>
</OGRVRTDataSource>""")

    assert ds is not None

    lyr = ds.GetLayer(0)

    wkt_list = ['POINT (1 2)', 'LINESTRING (3 4,5 6)', 'POLYGON ((0 0,0 1,1 1,1 0,0 0))']

    feat = lyr.GetNextFeature()
    i = 0
    while feat is not None:
        assert ogrtest.check_feature_geometry(feat, wkt_list[i]) == 0
        feat = lyr.GetNextFeature()
        i = i + 1

    ds = None

    gdal.Unlink('/vsimem/ogr_vrt_27.csv')

###############################################################################
# Invalid VRT testing


def test_ogr_vrt_28():

    with gdaltest.error_handler():
        ds = ogr.Open("<OGRVRTDataSource></foo>")
    assert ds is None

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_28_invalid.vrt', "<bla><OGRVRTDataSource></OGRVRTDataSource></bla>")
    with gdaltest.error_handler():
        ds = ogr.Open("/vsimem/ogr_vrt_28_invalid.vrt")
    assert ds is None
    gdal.Unlink("/vsimem/ogr_vrt_28_invalid.vrt")

    with gdaltest.error_handler():
        ds = ogr.Open("data/vrt/invalid.vrt")
    assert ds is not None

    for i in range(ds.GetLayerCount()):
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        lyr = ds.GetLayer(i)
        lyr.GetNextFeature()
        gdal.PopErrorHandler()
        assert gdal.GetLastErrorMsg() != '', \
            ('expected failure for layer %d of datasource %s' % (i, ds.GetName()))

    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr.Open("<OGRVRTDataSource><OGRVRTLayer/></OGRVRTDataSource>")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'expected error message on datasource opening'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("data/vrt/invalid2.vrt")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'expected error message on datasource opening'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("data/vrt/invalid3.vrt")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'expected error message on datasource opening'

###############################################################################
# Test OGRVRTWarpedLayer


def test_ogr_vrt_29():

    gdal.Unlink('tmp/ogr_vrt_29.shp')
    gdal.Unlink('tmp/ogr_vrt_29.shx')
    gdal.Unlink('tmp/ogr_vrt_29.dbf')
    gdal.Unlink('tmp/ogr_vrt_29.prj')

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ogr_vrt_29.shp')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('ogr_vrt_29', srs=sr)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))

    for i in range(5):
        for j in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField(0, i * 5 + j)
            feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%f %f)' % (2 + i / 5.0, 49 + j / 5.0)))
            lyr.CreateFeature(feat)
            feat = None

    ds = None

    # Invalid source layer
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/non_existing.shp</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    # Non-spatial layer
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="flat">
            <SrcDataSource>data/flat.dbf</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    # Missing TargetSRS
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    # Invalid TargetSRS
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>foo</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    # Invalid SrcSRS
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
        <SrcSRS>foo</SrcSRS>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    # TargetSRS == source SRS
    ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:4326</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    lyr = ds.GetLayer(0)
    ds = None

    # Forced extent
    ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:32631</TargetSRS>
        <ExtentXMin>426857</ExtentXMin>
        <ExtentYMin>5427475</ExtentYMin>
        <ExtentXMax>485608</ExtentXMax>
        <ExtentYMax>5516874</ExtentYMax>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    lyr = ds.GetLayer(0)
    bb = lyr.GetExtent()
    assert bb == (426857, 485608, 5427475, 5516874)
    ds = None

    f = open('tmp/ogr_vrt_29.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource relativetoVRT="1">ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>\n""")
    f.close()

    # Check reprojection in both directions
    ds = ogr.Open('tmp/ogr_vrt_29.vrt', update=1)
    lyr = ds.GetLayer(0)

    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    assert got_wkt.find('32631') != -1, 'did not get expected WKT'

    bb = lyr.GetExtent()
    expected_bb = (426857.98771727527, 485607.2165091355, 5427475.0501426803, 5516873.8591036052)

    for i in range(4):
        assert bb[i] == pytest.approx(expected_bb[i], abs=1), 'did not get expected extent'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(426857.987717275274917 5427937.523466162383556)') != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()

    feat.SetGeometry(None)
    assert lyr.SetFeature(feat) == 0

    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    assert lyr.SetFeature(feat) == 0
    feat = None

    lyr.SetSpatialFilterRect(499999, -1, 500001, 1)
    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(500000 0)') != 0:
        feat.DumpReadable()
        pytest.fail()
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('id', -99)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    assert lyr.CreateFeature(feat) == 0
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('id', -100)
    assert lyr.CreateFeature(feat) == 0
    feat = None

    ds = None

    # Check failed operations in read-only
    ds = ogr.Open('tmp/ogr_vrt_29.vrt')
    lyr = ds.GetLayer(0)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteFeature(1)
    gdal.PopErrorHandler()
    assert ret != 0

    feat = lyr.GetNextFeature()
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    assert ret != 0
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    assert ret != 0
    feat = None

    ds = None

    # Check in .shp file
    ds = ogr.Open('tmp/ogr_vrt_29.shp', update=1)
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(3.0 0.0)') != 0:
        feat.DumpReadable()
        pytest.fail()

    lyr.SetAttributeFilter('id = -99')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(3.0 0.0)') != 0:
        feat.DumpReadable()
        pytest.fail()

    lyr.SetAttributeFilter('id = -100')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1000)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-180 91)'))
    lyr.CreateFeature(feat)
    feat = None

    ds = None

    # Check failed reprojection when reading through VRT
    ds = ogr.Open('tmp/ogr_vrt_29.vrt', update=1)
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter('id = 1000')

    # Reprojection will fail
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    fid = feat.GetFID()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetFeature(fid)
    gdal.PopErrorHandler()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    feat = None

    lyr.DeleteFeature(fid)
    ds = None

    f = open('tmp/ogr_vrt_29_2.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource relativetoVRT="1">ogr_vrt_29.vrt</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:4326</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>\n""")
    f.close()

    # Check failed reprojection when writing through VRT
    ds = ogr.Open('tmp/ogr_vrt_29_2.vrt', update=1)
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-180 91)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    assert ret != 0
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-180 91)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    assert ret != 0
    feat = None

    ds = None

    # Some sanity operations before running test_ogrsf
    ds = ogr.Open('tmp/ogr_vrt_29.shp', update=1)
    ds.ExecuteSQL('REPACK ogr_vrt_29')
    ds.ExecuteSQL('RECOMPUTE EXTENT ON ogr_vrt_29')
    ds = None

    # Check with test_ogrsf
    if test_cli_utilities.get_test_ogrsf_path() is not None:

        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_29.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    gdal.Unlink('tmp/ogr_vrt_29.shp')
    gdal.Unlink('tmp/ogr_vrt_29.shx')
    gdal.Unlink('tmp/ogr_vrt_29.dbf')
    gdal.Unlink('tmp/ogr_vrt_29.prj')
    os.unlink('tmp/ogr_vrt_29.vrt')
    os.unlink('tmp/ogr_vrt_29_2.vrt')


###############################################################################
# Test OGRVRTUnionLayer

def test_ogr_vrt_30():

    for filename in ['tmp/ogr_vrt_30_1.shp',
                     'tmp/ogr_vrt_30_1.shx',
                     'tmp/ogr_vrt_30_1.dbf',
                     'tmp/ogr_vrt_30_1.prj',
                     'tmp/ogr_vrt_30_1.qix',
                     'tmp/ogr_vrt_30_2.shp',
                     'tmp/ogr_vrt_30_2.shx',
                     'tmp/ogr_vrt_30_2.dbf',
                     'tmp/ogr_vrt_30_2.prj',
                     'tmp/ogr_vrt_30_2.qix']:
        gdal.Unlink(filename)

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ogr_vrt_30_1.shp')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('ogr_vrt_30_1', srs=sr)
    lyr.CreateField(ogr.FieldDefn('id1', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('id2', ogr.OFTInteger))

    for i in range(5):
        for j in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField(0, i * 5 + j)
            feat.SetField(1, 100 + i * 5 + j)
            feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%f %f)' % (2 + i / 5.0, 49 + j / 5.0)))
            lyr.CreateFeature(feat)
            feat = None

    ds.ExecuteSQL('CREATE SPATIAL INDEX ON ogr_vrt_30_1')

    ds = None

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ogr_vrt_30_2.shp')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('ogr_vrt_30_2', srs=sr)
    lyr.CreateField(ogr.FieldDefn('id2', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('id3', ogr.OFTInteger))

    for i in range(5):
        for j in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField(0, 200 + i * 5 + j)
            feat.SetField(1, 300 + i * 5 + j)
            feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%f %f)' % (4 + i / 5.0, 49 + j / 5.0)))
            lyr.CreateFeature(feat)
            feat = None

    ds.ExecuteSQL('CREATE SPATIAL INDEX ON ogr_vrt_30_2')

    ds = None

    f = open('tmp/ogr_vrt_30.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="ogr_vrt_30_1">
            <SrcDataSource relativetoVRT="1">ogr_vrt_30_1.shp</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="ogr_vrt_30_2">
            <SrcDataSource relativetoVRT="1">ogr_vrt_30_2.shp</SrcDataSource>
        </OGRVRTLayer>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>\n""")
    f.close()

    # Check

    for check in range(10):
        ds = ogr.Open('tmp/ogr_vrt_30.vrt', update=1)
        lyr = ds.GetLayer(0)

        if check == 0:
            sr = lyr.GetSpatialRef()
            got_wkt = sr.ExportToWkt()
            assert got_wkt.find('GEOGCS["WGS 84"') != -1, 'did not get expected WKT'
        elif check == 1:
            bb = lyr.GetExtent()
            expected_bb = (2.0, 4.7999999999999998, 49.0, 49.799999999999997)

            for i in range(4):
                assert bb[i] == pytest.approx(expected_bb[i], abs=1), 'did not get expected extent'
        elif check == 2:
            feat_count = lyr.GetFeatureCount()
            assert feat_count == 2 * 5 * 5, 'did not get expected feature count'
        elif check == 3:
            assert lyr.GetLayerDefn().GetFieldCount() == 3, 'did not get expected field count'
            assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger64, \
                'did not get expected field type'
        elif check == 4:
            feat = lyr.GetNextFeature()
            i = 0
            while feat is not None:
                if i < 5 * 5:
                    assert feat.GetFID() == i, 'did not get expected value'
                    assert feat.GetFieldAsInteger("id1") == i, \
                        'did not get expected value'
                    assert feat.GetFieldAsInteger("id2") == 100 + i, \
                        'did not get expected value'
                    assert not feat.IsFieldSet("id3"), 'did not get expected value'
                    if ogrtest.check_feature_geometry(feat, 'POINT(%f %f)' % (2 + int(i / 5) / 5.0, 49 + int(i % 5) / 5.0)) != 0:
                        feat.DumpReadable()
                        pytest.fail('did not get expected value')
                else:
                    assert feat.GetFID() == i, 'did not get expected value'
                    assert not feat.IsFieldSet("id1"), 'did not get expected value'
                    assert feat.GetFieldAsInteger("id2") == 200 + i - 5 * 5, \
                        'did not get expected value'
                    assert feat.GetFieldAsInteger("id3") == 300 + i - 5 * 5, \
                        'did not get expected value'
                    assert ogrtest.check_feature_geometry(feat, 'POINT(%f %f)' % (4 + int((i - 5 * 5) / 5) / 5.0, 49 + int((i - 5 * 5) % 5) / 5.0)) == 0, \
                        'did not get expected value'

                i = i + 1
                feat = lyr.GetNextFeature()

        elif check == 5:
            assert lyr.GetGeomType() == ogr.wkbPoint, 'did not get expected geom type'

        elif check == 6:
            assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCIgnoreFields) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCRandomWrite) == 0, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCSequentialWrite) == 0, \
                'did not get expected capability'

        elif check == 7:
            lyr.SetSpatialFilterRect(2.49, 49.29, 4.49, 49.69)
            assert lyr.GetFeatureCount() == 10, 'did not get expected feature count'

        elif check == 8:
            lyr.SetAttributeFilter('id1 = 0')
            assert lyr.GetFeatureCount() == 1, 'did not get expected feature count'

        elif check == 9:
            # CreateFeature() should fail
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'
            feat = None

            # SetFeature() should fail
            lyr.ResetReading()
            feat = lyr.GetNextFeature()
            feat.SetField('id2', 45321)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'
            feat = None

            # Test feature existence : should fail
            lyr.SetAttributeFilter('id2 = 12345 or id2 = 45321')
            lyr.ResetReading()

            feat = lyr.GetNextFeature()
            assert feat is None, 'should have failed'

        ds = None

    # Check with test_ogrsf
    if test_cli_utilities.get_test_ogrsf_path() is not None:

        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_30.vrt --config OGR_VRT_MAX_OPENED 1')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/ogr_vrt_30.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # Test various optional attributes
    f = open('tmp/ogr_vrt_30.vrt', 'wt')
    f.write("""<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="ogr_vrt_30_1">
            <SrcDataSource relativetoVRT="1">ogr_vrt_30_1.shp</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="ogr_vrt_30_2">
            <SrcDataSource relativetoVRT="1">ogr_vrt_30_2.shp</SrcDataSource>
        </OGRVRTLayer>
        <SourceLayerFieldName>source_layer</SourceLayerFieldName>
        <PreserveSrcFID>ON</PreserveSrcFID>
        <FieldStrategy>Intersection</FieldStrategy>
        <GeometryType>wkbPoint25D</GeometryType>
        <LayerSRS>WGS72</LayerSRS>
        <FeatureCount>100</FeatureCount>
        <ExtentXMin>-180</ExtentXMin>
        <ExtentYMin>-90</ExtentYMin>
        <ExtentXMax>180</ExtentXMax>
        <ExtentYMax>90</ExtentYMax>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>\n""")
    f.close()

    for check in range(9):
        ds = ogr.Open('tmp/ogr_vrt_30.vrt', update=1)
        lyr = ds.GetLayer(0)

        if check == 0:
            sr = lyr.GetSpatialRef()
            got_wkt = sr.ExportToWkt()
            assert got_wkt.find('WGS 72') != -1, 'did not get expected WKT'

        elif check == 1:
            bb = lyr.GetExtent()
            expected_bb = (-180.0, 180.0, -90.0, 90.0)

            for i in range(4):
                assert bb[i] == pytest.approx(expected_bb[i], abs=1), 'did not get expected extent'

        elif check == 2:
            assert lyr.GetFeatureCount() == 100, 'did not get expected feature count'

        elif check == 3:
            assert lyr.GetLayerDefn().GetFieldCount() == 2, 'did not get expected field count'

        elif check == 4:
            feat = lyr.GetNextFeature()
            i = 0
            while feat is not None:
                if i < 5 * 5:
                    assert feat.GetFID() == i, 'did not get expected value'
                    assert feat.GetFieldAsString("source_layer") == 'ogr_vrt_30_1', \
                        'did not get expected value'
                    assert feat.GetFieldAsInteger("id2") == 100 + i, \
                        'did not get expected value'

                else:
                    assert feat.GetFID() == i - 5 * 5, 'did not get expected value'
                    assert feat.GetFieldAsString("source_layer") == 'ogr_vrt_30_2', \
                        'did not get expected value'
                    assert feat.GetFieldAsInteger("id2") == 200 + i - 5 * 5, \
                        'did not get expected value'

                i = i + 1
                feat = lyr.GetNextFeature()

        elif check == 5:
            assert lyr.GetGeomType() == ogr.wkbPoint25D, 'did not get expected geom type'

        elif check == 6:
            assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCIgnoreFields) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCRandomWrite) == 1, \
                'did not get expected capability'

            assert lyr.TestCapability(ogr.OLCSequentialWrite) == 1, \
                'did not get expected capability'

        elif check == 7:
            lyr.SetSpatialFilterRect(2.49, 49.29, 4.49, 49.69)
            assert lyr.GetFeatureCount() == 10, 'did not get expected feature count'

        elif check == 8:
            # invalid source_layer name with CreateFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('source_layer', 'random_name')
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'
            feat = None

            # unset source_layer name with CreateFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'
            feat = None

            # FID set with CreateFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetFID(999999)
            feat.SetField('source_layer', 'ogr_vrt_30_2')
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'
            feat = None

            # CreateFeature() OK
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('source_layer', 'ogr_vrt_30_2')
            feat.SetField('id2', 12345)
            assert lyr.CreateFeature(feat) == 0, 'should have succeeded'

            # SetFeature() OK
            feat.SetField('id2', 45321)
            assert lyr.SetFeature(feat) == 0, 'should have succeeded'

            # invalid source_layer name with SetFeature()
            feat.SetField('source_layer', 'random_name')
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'

            # unset source_layer name with SetFeature()
            feat.UnsetField('source_layer')
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'

            # FID unset with SetFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('source_layer', 'ogr_vrt_30_2')
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            assert ret != 0, 'should have failed'
            feat = None

            # Test feature existence (with passthru)
            lyr.SetAttributeFilter('id2 = 45321 AND OGR_GEOMETRY IS NULL')

            assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1, \
                'should have returned 1'

            lyr.ResetReading()

            feat = lyr.GetNextFeature()
            assert feat is not None, 'should have succeeded'

            # Test feature existence (without passthru)
            lyr.SetAttributeFilter("id2 = 45321 AND OGR_GEOMETRY IS NULL AND source_layer = 'ogr_vrt_30_2'")

            assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0, \
                'should have returned 0'

            lyr.ResetReading()

            feat = lyr.GetNextFeature()
            assert feat is not None, 'should have succeeded'

            # Test SyncToDisk()
            assert lyr.SyncToDisk() == 0, 'should have succeeded'

        ds = None

    for filename in ['tmp/ogr_vrt_30_1.shp',
                     'tmp/ogr_vrt_30_1.shx',
                     'tmp/ogr_vrt_30_1.dbf',
                     'tmp/ogr_vrt_30_1.prj',
                     'tmp/ogr_vrt_30_1.qix',
                     'tmp/ogr_vrt_30_2.shp',
                     'tmp/ogr_vrt_30_2.shx',
                     'tmp/ogr_vrt_30_2.dbf',
                     'tmp/ogr_vrt_30_2.prj',
                     'tmp/ogr_vrt_30_2.qix']:
        gdal.Unlink(filename)
    os.unlink('tmp/ogr_vrt_30.vrt')

###############################################################################
# Test anti-recursion mechanism with union layer


def test_ogr_vrt_31(shared_ds_flag=''):

    rec1 = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="rec1">
        <OGRVRTLayer name="rec2">
            <SrcDataSource%s>/vsimem/rec2.vrt</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="rec1">
            <SrcDataSource%s>/vsimem/rec1.vrt</SrcDataSource>
        </OGRVRTLayer>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>""" % (shared_ds_flag, shared_ds_flag)

    rec2 = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="rec2">
        <OGRVRTLayer name="rec1">
            <SrcDataSource%s>/vsimem/rec1.vrt</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="rec2">
            <SrcDataSource%s>/vsimem/rec2.vrt</SrcDataSource>
        </OGRVRTLayer>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>""" % (shared_ds_flag, shared_ds_flag)

    gdal.FileFromMemBuffer('/vsimem/rec1.vrt', rec1)
    gdal.FileFromMemBuffer('/vsimem/rec2.vrt', rec2)

    ds = ogr.Open('/vsimem/rec1.vrt')
    assert ds is not None

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetLayer(0).GetLayerDefn()
    ds.GetLayer(0).GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'error expected !'

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')

###############################################################################
# Test anti-recursion mechanism on shared DS


def test_ogr_vrt_32():

    return test_ogr_vrt_31(' shared="1"')


###############################################################################
# Test multi-geometry support

def test_ogr_vrt_33():

    try:
        import shutil
        shutil.rmtree('tmp/ogr_vrt_33')
    except OSError:
        pass

    ds = ogr.GetDriverByName('CSV').CreateDataSource('tmp/ogr_vrt_33', options=['GEOMETRY=AS_WKT'])
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone, options=['CREATE_CSVT=YES'])
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_4326_POINT", ogr.wkbPoint))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_32632_POLYGON", ogr.wkbPolygon))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_4326_LINESTRING", ogr.wkbLineString))
    lyr.CreateField(ogr.FieldDefn("X", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("Y", ogr.OFTReal))

    lyr = ds.CreateLayer('test2', geom_type=ogr.wkbNone, options=['CREATE_CSVT=YES'])
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_32632_POLYGON", ogr.wkbPolygon))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_4326_POINT", ogr.wkbPoint))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_32631_POINT", ogr.wkbPoint))
    lyr.CreateField(ogr.FieldDefn("Y", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("X", ogr.OFTReal))
    ds = None

    ds = ogr.Open('tmp/ogr_vrt_33', update=1)
    lyr = ds.GetLayerByName('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomField(0, ogr.CreateGeometryFromWkt('POINT (1 2)'))
    feat.SetGeomField(1, ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,1 0,0 0))'))
    feat.SetField("X", -1)
    feat.SetField("Y", -2)
    lyr.CreateFeature(feat)

    lyr = ds.GetLayerByName('test2')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomField(0, ogr.CreateGeometryFromWkt('POLYGON ((1 1,1 2,2 2,2 1,1 1))'))
    feat.SetGeomField(1, ogr.CreateGeometryFromWkt('POINT (3 4)'))
    feat.SetGeomField(2, ogr.CreateGeometryFromWkt('POINT (5 6)'))
    feat.SetField("X", -3)
    feat.SetField("Y", -4)
    lyr.CreateFeature(feat)
    ds = None

    for i in range(2):
        if i == 0:
            # Minimalist definition.
            ds_str = """<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
        else:
            # Implicit fields
            ds_str = """<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
        <GeometryField name="geom__WKT_EPSG_4326_POINT"/>
        <GeometryField name="geom__WKT_EPSG_32632_POLYGON"/>
    </OGRVRTLayer>
</OGRVRTDataSource>"""

        ds = ogr.Open(ds_str)
        lyr = ds.GetLayer(0)
        for j in range(5):
            if j == 0:
                assert lyr.GetGeomType() == ogr.wkbPoint
            elif j == 1:
                assert lyr.GetSpatialRef().ExportToWkt().find('GEOGCS') == 0
            elif j == 2:
                assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbPolygon
            elif j == 3:
                assert lyr.GetExtent(geom_field=1) == (0, 1, 0, 1)
            elif j == 4:
                assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef().ExportToWkt().find('PROJCS') == 0
                feat = lyr.GetNextFeature()
                if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
                    feat.DumpReadable()
                    pytest.fail()
                if feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
                    feat.DumpReadable()
                    pytest.fail()

        if test_cli_utilities.get_test_ogrsf_path() is not None:
            f = open('tmp/ogr_vrt_33.vrt', 'wb')
            f.write(ds_str.encode('ascii'))
            f.close()
            ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
            os.unlink('tmp/ogr_vrt_33.vrt')

            assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    if ogrtest.have_geos():
        # Select only second field and change various attributes
        ds_str = """<OGRVRTDataSource>
        <OGRVRTLayer name="test">
            <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
            <GeometryField name="foo" field="geom__WKT_EPSG_32632_POLYGON">
                <GeometryType>wkbPolygon25D</GeometryType>
                <SRS>+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs</SRS>
                <ExtentXMin>1</ExtentXMin>
                <ExtentYMin>2</ExtentYMin>
                <ExtentXMax>3</ExtentXMax>
                <ExtentYMax>4</ExtentYMax>
                <SrcRegion clip="true">POLYGON((0.5 0.5,1.5 0.5,1.5 1.5,0.5 1.5,0.5 0.5))</SrcRegion>
            </GeometryField>
        </OGRVRTLayer>
    </OGRVRTDataSource>"""
        for i in range(6):
            ds = ogr.Open(ds_str)
            lyr = ds.GetLayer(0)
            if i == 0:
                assert lyr.GetGeomType() == ogr.wkbPolygon25D
            elif i == 1:
                assert lyr.GetSpatialRef().ExportToWkt().find('+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs') >= 0
            elif i == 2:
                assert lyr.GetGeometryColumn() == 'foo'
            elif i == 3:
                assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1
                assert lyr.GetExtent(geom_field=0) == (1, 3, 2, 4)
            elif i == 4:
                assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
            elif i == 5:
                assert lyr.GetLayerDefn().GetGeomFieldCount() == 1
                feat = lyr.GetNextFeature()
                if feat.GetGeomFieldRef(0).ExportToWkt() not in ('POLYGON ((0.5 1.0,1 1,1.0 0.5,0.5 0.5,0.5 1.0))','POLYGON ((1 1,1.0 0.5,0.5 0.5,0.5 1.0,1 1))'):
                    feat.DumpReadable()
                    pytest.fail()

        if test_cli_utilities.get_test_ogrsf_path() is not None:
            f = open('tmp/ogr_vrt_33.vrt', 'wb')
            f.write(ds_str.encode('ascii'))
            f.close()
            ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
            os.unlink('tmp/ogr_vrt_33.vrt')

            assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # No geometry fields
    ds_str = """<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
        <GeometryType>wkbNone</GeometryType>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    for i in range(4):
        ds = ogr.Open(ds_str)
        lyr = ds.GetLayer(0)
        if i == 0:
            assert lyr.GetGeomType() == ogr.wkbNone
        elif i == 1:
            assert lyr.GetSpatialRef() is None
        elif i == 2:
            lyr.TestCapability(ogr.OLCFastFeatureCount)
            lyr.TestCapability(ogr.OLCFastGetExtent)
            lyr.TestCapability(ogr.OLCFastSpatialFilter)
        elif i == 3:
            assert lyr.GetLayerDefn().GetGeomFieldCount() == 0
            feat = lyr.GetNextFeature()
            if feat.GetGeomFieldRef(0) is not None:
                feat.DumpReadable()
                pytest.fail()

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ds_str = """<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
        <GeometryField name="geom__WKT_EPSG_4326_POINT"/>
        <GeometryField name="foo" encoding="PointFromColumns" x="X" y="Y" reportSrcColumn="false">
            <SRS>EPSG:4326</SRS>
        </GeometryField>
    </OGRVRTLayer>
</OGRVRTDataSource>
"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbPoint
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef().ExportToWkt().find('4326') >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (-1 -2)':
        feat.DumpReadable()
        pytest.fail()
    lyr.SetSpatialFilterRect(1, 0, 0, 0, 0)
    assert lyr.GetFeatureCount() == 0
    lyr.SetSpatialFilterRect(1, -1.1, -2.1, -0.9, -1.9)
    assert lyr.GetFeatureCount() == 1
    lyr.SetSpatialFilter(1, None)
    assert lyr.GetFeatureCount() == 1
    lyr.SetIgnoredFields(['geom__WKT_EPSG_4326_POINT'])
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0) is not None or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (-1 -2)':
        feat.DumpReadable()
        pytest.fail()
    lyr.SetIgnoredFields(['foo'])
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(1) is not None or \
       feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
        feat.DumpReadable()
        pytest.fail()

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # Warped layer without explicit WarpedGeomFieldName
    ds_str = """<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="test">
            <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().ExportToWkt().find('32631') >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt().find('POINT') < 0 or \
       feat.GetGeomFieldRef(0).ExportToWkt() == 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        pytest.fail()

    # Warped layer with explicit WarpedGeomFieldName (that is the first field)
    ds_str = """<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="test">
            <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <WarpedGeomFieldName>geom__WKT_EPSG_4326_POINT</WarpedGeomFieldName>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    assert lyr.GetSpatialRef().ExportToWkt().find('32631') >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt().find('POINT') < 0 or \
       feat.GetGeomFieldRef(0).ExportToWkt() == 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        pytest.fail()

    # Warped layer with explicit WarpedGeomFieldName (that does NOT exist)
    ds_str = """<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="test">
            <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <WarpedGeomFieldName>foo</WarpedGeomFieldName>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>"""
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr.Open(ds_str)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    # Warped layer with explicit WarpedGeomFieldName (that is the second field)
    ds_str = """<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="test">
            <SrcDataSource>tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <WarpedGeomFieldName>geom__WKT_EPSG_32632_POLYGON</WarpedGeomFieldName>
        <TargetSRS>EPSG:4326</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef().ExportToWkt().find('4326') >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(1).ExportToWkt().find('POLYGON') < 0 or \
       feat.GetGeomFieldRef(1).ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
        feat.DumpReadable()
        pytest.fail()

    # UnionLayer with default union strategy

    ds_str = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="test">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="test2">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    geom_fields = [['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326'],
                   ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632'],
                   ['geom__WKT_EPSG_4326_LINESTRING', ogr.wkbLineString, '4326'],
                   ['geom__WKT_EPSG_32631_POINT', ogr.wkbPoint, '32631']]
    assert lyr.GetLayerDefn().GetGeomFieldCount() == len(geom_fields)
    for i, geom_field in enumerate(geom_fields):
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() == geom_field[0]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() == geom_field[1]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_field[2]) >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3) is not None:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3).ExportToWkt() != 'POINT (5 6)':
        feat.DumpReadable()
        pytest.fail()

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # UnionLayer with intersection strategy

    ds_str = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="test">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="test2">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <FieldStrategy>Intersection</FieldStrategy>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    geom_fields = [['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326'],
                   ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632']]
    assert lyr.GetLayerDefn().GetGeomFieldCount() == len(geom_fields)
    for i, geom_field in enumerate(geom_fields):
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() == geom_field[0]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() == geom_field[1]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_field[2]) >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))':
        feat.DumpReadable()
        pytest.fail()

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # UnionLayer with FirstLayer strategy

    ds_str = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="test">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="test2">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <FieldStrategy>FirstLayer</FieldStrategy>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    geom_fields = [['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326'],
                   ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632'],
                   ['geom__WKT_EPSG_4326_LINESTRING', ogr.wkbLineString, '4326']]
    assert lyr.GetLayerDefn().GetGeomFieldCount() == len(geom_fields)
    for i, geom_field in enumerate(geom_fields):
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() == geom_field[0]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() == geom_field[1]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_field[2]) >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))':
        feat.DumpReadable()
        pytest.fail()

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # UnionLayer with explicit fields but without further information

    ds_str = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="test">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="test2">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <GeometryField name="geom__WKT_EPSG_32632_POLYGON"/>
        <GeometryField name="geom__WKT_EPSG_4326_POINT"/>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    geom_fields = [['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632'],
                   ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326']]
    assert lyr.GetLayerDefn().GetGeomFieldCount() == len(geom_fields)
    for i, geom_field in enumerate(geom_fields):
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() == geom_field[0]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() == geom_field[1]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_field[2]) >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (1 2)':
        feat.DumpReadable()
        pytest.fail()

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # UnionLayer with explicit fields with extra information

    ds_str = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="test">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="test2">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <GeometryField name="geom__WKT_EPSG_32632_POLYGON">
            <GeometryType>wkbPolygon25D</GeometryType>
        </GeometryField>
        <GeometryField name="geom__WKT_EPSG_4326_POINT">
            <SRS>EPSG:4322</SRS> <!-- will trigger reprojection -->
            <ExtentXMin>1</ExtentXMin>
            <ExtentYMin>2</ExtentYMin>
            <ExtentXMax>3</ExtentXMax>
            <ExtentYMax>4</ExtentYMax>
        </GeometryField>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    geom_fields = [['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon25D, '32632'],
                   ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4322']]
    assert lyr.GetLayerDefn().GetGeomFieldCount() == len(geom_fields)
    bb = lyr.GetExtent(geom_field=1)
    assert bb == (1, 3, 2, 4)
    for i, geom_field in enumerate(geom_fields):
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() == geom_field[0]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() == geom_field[1]
        assert lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_field[2]) >= 0
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(1).ExportToWkt().find('POINT (') != 0 or \
       feat.GetGeomFieldRef(1).ExportToWkt() == 'POINT (1 2)':
        feat.DumpReadable()
        pytest.fail()
    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # UnionLayer with geometry fields disabled

    ds_str = """<OGRVRTDataSource>
    <OGRVRTUnionLayer name="union_layer">
        <OGRVRTLayer name="test">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <OGRVRTLayer name="test2">
            <SrcDataSource shared="1">tmp/ogr_vrt_33</SrcDataSource>
        </OGRVRTLayer>
        <GeometryType>wkbNone</GeometryType>
    </OGRVRTUnionLayer>
</OGRVRTDataSource>"""
    ds = ogr.Open(ds_str)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 0
    assert lyr.GetLayerDefn().GetFieldCount() == 6, \
        lyr.GetLayerDefn().GetGeomFieldCount()
    feat = lyr.GetNextFeature()
    assert feat is not None

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str.encode('ascii'))
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ds = ogr.Open('tmp/ogr_vrt_33')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test UNION ALL SELECT * FROM test2')
    geom_fields = [['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326'],
                   ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632'],
                   ['geom__WKT_EPSG_4326_LINESTRING', ogr.wkbLineString, '4326'],
                   ['geom__WKT_EPSG_32631_POINT', ogr.wkbPoint, '32631']]
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == len(geom_fields)
    for i, geom_field in enumerate(geom_fields):
        assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() == geom_field[0]
        assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() == geom_field[1]
        assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_field[2]) >= 0
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3) is not None:
        feat.DumpReadable()
        pytest.fail()

    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3).ExportToWkt() != 'POINT (5 6)':
        feat.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

###############################################################################
# Test SetIgnoredFields() with with PointFromColumns geometries


def test_ogr_vrt_34():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    f = open('tmp/test.csv', 'wb')
    f.write('x,y\n'.encode('ascii'))
    f.write('2,49\n'.encode('ascii'))
    f.close()

    try:
        os.remove('tmp/test.csvt')
    except OSError:
        pass

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.csv</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <GeometryField encoding="PointFromColumns" x="x" y="y"/>
        <Field name="x" type="Real"/>
        <Field name="y" type="Real"/>
    </OGRVRTLayer>
</OGRVRTDataSource>"""

    ds = ogr.Open(vrt_xml)
    lyr = ds.GetLayerByName('test')
    lyr.SetIgnoredFields(['x', 'y'])
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    os.unlink('tmp/test.csv')

###############################################################################
# Test nullable fields


def test_ogr_vrt_35():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    f = open('tmp/test.csv', 'wb')
    f.write('c1,c2,WKT,WKT2\n'.encode('ascii'))
    f.write('1,,"POINT(2 49),"\n'.encode('ascii'))
    f.close()

    try:
        os.remove('tmp/test.csvt')
    except OSError:
        pass

    # Explicit nullable
    f = open('tmp/test.vrt', 'wb')
    f.write("""<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="1">test.csv</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <GeometryField encoding="WKT" field="WKT" name="g1" nullable="false"/>
        <GeometryField encoding="WKT" field="WKT2" name="g2"/>
        <Field name="c1" type="Integer" nullable="false"/>
        <Field name="c2" type="Integer"/>
    </OGRVRTLayer>
</OGRVRTDataSource>""".encode('ascii'))
    f.close()

    ds = ogr.Open('tmp/test.vrt')
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(0).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(1).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).IsNullable() == 1
    ds = None

    # Implicit nullable
    f = open('tmp/test2.vrt', 'wb')
    f.write("""<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="1">test.vrt</SrcDataSource>
        <SrcLayer>test</SrcLayer>
    </OGRVRTLayer>
</OGRVRTDataSource>""".encode('ascii'))
    f.close()

    ds = ogr.Open('tmp/test2.vrt')
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(0).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(1).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).IsNullable() == 1
    ds = None

    os.unlink('tmp/test.csv')
    os.unlink('tmp/test.vrt')
    os.unlink('tmp/test2.vrt')

###############################################################################
# Test editing direct geometries


def test_ogr_vrt_36():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_vrt_36.shp')
    lyr = ds.CreateLayer('ogr_vrt_36', geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn('id'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = '1'
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_36.vrt',
                           """<OGRVRTDataSource>
    <OGRVRTLayer name="ogr_vrt_36">
        <SrcDataSource relativeToVRT="1">/vsimem/ogr_vrt_36.shp</SrcDataSource>
        <GeometryType>wkbPoint</GeometryType>
        <LayerSRS>WGS84</LayerSRS>
    </OGRVRTLayer>
</OGRVRTDataSource>""")

    ds = ogr.Open('/vsimem/ogr_vrt_36.vrt', update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_vrt_36.shp')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['id'] != '1':
        f.DumpReadable()
        pytest.fail()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/ogr_vrt_36.shp')
    gdal.Unlink('/vsimem/ogr_vrt_36.vrt')

###############################################################################
# Test implicit non-spatial layers (#6336)


def test_ogr_vrt_37():

    with gdaltest.error_handler():
        ds = ogr.Open('data/vrt/vrt_test.vrt')

    lyr = ds.GetLayerByName('test6')
    assert lyr.GetGeomType() == ogr.wkbNone

    with gdaltest.error_handler():
        ds = ogr.Open('data/vrt/vrt_test.vrt')

    lyr = ds.GetLayerByName('test6')
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 0

###############################################################################
# Test reading geometry type


def test_ogr_vrt_38():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    types = [['Point', ogr.wkbPoint],
             ['LineString', ogr.wkbLineString],
             ['Polygon', ogr.wkbPolygon],
             ['MultiPoint', ogr.wkbMultiPoint],
             ['MultiLineString', ogr.wkbMultiLineString],
             ['MultiPolygon', ogr.wkbMultiPolygon],
             ['GeometryCollection', ogr.wkbGeometryCollection],
             ['CircularString', ogr.wkbCircularString],
             ['CompoundCurve', ogr.wkbCompoundCurve],
             ['CurvePolygon', ogr.wkbCurvePolygon],
             ['MultiCurve', ogr.wkbMultiCurve],
             ['MultiSurface', ogr.wkbMultiSurface],
             ['Curve', ogr.wkbCurve],
             ['Surface', ogr.wkbSurface]
             ]

    for (type_str, ogr_type) in types:
        for qualifier in ['', 'Z', 'M', 'ZM', '25D']:
            if qualifier == 'Z' and ogr_type <= ogr.wkbGeometryCollection:
                continue
            if qualifier == '25D' and ogr_type > ogr.wkbGeometryCollection:
                continue
            gdal.FileFromMemBuffer('/vsimem/ogr_vrt_38.vrt',
                                   """<OGRVRTDataSource>
        <OGRVRTLayer name="ogr_vrt_38">
            <SrcDataSource relativeToVRT="1">/vsimem/ogr_vrt_38.shp</SrcDataSource>
            <GeometryType>wkb%s%s</GeometryType>
        </OGRVRTLayer>
    </OGRVRTDataSource>""" % (type_str, qualifier))

            expected_geom_type = ogr_type
            if qualifier == 'Z' or qualifier == 'ZM' or qualifier == '25D':
                expected_geom_type = ogr.GT_SetZ(expected_geom_type)
            if qualifier == 'M' or qualifier == 'ZM':
                expected_geom_type = ogr.GT_SetM(expected_geom_type)

            ds = ogr.Open('/vsimem/ogr_vrt_38.vrt', update=1)
            lyr = ds.GetLayer(0)
            assert lyr.GetGeomType() == expected_geom_type, \
                (type_str, qualifier, lyr.GetGeomType())

    gdal.Unlink('/vsimem/ogr_vrt_38.vrt')

###############################################################################
# Test that attribute filtering works with <FID>


def test_ogr_vrt_39():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_39.csv',
                           """my_fid,val
30,1
25,2
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_39.csvt',
                           """Integer,Integer
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_39.vrt',
                           """<OGRVRTDataSource>
    <OGRVRTLayer name="ogr_vrt_39">
        <SrcDataSource relativeToVRT="1">/vsimem/ogr_vrt_39.csv</SrcDataSource>
        <GeometryType>wkbNone</GeometryType>
        <FID>my_fid</FID>
        <Field name="val" type="Integer" src="val"/>
    </OGRVRTLayer>
</OGRVRTDataSource>""")

    ds = ogr.Open('/vsimem/ogr_vrt_39.vrt')
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter('fid = 25')
    f = lyr.GetNextFeature()
    if f['val'] != 2:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_vrt_39.csv')
    gdal.Unlink('/vsimem/ogr_vrt_39.csvt')
    gdal.Unlink('/vsimem/ogr_vrt_39.vrt')

###############################################################################
# Test PointZM support with encoding="PointFromColumns"


def test_ogr_vrt_40():
    if gdaltest.vrt_ds is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_40.csv',
                           """id,x,y,z,m
1,1,2,3,4
""")

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_40.vrt',
                           """<OGRVRTDataSource>
  <OGRVRTLayer name="ogr_vrt_40">
    <SrcDataSource relativeToVRT="1">ogr_vrt_40.csv</SrcDataSource>
    <SrcLayer>ogr_vrt_40</SrcLayer>
    <GeometryField encoding="PointFromColumns" x="x" y="y" z="z" m="m"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
""")

    ds = ogr.Open('/vsimem/ogr_vrt_40.vrt')
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPointZM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT ZM (1 2 3 4)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_vrt_40.csv')
    gdal.Unlink('/vsimem/ogr_vrt_40.vrt')

###############################################################################
# Test GetExtent() on erroneous definition


def test_ogr_vrt_41():

    ds = ogr.Open("""<OGRVRTDataSource>
  <OGRVRTLayer name="test">
    <SrcDataSource>/i_dont/exist</SrcDataSource>
  </OGRVRTLayer>
</OGRVRTDataSource>""")
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        lyr.GetExtent()


###############################################################################
# Test reading nullable, default, unique


def test_ogr_vrt_nullable_unique():

    ds = ogr.Open("""<OGRVRTDataSource>
  <OGRVRTLayer name="poly">
    <SrcDataSource>data/poly.shp</SrcDataSource>
    <SrcLayer>poly</SrcLayer>
    <GeometryType>wkbPolygon</GeometryType>
    <Field name="area" type="Real"/>
    <Field name="eas_id" type="Integer64" width="11" nullable="false" unique="true"/>
    <Field name="prfedea" type="String"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).IsNullable()
    assert not feat_defn.GetFieldDefn(0).IsUnique()
    assert not feat_defn.GetFieldDefn(1).IsNullable()
    assert feat_defn.GetFieldDefn(1).IsUnique()


###############################################################################
# Test field names with same case


def test_ogr_vrt_field_names_same_case():

    ds = ogr.Open("""<OGRVRTDataSource>
  <OGRVRTLayer name="test">
    <SrcDataSource>{"type":"Feature","id":"foo","geometry":null,"properties":{"ID":"bar"}}</SrcDataSource>
    <SrcLayer>OGRGeoJSON</SrcLayer>
    <Field name="id" type="String" src="id"/>
    <Field name="id_from_uc" type="String" src="ID"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
""")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f['id'] == 'foo'
    assert f['id_from_uc'] == 'bar'

###############################################################################
#


def test_ogr_vrt_cleanup():

    if gdaltest.vrt_ds is None:
        pytest.skip()

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')

    try:
        os.unlink('tmp/ogr_vrt_33.vrt')
    except OSError:
        pass

    try:
        import shutil
        shutil.rmtree('tmp/ogr_vrt_33')
    except OSError:
        pass

    gdaltest.vrt_ds = None



